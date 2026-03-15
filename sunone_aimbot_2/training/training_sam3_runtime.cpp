#include "sunone_aimbot_2/training/training_sam3_runtime.h"

#include <chrono>

#include "sunone_aimbot_2/training/training_sam3_preset_loader.h"
#include "sunone_aimbot_2/sunone_aimbot_2.h"
#include "sunone_aimbot_2/include/other_tools.h"

namespace training {

namespace {

Sam3PresetLoader g_presetLoader;

SaveResult DefaultSaveProcessor(DatasetManager& datasetMgr,
                                const QueuedSaveRequest& request,
                                const std::vector<DetectionBox>& detections) {
    return datasetMgr.WriteSample(
        request.frame,
        detections,
        request.className,
        request.split,
        request.imageFormat,
        request.saveNegatives);
}

class DefaultSam3PreviewBackend final : public ISam3PreviewBackend {
public:
    bool Initialize(const std::string& enginePath) override {
        return labeler_.Initialize(enginePath);
    }

    void RequestStop() override {
        labeler_.RequestStop();
    }

    void Shutdown() override {
        labeler_.Shutdown();
    }

    Sam3Availability GetAvailability() const override {
        return labeler_.GetAvailability();
    }

    Sam3LabelResult LabelFrame(const Sam3InferenceRequest& request) override {
        return labeler_.LabelFrame(request);
    }

private:
    Sam3Labeler labeler_;
};

struct BlockingTestBackendState {
    std::mutex mutex;
    std::condition_variable cv;
    bool blockInitialize = false;
    std::string enginePath;
};

class BlockingTestBackend final : public ISam3PreviewBackend {
public:
    explicit BlockingTestBackend(std::shared_ptr<BlockingTestBackendState> state)
        : state_(std::move(state)) {
    }

    bool Initialize(const std::string& enginePath) override {
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [&]() { return !state_->blockInitialize || stopRequested_; });
        if (stopRequested_) {
            return false;
        }
        state_->enginePath = enginePath;
        return true;
    }

    void RequestStop() override {
        stopRequested_ = true;
        state_->cv.notify_all();
    }

    void Shutdown() override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->enginePath.clear();
    }

    Sam3Availability GetAvailability() const override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->enginePath.empty()) {
            return {false, "test backend idle"};
        }
        return {true, "ready:" + state_->enginePath};
    }

    Sam3LabelResult LabelFrame(const Sam3InferenceRequest&) override {
        Sam3LabelResult result;
        result.success = true;
        return result;
    }

    void SetBlockInitialize(bool block) {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->blockInitialize = block;
        }
        state_->cv.notify_all();
    }

private:
    std::shared_ptr<BlockingTestBackendState> state_;
    std::atomic<bool> stopRequested_{false};
};

bool WaitForConditionForRuntimeTests(const std::function<bool()>& predicate, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return predicate();
}

bool CanRunSam3Save(const TrainingRuntimeStatus& status, const std::shared_ptr<ISam3PreviewBackend>& backend) {
    return status.activeMode == InferenceMode::Label && backend != nullptr;
}

bool CanPublishPreviewResult(const TrainingRuntimeStatus& status,
                            const TrainingRuntimeSettings& settings,
                            const std::shared_ptr<ISam3PreviewBackend>& backend) {
    return status.activeMode == InferenceMode::Label && settings.previewEnabled && backend != nullptr;
}

}  // namespace

void TrainingSam3Runtime::InvalidatePreviewStateLocked() {
    ++previewPublicationToken_;
    previewRequested_ = false;
    ResetPreviewStateLocked();
}

bool ShouldRouteFrameToTrainingRuntime(InferenceMode mode) {
    return mode == InferenceMode::Label;
}

bool ShouldReinitializeSam3OnEnginePathChange(InferenceMode mode) {
    return mode == InferenceMode::Label;
}

void TrainingSam3Runtime::ResetPreviewStateLocked() {
    latestPreviewResult_ = Sam3LabelResult{};
    latestPreviewOverlaySnapshot_ = Sam3PreviewOverlaySnapshot{};
    pendingPreviewRequest_ = Sam3InferenceRequest{};
    status_.lastPreviewInferenceTimeMs = -1;
    status_.lastPreviewDetectionCount = 0;
    nextPreviewAt_ = std::chrono::steady_clock::time_point{};
}

