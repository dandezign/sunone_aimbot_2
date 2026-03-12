# CUDA Build and Run Checklist

This note documents the CUDA build path that worked in this repository and what to verify before running `ai.exe`.

## 1. Working Configure and Build Commands

From the repository root:

```powershell
Remove-Item build/cuda -Recurse -Force -ErrorAction SilentlyContinue

cmake -S . -B build/cuda -G "Visual Studio 18 2026" -A x64 `
  -T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1" `
  -DAIMBOT_USE_CUDA=ON `
  -DCMAKE_CUDA_FLAGS="--allow-unsupported-compiler" `
  -DCUDA_NVCC_FLAGS="--allow-unsupported-compiler"

cmake --build build/cuda --config Release
```

## 2. Why `-T "cuda=..."` Is Needed

On this machine, plain Visual Studio generation failed with:

```text
No CUDA toolset found.
```

The fix was to pass the CUDA toolset path explicitly:

```powershell
-T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1"
```

Use that form if Visual Studio does not have the CUDA build customization installed into its default MSBuild `BuildCustomizations` folder.

## 3. Dependencies Verified for the CUDA Build

These paths were present and valid during the successful build:

- `sunone_aimbot_2/modules/opencv/build/install/include/opencv2/opencv.hpp`
- `sunone_aimbot_2/modules/opencv/build/install/x64/vc18/lib/opencv_world4130.lib`
- `sunone_aimbot_2/modules/opencv/build/install/x64/vc18/bin/opencv_world4130.dll`
- `sunone_aimbot_2/modules/TensorRT-10.14.1.48/include/NvInfer.h`
- `sunone_aimbot_2/modules/TensorRT-10.14.1.48/lib/nvinfer_10.lib`
- `packages/Microsoft.ML.OnnxRuntime.DirectML.1.24.3/build/native/include/onnxruntime_cxx_api.h`
- `packages/Microsoft.Windows.CppWinRT.2.0.250303.1/build/native/Microsoft.Windows.CppWinRT.props`
- `C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1/bin/nvcc.exe`
- `C:/Program Files/NVIDIA/CUDNN/v9.20/lib/13.2/x64/cudnn.lib`
- `C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/cppwinrt/winrt/base.h`

## 4. Successful Configure Output

The configure step should report lines similar to these:

```text
-- The CUDA compiler identification is NVIDIA 13.1.80
-- Found CUDAToolkit: C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1/include
-- Aimbot backend: ON
-- OpenCV include: .../sunone_aimbot_2/modules/opencv/build/install/include
-- OpenCV lib: .../sunone_aimbot_2/modules/opencv/build/install/x64/vc18/lib/opencv_world4130.lib
-- ONNX Runtime package: .../packages/Microsoft.ML.OnnxRuntime.DirectML.1.24.3
-- C++/WinRT include: C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/cppwinrt
```

## 5. Output Files to Check After Build

The build should produce:

- `build/cuda/Release/ai.exe`

These runtime DLLs should also be next to the executable:

- `build/cuda/Release/opencv_world4130.dll`
- `build/cuda/Release/nvinfer_10.dll`
- `build/cuda/Release/nvonnxparser_10.dll`
- `build/cuda/Release/cudnn64_9.dll`
- `build/cuda/Release/onnxruntime.dll`
- `build/cuda/Release/onnxruntime_providers_shared.dll`
- `build/cuda/Release/DirectML.dll`

If `AIMBOT_COPY_RUNTIME_DLLS` remains enabled, CMake copies these automatically after build.

## 6. First Run Behavior

Run:

```powershell
.\build\cuda\Release\ai.exe
```

At startup the app creates these folders if they do not exist:

- `models`
- `models/depth`
- `screenshots`

The app loads the model from:

```text
models/<ai_model>
```

If `config.ini` points at a missing model, the app tries to load the first available model from `models/`. If no model exists there, startup stops.

## 7. Models for the CUDA Backend

For CUDA builds, `backend = TRT` is the intended fast path.

- If `ai_model` points to an `.engine`, the app loads it directly.
- If `ai_model` points to an `.onnx`, the TensorRT path can build an `.engine` automatically on first run and save it next to the ONNX file.
- When that happens, `config.ini` is updated to the new `.engine` filename.

Examples:

- `models/sunxds_0.8.0.engine`
- `models/sunxds_0.8.0.onnx`
- `models/depth/depth_anything_v2.engine`

## 8. Useful Runtime Controls

- `Home` - open or close the overlay
- `RightMouseButton` - target by default
- `F2` - exit
- `F3` - pause aiming
- `F4` - reload `config.ini`

## 9. Recommended CUDA Test Config

If you want a low-risk first test, start with settings close to these in `config.ini`:

```ini
capture_method = duplication_api
backend = TRT
capture_use_cuda = true
detection_resolution = 320
capture_fps = 60
circle_mask = false
depth_inference_enabled = false
depth_mask_enabled = false
```

## 10. Common Failure Points

If configure fails:

- `No CUDA toolset found.` -> add `-T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1"`
- `TensorRT headers not found` -> verify `sunone_aimbot_2/modules/TensorRT-10.14.1.48`
- `onnxruntime_cxx_api.h not found` -> restore NuGet packages into `packages/`
- `C++/WinRT headers not found` -> install Windows SDK `10.0.26100.0+`

If runtime fails:

- missing `opencv_world4130.dll` -> rebuild or recopy runtime DLLs
- missing `nvinfer_10.dll` or `nvonnxparser_10.dll` -> verify TensorRT `bin/`
- missing `cudnn64_9.dll` -> verify cuDNN `v9.20`
- no models found -> place an `.engine` or `.onnx` file in `models/`
