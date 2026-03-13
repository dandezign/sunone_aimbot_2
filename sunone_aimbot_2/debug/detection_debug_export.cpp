#include "sunone_aimbot_2/debug/detection_debug_export.h"
#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <mutex>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

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

namespace {

std::filesystem::path GetExecutablePath()
{
#ifdef _WIN32
    wchar_t exePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return std::filesystem::current_path();

    return std::filesystem::path(exePath);
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path GetDebugOutputRootPath()
{
    const std::filesystem::path executablePath = GetExecutablePath();
    if (executablePath.has_parent_path())
        return executablePath.parent_path() / "debug";

    return std::filesystem::current_path() / "debug";
}

} // namespace

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

std::filesystem::path GetDebugOutputRootForTests(const std::filesystem::path& executablePath)
{
    if (executablePath.has_parent_path())
        return executablePath.parent_path() / "debug";

    return std::filesystem::path("debug");
}

static std::string SerializeDetectorJson(const DetectorSnapshot& det) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
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
    json << std::fixed << std::setprecision(6);
    json << "[";
    for (size_t i = 0; i < det.detections.size(); ++i) {
        if (i > 0) json << ", ";
        const auto& entry = det.detections[i];
        json << "{\n";
        json << "        \"index\": " << entry.index << ",\n";
        json << "        \"class_id\": " << entry.classId << ",\n";
        json << "        \"confidence\": " << entry.confidence << ",\n";
        json << "        \"raw_box\": { \"x1\": " << entry.rawBox.x1
             << ", \"y1\": " << entry.rawBox.y1
             << ", \"x2\": " << entry.rawBox.x2
             << ", \"y2\": " << entry.rawBox.y2 << " },\n";
        json << "        \"final_box\": { \"x\": " << entry.finalBox.x
             << ", \"y\": " << entry.finalBox.y
             << ", \"w\": " << entry.finalBox.w
             << ", \"h\": " << entry.finalBox.h << " }\n";
        json << "      }";
    }
    json << "\n    ]";
    return json.str();
}

std::string FormatDetectionLabel(const DetectionDebugEntry& entry, const std::string& boxConvention) {
    std::ostringstream label;
    label << std::fixed << std::setprecision(2);
    label << "cls=" << entry.classId << " conf=" << entry.confidence;

    if (boxConvention == "xyxy") {
        label << " raw=(" << entry.rawBox.x1 << "," << entry.rawBox.y1
              << ")-(" << entry.rawBox.x2 << "," << entry.rawBox.y2 << ")";
    } else {
        label << " raw=(" << entry.rawBox.x1 << "," << entry.rawBox.y1
              << " " << entry.rawBox.x2 << "x" << entry.rawBox.y2 << ")";
    }

    label << " final=(" << entry.finalBox.x << "," << entry.finalBox.y
          << " " << entry.finalBox.w << "x" << entry.finalBox.h << ")";
    return label.str();
}

std::string BuildBundleJson(const BundleSnapshot& snapshot) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(6);
    json << "{\n";
    json << "  \"export_timestamp\": \"" << EscapeJson(snapshot.exportTimestampUtc) << "\",\n";
    json << "  \"bundle_id\": \"" << EscapeJson(snapshot.bundleId) << "\",\n";
    json << "  \"backend\": \"" << EscapeJson(snapshot.backend) << "\",\n";
    json << "  \"model\": \"" << EscapeJson(snapshot.modelPath) << "\",\n";
    if (snapshot.detectionResolution.has_value())
        json << "  \"detection_resolution\": " << *snapshot.detectionResolution << ",\n";
    else
        json << "  \"detection_resolution\": null,\n";
    if (snapshot.detectionBufferVersion.has_value())
        json << "  \"detection_buffer_version\": " << *snapshot.detectionBufferVersion << ",\n";
    else
        json << "  \"detection_buffer_version\": null,\n";
    
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
    const auto& debugDetections = snapshot.detector.detections;
    for (size_t i = 0; i < snapshot.boxes.size(); ++i) {
        const cv::Rect& box = snapshot.boxes[i];
        cv::rectangle(annotated, box, cv::Scalar(0, 0, 255), 2);
        cv::circle(annotated, cv::Point(box.x + box.width / 2, box.y + box.height / 2), 2, cv::Scalar(0, 255, 255), cv::FILLED);

        if (i >= debugDetections.size())
            continue;

        const std::string label = FormatDetectionLabel(debugDetections[i], snapshot.detector.boxConvention);
        int baseline = 0;
        const cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.4, 1, &baseline);
        const int labelX = (std::max)(0, box.x);
        const int labelBottom = (std::max)(textSize.height + baseline + 4, box.y - 4);
        const int labelTop = (std::max)(0, labelBottom - textSize.height - baseline - 6);
        const int labelRight = (std::min)(annotated.cols - 1, labelX + textSize.width + 6);
        cv::rectangle(
            annotated,
            cv::Rect(labelX, labelTop, (std::max)(1, labelRight - labelX), (std::max)(1, labelBottom - labelTop)),
            cv::Scalar(24, 24, 24),
            cv::FILLED);
        cv::putText(
            annotated,
            label,
            cv::Point(labelX + 3, labelBottom - baseline - 3),
            cv::FONT_HERSHEY_SIMPLEX,
            0.4,
            cv::Scalar(255, 255, 255),
            1,
            cv::LINE_AA);
    }
    return annotated;
}

