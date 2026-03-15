# SAM3 Person Preset Phase 1 - Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement person preset for SAM3 prompt encoding to enable working "person" detection in Label mode.

**Architecture:** Add a lookup table with pre-computed CLIP token sequences for common classes, starting with "person". Modify `EncodeSam3Prompt()` to check presets first, then fall back to hash-based encoding.

**Tech Stack:** C++, CLIP BPE tokenization constants, existing SAM3 TensorRT infrastructure

---

## File Structure

| File | Purpose |
|------|---------|
| `sunone_aimbot_2/training/training_sam3_labeler.h` | Header with preset data structures and encoding function |
| `tests/training_labeling_tests.cpp` | Unit tests for preset encoding |

---

## Chunk 1: Add Person Preset to Header

### Task 1.1: Add Preset Data Structure

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`

- [ ] **Step 1: Add Sam3PromptPreset struct after existing includes**

```cpp
namespace training {

// Preset prompt with pre-computed CLIP token sequences
struct Sam3PromptPreset {
    const char* name;
    std::vector<int64_t> inputIds;
    std::vector<int64_t> attentionMask;
};

}  // namespace training
```

- [ ] **Step 2: Add preset lookup function declaration in detail namespace**

Add after the existing `EncodeSam3Prompt` function declaration:
```cpp
namespace detail {

// Get preset for common prompts (e.g., "person")
// Returns nullptr if no preset exists for the given prompt
const Sam3PromptPreset* GetSam3PromptPreset(const std::string& prompt);

}  // namespace detail
```

### Task 1.2: Add Person Preset Data and Lookup Function

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`

- [ ] **Step 1: Add person preset data before the GetSam3PromptPreset function**

```cpp
namespace detail {

// Pre-computed CLIP token sequences for common prompts
// Token IDs from OpenAI CLIP BPE vocabulary
// 49406 = <|startoftext|> (BOS)
// 49407 = <|endoftext|> (EOS/PAD)
// 2533  = "person"

inline const Sam3PromptPreset kPersonPreset = {
    "person",
    // input_ids: BOS, "person", EOS, PAD...
    {49406, 2533, 49407, 49407, 49407, 49407, 49407, 49407,
     49407, 49407, 49407, 49407, 49407, 49407, 49407, 49407,
     49407, 49407, 49407, 49407, 49407, 49407, 49407, 49407,
     49407, 49407, 49407, 49407, 49407, 49407, 49407, 49407},
    // attention_mask: 1 for real tokens, 0 for padding
    {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// Case-insensitive string comparison
inline bool CaseInsensitiveEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline const Sam3PromptPreset* GetSam3PromptPreset(const std::string& prompt) {
    if (CaseInsensitiveEqual(prompt, "person") ||
        CaseInsensitiveEqual(prompt, "a person") ||
        CaseInsensitiveEqual(prompt, "photo of a person") ||
        CaseInsensitiveEqual(prompt, "a photo of a person")) {
        return &kPersonPreset;
    }
    return nullptr;
}

}  // namespace detail
```

### Task 1.3: Modify EncodeSam3Prompt to Use Presets

**Files:**
- Modify: `sunone_aimbot_2/training/training_sam3_labeler.h`

- [ ] **Step 1: Replace the existing EncodeSam3Prompt function body**

The current function starts at line 73. Replace the entire function with:

```cpp
inline void EncodeSam3Prompt(const std::string& prompt,
                             int tokenCount,
                             std::vector<int64_t>& inputIds,
                             std::vector<int64_t>& attentionMask) {
    constexpr int64_t kTokenBos = 49406;
    constexpr int64_t kTokenPad = 49407;
    constexpr int64_t kTokenEos = 49407;

    inputIds.assign(static_cast<size_t>(std::max(tokenCount, 0)), kTokenPad);
    attentionMask.assign(static_cast<size_t>(std::max(tokenCount, 0)), 0);

    if (prompt.empty() || tokenCount <= 0) {
        return;
    }

    // Try preset first (for common classes like "person")
    if (const auto* preset = detail::GetSam3PromptPreset(prompt)) {
        const size_t copyCount = std::min(static_cast<size_t>(tokenCount), 
                                          preset->inputIds.size());
        for (size_t i = 0; i < copyCount; ++i) {
            inputIds[i] = preset->inputIds[i];
            attentionMask[i] = preset->attentionMask[i];
        }
        return;
    }

    // Fall back to hash-based encoding for unknown prompts
    inputIds[0] = kTokenBos;
    attentionMask[0] = 1;

    const auto words = detail::SplitSam3PromptWords(prompt);
    int index = 1;
    for (const auto& word : words) {
        if (index >= tokenCount - 1) {
            break;
        }
        inputIds[static_cast<size_t>(index)] = detail::HashSam3PromptWord(word);
        attentionMask[static_cast<size_t>(index)] = 1;
        ++index;
    }

    if (index < tokenCount) {
        inputIds[static_cast<size_t>(index)] = kTokenEos;
        attentionMask[static_cast<size_t>(index)] = 1;
    }
}
```

