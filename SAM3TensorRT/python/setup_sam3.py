#!/usr/bin/env python3
# SAM3TensorRT/python/setup_sam3.py
"""
SAM3 Model Setup Script

Orchestrates the full pipeline:
  1. Download SAM3 from HuggingFace
  2. Export to ONNX format with weight packing
  3. Build TensorRT engine
  4. Copy to destination (interactive mode)

Usage:
  python setup_sam3.py                    # Run with defaults
  python setup_sam3.py --interact         # Interactive mode
  python setup_sam3.py --force            # Force rebuild all steps
  python setup_sam3.py --dry-run          # Show what would be done
  python setup_sam3.py --skip-download    # Skip download step
  python setup_sam3.py --skip-export      # Skip ONNX export
  python setup_sam3.py --skip-build       # Skip engine build
"""

import argparse
import logging
import sys
from pathlib import Path

from pipeline import Sam3Config, Sam3Downloader, Sam3Exporter, Sam3Builder
from pipeline.utils import setup_logging

logger = logging.getLogger("sam3")


def main():
    """Main entry point."""
    args = parse_args()

    # Setup logging
    setup_logging(args.log_level)

    # Load configuration
    config = Sam3Config.from_env(interact=args.interact)

    print("=" * 60)
    print("SAM3 Model Setup Pipeline")
    print("=" * 60)
    print(f"Repo root: {config.repo_root}")
    print(f"Models dir: {config.models_dir}")
    print(f"ONNX output: {config.onnx_output_dir}")
    print()

    # Check for skip flag validation
    errors = validate_skip_flags(args, config)
    if errors:
        for error in errors:
            print(f"Error: {error}")
        sys.exit(1)

    # Dry run mode
    if args.dry_run:
        print_dry_run(config, args)
        return

    # Step 1: Download
    if not args.skip_download:
        downloader = Sam3Downloader(config)
        model_dir = downloader.download(force=args.force)
    else:
        model_dir = config.weights_dir
        print(f"Skipping download. Using weights at {model_dir}")

    # Step 2: Export to ONNX
    if not args.skip_export:
        exporter = Sam3Exporter(config)
        onnx_path, onnx_data = exporter.export(model_dir, force=args.force)
    else:
        onnx_path = config.onnx_path
        print(f"Skipping ONNX export. Using {onnx_path}")

    # Step 3: Build TensorRT engine
    if not args.skip_build:
        builder = Sam3Builder(config)
        engine_path = builder.build(onnx_path, force=args.force)
    else:
        engine_path = config.engine_path
        print(f"Skipping engine build. Using {engine_path}")

    # Step 4: Copy to destination (interactive mode)
    if args.interact:
        dest_path = prompt_destination(config)
        builder = Sam3Builder(config)
        builder.copy_to(dest_path)
        print(f"\nEngine copied to: {dest_path}")

    # Final message
    print("\n" + "=" * 60)
    print("Setup complete!")
    print("=" * 60)
    print(f"Engine: {engine_path}")
    print()
    print("Next steps:")
    print("  1. Build the C++ project:")
    print(
        "     cmake --build SAM3TensorRT/cpp/build/cuda --config Release --target sam3"
    )
    print("  2. Run sam3.exe for auto-labeling")
    print("=" * 60)


def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="SAM3 Model Setup Pipeline",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python setup_sam3.py                    # Run full pipeline
  python setup_sam3.py --interact         # Prompt for destination path
  python setup_sam3.py --force            # Rebuild everything
  python setup_sam3.py --dry-run          # Show what would happen
        """,
    )
    parser.add_argument(
        "--interact",
        action="store_true",
        help="Interactive mode - prompt for destination path",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force rebuild all steps",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without executing",
    )
    parser.add_argument(
        "--skip-download",
        action="store_true",
        help="Skip download step",
    )
    parser.add_argument(
        "--skip-export",
        action="store_true",
        help="Skip ONNX export step",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip TensorRT engine build step",
    )
    parser.add_argument(
        "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        default="INFO",
        help="Logging level (default: INFO)",
    )
    return parser.parse_args()


def validate_skip_flags(args: argparse.Namespace, config: Sam3Config) -> list:
    """Validate skip flags against existing files.

    Args:
        args: Parsed arguments
        config: Configuration instance

    Returns:
        List of error messages (empty if valid)
    """
    errors = []

    if args.skip_download:
        downloader = Sam3Downloader(config)
        if not downloader.exists():
            errors.append("Weights not found. Run without --skip-download first.")

    if args.skip_export:
        if not config.onnx_path.exists() or not config.onnx_data_path.exists():
            errors.append("ONNX files not found. Run without --skip-export first.")

    if args.skip_build:
        if not config.engine_path.exists():
            errors.append("Engine not found. Run without --skip-build first.")

    return errors


def print_dry_run(config: Sam3Config, args: argparse.Namespace) -> None:
    """Print what would be done in dry-run mode.

    Args:
        config: Configuration instance
        args: Parsed arguments
    """
    print("\nDry run - showing what would be done:")
    print("-" * 40)

    if args.skip_download:
        print("[SKIP] Download (using existing weights)")
    else:
        downloader = Sam3Downloader(config)
        if downloader.exists():
            print("[SKIP] Download (already exists, use --force to re-download)")
        else:
            print(f"[RUN] Download {config.hf_repo} to {config.weights_dir}")

    if args.skip_export:
        print("[SKIP] ONNX export (using existing files)")
    else:
        exporter = Sam3Exporter(config)
        if exporter.exists() and not args.force:
            print("[SKIP] ONNX export (already exists, use --force to re-export)")
        else:
            print(f"[RUN] Export ONNX to {config.onnx_output_dir}")
            print(f"      Output: {config.onnx_path}")
            print(f"              {config.onnx_data_path}")

    if args.skip_build:
        print("[SKIP] Engine build (using existing engine)")
    else:
        builder = Sam3Builder(config)
        if builder.exists() and not args.force:
            print("[SKIP] Engine build (already exists, use --force to rebuild)")
        else:
            print(f"[RUN] Build TensorRT engine at {config.engine_path}")

    if args.interact:
        print("[RUN] Prompt for destination path (interactive mode)")

    print("-" * 40)


def prompt_destination(config: Sam3Config) -> Path:
    """Prompt user for destination path in interactive mode.

    Args:
        config: Configuration instance

    Returns:
        User-provided absolute path
    """
    print("\n" + "-" * 40)
    print("Interactive Mode")
    print("-" * 40)

    while True:
        path_str = input(
            "Enter full destination path for .engine file\n"
            "(e.g., I:\\CppProjects\\sunone_aimbot_2\\build\\cuda\\Release\\models): "
        ).strip()

        path = Path(path_str)

        if path.is_absolute():
            return path

        print(f"Error: Please provide an absolute path. Got: {path}")


if __name__ == "__main__":
    main()