BundleSnapshot CaptureBundleSnapshot() {
    return CaptureBundleSnapshot(std::nullopt);
}

BundleSnapshot CaptureBundleSnapshot(std::optional<std::string> pendingEvent) {
    BundleSnapshot snapshot;
#ifndef DETECTION_DEBUG_EXPORT_STUBS
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
    const auto timedCapture = GetTimedCaptureState();
    if (timedCapture.active && !timedCapture.prefix.empty()) {
        snapshot.bundleId = timedCapture.prefix + "_" + snapshot.bundleId;
    }

    if (pendingEvent.has_value() && !pendingEvent->empty())
        snapshot.eventLog.push_back(*pendingEvent);

#else
    const auto shared = GetSharedStateSnapshot();
    snapshot.detector = shared.detector;
    snapshot.eventLog = shared.eventLog;
    snapshot.exportTimestampUtc = MakeUtcTimestamp();
    snapshot.bundleId = MakeBundleIdForTests(snapshot.exportTimestampUtc);
    if (pendingEvent.has_value() && !pendingEvent->empty())
        snapshot.eventLog.push_back(*pendingEvent);
#endif
    return snapshot;
}

bool QueueBundleExport(
    const BundleSnapshot& snapshot,
    std::string* outPath,
    std::string* outStatus,
    ExportKind kind) {
    namespace fs = std::filesystem;

    const fs::path baseDir = GetDebugOutputRootPath();
    const fs::path bundleDir = baseDir / snapshot.bundleId;

    try {
        fs::create_directories(bundleDir);
    } catch (const std::exception& e) {
        if (outStatus) *outStatus = std::string("Failed to create bundle directory: ") + e.what();
        return false;
    }

    try {
        const fs::path rawPath = bundleDir / "frame_raw.png";
        const fs::path annotatedPath = bundleDir / "frame_annotated.png";
        const fs::path jsonPath = bundleDir / "detection_debug.json";
        const fs::path logPath = bundleDir / "debug_log.txt";

        if ((kind == ExportKind::RawFrame || kind == ExportKind::Bundle) && !snapshot.frame.empty()) {
            if (!cv::imwrite(rawPath.string(), snapshot.frame)) {
                if (outStatus) *outStatus = "Failed to write raw frame image";
                return false;
            }
        }

        if ((kind == ExportKind::AnnotatedFrame || kind == ExportKind::Bundle) && !snapshot.frame.empty()) {
            cv::Mat annotated = BuildAnnotatedFrame(snapshot);
            if (!cv::imwrite(annotatedPath.string(), annotated)) {
                if (outStatus) *outStatus = "Failed to write annotated frame image";
                return false;
            }
        }

        if (kind == ExportKind::Bundle) {
            std::string jsonContent = BuildBundleJson(snapshot);
            std::ofstream jsonFile(jsonPath);
            if (!jsonFile) {
                if (outStatus) *outStatus = "Failed to write JSON file";
                return false;
            }
            jsonFile << jsonContent;
            jsonFile.close();
        }

        if (kind == ExportKind::Bundle || kind == ExportKind::LogOnly) {
            std::ofstream logFile(logPath);
            if (!logFile) {
                if (outStatus) *outStatus = "Failed to write debug log";
                return false;
            }
            for (const auto& entry : snapshot.eventLog) {
                logFile << entry << "\n";
            }
            logFile.close();
        }
    } catch (const cv::Exception& e) {
        if (outStatus) *outStatus = std::string("OpenCV export failed: ") + e.what();
        return false;
    } catch (const std::exception& e) {
        if (outStatus) *outStatus = std::string("Debug export failed: ") + e.what();
        return false;
    }

    if (outPath) *outPath = bundleDir.string();
    if (outStatus) {
        switch (kind) {
        case ExportKind::RawFrame:
            *outStatus = snapshot.frame.empty() ? "Raw frame export skipped: no frame available" : "Raw frame export complete";
            break;
        case ExportKind::AnnotatedFrame:
            *outStatus = snapshot.frame.empty() ? "Annotated frame export skipped: no frame available" : "Annotated frame export complete";
            break;
        case ExportKind::LogOnly:
            *outStatus = "Debug log export complete";
            break;
        case ExportKind::Bundle:
        default:
            if (snapshot.frame.empty())
                *outStatus = "Bundle export complete (no frame available)";
            else if (snapshot.boxes.empty())
                *outStatus = "Bundle export complete (no detections available)";
            else
                *outStatus = "Export complete";
            break;
        }
    }
    return true;
}

} // namespace detection_debug
