#!/usr/bin/env bash
# Export YOLO26 models to ONNX
# Usage: ./scripts/export/export_yolo26.sh

set -e

echo "=========================================="
echo "YOLO26 Model Export"
echo "=========================================="

# Activate virtual environment
source .venv/bin/activate 2>/dev/null || source .venv/Scripts/activate

# Run export script
python scripts/export/export_yolo26.py --all --output models/

echo ""
echo "Export complete!"
echo "Models are in: models/"
echo ""
echo "Next steps:"
echo "  1. Copy .onnx files to sunone_aimbot_2/models/"
echo "  2. Run ai.exe"
echo "  3. Load YOLO26 model in config"
