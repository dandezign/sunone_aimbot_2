# SAM3 Dynamic Prompt Preset System Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dynamic preset system for SAM3 prompt tokenization with Python-based preset builder, JSON file loading, UI integration, and hot-reload support.

**Architecture:** Python tokenizer generates JSON preset files → C++ loader parses and provides lookup → UI dropdown for selection → Hot-reload watches file changes → Runtime reinitializes backend on preset change.

**Tech Stack:** Python 3.12+, transformers (CLIP tokenizer), C++17, nlohmann/json (or existing JSON lib), std::filesystem, ImGui UI.

**Related Spec:** `docs/superpowers/specs/2026-03-15-sam3-dynamic-preset-system-design.md`

---

## Phase 1: Preset Builder Tool + Default Presets

### Task 1.1: Create Preset Builder Script

**Files:**
- Create: `scripts/build_sam3_presets.py`
- Reference: `SAM3TensorRT/python/tokenize_prompt.py`

- [ ] **Step 1: Write preset builder script**

```python
#!/usr/bin/env python3
"""Build SAM3 preset JSON files from class names using CLIP tokenizer."""

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path

from transformers import CLIPTokenizer

DEFAULT_MODEL = "openai/clip-vit-base-patch32"
DEFAULT_MAX_LENGTH = 32


def tokenize_prompt(text: str, max_length: int, model: str) -> dict:
    """Tokenize a prompt and return ids/mask dict."""
    tokenizer = CLIPTokenizer.from_pretrained(model)
    encoded = tokenizer(
        text,
        padding="max_length",
        truncation=True,
        max_length=max_length,
        return_attention_mask=True,
    )
    return {
        "ids": [int(v) for v in encoded["input_ids"]],
        "mask": [int(v) for v in encoded["attention_mask"]],
    }


def build_presets(class_names: list[str], max_length: int, model: str) -> dict:
    """Build preset dict from class names."""
    presets = {}
    for name in class_names:
        print(f"Tokenizing: '{name}'")
        presets[name] = tokenize_prompt(name, max_length, model)
    return presets


def main() -> int:
    parser = argparse.ArgumentParser(description="Build SAM3 preset JSON file")
    parser.add_argument(
        "--class", "-c",
        action="append",
        dest="classes",
        default=[],
        help="Class name to tokenize (can be repeated)",
    )
    parser.add_argument(
        "--input", "-i",
        type=Path,
        help="Text file with class names (one per line)",
    )
    parser.add_argument(
        "--output", "-o",
        type=Path,
        required=True,
        help="Output JSON file path",
    )
    parser.add_argument(
        "--max-length",
        type=int,
        default=DEFAULT_MAX_LENGTH,
        help=f"Token sequence length (default: {DEFAULT_MAX_LENGTH})",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help=f"Tokenizer model (default: {DEFAULT_MODEL})",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Interactive mode: prompt for class names",
    )

    args = parser.parse_args()

    # Collect class names
    class_names = args.classes.copy()

    if args.input:
        with open(args.input, "r", encoding="utf-8") as f:
            class_names.extend(line.strip() for line in f if line.strip())

    if args.interactive:
        print("Interactive mode. Enter class names (empty line to finish):")
        while True:
            name = input("  Class: ").strip()
            if not name:
                break
            class_names.append(name)

    if not class_names:
        print("Error: No class names provided", file=sys.stderr)
        return 1

    # Build presets
    print(f"Tokenizing {len(class_names)} class(es) with {args.model}...")
    presets = build_presets(class_names, args.max_length, args.model)

    # Create output file
    output = {
        "version": 1,
        "created_at": datetime.utcnow().isoformat() + "Z",
        "source": "build_sam3_presets.py",
        "presets": presets,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)

    print(f"Created {args.output} with {len(presets)} preset(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Test preset builder with default classes**

Run:
```powershell
.\.venv\Scripts\Activate.ps1
py scripts/build_sam3_presets.py --class "head" --class "body" --class "weapon" --class "upper body" --class "lower body" --output models/presets/default.json
```

Expected output:
```
Tokenizing: 'head'
Tokenizing: 'body'
Tokenizing: 'weapon'
Tokenizing: 'upper body'
Tokenizing: 'lower body'
Tokenizing 5 class(es) with openai/clip-vit-base-patch32...
Created models/presets/default.json with 5 preset(s)
```

- [ ] **Step 3: Verify JSON output format**

Run:
```powershell
cat models/presets/default.json
```

Expected content (abbreviated):
```json
{
  "version": 1,
  "created_at": "2026-03-15T...",
  "source": "build_sam3_presets.py",
  "presets": {
    "head": {
      "ids": [49406, 1234, 49407, ...],
      "mask": [1, 1, 1, 0, ...]
    },
    ...
  }
}
```

- [ ] **Step 4: Test interactive mode**

Run:
```powershell
py scripts/build_sam3_presets.py --interactive --output models/presets/test.json
```

Expected: Prompts for class names, creates file.

- [ ] **Step 5: Commit**

```bash
git add scripts/build_sam3_presets.py models/presets/default.json
git commit -m "feat: add SAM3 preset builder tool and default presets"
```

---

### Task 1.2: Add JSON Library Dependency (if not present)

**Files:**
- Modify: `CMakeLists.txt` (add nlohmann/json if not present)
- Check: existing JSON usage in project

- [ ] **Step 1: Check for existing JSON library**

Run:
```powershell
grep -r "nlohmann" CMakeLists.txt sunone_aimbot_2/
```

If found: note include path, skip to Task 2.

If not found: proceed with adding nlohmann/json.

- [ ] **Step 2: Add nlohmann/json via FetchContent**

Modify `CMakeLists.txt`:
```cmake
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)
```

- [ ] **Step 3: Link to ai target**

Add to `CMakeLists.txt`:
```cmake
target_link_libraries(ai PRIVATE nlohmann_json::nlohmann_json)
```

- [ ] **Step 4: Test build**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "chore: add nlohmann/json dependency for preset parsing"
```

