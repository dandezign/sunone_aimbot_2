# SAM3 Bounding Box Extraction Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add bounding box extraction to SAM3TensorRT C++ implementation with CUDA kernel backend and YOLO format output for training data generation.

**Architecture:** Custom CUDA kernels compute bounding boxes from instance mask logits using atomic min/max operations. Results are converted from (x_min, y_min, x_max, y_max) to (x, y, w, h) format on CPU. YOLO labels are saved to the existing training dataset structure.

**Tech Stack:** C++17, CUDA 13.1, TensorRT 10.x, OpenCV with CUDA support, CMake

---

## File Structure

| File | Purpose |
|------|---------|
| `SAM3TensorRT/cpp/include/sam3.hpp` | Public types: `SAM3_PCS_RESULT`, `SAM3_BBOX_BACKEND`, `SAM3_BBOX_OPTIONS` |
| `SAM3TensorRT/cpp/include/sam3.cuh` | `SAM3_PCS` class declaration with new methods and private buffers |
| `SAM3TensorRT/cpp/include/prepost.cuh` | CUDA kernel declarations for bbox extraction |
| `SAM3TensorRT/cpp/src/sam3/sam3_trt/prepost.cu` | CUDA kernel implementations |
| `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu` | Method implementations, buffer allocation |
| `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp` | CLI with `-prompt` and `-class` arguments |

> **Note on CMakeLists.txt:** No changes required if OpenCV is already configured. If the project fails to find OpenCV CUDA headers, ensure the OpenCV build at `I:\CppProjects\sunone_aimbot_2\sunone_aimbot_2\modules\opencv` is properly configured in the CMake cache.

---

## Chunk 1: Data Structures and Header Updates

### Task 1: Update sam3.hpp with new types

**Files:**
- Modify: `SAM3TensorRT/cpp/include/sam3.hpp`

- [ ] **Step 1: Add SAM3_BBOX_BACKEND enum after SAM3_VISUALIZATION enum**

```cpp
typedef enum {
    BBOX_BACKEND_OPENCV_CPU,    // Fallback, simpler
    BBOX_BACKEND_OPENCV_CUDA,   // Uses cv::cuda::GpuMat
    BBOX_BACKEND_CUDA_KERNEL    // Custom CUDA kernel (fastest)
} SAM3_BBOX_BACKEND;
```

- [ ] **Step 2: Add SAM3_BBOX_OPTIONS struct after the enum**

```cpp
typedef struct {
    SAM3_BBOX_BACKEND backend = BBOX_BACKEND_CUDA_KERNEL;
    float score_threshold = 0.5f;      // Filter results by instance confidence
    int min_box_area = 100;            // Filter tiny boxes (pixels^2)
    bool include_contours = false;     // Export polygon points
} SAM3_BBOX_OPTIONS;
```

- [ ] **Step 3: Update SAM3_PCS_RESULT struct to add new fields**

Replace the existing struct:

```cpp
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
```

- [ ] **Step 4: Verify header compiles**

Run: `cd SAM3TensorRT/cpp && cmake --build build --config Release --target sam3_trt 2>&1 | head -50`

Expected: Build succeeds or shows only unrelated errors (OpenCV path issues are expected if not configured)

- [ ] **Step 5: Commit**

```bash
git add SAM3TensorRT/cpp/include/sam3.hpp
git commit -m "feat(sam3): add bounding box types and extend SAM3_PCS_RESULT struct"
```

---

### Task 2: Update sam3.cuh with new method declarations

**Files:**
- Modify: `SAM3TensorRT/cpp/include/sam3.cuh`

- [ ] **Step 1: Add new public method declarations after `set_prompt()`**

```cpp
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
```

- [ ] **Step 2: Add new private member variables after `output_sizes`**

```cpp
    // Bounding box extraction buffers
    int* d_mask_bboxes;        // Device: [200, 4] mask-space bboxes
    int* d_image_bboxes;       // Device: [200, 4] image-space bboxes
    float* d_instance_scores;  // Device: [200] confidence scores
    int* h_bbox_buffer;        // Host: [200, 4] for copying results
    float* h_score_buffer;     // Host: [200] for copying scores
    
    int _current_class_id = 0;
```

