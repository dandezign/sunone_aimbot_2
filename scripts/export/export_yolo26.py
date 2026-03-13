#!/usr/bin/env python3
"""
YOLO26 Model Export Script

Exports YOLO26 models (n/s/m) from pretrained PyTorch weights to ONNX format.
Optional TensorRT engine export for NVIDIA GPUs.

Usage:
    python export_yolo26.py --model yolo26n --output models/
    python export_yolo26.py --model yolo26s --opset 13 --simplify
    python export_yolo26.py --all  # Export all models (n/s/m)
"""

import argparse
import os
import sys
from pathlib import Path
from datetime import datetime

try:
    from ultralytics import YOLO
except ImportError:
    print("ERROR: Ultralytics not installed. Run: pip install -U ultralytics")
    sys.exit(1)


# YOLO26 model variants
YOLO26_MODELS = {
    "yolo26n": {"name": "yolo26n.pt", "params": "2.4M", "flops": "5.4B"},
    "yolo26s": {"name": "yolo26s.pt", "params": "9.5M", "flops": "20.7B"},
    "yolo26m": {"name": "yolo26m.pt", "params": "20.4M", "flops": "67.5B"},
}


def export_model(
    model_name: str,
    output_dir: str,
    opset: int = 13,
    simplify: bool = True,
    dynamic: bool = True,
    export_tensorrt: bool = False,
    fp16: bool = True,
) -> bool:
    """
    Export a YOLO26 model to ONNX (and optionally TensorRT).

    Args:
        model_name: Model variant ('yolo26n', 'yolo26s', 'yolo26m')
        output_dir: Output directory for exported models
        opset: ONNX opset version (13 recommended)
        simplify: Run ONNX simplifier
        dynamic: Enable dynamic axes (variable input size)
        export_tensorrt: Export to TensorRT engine
        fp16: Use FP16 precision for TensorRT

    Returns:
        True if export successful, False otherwise
    """
    if model_name not in YOLO26_MODELS:
        print(f"ERROR: Unknown model '{model_name}'")
        print(f"Available: {', '.join(YOLO26_MODELS.keys())}")
        return False

    print(f"\n{'=' * 60}")
    print(f"Exporting {model_name.upper()}")
    print(f"{'=' * 60}")

    model_info = YOLO26_MODELS[model_name]
    print(f"Model: {model_info['name']}")
    print(f"Parameters: {model_info['params']}")
    print(f"FLOPs: {model_info['flops']}")
    print(f"Opset: {opset}")
    print(f"Simplify: {simplify}")
    print(f"Dynamic: {dynamic}")

    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    # Load pretrained model
    print(f"\nLoading {model_name} from Ultralytics hub...")
    try:
        model = YOLO(model_info["name"])
    except Exception as e:
        print(f"ERROR: Failed to load model: {e}")
        return False

    # Export to ONNX
    onnx_path = output_path / f"{model_name}.onnx"
    print(f"\nExporting to ONNX: {onnx_path}")

    try:
        model.export(
            format="onnx",
            opset=opset,
            simplify=simplify,
            dynamic=dynamic,
            imgsz=640,
            half=False,  # FP32 for ONNX (FP16 for TensorRT)
        )
        print(f"✅ ONNX export successful: {onnx_path}")
    except Exception as e:
        print(f"❌ ONNX export failed: {e}")
        return False

    # Export to TensorRT (optional)
    if export_tensorrt:
        print(f"\n{'=' * 60}")
        print(f"Exporting TensorRT Engine (FP{'16' if fp16 else '32'})")
        print(f"{'=' * 60}")

        engine_path = output_path / f"{model_name}.engine"
        print(f"Exporting to TensorRT: {engine_path}")

        try:
            model.export(
                format="engine",
                device=0,
                half=fp16,
                imgsz=640,
                dynamic=dynamic,
            )
            print(f"✅ TensorRT export successful: {engine_path}")
        except Exception as e:
            print(f"⚠️  TensorRT export failed (will auto-build on first run): {e}")

    # Verify outputs
    print(f"\n{'=' * 60}")
    print("Export Summary")
    print(f"{'=' * 60}")

    exported_files = []
    if onnx_path.exists():
        size_mb = onnx_path.stat().st_size / (1024 * 1024)
        exported_files.append(f"  ✅ {onnx_path.name} ({size_mb:.2f} MB)")

    if export_tensorrt:
        engine_path = output_path / f"{model_name}.engine"
        if engine_path.exists():
            size_mb = engine_path.stat().st_size / (1024 * 1024)
            exported_files.append(f"  ✅ {engine_path.name} ({size_mb:.2f} MB)")
        else:
            exported_files.append(f"  ⚠️  {model_name}.engine (will build on first run)")

    for f in exported_files:
        print(f)

    print(f"\nTimestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'=' * 60}\n")

    return True


