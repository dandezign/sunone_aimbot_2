"""
Download SAM3 PyTorch weights from HuggingFace

This script downloads sam3.pt (~3.4 GB) for use with:
- Ultralytics Python API (testing/prototyping)
- ONNX export (intermediate step for TensorRT)
- Backup/archive purposes

Requirements:
- HuggingFace token with access to facebook/sam3
- huggingface_hub package: pip install huggingface_hub

Usage:
    python download_sam3_pytorch.py
    python download_sam3_pytorch.py --output models/
"""

import os
import sys
from pathlib import Path

try:
    from huggingface_hub import login, hf_hub_download
except ImportError:
    print("Error: huggingface_hub not installed")
    print("Run: pip install huggingface_hub")
    sys.exit(1)


def download_sam3_pytorch(output_dir: str = "models", token: str = None):  # type: ignore
    """
    Download sam3.pt from HuggingFace

    Args:
        output_dir: Directory to save the model (default: "models")
        token: HuggingFace token (default: HF_TOKEN env var)

    Returns:
        Path to downloaded file
    """
    # Get token from environment or parameter
    if token is None:
        token = os.environ.get("HF_TOKEN")

    if not token:
        print("Error: HuggingFace token not provided")
        print("Set HF_TOKEN environment variable or pass --token parameter")
        print("\nTo get a token:")
        print("1. Visit: https://huggingface.co/settings/tokens")
        print("2. Create new token with 'Read' permissions")
        print("3. Copy the token (starts with 'hf_')")
        print("4. Run: $env:HF_TOKEN = 'hf_xxxxx'")
        sys.exit(1)

    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("SAM3 PyTorch Model Download")
    print("=" * 60)
    print(f"\nOutput directory: {output_path.absolute()}")
    print(f"File: sam3.pt (~3.4 GB)")
    print()

    # Check if already downloaded
    sam3_path = output_path / "sam3.pt"
    if sam3_path.exists():
        file_size_gb = sam3_path.stat().st_size / (1024**3)
        print(f"[OK] File already exists: {sam3_path}")
        print(f"  Size: {file_size_gb:.2f} GB")
        print(f"\nTo redownload, delete the file or use --force")
        return str(sam3_path)

    # Login to HuggingFace
    try:
        login(token=token)
        print("[OK] Logged in to HuggingFace")
    except Exception as e:
        print(f"[ERR] Login failed: {e}")
        print("\nMake sure your token is valid and has 'Read' permissions")
        sys.exit(1)

    # Download the model
    print("\nDownloading sam3.pt...")
    print("This may take 5-20 minutes depending on your internet connection")
    print()

    try:
        downloaded_path = hf_hub_download(
            repo_id="facebook/sam3",
            filename="sam3.pt",
            local_dir=str(output_path),
            local_dir_use_symlinks=False,  # Download directly, don't use cache symlink
            force_download=False,  # Don't redownload if exists
        )

        # Verify download
        if os.path.exists(downloaded_path):
            file_size_gb = os.path.getsize(downloaded_path) / (1024**3)
            print("\n" + "=" * 60)
            print("[OK] Download successful!")
            print("=" * 60)
            print(f"  Path: {downloaded_path}")
            print(f"  Size: {file_size_gb:.2f} GB")
            print()
            print("Next steps:")
            print("  1. Test with Ultralytics:")
            print(
                "     python -c \"from ultralytics import SAM; m = SAM('models/sam3.pt'); m('test.jpg')\""
            )
            print()
            print("  2. Convert to TensorRT:")
            print(
                "     .\\scripts\\sam3_download_and_convert.ps1 -DownloadMethod direct"
            )
            print()
            return downloaded_path
        else:
            print("[ERR] Download failed: file not found")
            sys.exit(1)

    except Exception as e:
        print(f"\n[ERR] Download failed: {e}")
        print("\nPossible causes:")
        print("  - HuggingFace token doesn't have access to facebook/sam3")
        print("  - Request for access hasn't been approved yet")
        print("  - Network connection issue")
        print("\nTo request access:")
        print("  1. Visit: https://huggingface.co/facebook/sam3")
        print("  2. Click 'Request access'")
        print("  3. Wait for approval email (24-48 hours)")
        sys.exit(1)


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Download SAM3 PyTorch weights from HuggingFace",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python download_sam3_pytorch.py
  python download_sam3_pytorch.py --output models/
  python download_sam3_pytorch.py --token hf_xxxxx
  python download_sam3_pytorch.py --force --output models/
        """,
    )

    parser.add_argument(
        "--output",
        "-o",
        type=str,
        default="models",
        help="Output directory for sam3.pt (default: models)",
    )

    parser.add_argument(
        "--token",
        "-t",
        type=str,
        default=None,
        help="HuggingFace token (default: HF_TOKEN env var)",
    )

    parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Force redownload even if file exists",
    )

    args = parser.parse_args()

    # Handle force redownload
    sam3_path = Path(args.output) / "sam3.pt"
    if args.force and sam3_path.exists():
        print(f"Removing existing file: {sam3_path}")
        sam3_path.unlink()

    # Download
    download_sam3_pytorch(output_dir=args.output, token=args.token)


if __name__ == "__main__":
    main()
