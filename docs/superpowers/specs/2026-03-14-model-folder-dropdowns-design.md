# Model Folder Dropdowns Design

> Document Type: Design Specification
> Project: Sunone Aimbot 2
> Date: 2026-03-14
> Status: Draft

## Goal

Make model selection in the overlay read from the `models` folder next to `ai.exe`.

This applies to two places:

- `Core -> AI` model selection
- `Training -> Label` SAM3 engine selection

## Problem

The `Core -> AI` section already uses a dropdown, but its model discovery should be anchored to the executable-adjacent `models` folder. The `Training -> Label` SAM3 setting still uses a free-text engine path, which is slower to use and easier to misconfigure.

The UI should present the models that actually exist beside the running app, not depend on the current working directory or manual typing.

## Scope

### In Scope

- keep `Core -> AI` as a dropdown fed by the executable-adjacent `models` folder
- replace the SAM3 text path input with a dropdown
- list only `.engine` files for the SAM3 dropdown
- keep config values and runtime loading consistent with the selected entries
- show a clear message when no matching files exist

### Out of Scope

- generic file browsing outside the `models` folder
- support for selecting `.onnx` or `.pt` in the SAM3 runtime dropdown
- automatic model conversion or export from the dropdown UI

## Design

### 1. Shared Model Folder Resolution

Add one shared helper that resolves the `models` folder relative to the running executable path.

That helper should be the source of truth for all model-folder discovery and path resolution used by the overlay, startup fallback, detector reload, and SAM3 runtime settings. It should not rely on `std::filesystem::current_path()`.

Persisted value contract:

- `config.ai_model` stays filename-only
- `config.training_sam3_engine_path` stores `models/<filename>`
- both dropdowns display filenames only

### 2. Core AI Model Dropdown

Keep the existing `Core -> AI` model dropdown, but make sure its backing list comes from the resolved executable-adjacent `models` folder.

The dropdown should continue to show the detector model files that belong in the Core AI path.
Preserve the current detector dropdown semantics, including backend-aware detector entries. Only the model-folder root and the `sam3*` exclusion change.

Filter rule:

- `Core -> AI` excludes files whose basename starts with `sam3`
- `Training -> Label` includes only `.engine` files whose basename starts with `sam3`

### 3. SAM3 Engine Dropdown

Replace the free-text SAM3 engine input in `Training -> Label` with a dropdown.

The dropdown should:

- read from the same resolved `models` folder
- include only `.engine` files whose basename starts with `sam3`
- store the selected result in `config.training_sam3_engine_path`
- use the stored value to drive the runtime backend path

This dropdown should not show `.onnx`, `.pt`, or unrelated files.

### 4. Empty-State Behavior

If no matching files are found:

- `Core -> AI` should continue to show its current no-model message
- `Training -> Label` should show a SAM3-specific no-engine message such as `No SAM3 engine files available in the models folder.`

### 5. Config Behavior

The selected values should remain persisted in config.

If a stored model is missing from the discovered list, the UI should fall back cleanly:

- keep a valid current selection when possible
- otherwise select the first available entry
- if nothing is available, show the empty state without crashing

Runtime fallback rule:

- if the stored SAM3 entry is missing, select and persist the first discovered SAM3 engine
- if no SAM3 engines exist, do not persist a replacement value; pass an empty engine path to the runtime for the current session and show the empty state

### 6. Verification

Verify these cases:

- `Core -> AI` lists detector models from the `models` folder next to `ai.exe`
- `Training -> Label` lists only `.engine` files from that same folder
- selecting a Core AI model updates config and detector reload behavior
- selecting a SAM3 engine updates config and reaches the labeling runtime
- empty-folder cases render clear messages instead of broken controls

## Recommendation

Implement this with one shared folder-resolution helper and two focused list helpers:

- detector model list helper
- SAM3 engine list helper

That keeps the UI simple and avoids duplicating path logic across the overlay.