- [ ] **Step 3: Add private method declarations before `visualize_on_dGPU`**

```cpp
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
```

- [ ] **Step 4: Add allocate_bbox_buffers() declaration in private section**

```cpp
    void allocate_bbox_buffers();
```

- [ ] **Step 5: Commit**

```bash
git add SAM3TensorRT/cpp/include/sam3.cuh
git commit -m "feat(sam3): add bounding box method declarations and buffers"
```

---

### Task 3: Update prepost.cuh with kernel declarations

**Files:**
- Modify: `SAM3TensorRT/cpp/include/prepost.cuh`

- [ ] **Step 1: Add kernel declarations after `draw_instance_seg_mask` declaration**

```cpp
__global__ void compute_bboxes_from_masks(
    const float* __restrict__ mask_logits,
    int* bbox_output,
    float* scores_output,
    int num_instances,
    int mask_width,
    int mask_height,
    float prob_threshold
);

__global__ void scale_bboxes_to_image(
    const int* mask_bboxes,
    int* image_bboxes,
    int num_instances,
    int mask_width,
    int mask_height,
    int image_width,
    int image_height
);
```

- [ ] **Step 2: Add constants for bbox extraction**

```cpp
#define SAM3_MAX_INSTANCES 200
#define SAM3_BBOX_COORDS 4  // x_min, y_min, x_max, y_max
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/include/prepost.cuh
git commit -m "feat(sam3): add bounding box kernel declarations"
```

---

## Chunk 2: CUDA Kernel Implementation

