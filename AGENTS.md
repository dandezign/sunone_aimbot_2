# AGENTS.md

## Scope

These instructions apply to the whole repository.

This is a Windows-only C++17 project built with CMake and Visual Studio 18 2026. The repo also contains Python tooling under `scripts/`.

## Repo facts

- Main app target: `ai`
- Main source tree: `sunone_aimbot_2/`
- Main build file: `CMakeLists.txt`
- C++ tests are standalone executables, not a `ctest` suite
- DML output: `build/dml/Release/ai.exe`
- CUDA output: `build/cuda/Release/ai.exe`
- The root `CMakeLists.txt` hard-requires generator `Visual Studio 18 2026`
- CUDA builds also require `-T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1"`

## Cursor / Copilot rules

No repo-specific rule files were found:

- no `.cursorrules`
- no `.cursor/rules/`
- no `.github/copilot-instructions.md`

Use this file as the repo-local agent guide.

## Agent operating mode

Use retrieval-led reasoning over pre-training-led reasoning for repo-specific work.

- Do not rely on generic CMake, CUDA, OpenCV, TensorRT, ImGui, or project-memory assumptions when the repo can answer the question directly.
- Before writing code, first explore the affected project area, then read the relevant local files and docs, then implement.
- Treat this `AGENTS.md` as persistent repo context for broad guidance.
- Use skills and external knowledge for specialized workflows, but use local repo files as the source of truth for project behavior.
- When guidance in memory conflicts with local code or docs, follow the local repo.

## Retrieval map for common tasks

Use these files first instead of guessing:

- Build / generator / dependencies: `CMakeLists.txt`, `docs/build.md`, `docs/cuda-build-run.md`
- Runtime config keys and defaults: `sunone_aimbot_2/config/config.cpp`, `sunone_aimbot_2/config/config.h`, `docs/config.md`
- Detection debug behavior: `tests/detection_debug_tools_test.cpp`, `sunone_aimbot_2/debug/`
- Training labeling behavior: `tests/training_labeling_tests.cpp`, `sunone_aimbot_2/training/`
- Preferred helper scripts: `.opencode/plugins/tool-search/scripts/Build-Cuda.ps1`, `.opencode/plugins/tool-search/scripts/Build-Dml.ps1`, `.opencode/plugins/tool-search/scripts/Check-Cpp.ps1`
- Python tooling: `scripts/README.md`, `scripts/requirements.txt`, `scripts/labelImg/tests/`

If a task touches one of these areas, read the relevant file(s) before changing code.

## Preferred build commands

Prefer the repo helper scripts when possible.

```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1 -Parallel
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1 -Rebuild

powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Dml.ps1
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Dml.ps1 -Parallel
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Dml.ps1 -Rebuild
```

## Manual configure/build commands

### DML

```powershell
powershell -ExecutionPolicy Bypass -File tools/setup_opencv_dml.ps1
cmake -S . -B build/dml -G "Visual Studio 18 2026" -A x64 -DAIMBOT_USE_CUDA=OFF
cmake --build build/dml --config Release
```

### CUDA

```powershell
cmake -S . -B build/cuda -G "Visual Studio 18 2026" -A x64 `
  -T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1" `
  -DAIMBOT_USE_CUDA=ON `
  -DCMAKE_CUDA_FLAGS="--allow-unsupported-compiler" `
  -DCUDA_NVCC_FLAGS="--allow-unsupported-compiler"
cmake --build build/cuda --config Release
```

## Important targets

- `ai`
- `detection_debug_tools_tests`
- `training_labeling_tests`

## Build and run a single C++ test target

There is no registered single-test `ctest` workflow here. The smallest practical C++ test unit is a single test executable.

### Build one test target

```powershell
cmake --build build/dml --config Release --target detection_debug_tools_tests
cmake --build build/dml --config Release --target training_labeling_tests

cmake --build build/cuda --config Release --target detection_debug_tools_tests
cmake --build build/cuda --config Release --target training_labeling_tests
```

### Run one test target

```powershell
.\build\dml\Release\detection_debug_tools_tests.exe
.\build\dml\Release\training_labeling_tests.exe

