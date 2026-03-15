#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>

#include "imgui/imgui.h"
#include "sunone_aimbot_2.h"
#include "overlay.h"
#include "overlay/config_dirty.h"
#include "overlay/draw_settings.h"
#include "include/other_tools.h"
#include "capture.h"
#include "training/training_dataset_manager.h"
#include "training/training_label_runtime.h"
#include "training/training_sam3_labeler.h"
#include "training/training_inference_mode.h"
#include "training/training_sam3_runtime.h"

extern Config config;

static int prev_training_preview_interval_ms = 0;
static bool prev_training_label_enabled = false;

namespace {

struct Sam3UiSnapshot {
    std::string enginePath;
    float maskThreshold = 0.5f;
    int minMaskPixels = 64;
    float minConfidence = 0.3f;
    int minBoxWidth = 20;
    int minBoxHeight = 20;
    float minMaskFillRatio = 0.01f;
    int maxDetections = 100;
    bool drawPreviewBoxes = true;
    bool drawConfidenceLabels = true;
};

Sam3UiSnapshot ReadSam3UiSnapshot()
{
    Sam3UiSnapshot snapshot;
    snapshot.enginePath = config.training_sam3_engine_path;
    snapshot.maskThreshold = config.training_sam3_mask_threshold;
    snapshot.minMaskPixels = config.training_sam3_min_mask_pixels;
    snapshot.minConfidence = config.training_sam3_min_confidence;
    snapshot.minBoxWidth = config.training_sam3_min_box_width;
    snapshot.minBoxHeight = config.training_sam3_min_box_height;
    snapshot.minMaskFillRatio = config.training_sam3_min_mask_fill_ratio;
    snapshot.maxDetections = config.training_sam3_max_detections;
    snapshot.drawPreviewBoxes = config.training_sam3_draw_preview_boxes;
    snapshot.drawConfidenceLabels = config.training_sam3_draw_confidence_labels;
    return snapshot;
}

void WriteSam3EnginePath(const std::string& value)
{
    if (config.training_sam3_engine_path != value) {
        config.training_sam3_engine_path = value;
        OverlayConfig_MarkDirty();
    }
}

}  // namespace

static void WriteSam3ConfigFloat(float& target, float value, float minValue, float maxValue)
{
    const float clamped = std::clamp(value, minValue, maxValue);
    if (target == clamped) {
        return;
    }

    target = clamped;
    OverlayConfig_MarkDirty();
}

static void WriteSam3ConfigInt(int& target, int value, int minValue, int maxValue)
{
    const int clamped = std::clamp(value, minValue, maxValue);
    if (target == clamped) {
        return;
    }

    target = clamped;
    OverlayConfig_MarkDirty();
}

static void WriteSam3ConfigBool(bool& target, bool value)
{
    if (target == value) {
        return;
    }

    target = value;
    OverlayConfig_MarkDirty();
}

