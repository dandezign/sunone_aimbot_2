# SAM3 Setup Orchestration Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a master Python script that orchestrates SAM3 model setup (download → ONNX export → TensorRT engine build) and update CMakeLists.txt to copy models/training to build output.

**Architecture:** Modular `pipeline/` package with separate modules for config, download, ONNX export, and engine build. CLI entry point orchestrates the pipeline with `--interact`, `--force`, `--dry-run`, and `--skip-*` flags.

**Tech Stack:** Python 3.12+, PyTorch, Transformers, huggingface_hub, ONNX, TensorRT/trtexec, CMake

---

## File Structure

### New Files (Create)
```
SAM3TensorRT/python/
├── pipeline/
│   ├── __init__.py            # Package init
│   ├── config.py              # Config class, .env loading
│   ├── download.py            # HuggingFace download
│   ├── onnx_export.py         # ONNX export + weight packing
│   ├── engine_build.py        # TensorRT engine build
│   └── utils.py               # Shared utilities
├── setup_sam3.py              # CLI entry point
└── requirements.txt           # Python dependencies
```

### Modified Files
```
SAM3TensorRT/python/onnxexport.py          # Refactor to use pipeline/
SAM3TensorRT/cpp/CMakeLists.txt            # Add models/training copy, rename exe
.env                                       # Add SAM3 config variables
```

---

## Chunk 1: Foundation (utils.py, config.py, __init__.py)

### Task 1: Create pipeline/utils.py

**Files:**
- Create: `SAM3TensorRT/python/pipeline/utils.py`

- [ ] **Step 1: Create pipeline directory**

```bash
mkdir -p SAM3TensorRT/python/pipeline
```

- [ ] **Step 2: Write utils.py with shared utilities**

```python
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
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/python/pipeline/utils.py
git commit -m "feat(sam3): add pipeline/utils.py with shared utilities"
```

---

### Task 2: Create pipeline/config.py

**Files:**
- Create: `SAM3TensorRT/python/pipeline/config.py`

- [ ] **Step 1: Write config.py with Sam3Config class**

```python
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
    repo_root: Path = field(default_factory=lambda: Path(__file__).parent.parent.parent.parent)
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
            onnx_output_dir=Path(os.getenv("SAM3_ONNX_OUTPUT_DIR", "SAM3TensorRT/python/onnx_weights")),
            hf_token=os.getenv("HF_TOKEN"),
            hf_repo=os.getenv("SAM3_HUGGINGFACE_REPO", "facebook/sam3"),
            tensorrt_bin=Path(os.getenv("TRTEXEC_PATH")) if os.getenv("TRTEXEC_PATH") else None,
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
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/pipeline/config.py
git commit -m "feat(sam3): add pipeline/config.py with Sam3Config class"
```

---

### Task 3: Create pipeline/__init__.py (empty)

**Files:**
- Create: `SAM3TensorRT/python/pipeline/__init__.py`

- [ ] **Step 1: Create empty __init__.py**

```python
# SAM3TensorRT/python/pipeline/__init__.py
"""SAM3 pipeline package for model setup orchestration."""

# Exports will be added after all modules are created
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/pipeline/__init__.py
git commit -m "feat(sam3): add pipeline/__init__.py (empty)"
```

---

## Chunk 2: Download Module

### Task 4: Create pipeline/download.py

**Files:**
- Create: `SAM3TensorRT/python/pipeline/download.py`

- [ ] **Step 1: Write download.py with Sam3Downloader class**

```python
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
            weight_files = list(self.model_dir.glob("*.safetensors")) + \
                          list(self.model_dir.glob("*.bin"))
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
            total_size = sum(f.stat().st_size for f in self.model_dir.rglob("*") if f.is_file())
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
        return len(list(self.model_dir.glob("*.safetensors"))) > 0 or \
               len(list(self.model_dir.glob("*.bin"))) > 0
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/pipeline/download.py
git commit -m "feat(sam3): add pipeline/download.py with Sam3Downloader"
```

---

## Chunk 3: ONNX Export Module

### Task 5: Create pipeline/onnx_export.py

**Files:**
- Create: `SAM3TensorRT/python/pipeline/onnx_export.py`

- [ ] **Step 1: Write onnx_export.py with Sam3Exporter class**

