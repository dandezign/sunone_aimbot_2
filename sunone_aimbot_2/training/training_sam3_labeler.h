#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include "sunone_aimbot_2/training/training_dataset_manager.h"

namespace training {

struct Sam3Availability {
    bool ready;
    std::string message;
};

struct Sam3LabelResult {
    bool success;
    std::vector<DetectionBox> boxes;
    std::string error;
};

class Sam3Labeler {
public:
    Sam3Labeler();
    ~Sam3Labeler();
    
    Sam3Availability GetAvailability() const;
    Sam3LabelResult LabelFrame(const cv::Mat& frame, const std::string& prompt);
    
    Sam3Availability GetAvailabilityForTests() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace training
