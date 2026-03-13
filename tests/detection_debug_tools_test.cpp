#include "sunone_aimbot_2/debug/detection_debug_state.h"
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

    return 0;
}
