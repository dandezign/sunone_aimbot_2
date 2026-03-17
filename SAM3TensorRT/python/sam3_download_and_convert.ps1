<#
.SYNOPSIS
    Download SAM3 model and convert to TensorRT engine for Sunone Aimbot 2
.DESCRIPTION
    Automates the complete SAM3 model download, ONNX export, and TensorRT engine build process.
    Uses the project's virtual environment (.venv) for Python dependencies.
    
    Two download methods:
    - "hybrid": Download sam3.pt + convert to TensorRT (recommended)
    - "direct": Export to ONNX + convert to TensorRT (faster)
.PARAMETER HuggingFaceToken
    HuggingFace access token (or set HF_TOKEN env var)
.PARAMETER OutputDir
    Output directory (default: models/)
.PARAMETER Precision
    TensorRT precision: fp16, fp32, int8 (default: fp16)
.PARAMETER DownloadMethod
    Method: "hybrid" or "direct" (default: hybrid)
.PARAMETER SkipPythonDeps
    Skip pip install if set
.PARAMETER Force
    Force rebuild
.EXAMPLE
    .\sam3_download_and_convert.ps1 -HuggingFaceToken "hf_xxx"
.EXAMPLE
    $env:HF_TOKEN="hf_xxx"; .\sam3_download_and_convert.ps1
.NOTES
    Requires: Python 3.10+, CUDA 13.1, TensorRT 10.14.1.48
    Virtual Env: Auto-activates .venv
#>

