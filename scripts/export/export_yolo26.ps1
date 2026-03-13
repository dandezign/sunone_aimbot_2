# Export YOLO26 models to ONNX
# Usage: .\scripts\export\export_yolo26.ps1

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "YOLO26 Model Export" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Activate virtual environment
$venvActivate = ".venv\Scripts\Activate.ps1"
if (Test-Path $venvActivate) {
    & $venvActivate
    Write-Host "[OK] Virtual environment activated" -ForegroundColor Green
} else {
    Write-Host "[WARN] Virtual environment not found. Creating..." -ForegroundColor Yellow
    py -3.12 -m venv .venv
    & $venvActivate
}

# Check dependencies
Write-Host ""
Write-Host "Checking dependencies..." -ForegroundColor Cyan
$ultralyticsVersion = py -3.12 -c "import ultralytics; print(ultralytics.__version__)" 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "  [OK] Ultralytics: $ultralyticsVersion" -ForegroundColor Green
} else {
    Write-Host "  [ERROR] Ultralytics not installed" -ForegroundColor Red
    Write-Host "  Installing..." -ForegroundColor Yellow
    py -3.12 -m pip install ultralytics onnx onnxruntime-gpu
}

# Run export
Write-Host ""
Write-Host "Exporting YOLO26 models..." -ForegroundColor Cyan
py -3.12 scripts\export\export_yolo26.py --all --output models/

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Export Complete!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Models exported to: models/" -ForegroundColor White
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Models are in models/ folder" -ForegroundColor White
Write-Host "  2. Run .\build\cuda\Release\ai.exe" -ForegroundColor White
Write-Host "  3. Load YOLO26 model in config UI" -ForegroundColor White
Write-Host ""
