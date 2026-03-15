# SAM3 Training Mode Switch Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real `Detect | Label | Paused` inference mode, route live frames to SAM3 when label mode is active, stop YOLO/DML live inference in that mode, and support hotkey-driven labeled sample saves.

**Architecture:** Keep the current detector threads intact for `Detect`, but centralize backend ownership in one shared runtime mode. Add a persistent SAM3 training runtime that mirrors the detector flow: it owns engine initialization, preview inference, preview results, and a save queue. Keep detector results and training preview results in separate buffers so gameplay and labeling state do not interfere.

**Tech Stack:** C++17, existing config and overlay systems, TensorRT/CUDA behind `USE_CUDA`, DirectML detector path, OpenCV, existing `training/` runtime helpers, standalone test executable pattern in `tests/training_labeling_tests.cpp`.

---

## File Map

**Create:**
- `sunone_aimbot_2/training/training_inference_mode.h` - shared inference mode enum and helpers
- `sunone_aimbot_2/training/training_sam3_runtime.h` - persistent SAM3 runtime API for preview and save jobs
- `sunone_aimbot_2/training/training_sam3_runtime.cpp` - mode-aware SAM3 runtime, worker thread, preview cache, save queue

**Modify:**
- `sunone_aimbot_2/config/config.h` - remove `training_label_enabled` as a runtime-routing control, add explicit training mode settings if needed
- `sunone_aimbot_2/config/config.cpp` - load/save updated training mode fields and defaults
- `sunone_aimbot_2/overlay/draw_training.cpp` - replace old enable checkbox with mode control, show active backend state, hook training runtime status
- `sunone_aimbot_2/capture/capture.cpp` - route frames by active inference mode and enqueue label saves through training runtime
- `sunone_aimbot_2/detector/trt_detector.cpp` - honor `Label` mode like `Paused`, clear detector output on mode changes if needed
- `sunone_aimbot_2/detector/dml_detector.cpp` - honor `Label` mode like `Paused`, clear detector output on mode changes if needed
- `sunone_aimbot_2/training/training_label_runtime.h` - expose or integrate the new runtime ownership boundary cleanly
- `sunone_aimbot_2/training/training_label_runtime.cpp` - delegate preview/save ownership to persistent SAM3 runtime where appropriate
- `sunone_aimbot_2/training/training_sam3_labeler.h` - define real initialization/status API for persistent backend use
- `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp` - load TensorRT engine, create context and CUDA stream, surface exact init failures
- `sunone_aimbot_2/training/training_sam3_labeler_stub.cpp` - mirror new API with stable stub behavior
- `sunone_aimbot_2/sunone_aimbot_2.cpp` - define shared active inference mode state and any global coordination points
- `sunone_aimbot_2/keyboard/keyboard_listener.cpp` - map pause hotkey onto `Paused` instead of competing booleans
- `tests/training_labeling_tests.cpp` - add TDD coverage for mode transitions, save-queue snapshots, and status behavior
- `CMakeLists.txt` - compile new runtime files into app and tests target

**Reference While Implementing:**
- `docs/superpowers/specs/2026-03-14-sam3-training-mode-switch-design.md`
- `docs/superpowers/specs/2026-03-13-sam3-training-label-design.md`
- `sunone_aimbot_2/detector/trt_detector.cpp`
- `sunone_aimbot_2/detector/dml_detector.cpp`
- `sunone_aimbot_2/capture/capture.cpp`
- `sunone_aimbot_2/training/training_label_runtime.cpp`
- `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

## Chunk 1: Inference Mode Foundation

### Task 1: Add a shared inference mode and lock down transition rules

**Files:**
- Create: `sunone_aimbot_2/training/training_inference_mode.h`
- Modify: `sunone_aimbot_2/sunone_aimbot_2.cpp`
- Modify: `sunone_aimbot_2/keyboard/keyboard_listener.cpp`
- Modify: `tests/training_labeling_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing mode-transition behavior test**

