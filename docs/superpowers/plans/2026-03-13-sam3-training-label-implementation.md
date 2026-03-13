# SAM3 Training Label Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `Training -> Label` editor workflow that uses SAM3 TensorRT inference to preview prompt-driven detections and save YOLO26-ready detection samples under `<exe_dir>/training/`.

**Architecture:** Keep the UI thin. Put dataset I/O, queued save jobs, and SAM3 inference behind focused `training/` runtime helpers. Wire the hotkey in the capture loop so labeled saves work while the overlay is hidden, and keep the preview path in the overlay so prompt tuning stays responsive.

**Tech Stack:** C++17, ImGui, OpenCV, existing config system, existing capture thread, TensorRT/CUDA behind `USE_CUDA`, standalone C++ test executable pattern from `tests/detection_debug_tools_test.cpp`.

---

## File Map

**Create:**
- `sunone_aimbot_2/training/training_label_types.h` - shared enums and structs for prompt/class/split/save requests
- `sunone_aimbot_2/training/training_dataset_manager.h` - dataset layout and metadata API
- `sunone_aimbot_2/training/training_dataset_manager.cpp` - folder creation, class catalog, YAML and batch generation, image/label writing
- `sunone_aimbot_2/training/training_label_runtime.h` - editor state, preview cache, queued save API, capture-thread entry points
- `sunone_aimbot_2/training/training_label_runtime.cpp` - worker queue, snapshot capture rules, status updates, save orchestration
- `sunone_aimbot_2/training/training_sam3_labeler.h` - SAM3 detect-only interface
- `sunone_aimbot_2/training/training_sam3_labeler_stub.cpp` - non-CUDA or unavailable stub implementation
- `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp` - CUDA/TensorRT-backed implementation using `SAM3TensorRT/` patterns
- `sunone_aimbot_2/overlay/draw_training.cpp` - `Training -> Label` UI
- `tests/training_labeling_tests.cpp` - dataset manager and runtime logic tests

**Modify:**
- `sunone_aimbot_2/config/config.h` - add persisted training-label settings
- `sunone_aimbot_2/config/config.cpp` - load/save defaults and persisted values for training-label settings
- `sunone_aimbot_2/overlay/draw_settings.h` - declare `draw_training()`
- `sunone_aimbot_2/overlay/overlay.cpp` - add `Training` sidebar group and `Label` tab
- `sunone_aimbot_2/capture/capture.cpp` - trigger hotkey-driven label saves from the capture loop using frozen frame snapshots
- `CMakeLists.txt` - compile new runtime files, conditional CUDA files, and new tests target

**Reference While Implementing:**
- `docs/superpowers/specs/2026-03-13-sam3-training-label-design.md`
- `sunone_aimbot_2/overlay/draw_capture.cpp`
- `sunone_aimbot_2/overlay/draw_debug.cpp`
- `sunone_aimbot_2/debug/detection_debug_export.cpp`
- `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu`

## Chunk 1: Runtime Foundations

### Task 1: Add the training-label test target and shared types

**Files:**
- Create: `tests/training_labeling_tests.cpp`
- Create: `sunone_aimbot_2/training/training_label_types.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test target and the first failing assertions**

```cpp
#include "sunone_aimbot_2/training/training_label_types.h"

int main() {
    training::QueuedSaveRequest req;
    req.prompt = "enemy torso";
    req.className = "player";
    req.imageFormat = ".jpg";
    if (req.className != "player") return 1;
    return 0;
}
```

- [ ] **Step 2: Register the new standalone test target in `CMakeLists.txt`**

```cmake
add_executable(training_labeling_tests
    "${CMAKE_SOURCE_DIR}/tests/training_labeling_tests.cpp"
)

target_include_directories(training_labeling_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${SRC_DIR}"
    "${AIMBOT_OPENCV_INCLUDE_DIR}"
)

target_compile_definitions(training_labeling_tests PRIVATE
    UNICODE
    _UNICODE
    _CONSOLE
)

target_link_libraries(training_labeling_tests PRIVATE
    "${AIMBOT_OPENCV_LIBRARY}"
)

