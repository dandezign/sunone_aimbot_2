# SAM3TensorRT/python/pipeline/onnx_export.py
"""ONNX export and weight packing for SAM3."""

import logging
from pathlib import Path
from typing import Tuple

import onnx
import torch
from onnx.external_data_helper import convert_model_to_external_data
from PIL import Image

from .config import Sam3Config
from .utils import ensure_dir, format_size

logger = logging.getLogger("sam3")


class Sam3ONNXWrapper(torch.nn.Module):
    """Wrapper for SAM3 model with explicit output handling.

    This ensures all outputs are properly traced during ONNX export.
    """

    def __init__(self, sam3):
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
        from transformers import Sam3Model, Sam3Processor

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
        data_size = (
            self.onnx_data_path.stat().st_size if self.onnx_data_path.exists() else 0
        )
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
