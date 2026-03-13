#include "sunone_aimbot_2/detector/yolo26_decode.h"

#include <algorithm>
#include <cmath>

namespace yolo26 {

namespace {

bool HasExpectedShape(const std::vector<int64_t>& shape) {
    const int64_t expectedSize = 300 * 6;
    const int64_t actualSize = shape.empty()
        ? expectedSize
        : (shape.size() == 1 ? shape[0] : shape[shape.size() - 2] * shape[shape.size() - 1]);
    return actualSize == expectedSize;
}

} // namespace

cv::Rect ScaleRawXyxyToRect(
    const detection_debug::RawBoxDebug& rawBox,
    float xGain,
    float yGain) {
    const int gameX1 = static_cast<int>(rawBox.x1 * xGain);
    const int gameY1 = static_cast<int>(rawBox.y1 * yGain);
    const int gameX2 = static_cast<int>(rawBox.x2 * xGain);
    const int gameY2 = static_cast<int>(rawBox.y2 * yGain);
    return cv::Rect(gameX1, gameY1, std::max(0, gameX2 - gameX1), std::max(0, gameY2 - gameY1));
}

std::vector<Detection> DecodeOutputs(
    const float* output,
    const std::vector<int64_t>& shape,
    int numClasses,
    float confThreshold,
    float xGain,
    float yGain,
    std::vector<detection_debug::RawBoxDebug>* rawBoxes) {
    std::vector<Detection> detections;
    if (!output || !HasExpectedShape(shape)) {
        return detections;
    }

    for (int i = 0; i < 300; ++i) {
        const float conf = output[i * 6 + 4];
        if (conf < confThreshold || std::isnan(conf) || std::isinf(conf)) {
            continue;
        }

        const detection_debug::RawBoxDebug rawBox{
            output[i * 6 + 0],
            output[i * 6 + 1],
            output[i * 6 + 2],
            output[i * 6 + 3],
        };
        const float classIdFloat = output[i * 6 + 5];

        if (std::isnan(rawBox.x1) || std::isnan(rawBox.y1) || std::isnan(rawBox.x2) || std::isnan(rawBox.y2) ||
            std::isinf(rawBox.x1) || std::isinf(rawBox.y1) || std::isinf(rawBox.x2) || std::isinf(rawBox.y2) ||
            rawBox.x1 < 0.0f || rawBox.y1 < 0.0f || rawBox.x2 <= rawBox.x1 || rawBox.y2 <= rawBox.y1) {
            continue;
        }

        const int classId = static_cast<int>(classIdFloat);
        if (classId < 0 || classId >= numClasses) {
            continue;
        }

        const cv::Rect box = ScaleRawXyxyToRect(rawBox, xGain, yGain);
        if (box.width <= 0 || box.height <= 0)
            continue;

        detections.push_back({box, conf, classId});
        if (rawBoxes) {
            rawBoxes->push_back(rawBox);
        }
    }

    return detections;
}

} // namespace yolo26
