#pragma once

#include "sunone_aimbot_2/training/training_label_types.h"
#include "sunone_aimbot_2/training/training_dataset_manager.h"
#include <queue>
#include <string>
#include <mutex>

namespace training {

constexpr size_t kMaxQueuedSaves = 10;

class SaveQueue {
public:
    bool Enqueue(const QueuedSaveRequest& request);
    bool Dequeue(QueuedSaveRequest& outRequest);
    QueuedSaveRequest PeekForTests() const;
    std::string GetStatusForTests() const;
    size_t Size() const;
    bool IsFull() const;

private:
    std::queue<QueuedSaveRequest> queue_;
    std::string status_;
    mutable std::mutex mutex_;
};

class RuntimeManager {
public:
    explicit RuntimeManager(DatasetManager& datasetMgr);
    
    bool EnqueueSave(const QueuedSaveRequest& request);
    bool ProcessNextSave();
    void ProcessAllPending();
    
    std::string GetStatus() const;
    size_t GetQueueSize() const;
    QueuedSaveRequest PeekForTests() const;
    std::string GetStatusForTests() const;
    bool DequeueForTests(QueuedSaveRequest& outRequest);

private:
    DatasetManager& datasetMgr_;
    SaveQueue queue_;
};

float GetFrameMeanForTests(const cv::Mat& frame);

}  // namespace training
