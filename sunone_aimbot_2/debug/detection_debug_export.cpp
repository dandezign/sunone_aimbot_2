#include "sunone_aimbot_2/debug/detection_debug_export.h"
#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <mutex>
#include <filesystem>

#ifndef DETECTION_DEBUG_EXPORT_STUBS
#include "sunone_aimbot_2/capture/capture.h"
#include "sunone_aimbot_2/detector/detection_buffer.h"
#include "sunone_aimbot_2/config/config.h"

extern cv::Mat latestFrame;
extern std::mutex frameMutex;
extern DetectionBuffer detectionBuffer;
extern std::mutex configMutex;
extern Config config;
#endif

namespace detection_debug {

static std::string MakeUtcTimestamp() {
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

std::string MakeBundleIdForTests(const std::string& utcTimestamp) {
    std::string id = utcTimestamp;
    for (char& c : id) {
        if (c == 'T') c = '_';
        else if (c == ':') c = '-';
        else if (c == '.') c = '-';
        else if (c == 'Z') c = '\0';
    }
    id.resize(id.size() - 1);
    id += "_yolo26_debug";
    return id;
}

static std::string EscapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<int>(c);
                result += oss.str();
            } else {
                result += c;
            }
        }
    }
    return result;
}

static std::string SerializeDetectorJson(const DetectorSnapshot& det) {
    std::ostringstream json;
    json << "{\n";
    json << "      \"timestamp\": \"" << EscapeJson(det.detectorTimestampUtc) << "\",\n";
    
    if (det.modelWidth.has_value() && det.modelHeight.has_value()) {
        json << "      \"input_size\": { \"width\": " << det.detectorInputWidth.value_or(0)
             << ", \"height\": " << det.detectorInputHeight.value_or(0) << " },\n";
        json << "      \"model_size\": { \"width\": " << det.modelWidth.value()
             << ", \"height\": " << det.modelHeight.value() << " },\n";
    } else {
        json << "      \"input_size\": null,\n";
        json << "      \"model_size\": null,\n";
    }
    
    json << "      \"output_shape\": [";
    for (size_t i = 0; i < det.outputShape.size(); ++i) {
        if (i > 0) json << ", ";
        json << det.outputShape[i];
    }
    json << "],\n";
    
    if (det.xGain.has_value()) {
        json << "      \"x_gain\": " << det.xGain.value() << ",\n";
    } else {
        json << "      \"x_gain\": null,\n";
    }
    
    if (det.yGain.has_value()) {
        json << "      \"y_gain\": " << det.yGain.value() << ",\n";
    } else {
        json << "      \"y_gain\": null,\n";
    }
    
    json << "      \"box_convention\": \"" << EscapeJson(det.boxConvention) << "\"\n";
    json << "    }";
    return json.str();
}

static std::string SerializeDetectionsJson(const DetectorSnapshot& det) {
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < det.detections.size(); ++i) {
        if (i > 0) json << ", ";
        const auto& entry = det.detections[i];
        json << "{\n";
        json << "        \"index\": " << entry.index << ",\n";
        json << "        \"class_id\": " << entry.classId << ",\n";
        json << "        \"confidence\": " << entry.confidence << ",\n";
        json << "        \"raw_box\": { \"x\": " << entry.rawBox.x
             << ", \"y\": " << entry.rawBox.y
             << ", \"w\": " << entry.rawBox.w
             << ", \"h\": " << entry.rawBox.h << " },\n";
        json << "        \"final_box\": { \"x\": " << entry.finalBox.x
             << ", \"y\": " << entry.finalBox.y
             << ", \"w\": " << entry.finalBox.w
             << ", \"h\": " << entry.finalBox.h << " }\n";
        json << "      }";
    }
    json << "\n    ]";
    return json.str();
}

