# SAM3 Capture Preview Overlay and Debug Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Draw SAM3 boxes and confidence labels on the existing capture preview, add SAM3 detection controls under Training -> Label, and add enough debug instrumentation to diagnose false positives without hiding them.

**Architecture:** Publish one immutable SAM3 preview overlay snapshot that contains a deep-copied frame, the matching result, a frame id, and debug counters. Route SAM3 postprocess settings from config through capture and runtime as snapshotted request data so preview and save jobs use stable settings, then render the matched snapshot in the capture preview path and expose tuning controls in the Training UI.

**Tech Stack:** C++, ImGui, OpenCV `cv::Mat`, existing capture preview rendering in `overlay/draw_debug.cpp`, config persistence in `config.cpp`, SAM3 TensorRT runtime in `training_sam3_runtime.*` and `training_sam3_labeler_trt.cpp`

---

## File Structure

| File | Purpose |
|---|---|
| `sunone_aimbot_2/config/config.h` | Add persistent SAM3 postprocess and overlay toggle fields |
| `sunone_aimbot_2/config/config.cpp` | Initialize, load, clamp, and save new SAM3 config fields |
| `sunone_aimbot_2/training/training_label_types.h` | Add `Sam3PostprocessSettings` and extend queued save request for snapped save-time postprocess settings |
| `sunone_aimbot_2/training/training_dataset_manager.h` | Keep `DetectionBox` shape/confidence model compatible with new decoder flow |
| `sunone_aimbot_2/training/training_sam3_labeler.h` | Add debug-counter/result model definitions shared by runtime and UI |
| `sunone_aimbot_2/training/training_sam3_labeler_stub.cpp` | Keep stub backend in sync with the new inference request/result signatures |
| `sunone_aimbot_2/training/training_sam3_runtime.h` | Add immutable preview overlay snapshot API and request plumbing |
| `sunone_aimbot_2/training/training_sam3_runtime.cpp` | Publish matched preview snapshot, snapshot settings per job, invalidate stale state |
| `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp` | Replace hardcoded postprocess constants with request settings, collect debug counters, sort-before-cap |
| `sunone_aimbot_2/capture/capture.cpp` | Snapshot new SAM3 config values and include them in preview/save submissions |
| `sunone_aimbot_2/overlay/draw_training.cpp` | Add SAM3 Detection controls and debug counters under Training -> Label |
| `sunone_aimbot_2/overlay/draw_debug.cpp` | Render matched SAM3 preview snapshot in capture preview with boxes + confidence labels |
| `tests/training_labeling_tests.cpp` | Add unit/integration coverage for settings snapshotting and decoder result ordering where practical |

---

## Chunk 1: Define shared SAM3 settings and debug models

### Task 1: Add SAM3 postprocess and debug data models

**Files:**
- Modify: `sunone_aimbot_2/training/training_label_types.h`
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`
- Modify: `sunone_aimbot_2/training/training_dataset_manager.h`

- [ ] **Step 1: Add `Sam3PostprocessSettings` to `training_label_types.h`**

Add a shared settings struct with explicit defaults:

```cpp
struct Sam3PostprocessSettings {
    float maskThreshold = 0.5f;
    int minMaskPixels = 64;
    float minConfidence = 0.3f;
    int minBoxWidth = 20;
    int minBoxHeight = 20;
    float minMaskFillRatio = 0.01f;
    int maxDetections = 100;
};
```

- [ ] **Step 2: Extend queued save request to snapshot SAM3 postprocess settings**

Add a field such as:

```cpp
Sam3PostprocessSettings sam3Postprocess;
```

This must live in the queued request object, not be looked up later from live config.

- [ ] **Step 3: Add debug-counter structs to `training_sam3_labeler.h`**

Add:

```cpp
struct Sam3DebugCounters {
    int rawMaskSlots = 0;
    int nonEmptyMasks = 0;
    int afterMinMaskPixels = 0;
    int afterMinBoxWidthHeight = 0;
    int afterMinMaskFillRatio = 0;
    int afterMinConfidence = 0;
    int finalBoxesBeforeCap = 0;
    int finalBoxesRendered = 0;
};
```

and attach it to `Sam3LabelResult`.

- [ ] **Step 4: Update shared SAM3 inference signatures**

Update declarations and adapters so the new unified request/result model is consistent across:

- `Sam3Labeler::LabelFrame(...)`
- `ISam3PreviewBackend`
- runtime adapters
- `training_sam3_labeler_stub.cpp`

- [ ] **Step 5: Confirm `DetectionBox` carries confidence**

Keep or normalize the `DetectionBox` structure so it includes:

```cpp
float confidence = 0.0f;
```

If any constructor/aggregate usage broke after the earlier change, fix those call sites before continuing.

- [ ] **Step 6: Build at the end of Chunk 1 to catch type breakage early**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: build may fail at call sites that still need updating, but the new shared structs should compile cleanly once those references are fixed.

---

## Chunk 2: Add persistent SAM3 config and safe config flow

### Task 2: Persist Training -> Label SAM3 controls

**Files:**
- Modify: `sunone_aimbot_2/config/config.h`
- Modify: `sunone_aimbot_2/config/config.cpp`
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp`

