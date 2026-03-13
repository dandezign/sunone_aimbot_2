# YOLO26 Alignment Debug Tools Design

> Document Type: Design Specification
> Project: Sunone Aimbot 2
> Date: 2026-03-13
> Status: Draft

## Goal

Add manual debugging tools under `Monitor -> Debug` so live YOLO26 box misalignment can be investigated from the editor without relying on overlay-only debug behavior.

The tools must make it easy to save the exact capture frame, an annotated frame, and structured detector metadata from the same moment in time.

## Problem

The current symptom is that YOLO26 boxes appear to float away from the target and change size as the target moves, even when the object itself remains the same.

That symptom strongly suggests a coordinate-space bug, not a rendering bug. The most likely candidates are:

- the decoder is treating YOLO26 coordinates as `x,y,w,h` top-left boxes when they are actually center-based boxes
- the decoder is scaling from the wrong source space
- the capture frame, detector frame, and debug preview are not using the same dimensions
- the model output shape is being interpreted differently than the export actually emits

The current Debug tab is too thin to prove which one is happening. It has screenshot hotkey settings and console verbosity, but it does not capture enough evidence to diagnose transform bugs from one saved session.

## Scope

### In Scope

- extend `Monitor -> Debug` with manual capture and export actions
- add timed capture support for repeated debugging snapshots
- save one debug bundle containing raw frame, annotated frame, and JSON metadata
- add editor-visible status, counters, and last-path feedback
- add read-only geometry diagnostics that expose the active transform chain
- add rolling debug event logging that can be exported from the editor
- add detector-side debug metadata needed to understand decode and scaling behavior

### Out of Scope

- no game overlay debug windows
- no browser-based visual tools
- no permanent always-on debug drawing outside the existing editor preview
- no speculative fix to the YOLO26 decoder in this design document
- no automatic upload, telemetry, or remote reporting

## Design

### 1. Add A Dedicated Detection Diagnostics Section

`sunone_aimbot_2/overlay/draw_debug.cpp` will gain a new `Detection Diagnostics` section under `Monitor -> Debug`.

This section will be manual and operator-driven. It will include:

- `Save Raw Frame`
- `Save Annotated Frame`
- `Save Debug Bundle`
- `Export Debug Log`
- timed capture controls and start-stop buttons
- read-only status text for the last action result
- read-only path text for the last saved artifact or bundle directory

The existing screenshot hotkey settings stay in place. They remain separate from the new diagnostics tools.

### 2. Save A Full Debug Bundle

The main action will be `Save Debug Bundle`.

One bundle will save all three artifacts together:

- raw frame image
- annotated frame image
- JSON metadata

Recommended output layout:

```text
debug/
  2026-03-13_21-10-44-182_yolo26_debug/
    frame_raw.png
    frame_annotated.png
    detection_debug.json
    debug_log.txt
```

This keeps one debugging moment self-contained. A single folder should answer these questions:

- what frame was detected
- what boxes were produced
- what sizes and gains were used
- which backend, model, and settings were active
- what recent detector events happened before export

The output root is `debug/` relative to the application working directory. This project already normalizes runtime work to the executable directory, so the practical output location is `<exe-dir>/debug/`.

Each bundle will use a single immutable snapshot assembled before any file-writing begins. That snapshot will contain:

- one copied frame buffer
- one copied detection list
- one copied detector debug snapshot
- one copied config snapshot
- one export timestamp and bundle id

All files in the bundle must be written from that immutable snapshot only.

### 3. Add Timed Capture Support

The Debug tab will add timed capture controls for repeated snapshots during motion.

Planned controls:

- capture interval in milliseconds
- burst count
- optional filename prefix
- start timed capture
- stop timed capture
- live progress text

Timed capture will reuse the same export path as manual capture. Each step saves a full bundle. This is important because the bug appears during motion, not only in a single still frame.

Timed capture policy:

- the interval is measured from the start of one snapshot attempt to the start of the next snapshot attempt
- captures must never overlap
- if one capture takes longer than the interval, the next capture starts immediately after the previous one finishes
- non-fatal file omissions, such as missing frame or missing detections, still count as completed captures and the burst continues
- fatal failures, such as inability to create the bundle directory or write the JSON metadata file, stop the burst and report the failure