void TrainingSam3Runtime::ResetSaveStateLocked() {
    saveQueue_ = std::queue<QueuedSaveRequest>{};
    status_.lastSaveResult = "Save queue idle";
}

void TrainingSam3Runtime::WaitForBackendCallToFinish() {
    std::unique_lock<std::mutex> lock(mutex_);
    backendCallCv_.wait(lock, [&]() { return !backendCallInFlight_; });
}

void TrainingSam3Runtime::WaitForSaveWriteToFinishLocked(std::unique_lock<std::mutex>& lock) {
    saveWriteCv_.wait(lock, [&]() { return !saveWriteInFlight_; });
}

TrainingSam3Runtime::TrainingSam3Runtime(Sam3PreviewBackendFactory backendFactory)
    : TrainingSam3Runtime(nullptr, std::move(backendFactory), {}) {
}

TrainingSam3Runtime::TrainingSam3Runtime(DatasetManager* datasetMgr,
                                         Sam3PreviewBackendFactory backendFactory,
                                         Sam3SaveProcessor saveProcessor)
    : backendFactory_(std::move(backendFactory)),
      saveProcessor_(std::move(saveProcessor)),
      datasetMgr_(datasetMgr) {
    if (!backendFactory_) {
        backendFactory_ = []() { return std::make_unique<DefaultSam3PreviewBackend>(); };
    }
    if (!saveProcessor_) {
        saveProcessor_ = DefaultSaveProcessor;
    }

    workerThread_ = std::thread(&TrainingSam3Runtime::WorkerLoop, this);
}

TrainingSam3Runtime::~TrainingSam3Runtime() {
    Shutdown();
}

void TrainingSam3Runtime::UpdateMode(InferenceMode mode) {
    std::shared_ptr<ISam3PreviewBackend> backendToShutdown;
    bool notifyWorker = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!ShouldRouteFrameToTrainingRuntime(mode)) {
            WaitForSaveWriteToFinishLocked(lock);
            status_.activeMode = mode;
            ++generation_;
            initRequested_ = false;
            InvalidatePreviewStateLocked();
            ResetSaveStateLocked();
            backendToShutdown = std::move(backend_);
            status_.backend = {false, "SAM3 backend inactive"};
            status_.backendOwningInference = false;
            notifyWorker = true;
        } else if (settings_.enginePath.empty()) {
            WaitForSaveWriteToFinishLocked(lock);
            status_.activeMode = mode;
            InvalidatePreviewStateLocked();
            ResetSaveStateLocked();
            status_.backend = {false, "SAM3 engine path not set"};
            status_.backendOwningInference = false;
        } else if (!backend_ && !initRequested_ && !backendCallInFlight_) {
            status_.activeMode = mode;
            ++generation_;
            initRequested_ = true;
            InvalidatePreviewStateLocked();
            ResetSaveStateLocked();
            status_.backend = {false, "Initializing SAM3 backend"};
            status_.backendOwningInference = false;
            notifyWorker = true;
        } else {
            status_.activeMode = mode;
        }
    }

    if (backendToShutdown) {
        backendToShutdown->RequestStop();
        cv_.notify_all();
        WaitForBackendCallToFinish();
        backendToShutdown->Shutdown();
    }
    if (notifyWorker) {
        cv_.notify_all();
    }
}

