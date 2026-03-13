# Detection Debug Editor Tools Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove all YOLO26 debug overlay behavior and replace it with manual debugging tools in the ImGui `Monitor -> Debug` tab.

**Architecture:** Delete overlay-driven debug rendering (ImGui floating window, game-overlay HUD, foreground box drawing). Add editor-only debug actions that export detection logs and annotated screenshots to timestamped folders. Reuse existing capture/screenshot infrastructure where practical.

**Tech Stack:** C++17, ImGui for editor UI, OpenCV for image annotation, standard filesystem for output.

---

## File Structure

### Files to Delete
- `sunone_aimbot_2/overlay/yolo26_game_debug.h`
- `sunone_aimbot_2/overlay/yolo26_game_debug.cpp`
- `tests/yolo26_game_debug_test.cpp`

### Files to Modify
- `CMakeLists.txt` — remove test target and helper source
- `sunone_aimbot_2/config/config.h` — remove overlay-only config fields
- `sunone_aimbot_2/config/config.cpp` — remove defaults, load, save for overlay-only fields
- `sunone_aimbot_2/overlay/draw_debug.cpp` — delete overlay panel functions, add editor debug actions
- `sunone_aimbot_2/overlay/overlay.cpp` — remove calls to deleted overlay functions
- `sunone_aimbot_2/sunone_aimbot_2.cpp` — remove game-overlay HUD rendering path
- `sunone_aimbot_2/overlay/debug_export.cpp` — refactor or delete (no overlay-triggered path)
- `sunone_aimbot_2/sunone_aimbot_2.h` — remove forward declarations for deleted functions

### New Files to Create
- `sunone_aimbot_2/overlay/detection_debug_export.h` — export helper interface
- `sunone_aimbot_2/overlay/detection_debug_export.cpp` — export implementation (folder creation, JSON, annotation, screenshot)

---

## Chunk 1: Remove Overlay Debug Rendering

### Task 1: Delete YOLO26 Game Overlay HUD Helper Files

**Files:**
- Delete: `sunone_aimbot_2/overlay/yolo26_game_debug.h`
- Delete: `sunone_aimbot_2/overlay/yolo26_game_debug.cpp`
- Delete: `tests/yolo26_game_debug_test.cpp`

- [ ] **Step 1: Delete the three files**

```bash
rm sunone_aimbot_2/overlay/yolo26_game_debug.h
rm sunone_aimbot_2/overlay/yolo26_game_debug.cpp
rm tests/yolo26_game_debug_test.cpp
```

- [ ] **Step 2: Verify files are deleted**

```bash
git status --short
```
Expected: Three files shown as deleted.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "refactor: delete YOLO26 game overlay HUD helper files"
```

### Task 2: Remove CMake Test Target

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Remove test target block (lines ~217-248)**

Find and delete:
```cmake
add_executable(yolo26_game_debug_tests
    "${CMAKE_SOURCE_DIR}/tests/yolo26_game_debug_test.cpp"
    "${SRC_DIR}/overlay/yolo26_game_debug.cpp"
)