---

## Phase 2: Runtime Preset Loader

### Task 2.1: Create Preset Loader Class

**Files:**
- Create: `sunone_aimbot_2/training/training_sam3_preset_loader.h`
- Create: `sunone_aimbot_2/training/training_sam3_preset_loader.cpp`
- Test: `tests/training_labeling_tests.cpp` (add loader tests)

- [ ] **Step 1: Write header file**

```cpp
#ifndef TRAINING_SAM3_PRESET_LOADER_H
#define TRAINING_SAM3_PRESET_LOADER_H

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace training {

struct Sam3PromptPreset {
    std::string name;
    std::vector<int64_t> input_ids;      // 32 tokens
    std::vector<int64_t> attention_mask; // 32 tokens
};

class Sam3PresetLoader {
public:
    Sam3PresetLoader();
    ~Sam3PresetLoader();

    // Load preset file from path. Returns true on success.
    bool LoadFromFile(const std::filesystem::path& path);
    
    // Get tokens for a preset class. Returns nullptr if not found.
    const Sam3PromptPreset* GetPreset(const std::string& className) const;
    
    // Get list of all available preset names
    std::vector<std::string> GetPresetNames() const;
    
    // Check if file changed and reload if needed
    bool CheckForChanges();
    
    // Enable/disable hot reload watching
    void EnableHotReload(bool enabled);
    
    // Get last error message
    const std::string& GetError() const { return lastError_; }
    
    // Check if presets are loaded
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
```

- [ ] **Step 2: Write implementation file**