void TrainingSam3Runtime::UpdateSettings(const TrainingRuntimeSettings& settings) {
    std::shared_ptr<ISam3PreviewBackend> backendToShutdown;
    bool notifyWorker = false;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool engineChanged = settings.enginePath != settings_.enginePath;
        const bool presetChanged = settings.prompt != settings_.prompt;

        const bool previewDisabled = settings_.previewEnabled && !settings.previewEnabled;

        if (engineChanged && ShouldReinitializeSam3OnEnginePathChange(status_.activeMode)) {
            WaitForSaveWriteToFinishLocked(lock);
            settings_ = settings;
            ++generation_;
            initRequested_ = !settings_.enginePath.empty();
            InvalidatePreviewStateLocked();
            ResetSaveStateLocked();
            backendToShutdown = std::move(backend_);
            status_.backend = settings_.enginePath.empty()
                ? Sam3Availability{false, "SAM3 engine path not set"}
                : Sam3Availability{false, "Reinitializing SAM3 backend"};
            status_.backendOwningInference = false;
            notifyWorker = initRequested_;
        } else if (presetChanged && status_.backend.ready) {
            WaitForSaveWriteToFinishLocked(lock);
            settings_ = settings;
            ++generation_;
            initRequested_ = true;
            InvalidatePreviewStateLocked();
            ResetSaveStateLocked();
            backendToShutdown = std::move(backend_);
            status_.backend = {false, "Reinitializing SAM3 backend on preset change"};
            status_.backendOwningInference = false;
            notifyWorker = true;
        } else {
            settings_ = settings;
            if (previewDisabled) {
                InvalidatePreviewStateLocked();
            }
            // Only request init if: in Label mode, engine path set, no backend exists, 
            // not already initializing, and not currently processing a backend call
            if (ShouldRouteFrameToTrainingRuntime(status_.activeMode) && 
                !settings_.enginePath.empty() && 
                !backend_ && 
                !initRequested_ && 
                !backendCallInFlight_) {
                ++generation_;
                initRequested_ = true;
                InvalidatePreviewStateLocked();
                ResetSaveStateLocked();
                status_.backend = {false, "Initializing SAM3 backend"};
                status_.backendOwningInference = false;
                notifyWorker = true;
            }
        }
    }

    if (backendToShutdown) {
        backendToShutdown->RequestStop();
        cv_.notify_all();
        WaitForBackendCallToFinish();
        backendToShutdown->Shutdown();
    }
    if (notifyWorker) {
        cv_.notify_all();
    }
}

void TrainingSam3Runtime::SubmitPreviewFrame(const Sam3InferenceRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ShouldRouteFrameToTrainingRuntime(status_.activeMode) || !settings_.previewEnabled || !backend_) {
        return;
    }

    pendingPreviewRequest_ = request;
    pendingPreviewRequest_.frame = request.frame.clone();
    previewRequested_ = true;
    cv_.notify_all();
}

bool TrainingSam3Runtime::EnqueueSave(const QueuedSaveRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (datasetMgr_ == nullptr) {
        status_.lastSaveResult = "Save processing unavailable";
        status_.lastSavedImagePath.clear();
        status_.lastSavedLabelPath.clear();
        return false;
    }
    if (saveQueue_.size() >= kMaxTrainingRuntimeQueuedSaves) {
        status_.lastSaveResult = "Save queue full";
        return false;
    }
    QueuedSaveRequest snapshot = request;
    snapshot.frame = request.frame.clone();
    saveQueue_.push(std::move(snapshot));
    status_.lastSaveResult = "Queued " + std::to_string(saveQueue_.size()) + " save request(s)";
    cv_.notify_all();
    return true;
}

TrainingRuntimeStatus TrainingSam3Runtime::GetStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

Sam3LabelResult TrainingSam3Runtime::GetLatestPreviewResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestPreviewResult_;
}

Sam3PreviewOverlaySnapshot TrainingSam3Runtime::GetLatestPreviewOverlaySnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Sam3PreviewOverlaySnapshot snapshot = latestPreviewOverlaySnapshot_;
    if (!snapshot.frame.empty()) {
        snapshot.frame = snapshot.frame.clone();
    }
    return snapshot;
}

void TrainingSam3Runtime::InvalidatePreviewSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    InvalidatePreviewStateLocked();
}

void TrainingSam3Runtime::Shutdown() {
    std::shared_ptr<ISam3PreviewBackend> backendToShutdown;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return;
        }

        WaitForSaveWriteToFinishLocked(lock);
        shuttingDown_ = true;
        stopRequested_ = true;
        ++generation_;
        ResetPreviewStateLocked();
        ResetSaveStateLocked();
        backendToShutdown = std::move(backend_);
        status_.backend = {false, "SAM3 backend inactive"};
        status_.backendOwningInference = false;
    }

    if (backendToShutdown) {
        backendToShutdown->RequestStop();
    }
    cv_.notify_all();
    WaitForBackendCallToFinish();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    if (backendToShutdown) {
        backendToShutdown->Shutdown();
    }
}

