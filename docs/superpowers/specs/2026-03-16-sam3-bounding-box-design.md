# SAM3 Bounding Box Extraction Design

**Date:** 2026-03-16  
**Status:** Draft  
**Author:** AI Assistant  

## Summary

Add bounding box extraction capability to the SAM3TensorRT C++ implementation. The system will extract bounding boxes from instance segmentation masks using a custom CUDA kernel, support YOLO format output for training data generation, and integrate with the existing training pipeline.

## Existing Code

### Current SAM3_PCS_RESULT Struct

The existing struct in `SAM3TensorRT/cpp/include/sam3.hpp`:

```cpp
// CURRENT (never populated)
typedef struct {
    float score;
    int box_x, box_y, box_w, box_h;  // Defined but never set
    std::vector<int> mask_x;          // Was intended for contour X coords
    std::vector<int> mask_y;          // Was intended for contour Y coords
} SAM3_PCS_RESULT;
```

**Changes:** Extend the struct with new fields for mask coordinates, class ID, and rename contour fields for clarity.

## Requirements

### Functional Requirements

1. **FR1:** Extract bounding boxes from SAM3 instance masks (up to 200 instances per image)
2. **FR2:** Support YOLO format output: `class_id x_center y_center width height`
3. **FR3:** Return bounding boxes in both original image coordinates and mask coordinates (288x288)
4. **FR4:** Filter detections by confidence threshold and minimum box area
5. **FR5:** Save YOLO label files to `scripts/training/datasets/game/labels/`
6. **FR6:** Support class name to ID mapping via `scripts/training/predefined_classes.txt`

### Non-Functional Requirements

1. **NFR1:** Bounding box extraction should complete in < 2ms (CUDA kernel)
2. **NFR2:** API should support multiple backends (CUDA kernel, OpenCV CUDA, OpenCV CPU)
3. **NFR3:** No breaking changes to existing `SAM3_PCS` API

### Constraints

1. **C1:** Must use existing TensorRT output buffers (`output_gpu[0]`)
2. **C2:** Must integrate with OpenCV CUDA build at `I:\CppProjects\sunone_aimbot_2\sunone_aimbot_2\modules\opencv`
3. **C3:** Output format must match existing YOLO training data structure

## Design

### Data Structures

```cpp
// SAM3TensorRT/cpp/include/sam3.hpp - Updated struct
typedef struct {
    float score;           // Confidence from sigmoid of mask mean
    int class_id;          // NEW: Mapped from prompt/class argument
    
    // Bounding box in ORIGINAL IMAGE coordinates (now populated)
    int box_x, box_y, box_w, box_h;
    
    // NEW: Bounding box in MASK coordinates (288x288 space)
    int mask_x, mask_y, mask_w, mask_h;
    
    // Renamed for clarity (was mask_x, mask_y)
    std::vector<int> contour_x;
    std::vector<int> contour_y;
} SAM3_PCS_RESULT;

// NEW: Enum for backend selection
typedef enum {
    BBOX_BACKEND_OPENCV_CPU,    // Fallback, simpler
    BBOX_BACKEND_OPENCV_CUDA,   // Uses cv::cuda::GpuMat
    BBOX_BACKEND_CUDA_KERNEL    // Custom CUDA kernel (fastest)
} SAM3_BBOX_BACKEND;

// NEW: Struct for extraction parameters
typedef struct {
    SAM3_BBOX_BACKEND backend = BBOX_BACKEND_CUDA_KERNEL;
    float score_threshold = 0.5f;      // Filter results by instance confidence (post-kernel)
    int min_box_area = 100;            // Filter tiny boxes (pixels^2)
    bool include_contours = false;     // Export polygon points
} SAM3_BBOX_OPTIONS;
```

**Note on thresholds:**
- `prob_threshold` (kernel parameter): Used to binarize mask pixels for bbox computation
- `score_threshold` (BBOX_OPTIONS): Used to filter entire instances from results based on mean confidence
- `min_box_area` (BBOX_OPTIONS): Filters out degenerate bboxes (0-width/height or tiny detections)
```

### API Design

```cpp
class SAM3_PCS {
public:
    // Existing methods remain unchanged
    
    // NEW: Set class mapping for YOLO output
    void set_class_id(int class_id);
    
    // NEW: Main bounding box extraction method
    std::vector<SAM3_PCS_RESULT> extract_bounding_boxes(
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options = SAM3_BBOX_OPTIONS{}
    );
    