```python
# SAM3TensorRT/python/pipeline/onnx_export.py
"""ONNX export and weight packing for SAM3."""

import logging
from pathlib import Path
from typing import Tuple, Optional

import onnx
import torch
from onnx.external_data_helper import convert_model_to_external_data
from PIL import Image
from transformers import Sam3Model, Sam3Processor

from .config import Sam3Config
from .utils import ensure_dir, format_size

logger = logging.getLogger("sam3")


class Sam3ONNXWrapper(torch.nn.Module):
    """Wrapper for SAM3 model with explicit output handling.
    
    This ensures all outputs are properly traced during ONNX export.
    """
    
    def __init__(self, sam3: Sam3Model):
        super().__init__()
        self.sam3 = sam3
    
    def forward(
        self,
        pixel_values: torch.Tensor,
        input_ids: torch.Tensor,
        attention_mask: torch.Tensor,
    ) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor, torch.Tensor]:
        """Forward pass with explicit output extraction.
        
        Args:
            pixel_values: Image tensor [batch, 3, H, W]
            input_ids: Token IDs [batch, seq_len]
            attention_mask: Attention mask [batch, seq_len]
            
        Returns:
            Tuple of (pred_masks, pred_boxes, pred_logits, semantic_seg)
        """
        outputs = self.sam3(
            pixel_values=pixel_values,
            input_ids=input_ids,
            attention_mask=attention_mask,
        )
        
        # Validate outputs exist
        if outputs.pred_masks is None:
            raise RuntimeError("pred_masks is None!")
        if outputs.pred_boxes is None:
            raise RuntimeError("pred_boxes is None!")
        if outputs.pred_logits is None:
            raise RuntimeError("pred_logits is None!")
        if outputs.semantic_seg is None:
            raise RuntimeError("semantic_seg is None!")
        
        # Return all 4 outputs as contiguous tensors
        return (
            outputs.pred_masks.contiguous(),
            outputs.pred_boxes.contiguous(),
            outputs.pred_logits.contiguous(),
            outputs.semantic_seg.contiguous(),
        )


class Sam3Exporter:
    """Exports SAM3 model to ONNX format with weight packing."""
    
    def __init__(self, config: Sam3Config):
        """Initialize exporter with configuration.
        
        Args:
            config: Sam3Config instance
        """
        self.config = config
        self.output_dir = config.onnx_output_dir
        self.onnx_path = config.onnx_path
        self.onnx_data_path = config.onnx_data_path
    
    def export(self, model_dir: Path, force: bool = False) -> Tuple[Path, Path]:
        """Export SAM3 to ONNX format with weight packing.
        
        Args:
            model_dir: Path to downloaded model weights
            force: Re-export even if ONNX exists
            
        Returns:
            Tuple of (onnx_path, onnx_data_path)
        """
        # Check if already exported
        if self.onnx_path.exists() and self.onnx_data_path.exists() and not force:
            logger.info(f"ONNX already exists at {self.onnx_path}")
            return self.onnx_path, self.onnx_data_path
        
        logger.info("Exporting to ONNX...")
        
        # Load model and processor
        logger.info("Loading model...")
        model = Sam3Model.from_pretrained(str(model_dir)).to("cpu").eval()
        processor = Sam3Processor.from_pretrained(str(model_dir))
        
        # Create wrapper
        wrapper = Sam3ONNXWrapper(model).to("cpu").eval()
        
        # Prepare dummy inputs
        logger.info("Preparing sample inputs...")
        dummy_image = Image.new("RGB", (1008, 1008), color=(128, 128, 128))
        inputs = processor(images=dummy_image, text="object", return_tensors="pt")
        
        pixel_values = inputs["pixel_values"]
        input_ids = inputs["input_ids"]
        attention_mask = inputs["attention_mask"]
        
        # Test forward pass
        logger.info("Testing forward pass...")
        with torch.no_grad():
            test_outputs = wrapper(pixel_values, input_ids, attention_mask)
            for i, tensor in enumerate(test_outputs):
                logger.info(f"  Output {i}: shape={tensor.shape}, dtype={tensor.dtype}")
        
        # Create output directory
        ensure_dir(self.output_dir)
        
        # Export to temporary ONNX file
        temp_onnx = self.output_dir / "temp_sam3.onnx"
        logger.info(f"Exporting to {temp_onnx}...")
        
        torch.onnx.export(
            wrapper,
            (pixel_values, input_ids, attention_mask),
            str(temp_onnx),
            input_names=["pixel_values", "input_ids", "attention_mask"],
            output_names=["pred_masks", "pred_boxes", "pred_logits", "semantic_seg"],
            opset_version=17,
            do_constant_folding=True,
            verbose=False,
        )
        
        # Pack weights into external data
        logger.info("Packing weights into external data...")
        self._pack_weights(temp_onnx)
        
        # Cleanup temp file
        if temp_onnx.exists():
            temp_onnx.unlink()
        
        # Log sizes
        onnx_size = self.onnx_path.stat().st_size if self.onnx_path.exists() else 0
        data_size = self.onnx_data_path.stat().st_size if self.onnx_data_path.exists() else 0
        logger.info(f"Exported to {self.onnx_path} ({format_size(onnx_size)})")
        logger.info(f"Data file: {self.onnx_data_path} ({format_size(data_size)})")
        
        return self.onnx_path, self.onnx_data_path
    
    def _pack_weights(self, temp_onnx: Path) -> None:
        """Pack ONNX weights into external .data file.
        
        Args:
            temp_onnx: Path to temporary ONNX file
        """
        model = onnx.load(str(temp_onnx))
        
        convert_model_to_external_data(
            model,
            all_tensors_to_one_file=True,
            location=self.config.onnx_filename + ".data",
            size_threshold=500_000_000,  # 500MB threshold
        )
        
        onnx.save(model, str(self.onnx_path))
    
    def exists(self) -> bool:
        """Check if ONNX files already exist.
        
        Returns:
            True if both .onnx and .onnx.data exist
        """
        return self.onnx_path.exists() and self.onnx_data_path.exists()
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/pipeline/onnx_export.py
git commit -m "feat(sam3): add pipeline/onnx_export.py with Sam3Exporter"
```