void TrainingSam3Runtime::WorkerLoop() {
    while (true) {
        std::shared_ptr<ISam3PreviewBackend> backend;
        std::string enginePath;
        cv::Mat frame;
        Sam3InferenceRequest previewRequest;
        QueuedSaveRequest saveRequest;
        std::uint64_t generation = 0;
        std::uint64_t previewToken = 0;
        bool runInitialize = false;
        bool runPreview = false;
        bool runSave = false;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (!stopRequested_) {
                const bool canRunPreview = previewRequested_
                    && CanPublishPreviewResult(status_, settings_, backend_)
                    && !pendingPreviewRequest_.frame.empty();
                if (initRequested_) {
                    break;
                }
                if (!saveQueue_.empty() && datasetMgr_ && CanRunSam3Save(status_, backend_)) {
                    break;
                }
                if (canRunPreview) {
                    const auto now = std::chrono::steady_clock::now();
                    if (nextPreviewAt_ == std::chrono::steady_clock::time_point{} || now >= nextPreviewAt_) {
                        break;
                    }
                    cv_.wait_until(lock, nextPreviewAt_);
                    continue;
                }
                cv_.wait(lock);
            }

            if (stopRequested_) {
                break;
            }

            if (initRequested_) {
                runInitialize = true;
                generation = generation_;
                enginePath = settings_.enginePath;
                initRequested_ = false;
                backendCallInFlight_ = true;
            } else if (!saveQueue_.empty() && datasetMgr_ && CanRunSam3Save(status_, backend_)) {
                runSave = true;
                generation = generation_;
                backend = backend_;
                saveRequest = saveQueue_.front();
                saveQueue_.pop();
                backendCallInFlight_ = true;
            } else if (previewRequested_ && backend_ && ShouldRouteFrameToTrainingRuntime(status_.activeMode)) {
                if (pendingPreviewRequest_.frame.empty()) {
                    previewRequested_ = false;
                    continue;
                }
                runPreview = true;
                generation = generation_;
                previewToken = previewPublicationToken_;
                backend = backend_;
                previewRequest = pendingPreviewRequest_;
                previewRequest.frame = pendingPreviewRequest_.frame.clone();
                previewRequested_ = false;
                backendCallInFlight_ = true;
            }
        }

        if (runInitialize) {
            std::shared_ptr<ISam3PreviewBackend> newBackend;
            Sam3Availability availability{false, "Failed to create SAM3 backend"};

            try {
                if (!enginePath.empty()) {
                    auto createdBackend = backendFactory_ ? backendFactory_() : nullptr;
                    if (createdBackend) {
                        newBackend = std::shared_ptr<ISam3PreviewBackend>(std::move(createdBackend));
                        const bool initialized = newBackend->Initialize(enginePath);
                        availability = newBackend->GetAvailability();
                        if (!initialized && availability.message.empty()) {
                            availability.message = "Failed to initialize SAM3 backend";
                        }
                        if (!initialized || !availability.ready) {
                            newBackend->Shutdown();
                            newBackend.reset();
                        }
                    }
                } else {
                    availability = {false, "SAM3 engine path not set"};
                }
            } catch (const std::exception& e) {
                availability = {false, std::string("SAM3 init exception: ") + e.what()};
                if (newBackend) {
                    newBackend->RequestStop();
                    newBackend->Shutdown();
                    newBackend.reset();
                }
            } catch (...) {
                availability = {false, "SAM3 init exception"};
                if (newBackend) {
                    newBackend->RequestStop();
                    newBackend->Shutdown();
                    newBackend.reset();
                }
            }

            std::lock_guard<std::mutex> lock(mutex_);
            backendCallInFlight_ = false;
            backendCallCv_.notify_all();
            if (generation != generation_ || stopRequested_ || !ShouldRouteFrameToTrainingRuntime(status_.activeMode)) {
                if (newBackend) {
                    newBackend->RequestStop();
                    newBackend->Shutdown();
                }
                continue;
            }

            backend_ = newBackend;
            status_.backend = availability;
            status_.backendOwningInference = backend_ != nullptr;
            continue;
        }

        if (runSave && backend && datasetMgr_) {
            Sam3InferenceRequest saveInferenceRequest;
            saveInferenceRequest.frame = saveRequest.frame;
            saveInferenceRequest.prompt = saveRequest.prompt;
            saveInferenceRequest.postprocess = saveRequest.sam3Postprocess;
            saveInferenceRequest.frameId = 0;
            Sam3LabelResult saveInference;
            try {
                saveInference = backend->LabelFrame(saveInferenceRequest);
            } catch (const std::exception& e) {
                saveInference.success = false;
                saveInference.error = std::string("Save inference exception: ") + e.what();
            } catch (...) {
                saveInference.success = false;
                saveInference.error = "Save inference exception";
            }
            std::unique_lock<std::mutex> lock(mutex_);
            if (generation != generation_ || stopRequested_ || !CanRunSam3Save(status_, backend_)) {
                backendCallInFlight_ = false;
                backendCallCv_.notify_all();
                continue;
            }

            std::string saveStatus;
            if (!saveInference.success) {
                saveStatus = "Save failed: " + saveInference.error;
                status_.lastSavedImagePath.clear();
                status_.lastSavedLabelPath.clear();
            } else {
                saveWriteInFlight_ = true;
                lock.unlock();
                SaveResult saveResult;
                std::string saveProcessorError;
                try {
                    saveResult = saveProcessor_(*datasetMgr_, saveRequest, saveInference.boxes);
                } catch (const std::exception& e) {
                    saveProcessorError = std::string("Save write exception: ") + e.what();
                } catch (...) {
                    saveProcessorError = "Save write exception";
                }
                lock.lock();
                saveWriteInFlight_ = false;
                saveWriteCv_.notify_all();

                if (generation != generation_ || stopRequested_ || !CanRunSam3Save(status_, backend_)) {
                    backendCallInFlight_ = false;
                    backendCallCv_.notify_all();
                    continue;
                }

                if (!saveProcessorError.empty()) {
                    saveStatus = saveProcessorError;
                    status_.lastSavedImagePath.clear();
                    status_.lastSavedLabelPath.clear();
                } else if (saveResult.savedLabel) {
                    saveStatus = "Saved image and label: " + saveResult.imagePath.string();
                    status_.lastSavedImagePath = saveResult.savedImage ? saveResult.imagePath.string() : std::string{};
                    status_.lastSavedLabelPath = saveResult.savedLabel ? saveResult.labelPath.string() : std::string{};
                } else if (saveResult.savedImage) {
                    saveStatus = "Saved image only: " + saveResult.imagePath.string();
                    status_.lastSavedImagePath = saveResult.savedImage ? saveResult.imagePath.string() : std::string{};
                    status_.lastSavedLabelPath.clear();
                } else {
                    saveStatus = "No detections saved";
                    status_.lastSavedImagePath.clear();
                    status_.lastSavedLabelPath.clear();
                }
            }

            backendCallInFlight_ = false;
            backendCallCv_.notify_all();
            status_.lastSaveResult = saveStatus;
            continue;
        }

        if (runPreview && backend) {
            const auto start = std::chrono::steady_clock::now();
            Sam3LabelResult previewResult;
            try {
                previewResult = backend->LabelFrame(previewRequest);
            } catch (const std::exception& e) {
                previewResult.success = false;
                previewResult.error = std::string("Preview inference exception: ") + e.what();
            } catch (...) {
                previewResult.success = false;
                previewResult.error = "Preview inference exception";
            }
            const auto end = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            std::lock_guard<std::mutex> lock(mutex_);
            backendCallInFlight_ = false;
            backendCallCv_.notify_all();
            if (generation != generation_ || stopRequested_ || previewToken != previewPublicationToken_ ||
                !CanPublishPreviewResult(status_, settings_, backend_) || previewRequest.frame.empty()) {
                continue;
            }

            latestPreviewResult_ = previewResult;
            latestPreviewOverlaySnapshot_.frame = previewRequest.frame.clone();
            latestPreviewOverlaySnapshot_.result = previewResult;
            latestPreviewOverlaySnapshot_.frameId = previewRequest.frameId;
            latestPreviewOverlaySnapshot_.valid = previewResult.success && !previewRequest.frame.empty();
            status_.lastPreviewInferenceTimeMs = elapsed;
            status_.lastPreviewDetectionCount = previewResult.success ? previewResult.boxes.size() : 0;
            const int previewIntervalMs = settings_.previewIntervalMs > 0 ? settings_.previewIntervalMs : 0;
            nextPreviewAt_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(previewIntervalMs);
        }
    }
}

