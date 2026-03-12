import { tool } from "@opencode-ai/plugin"
import { TOOL_REGISTRY } from "../tool-registry"

export const getBuildCommandTool = tool({
  description: TOOL_REGISTRY.getBuildCommand.description,
  args: {
    backend: tool.schema.string().optional().default("auto").describe(
      "Build backend to target: 'auto', 'cuda', or 'dml'. " +
      "For this project, 'cuda' uses TensorRT with explicit CUDA toolset, 'dml' uses DirectML. " +
      "Default 'auto' detects based on build directory."
    )
  },
  async execute(args, context) {
    const { $, directory } = context
    const { backend = "auto" } = args
    
    try {
      // Check for CMake (this project's build system)
      const hasCMake = await $`test -f CMakeLists.txt`.then(() => true).catch(() => false)
      if (!hasCMake) {
        return {
          buildCommand: null,
          message: "No CMakeLists.txt found - this project requires CMake",
          directory,
          type: "unknown"
        }
      }

      // This project specifics: sunone_aimbot_2 with CUDA/DML backends
      const hasCudaBuild = await $`test -d build/cuda`.then(() => true).catch(() => false)
      const hasDmlBuild = await $`test -d build/dml`.then(() => true).catch(() => false)
      const hasCudaCache = hasCudaBuild && await $`test -f build/cuda/CMakeCache.txt`.then(() => true).catch(() => false)
      const hasDmlCache = hasDmlBuild && await $`test -f build/dml/CMakeCache.txt`.then(() => true).catch(() => false)

      // Determine which backend to use
      let targetBackend = backend
      if (backend === "auto") {
        targetBackend = hasCudaCache ? "cuda" : hasDmlCache ? "dml" : "cuda"
      }

      // CUDA toolset path (verified working on this machine)
      const cudaPath = "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1"

      if (targetBackend === "cuda") {
        if (hasCudaCache) {
          return {
            buildCommand: "cmake --build build/cuda --config Release",
            buildParallel: "cmake --build build/cuda --config Release --parallel",
            cleanCommand: "cmake --build build/cuda --target clean",
            rebuildCommand: "Remove-Item build/cuda -Recurse -Force -ErrorAction SilentlyContinue; cmake -S . -B build/cuda -G \"Visual Studio 18 2026\" -A x64 -T \"cuda=" + cudaPath + "\" -DAIMBOT_USE_CUDA=ON -DCMAKE_CUDA_FLAGS=\"--allow-unsupported-compiler\" -DCUDA_NVCC_FLAGS=\"--allow-unsupported-compiler\" && cmake --build build/cuda --config Release",
            type: "cmake-cuda",
            configured: true,
            directory,
            backend: "cuda",
            output: "build/cuda/Release/ai.exe",
            dlls: [
              "build/cuda/Release/opencv_world4130.dll",
              "build/cuda/Release/nvinfer_10.dll",
              "build/cuda/Release/cudnn64_9.dll",
              "build/cuda/Release/onnxruntime.dll"
            ],
            suggestions: [
              "Build configured for CUDA/TensorRT",
              "Executable: build/cuda/Release/ai.exe",
              "Requires CUDA 13.1, cuDNN 9.20, TensorRT 10.14.1.48",
              "If you get 'No CUDA toolset' errors, ensure -T cuda=... is used"
            ]
          }
        } else {
          return {
            buildCommand: "cmake -S . -B build/cuda -G \"Visual Studio 18 2026\" -A x64 -T \"cuda=" + cudaPath + "\" -DAIMBOT_USE_CUDA=ON -DCMAKE_CUDA_FLAGS=\"--allow-unsupported-compiler\" -DCUDA_NVCC_FLAGS=\"--allow-unsupported-compiler\" && cmake --build build/cuda --config Release",
            buildParallel: null,
            cleanCommand: "Remove-Item build/cuda -Recurse -Force -ErrorAction SilentlyContinue",
            rebuildCommand: "Remove-Item build/cuda -Recurse -Force -ErrorAction SilentlyContinue; cmake -S . -B build/cuda -G \"Visual Studio 18 2026\" -A x64 -T \"cuda=" + cudaPath + "\" -DAIMBOT_USE_CUDA=ON -DCMAKE_CUDA_FLAGS=\"--allow-unsupported-compiler\" -DCUDA_NVCC_FLAGS=\"--allow-unsupported-compiler\" && cmake --build build/cuda --config Release",
            type: "cmake-cuda",
            configured: false,
            directory,
            backend: "cuda",
            output: "build/cuda/Release/ai.exe",
            suggestions: [
              "First-time CUDA configure and build",
              "This will configure with explicit CUDA 13.1 toolset",
              "Requires Visual Studio 2026 with C++ tools",
              "Ensure CUDA 13.1, cuDNN 9.20, TensorRT 10.14.1.48 are installed",
              "After this, subsequent builds only need: cmake --build build/cuda --config Release"
            ]
          }
        }
      }

      if (targetBackend === "dml") {
        if (hasDmlCache) {
          return {
            buildCommand: "cmake --build build/dml --config Release",
            buildParallel: "cmake --build build/dml --config Release --parallel",
            cleanCommand: "cmake --build build/dml --target clean",
            rebuildCommand: "Remove-Item build/dml -Recurse -Force -ErrorAction SilentlyContinue; cmake -S . -B build/dml -G \"Visual Studio 18 2026\" -A x64 -DAIMBOT_USE_CUDA=OFF && cmake --build build/dml --config Release",
            type: "cmake-dml",
            configured: true,
            directory,
            backend: "dml",
            output: "build/dml/Release/ai.exe",
            suggestions: [
              "Build configured for DirectML",
              "Executable: build/dml/Release/ai.exe",
              "Works on all GPUs (NVIDIA/AMD/Intel)",
              "Requires NuGet packages restored"
            ]
          }
        } else {
          return {
            buildCommand: "powershell -ExecutionPolicy Bypass -File tools/setup_opencv_dml.ps1; cmake -S . -B build/dml -G \"Visual Studio 18 2026\" -A x64 -DAIMBOT_USE_CUDA=OFF && cmake --build build/dml --config Release",
            buildParallel: null,
            cleanCommand: "Remove-Item build/dml -Recurse -Force -ErrorAction SilentlyContinue",
            rebuildCommand: "Remove-Item build/dml -Recurse -Force -ErrorAction SilentlyContinue; powershell -ExecutionPolicy Bypass -File tools/setup_opencv_dml.ps1; cmake -S . -B build/dml -G \"Visual Studio 18 2026\" -A x64 -DAIMBOT_USE_CUDA=OFF && cmake --build build/dml --config Release",
            type: "cmake-dml",
            configured: false,
            directory,
            backend: "dml",
            output: "build/dml/Release/ai.exe",
            suggestions: [
              "First-time DML configure and build",
              "Downloads prebuilt OpenCV (no CUDA needed)",
              "Works on all GPUs",
              "Requires NuGet packages (auto-restored by VS or CMake)"
            ]
          }
        }
      }

      return {
        buildCommand: null,
        message: "No matching build configuration found",
        backend,
        hasCudaBuild,
        hasDmlBuild,
        directory
      }
    } catch (error) {
      return {
        error: `Failed to detect build system: ${error}`,
        directory
      }
    }
  }
})

export default getBuildCommandTool