```cpp
#include "training_sam3_preset_loader.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace training {

Sam3PresetLoader::Sam3PresetLoader() = default;
Sam3PresetLoader::~Sam3PresetLoader() = default;

bool Sam3PresetLoader::LoadFromFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        lastError_ = "Preset file not found: " + path.string();
        return false;
    }

    try {
        lastModified_ = std::filesystem::last_write_time(path);
    } catch (const std::exception& e) {
        lastError_ = "Failed to read file timestamp: " + std::string(e.what());
        return false;
    }

    presetPath_ = path;
    return ParseJsonFile(path);
}

bool Sam3PresetLoader::ParseJsonFile(const std::filesystem::path& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            lastError_ = "Failed to open file: " + path.string();
            return false;
        }

        json root;
        file >> root;

        if (!root.contains("presets") || !root["presets"].is_object()) {
            lastError_ = "Missing 'presets' object in JSON";
            return false;
        }

        presets_.clear();
        const auto& presetsObj = root["presets"];

        for (const auto& [name, value] : presetsObj.items()) {
            if (!value.contains("ids") || !value.contains("mask")) {
                std::cerr << "[PresetLoader] Skipping '" << name << "': missing ids/mask" << std::endl;
                continue;
            }

            Sam3PromptPreset preset;
            preset.name = name;

            for (const auto& id : value["ids"]) {
                preset.input_ids.push_back(id.get<int64_t>());
            }
            for (const auto& m : value["mask"]) {
                preset.attention_mask.push_back(m.get<int64_t>());
            }

            if (!ValidatePreset(name, preset)) {
                continue;  // Validation failed, skip
            }

            presets_[name] = std::move(preset);
        }

        if (presets_.empty()) {
            lastError_ = "No valid presets found in file";
            return false;
        }

        std::cout << "[PresetLoader] Loaded " << presets_.size() << " preset(s) from " << path.string() << std::endl;
        return true;

    } catch (const json::exception& e) {
        lastError_ = "JSON parse error: " + std::string(e.what());
        return false;
    } catch (const std::exception& e) {
        lastError_ = "Error: " + std::string(e.what());
        return false;
    }
}

bool Sam3PresetLoader::ValidatePreset(const std::string& name, const Sam3PromptPreset& preset) {
    if (preset.input_ids.size() != 32) {
        std::cerr << "[PresetLoader] Skipping '" << name << "': ids size is " 
                  << preset.input_ids.size() << ", expected 32" << std::endl;
        return false;
    }
    if (preset.attention_mask.size() != 32) {
        std::cerr << "[PresetLoader] Skipping '" << name << "': mask size is " 
                  << preset.attention_mask.size() << ", expected 32" << std::endl;
        return false;
    }
    if (preset.input_ids[0] != 49406) {
        std::cerr << "[PresetLoader] Skipping '" << name << "': first token is not BOS (49406)" << std::endl;
        return false;
    }
    return true;
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
    if (!hotReloadEnabled_ || presetPath_.empty()) {
        return false;
    }

    if (!std::filesystem::exists(presetPath_)) {
        return false;  // File deleted, stop watching
    }

    try {
        auto currentModified = std::filesystem::last_write_time(presetPath_);
        if (currentModified != lastModified_) {
            std::cout << "[PresetLoader] Detected change in " << presetPath_.string() << ", reloading..." << std::endl;
            return ParseJsonFile(presetPath_);
        }
    } catch (const std::exception&) {
        return false;
    }

    return false;
}

void Sam3PresetLoader::EnableHotReload(bool enabled) {
    hotReloadEnabled_ = enabled;
}

}  // namespace training
```

- [ ] **Step 3: Write failing test**

Add to `tests/training_labeling_tests.cpp`:

```cpp
// Test Sam3PresetLoader loads valid preset file
{
    training::Sam3PresetLoader loader;
    const bool loaded = loader.LoadFromFile("models/presets/default.json");
    Check(loaded, "preset loader should load default.json");
    Check(loader.IsLoaded(), "loader should report loaded state");
    
    const auto names = loader.GetPresetNames();
    Check(names.size() == 5, "default.json should have 5 presets");
    
    const auto* headPreset = loader.GetPreset("head");
    Check(headPreset != nullptr, "head preset should exist");
    Check(headPreset->input_ids.size() == 32, "head preset should have 32 ids");
    Check(headPreset->input_ids[0] == 49406, "head preset should start with BOS");
}
```

- [ ] **Step 4: Add include to test file**

Add to top of `tests/training_labeling_tests.cpp`:
```cpp
#include "sunone_aimbot_2/training/training_sam3_preset_loader.h"
```

- [ ] **Step 5: Run test to verify it passes**

Run:
```powershell
cmake --build build/cuda --config Release --target training_labeling_tests
.\build\cuda\Release\training_labeling_tests.exe
```

Expected: Test passes.

- [ ] **Step 6: Commit**

```bash
git add sunone_aimbot_2/training/training_sam3_preset_loader.h sunone_aimbot_2/training/training_sam3_preset_loader.cpp tests/training_labeling_tests.cpp
git commit -m "feat: add Sam3PresetLoader class with JSON parsing and hot-reload"
```

---