```cpp
Check(training::CanPauseFromMode(training::InferenceMode::Detect), "detect mode should allow pause transition");
Check(!training::CanPauseFromMode(training::InferenceMode::Label), "label mode pause hotkey should not create conflicting ownership");
Check(training::ShouldDetectorProcessFrames(training::InferenceMode::Detect), "detect mode should own detector frames");
Check(!training::ShouldDetectorProcessFrames(training::InferenceMode::Label), "label mode should not own detector frames");
Check(!training::ShouldDetectorProcessFrames(training::InferenceMode::Paused), "paused mode should not own detector frames");
```

- [ ] **Step 2: Run the test target to verify it fails**

Run: `cmake --build "build\cuda" --config Release --target training_labeling_tests`
Expected: FAIL with missing `training_inference_mode.h` or missing enum/helpers.

- [ ] **Step 3: Add the minimum shared inference mode header**

```cpp
namespace training {

enum class InferenceMode {
    Detect,
    Label,
    Paused,
};

const char* ToInferenceModeName(InferenceMode mode);

}  // namespace training
```

- [ ] **Step 4: Wire one shared `activeInferenceMode` state**

Use `Detect` as startup default. Map the pause hotkey to toggle between `Paused` and `Detect` only when label mode is not active.

- [ ] **Step 5: Re-run the test target and make sure the mode test passes**

Run: `cmake --build "build\cuda" --config Release --target training_labeling_tests`
Expected: PASS

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: PASS with exit code `0`

## Chunk 2: Route Detector Ownership By Mode

### Task 2: Make YOLO and DML honor `Label` mode like `Paused`

**Files:**
- Modify: `sunone_aimbot_2/detector/trt_detector.cpp`
- Modify: `sunone_aimbot_2/detector/dml_detector.cpp`
- Modify: `sunone_aimbot_2/capture/capture.cpp`
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing test for detector suppression and exclusive ownership across `Detect -> Label -> Paused -> Detect`**

```cpp
Check(training::ShouldDetectorProcessFrames(training::InferenceMode::Detect), "detect mode should process detector frames");
Check(!training::ShouldDetectorProcessFrames(training::InferenceMode::Label), "label mode should suppress detector frames");
Check(!training::ShouldDetectorProcessFrames(training::InferenceMode::Paused), "paused mode should suppress detector frames");
Check(training::HasExclusiveBackendOwnership(training::InferenceMode::Detect), "detect mode should have one active backend owner");
Check(training::HasExclusiveBackendOwnership(training::InferenceMode::Label), "label mode should have one active backend owner");
Check(training::HasExclusiveBackendOwnership(training::InferenceMode::Paused), "paused mode should have zero or one stable owner state");
Check(training::EnteringLabelModeClearsDetectorStateForTests(), "entering label mode should clear detector buffer and bump version");
```

- [ ] **Step 2: Run the test binary to verify it fails first**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: FAIL with missing helper or wrong mode-routing behavior.

- [ ] **Step 3: Implement one helper that answers detector ownership**

Put the helper next to the inference mode enum so both detector backends and capture share the same routing rule.

- [ ] **Step 4: Update TensorRT and DML detector paths to treat `Label` like `Paused`**

When mode is `Label` or `Paused`:
- clear `detectionBuffer`
- increment version
- notify waiters
- skip live detector processing

- [ ] **Step 5: Update capture frame routing to stop feeding detector paths in `Label` mode**

Do not let capture enqueue the same frame into both detector and SAM3 preview paths.

- [ ] **Step 6: Re-run tests and build**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: PASS

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS

## Chunk 3: Persistent SAM3 Backend Initialization

