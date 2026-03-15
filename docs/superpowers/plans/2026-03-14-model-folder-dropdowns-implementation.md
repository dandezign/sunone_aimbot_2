# Model Folder Dropdowns Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Core AI and SAM3 model selection read from the executable-adjacent `models` folder, with SAM3 using a dropdown of `sam3*.engine` files.

**Architecture:** Add one shared executable-relative model-folder resolution path in a non-UI helper layer and reuse it for discovery, startup fallback, overlay dropdowns, and runtime settings. Keep `config.ai_model` filename-only, keep `config.training_sam3_engine_path` as `models/<filename>`, and filter Core AI and SAM3 model lists with explicit rules.

**Tech Stack:** C++17, ImGui overlay, existing config system, `std::filesystem`, standalone `training_labeling_tests`, CUDA/DML app builds.

---

## File Map

**Create:**
- none expected

**Modify:**
- `sunone_aimbot_2/include/other_tools.h` - declare shared executable-relative model-folder helpers and focused filters
- `sunone_aimbot_2/scr/other_tools.cpp` - implement shared executable-relative model discovery used by UI and startup/runtime code
- `sunone_aimbot_2/overlay/overlay.cpp` - use the shared list helpers instead of owning discovery logic
- `sunone_aimbot_2/overlay/draw_ai.cpp` - keep Core AI dropdown but feed it from the shared helper behavior
- `sunone_aimbot_2/overlay/draw_training.cpp` - replace SAM3 text path with dropdown and empty-state UI
- `sunone_aimbot_2/training/training_label_runtime.h` - consume shared model-folder helpers in runtime/config-facing code as needed
- `sunone_aimbot_2/training/training_label_runtime.cpp` - resolve runtime SAM3 engine fallback from executable-relative model folder if saved entry is missing
- `sunone_aimbot_2/sunone_aimbot_2.cpp` - align startup detector model fallback with the same model-folder helper/path rules if needed
- `tests/training_labeling_tests.cpp` - add TDD coverage for filename/path contracts and dropdown-filter behavior

**Reference:**
- `docs/superpowers/specs/2026-03-14-model-folder-dropdowns-design.md`
- `sunone_aimbot_2/include/other_tools.h`
- `sunone_aimbot_2/scr/other_tools.cpp`
- `sunone_aimbot_2/overlay/draw_ai.cpp`
- `sunone_aimbot_2/overlay/draw_training.cpp`
- `sunone_aimbot_2/overlay/overlay.cpp`
- `sunone_aimbot_2/sunone_aimbot_2.cpp`
- `sunone_aimbot_2/training/training_label_runtime.cpp`

## Chunk 1: Shared Model Discovery

### Task 1: Add one source of truth for executable-relative model discovery

**Files:**
- Modify: `sunone_aimbot_2/include/other_tools.h`
- Modify: `sunone_aimbot_2/scr/other_tools.cpp`
- Modify: `sunone_aimbot_2/overlay/overlay.cpp`
- Modify: `sunone_aimbot_2/training/training_label_runtime.h`
- Modify: `sunone_aimbot_2/training/training_label_runtime.cpp`
- Test: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
const auto exePath = training::GetTrainingRuntimeExecutablePath();
const auto modelsDir = GetExecutableModelsDirectoryForTests();
Check(modelsDir == exePath.parent_path() / "models", "models directory should resolve next to ai.exe");
Check(ShouldIncludeCoreAiModelForTests("yolo26n.engine"), "detector engine should stay visible");
Check(!ShouldIncludeCoreAiModelForTests("sam3_fp16.engine"), "sam3 engine should stay out of Core AI list");
Check(ShouldIncludeSam3EngineForTests("sam3_fp16.engine"), "sam3 engine should appear in SAM3 list");
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build "build\cuda" --config Release --target training_labeling_tests`
Expected: FAIL with missing helper or missing include.

- [ ] **Step 3: Add shared executable-relative model-folder resolution**

Implement one helper path contract in the shared helper layer used by overlay and runtime code:

```cpp
std::filesystem::path GetExecutableModelsDirectory();
```

- [ ] **Step 4: Add focused list helpers**

Rules:
- Core AI keeps current backend-aware detector list semantics
- Core AI excludes basenames starting with `sam3`
- SAM3 list includes only `.engine` files whose basename starts with `sam3`

- [ ] **Step 5: Run tests to verify it passes**

Run: `cmake --build "build\cuda" --config Release --target training_labeling_tests`
Expected: PASS

Run: `build\cuda\Release\training_labeling_tests.exe`
Expected: PASS

## Chunk 2: Core AI Dropdown Alignment

### Task 2: Keep Core AI dropdown, but back it with the shared model-folder rules

**Files:**
- Modify: `sunone_aimbot_2/scr/other_tools.cpp`
- Modify: `sunone_aimbot_2/overlay/overlay.cpp`
- Modify: `sunone_aimbot_2/overlay/draw_ai.cpp`
- Modify: `sunone_aimbot_2/sunone_aimbot_2.cpp`
- Test: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing behavior test**

```cpp
Check(training::ShouldIncludeCoreAiModelForTests("yolo26n.engine"), "detector engine should stay visible");
Check(training::ShouldIncludeCoreAiModelForTests("yolo26n.onnx"), "detector onnx should stay visible");
Check(!training::ShouldIncludeCoreAiModelForTests("sam3_fp16.engine"), "sam3 engine should not appear in Core AI dropdown");
Check(training::ResolveDetectorModelConfigValueForTests("yolo26n.engine") == "yolo26n.engine", "detector config value should stay filename-only");
```

- [ ] **Step 2: Run test to verify it fails**

Run: `build\cuda\Release\training_labeling_tests.exe`
Expected: FAIL with missing filter helper or wrong inclusion behavior.

- [ ] **Step 3: Implement the minimal filter and keep dropdown behavior**

Keep `config.ai_model` filename-only. Keep the Core AI ImGui combo. Change only the discovery source and filtering.

- [ ] **Step 4: Align startup fallback with the same rules**

If the saved detector model is missing:
- select the first discovered non-SAM3 detector model
- persist/use filename-only value
- keep backend-aware detector list semantics intact

- [ ] **Step 5: Run tests and build**

Run: `build\cuda\Release\training_labeling_tests.exe`
Expected: PASS

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS

## Chunk 3: SAM3 Dropdown

### Task 3: Replace SAM3 text input with a dropdown of `sam3*.engine`

**Files:**
- Modify: `sunone_aimbot_2/scr/other_tools.cpp`
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp`
- Modify: `sunone_aimbot_2/overlay/overlay.cpp`
- Modify: `sunone_aimbot_2/training/training_label_runtime.cpp`
- Test: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing behavior test**

