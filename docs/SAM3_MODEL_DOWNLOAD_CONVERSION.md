# SAM3 Model Download & TensorRT Engine Conversion Guide

**Created:** 2026-03-13  
**Platform:** Windows (CUDA 13.1, TensorRT 10.14.1.48)  
**Status:** Ready for implementation

---

## Executive Summary

This guide provides step-by-step instructions for downloading the **SAM3 (Segment Anything Model 3)** from HuggingFace, converting it to ONNX format, and building a TensorRT engine for use in the Sunone Aimbot 2 training label feature.

### What We Have

✅ **Reference Implementation:** `SAM3TensorRT/` repo with:
- Python ONNX export script (`python/onnxexport.py`)
- C++/CUDA TensorRT inference library (`cpp/src/sam3/sam3_trt/`)
- Docker environment for Linux deployment
- Complete documentation and benchmarks

✅ **Project Infrastructure:**
- CUDA 13.1 toolset installed
- TensorRT 10.14.1.48 available
- cuDNN 9.20 configured
- OpenCV with CUDA support built

✅ **Integration Stubs:**
- `training_sam3_labeler.h` - SAM3 interface definition
- `training_sam3_labeler_trt.cpp` - CUDA backend (ready for implementation)
- `training_sam3_labeler_stub.cpp` - Non-CUDA fallback

### What We Need

1. **HuggingFace Access:** Request access to gated `facebook/sam3` model
2. **Python Environment:** Install required packages (`transformers`, `accelerate`, `onnx`)
3. **ONNX Export:** Run export script to generate `sam3_static.onnx` + weights
4. **TensorRT Engine:** Use `trtexec` to build optimized FP16 engine
5. **Model Integration:** Copy engine to `models/` directory for runtime loading

---

## Prerequisites

### Hardware Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| GPU | RTX 3090 (24GB) | RTX 4090 / H100 |
| RAM | 32GB | 64GB+ |
| Storage | 50GB free | 100GB+ SSD |
| CUDA | 12.6+ | 13.1 (installed) |

### Software Requirements

| Software | Version | Status |
|----------|---------|--------|
| CUDA Toolkit | 13.1 | ✅ Installed |
| TensorRT | 10.14.1.48 | ✅ Installed |
| cuDNN | 9.20 | ✅ Installed |
| Python | 3.10+ | ⚠️ Verify |
| pip | 24.0+ | ⚠️ Verify |

### HuggingFace Access

**CRITICAL:** SAM3 is a gated model requiring approval.

1. Visit: https://huggingface.co/facebook/sam3
2. Click "Request access" button
3. Wait for approval (typically 24-48 hours)
4. Generate access token: https://huggingface.co/settings/tokens
   - Select "Read" permissions
   - Copy token (e.g., `hf_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`)

---

## Step 1: Install Python Dependencies

### Create Virtual Environment (Recommended)

```powershell
# Navigate to project root
cd I:\CppProjects\sunone_aimbot_2

# Create virtual environment
python -m venv .venv_sam3

# Activate virtual environment
.\.venv_sam3\Scripts\Activate.ps1
```

### Install Required Packages

```powershell
# Upgrade pip
python -m pip install --upgrade pip

# Install core dependencies
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu126
pip install transformers==5.0.0rc1 accelerate onnx onnxruntime-gpu
pip install opencv-python pillow requests
```

### Verify Installation

```powershell
python -c "import torch; print(f'PyTorch: {torch.__version__}')"
python -c "import transformers; print(f'Transformers: {transformers.__version__}')"
python -c "import onnx; print(f'ONNX: {onnx.__version__}')"
python -c "import torch; print(f'CUDA available: {torch.cuda.is_available()}')"
```

---

## Step 2: Download & Export SAM3 to ONNX

### Set HuggingFace Token

```powershell
# Option 1: Environment variable (recommended)
$env:HF_TOKEN = "hf_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# Option 2: Login via CLI
huggingface-cli login
# Paste your token when prompted
```

### Run ONNX Export Script

```powershell
# Navigate to SAM3TensorRT python directory
cd I:\CppProjects\sunone_aimbot_2\SAM3TensorRT\python

# Run export script
python onnxexport.py
```

### Expected Output

```
input_ids torch.Size([1, 32]) torch.int64
tensor([[    49406,      1000,     53200,      1415,       271,     49407,
          49407,     49407,     49407,     49407,     49407,     49407,
          49407,     49407,     49407,     49407,     49407,     49407,
          49407,     49407,     49407,     49407,     49407,     49407,
          49407,     49407,     49407,     49407,     49407,     49407,
          49407,     49407]])

attention_mask torch.Size([1, 32]) torch.int64
tensor([[1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0]])

Exported to onnx_weights/sam3_dynamic.onnx
```

