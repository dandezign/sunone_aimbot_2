#ifndef TRAINING_INFERENCE_MODE_H
#define TRAINING_INFERENCE_MODE_H

#include "../detector/detection_buffer.h"

#include <atomic>
#include <mutex>

namespace training {

enum class InferenceMode {
    Detect,
    Label,
    Paused,
};

inline const char* ToInferenceModeName(InferenceMode mode)
{
    switch (mode)
    {
    case InferenceMode::Detect:
        return "Detect";
    case InferenceMode::Label:
        return "Label";
    case InferenceMode::Paused:
        return "Paused";
    }

    return "Unknown";
}

inline bool CanPauseFromMode(InferenceMode mode)
{
    return mode == InferenceMode::Detect || mode == InferenceMode::Paused;
}

inline bool ShouldDetectorProcessFrames(InferenceMode mode)
{
    return mode == InferenceMode::Detect;
}

inline bool HasExclusiveBackendOwnership(InferenceMode mode)
{
    const bool detectorOwnsFrames = ShouldDetectorProcessFrames(mode);
    const bool trainingOwnsFrames = mode == InferenceMode::Label;

    if (mode == InferenceMode::Paused)
        return !detectorOwnsFrames && !trainingOwnsFrames;

    return detectorOwnsFrames != trainingOwnsFrames;
}

}  // namespace training

extern std::atomic<training::InferenceMode> activeInferenceMode;
extern std::atomic<bool> detectionPaused;
extern DetectionBuffer detectionBuffer;

namespace training {

inline void ClearDetectorStateForInactiveMode()
{
    std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
    detectionBuffer.boxes.clear();
    detectionBuffer.classes.clear();
    ++detectionBuffer.version;
    detectionBuffer.cv.notify_all();
}

}  // namespace training

inline void SetInferenceMode(training::InferenceMode mode)
{
    activeInferenceMode.store(mode);
    detectionPaused.store(!training::ShouldDetectorProcessFrames(mode));

    if (!training::ShouldDetectorProcessFrames(mode))
        training::ClearDetectorStateForInactiveMode();
}

#endif  // TRAINING_INFERENCE_MODE_H
