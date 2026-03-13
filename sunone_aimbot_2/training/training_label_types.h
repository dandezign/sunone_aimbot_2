#pragma once

#include <string>
#include <opencv2/opencv.hpp>

namespace training {

enum class DatasetSplit { Train, Val };

struct QueuedSaveRequest {
    cv::Mat frame;
    std::string prompt;
    std::string className;
    DatasetSplit split = DatasetSplit::Train;
    bool saveNegatives = false;
    std::string imageFormat = ".jpg";
};

}  // namespace training