    // NEW: Convenience method for YOLO output
    bool save_yolo_labels(
        const std::string& image_path,
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options = SAM3_BBOX_OPTIONS{}
    );

private:
    // NEW: Internal buffers for bbox computation
    int* d_mask_bboxes;        // Device: [200, 4] mask-space bboxes
    int* d_image_bboxes;       // Device: [200, 4] image-space bboxes
    float* d_instance_scores;  // Device: [200] confidence scores
    int* h_bbox_buffer;        // Host: [200, 4] for copying results
    float* h_score_buffer;     // Host: [200] for copying scores
    
    int _current_class_id = 0;
    
    // Backend implementations
    std::vector<SAM3_PCS_RESULT> extract_bboxes_cuda_kernel(
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options
    );
    std::vector<SAM3_PCS_RESULT> extract_bboxes_opencv_cuda(
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options
    );
    std::vector<SAM3_PCS_RESULT> extract_bboxes_opencv_cpu(
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options
    );
};
```

### Error Handling

| Scenario | Behavior |
|----------|----------|
| No instances found (all filtered) | Return empty `std::vector<SAM3_PCS_RESULT>` |
| Class name not in predefined_classes.txt | Log error to stderr, return -1 from `load_class_id_from_file()`, CLI exits with code 1 |
| Output directory doesn't exist | `save_yolo_labels()` creates it via `std::filesystem::create_directories()` |
| Output directory not writable | `save_yolo_labels()` returns `false`, logs error to stderr |
| CUDA memory allocation fails | Throw `std::runtime_error` with CUDA error message |
| `infer_on_image()` not called first | Return empty vector (no mask data available) |

### Buffer Management

- Buffers (`d_mask_bboxes`, `d_image_bboxes`, etc.) are allocated once in the `SAM3_PCS` constructor and reused for all subsequent calls to `extract_bounding_boxes()`.
- Destructor frees all allocated GPU and host memory.
- No re-allocation on repeated calls — buffers are fixed-size for 200 instances.

### CUDA Kernels

```cpp
// SAM3TensorRT/cpp/include/prepost.cuh - New kernel declarations

__global__ void compute_bboxes_from_masks(
    const float* __restrict__ mask_logits,  // [1, 200, 288, 288]
    int* bbox_output,                        // [200, 4] - x_min, y_min, x_max, y_max
    float* scores_output,                    // [200] - mean confidence per instance
    int num_instances,                       // 200
    int mask_width,                          // 288
    int mask_height,                         // 288
    float prob_threshold                     // 0.5
);

__global__ void scale_bboxes_to_image(
    const int* mask_bboxes,      // [200, 4] in mask space
    int* image_bboxes,           // [200, 4] in image space
    int num_instances,
    int mask_width, int mask_height,
    int image_width, int image_height
);
```

### Kernel Implementation Strategy

```
compute_bboxes_from_masks kernel:
- Grid: (200 blocks, 1, 1)  - one block per instance
- Block: (288 threads, 1, 1) - one thread per row

Each thread:
1. Iterates across all columns in its row
2. Computes sigmoid(logit) for each pixel
3. If prob > threshold: atomicMin/Max on bbox corners
4. Accumulates score sum for mean confidence

Shared memory per block:
- int[4] for bbox (x_min, y_min, x_max, y_max)
- float for score accumulator
- int for pixel count above threshold

Score calculation: Mean confidence is computed from ALL pixels in the mask, not just those above threshold. This provides a quality metric for the entire instance detection.
```

### Bbox Format Conversion

The CUDA kernel outputs `[x_min, y_min, x_max, y_max]` for efficient atomic operations. This is converted to `[x, y, w, h]` format on the CPU after copying results:

```cpp
// Conversion in extract_bboxes_cuda_kernel()
for (int i = 0; i < num_instances; ++i) {
    int x_min = h_bbox_buffer[i * 4 + 0];
    int y_min = h_bbox_buffer[i * 4 + 1];
    int x_max = h_bbox_buffer[i * 4 + 2];
    int y_max = h_bbox_buffer[i * 4 + 3];
    
    SAM3_PCS_RESULT result;
    result.box_x = x_min;
    result.box_y = y_min;
    result.box_w = x_max - x_min;
    result.box_h = y_max - y_min;
    // ... same for mask coords
}
```

### Utility Functions

```cpp
// Standalone utility in sam3_pcs_app.cpp (not a class member)
// Returns class ID (0-indexed) or -1 if not found
int load_class_id_from_file(
    const std::string& class_name,
    const std::string& classes_file_path = "scripts/training/predefined_classes.txt"
);
```

### YOLO Output Format

```
File: scripts/training/datasets/game/labels/<image_name>.txt
Format: <class_id> <x_center> <y_center> <width> <height>

