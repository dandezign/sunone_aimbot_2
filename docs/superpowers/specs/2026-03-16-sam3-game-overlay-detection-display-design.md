# SAM3 Detection Boxes on Game Overlay

**Date**: 2026-03-16  
**Status**: Draft  
**Author**: AI Assistant  

## Summary

Add SAM3 detection box rendering to the DirectX game overlay, displaying bounding boxes, confidence labels, and class names for detections made by the SAM3 model during Label mode. This mirrors the existing YOLO detection box display functionality.

## Motivation

Currently, SAM3 detections are only visible in the ImGui debug preview window. Users running the Training → Label workflow need to see SAM3 detections on the actual game overlay to:
- Verify detections are working in real-time without switching to the debug window
- Adjust SAM3 parameters (confidence threshold, mask settings) with immediate visual feedback
- Confirm labeling accuracy before saving training data

## Scope

- Add SAM3 box rendering to the game overlay in `sunone_aimbot_2.cpp`
- Add configurable color and thickness settings for SAM3 boxes
- Add UI controls in the Training tab for customizing SAM3 box appearance
- Only display SAM3 boxes when `InferenceMode::Label` is active

Out of scope:
- Modifying SAM3 inference logic
- Adding new SAM3 features beyond visualization
- Changing the debug preview rendering

## Architecture

### Component Integration

```
┌─────────────────────────────────────────────────────────────┐
│                  Game Overlay Rendering Loop                 │
│                    (sunone_aimbot_2.cpp)                     │
├─────────────────────────────────────────────────────────────┤
│  1. Draw capture region frame                                │
│  2. Draw YOLO detection boxes (existing)                     │
│  3. Draw SAM3 detection boxes (NEW)                          │
│  4. Draw future points                                       │
│  5. Draw crosshair / icons                                   │
└─────────────────────────────────────────────────────────────┘
```

### Data Source

SAM3 detection data is retrieved via the existing snapshot mechanism:

```cpp
// Existing function in training_sam3_runtime.cpp
Sam3PreviewOverlaySnapshot GetTrainingLatestPreviewOverlaySnapshot();
```

The snapshot contains:
- `frame`: The cv::Mat used for inference (provides resolution for coordinate scaling)
- `result.boxes`: Vector of `DetectionBox` structs with `rect` and `confidence`
- `valid`: Boolean indicating if the snapshot contains valid data

### Coordinate Transformation

SAM3 boxes are in the frame's pixel coordinates. They must be transformed to screen coordinates:

```
screenX = baseX + box.x * (regionW / frame.cols)
screenY = baseY + box.y * (regionH / frame.rows)
screenW = box.width * (regionW / frame.cols)
screenH = box.height * (regionH / frame.rows)
```

Where:
- `baseX`, `baseY`: Top-left corner of the capture region on screen
- `regionW`, `regionH`: Width/height of the capture region on screen
- `frame.cols`, `frame.rows`: Dimensions of the SAM3 inference frame

## Implementation Details

### 1. Configuration Fields

**File**: `sunone_aimbot_2/config/config.h`

**Add the following NEW member variables** to the `Config` class:

```cpp
// SAM3 game overlay box appearance
int training_sam3_box_a = 200;        // Alpha (0-255)
int training_sam3_box_r = 0;          // Red channel (0-255)
int training_sam3_box_g = 255;        // Green channel (0-255)
int training_sam3_box_b = 100;        // Blue channel (0-255)
float training_sam3_box_thickness = 2.0f;  // Outline thickness
```

**Existing fields used** (already defined, no changes needed):
- `training_sam3_draw_preview_boxes` - Toggle for box visibility
- `training_sam3_draw_confidence_labels` - Toggle for confidence text
- `training_sam3_preset_class` - Class name for labels

**File**: `sunone_aimbot_2/config/config.cpp`

Add defaults in `Config::Config()` (after existing SAM3 defaults around lines 270-281):

```cpp
training_sam3_box_a = 200;
training_sam3_box_r = 0;
training_sam3_box_g = 255;
training_sam3_box_b = 100;
training_sam3_box_thickness = 2.0f;
```

