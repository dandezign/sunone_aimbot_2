# SAM3 Game Overlay Detection Display Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add SAM3 detection box rendering to the DirectX game overlay with configurable color and thickness.

**Architecture:** Insert SAM3 box rendering code into the existing overlay loop, using the same coordinate transformation pattern as YOLO boxes. Config fields control appearance, UI controls in Training tab allow customization.

**Tech Stack:** C++17, ImGui, DirectX overlay, OpenCV

---

## File Structure

| File | Responsibility |
|------|----------------|
| `sunone_aimbot_2/config/config.h` | Config field declarations |
| `sunone_aimbot_2/config/config.cpp` | Config defaults, read, write |
| `sunone_aimbot_2/overlay/draw_training.cpp` | UI controls for SAM3 box appearance |
| `sunone_aimbot_2/sunone_aimbot_2.cpp` | SAM3 box rendering on game overlay |

---

## Chunk 1: Config Fields

### Task 1: Add Config Fields to Header

**Files:**
- Modify: `sunone_aimbot_2/config/config.h:235` (after `training_sam3_presets_dir`)

- [ ] **Step 1: Add 5 new config fields**

Insert after line 235 (after `std::string training_sam3_presets_dir;`):

```cpp
    // SAM3 game overlay box appearance
    int training_sam3_box_a;
    int training_sam3_box_r;
    int training_sam3_box_g;
    int training_sam3_box_b;
    float training_sam3_box_thickness;
```

- [ ] **Step 2: Verify the edit**

Run: `grep -n "training_sam3_box" sunone_aimbot_2/config/config.h`
Expected: 5 lines matching the new fields

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/config/config.h
git commit -m "feat: add SAM3 overlay box config fields"
```

---

### Task 2: Add Defaults to Config Constructor

**Files:**
- Modify: `sunone_aimbot_2/config/config.cpp:281` (after `training_sam3_presets_dir` default)

- [ ] **Step 1: Add default values**

Insert after line 281 (after `training_sam3_presets_dir = "models/presets";`):

```cpp
        training_sam3_box_a = 200;
        training_sam3_box_r = 0;
        training_sam3_box_g = 255;
        training_sam3_box_b = 100;
        training_sam3_box_thickness = 2.0f;
```

- [ ] **Step 2: Verify the edit**

Run: `grep -n "training_sam3_box" sunone_aimbot_2/config/config.cpp | head -5`
Expected: 5 lines showing the defaults

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/config/config.cpp
git commit -m "feat: add defaults for SAM3 overlay box config"
```

---

### Task 3: Add Read Logic

**Files:**
- Modify: `sunone_aimbot_2/config/config.cpp:680` (after `training_sam3_presets_dir` read)

- [ ] **Step 1: Add read calls**

Insert after line 680 (after `training_sam3_presets_dir = get_string(...)`):

```cpp
    training_sam3_box_a = get_int("training_sam3_box_a", 200);
    training_sam3_box_r = get_int("training_sam3_box_r", 0);
    training_sam3_box_g = get_int("training_sam3_box_g", 255);
    training_sam3_box_b = get_int("training_sam3_box_b", 100);
    training_sam3_box_thickness = get_float("training_sam3_box_thickness", 2.0f);
```

- [ ] **Step 2: Verify the edit**

Run: `grep -n "training_sam3_box" sunone_aimbot_2/config/config.cpp | grep get_`
Expected: 5 lines showing the get_* calls

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/config/config.cpp
git commit -m "feat: add config read for SAM3 overlay box fields"
```

---

### Task 4: Add Write Logic

**Files:**
- Modify: `sunone_aimbot_2/config/config.cpp:943` (after `training_sam3_presets_dir` write)

- [ ] **Step 1: Add write statements**

Insert after line 943 (after `<< "training_sam3_presets_dir = " << training_sam3_presets_dir << "\n\n";`):

```cpp
        << "training_sam3_box_a = " << training_sam3_box_a << "\n"
        << "training_sam3_box_r = " << training_sam3_box_r << "\n"
        << "training_sam3_box_g = " << training_sam3_box_g << "\n"
        << "training_sam3_box_b = " << training_sam3_box_b << "\n"
        << "training_sam3_box_thickness = " << training_sam3_box_thickness << "\n\n";
