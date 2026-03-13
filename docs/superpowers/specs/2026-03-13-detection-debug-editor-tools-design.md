# Detection Debug Editor Tools Design

> Document Type: Design Specification
> Project: Sunone Aimbot 2
> Date: 2026-03-13
> Status: Draft

## Goal

Remove all YOLO26 debug overlay behavior and replace it with manual debugging tools in the ImGui settings editor under `Monitor -> Debug`.

This change is limited to debugging support. It must not change detection decoding, scaling, filtering, targeting, or tracking behavior.

## Problem

The current debugging work is split across two overlay systems:

- an ImGui editor overlay
- an always-on game overlay

That split makes the debug path harder to reason about and harder to use. The user wants the debug overlay removed entirely and wants debugging to happen through explicit tools in the editor instead.

## Scope

### In Scope

- Remove YOLO26-specific overlay windows, overlay boxes, and overlay HUD debug rendering
- Remove temporary helper files and test files that only support the overlay HUD path
- Extend `Monitor -> Debug` with manual detection-debug actions
- Save detection logs to dedicated debug folders
- Save screenshots with current detections drawn on top
- Save a combined debug bundle with log, screenshot, annotated screenshot, and config snapshot
- Show last action status and last saved path in the Debug tab

### Out of Scope

- Any change to detector output, coordinate math, class handling, or confidence filtering
- Any change to targeting logic
- Any automatic capture-on-detect behavior
- Any automatic cleanup of debug folders
- Any new always-on visual overlay for detection debugging

## Design

### 1. Remove Overlay-Based YOLO26 Debugging

The following behavior will be removed:

- ImGui floating `YOLO26 Debug` window
- ImGui foreground debug box drawing
- game-overlay YOLO26 HUD rendering
- overlay-only config options whose only purpose is drawing those elements

Files affected include:

- `sunone_aimbot_2/overlay/draw_debug.cpp`
- `sunone_aimbot_2/overlay/overlay.cpp`
- `sunone_aimbot_2/sunone_aimbot_2.cpp`
- `sunone_aimbot_2/overlay/yolo26_game_debug.h`
- `sunone_aimbot_2/overlay/yolo26_game_debug.cpp`
- `tests/yolo26_game_debug_test.cpp`
- `CMakeLists.txt`
- `sunone_aimbot_2/config/config.h`
- `sunone_aimbot_2/config/config.cpp`

The old overlay-triggered `debug_export` path will be removed completely. If any low-level file-writing code is worth keeping, it will be moved behind editor-triggered helpers so no overlay-driven debug export path remains.

### 2. Add Manual Debug Tools to `Monitor -> Debug`

The Debug tab will gain a new section for detection debugging. The section will provide manual actions only.

Planned controls:

- `Save Detection Log`
- `Save Annotated Screenshot`
- `Save Debug Bundle`
- read-only text for the last saved path
- read-only text for the last action status

These actions are editor-only. This design does not add new hotkeys or reuse the screenshot hotkey for detection-debug exports.

This is intentional. The user chose manual-only capture.

The existing screenshot binding settings remain in place. The new detection-debug tools are separate from the normal screenshot hotkey flow.

### 3. Output Layout

Debug outputs will be written under a dedicated timestamped folder, separate from normal screenshots.

Proposed layout:

```text
debug/
  2026-03-13_14-22-08-417_detection_debug/
    detections.json
    screenshot_raw.png
    screenshot_annotated.png
    config_snapshot.txt
```

The `debug/` root is relative to the process working directory, which this application already normalizes to the executable directory on startup.

This keeps one debugging session self-contained and easy to inspect later.

### 4. Detection Log Format

`detections.json` will store:

- timestamp
- current model name
- detection resolution
- capture resolution if available from the latest frame
- ordered list of current detections
- per-detection game-space rectangle
- class id

The file is for inspection, not replay. It should stay small and direct.

`game_space` means the current post-scaled rectangle stored in `detectionBuffer`. This task does not add new detector-side fields. Confidence remains unavailable in this export on purpose because the current shared debug data path does not store it, and this task must not modify detector outputs.

Example shape:

```json
{
  "timestamp": "2026-03-13_14-22-08",
  "model": "model.onnx",
  "detection_resolution": 320,
  "frame": { "width": 320, "height": 320 },
  "detections": [
    {
      "id": 0,
      "class_id": 1,
      "game_space": { "x": 84, "y": 112, "w": 46, "h": 90 }
    }
  ]
}
```

### 5. Annotated Screenshot

