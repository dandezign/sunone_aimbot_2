# SAM3TensorRT/python/pipeline/download.py
"""HuggingFace model download for SAM3."""

import logging
from pathlib import Path
from typing import Optional

from huggingface_hub import login, hf_hub_download

from .config import Sam3Config
from .utils import format_size

logger = logging.getLogger("sam3")


class Sam3Downloader:
    """Downloads SAM3 model from HuggingFace.

    Downloads sam3.pt directly (Ultralytics format) AND the full HuggingFace
    model structure for ONNX export with transformers.
    """

    # Target file in HuggingFace repo
    SAM3_PT_FILENAME = "sam3.pt"

    def __init__(self, config: Sam3Config):
        """Initialize downloader with configuration.

        Args:
            config: Sam3Config instance
        """
        self.config = config
        self.model_dir = config.models_dir / "sam3_weights"
        self.sam3_pt_path = self.model_dir / self.SAM3_PT_FILENAME

    def download(self, force: bool = False) -> Path:
        """Download SAM3 model from HuggingFace.

        Downloads sam3.pt directly (Ultralytics checkpoint format).
        This is the recommended format for ONNX export.

        Args:
            force: Redownload even if exists

        Returns:
            Path to downloaded model directory
        """
        # Check if sam3.pt already exists
        if self.sam3_pt_path.exists() and not force:
            file_size = self.sam3_pt_path.stat().st_size
            logger.info(f"sam3.pt already exists at {self.sam3_pt_path}")
            logger.info(f"Size: {format_size(file_size)}")
            return self.model_dir

        # Login if token provided
        if self.config.hf_token:
            logger.info("Logging in to HuggingFace...")
            login(token=self.config.hf_token)

        logger.info(f"Downloading {self.config.hf_repo}/{self.SAM3_PT_FILENAME}...")

        # Create output directory
        self.model_dir.mkdir(parents=True, exist_ok=True)

        try:
            # Download sam3.pt directly
            downloaded_path = hf_hub_download(
                repo_id=self.config.hf_repo,
                filename=self.SAM3_PT_FILENAME,
                local_dir=str(self.model_dir),
                local_dir_use_symlinks=False,
                force_download=force,
            )

            # Verify download
            if self.sam3_pt_path.exists():
                file_size = self.sam3_pt_path.stat().st_size
                logger.info(f"Downloaded to {self.sam3_pt_path}")
                logger.info(f"Size: {format_size(file_size)}")
                return self.model_dir
            else:
                raise RuntimeError(
                    f"Download succeeded but file not found at {self.sam3_pt_path}"
                )

        except Exception as e:
            logger.error(f"Download failed: {e}")
            raise

    def get_sam3_pt_path(self) -> Optional[Path]:
        """Get path to sam3.pt file.

        Returns:
            Path to sam3.pt, or None if not found
        """
        if self.sam3_pt_path.exists():
            return self.sam3_pt_path
        return None

    def get_pytorch_path(self) -> Optional[Path]:
        """Get path to PyTorch weights file.

        Returns:
            Path to sam3.pt, or None if not found
        """
        return self.get_sam3_pt_path()

    def exists(self) -> bool:
        """Check if sam3.pt already exists.

        Returns:
            True if sam3.pt exists
        """
        return self.sam3_pt_path.exists()
