#include "sunone_aimbot_2/training/training_label_types.h"
#include "sunone_aimbot_2/training/training_dataset_manager.h"
#include "sunone_aimbot_2/training/training_label_runtime.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <fstream>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

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
            
            Check(runtime.EnqueueSave(req), "enqueue should succeed");
            
            // Mutate original request after enqueue - queue should preserve original values
            req.prompt = "changed later";
            req.className = "weapon";
            req.split = training::DatasetSplit::Val;
            req.imageFormat = ".jpg";
            originalFrame.setTo(cv::Scalar(0, 0, 0));
            
            const auto peeked = runtime.PeekForTests();
            Check(peeked.prompt == "enemy torso", "queue must capture enqueue-time prompt");
            Check(peeked.className == "player", "queue must capture class at enqueue time");
            Check(peeked.split == training::DatasetSplit::Train, "queue must capture split at enqueue time");
            Check(peeked.imageFormat == ".png", "queue must capture image format at enqueue time");
            Check(training::GetFrameMeanForTests(peeked.frame) != 0, "queue must clone frame at enqueue time");
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
