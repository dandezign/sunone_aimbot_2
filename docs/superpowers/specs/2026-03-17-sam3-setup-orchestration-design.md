# SAM3 Setup Orchestration Design

**Date**: 2026-03-17
**Status**: Draft
**Author**: AI Agent

## Summary

Create a master Python script (`setup_sam3.py`) that orchestrates the SAM3 model setup pipeline and update `SAM3TensorRT/cpp/CMakeLists.txt` to copy models and training data to the build output.

## Prerequisites

1. **NVIDIA Driver**: 550.x or later (for CUDA 13.1)
2. **Disk Space**: Minimum 15GB free (download ~4GB, ONNX export ~3.2GB, engine build ~varies)
3. **Python**: 3.12+ (matching main project requirements)
4. **GPU**: NVIDIA GPU with compute capability 8.6+

## Scope

### In Scope
1. **Python orchestration script** - Download, ONNX export, weight packing, TensorRT engine build
2. **CMakeLists.txt updates** - Copy models/training to build output, rename executable
3. **Environment configuration** - `.env` file for configurable paths

### Out of Scope
1. DML support for SAM3TensorRT (CUDA only for now)
2. Other models (YOLO26, YOLO26-Seg) - architecture is extensible but implementation deferred
3. Training pipeline execution

## Design

### 1. Directory Structure

```
SAM3TensorRT/python/
├── setup_sam3.py              # CLI entry point
├── pipeline/
│   ├── __init__.py            # Package init, exports main classes
│   ├── config.py              # Config class, .env loading, path resolution
│   ├── download.py            # HuggingFace model download
│   ├── onnx_export.py         # ONNX export + weight packing
│   ├── engine_build.py        # TensorRT engine building via trtexec
│   └── utils.py               # Shared utilities (logging, file ops)
├── onnxexport.py              # Existing (refactored to use pipeline/)
└── tokenize_prompt.py         # Existing (unchanged)
```

**`pipeline/__init__.py` exports:**
```python
from .config import Sam3Config
from .download import Sam3Downloader
from .onnx_export import Sam3Exporter
from .engine_build import Sam3Builder

__all__ = ["Sam3Config", "Sam3Downloader", "Sam3Exporter", "Sam3Builder"]
```

**`pipeline/utils.py` utilities:**
- `setup_logging(level: str = "INFO")` - Configure logging with timestamp format
- `calculate_sha256(file_path: Path) -> str` - Compute file hash for integrity checks
- `ensure_dir(path: Path) -> Path` - Create directory if not exists, return path
- `format_size(bytes: int) -> str` - Human-readable file size (e.g., "3.2 GB")

### 2. Config Module (`pipeline/config.py`)

Centralized configuration management with `.env` loading and path resolution.

```python
@dataclass
class Sam3Config:
    # Paths (loaded from .env or defaults)
    repo_root: Path                    # Auto-detected from script location
    models_dir: Path                   # models/ (relative to repo_root)
    onnx_output_dir: Path              # SAM3TensorRT/python/onnx_weights/
    
    # HuggingFace settings
    hf_token: Optional[str]            # From .env HF_TOKEN
    hf_repo: str = "facebook/sam3"     # From .env SAM3_HUGGINGFACE_REPO
    
    # TensorRT settings
    tensorrt_bin: Optional[Path]       # trtexec path (auto-detected)
    fp16: bool = True                  # Use FP16 precision
    
    # Output settings
    onnx_filename: str = "sam3_dynamic.onnx"
    engine_filename: str = "sam3_fp16.engine"
```

**Key methods:**
- `from_env(cls, interact: bool = False)` - Load from `.env` file
- `resolve_output_path(self, path: Optional[str] = None)` - Handle interactive mode

### 3. Download Module (`pipeline/download.py`)

Downloads SAM3 model from HuggingFace using `huggingface_hub.snapshot_download`.

**Behavior:**
- Downloads to `models/sam3_weights/` (local cache)
- Skips if weights already exist (resume support)
- Uses `HF_TOKEN` from `.env` for private repos

### 4. ONNX Export Module (`pipeline/onnx_export.py`)

Exports SAM3 to ONNX format with weight packing.

**Behavior:**
- Loads model from `models/sam3_weights/`
- Uses `Sam3ONNXWrapper` for clean ONNX graph with explicit outputs
- Packs large weights into `.onnx.data` file (threshold: 500MB)
- Removes temporary files after packing
- Skips if `.onnx` + `.onnx.data` already exist

**Sam3ONNXWrapper class (handles all 4 outputs):**
```python
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
        # Return all 4 outputs as tensors (required for TensorRT inference)
        return (
            outputs.pred_masks.contiguous(),    # [batch, num_queries, H, W]
            outputs.pred_boxes.contiguous(),    # [batch, num_queries, 4] - xyxy normalized
            outputs.pred_logits.contiguous(),   # [batch, num_queries] - confidence
            outputs.semantic_seg.contiguous(),  # [batch, 1, H, W]
        )
```