Add read logic in `Config::read()` (after existing SAM3 reads around lines 675-680):

```cpp
training_sam3_box_a = get_int("training_sam3_box_a", 200);
training_sam3_box_r = get_int("training_sam3_box_r", 0);
training_sam3_box_g = get_int("training_sam3_box_g", 255);
training_sam3_box_b = get_int("training_sam3_box_b", 100);
training_sam3_box_thickness = get_float("training_sam3_box_thickness", 2.0f);
```

Add write logic in `Config::write()` (after existing SAM3 writes around lines 938-943):

```cpp
file << "training_sam3_box_a = " << training_sam3_box_a << "\n"
     << "training_sam3_box_r = " << training_sam3_box_r << "\n"
     << "training_sam3_box_g = " << training_sam3_box_g << "\n"
     << "training_sam3_box_b = " << training_sam3_box_b << "\n"
     << "training_sam3_box_thickness = " << training_sam3_box_thickness << "\n\n";
```

### 2. UI Controls

**File**: `sunone_aimbot_2/overlay/draw_training.cpp`

**Step 1**: Add new fields to the `Sam3UiSnapshot` struct (around line 32):

```cpp
struct Sam3UiSnapshot {
    std::string enginePath;
    float maskThreshold = 0.5f;
    int minMaskPixels = 64;
    float minConfidence = 0.3f;
    int minBoxWidth = 20;
    int minBoxHeight = 20;
    float minMaskFillRatio = 0.01f;
    int maxDetections = 100;
    bool drawPreviewBoxes = true;
    bool drawConfidenceLabels = true;
    // NEW: Game overlay box appearance
    int boxA = 200;
    int boxR = 0;
    int boxG = 255;
    int boxB = 100;
    float boxThickness = 2.0f;
};
```

**Step 2**: Update `ReadSam3UiSnapshot()` to populate the new fields (around line 45):

```cpp
Sam3UiSnapshot ReadSam3UiSnapshot()
{
    Sam3UiSnapshot snapshot;
    snapshot.enginePath = config.training_sam3_engine_path;
    snapshot.maskThreshold = config.training_sam3_mask_threshold;
    snapshot.minMaskPixels = config.training_sam3_min_mask_pixels;
    snapshot.minConfidence = config.training_sam3_min_confidence;
    snapshot.minBoxWidth = config.training_sam3_min_box_width;
    snapshot.minBoxHeight = config.training_sam3_min_box_height;
    snapshot.minMaskFillRatio = config.training_sam3_min_mask_fill_ratio;
    snapshot.maxDetections = config.training_sam3_max_detections;
    snapshot.drawPreviewBoxes = config.training_sam3_draw_preview_boxes;
    snapshot.drawConfidenceLabels = config.training_sam3_draw_confidence_labels;
    // NEW:
    snapshot.boxA = config.training_sam3_box_a;
    snapshot.boxR = config.training_sam3_box_r;
    snapshot.boxG = config.training_sam3_box_g;
    snapshot.boxB = config.training_sam3_box_b;
    snapshot.boxThickness = config.training_sam3_box_thickness;
    return snapshot;
}
```

**Step 3**: Add controls to the existing `DrawSam3DetectionSection()` function (around line 344), using the existing helper pattern:

```cpp
// Box appearance section
ImGui::SeparatorText("Game Overlay Appearance");

ImGui::Text("Box Color (ARGB):");
ImGui::SameLine();

// Alpha (0-255)
ImGui::PushItemWidth(60);
ImGui::SliderInt("A##sam3_box_a", &snapshot.boxA, 0, 255);
ImGui::SameLine();

// Red (0-255)
ImGui::SliderInt("R##sam3_box_r", &snapshot.boxR, 0, 255);
ImGui::SameLine();

// Green (0-255)
ImGui::SliderInt("G##sam3_box_g", &snapshot.boxG, 0, 255);
ImGui::SameLine();

// Blue (0-255)
ImGui::SliderInt("B##sam3_box_b", &snapshot.boxB, 0, 255);
ImGui::PopItemWidth();

// Thickness (0.5 - 5.0)
ImGui::SliderFloat("Thickness", &snapshot.boxThickness, 0.5f, 5.0f, "%.1f");

// Write back using helper functions
WriteSam3ConfigInt(config.training_sam3_box_a, snapshot.boxA, 0, 255);
WriteSam3ConfigInt(config.training_sam3_box_r, snapshot.boxR, 0, 255);
WriteSam3ConfigInt(config.training_sam3_box_g, snapshot.boxG, 0, 255);
WriteSam3ConfigInt(config.training_sam3_box_b, snapshot.boxB, 0, 255);
WriteSam3ConfigFloat(config.training_sam3_box_thickness, snapshot.boxThickness, 0.5f, 5.0f);
```

