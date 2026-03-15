#include "sunone_aimbot_2/training/training_label_types.h"
#include "sunone_aimbot_2/training/training_dataset_manager.h"
#include "sunone_aimbot_2/training/training_label_runtime.h"
#include "sunone_aimbot_2/training/training_sam3_labeler.h"
#include "sunone_aimbot_2/training/training_sam3_runtime.h"
#include "sunone_aimbot_2/training/training_sam3_preset_loader.h"
#include "sunone_aimbot_2/overlay/sam3_preview_render_rules.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool WaitForCondition(const std::function<bool()>& predicate, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return predicate();
}

struct RecordingBackendState {
    mutable std::mutex mutex;
    std::vector<training::Sam3InferenceRequest> requests;
    std::string enginePath;
    training::Sam3LabelResult nextResult;
    int initializeCalls = 0;
    bool throwOnInitialize = false;
    bool throwOnPreview = false;
    bool throwOnSave = false;
};

struct BlockingPreviewBackendState {
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::vector<training::Sam3InferenceRequest> requests;
    std::string enginePath;
    bool blockPreview = false;
    int initializeCalls = 0;
    int completedPreviewCalls = 0;
};

class RecordingBackend final : public training::ISam3PreviewBackend {
public:
    explicit RecordingBackend(std::shared_ptr<RecordingBackendState> state)
        : state_(std::move(state)) {
    }

    bool Initialize(const std::string& enginePath) override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->throwOnInitialize) {
            throw std::runtime_error("initialize failed");
        }
        state_->enginePath = enginePath;
        state_->initializeCalls++;
        return true;
    }

    void RequestStop() override {
    }

    void Shutdown() override {
    }

    training::Sam3Availability GetAvailability() const override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->enginePath.empty()) {
            return {false, "idle"};
        }
        return {true, "ready:" + state_->enginePath};
    }

    training::Sam3LabelResult LabelFrame(const training::Sam3InferenceRequest& request) override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->requests.push_back(request);
        if (request.frameId == 0 && state_->throwOnSave) {
            throw std::runtime_error("save inference failed");
        }
        if (request.frameId != 0 && state_->throwOnPreview) {
            throw std::runtime_error("preview inference failed");
        }
        if (!state_->nextResult.success && state_->nextResult.error.empty()) {
            state_->nextResult.success = true;
        }
        return state_->nextResult;
    }

private:
    std::shared_ptr<RecordingBackendState> state_;
};

class BlockingPreviewBackend final : public training::ISam3PreviewBackend {
public:
    explicit BlockingPreviewBackend(std::shared_ptr<BlockingPreviewBackendState> state)
        : state_(std::move(state)) {
    }

    bool Initialize(const std::string& enginePath) override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->enginePath = enginePath;
        state_->initializeCalls++;
        return true;
    }

    void RequestStop() override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->blockPreview = false;
        state_->cv.notify_all();
    }

    void Shutdown() override {
    }

    training::Sam3Availability GetAvailability() const override {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->enginePath.empty()) {
            return {false, "idle"};
        }
        return {true, "ready:" + state_->enginePath};
    }

    training::Sam3LabelResult LabelFrame(const training::Sam3InferenceRequest& request) override {
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->requests.push_back(request);
        state_->cv.wait(lock, [&]() { return !state_->blockPreview; });
        state_->completedPreviewCalls++;
        training::Sam3LabelResult result;
        result.success = true;
        return result;
    }

private:
    std::shared_ptr<BlockingPreviewBackendState> state_;
};

}  // namespace

