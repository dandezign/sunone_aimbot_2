#define _CRT_SECURE_NO_WARNINGS
#include "sunone_aimbot_2/training/training_sam3_labeler.h"

namespace training {

struct Sam3Labeler::Impl {
    bool initialized = false;
    std::string availabilityMessage = "SAM3 stub backend (USE_CUDA not defined)";
    std::atomic<bool> stopRequested{false};
};

Sam3Labeler::Sam3Labeler() : impl_(new Impl()) {
}

Sam3Labeler::~Sam3Labeler() {
    Shutdown();
    delete impl_;
}

bool Sam3Labeler::Initialize(const std::string& enginePath) {
    (void)enginePath;
    impl_->stopRequested.store(false);
    impl_->initialized = false;
    impl_->availabilityMessage = "SAM3 TensorRT backend not available (USE_CUDA not defined)";
    return false;
}

void Sam3Labeler::RequestStop() {
    impl_->stopRequested.store(true);
}

void Sam3Labeler::Shutdown() {
    impl_->stopRequested.store(true);
    impl_->initialized = false;
    impl_->availabilityMessage = "SAM3 stub backend (USE_CUDA not defined)";
}

Sam3Availability Sam3Labeler::GetAvailability() const {
    return GetAvailabilityForTests();
}

Sam3LabelResult Sam3Labeler::LabelFrame(const Sam3InferenceRequest& request) {
    (void)request;

    Sam3LabelResult result;
    result.success = false;
    result.error = impl_->stopRequested.load() ? "SAM3 preview canceled" : impl_->availabilityMessage;
    return result;
}

Sam3Availability Sam3Labeler::GetAvailabilityForTests() const {
    Sam3Availability avail;
    avail.ready = impl_->initialized;
    avail.message = impl_->availabilityMessage;
    return avail;
}

}  // namespace training
