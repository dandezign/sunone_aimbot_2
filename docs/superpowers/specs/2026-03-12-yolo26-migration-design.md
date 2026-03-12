# YOLO26 Migration Implementation Design

> **Document Type:** Design Specification  
> **Project:** Sunone Aimbot 2  
> **Date:** 2026-03-12  
> **Status:** Approved

---

## Executive Summary

Migrate from YOLOv8/v11 to YOLO26 with a **clean break** approach, removing NMS post-processing entirely and simplifying the detection pipeline. This migration reduces codebase by ~15%, improves latency by 5-8ms, and provides better small-object detection.

---

## Goals

| Goal | Target |
|------|--------|
| **Code Reduction** | -15% (~465 lines removed) |
| **Latency Improvement** | -5-8ms (NMS elimination) |
| **Small-Object mAP** | +2.3% (YOLO26 STAL improvement) |
| **Model Support** | YOLO26n, YOLO26s, YOLO26m |
| **Backward Compatibility** | None (clean break) |

---

## Architecture

### Current State (Pre-Migration)

```
┌─────────────┐    ┌─────────────┐    ┌──────────────┐    ┌─────────┐    ┌──────────┐
│  Preprocess │ -> │  Inference │ -> │  Decode + NMS │ -> │  Filter │ -> │  Return  │
│             │    │            │    │  (80 lines)   │    │         │    │          │
└─────────────┘    └─────────────┘    └──────────────┘    └─────────┘    └──────────┘
                                                    ↑
                                         postProcess.cpp (400 lines)
```

### Target State (Post-Migration)

```
┌─────────────┐    ┌─────────────┐    ┌──────────────┐    ┌──────────┐
│  Preprocess │ -> │  Inference │ -> │   Decode     │ -> │  Return  │
│             │    │            │    │  (simple)    │    │          │
└─────────────┘    └─────────────┘    └──────────────┘    └──────────┘
```

### YOLO26 Output Format

| Attribute | Value |
|-----------|-------|
| **Shape** | `(N, 300, 6)` |
| **Max Detections** | 300 (native limit) |
| **Format per Detection** | `[x, y, w, h, conf, class_id]` |
| **NMS Required** | No (end-to-end, NMS-free) |

---

## Files Affected

### Files to Delete

| File | Lines | Reason |
|------|-------|--------|
| `sunone_aimbot_2/detector/postProcess.cpp` | ~400 | NMS implementation obsolete |
| `sunone_aimbot_2/detector/postProcess.h` | ~50 | NMS declarations obsolete |

### Files to Modify

| File | Lines Changed | Change Summary |
|------|---------------|----------------|
| `sunone_aimbot_2/detector/trt_detector.cpp` | ~80 | Remove NMS calls, new decoder |
| `sunone_aimbot_2/detector/dml_detector.cpp` | ~80 | Remove NMS calls, new decoder |
| `sunone_aimbot_2/config/config.h` | ~10 | Remove `nms_threshold` field |
| `sunone_aimbot_2/config/config.cpp` | ~15 | Remove NMS config loading |
| `sunone_aimbot_2/sunone_aimbot_2.h` | ~5 | Update if Detection struct changes |

### Build Files to Modify

| File | Change |
|------|--------|
| `CMakeLists.txt` | Remove `detector/postProcess.cpp` from sources |

---

## Design Decisions

### 1. Clean Break (No Backward Compatibility)

**Decision:** Remove all YOLOv8/v11 support. Existing models will not work.

**Rationale:**
- 40% code reduction in detector module
- Simpler maintenance and debugging
- Aligns with future training feature plans
- No existing YOLO26 trained models to preserve

### 2. Decoder Rewrite (Not Unified)

**Decision:** New `decodeYolo26Outputs()` function, remove old decoder.

**Rationale:**
- Self-documenting code
- No runtime format detection overhead
- Easier to test and verify
- Clean separation of concerns

### 3. Config Cleanup (Remove, Don't Deprecate)

**Decision:** Remove `nms_threshold` entirely from config.

**Rationale:**
- Clean break philosophy
- No legacy config baggage
- Simpler config validation
- Document change for existing users

---

## Implementation Details

### Detection Struct (Unchanged)

```cpp
struct Detection
{
    cv::Rect box;
    float confidence;
    int classId;
};
```

### New Decoder Signature

```cpp
// trt_detector.cpp / dml_detector.cpp
std::vector<Detection> decodeYolo26Outputs(
    const float* output,
    const std::vector<int64_t>& shape,
    int numClasses,
    float confThreshold,
    int imageWidth,   // For bounds validation
    int imageHeight   // For bounds validation
);
```

### Decoder Logic with Validation

```cpp
std::vector<Detection> decodeYolo26Outputs(...) {
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
        // Log warning: unexpected output shape
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
    
    // Return all detections up to 300 (YOLO26 native limit)
    // Note: max_detections config can cap this in caller if desired
    return detections;
}
```

### Config Changes

**Before:**
```cpp
// config.h
struct Config {
    float confidence_threshold;
    float nms_threshold;  // ← REMOVE
    int max_detections;
};
```

