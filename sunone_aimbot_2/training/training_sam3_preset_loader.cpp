#include "sunone_aimbot_2/training/training_sam3_preset_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace training {

namespace {
constexpr int64_t kPromptLength = 32;
constexpr int64_t kPadToken = 49407;
constexpr int64_t kBosToken = 49406;
#ifndef SAM3_DEFAULT_TOKENIZER_SCRIPT
#define SAM3_DEFAULT_TOKENIZER_SCRIPT "SAM3TensorRT/python/tokenize_prompt.py"
#endif

std::string trim_copy(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string shell_quote(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        if (ch == '"') {
            out += "\\\"";
        } else {
            out += ch;
        }
    }
    out += "\"";
    return out;
}

std::string normalize_windows_path(std::string s) {
#ifdef _WIN32
    std::replace(s.begin(), s.end(), '/', '\\');
#endif
    return s;
}

bool parse_ids_csv(const std::string& csv, std::vector<int64_t>& parsed) {
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) {
            continue;
        }

        size_t processed = 0;
        long long value = 0;
        try {
            value = std::stoll(item, &processed);
        } catch (const std::exception&) {
            return false;
        }

        if (processed != item.size()) {
            return false;
        }

        parsed.push_back(static_cast<int64_t>(value));
    }

    return !parsed.empty();
}

bool parse_mask_csv(const std::string& csv, std::vector<int64_t>& mask) {
    std::vector<int64_t> parsed;
    if (!parse_ids_csv(csv, parsed)) {
        return false;
    }

    for (int64_t v : parsed) {
        if (v != 0 && v != 1) {
            return false;
        }
    }

    mask = std::move(parsed);
    return true;
}

bool run_command_capture(const std::string& command, std::string& output) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        return false;
    }

    char buffer[512];
    output.clear();
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#ifdef _WIN32
    const int code = _pclose(pipe);
#else
    const int code = pclose(pipe);
#endif

    return code == 0;
}
}  // namespace

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

bool Sam3PresetLoader::TokenizeWithPython(const std::string& prompt,
                                          int tokenCount,
                                          std::vector<int64_t>& inputIds,
                                          std::vector<int64_t>& attentionMask) {
    const char* python_env = std::getenv("SAM3_TOKENIZER_PYTHON");
    const char* script_env = std::getenv("SAM3_TOKENIZER_SCRIPT");

    const std::string python_cmd =
        (python_env && *python_env) ? std::string(python_env) : "python";
    const std::string script_path =
        (script_env && *script_env) ? std::string(script_env)
                                    : std::string(SAM3_DEFAULT_TOKENIZER_SCRIPT);

    const std::string python_cmd_norm = normalize_windows_path(python_cmd);
    const std::string script_path_norm = normalize_windows_path(script_path);

#ifdef _WIN32
    const std::string command = python_cmd_norm + " " + script_path_norm +
                                " --text " + shell_quote(prompt) +
                                " --max-length " + std::to_string(tokenCount);
#else
    const std::string command = shell_quote(python_cmd_norm) + " " +
                                shell_quote(script_path_norm) + " --text " +
                                shell_quote(prompt) + " --max-length " +
                                std::to_string(tokenCount);
#endif

    std::string output;
    if (!run_command_capture(command, output)) {
        return false;
    }

    std::stringstream ss(output);
    std::string line;
    std::string ids_line;
    std::string mask_line;

    while (std::getline(ss, line)) {
        line = trim_copy(line);
        if (line.rfind("IDS:", 0) == 0) {
            ids_line = line.substr(4);
        } else if (line.rfind("MASK:", 0) == 0) {
            mask_line = line.substr(5);
        }
    }

    std::vector<int64_t> parsed_ids;
    std::vector<int64_t> parsed_mask;
    if (!parse_ids_csv(ids_line, parsed_ids) ||
        !parse_mask_csv(mask_line, parsed_mask)) {
        return false;
    }

    if (parsed_ids.size() != static_cast<size_t>(tokenCount) ||
        parsed_mask.size() != static_cast<size_t>(tokenCount)) {
        return false;
    }

    inputIds = std::move(parsed_ids);
    attentionMask = std::move(parsed_mask);
    return true;
}

}  // namespace training