static inline void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void DrawTrainingLabelSection()
{
    if (!ImGui::CollapsingHeader("Label Settings", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Checkbox("Enable Training Label", &config.training_label_enabled);
    ImGui::SameLine();
    HelpMarker("When enabled, pressing the hotkey will save labeled samples to the training dataset.");

    char hotkeyBuffer[128];
    snprintf(hotkeyBuffer, sizeof(hotkeyBuffer), "%s", 
             config.training_label_hotkey.empty() ? "None" : 
             config.training_label_hotkey[0].c_str());
    
    if (ImGui::InputText("Label Hotkey", hotkeyBuffer, sizeof(hotkeyBuffer))) {
        config.training_label_hotkey.clear();
        if (strlen(hotkeyBuffer) > 0 && strcmp(hotkeyBuffer, "None") != 0) {
            config.training_label_hotkey.push_back(hotkeyBuffer);
        }
        OverlayConfig_MarkDirty();
    }

    {
        char promptBuffer[256];
        snprintf(promptBuffer, sizeof(promptBuffer), "%s", config.training_label_prompt.c_str());
        if (ImGui::InputTextWithHint("SAM3 Prompt", "e.g., enemy player", promptBuffer, sizeof(promptBuffer))) {
            config.training_label_prompt = promptBuffer;
            OverlayConfig_MarkDirty();
        }
    }
    ImGui::SameLine();
    HelpMarker("Descriptive phrase for SAM3 to detect.");

    {
        char classBuffer[128];
        snprintf(classBuffer, sizeof(classBuffer), "%s", config.training_label_class.c_str());
        if (ImGui::InputTextWithHint("Class Name", "e.g., player", classBuffer, sizeof(classBuffer))) {
            config.training_label_class = classBuffer;
            OverlayConfig_MarkDirty();
        }
    }
    ImGui::SameLine();
    HelpMarker("Stable YOLO dataset class name used in label files.");

    const char* splitItems[] = {"train", "val"};
    int splitIdx = (config.training_label_split == "val") ? 1 : 0;
    if (ImGui::Combo("Dataset Split", &splitIdx, splitItems, IM_ARRAYSIZE(splitItems))) {
        config.training_label_split = splitItems[splitIdx];
        OverlayConfig_MarkDirty();
    }

    if (ImGui::Checkbox("Save Negatives", &config.training_label_save_negatives)) {
        OverlayConfig_MarkDirty();
    }
    ImGui::SameLine();
    HelpMarker("Save images even when no detections are found (no label file).");

    const char* formatItems[] = {".jpg", ".png"};
    int formatIdx = (config.training_label_image_format == ".png") ? 1 : 0;
    if (ImGui::Combo("Image Format", &formatIdx, formatItems, IM_ARRAYSIZE(formatItems))) {
        config.training_label_image_format = formatItems[formatIdx];
        OverlayConfig_MarkDirty();
    }

    ImGui::Separator();

    if (ImGui::Checkbox("Preview Enabled", &config.training_label_preview_enabled)) {
        OverlayConfig_MarkDirty();
    }
    ImGui::SameLine();
    HelpMarker("Publishes synced SAM3 snapshot frames for the existing capture preview in Label mode.");

    if (ImGui::SliderInt("Preview Interval (ms)", &config.training_label_preview_interval_ms, 100, 2000)) {
        OverlayConfig_MarkDirty();
    }
}

static void DrawSam3StatusSection()
{
    if (!ImGui::CollapsingHeader("SAM3 Status", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // Mode selector
    const char* modeItems[] = {"Detect", "Label", "Paused"};
    int currentModeIndex = 0;
    switch (activeInferenceMode.load()) {
        case training::InferenceMode::Detect: currentModeIndex = 0; break;
        case training::InferenceMode::Label: currentModeIndex = 1; break;
        case training::InferenceMode::Paused: currentModeIndex = 2; break;
    }
    
    if (ImGui::Combo("Inference Mode", &currentModeIndex, modeItems, IM_ARRAYSIZE(modeItems))) {
        switch (currentModeIndex) {
            case 0: SetInferenceMode(training::InferenceMode::Detect); break;
            case 1: SetInferenceMode(training::InferenceMode::Label); break;
            case 2: SetInferenceMode(training::InferenceMode::Paused); break;
        }
    }
    ImGui::SameLine();
    HelpMarker("Detect = YOLO inference, Label = SAM3 inference (stops YOLO), Paused = no inference");

    // Engine path dropdown
    const Sam3UiSnapshot sam3Config = ReadSam3UiSnapshot();

    {
        const char* engineItems[] = {"sam3_fp16.engine"};
        int currentEngineIndex = 0;
        
        if (!sam3Config.enginePath.empty()) {
            const size_t lastSlash = sam3Config.enginePath.find_last_of("/\\");
            std::string currentFilename = (lastSlash != std::string::npos)
                ? sam3Config.enginePath.substr(lastSlash + 1)
                : sam3Config.enginePath;
            if (currentFilename == "sam3_fp16.engine") {
                currentEngineIndex = 0;
            }
        }
        
        if (ImGui::Combo("Engine", &currentEngineIndex, engineItems, IM_ARRAYSIZE(engineItems))) {
            WriteSam3EnginePath("models/sam3_fp16.engine");
        }
    }
    ImGui::SameLine();
    HelpMarker("Path to SAM3 TensorRT engine file.");

    // Real runtime status
    const auto currentMode = activeInferenceMode.load();
    const auto runtimeStatus = training::GetTrainingRuntimeStatus();
    const auto previewSnapshot = training::GetTrainingLatestPreviewOverlaySnapshot();
    const auto& previewResult = previewSnapshot.result;
    
    if (currentMode == training::InferenceMode::Label) {
        if (runtimeStatus.backend.ready) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Mode: Label (SAM3 ready, %zu detections)", 
                previewResult.boxes.size());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mode: Label (%s)", runtimeStatus.backend.message.c_str());
        }
    } else if (currentMode == training::InferenceMode::Paused) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mode: Paused (no inference)");
    } else {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Mode: Detect (YOLO active)");
    }

    ImGui::Text("Backend: %s", runtimeStatus.backend.message.c_str());
    ImGui::Text("Last preview detections: %zu", runtimeStatus.lastPreviewDetectionCount);
    ImGui::Text("Preview snapshot: %s", previewSnapshot.valid ? "valid" : "waiting");
    ImGui::Text("Raw mask slots: %d", previewResult.counters.rawMaskSlots);
    ImGui::Text("Non-empty masks: %d", previewResult.counters.nonEmptyMasks);
    ImGui::Text("After min mask pixels: %d", previewResult.counters.afterMinMaskPixels);
    ImGui::Text("After min width/height: %d", previewResult.counters.afterMinBoxWidthHeight);
    ImGui::Text("After min fill ratio: %d", previewResult.counters.afterMinMaskFillRatio);
    ImGui::Text("After min confidence: %d", previewResult.counters.afterMinConfidence);
    ImGui::Text("Final boxes before cap: %d", previewResult.counters.finalBoxesBeforeCap);
    ImGui::Text("Final boxes rendered: %d", previewResult.counters.finalBoxesRendered);

    ImGui::Separator();
    ImGui::TextUnformatted(runtimeStatus.lastSaveResult.empty()
        ? "Save queue idle"
        : runtimeStatus.lastSaveResult.c_str());
    if (!runtimeStatus.lastSavedImagePath.empty()) {
        ImGui::Text("Last saved image: %s", runtimeStatus.lastSavedImagePath.c_str());
        ImGui::Text("Last saved label: %s",
            runtimeStatus.lastSavedLabelPath.empty() ? "n/a" : runtimeStatus.lastSavedLabelPath.c_str());
    }
}

static void DrawSam3DetectionSection()
{
    if (!ImGui::CollapsingHeader("SAM3 Detection", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const Sam3UiSnapshot sam3Config = ReadSam3UiSnapshot();

    float maskThreshold = sam3Config.maskThreshold;
    if (ImGui::SliderFloat("Mask Threshold", &maskThreshold, 0.0f, 1.0f, "%.2f")) {
        WriteSam3ConfigFloat(config.training_sam3_mask_threshold, maskThreshold, 0.0f, 1.0f);
    }

    int minMaskPixels = sam3Config.minMaskPixels;
    if (ImGui::SliderInt("Min Mask Pixels", &minMaskPixels, 0, 1000000)) {
        WriteSam3ConfigInt(config.training_sam3_min_mask_pixels, minMaskPixels, 0, 1000000);
    }

    float minConfidence = sam3Config.minConfidence;
    if (ImGui::SliderFloat("Min Confidence", &minConfidence, 0.0f, 1.0f, "%.2f")) {
        WriteSam3ConfigFloat(config.training_sam3_min_confidence, minConfidence, 0.0f, 1.0f);
    }

    int minBoxWidth = sam3Config.minBoxWidth;
    if (ImGui::SliderInt("Min Box Width", &minBoxWidth, 1, 4096)) {
        WriteSam3ConfigInt(config.training_sam3_min_box_width, minBoxWidth, 1, 4096);
    }

    int minBoxHeight = sam3Config.minBoxHeight;
    if (ImGui::SliderInt("Min Box Height", &minBoxHeight, 1, 4096)) {
        WriteSam3ConfigInt(config.training_sam3_min_box_height, minBoxHeight, 1, 4096);
    }

    float minFillRatio = sam3Config.minMaskFillRatio;
    if (ImGui::SliderFloat("Min Mask Fill Ratio", &minFillRatio, 0.0f, 1.0f, "%.2f")) {
        WriteSam3ConfigFloat(config.training_sam3_min_mask_fill_ratio, minFillRatio, 0.0f, 1.0f);
    }

    int maxDetections = sam3Config.maxDetections;
    if (ImGui::SliderInt("Max Detections", &maxDetections, 1, 1000)) {
        WriteSam3ConfigInt(config.training_sam3_max_detections, maxDetections, 1, 1000);
    }

    bool drawPreviewBoxes = sam3Config.drawPreviewBoxes;
    if (ImGui::Checkbox("Draw Preview Boxes", &drawPreviewBoxes)) {
        WriteSam3ConfigBool(config.training_sam3_draw_preview_boxes, drawPreviewBoxes);
    }
    ImGui::SameLine();
    HelpMarker("Draw SAM3 boxes on the capture preview only when Label mode has a valid synced snapshot.");

    bool drawConfidenceLabels = sam3Config.drawConfidenceLabels;
    if (ImGui::Checkbox("Draw Confidence Labels", &drawConfidenceLabels)) {
        WriteSam3ConfigBool(config.training_sam3_draw_confidence_labels, drawConfidenceLabels);
    }
    ImGui::SameLine();
    HelpMarker("Show per-box confidence text on the synced SAM3 capture preview.");
}

static void DrawDatasetInfoSection()
{
    if (!ImGui::CollapsingHeader("Dataset Info"))
        return;

    const auto exePath = GetCurrentExecutablePath();
    const auto trainingRoot = training::DatasetManager::GetTrainingRootForExe(exePath);
    
    ImGui::Text("Dataset Root: %s", trainingRoot.string().c_str());
    
    const auto imagesTrain = training::DatasetManager::GetImagesDir(trainingRoot, training::DatasetSplit::Train);
    const auto labelsTrain = training::DatasetManager::GetLabelsDir(trainingRoot, training::DatasetSplit::Train);
    
    size_t trainImages = 0, trainLabels = 0;
    if (std::filesystem::exists(imagesTrain)) {
        trainImages = std::distance(
            std::filesystem::directory_iterator(imagesTrain),
            std::filesystem::directory_iterator());
    }
    if (std::filesystem::exists(labelsTrain)) {
        trainLabels = std::distance(
            std::filesystem::directory_iterator(labelsTrain),
            std::filesystem::directory_iterator());
    }
    
    ImGui::Text("Train images: %zu, Train labels: %zu", trainImages, trainLabels);
    
    const auto classesPath = trainingRoot / "predefined_classes.txt";
    if (std::filesystem::exists(classesPath)) {
        training::DatasetManager mgr(trainingRoot);
        const auto classes = mgr.LoadClasses();
        
        ImGui::Text("Classes (%zu):", classes.size());
        for (size_t i = 0; i < classes.size(); ++i) {
            ImGui::SameLine();
            ImGui::Text("  [%zu]=%s", i, classes[i].c_str());
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No class catalog found. Will be created on first save.");
    }
}

void draw_training()
{
    if (prev_training_label_enabled != config.training_label_enabled) {
        prev_training_label_enabled = config.training_label_enabled;
        OverlayConfig_MarkDirty();
    }
    
    if (prev_training_preview_interval_ms != config.training_label_preview_interval_ms) {
        prev_training_preview_interval_ms = config.training_label_preview_interval_ms;
        OverlayConfig_MarkDirty();
    }

    ImGui::TextUnformatted("SAM3 Training Label Editor");
    ImGui::Separator();
    ImGui::Spacing();

    DrawTrainingLabelSection();
    ImGui::Spacing();
    DrawSam3DetectionSection();
    ImGui::Spacing();
    DrawSam3StatusSection();
    ImGui::Spacing();
    DrawDatasetInfoSection();
}
