#define _CRT_SECURE_NO_WARNINGS
#include "sunone_aimbot_2/training/training_sam3_labeler.h"

namespace training {

struct Sam3Labeler::Impl {
    bool initialized = false;
};

Sam3Labeler::Sam3Labeler() : impl_(new Impl()) {
}

Sam3Labeler::~Sam3Labeler() {
    delete impl_;
}

Sam3Availability Sam3Labeler::GetAvailability() const {
    return GetAvailabilityForTests();
}

Sam3LabelResult Sam3Labeler::LabelFrame(const cv::Mat& frame, const std::string& prompt) {
    (void)frame;
    (void)prompt;
    
    Sam3LabelResult result;
    result.success = false;
    result.error = "SAM3 TensorRT backend not available (USE_CUDA not defined)";
    return result;
}

Sam3Availability Sam3Labeler::GetAvailabilityForTests() const {
    Sam3Availability avail;
    avail.ready = false;
    avail.message = "SAM3 stub backend (USE_CUDA not defined)";
    return avail;
}

}  // namespace training