set_target_properties(yolo26_game_debug_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(yolo26_game_debug_tests PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/overlay"
)
```

- [ ] **Step 2: Remove helper source from main target**

Find in `AIMBOT_SOURCES` list and remove:
```cmake
"${SRC_DIR}/overlay/yolo26_game_debug.cpp"
```

- [ ] **Step 3: Verify build still configures**

```bash
cd build/cuda && cmake ../.. 2>&1 | tail -20
```
Expected: No errors about missing files.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "refactor: remove YOLO26 game debug test target from CMake"
```

### Task 3: Remove Config Fields from `config.h`

**Files:**
- Modify: `sunone_aimbot_2/config/config.h`

- [ ] **Step 1: Find and remove these fields from the `// Debug (YOLO26)` section**

Remove:
```cpp
bool debug_show_yolo26_panel;      // F12 toggle
bool debug_draw_raw_boxes;         // Red boxes (model space)
bool debug_draw_scaled_boxes;      // Green boxes (game space)
bool debug_show_labels;            // Show coordinate labels
bool debug_export_frame;           // Export frame for analysis
```

Also remove the `// Debug (YOLO26)` comment line if it becomes orphaned.

- [ ] **Step 2: Verify no other code references these fields (will fail compile if so)**

```bash
grep -r "debug_show_yolo26_panel\|debug_draw_raw_boxes\|debug_draw_scaled_boxes\|debug_show_labels\|debug_export_frame" sunone_aimbot_2/ --include="*.cpp" --include="*.h" | grep -v config.cpp
```
Expected: Results only in `config.cpp` and files we will modify in later tasks.

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/config/config.h
git commit -m "refactor: remove YOLO26 overlay-only config field declarations"
```

### Task 4: Remove Config Defaults, Load, Save from `config.cpp`

**Files:**
- Modify: `sunone_aimbot_2/config/config.cpp`

- [ ] **Step 1: Remove defaults in `Config::Config()` (around lines 251-256)**

Remove:
```cpp
// Debug (YOLO26) - enabled for testing
debug_show_yolo26_panel = true;    // Panel visible
debug_draw_raw_boxes = false;      // Red boxes off
debug_draw_scaled_boxes = true;    // Green boxes on
debug_show_labels = false;         // Labels off
debug_export_frame = false;
```

- [ ] **Step 2: Remove load logic in `Config::loadConfig()` (around lines 591-596)**

Remove:
```cpp
// Debug (YOLO26)
debug_show_yolo26_panel = get_long("debug_show_yolo26_panel", debug_show_yolo26_panel ? 1 : 0) != 0;
debug_draw_raw_boxes = get_long("debug_draw_raw_boxes", debug_draw_raw_boxes ? 1 : 0) != 0;
debug_draw_scaled_boxes = get_long("debug_draw_scaled_boxes", debug_draw_scaled_boxes ? 1 : 0) != 0;
debug_show_labels = get_long("debug_show_labels", debug_show_labels ? 1 : 0) != 0;
debug_export_frame = get_long("debug_export_frame", debug_export_frame ? 1 : 0) != 0;
```

- [ ] **Step 3: Remove save logic in `Config::saveConfig()` (around lines 833-838)**

Remove:
```cpp
file << "# Debug (YOLO26)\n"
    << "debug_show_yolo26_panel = " << (debug_show_yolo26_panel ? 1 : 0) << "\n"
    << "debug_draw_raw_boxes = " << (debug_draw_raw_boxes ? 1 : 0) << "\n"
    << "debug_draw_scaled_boxes = " << (debug_draw_scaled_boxes ? 1 : 0) << "\n"
    << "debug_show_labels = " << (debug_show_labels ? 1 : 0) << "\n"
    << "debug_export_frame = " << (debug_export_frame ? 1 : 0) << "\n\n";
```

- [ ] **Step 4: Verify build compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```
Expected: Build succeeds, no undefined field errors.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/config/config.cpp
git commit -m "refactor: remove YOLO26 config defaults, load, and save logic"
```

### Task 5: Remove `drawYolo26DebugPanel()` and `drawYolo26DebugBoxes()` from `draw_debug.cpp`

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_debug.cpp`

- [ ] **Step 1: Delete `drawYolo26DebugPanel()` function (around lines 372-409)**

Remove entire function including:
- ImGui window creation
- Detection list display
- Checkbox controls
- Export button

- [ ] **Step 2: Delete `drawYolo26DebugBoxes()` function (around lines 411-445)**

Remove entire function including:
- Foreground draw list box rendering
- Raw model box drawing
- Scaled game box drawing

- [ ] **Step 3: Verify no compile errors in this file**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1 2>&1 | Select-String -Pattern "draw_debug.cpp"
```
Expected: No errors.

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_debug.cpp
git commit -m "refactor: remove YOLO26 debug panel and box drawing functions"
```

### Task 6: Remove Overlay Function Calls from `overlay.cpp`

**Files:**
- Modify: `sunone_aimbot_2/overlay/overlay.cpp`

- [ ] **Step 1: Remove calls to deleted functions (around lines 1001-1009)**

Remove:
```cpp
// YOLO26 debug panel (must be before ImGui::Render)
drawYolo26DebugPanel();

ImGui::Render();

// YOLO26 debug rendering (overlays after main render)
drawYolo26DebugBoxes();

if (config.debug_export_frame) {
    exportDebugFrame();
}
```

The `exportDebugFrame()` call will be removed because `debug_export_frame` config no longer exists.

- [ ] **Step 2: Remove forward declaration from `overlay.h`**

Modify: `sunone_aimbot_2/overlay/overlay.h`

Remove:
```cpp
void drawYolo26DebugBoxes();
```

- [ ] **Step 3: Verify build compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/overlay.cpp sunone_aimbot_2/overlay/overlay.h
git commit -m "refactor: remove YOLO26 debug function calls from overlay loop"
```

### Task 7: Remove Game Overlay HUD from `sunone_aimbot_2.cpp`

**Files:**
- Modify: `sunone_aimbot_2/sunone_aimbot_2.cpp`
- Modify: `sunone_aimbot_2/sunone_aimbot_2.h`

- [ ] **Step 1: Remove include for deleted helper**

Remove:
```cpp
#include "overlay/yolo26_game_debug.h"
```

- [ ] **Step 2: Remove game-overlay HUD rendering block (around lines 1939-1981)**

Remove the entire `if (config.debug_show_yolo26_panel)` block that:
- Builds `debugDetections` vector
- Calls `yolo26_debug::BuildGameOverlayPlan()`
- Renders filled rects, outlined rects, and text via `gameOverlayPtr`

- [ ] **Step 3: Remove forward declaration from header**

Modify: `sunone_aimbot_2/sunone_aimbot_2.h`

Remove:
```cpp
void drawYolo26DebugPanel();
```

- [ ] **Step 4: Verify build compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add sunone_aimbot_2/sunone_aimbot_2.cpp sunone_aimbot_2/sunone_aimbot_2.h
git commit -m "refactor: remove YOLO26 HUD rendering from game overlay loop"
```

### Task 8: Remove or Refactor `debug_export.cpp`

**Files:**
- Modify or Delete: `sunone_aimbot_2/overlay/debug_export.cpp`
- Modify: `sunone_aimbot_2/overlay/debug_export.h`

- [ ] **Step 1: Inspect current `debug_export.cpp` usage**

```bash
grep -r "exportDebugFrame" sunone_aimbot_2/ --include="*.cpp" --include="*.h"
```
Expected: Only reference was in `overlay.cpp` which we already removed.

- [ ] **Step 2: Delete both files**

```bash
rm sunone_aimbot_2/overlay/debug_export.cpp
rm sunone_aimbot_2/overlay/debug_export.h
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "refactor: delete legacy debug_export module (overlay-triggered path removed)"
```

---

## Chunk 2: Add Editor Debug Export Helper

### Task 9: Create Export Helper Header

**Files:**
- Create: `sunone_aimbot_2/overlay/detection_debug_export.h`

- [ ] **Step 1: Write header with export function declarations**

```cpp
#pragma once

#include <string>

namespace detection_debug {

struct ExportResult {
    bool success;
    std::string message;
    std::string folder_path;
};

ExportResult saveDetectionLog(const std::string& base_folder);
ExportResult saveAnnotatedScreenshot(const std::string& base_folder);
ExportResult saveDebugBundle();

} // namespace detection_debug
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/overlay/detection_debug_export.h
git commit -m "feat: add detection debug export helper header"
```

### Task 10: Implement Folder Creation and Timestamp Utilities

**Files:**
- Create: `sunone_aimbot_2/overlay/detection_debug_export.cpp`

- [ ] **Step 1: Write includes and namespace**

```cpp
#include "detection_debug_export.h"
#include "../config/config.h"
#include "../detector/detection_buffer.h"
#include "../sunone_aimbot_2.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <opencv2/opencv.hpp>

extern Config config;
extern DetectionBuffer detectionBuffer;

namespace detection_debug {
namespace {

std::string formatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::stringstream ss;
    struct tm localTime;
    localtime_s(&localTime, &t);
    ss << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");
    ss << "-" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string createTimestampedFolder(const std::string& base_folder) {
    std::string folderName = formatTimestamp() + "_detection_debug";
    std::string fullPath = base_folder + "/" + folderName;
    std::error_code ec;
    std::filesystem::create_directories(fullPath, ec);
    if (ec) {
        return "";
    }
    return fullPath;
}

} // namespace
} // namespace detection_debug
```

- [ ] **Step 2: Verify compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/overlay/detection_debug_export.cpp
git commit -m "feat: add timestamp and folder creation utilities for debug export"
```

### Task 11: Implement Detection Log Export

**Files:**
- Modify: `sunone_aimbot_2/overlay/detection_debug_export.cpp`

- [ ] **Step 1: Write `saveDetectionLog()` function**

```cpp
ExportResult saveDetectionLog(const std::string& base_folder) {
    ExportResult result{};
    
    std::string folder = createTimestampedFolder(base_folder);
    if (folder.empty()) {
        result.success = false;
        result.message = "Failed to create debug folder";
        return result;
    }
    result.folder_path = folder;
    
    std::ofstream f(folder + "/detections.json");
    if (!f.is_open()) {
        result.success = false;
        result.message = "Failed to create detections.json";
        return result;
    }
    
    auto timestamp = formatTimestamp();
    f << "{\n";
    f << "  \"timestamp\": \"" << timestamp << "\",\n";
    f << "  \"model\": \"" << config.ai_model << "\",\n";
    f << "  \"detection_resolution\": " << config.detection_resolution << ",\n";
    f << "  \"frame\": { \"width\": " << config.detection_resolution 
      << ", \"height\": " << config.detection_resolution << " },\n";
    f << "  \"detections\": [\n";
    
    {
        std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
        for (size_t i = 0; i < detectionBuffer.boxes.size(); ++i) {
            const auto& box = detectionBuffer.boxes[i];
            const int classId = i < detectionBuffer.classes.size() 
                ? detectionBuffer.classes[i] : -1;
            
            f << "    {\n";
            f << "      \"id\": " << i << ",\n";
            f << "      \"class_id\": " << classId << ",\n";
            f << "      \"game_space\": { ";
            f << "\"x\": " << box.x << ", ";
            f << "\"y\": " << box.y << ", ";
            f << "\"w\": " << box.width << ", ";
            f << "\"h\": " << box.height;
            f << " }\n";
            f << "    }";
            if (i + 1 < detectionBuffer.boxes.size()) {
                f << ",";
            }
            f << "\n";
        }
    }
    
    f << "  ]\n";
    f << "}\n";
    f.close();
    
    result.success = true;
    result.message = "Detection log saved";
    return result;
}
```

- [ ] **Step 2: Verify compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/overlay/detection_debug_export.cpp
git commit -m "feat: implement detection log JSON export"
```

### Task 12: Implement Annotated Screenshot Export

**Files:**
- Modify: `sunone_aimbot_2/overlay/detection_debug_export.cpp`

- [ ] **Step 1: Write helper function for annotation**

```cpp
cv::Mat annotateFrameWithDetections(const cv::Mat& frame) {
    cv::Mat annotated;
    frame.copyTo(annotated);
    
    std::lock_guard<std::mutex> lock(detectionBuffer.mutex);
    for (size_t i = 0; i < detectionBuffer.boxes.size(); ++i) {
        const auto& box = detectionBuffer.boxes[i];
        const int classId = i < detectionBuffer.classes.size() 
            ? detectionBuffer.classes[i] : -1;
        
        // Draw red box (2px)
        cv::rectangle(annotated, 
            cv::Point(box.x, box.y),
            cv::Point(box.x + box.width, box.y + box.height),
            cv::Scalar(0, 0, 255),  // BGR red
            2);
        
        // Draw yellow label
        std::string label = "Class " + std::to_string(classId);
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 
            0.5, 1, &baseline);
        
        cv::Point textPos(box.x, box.y - 4);
        if (textPos.y < textSize.height + 2) {
            textPos.y = box.y + box.height + textSize.height + 2;
        }
        
        cv::rectangle(annotated,
            cv::Point(textPos.x, textPos.y - textSize.height - 2),
            cv::Point(textPos.x + textSize.width, textPos.y + baseline),
            cv::Scalar(0, 0, 255),  // Red background
            cv::FILLED);
        
        cv::putText(annotated, label, textPos, cv::FONT_HERSHEY_SIMPLEX,
            0.5, cv::Scalar(0, 255, 255),  // Yellow text (BGR)
            1);
    }
    
    return annotated;
}
```

- [ ] **Step 2: Write `saveAnnotatedScreenshot()` function**

```cpp
ExportResult saveAnnotatedScreenshot(const std::string& base_folder) {
    ExportResult result{};
    
    std::string folder = createTimestampedFolder(base_folder);
    if (folder.empty()) {
        result.success = false;
        result.message = "Failed to create debug folder";
        return result;
    }
    result.folder_path = folder;
    
    cv::Mat frameCopy;
    {
        std::lock_guard<std::mutex> lk(frameMutex);
        if (latestFrame.empty()) {
            result.success = false;
            result.message = "No frame available for screenshot";
            return result;
        }
        latestFrame.copyTo(frameCopy);
    }
    
    // Save raw screenshot
    cv::imwrite(folder + "/screenshot_raw.png", frameCopy);
    
    // Save annotated screenshot
    cv::Mat annotated = annotateFrameWithDetections(frameCopy);
    cv::imwrite(folder + "/screenshot_annotated.png", annotated);
    
    result.success = true;
    result.message = "Screenshots saved";
    return result;
}
```

- [ ] **Step 3: Verify compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/detection_debug_export.cpp
git commit -m "feat: implement annotated screenshot export with OpenCV drawing"
```

### Task 13: Implement Debug Bundle Export

**Files:**
- Modify: `sunone_aimbot_2/overlay/detection_debug_export.cpp`
- Modify: `sunone_aimbot_2/overlay/detection_debug_export.h`

- [ ] **Step 1: Write config snapshot helper**

```cpp
void writeConfigSnapshot(const std::string& folder) {
    std::ofstream f(folder + "/config_snapshot.txt");
    if (!f.is_open()) return;
    
    f << "[Detection]\n";
    f << "model_path = " << config.ai_model << "\n";
    f << "confidence_threshold = " << config.confidence_threshold << "\n";
    f << "detection_resolution = " << config.detection_resolution << "\n";
    f << "\n[Capture]\n";
    f << "capture_method = " << config.capture_method << "\n";
    f << "capture_fps = " << config.capture_fps << "\n";
}
```

- [ ] **Step 2: Write `saveDebugBundle()` function**

```cpp
ExportResult saveDebugBundle() {
    ExportResult result{};
    
    const std::string base_folder = "debug";
    std::string folder = createTimestampedFolder(base_folder);
    if (folder.empty()) {
        result.success = false;
        result.message = "Failed to create debug folder";
        return result;
    }
    result.folder_path = folder;
    
    // Save detection log
    auto logResult = saveDetectionLog(base_folder);
    if (!logResult.success) {
        result.message += "Log export failed. ";
    }
    
    // Save config snapshot
    writeConfigSnapshot(folder);
    
    // Save screenshots
    auto shotResult = saveAnnotatedScreenshot(base_folder);
    if (!shotResult.success) {
        result.message += "Screenshot export skipped (no frame). ";
    }
    
    result.success = true;
    if (result.message.empty()) {
        result.message = "Debug bundle saved";
    }
    return result;
}
```

- [ ] **Step 3: Verify compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/detection_debug_export.h sunone_aimbot_2/overlay/detection_debug_export.cpp
git commit -m "feat: implement debug bundle export with config snapshot"
```

---

## Chunk 3: Add ImGui Editor UI

### Task 14: Add Editor State for Export Status

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_debug.cpp`

- [ ] **Step 1: Add static state variables at file scope**

```cpp
static std::string g_lastDebugExportStatus;
static std::string g_lastDebugExportPath;
static std::chrono::steady_clock::time_point g_lastExportTime;
```

- [ ] **Step 2: Add include for export helper**

```cpp
#include "detection_debug_export.h"
```

- [ ] **Step 3: Verify compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_debug.cpp
git commit -m "feat: add editor state for debug export status tracking"
```

### Task 15: Add Debug Section UI in `draw_debug()`

**Files:**
- Modify: `sunone_aimbot_2/overlay/draw_debug.cpp`

- [ ] **Step 1: Find `draw_debug()` function (around line 345)**

- [ ] **Step 2: Add new section for detection debug tools**

Add after existing screenshot section:

```cpp
void draw_debug()
{
    // ... existing screenshot section ...
    
    // Detection Debug Tools section
    if (OverlayUI::BeginSection("Detection Debug", "debug_section_detection_debug"))
    {
        ImGui::TextUnformatted("Export current detection state for analysis.");
        ImGui::Separator();
        
        const float buttonWidth = 200.0f;
        
        if (ImGui::Button("Save Detection Log", ImVec2(buttonWidth, 0.0f))) {
            auto result = detection_debug::saveDetectionLog("debug");
            g_lastDebugExportStatus = result.message;
            g_lastDebugExportPath = result.folder_path;
            g_lastExportTime = std::chrono::steady_clock::now();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(exports detections.json)");
        
        if (ImGui::Button("Save Annotated Screenshot", ImVec2(buttonWidth, 0.0f))) {
            auto result = detection_debug::saveAnnotatedScreenshot("debug");
            g_lastDebugExportStatus = result.message;
            g_lastDebugExportPath = result.folder_path;
            g_lastExportTime = std::chrono::steady_clock::now();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(exports raw + annotated PNG)");
        
        if (ImGui::Button("Save Debug Bundle", ImVec2(buttonWidth, 0.0f))) {
            auto result = detection_debug::saveDebugBundle();
            g_lastDebugExportStatus = result.message;
            g_lastDebugExportPath = result.folder_path;
            g_lastExportTime = std::chrono::steady_clock::now();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(exports all files)");
        
        ImGui::Separator();
        
        if (!g_lastDebugExportStatus.empty()) {
            auto elapsed = std::chrono::steady_clock::now() - g_lastExportTime;
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            
            ImGui::Text("Last action: %s", g_lastDebugExportStatus.c_str());
            ImGui::Text("Last path: %s", g_lastDebugExportPath.c_str());
            ImGui::Text(" (%lld seconds ago)", seconds);
        } else {
            ImGui::TextDisabled("No exports yet this session.");
        }
        
        OverlayUI::EndSection();
    }
}
```

- [ ] **Step 3: Verify compiles**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_debug.cpp
git commit -m "feat: add Detection Debug section with export buttons to Debug tab"
```

---

## Chunk 4: Verification and Cleanup

### Task 16: Full Build Verification

**Files:**
- All modified files

- [ ] **Step 1: Clean rebuild**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-CudaRebuild.ps1
```
Expected: Build succeeds with no errors.

- [ ] **Step 2: Run the executable**

```bash
.\build\cuda\Release\ai.exe
```
Expected: Application starts normally.

- [ ] **Step 3: Verify no YOLO26 overlay windows appear**

- Open the overlay (HOME key by default)
- Navigate to `Monitor -> Debug`
- Verify: No "YOLO26 Debug" floating window appears
- Verify: No red/green boxes drawn on screen

- [ ] **Step 4: Verify new Debug tools appear**

- In `Monitor -> Debug`, verify three buttons exist:
  - "Save Detection Log"
  - "Save Annotated Screenshot"  
  - "Save Debug Bundle"

- [ ] **Step 5: Test each export button**

Click each button and verify:
- Status text updates
- Folder is created under `debug/`
- Expected files are written

- [ ] **Step 6: Verify output structure**

```bash
dir debug /s
```
Expected: Folder structure matches spec:
```
debug/
  YYYY-MM-DD_HH-MM-SS-NNN_detection_debug/
    detections.json
    screenshot_raw.png
    screenshot_annotated.png
    config_snapshot.txt
```

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "chore: verify build and debug export functionality"
```

### Task 17: Clean Up Orphaned References

**Files:**
- Various

- [ ] **Step 1: Search for any remaining YOLO26 debug references**

```bash
grep -r "yolo26.*debug\|debug.*yolo26\|drawYolo26" sunone_aimbot_2/ --include="*.cpp" --include="*.h" --include="*.hpp"
```
Expected: No results except possibly in comments or detector internals.

- [ ] **Step 2: Search for orphaned config references**

```bash
grep -r "debug_show_yolo26_panel\|debug_draw_raw_boxes\|debug_draw_scaled_boxes\|debug_show_labels\|debug_export_frame" sunone_aimbot_2/ --include="*.cpp" --include="*.h"
```
Expected: No results.

- [ ] **Step 3: Fix any remaining references found**

- [ ] **Step 4: Final rebuild**

```bash
powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
```

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: remove any remaining orphaned YOLO26 debug references"
```

---

## Verification Checklist

Before marking complete:

- [ ] Build succeeds
- [ ] No YOLO26 overlay panel or game-overlay HUD code remains wired
- [ ] `Monitor -> Debug` shows Detection Debug section with three buttons
- [ ] `Save Detection Log` creates `debug/<timestamp>/detections.json`
- [ ] `Save Annotated Screenshot` creates raw + annotated PNG files
- [ ] `Save Debug Bundle` creates complete output set
- [ ] No detection behavior was changed (no decoder, scaling, or targeting modifications)
- [ ] Status text updates after each export action

---
