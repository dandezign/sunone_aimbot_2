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


def test_config_resolve_output_path():
    """Test resolve_output_path method."""
    config = Sam3Config()

    # Without path argument, returns engine_path
    default_path = config.resolve_output_path()
    assert default_path == config.engine_path

    # With absolute path, returns that path resolved
    custom_path = config.resolve_output_path("/custom/path/engine.engine")
    assert custom_path == Path("/custom/path/engine.engine").resolve()