int main() {
    try {
        // Test QueuedSaveRequest basic fields
        training::QueuedSaveRequest req;
        req.prompt = "enemy torso";
        req.className = "player";
        req.imageFormat = ".jpg";
        Check(req.className == "player", "className should match assigned value");
        Check(req.prompt == "enemy torso", "prompt should match assigned value");
        Check(req.imageFormat == ".jpg", "imageFormat should match assigned value");
        Check(req.split == training::DatasetSplit::Train, "default split should be Train");
        Check(req.saveNegatives == false, "default saveNegatives should be false");
        Check(req.sam3Postprocess.maskThreshold == 0.5f, "default SAM3 mask threshold should be 0.5");
        Check(req.sam3Postprocess.minMaskPixels == 64, "default SAM3 min mask pixels should be 64");
        Check(req.sam3Postprocess.minConfidence == 0.3f, "default SAM3 min confidence should be 0.3");
        Check(req.sam3Postprocess.minBoxWidth == 20, "default SAM3 min box width should be 20");
        Check(req.sam3Postprocess.minBoxHeight == 20, "default SAM3 min box height should be 20");
        Check(req.sam3Postprocess.minMaskFillRatio == 0.01f, "default SAM3 min mask fill ratio should be 0.01");
        Check(req.sam3Postprocess.maxDetections == 100, "default SAM3 max detections should be 100");

        training::Sam3DebugCounters counters;
        Check(counters.rawMaskSlots == 0, "default raw mask slots should be 0");
        Check(counters.finalBoxesRendered == 0, "default final boxes rendered should be 0");

        // Test DatasetSplit enum
        training::DatasetSplit valSplit = training::DatasetSplit::Val;
        Check(valSplit != training::DatasetSplit::Train, "Val should differ from Train");

        // Test GetTrainingRootForTests
        const auto root = training::GetTrainingRootForTests("I:/games/ai.exe");
        Check(root == fs::path("I:/games/training"), "training root should use exe dir");

        // Test DatasetManager with unique test root
        const auto writableRoot = training::MakeUniqueTestRootForTests();
        training::DatasetManager mgr(writableRoot);
        mgr.EnsureLayout();
        mgr.SaveClasses({"player", "head", "weapon"});

        Check(fs::exists(writableRoot / "predefined_classes.txt"), "class catalog missing");
        Check(fs::exists(writableRoot / "game.yaml"), "dataset yaml missing");
        Check(fs::exists(writableRoot / "start_train.bat"), "train batch missing");
        Check(fs::exists(writableRoot / "datasets/game/images/train"), "train images dir missing");
        Check(fs::exists(writableRoot / "datasets/game/images/val"), "val images dir missing");
        Check(fs::exists(writableRoot / "datasets/game/labels/train"), "train labels dir missing");
        Check(fs::exists(writableRoot / "datasets/game/labels/val"), "val labels dir missing");

        // Test class catalog content
        std::ifstream classFile(writableRoot / "predefined_classes.txt");
        std::string line;
        std::vector<std::string> loadedClasses;
        while (std::getline(classFile, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                loadedClasses.push_back(line);
            }
        }
        Check(loadedClasses.size() == 3, "class catalog should have 3 classes");
        Check(loadedClasses[0] == "player", "first class should be player");
        Check(loadedClasses[1] == "head", "second class should be head");
        Check(loadedClasses[2] == "weapon", "third class should be weapon");

        // Test game.yaml content
        std::ifstream yamlFile(writableRoot / "game.yaml");
        std::string yamlContent((std::istreambuf_iterator<char>(yamlFile)),
                                 std::istreambuf_iterator<char>());
        Check(yamlContent.find("path: datasets/game") != std::string::npos, "yaml should have path");
        Check(yamlContent.find("train: images/train") != std::string::npos, "yaml should have train");
        Check(yamlContent.find("val: images/val") != std::string::npos, "yaml should have val");
        Check(yamlContent.find("0: player") != std::string::npos, "yaml should have class 0");
        Check(yamlContent.find("1: head") != std::string::npos, "yaml should have class 1");
        Check(yamlContent.find("2: weapon") != std::string::npos, "yaml should have class 2");

        // Test start_train.bat content
        std::ifstream batFile(writableRoot / "start_train.bat");
        std::string batContent((std::istreambuf_iterator<char>(batFile)),
                                std::istreambuf_iterator<char>());
        Check(batContent.find("yolo detect train") != std::string::npos, "bat should have yolo command");
        Check(batContent.find("data=game.yaml") != std::string::npos, "bat should reference game.yaml");
        Check(batContent.find("model=yolo26n.pt") != std::string::npos, "bat should use yolo26n.pt");

        // Test GetClassId
        Check(mgr.GetClassId("player") == 0, "player should be class 0");
        Check(mgr.GetClassId("head") == 1, "head should be class 1");
        Check(mgr.GetClassId("weapon") == 2, "weapon should be class 2");
        Check(mgr.GetClassId("unknown") == -1, "unknown class should return -1");

        // Test WriteSample with positive detections
        cv::Mat sampleFrame(480, 640, CV_8UC3, cv::Scalar(128, 128, 128));
        training::DetectionBox box{0, cv::Rect(100, 120, 80, 100)};
        
        const auto positiveResult = mgr.WriteSample(
            sampleFrame, {box}, "player", training::DatasetSplit::Train, ".jpg", true);
        
        Check(positiveResult.savedImage, "positive sample should save image");
        Check(positiveResult.savedLabel, "positive sample should save label");
        Check(positiveResult.classId == 0, "class id should be 0 for player");
        Check(positiveResult.imagePath.extension() == ".jpg", "image format should be .jpg");
        Check(positiveResult.labelPath.extension() == ".txt", "label format should be .txt");
        Check(training::IsPairedSampleNameForTests(positiveResult.imagePath, positiveResult.labelPath),
              "image and label should have matching names");
        Check(fs::exists(positiveResult.imagePath), "image file should exist");
        Check(fs::exists(positiveResult.labelPath), "label file should exist");

        // Verify label file content
        std::ifstream labelFile(positiveResult.labelPath);
        std::string labelLine;
        std::getline(labelFile, labelLine);
        Check(!labelLine.empty(), "label file should have content");
        Check(labelLine.find("0 ") == 0, "label should start with class 0");

        // Test WriteSample with no detections (negative) and saveNegatives=true
        const auto negativeSaved = mgr.WriteSample(
            sampleFrame, {}, "player", training::DatasetSplit::Val, ".jpg", true);
        Check(negativeSaved.savedImage, "negative-enabled path should save image");
        Check(!negativeSaved.savedLabel, "negative-enabled path should not save label");
        Check(fs::exists(negativeSaved.imagePath), "negative image file should exist");
        Check(!fs::exists(negativeSaved.labelPath), "negative label file should not exist");

        // Test WriteSample with no detections (negative) and saveNegatives=false
        const auto negativeDropped = mgr.WriteSample(
            sampleFrame, {}, "player", training::DatasetSplit::Val, ".jpg", false);
        Check(!negativeDropped.savedImage && !negativeDropped.savedLabel,
              "negative-disabled path should save nothing");

        // Test WriteSample with invalid box (should be rejected after clamping)
        training::DetectionBox invalidBox{0, cv::Rect(-10, 5, 5, 20)};
        const auto rejected = mgr.WriteSample(
            sampleFrame, {invalidBox}, "player", training::DatasetSplit::Train, ".jpg", true);
        Check(!rejected.savedLabel, "invalid box should not save label");
        Check(rejected.savedImage, "invalid box with saveNegatives=true should save image only");

        // Test box clamping and normalization
        training::DetectionBox edgeBox{0, cv::Rect(600, 450, 100, 80)};
        const auto edgeResult = mgr.WriteSample(
            sampleFrame, {edgeBox}, "player", training::DatasetSplit::Train, ".jpg", false);
        Check(edgeResult.savedLabel, "edge box should be clamped and saved");
        
        std::ifstream edgeLabelFile(edgeResult.labelPath);
        std::string edgeLabelLine;
        std::getline(edgeLabelFile, edgeLabelLine);
        Check(!edgeLabelLine.empty(), "edge label file should have content");
        
        // Parse and verify normalized values are in [0, 1]
        std::istringstream iss(edgeLabelLine);
        int cls;
        float xc, yc, w, h;
        iss >> cls >> xc >> yc >> w >> h;
        Check(cls == 0, "class should be 0");
        Check(xc >= 0.0f && xc <= 1.0f, "xCenter should be normalized");
        Check(yc >= 0.0f && yc <= 1.0f, "yCenter should be normalized");
        Check(w >= 0.0f && w <= 1.0f, "width should be normalized");
        Check(h >= 0.0f && h <= 1.0f, "height should be normalized");

        // Test preview runtime propagates SAM3 postprocess settings
        {
            auto backendState = std::make_shared<RecordingBackendState>();
            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "preview person";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;
            settings.postprocess.maskThreshold = 0.61f;
            settings.postprocess.minMaskPixels = 321;
            settings.postprocess.minConfidence = 0.74f;
            settings.postprocess.minBoxWidth = 44;
            settings.postprocess.minBoxHeight = 55;
            settings.postprocess.minMaskFillRatio = 0.18f;
            settings.postprocess.maxDetections = 9;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "preview runtime should initialize backend");

            training::Sam3InferenceRequest previewRequest;
            previewRequest.frame = cv::Mat::ones(32, 32, CV_8UC3);
            previewRequest.prompt = "preview person";
            previewRequest.postprocess = settings.postprocess;
            previewRequest.frameId = 77;
            runtime.SubmitPreviewFrame(previewRequest);
            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return !backendState->requests.empty();
            }, 1000), "preview runtime should submit inference request");

            training::Sam3InferenceRequest requestSnapshot;
            {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                requestSnapshot = backendState->requests.back();
            }

            Check(requestSnapshot.prompt == "preview person", "preview request should keep prompt");
            Check(requestSnapshot.frameId == 77, "preview request should keep frame id");
            Check(requestSnapshot.postprocess.maskThreshold == 0.61f, "preview request should keep mask threshold");
            Check(requestSnapshot.postprocess.minMaskPixels == 321, "preview request should keep min mask pixels");
            Check(requestSnapshot.postprocess.minConfidence == 0.74f, "preview request should keep min confidence");
            Check(requestSnapshot.postprocess.minBoxWidth == 44, "preview request should keep min box width");
            Check(requestSnapshot.postprocess.minBoxHeight == 55, "preview request should keep min box height");
            Check(requestSnapshot.postprocess.minMaskFillRatio == 0.18f, "preview request should keep fill ratio");
            Check(requestSnapshot.postprocess.maxDetections == 9, "preview request should keep max detections");

            runtime.Shutdown();
        }

        // Test preview snapshot publishes matched deep-copied frame and invalidates stale state
        {
            auto backendState = std::make_shared<RecordingBackendState>();
            backendState->nextResult.success = true;
            backendState->nextResult.boxes.push_back(training::DetectionBox{0, cv::Rect(2, 3, 10, 12), 0.73f});

            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "snapshot person";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "snapshot runtime should initialize backend");

            cv::Mat previewFrame(16, 16, CV_8UC3, cv::Scalar(11, 22, 33));
            training::Sam3InferenceRequest previewRequest;
            previewRequest.frame = previewFrame;
            previewRequest.prompt = "snapshot person";
            previewRequest.frameId = 101;
            runtime.SubmitPreviewFrame(previewRequest);

            Check(WaitForCondition([&runtime]() {
                return runtime.GetLatestPreviewOverlaySnapshot().valid;
            }, 1000), "preview snapshot should become valid");

            previewFrame.setTo(cv::Scalar(0, 0, 0));

            const auto snapshot = runtime.GetLatestPreviewOverlaySnapshot();
            Check(snapshot.valid, "preview snapshot should be valid");
            Check(snapshot.frameId == 101, "preview snapshot should keep frame id");
            Check(snapshot.result.success, "preview snapshot should keep result success");
            Check(snapshot.result.boxes.size() == 1, "preview snapshot should keep result boxes");
            Check(snapshot.result.boxes[0].confidence == 0.73f, "preview snapshot should keep confidence");
            Check(training::GetFrameMeanForTests(snapshot.frame) != 0, "preview snapshot should deep copy frame");

            runtime.UpdateMode(training::InferenceMode::Detect);
            const auto modeInvalidated = runtime.GetLatestPreviewOverlaySnapshot();
            Check(!modeInvalidated.valid, "mode change should invalidate preview snapshot");

            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "runtime should reinitialize after returning to label mode");

            runtime.SubmitPreviewFrame(previewRequest);
            Check(WaitForCondition([&runtime]() {
                return runtime.GetLatestPreviewOverlaySnapshot().valid;
            }, 1000), "preview snapshot should become valid after re-entering label mode");

            settings.previewEnabled = false;
            runtime.UpdateSettings(settings);
            const auto previewDisabled = runtime.GetLatestPreviewOverlaySnapshot();
            Check(!previewDisabled.valid, "disabling preview should invalidate preview snapshot");

            runtime.Shutdown();
            const auto shutdownInvalidated = runtime.GetLatestPreviewOverlaySnapshot();
            Check(!shutdownInvalidated.valid, "shutdown should invalidate preview snapshot");
        }

        // Test preview snapshot preserves submitted request snapshot instead of live settings
        {
            auto backendState = std::make_shared<RecordingBackendState>();
            backendState->nextResult.success = true;

            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "live prompt";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;
            settings.postprocess.maskThreshold = 0.25f;
            settings.postprocess.minConfidence = 0.41f;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "request-preserve runtime should initialize backend");

            training::Sam3InferenceRequest request;
            request.frame = cv::Mat::ones(12, 12, CV_8UC3);
            request.prompt = "snapshotted prompt";
            request.postprocess.maskThreshold = 0.87f;
            request.postprocess.minMaskPixels = 777;
            request.postprocess.minConfidence = 0.91f;
            request.postprocess.minBoxWidth = 33;
            request.postprocess.minBoxHeight = 34;
            request.postprocess.minMaskFillRatio = 0.22f;
            request.postprocess.maxDetections = 5;
            request.frameId = 500;

            runtime.SubmitPreviewFrame(request);

            settings.prompt = "mutated live prompt";
            settings.postprocess.maskThreshold = 0.11f;
            settings.postprocess.minMaskPixels = 1;
            settings.postprocess.minConfidence = 0.12f;
            settings.postprocess.minBoxWidth = 2;
            settings.postprocess.minBoxHeight = 3;
            settings.postprocess.minMaskFillRatio = 0.01f;
            settings.postprocess.maxDetections = 99;
            runtime.UpdateSettings(settings);

            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return !backendState->requests.empty();
            }, 1000), "request-preserve runtime should submit inference request");

            training::Sam3InferenceRequest submitted;
            {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                submitted = backendState->requests.back();
            }

            Check(submitted.prompt == "snapshotted prompt", "preview runtime should preserve submitted prompt");
            Check(submitted.postprocess.maskThreshold == 0.87f, "preview runtime should preserve submitted mask threshold");
            Check(submitted.postprocess.minMaskPixels == 777, "preview runtime should preserve submitted min mask pixels");
            Check(submitted.postprocess.minConfidence == 0.91f, "preview runtime should preserve submitted min confidence");
            Check(submitted.postprocess.minBoxWidth == 33, "preview runtime should preserve submitted min box width");
            Check(submitted.postprocess.minBoxHeight == 34, "preview runtime should preserve submitted min box height");
            Check(submitted.postprocess.minMaskFillRatio == 0.22f, "preview runtime should preserve submitted min fill ratio");
            Check(submitted.postprocess.maxDetections == 5, "preview runtime should preserve submitted max detections");
            Check(submitted.frameId == 500, "preview runtime should preserve submitted frame id");

            runtime.Shutdown();
        }

        // Test preview disable keeps backend alive and save flow working
        {
            const auto runtimeRoot = training::MakeUniqueTestRootForTests();
            training::DatasetManager testMgr(runtimeRoot);
            auto backendState = std::make_shared<RecordingBackendState>();
            backendState->nextResult.success = true;

            training::TrainingSam3Runtime runtime(
                &testMgr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                [](training::DatasetManager&, const training::QueuedSaveRequest&, const std::vector<training::DetectionBox>&) {
                    return training::SaveResult{false, false, -1, {}, {}};
                });

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "keep backend alive";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "preview-disable runtime should initialize backend");

            settings.previewEnabled = false;
            runtime.UpdateSettings(settings);

            const auto statusAfterDisable = runtime.GetStatus();
            Check(statusAfterDisable.backend.ready, "preview disable should keep backend ready");
            Check(statusAfterDisable.backendOwningInference, "preview disable should keep backend ownership");

            training::QueuedSaveRequest request;
            request.frame = cv::Mat::ones(20, 20, CV_8UC3);
            request.prompt = "save still works";
            request.className = "player";
            request.sam3Postprocess.minConfidence = 0.55f;

            Check(runtime.EnqueueSave(request), "preview disable should not block save enqueue");
            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return !backendState->requests.empty();
            }, 1000), "save should still reach backend when preview disabled");

            int initializeCalls = 0;
            {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                initializeCalls = backendState->initializeCalls;
            }
            Check(initializeCalls == 1, "preview disable should not reinitialize backend");

            runtime.Shutdown();
            fs::remove_all(runtimeRoot);
        }

        // Test explicit preview invalidation clears stale snapshot while backend stays ready
        {
            auto backendState = std::make_shared<RecordingBackendState>();
            backendState->nextResult.success = true;

            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "invalidate stale";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "invalidation runtime should initialize backend");

            training::Sam3InferenceRequest request;
            request.frame = cv::Mat::ones(10, 10, CV_8UC3);
            request.prompt = "invalidate stale";
            request.frameId = 42;
            runtime.SubmitPreviewFrame(request);
            Check(WaitForCondition([&runtime]() {
                return runtime.GetLatestPreviewOverlaySnapshot().valid;
            }, 1000), "invalidation runtime should publish valid preview");

            runtime.InvalidatePreviewSnapshot();
            const auto invalidated = runtime.GetLatestPreviewOverlaySnapshot();
            Check(!invalidated.valid, "explicit invalidation should clear preview snapshot");
            Check(runtime.GetStatus().backend.ready, "explicit invalidation should keep backend ready");

            runtime.Shutdown();
        }

        // Test in-flight preview cannot republish after explicit invalidation
        {
            auto backendState = std::make_shared<BlockingPreviewBackendState>();
            backendState->blockPreview = true;

            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<BlockingPreviewBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "barrier test";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "barrier runtime should initialize backend");

            training::Sam3InferenceRequest request;
            request.frame = cv::Mat::ones(14, 14, CV_8UC3);
            request.prompt = "barrier test";
            request.frameId = 700;
            runtime.SubmitPreviewFrame(request);

            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return !backendState->requests.empty();
            }, 1000), "barrier runtime should dispatch blocked preview");

            runtime.InvalidatePreviewSnapshot();
            {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                backendState->blockPreview = false;
            }
            backendState->cv.notify_all();

            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return backendState->completedPreviewCalls == 1;
            }, 1000), "blocked preview should finish after invalidation");
            const auto invalidated = runtime.GetLatestPreviewOverlaySnapshot();
            Check(!invalidated.valid, "in-flight preview should not republish after invalidation");

            runtime.Shutdown();
        }

        // Test disabling preview blocks in-flight preview republish
        {
            auto backendState = std::make_shared<BlockingPreviewBackendState>();
            backendState->blockPreview = true;

            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<BlockingPreviewBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "disable barrier";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "disable barrier runtime should initialize backend");

            training::Sam3InferenceRequest request;
            request.frame = cv::Mat::ones(15, 15, CV_8UC3);
            request.prompt = "disable barrier";
            request.frameId = 701;
            runtime.SubmitPreviewFrame(request);

            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return !backendState->requests.empty();
            }, 1000), "disable barrier runtime should dispatch blocked preview");

            settings.previewEnabled = false;
            runtime.UpdateSettings(settings);
            {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                backendState->blockPreview = false;
            }
            backendState->cv.notify_all();

            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return backendState->completedPreviewCalls == 1;
            }, 1000), "blocked preview should finish after preview disable");
            const auto previewDisabled = runtime.GetLatestPreviewOverlaySnapshot();
            Check(!previewDisabled.valid, "disabled preview should remain invalid after blocked preview completes");
            Check(runtime.GetStatus().backend.ready, "disabled preview should keep backend ready after blocked preview completes");

            runtime.Shutdown();
        }

        Check(training::BlockedInitCancelsOnModeChangeForTests(),
              "blocked init should cancel cleanly on mode change");
        Check(training::BlockedInitCancelsOnEnginePathChangeForTests(),
              "blocked init should cancel cleanly on engine-path change");

        // Overlay toggle contract: confidence labels require preview boxes to be enabled.
        {
            const bool drawBoxes = false;
            const bool drawLabels = true;
            Check(!ShouldDrawSam3PreviewBoxes(drawBoxes),
                  "preview boxes should stay disabled when toggle is off");
            Check(!ShouldDrawSam3PreviewConfidenceLabels(drawBoxes, drawLabels),
                  "confidence labels should not render when preview boxes are disabled");
        }

        // Test save runtime propagates snapshotted SAM3 postprocess settings
        {
            const auto runtimeRoot = training::MakeUniqueTestRootForTests();
            training::DatasetManager testMgr(runtimeRoot);
            auto backendState = std::make_shared<RecordingBackendState>();
            training::TrainingSam3Runtime runtime(
                &testMgr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                [](training::DatasetManager&, const training::QueuedSaveRequest&, const std::vector<training::DetectionBox>&) {
                    return training::SaveResult{false, false, -1, {}, {}};
                });

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.prompt = "runtime save";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "save runtime should initialize backend");

            training::QueuedSaveRequest request;
            request.frame = cv::Mat::ones(24, 24, CV_8UC3);
            request.prompt = "save person";
            request.className = "player";
            request.sam3Postprocess.maskThreshold = 0.42f;
            request.sam3Postprocess.minMaskPixels = 123;
            request.sam3Postprocess.minConfidence = 0.66f;
            request.sam3Postprocess.minBoxWidth = 30;
            request.sam3Postprocess.minBoxHeight = 31;
            request.sam3Postprocess.minMaskFillRatio = 0.07f;
            request.sam3Postprocess.maxDetections = 4;

            Check(runtime.EnqueueSave(request), "save runtime should enqueue request");
            Check(WaitForCondition([backendState]() {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                return !backendState->requests.empty();
            }, 1000), "save runtime should submit inference request");

            training::Sam3InferenceRequest requestSnapshot;
            {
                std::lock_guard<std::mutex> lock(backendState->mutex);
                requestSnapshot = backendState->requests.back();
            }

            Check(requestSnapshot.prompt == "save person", "save request should keep prompt");
            Check(requestSnapshot.postprocess.maskThreshold == 0.42f, "save request should keep mask threshold");
            Check(requestSnapshot.postprocess.minMaskPixels == 123, "save request should keep min mask pixels");
            Check(requestSnapshot.postprocess.minConfidence == 0.66f, "save request should keep min confidence");
            Check(requestSnapshot.postprocess.minBoxWidth == 30, "save request should keep min box width");
            Check(requestSnapshot.postprocess.minBoxHeight == 31, "save request should keep min box height");
            Check(requestSnapshot.postprocess.minMaskFillRatio == 0.07f, "save request should keep fill ratio");
            Check(requestSnapshot.postprocess.maxDetections == 4, "save request should keep max detections");

            runtime.Shutdown();
            fs::remove_all(runtimeRoot);
        }

        // Test runtime survives worker exception during preview inference.
        {
            auto backendState = std::make_shared<RecordingBackendState>();
            backendState->throwOnPreview = true;
            training::TrainingSam3Runtime runtime(
                nullptr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                {});

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "exception-preview runtime should initialize backend");

            training::Sam3InferenceRequest request;
            request.frame = cv::Mat::ones(8, 8, CV_8UC3);
            request.prompt = "throw preview";
            request.frameId = 900;
            runtime.SubmitPreviewFrame(request);

            Check(WaitForCondition([&runtime]() {
                return !runtime.GetLatestPreviewOverlaySnapshot().valid;
            }, 1000), "worker should recover from preview exception without publishing snapshot");

            backendState->throwOnPreview = false;
            runtime.SubmitPreviewFrame(request);
            Check(WaitForCondition([&runtime]() {
                return runtime.GetLatestPreviewOverlaySnapshot().valid;
            }, 1000), "runtime should remain usable after preview exception");

            runtime.Shutdown();
        }

        // Test runtime contains save-processor exceptions and reports status.
        {
            const auto runtimeRoot = training::MakeUniqueTestRootForTests();
            training::DatasetManager testMgr(runtimeRoot);
            auto backendState = std::make_shared<RecordingBackendState>();
            backendState->nextResult.success = true;

            training::TrainingSam3Runtime runtime(
                &testMgr,
                [backendState]() -> std::unique_ptr<training::ISam3PreviewBackend> {
                    return std::make_unique<RecordingBackend>(backendState);
                },
                [](training::DatasetManager&, const training::QueuedSaveRequest&, const std::vector<training::DetectionBox>&) -> training::SaveResult {
                    throw std::runtime_error("disk write failed");
                });

            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;

            runtime.UpdateSettings(settings);
            runtime.UpdateMode(training::InferenceMode::Label);
            Check(WaitForCondition([&runtime]() { return runtime.GetStatus().backend.ready; }, 1000),
                  "exception-save runtime should initialize backend");

            training::QueuedSaveRequest request;
            request.frame = cv::Mat::ones(20, 20, CV_8UC3);
            request.prompt = "throw save";
            request.className = "player";
            Check(runtime.EnqueueSave(request), "save request should enqueue before processor exception");

            Check(WaitForCondition([&runtime]() {
                return runtime.GetStatus().lastSaveResult.find("Save write exception") != std::string::npos;
            }, 1000), "runtime should report save-processor exception without crashing worker");

            runtime.Shutdown();
            fs::remove_all(runtimeRoot);
        }

        // Test runtime can restart after explicit shutdown via shared wrapper API.
        {
            const auto runtimeRoot = training::MakeUniqueTestRootForTests();
            training::DatasetManager testMgr(runtimeRoot);

            training::EnsureTrainingRuntimeInitialized(&testMgr);
            training::TrainingRuntimeSettings settings;
            settings.enginePath = "sam3.engine";
            settings.previewEnabled = true;
            settings.previewIntervalMs = 0;
            training::UpdateTrainingRuntimeSettings(settings);
            training::UpdateTrainingRuntimeMode(training::InferenceMode::Label);

            training::ShutdownTrainingRuntime();

            training::EnsureTrainingRuntimeInitialized(&testMgr);
            training::UpdateTrainingRuntimeSettings(settings);
            training::UpdateTrainingRuntimeMode(training::InferenceMode::Label);
            const auto restartedStatus = training::GetTrainingRuntimeStatus();
            Check(restartedStatus.activeMode == training::InferenceMode::Label,
                  "shared runtime should restart after shutdown");

            training::ShutdownTrainingRuntime();
            fs::remove_all(runtimeRoot);
        }

        // Test runtime queue with enqueue-time snapshots
        {
            training::DatasetManager testMgr(training::MakeUniqueTestRootForTests());
            training::RuntimeManager runtime(testMgr);
            
            cv::Mat originalFrame(480, 640, CV_8UC3, cv::Scalar(100, 150, 200));
            
            training::QueuedSaveRequest req;
            req.frame = originalFrame.clone();
            req.prompt = "enemy torso";
            req.className = "player";
            req.split = training::DatasetSplit::Train;
            req.imageFormat = ".png";
            req.saveNegatives = true;
            req.sam3Postprocess.minConfidence = 0.65f;
            req.sam3Postprocess.maxDetections = 7;
            
            Check(runtime.EnqueueSave(req), "enqueue should succeed");
            
            // Mutate original request after enqueue - queue should preserve original values
            req.prompt = "changed later";
            req.className = "weapon";
            req.split = training::DatasetSplit::Val;
            req.imageFormat = ".jpg";
            req.sam3Postprocess.minConfidence = 0.15f;
            req.sam3Postprocess.maxDetections = 99;
            originalFrame.setTo(cv::Scalar(0, 0, 0));
            
            const auto peeked = runtime.PeekForTests();
            Check(peeked.prompt == "enemy torso", "queue must capture enqueue-time prompt");
            Check(peeked.className == "player", "queue must capture class at enqueue time");
            Check(peeked.split == training::DatasetSplit::Train, "queue must capture split at enqueue time");
            Check(peeked.imageFormat == ".png", "queue must capture image format at enqueue time");
            Check(peeked.sam3Postprocess.minConfidence == 0.65f, "queue must capture SAM3 confidence at enqueue time");
            Check(peeked.sam3Postprocess.maxDetections == 7, "queue must capture SAM3 max detections at enqueue time");
            Check(training::GetFrameMeanForTests(peeked.frame) != 0, "queue must clone frame at enqueue time");
        }

        // Test sort-before-cap keeps highest-confidence detections even if they appear later.
        {
            std::vector<training::DetectionBox> detections;
            detections.push_back(training::DetectionBox{0, cv::Rect(0, 0, 10, 10), 0.20f});
            detections.push_back(training::DetectionBox{0, cv::Rect(1, 1, 10, 10), 0.10f});
            detections.push_back(training::DetectionBox{0, cv::Rect(2, 2, 10, 10), 0.95f});
            detections.push_back(training::DetectionBox{0, cv::Rect(3, 3, 10, 10), 0.80f});

            const auto capped = training::SortAndCapDetectionsForTests(detections, 2);
            Check(capped.size() == 2, "sort-before-cap should return capped detection count");
            Check(capped[0].confidence == 0.95f, "highest-confidence detection should survive truncation");
            Check(capped[1].confidence == 0.80f, "second-highest-confidence detection should survive truncation");
        }

        // Test queue full behavior
        {
            training::DatasetManager testMgr(training::MakeUniqueTestRootForTests());
            training::RuntimeManager runtime(testMgr);
            
            training::QueuedSaveRequest req;
            req.frame = cv::Mat::ones(480, 640, CV_8UC3) * 128;
            req.prompt = "test";
            req.className = "player";
            req.split = training::DatasetSplit::Train;
            req.imageFormat = ".jpg";
            req.saveNegatives = false;
            
            // Fill the queue
            for (size_t i = 0; i < training::kMaxQueuedSaves; ++i) {
                Check(runtime.EnqueueSave(req), "enqueue should succeed until full");
            }
            
            // Queue should now be full
            Check(!runtime.EnqueueSave(req), "queue should reject once full");
            Check(runtime.GetStatusForTests() == "Save queue full", "queue-full status mismatch");
            
            // Dequeue one and verify FIFO order
            training::QueuedSaveRequest dequeued;
            Check(runtime.DequeueForTests(dequeued), "dequeue should succeed");
            Check(dequeued.prompt == "test", "queue should preserve FIFO order");
        }

        // Test SAM3 person preset returns correct CLIP token IDs
        {
            std::vector<int64_t> inputIds;
            std::vector<int64_t> attentionMask;
            
            training::detail::EncodeSam3Prompt("person", 32, inputIds, attentionMask);
            
            Check(inputIds.size() == 32, "inputIds should have 32 elements");
            Check(attentionMask.size() == 32, "attentionMask should have 32 elements");
            
            // Check BOS token (49406 = <|startoftext|>)
            Check(inputIds[0] == 49406, "BOS token should be 49406");
            Check(attentionMask[0] == 1, "BOS attention mask should be 1");
            
            // Check "person" token (2533)
            Check(inputIds[1] == 2533, "person token should be 2533");
            Check(attentionMask[1] == 1, "person attention mask should be 1");
            
            // Check EOS token (49407 = <|endoftext|>)
            Check(inputIds[2] == 49407, "EOS token should be 49407");
            Check(attentionMask[2] == 1, "EOS attention mask should be 1");
            
            // Check padding
            for (size_t i = 3; i < 32; ++i) {
                Check(inputIds[i] == 49407, "padding token should be 49407");
                Check(attentionMask[i] == 0, "padding attention mask should be 0");
            }
        }

        // Test SAM3 person preset case-insensitivity
        {
            std::vector<int64_t> inputIds1, attentionMask1;
            std::vector<int64_t> inputIds2, attentionMask2;
            
            training::detail::EncodeSam3Prompt("person", 32, inputIds1, attentionMask1);
            training::detail::EncodeSam3Prompt("PERSON", 32, inputIds2, attentionMask2);
            
            Check(inputIds1 == inputIds2, "case-insensitive person preset should produce same tokens");
            Check(attentionMask1 == attentionMask2, "case-insensitive person preset should produce same masks");
        }

        // Test SAM3 person preset handles variations
        {
            std::vector<int64_t> ids1, mask1;
            std::vector<int64_t> ids2, mask2;
            std::vector<int64_t> ids3, mask3;
            
            training::detail::EncodeSam3Prompt("person", 32, ids1, mask1);
            training::detail::EncodeSam3Prompt("a person", 32, ids2, mask2);
            training::detail::EncodeSam3Prompt("a photo of a person", 32, ids3, mask3);
            
            Check(ids1 == ids2, "person variations should produce same tokens");
            Check(ids2 == ids3, "person variations should produce same tokens");
        }

        // Test SAM3 person preset recognizes related terms
        {
            std::vector<int64_t> ids1, mask1;
            std::vector<int64_t> ids2, mask2;
            std::vector<int64_t> ids3, mask3;
            
            training::detail::EncodeSam3Prompt("person", 32, ids1, mask1);
            training::detail::EncodeSam3Prompt("people", 32, ids2, mask2);
            training::detail::EncodeSam3Prompt("human", 32, ids3, mask3);
            
            Check(ids1 == ids2, "people should use person preset");
            Check(ids2 == ids3, "human should use person preset");
        }

        // Test SAM3 unknown prompt falls back to hash encoding
        {
            std::vector<int64_t> inputIds;
            std::vector<int64_t> attentionMask;
            
            training::detail::EncodeSam3Prompt("unknown_xyz_class", 32, inputIds, attentionMask);
            
            // Should still have BOS
            Check(inputIds[0] == 49406, "unknown prompt should have BOS token");
            Check(attentionMask[0] == 1, "unknown prompt should have BOS attention mask");
            
            // Should have hash-based token (not the person token 2533)
            Check(inputIds[1] != 2533, "unknown prompt should not use person token");
            // Hash encoding produces IDs in range [1000, 48000)
            Check(inputIds[1] >= 1000 && inputIds[1] < 48000, "unknown prompt should have hash-based token");
        }

        // Test Sam3PresetLoader loads valid preset file
        {
            training::Sam3PresetLoader loader;
            const bool loaded = loader.LoadFromFile("models/presets/default.json");
            Check(loaded, ("preset loader should load default.json: " + loader.GetError()).c_str());
            Check(loader.IsLoaded(), "loader should report loaded state");
            
            const auto names = loader.GetPresetNames();
            Check(names.size() == 5, "default.json should have 5 presets");
            
            const auto* headPreset = loader.GetPreset("head");
            Check(headPreset != nullptr, "head preset should exist");
            Check(headPreset->inputIds.size() == 32, "head preset should have 32 ids");
            Check(headPreset->inputIds[0] == 49406, "head preset should start with BOS");
            Check(headPreset->attentionMask[0] == 1, "head preset BOS should have mask 1");
            
            const auto* personPreset = loader.GetPreset("person");
            Check(personPreset != nullptr, "person preset should exist");
            Check(personPreset->inputIds[1] == 2533, "person preset should have token 2533");
            
            const auto* missingPreset = loader.GetPreset("nonexistent");
            Check(missingPreset == nullptr, "nonexistent preset should return nullptr");
        }

        // Test Sam3PresetLoader handles missing file
        {
            training::Sam3PresetLoader loader;
            const bool loaded = loader.LoadFromFile("nonexistent/path/presets.json");
            Check(!loaded, "loader should fail for missing file");
            Check(loader.GetError().find("not found") != std::string::npos, "error should mention file not found");
        }

        // Test Sam3PresetLoader GetPresetNames returns all keys
        {
            training::Sam3PresetLoader loader;
            const bool loaded = loader.LoadFromFile("models/presets/default.json");
            Check(loaded, "loader should load default.json");
            
            const auto names = loader.GetPresetNames();
            Check(std::find(names.begin(), names.end(), "person") != names.end(), "should have person preset");
            Check(std::find(names.begin(), names.end(), "head") != names.end(), "should have head preset");
            Check(std::find(names.begin(), names.end(), "player") != names.end(), "should have player preset");
            Check(std::find(names.begin(), names.end(), "weapon") != names.end(), "should have weapon preset");
            Check(std::find(names.begin(), names.end(), "vehicle") != names.end(), "should have vehicle preset");
        }

        // Test Sam3PresetLoader hot reload disabled
        {
            training::Sam3PresetLoader loader;
            const bool loaded = loader.LoadFromFile("models/presets/default.json");
            Check(loaded, "loader should load default.json");
            
            loader.EnableHotReload(false);
            const bool changed = loader.CheckForChanges();
            Check(!changed, "hot reload should be disabled");
        }

        // Cleanup test directory
        try {
            fs::remove_all(writableRoot);
        } catch (...) {
            // Ignore cleanup errors
        }

        std::cout << "training_labeling_tests passed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "training_labeling_tests failed: " << e.what() << std::endl;
        return 1;
    }
}
