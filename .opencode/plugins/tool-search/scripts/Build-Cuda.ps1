# Build-Cuda.ps1
# Builds sunone_aimbot_2 with CUDA/TensorRT backend

param(
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Parallel
)

$ErrorActionPreference = "Stop"
$CUDA_PATH = "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1"
$BUILD_DIR = "build/cuda"

function Clean-Build {
    Write-Host "Cleaning CUDA build directory..." -ForegroundColor Yellow
    if (Test-Path $BUILD_DIR) {
        Remove-Item $BUILD_DIR -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Configure-Build {
    Write-Host "Configuring CUDA build with TensorRT..." -ForegroundColor Green
    cmake -S . -B $BUILD_DIR `
        -G "Visual Studio 18 2026" `
        -A x64 `
        -T "cuda=$CUDA_PATH" `
        -DAIMBOT_USE_CUDA=ON `
        -DCMAKE_CUDA_FLAGS="--allow-unsupported-compiler" `
        -DCUDA_NVCC_FLAGS="--allow-unsupported-compiler"
}

function Build-Project {
    Write-Host "Building CUDA project..." -ForegroundColor Green
    if ($Parallel) {
        cmake --build $BUILD_DIR --config Release --parallel
    } else {
        cmake --build $BUILD_DIR --config Release
    }
}

if ($Clean) {
    Clean-Build
    exit 0
}

if ($Rebuild) {
    Clean-Build
    Configure-Build
    Build-Project
} elseif (Test-Path "$BUILD_DIR/CMakeCache.txt") {
    Build-Project
} else {
    Configure-Build
    Build-Project
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "Output: $BUILD_DIR/Release/ai.exe"
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}