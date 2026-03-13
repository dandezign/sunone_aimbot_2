#pragma once

#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>

namespace detection_debug {

struct BundleSnapshot {
    std::string exportTimestampUtc;
    std::string bundleId;
    std::string frameTimestampUtc;
    cv::Mat frame;
    std::vector<cv::Rect> boxes;
    std::vector<int> classes;
    std::optional<int> detectionBufferVersion;
    DetectorSnapshot detector;
    std::vector<std::string> eventLog;
    std::string backend;
    std::string modelPath;
    std::optional<int> detectionResolution;
};

BundleSnapshot CaptureBundleSnapshot();
bool QueueBundleExport(const BundleSnapshot& snapshot, std::string* outPath, std::string* outStatus);
std::string BuildBundleJson(const BundleSnapshot& snapshot);
std::string MakeBundleIdForTests(const std::string& utcTimestamp);

} // namespace detection_debug