### Task 2.2: Integrate Preset Loader into SAM3 Labeler

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`
- Modify: `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp`
- Reference: existing `GetSam3PromptPreset()` function

- [ ] **Step 1: Add preset loader member to Sam3Labeler**

Modify `training_sam3_labeler.h`:
```cpp
// Add to Sam3Labeler class (around line 230):
#include "training_sam3_preset_loader.h"

class Sam3Labeler {
public:
    // ... existing members ...
    
    // Set preset loader for token lookup
    void SetPresetLoader(const Sam3PresetLoader* loader);

private:
    // ... existing members ...
    const Sam3PresetLoader* presetLoader_ = nullptr;
};
```

- [ ] **Step 2: Implement SetPresetLoader**

Modify `training_sam3_labeler_trt.cpp`:
```cpp
void Sam3Labeler::SetPresetLoader(const Sam3PresetLoader* loader) {
    presetLoader_ = loader;
}
```

- [ ] **Step 3: Update EncodeSam3Prompt to use preset loader**

Modify `detail::EncodeSam3Prompt()` in `training_sam3_labeler_trt.cpp`:

```cpp
inline void EncodeSam3Prompt(const std::string& prompt,
                             int tokenCount,
                             std::vector<int64_t>& inputIds,
                             std::vector<int64_t>& attentionMask) {
    // Initialize with padding
    inputIds.assign(tokenCount, 49407);
    attentionMask.assign(tokenCount, 0);

    // Check preset loader first (dynamic presets)
    if (presetLoader_ != nullptr) {
        if (const auto* preset = presetLoader_->GetPreset(prompt)) {
            const size_t copyCount = std::min(
                static_cast<size_t>(tokenCount),
                std::min(preset->input_ids.size(), preset->attention_mask.size()));
            for (size_t i = 0; i < copyCount; ++i) {
                inputIds[i] = preset->input_ids[i];
                attentionMask[i] = preset->attention_mask[i];
            }
            return;
        }
    }

    // Fallback to hardcoded person preset (for backward compatibility)
    if (/* ... existing person preset logic ... */) {
        return;
    }

    // Final fallback: hash-based encoding
    // ... existing hash logic ...
}
```

- [ ] **Step 4: Build and test**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/training/training_sam3_labeler.h sunone_aimbot_2/training/training_sam3_labeler_trt.cpp
git commit -m "feat: integrate preset loader into SAM3 labeler token encoding"
```

---

## Phase 3: Configuration + UI Integration

### Task 3.1: Add Config Fields

**Files:**
- Modify: `sunone_aimbot_2/config/config.h`
- Modify: `sunone_aimbot_2/config/config.cpp`

- [ ] **Step 1: Add config fields to header**

Modify `config.h` (add to Config struct):
```cpp
std::string training_sam3_preset_file;           // e.g., "default.json"
std::string training_sam3_preset_class;          // e.g., "head"
bool training_sam3_preset_hot_reload = true;     // enable/disable file watching
std::string training_sam3_presets_dir = "models/presets";  // preset directory
```

- [ ] **Step 2: Add config read/write in config.cpp**

Modify `config.cpp` (add to LoadConfig/SaveConfig):
```cpp
// In LoadConfig():
READ_STR(training_sam3_preset_file, "default.json");
READ_STR(training_sam3_preset_class, "head");
READ_BOOL(training_sam3_preset_hot_reload, true);
READ_STR(training_sam3_presets_dir, "models/presets");

// In SaveConfig():
WRITE_STR(training_sam3_preset_file);
WRITE_STR(training_sam3_preset_class);
WRITE_BOOL(training_sam3_preset_hot_reload);
WRITE_STR(training_sam3_presets_dir);
```

- [ ] **Step 3: Add config UI in draw_settings.h or draw_training.cpp**

Add to Training tab UI (modify `draw_training.cpp`):
```cpp
ImGui::TextUnformatted("Preset Configuration");
ImGui::Separator();

char presetFileBuf[256];
strcpy_s(presetFileBuf, config.training_sam3_preset_file.c_str());
if (ImGui::InputText("Preset File", presetFileBuf, IM_ARRAYSIZE(presetFileBuf))) {
    config.training_sam3_preset_file = presetFileBuf;
    OverlayConfig_MarkDirty();
}
ImGui::SameLine();
HelpMarker("Preset JSON file in models/presets/");

ImGui::Checkbox("Hot Reload", &config.training_sam3_preset_hot_reload);
ImGui::SameLine();
HelpMarker("Automatically reload preset file when it changes");
```