---

### Task 6: Refactor existing onnxexport.py

**Files:**
- Modify: `SAM3TensorRT/python/onnxexport.py`

- [ ] **Step 1: Replace onnxexport.py with thin wrapper**

```python
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
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/onnxexport.py
git commit -m "refactor(sam3): simplify onnxexport.py to use pipeline module"
```

---

## Chunk 4: Engine Build Module

### Task 7: Create pipeline/engine_build.py

**Files:**
- Create: `SAM3TensorRT/python/pipeline/engine_build.py`

- [ ] **Step 1: Write engine_build.py with Sam3Builder class**

```python
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
            # Input shapes
            "--minShapes=pixel_values:1x3x1008x1008",
            "--optShapes=pixel_values:1x3x1008x1008",
            "--maxShapes=pixel_values:1x3x1008x1008",
            "--minShapes=input_ids:1x32",
            "--optShapes=input_ids:1x32",
            "--maxShapes=input_ids:1x32",
            "--minShapes=attention_mask:1x32",
            "--optShapes=attention_mask:1x32",
            "--maxShapes=attention_mask:1x32",
            # Memory and performance
            "--memPoolSize=workspace:4096",
            "--timingCacheFile=sam3_cache.cache",
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
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/pipeline/engine_build.py
git commit -m "feat(sam3): add pipeline/engine_build.py with Sam3Builder"
```

---

## Chunk 5: CLI Entry Point

### Task 8: Create setup_sam3.py

**Files:**
- Create: `SAM3TensorRT/python/setup_sam3.py`

- [ ] **Step 1: Write setup_sam3.py CLI entry point**

```python
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
    print("     cmake --build SAM3TensorRT/cpp/build/cuda --config Release --target sam3")
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
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/setup_sam3.py
git commit -m "feat(sam3): add setup_sam3.py CLI entry point"
```

---

## Chunk 6: CMakeLists.txt Updates

### Task 9: Update SAM3TensorRT/cpp/CMakeLists.txt

**Files:**
- Modify: `SAM3TensorRT/cpp/CMakeLists.txt`

- [ ] **Step 1: Read current CMakeLists.txt to find insertion points**

Run: Read `SAM3TensorRT/cpp/CMakeLists.txt` to locate:
1. Target definition for `sam3_pcs_app` (to rename to `sam3`)
2. End of file (to add post-build commands)

- [ ] **Step 2: Rename executable target from sam3_pcs_app to sam3**

Find the line:
```cmake
add_executable(sam3_pcs_app
```

Replace with:
```cmake
add_executable(sam3
```

- [ ] **Step 3: Update all references to sam3_pcs_app target**

Find and replace all occurrences of `sam3_pcs_app` with `sam3`:
- `target_link_libraries(sam3_pcs_app` → `target_link_libraries(sam3`
- `target_include_directories(sam3_pcs_app` → `target_include_directories(sam3`
- `sam3_copy_if_exists(sam3_pcs_app` → `sam3_copy_if_exists(sam3`

