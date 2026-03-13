# SAM3 Training Label Design

> Document Type: Design Specification
> Project: Sunone Aimbot 2
> Date: 2026-03-13
> Status: Draft

## Goal

Add a dedicated `Training -> Label` workflow to the ImGui editor for live SAM3-assisted dataset creation.

The user should be able to:

- watch the live capture preview from the current source, whether it is a game, monitor, video, or virtual camera
- enter and refine a SAM3 text prompt until detections look correct
- map that prompt to a stable dataset class
- press a hotkey to save the current frame and YOLO26 detection labels into one structured dataset under the executable directory
- keep playing or scrubbing video and repeat the process without leaving the editor

This feature replaces old YOLO11-era assumptions for training output. New generated training metadata and commands must target modern Ultralytics YOLO26 detection training.

## Problem

The repository already contains:

- an ImGui editor with capture preview and debug tooling
- a reference SAM3 TensorRT implementation in `SAM3TensorRT/`
- an older manual dataset scaffold in `scripts/custom_dataset/`

What it does not have is a live assisted-labeling path that connects capture preview, SAM3 prompt tuning, class selection, hotkey-driven saving, and YOLO26-ready dataset output.

The current manual dataset scaffold is also not the best runtime target for Ultralytics detect training. Modern Ultralytics detection datasets expect image splits under `images/...` and labels under matching `labels/...` directories, with dataset metadata in YAML. The new feature should generate exactly that layout.

## Scope

### In Scope

- Add a new `Training` group to the editor sidebar
- Add a `Label` tab under that group
- Show live capture preview inside the Label tab
- Allow live SAM3 prompt editing and preview inference
- Allow prompt-to-class mapping where prompt text is separate from saved class name
- Manage dataset classes through the UI with reload-from-disk support
- Save one runtime dataset rooted at `<exe_dir>/training/`
- Save YOLO detection labels only, not segmentation labels
- Support positive and negative sample saving through one hotkey-driven flow
- Generate YOLO26-ready dataset files: `game.yaml`, `predefined_classes.txt`, and `start_train.bat`
- Persist new labeling settings through the existing flat config system

### Out of Scope

- Training the model inside the app
- Building a segmentation dataset
- Multi-project dataset management
- Automatic batch labeling of whole folders
- Automatic video timeline tracking across frames
- Changing detector, targeting, or overlay behavior outside the new training feature

## User Workflow

1. Open the overlay editor and go to `Training -> Label`.
2. Confirm the live capture preview looks correct.
3. Type or refine a SAM3 prompt, such as `enemy head silhouette` or `rifle on ground`.
4. Choose the saved dataset class, such as `head` or `weapon`.
5. Watch the preview boxes update at a throttled interval.
6. Press the labeling hotkey when the current frame and current prompt look correct.
7. The app snapshots the frame, runs SAM3 for that snapshot, converts results to YOLO boxes, and saves image plus labels into the selected split.
8. Keep playing or scrubbing video and repeat.

If no boxes are found, behavior depends on the `Save negatives when no detections` toggle:

- enabled: save the image only, with no label file
- disabled: save nothing and report the miss in the UI

## Design

### 1. Editor Navigation And Layout

Add a new sidebar group named `Training` in `sunone_aimbot_2/overlay/overlay.cpp`.

Add one tab for this task:

- `Label` - live SAM3-assisted dataset creation

The tab should follow the existing editor pattern used by `Core` and `Monitor`. It should reuse the current section style helpers instead of introducing a new visual system.

### 2. Label Tab UI

`Training -> Label` should be split into four sections.

#### SAM3 Session

- enable checkbox for SAM3 assisted labeling
- TensorRT engine path field
- prompt text input
- preview auto-refresh toggle
- preview refresh interval in ms
- confidence threshold
- minimum box area filter
- maximum detections
- hotkey selector for `Save Labeled Sample`

#### Class Mapping

- class dropdown for current saved class
- add class
- rename class
- remove class
- reload class list from disk
- save class metadata to disk
- read-only summary such as `Prompt -> player`

#### Dataset Output

- dataset root display
- split selector: `train` or `val`
- toggle: `Save negatives when no detections`
- image format selector: `jpg` or `png`
- status line for last save result
- status line for last save path
- status line for last detection count
- counts for train images, val images, and classes

#### Live Preview

- live capture preview based on the same frame source shown elsewhere in the editor
- current SAM3 preview boxes drawn on top of a copied frame
- prompt and saved class shown near the preview
- last preview inference time and detection count