```

- [ ] **Step 2: Verify the edit**

Run: `grep -n "training_sam3_box" sunone_aimbot_2/config/config.cpp | grep "<<"`
Expected: 5 lines showing the write statements

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/config/config.cpp
git commit -m "feat: add config write for SAM3 overlay box fields"
```

---

## Chunk 2: UI Controls

### Task 5: Add Sam3UiSnapshot Fields

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp:43` (after `drawConfidenceLabels` in struct)

- [ ] **Step 1: Add fields to Sam3UiSnapshot struct**

Find line 43 (the closing brace of `Sam3UiSnapshot` struct) and add before it:

```cpp
    // Game overlay box appearance
    int boxA = 200;
    int boxR = 0;
    int boxG = 255;
    int boxB = 100;
    float boxThickness = 2.0f;
```

- [ ] **Step 2: Update ReadSam3UiSnapshot() function**

Find line 57-58 (after `snapshot.drawConfidenceLabels = ...`) and add before `return snapshot;`:

```cpp
    // Game overlay box appearance
    snapshot.boxA = config.training_sam3_box_a;
    snapshot.boxR = config.training_sam3_box_r;
    snapshot.boxG = config.training_sam3_box_g;
    snapshot.boxB = config.training_sam3_box_b;
    snapshot.boxThickness = config.training_sam3_box_thickness;
```

- [ ] **Step 3: Verify the edit**

Run: `grep -n "boxA\|boxR\|boxG\|boxB\|boxThickness" sunone_aimbot_2/overlay/draw_training.cpp`
Expected: 10 lines (5 in struct, 5 in ReadSam3UiSnapshot)

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_training.cpp
git commit -m "feat: add SAM3 box fields to Sam3UiSnapshot"
```

---

### Task 6: Add UI Controls

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_training.cpp:398` (before function closing brace)

- [ ] **Step 1: Find insertion point**

Run: `grep -n "Draw Confidence Labels" sunone_aimbot_2/overlay/draw_training.cpp`
Expected: Line 393-398

- [ ] **Step 2: Add UI controls following existing pattern**

Insert before line 399 (before the closing brace `}`), after the HelpMarker for "Draw Confidence Labels":

```cpp

    // Game Overlay Appearance
    ImGui::SeparatorText("Game Overlay Appearance");

    int boxA = sam3Config.boxA;
    int boxR = sam3Config.boxR;
    int boxG = sam3Config.boxG;
    int boxB = sam3Config.boxB;
    float boxThickness = sam3Config.boxThickness;

    ImGui::Text("Box Color (ARGB):");
    ImGui::SameLine();

    ImGui::PushItemWidth(60);
    if (ImGui::SliderInt("A##sam3_box_a", &boxA, 0, 255)) {
        WriteSam3ConfigInt(config.training_sam3_box_a, boxA, 0, 255);
    }
    ImGui::SameLine();
    if (ImGui::SliderInt("R##sam3_box_r", &boxR, 0, 255)) {
        WriteSam3ConfigInt(config.training_sam3_box_r, boxR, 0, 255);
    }
    ImGui::SameLine();
    if (ImGui::SliderInt("G##sam3_box_g", &boxG, 0, 255)) {
        WriteSam3ConfigInt(config.training_sam3_box_g, boxG, 0, 255);
    }
    ImGui::SameLine();
    if (ImGui::SliderInt("B##sam3_box_b", &boxB, 0, 255)) {
        WriteSam3ConfigInt(config.training_sam3_box_b, boxB, 0, 255);
    }
    ImGui::PopItemWidth();

    if (ImGui::SliderFloat("Thickness", &boxThickness, 0.5f, 5.0f, "%.1f")) {
        WriteSam3ConfigFloat(config.training_sam3_box_thickness, boxThickness, 0.5f, 5.0f);
    }
