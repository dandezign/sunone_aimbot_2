#ifndef TRAINING_SAM3_PRESET_LOADER_H
#define TRAINING_SAM3_PRESET_LOADER_H

#include "sunone_aimbot_2/training/training_sam3_labeler.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace training {

class Sam3PresetLoader {
public:
    Sam3PresetLoader();
    ~Sam3PresetLoader();

    bool LoadFromFile(const std::filesystem::path& path);
    const Sam3PromptPreset* GetPreset(const std::string& className) const;
    std::vector<std::string> GetPresetNames() const;
    bool CheckForChanges();
    void EnableHotReload(bool enabled);
    const std::string& GetError() const { return lastError_; }
    bool IsLoaded() const { return !presets_.empty(); }

private:
    bool ParseJsonFile(const std::filesystem::path& path);
    bool ValidatePreset(const std::string& name, const Sam3PromptPreset& preset);

    std::unordered_map<std::string, Sam3PromptPreset> presets_;
    std::filesystem::path presetPath_;
    std::filesystem::file_time_type lastModified_;
    bool hotReloadEnabled_ = true;
    std::string lastError_;
};

}  // namespace training

#endif
