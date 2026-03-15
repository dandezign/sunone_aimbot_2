#pragma once

#include "sunone_aimbot_2/training/training_label_types.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

namespace training {

struct DetectionBox {
    int classId;
    cv::Rect rect;
    float confidence = 0.0f;  // Detection confidence score (0.0 - 1.0)
};

struct YoloRow {
    int classId;
    float xCenter;
    float yCenter;
    float width;
    float height;
};

struct SaveResult {
    bool savedImage;
    bool savedLabel;
    int classId;
    std::filesystem::path imagePath;
    std::filesystem::path labelPath;
};

class DatasetManager {
public:
    explicit DatasetManager(const std::filesystem::path& root);
    
    void EnsureLayout();
    void SaveClasses(const std::vector<std::string>& classNames);
    void GenerateGameYaml(const std::vector<std::string>& classNames);
    void GenerateTrainBatch(const std::vector<std::string>& classNames);
    
    SaveResult WriteSample(
        const cv::Mat& frame,
        const std::vector<DetectionBox>& detections,
        const std::string& className,
        DatasetSplit split,
        const std::string& imageFormat,
        bool saveNegatives);
    
    int GetClassId(const std::string& className) const;
    std::vector<std::string> LoadClasses() const;
    
    static std::filesystem::path GetTrainingRootForExe(const std::filesystem::path& exePath);
    static std::filesystem::path GetImagesDir(const std::filesystem::path& root, DatasetSplit split);
    static std::filesystem::path GetLabelsDir(const std::filesystem::path& root, DatasetSplit split);

private:
    std::filesystem::path root_;
    std::vector<std::string> classNames_;
    
    void LoadOrCreateClassCatalog();
    std::optional<YoloRow> ToYoloRow(const cv::Rect& box, const cv::Size& size, int classId) const;
    void WriteLabelFile(const std::filesystem::path& path, const std::vector<YoloRow>& rows) const;
    std::filesystem::path GenerateUniqueFilename(const std::filesystem::path& dir, const std::string& extension) const;
};

std::filesystem::path GetTrainingRootForTests(const std::string& exePath);
std::filesystem::path MakeUniqueTestRootForTests();
bool IsPairedSampleNameForTests(const std::filesystem::path& imagePath, const std::filesystem::path& labelPath);

}  // namespace training
