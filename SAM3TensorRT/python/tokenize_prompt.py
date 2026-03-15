#!/usr/bin/env python3
"""Tokenize SAM3 text prompts into CLIP input_ids and attention_mask CSV lines.

Output format:
  IDS:<comma-separated-int64>
  MASK:<comma-separated-int64>
"""

import argparse
import sys

from transformers import CLIPTokenizer


DEFAULT_MODEL = "openai/clip-vit-base-patch32"


def main() -> int:
    parser = argparse.ArgumentParser(description="Tokenize a SAM3 prompt with CLIP tokenizer")
    parser.add_argument("--text", required=True, help="Prompt text")
    parser.add_argument("--max-length", type=int, default=32, help="Token sequence length")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Tokenizer model name")
    args = parser.parse_args()

    if args.max_length <= 0:
        print("max-length must be > 0", file=sys.stderr)
        return 2

    try:
        tokenizer = CLIPTokenizer.from_pretrained(args.model)
        encoded = tokenizer(
            args.text,
            padding="max_length",
            truncation=True,
            max_length=args.max_length,
            return_attention_mask=True,
        )
    except Exception as exc:  # pragma: no cover - runtime path
        print(f"Tokenizer error: {exc}", file=sys.stderr)
        return 3

    input_ids = encoded.get("input_ids", [])
    attention_mask = encoded.get("attention_mask", [])

    if len(input_ids) != args.max_length or len(attention_mask) != args.max_length:
        print("Tokenizer output length mismatch", file=sys.stderr)
        return 4

    print("IDS:" + ",".join(str(int(v)) for v in input_ids))
    print("MASK:" + ",".join(str(int(v)) for v in attention_mask))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