def export_all(output_dir: str, **kwargs) -> dict:
    """
    Export all YOLO26 models (n/s/m).

    Returns:
        Dictionary with export results
    """
    results = {}

    print(f"\n{'=' * 70}")
    print("EXPORTING ALL YOLO26 MODELS")
    print(f"{'=' * 70}\n")

    for model_name in YOLO26_MODELS.keys():
        success = export_model(model_name, output_dir, **kwargs)
        results[model_name] = success

        if not success:
            print(f"⚠️  {model_name} export FAILED, continuing with next model...\n")

    # Summary
    print(f"\n{'=' * 70}")
    print("FINAL SUMMARY")
    print(f"{'=' * 70}")

    for model_name, success in results.items():
        status = "✅ SUCCESS" if success else "❌ FAILED"
        print(f"  {model_name}: {status}")

    total_success = sum(results.values())
    print(f"\nTotal: {total_success}/{len(results)} models exported successfully")
    print(f"{'=' * 70}\n")

    return results


def main():
    parser = argparse.ArgumentParser(
        description="Export YOLO26 models to ONNX/TensorRT",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python export_yolo26.py --model yolo26n
  python export_yolo26.py --model yolo26s --opset 13 --no-simplify
  python export_yolo26.py --all --output models/
  python export_yolo26.py --model yolo26m --tensorrt --fp16
        """,
    )

    parser.add_argument(
        "--model",
        "-m",
        choices=list(YOLO26_MODELS.keys()),
        help="Model variant to export (yolo26n, yolo26s, yolo26m)",
    )
    parser.add_argument(
        "--all", "-a", action="store_true", help="Export all YOLO26 models (n/s/m)"
    )
    parser.add_argument(
        "--output", "-o", default="models", help="Output directory (default: models/)"
    )
    parser.add_argument(
        "--opset", type=int, default=13, help="ONNX opset version (default: 13)"
    )
    parser.add_argument(
        "--no-simplify", action="store_true", help="Disable ONNX simplifier"
    )
    parser.add_argument(
        "--no-dynamic",
        action="store_true",
        help="Disable dynamic axes (fixed input size)",
    )
    parser.add_argument(
        "--tensorrt",
        action="store_true",
        help="Export to TensorRT engine (requires NVIDIA GPU)",
    )
    parser.add_argument(
        "--fp32", action="store_true", help="Use FP32 for TensorRT (default: FP16)"
    )

    args = parser.parse_args()

    # Validate arguments
    if not args.model and not args.all:
        parser.print_help()
        print("\nERROR: Specify --model or --all")
        sys.exit(1)

    if args.model and args.all:
        parser.print_help()
        print("\nERROR: Cannot specify both --model and --all")
        sys.exit(1)

    # Export
    export_kwargs = {
        "opset": args.opset,
        "simplify": not args.no_simplify,
        "dynamic": not args.no_dynamic,
        "export_tensorrt": args.tensorrt,
        "fp16": not args.fp32,
    }

    if args.all:
        results = export_all(args.output, **export_kwargs)
        # Exit with error if any failed
        if not all(results.values()):
            sys.exit(1)
    else:
        success = export_model(args.model, args.output, **export_kwargs)
        if not success:
            sys.exit(1)

    print("Export complete! ✅\n")


if __name__ == "__main__":
    main()