std::string BuildBundleJson(const BundleSnapshot& snapshot) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    json << "{\n";
    json << "  \"export_timestamp\": \"" << EscapeJson(snapshot.exportTimestampUtc) << "\",\n";
    json << "  \"bundle_id\": \"" << EscapeJson(snapshot.bundleId) << "\",\n";
    json << "  \"backend\": \"" << EscapeJson(snapshot.backend) << "\",\n";
    json << "  \"model\": \"" << EscapeJson(snapshot.modelPath) << "\",\n";
    
    json << "  \"frame\": {\n";
    json << "    \"width\": " << snapshot.frame.cols << ",\n";
    json << "    \"height\": " << snapshot.frame.rows << ",\n";
    json << "    \"timestamp\": \"" << EscapeJson(snapshot.frameTimestampUtc) << "\"\n";
    json << "  },\n";
    
    json << "  \"detector\": " << SerializeDetectorJson(snapshot.detector) << ",\n";
    
    json << "  \"detections\": " << SerializeDetectionsJson(snapshot.detector) << ",\n";
    
    json << "  \"event_log\": [";
    for (size_t i = 0; i < snapshot.eventLog.size(); ++i) {
        if (i > 0) json << ", ";
        json << "\"" << EscapeJson(snapshot.eventLog[i]) << "\"";
    }
    json << "]\n";
    
    json << "}\n";
    return json.str();
}

static cv::Mat BuildAnnotatedFrame(const BundleSnapshot& snapshot) {
    cv::Mat annotated = snapshot.frame.clone();
    for (size_t i = 0; i < snapshot.boxes.size(); ++i) {
        const cv::Rect& box = snapshot.boxes[i];
        cv::rectangle(annotated, box, cv::Scalar(0, 0, 255), 2);
        cv::circle(annotated, cv::Point(box.x + box.width / 2, box.y + box.height / 2), 2, cv::Scalar(0, 255, 255), cv::FILLED);
    }
    return annotated;
}

BundleSnapshot CaptureBundleSnapshot() {
#ifndef DETECTION_DEBUG_EXPORT_STUBS
    BundleSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        if (!latestFrame.empty()) {
            latestFrame.copyTo(snapshot.frame);
        }
        snapshot.frameTimestampUtc = MakeUtcTimestamp();
    }
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        snapshot.boxes = detectionBuffer.boxes;
        snapshot.classes = detectionBuffer.classes;
        snapshot.detectionBufferVersion = detectionBuffer.version;
    }
    {
        std::lock_guard<std::mutex> lock(configMutex);
        snapshot.backend = config.backend;
        snapshot.modelPath = config.ai_model;
        snapshot.detectionResolution = config.detection_resolution;
    }

    const auto shared = GetSharedStateSnapshot();
    snapshot.detector = shared.detector;
    snapshot.eventLog = shared.eventLog;
    snapshot.exportTimestampUtc = MakeUtcTimestamp();
    snapshot.bundleId = MakeBundleIdForTests(snapshot.exportTimestampUtc);
    return snapshot;
#else
    return BundleSnapshot{};
#endif
}

bool QueueBundleExport(const BundleSnapshot& snapshot, std::string* outPath, std::string* outStatus) {
    namespace fs = std::filesystem;
    
    const fs::path baseDir = "debug";
    const fs::path bundleDir = baseDir / snapshot.bundleId;
    
    try {
        fs::create_directories(bundleDir);
    } catch (const std::exception& e) {
        if (outStatus) *outStatus = std::string("Failed to create bundle directory: ") + e.what();
        return false;
    }
    
    const fs::path rawPath = bundleDir / "frame_raw.png";
    const fs::path annotatedPath = bundleDir / "frame_annotated.png";
    const fs::path jsonPath = bundleDir / "detection_debug.json";
    const fs::path logPath = bundleDir / "debug_log.txt";
    
    if (!snapshot.frame.empty()) {
        cv::imwrite(rawPath.string(), snapshot.frame);
        cv::Mat annotated = BuildAnnotatedFrame(snapshot);
        cv::imwrite(annotatedPath.string(), annotated);
    }
    
    std::string jsonContent = BuildBundleJson(snapshot);
    std::ofstream jsonFile(jsonPath);
    if (!jsonFile) {
        if (outStatus) *outStatus = "Failed to write JSON file";
        return false;
    }
    jsonFile << jsonContent;
    jsonFile.close();
    
    std::ofstream logFile(logPath);
    if (logFile) {
        for (const auto& entry : snapshot.eventLog) {
            logFile << entry << "\n";
        }
        logFile.close();
    }
    
    if (outPath) *outPath = bundleDir.string();
    if (outStatus) *outStatus = "Export complete";
    return true;
}

} // namespace detection_debug
