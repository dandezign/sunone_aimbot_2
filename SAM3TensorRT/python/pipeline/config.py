# SAM3TensorRT/python/pipeline/config.py
"""Configuration management for SAM3 pipeline."""

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from dotenv import load_dotenv


@dataclass
class Sam3Config:
    """Configuration for SAM3 pipeline.

    All paths are resolved relative to repo_root.
    """

    # Paths
    repo_root: Path = field(
        default_factory=lambda: Path(__file__).parent.parent.parent.parent
    )
    models_dir: Path = field(default=Path("models"))
    onnx_output_dir: Path = field(default=Path("SAM3TensorRT/python/onnx_weights"))

    # HuggingFace settings
    hf_token: Optional[str] = None
    hf_repo: str = "facebook/sam3"

    # TensorRT settings
    tensorrt_bin: Optional[Path] = None
    fp16: bool = True

    # Output settings
    onnx_filename: str = "sam3_dynamic.onnx"
    engine_filename: str = "sam3_fp16.engine"

    def __post_init__(self):
        """Resolve relative paths to absolute paths."""
        if not self.models_dir.is_absolute():
            self.models_dir = self.repo_root / self.models_dir
        if not self.onnx_output_dir.is_absolute():
            self.onnx_output_dir = self.repo_root / self.onnx_output_dir

    @classmethod
    def from_env(cls, interact: bool = False) -> "Sam3Config":
        """Load configuration from .env file.

        Args:
            interact: Whether interactive mode is enabled

        Returns:
            Sam3Config instance with loaded values
        """
        # Load .env from repo root
        repo_root = Path(__file__).parent.parent.parent.parent
        env_file = repo_root / ".env"
        if env_file.exists():
            load_dotenv(env_file)

        return cls(
            repo_root=repo_root,
            models_dir=Path(os.getenv("SAM3_OUTPUT_DIR", "models")),
            onnx_output_dir=Path(
                os.getenv("SAM3_ONNX_OUTPUT_DIR", "SAM3TensorRT/python/onnx_weights")
            ),
            hf_token=os.getenv("HF_TOKEN"),
            hf_repo=os.getenv("SAM3_HUGGINGFACE_REPO", "facebook/sam3"),
            tensorrt_bin=Path(os.getenv("TRTEXEC_PATH"))
            if os.getenv("TRTEXEC_PATH")
            else None,
            onnx_filename=os.getenv("SAM3_ONNX_FILENAME", "sam3_dynamic.onnx"),
        )

    @property
    def onnx_path(self) -> Path:
        """Full path to ONNX model file."""
        return self.onnx_output_dir / self.onnx_filename

    @property
    def onnx_data_path(self) -> Path:
        """Full path to ONNX data file."""
        return self.onnx_output_dir / (self.onnx_filename + ".data")

    @property
    def engine_path(self) -> Path:
        """Full path to TensorRT engine file."""
        return self.models_dir / self.engine_filename

    @property
    def weights_dir(self) -> Path:
        """Full path to downloaded weights directory."""
        return self.models_dir / "sam3_weights"

    def resolve_output_path(self, path: Optional[str] = None) -> Path:
        """Resolve output path for engine file.

        Args:
            path: Optional custom path (for interactive mode)

        Returns:
            Resolved absolute path
        """
        if path:
            return Path(path).resolve()
        return self.engine_path