### 3. Rendering Code

**File**: `sunone_aimbot_2/sunone_aimbot_2.cpp`

Insert after the YOLO boxes section (after line ~1936, before the "FUTURE POINTS" comment):

```cpp
// SAM3 DETECTION BOXES (Label Mode)
// Note: activeInferenceMode is std::atomic<training::InferenceMode> (see line 43)
if (activeInferenceMode.load() == training::InferenceMode::Label &&
    config.training_sam3_draw_preview_boxes)
{
    const auto sam3Snapshot = training::GetTrainingLatestPreviewOverlaySnapshot();
    
    if (sam3Snapshot.valid && sam3Snapshot.result.success && 
        !sam3Snapshot.result.boxes.empty() && !sam3Snapshot.frame.empty())
    {
        const int sam3FrameW = sam3Snapshot.frame.cols;
        const int sam3FrameH = sam3Snapshot.frame.rows;
        
        if (sam3FrameW > 0 && sam3FrameH > 0)
        {
            // Scale from SAM3 frame coordinates to screen coordinates
            const float sam3ScaleX = static_cast<float>(regionW) / static_cast<float>(sam3FrameW);
            const float sam3ScaleY = static_cast<float>(regionH) / static_cast<float>(sam3FrameH);
            
            // Get color from config
            auto clamp255 = [](int& v) { if (v < 0) v = 0; else if (v > 255) v = 255; };
            int a = config.training_sam3_box_a;
            int r = config.training_sam3_box_r;
            int g = config.training_sam3_box_g;
            int b = config.training_sam3_box_b;
            clamp255(a); clamp255(r); clamp255(g); clamp255(b);
            
            const uint32_t boxCol = 
                (static_cast<uint32_t>(a) << 24) |
                (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) |
                static_cast<uint32_t>(b);
            
            float thickness = config.training_sam3_box_thickness;
            if (thickness <= 0.f) thickness = 1.f;
            
            for (const auto& detection : sam3Snapshot.result.boxes)
            {
                const cv::Rect& box = detection.rect;
                
                if (box.width <= 0 || box.height <= 0) continue;
                
                // Transform to screen coordinates
                float x = baseX + static_cast<float>(box.x) * sam3ScaleX;
                float y = baseY + static_cast<float>(box.y) * sam3ScaleY;
                float w = static_cast<float>(box.width) * sam3ScaleX;
                float h = static_cast<float>(box.height) * sam3ScaleY;
                
                // Cull boxes outside the detection region
                if (x + w < baseX || y + h < baseY ||
                    x > baseX + regionW || y > baseY + regionH)
                    continue;
                
                // Draw the bounding box
                gameOverlayPtr->AddRect({ x, y, w, h }, boxCol, thickness);
                
                // Draw confidence label
                if (config.training_sam3_draw_confidence_labels)
                {
                    wchar_t confLabel[16];
                    swprintf(confLabel, sizeof(confLabel) / sizeof(wchar_t), L"%.2f", detection.confidence);
                    
                    const uint32_t textCol = ARGB(230, 255, 255, 255);
                    const float textY = std::max(static_cast<float>(baseY), y - 16.0f);
                    
                    gameOverlayPtr->AddText(x + 2.0f, textY, confLabel, 15.0f, textCol);
                }
                
                // Draw class name label
                if (!config.training_sam3_preset_class.empty())
                {
                    const uint32_t classCol = ARGB(230, 255, 220, 100);
                    const float classY = y + h + 2.0f;
                    
                    // Convert std::string to std::wstring
                    // Note: ASCII-only; for UTF-8, would need MultiByteToWideChar
                    std::wstring className(config.training_sam3_preset_class.begin(), 
                                          config.training_sam3_preset_class.end());
                    
                    gameOverlayPtr->AddText(x + 2.0f, classY, className, 15.0f, classCol);
                }
            }
        }
    }
}
```

