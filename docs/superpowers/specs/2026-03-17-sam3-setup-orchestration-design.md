# SAM3 Setup Orchestration Design

**Date**: 2026-03-17
**Status**: Draft
**Author**: AI Agent

## Summary

Create a master Python script (`setup_sam3.py`) that orchestrates the SAM3 model setup pipeline and update `SAM3TensorRT/cpp/CMakeLists.txt` to copy models and training data to the build output.

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

### 2. Config Module (`pipeline/config.py`)

Centralized configuration management with `.env` loading and path resolution.

```python
@dataclass
class Sam3Config:
    # Paths (loaded from .env or defaults)
    repo_root: Path                    # Auto-detected from script location
    models_dir: Path                   # sunone_aimbot_2/models/
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
- Uses `Sam3ONNXWrapper` for clean ONNX graph
- Packs large weights into `.onnx.data` file (threshold: 500MB)
- Removes temporary files after packing
- Skips if `.onnx` + `.onnx.data` already exist

**Output:**
- `onnx_weights/sam3_dynamic.onnx` (~39MB)
- `onnx_weights/sam3_dynamic.onnx.data` (~3.2GB)

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
python setup_sam3.py --interact         # Interactive mode
python setup_sam3.py --force            # Force rebuild all steps
python setup_sam3.py --skip-download    # Skip download step
python setup_sam3.py --skip-export      # Skip ONNX export
python setup_sam3.py --skip-build       # Skip engine build
```

**Pipeline flow:**

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Download  │────▶│  ONNX Export │────▶│ Weight Pack │────▶│ Build Engine│
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
      │                   │                   │                   │
      ▼                   ▼                   ▼                   ▼
 models/            onnx_weights/        onnx_weights/      models/
 sam3_weights/      sam3_dynamic.onnx    sam3_dynamic.      sam3_fp16.engine
                    (temp)               onnx.data
```

### 7. CMakeLists.txt Changes

**File:** `SAM3TensorRT/cpp/CMakeLists.txt`

**Changes:**

1. **Rename executable**: `sam3_pcs_app` → `sam3`
2. **Build output directory**: `SAM3TensorRT/cpp/build/cuda/Release/`
3. **Copy models/training to build output**:
   - `models/` → `<build_output>/models/`
   - `training/` → `<build_output>/training/`
   - Create `results/` directory

**Expected build output:**
```
SAM3TensorRT/cpp/build/cuda/Release/
├── sam3.exe                    # Renamed from sam3_pcs_app.exe
├── models/                     # Copied from sunone_aimbot_2/models/
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

```env
# HuggingFace Configuration
HF_TOKEN=your_huggingface_token_here

# SAM3 Model Configuration
SAM3_HUGGINGFACE_REPO=facebook/sam3
SAM3_OUTPUT_DIR=models
SAM3_DEFAULT_ENGINE_PATH=models/sam3_fp16.engine
SAM3_ONNX_OUTPUT_DIR=onnx_weights
SAM3_ONNX_FILENAME=sam3_dynamic.onnx

# TensorRT Configuration
TENSORRT_VERSION=10.14.1.48
TRTEXEC_PATH=                        # Optional: override auto-detection

# CUDA Configuration
CUDA_PATH=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1
CUDNN_ROOT=C:/Program Files/NVIDIA/CUDNN/v9.20

# Python Configuration
CLIP_TOKENIZER_MODEL=openai/clip-vit-base-patch32
```

## Error Handling

1. **Missing HF_TOKEN**: Warn user, may fail for gated models
2. **trtexec not found**: Clear error message with installation instructions
3. **CUDA not available**: Error with GPU/driver requirements
4. **Disk space**: Warn if < 10GB available (model is ~4GB)
5. **Network errors**: Retry logic with exponential backoff

## Testing

1. **Unit tests** for each module in `pipeline/`
2. **Integration test** for full pipeline
3. **Resume test** - verify skip logic works
4. **Interactive mode test** - verify path prompt and copy

## Dependencies

### Python packages (from `scripts/requirements.txt`):
- `torch`
- `transformers`
- `huggingface_hub`
- `onnx`
- `python-dotenv`

### External:
- TensorRT 10.x (for `trtexec`)
- CUDA 13.1
- cuDNN 9.20

## Migration Plan

1. Create `pipeline/` package with all modules
2. Refactor existing `onnxexport.py` to use `pipeline/onnx_export.py`
3. Create `setup_sam3.py` CLI entry point
4. Update `.env` with new configuration options
5. Update `SAM3TensorRT/cpp/CMakeLists.txt`
6. Test full pipeline end-to-end

## Future Extensions

1. **YOLO26 support**: Add `pipeline/yolo26_download.py`, `pipeline/yolo26_export.py`
2. **DML support**: Add DML build configuration to CMakeLists.txt
3. **Model versioning**: Support multiple SAM3 versions
4. **Auto-update**: Check for newer model versions on HuggingFace