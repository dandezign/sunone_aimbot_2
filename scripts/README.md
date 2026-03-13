# Scripts Directory

Scripts for model export, dataset management, and training workflows.

## 📁 Structure

```
scripts/
├── export/           # Model export scripts
│   ├── export_yolo26.py      # Export YOLO26 to ONNX/TensorRT
│   ├── export_yolo26.ps1     # PowerShell wrapper
│   ├── export_yolo26.sh      # Bash wrapper
│   └── README.md
├── dataset/          # Dataset utilities (future)
├── train/            # Training scripts (future)
└── requirements.txt  # Python dependencies
```

## 🚀 Quick Start

### 1. Virtual Environment (Python 3.12)

```powershell
# Create (if not exists)
py -3.12 -m venv .venv

# Activate (PowerShell)
.\.venv\Scripts\Activate.ps1

# Activate (Git Bash)
source .venv/bin/activate
```

### 2. Install Dependencies

```powershell
pip install -r scripts/requirements.txt
```

### 3. Export YOLO26 Models

```powershell
# Easy way (PowerShell)
.\scripts\export\export_yolo26.ps1

# Direct Python
py -3.12 scripts\export\export_yolo26.py --all --output models/

# Export specific model
py -3.12 scripts\export\export_yolo26.py --model yolo26n
```

## 📦 Output

Models are exported to `models/` folder:
- `yolo26n.onnx` - Nano model (~6 MB)
- `yolo26s.onnx` - Small model (~20 MB)
- `yolo26m.onnx` - Medium model (~40 MB)

TensorRT engines (`.engine`) are auto-built on first run in the aimbot.

## 🔧 Future Scripts (Planned)

### Dataset Scripts (`dataset/`)
- `collect_screenshots.py` - Capture game frames
- `annotate_helper.py` - Semi-automatic labeling
- `export_yolo_format.py` - Convert to YOLO format

### Training Scripts (`train/`)
- `train_yolo26.py` - Full training pipeline
- `transfer_learning.py` - Fine-tune from COCO
- `export_best_model.py` - Export best checkpoint

## 📝 Requirements

- Python 3.12+
- Ultralytics 8.3+
- CUDA 13.1+ (for GPU acceleration)
- TensorRT 10.14+ (for engine export)

See `requirements.txt` for full list.
