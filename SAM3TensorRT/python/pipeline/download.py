# SAM3TensorRT/python/pipeline/download.py
"""HuggingFace model download for SAM3."""

import logging
from pathlib import Path
from typing import Optional

from huggingface_hub import login, snapshot_download

from .config import Sam3Config
from .utils import format_size

logger = logging.getLogger("sam3")


class Sam3Downloader:
    """Downloads SAM3 model from HuggingFace."""

    def __init__(self, config: Sam3Config):
        """Initialize downloader with configuration.

        Args:
            config: Sam3Config instance
        """
        self.config = config
        self.model_dir = config.weights_dir

    def download(self, force: bool = False) -> Path:
        """Download SAM3 model from HuggingFace.

        Args:
            force: Redownload even if exists

        Returns:
            Path to downloaded model directory
        """
        # Check if already downloaded
        if self.model_dir.exists() and not force:
            # Check for at least one weight file
            weight_files = list(self.model_dir.glob("*.safetensors")) + list(
                self.model_dir.glob("*.bin")
            )
            if weight_files:
                logger.info(f"Model already exists at {self.model_dir}")
                return self.model_dir

        # Login if token provided
        if self.config.hf_token:
            logger.info("Logging in to HuggingFace...")
            login(token=self.config.hf_token)

        logger.info(f"Downloading {self.config.hf_repo}...")

        # Create output directory
        self.model_dir.mkdir(parents=True, exist_ok=True)

        try:
            # Download with resume support
            snapshot_download(
                repo_id=self.config.hf_repo,
                local_dir=self.model_dir,
                local_dir_use_symlinks=False,
                resume_download=True,
            )

            # Log size
            total_size = sum(
                f.stat().st_size for f in self.model_dir.rglob("*") if f.is_file()
            )
            logger.info(f"Downloaded to {self.model_dir} ({format_size(total_size)})")

            return self.model_dir

        except Exception as e:
            logger.error(f"Download failed: {e}")
            raise

    def get_pytorch_path(self) -> Optional[Path]:
        """Get path to PyTorch weights file.

        Returns:
            Path to .safetensors or .bin file, or None if not found
        """
        safetensors = list(self.model_dir.glob("model.safetensors"))
        if safetensors:
            return safetensors[0]

        bin_files = list(self.model_dir.glob("pytorch_model.bin"))
        if bin_files:
            return bin_files[0]

        return None

    def exists(self) -> bool:
        """Check if weights already exist.

        Returns:
            True if weights directory has weight files
        """
        if not self.model_dir.exists():
            return False
        return (
            len(list(self.model_dir.glob("*.safetensors"))) > 0
            or len(list(self.model_dir.glob("*.bin"))) > 0
        )