Example:
0 0.217593 0.546296 0.148148 0.253704
7 0.232871 0.447222 0.030556 0.035185
```

Coordinates are normalized to [0, 1] range.

### CLI Usage

```bash
# Standard inference - visualization only, no labels saved
sam3_pcs_app images/ engine.engine

# Inference with YOLO label generation
sam3_pcs_app images/ engine.engine -prompt "person" -class "player"

# Different detection target
sam3_pcs_app images/ engine.engine -prompt "helmet" -class "head"

# If -class is omitted, no YOLO labels are saved (just visualization)
sam3_pcs_app images/ engine.engine -prompt "person"
```

**CLI Arguments:**
- `-prompt "text"`: The text prompt passed to CLIP tokenizer for semantic segmentation. Determines what objects to detect (e.g., "person", "car", "helmet").
- `-class "name"`: The YOLO class name to assign to detections. If provided, YOLO label files are saved. If omitted, only visualization is output.

## File Changes

| File | Action | Changes |
|------|--------|---------|
| `SAM3TensorRT/cpp/include/sam3.hpp` | Modify | Add `SAM3_BBOX_BACKEND`, `SAM3_BBOX_OPTIONS`, extend `SAM3_PCS_RESULT` |
| `SAM3TensorRT/cpp/include/sam3.cuh` | Modify | Add method declarations, new private members |
| `SAM3TensorRT/cpp/include/prepost.cuh` | Modify | Add kernel declarations |
| `SAM3TensorRT/cpp/src/sam3/sam3_trt/prepost.cu` | Modify | Implement CUDA kernels |
| `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu` | Modify | Implement methods, allocate/free buffers |
| `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp` | Modify | Add `-prompt` and `-class` CLI arguments |
| `SAM3TensorRT/cpp/CMakeLists.txt` | Modify | Add OpenCV CUDA include path |

## Implementation Phases

### Phase 1: Core Implementation
1. Add data structures to `sam3.hpp`
2. Implement CUDA kernels in `prepost.cu`
3. Add buffer allocation in `SAM3_PCS` constructor/destructor
4. Implement `extract_bounding_boxes()` with CUDA kernel backend

### Phase 2: YOLO Output
1. Implement `save_yolo_labels()` method
2. Add `load_class_id_from_file()` utility reading `scripts/training/predefined_classes.txt`
3. Update CLI argument parsing for `-prompt` and `-class`

### Phase 3: Alternative Backends (Optional)
1. Implement OpenCV CUDA backend
2. Implement OpenCV CPU backend as fallback

## Testing

### Unit Tests
1. **Test: Sigmoid computation**
   - Input: Known logit values (e.g., 0.0, 1.0, -1.0)
   - Expected: Correct sigmoid values (0.5, 0.731, 0.269)

2. **Test: Bbox from simple mask**
   - Input: 288x288 mask with single rectangle (50,50) to (100,100) at prob=1.0
   - Expected: bbox = (50, 50, 50, 50) in mask coords

3. **Test: Coordinate scaling**
   - Input: mask bbox (144, 144, 28, 28) at 288x288, image size 1920x1080
   - Expected: image bbox ≈ (960, 540, 186, 105)

### Integration Tests
1. **Test: Full pipeline with known image**
   - Input: Test image with person, prompt "person", class "player"
   - Expected: YOLO label file created with correct format

2. **Test: Empty detection**
   - Input: Image with no matching objects
   - Expected: Empty vector returned, no label file created

### Performance Tests
1. **Benchmark: CUDA kernel extraction**
   - Input: 200 instances of 288x288 masks
   - Expected: < 2ms total extraction time

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| OpenCV CUDA build compatibility | Medium | Test with provided OpenCV build path |
| Memory leaks in new buffers | Medium | Add comprehensive destructor cleanup |
| Class mapping errors | Low | Validate class names against predefined_classes.txt, return error code |
| Coordinate scaling errors | Medium | Test with various image sizes, verify math |
| CUDA allocation failure | Medium | Wrap allocations in try-catch with clear error message |

## Success Criteria

1. Bounding boxes extracted correctly from instance masks
2. YOLO labels saved in correct format and location
3. Extraction completes in < 2ms per image
4. No memory leaks (verified via cuda-memcheck or similar)
5. CLI works with `-prompt` and `-class` arguments
6. All edge cases handled gracefully (empty detections, missing files, etc.)