### Task 3: Turn `Sam3Labeler` into a real initialized backend with minimal preview inference support

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`
- Modify: `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp`
- Modify: `sunone_aimbot_2/training/training_sam3_labeler_stub.cpp`
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing status test for engine-path validation**

```cpp
training::Sam3Labeler labeler;
const auto avail = labeler.GetAvailabilityForTests();
Check(!avail.ready, "fresh stub labeler should not be ready before init");
Check(!avail.message.empty(), "availability message should explain backend state");
```

- [ ] **Step 2: Run the tests to verify they fail for the new API**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: FAIL with missing initialization/status behavior.

- [ ] **Step 3: Add explicit initialize/shutdown semantics to `Sam3Labeler`**

Minimum API shape:

```cpp
bool Initialize(const std::string& enginePath);
void Shutdown();
Sam3Availability GetAvailability() const;
```

- [ ] **Step 4: Implement real TensorRT initialization in the CUDA backend**

Use the `SAM3TensorRT` reference pattern to:
- validate file exists and is non-empty
- create TensorRT runtime
- deserialize engine
- create execution context
- create CUDA stream
- store exact failure message on any error

- [ ] **Step 5: Implement the minimum preview inference path needed by the live SAM3 runtime**

The first implementation may be narrow, but it must be real enough for preview-runtime wiring:
- accept frame plus prompt
- execute the backend path needed for preview
- return a stable success or failure result
- surface exact runtime errors instead of fake success

- [ ] **Step 6: Re-run tests and CUDA build**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: PASS

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS

## Chunk 4: SAM3 Live Preview Runtime

### Task 4: Add a persistent SAM3 training runtime that mirrors detector flow

**Files:**
- Create: `sunone_aimbot_2/training/training_sam3_runtime.h`
- Create: `sunone_aimbot_2/training/training_sam3_runtime.cpp`
- Modify: `sunone_aimbot_2/training/training_label_runtime.h`
- Modify: `sunone_aimbot_2/training/training_label_runtime.cpp`
- Modify: `tests/training_labeling_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test for frame-routing, preview ownership, and dirty reinit behavior**

```cpp
Check(training::ShouldRouteFrameToTrainingRuntime(training::InferenceMode::Label), "label mode should route frames to training runtime");
Check(!training::ShouldRouteFrameToTrainingRuntime(training::InferenceMode::Detect), "detect mode should not route frames to training runtime");
Check(!training::ShouldRouteFrameToTrainingRuntime(training::InferenceMode::Paused), "paused mode should not route frames to training runtime");
Check(training::ShouldReinitializeSam3OnEnginePathChange(training::InferenceMode::Label), "label mode should reinitialize SAM3 on engine path changes");
Check(training::EnginePathChangeDropsOldOwnershipBeforeReinitForTests(), "engine-path changes in label mode must drop old backend ownership before publishing new ready state");
```

- [ ] **Step 2: Build tests and verify they fail**

Run: `cmake --build "build\cuda" --config Release --target training_labeling_tests`
Expected: FAIL with missing preview runtime types.

- [ ] **Step 3: Add the runtime types and thread skeleton**

The runtime should own:
- current backend instance
- current prompt/settings snapshot
- latest frame snapshot
- preview worker thread
- preview result cache
- save queue

The runtime owns the training preview cache and training status fields. `detectionBuffer` remains detector-only. `draw_training.cpp` reads the training runtime cache only.

- [ ] **Step 4: Route capture frames into SAM3 preview runtime only in `Label` mode**

Make sure only the latest frame is kept for preview work. Do not build an unbounded preview queue.

- [ ] **Step 5: Make SAM3 runtime own backend init, shutdown, and dirty reinit**

When mode changes:
- entering `Label` initializes backend if needed
- leaving `Label` stops preview ownership cleanly
- changing engine path during `Label` marks the backend dirty and triggers async reinit
- old backend must stop owning inference before new readiness becomes active

- [ ] **Step 6: Add status accessors for UI consumption**

Expose:
- active mode
- backend ready/error state
- last preview inference time
- last preview detection count
- last save result

- [ ] **Step 7: Re-run test target and app build**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: PASS

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS

## Chunk 5: Hotkey Save Path Through SAM3 Runtime

### Task 5: Save frozen labeled frames through the persistent training runtime

**Files:**
- Modify: `sunone_aimbot_2/capture/capture.cpp`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.h`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.cpp`
- Modify: `sunone_aimbot_2/training/training_label_runtime.cpp`
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing save behavior test for frozen frame plus settings pairing**

