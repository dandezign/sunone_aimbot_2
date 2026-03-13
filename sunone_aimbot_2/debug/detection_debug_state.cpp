#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <algorithm>
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
TimedCaptureState g_timedCaptureState;
int64_t g_nextTimedCaptureAtMs = 0;
bool g_timedCaptureInFlight = false;
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

void StartTimedCapture(const TimedCaptureRequest& request) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_timedCaptureState.active = request.remainingShots > 0;
    g_timedCaptureState.intervalMs = std::max(1, request.intervalMs);
    g_timedCaptureState.remainingShots = std::max(0, request.remainingShots);
    g_timedCaptureState.completedShots = 0;
    g_timedCaptureState.prefix = request.prefix;
    g_timedCaptureState.lastStatus = g_timedCaptureState.active ? "Timed capture running" : "Timed capture idle";
    g_timedCaptureState.lastPath.clear();
    g_nextTimedCaptureAtMs = 0;
    g_timedCaptureInFlight = false;
}

void StopTimedCapture() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_timedCaptureState.active = false;
    g_timedCaptureState.remainingShots = 0;
    g_timedCaptureInFlight = false;
    if (g_timedCaptureState.lastStatus.empty()) {
        g_timedCaptureState.lastStatus = "Timed capture stopped";
    }
}

TimedCaptureState GetTimedCaptureState() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    return g_timedCaptureState;
}

bool ShouldTriggerTimedCapture(int64_t currentTimeMs) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    if (!g_timedCaptureState.active || g_timedCaptureState.remainingShots <= 0 || g_timedCaptureInFlight) {
        return false;
    }

    if (g_nextTimedCaptureAtMs == 0 || currentTimeMs >= g_nextTimedCaptureAtMs) {
        g_timedCaptureInFlight = true;
        g_nextTimedCaptureAtMs = currentTimeMs + g_timedCaptureState.intervalMs;
        return true;
    }

    return false;
}

void RecordTimedCaptureComplete() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    if (!g_timedCaptureInFlight) {
        return;
    }

    g_timedCaptureInFlight = false;
    if (g_timedCaptureState.remainingShots > 0) {
        --g_timedCaptureState.remainingShots;
        ++g_timedCaptureState.completedShots;
    }
    if (g_timedCaptureState.remainingShots <= 0) {
        g_timedCaptureState.active = false;
        if (g_timedCaptureState.lastStatus.empty() || g_timedCaptureState.lastStatus == "Timed capture running") {
            g_timedCaptureState.lastStatus = "Timed capture completed";
        }
    }
}

void SetTimedCaptureStatus(const std::string& status, const std::string& path) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_timedCaptureState.lastStatus = status;
    if (!path.empty()) {
        g_timedCaptureState.lastPath = path;
    }
}

void ResetForTests() {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_lastDetectorSnapshot = DetectorSnapshot{};
    g_eventLog.clear();
    g_timedCaptureState = TimedCaptureState{};
    g_nextTimedCaptureAtMs = 0;
    g_timedCaptureInFlight = false;
}

} // namespace detection_debug
