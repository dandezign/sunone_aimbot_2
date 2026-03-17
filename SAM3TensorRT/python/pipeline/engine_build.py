# SAM3TensorRT/python/pipeline/engine_build.py
"""TensorRT engine building for SAM3."""

import logging
import shutil
import subprocess
from pathlib import Path
from typing import List, Optional

from .config import Sam3Config
from .utils import ensure_dir, format_size

logger = logging.getLogger("sam3")


class Sam3Builder:
    """Builds TensorRT engine from ONNX model using trtexec."""

    # Possible trtexec locations (searched in order)
    TRTEXEC_PATHS = [
        "sunone_aimbot_2/modules/TensorRT-{version}/bin/trtexec.exe",
        "C:/Program Files/NVIDIA GPU Computing Toolkit/TensorRT/v{version}/bin/trtexec.exe",
        "C:/Program Files/TensorRT/bin/trtexec.exe",
    ]

    DEFAULT_TENSORRT_VERSION = "10.14.1.48"

    def __init__(self, config: Sam3Config):
        """Initialize builder with configuration.

        Args:
            config: Sam3Config instance
        """
        self.config = config
        self.engine_path = config.engine_path

    def build(self, onnx_path: Path, force: bool = False) -> Path:
        """Build TensorRT engine from ONNX.

        Args:
            onnx_path: Path to ONNX model
            force: Rebuild even if engine exists

        Returns:
            Path to built engine file
        """
        # Check if already built
        if self.engine_path.exists() and not force:
            logger.info(f"Engine already exists at {self.engine_path}")
            return self.engine_path

        # Find trtexec
        trtexec = self._find_trtexec()
        if not trtexec:
            raise FileNotFoundError(
                "trtexec not found. Install TensorRT or set TRTEXEC_PATH in .env\n"
                "Expected locations:\n"
                "  - sunone_aimbot_2/modules/TensorRT-10.14.1.48/bin/trtexec.exe\n"
                "  - C:/Program Files/NVIDIA GPU Computing Toolkit/TensorRT/v10/bin/trtexec.exe"
            )

        logger.info("Building TensorRT engine...")
        logger.info(f"  ONNX: {onnx_path}")
        logger.info(f"  Output: {self.engine_path}")

        # Ensure output directory exists
        ensure_dir(self.engine_path.parent)

        # Build command
        cmd = self._build_command(trtexec, onnx_path)

        # Run trtexec
        logger.info(f"Running: {' '.join(cmd)}")
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=onnx_path.parent,
        )

        if result.returncode != 0:
            logger.error(f"trtexec failed:\n{result.stderr}")
            raise RuntimeError(f"Engine build failed with code {result.returncode}")

        # Check if engine was created
        if not self.engine_path.exists():
            # Try to find it in ONNX directory
            temp_engine = onnx_path.parent / self.config.engine_filename
            if temp_engine.exists():
                shutil.move(str(temp_engine), str(self.engine_path))
            else:
                raise RuntimeError("Engine file was not created")

        # Log size
        engine_size = self.engine_path.stat().st_size
        logger.info(f"Built engine at {self.engine_path} ({format_size(engine_size)})")

        return self.engine_path

    def _find_trtexec(self) -> Optional[Path]:
        """Find trtexec executable.

        Returns:
            Path to trtexec, or None if not found
        """
        # Check config override
        if self.config.tensorrt_bin:
            if self.config.tensorrt_bin.exists():
                return self.config.tensorrt_bin

        # Search standard locations
        version = self.DEFAULT_TENSORRT_VERSION
        for pattern in self.TRTEXEC_PATHS:
            path = self.config.repo_root / pattern.format(version=version)
            if path.exists():
                return path

        return None

    def _build_command(self, trtexec: Path, onnx_path: Path) -> List[str]:
        """Build trtexec command.

        Args:
            trtexec: Path to trtexec executable
            onnx_path: Path to ONNX model

        Returns:
            Command as list of strings
        """
        cmd = [
            str(trtexec),
            f"--onnx={onnx_path.name}",
            f"--saveEngine={self.config.engine_filename}",
            "--memPoolSize=workspace:4096",
        ]

        # Add FP16 flag if enabled
        if self.config.fp16:
            cmd.append("--fp16")

        return cmd

    def copy_to(self, dest_path: Path) -> Path:
        """Copy engine to destination path.

        Args:
            dest_path: Destination path for engine copy

        Returns:
            Path to copied engine
        """
        if not self.engine_path.exists():
            raise FileNotFoundError(f"Engine not found at {self.engine_path}")

        ensure_dir(dest_path.parent)
        shutil.copy2(self.engine_path, dest_path)
        logger.info(f"Copied engine to {dest_path}")

        return dest_path

    def exists(self) -> bool:
        """Check if engine already exists.

        Returns:
            True if engine file exists
        """
        return self.engine_path.exists()