The annotated screenshot will be created from the latest captured frame already stored in memory.

Rendering rules:

- draw current detection boxes onto a copy of the latest frame
- draw class id labels near the boxes
- do not alter the live frame in memory
- do not depend on the game overlay or ImGui overlay for annotation

Implementation detail:

- use OpenCV image drawing on the copied frame
- use `cv::rectangle` for box outlines
- use `cv::putText` for class labels
- draw boxes in red with a 2 px outline
- draw labels in yellow near the top-left corner of each box, or just above the box when space allows
- use a small OpenCV Hershey font sized for readability on 320-640 px captures

This keeps the saved image tied to the actual capture source instead of the overlay presentation layer.

### 6. Debug Bundle Action

`Save Debug Bundle` will run the full export in one action:

1. create timestamped folder
2. snapshot current detections
3. snapshot current config values relevant to debugging
4. save raw screenshot if a frame is available
5. save annotated screenshot if a frame is available
6. update editor status text with success or error

If there is no frame or no detections, the export still succeeds where possible and reports what was missing.

The UI status is stored in a small editor-side state object owned by the Debug tab code. Each action updates two fields:

- last status message
- last saved directory path

Exports run inline from the manual button press. Console logging is used on failure only.

## Architecture

### Editor UI Layer

`draw_debug.cpp` remains the user-facing entry point for the Debug tab. It will call a focused export helper instead of owning export logic directly.

### Export Helper Layer

A helper module will own:

- timestamped folder creation
- JSON/text file writing
- frame copy and annotation
- status reporting back to the UI

This keeps UI code small and keeps file-writing logic out of the draw function.

The current `sunone_aimbot_2/overlay/debug_export.cpp` file will be refactored into this helper role if practical. If the old structure is more confusing than useful, it will be deleted and replaced with a clearer helper module. In either case, no overlay-triggered export entry point will remain.

### Data Sources

The export helper will read from existing shared state only:

- `latestFrame` guarded by `frameMutex`
- `detectionBuffer` guarded by `detectionBuffer.mutex`
- `config`

No new detector-side data path is required.

## Error Handling

- If no frame is available, still write detection/config files and report that screenshots were skipped
- If no detections are available, still save the frame and write an empty detection list
- If directory creation or image writing fails, surface the failure in the Debug tab status and log to console when useful
- Never block the UI longer than needed for a single export action

## Testing And Verification

Verification for this change will focus on behavior, not detector math.

Required checks:

- build succeeds after overlay debug code is removed
- no YOLO26 overlay panel or game-overlay debug HUD code remains wired into runtime
- `Monitor -> Debug` shows the new manual tools
- `Save Detection Log` creates a timestamped folder and valid detection file
- `Save Annotated Screenshot` creates an image with visible drawn boxes when detections exist
- `Save Debug Bundle` creates the full output set

If a small helper for annotation/export formatting is extracted into pure logic, a focused test may be added for that helper. No detector behavior tests are part of this task.

## Implementation Notes

- Reuse the existing screenshot/debug export ideas where they help, but remove the overlay-centric behavior completely
- Prefer PNG for debug screenshots because annotation readability matters more than file size
- Keep the Debug tab wording direct and functional
- Remove these overlay-only config fields entirely because they only existed for the removed YOLO26 overlay path:
  - remove their declarations from `config.h`
  - remove their defaults, load, and save paths from `config.cpp`
  - `debug_show_yolo26_panel`
  - `debug_draw_raw_boxes`
  - `debug_draw_scaled_boxes`
  - `debug_show_labels`
  - `debug_export_frame`

Before implementation, verify there are no additional YOLO26 overlay-only config fields left behind outside this list.

Expected file actions:

- delete `sunone_aimbot_2/overlay/yolo26_game_debug.h`
- delete `sunone_aimbot_2/overlay/yolo26_game_debug.cpp`
- delete `tests/yolo26_game_debug_test.cpp`
- modify `CMakeLists.txt` to remove the temporary test target and helper source
- modify `sunone_aimbot_2/overlay/draw_debug.cpp` to delete `drawYolo26DebugPanel()` and `drawYolo26DebugBoxes()` and replace them with editor-triggered debug actions
- modify runtime/editor files to remove all remaining calls into the old overlay debug path

No automatic retention or cleanup policy is added in this task. Debug artifacts remain until the user deletes them.

## Success Criteria

- All YOLO26 debug overlay behavior is gone from runtime
- Detection debugging is available through `Monitor -> Debug`
- Manual exports produce usable files for offline analysis
- No detection behavior changes are introduced