- [ ] **Step 1: Add SAM3 config fields to `config.h`**

Add:

```cpp
float training_sam3_mask_threshold;
int training_sam3_min_mask_pixels;
float training_sam3_min_confidence;
int training_sam3_min_box_width;
int training_sam3_min_box_height;
float training_sam3_min_mask_fill_ratio;
int training_sam3_max_detections;
bool training_sam3_draw_preview_boxes;
bool training_sam3_draw_confidence_labels;
```

- [ ] **Step 2: Add defaults in `config.cpp` constructor/reset path**

Use the values from the approved spec:

- mask threshold `0.5f`
- min mask pixels `64`
- min confidence `0.3f`
- min box width `20`
- min box height `20`
- min fill ratio `0.01f`
- max detections `100`
- draw preview boxes `true`
- draw confidence labels `true`

- [ ] **Step 3: Load and clamp all new fields in `config.cpp`**

Use these exact persisted key names:

- `training_sam3_mask_threshold`
- `training_sam3_min_mask_pixels`
- `training_sam3_min_confidence`
- `training_sam3_min_box_width`
- `training_sam3_min_box_height`
- `training_sam3_min_mask_fill_ratio`
- `training_sam3_max_detections`
- `training_sam3_draw_preview_boxes`
- `training_sam3_draw_confidence_labels`

Clamp using these exact ranges:

- `maskThreshold`: `[0.0, 1.0]`
- `minConfidence`: `[0.0, 1.0]`
- `minMaskFillRatio`: `[0.0, 1.0]`
- `minMaskPixels`: `[0, 1000000]`
- `minBoxWidth`: `[1, 4096]`
- `minBoxHeight`: `[1, 4096]`
- `maxDetections`: `[1, 1000]`

- [ ] **Step 4: Save the new fields in `config.cpp`**

Write all fields back in the same style as existing config persistence.

- [ ] **Step 5: Use `configMutex` as the required safe-write path before wiring widgets**

For all new SAM3 Training -> Label controls, write config values under `configMutex`, then mark config dirty. Do not invent a second config update path for this work.

- [ ] **Step 6: Add Training -> Label controls in `draw_training.cpp`**

Create a new section named `SAM3 Detection` under the existing label UI with:

- `Mask Threshold`
- `Min Mask Pixels`
- `Min Confidence`
- `Min Box Width`
- `Min Box Height`
- `Min Mask Fill Ratio`
- `Max Detections`
- `Draw Preview Boxes`
- `Draw Confidence Labels`

Use the same visual style as `draw_ai.cpp` where practical.

- [ ] **Step 7: Mark config dirty on change**

For every new control, clamp -> write -> mark dirty. Do not rely on incidental persistence.

---

## Chunk 3: Route SAM3 settings and frame identity through capture/runtime

### Task 3: Snapshot preview/save requests correctly

**Files:**
- Modify: `sunone_aimbot_2/capture/capture.cpp`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.h`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.cpp`

- [ ] **Step 1: Add a `Sam3InferenceRequest` model**

Define a request object with:

```cpp
struct Sam3InferenceRequest {
    cv::Mat frame;
    std::string prompt;
    Sam3PostprocessSettings postprocess;
    uint64_t frameId = 0;
};
```

Place it where runtime/backend code can share it cleanly.

- [ ] **Step 2: Add frame id generation in capture path**

When capture submits preview frames in Label mode, assign a monotonically increasing `frameId` to each SAM3 preview submission.

Keep the counter in the capture submission path as the single source of truth for preview frame identity.

- [ ] **Step 3: Extend `CaptureThreadConfig` with new SAM3 fields**

The capture-thread config snapshot in `capture.cpp` must include the new Training -> Label SAM3 fields so capture can snapshot them safely once per loop and pass them into preview/save requests.

- [ ] **Step 4: Snapshot SAM3 postprocess config in `capture.cpp`**

