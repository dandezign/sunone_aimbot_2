# SAM3 Bounding Box Extraction Design

**Date:** 2026-03-16  
**Status:** Draft  
**Author:** AI Assistant  

## Summary

Add bounding box extraction capability to the SAM3TensorRT C++ implementation. The system will extract bounding boxes from instance segmentation masks using a custom CUDA kernel, support YOLO format output for training data generation, and integrate with the existing training pipeline.

## Requirements

### Functional Requirements

1. **FR1:** Extract bounding boxes from SAM3 instance masks (up to 200 instances per image)
2. **FR2:** Support YOLO format output: `class_id x_center y_center width height`
3. **FR3:** Return bounding boxes in both original image coordinates and mask coordinates (288x288)
4. **FR4:** Filter detections by confidence threshold and minimum box area
5. **FR5:** Save YOLO label files to `scripts/training/datasets/game/labels/`
6. **FR6:** Support class name to ID mapping via `predefined_classes.txt`

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
// sam3.hpp - Updated struct
typedef struct {
    float score;           // Confidence from sigmoid of mask mean
    int class_id;          // Mapped from prompt/class argument
    
    // Bounding box in ORIGINAL IMAGE coordinates
    int box_x, box_y, box_w, box_h;
    
    // Bounding box in MASK coordinates (288x288 space)
    int mask_x, mask_y, mask_w, mask_h;
    
    // Optional: contour points for polygon export
    std::vector<int> contour_x;
    std::vector<int> contour_y;
} SAM3_PCS_RESULT;

// New enum for backend selection
typedef enum {
    BBOX_BACKEND_OPENCV_CPU,    // Fallback, simpler
    BBOX_BACKEND_OPENCV_CUDA,   // Uses cv::cuda::GpuMat
    BBOX_BACKEND_CUDA_KERNEL    // Custom CUDA kernel (fastest)
} SAM3_BBOX_BACKEND;

// New struct for extraction parameters
typedef struct {
    SAM3_BBOX_BACKEND backend = BBOX_BACKEND_CUDA_KERNEL;
    float score_threshold = 0.5f;      // Filter by confidence
    int min_box_area = 100;            // Filter tiny boxes (pixels^2)
    bool include_contours = false;     // Export polygon points
} SAM3_BBOX_OPTIONS;
```

### API Design

```cpp
class SAM3_PCS {
public:
    // Existing methods remain unchanged
    
    // New methods
    void set_class_id(int class_id);
    
    std::vector<SAM3_PCS_RESULT> extract_bounding_boxes(
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options = SAM3_BBOX_OPTIONS{}
    );
    
    bool save_yolo_labels(
        const std::string& image_path,
        const cv::Size& original_size,
        const SAM3_BBOX_OPTIONS& options = SAM3_BBOX_OPTIONS{}
    );

private:
    // New internal buffers
    int* d_mask_bboxes;        // Device: [200, 4] mask-space bboxes
    int* d_image_bboxes;       // Device: [200, 4] image-space bboxes
    float* d_instance_scores;  // Device: [200] confidence scores
    int* h_bbox_buffer;        // Host: [200, 4] for copying results
    float* h_score_buffer;     // Host: [200] for copying scores
    
    int _current_class_id = 0;
    
    // Backend implementations
    std::vector<SAM3_PCS_RESULT> extract_bboxes_cuda_kernel(...);
    std::vector<SAM3_PCS_RESULT> extract_bboxes_opencv_cuda(...);
    std::vector<SAM3_PCS_RESULT> extract_bboxes_opencv_cpu(...);
};
```

### CUDA Kernels

```cpp
// prepost.cuh - New kernel declarations

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

## File Changes

| File | Action | Changes |
|------|--------|---------|
| `cpp/include/sam3.hpp` | Modify | Add `SAM3_BBOX_BACKEND`, `SAM3_BBOX_OPTIONS`, update `SAM3_PCS_RESULT` |
| `cpp/include/sam3.cuh` | Modify | Add method declarations, new private members |
| `cpp/include/prepost.cuh` | Modify | Add kernel declarations |
| `cpp/src/sam3/sam3_trt/prepost.cu` | Modify | Implement CUDA kernels |
| `cpp/src/sam3/sam3_trt/sam3.cu` | Modify | Implement methods, allocate/free buffers |
| `cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp` | Modify | Add `-prompt` and `-class` CLI arguments |
| `cpp/CMakeLists.txt` | Modify | Add OpenCV CUDA include path |

## Implementation Phases

### Phase 1: Core Implementation
1. Add data structures to `sam3.hpp`
2. Implement CUDA kernels in `prepost.cu`
3. Add buffer allocation in `SAM3_PCS` constructor/destructor
4. Implement `extract_bounding_boxes()` with CUDA kernel backend

### Phase 2: YOLO Output
1. Implement `save_yolo_labels()` method
2. Add `load_class_id_from_file()` utility
3. Update CLI argument parsing for `-prompt` and `-class`

### Phase 3: Alternative Backends (Optional)
1. Implement OpenCV CUDA backend
2. Implement OpenCV CPU backend as fallback

## Testing

1. **Unit tests:** Verify bbox extraction against known mask patterns
2. **Integration tests:** Run full pipeline on test images, verify YOLO output format
3. **Performance tests:** Measure extraction time with CUDA kernel backend

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| OpenCV CUDA build compatibility | Medium | Test with provided OpenCV build path |
| Memory leaks in new buffers | Medium | Add comprehensive destructor cleanup |
| Class mapping errors | Low | Validate class names against predefined_classes.txt |
| Coordinate scaling errors | Medium | Test with various image sizes |

## Success Criteria

1. Bounding boxes extracted correctly from instance masks
2. YOLO labels saved in correct format and location
3. Extraction completes in < 2ms per image
4. No memory leaks
5. CLI works with `-prompt` and `-class` arguments