if(MSVC)
    target_compile_options(training_labeling_tests PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W3>
        $<$<COMPILE_LANGUAGE:CXX>:/sdl>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    )

    set_target_properties(training_labeling_tests PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
```

- [ ] **Step 3: Run the build to verify it fails because the training headers do not exist yet**

Run: `cmake --build "build\dml" --config Release --target training_labeling_tests`
Expected: FAIL with missing `training_label_types.h` or missing `training::QueuedSaveRequest`

- [ ] **Step 4: Add `training_label_types.h` with the minimum shared structs and enums**

```cpp
namespace training {

enum class DatasetSplit { Train, Val };

struct QueuedSaveRequest {
    cv::Mat frame;
    std::string prompt;
    std::string className;
    DatasetSplit split = DatasetSplit::Train;
    bool saveNegatives = false;
    std::string imageFormat = ".jpg";
};

}  // namespace training
```

- [ ] **Step 5: Re-run the build and make sure the target now compiles**

Run: `cmake --build "build\dml" --config Release --target training_labeling_tests`
Expected: PASS

- [ ] **Step 6: Optional checkpoint if the user asks for commits**

Files to stage:
- `tests/training_labeling_tests.cpp`
- `sunone_aimbot_2/training/training_label_types.h`
- `CMakeLists.txt`

### Task 2: Implement dataset paths, class catalog, and metadata generation

**Files:**
- Create: `sunone_aimbot_2/training/training_dataset_manager.h`
- Create: `sunone_aimbot_2/training/training_dataset_manager.cpp`
- Modify: `tests/training_labeling_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Extend the failing test to lock down dataset root, folder layout, class catalog, and metadata output**

```cpp
const auto root = training::GetTrainingRootForTests("I:/games/ai.exe");
Check(root == std::filesystem::path("I:/games/training"), "training root should use exe dir");

const auto writableRoot = MakeUniqueTestRootForTests();
training::DatasetManager mgr(writableRoot);
mgr.SaveClasses({"player", "head", "weapon"});

Check(std::filesystem::exists(writableRoot / "predefined_classes.txt"), "class catalog missing");
Check(std::filesystem::exists(writableRoot / "game.yaml"), "dataset yaml missing");
Check(std::filesystem::exists(writableRoot / "start_train.bat"), "train batch missing");
```

- [ ] **Step 2: Add the new source file to the test target and run the build to verify it fails**

```cmake
add_executable(training_labeling_tests
    "${CMAKE_SOURCE_DIR}/tests/training_labeling_tests.cpp"
    "${SRC_DIR}/training/training_dataset_manager.cpp"
)
```

Run: `cmake --build "build\dml" --config Release --target training_labeling_tests`
Expected: FAIL with missing `DatasetManager` or unresolved functions

- [ ] **Step 3: Implement the dataset manager with exact path rules from the spec**

```cpp
std::filesystem::path GetTrainingRootForExe(const std::filesystem::path& exePath) {
    return exePath.parent_path() / "training";
}

void DatasetManager::EnsureLayout() {
    fs::create_directories(root_ / "datasets/game/images/train");
    fs::create_directories(root_ / "datasets/game/images/val");
    fs::create_directories(root_ / "datasets/game/labels/train");
    fs::create_directories(root_ / "datasets/game/labels/val");
}
```

- [ ] **Step 4: Generate `predefined_classes.txt`, `game.yaml`, and `start_train.bat` from one source list**

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

```bat
yolo detect train data=game.yaml model=yolo26n.pt epochs=100 imgsz=640
pause
```

- [ ] **Step 5: Re-run the test build and then execute the test binary**

Run: `cmake --build "build\dml" --config Release --target training_labeling_tests`
Expected: PASS

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: PASS with exit code `0`

- [ ] **Step 6: Optional checkpoint if the user asks for commits**

Files to stage:
- `sunone_aimbot_2/training/training_dataset_manager.h`
- `sunone_aimbot_2/training/training_dataset_manager.cpp`
- `tests/training_labeling_tests.cpp`
- `CMakeLists.txt`

### Task 3: Implement sample writing, negative handling, and box normalization guards

**Files:**
- Modify: `sunone_aimbot_2/training/training_dataset_manager.h`
- Modify: `sunone_aimbot_2/training/training_dataset_manager.cpp`
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Add failing tests for positive samples, negative samples, and invalid-box rejection**

```cpp
training::DetectionBox box{0, cv::Rect(10, 20, 40, 60)};
const auto positive = mgr.WriteSample(sampleFrame, {box}, "player", training::DatasetSplit::Train, ".jpg", true);
Check(positive.savedImage, "positive sample should save image");
Check(positive.savedLabel, "positive sample should save label");
Check(positive.classId == 0, "class id should come from ordered class catalog");
Check(positive.imagePath.extension() == ".jpg", "image format mismatch");
Check(positive.labelPath.extension() == ".txt", "label format mismatch");
Check(IsPairedSampleNameForTests(positive.imagePath, positive.labelPath), "paired file naming mismatch");

const auto negativeSaved = mgr.WriteSample(sampleFrame, {}, "player", training::DatasetSplit::Val, ".jpg", true);
Check(negativeSaved.savedImage, "negative-enabled path should save image");
Check(!negativeSaved.savedLabel, "negative-enabled path should not save label");

const auto negativeDropped = mgr.WriteSample(sampleFrame, {}, "player", training::DatasetSplit::Val, ".jpg", false);
Check(!negativeDropped.savedImage && !negativeDropped.savedLabel, "negative-disabled path should save nothing");

training::DetectionBox invalid{0, cv::Rect(-10, 5, 5, 20)};
const auto rejected = mgr.WriteSample(sampleFrame, {invalid}, "player", training::DatasetSplit::Train, ".jpg", true);
Check(!rejected.savedLabel, "boxes that become non-positive after clamping must be discarded");
Check(rejected.savedImage, "all-rejected detections should follow negative-enabled image-only behavior");
```

- [ ] **Step 2: Run the tests to verify they fail before implementation**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: FAIL with missing sample-writing behavior

- [ ] **Step 3: Implement clamped YOLO box conversion and sample writing**

```cpp
cv::Rect ClampBox(const cv::Rect& box, const cv::Size& size) {
    const cv::Rect bounds(0, 0, size.width, size.height);
    return box & bounds;
}

std::optional<YoloRow> ToYoloRow(const cv::Rect& box, const cv::Size& size, int classId) {
    const cv::Rect clamped = ClampBox(box, size);
    if (clamped.width <= 0 || clamped.height <= 0) return std::nullopt;
    return BuildNormalizedRow(clamped, size, classId);
}
```

Also assert in the test that the generated label text contains class `0` for `player` and normalized values in `[0, 1]`.

- [ ] **Step 4: Make negative-save behavior match the spec exactly**

```cpp
if (detections.empty()) {
    WriteImageOnlyIfRequested(...);
    return SaveResult{/*savedImage=*/saveNegatives, /*savedLabel=*/false};
}
```

- [ ] **Step 5: Re-run the tests and make sure they pass**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: PASS with exit code `0`

- [ ] **Step 6: Optional checkpoint if the user asks for commits**

Files to stage:
- `sunone_aimbot_2/training/training_dataset_manager.h`
- `sunone_aimbot_2/training/training_dataset_manager.cpp`
- `tests/training_labeling_tests.cpp`

### Task 4: Implement queued save snapshots and class-safe persistence rules

**Files:**
- Create: `sunone_aimbot_2/training/training_label_runtime.h`
- Create: `sunone_aimbot_2/training/training_label_runtime.cpp`
- Modify: `tests/training_labeling_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for queue snapshots and class-save safety**

```cpp
training::QueuedSaveRequest req;
req.frame = originalFrame;
req.prompt = "enemy torso";
req.className = "player";
req.split = training::DatasetSplit::Train;
req.imageFormat = ".png";

runtime.Enqueue(req);
req.prompt = "changed later";
req.className = "weapon";
req.split = training::DatasetSplit::Val;
req.imageFormat = ".jpg";
originalFrame.setTo(cv::Scalar(0, 0, 0));
Check(runtime.PeekForTests().prompt == "enemy torso", "queue must capture enqueue-time settings");
Check(runtime.PeekForTests().className == "player", "queue must capture class at enqueue time");
Check(runtime.PeekForTests().split == training::DatasetSplit::Train, "queue must capture split at enqueue time");
Check(runtime.PeekForTests().imageFormat == ".png", "queue must capture image format at enqueue time");
Check(GetFrameMeanForTests(runtime.PeekForTests().frame) != 0, "queue must clone frame at enqueue time");

for (size_t i = 1; i < training::kMaxQueuedSaves; ++i) runtime.Enqueue(req);
Check(!runtime.Enqueue(req), "queue should reject once full");
Check(runtime.GetStatusForTests() == "Save queue full", "queue-full status mismatch");
Check(runtime.DequeueForTests().prompt == "enemy torso", "queue should preserve FIFO order");

Check(!mgr.CanApplyDestructiveClassChange(), "existing labels should block destructive class changes");
```

- [ ] **Step 2: Add the runtime source file to the test target and run the build to verify it fails**

```cmake
add_executable(training_labeling_tests
    "${CMAKE_SOURCE_DIR}/tests/training_labeling_tests.cpp"
    "${SRC_DIR}/training/training_dataset_manager.cpp"
    "${SRC_DIR}/training/training_label_runtime.cpp"
)
```

Run: `cmake --build "build\dml" --config Release --target training_labeling_tests`
Expected: FAIL with missing runtime queue implementation

- [ ] **Step 3: Implement the runtime queue with a bounded FIFO and enqueue-time snapshots**

```cpp
if (queue_.size() >= kMaxQueuedSaves) {
    status_ = "Save queue full";
    return false;
}
queue_.push_back(request);  // request is already a frozen copy
```

- [ ] **Step 4: Implement class-save safety helpers that block destructive changes once label files exist**

```cpp
bool DatasetManager::CanApplyDestructiveClassChange() const {
    return CountExistingLabelFiles() == 0;
}
```

This helper gates delete, reorder, and reload-that-reorders.

- [ ] **Step 5: Re-run the tests and make sure they pass**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: PASS with exit code `0`

- [ ] **Step 6: Optional checkpoint if the user asks for commits**

Files to stage:
- `sunone_aimbot_2/training/training_label_runtime.h`
- `sunone_aimbot_2/training/training_label_runtime.cpp`
- `tests/training_labeling_tests.cpp`
- `CMakeLists.txt`

## Chunk 2: UI And End-To-End Integration

### Task 5: Add the SAM3 labeler interface and build-safe stub

**Files:**
- Create: `sunone_aimbot_2/training/training_sam3_labeler.h`
- Create: `sunone_aimbot_2/training/training_sam3_labeler_stub.cpp`
- Create: `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Write a failing test for the non-CUDA unavailable path**

```cpp
training::Sam3Labeler labeler;
const auto status = labeler.GetAvailabilityForTests();
Check(!status.ready, "stub backend should report unavailable without CUDA engine");
```

- [ ] **Step 2: Run the test build and verify it fails before the labeler exists**

Run: `cmake --build "build\dml" --config Release --target training_labeling_tests`
Expected: FAIL with missing `Sam3Labeler`

- [ ] **Step 3: Add a build-safe interface and a stub implementation first**

```cpp
class Sam3Labeler {
public:
    Availability GetAvailabilityForTests() const;
    Result LabelFrame(const cv::Mat& frame, const std::string& prompt);
};
```

- [ ] **Step 4: Add the CUDA-only TensorRT implementation behind `USE_CUDA`**

```cpp
#ifdef USE_CUDA
// load engine, preprocess frame, run SAM3, convert masks to boxes
#endif
```

- [ ] **Step 5: Re-run the DML tests and then build the app in CUDA mode**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: PASS

Run: `cmake --build "build\dml" --config Release --target ai`
Expected: PASS

Run: `cmake --build "build\cuda" --config Release --target ai`
Expected: PASS

- [ ] **Step 6: Optional checkpoint if the user asks for commits**

Files to stage:
- `sunone_aimbot_2/training/training_sam3_labeler.h`
- `sunone_aimbot_2/training/training_sam3_labeler_stub.cpp`
- `sunone_aimbot_2/training/training_sam3_labeler_trt.cpp`
- `CMakeLists.txt`

### Task 6: Wire hotkey-driven save requests into the capture loop

**Files:**
- Modify: `sunone_aimbot_2/capture/capture.cpp`
- Modify: `sunone_aimbot_2/config/config.h`
- Modify: `sunone_aimbot_2/config/config.cpp`
- Modify: `sunone_aimbot_2/training/training_label_runtime.h`
- Modify: `sunone_aimbot_2/training/training_label_runtime.cpp`

- [ ] **Step 1: Add failing runtime tests for enqueue-on-hotkey semantics where possible**

```cpp
training::LabelSettings settings;
settings.hotkey = {"F8"};
settings.className = "player";
settings.prompt = "enemy torso";
```

Add the test around the helper that converts current settings plus a frame copy into a queued request.

- [ ] **Step 2: Run the tests to confirm the helper behavior is not implemented yet**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: FAIL in the new helper assertions

- [ ] **Step 3: Add persisted config fields for the training workflow**

```cpp
bool training_label_enabled;
std::string training_sam3_engine_path;
std::string training_label_prompt;
std::string training_label_class;
std::string training_label_split;
std::vector<std::string> training_label_hotkey;
bool training_label_save_negatives;
bool training_label_preview_enabled;
int training_label_preview_interval_ms;
float training_label_confidence_threshold;
int training_label_min_box_area;
int training_label_max_detections;
std::string training_dataset_image_format;
```

Persist `training_label_class` as a class name, not a numeric class id. Disable saving if that class name no longer exists in the current class catalog.

- [ ] **Step 4: Trigger queued saves from the capture loop after a valid frame copy exists**

```cpp
const bool trainingHotkeyPressed =
    currentCfg.training_label_enabled &&
    training::IsTrainingHotkeyPressedThisFrame(currentCfg.training_label_hotkey) &&
    training::ShouldAcceptHotkeyNow(...);

if (trainingHotkeyPressed && !screenshotCpu.empty()) {
    training::EnqueueFromCaptureSnapshot(screenshotCpu, currentCfg, exePath);
}
```

- [ ] **Step 5: Keep the hotkey path independent from overlay visibility and screenshot saving**

Validate in code that:
- the save request uses a cloned `cv::Mat`
- prompt/class/split/image format are copied at enqueue time
- screenshot hotkey behavior remains unchanged

- [ ] **Step 6: Re-run the logic tests and build both app targets**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: PASS

Run: `cmake --build "build\dml" --config Release --target ai`
Expected: PASS

Run: `cmake --build "build\cuda" --config Release --target ai`
Expected: PASS

- [ ] **Step 7: Optional checkpoint if the user asks for commits**

Files to stage:
- `sunone_aimbot_2/capture/capture.cpp`
- `sunone_aimbot_2/config/config.h`
- `sunone_aimbot_2/config/config.cpp`
- `sunone_aimbot_2/training/training_label_runtime.h`
- `sunone_aimbot_2/training/training_label_runtime.cpp`

### Task 7: Add the `Training -> Label` editor tab and preview controls

**Files:**
- Create: `sunone_aimbot_2/overlay/draw_training.cpp`
- Modify: `sunone_aimbot_2/overlay/draw_settings.h`
- Modify: `sunone_aimbot_2/overlay/overlay.cpp`
- Modify: `sunone_aimbot_2/training/training_label_runtime.h`
- Modify: `sunone_aimbot_2/training/training_label_runtime.cpp`

- [ ] **Step 1: Add the new tab entry and a failing link target**

```cpp
{ "Label", "Training", "SAM3-assisted dataset labeling.", draw_training },
```

- [ ] **Step 2: Build the app to verify it fails until `draw_training()` exists**

Run: `cmake --build "build\cuda" --config Release --target ai`
Expected: FAIL with unresolved `draw_training`

- [ ] **Step 3: Implement the new UI file using the existing section helpers**

```cpp
void draw_training() {
    if (OverlayUI::BeginSection("SAM3 Session", "training_section_session")) {
        // engine path, prompt, thresholds, hotkey
        OverlayUI::EndSection();
    }
}
```

Make sure the tab contains all four spec sections and their required fields:
- `SAM3 Session`
- `Class Mapping`
- `Dataset Output`
- `Live Preview`

- [ ] **Step 4: Reuse preview upload patterns from `draw_debug.cpp`, but draw training boxes from runtime preview state**

```cpp
const auto preview = training::GetPreviewSnapshot();
cv::Mat annotated = preview.frame.clone();
for (const auto& det : preview.boxes) {
    cv::rectangle(annotated, det.box, cv::Scalar(0, 255, 0), 2);
}
```

- [ ] **Step 5: Make class editing follow the spec exactly**

Ensure the UI:
- stages add and rename in memory
- blocks destructive changes when label files already exist
- uses explicit `Save class metadata to disk`
- supports `Reload from disk`

- [ ] **Step 6: Rebuild the app and confirm the editor links successfully**

Run: `cmake --build "build\dml" --config Release --target ai`
Expected: PASS

Run: `cmake --build "build\cuda" --config Release --target ai`
Expected: PASS

- [ ] **Step 7: Optional checkpoint if the user asks for commits**

Files to stage:
- `sunone_aimbot_2/overlay/draw_training.cpp`
- `sunone_aimbot_2/overlay/draw_settings.h`
- `sunone_aimbot_2/overlay/overlay.cpp`

### Task 8: Final integration verification across CUDA and non-CUDA builds

**Files:**
- Modify only if needed during bug-fix follow-up from the checks below

- [ ] **Step 1: Build both app targets in release mode**

Run: `cmake --build "build\dml" --config Release --target ai`
Expected: PASS

Run: `cmake --build "build\cuda" --config Release --target ai`
Expected: PASS

- [ ] **Step 2: Launch the app and verify the new editor path appears**

Run: `.\build\cuda\Release\ai.exe`
Expected:
- overlay opens with `HOME`
- sidebar shows `Training`
- `Training -> Label` renders without layout breakage

- [ ] **Step 3: Verify config persistence**

Manual check:
- set engine path, prompt, class, split, hotkey, preview settings, thresholds, negative-save toggle, and image format
- save config through the existing dirty-save path
- restart the app
- confirm the values return

- [ ] **Step 4: Verify dataset generation**

Manual check after one positive save and one negative save:
- `<exe_dir>/training/predefined_classes.txt` exists
- `<exe_dir>/training/game.yaml` exists
- `<exe_dir>/training/start_train.bat` exists
- `<exe_dir>/training/datasets/game/images/train/` contains the positive image
- `<exe_dir>/training/datasets/game/labels/train/` contains the matching `.txt`
- `<exe_dir>/training/datasets/game/images/val/` contains the negative image if negatives are enabled
- no negative `.txt` exists for the negative-only frame

- [ ] **Step 5: Verify preview and save coherence**

Manual check:
- adjust prompt until preview boxes look correct
- verify preview refresh obeys the configured interval instead of updating every ImGui frame
- verify confidence, minimum box area, and max-detections filters affect preview and save-time results consistently
- press the label hotkey several times quickly
- confirm the queue accepts requests up to the limit
- confirm holding the hotkey does not enqueue repeated saves without a new key press
- confirm later UI prompt edits do not change already queued saves
- inspect saved YOLO rows and verify they match the intended class id and visible boxes

- [ ] **Step 6: Verify regressions stay clean when training labeling is off**

Manual check:
- screenshot hotkey still works as before
- existing `Monitor -> Debug` tools still work
- normal inference, targeting, and capture behavior are unchanged when `training_label_enabled` is off

- [ ] **Step 7: Run the standalone tests one last time**

Run: `.\build\dml\Release\training_labeling_tests.exe`
Expected: PASS with exit code `0`

- [ ] **Step 8: Optional checkpoint if the user asks for commits**

Files to stage:
- all files touched by the feature

## Plan Notes

- Do not update legacy `scripts/custom_dataset/` files as part of the first implementation pass unless a mismatch blocks the new runtime dataset flow.
- Keep DML builds compiling by using the SAM3 stub path when `USE_CUDA` is off.
- Reuse existing editor patterns from `draw_capture.cpp` and `draw_debug.cpp` instead of inventing a new UI system.
- Prefer helper functions that are easy to test outside the running app.

Plan complete and saved to `docs/superpowers/plans/2026-03-13-sam3-training-label-implementation.md`. Ready to execute?