When capture builds training runtime settings and save requests, include the new SAM3 config values as a snapshot.

- [ ] **Step 5: Extend runtime settings/request flow**

`TrainingRuntimeSettings` should continue to hold runtime-level values, but preview and save jobs must carry `Sam3PostprocessSettings` snapshots, not read mutable config later.

Use the same unified `Sam3InferenceRequest` model for both preview inference and save inference. Do not create one request model for preview and a different ad hoc data path for save.

- [ ] **Step 6: Update preview submission path**

Preview submission must hand runtime a complete inference request snapshot, not only a frame.

- [ ] **Step 7: Update save submission path**

Save requests must carry the same postprocess settings snapshot used at enqueue time.

- [ ] **Step 8: Consume snapped save settings at execution time**

When the save worker runs, it must use the `Sam3PostprocessSettings` stored in the queued save request. It must not look up current live config values.

Where practical, convert the queued save work into the same `Sam3InferenceRequest` shape before calling the backend so preview and save share one inference API contract.

- [ ] **Step 9: Invalidate stale preview state on runtime transitions**

When mode changes, engine path changes, preview is disabled, backend becomes unavailable, or runtime shuts down, clear the published preview overlay snapshot.

---

## Chunk 4: Publish an immutable matched preview overlay snapshot

### Task 4: Stop mixing live frame and stale result

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.h`
- Modify: `sunone_aimbot_2/training/training_sam3_runtime.cpp`

- [ ] **Step 1: Add `Sam3PreviewOverlaySnapshot` to runtime-facing types**

Use the approved shape:

```cpp
struct Sam3PreviewOverlaySnapshot {
    cv::Mat frame;
    Sam3LabelResult result;
    uint64_t frameId = 0;
    bool valid = false;
};
```

- [ ] **Step 2: Make the frame a deep-owned snapshot**

At publication time, use `clone()` or equivalent so the snapshot frame does not alias mutable capture storage.

- [ ] **Step 3: Publish snapshot as one unit**

Do not expose separate getters for “latest preview frame” and “latest preview result” for overlay composition.

Provide one API such as:

```cpp
Sam3PreviewOverlaySnapshot GetTrainingLatestPreviewOverlaySnapshot();
```

Return by value under mutex unless there is a strong codebase reason to use shared immutable pointers.

- [ ] **Step 4: Populate snapshot only after preview inference completes**

When the runtime finishes a preview inference successfully or unsuccessfully, publish a complete snapshot containing:

- the exact frame used for that inference
- the exact result from that inference
- the frame id
- validity bit

- [ ] **Step 5: Preserve existing status API**

Keep `GetTrainingRuntimeStatus()` for status text and counts, but make overlay rendering depend on the new snapshot API.

- [ ] **Step 6: Define invalid-snapshot preview behavior explicitly**

In `Label` mode, if there is no valid SAM3 snapshot yet, the capture preview should show no SAM3 overlay and should not compose stale detections over a live capture frame. Prefer showing the last published image only if it is still the current valid snapshot object; otherwise draw the base preview without SAM3 detections.

---

## Chunk 5: Fix decoder flow and collect debug counters

### Task 5: Make SAM3 postprocess evidence-driven

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp`
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`

- [ ] **Step 1: Remove hardcoded postprocess constants from decoder flow**

Use `Sam3PostprocessSettings` passed in with the inference request instead of embedded constants for:

- mask threshold
- min mask pixels
- min box size
- min fill ratio
- min confidence
- max detections

- [ ] **Step 2: Define the confidence formula explicitly**

Version 1 confidence must use the approved derived score from mask statistics:

- average probability of positive pixels
- combined with fill ratio

Keep this formula centralized and obvious.

- [ ] **Step 3: Implement decoder stages in the approved order**

Required order:

1. scan all mask slots
2. compute mask stats, bounds, fill ratio, confidence candidate metrics
3. apply min-pixel / min-box / min-fill-ratio / min-confidence filters
4. collect survivors
5. sort by confidence descending
6. apply `maxDetections`

Do not early-break on raw mask order before sorting.

- [ ] **Step 4: Populate debug counters at each filter stage**

Count:

- raw mask slots
- non-empty masks
- after min mask pixels
- after min width/height
- after min fill ratio
- after min confidence
- final boxes before cap
- final boxes rendered

- [ ] **Step 5: Return counters through `Sam3LabelResult`**

The result object must carry both detections and counters so UI and runtime can inspect the exact same inference outcome.

- [ ] **Step 6: Add targeted test coverage if practical**

At minimum, add a narrow test or helper-driven verification for sort-before-cap ordering. Stage counters may use manual verification if the codebase does not already expose an easy pure-function seam, but sort-before-cap should not remain untested if a small seam can be introduced cleanly.

---

## Chunk 6: Render SAM3 overlay on the existing capture preview path

### Task 6: Draw boxes and confidence labels on capture preview

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_debug.cpp`

