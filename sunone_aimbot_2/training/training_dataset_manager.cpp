#define _CRT_SECURE_NO_WARNINGS
#include "sunone_aimbot_2/training/training_dataset_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

namespace training {

namespace {

fs::path GenerateTimestampFilename(const fs::path& dir, const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    oss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    oss << extension;
    
    return dir / oss.str();
}

std::string FormatYoloRow(const YoloRow& row) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << row.classId << " "
        << row.xCenter << " "
        << row.yCenter << " "
        << row.width << " "
        << row.height;
    return oss.str();
}

}  // namespace

DatasetManager::DatasetManager(const fs::path& root)
    : root_(root) {
    LoadOrCreateClassCatalog();
}

void DatasetManager::EnsureLayout() {
    fs::create_directories(root_ / "datasets/game/images/train");
    fs::create_directories(root_ / "datasets/game/images/val");
    fs::create_directories(root_ / "datasets/game/labels/train");
    fs::create_directories(root_ / "datasets/game/labels/val");
}

void DatasetManager::SaveClasses(const std::vector<std::string>& classNames) {
    classNames_ = classNames;
    
    const auto path = root_ / "predefined_classes.txt";
    std::ofstream file(path);
    for (const auto& name : classNames_) {
        file << name << "\n";
    }
    
    GenerateGameYaml(classNames);
    GenerateTrainBatch(classNames);
}

void DatasetManager::GenerateGameYaml(const std::vector<std::string>& classNames) {
    const auto path = root_ / "game.yaml";
    std::ofstream file(path);
    
    file << "path: datasets/game\n";
    file << "train: images/train\n";
    file << "val: images/val\n";
    file << "test:\n";
    file << "\n";
    file << "names:\n";
    for (size_t i = 0; i < classNames.size(); ++i) {
        file << "  " << i << ": " << classNames[i] << "\n";
    }
}

void DatasetManager::GenerateTrainBatch(const std::vector<std::string>& classNames) {
    (void)classNames;
    const auto path = root_ / "start_train.bat";
    std::ofstream file(path);
    
    file << "yolo detect train data=game.yaml model=yolo26n.pt epochs=100 imgsz=640\n";
    file << "pause\n";
}

SaveResult DatasetManager::WriteSample(
    const cv::Mat& frame,
    const std::vector<DetectionBox>& detections,
    const std::string& className,
    DatasetSplit split,
    const std::string& imageFormat,
    bool saveNegatives) {
    
    SaveResult result{};
    result.savedImage = false;
    result.savedLabel = false;
    result.classId = -1;
    
    if (classNames_.empty()) {
        LoadOrCreateClassCatalog();
    }
    
    int classId = GetClassId(className);
    if (classId < 0) {
        return result;
    }
    result.classId = classId;
    
    const auto imagesDir = GetImagesDir(root_, split);
    const auto labelsDir = GetLabelsDir(root_, split);
    
    const auto imagePath = GenerateUniqueFilename(imagesDir, imageFormat);
    const auto labelPath = GenerateUniqueFilename(labelsDir, ".txt");
    
    result.imagePath = imagePath;
    result.labelPath = labelPath;
    
    if (detections.empty()) {
        if (saveNegatives) {
            result.savedImage = cv::imwrite(imagePath.string(), frame);
        }
        result.savedLabel = false;
        return result;
    }
    
    std::vector<YoloRow> rows;
    for (const auto& det : detections) {
        auto yoloRow = ToYoloRow(det.rect, frame.size(), det.classId);
        if (yoloRow) {
            rows.push_back(*yoloRow);
        }
    }
    
    if (rows.empty()) {
        if (saveNegatives) {
            result.savedImage = cv::imwrite(imagePath.string(), frame);
        }
        result.savedLabel = false;
        return result;
    }
    
    result.savedImage = cv::imwrite(imagePath.string(), frame);
    if (result.savedImage) {
        WriteLabelFile(labelPath, rows);
        result.savedLabel = true;
    }
    
    return result;
}

int DatasetManager::GetClassId(const std::string& className) const {
    auto it = std::find(classNames_.begin(), classNames_.end(), className);
    if (it != classNames_.end()) {
        return static_cast<int>(std::distance(classNames_.begin(), it));
    }
    return -1;
}

std::vector<std::string> DatasetManager::LoadClasses() const {
    const auto path = root_ / "predefined_classes.txt";
    std::vector<std::string> classes;
    
    if (!fs::exists(path)) {
        return classes;
    }
    
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            classes.push_back(line);
        }
    }
    
    return classes;
}

fs::path DatasetManager::GetTrainingRootForExe(const fs::path& exePath) {
    return exePath.parent_path() / "training";
}

fs::path DatasetManager::GetImagesDir(const fs::path& root, DatasetSplit split) {
    return root / "datasets/game/images" / (split == DatasetSplit::Train ? "train" : "val");
}

fs::path DatasetManager::GetLabelsDir(const fs::path& root, DatasetSplit split) {
    return root / "datasets/game/labels" / (split == DatasetSplit::Train ? "train" : "val");
}

void DatasetManager::LoadOrCreateClassCatalog() {
    classNames_ = LoadClasses();
}

std::optional<YoloRow> DatasetManager::ToYoloRow(const cv::Rect& box, const cv::Size& size, int classId) const {
    cv::Rect clamped = box & cv::Rect(0, 0, size.width, size.height);
    
    if (clamped.width <= 0 || clamped.height <= 0) {
        return std::nullopt;
    }
    
    float xCenter = (clamped.x + clamped.width / 2.0f) / size.width;
    float yCenter = (clamped.y + clamped.height / 2.0f) / size.height;
    float width = static_cast<float>(clamped.width) / size.width;
    float height = static_cast<float>(clamped.height) / size.height;
    
    xCenter = std::clamp(xCenter, 0.0f, 1.0f);
    yCenter = std::clamp(yCenter, 0.0f, 1.0f);
    width = std::clamp(width, 0.0f, 1.0f);
    height = std::clamp(height, 0.0f, 1.0f);
    
    return YoloRow{classId, xCenter, yCenter, width, height};
}

void DatasetManager::WriteLabelFile(const fs::path& path, const std::vector<YoloRow>& rows) const {
    std::ofstream file(path);
    for (const auto& row : rows) {
        file << FormatYoloRow(row) << "\n";
    }
}

fs::path DatasetManager::GenerateUniqueFilename(const fs::path& dir, const std::string& extension) const {
    fs::path path = GenerateTimestampFilename(dir, extension);
    int counter = 0;
    while (fs::exists(path)) {
        path = dir / ("sample_" + std::to_string(++counter) + extension);
    }
    return path;
}

fs::path GetTrainingRootForTests(const std::string& exePath) {
    fs::path exeDir = exePath;
    if (exeDir.has_extension()) {
        exeDir = exeDir.parent_path();
    }
    return exeDir / "training";
}

fs::path MakeUniqueTestRootForTests() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return fs::temp_directory_path() / ("training_test_" + std::to_string(ms));
}

bool IsPairedSampleNameForTests(const fs::path& imagePath, const fs::path& labelPath) {
    return imagePath.stem() == labelPath.stem();
}

}  // namespace training
