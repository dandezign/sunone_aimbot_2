#!/usr/bin/env python3
"""Build SAM3 preset JSON files from class names.

Wraps SAM3TensorRT/python/tokenize_prompt.py for batch processing.
"""

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

TOKENIZER_SCRIPT = (
    Path(__file__).parent.parent / "SAM3TensorRT" / "python" / "tokenize_prompt.py"
)
DEFAULT_MAX_LENGTH = 32


def tokenize_with_sam3_reference(
    text: str, max_length: int = DEFAULT_MAX_LENGTH
) -> Optional[dict]:
    """Call SAM3TensorRT tokenizer and parse IDS:/MASK: output.

    Reuses the exact same tokenizer as the C++ reference implementation.
    """
    result = subprocess.run(
        [
            sys.executable,
            str(TOKENIZER_SCRIPT),
            "--text",
            text,
            "--max-length",
            str(max_length),
        ],
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        print(f"Tokenizer error: {result.stderr}", file=sys.stderr)
        return None

    input_ids = []
    attention_mask = []

    for line in result.stdout.strip().split("\n"):
        if line.startswith("IDS:"):
            input_ids = [int(v) for v in line[4:].split(",")]
        elif line.startswith("MASK:"):
            attention_mask = [int(v) for v in line[5:].split(",")]

    if not input_ids or not attention_mask:
        print(f"Failed to parse tokenizer output for '{text}'", file=sys.stderr)
        return None

    return {
        "ids": input_ids,
        "mask": attention_mask,
    }


def build_presets(class_names: list[str], max_length: int) -> dict:
    """Build preset dict from class names using SAM3TensorRT tokenizer."""
    presets = {}
    for name in class_names:
        print(f"Tokenizing: '{name}'")
        tokens = tokenize_with_sam3_reference(name, max_length)
        if tokens:
            presets[name] = tokens
    return presets


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build SAM3 preset JSON files from class names"
    )
    parser.add_argument(
        "--class",
        action="append",
        dest="classes",
        required=True,
        help="Class name to tokenize (can be specified multiple times)",
    )
    parser.add_argument(
        "--max-length",
        type=int,
        default=DEFAULT_MAX_LENGTH,
        help=f"Token sequence length (default: {DEFAULT_MAX_LENGTH})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Output JSON file path",
    )

    args = parser.parse_args()

    if not TOKENIZER_SCRIPT.exists():
        print(f"Tokenizer script not found: {TOKENIZER_SCRIPT}", file=sys.stderr)
        return 1

    print(f"Tokenizing {len(args.classes)} class(es) using SAM3TensorRT tokenizer...")

    presets = build_presets(args.classes, args.max_length)

    if not presets:
        print("Failed to tokenize any classes", file=sys.stderr)
        return 2

    output_data = {
        "version": 1,
        "created_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "source": "build_sam3_presets.py (SAM3TensorRT tokenizer)",
        "presets": presets,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)

    with open(args.output, "w") as f:
        json.dump(output_data, f, indent=2)

    print(f"Created {args.output} with {len(presets)} preset(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
