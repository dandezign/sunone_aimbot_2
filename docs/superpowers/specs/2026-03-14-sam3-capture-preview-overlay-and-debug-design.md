# SAM3 Capture Preview Overlay and Detection Debug Design

**Date:** 2026-03-14  
**Status:** Proposed and user-approved  
**Priority:** High  

## Goal

Show SAM3 detections visually on the existing capture preview path, add SAM3-specific postprocess controls under `Training -> Label`, and instrument the SAM3 detection pipeline so false positives can be diagnosed instead of hidden.

## Problem Statement

SAM3 preview is now initializing and producing detections, but the current workflow still has two major problems:

1. The user cannot see SAM3 detections on the live preview in the same way they can inspect the existing capture preview.
2. SAM3 can report large numbers of detections even when no person is visible, and the current implementation does not expose enough tuning or debug information to determine whether the problem is thresholds, poor postprocess logic, or a deeper decoding mistake.

The current Training tab only shows text status. That is not sufficient for debugging or validating SAM3.

## Key Decisions

### 1. Use the existing capture preview path

SAM3 detections will be drawn on the current capture preview rendering path in `overlay/draw_debug.cpp`, not in a separate Training-only preview.

This matches the existing user workflow and ensures the user can inspect SAM3 output on the same visual surface already used for capture debugging.

### 2. Draw boxes and confidence labels

The first visual version will draw:

- bounding boxes
- confidence text labels

Confidence text will be toggleable from the Training UI.

### 3. Do not trust the current frame/result pairing

The current code draws the capture preview from a live frame source while SAM3 preview results are produced asynchronously on a worker thread. A SAM3 result can belong to an older frame than the preview image currently on screen.

To avoid misaligned boxes, the overlay must render one immutable matched preview snapshot:

- preview frame snapshot
- preview result
- `frameId`
- validity flag

The overlay must never assemble frame and result from separate getters.

### 4. Do not use max detections as the main fix

`Max detections` remains a useful exposed control, but it must not serve as the main solution to false positives.

The deeper goal is to understand why SAM3 is producing too many detections and expose enough data to fix that behavior correctly.

## Architecture

### A. Matched SAM3 preview snapshot

Add a single immutable snapshot model for the capture-preview overlay path. This snapshot must bundle together everything needed to draw a correct SAM3 view:

```cpp
struct Sam3PreviewOverlaySnapshot {
    cv::Mat frame;
    Sam3LabelResult result;
    uint64_t frameId = 0;
    bool valid = false;
};
```

The runtime will publish this snapshot after preview inference completes. The overlay will consume this snapshot instead of mixing a live frame from one source with detections from another.

The published snapshot frame must be a deep-owned image copy at publication time. It must not alias mutable backing storage that can later be overwritten by capture.

### Publication rule

The runtime must publish the entire snapshot as one unit. The overlay must read the entire snapshot as one unit.

Acceptable implementation patterns:

- return-by-value under runtime mutex
- atomic/shared publication of `std::shared_ptr<const Sam3PreviewOverlaySnapshot>`

The design must not rely on backend generation for frame identity. `frameId` is diagnostic frame identity only.

### Label-mode preview behavior

In `Label` mode, the capture preview image becomes the SAM3-synchronized snapshot frame. That means the preview updates at the SAM3 preview cadence, not at raw capture FPS.

### B. SAM3 postprocess settings

Create one explicit settings struct for SAM3 preview/save postprocess behavior:

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

This struct should travel through runtime as a snapshot, not as mutable shared backend state.

### Clamp ranges

All values must be clamped both in UI and when loading config:

- `maskThreshold`: `[0.0, 1.0]`
- `minConfidence`: `[0.0, 1.0]`
- `minMaskFillRatio`: `[0.0, 1.0]`
- `minMaskPixels`: `[0, 1000000]`
- `minBoxWidth`: `[1, 4096]`
- `minBoxHeight`: `[1, 4096]`
- `maxDetections`: `[1, 1000]`

### C. Inference request model

Define one request model that can be used for preview inference and save inference:

```cpp
struct Sam3InferenceRequest {
    cv::Mat frame;
    std::string prompt;
    Sam3PostprocessSettings postprocess;
    uint64_t frameId = 0;
};
```

The backend API should accept this request object rather than loose frame/prompt arguments.

### D. Preview/save job snapshotting

Both preview and save work items should use the settings active at submission time.

That means preview inference must snapshot:

- frame
- prompt
- postprocess settings
- frame id

Queued save work should also snapshot postprocess settings so that delayed save operations do not silently use newer UI values.

`QueuedSaveRequest` must therefore be extended to carry `Sam3PostprocessSettings`.

### E. Decoder output diagnostics

Add a structured debug block to the SAM3 result model:

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

Attach this to the inference result or preview snapshot so the overlay and Training UI can display the exact filtering path.

### F. Decoder ordering

The SAM3 decoder must process masks in this order:

1. scan all mask slots
2. compute mask stats, box bounds, fill ratio, and confidence candidate metrics
3. apply `minMaskPixels`, `minBoxWidth`/`minBoxHeight`, `minMaskFillRatio`, and `minConfidence`
4. collect survivors
5. sort by confidence descending
6. apply `maxDetections`

This prevents mask order from deciding which detections survive.

### Confidence model for version 1

For the first implementation, SAM3 confidence will use the currently derived mask-statistics score:

- average probability of positive pixels
- combined with mask fill ratio

This confidence model is accepted as a practical v1 debug score and filter input. It is not treated as a final proof that the SAM3 decoding model is correct.