```cpp
Check(training::ShouldIncludeSam3EngineForTests("sam3_fp16.engine"), "sam3 engine should appear in SAM3 dropdown");
Check(!training::ShouldIncludeSam3EngineForTests("yolo26n.engine"), "detector engine should not appear in SAM3 dropdown");
Check(!training::ShouldIncludeSam3EngineForTests("sam3_dynamic.onnx"), "sam3 onnx should not appear in SAM3 runtime dropdown");
Check(training::ResolveSam3EngineConfigValueForTests("sam3_fp16.engine") == "models/sam3_fp16.engine", "sam3 config value should persist as models/<filename>");
```

- [ ] **Step 2: Run test to verify it fails**

Run: `build\cuda\Release\training_labeling_tests.exe`
Expected: FAIL with missing helper or wrong filter behavior.

- [ ] **Step 3: Replace the SAM3 text field with a dropdown**

Behavior:
- display filenames only
- persist `config.training_sam3_engine_path` as `models/<filename>`
- if no SAM3 engines exist, show a clear empty-state message instead of a combo

- [ ] **Step 4: Add runtime fallback for missing saved SAM3 entry**

Rules:
- if saved `models/<filename>` exists in the discovered SAM3 list, keep it
- otherwise select and persist the first discovered SAM3 engine
- if none exist, do not persist a replacement value; pass empty engine path to runtime for the session and show empty state

- [ ] **Step 5: Add fallback and empty-state regression checks**

Add focused tests for:
- missing saved detector entry falls back to first valid detector model
- missing saved SAM3 entry falls back to first valid SAM3 engine
- no SAM3 engines yields empty runtime engine path for the session

- [ ] **Step 6: Run tests and build**

Run: `build\cuda\Release\training_labeling_tests.exe`
Expected: PASS

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS

## Chunk 4: Final Verification

### Task 4: Verify integrated model-dropdown behavior

**Files:**
- No new source files expected unless fixes are needed

- [ ] **Step 1: Run focused tests**

Run: `build\cuda\Release\training_labeling_tests.exe`
Expected: PASS

- [ ] **Step 2: Run CUDA build**

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS and emit `build/cuda/Release/ai.exe`

- [ ] **Step 3: Manual UI check**

Run manually:
- launch `build\cuda\Release\ai.exe`
- open `Core -> AI`
- confirm dropdown shows detector models from `models` beside `ai.exe`
- confirm `sam3*.engine` is excluded there
- change the Core AI selection and confirm config updates plus detector reload behavior
- open `Training -> Label`
- confirm SAM3 dropdown shows only `sam3*.engine`
- change the SAM3 engine selection and confirm the status path updates cleanly
- verify empty `models` folder behavior shows the Core AI no-model message and the SAM3-specific no-engine message