**After:**
```cpp
// config.h
struct Config {
    float confidence_threshold;
    // nms_threshold removed - YOLO26 doesn't use NMS
    int max_detections;  // Now reflects native 300 limit
};
```

### INI File Changes

**Before:**
```ini
[Detection]
model_path=models/yolov8n.onnx
conf_threshold=0.25
nms_threshold=0.45
max_detections=100
```

**After:**
```ini
[Detection]
model_path=models/yolo26n.onnx
conf_threshold=0.25
max_detections=300
```

---

## Removed Code Summary

### postProcess.cpp (Entire File - ~400 Lines)

**Key functions removed:**
- `NMS()` - Non-maximum suppression
- `postProcessYolo()` - YOLOv8 output decoding with NMS
- `postProcessYoloDML()` - DirectML post-processing with NMS

**Impact:** All NMS logic eliminated. No replacement needed.

### Config Fields Removed

| Field | Type | Default | Replacement |
|-------|------|---------|-------------|
| `nms_threshold` | float | 0.45 | None (not used) |

---

## Testing Strategy

### Pre-Trained Model Testing

Since custom datasets are out of scope, test with COCO pretrained models:

```bash
# Export YOLO26n/s/m to ONNX
yolo export model=yolo26n.pt format=onnx dynamic=true simplify=true
yolo export model=yolo26s.pt format=onnx dynamic=true simplify=true
yolo export model=yolo26m.pt format=onnx dynamic=true simplify=true
```

### Validation Tests

| Test | Expected Result |
|------|-----------------|
| Load YOLO26n ONNX | Success, shape verified `(N, 300, 6)` |
| Load YOLO26s ONNX | Success, shape verified `(N, 300, 6)` |
| Load YOLO26m ONNX | Success, shape verified `(N, 300, 6)` |
| Inference on test image | ≥5 COCO objects detected with conf >0.25 |
| Null output handling | Returns empty vector (no crash) |
| Shape mismatch handling | Returns empty vector, logs warning |
| Invalid coordinates | Skipped silently (no crash) |
| Invalid classId | Skipped silently (no crash) |
| No NMS in pipeline | Confirmed via code review |
| Latency measurement | 5-8ms improvement vs baseline |
| Config without nms_threshold | Loads without error |
| Old config with nms_threshold | Field ignored (graceful degradation) |

### Config Migration Behavior

| Scenario | Behavior |
|----------|----------|
| **New config (no nms_threshold)** | Loads normally |
| **Old config (has nms_threshold)** | Field silently ignored, no error |
| **Rationale** | Graceful degradation avoids breaking existing users |

### Benchmark Comparison

| Metric | Current (YOLOv8) | Target (YOLO26) |
|--------|------------------|-----------------|
| Total pipeline latency | ~26ms | ~18-21ms |
| Detector module lines | ~650 | ~210 |
| Config fields | 23 | 22 (-1) |

---

## Out of Scope

The following are explicitly **NOT** part of this migration:

- Dataset collection or annotation
- Model training workflows
- Custom model training UI
- Dataset management features
- Training pipeline integration

These will be addressed in future implementation plans.

---

## Migration Path for Existing Users

Users upgrading from previous versions need to:

1. **Download new config** - Old `config.ini` has removed fields
2. **Export YOLO26 models** - Old `.pt` files incompatible
3. **Update model paths** - Point to new YOLO26 ONNX/Engine files

**Migration Script (Future):** Could auto-migrate config, but manual is fine for now.

---

## Success Criteria

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

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Ultralytics | 8.3+ | YOLO26 model export |
| OpenCV | Existing | Preprocessing (unchanged) |
| TensorRT | Existing | CUDA inference (unchanged) |
| DirectML | Existing | DML inference (unchanged) |

---

## Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| YOLO26 export issues | Low | High | Test export with latest Ultralytics first |
| Output format mismatch | Medium | High | Validate ONNX output shape in Netron |
| Config migration confusion | Medium | Low | Document changes clearly |
| Performance regression | Low | High | Benchmark before/after |

---

## References

- [YOLO26 Research Analysis](../YOLO26_RESEARCH.md)
- [Model Conversion Pipeline](../model-conversion-pipeline.md)
- [Ultralytics YOLO26 Docs](https://docs.ultralytics.com/models/yolo26/)

---

## Appendix: Export Commands

```bash
# YOLO26n (nano) - For low-end PCs
yolo export model=yolo26n.pt format=onnx opset=13 simplify=true dynamic=true

# YOLO26s (small) - Recommended balance
yolo export model=yolo26s.pt format=onnx opset=13 simplify=true dynamic=true

# YOLO26m (medium) - For high-end GPUs
yolo export model=yolo26m.pt format=onnx opset=13 simplify=true dynamic=true

# TensorRT (NVIDIA GPU)
yolo export model=yolo26n.pt format=engine device=0 fp16=true
```

---

**Document Version:** 1.0  
**Author:** Sunone Aimbot 2 Development Team  
**Approved:** 2026-03-12