The UI and debug counters should make it possible to revisit this confidence model later if evidence shows it is not meaningful enough.

## UI Design

### Training -> Label -> SAM3 Detection

Add a new section under the existing Training label UI with controls similar in style to the Core AI detection controls.

Controls:

- `Mask Threshold`
- `Min Mask Pixels`
- `Min Confidence`
- `Min Box Width`
- `Min Box Height`
- `Min Mask Fill Ratio`
- `Max Detections`
- `Draw Preview Boxes`
- `Draw Confidence Labels`

These controls should:

- persist in config
- clamp invalid values to safe ranges
- write through a thread-safe config update path
- mark config dirty when changed
- update preview behavior without requiring restart

### Config write rule

SAM3 UI changes must not write unsynchronized data into `config`. Either:

- take `configMutex` while writing SAM3 config values, or
- route updates through a thread-safe helper that applies the change and marks config dirty

The same rule applies to all new Training -> Label SAM3 controls.

### Training status/debug display

The Training tab should continue to show high-level runtime status, but it should also show SAM3 debug counters:

- raw masks
- masks after each filter stage
- final rendered detections

This makes it possible to distinguish between:

- broad weak activations
- geometric noise
- confidence scoring issues
- a more fundamental decoder problem

## Overlay Rendering Design

### Render conditions

Draw SAM3 detections on the capture preview only when:

- current inference mode is `Label`
- SAM3 backend is ready
- preview snapshot is valid
- overlay is using the matched published snapshot
- `Draw Preview Boxes` is enabled

### Render contents

For each detection:

- draw a box outline over the preview image
- draw confidence text if `Draw Confidence Labels` is enabled

The overlay must map box coordinates using the preview snapshot frame dimensions, not a different live frame.

### Overlay layering rules in Label mode

When `Label` mode is active and a valid SAM3 preview snapshot exists:

- base preview image: `Sam3PreviewOverlaySnapshot.frame`
- SAM3 boxes: on
- SAM3 confidence labels: toggleable
- YOLO detection overlay from `detectionBuffer`: off
- future-position overlay: off
- depth-mask overlay: off unless later converted to use the same snapshot frame

This avoids mixing overlays sourced from different frames.

## Config Design

Add persistent config fields:

- `training_sam3_mask_threshold`
- `training_sam3_min_mask_pixels`
- `training_sam3_min_confidence`
- `training_sam3_min_box_width`
- `training_sam3_min_box_height`
- `training_sam3_min_mask_fill_ratio`
- `training_sam3_max_detections`
- `training_sam3_draw_preview_boxes`
- `training_sam3_draw_confidence_labels`

These must be defined in `config.h`, loaded in `config.cpp`, saved in `config.cpp`, and initialized with defaults.

Any invalid or missing persisted value must be clamped back to its safe default range during config load.

## Files Expected to Change

### Overlay / UI
- `sunone_aimbot_2/overlay/draw_debug.cpp`
- `sunone_aimbot_2/overlay/draw_training.cpp`

### Config
- `sunone_aimbot_2/config/config.h`
- `sunone_aimbot_2/config/config.cpp`

### Capture / settings propagation
- `sunone_aimbot_2/capture/capture.cpp`

### Runtime
- `sunone_aimbot_2/training/training_sam3_runtime.h`
- `sunone_aimbot_2/training/training_sam3_runtime.cpp`
- `sunone_aimbot_2/training/training_label_types.h`

### SAM3 decoding
- `sunone_aimbot_2/training/training_sam3_labeler.h`
- `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp`

## Risks and Mitigations

### Risk: frame/result mismatch

If overlay draws results on a different frame than the one SAM3 processed, boxes will appear wrong even when detections are correct.

**Mitigation:** publish and draw a matched preview snapshot.

### Risk: settings race during save/preview

If the UI changes postprocess values while work is queued, preview and save behavior can become inconsistent.

**Mitigation:** snapshot SAM3 postprocess settings per job.

### Risk: unsynchronized config writes

If the Training UI writes new live SAM3 values into `config` without synchronization, the capture thread can observe partially updated state.

**Mitigation:** use `configMutex` or a thread-safe config update helper for all new SAM3 controls.

### Risk: false positives are deeper than thresholding

Threshold tuning may not solve the detection flood if the wrong tensors are being interpreted as final object detections.

**Mitigation:** expose stage counters and preserve evidence before changing the decoding model further.

### Risk: stale preview snapshot

An old snapshot can remain visible after mode changes or backend resets.

**Mitigation:** invalidate the stored SAM3 preview snapshot whenever mode changes, engine path changes, preview is disabled, backend becomes unavailable, or runtime shuts down.

## Success Criteria

1. The existing capture preview can show SAM3 boxes in `Label` mode.
2. Confidence labels can be toggled on/off.
3. Training -> Label exposes SAM3 postprocess controls.
4. SAM3 preview/save use snapshotted settings, not drifting shared state.
5. Debug counters show how many masks survive each stage.
6. `Max Detections` is applied only after all candidates are scored and sorted.
7. The system provides enough evidence to decide whether the remaining false positives are threshold issues or a deeper SAM3 decoding issue.
8. In `Label` mode, the capture preview no longer mixes a live frame with stale SAM3 detections.

## Out of Scope for This Design

- semantic-mask overlay rendering
- polygon/mask contour rendering
- a final redesign of SAM3 decoding beyond the current bounding-box extraction pipeline
- moving SAM3 controls into Core -> AI

Those may follow once the visual/debug foundation is in place.