- [ ] **Step 4: Build and test**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/config/config.h sunone_aimbot_2/config/config.cpp sunone_aimbot_2/overlay/draw_training.cpp
git commit -m "feat: add preset config fields and UI"
```

---

### Task 3.2: Add Preset Class Dropdown UI

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.h` (expose loader)

- [ ] **Step 1: Expose preset loader from runtime**

Modify `training_sam3_runtime.h`:
```cpp
// Add function to get global preset loader
const Sam3PresetLoader* GetTrainingPresetLoader();
```

- [ ] **Step 2: Implement in training_sam3_runtime.cpp**

```cpp
const Sam3PresetLoader* GetTrainingPresetLoader() {
    auto runtime = GetSharedRuntimeSnapshot();
    if (runtime) {
        return runtime->GetPresetLoader();  // Add this method too
    }
    return nullptr;
}
```

- [ ] **Step 3: Add dropdown UI**

Modify `draw_training.cpp`:
```cpp
// After preset file selector:
if (const auto* loader = training::GetTrainingPresetLoader()) {
    if (loader->IsLoaded()) {
        const auto names = loader->GetPresetNames();
        int currentIndex = 0;
        for (size_t i = 0; i < names.size(); ++i) {
            if (names[i] == config.training_sam3_preset_class) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        if (ImGui::Combo("Preset Class", &currentIndex, 
                         [](void* data, int idx, const char** out) -> bool {
                             auto* names = static_cast<std::vector<std::string>*>(data);
                             *out = (*names)[idx].c_str();
                             return true;
                         },
                         &names,
                         static_cast<int>(names.size()))) {
            config.training_sam3_preset_class = names[currentIndex];
            config.training_label_prompt = names[currentIndex];  // Sync with prompt
            OverlayConfig_MarkDirty();
        }
    }
}
```

- [ ] **Step 4: Build and test**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_training.cpp sunone_aimbot_2/training/training_sam3_runtime.h sunone_aimbot_2/training/training_sam3_runtime.cpp
git commit -m "feat: add preset class dropdown UI"
```

---

## Phase 4: Hot Reload + Runtime Reinit

### Task 4.1: Implement Hot Reload Watcher

**Files:**
- Modify: `sunone_aimbot_2/capture/capture.cpp`

- [ ] **Step 1: Add hot reload check in capture loop**

Modify `capture.cpp` (in main capture loop, around line 923):
```cpp
// Training preset hot reload check
if (config.training_sam3_preset_hot_reload) {
    if (auto* loader = training::GetTrainingPresetLoader()) {
        if (loader->CheckForChanges()) {
            std::cout << "[Capture] Preset file reloaded, triggering reinit..." << std::endl;
            // Trigger reinit by marking settings as changed
            settingsDirty = true;
        }
    }
}
```

- [ ] **Step 2: Build and test**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 3: Manual test hot reload**

1. Run `ai.exe`
2. Go to Training → Label tab
3. Modify `models/presets/default.json` (add a test class)
4. Check if console shows reload message

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/capture/capture.cpp
git commit -m "feat: add hot reload watcher for preset files"
```

---

### Task 4.2: Implement Runtime Reinitialization on Preset Change

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.cpp`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.h`

- [ ] **Step 1: Add preset change detection in UpdateSettings**

Modify `UpdateSettings()`:
```cpp
void TrainingSam3Runtime::UpdateSettings(const TrainingRuntimeSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const bool presetChanged = (settings.prompt != settings_.prompt);
    
    settings_ = settings;
    
    if (presetChanged && status_.backend.ready) {
        // Preset changed, need to reinitialize
        std::cout << "[SAM3Runtime] Preset changed, scheduling reinit..." << std::endl;
        initRequested_ = true;
        // Backend will be shutdown+reinit in worker loop
    }
    
    cv_.notify_one();
}
```

- [ ] **Step 2: Handle reinit in worker loop**

