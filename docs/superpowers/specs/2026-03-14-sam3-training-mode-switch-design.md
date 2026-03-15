# SAM3 Training Mode Switch Design

> Document Type: Design Specification
> Project: Sunone Aimbot 2
> Date: 2026-03-14
> Status: Draft

## Goal

Add a real training-label inference mode that suspends live YOLO detection and runs live SAM3 inference instead.

The user should be able to:

- switch the app from normal detection mode into training-label mode
- see live SAM3 prompt-driven preview results on incoming frames
- save the exact labeled frame through a hotkey
- avoid running YOLO and SAM3 live inference at the same time on the GPU

## Problem

The current codebase starts live detector inference at app startup and keeps it running until paused. That behavior works for normal gameplay, but it conflicts with SAM3 labeling for three reasons:

- YOLO keeps consuming GPU while the user is trying to label
- the current SAM3 backend is only a stub and does not own a live frame-processing path
- the label hotkey path queues saves, but it does not yet run a persistent SAM3 preview-and-save workflow

If SAM3 is added as a one-off side call, the user will not get the same live feedback loop that already exists for YOLO detection.

## Scope

### In Scope

- add an explicit runtime inference mode for normal detection versus training labeling
- suspend YOLO or DML live inference when training-label mode is active
- add a SAM3 runtime path that mirrors the existing detector flow
- keep a persistent SAM3 backend alive while training-label mode is active
- publish live SAM3 preview detections to the training UI
- save labeled frames and YOLO-format boxes through the training hotkey
- keep only one live inference backend active at a time

### Out of Scope

- running YOLO and SAM3 live inference at the same time
- full backend unification behind one abstract detector interface
- segmentation dataset export
- automatic multi-frame tracking

## User Workflow

1. Start the app in normal detection mode.
2. Open `Training -> Label`.
3. Enable training-label mode.
4. The app pauses normal live YOLO detection and activates live SAM3 inference.
5. Type or refine a prompt and watch SAM3 preview boxes update on live frames.
6. Press the label hotkey to save the exact frame plus YOLO label file.
7. Disable training-label mode to return to normal detection.

## Design

### 1. Runtime Inference Modes

Add one shared runtime mode enum with at least these states:

- `Detect` - current YOLO or DML live detector path is active
- `Label` - live SAM3 training-label path is active
- `Paused` - no live inference backend is active

`Detect` remains the default startup mode. The existing pause behavior should map cleanly onto `Paused` instead of fighting a separate training flag.

The mode must be the single source of truth for which backend owns incoming frames.

`activeInferenceMode` replaces backend ownership decisions across the app. The pause hotkey must switch the app into or out of `Paused`. The current `training_label_enabled` config flag is removed as a runtime routing control and replaced by a UI control that sets `activeInferenceMode = Label`. Backend ownership comes only from `activeInferenceMode`.

### 2. Ownership Rules

Only one live backend may own inference work at a time.

#### Detect Mode

- capture feeds the existing YOLO or DML detector path
- detector results continue to populate `detectionBuffer`
- SAM3 runtime stays idle or unloaded

#### Label Mode

- capture stops feeding live YOLO or DML inference
- SAM3 runtime receives incoming frames instead
- SAM3 preview results go to a separate training preview buffer
- label hotkey saves use the training-label path only
- entering `Label` clears and version-bumps `detectionBuffer`
- detector-driven aim and detector overlay consumers must treat `Label` the same way they treat `Paused`

#### Paused Mode

- capture does not feed live inference to either backend
- shared result buffers are cleared or marked stale

### 3. Mirrored SAM3 Runtime Path

SAM3 should follow the same shape as the existing detector pipeline rather than acting as a one-shot helper.

Add a persistent training-label runtime with these responsibilities:

- own the active SAM3 backend instance
- own a worker thread for live preview inference
- own the save queue for frozen label-save requests
- accept the latest frame snapshot from capture
- accept the current prompt and other label settings
- run inference on a throttled cadence
- publish the latest preview detections, timing, and error state

The overlay must read cached SAM3 preview state. It must not create a new backend object every frame.

### 4. Buffers And UI State

Keep YOLO results and SAM3 preview results separate.

- `detectionBuffer` remains the normal detector output buffer
- add a dedicated training preview result cache for SAM3
- add status fields for backend readiness, active mode, last preview latency, last detection count, and last save result

The `Training -> Label` page should show:

- active inference mode
- SAM3 backend readiness and last error
- engine path
- current prompt and saved class
- current preview detection count
- last saved image and label path

### 5. Capture Flow

Capture remains the producer of live frames, but it routes frames based on active mode.

#### Detect Mode

- current behavior stays in place
- frame goes to YOLO or DML processing

#### Label Mode

- frame goes to SAM3 preview runtime instead of detector processing
- detector processing is skipped for that frame

This keeps the switching point near the current frame-source logic and avoids duplicating capture ownership.

### 6. Hotkey Save Flow

When training-label mode is active and the label hotkey is pressed:

- snapshot the current frame
- snapshot the current prompt, class, split, negative-save option, and image format
- enqueue a save job
- let `TrainingLabelRuntime` process that frozen save request through its owned SAM3 backend and save queue
- convert the result to detection boxes
- save the image and YOLO label file through the dataset manager

The saved image and saved labels must always come from the same frozen frame.

If no boxes are found:

- save image only when negative saving is enabled
- otherwise save nothing and report the miss

### 7. Backend Initialization

The SAM3 backend should initialize when label mode becomes active, not at every UI draw and not at app startup by default.

Initialization responsibilities:

- validate the configured engine path
- deserialize the TensorRT engine
- create execution context and CUDA stream
- validate required bindings
- surface exact failure messages to the UI

If the user changes the engine path while already in `Label`, the runtime should mark the backend dirty and perform an async reinitialization against the new path. The old backend should stop owning inference before the new one becomes active.

If initialization fails, label mode should remain visible but report a clear error state instead of crashing or silently falling back.

### 8. Testing Strategy

Build the change in layers.

#### Layer 1: Mode Control

- prove that frame routing switches between detector and training runtime
- prove that only one live backend is active at a time
- prove that entering `Label` clears detector output and stops detector updates
- prove that `Paused`, `Label`, and `Detect` transitions never leave two active backends
- prove that the pause hotkey and label-mode toggle cannot leave conflicting runtime state

#### Layer 2: SAM3 Initialization

- prove that valid engine-path handoff reaches the SAM3 runtime
- prove that missing or invalid engine paths produce stable status messages

#### Layer 3: Save Flow

- prove that the label hotkey path snapshots frame plus settings together
- prove that negative-save behavior remains correct

#### Layer 4: Integration Verification

- CUDA build passes
- `Training -> Label` shows mode and backend state correctly
- enabling label mode stops normal live detection updates
- disabling label mode resumes normal live detection updates
- changing engine path during `Label` triggers clean backend reinitialization

## Recommendation

Implement the mirrored SAM3 runtime path now, but keep the architecture narrow:

- add explicit inference mode control
- add frame routing based on active mode
- add persistent SAM3 preview runtime
- keep YOLO and SAM3 result buffers separate

Do not attempt a full detector abstraction refactor in the same change.
