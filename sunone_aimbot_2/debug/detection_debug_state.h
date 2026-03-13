#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace detection_debug {

inline constexpr size_t kMaxDebugEvents = 256;

struct RawBoxDebug {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
};

struct FinalBoxDebug {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct DetectionDebugEntry {
    int index = 0;
    int classId = -1;
    float confidence = 0.0f;
    RawBoxDebug rawBox;
    FinalBoxDebug finalBox;
};

DetectionDebugEntry MakeDetectionDebugEntry(
    int index,
    int classId,
    float confidence,
    const RawBoxDebug& rawBox,
    const cv::Rect& finalBox);

struct DetectorSnapshot {
    std::string backend;
    std::string modelPath;
    std::string detectorTimestampUtc;
    std::optional<int> detectorInputWidth;
    std::optional<int> detectorInputHeight;
    std::optional<int> modelWidth;
    std::optional<int> modelHeight;
    std::vector<int64_t> outputShape;
    std::optional<float> xGain;
    std::optional<float> yGain;
    std::string boxConvention;
    std::vector<DetectionDebugEntry> detections;
};

struct SharedStateSnapshot {
    DetectorSnapshot detector;
    std::vector<std::string> eventLog;
};

struct TimedCaptureRequest {
    int intervalMs = 1000;
    int remainingShots = 1;
    std::string prefix;
};

struct TimedCaptureState {
    bool active = false;
    int intervalMs = 1000;
    int remainingShots = 0;
    int completedShots = 0;
    std::string prefix;
    std::string lastStatus;
    std::string lastPath;
};

void PublishDetectorSnapshot(const DetectorSnapshot& snapshot);
void AppendEvent(const std::string& message);
SharedStateSnapshot GetSharedStateSnapshot();
void StartTimedCapture(const TimedCaptureRequest& request);
void StopTimedCapture();
TimedCaptureState GetTimedCaptureState();
bool ShouldTriggerTimedCapture(int64_t currentTimeMs);
void RecordTimedCaptureComplete();
void SetTimedCaptureStatus(const std::string& status, const std::string& path = std::string());
void ResetForTests();
std::string MakeUtcTimestamp();

} // namespace detection_debug
