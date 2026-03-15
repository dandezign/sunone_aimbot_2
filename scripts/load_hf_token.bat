@echo off
REM Load HuggingFace token from .env file and set environment variable
REM Usage: load_hf_token.bat

setlocal enabledelayedexpansion

REM Check if .env file exists
if not exist ".env" (
    echo [ERR] .env file not found!
    echo Create .env file with your HF_TOKEN
    echo Example: HF_TOKEN=hf_xxxxx
    exit /b 1
)

REM Read HF_TOKEN from .env file
for /f "tokens=1,2 delims==" %%a in (.env) do (
    if "%%a"=="HF_TOKEN" (
        set "HF_TOKEN=%%b"
        setx HF_TOKEN "%%b" >nul
        echo [OK] HF_TOKEN loaded and set
        goto :eof
    )
)

echo [ERR] HF_TOKEN not found in .env file
exit /b 1