bool EnginePathChangeDropsOldOwnershipBeforeReinitForTests() {
    auto backendState = std::make_shared<BlockingTestBackendState>();
    auto backend = std::make_shared<BlockingTestBackend>(backendState);
    TrainingSam3Runtime runtime([backendState]() -> std::unique_ptr<ISam3PreviewBackend> {
        return std::make_unique<BlockingTestBackend>(backendState);
    });

    TrainingRuntimeSettings settings;
    settings.enginePath = "engine-a";
    settings.previewEnabled = true;

    runtime.UpdateSettings(settings);
    runtime.UpdateMode(InferenceMode::Label);
    if (!WaitForConditionForRuntimeTests([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000)) {
        return false;
    }

    backend->SetBlockInitialize(true);
    settings.enginePath = "engine-b";
    runtime.UpdateSettings(settings);

    const auto dirtyStatus = runtime.GetStatus();
    if (dirtyStatus.backend.ready || dirtyStatus.backendOwningInference) {
        backend->SetBlockInitialize(false);
        runtime.Shutdown();
        return false;
    }

    backend->SetBlockInitialize(false);
    const bool reinitialized = WaitForConditionForRuntimeTests(
        [&runtime]() {
            const auto readyStatus = runtime.GetStatus();
            return readyStatus.backend.ready && readyStatus.backend.message == "ready:engine-b";
        },
        1000);
    runtime.Shutdown();
    return reinitialized;
}

bool BlockedInitCancelsOnModeChangeForTests() {
    auto backendState = std::make_shared<BlockingTestBackendState>();
    backendState->blockInitialize = true;
    TrainingSam3Runtime runtime([backendState]() -> std::unique_ptr<ISam3PreviewBackend> {
        return std::make_unique<BlockingTestBackend>(backendState);
    });

    TrainingRuntimeSettings settings;
    settings.enginePath = "engine-blocked";
    settings.previewEnabled = true;

    runtime.UpdateSettings(settings);
    runtime.UpdateMode(InferenceMode::Label);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runtime.UpdateMode(InferenceMode::Detect);

    {
        std::lock_guard<std::mutex> lock(backendState->mutex);
        backendState->blockInitialize = false;
    }
    backendState->cv.notify_all();

    const bool canceled = WaitForConditionForRuntimeTests(
        [&runtime]() {
            const auto status = runtime.GetStatus();
            return status.activeMode == InferenceMode::Detect &&
                !status.backend.ready &&
                !status.backendOwningInference;
        },
        1000);
    runtime.Shutdown();
    return canceled;
}

bool BlockedInitCancelsOnEnginePathChangeForTests() {
    auto backendState = std::make_shared<BlockingTestBackendState>();
    backendState->blockInitialize = true;
    TrainingSam3Runtime runtime([backendState]() -> std::unique_ptr<ISam3PreviewBackend> {
        return std::make_unique<BlockingTestBackend>(backendState);
    });

    TrainingRuntimeSettings settings;
    settings.enginePath = "engine-a";
    settings.previewEnabled = true;

    runtime.UpdateSettings(settings);
    runtime.UpdateMode(InferenceMode::Label);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    settings.enginePath = "engine-b";
    runtime.UpdateSettings(settings);

    {
        std::lock_guard<std::mutex> lock(backendState->mutex);
        backendState->blockInitialize = false;
    }
    backendState->cv.notify_all();

    const bool reinitialized = WaitForConditionForRuntimeTests(
        [&runtime]() {
            const auto status = runtime.GetStatus();
            return status.activeMode == InferenceMode::Label &&
                status.backend.ready &&
                status.backend.message == "ready:engine-b";
        },
        1000);
    runtime.Shutdown();
    return reinitialized;
}

}  // namespace training