---

## Chunk 2: Add Unit Tests

### Task 2.1: Add Tests for Person Preset

**Files:**
- Modify: `tests/training_labeling_tests.cpp`

- [ ] **Step 1: Add test for person preset returns correct token IDs**

Find the existing test file and add after existing SAM3 tests (or at end of file if none exist):

```cpp
TEST_CASE("SAM3 person preset returns correct CLIP token IDs") {
    using namespace training;
    
    std::vector<int64_t> inputIds;
    std::vector<int64_t> attentionMask;
    
    // Test exact match
    detail::EncodeSam3Prompt("person", 32, inputIds, attentionMask);
    
    REQUIRE(inputIds.size() == 32);
    REQUIRE(attentionMask.size() == 32);
    
    // Check BOS token
    REQUIRE(inputIds[0] == 49406);
    REQUIRE(attentionMask[0] == 1);
    
    // Check "person" token
    REQUIRE(inputIds[1] == 2533);
    REQUIRE(attentionMask[1] == 1);
    
    // Check EOS token
    REQUIRE(inputIds[2] == 49407);
    REQUIRE(attentionMask[2] == 1);
    
    // Check padding
    for (size_t i = 3; i < 32; ++i) {
        REQUIRE(inputIds[i] == 49407);
        REQUIRE(attentionMask[i] == 0);
    }
}

TEST_CASE("SAM3 person preset is case-insensitive") {
    using namespace training;
    
    std::vector<int64_t> inputIds1, attentionMask1;
    std::vector<int64_t> inputIds2, attentionMask2;
    
    detail::EncodeSam3Prompt("person", 32, inputIds1, attentionMask1);
    detail::EncodeSam3Prompt("PERSON", 32, inputIds2, attentionMask2);
    
    REQUIRE(inputIds1 == inputIds2);
    REQUIRE(attentionMask1 == attentionMask2);
}

TEST_CASE("SAM3 person preset handles variations") {
    using namespace training;
    
    std::vector<int64_t> ids1, mask1;
    std::vector<int64_t> ids2, mask2;
    std::vector<int64_t> ids3, mask3;
    
    detail::EncodeSam3Prompt("person", 32, ids1, mask1);
    detail::EncodeSam3Prompt("a person", 32, ids2, mask2);
    detail::EncodeSam3Prompt("a photo of a person", 32, ids3, mask3);
    
    // All variations should use the same preset
    REQUIRE(ids1 == ids2);
    REQUIRE(ids2 == ids3);
}

TEST_CASE("SAM3 unknown prompt falls back to hash encoding") {
    using namespace training;
    
    std::vector<int64_t> inputIds;
    std::vector<int64_t> attentionMask;
    
    detail::EncodeSam3Prompt("unknown_xyz_class", 32, inputIds, attentionMask);
    
    // Should still have BOS
    REQUIRE(inputIds[0] == 49406);
    REQUIRE(attentionMask[0] == 1);
    
    // Should have hash-based token (not a preset)
    // Hash encoding produces IDs in range [1000, 48000)
    REQUIRE(inputIds[1] >= 1000);
    REQUIRE(inputIds[1] < 48000);
    REQUIRE(inputIds[1] != 2533);  // Not the person token
}
```

---

## Chunk 3: Build and Test

### Task 3.1: Build CUDA Version

- [ ] **Step 1: Run CUDA build**

```powershell
powershell -ExecutionPolicy Bypass -File Build-Cuda.ps1
```

Expected: Build succeeds with no errors

### Task 3.2: Run Unit Tests

- [ ] **Step 1: Run training_labeling_tests.exe**

```powershell
cd build/cuda/Release
./training_labeling_tests.exe
```

Expected: All new preset tests pass (existing dataset tests may still fail - that's expected)

### Task 3.3: Verify Integration

- [ ] **Step 1: Verify SAM3 labeler compiles**

Check that `training_sam3_labeler_trt.cpp` compiles without errors.

---

## Summary

This plan implements:
1. **Preset data structure** for storing pre-computed token sequences
2. **Person preset** with correct CLIP token IDs from reference implementation
3. **Preset lookup function** with case-insensitive matching
4. **Modified encoding function** that tries presets first, then falls back to hash
5. **Unit tests** for preset functionality

After implementation, the "person" class in SAM3 Label mode should work correctly because it will use the real CLIP token IDs that the TensorRT engine expects.
