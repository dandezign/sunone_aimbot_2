#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <deque>
#include <mutex>

namespace detection_debug {
namespace {
std::mutex g_debugMutex;
DetectorSnapshot g_lastDetectorSnapshot;
std::deque<std::string> g_eventLog;
} // namespace

void PublishDetectorSnapshot(const DetectorSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_lastDetectorSnapshot = snapshot;
}

void AppendEvent(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_eventLog.push_back(message);
    while (g_eventLog.size() > kMaxDebugEvents) {
        g_eventLog.pop_front();
    }
}

SharedStateSnapshot GetSharedStateSnapshot() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    SharedStateSnapshot snapshot;
    snapshot.detector = g_lastDetectorSnapshot;
    snapshot.eventLog.assign(g_eventLog.begin(), g_eventLog.end());
    return snapshot;
}

void ResetForTests() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_lastDetectorSnapshot = DetectorSnapshot{};
    g_eventLog.clear();
}

} // namespace detection_debug
