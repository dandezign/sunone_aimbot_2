# SAM3TensorRT/python/onnxexport.py
"""
ONNX export script for SAM3.

This is a thin wrapper around the pipeline.onnx_export module.
For full functionality, use setup_sam3.py instead.

Usage:
    python onnxexport.py
"""

from pathlib import Path

from pipeline.config import Sam3Config
from pipeline.download import Sam3Downloader
from pipeline.onnx_export import Sam3Exporter


def main():
    """Export SAM3 to ONNX format."""
    # Load config
    config = Sam3Config.from_env()

    # Download if needed
    downloader = Sam3Downloader(config)
    if not downloader.exists():
        print("Model weights not found. Please run setup_sam3.py first.")
        print("  python setup_sam3.py --skip-build")
        return

    model_dir = downloader.model_dir

    # Export to ONNX
    exporter = Sam3Exporter(config)
    onnx_path, onnx_data = exporter.export(model_dir)

    print(f"\nExported to:")
    print(f"  {onnx_path}")
    print(f"  {onnx_data}")


if __name__ == "__main__":
    main()