namespace {
std::shared_ptr<training::TrainingSam3Runtime> g_sharedRuntime;
std::mutex g_sharedRuntimeMutex;

class Sam3PreviewBackendAdapter final : public training::ISam3PreviewBackend {
public:
    bool Initialize(const std::string& enginePath) override {
        backend_.reset(new training::Sam3Labeler());
        return backend_->Initialize(enginePath);
    }
    
    void RequestStop() override {
        if (backend_) backend_->RequestStop();
    }
    
    void Shutdown() override {
        if (backend_) backend_->Shutdown();
    }
    
    training::Sam3Availability GetAvailability() const override {
        if (backend_) return backend_->GetAvailability();
        return training::Sam3Availability{false, "Backend not initialized"};
    }
    
    training::Sam3LabelResult LabelFrame(const training::Sam3InferenceRequest& request) override {
        if (backend_) return backend_->LabelFrame(request);
        training::Sam3LabelResult result{};
        result.success = false;
        result.error = "Backend not initialized";
        return result;
    }
    
private:
    std::unique_ptr<training::Sam3Labeler> backend_;
};
}  // namespace

namespace training {

namespace {

std::shared_ptr<TrainingSam3Runtime> GetSharedRuntimeSnapshot() {
    std::lock_guard<std::mutex> lock(g_sharedRuntimeMutex);
    return g_sharedRuntime;
}

}  // namespace

TrainingRuntimeStatus GetTrainingRuntimeStatus() {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        return runtime->GetStatus();
    }
    return TrainingRuntimeStatus{};
}

