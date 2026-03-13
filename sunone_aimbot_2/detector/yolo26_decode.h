#pragma once

#include <cstdint>
#include <vector>

#include <opencv2/opencv.hpp>

#include "sunone_aimbot_2/debug/detection_debug_state.h"
#include "sunone_aimbot_2/detector/detection_buffer.h"

namespace yolo26 {

cv::Rect ScaleRawXyxyToRect(
    const detection_debug::RawBoxDebug& rawBox,
    float xGain,
    float yGain);

std::vector<Detection> DecodeOutputs(
    const float* output,
    const std::vector<int64_t>& shape,
    int numClasses,
    float confThreshold,
    float xGain,
    float yGain,
    std::vector<detection_debug::RawBoxDebug>* rawBoxes = nullptr);

} // namespace yolo26
