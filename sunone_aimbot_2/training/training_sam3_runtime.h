#pragma once

#include "sunone_aimbot_2/training/training_inference_mode.h"
#include "sunone_aimbot_2/training/training_dataset_manager.h"
#include "sunone_aimbot_2/training/training_label_types.h"
#include "sunone_aimbot_2/training/training_sam3_labeler.h"

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace training {

constexpr size_t kMaxTrainingRuntimeQueuedSaves = 10;

struct TrainingRuntimeSettings {
    std::string enginePath;
    std::string prompt;
    Sam3PostprocessSettings postprocess;
    bool previewEnabled = true;
    int previewIntervalMs = 0;
};

struct TrainingRuntimeStatus {
    InferenceMode activeMode = InferenceMode::Detect;
    Sam3Availability backend{false, "SAM3 backend inactive"};
    bool backendOwningInference = false;
    long long lastPreviewInferenceTimeMs = -1;
    size_t lastPreviewDetectionCount = 0;
    std::string lastSaveResult = "Save queue idle";
    std::string lastSavedImagePath;
    std::string lastSavedLabelPath;
};

struct Sam3PreviewOverlaySnapshot {
    cv::Mat frame;
    Sam3LabelResult result;
    uint64_t frameId = 0;
    bool valid = false;
};

class ISam3PreviewBackend {
public:
    virtual ~ISam3PreviewBackend() = default;

    virtual bool Initialize(const std::string& enginePath) = 0;
    virtual void RequestStop() = 0;
    virtual void Shutdown() = 0;
    virtual Sam3Availability GetAvailability() const = 0;
    virtual Sam3LabelResult LabelFrame(const Sam3InferenceRequest& request) = 0;
};

using Sam3PreviewBackendFactory = std::function<std::unique_ptr<ISam3PreviewBackend>()>;
using Sam3SaveProcessor = std::function<SaveResult(
    DatasetManager&,
    const QueuedSaveRequest&,
    const std::vector<DetectionBox>&)>;

class TrainingSam3Runtime {
public:
    explicit TrainingSam3Runtime(Sam3PreviewBackendFactory backendFactory);
    explicit TrainingSam3Runtime(DatasetManager* datasetMgr = nullptr,
                                 Sam3PreviewBackendFactory backendFactory = {},
                                 Sam3SaveProcessor saveProcessor = {});
    ~TrainingSam3Runtime();
    TrainingSam3Runtime(const TrainingSam3Runtime&) = delete;
    TrainingSam3Runtime& operator=(const TrainingSam3Runtime&) = delete;

    void UpdateMode(InferenceMode mode);
    void UpdateSettings(const TrainingRuntimeSettings& settings);
    void SubmitPreviewFrame(const Sam3InferenceRequest& request);
    bool EnqueueSave(const QueuedSaveRequest& request);
    TrainingRuntimeStatus GetStatus() const;
    Sam3LabelResult GetLatestPreviewResult() const;
    Sam3PreviewOverlaySnapshot GetLatestPreviewOverlaySnapshot() const;
    void InvalidatePreviewSnapshot();
    void Shutdown();

private:
    void InvalidatePreviewStateLocked();
    void ResetPreviewStateLocked();
    void ResetSaveStateLocked();
    void WaitForBackendCallToFinish();
    void WaitForSaveWriteToFinishLocked(std::unique_lock<std::mutex>& lock);
    void WorkerLoop();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable backendCallCv_;
    std::condition_variable saveWriteCv_;
    Sam3PreviewBackendFactory backendFactory_;
    Sam3SaveProcessor saveProcessor_;
    DatasetManager* datasetMgr_ = nullptr;
    std::shared_ptr<ISam3PreviewBackend> backend_;
    TrainingRuntimeSettings settings_;
    TrainingRuntimeStatus status_;
    Sam3LabelResult latestPreviewResult_;
    Sam3PreviewOverlaySnapshot latestPreviewOverlaySnapshot_;
    Sam3InferenceRequest pendingPreviewRequest_;
    std::queue<QueuedSaveRequest> saveQueue_;
    std::thread workerThread_;
    bool stopRequested_ = false;
    bool initRequested_ = false;
    bool previewRequested_ = false;
    bool backendCallInFlight_ = false;
    bool saveWriteInFlight_ = false;
    bool shuttingDown_ = false;
    std::uint64_t generation_ = 0;
    std::uint64_t previewPublicationToken_ = 0;
    std::chrono::steady_clock::time_point nextPreviewAt_{};
};

bool ShouldRouteFrameToTrainingRuntime(InferenceMode mode);
bool ShouldReinitializeSam3OnEnginePathChange(InferenceMode mode);
bool EnginePathChangeDropsOldOwnershipBeforeReinitForTests();
bool BlockedInitCancelsOnModeChangeForTests();
bool BlockedInitCancelsOnEnginePathChangeForTests();
void InvalidateTrainingPreviewSnapshot();

TrainingRuntimeStatus GetTrainingRuntimeStatus();
Sam3LabelResult GetTrainingLatestPreviewResult();
Sam3PreviewOverlaySnapshot GetTrainingLatestPreviewOverlaySnapshot();
void EnsureTrainingRuntimeInitialized(DatasetManager* datasetMgr);
void UpdateTrainingRuntimeSettings(const TrainingRuntimeSettings& settings);
void UpdateTrainingRuntimeMode(InferenceMode mode);
void SubmitTrainingPreviewFrame(const Sam3InferenceRequest& request);
bool EnqueueTrainingSave(const QueuedSaveRequest& request);
void ShutdownTrainingRuntime();

Sam3PresetLoader* GetTrainingPresetLoader();

}  // namespace training
