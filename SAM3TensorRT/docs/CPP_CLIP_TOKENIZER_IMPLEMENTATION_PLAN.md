# C++ CLIP Tokenizer Integration Plan (SAM3)

## Goal

Replace or supplement the current Python-based prompt tokenization flow with a native C++ tokenizer path for SAM3 free-text prompts.

Current behavior:

- `sam3_pcs_app` supports:
  - `prompt=<text>` via Python helper script
  - `ids:<csv>` direct token input
- Python helper script: `SAM3TensorRT/python/tokenize_prompt.py`

Target behavior:

- `sam3_pcs_app` can tokenize prompt text natively in C++ without requiring Python at runtime.

## Research Summary

### Candidate libraries reviewed

1. `mlc-ai/tokenizers-cpp`
- URL: https://github.com/mlc-ai/tokenizers-cpp
- Status: Strong candidate
- Why:
  - Cross-platform C++ API
  - Explicit CMake integration path
  - Can load tokenizer from Hugging Face tokenizer JSON and encode text to ids
  - Good fit for CLIP-compatible tokenizer workflows

2. `huggingface/tokenizers`
- URL: https://github.com/huggingface/tokenizers
- Status: Core engine, but not the best direct integration target here
- Why:
  - Rust-first repo with various language bindings
  - More integration overhead for direct C++ app consumption in this project

3. `openai/CLIP`
- URL: https://github.com/openai/CLIP
- Status: Reference only
- Why:
  - Useful tokenizer behavior reference
  - Primarily Python; not a direct C++ tokenizer library for deployment

4. `ggml-org/llama.cpp`
- URL: https://github.com/ggml-org/llama.cpp
- Status: Not relevant for this specific CLIP tokenizer requirement

### Recommended path

Use `mlc-ai/tokenizers-cpp` as the native tokenizer backend.

## Tokenizer Artifacts and Data

Expected CLIP tokenizer model ID:

- `openai/clip-vit-base-patch32`

Current downloaded cache location on this machine (Python path already validated):

- `C:/Users/dande/.cache/huggingface/hub/models--openai--clip-vit-base-patch32`

Tokenizer files in snapshot folder include:

- `tokenizer.json`
- `vocab.json`
- `merges.txt`
- `special_tokens_map.json`
- `tokenizer_config.json`

For C++ integration, prefer `tokenizer.json` as primary artifact if supported by selected C++ tokenizer API.

## Proposed Integration Design

### Runtime strategy

Implement a two-tier tokenizer flow in `sam3_pcs_app`:

1. Primary: native C++ tokenizer path
2. Fallback: existing Python helper path
3. Last-resort: `ids:<csv>` manual token input

This avoids regressions while enabling gradual migration.

### Prompt handling rules (must preserve)

- Max sequence length: 32
- Input tensors required by model:
  - `input_ids` length 32
  - `attention_mask` length 32
- Maintain compatibility with current SAM3 TensorRT engine expectations.

## Step-by-Step Implementation Plan

1. Vendor dependency
- Add `tokenizers-cpp` as submodule under `SAM3TensorRT/cpp/third_party/tokenizers-cpp`
- Pin to a known commit/tag for reproducibility.

2. CMake integration
- Update `SAM3TensorRT/cpp/CMakeLists.txt`:
  - Add subdirectory for tokenizer dependency
  - Link tokenizer target(s) to `sam3_pcs_app`
  - Add include directories as needed
- Keep build optional behind a feature flag first:
  - Example: `SAM3_USE_CPP_TOKENIZER` default `ON` on Windows once validated

3. Implement C++ tokenizer wrapper
- Add new source files:
  - `SAM3TensorRT/cpp/include/sam3_tokenizer.hpp`
  - `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_tokenizer.cpp`
- Responsibilities:
  - Load tokenizer blob/json
  - Encode prompt text
  - Build fixed-length `input_ids` and `attention_mask`

4. Wire into `sam3_pcs_app`
- Replace current direct prompt tokenization branching with:
  - Try C++ tokenizer
  - If failure and Python helper available, fallback to Python helper
  - Keep `ids:<csv>` path untouched

5. Tokenizer file discovery
- Add deterministic search order:
  1. `SAM3_TOKENIZER_JSON` env var path
  2. Project-local known path under `SAM3TensorRT`
  3. Hugging Face cache path
- Surface clear errors when tokenizer file cannot be found.

6. Logging and diagnostics
- Add concise startup messages:
  - `Tokenizer backend: cpp|python|manual`
  - Tokenizer file path in use
- Keep verbose diagnostics optional behind env flag.

7. Testing
- Validate prompts:
  - `prompt=person`
  - `prompt=cat`
  - `prompt=red backpack`
  - `ids:49406,2533,49407`
- Validate both benchmark and non-benchmark output modes.
- Confirm no behavior regression in result generation.

8. Documentation
- Update `SAM3TensorRT/WINDOWS_SAM3_PROMPT_TEST_GUIDE.md`
- Add a new section for native tokenizer enablement and fallback behavior.

## Risks and Mitigations

1. Token ID mismatch vs Python transformers
- Risk: subtle tokenization differences cause segmentation quality changes
- Mitigation: compare token IDs and masks from C++ path vs Python path for a prompt matrix.

2. Build complexity on Windows
- Risk: additional native dependency can complicate toolchain setup
- Mitigation: keep Python fallback path and make C++ tokenizer feature-gated initially.

3. Runtime artifact management
- Risk: tokenizer model files not found at runtime
- Mitigation: explicit env var override + clear error messaging + documented default paths.

## Acceptance Criteria

1. `prompt=<any text>` succeeds with C++ tokenizer backend and exit code `0`.
2. For tested prompts, token sequences match Python helper output (or are validated equivalent for model behavior).
3. `ids:<csv>` path remains fully functional.
4. Windows build remains reproducible with current CUDA/TensorRT/OpenCV setup.
5. Documentation updated with commands and fallback behavior.

## Deferred Work

- Remove Python fallback entirely after confidence period.
- Add unit tests that assert tokenization parity for a fixed prompt set.
- Add offline tokenizer bundle option in repository for fully air-gapped deployments.