### 4. Required Includes

The following header is needed. Note: the codebase uses paths relative to `${SRC_DIR}`:

```cpp
#include "training/training_sam3_runtime.h"  // For GetTrainingLatestPreviewOverlaySnapshot(), InferenceMode
```

**Note**: The `ARGB()` function is defined in `overlay/Game_overlay.h:13` as:
```cpp
inline constexpr uint32_t ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}
```

## Configuration

| Config Key | Type | Default | Description |
|------------|------|---------|-------------|
| `training_sam3_box_a` | int | 200 | Box alpha channel (0-255) |
| `training_sam3_box_r` | int | 0 | Box red channel (0-255) |
| `training_sam3_box_g` | int | 255 | Box green channel (0-255) |
| `training_sam3_box_b` | int | 100 | Box blue channel (0-255) |
| `training_sam3_box_thickness` | float | 2.0 | Box outline thickness in pixels |

Existing config fields used:
| Config Key | Purpose |
|------------|---------|
| `training_sam3_draw_preview_boxes` | Toggle for box visibility |
| `training_sam3_draw_confidence_labels` | Toggle for confidence text |
| `training_sam3_preset_class` | Class name to display below boxes |

## Error Handling

1. **Invalid snapshot**: Check `snapshot.valid` before accessing data
2. **Empty frame**: Check `!frame.empty()` and `frame.cols > 0 && frame.rows > 0` before coordinate transformation
3. **Empty boxes**: Check `!boxes.empty()` before iterating
4. **Coordinate bounds**: Cull boxes that fall outside the capture region
5. **Invalid dimensions**: Guard against zero/negative box dimensions

## Testing

### Manual Testing Checklist

1. [ ] Start app in Label mode with SAM3 engine loaded
2. [ ] Verify SAM3 boxes appear on game overlay (not just debug window)
3. [ ] Verify boxes have correct positions matching the debug preview
4. [ ] Verify confidence labels appear above boxes
5. [ ] Verify class name appears below boxes
6. [ ] Switch to Detect mode - verify boxes disappear
7. [ ] Switch back to Label mode - verify boxes reappear
8. [ ] Change box color in UI - verify changes take effect immediately
9. [ ] Change box thickness in UI - verify changes take effect immediately
10. [ ] Disable "Draw Preview Boxes" - verify boxes disappear
11. [ ] Re-enable "Draw Preview Boxes" - verify boxes reappear

### Edge Cases

- No detections: No boxes shown (normal behavior)
- Frame size changes: Coordinate scaling adjusts correctly
- Config out of range: Values clamped to valid range
- Empty preset class: No class label displayed (handled by `if (!empty())` check)

## Files Modified

| File | Changes |
|------|---------|
| `sunone_aimbot_2/config/config.h` | Add 5 config fields |
| `sunone_aimbot_2/config/config.cpp` | Add defaults, read, write for 5 fields |
| `sunone_aimbot_2/overlay/draw_training.cpp` | Add UI controls for color/thickness |
| `sunone_aimbot_2/sunone_aimbot_2.cpp` | Add SAM3 box rendering code |

## Dependencies

- Existing `training::GetTrainingLatestPreviewOverlaySnapshot()` function
- Existing `gameOverlayPtr` overlay interface
- Existing config infrastructure
- Existing `training::InferenceMode` enum

## Risks

- **Performance**: Minimal impact - SAM3 inference already runs asynchronously; rendering is simple rectangle drawing
- **Thread safety**: Snapshot retrieval is thread-safe (returns copy under mutex)
- **Config migration**: New fields have sensible defaults; old config files will use defaults