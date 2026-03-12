# Build-Dml.ps1
# Builds sunone_aimbot_2 with DirectML backend

param(
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Parallel
)

$ErrorActionPreference = "Stop"
$BUILD_DIR = "build/dml"

function Clean-Build {
    Write-Host "Cleaning DML build directory..." -ForegroundColor Yellow
    if (Test-Path $BUILD_DIR) {
        Remove-Item $BUILD_DIR -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Setup-OpenCV {
    Write-Host "Setting up OpenCV for DML..." -ForegroundColor Green
    if (Test-Path "tools/setup_opencv_dml.ps1") {
        & "tools/setup_opencv_dml.ps1"
    }
}

function Configure-Build {
    Write-Host "Configuring DML build with DirectML..." -ForegroundColor Green
    cmake -S . -B $BUILD_DIR `
        -G "Visual Studio 18 2026" `
        -A x64 `
        -DAIMBOT_USE_CUDA=OFF
}

function Build-Project {
    Write-Host "Building DML project..." -ForegroundColor Green
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
    Setup-OpenCV
    Configure-Build
    Build-Project
} elseif (Test-Path "$BUILD_DIR/CMakeCache.txt") {
    Build-Project
} else {
    Setup-OpenCV
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