### Task 4: Implement compute_bboxes_from_masks kernel

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/prepost.cu`

- [ ] **Step 1: Add compute_bboxes_from_masks kernel implementation at end of file**

```cpp
__global__ void compute_bboxes_from_masks(
    const float* __restrict__ mask_logits,
    int* bbox_output,
    float* scores_output,
    int num_instances,
    int mask_width,
    int mask_height,
    float prob_threshold)
{
    // One block per instance
    int instance_idx = blockIdx.x;
    if (instance_idx >= num_instances) return;
    
    // Shared memory for block-level reduction
    __shared__ int s_x_min, s_y_min, s_x_max, s_y_max;
    __shared__ float s_score_sum;
    __shared__ int s_pixel_count;
    
    // Initialize shared memory (first thread in block)
    if (threadIdx.x == 0 && threadIdx.y == 0) {
        s_x_min = mask_width;
        s_y_min = mask_height;
        s_x_max = -1;
        s_y_max = -1;
        s_score_sum = 0.0f;
        s_pixel_count = 0;
    }
    __syncthreads();
    
    // Each thread processes one row of the mask
    int y = threadIdx.x;
    const int mask_size = mask_width * mask_height;
    const float* instance_mask = mask_logits + instance_idx * mask_size;
    
    for (int x = 0; x < mask_width; ++x) {
        int idx = y * mask_width + x;
        if (idx >= mask_size) continue;
        
        float logit = instance_mask[idx];
        float prob = 1.0f / (1.0f + expf(-logit));  // sigmoid
        
        // Accumulate score for all pixels
        atomicAdd(&s_score_sum, prob);
        atomicAdd(&s_pixel_count, 1);
        
        // Update bbox only for pixels above threshold
        if (prob >= prob_threshold) {
            atomicMin(&s_x_min, x);
            atomicMin(&s_y_min, y);
            atomicMax(&s_x_max, x);
            atomicMax(&s_y_max, y);
        }
    }
    __syncthreads();
    
    // Write results (first thread in block)
    if (threadIdx.x == 0 && threadIdx.y == 0) {
        // Check if any pixels were above threshold
        if (s_x_max >= 0) {
            bbox_output[instance_idx * 4 + 0] = s_x_min;
            bbox_output[instance_idx * 4 + 1] = s_y_min;
            bbox_output[instance_idx * 4 + 2] = s_x_max;
            bbox_output[instance_idx * 4 + 3] = s_y_max;
        } else {
            // No valid bbox - set to invalid values
            bbox_output[instance_idx * 4 + 0] = -1;
            bbox_output[instance_idx * 4 + 1] = -1;
            bbox_output[instance_idx * 4 + 2] = -1;
            bbox_output[instance_idx * 4 + 3] = -1;
        }
        
        // Compute mean score
        scores_output[instance_idx] = s_pixel_count > 0 ? 
            s_score_sum / static_cast<float>(s_pixel_count) : 0.0f;
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/prepost.cu
git commit -m "feat(sam3): implement compute_bboxes_from_masks CUDA kernel"
```

---

### Task 5: Implement scale_bboxes_to_image kernel

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/prepost.cu`

- [ ] **Step 1: Add scale_bboxes_to_image kernel implementation after compute_bboxes_from_masks**

```cpp
__global__ void scale_bboxes_to_image(
    const int* mask_bboxes,
    int* image_bboxes,
    int num_instances,
    int mask_width,
    int mask_height,
    int image_width,
    int image_height)
{
    int instance_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (instance_idx >= num_instances) return;
    
    int x_min = mask_bboxes[instance_idx * 4 + 0];
    int y_min = mask_bboxes[instance_idx * 4 + 1];
    int x_max = mask_bboxes[instance_idx * 4 + 2];
    int y_max = mask_bboxes[instance_idx * 4 + 3];
    
    // Check for invalid bbox
    if (x_min < 0) {
        image_bboxes[instance_idx * 4 + 0] = -1;
        image_bboxes[instance_idx * 4 + 1] = -1;
        image_bboxes[instance_idx * 4 + 2] = -1;
        image_bboxes[instance_idx * 4 + 3] = -1;
        return;
    }
    
    // Scale coordinates from mask space to image space
    image_bboxes[instance_idx * 4 + 0] = (x_min * image_width) / mask_width;
    image_bboxes[instance_idx * 4 + 1] = (y_min * image_height) / mask_height;
    image_bboxes[instance_idx * 4 + 2] = ((x_max + 1) * image_width) / mask_width - 1;
    image_bboxes[instance_idx * 4 + 3] = ((y_max + 1) * image_height) / mask_height - 1;
    
    // Clamp to image bounds
    image_bboxes[instance_idx * 4 + 0] = max(0, min(image_width - 1, image_bboxes[instance_idx * 4 + 0]));
    image_bboxes[instance_idx * 4 + 1] = max(0, min(image_height - 1, image_bboxes[instance_idx * 4 + 1]));
    image_bboxes[instance_idx * 4 + 2] = max(0, min(image_width - 1, image_bboxes[instance_idx * 4 + 2]));
    image_bboxes[instance_idx * 4 + 3] = max(0, min(image_height - 1, image_bboxes[instance_idx * 4 + 3]));
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/prepost.cu
git commit -m "feat(sam3): implement scale_bboxes_to_image CUDA kernel"
```

---

## Chunk 3: SAM3_PCS Class Implementation

### Task 6: Add buffer allocation in constructor

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

- [ ] **Step 1: Add allocate_bbox_buffers() method implementation before constructor**

```cpp
void SAM3_PCS::allocate_bbox_buffers() {
    const int num_instances = SAM3_MAX_INSTANCES;
    const size_t bbox_bytes = num_instances * SAM3_BBOX_COORDS * sizeof(int);
    const size_t score_bytes = num_instances * sizeof(float);
    
    // Allocate device buffers
    cuda_check(cudaMalloc(&d_mask_bboxes, bbox_bytes),
               "allocating d_mask_bboxes");
    cuda_check(cudaMalloc(&d_image_bboxes, bbox_bytes),
               "allocating d_image_bboxes");
    cuda_check(cudaMalloc(&d_instance_scores, score_bytes),
               "allocating d_instance_scores");
    
    // Allocate host buffers
    cuda_check(cudaHostAlloc(&h_bbox_buffer, bbox_bytes, cudaHostAllocDefault),
               "allocating h_bbox_buffer");
    cuda_check(cudaHostAlloc(&h_score_buffer, score_bytes, cudaHostAllocDefault),
               "allocating h_score_buffer");
    
    // Initialize to zero/invalid
    cudaMemset(d_mask_bboxes, -1, bbox_bytes);
    cudaMemset(d_image_bboxes, -1, bbox_bytes);
    cudaMemset(d_instance_scores, 0, score_bytes);
    memset(h_bbox_buffer, -1, bbox_bytes);
    memset(h_score_buffer, 0, score_bytes);
}
```

- [ ] **Step 2: Add buffer cleanup in destructor**

Update the destructor to free bbox buffers:

```cpp
SAM3_PCS::~SAM3_PCS() {
    // Existing cleanup
    for (auto &ptr : input_gpu) {
        if (ptr) {
            cudaFree(ptr);
        }
    }

    for (auto &ptr : output_gpu) {
        if (ptr) {
            cudaFree(ptr);
        }
    }
    
    // NEW: Bounding box buffer cleanup
    if (d_mask_bboxes) cudaFree(d_mask_bboxes);
    if (d_image_bboxes) cudaFree(d_image_bboxes);
    if (d_instance_scores) cudaFree(d_instance_scores);
    if (h_bbox_buffer) cudaFreeHost(h_bbox_buffer);
    if (h_score_buffer) cudaFreeHost(h_score_buffer);
}
```

- [ ] **Step 3: Call allocate_bbox_buffers() in constructor after allocate_io_buffers()**

Find the line `allocate_io_buffers();` in the constructor and add after it:

```cpp
  allocate_io_buffers();
  allocate_bbox_buffers();  // NEW
  setup_color_palette();
```

- [ ] **Step 4: Initialize pointers to nullptr**

Add initialization in constructor initializer list or at start:

```cpp
SAM3_PCS::SAM3_PCS(const std::string engine_path, const float vis_alpha,
                   const float prob_threshold)
    : _engine_path(engine_path), _overlay_alpha(vis_alpha),
      _probability_threshold(prob_threshold),
      d_mask_bboxes(nullptr), d_image_bboxes(nullptr),
      d_instance_scores(nullptr), h_bbox_buffer(nullptr),
      h_score_buffer(nullptr), _current_class_id(0) {
```

- [ ] **Step 5: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu
git commit -m "feat(sam3): add bounding box buffer allocation and cleanup"
```

---

### Task 7: Implement set_class_id() method

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

- [ ] **Step 1: Add set_class_id() implementation after set_prompt()**

```cpp
void SAM3_PCS::set_class_id(int class_id) {
    _current_class_id = class_id;
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu
git commit -m "feat(sam3): implement set_class_id() method"
```

---

### Task 8: Implement extract_bboxes_cuda_kernel() method

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

- [ ] **Step 1: Add extract_bboxes_cuda_kernel() implementation before destructor**

```cpp
std::vector<SAM3_PCS_RESULT> SAM3_PCS::extract_bboxes_cuda_kernel(
    const cv::Size& original_size,
    const SAM3_BBOX_OPTIONS& options)
{
    std::vector<SAM3_PCS_RESULT> results;
    
    // Check if inference has been run (output_gpu should have data)
    if (output_gpu.empty() || !output_gpu[0]) {
        std::cerr << "Warning: No inference results available. Call infer_on_image() first."
                  << std::endl;
        return results;
    }
    
    const int num_instances = SAM3_MAX_INSTANCES;
    const int mask_width = SAM3_OUTMASK_WIDTH;
    const int mask_height = SAM3_OUTMASK_HEIGHT;
    
    // Launch bbox computation kernel
    compute_bboxes_from_masks<<<num_instances, mask_height, 0, sam3_stream>>>(
        static_cast<float*>(output_gpu[0]),  // instance mask logits
        d_mask_bboxes,
        d_instance_scores,
        num_instances,
        mask_width,
        mask_height,
        _probability_threshold  // Use same threshold for binarization
    );
    
    // Launch scaling kernel
    int block_size = 256;
    int grid_size = (num_instances + block_size - 1) / block_size;
    scale_bboxes_to_image<<<grid_size, block_size, 0, sam3_stream>>>(
        d_mask_bboxes,
        d_image_bboxes,
        num_instances,
        mask_width,
        mask_height,
        original_size.width,
        original_size.height
    );
    
    // Copy results to host
    const size_t bbox_bytes = num_instances * SAM3_BBOX_COORDS * sizeof(int);
    const size_t score_bytes = num_instances * sizeof(float);
    
    cudaMemcpyAsync(h_bbox_buffer, d_image_bboxes, bbox_bytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
    cudaMemcpyAsync(h_score_buffer, d_instance_scores, score_bytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
    
    // Also copy mask-space bboxes for mask coordinates
    int* h_mask_bbox_temp;
    cudaHostAlloc(&h_mask_bbox_temp, bbox_bytes, cudaHostAllocDefault);
    cudaMemcpyAsync(h_mask_bbox_temp, d_mask_bboxes, bbox_bytes,
                    cudaMemcpyDeviceToHost, sam3_stream);
    
    cudaStreamSynchronize(sam3_stream);
    
    // Convert results to SAM3_PCS_RESULT format
    for (int i = 0; i < num_instances; ++i) {
        float score = h_score_buffer[i];
        
        // Filter by score threshold
        if (score < options.score_threshold) continue;
        
        // Get mask-space bbox
        int mask_x_min = h_mask_bbox_temp[i * 4 + 0];
        int mask_y_min = h_mask_bbox_temp[i * 4 + 1];
        int mask_x_max = h_mask_bbox_temp[i * 4 + 2];
        int mask_y_max = h_mask_bbox_temp[i * 4 + 3];
        
        // Skip invalid bboxes
        if (mask_x_min < 0) continue;
        
        // Get image-space bbox
        int img_x_min = h_bbox_buffer[i * 4 + 0];
        int img_y_min = h_bbox_buffer[i * 4 + 1];
        int img_x_max = h_bbox_buffer[i * 4 + 2];
        int img_y_max = h_bbox_buffer[i * 4 + 3];
        
        // Convert to x, y, w, h format
        int mask_w = mask_x_max - mask_x_min + 1;
        int mask_h = mask_y_max - mask_y_min + 1;
        int img_w = img_x_max - img_x_min + 1;
        int img_h = img_y_max - img_y_min + 1;
        
        // Filter by minimum box area
        if (img_w * img_h < options.min_box_area) continue;
        
        SAM3_PCS_RESULT result;
        result.score = score;
        result.class_id = _current_class_id;
        
        // Image coordinates
        result.box_x = img_x_min;
        result.box_y = img_y_min;
        result.box_w = img_w;
        result.box_h = img_h;
        
        // Mask coordinates
        result.mask_x = mask_x_min;
        result.mask_y = mask_y_min;
        result.mask_w = mask_w;
        result.mask_h = mask_h;
        
        results.push_back(result);
    }
    
    cudaFreeHost(h_mask_bbox_temp);
    
    return results;
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu
git commit -m "feat(sam3): implement extract_bboxes_cuda_kernel() method"
```

---

### Task 9: Implement extract_bounding_boxes() main method

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

- [ ] **Step 1: Add extract_bounding_boxes() implementation that dispatches to backends**

```cpp
std::vector<SAM3_PCS_RESULT> SAM3_PCS::extract_bounding_boxes(
    const cv::Size& original_size,
    const SAM3_BBOX_OPTIONS& options)
{
    switch (options.backend) {
        case BBOX_BACKEND_CUDA_KERNEL:
            return extract_bboxes_cuda_kernel(original_size, options);
        case BBOX_BACKEND_OPENCV_CUDA:
            return extract_bboxes_opencv_cuda(original_size, options);
        case BBOX_BACKEND_OPENCV_CPU:
            return extract_bboxes_opencv_cpu(original_size, options);
        default:
            std::cerr << "Warning: Unknown backend, using CUDA kernel" << std::endl;
            return extract_bboxes_cuda_kernel(original_size, options);
    }
}
```

- [ ] **Step 2: Add stub implementations for OpenCV backends (Phase 3)**

```cpp
std::vector<SAM3_PCS_RESULT> SAM3_PCS::extract_bboxes_opencv_cuda(
    const cv::Size& original_size,
    const SAM3_BBOX_OPTIONS& options)
{
    std::cerr << "Warning: OpenCV CUDA backend not yet implemented, "
              << "falling back to CUDA kernel" << std::endl;
    return extract_bboxes_cuda_kernel(original_size, options);
}

std::vector<SAM3_PCS_RESULT> SAM3_PCS::extract_bboxes_opencv_cpu(
    const cv::Size& original_size,
    const SAM3_BBOX_OPTIONS& options)
{
    std::cerr << "Warning: OpenCV CPU backend not yet implemented, "
              << "falling back to CUDA kernel" << std::endl;
    return extract_bboxes_cuda_kernel(original_size, options);
}
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu
git commit -m "feat(sam3): implement extract_bounding_boxes() with backend dispatch"
```

---

### Task 10: Implement save_yolo_labels() method

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

- [ ] **Step 1: Add includes at top of file**

```cpp
#include <fstream>
#include <iomanip>
```

- [ ] **Step 2: Add save_yolo_labels() implementation**

```cpp
bool SAM3_PCS::save_yolo_labels(
    const std::string& image_path,
    const cv::Size& original_size,
    const SAM3_BBOX_OPTIONS& options)
{
    // Extract bounding boxes
    auto detections = extract_bounding_boxes(original_size, options);
    
    if (detections.empty()) {
        return false;  // No detections
    }
    
    // Derive label file path from image path
    std::filesystem::path img_path(image_path);
    std::string label_filename = img_path.stem().string() + ".txt";
    
    // Use the training dataset labels directory
    std::filesystem::path label_dir = 
        std::filesystem::current_path() / "scripts" / "training" / 
        "datasets" / "game" / "labels";
    
    // Create directory if it doesn't exist
    std::filesystem::create_directories(label_dir);
    
    std::filesystem::path label_path = label_dir / label_filename;
    
    // Write YOLO format
    std::ofstream fout(label_path);
    if (!fout.is_open()) {
        std::cerr << "Error: Could not open label file for writing: " 
                  << label_path << std::endl;
        return false;
    }
    
    for (const auto& det : detections) {
        // Normalize to 0-1 range
        float x_center = (det.box_x + det.box_w / 2.0f) / original_size.width;
        float y_center = (det.box_y + det.box_h / 2.0f) / original_size.height;
        float width = static_cast<float>(det.box_w) / original_size.width;
        float height = static_cast<float>(det.box_h) / original_size.height;
        
        fout << det.class_id << " "
             << std::fixed << std::setprecision(6)
             << x_center << " " << y_center << " "
             << width << " " << height << "\n";
    }
    fout.close();
    
    std::cout << "Saved " << detections.size() << " detections to " 
              << label_path << std::endl;
    
    return true;
}
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu
git commit -m "feat(sam3): implement save_yolo_labels() method"
```

---

## Chunk 4: CLI Application Updates

### Task 11: Add load_class_id_from_file utility

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp`

- [ ] **Step 1: Add required includes at top of file**

```cpp
#include <fstream>  // For load_class_id_from_file
#include <filesystem>  // For path handling
```

- [ ] **Step 2: Add load_class_id_from_file() function in anonymous namespace**

Add after the existing `trim_copy` function (around line 63):

```cpp
int load_class_id_from_file(
    const std::string& class_name,
    const std::string& classes_file_path)
{
    std::ifstream file(classes_file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open classes file: " 
                  << classes_file_path << std::endl;
        return -1;
    }
    
    std::string line;
    int class_id = 0;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line = trim_copy(line);
        
        if (line.empty()) continue;
        
        if (line == class_name) {
            return class_id;
        }
        class_id++;
    }
    
    std::cerr << "Error: Class name '" << class_name 
              << "' not found in " << classes_file_path << std::endl;
    return -1;
}
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp
git commit -m "feat(sam3): add load_class_id_from_file() utility"
```

---

### Task 12: Update CLI argument parsing for -prompt and -class

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp`

- [ ] **Step 1: Update main() to parse -prompt and -class arguments**

Replace the argument parsing section (after `const std::string in_dir = argv[1];`) with:

```cpp
  const std::string in_dir = argv[1];
  std::string epath = argv[2];
  bool benchmark = false;
  std::string prompt_arg = "person";  // default
  std::string class_arg = "";         // empty = no YOLO labels
  
  for (int argi = 3; argi < argc; ++argi) {
    std::string arg = argv[argi];
    
    if (arg == "0" || arg == "1") {
      benchmark = (arg == "1");
      continue;
    }
    
    if (arg == "-prompt" && argi + 1 < argc) {
      prompt_arg = argv[++argi];
      continue;
    }
    
    if (arg == "-class" && argi + 1 < argc) {
      class_arg = argv[++argi];
      continue;
    }
    
    // Legacy: support old prompt= and ids: format
    if (arg.rfind("prompt=", 0) == 0 || arg.rfind("ids:", 0) == 0 ||
        arg == "person") {
      prompt_arg = arg;
      continue;
    }
    
    std::cout << "Ignoring unrecognized argument: " << arg << std::endl;
  }
```

- [ ] **Step 2: Add class ID resolution after prompt parsing**

Add after `pcs.set_prompt(iid, iam);`:

```cpp
  pcs.set_prompt(iid, iam);
  
  // Resolve class ID if -class was specified
  int class_id = -1;
  bool save_yolo = false;
  
  if (!class_arg.empty()) {
    std::string classes_path = "scripts/training/predefined_classes.txt";
    
    // Check if file exists relative to current directory
    if (!std::filesystem::exists(classes_path)) {
      // Try relative to executable location
      classes_path = std::filesystem::current_path().string() + 
                     "/scripts/training/predefined_classes.txt";
    }
    
    class_id = load_class_id_from_file(class_arg, classes_path);
    
    if (class_id < 0) {
      std::cerr << "Error: Could not resolve class '" << class_arg << "'" 
                << std::endl;
      return 1;
    }
    
    pcs.set_class_id(class_id);
    save_yolo = true;
    std::cout << "YOLO class '" << class_arg << "' mapped to ID " 
              << class_id << std::endl;
  }
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp
git commit -m "feat(sam3): add -prompt and -class CLI arguments"
```

---

### Task 13: Update infer_one_image to optionally save YOLO labels

**Files:**
- Modify: `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp`

- [ ] **Step 1: Update infer_one_image function signature**

Change the function signature to accept save_yolo parameter:

```cpp
void infer_one_image(SAM3_PCS &pcs, const cv::Mat &img, cv::Mat &result,
                      const SAM3_VISUALIZATION vis, const std::string outfile,
                      bool benchmark_run, bool save_yolo) {
```

- [ ] **Step 2: Add YOLO label saving logic in infer_one_image**

Update the function body:

```cpp
void infer_one_image(SAM3_PCS &pcs, const cv::Mat &img, cv::Mat &result,
                      const SAM3_VISUALIZATION vis, const std::string outfile,
                      bool benchmark_run, bool save_yolo) {
  bool success = pcs.infer_on_image(img, result, vis);

  if (benchmark_run) {
    return;
  }

  if (vis == SAM3_VISUALIZATION::VIS_NONE) {
    cv::Mat seg = cv::Mat(SAM3_OUTMASK_WIDTH, SAM3_OUTMASK_HEIGHT, CV_32FC1,
                          pcs.output_cpu[1]);
    // these are raw logits and should be passed through sigmoid before for any
    // quantitative use
  } else {
    cv::imwrite(outfile, result);
  }
  
  // Save YOLO labels if requested
  if (save_yolo) {
    SAM3_BBOX_OPTIONS bbox_opts;
    bbox_opts.backend = BBOX_BACKEND_CUDA_KERNEL;
    bbox_opts.score_threshold = 0.5f;
    bbox_opts.min_box_area = 100;
    
    pcs.save_yolo_labels(outfile, img.size(), bbox_opts);
  }
}
```

- [ ] **Step 3: Update the call to infer_one_image in main()**

Update the call to pass the save_yolo flag:

```cpp
      start = std::chrono::system_clock::now();
      infer_one_image(pcs, img, result, visualize, outfile.string(), benchmark, save_yolo);
```

- [ ] **Step 4: Commit**

```bash
git add SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp
git commit -m "feat(sam3): add YOLO label saving to infer_one_image"
```

- [ ] **Step 5: Verify CLI changes compile**

Run: `cd SAM3TensorRT/cpp && cmake --build build --config Release --target sam3_pcs_app 2>&1 | head -50`

Expected: Build succeeds or shows only unrelated errors

---

## Chunk 5: Build and Test

### Task 14: Build the project

- [ ] **Step 1: Configure CMake (if needed)**

```bash
cd SAM3TensorRT/cpp
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1"
```

- [ ] **Step 2: Build the project**

```bash
cmake --build build --config Release
```

Expected: Build succeeds without errors

- [ ] **Step 3: Verify executables exist**

```bash
dir build\Release\sam3_pcs_app.exe
```

Expected: File exists

---

### Task 15: Integration test

> **Note:** A SAM3 TensorRT engine file (.engine) is required for these tests. The engine is not generated by this plan - use an existing engine or generate one using the Python export scripts.

- [ ] **Step 1: Run basic inference (no YOLO output)**

```bash
cd SAM3TensorRT\cpp
build\Release\sam3_pcs_app.exe ..\demo\ path\to\engine.engine -prompt "person"
```

Expected: Inference runs, visualization saved to results\

- [ ] **Step 2: Run inference with YOLO label output**

```bash
build\Release\sam3_pcs_app.exe ..\demo\ path\to\engine.engine -prompt "person" -class "player"
```

Expected: 
- Inference runs
- YOLO label file created in `scripts\training\datasets\game\labels\`
- Label file format: `<class_id> <x_center> <y_center> <width> <height>`

- [ ] **Step 3: Verify label file format**

```bash
type scripts\training\datasets\game\labels\*.txt
```

Expected: Lines in YOLO format with 6 decimal precision

- [ ] **Step 4: Test error handling - unknown class**

```bash
build\Release\sam3_pcs_app.exe ..\demo\ path\to\engine.engine -prompt "person" -class "nonexistent_class"
```

Expected: Error message about unknown class, exit code 1

- [ ] **Step 5: Performance benchmark (optional)**

Run inference with timing to verify < 2ms bbox extraction:

```bash
build\Release\sam3_pcs_app.exe ..\demo\ path\to\engine.engine -prompt "person" -class "player" 1
```

The benchmark mode (last arg = 1) outputs timing per image. Bbox extraction should add < 2ms.

Expected: Total time comparable to baseline + ~1-2ms for bbox extraction

- [ ] **Step 6: Memory leak check (optional)**

If cuda-memcheck is available:

```bash
cuda-memcheck build\Release\sam3_pcs_app.exe ..\demo\ path\to\engine.engine -prompt "person"
```

Expected: No memory leaks detected

---

### Task 16: Final commit

- [ ] **Step 1: Verify all changes are committed**

```bash
git status
```

Expected: No uncommitted changes

- [ ] **Step 2: Create summary commit if needed**

```bash
git add -A
git commit -m "feat(sam3): complete bounding box extraction with YOLO output"
```

---

## Success Criteria Checklist

- [ ] `SAM3_PCS_RESULT` struct extended with class_id, mask coordinates, renamed contours
- [ ] `SAM3_BBOX_BACKEND` and `SAM3_BBOX_OPTIONS` types added
- [ ] CUDA kernels `compute_bboxes_from_masks` and `scale_bboxes_to_image` implemented
- [ ] `extract_bounding_boxes()` method works with CUDA kernel backend
- [ ] `save_yolo_labels()` writes YOLO format to correct directory
- [ ] CLI accepts `-prompt` and `-class` arguments
- [ ] Build succeeds without errors
- [ ] Integration test passes with YOLO label output