- [ ] **Step 4: Add post-build commands for models/training copy**

Add at end of file (before any install commands if present):

```cmake
# ============================================
# Model and Training Data Copy
# ============================================

# Source directories (from repo root)
set(SAM3_MODELS_SOURCE "${SAM3_WORKSPACE_ROOT}/models")
set(SAM3_TRAINING_SOURCE "${SAM3_WORKSPACE_ROOT}/sunone_aimbot_2/scripts/training")

# Destination directories (in build output)
set(SAM3_BUILD_OUTPUT "$<TARGET_FILE_DIR:sam3>")
set(SAM3_MODELS_DEST "${SAM3_BUILD_OUTPUT}/models")
set(SAM3_TRAINING_DEST "${SAM3_BUILD_OUTPUT}/training")
set(SAM3_RESULTS_DEST "${SAM3_BUILD_OUTPUT}/results")

# Post-build setup
add_custom_command(TARGET sam3 POST_BUILD
    # Create output directories
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SAM3_MODELS_DEST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SAM3_TRAINING_DEST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SAM3_RESULTS_DEST}"
    
    # Copy models if source exists
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${SAM3_MODELS_SOURCE}"
        "${SAM3_MODELS_DEST}"
    
    # Copy training data
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${SAM3_TRAINING_SOURCE}"
        "${SAM3_TRAINING_DEST}"
    
    COMMENT "Setting up models/ and training/ directories..."
)
```

- [ ] **Step 5: Commit**

```bash
git add SAM3TensorRT/cpp/CMakeLists.txt
git commit -m "feat(sam3): rename exe to sam3, add models/training copy"
```

---

## Chunk 7: Requirements and Documentation

### Task 10: Create SAM3TensorRT/python/requirements.txt

**Files:**
- Create: `SAM3TensorRT/python/requirements.txt`

- [ ] **Step 1: Write requirements.txt**

```txt
# SAM3 Pipeline Dependencies
# Install with: pip install -r requirements.txt

# Core ML
torch>=2.0.0
transformers>=4.40.0
huggingface_hub>=0.20.0

# ONNX
onnx>=1.15.0

# Utilities
python-dotenv>=1.0.0
Pillow>=10.0.0
requests>=2.28.0

# Optional: for development/testing
pytest>=7.0.0
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/requirements.txt
git commit -m "feat(sam3): add requirements.txt for pipeline dependencies"
```

---

### Task 11: Update .env with SAM3 configuration

**Files:**
- Modify: `.env`

- [ ] **Step 1: Add SAM3 configuration to .env**

Append to existing `.env` file:

```env
# ============================================
# SAM3 Model Configuration
# ============================================
SAM3_HUGGINGFACE_REPO=facebook/sam3
SAM3_OUTPUT_DIR=models
SAM3_DEFAULT_ENGINE_PATH=models/sam3_fp16.engine
SAM3_ONNX_OUTPUT_DIR=SAM3TensorRT/python/onnx_weights
SAM3_ONNX_FILENAME=sam3_dynamic.onnx

# TensorRT Configuration
TENSORRT_VERSION=10.14.1.48
TRTEXEC_PATH=

# CLIP Tokenizer
CLIP_TOKENIZER_MODEL=openai/clip-vit-base-patch32
```

- [ ] **Step 2: Commit**

```bash
git add .env
git commit -m "feat(sam3): add SAM3 configuration to .env"
```

**Note on Error Handling:** The following error handling features from the spec are deferred to a future iteration:
- Disk space check (warn if < 10GB)
- Network retry logic with exponential backoff
- SHA256 hash validation for file integrity
- Malformed .env line handling
- CUDA version mismatch detection

These can be added incrementally as needed.

---

### Task 11.5: Update pipeline/__init__.py with exports

**Files:**
- Modify: `SAM3TensorRT/python/pipeline/__init__.py`

- [ ] **Step 1: Update __init__.py with all exports**

```python
# SAM3TensorRT/python/pipeline/__init__.py
"""SAM3 pipeline package for model setup orchestration."""

from .config import Sam3Config
from .download import Sam3Downloader
from .onnx_export import Sam3Exporter
from .engine_build import Sam3Builder

__all__ = ["Sam3Config", "Sam3Downloader", "Sam3Exporter", "Sam3Builder"]
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/python/pipeline/__init__.py
git commit -m "feat(sam3): add exports to pipeline/__init__.py"
```