.\build\cuda\Release\detection_debug_tools_tests.exe
.\build\cuda\Release\training_labeling_tests.exe
```

### Best single-test workflow

```powershell
cmake --build build/cuda --config Release --target training_labeling_tests
.\build\cuda\Release\training_labeling_tests.exe
```

If you only changed detection debug code, run `detection_debug_tools_tests` instead.

## App build commands

```powershell
cmake --build build/dml --config Release --target ai
cmake --build build/cuda --config Release --target ai
```

## Python tooling commands

### Setup

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r scripts/requirements.txt
```

### Format / lint / test

```powershell
python -m black scripts
python -m flake8 scripts

python -m pytest scripts/labelImg/tests -q
python -m pytest scripts/labelImg/tests/test_utils.py -q
python -m pytest scripts/labelImg/tests/test_utils.py::TestUtils::test_nautalSort_noError -q
```

## Lint / static analysis notes

- There is no checked-in repo-wide C++ formatter or clang-tidy config.
- For C++, successful compilation is the first gate.
- The repo also provides a helper static-analysis script:

```powershell
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Check-Cpp.ps1
```

- For Python, `black`, `flake8`, and `pytest` are listed in `scripts/requirements.txt`.

## Code style: general

- Follow the existing local style of the file you edit.
- Keep diffs small and task-focused.
- Do not perform drive-by reformatting.
- Preserve Windows-specific behavior unless the task explicitly changes it.
- Prefer focused edits over broad refactors.

## Code style: C++

### Structure

- Target standard is C++17.
- Use `#pragma once` in headers.
- Prefer small helpers in anonymous namespaces for file-local logic.
- Use named namespaces such as `training` and `detection_debug` for module code.
- Prefer `constexpr`, `const`, `std::optional`, and `std::filesystem` where they fit.

### Includes

- In `.cpp` files, include the matching project header first when there is one.
- Then include other project headers.
- Then include standard library or third-party headers.
- Use project-root-relative includes like `#include "sunone_aimbot_2/debug/detection_debug_export.h"`.
- Preserve existing Windows header guards such as `WIN32_LEAN_AND_MEAN` and `_WINSOCKAPI_`.

### Naming

This repo is not fully uniform. Match the surrounding file.

- Types, structs, enums, and many public helpers use `PascalCase`
- Some legacy methods and config members use `lowerCamelCase`
- Some private members use trailing underscore, e.g. `state_`
- Persisted config keys are commonly `snake_case`

Do not rename unrelated identifiers only for style consistency.

### Formatting and types

- Use 4-space indentation.
- Keep braces and whitespace consistent with the current file.
- Prefer early returns for simple guard cases.
- Be explicit at API boundaries.
- Use `auto` when the type is obvious from the initializer.
- Prefer `const T&` for non-trivial read-only parameters.

### Error handling and concurrency

- Follow the local module pattern.
- In tests, this repo commonly uses `Check(...)` helpers that throw `std::runtime_error`, with `main()` catching and printing a clear failure message.
- In runtime code, prefer explicit status returns and guard checks over exception-driven control flow.
- Do not silently swallow errors.
- Guard shared mutable state with `std::mutex`, `std::lock_guard`, or `std::unique_lock`.
- Keep locked regions short.

## Code style: tests

- Existing C++ tests are executable-style tests with `main()`.
- Add focused assertions close to the behavior under test.
- Prefer deterministic tests over timing-heavy tests.
- Keep timeouts short when waiting is unavoidable.

## Code style: Python

- Use `black` formatting.
- Prefer `pathlib.Path` for filesystem paths.
- Use `argparse` for CLI scripts.
- Follow the style already present in the Python subproject you edit; `scripts/labelImg/` is older than the export scripts.

## Verification guidance

- Start with the narrowest relevant build or test target.
- If shared code changed, validate both DML and CUDA when practical.
- For CUDA work, prefer the repo helper script over inventing new flags.
- For DML work, remember `tools/setup_opencv_dml.ps1` may be required on a fresh checkout.

## What not to do

- Do not switch away from generator `Visual Studio 18 2026`.
- Do not assume `ctest` works here without first adding CTest registration.
- Do not add mass renames or repo-wide formatting unless explicitly requested.
- Do not validate only one backend when the changed code is shared by both.
