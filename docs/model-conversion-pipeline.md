# Model Conversion Pipeline

This document explains how the aimbot converts PyTorch models into runtime formats.

---

## Overview

The conversion flows through three stages:

```
PyTorch (.pt) → ONNX (.onnx) → TensorRT Engine (.engine)
```

Each stage serves a distinct purpose. PyTorch stores the trained weights. ONNX provides a portable intermediate format. TensorRT Engine delivers optimized GPU inference.

---

## Stage 1: PyTorch to ONNX

**Tool:** Ultralytics YOLO CLI (external)

**Command:**

```bash
yolo export model=model.pt format=onnx dynamic=true simplify=true
```

**What happens:**

- The exporter traces the model computation graph
- Dynamic axes enable variable batch sizes and input resolutions
- ONNX simplifier removes redundant operations
- Output: `model.onnx`

**Where this runs:**

Run this command on your development machine before deploying. The aimbot loads ONNX files directly—it does not convert PyTorch files at runtime.

**Configuration options:**

| Flag | Purpose |
|------|---------|
| `dynamic=true` | Allow variable input sizes (required for detection_resolution changes) |
| `simplify=true` | Remove redundant ops for cleaner graphs |
| `opset=13` | ONNX operator set version (13+ recommended) |

---

## Stage 2: ONNX to TensorRT Engine

**Location:** `tensorrt/nvinf.cpp`, function `buildEngineFromOnnx()`

**When this happens:**

The conversion occurs automatically when you point `config.ini` at an ONNX file and no matching `.engine` file exists.

**Process flow:**

1. **Parse ONNX**
   - `nvonnxparser::IParser` reads the model graph
   - Validates operators against TensorRT supported ops

2. **Configure Profile**
   - Static input: locks to model's trained resolution
   - Dynamic input: sets min/optim/max ranges (160/320/640)

3. **Set Precision**
   - FP32 (default): maximum compatibility
   - FP16: 2x speedup on supported GPUs
   - FP8: experimental, requires Ada/Hopper GPUs

4. **Build Engine**
   - `builder->buildSerializedNetwork()` runs layer fusion and kernel selection
   - Progress displays in overlay during build

5. **Serialize**
   - Writes optimized engine to disk alongside the ONNX file
   - Updates `config.ini` to use the engine for future runs

**Configuration options (config.ini):**

```ini
[Export]
export_enable_fp16=true      # Enable half-precision
export_enable_fp8=false      # Enable 8-bit (experimental)
fixed_input_size=false       # Static vs dynamic optimization
```

---

## Stage 3: Runtime Loading

**Location:** `detector/trt_detector.cpp`, function `loadEngine()`

**Logic:**

```cpp
if (extension == ".engine") {
    load directly
} else if (extension == ".onnx") {
    if (engine exists) load it
    else build from ONNX, save, then load
}
```

**First-run behavior:**

When you set `ai_model=model.onnx` and no `model.engine` exists:

1. The detector calls `buildEngineFromOnnx()`
2. TensorRT compiles the optimized engine (may take 2-10 minutes)
3. The engine saves to `models/model.engine`
4. `config.ini` updates to `ai_model=model.engine`
5. Subsequent runs load instantly from the cached engine

---

## File Locations

| Stage | Input | Output | Location |
|-------|-------|--------|----------|
| PT → ONNX | `model.pt` | `model.onnx` | External (your machine) |
| ONNX → Engine | `model.onnx` | `model.engine` | `models/` folder |
| Runtime | `model.engine` | — | GPU memory |

---

## Troubleshooting

**Engine build fails:**

- Verify ONNX file loads in Netron (visualizer)
- Check TensorRT supports all operators in the graph
- Try `simplify=true` during ONNX export

**Out of memory during build:**

- Reduce `detection_resolution` in config
- Disable FP16 and use FP32
- Close other GPU applications

**Engine incompatible after GPU/driver change:**

- Delete the `.engine` file
- The app rebuilds automatically on next run

---

## Key Source Files

| File | Purpose |
|------|---------|
| `tensorrt/nvinf.cpp` | Engine builder (`buildEngineFromOnnx()`) |
| `tensorrt/nvinf.h` | TensorRT interface declarations |
| `detector/trt_detector.cpp` | Runtime loader with auto-conversion |
| `detector/trt_detector.h` | Detector class definition |

---

## Summary

1. Export PyTorch to ONNX using Ultralytics CLI before deployment
2. Place ONNX files in the `models/` folder
3. The app converts ONNX to TensorRT Engine on first run
4. Subsequent runs load the optimized engine instantly
5. Delete `.engine` files to force rebuilds after model or system changes
