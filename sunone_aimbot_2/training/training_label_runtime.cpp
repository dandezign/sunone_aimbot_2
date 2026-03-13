#define _CRT_SECURE_NO_WARNINGS
#include "sunone_aimbot_2/training/training_label_runtime.h"
#include <opencv2/opencv.hpp>

namespace training {

bool SaveQueue::Enqueue(const QueuedSaveRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.size() >= kMaxQueuedSaves) {
        status_ = "Save queue full";
        return false;
    }
    
    queue_.push(request);
    status_ = "Queued " + std::to_string(queue_.size()) + " saves";
    return true;
}

bool SaveQueue::Dequeue(QueuedSaveRequest& outRequest) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return false;
    }
    
    outRequest = queue_.front();
    queue_.pop();
    return true;
}

QueuedSaveRequest SaveQueue::PeekForTests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return QueuedSaveRequest{};
    }
    
    return queue_.front();
}

std::string SaveQueue::GetStatusForTests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

size_t SaveQueue::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool SaveQueue::IsFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() >= kMaxQueuedSaves;
}

RuntimeManager::RuntimeManager(DatasetManager& datasetMgr)
    : datasetMgr_(datasetMgr) {
}

bool RuntimeManager::EnqueueSave(const QueuedSaveRequest& request) {
    return queue_.Enqueue(request);
}

bool RuntimeManager::ProcessNextSave() {
    QueuedSaveRequest request;
    if (!queue_.Dequeue(request)) {
        return false;
    }
    
    std::vector<DetectionBox> boxes;
    datasetMgr_.WriteSample(
        request.frame,
        boxes,
        request.className,
        request.split,
        request.imageFormat,
        request.saveNegatives);
    
    return true;
}

void RuntimeManager::ProcessAllPending() {
    while (ProcessNextSave()) {
    }
}

std::string RuntimeManager::GetStatus() const {
    return queue_.GetStatusForTests();
}

size_t RuntimeManager::GetQueueSize() const {
    return queue_.Size();
}

QueuedSaveRequest RuntimeManager::PeekForTests() const {
    return queue_.PeekForTests();
}

std::string RuntimeManager::GetStatusForTests() const {
    return queue_.GetStatusForTests();
}

bool RuntimeManager::DequeueForTests(QueuedSaveRequest& outRequest) {
    return queue_.Dequeue(outRequest);
}

float GetFrameMeanForTests(const cv::Mat& frame) {
    if (frame.empty()) {
        return 0.0f;
    }
    cv::Scalar mean = cv::mean(frame);
    return (mean[0] + mean[1] + mean[2]) / 3.0f;
}

}  // namespace training