[CmdletBinding()]
param(
    [string]$HuggingFaceToken = $env:HF_TOKEN,
    [string]$OutputDir = "models",
    [ValidateSet("fp16","fp32","int8")]
    [string]$Precision = "fp16",
    [ValidateSet("hybrid","direct")]
    [string]$DownloadMethod = "hybrid",
    [switch]$SkipPythonDeps,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

function Get-DotEnvValue {
    param(
        [string]$FilePath,
        [string]$Key
    )

    if (!(Test-Path $FilePath)) {
        return $null
    }

    foreach ($line in [System.IO.File]::ReadAllLines($FilePath)) {
        if ($line -match '^\s*#' -or [string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        if ($line -match "^\s*$([regex]::Escape($Key))\s*=\s*(.*)\s*$") {
            return $Matches[1].Trim('"').Trim("'")
        }
    }

    return $null
}

function Find-TrtExec {
    param([string]$Root)

    $candidates = @(
        (Join-Path $Root "sunone_aimbot_2\modules\TensorRT-10.14.1.48\bin\trtexec.exe"),
        (Join-Path $Root "sunone_aimbot_2\tensorrt\bin\trtexec.exe"),
        "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\v10.14.1.48\bin\trtexec.exe",
        "C:\Program Files\TensorRT\bin\trtexec.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $searchRoots = @(
        (Join-Path $Root "sunone_aimbot_2\modules"),
        (Join-Path $Root "sunone_aimbot_2\tensorrt"),
        "C:\Program Files",
        "C:\Program Files\NVIDIA GPU Computing Toolkit"
    )

    foreach ($searchRoot in $searchRoots) {
        if (Test-Path $searchRoot) {
            $match = Get-ChildItem -Path $searchRoot -Filter "trtexec.exe" -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($match) {
                return $match.FullName
            }
        }
    }

    return $null
}

function Repack-OnnxExternalData {
    param(
        [string]$PythonExe,
        [string]$SourceOnnxPath,
        [string]$PackedOnnxPath,
        [string]$PackedDataName
    )

    $packScript = @'
import sys
import onnx

source_path, packed_path, data_name = sys.argv[1:4]
model = onnx.load_model(source_path, load_external_data=True)
onnx.save_model(
    model,
    packed_path,
    save_as_external_data=True,
    all_tensors_to_one_file=True,
    location=data_name,
    size_threshold=0,
    convert_attribute=False,
)
'@

    & $PythonExe -c $packScript $SourceOnnxPath $PackedOnnxPath $PackedDataName
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to repack ONNX external data."
    }
}

# Activate venv
$VenvPython = Join-Path $ProjectRoot ".venv\Scripts\python.exe"
if (Test-Path $VenvPython) {
    Write-Host "`n[INFO] Using virtual environment Python" -ForegroundColor Cyan
    $PythonCmd = $VenvPython
} else {
    Write-Host "`n[INFO] Venv not found, using system Python" -ForegroundColor Yellow
    $PythonCmd = "py"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "SAM3 Download & Conversion" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Check Python
try {
    $pyVer = & $PythonCmd --version 2>&1
    Write-Host "  [OK] Python: $pyVer" -ForegroundColor Green
} catch {
    Write-Host "  [ERR] Python not found!" -ForegroundColor Red
    exit 1
}

# Check TensorRT
$trtexec = Find-TrtExec -Root $ProjectRoot
if (Test-Path $trtexec) {
    Write-Host "  [OK] TensorRT: $trtexec" -ForegroundColor Green
} else {
    Write-Host "  [ERR] TensorRT not found!" -ForegroundColor Red
    exit 1
}

# Load token from .env if needed
if ([string]::IsNullOrEmpty($HuggingFaceToken)) {
    $HuggingFaceToken = Get-DotEnvValue -FilePath (Join-Path $ProjectRoot ".env") -Key "HF_TOKEN"
}

# Check HF token
if ([string]::IsNullOrEmpty($HuggingFaceToken)) {
    Write-Host "`n  [ERR] HF token required!" -ForegroundColor Red
    Write-Host "  Set HF_TOKEN env var or use -HuggingFaceToken" -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "  [OK] HF Token: Provided" -ForegroundColor Green
}

$env:HF_TOKEN = $HuggingFaceToken

# Install deps
if (!$SkipPythonDeps) {
    Write-Host "`n[STEP] Installing Python packages..." -ForegroundColor Cyan
    & $PythonCmd -m pip install --quiet torch torchvision transformers==5.0.0rc1 accelerate onnx onnxruntime-gpu opencv-python pillow requests huggingface_hub
    Write-Host "  [OK] Dependencies installed" -ForegroundColor Green
}

# Download sam3.pt if hybrid
if ($DownloadMethod -eq "hybrid") {
    Write-Host "`n[STEP] Downloading sam3.pt (Hybrid method)..." -ForegroundColor Cyan
    if (!(Test-Path "$ProjectRoot\models\sam3.pt")) {
        $downloadScript = Join-Path $ProjectRoot "scripts\download_sam3_pytorch.py"
        & $PythonCmd $downloadScript --output models
    } else {
        Write-Host "  [INFO] sam3.pt already exists" -ForegroundColor Gray
    }
}

# Export to ONNX
Write-Host "`n[STEP] Exporting to ONNX..." -ForegroundColor Cyan
$onnxExportDir = Join-Path $ProjectRoot "SAM3TensorRT\python"
$onnxSourcePath = Join-Path $ProjectRoot "SAM3TensorRT\python\onnx_weights\sam3_dynamic.onnx"
$packedOnnxPath = Join-Path $ProjectRoot "$OutputDir\sam3_dynamic.onnx"
$packedOnnxDataPath = "$packedOnnxPath.data"

New-Item -ItemType Directory -Path (Split-Path -Parent $packedOnnxPath) -Force | Out-Null

Push-Location $onnxExportDir
try {
    & $PythonCmd onnxexport.py
} finally {
    Pop-Location
}

Write-Host "`n[STEP] Repacking ONNX into a single data file..." -ForegroundColor Cyan
if (Test-Path $packedOnnxPath) {
    Remove-Item $packedOnnxPath -Force
}
if (Test-Path $packedOnnxDataPath) {
    Remove-Item $packedOnnxDataPath -Force
}
Repack-OnnxExternalData -PythonExe $PythonCmd -SourceOnnxPath $onnxSourcePath -PackedOnnxPath $packedOnnxPath -PackedDataName "sam3_dynamic.onnx.data"
Write-Host "  [OK] Packed ONNX: $packedOnnxPath" -ForegroundColor Green

# Build TensorRT
Write-Host "`n[STEP] Building TensorRT engine..." -ForegroundColor Cyan
$onnxPath = $packedOnnxPath
$enginePath = Join-Path $ProjectRoot "$OutputDir\sam3_$Precision.engine"

New-Item -ItemType Directory -Path (Split-Path -Parent $enginePath) -Force | Out-Null

& $trtexec --onnx=$onnxPath --saveEngine=$enginePath --$Precision --memPoolSize=workspace:4096

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "SUCCESS!" -ForegroundColor Green
    Write-Host "ONNX: $packedOnnxPath" -ForegroundColor White
    Write-Host "Engine: $enginePath" -ForegroundColor White
    Write-Host "========================================" -ForegroundColor Cyan
} else {
    Write-Host "`n[ERR] TensorRT build failed!" -ForegroundColor Red
    exit 1
}