```

- [ ] **Step 3: Verify the edit**

Run: `grep -n "Game Overlay Appearance" sunone_aimbot_2/overlay/draw_training.cpp`
Expected: 1 line showing the new section header

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_training.cpp
git commit -m "feat: add UI controls for SAM3 overlay box appearance"
```

---

## Chunk 3: Rendering Code

### Task 7: Add SAM3 Box Rendering

**Files:**
- Modify: `sunone_aimbot_2/sunone_aimbot_2.cpp:1937` (after YOLO boxes, before "FUTURE POINTS")

- [ ] **Step 1: Find the insertion point**

Run: `grep -n "FUTURE POINTS" sunone_aimbot_2/sunone_aimbot_2.cpp`
Expected: Line 1939

- [ ] **Step 2: Add the SAM3 rendering code**

Insert before line 1939 (before `// FUTURE POINTS`):

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
                    
                    // Build color using ARGB() helper (defined in Game_overlay.h)
                    const uint32_t boxCol = ARGB(
                        static_cast<uint8_t>(std::clamp(config.training_sam3_box_a, 0, 255)),
                        static_cast<uint8_t>(std::clamp(config.training_sam3_box_r, 0, 255)),
                        static_cast<uint8_t>(std::clamp(config.training_sam3_box_g, 0, 255)),
                        static_cast<uint8_t>(std::clamp(config.training_sam3_box_b, 0, 255))
                    );
                    
                    // Thickness is clamped by WriteSam3ConfigFloat (0.5f - 5.0f), but clamp again for safety
                    // in case config was manually edited or loaded from an old file
                    const float thickness = std::clamp(config.training_sam3_box_thickness, 0.5f, 5.0f);
                    
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

- [ ] **Step 3: Verify the edit**

Run: `grep -n "SAM3 DETECTION BOXES" sunone_aimbot_2/sunone_aimbot_2.cpp`
Expected: 1 line showing the new comment

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/sunone_aimbot_2.cpp
git commit -m "feat: add SAM3 detection box rendering to game overlay"
```

---

## Chunk 4: Build and Test

### Task 8: Build and Verify

- [ ] **Step 1: Build the CUDA project**

Run: `powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1`
Expected: Build succeeds with no errors

- [ ] **Step 2: Run the tests**

Run: `.\build\cuda\Release\training_labeling_tests.exe`
Expected: All tests pass

Run: `.\build\cuda\Release\detection_debug_tools_tests.exe`
Expected: All tests pass

- [ ] **Step 3: Verify no compiler warnings**

Check the build output for any new warnings related to the new code.

---

## Verification Checklist

After implementation, verify:

1. [ ] Config fields exist in `config.h`
2. [ ] Config defaults are set in `config.cpp` constructor
3. [ ] Config read works from `config.ini`
4. [ ] Config write saves to `config.ini`
5. [ ] UI controls appear in Training tab
6. [ ] SAM3 boxes appear on game overlay in Label mode
7. [ ] Boxes have correct positions matching debug preview
8. [ ] Confidence labels appear above boxes
9. [ ] Class name appears below boxes
10. [ ] Boxes disappear when switching to Detect mode
11. [ ] Color changes in UI take effect immediately
12. [ ] Thickness changes in UI take effect immediately

---

## Manual Testing Steps

1. Start app with SAM3 engine loaded
2. Switch to Label mode
3. Verify SAM3 detection boxes appear on game overlay
4. Verify boxes match positions in debug preview window
5. Change box color (A, R, G, B) in UI - verify immediate effect
6. Change thickness in UI - verify immediate effect
7. Toggle "Draw Preview Boxes" off - verify boxes disappear
8. Toggle "Draw Preview Boxes" on - verify boxes reappear
9. Switch to Detect mode - verify SAM3 boxes disappear
10. Switch back to Label mode - verify SAM3 boxes reappear