The preview should show boxes only. SAM3 masks may be used internally to compute boxes, but the user-facing labeling result is detection-only.

### 3. Runtime Components

This feature should not be implemented as one large draw file. Use small runtime helpers.

#### `training_label_state`

Owns editor-facing labeling state:

- current prompt
- selected class name
- selected split
- negative-save toggle
- preview refresh settings
- last status text
- last saved paths
- last detection count
- hotkey binding

This state is editor runtime state. It can be backed by config for fields that should persist.

#### `training_dataset_manager`

Owns dataset disk layout and metadata:

- ensure directories exist
- load and save `predefined_classes.txt`
- regenerate `game.yaml`
- regenerate `start_train.bat`
- count samples per split
- allocate stable filenames
- write images
- write YOLO label files

#### `training_sam3_labeler`

Owns SAM3 inference integration.

Version 1 supports one backend only:

- load a SAM3 TensorRT engine based on the `SAM3TensorRT/` reference

Responsibilities:

- accept a frame snapshot and prompt
- run SAM3 inference
- convert the result to detection boxes
- filter by confidence, size, and count

The rest of the app should consume only detect-style results. It should not need to know about masks.

#### `training_label_capture`

Owns the save job flow:

- snapshot the latest frame safely
- call the SAM3 labeler for the snapshot
- translate detections into YOLO label rows for the selected class
- write outputs through the dataset manager
- update editor status on success or failure

### 4. Preview And Save Execution Model

Preview inference and save inference should be separated.

#### Preview Path

- use a throttled timer, not every ImGui frame
- run on copied frame data only
- update the latest preview result atomically
- never mutate the shared live frame

#### Save Path

- trigger from the dedicated labeling hotkey
- snapshot the current frame at trigger time
- queue the save job to a worker thread
- run SAM3 against that frozen frame snapshot
- save the exact snapshot that was labeled

This ensures the saved image and saved labels always correspond to the same frame.

Save-job backpressure rule for version 1:

- use one worker thread and a bounded FIFO queue
- allow repeated hotkey saves up to a small fixed queue limit such as 16 snapshots
- if the queue is full, reject the new save request and show `Save queue full` in the UI status
- each queued save captures its prompt, selected class name, split, and image format at enqueue time so delayed jobs cannot inherit later UI edits

### 5. Dataset Layout

The runtime dataset should live under the executable directory.

Proposed layout:

```text
training/
  game.yaml
  predefined_classes.txt
  start_train.bat
  datasets/
    game/
      images/
        train/
        val/
      labels/
        train/
        val/
```

This layout is intentionally aligned with Ultralytics detection docs for YOLO26.

`<exe_dir>/training/` is the single runtime root for this feature. The implementation should derive it from the executable directory, not from an arbitrary process working directory.

Key rules:

- training images go in `images/train/`
- validation images go in `images/val/`
- label files go in matching `labels/train/` and `labels/val/` directories
- if a saved frame has no objects and negatives are enabled, save the image and do not create a label file
- labels are YOLO normalized `class x_center y_center width height`

Example generated YAML:

```yaml
path: datasets/game
train: images/train
val: images/val
test:

names:
  0: player
  1: head
  2: weapon
```

Example generated label row:

```text
1 0.512500 0.331250 0.084375 0.137500
```

### 6. Class Catalog And `predefined_classes.txt`

`predefined_classes.txt` is not a YOLO26 training requirement. It is a helper file that already exists in the repository for manual labeling tools such as the bundled LabelImg copy.

For the new feature, keep it, but narrow its purpose:

- it becomes the editable human-readable class catalog
- the UI reads and writes it
- the app regenerates `game.yaml` from it
- YOLO label files use the class index derived from that ordered class list

This preserves compatibility with external labeling tools while making `game.yaml` the real Ultralytics training contract.

Class editing rules:

- add class appends a new class name in memory
- rename class updates the in-memory list only
- reload-from-disk replaces the in-memory list with the file contents and discards unsaved class edits
- `Save class metadata to disk` writes `predefined_classes.txt`, regenerates `game.yaml`, and regenerates `start_train.bat`

Because class order controls YOLO class ids, the implementation must treat destructive changes as unsafe once any label files exist.

Version 1 safety rule:

- if any label files already exist in the dataset, deletion, reordering, and reload that changes class order are blocked
- the UI should explain that a future label-remap tool would be required to perform those actions safely
- add and rename remain allowed because they do not remap existing ids when handled carefully

### 7. Prompt-To-Class Mapping

Prompt and saved class are separate by design.