```cpp
auto saveBehavior = training::EvaluateQueuedSaveSnapshotForTests(
    /*initialPrompt=*/"player",
    /*initialClass=*/"player",
    /*mutatedPrompt=*/"weapon",
    /*mutatedClass=*/"weapon",
    /*saveNegatives=*/false);
Check(saveBehavior.usedPrompt == "player", "queued save must keep original prompt snapshot");
Check(saveBehavior.usedClass == "player", "queued save must keep original class snapshot");
Check(!saveBehavior.savedImage && !saveBehavior.savedLabel, "no detections with negative-save off must save nothing");

auto negativeBehavior = training::EvaluateQueuedSaveSnapshotForTests(
    /*initialPrompt=*/"player",
    /*initialClass=*/"player",
    /*mutatedPrompt=*/"weapon",
    /*mutatedClass=*/"weapon",
    /*saveNegatives=*/true);
Check(negativeBehavior.savedImage, "no detections with negative-save on must save image");
Check(!negativeBehavior.savedLabel, "no detections with negative-save on must not save label");
```

- [ ] **Step 2: Run the tests and verify they fail before save-path edits**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: FAIL with missing runtime-owned save path behavior.

- [ ] **Step 3: Make capture hotkey handling enqueue into `TrainingSam3Runtime`**

Do not let capture instantiate its own local runtime owner. Capture should only:
- snapshot frame
- snapshot current label settings
- enqueue one save request

- [ ] **Step 4: Let the SAM3 runtime own save processing**

For each save request:
- run SAM3 on the frozen frame
- convert masks to detection boxes
- call dataset manager write path
- update last-save status

- [ ] **Step 5: Preserve existing negative-save behavior**

If there are no detections:
- save image only when negative-save is enabled
- otherwise report miss and save nothing

- [ ] **Step 6: Re-run tests and verify save-path logic still passes**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: PASS

## Chunk 6: Overlay And Mode UX

### Task 6: Replace the old enable toggle with a real mode switch and live SAM3 status

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp`
- Modify: `sunone_aimbot_2/config/config.h`
- Modify: `sunone_aimbot_2/config/config.cpp`
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write the failing config/default test for training mode fields and routing ownership**

```cpp
Config cfg;
cfg.loadConfig("nonexistent-test-config.ini");
Check(cfg.training_sam3_engine_path == "models/sam3_fp16.engine", "default engine path mismatch");
Check(training::ShouldDetectorProcessFrames(training::InferenceMode::Detect), "mode control should own routing");
cfg.training_label_enabled = true;
Check(training::ShouldDetectorProcessFrames(training::InferenceMode::Detect), "old training_label_enabled flag must not change detect routing");
```

- [ ] **Step 2: Run the tests to verify the UI/config expectations fail first if needed**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: FAIL if config or mode control assumptions are not yet true.

- [ ] **Step 3: Replace the old `Enable Training Label` checkbox with explicit mode control**

Recommended UI labels:
- `Detect`
- `Label`
- `Paused`

Show current backend state and last preview/save status under the mode control.

- [ ] **Step 4: Remove `training_label_enabled` as a runtime-routing control**

Do not leave both the old enable flag and the new mode switch deciding ownership. Mode control must be the only routing source.

- [ ] **Step 5: Re-run build and interactive smoke test**

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS

Run manually:
- launch `build\cuda\Release\ai.exe`
- confirm startup is in `Detect`
- switch to `Training -> Label`
- change mode to `Label`
- confirm normal detector updates stop
- confirm SAM3 backend status is shown
- switch back to `Detect`
- confirm normal detector updates resume

## Chunk 7: Final Verification

### Task 7: Verify mode ownership, SAM3 init, and no-regression behavior

**Files:**
- No planned source changes unless fixes are needed

- [ ] **Step 1: Run the standalone training tests**

Run: `.uild\cuda\Release\training_labeling_tests.exe`
Expected: PASS with exit code `0`

- [ ] **Step 2: Run the CUDA build**

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS and emit `build/cuda/Release/ai.exe`

- [ ] **Step 3: Manual mode-switch verification**

Verify:
- app starts in `Detect`
- `Label` mode clears detector output
- `Label` mode does not keep YOLO or DML live updates running
- `Paused` blocks both backends
- returning to `Detect` resumes detector updates

- [ ] **Step 4: Manual SAM3 verification**

Verify:
- valid engine path reports ready state in `Label`
- invalid engine path reports exact failure message
- changing engine path in `Label` triggers clean reinitialization

- [ ] **Step 5: Manual save verification**

Verify:
- label hotkey saves a frozen frame plus label file
- negative-save option still behaves correctly
- UI shows last saved paths and status