### Verify Output Files

```powershell
# Check export directory
ls onnx_weights/

# Expected files:
# - sam3_dynamic.onnx (~10-20 MB)
# - sam3_dynamic.onnx_data (~3-4 GB external weights)
```

### Troubleshooting

**Error: "You are trying to access a gated repo"**
- Solution: Ensure `HF_TOKEN` is set or run `huggingface-cli login`

**Error: "CUDA out of memory"**
- Solution: Export on CPU (default in script), or close other GPU applications

**Error: "ModuleNotFoundError: No module named 'transformers.models.sam3'"**
- Solution: Upgrade transformers: `pip install --upgrade transformers==5.0.0rc1`

---

## Step 3: Build TensorRT Engine

### Locate trtexec Tool

```powershell
# TensorRT 10.14.1.48 installation path
$trtexec = "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\v10.14.1.48\bin\trtexec.exe"

# Verify installation
& $trtexec --version
```

### Build FP16 Engine

```powershell
# Navigate to ONNX export directory
cd I:\CppProjects\sunone_aimbot_2\SAM3TensorRT\python\onnx_weights

# Build TensorRT engine with FP16 precision
& $trtexec `
    --onnx=sam3_dynamic.onnx `
    --saveEngine=..\..\models\sam3_fp16.engine `
    --fp16 `
    --verbose `
    --workspace=4096 `
    --memPoolSize=workspace:4096
```

### Alternative: Static Shape Engine (Better Performance)

For fixed 1008x1008 input size (recommended for auto-labeling):

```powershell
& $trtexec `
    --onnx=sam3_dynamic.onnx `
    --saveEngine=..\..\models\sam3_fp16_static.engine `
    --fp16 `
    --minShapes=pixel_values:1x3x1008x1008,input_ids:1x32,attention_mask:1x32 `
    --optShapes=pixel_values:1x3x1008x1008,input_ids:1x32,attention_mask:1x32 `
    --maxShapes=pixel_values:1x3x1008x1008,input_ids:1x32,attention_mask:1x32 `
    --verbose `
    --workspace=4096
```

### Expected Build Output

```
[INFO] Loading ONNX model: sam3_dynamic.onnx
[INFO] Parsing ONNX model...
[INFO] Building engine with FP16 precision...
[INFO] Optimizing layers...
[INFO] Engine built successfully
[INFO] Engine size: 1.8 GB
[INFO] Saved to: ../../models/sam3_fp16.engine
```

### Verify Engine File

```powershell
# Check engine file
ls ..\..\models\sam3_fp16.engine

# Expected size: 1.5-2.0 GB
```

### Troubleshooting

**Error: "Could not find ONNX model"**
- Solution: Ensure `sam3_dynamic.onnx_data` is in the same directory as `.onnx` file

**Error: "CUDA out of memory during engine build"**
- Solution: Increase workspace: `--workspace=8192` or close other GPU apps

**Error: "TensorRT version mismatch"**
- Solution: Verify ONNX opset compatibility (opset 17 recommended)

---

## Step 4: Integrate with Sunone Aimbot 2

### Copy Engine to Models Directory

```powershell
# Ensure models directory exists
cd I:\CppProjects\sunone_aimbot_2\models

# Verify engine is in place
ls sam3_fp16.engine
```

### Update Training Config

The engine path should be configured in the UI under **Settings → Training → Label**:

- **SAM3 Engine Path:** `I:\CppProjects\sunone_aimbot_2\models\sam3_fp16.engine`
- **SAM3 Prompt:** Default text prompt (e.g., "person", "enemy", "player")
- **Confidence Threshold:** 0.5 (adjust based on accuracy)

### Test Inference

Once the engine is built, test with the SAM3TensorRT demo app:

```powershell
# Build SAM3TensorRT C++ library
cd I:\CppProjects\sunone_aimbot_2\SAM3TensorRT\cpp
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_CUDA_ARCHITECTURES=86  # Adjust for your GPU

# Build
cmake --build . --config Release

# Run demo
.\Release\sam3_pcs_app.exe C:\path\to\test\images ..\..\models\sam3_fp16.engine
```

---

## PowerShell Automation Script

Save this as `scripts\sam3_download_and_convert.ps1`:

```powershell
<#
.SYNOPSIS
    Download SAM3 model and convert to TensorRT engine
.DESCRIPTION
    Automates the complete SAM3 model download, ONNX export, and TensorRT engine build process
.PARAMETER HuggingFaceToken
    HuggingFace access token (or set HF_TOKEN environment variable)
.PARAMETER OutputDir
    Output directory for engine file (default: models/)
.EXAMPLE
    .\sam3_download_and_convert.ps1 -HuggingFaceToken "hf_xxxxx"
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$HuggingFaceToken = $env:HF_TOKEN,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "models",
    
    [Parameter(Mandatory=$false)]
    [string]$Precision = "fp16"
)

# Error handling
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "SAM3 Model Download & Conversion Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Check prerequisites
Write-Host "[1/5] Checking prerequisites..." -ForegroundColor Yellow

# Check Python
try {
    $pythonVersion = python --version 2>&1
    Write-Host "  ✓ Python: $pythonVersion" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Python not found! Install Python 3.10+" -ForegroundColor Red
    exit 1
}

# Check CUDA
$cudaPath = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1"
if (Test-Path $cudaPath) {
    Write-Host "  ✓ CUDA: v13.1 found" -ForegroundColor Green
} else {
    Write-Host "  ⚠ CUDA v13.1 not found at expected path" -ForegroundColor Yellow
}

# Check TensorRT
$trtexec = "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\v10.14.1.48\bin\trtexec.exe"
if (Test-Path $trtexec) {
    Write-Host "  ✓ TensorRT: v10.14.1.48 found" -ForegroundColor Green
} else {
    Write-Host "  ✗ TensorRT not found!" -ForegroundColor Red
    exit 1
}

# Check HF token
if ([string]::IsNullOrEmpty($HuggingFaceToken)) {
    Write-Host "  ✗ HuggingFace token not provided!" -ForegroundColor Red
    Write-Host "    Set HF_TOKEN env var or use -HuggingFaceToken parameter" -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "  ✓ HuggingFace token provided" -ForegroundColor Green
}

# Step 2: Setup environment
Write-Host ""
Write-Host "[2/5] Setting up Python environment..." -ForegroundColor Yellow

$env:HF_TOKEN = $HuggingFaceToken

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
    Write-Host "  ✓ Created output directory: $OutputDir" -ForegroundColor Green
}

# Step 3: Export to ONNX
Write-Host ""
Write-Host "[3/5] Exporting SAM3 to ONNX..." -ForegroundColor Yellow

$onnxExportDir = "SAM3TensorRT\python"
$onnxOutputDir = "$onnxExportDir\onnx_weights"

if (!(Test-Path $onnxExportDir)) {
    Write-Host "  ✗ SAM3TensorRT directory not found!" -ForegroundColor Red
    exit 1
}

Set-Location $onnxExportDir

# Check if already exported
if ((Test-Path "onnx_weights\sam3_dynamic.onnx") -and (Test-Path "onnx_weights\sam3_dynamic.onnx_data")) {
    Write-Host "  ℹ ONNX files already exist, skipping export" -ForegroundColor Cyan
} else {
    Write-Host "  Running ONNX export script..." -ForegroundColor Gray
    python onnxexport.py
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ✗ ONNX export failed!" -ForegroundColor Red
        exit 1
    }
}

Set-Location "..\.."

# Step 4: Build TensorRT engine
Write-Host ""
Write-Host "[4/5] Building TensorRT engine..." -ForegroundColor Yellow

$enginePath = "$OutputDir\sam3_$Precision.engine"

if (Test-Path $enginePath) {
    Write-Host "  ℹ Engine already exists: $enginePath" -ForegroundColor Cyan
    Write-Host "  Delete to rebuild or use different filename" -ForegroundColor Gray
} else {
    Write-Host "  Running trtexec..." -ForegroundColor Gray
    
    $trtexecArgs = @(
        "--onnx=$onnxOutputDir\sam3_dynamic.onnx",
        "--saveEngine=$enginePath",
        "--$Precision",
        "--verbose",
        "--workspace=4096"
    )
    
    & $trtexec $trtexecArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ✗ TensorRT engine build failed!" -ForegroundColor Red
        exit 1
    }
}

# Step 5: Verify and summarize
Write-Host ""
Write-Host "[5/5] Verifying output..." -ForegroundColor Yellow

if (Test-Path $enginePath) {
    $engineSize = (Get-Item $enginePath).Length / 1GB
    Write-Host "  ✓ Engine created: $enginePath" -ForegroundColor Green
    Write-Host "    Size: {0:N2} GB" -f $engineSize -ForegroundColor Gray
} else {
    Write-Host "  ✗ Engine file not found!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "SUCCESS! SAM3 engine ready for use" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Open Sunone Aimbot 2 overlay" -ForegroundColor White
Write-Host "2. Go to Settings → Training → Label" -ForegroundColor White
Write-Host "3. Set SAM3 Engine Path to: $((Resolve-Path $enginePath).Path)" -ForegroundColor White
Write-Host "4. Configure your prompt and hotkey" -ForegroundColor White
Write-Host ""
```

