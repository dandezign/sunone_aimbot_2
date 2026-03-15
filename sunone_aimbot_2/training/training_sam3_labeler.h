#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include "sunone_aimbot_2/training/training_dataset_manager.h"
#include "sunone_aimbot_2/training/training_label_types.h"

namespace training {

class Sam3PresetLoader;

struct Sam3Availability {
    bool ready;
    std::string message;
};

struct Sam3DebugCounters {
    int rawMaskSlots = 0;
    int nonEmptyMasks = 0;
    int afterMinMaskPixels = 0;
    int afterMinBoxWidthHeight = 0;
    int afterMinMaskFillRatio = 0;
    int afterMinConfidence = 0;
    int finalBoxesBeforeCap = 0;
    int finalBoxesRendered = 0;
};

struct Sam3LabelResult {
    bool success = false;
    std::vector<DetectionBox> boxes;
    std::string error;
    Sam3DebugCounters counters;
};

// Preset prompt with pre-computed CLIP token sequences
struct Sam3PromptPreset {
    const char* name;
    std::vector<int64_t> inputIds;
    std::vector<int64_t> attentionMask;
};

namespace detail {

inline std::vector<std::string> SplitSam3PromptWords(const std::string& prompt) {
    std::vector<std::string> words;
    std::string current;
    current.reserve(prompt.size());

    for (unsigned char ch : prompt) {
        if (std::isalnum(ch) || ch == '_' || ch == '-') {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }

    if (!current.empty()) {
        words.push_back(current);
    }

    if (words.empty() && !prompt.empty()) {
        std::string lowered;
        lowered.reserve(prompt.size());
        for (unsigned char ch : prompt) {
            lowered.push_back(static_cast<char>(std::tolower(ch)));
        }
        words.push_back(lowered);
    }

    return words;
}

inline int64_t HashSam3PromptWord(const std::string& word) {
    constexpr uint32_t kHashOffset = 2166136261u;
    constexpr uint32_t kHashPrime = 16777619u;
    constexpr int64_t kTokenHashBase = 1000;
    constexpr int64_t kTokenHashRange = 47000;

    uint32_t hash = kHashOffset;
    for (unsigned char ch : word) {
        hash ^= ch;
        hash *= kHashPrime;
    }

    return kTokenHashBase + static_cast<int64_t>(hash % kTokenHashRange);
}

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

// Get preset for common prompts (e.g., "person")
// Returns nullptr if no preset exists for the given prompt
inline const Sam3PromptPreset* GetSam3PromptPreset(const std::string& prompt) {
    // Strip common prefixes and check for "person"
    std::string normalized = prompt;
    
    // Convert to lowercase for matching
    std::string lowered;
    lowered.reserve(normalized.size());
    for (unsigned char ch : normalized) {
        lowered.push_back(static_cast<char>(std::tolower(ch)));
    }
    
    const char* prefixes[] = {"a photo of ", "photo of ", "a ", "the ", "an "};
    bool removedPrefix = true;
    while (removedPrefix) {
        removedPrefix = false;
        for (const char* prefix : prefixes) {
            const size_t prefixLen = strlen(prefix);
            if (lowered.rfind(prefix, 0) == 0) {
                lowered = lowered.substr(prefixLen);
                removedPrefix = true;
                break;
            }
        }
    }
    
    // Check for exact match or common variations
    if (lowered == "person" ||
        lowered == "people" ||
        lowered == "human" ||
        lowered == "man" ||
        lowered == "woman" ||
        lowered == "guy" ||
        lowered == "girl") {
        return &kPersonPreset;
    }
    
    return nullptr;
}

inline void EncodeSam3Prompt(const Sam3PromptPreset* preset,
                              const std::string& prompt,
                              int tokenCount,
                              std::vector<int64_t>& inputIds,
                              std::vector<int64_t>& attentionMask) {
    constexpr int64_t kTokenBos = 49406;
    constexpr int64_t kTokenPad = 49407;
    constexpr int64_t kTokenEos = 49407;

    inputIds.assign(static_cast<size_t>(std::max(tokenCount, 0)), kTokenPad);
    attentionMask.assign(static_cast<size_t>(std::max(tokenCount, 0)), 0);

    if (tokenCount <= 0) {
        return;
    }

    if (preset != nullptr) {
        const size_t copyCount = std::min(static_cast<size_t>(tokenCount), 
                                          preset->inputIds.size());
        for (size_t i = 0; i < copyCount; ++i) {
            inputIds[i] = preset->inputIds[i];
            attentionMask[i] = preset->attentionMask[i];
        }
        return;
    }

    if (prompt.empty()) {
        return;
    }

    inputIds[0] = kTokenBos;
    attentionMask[0] = 1;

    const auto words = SplitSam3PromptWords(prompt);
    int index = 1;
    for (const auto& word : words) {
        if (index >= tokenCount - 1) {
            break;
        }
        inputIds[static_cast<size_t>(index)] = HashSam3PromptWord(word);
        attentionMask[static_cast<size_t>(index)] = 1;
        ++index;
    }

    if (index < tokenCount) {
        inputIds[static_cast<size_t>(index)] = kTokenEos;
        attentionMask[static_cast<size_t>(index)] = 1;
    }
}

}  // namespace detail

inline void EncodeSam3PromptForTests(const std::string& prompt,
                                     int tokenCount,
                                     std::vector<int64_t>& inputIds,
                                     std::vector<int64_t>& attentionMask) {
    detail::EncodeSam3Prompt(nullptr, prompt, tokenCount, inputIds, attentionMask);
}

class Sam3Labeler {
public:
    Sam3Labeler();
    ~Sam3Labeler();
    Sam3Labeler(const Sam3Labeler&) = delete;
    Sam3Labeler& operator=(const Sam3Labeler&) = delete;
    Sam3Labeler(Sam3Labeler&&) = delete;
    Sam3Labeler& operator=(Sam3Labeler&&) = delete;
    
    bool Initialize(const std::string& enginePath);
    void RequestStop();
    void Shutdown();
    Sam3Availability GetAvailability() const;
    Sam3LabelResult LabelFrame(const Sam3InferenceRequest& request);
    
    void SetPresetLoader(const Sam3PresetLoader* loader);
    Sam3Availability GetAvailabilityForTests() const;

private:
    struct Impl;
    Impl* impl_;
};

inline std::vector<DetectionBox> SortAndCapDetectionsForTests(std::vector<DetectionBox> detections,
                                                              int maxDetections) {
    std::sort(detections.begin(), detections.end(),
        [](const DetectionBox& a, const DetectionBox& b) {
            return a.confidence > b.confidence;
        });

    if (maxDetections >= 0 && static_cast<int>(detections.size()) > maxDetections) {
        detections.resize(static_cast<size_t>(maxDetections));
    }

    return detections;
}

}  // namespace training