- [ ] **Step 1: Inspect `draw_debug_frame()` and locate the image draw call**

Keep the existing preview rendering path intact. Add SAM3 drawing after the preview image is placed and after scale factors are known.

- [ ] **Step 2: In Label mode, use the SAM3 matched snapshot as the preview source**

When `InferenceMode::Label` is active and a valid SAM3 snapshot exists, the preview image should come from the SAM3 snapshot frame, not the current live capture frame.

This is what keeps boxes aligned.

The preview therefore updates at SAM3 preview cadence in Label mode, not at raw capture FPS.

- [ ] **Step 3: Define Label-mode overlay precedence**

When using a valid SAM3 preview snapshot:

- base image: SAM3 snapshot frame
- SAM3 boxes: on if enabled
- SAM3 confidence labels: on if enabled
- YOLO overlay from `detectionBuffer`: off
- future-position overlay: off
- depth-mask overlay: off unless later synchronized to the same snapshot frame

- [ ] **Step 4: Draw boxes using snapshot-frame dimensions**

Map rectangle coordinates using the snapshot frame size and the same preview scale values used by the capture preview image.

- [ ] **Step 5: Draw confidence labels as toggleable overlay text**

Use this exact concise format:

```text
0.73
```

- [ ] **Step 6: Verify clipping and scaling**

Boxes and labels must stay within the preview clip rect and scale correctly when preview zoom/size changes.

---

## Chunk 7: Show SAM3 debug state in Training UI

### Task 7: Expose the evidence in the Label tab

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp`

- [ ] **Step 1: Replace the current text-only SAM3 preview summary with richer status**

Keep the simple ready/initializing text, but add a dedicated debug area for:

- last preview detection count
- raw mask slots
- non-empty masks
- per-stage filter counts

- [ ] **Step 2: Keep the UI concise**

Do not dump dozens of boxes into the Training tab. The visual inspection belongs in the capture preview. The Training tab should show controls and counters.

- [ ] **Step 3: Show current postprocess values if helpful**

Show these exact debug rows in Training -> Label:

- `Raw mask slots`
- `Non-empty masks`
- `After min mask pixels`
- `After min width/height`
- `After min fill ratio`
- `After min confidence`
- `Final boxes before cap`
- `Final boxes rendered`

---

## Chunk 8: Verification

### Task 8: Build and validate behavior end to end

**Files:**
- Test: `build/cuda/Release/ai.exe`
- Test: `build/cuda/Release/training_labeling_tests.exe`

- [ ] **Step 1: Build CUDA project**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

Expected: `ai.exe` and `training_labeling_tests.exe` build successfully.

- [ ] **Step 2: Run training labeling tests**

Run:

```powershell
build/cuda/Release/training_labeling_tests.exe
```

Expected: New SAM3-related compile/runtime checks pass. If the known pre-existing dataset-name assertion still fails, record it explicitly instead of misattributing it to this work.

- [ ] **Step 3: Manual preview verification**

Run `ai.exe` and verify:

1. Switch to `Label` mode
2. Capture preview now shows SAM3 frame snapshot, not mixed live frame + stale boxes
3. Boxes render on the preview image
4. Confidence labels toggle on/off
5. Changing SAM3 controls changes future preview results
6. Training tab shows debug counters

- [ ] **Step 4: Manual false-positive diagnosis check**

With no visible person, inspect whether counters reveal the source of the noise:

- lots of raw masks but few survivors
- lots of survivors before confidence
- lots of survivors even after confidence

Document the observed pattern before deciding on any second-round decoder changes.

- [ ] **Step 5: Verify snapshot invalidation**

Check these transitions:

- Label -> Detect
- Detect -> Label
- engine path change
- preview disabled
- backend unavailable

Expected: stale SAM3 preview overlay does not remain visible.

---

## Notes for execution

- Do not reintroduce ad hoc text-only detection rendering as a substitute for the preview overlay.
- Do not use `maxDetections` as the main explanation for false positives.
- Do not mix YOLO and SAM3 overlays in Label mode unless they are explicitly synchronized to the same frame source.
- Keep frame/result ownership simple: one published snapshot, one consumer path.
- If a test is missing for a critical behavior, add the narrowest useful test instead of broad refactoring.
