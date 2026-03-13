#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include "sunone_aimbot_2/debug/detection_debug_export.h"
#include <cassert>
#include <string>

int main() {
    detection_debug::ResetForTests();
    for (int i = 0; i < 300; ++i) {
        detection_debug::AppendEvent("event-" + std::to_string(i));
    }
    const auto snapshot = detection_debug::GetSharedStateSnapshot();
    assert(!snapshot.eventLog.empty());
    assert(snapshot.eventLog.size() <= detection_debug::kMaxDebugEvents);
    assert(snapshot.eventLog.back().find("event-299") != std::string::npos);

    detection_debug::ResetForTests();
    detection_debug::DetectorSnapshot debugSnapshot;
    debugSnapshot.backend = "TRT";
    debugSnapshot.outputShape = {1, 300, 6};
    detection_debug::PublishDetectorSnapshot(debugSnapshot);

    auto first = detection_debug::GetSharedStateSnapshot();
    first.detector.backend = "mutated";

    auto second = detection_debug::GetSharedStateSnapshot();
    assert(second.detector.backend == "TRT");

    const auto bundleId = detection_debug::MakeBundleIdForTests("2026-03-13T21:10:44.182Z");
    assert(bundleId.find("yolo26_debug") != std::string::npos);

    detection_debug::BundleSnapshot bundleSnapshot;
    bundleSnapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
    bundleSnapshot.bundleId = bundleId;
    const auto jsonText = detection_debug::BuildBundleJson(bundleSnapshot);
    assert(jsonText.find("\"export_timestamp\"") != std::string::npos);
    assert(jsonText.find("\"detector\"") != std::string::npos);
    assert(jsonText.find("\"detections\"") != std::string::npos);

    detection_debug::BundleSnapshot nullTestSnapshot;
    nullTestSnapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
    nullTestSnapshot.bundleId = "2026-03-13_21-10-44-182_yolo26_debug";
    const std::string json = detection_debug::BuildBundleJson(nullTestSnapshot);
    assert(json.find("\"output_shape\":[]") != std::string::npos);
    assert(json.find("\"x_gain\":null") != std::string::npos);

    return 0;
}
