# SAM3 Person Preset - Phase 1 Design

**Date:** 2026-03-14  
**Status:** Design Complete  
**Priority:** High  

## Goal

Implement a **person preset** for SAM3 prompt encoding to enable immediate working detection for the "person" class, fixing the current broken hash-based tokenizer that generates invalid CLIP token IDs.

## Problem Statement

The current `EncodeSam3Prompt()` function in `training_sam3_labeler.h` uses a hash-based fake tokenizer:

```cpp
inline int64_t HashSam3PromptWord(const std::string& word) {
    // FNV-1a hash - generates arbitrary IDs, not real CLIP tokens!
    return kTokenHashBase + static_cast<int64_t>(hash % kTokenHashRange);
}
```

This produces token IDs that do not exist in CLIP's vocabulary. The SAM3 TensorRT engine expects real CLIP BPE token IDs.

From the reference implementation (`SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp`):
```cpp
// tokenized version of 'person' - these are REAL CLIP token IDs
std::vector<int64_t> iid={49406, 2533, 49407, 49407, 49407, ...};
std::vector<int64_t> iam={1, 1, 1, 0, 0, 0, ...};
```

Where:
- `49406` = `<|startoftext|>` (BOS)
- `2533` = "person" token ID
- `49407` = `<|endoftext|>` (EOS/PAD)

## Solution: Person Preset Lookup

Replace hash-based encoding with a lookup table containing pre-computed CLIP token sequences for common classes.

### Preset Data Structure

```cpp
struct Sam3PromptPreset {
    std::string name;
    std::vector<int64_t> inputIds;
    std::vector<int64_t> attentionMask;
};
```

### Initial Preset: "person"

Based on the reference implementation and CLIP tokenization:

| Token Position | Token ID | Meaning |
|---------------|----------|---------|
| 0 | 49406 | `<\|startoftext\|>` (BOS) |
| 1 | 2533 | "person" |
| 2-31 | 49407 | `<\|endoftext\|>` (PAD) |

Attention mask: `[1, 1, 1, 0, 0, ...]` (1 for real tokens, 0 for padding)

### Fallback Strategy

For prompts without a preset:
1. Check if prompt matches a preset name (case-insensitive)
2. If match found: use preset token sequence
3. If no match: fall back to hash-based encoding (for backward compatibility) OR return error

**Decision**: Use hash fallback initially to avoid breaking existing code, log warning for debugging.

## Architecture

```
trainings_sam3_labeler.h
├── Sam3PromptPreset (struct)
├── GetSam3PromptPreset(name) → returns preset or null
├── EncodeSam3PromptV2(prompt, tokenCount, outIds, outMask)
│   ├── Look up preset by name
│   ├── If found: copy preset tokens
│   └── If not found: fall back to hash-based encoding
└── (deprecated) EncodeSam3Prompt() - keep for compatibility
```

## Files Modified

1. **sunone_aimbot_2/training/training_sam3_labeler.h**
   - Add `Sam3PromptPreset` struct
   - Add preset lookup function
   - Modify `EncodeSam3Prompt()` to use presets
   - Add person preset data

2. **sunone_aimbot_2/training/training_sam3_labeler_trt.cpp**
   - No changes required (uses the header function)

## Testing Strategy

### Unit Tests
- Test preset lookup for "person" returns correct token IDs
- Test preset lookup for "PERSON" (case-insensitive) works
- Test unknown prompt falls back to hash encoding
- Test token count parameter is respected

### Integration Tests
- Build CUDA version
- Run training_labeling_tests.exe
- Verify SAM3 labeler initializes correctly
- (Manual) Test person detection in Label mode

## Success Criteria

1. ✅ "person" preset returns token IDs: `[49406, 2533, 49407, 49407, ...]`
2. ✅ Unit tests pass
3. ✅ Build succeeds
4. ✅ SAM3 labeler initializes without errors
5. ✅ (Manual) Person detection works in Label mode

## Future Work (Phase 2)

- Implement real CLIP BPE tokenizer
- Add more presets (cat, dog, car, etc.)
- Load presets from configuration file
- Remove hash-based fallback

## References

1. SAM3 Reference Implementation: `SAM3TensorRT/cpp/src/sam3/sam3_apps/sam3_pcs_app.cpp`
2. CLIP Tokenization: https://huggingface.co/docs/transformers/model_doc/clip
3. OpenAI CLIP: https://github.com/openai/CLIP