Modify `WorkerLoop()` to handle preset-change reinit (around line 400):
```cpp
if (initRequested_ && status_.backend.ready) {
    // Shut down existing backend first
    if (backend_) {
        backend_->RequestStop();
        backend_->Shutdown();
        backend_.reset();
        status_.backend = {false, "Reinitializing..."};
        status_.backendOwningInference = false;
    }
}
```

- [ ] **Step 3: Add reinit progress status**

Modify status struct to include reinit state:
```cpp
struct TrainingRuntimeStatus {
    // ... existing fields ...
    bool isReinitializing = false;  // Set during preset-change reinit
};
```

- [ ] **Step 4: Build and test**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 5: Manual test reinit**

1. Run `ai.exe`
2. Go to Training → Label
3. Select different preset class from dropdown
4. Check status shows "Reinitializing..." then "SAM3 TensorRT ready"
5. Verify detections use new class

- [ ] **Step 6: Commit**

```bash
git add sunone_aimbot_2/training/training_sam3_runtime.cpp sunone_aimbot_2/training/training_sam3_runtime.h
git commit -m "feat: reinitialize SAM3 backend on preset change"
```

---

## Phase 5: Testing + Polish

### Task 5.1: Add Integration Tests

**Files:**
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Add preset loader integration test**

```cpp
// Test full preset loading + lookup flow
{
    training::Sam3PresetLoader loader;
    Check(loader.LoadFromFile("models/presets/default.json"),
          "integration test should load default.json");
    
    const auto* head = loader.GetPreset("head");
    Check(head != nullptr, "head preset should be found");
    Check(head->name == "head", "preset name should match");
    
    const auto* upperBody = loader.GetPreset("upper body");
    Check(upperBody != nullptr, "multi-word preset should be found");
    Check(upperBody->input_ids[0] == 49406, "multi-word preset should have BOS");
}
```

- [ ] **Step 2: Run tests**

Run:
```powershell
cmake --build build/cuda --config Release --target training_labeling_tests
.\build\cuda\Release\training_labeling_tests.exe
```

Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/training_labeling_tests.cpp
git commit -m "test: add preset loader integration tests"
```

---

### Task 5.2: Manual Validation

**Files:** None (manual testing)

- [ ] **Step 1: Test preset builder**

```powershell
py scripts/build_sam3_presets.py --class "test class" --output models/presets/test.json
```

Verify JSON created correctly.

- [ ] **Step 2: Test app loads presets**

1. Run `ai.exe`
2. Go to Training → Label
3. Verify preset dropdown shows: head, body, weapon, upper body, lower body

- [ ] **Step 3: Test preset selection**

1. Select "head" from dropdown
2. Verify prompt field updates to "head"
3. Verify SAM3 initializes

- [ ] **Step 4: Test hot reload**

1. Edit `models/presets/default.json` (add a new class)
2. Verify console shows reload message
3. Verify new class appears in dropdown

- [ ] **Step 5: Test reinit**

1. Select different preset class
2. Verify status shows reinit progress
3. Verify detections change to new class

---

### Task 5.3: Documentation

**Files:**
- Create: `docs/presets.md`

- [ ] **Step 1: Write user guide**

```markdown
# SAM3 Preset User Guide

## Quick Start

1. Run preset builder:
   ```powershell
   py scripts/build_sam3_presets.py --class "head" --class "body" --output models/presets/my_presets.json
   ```

2. In app: Training → Label → Preset File: select `my_presets.json`

3. Select preset class from dropdown

## Creating Custom Presets

See `scripts/build_sam3_presets.py --help` for options.

## Hot Reload

Enable "Hot Reload" checkbox to auto-reload preset file when it changes.
```

- [ ] **Step 2: Commit**

```bash
git add docs/presets.md
git commit -m "docs: add SAM3 preset user guide"
```

---

## Final Verification

- [ ] **Step 1: Full build**

```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: Build succeeds.

- [ ] **Step 2: All tests pass**

```powershell
.\build\cuda\Release\training_labeling_tests.exe
.\build\cuda\Release\detection_debug_tools_tests.exe
```

Expected: Both pass.

- [ ] **Step 3: Manual app test**

Run `ai.exe`, verify all preset features work end-to-end.

- [ ] **Step 4: Create summary commit if needed**

```bash
git commit --allow-empty -m "chore: SAM3 dynamic preset system complete"
```

---

**Plan complete. Ready to execute?**
