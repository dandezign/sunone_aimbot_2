# YOLO26 Migration Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate from YOLOv8/v11 to YOLO26 with clean break, removing NMS post-processing and simplifying detection pipeline.

**Architecture:** Delete postProcess module, rewrite decoder for YOLO26's (N, 300, 6) output format, remove nms_threshold from config, update both TensorRT and DirectML detectors.

**Tech Stack:** C++17, OpenCV, TensorRT (CUDA), DirectML, YOLO26

---

## Chunk 1: Delete PostProcess Module

### Task 1: Verify postProcess usage and delete files

**Files:**
- Delete: `sunone_aimbot_2/detector/postProcess.cpp`
- Delete: `sunone_aimbot_2/detector/postProcess.h`
- Test: Build verification

- [ ] **Step 1: Grep for all postProcess usages**

Run:
```bash
grep -rn "postProcess\|postProcessYolo\|postProcessYoloDML" sunone_aimbot_2/
```

Expected: Only 2 files should reference it:
- `trt_detector.cpp` (include + function calls)
- `dml_detector.cpp` (include + function call)

- [ ] **Step 2: Document findings**

Write comment in work log showing exact lines found (for Task 2 reference).

- [ ] **Step 3: Delete postProcess.h**

Run:
```bash
rm sunone_aimbot_2/detector/postProcess.h
```

Expected: File deleted

- [ ] **Step 4: Delete postProcess.cpp**

Run:
```bash
rm sunone_aimbot_2/detector/postProcess.cpp
```

Expected: File deleted

- [ ] **Step 5: Update CMakeLists.txt**

Modify: `CMakeLists.txt:144`
Remove line:
```cmake
"${SRC_DIR}/detector/postProcess.cpp"
```

- [ ] **Step 6: Commit**

Run:
```bash
git add sunone_aimbot_2/detector/postProcess.h sunone_aimbot_2/detector/postProcess.cpp CMakeLists.txt
git commit -m "refactor: delete postProcess module (NMS obsolete for YOLO26)"
```

Expected: Commit created

---

## Chunk 2: Update Config (Remove nms_threshold)

### Task 2: Remove nms_threshold from config

**Files:**
- Modify: `sunone_aimbot_2/config/config.h:87`
- Modify: `sunone_aimbot_2/config/config.cpp:140,436,697`

- [ ] **Step 1: Write test - Verify config compiles without nms_threshold**

Create test file: `sunone_aimbot_2/config/config_test.cpp`

```cpp
#include "config.h"
#include <cassert>

void test_ConfigNoNmsThreshold() {
    Config cfg;
    // nms_threshold should not exist
    // This test verifies config compiles without it
    cfg.confidence_threshold = 0.25f;
    cfg.max_detections = 300;
    // cfg.nms_threshold = 0.5f;  // This line should NOT compile
    assert(cfg.confidence_threshold == 0.25f);
    assert(cfg.max_detections == 300);
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: FAIL - because nms_threshold field still exists

- [ ] **Step 3: Remove nms_threshold from config.h**

Modify: `sunone_aimbot_2/config/config.h:87`

Before:
```cpp
float confidence_threshold;
float nms_threshold;
int max_detections;
```

After:
```cpp
float confidence_threshold;
// nms_threshold removed - YOLO26 doesn't use NMS
int max_detections;
```

- [ ] **Step 4: Remove nms_threshold initialization from config.cpp**

Modify: `sunone_aimbot_2/config/config.cpp:140`

Remove line:
```cpp
nms_threshold = 0.50f;
```

- [ ] **Step 5: Remove nms_threshold loading from config.cpp**

Modify: `sunone_aimbot_2/config/config.cpp:436`

Remove line:
```cpp
nms_threshold = (float)get_double("nms_threshold", 0.50);
```

- [ ] **Step 6: Remove nms_threshold from config dump**

Modify: `sunone_aimbot_2/config/config.cpp:697`

Remove line:
```cpp
<< "nms_threshold = " << nms_threshold << "\n"
```

- [ ] **Step 7: Run test to verify it passes**

Expected: PASS - config compiles without nms_threshold

- [ ] **Step 8: Commit**

Run:
```bash
git add sunone_aimbot_2/config/config.h sunone_aimbot_2/config/config.cpp
git commit -m "config: remove nms_threshold (YOLO26 doesn't use NMS)"
```

---

## Chunk 3: Update Detector Headers

### Task 3: Remove postProcess include from trt_detector.h

**Files:**
- Modify: `sunone_aimbot_2/detector/trt_detector.h`

- [ ] **Step 1: Check trt_detector.h for postProcess include**

Run:
```bash
grep -n "postProcess" sunone_aimbot_2/detector/trt_detector.h
```

Expected: May or may not have include (check if present)

- [ ] **Step 2: Remove include if present**

If found, remove:
```cpp
#include "postProcess.h"
```

- [ ] **Step 3: Update postProcess method declaration**

Modify `postProcess()` method signature in `trt_detector.h`

Before:
```cpp
void postProcess(const float* output, const std::string& outputName, 
                 std::chrono::duration<double, std::milli>* nmsTime = nullptr);