Examples:

- prompt: `enemy torso visible in smoke` -> save class: `player`
- prompt: `helmeted head profile` -> save class: `head`
- prompt: `rifle leaning on wall` -> save class: `weapon`

This separation is required because the prompt is for SAM3 recall quality, while the class is for dataset consistency.

### 8. File Naming

Saved image and label names should be paired and collision-safe.

Recommended format:

```text
2026-03-13_22-31-18-417_000123.jpg
2026-03-13_22-31-18-417_000123.txt
```

That format is readable, sortable, and safe under repeated hotkey use.

### 9. Config Persistence

Add flat config keys for the new training workflow. Keep the existing config style.

Suggested keys:

- `training_label_enabled`
- `training_sam3_engine_path`
- `training_label_prompt`
- `training_label_class`
- `training_label_split`
- `training_label_save_negatives`
- `training_label_preview_enabled`
- `training_label_preview_interval_ms`
- `training_label_confidence_threshold`
- `training_label_min_box_area`
- `training_label_max_detections`
- `training_label_hotkey`
- `training_dataset_image_format`

Persist only stable settings. Do not persist volatile status strings such as last save path or last preview count.

`training_label_class` must persist the selected class name, not a raw class index. Save-time code resolves that class name against the current class list. If the class name no longer exists, saving is disabled until the user selects a valid class.

### 10. YOLO26 Alignment

Old YOLO11-era output assumptions should be treated as legacy.

For this feature:

- generated training command templates must use YOLO26 model names
- generated dataset metadata must match modern Ultralytics detect docs
- any new UI copy, helper scripts, or documentation created by this feature must refer to YOLO26, not YOLO11

Generated `start_train.bat` baseline:

```bat
yolo detect train data=game.yaml model=yolo26n.pt epochs=100 imgsz=640
pause
```

This batch file lives in `<exe_dir>/training/` and is expected to run from that directory.

The existing files under `scripts/custom_dataset/` can remain as legacy examples unless this task chooses to update them later, but the new runtime-owned training output under `training/` is the source of truth for this feature.

### 11. Error Handling

- if the SAM3 model or engine path is missing, disable preview and save actions with a clear status message
- if the latest frame is unavailable, do not save and report `No frame available`
- if a class list is empty, disable save and prompt the user to add a class
- if directory creation fails, surface the error in the Label tab status
- if SAM3 inference fails, keep the app running and report the failure without crashing the overlay
- if metadata regeneration fails, keep existing files intact where possible and report the failure
- clamp all boxes to image bounds before YOLO normalization
- discard any box whose width or height becomes non-positive after clamping

### 12. Testing And Verification

Verification should cover behavior, dataset correctness, and isolation from existing features.

#### UI Verification

- `Training` group appears in sidebar
- `Label` tab renders and uses existing editor styling
- prompt, class, split, and save toggles can be edited
- hotkey selection persists after restart

#### Dataset Verification

- saving a positive sample creates image plus label file in the selected split
- saving a negative sample creates image only when negatives are enabled
- saving a negative sample creates nothing when negatives are disabled
- `game.yaml` matches the current class list
- `predefined_classes.txt` matches the current class list
- generated paths match Ultralytics detect expectations

#### Inference Verification

- preview boxes update on the configured interval
- save-time boxes match the frame that was saved
- min-area, confidence, and max-detection filters apply consistently

#### Regression Verification

- existing capture settings still work
- existing debug tools still work
- normal inference and targeting are unchanged when training labeling is off

## Architecture Notes For Implementation

- keep UI drawing code thin and move file-writing and inference work out of the draw function
- use copied `cv::Mat` frames for both preview and saving
- use one background worker first; expand only if real bottlenecks appear
- prefer small focused source files over extending `draw_capture.cpp` or `draw_debug.cpp` with unrelated logic

## Open Decisions Resolved In This Spec

- workflow: real-time capture and label
- capture source: same workflow regardless of game or video
- target output: detection only
- prompt and class: separate
- dataset count: one active dataset
- negative handling: user-selectable
- class management: hybrid UI plus reload-from-disk
- training target: modern YOLO26, not legacy YOLO11

## Implementation Summary

Build a new `Training -> Label` workflow that reuses the live capture preview model already present in the editor, integrates SAM3 for prompt-driven box generation, writes one YOLO26-ready detection dataset under `training/`, and keeps prompt text separate from stable class ids.

This design gives the user a fast loop for building a custom mixed-class dataset while keeping the resulting training output aligned with modern Ultralytics detection requirements.
