#pragma once

#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace detection_debug {

enum class ExportKind {
    RawFrame,
    AnnotatedFrame,
    Bundle,
    LogOnly,
};

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
BundleSnapshot CaptureBundleSnapshot(std::optional<std::string> pendingEvent);
bool QueueBundleExport(
    const BundleSnapshot& snapshot,
    std::string* outPath,
    std::string* outStatus,
    ExportKind kind = ExportKind::Bundle);
std::string BuildBundleJson(const BundleSnapshot& snapshot);
std::string FormatDetectionLabel(const DetectionDebugEntry& entry, const std::string& boxConvention);
std::string MakeBundleIdForTests(const std::string& utcTimestamp);
std::filesystem::path GetDebugOutputRootForTests(const std::filesystem::path& executablePath);

} // namespace detection_debug
