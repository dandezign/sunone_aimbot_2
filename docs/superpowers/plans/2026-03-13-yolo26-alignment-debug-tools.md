# YOLO26 Alignment Debug Tools Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add editor-only debug tooling that captures raw frames, annotated frames, logs, and detector metadata so YOLO26 box drift can be diagnosed from saved evidence.

**Architecture:** Add a shared debug-state module that both detectors can publish into and the Debug tab can read from. Add an export module that snapshots frame, detections, config, and detector metadata into one immutable bundle, then writes files off the draw path. Keep the current detector math unchanged; this work gathers proof before any fix.

**Tech Stack:** C++17, ImGui, OpenCV, ONNX Runtime DirectML, TensorRT, standard filesystem/threading, CMake.

---

## File Structure

### Create
- `sunone_aimbot_2/debug/detection_debug_state.h` - shared snapshot types, event-log API, timed-capture state API
- `sunone_aimbot_2/debug/detection_debug_state.cpp` - mutex-protected shared state, bounded event log, timed-capture bookkeeping
- `sunone_aimbot_2/debug/detection_debug_export.h` - bundle snapshot and export API
- `sunone_aimbot_2/debug/detection_debug_export.cpp` - bundle assembly, JSON writing, image annotation, worker-backed export queue
- `tests/detection_debug_tools_test.cpp` - pure helper checks for JSON contract, event-log truncation, and bundle naming

### Modify
- `CMakeLists.txt` - compile new debug modules into `ai`; add `detection_debug_tools_tests` console target
- `sunone_aimbot_2/detector/trt_detector.cpp` - publish decode metadata and event-log records after YOLO26 decode
- `sunone_aimbot_2/detector/dml_detector.cpp` - publish decode metadata and event-log records after YOLO26 decode
- `sunone_aimbot_2/overlay/draw_debug.cpp` - add diagnostics UI, timed capture controls, status text, and export buttons
- `sunone_aimbot_2/capture/capture.cpp` - drive timed capture requests from the capture loop using the actual `latestFrame` producer thread
- `sunone_aimbot_2/config/config.h` - only if needed for persisted debug defaults; otherwise leave unchanged
- `sunone_aimbot_2/config/config.cpp` - only if needed for persisted debug defaults; otherwise leave unchanged

### Responsibilities
- `detection_debug_state.*` owns data that must be shared safely across detectors, capture, and UI
- `detection_debug_export.*` owns immutable snapshot assembly and artifact writing
- detector files only publish facts; they do not write files or know about ImGui
- `draw_debug.cpp` only renders controls and calls helper APIs
- `capture.cpp` is the right place to trigger timed exports because it already owns the live frame cadence and screenshot save timing

---

## Chunk 1: Shared Debug State And Test Harness

### Task 1: Create the shared debug-state header

**Files:**
- Create: `sunone_aimbot_2/debug/detection_debug_state.h`

- [ ] **Step 1: Write the failing test for bounded event-log behavior**