### 4. Add Read-Only Geometry Diagnostics

The Debug tab will show the live transform chain that the detectors and preview use.

Planned fields:

- backend: `DML` or `TRT`
- current model path
- latest capture frame size
- detector input size
- configured detection resolution
- detector output tensor shape
- decoder output convention in use
- x and y gains used to scale model space to detection space
- number of detections in the current snapshot
- min and max box extents in the current frame

This section stays read-only. It is diagnostic, not a control surface.

UI source rules:

- `live` fields read from the latest shared state once per editor draw
- `snapshot` fields shown after an export come from the last saved bundle snapshot
- labels must distinguish `Live` from `Last Saved` to avoid mixing data from different moments

### 5. Add Structured Detector Metadata

The current shared detection state only stores boxes and class ids. That is not enough to debug alignment errors.

Add a focused debug snapshot structure that records the last decode context, such as:

- timestamp
- backend
- model name
- input frame width and height given to the detector
- model width and height used for preprocessing
- output tensor dimensions
- scale gains used during decode
- whether the decoder interpreted boxes as top-left or center-based
- per-detection raw values before final rectangle conversion
- final rectangles written into `detectionBuffer`

This structure should be updated by both `sunone_aimbot_2/detector/dml_detector.cpp` and `sunone_aimbot_2/detector/trt_detector.cpp` so the same Debug tab works across both backends.

Minimum common contract across both backends:

- backend name
- timestamp
- detector input width and height
- model width and height
- output tensor shape as an integer array
- x gain and y gain as floats
- box convention string, for example `top_left_wh` or `center_wh`
- list of final rectangles

Backend-specific fields may be added, but unsupported fields must still appear in JSON as `null` or empty arrays instead of being omitted.

### 6. Annotated Frame Rules

The annotated frame must be drawn from a copied image buffer, never from live UI rendering.

Annotation contents:

- final detection rectangles
- class id and confidence when available
- optional center point marker for each final box
- optional text block with frame size, model size, gains, backend, and output shape

The annotation should come from the same frame snapshot saved as `frame_raw.png`, so later review can compare raw and annotated versions pixel-for-pixel.

Performance rule:

- UI code may request an export, but the actual image encoding and file writing should happen off the draw path, using a worker helper that receives the immutable snapshot

This keeps the editor responsive while preserving snapshot integrity.

### 7. Rolling Debug Event Log

Add a small in-memory event log that records the recent detector and export events.

Examples:

- detector reloads
- model input size changes
- output tensor shape changes
- invalid output shape warnings
- timed capture start and stop
- export success and failure

The log should be bounded in size so it cannot grow without limit. The Debug tab will show recent entries and allow export to `debug_log.txt`.

### 8. Keep The Data Flow Honest

The debug bundle should be assembled from the same shared state that runtime behavior uses:

- `latestFrame` protected by `frameMutex`
- current detection rectangles from `detectionBuffer`
- detector debug snapshot from the new debug metadata path
- active config snapshot

This prevents a separate debug-only path from drifting away from the real detector path.

Snapshot assembly order:

1. copy `latestFrame` under `frameMutex`
2. copy `detectionBuffer` under `detectionBuffer.mutex`
3. copy detector debug metadata under its own mutex
4. copy config under `configMutex`
5. stamp the export time and bundle id

The snapshot may represent the nearest consistent state, not a globally locked world-stop. To make that explicit, JSON will include:

- frame snapshot timestamp
- detector snapshot timestamp
- export timestamp
- detection buffer version if available

That lets post-run analysis spot stale or mismatched state instead of guessing.

## Exported JSON Contract

`detection_debug.json` will use a stable top-level structure:

```json
{
  "export_timestamp": "2026-03-13T21:10:44.182Z",
  "bundle_id": "2026-03-13_21-10-44-182_yolo26_debug",
  "backend": "TRT",
  "model": "models/example.engine",
  "frame": {
    "width": 320,
    "height": 320,
    "timestamp": "2026-03-13T21:10:44.170Z"
  },
  "detector": {
    "timestamp": "2026-03-13T21:10:44.171Z",
    "input_size": { "width": 320, "height": 320 },
    "model_size": { "width": 640, "height": 640 },
    "output_shape": [1, 300, 6],
    "x_gain": 0.5,
    "y_gain": 0.5,
    "box_convention": "top_left_wh"
  },
  "detections": [
    {
      "index": 0,
      "class_id": 1,
      "confidence": 0.91,
      "raw_box": {
        "x": 220.0,
        "y": 180.0,
        "w": 64.0,
        "h": 128.0
      },
      "final_box": {
        "x": 110,
        "y": 90,
        "w": 32,
        "h": 64
      }
    }
  ],
  "event_log": [
    "[21:10:42.501] output shape changed to [1,300,6]"
  ]
}
```

Contract rules:

- required top-level keys must always exist
- missing numeric values use `null`
- missing arrays use `[]`
- timestamps use ISO 8601 UTC strings
- `output_shape` is always an integer array
- `raw_box` stores detector-space values before final rectangle conversion
- `final_box` stores the rectangle written to runtime detection state

## Architecture

### UI Layer

`sunone_aimbot_2/overlay/draw_debug.cpp` remains the entry point for editor controls and status display.

It should stay thin. The draw code should call dedicated helpers for:

- snapshot collection
- frame annotation
- bundle writing
- timed capture state updates
- log export

### Shared Debug State Layer

Add a shared debug module that owns:

- the last detector debug snapshot
- a bounded event log
- export helper functions
- timed capture state

This module should live outside the draw function so detector code and UI code can both use it safely.

### Detector Integration Layer

`sunone_aimbot_2/detector/dml_detector.cpp` and `sunone_aimbot_2/detector/trt_detector.cpp` will publish the decode context into the shared debug state after each detection pass.

That update should happen close to the decode path so the saved metadata matches the exact math that produced the final boxes.

## Likely Root-Cause Hypothesis To Validate

The first hypothesis to validate is that YOLO26 outputs are center-based and the current decoders are treating them as top-left boxes.

Current code paths that need inspection:

- `sunone_aimbot_2/detector/trt_detector.cpp:76`
- `sunone_aimbot_2/detector/dml_detector.cpp:129`

Both decoders currently convert:

- `model_x` directly to `game_x`
- `model_y` directly to `game_y`
- `model_w` directly to `game_w`
- `model_h` directly to `game_h`

If the model emits box centers, the correct final rectangle would need a half-width and half-height shift before drawing or tracking. The new tools must capture enough evidence to prove or disprove this cleanly before any fix is attempted.

## Error Handling

- if no frame is available, export metadata and log anyway and report that image files were skipped
- if no detections are available, still export the frame and the empty metadata snapshot
- if metadata is partially unavailable, write what exists and record missing fields explicitly
- if file creation fails, surface the failure in the Debug tab and record it in the event log
- timed capture must stop cleanly on failure and report where it stopped

## Testing And Verification

Verification must focus on evidence collection and on the eventual root-cause investigation.

Required checks:

- the editor shows the new diagnostics controls under `Monitor -> Debug`
- manual `Save Debug Bundle` writes raw image, annotated image, JSON metadata, and log file together
- timed capture writes the expected number of bundles at the requested interval
- exported metadata reflects live detector values for both `TRT` and `DML` when available
- the debug panel reports missing frame or missing detections without crashing
- a saved bundle contains enough information to tell whether the decoder used the wrong box convention

If practical, pure helpers such as annotation formatting, JSON generation, and debug log truncation should get focused tests.

## Expected File Actions

- modify `sunone_aimbot_2/overlay/draw_debug.cpp`
- add a shared debug helper module for detector snapshots, logs, and exports
- modify `sunone_aimbot_2/detector/dml_detector.cpp`
- modify `sunone_aimbot_2/detector/trt_detector.cpp`
- modify the relevant headers to expose shared debug state safely
- add focused tests for helper logic if a testable pure layer is extracted

## Success Criteria

- the Debug tab can produce one self-contained debug bundle from a single button press
- timed capture can produce repeated bundles during motion
- the exported JSON clearly shows the active frame size, model size, tensor shape, gains, and decoded boxes
- the log export captures recent events that matter to detector debugging
- one saved bundle is enough to diagnose whether the box drift comes from decode math, scaling math, or preview-space mismatch
