# YOLO26 Model Export Script

Exports YOLO26 models (n/s/m) from PyTorch to ONNX and TensorRT formats.

## Usage

```powershell
# Export all YOLO26 models (n/s/m) to ONNX
.\scripts\export\export_yolo26.ps1

# Export specific model
.\scripts\export\export_yolo26.ps1 -model yolo26n

# Export with custom settings
.\scripts\export\export_yolo26.ps1 -model yolo26s -opset 13 -simplify
```

## Requirements

- Python 3.8+
- Ultralytics 8.3+
- CUDA 13.1+ (for TensorRT export)
- cuDNN 9.20+
- TensorRT 10.14+

## Installation

```powershell
pip install -U ultralytics
```

## Output

Models are exported to `models/` folder:
- `yolo26n.onnx` - ONNX format (DirectML/CPU)
- `yolo26s.onnx` - ONNX format (DirectML/CPU)
- `yolo26m.onnx` - ONNX format (DirectML/CPU)
- `yolo26n.engine` - TensorRT engine (CUDA, auto-built on first run)