---

### Task 11.6: Add unit tests (basic)

**Note:** Full unit test coverage is deferred to a future iteration. This task creates basic smoke tests.

**Files:**
- Create: `SAM3TensorRT/python/tests/__init__.py`
- Create: `SAM3TensorRT/python/tests/test_config.py`

- [ ] **Step 1: Create tests directory and __init__.py**

```bash
mkdir -p SAM3TensorRT/python/tests
```

```python
# SAM3TensorRT/python/tests/__init__.py
"""Tests for SAM3 pipeline."""
```

- [ ] **Step 2: Write test_config.py**

```python
# SAM3TensorRT/python/tests/test_config.py
"""Tests for Sam3Config."""

import pytest
from pathlib import Path

from pipeline.config import Sam3Config


def test_config_defaults():
    """Test that Sam3Config has sensible defaults."""
    config = Sam3Config()
    
    assert config.hf_repo == "facebook/sam3"
    assert config.fp16 is True
    assert config.onnx_filename == "sam3_dynamic.onnx"
    assert config.engine_filename == "sam3_fp16.engine"


def test_config_path_resolution():
    """Test that relative paths are resolved to absolute."""
    config = Sam3Config()
    
    assert config.models_dir.is_absolute()
    assert config.onnx_output_dir.is_absolute()


def test_config_from_env(monkeypatch):
    """Test loading config from environment."""
    monkeypatch.setenv("SAM3_HUGGINGFACE_REPO", "custom/sam3")
    monkeypatch.setenv("HF_TOKEN", "test_token")
    
    config = Sam3Config.from_env()
    
    assert config.hf_repo == "custom/sam3"
    assert config.hf_token == "test_token"


def test_config_properties():
    """Test config property accessors."""
    config = Sam3Config()
    
    assert config.onnx_path.name == "sam3_dynamic.onnx"
    assert config.onnx_data_path.name == "sam3_dynamic.onnx.data"
    assert config.engine_path.name == "sam3_fp16.engine"
    assert config.weights_dir.name == "sam3_weights"
```

- [ ] **Step 3: Run tests**

```bash
cd SAM3TensorRT/python
pytest tests/test_config.py -v
```

Expected: All tests pass

- [ ] **Step 4: Commit**

```bash
git add SAM3TensorRT/python/tests/
git commit -m "test(sam3): add basic unit tests for Sam3Config"
```

---

## Verification

### Task 12: Build and test the pipeline

- [ ] **Step 1: Run setup_sam3.py in dry-run mode**

```bash
cd SAM3TensorRT/python
python setup_sam3.py --dry-run
```

Expected: Shows what would be done without executing

- [ ] **Step 2: Run full pipeline (if hardware available)**

```bash
python setup_sam3.py
```

Expected: Downloads, exports, and builds engine

- [ ] **Step 3: Build C++ project**

```bash
cmake --build SAM3TensorRT/cpp/build/cuda --config Release --target sam3
```

Expected: Builds `sam3.exe` with models/ and training/ directories copied

- [ ] **Step 4: Verify output structure**

```bash
ls SAM3TensorRT/cpp/build/cuda/Release/
```

Expected:
```
sam3.exe
models/
training/
results/
*.dll
```

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat(sam3): complete SAM3 setup orchestration implementation"
```

---

## Summary

| Task | Description | Files |
|------|-------------|-------|
| 1 | Create utils.py | `pipeline/utils.py` |
| 2 | Create config.py | `pipeline/config.py` |
| 3 | Create __init__.py (empty) | `pipeline/__init__.py` |
| 4 | Create download.py | `pipeline/download.py` |
| 5 | Create onnx_export.py | `pipeline/onnx_export.py` |
| 6 | Refactor onnxexport.py | `onnxexport.py` |
| 7 | Create engine_build.py | `pipeline/engine_build.py` |
| 8 | Create setup_sam3.py | `setup_sam3.py` |
| 9 | Update CMakeLists.txt | `CMakeLists.txt` |
| 10 | Create requirements.txt | `requirements.txt` |
| 11 | Update .env | `.env` |
| 11.5 | Update __init__.py with exports | `pipeline/__init__.py` |
| 11.6 | Add unit tests | `tests/test_config.py` |
| 12 | Verify | - |

**Deferred to future iteration:**
- Advanced error handling (disk space, retry logic, SHA256 validation)
- Comprehensive unit test coverage for all modules
- Integration tests