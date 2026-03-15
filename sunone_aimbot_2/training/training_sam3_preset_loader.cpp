#include "sunone_aimbot_2/training/training_sam3_preset_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace training {

Sam3PresetLoader::Sam3PresetLoader()
    : lastModified_{} {
}

Sam3PresetLoader::~Sam3PresetLoader() {
    // Free _strdup'd preset names
    for (auto& [name, preset] : presets_) {
        free(const_cast<char*>(preset.name));
    }
}

bool Sam3PresetLoader::LoadFromFile(const fs::path& path) {
    if (!fs::exists(path)) {
        lastError_ = "Preset file not found: " + path.string();
        return false;
    }

    try {
        lastModified_ = fs::last_write_time(path);
    } catch (const std::exception& e) {
        lastError_ = "Failed to get file timestamp: " + std::string(e.what());
        return false;
    }

    presetPath_ = path;
    return ParseJsonFile(path);
}

const Sam3PromptPreset* Sam3PresetLoader::GetPreset(const std::string& className) const {
    auto it = presets_.find(className);
    if (it != presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> Sam3PresetLoader::GetPresetNames() const {
    std::vector<std::string> names;
    names.reserve(presets_.size());
    for (const auto& [name, _] : presets_) {
        names.push_back(name);
    }
    return names;
}

bool Sam3PresetLoader::CheckForChanges() {
    if (!hotReloadEnabled_ || presetPath_.empty() || presets_.empty()) {
        return false;
    }

    if (!fs::exists(presetPath_)) {
        lastError_ = "Preset file no longer exists: " + presetPath_.string();
        return false;
    }

    try {
        auto currentModified = fs::last_write_time(presetPath_);
        if (currentModified > lastModified_) {
            lastModified_ = currentModified;
            return ParseJsonFile(presetPath_);
        }
    } catch (const std::exception& e) {
        lastError_ = "Failed to check file timestamp: " + std::string(e.what());
        return false;
    }

    return false;
}

void Sam3PresetLoader::EnableHotReload(bool enabled) {
    hotReloadEnabled_ = enabled;
}

bool Sam3PresetLoader::ParseJsonFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Failed to open preset file: " + path.string();
        return false;
    }

    try {
        json j;
        file >> j;

        if (!j.is_object()) {
            lastError_ = "Preset file must contain a JSON object";
            return false;
        }

        // Support both flat format and nested format with "presets" key
        json presetsObj;
        if (j.contains("presets") && j["presets"].is_object()) {
            presetsObj = j["presets"];
        } else {
            presetsObj = j;
        }

        presets_.clear();

        for (const auto& [key, value] : presetsObj.items()) {
            if (!value.is_object()) {
                lastError_ = "Preset '" + key + "' must be a JSON object";
                return false;
            }

            Sam3PromptPreset preset;
            preset.name = _strdup(key.c_str());  // Caller owns memory; freed when preset destroyed

            if (!value.contains("input_ids") || !value.contains("attention_mask")) {
                lastError_ = "Preset '" + key + "' missing required fields (input_ids, attention_mask)";
                return false;
            }

            const auto& idsArray = value["input_ids"];
            const auto& maskArray = value["attention_mask"];

            if (!idsArray.is_array() || !maskArray.is_array()) {
                lastError_ = "Preset '" + key + "' has invalid token arrays";
                return false;
            }

            preset.inputIds.reserve(idsArray.size());
            preset.attentionMask.reserve(maskArray.size());

            for (const auto& id : idsArray) {
                if (!id.is_number_integer()) {
                    lastError_ = "Preset '" + key + "' has non-integer input_ids";
                    return false;
                }
                preset.inputIds.push_back(id.get<int64_t>());
            }

            for (const auto& mask : maskArray) {
                if (!mask.is_number_integer()) {
                    lastError_ = "Preset '" + key + "' has non-integer attention_mask";
                    return false;
                }
                preset.attentionMask.push_back(mask.get<int64_t>());
            }

            if (!ValidatePreset(key, preset)) {
                return false;
            }

            presets_[key] = std::move(preset);
        }

        lastError_.clear();
        return true;

    } catch (const json::parse_error& e) {
        lastError_ = "JSON parse error: " + std::string(e.what());
        return false;
    } catch (const std::exception& e) {
        lastError_ = "Failed to parse preset file: " + std::string(e.what());
        return false;
    }
}

bool Sam3PresetLoader::ValidatePreset(const std::string& name, const Sam3PromptPreset& preset) {
    constexpr int64_t kBosToken = 49406;
    constexpr int64_t kPadToken = 49407;
    constexpr size_t kTokenLength = 32;

    if (preset.inputIds.size() != kTokenLength) {
        lastError_ = "Preset '" + name + "' has invalid input_ids length: expected " +
                     std::to_string(kTokenLength) + ", got " + std::to_string(preset.inputIds.size());
        return false;
    }

    if (preset.attentionMask.size() != kTokenLength) {
        lastError_ = "Preset '" + name + "' has invalid attention_mask length: expected " +
                     std::to_string(kTokenLength) + ", got " + std::to_string(preset.attentionMask.size());
        return false;
    }

    if (preset.inputIds[0] != kBosToken) {
        lastError_ = "Preset '" + name + "' must start with BOS token (49406), got " +
                     std::to_string(preset.inputIds[0]);
        return false;
    }

    if (preset.attentionMask[0] != 1) {
        lastError_ = "Preset '" + name + "' BOS token must have attention mask 1, got " +
                     std::to_string(preset.attentionMask[0]);
        return false;
    }

    return true;
}

}  // namespace training
