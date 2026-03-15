#pragma once

#include <cstdint>
#include <string>
#include <opencv2/opencv.hpp>

namespace training {

enum class DatasetSplit { Train, Val };

struct Sam3PostprocessSettings {
    float maskThreshold = 0.5f;
    int minMaskPixels = 64;
    float minConfidence = 0.3f;
    int minBoxWidth = 20;
    int minBoxHeight = 20;
    float minMaskFillRatio = 0.01f;
    int maxDetections = 100;
};

struct Sam3InferenceRequest {
    cv::Mat frame;
    std::string prompt;
    Sam3PostprocessSettings postprocess;
    uint64_t frameId = 0;
};

struct QueuedSaveRequest {
    cv::Mat frame;
    std::string prompt;
    std::string className;
    DatasetSplit split = DatasetSplit::Train;
    bool saveNegatives = false;
    std::string imageFormat = ".jpg";
    Sam3PostprocessSettings sam3Postprocess;
};

}  // namespace training
