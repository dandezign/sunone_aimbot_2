#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>

#include "imgui/imgui.h"
#include "sunone_aimbot_2.h"
#include "overlay.h"
#include "overlay/config_dirty.h"
#include "overlay/draw_settings.h"
#include "capture.h"
#include "training/training_dataset_manager.h"
#include "training/training_label_runtime.h"
#include "training/training_sam3_labeler.h"

extern Config config;
extern std::mutex configMutex;

static int prev_training_preview_interval_ms = 0;
static bool prev_training_label_enabled = false;
static std::string g_trainingStatus = "Training label system ready.";
static std::string g_lastSavedImagePath;
static std::string g_lastSavedLabelPath;
static int g_lastSavedClassId = -1;
static bool g_lastSavedWasNegative = false;

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

    {
        std::lock_guard<std::mutex> lock(configMutex);
        char hotkeyBuffer[128];
        snprintf(hotkeyBuffer, sizeof(hotkeyBuffer), "%s", 
                 config.training_label_hotkey.empty() ? "None" : 
                 config.training_label_hotkey[0].c_str());
        
        if (ImGui::InputText("Label Hotkey", hotkeyBuffer, sizeof(hotkeyBuffer))) {
            config.training_label_hotkey.clear();
            if (strlen(hotkeyBuffer) > 0 && strcmp(hotkeyBuffer, "None") != 0) {
                config.training_label_hotkey.push_back(hotkeyBuffer);
            }
        }
    }

    {
        char promptBuffer[256];
        snprintf(promptBuffer, sizeof(promptBuffer), "%s", config.training_label_prompt.c_str());
        if (ImGui::InputTextWithHint("SAM3 Prompt", "e.g., enemy player", promptBuffer, sizeof(promptBuffer))) {
            config.training_label_prompt = promptBuffer;
        }
    }
    ImGui::SameLine();
    HelpMarker("Descriptive phrase for SAM3 to detect.");

    {
        char classBuffer[128];
        snprintf(classBuffer, sizeof(classBuffer), "%s", config.training_label_class.c_str());
        if (ImGui::InputTextWithHint("Class Name", "e.g., player", classBuffer, sizeof(classBuffer))) {
            config.training_label_class = classBuffer;
        }
    }
    ImGui::SameLine();
    HelpMarker("Stable YOLO dataset class name used in label files.");

    const char* splitItems[] = {"train", "val"};
    int splitIdx = (config.training_label_split == "val") ? 1 : 0;
    if (ImGui::Combo("Dataset Split", &splitIdx, splitItems, IM_ARRAYSIZE(splitItems))) {
        config.training_label_split = splitItems[splitIdx];
    }

    ImGui::Checkbox("Save Negatives", &config.training_label_save_negatives);
    ImGui::SameLine();
    HelpMarker("Save images even when no detections are found (no label file).");

    const char* formatItems[] = {".jpg", ".png"};
    int formatIdx = (config.training_label_image_format == ".png") ? 1 : 0;
    if (ImGui::Combo("Image Format", &formatIdx, formatItems, IM_ARRAYSIZE(formatItems))) {
        config.training_label_image_format = formatItems[formatIdx];
    }

    ImGui::Separator();

    ImGui::Checkbox("Preview Enabled", &config.training_label_preview_enabled);
    ImGui::SameLine();
    HelpMarker("Show SAM3 detection preview when overlay is visible.");

    ImGui::SliderInt("Preview Interval (ms)", &config.training_label_preview_interval_ms, 100, 2000);
}

static void DrawSam3StatusSection()
{
    if (!ImGui::CollapsingHeader("SAM3 Status", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    training::Sam3Labeler labeler;
    const auto avail = labeler.GetAvailability();

    if (avail.ready) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "SAM3 Backend: Ready");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "SAM3 Backend: %s", avail.message.c_str());
    }

    {
        char engineBuffer[512];
        snprintf(engineBuffer, sizeof(engineBuffer), "%s", config.training_sam3_engine_path.c_str());
        if (ImGui::InputTextWithHint("Engine Path", "path/to/sam3.engine", engineBuffer, sizeof(engineBuffer))) {
            config.training_sam3_engine_path = engineBuffer;
        }
    }
    ImGui::SameLine();
    HelpMarker("Path to SAM3 TensorRT engine file.");

    ImGui::Separator();
    ImGui::TextUnformatted(g_trainingStatus.c_str());
    
    if (!g_lastSavedImagePath.empty()) {
        ImGui::Text("Last saved image: %s", g_lastSavedImagePath.c_str());
        ImGui::Text("Last saved label: %s", g_lastSavedLabelPath.c_str());
        ImGui::Text("Class ID: %d", g_lastSavedClassId);
        ImGui::Text("Negative only: %s", g_lastSavedWasNegative ? "Yes" : "No");
    }
}

static void DrawDatasetInfoSection()
{
    if (!ImGui::CollapsingHeader("Dataset Info"))
        return;

    const auto exePath = std::filesystem::current_path() / "ai.exe";
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
    DrawSam3StatusSection();
    ImGui::Spacing();
    DrawDatasetInfoSection();
}
