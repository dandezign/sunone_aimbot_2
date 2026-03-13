#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <deque>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace detection_debug {

std::string MakeUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_s(&tm_buf, &time);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

DetectionDebugEntry MakeDetectionDebugEntry(
    int index,
    int classId,
    float confidence,
    const RawBoxDebug& rawBox,
    const cv::Rect& finalBox) {
    DetectionDebugEntry entry;
    entry.index = index;
    entry.classId = classId;
    entry.confidence = confidence;
    entry.rawBox = rawBox;
    entry.finalBox.x = finalBox.x;
    entry.finalBox.y = finalBox.y;
    entry.finalBox.w = finalBox.width;
    entry.finalBox.h = finalBox.height;
    return entry;
}
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
