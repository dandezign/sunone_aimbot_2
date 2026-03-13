#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include "sunone_aimbot_2/debug/detection_debug_export.h"
#include "sunone_aimbot_2/detector/yolo26_decode.h"
#include <array>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <opencv2/opencv.hpp>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
    detection_debug::ResetForTests();
    for (int i = 0; i < 300; ++i) {
        detection_debug::AppendEvent("event-" + std::to_string(i));
    }
    const auto snapshot = detection_debug::GetSharedStateSnapshot();
    Check(!snapshot.eventLog.empty(), "event log should not be empty");
    Check(snapshot.eventLog.size() <= detection_debug::kMaxDebugEvents, "event log should be capped");
    Check(snapshot.eventLog.back().find("event-299") != std::string::npos, "event log should keep latest event");

    detection_debug::ResetForTests();
    detection_debug::DetectorSnapshot debugSnapshot;
    debugSnapshot.backend = "TRT";
    debugSnapshot.outputShape = {1, 300, 6};
    detection_debug::PublishDetectorSnapshot(debugSnapshot);

    auto first = detection_debug::GetSharedStateSnapshot();
    first.detector.backend = "mutated";

    auto second = detection_debug::GetSharedStateSnapshot();
    Check(second.detector.backend == "TRT", "snapshot copy should be isolated");

    const auto bundleId = detection_debug::MakeBundleIdForTests("2026-03-13T21:10:44.182Z");
    Check(bundleId.find("yolo26_debug") != std::string::npos, "bundle id should include suffix");
    Check(detection_debug::GetDebugOutputRootForTests("I:/games/ai.exe") == std::filesystem::path("I:/games/debug"), "debug output root should use executable directory");

    detection_debug::BundleSnapshot bundleSnapshot;
    bundleSnapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
    bundleSnapshot.bundleId = bundleId;
    const auto jsonText = detection_debug::BuildBundleJson(bundleSnapshot);
    Check(jsonText.find("\"export_timestamp\"") != std::string::npos, "bundle json should include export timestamp");
    Check(jsonText.find("\"detector\"") != std::string::npos, "bundle json should include detector section");
    Check(jsonText.find("\"detections\"") != std::string::npos, "bundle json should include detections section");

    detection_debug::BundleSnapshot nullTestSnapshot;
    nullTestSnapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
    nullTestSnapshot.bundleId = "2026-03-13_21-10-44-182_yolo26_debug";
    const std::string json = detection_debug::BuildBundleJson(nullTestSnapshot);
    Check(json.find("\"output_shape\": [") != std::string::npos, "empty snapshot should serialize output shape");
    Check(json.find("\"x_gain\": null") != std::string::npos, "empty snapshot should serialize null gains");

    // Test MakeDetectionDebugEntry preserves raw and final box data
    {
        const cv::Rect finalBox(110, 90, 32, 64);
        const auto entry = detection_debug::MakeDetectionDebugEntry(0, 1, 0.91f, {220.0f, 180.0f, 284.0f, 308.0f}, finalBox);
        Check(entry.classId == 1, "debug entry should preserve class id");
        Check(entry.rawBox.x1 == 220.0f, "debug entry should preserve raw x1");
        Check(entry.finalBox.w == 32, "debug entry should preserve final width");
    }

    {
        std::array<float, 300 * 6> output{};
        output[0] = 485.25f;
        output[1] = 369.5f;
        output[2] = 533.0f;
        output[3] = 455.25f;
        output[4] = 0.888184f;
        output[5] = 0.0f;

        std::vector<detection_debug::RawBoxDebug> rawBoxes;
        const auto detections = yolo26::DecodeOutputs(
            output.data(), {1, 300, 6}, 3, 0.5f, 1.0f, 1.0f, &rawBoxes);

        Check(detections.size() == 1, "xyxy decode should keep valid detection");
        Check(rawBoxes.size() == 1, "xyxy decode should publish raw box");
        Check(rawBoxes[0].x1 == 485.25f, "raw box should preserve x1");
        Check(detections[0].box.x == 485, "decoded x should use x1");
        Check(detections[0].box.y == 369, "decoded y should use y1");
        Check(detections[0].box.width == 48, "decoded width should use x2 - x1");
        Check(detections[0].box.height == 86, "decoded height should use y2 - y1");
    }

    {
        std::array<float, 300 * 6> output{};
        output[0] = 40.0f;
        output[1] = 80.0f;
        output[2] = 39.0f;
        output[3] = 120.0f;
        output[4] = 0.9f;
        output[5] = 1.0f;

        const auto detections = yolo26::DecodeOutputs(
            output.data(), {1, 300, 6}, 3, 0.5f, 1.0f, 1.0f, nullptr);
        Check(detections.empty(), "decoder should reject x2 <= x1 boxes");
    }

    {
        detection_debug::BundleSnapshot snapshot;
        snapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
        snapshot.bundleId = "2026-03-13_21-10-44-182_yolo26_debug";
        snapshot.detector.boxConvention = "xyxy";
        snapshot.detector.detections.push_back(
            detection_debug::MakeDetectionDebugEntry(0, 2, 0.83f, {36.125f, 374.5f, 124.125f, 437.0f}, cv::Rect(36, 374, 88, 63)));
        const std::string jsonText = detection_debug::BuildBundleJson(snapshot);
        Check(jsonText.find("\"box_convention\": \"xyxy\"") != std::string::npos, "json should report xyxy convention");
        Check(jsonText.find("\"x1\": 36.125000") != std::string::npos, "json should serialize raw x1");
        Check(jsonText.find("\"x2\": 124.125000") != std::string::npos, "json should serialize raw x2");
    }

    {
        const auto entry = detection_debug::MakeDetectionDebugEntry(0, 2, 0.83f, {36.125f, 374.5f, 124.125f, 437.0f}, cv::Rect(36, 374, 88, 63));
        const std::string label = detection_debug::FormatDetectionLabel(entry, "xyxy");
        Check(label.find("cls=2") != std::string::npos, "label should include class id");
        Check(label.find("conf=0.83") != std::string::npos, "label should include confidence");
        Check(label.find("raw=") != std::string::npos, "label should include raw box");
        Check(label.find("final=") != std::string::npos, "label should include final box");
    }

    {
        detection_debug::ResetForTests();
        detection_debug::AppendEvent("existing-event");
        const auto snapshot = detection_debug::CaptureBundleSnapshot("[ManualExport] requested bundle export");
        Check(!snapshot.eventLog.empty(), "snapshot should include copied event log");
        Check(snapshot.eventLog.back().find("requested bundle export") != std::string::npos, "snapshot should include pending export event");
    }

    detection_debug::ResetForTests();
    detection_debug::TimedCaptureRequest request;
    request.intervalMs = 200;
    request.remainingShots = 3;
    request.prefix = "burst";
    detection_debug::StartTimedCapture(request);
    auto timedState = detection_debug::GetTimedCaptureState();
    Check(timedState.active, "timed capture should start active");
    Check(timedState.remainingShots == 3, "timed capture should keep requested count");
    Check(timedState.prefix == "burst", "timed capture should keep prefix");

    Check(detection_debug::ShouldTriggerTimedCapture(1000), "timed capture should trigger immediately");
    detection_debug::RecordTimedCaptureComplete();
    timedState = detection_debug::GetTimedCaptureState();
    Check(timedState.remainingShots == 2, "timed capture should decrement count after completion");
    Check(!detection_debug::ShouldTriggerTimedCapture(1100), "timed capture should wait for interval");
    Check(detection_debug::ShouldTriggerTimedCapture(1200), "timed capture should trigger after interval");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "detection_debug_tools_tests failed: " << e.what() << std::endl;
        return 1;
    }
}