---

## Usage Examples

### Quick Start (Interactive)

```powershell
# Run script with token
cd I:\CppProjects\sunone_aimbot_2\scripts
.\sam3_download_and_convert.ps1 -HuggingFaceToken "hf_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

### Using Environment Variable

```powershell
# Set token once
$env:HF_TOKEN = "hf_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# Run script without parameter
.\sam3_download_and_convert.ps1
```

### Rebuild with Different Precision

```powershell
# FP32 (maximum accuracy, slower)
.\sam3_download_and_convert.ps1 -Precision fp32 -OutputDir "models\fp32"

# FP16 (recommended balance)
.\sam3_download_and_convert.ps1 -Precision fp16 -OutputDir "models\fp16"
```

---

## Performance Benchmarks

### Expected Inference Times (per image)

| GPU | FP16 Latency | Recommended Batch |
|-----|--------------|-------------------|
| RTX 3090 | 75ms | 4 |
| RTX 4090 | 45ms | 4-8 |
| RTX 4080 | 55ms | 4 |
| A100 | 49ms | 8 |
| H100 | 25-35ms | 8-16 |

### Memory Usage

| Component | FP16 Size |
|-----------|-----------|
| Model Weights | ~1.7 GB |
| Activation Memory | ~1.0 GB |
| Workspace | ~4.0 GB |
| **Total** | **~6.7 GB** |

---

## Integration with Training Label Feature

### Config Fields (from `config.h`)

```cpp
// Training → Label settings
std::string sam3EnginePath;     // Path to .engine file
std::string defaultPrompt;      // Default SAM3 text prompt
float sam3ConfidenceThreshold;  // Detection confidence (0.0-1.0)
int sam3MaxDetections;          // Maximum boxes per frame
```

### Runtime Loading (from `training_sam3_labeler_trt.cpp`)

```cpp
// TODO: Implement in training_sam3_labeler_trt.cpp
Sam3Labeler::Sam3Labeler() : impl_(new Impl()) {
    // Load engine from config
    std::string enginePath = config.sam3EnginePath;
    
    // Initialize TensorRT runtime
    auto runtime = std::unique_ptr<nvinfer1::IRuntime>(
        nvinfer1::createInferRuntime(logger));
    
    // Load engine file
    std::ifstream file(enginePath, std::ios::binary);
    // ... deserialize engine ...
    
    // Create execution context
    impl_->context = std::unique_ptr<nvinfer1::IExecutionContext>(
        engine->createExecutionContext());
    
    impl_->initialized = true;
}
```

### Hotkey Workflow

1. User presses hotkey (e.g., `F12`)
2. Capture loop grabs current frame
3. Frame sent to SAM3 labeler with prompt
4. SAM3 returns bounding boxes
5. Boxes + class saved to YOLO dataset
6. User continues gaming/watching

---

## Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| "Gated repo access denied" | Missing HF token approval | Request access at huggingface.co/facebook/sam3 |
| "CUDA out of memory" | Insufficient VRAM | Close other GPU apps, reduce batch size |
| "ONNX parser error" | Missing external weights | Ensure `.onnx_data` file is present |
| "Engine build timeout" | Large model, slow GPU | Increase timeout, use FP16 |
| "No detections" | Wrong prompt format | Use simple nouns: "person", not "a person" |

### Debugging Tips

```powershell
# Enable verbose TensorRT logging
$env:TENSORRT_VERBOSITY = "verbose"

# Check GPU memory usage
nvidia-smi dmon -s muvpc

# Test ONNX model with Netron
winget install netron
netron models/sam3_fp16.engine
```

---

## References

### Primary Sources

- [SAM3 GitHub](https://github.com/facebookresearch/sam3) - Official implementation
- [SAM3 HuggingFace](https://huggingface.co/facebook/sam3) - Model download
- [SAM3 Paper](https://arxiv.org/abs/2511.16719) - Architecture details
- [TensorRT 10.x Docs](https://docs.nvidia.com/deeplearning/tensorrt/) - API reference

### Project Files

- `SAM3TensorRT/README.md` - Quickstart guide
- `SAM3TensorRT/RESEARCH.md` - Comprehensive integration research
- `SAM3TensorRT/python/onnxexport.py` - ONNX export script
- `SAM3TensorRT/cpp/src/sam3/sam3_trt/sam3.cu` - C++ inference implementation

---

**Last Updated:** 2026-03-13  
**Valid Until:** Next SAM3 or TensorRT major release  
**Contact:** Check `SAM3TensorRT/RESEARCH.md` for detailed integration patterns
