# Windows SAM3 Prompt Test Guide

This guide uses the built executable:

- `SAM3TensorRT/cpp/build-win-cuda/Release/sam3_pcs_app.exe`

And these test assets:

- `SAM3TensorRT/demo`
- `models/sam3_fp16.engine`

Tokenizer helper used for free-text prompts:

- `SAM3TensorRT/python/tokenize_prompt.py`

## 0) One-Time Prompt Tokenizer Setup

Set tokenizer runtime variables in PowerShell (recommended):

```powershell
$env:SAM3_TOKENIZER_PYTHON="I:\CppProjects\sunone_aimbot_2\.venv\Scripts\python.exe"
$env:SAM3_TOKENIZER_SCRIPT="I:\CppProjects\sunone_aimbot_2\SAM3TensorRT\python\tokenize_prompt.py"
```

Notes:

- First use downloads CLIP tokenizer files from Hugging Face.
- Keep internet enabled for first run, then it uses local cache.

## 1) Benchmark Test: Free-Text Prompt (Any Text)

```powershell
& "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/cpp/build-win-cuda/Release/sam3_pcs_app.exe" "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/demo" "I:/CppProjects/sunone_aimbot_2/models/sam3_fp16.engine" 1 prompt=cat
```

Multi-word example:

```powershell
& "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/cpp/build-win-cuda/Release/sam3_pcs_app.exe" "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/demo" "I:/CppProjects/sunone_aimbot_2/models/sam3_fp16.engine" 1 "prompt=red backpack"
```

Expected:

- Prints TensorRT load/binding logs
- Exit code `0`

## 2) Benchmark Test: Default Person Prompt

Run from anywhere:

```powershell
& "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/cpp/build-win-cuda/Release/sam3_pcs_app.exe" "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/demo" "I:/CppProjects/sunone_aimbot_2/models/sam3_fp16.engine" 1 prompt=person
```

Expected:

- Prints TensorRT load/binding logs
- Ends with exit code `0`

## 3) Benchmark Test: Explicit Token IDs Prompt

```powershell
& "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/cpp/build-win-cuda/Release/sam3_pcs_app.exe" "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/demo" "I:/CppProjects/sunone_aimbot_2/models/sam3_fp16.engine" 1 ids:49406,2533,49407
```

Expected:

- Same successful startup/inference path
- Exit code `0`

## 4) Non-Benchmark Test: Write Output Images

Run from the executable folder so output goes to local `results/`:

```powershell
Set-Location "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/cpp/build-win-cuda/Release"
./sam3_pcs_app.exe "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/demo" "I:/CppProjects/sunone_aimbot_2/models/sam3_fp16.engine" 0 prompt=person
```

Check generated files:

```powershell
Get-ChildItem "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/cpp/build-win-cuda/Release/results" -File
```

Expected outputs include:

- `instance_box.jpeg`
- `semantic_puppies.png`

## 5) Negative Test: Unsupported Prompt

```powershell
./sam3_pcs_app.exe "I:/CppProjects/sunone_aimbot_2/SAM3TensorRT/demo" "I:/CppProjects/sunone_aimbot_2/models/sam3_fp16.engine" 1 prompt=cat
```

Expected (if tokenizer helper is available):

- Command should succeed (exit `0`) because free-text prompting is enabled.

Expected (if tokenizer helper is missing/not configured):

- `Unsupported prompt argument: prompt=cat`
- `Supported values: prompt=<any text> or ids:<csv_of_token_ids>`
- `Hint: set SAM3_TOKENIZER_PYTHON and SAM3_TOKENIZER_SCRIPT...`
- Exit code `1`

## 6) Quick Exit Code Check

After any run:

```powershell
$LASTEXITCODE
```

Interpretation:

- `0` = success
- `1` = prompt validation failure (intentional for negative test)
- `-1073741515` = missing runtime DLL(s)