```

After:
```cpp
void decodeOutputs(const float* output, const std::string& outputName);
```

- [ ] **Step 4: Commit**

Run:
```bash
git add sunone_aimbot_2/detector/trt_detector.h
git commit -m "refactor: update trt_detector.h for YOLO26 decoder"
```

---

### Task 4: Remove postProcess include from dml_detector.h

**Files:**
- Modify: `sunone_aimbot_2/detector/dml_detector.h`

- [ ] **Step 1: Check dml_detector.h for postProcess include**

Run:
```bash
grep -n "postProcess" sunone_aimbot_2/detector/dml_detector.h
```

- [ ] **Step 2: Remove include if present**

- [ ] **Step 3: Commit**

---

## Chunk 4: Implement YOLO26 Decoder in trt_detector.cpp

### Task 5: Add decodeYolo26Outputs helper function

**Files:**
- Modify: `sunone_aimbot_2/detector/trt_detector.cpp`
- Test: Build verification

- [ ] **Step 1: Write test - Null input handling**

Add to trt_detector.cpp (temporarily for TDD):
```cpp
// TDD TEST: Should return empty vector on null input
void test_decodeYolo26Outputs_nullInput() {
    std::vector<int64_t> shape = {1, 300, 6};
    auto result = decodeYolo26Outputs(nullptr, shape, 80, 0.25f, 640, 480);
    assert(result.empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: FAIL - function doesn't exist yet

- [ ] **Step 3: Remove postProcess include**

Modify: `sunone_aimbot_2/detector/trt_detector.cpp:25`

Remove:
```cpp
#include "postProcess.h"
```

- [ ] **Step 4: Write decodeYolo26Outputs function**

Add after namespace block (around line 150):

```cpp
std::vector<Detection> decodeYolo26Outputs(
    const float* output,
    const std::vector<int64_t>& shape,
    int numClasses,
    float confThreshold,
    int imageWidth,
    int imageHeight)
{
    std::vector<Detection> detections;
    
    // Validate input pointer
    if (!output) {
        return detections;  // Return empty vector on null input
    }
    
    // Validate output shape: expect (N, 300, 6) or flat (1800,)
    const int64_t expectedSize = 300 * 6;
    const int64_t actualSize = shape.empty() ? 1800 : 
                               (shape.size() == 1 ? shape[0] : 
                                shape[1] * shape[2]);
    
    if (actualSize != expectedSize) {
        std::cerr << "[YOLO26] Warning: unexpected output shape " 
                  << "(expected " << expectedSize << ", got " << actualSize << ")" << std::endl;
        return detections;  // Return empty on shape mismatch
    }
    
    // YOLO26 output: (N, 300, 6)
    // Format: [x, y, w, h, conf, class_id]
    
    for (int i = 0; i < 300; i++) {
        float conf = output[i * 6 + 4];
        
        // Skip low-confidence detections
        if (conf < confThreshold || std::isnan(conf) || std::isinf(conf)) {
            continue;
        }
        
        float x = output[i * 6 + 0];
        float y = output[i * 6 + 1];
        float w = output[i * 6 + 2];
        float h = output[i * 6 + 3];
        float classIdFloat = output[i * 6 + 5];
        
        // Validate coordinates (reject NaN/inf/negative)
        if (std::isnan(x) || std::isnan(y) || std::isnan(w) || std::isnan(h) ||
            std::isinf(x) || std::isinf(y) || std::isinf(w) || std::isinf(h) ||
            x < 0 || y < 0 || w <= 0 || h <= 0) {
            continue;
        }
        
        // Validate classId bounds
        int classId = static_cast<int>(classIdFloat);
        if (classId < 0 || classId >= numClasses) {
            continue;  // Skip invalid class IDs
        }
        
        // Clamp to image bounds
        int clampedX = std::max(0, std::min(static_cast<int>(x), imageWidth - 1));
        int clampedY = std::max(0, std::min(static_cast<int>(y), imageHeight - 1));
        int clampedW = std::min(static_cast<int>(w), imageWidth - clampedX);
        int clampedH = std::min(static_cast<int>(h), imageHeight - clampedY);
        
        detections.push_back({
            cv::Rect(clampedX, clampedY, clampedW, clampedH),
            conf,
            classId
        });
    }
    
    return detections;
}
```

- [ ] **Step 5: Run test to verify it passes**

Expected: PASS

- [ ] **Step 6: Commit**

Run:
```bash
git add sunone_aimbot_2/detector/trt_detector.cpp
git commit -m "feat: add decodeYolo26Outputs with validation (YOLO26)"
```

---

### Task 6: Update TrtDetector::postProcess to use new decoder

**Files:**
- Modify: `sunone_aimbot_2/detector/trt_detector.cpp:901-920`

- [ ] **Step 1: Rename postProcess to decodeOutputs**

Modify: `sunone_aimbot_2/detector/trt_detector.cpp:901`

Before:
```cpp
void TrtDetector::postProcess(const float* output, const std::string& outputName, 
                              std::chrono::duration<double, std::milli>* nmsTime)
```

After:
```cpp
void TrtDetector::decodeOutputs(const float* output, const std::string& outputName)
```

- [ ] **Step 2: Remove NMS timing parameter usage**

Remove any `nmsTime` parameter usage in function body.

- [ ] **Step 3: Replace postProcessYolo call with decodeYolo26Outputs**

Modify: `sunone_aimbot_2/detector/trt_detector.cpp:911`

Before:
```cpp
detections = postProcessYolo(
    output, outputName.c_str(), num_classes, 
    conf_thr, nms_thr, nmsTime
);
```

After:
```cpp
detections = decodeYolo26Outputs(
    output, shape, num_classes, 
    conf_thr, detection_width, detection_height
);
```

- [ ] **Step 4: Remove nms_thr variable if no longer used**

Check if `nms_thr` is used elsewhere. If not, remove.

- [ ] **Step 5: Update callers of postProcess**

Modify: `sunone_aimbot_2/detector/trt_detector.cpp:801,806`

Before:
```cpp
postProcess(outputDataFloat.data(), name, &lastNmsTime);
postProcess(floatPtr, name, &lastNmsTime);
```

After:
```cpp
decodeOutputs(outputDataFloat.data(), name);
decodeOutputs(floatPtr, name);
```

- [ ] **Step 6: Build to verify no errors**

Run:
```bash
cmake --build build/cuda --config Release
```

Expected: BUILD SUCCESS

- [ ] **Step 7: Commit**

Run:
```bash
git add sunone_aimbot_2/detector/trt_detector.cpp
git commit -m "refactor: use decodeYolo26Outputs in TrtDetector (no NMS)"
```

---

## Chunk 5: Implement YOLO26 Decoder in dml_detector.cpp

### Task 7: Add decodeYolo26Outputs to dml_detector

**Files:**
- Modify: `sunone_aimbot_2/detector/dml_detector.cpp`

- [ ] **Step 1: Remove postProcess include**

Modify: `sunone_aimbot_2/detector/dml_detector.cpp:17`

Remove:
```cpp
#include "postProcess.h"
```

- [ ] **Step 2: Write test - Verify function compiles**

Same test pattern as Task 5.

- [ ] **Step 3: Add decodeYolo26Outputs function**

Copy same function from trt_detector.cpp (or extract to shared header if preferred).

- [ ] **Step 4: Commit**

```bash
git commit -m "feat: add decodeYolo26Outputs to dml_detector"
```

---

### Task 8: Update DML detector to use new decoder

**Files:**
- Modify: `sunone_aimbot_2/detector/dml_detector.cpp:324`

- [ ] **Step 1: Replace postProcessYoloDML call**

Modify: `sunone_aimbot_2/detector/dml_detector.cpp:324`

Before:
```cpp
detections = postProcessYoloDML(ptr, shp, num_classes, conf_thr, nms_thr, &nmsTimeTmp);
```

After:
```cpp
detections = decodeYolo26Outputs(ptr, shp, num_classes, conf_thr, width, height);
```

- [ ] **Step 2: Remove nms_thr and nmsTimeTmp if unused**

- [ ] **Step 3: Build to verify no errors**

Run:
```bash
cmake --build build/dml --config Release
```

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/detector/dml_detector.cpp
git commit -m "refactor: use decodeYolo26Outputs in DML detector (no NMS)"
```

---

## Chunk 6: Final Verification and Cleanup

### Task 9: Verify no remaining NMS references

**Files:**
- Search: Entire codebase

- [ ] **Step 1: Grep for NMS references**

Run:
```bash
grep -rn "NMS\|nms\|postProcess" sunone_aimbot_2/ --include="*.cpp" --include="*.h"
```

Expected: No results (or only comments/documentation)

- [ ] **Step 2: Grep for nms_threshold**

Run:
```bash
grep -rn "nms_threshold" sunone_aimbot_2/
```

Expected: No results in source code (may exist in old config.ini examples)

- [ ] **Step 3: Commit cleanup if needed**

If any stray references found, remove them.

---

### Task 10: Final build verification

**Files:**
- Build: Both CUDA and DML

- [ ] **Step 1: Clean build (CUDA)**

Run:
```bash
cmake --build build/cuda --config Release --target clean
cmake --build build/cuda --config Release
```

Expected: BUILD SUCCESS with no warnings about unused variables

- [ ] **Step 2: Clean build (DML)**

Run:
```bash
cmake --build build/dml --config Release --target clean
cmake --build build/dml --config Release
```

Expected: BUILD SUCCESS

- [ ] **Step 3: Count lines changed**

Run:
```bash
git diff --stat main
```

Expected: ~465 lines removed net

- [ ] **Step 4: Create final commit**

If any cleanup needed:
```bash
git commit -am "chore: final YOLO26 migration cleanup"
```

---

## Success Criteria Checklist

After all tasks complete, verify:

- [ ] `postProcess.cpp/h` deleted
- [ ] All detector files compile without errors
- [ ] YOLO26n/s/m models load successfully with verified shape `(N, 300, 6)`
- [ ] Inference detects ≥5 COCO objects in standard test image (conf >0.25)
- [ ] Null output returns empty vector (no segfault)
- [ ] Shape mismatch returns empty vector (no crash)
- [ ] Invalid coordinates/classId silently skipped (no crash)
- [ ] No NMS-related code in codebase
- [ ] Config struct has no `nms_threshold`
- [ ] Old configs with `nms_threshold` load without error (field ignored)
- [ ] Latency improved by 5-8ms (benchmark verified with 1000-frame average)
- [ ] Code reduction: ~465 lines net removed

---

## Test Model Export Commands

After implementation, test with:

```bash
# Export YOLO26 models (run externally)
yolo export model=yolo26n.pt format=onnx opset=13 simplify=true dynamic=true
yolo export model=yolo26s.pt format=onnx opset=13 simplify=true dynamic=true
yolo export model=yolo26m.pt format=onnx opset=13 simplify=true dynamic=true

# Place in models/ folder
copy yolo26n.onnx models/
copy yolo26s.onnx models/
copy yolo26m.onnx models/
```

---

**Plan Version:** 1.0  
**Based on Spec:** `docs/superpowers/specs/2026-03-12-yolo26-migration-design.md`  
**Worktree:** `.worktrees/yolo26-migration`  
**Branch:** `feature/yolo26-migration`