**torch.onnx.export call:**
```python
torch.onnx.export(
    wrapper,
    (pixel_values, input_ids, attention_mask),
    onnx_path,
    input_names=["pixel_values", "input_ids", "attention_mask"],
    output_names=["pred_masks", "pred_boxes", "pred_logits", "semantic_seg"],
    opset_version=17,
    do_constant_folding=True,
)
```

**Weight packing implementation:**
```python
import onnx
from onnx.external_data_helper import convert_model_to_external_data

# After torch.onnx.export, pack weights
model = onnx.load(str(temp_onnx_path))
convert_model_to_external_data(
    model,
    all_tensors_to_one_file=True,
    location=onnx_filename + ".data",
    size_threshold=500_000_000,  # 500MB threshold
)
onnx.save(model, str(final_onnx_path))
```

**Output:**
- `onnx_weights/sam3_dynamic.onnx` (~39MB)
- `onnx_weights/sam3_dynamic.onnx.data` (~3.2GB)

**ONNX model outputs (used for inference):**
| Output | Shape | Description |
|--------|-------|-------------|
| `pred_masks` | `[1, 200, 288, 288]` | Segmentation masks for each query |
| `pred_boxes` | `[1, 200, 4]` | Bounding boxes (xyxy normalized [0,1]) |
| `pred_logits` | `[1, 200]` | Confidence scores per query |
| `semantic_seg` | `[1, 1, 288, 288]` | Semantic segmentation output |

### 5. Engine Build Module (`pipeline/engine_build.py`)

Converts ONNX to TensorRT engine using `trtexec`.

**Behavior:**
- Auto-detects `trtexec` from standard TensorRT locations
- Uses FP16 precision by default
- Moves engine to `models/sam3_fp16.engine`
- `copy_to()` method for interactive mode copying
- Skips if engine already exists

**trtexec command:**
```bash
trtexec \
    --onnx=sam3_dynamic.onnx \
    --saveEngine=sam3_fp16.engine \
    --minShapes=pixel_values:1x3x1008x1008 \
    --optShapes=pixel_values:1x3x1008x1008 \
    --maxShapes=pixel_values:1x3x1008x1008 \
    --fp16 \
    --memPoolSize=workspace:4096
```

### 6. CLI Entry Point (`setup_sam3.py`)

CLI entry point with argument parsing and orchestration.

**Usage:**
```bash
python setup_sam3.py                    # Run with defaults
python setup_sam3.py --interact         # Interactive mode (prompt for destination)
python setup_sam3.py --force            # Force rebuild all steps
python setup_sam3.py --dry-run          # Show what would be done without executing
python setup_sam3.py --skip-download    # Skip download step
python setup_sam3.py --skip-export      # Skip ONNX export
python setup_sam3.py --skip-build       # Skip engine build
```

**Pipeline flow (3 stages):**

```
┌─────────────┐     ┌─────────────────────┐     ┌─────────────┐
│   Download  │────▶│ ONNX Export + Pack  │────▶│ Build Engine│
└─────────────┘     └─────────────────────┘     └─────────────┘
      │                       │                       │
      ▼                       ▼                       ▼
 models/              SAM3TensorRT/python/      models/
 sam3_weights/        onnx_weights/             sam3_fp16.engine
                      sam3_dynamic.onnx
                      sam3_dynamic.onnx.data
```

**Note:** Weight packing is integrated into the ONNX Export module, not a separate stage.

**Skip flag validation:**
- If `--skip-download` and weights don't exist → Error: "Weights not found. Run without --skip-download first."
- If `--skip-export` and ONNX files don't exist → Error: "ONNX files not found. Run without --skip-export first."
- If `--skip-build` and engine doesn't exist → Error: "Engine not found. Run without --skip-build first."

### 7. CMakeLists.txt Changes

**File:** `SAM3TensorRT/cpp/CMakeLists.txt`

**Changes:**

1. **Rename executable**: `sam3_pcs_app` → `sam3`
2. **Build output directory**: `SAM3TensorRT/cpp/build/cuda/Release/`
3. **Copy models/training to build output**:
   - `models/` → `<build_output>/models/`
   - `training/` → `<build_output>/training/`
   - Create `results/` directory

**Implementation details:**