```cpp
int main() {
    detection_debug::ResetForTests();
    for (int i = 0; i < 300; ++i) {
        detection_debug::AppendEvent("event-" + std::to_string(i));
    }
    const auto snapshot = detection_debug::GetSharedStateSnapshot();
    assert(!snapshot.eventLog.empty());
    assert(snapshot.eventLog.size() <= detection_debug::kMaxDebugEvents);
    assert(snapshot.eventLog.back().find("event-299") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -S . -B build/cuda -G "Visual Studio 18 2026" -A x64 -T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1" -DAIMBOT_USE_CUDA=ON -DCMAKE_CUDA_FLAGS="--allow-unsupported-compiler" -DCUDA_NVCC_FLAGS="--allow-unsupported-compiler" && cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL because `detection_debug_state.h` and the test target do not exist yet.

- [ ] **Step 3: Add the shared types and API declarations**

```cpp
namespace detection_debug {

inline constexpr size_t kMaxDebugEvents = 256;

struct RawBoxDebug {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct FinalBoxDebug {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct DetectionDebugEntry {
    int index = 0;
    int classId = -1;
    float confidence = 0.0f;
    RawBoxDebug rawBox;
    FinalBoxDebug finalBox;
};

struct DetectorSnapshot {
    std::string backend;
    std::string modelPath;
    std::string detectorTimestampUtc;
    std::optional<int> detectorInputWidth;
    std::optional<int> detectorInputHeight;
    std::optional<int> modelWidth;
    std::optional<int> modelHeight;
    std::vector<int64_t> outputShape;
    std::optional<float> xGain;
    std::optional<float> yGain;
    std::string boxConvention;
    std::vector<DetectionDebugEntry> detections;
};

struct SharedStateSnapshot {
    DetectorSnapshot detector;
    std::vector<std::string> eventLog;
};

void PublishDetectorSnapshot(const DetectorSnapshot& snapshot);
void AppendEvent(const std::string& message);
SharedStateSnapshot GetSharedStateSnapshot();
void ResetForTests();

} // namespace detection_debug
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/debug/detection_debug_state.h
git commit -m "feat: add shared YOLO26 debug state interface"
```

### Task 2: Implement shared debug state in `.cpp`

**Files:**
- Create: `sunone_aimbot_2/debug/detection_debug_state.cpp`

- [ ] **Step 1: Write the failing test for snapshot copy semantics**

```cpp
int main() {
    detection_debug::ResetForTests();
    detection_debug::DetectorSnapshot snapshot;
    snapshot.backend = "TRT";
    snapshot.outputShape = {1, 300, 6};
    detection_debug::PublishDetectorSnapshot(snapshot);

    auto first = detection_debug::GetSharedStateSnapshot();
    first.detector.backend = "mutated";

    auto second = detection_debug::GetSharedStateSnapshot();
    assert(second.detector.backend == "TRT");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL because the implementation is missing.

- [ ] **Step 3: Implement mutex-protected state and a bounded event log**

```cpp
namespace detection_debug {
namespace {
std::mutex g_debugMutex;
DetectorSnapshot g_lastDetectorSnapshot;
std::deque<std::string> g_eventLog;
}

void PublishDetectorSnapshot(const DetectorSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_lastDetectorSnapshot = snapshot;
}

void AppendEvent(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_debugMutex);
    g_eventLog.push_back(message);
    while (g_eventLog.size() > kMaxDebugEvents) {
        g_eventLog.pop_front();
    }
}
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: PASS for the new shared-state checks.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/debug/detection_debug_state.cpp tests/detection_debug_tools_test.cpp CMakeLists.txt
git commit -m "feat: add shared YOLO26 debug state implementation and tests"
```

### Task 3: Add the lightweight test target

**Files:**
- Modify: `CMakeLists.txt:131`
- Modify: `CMakeLists.txt:213`

- [ ] **Step 1: Add only `detection_debug_state.cpp` to `AIMBOT_SOURCES` first**

```cmake
    "${SRC_DIR}/debug/detection_debug_state.cpp"
```

- [ ] **Step 2: Add a standalone console test target**

```cmake
add_executable(detection_debug_tools_tests
    "${CMAKE_SOURCE_DIR}/tests/detection_debug_tools_test.cpp"
    "${SRC_DIR}/debug/detection_debug_state.cpp"
)

target_include_directories(detection_debug_tools_tests PRIVATE
    "${SRC_DIR}"
)
```

- [ ] **Step 3: Run the test target build**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: PASS after the target and source lists are correct.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt tests/detection_debug_tools_test.cpp
git commit -m "build: add detection debug tools test target"
```

---

## Chunk 2: Snapshot Export And Detector Instrumentation

### Task 4: Create the export helper API

**Files:**
- Create: `sunone_aimbot_2/debug/detection_debug_export.h`

- [ ] **Step 1: Write the failing test for bundle-name and JSON-key contract**

```cpp
int main() {
    const auto bundleId = detection_debug::MakeBundleIdForTests("2026-03-13T21:10:44.182Z");
    assert(bundleId.find("yolo26_debug") != std::string::npos);

    detection_debug::BundleSnapshot snapshot;
    snapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
    snapshot.bundleId = bundleId;
    const auto jsonText = detection_debug::BuildBundleJson(snapshot);
    assert(jsonText.find("\"export_timestamp\"") != std::string::npos);
    assert(jsonText.find("\"detector\"") != std::string::npos);
    assert(jsonText.find("\"detections\"") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL because the export helper declarations do not exist yet.

- [ ] **Step 3: Declare immutable bundle snapshot and export entry points**

```cpp
namespace detection_debug {

struct BundleSnapshot {
    std::string exportTimestampUtc;
    std::string bundleId;
    std::string frameTimestampUtc;
    cv::Mat frame;
    std::vector<cv::Rect> boxes;
    std::vector<int> classes;
    std::optional<int> detectionBufferVersion;
    DetectorSnapshot detector;
    std::vector<std::string> eventLog;
    std::string backend;
    std::string modelPath;
    std::optional<int> detectionResolution;
};

BundleSnapshot CaptureBundleSnapshot();
bool QueueBundleExport(const BundleSnapshot& snapshot, std::string* outPath, std::string* outStatus);
std::string BuildBundleJson(const BundleSnapshot& snapshot);
std::string MakeBundleIdForTests(const std::string& utcTimestamp);

} // namespace detection_debug
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/debug/detection_debug_export.h
git commit -m "feat: add YOLO26 debug export interface"
```

### Task 5: Implement immutable snapshot capture and worker-backed export

**Files:**
- Create: `sunone_aimbot_2/debug/detection_debug_export.cpp`
- Modify: `CMakeLists.txt:131`
- Modify: `CMakeLists.txt:213`

- [ ] **Step 1: Write the failing test for JSON null-and-array behavior**

```cpp
int main() {
    detection_debug::BundleSnapshot snapshot;
    snapshot.exportTimestampUtc = "2026-03-13T21:10:44.182Z";
    snapshot.bundleId = "2026-03-13_21-10-44-182_yolo26_debug";
    const std::string json = detection_debug::BuildBundleJson(snapshot);
    assert(json.find("\"output_shape\":[]") != std::string::npos);
    assert(json.find("\"x_gain\":null") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL because JSON serialization is not implemented.

- [ ] **Step 3: Implement snapshot capture and export queue**

```cpp
BundleSnapshot CaptureBundleSnapshot() {
    BundleSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        if (!latestFrame.empty()) {
            latestFrame.copyTo(snapshot.frame);
        }
        snapshot.frameTimestampUtc = MakeUtcTimestamp();
    }
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        snapshot.boxes = detectionBuffer.boxes;
        snapshot.classes = detectionBuffer.classes;
        snapshot.detectionBufferVersion = detectionBuffer.version;
    }
    {
        std::lock_guard<std::mutex> lock(configMutex);
        snapshot.backend = config.backend;
        snapshot.modelPath = config.ai_model;
        snapshot.detectionResolution = config.detection_resolution;
    }

    const auto shared = detection_debug::GetSharedStateSnapshot();
    snapshot.detector = shared.detector;
    snapshot.eventLog = shared.eventLog;
    snapshot.exportTimestampUtc = MakeUtcTimestamp();
    snapshot.bundleId = MakeBundleIdForTests(snapshot.exportTimestampUtc);
    return snapshot;
}
```

- [ ] **Step 4: Implement annotation and JSON writing helpers**

```cpp
static cv::Mat BuildAnnotatedFrame(const BundleSnapshot& snapshot) {
    cv::Mat annotated = snapshot.frame.clone();
    for (size_t i = 0; i < snapshot.boxes.size(); ++i) {
        const cv::Rect& box = snapshot.boxes[i];
        cv::rectangle(annotated, box, cv::Scalar(0, 0, 255), 2);
        cv::circle(annotated, cv::Point(box.x + box.width / 2, box.y + box.height / 2), 2, cv::Scalar(0, 255, 255), cv::FILLED);
    }
    return annotated;
}
```

- [ ] **Step 5: Extend CMake now that the export source exists**

```cmake
list(APPEND AIMBOT_SOURCES
    "${SRC_DIR}/debug/detection_debug_export.cpp"
)

target_sources(detection_debug_tools_tests PRIVATE
    "${SRC_DIR}/debug/detection_debug_export.cpp"
)

target_link_libraries(detection_debug_tools_tests PRIVATE
    "${AIMBOT_OPENCV_LIBRARY}"
)
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add sunone_aimbot_2/debug/detection_debug_export.cpp
git commit -m "feat: add YOLO26 debug bundle export pipeline"
```

### Task 6: Publish detector metadata from the DML path

**Files:**
- Modify: `sunone_aimbot_2/detector/dml_detector.cpp:280`
- Modify: `sunone_aimbot_2/debug/detection_debug_state.h`
- Modify: `sunone_aimbot_2/debug/detection_debug_state.cpp`

- [ ] **Step 1: Write the failing test for a helper that preserves raw and final box data**

```cpp
int main() {
    const cv::Rect finalBox(110, 90, 32, 64);
    const auto entry = detection_debug::MakeDetectionDebugEntry(0, 1, 0.91f, {220.0f, 180.0f, 64.0f, 128.0f}, finalBox);
    assert(entry.classId == 1);
    assert(entry.rawBox.x == 220.0f);
    assert(entry.finalBox.w == 32);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL because `MakeDetectionDebugEntry(...)` does not exist yet.

- [ ] **Step 3: Add `MakeDetectionDebugEntry(...)` to the shared debug module**

```cpp
DetectionDebugEntry MakeDetectionDebugEntry(
    int index,
    int classId,
    float confidence,
    const RawBoxDebug& rawBox,
    const cv::Rect& finalBox);
```

- [ ] **Step 4: Publish a `DetectorSnapshot` right after decode**

```cpp
detection_debug::DetectorSnapshot debugSnapshot;
debugSnapshot.backend = "DML";
debugSnapshot.modelPath = model_path;
debugSnapshot.detectorTimestampUtc = detection_debug::MakeUtcTimestamp();
debugSnapshot.detectorInputWidth = target_w;
debugSnapshot.detectorInputHeight = target_h;
debugSnapshot.modelWidth = 640;
debugSnapshot.modelHeight = 640;
debugSnapshot.outputShape = outShape;
debugSnapshot.xGain = x_gain;
debugSnapshot.yGain = y_gain;
debugSnapshot.boxConvention = "top_left_wh";
for (size_t i = 0; i < detections.size(); ++i) {
    // copy confidence, raw box, final box
}
detection_debug::PublishDetectorSnapshot(debugSnapshot);
```

- [ ] **Step 5: Append useful DML debug events**

```cpp
detection_debug::AppendEvent("[DML] output shape = [1,300,6]");
detection_debug::AppendEvent("[DML] detector input = 320x320");
```

- [ ] **Step 5: Run the test target and CUDA build**
- [ ] **Step 6: Run the test target and CUDA build**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests && powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS for tests and successful CUDA build.

- [ ] **Step 7: Commit**

```bash
git add sunone_aimbot_2/detector/dml_detector.cpp
git commit -m "feat: publish DML YOLO26 debug metadata"
```

### Task 7: Publish detector metadata from the TRT path

**Files:**
- Modify: `sunone_aimbot_2/detector/trt_detector.cpp:978`

- [ ] **Step 1: Reuse the shared helper tests from Task 6 and add TRT wiring only**

Run: no new command. Expected: the shared helper test already protects raw/final box preservation.

- [ ] **Step 2: Publish the TRT decode snapshot after `decodeYolo26Outputs(...)`**

```cpp
detection_debug::DetectorSnapshot debugSnapshot;
debugSnapshot.backend = "TRT";
debugSnapshot.modelPath = inputName;
debugSnapshot.detectorTimestampUtc = detection_debug::MakeUtcTimestamp();
debugSnapshot.detectorInputWidth = gameWidth;
debugSnapshot.detectorInputHeight = gameHeight;
debugSnapshot.modelWidth = modelSize;
debugSnapshot.modelHeight = modelSize;
debugSnapshot.outputShape = shapeIt->second;
debugSnapshot.xGain = x_gain;
debugSnapshot.yGain = y_gain;
debugSnapshot.boxConvention = "top_left_wh";
detection_debug::PublishDetectorSnapshot(debugSnapshot);
```

- [ ] **Step 3: Add TRT event-log records for shape or input changes**

Run this logic only when values change, not every frame.

- [ ] **Step 4: Run the tests and CUDA build**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests && powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS and successful build.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/detector/trt_detector.cpp
git commit -m "feat: publish TRT YOLO26 debug metadata"
```

---

## Chunk 3: Editor Controls, Timed Capture, And End-To-End Verification

### Task 8: Add timed capture state and capture-loop integration

**Files:**
- Modify: `sunone_aimbot_2/debug/detection_debug_state.h`
- Modify: `sunone_aimbot_2/debug/detection_debug_state.cpp`
- Modify: `sunone_aimbot_2/capture/capture.cpp:311`

- [ ] **Step 1: Write the failing test for burst countdown behavior**

```cpp
int main() {
    detection_debug::ResetForTests();
    detection_debug::TimedCaptureRequest request;
    request.intervalMs = 200;
    request.remainingShots = 3;
    detection_debug::StartTimedCapture(request);
    auto state = detection_debug::GetTimedCaptureState();
    assert(state.active);
    assert(state.remainingShots == 3);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL because timed capture types and APIs do not exist yet.

- [ ] **Step 3: Add timed-capture request/state structs and APIs**

```cpp
struct TimedCaptureRequest {
    int intervalMs = 1000;
    int remainingShots = 1;
    std::string prefix;
};

struct TimedCaptureState {
    bool active = false;
    int intervalMs = 1000;
    int remainingShots = 0;
    std::string lastStatus;
};
```

- [ ] **Step 4: Trigger exports from the capture loop**

Inside `captureThread(...)`, after `latestFrame` has been updated and before frame limiting, call a helper that:

- checks whether a timed shot is due
- captures one immutable bundle snapshot
- queues export
- decrements remaining shots
- stops on fatal failures
- measures interval from one shot start to the next shot start
- starts the next shot immediately if the last export took longer than the interval
- treats missing frame or empty detections as non-fatal omissions that still count as a completed shot

- [ ] **Step 5: Run tests and a CUDA build**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests && powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: PASS and successful build.

- [ ] **Step 6: Commit**

```bash
git add sunone_aimbot_2/debug/detection_debug_state.h sunone_aimbot_2/debug/detection_debug_state.cpp sunone_aimbot_2/capture/capture.cpp
git commit -m "feat: add timed YOLO26 debug capture scheduling"
```

### Task 9: Extend `Monitor -> Debug` with diagnostics controls

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_debug.cpp:341`

- [ ] **Step 1: Write the failing test for JSON visibility fields you will surface in UI**

```cpp
int main() {
    detection_debug::BundleSnapshot snapshot;
    snapshot.backend = "TRT";
    snapshot.detectionResolution = 320;
    snapshot.detector.modelWidth = 640;
    const std::string json = detection_debug::BuildBundleJson(snapshot);
    assert(json.find("\"backend\":\"TRT\"") != std::string::npos);
    assert(json.find("\"detection_resolution\":320") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/cuda --config Release --target detection_debug_tools_tests`
Expected: FAIL until the exported bundle includes the exact UI-visible fields.

- [ ] **Step 3: Add a `Detection Diagnostics` section to `draw_debug()`**

Include:

- buttons for `Save Raw Frame`, `Save Annotated Frame`, `Save Debug Bundle`, `Export Debug Log`
- timed capture inputs for interval, burst count, prefix
- `Start Timed Capture` and `Stop Timed Capture`
- read-only `Live` fields for frame size, detection resolution, output shape, gains, backend, box convention, detection count
- read-only `Live` fields for current model path, detector input size, model size, and min/max box extents
- read-only `Last Saved` fields for status, path, backend, detection count, and bundle id
- explicit labels that separate `Live` values from `Last Saved` values

- [ ] **Step 4: Keep the draw path thin**

Call helper APIs only. Do not add filesystem logic directly into `draw_debug.cpp`.

- [ ] **Step 5: Run a full clean CUDA build**

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1 -Rebuild`
Expected: Build succeeds and `build/cuda/Release/ai.exe` is produced.

- [ ] **Step 6: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_debug.cpp
git commit -m "feat: add YOLO26 diagnostics controls to monitor debug tab"
```

### Task 10: Verify the tools manually from the running editor

**Files:**
- Modify: none unless defects are found

- [ ] **Step 1: Launch the CUDA build**

Run: `build/cuda/Release/ai.exe`
Expected: Application starts and the editor opens with `Home`.

- [ ] **Step 2: Verify manual bundle export**

In `Monitor -> Debug`, click `Save Debug Bundle`.
Expected:
- a new folder appears under `build/cuda/Release/debug/`
- it contains `frame_raw.png`, `frame_annotated.png`, `detection_debug.json`, and `debug_log.txt`

- [ ] **Step 3: Verify timed capture**

Set interval `250 ms`, burst `5`, then click `Start Timed Capture`.
Expected:
- five bundle folders are created without overlapping writes
- the UI reports completion or the first fatal failure clearly

- [ ] **Step 4: Inspect one JSON bundle for root-cause evidence**

Confirm it includes:
- backend
- frame size
- frame timestamp
- detection resolution
- model size
- detector timestamp
- detection buffer version when available
- output shape
- x/y gains
- `box_convention`
- raw and final box data

- [ ] **Step 5: Rebuild DML as a compile safety check**

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Dml.ps1`
Expected: DML build succeeds after the shared debug module changes.

- [ ] **Step 6: Launch the DML path and save one bundle**

Run: `build/cuda/Release/ai.exe`
Expected: after switching `backend = DML` in `build/cuda/Release/config.ini`, one Debug-tab export writes a bundle whose JSON shows `"backend":"DML"` and live detector values instead of empty placeholders.

- [ ] **Step 7: Verify missing-frame and empty-detection handling**

Manual check:
- start the app before a valid frame source is available, then trigger `Save Debug Bundle`
- pause or force a no-detection scene, then trigger another bundle save

Expected:
- the editor reports the omission clearly
- export still writes JSON and log files when possible
- the app does not crash or hang

- [ ] **Step 8: Commit any final fixes from verification**

```bash
git add -A
git commit -m "fix: polish YOLO26 diagnostics export workflow"
```

### Task 11: Record the first decoder hypothesis without changing behavior

**Files:**
- Modify: `sunone_aimbot_2/detector/dml_detector.cpp:129`
- Modify: `sunone_aimbot_2/detector/trt_detector.cpp:76`

- [ ] **Step 1: Add one short code comment near each YOLO26 decoder entry point**

```cpp
// Debug bundles are expected to confirm whether YOLO26 emits center-based or top-left boxes.
```

- [ ] **Step 2: Do not change decode math in this task**

Run: no command. Expected: no behavior change.

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/detector/dml_detector.cpp sunone_aimbot_2/detector/trt_detector.cpp
git commit -m "docs: mark YOLO26 box-convention hypothesis in decoder paths"
```
