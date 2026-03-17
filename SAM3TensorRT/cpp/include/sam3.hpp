#pragma once

#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <vector>
#include <array>
#include <string>

typedef enum {
    TASK_PROMPTABLE_CONCEPT_SEGMENTATION,
    TASK_TRACKING
} SAM3_TASKS;

typedef enum {
    VIS_NONE,
    VIS_SEMANTIC_SEGMENTATION,
    VIS_INSTANCE_SEGMENTATION
} SAM3_VISUALIZATION;

typedef enum {
    BBOX_BACKEND_OPENCV_CPU,    // Fallback, simpler
    BBOX_BACKEND_OPENCV_CUDA,   // Uses cv::cuda::GpuMat
    BBOX_BACKEND_CUDA_KERNEL    // Custom CUDA kernel (fastest)
} SAM3_BBOX_BACKEND;

typedef struct {
    SAM3_BBOX_BACKEND backend = BBOX_BACKEND_CUDA_KERNEL;
    float score_threshold = 0.5f;      // Filter results by instance confidence
    int min_box_area = 100;            // Filter tiny boxes (pixels^2)
    float max_box_coverage = 0.8f;     // Filter boxes covering >80% of image
    bool include_contours = false;     // Export polygon points
} SAM3_BBOX_OPTIONS;

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