```cmake
# Source directories (from repo root)
set(SAM3_MODELS_SOURCE "${SAM3_WORKSPACE_ROOT}/models")
set(SAM3_TRAINING_SOURCE "${SAM3_WORKSPACE_ROOT}/sunone_aimbot_2/scripts/training")

# Destination directories (generator expressions for build output)
set(SAM3_BUILD_OUTPUT "$<TARGET_FILE_DIR:sam3>")
set(SAM3_MODELS_DEST "${SAM3_BUILD_OUTPUT}/models")
set(SAM3_TRAINING_DEST "${SAM3_BUILD_OUTPUT}/training")
set(SAM3_RESULTS_DEST "${SAM3_BUILD_OUTPUT}/results")

# Post-build commands
add_custom_command(TARGET sam3 POST_BUILD
    # Create output directories
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SAM3_MODELS_DEST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SAM3_TRAINING_DEST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SAM3_RESULTS_DEST}"
    
    # Copy models only if source exists (guard against missing directory)
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

**Important:** `copy_directory_if_different` will fail if the source directory doesn't exist. Therefore:
1. Run the Python setup script (`setup_sam3.py`) first to create `models/` with `sam3_fp16.engine`
2. Then run CMake build to copy files to build output
3. If `models/` source doesn't exist, CMake will emit a warning but continue (the `make_directory` commands ensure destination dirs are created regardless)

**Expected build output:**
```
SAM3TensorRT/cpp/build/cuda/Release/
├── sam3.exe                    # Renamed from sam3_pcs_app.exe
├── models/                     # Copied from models/ (repo root)
│   ├── sam3_fp16.engine
│   └── presets/
├── training/                   # Copied from sunone_aimbot_2/scripts/training/
│   ├── predefined_classes.txt
│   ├── game.yaml
│   └── datasets/
├── results/                    # Created empty for output
└── *.dll                       # TensorRT, CUDA, OpenCV DLLs
```

### 8. Environment Configuration

**File:** `.env` (at repo root)

**Path convention:** All relative paths in `.env` are resolved relative to `repo_root` (auto-detected from script location).

```env
# HuggingFace Configuration
HF_TOKEN=your_huggingface_token_here

# SAM3 Model Configuration
SAM3_HUGGINGFACE_REPO=facebook/sam3
SAM3_OUTPUT_DIR=models                        # Relative to repo_root
SAM3_DEFAULT_ENGINE_PATH=models/sam3_fp16.engine
SAM3_ONNX_OUTPUT_DIR=SAM3TensorRT/python/onnx_weights  # Relative to repo_root
SAM3_ONNX_FILENAME=sam3_dynamic.onnx

# TensorRT Configuration
TENSORRT_VERSION=10.14.1.48
TRTEXEC_PATH=                                 # Optional: absolute path to override auto-detection

# CUDA Configuration
CUDA_PATH=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1
CUDNN_ROOT=C:/Program Files/NVIDIA/CUDNN/v9.20

# Python Configuration
CLIP_TOKENIZER_MODEL=openai/clip-vit-base-patch32
```

**CLI argument precedence:** CLI arguments override `.env` values. For example, `--force` always rebuilds regardless of existing files.

## Error Handling

1. **Missing HF_TOKEN**: Warn user, may fail for gated models
2. **trtexec not found**: Clear error message with installation instructions
3. **CUDA not available**: Error with GPU/driver requirements
4. **Disk space**: Warn if < 10GB available (model is ~4GB)
5. **Network errors**: Retry logic with exponential backoff (3 retries)
6. **Corrupted ONNX files**: Compare SHA256 hash with HuggingFace-provided hash (if available). If mismatch, delete and re-export. If no hash available, re-export if file size is unexpected.
7. **Partial downloads**: Use `huggingface_hub` resume capability, delete incomplete files on failure
8. **Malformed .env**: Skip malformed lines, warn user with line number
9. **Invalid interactive path**: Re-prompt user with error message until valid absolute path provided
10. **CUDA version mismatch**: Detect PyTorch CUDA vs system CUDA, warn if incompatible

## Testing

1. **Unit tests** for each module in `pipeline/`
2. **Integration test** for full pipeline
3. **Resume test** - verify skip logic works
4. **Interactive mode test** - verify path prompt and copy

## Dependencies

### Python packages:

Create new file `SAM3TensorRT/python/requirements.txt` (not using `scripts/requirements.txt` as it lacks SAM3-specific packages):

```txt
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

# Optional: for development
pytest>=7.0.0
```

**Note:** PyTorch CUDA compatibility:
- System CUDA 13.1 is compatible with PyTorch built for CUDA 12.x (forward compatibility)
- Install PyTorch with CUDA 12.4 support (latest stable):
```bash
pip install torch --index-url https://download.pytorch.org/whl/cu124
```
- The PyTorch CUDA version does NOT need to match system CUDA exactly for inference

### External:
- TensorRT 10.14.1.48 (for `trtexec`)
- CUDA 13.1
- cuDNN 9.20

## Migration Plan

1. Create `pipeline/` package with all modules
2. **Refactor existing `onnxexport.py`**:
   - Move `Sam3ONNXWrapper` class to `pipeline/onnx_export.py`
   - Update `onnxexport.py` to be a thin wrapper that calls `pipeline.onnx_export.Sam3Exporter`
   - Keep backward compatibility for existing users who import `onnxexport.py` directly
3. Create `setup_sam3.py` CLI entry point
4. Update `.env` with new configuration options
5. Update `SAM3TensorRT/cpp/CMakeLists.txt`
6. Test full pipeline end-to-end

## Future Extensions

1. **YOLO26 support**: Add `pipeline/yolo26_download.py`, `pipeline/yolo26_export.py`
2. **DML support**: Add DML build configuration to CMakeLists.txt
3. **Model versioning**: Support multiple SAM3 versions
4. **Auto-update**: Check for newer model versions on HuggingFace