# SAM3TensorRT/python/pipeline/utils.py
"""Shared utilities for SAM3 pipeline."""

import hashlib
import logging
from pathlib import Path


def setup_logging(level: str = "INFO") -> logging.Logger:
    """Configure logging with timestamp format.

    Args:
        level: Log level (DEBUG, INFO, WARNING, ERROR, CRITICAL)

    Returns:
        Configured logger instance
    """
    logging.basicConfig(
        level=getattr(logging, level.upper()),
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    return logging.getLogger("sam3")


def calculate_sha256(file_path: Path) -> str:
    """Compute SHA256 hash of a file.

    Args:
        file_path: Path to the file

    Returns:
        Hexadecimal hash string
    """
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            sha256_hash.update(chunk)
    return sha256_hash.hexdigest()


def ensure_dir(path: Path) -> Path:
    """Create directory if it doesn't exist.

    Args:
        path: Directory path to create

    Returns:
        The path (for chaining)
    """
    path.mkdir(parents=True, exist_ok=True)
    return path


def format_size(bytes: int) -> str:
    """Convert bytes to human-readable size.

    Args:
        bytes: Size in bytes

    Returns:
        Human-readable string (e.g., "3.2 GB")
    """
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if bytes < 1024:
            return f"{bytes:.1f} {unit}"
        bytes /= 1024
    return f"{bytes:.1f} PB"