Sam3LabelResult GetTrainingLatestPreviewResult() {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        return runtime->GetLatestPreviewResult();
    }
    return Sam3LabelResult{};
}

Sam3PreviewOverlaySnapshot GetTrainingLatestPreviewOverlaySnapshot() {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        return runtime->GetLatestPreviewOverlaySnapshot();
    }
    return Sam3PreviewOverlaySnapshot{};
}

void EnsureTrainingRuntimeInitialized(DatasetManager* datasetMgr) {
    std::lock_guard<std::mutex> lock(g_sharedRuntimeMutex);
    if (!g_sharedRuntime) {
        g_sharedRuntime = std::make_shared<TrainingSam3Runtime>(
            datasetMgr,
            []() -> std::unique_ptr<ISam3PreviewBackend> {
                return std::make_unique<Sam3PreviewBackendAdapter>();
            },
            nullptr);
    }
}

void UpdateTrainingRuntimeSettings(const TrainingRuntimeSettings& settings) {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        runtime->UpdateSettings(settings);
    }
}

void UpdateTrainingRuntimeMode(InferenceMode mode) {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        runtime->UpdateMode(mode);
    }
}

void SubmitTrainingPreviewFrame(const Sam3InferenceRequest& request) {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        runtime->SubmitPreviewFrame(request);
    }
}

bool EnqueueTrainingSave(const QueuedSaveRequest& request) {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        return runtime->EnqueueSave(request);
    }
    return false;
}

void InvalidateTrainingPreviewSnapshot() {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        runtime->InvalidatePreviewSnapshot();
    }
}

void ShutdownTrainingRuntime() {
    std::shared_ptr<TrainingSam3Runtime> runtime;
    {
        std::lock_guard<std::mutex> lock(g_sharedRuntimeMutex);
        runtime = g_sharedRuntime;
        g_sharedRuntime.reset();
    }
    if (runtime) {
        runtime->Shutdown();
    }
}

Sam3PresetLoader* GetTrainingPresetLoader() {
    static bool initialized = false;
    static std::mutex initMutex;
    
    if (!initialized) {
        std::lock_guard<std::mutex> lock(initMutex);
        if (!initialized) {
            const auto exePath = GetCurrentExecutablePath();
            const auto trainingRoot = DatasetManager::GetTrainingRootForExe(exePath);
            const auto presetPath = trainingRoot / std::filesystem::path("presets") / config.training_sam3_preset_file;
            g_presetLoader.LoadFromFile(presetPath);
            g_presetLoader.EnableHotReload(config.training_sam3_preset_hot_reload);
            initialized = true;
        }
    }
    
    return &g_presetLoader;
}

}  // namespace training
