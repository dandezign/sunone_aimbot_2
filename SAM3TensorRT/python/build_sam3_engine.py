#!/usr/bin/env python3
"""
SAM3 TensorRT Engine Builder

Downloads SAM3 from HuggingFace, exports to ONNX, and builds TensorRT engine.

Usage:
    python build_sam3_engine.py
    python build_sam3_engine.py --precision fp16
    python build_sam3_engine.py --force

Requirements:
    - HF_TOKEN environment variable (or --token argument)
    - PyTorch, transformers, onnx, huggingface_hub

Output:
    models/sam3_dynamic.onnx        - ONNX model structure
    models/sam3_dynamic.onnx.data   - ONNX weights (~3.3 GB)
    models/sam3_fp16.engine         - TensorRT engine (~1.7 GB)
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Suppress transformers warnings
os.environ["TRANSFORMERS_VERBOSITY"] = "error"


def format_size(path: Path) -> str:
    """Format file size in human readable format."""
    size = path.stat().st_size
    for unit in ["B", "KB", "MB", "GB"]:
        if size < 1024:
            return f"{size:.2f} {unit}"
        size /= 1024
    return f"{size:.2f} TB"


def find_trtexec() -> Path | None:
    """Find trtexec.exe on the system."""
    script_dir = Path(__file__).resolve()
    project_root = script_dir.parents[
        2
    ]  # SAM3TensorRT/python -> SAM3TensorRT -> project root

    candidates = [
        project_root
        / "sunone_aimbot_2"
        / "modules"
        / "TensorRT-10.14.1.48"
        / "bin"
        / "trtexec.exe",
        project_root / "sunone_aimbot_2" / "tensorrt" / "bin" / "trtexec.exe",
        Path(
            "C:/Program Files/NVIDIA GPU Computing Toolkit/TensorRT/v10.14.1.48/bin/trtexec.exe"
        ),
        Path("C:/Program Files/TensorRT/bin/trtexec.exe"),
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    # Search recursively
    search_roots = [
        project_root / "sunone_aimbot_2" / "modules",
        Path("C:/Program Files/NVIDIA GPU Computing Toolkit"),
    ]

    for root in search_roots:
        if root.exists():
            matches = list(root.rglob("trtexec.exe"))
            if matches:
                return matches[0]

    return None


def step_download(token: str, output_dir: Path) -> Path:
    """Download sam3.pt from HuggingFace."""
    from huggingface_hub import login, hf_hub_download

    sam3_pt = output_dir / "sam3.pt"

    if sam3_pt.exists():
        print(f"  [SKIP] sam3.pt already exists ({format_size(sam3_pt)})")
        return sam3_pt

    print(f"  Downloading sam3.pt (~3.4 GB)...")
    login(token=token)

    hf_hub_download(
        repo_id="facebook/sam3",
        filename="sam3.pt",
        local_dir=str(output_dir),
        local_dir_use_symlinks=False,
    )

    print(f"  [OK] Downloaded: {sam3_pt} ({format_size(sam3_pt)})")
    return sam3_pt


def step_export_onnx(output_dir: Path) -> Path:
    """Export SAM3 to ONNX format."""
    import torch
    from PIL import Image
    import requests
    from transformers.models.sam3 import Sam3Processor, Sam3Model

    onnx_dir = Path(__file__).parent / "onnx_weights"
    onnx_path = onnx_dir / "sam3_dynamic.onnx"

    if onnx_path.exists():
        # Check if export is complete (has external data)
        total_size = sum(f.stat().st_size for f in onnx_dir.iterdir() if f.is_file())
        if total_size > 3e9:  # > 3GB means complete export
            print(
                f"  [SKIP] ONNX already exported ({format_size(onnx_path)} + weights)"
            )
            return onnx_path

    print("  Loading SAM3 model...")
    device = "cpu"  # Use CPU for maximum compatibility
    model = Sam3Model.from_pretrained("facebook/sam3").to(device).eval()
    processor = Sam3Processor.from_pretrained("facebook/sam3")

    print("  Preparing sample inputs...")
    image_url = "http://images.cocodataset.org/val2017/000000077595.jpg"
    image = Image.open(requests.get(image_url, stream=True).raw).convert("RGB")
    inputs = processor(images=image, text="person", return_tensors="pt").to(device)

    # Wrapper for clean ONNX graph
    class Sam3ONNXWrapper(torch.nn.Module):
        def __init__(self, sam3):
            super().__init__()
            self.sam3 = sam3

        def forward(self, pixel_values, input_ids, attention_mask):
            outputs = self.sam3(
                pixel_values=pixel_values,
                input_ids=input_ids,
                attention_mask=attention_mask,
            )
            # Return all 4 outputs for proper bounding box extraction with NMS
            # pred_boxes: [1, 200, 4] - normalized (x1, y1, x2, y2)
            # pred_logits: [1, 200] - instance confidence scores
            return (
                outputs.pred_masks,  # [1, 200, 288, 288]
                outputs.pred_boxes,  # [1, 200, 4]
                outputs.pred_logits,  # [1, 200]
                outputs.semantic_seg,  # [1, 1, 288, 288]
            )

    wrapper = Sam3ONNXWrapper(model).to(device).eval()

    print("  Exporting to ONNX (this takes ~2 minutes)...")
    onnx_dir.mkdir(exist_ok=True)

    torch.onnx.export(
        wrapper,
        (inputs["pixel_values"], inputs["input_ids"], inputs["attention_mask"]),
        str(onnx_path),
        input_names=["pixel_values", "input_ids", "attention_mask"],
        output_names=["instance_masks", "pred_boxes", "pred_logits", "semantic_seg"],
        dynamo=False,
        opset_version=17,
    )

    total_size = sum(f.stat().st_size for f in onnx_dir.iterdir() if f.is_file())
    print(f"  [OK] Exported: {onnx_path} (total {total_size / 1e9:.2f} GB)")
    return onnx_path


def step_pack_onnx(source_dir: Path, output_dir: Path) -> tuple[Path, Path]:
    """Pack ONNX weights into single data file."""
    import onnx

    source_onnx = source_dir / "onnx_weights" / "sam3_dynamic.onnx"
    packed_onnx = output_dir / "sam3_dynamic.onnx"
    packed_data = output_dir / "sam3_dynamic.onnx.data"

    if packed_onnx.exists() and packed_data.exists():
        data_size = packed_data.stat().st_size
        if data_size > 3e9:  # > 3GB means complete pack
            print(f"  [SKIP] ONNX already packed ({format_size(packed_data)})")
            return packed_onnx, packed_data

    print("  Loading ONNX with external data...")
    model = onnx.load_model(str(source_onnx), load_external_data=True)

    print("  Packing weights into single file...")
    onnx.save_model(
        model,
        str(packed_onnx),
        save_as_external_data=True,
        all_tensors_to_one_file=True,
        location="sam3_dynamic.onnx.data",
        size_threshold=0,  # Pack ALL tensors
        convert_attribute=False,
    )

    print(f"  [OK] Packed: {packed_onnx} ({format_size(packed_onnx)})")
    print(f"        {packed_data} ({format_size(packed_data)})")
    return packed_onnx, packed_data


def step_build_engine(onnx_path: Path, output_dir: Path, precision: str) -> Path:
    """Build TensorRT engine using trtexec."""
    trtexec = find_trtexec()
    if not trtexec:
        raise RuntimeError("trtexec.exe not found. Install TensorRT.")

    engine_path = output_dir / f"sam3_{precision}.engine"

    if engine_path.exists():
        print(f"  [SKIP] Engine already exists ({format_size(engine_path)})")
        return engine_path

    print(f"  Building {precision.upper()} engine (~2 minutes)...")
    print(f"  trtexec: {trtexec}")

    cmd = [
        str(trtexec),
        f"--onnx={onnx_path}",
        f"--saveEngine={engine_path}",
        f"--{precision}",
        "--memPoolSize=workspace:4096",
    ]

    result = subprocess.run(cmd, capture_output=False)

    if result.returncode != 0:
        raise RuntimeError(f"trtexec failed with code {result.returncode}")

    print(f"  [OK] Engine: {engine_path} ({format_size(engine_path)})")
    return engine_path


def main():
    parser = argparse.ArgumentParser(
        description="Build SAM3 TensorRT engine",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--output",
        "-o",
        default="models",
        help="Output directory (default: models)",
    )
    parser.add_argument(
        "--precision",
        "-p",
        choices=["fp16", "fp32"],
        default="fp16",
        help="TensorRT precision (default: fp16)",
    )
    parser.add_argument(
        "--token",
        "-t",
        help="HuggingFace token (default: HF_TOKEN env var)",
    )
    parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Force rebuild all steps",
    )
    parser.add_argument(
        "--skip-download",
        action="store_true",
        help="Skip download (use existing sam3.pt)",
    )
    parser.add_argument(
        "--skip-engine",
        action="store_true",
        help="Stop after ONNX export (skip TensorRT)",
    )

    args = parser.parse_args()

    # Get token
    token = args.token or os.environ.get("HF_TOKEN")
    if not token and not args.skip_download:
        print("Error: HF_TOKEN not set")
        print("Set environment variable: $env:HF_TOKEN = 'hf_xxxxx'")
        print("Or use: --token hf_xxxxx")
        sys.exit(1)

    # Paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parents[1]  # SAM3TensorRT -> project root
    output_dir = project_root / args.output

    output_dir.mkdir(parents=True, exist_ok=True)

    # Force rebuild: delete existing outputs
    if args.force:
        print("\n[FORCE] Removing existing outputs...")
        engine_path = output_dir / f"sam3_{args.precision}.engine"
        for p in [
            output_dir / "sam3_dynamic.onnx",
            output_dir / "sam3_dynamic.onnx.data",
            engine_path,
        ]:
            if p.exists():
                print(f"  Deleting: {p}")
                if p.is_dir():
                    shutil.rmtree(p)
                else:
                    p.unlink()

        # Also clear ONNX weights
        onnx_weights = script_dir / "onnx_weights"
        if onnx_weights.exists():
            shutil.rmtree(onnx_weights)

    print("=" * 60)
    print("SAM3 TensorRT Engine Builder")
    print("=" * 60)
    print(f"Output: {output_dir}")
    print(f"Precision: {args.precision}")
    print()

    try:
        # Step 1: Download
        print("[1/4] Downloading SAM3 weights...")
        if not args.skip_download:
            step_download(token, output_dir)
        else:
            print("  [SKIP] Download skipped")

        # Step 2: Export ONNX
        print("\n[2/4] Exporting to ONNX...")
        step_export_onnx(script_dir)

        # Step 3: Pack ONNX
        print("\n[3/4] Packing ONNX weights...")
        onnx_path, data_path = step_pack_onnx(script_dir, output_dir)

        # Step 4: Build TensorRT
        if args.skip_engine:
            print("\n[SKIP] TensorRT engine build skipped")
        else:
            print("\n[4/4] Building TensorRT engine...")
            engine_path = step_build_engine(onnx_path, output_dir, args.precision)

        print("\n" + "=" * 60)
        print("SUCCESS!")
        print("=" * 60)
        print(f"  ONNX:  {onnx_path}")
        print(f"  Data:  {data_path}")
        if not args.skip_engine:
            print(f"  Engine: {engine_path}")
        print()

    except Exception as e:
        print(f"\n[ERROR] {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
