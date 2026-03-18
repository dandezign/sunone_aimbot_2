# SAM3 Video Labeler Design

## Overview

A standalone ImGui application for generating labeled training data using SAM3 TensorRT inference. Users play video files, pause on frames of interest, run SAM3 inference with multi-class prompts, edit predicted bounding boxes, and export to YOLO format.

## Goals

- Generate YOLO-format training data from video files
- Support multi-class labeling via sequential SAM3 inference
- Provide full box editing (add, remove, resize, move)
- Maintain compatibility with existing training dataset structure
- Enable prompt-to-class configuration with persistence

## Non-Goals

- Real-time inference during playback (training workflow accepts ~7 FPS)
- Screen capture integration (planned for v2)
- Automatic annotation without human review

## Architecture

```
SAM3TensorRT/
├── cpp/
│   ├── src/sam3/              # Existing SAM3 inference (reuse)
│   └── apps/
│       └── sam3_labeler/      # NEW application
│           ├── CMakeLists.txt
│           ├── main.cpp
│           ├── app/
│           │   ├── app.h
│           │   ├── app.cpp
│           │   └── app_config.h
│           ├── ui/
│           │   ├── ui_renderer.h
│           │   ├── panels/
│           │   │   ├── capture_panel.h
│           │   │   ├── labels_panel.h
│           │   │   ├── settings_panel.h
│           │   │   └── log_console.h
│           │   └── canvas/
│           │       ├── video_canvas.h
│           │       └── box_editor.h
│           ├── video/
│           │   └── video_player.h
│           ├── inference/
│           │   ├── sam3_labeler_backend.h
│           │   └── multi_prompt_runner.h
│           └── export/
│               └── yolo_exporter.h
└── configs/labeler/
    ├── prompts.yaml
    └── settings.yaml
```

## Data Flow

```
Video File → VideoPlayer → Frame Buffer (cv::Mat, original pixels)
                                 ↓
                          Pause Frame
                                 ↓
                     MultiPromptRunner
                     (sequential inference, skips disabled prompts)
                                 ↓
                     PredictionSet (aggregated boxes per class)
                                 ↓
                          BoxEditor
                     (user adjustments, pixel coords internally)
                                 ↓
                     YOLOExporter
                     (converts to normalized 0-1)
                                 ↓
                 scripts/training/datasets/game/
```

## Components

### VideoPlayer

Wraps OpenCV `VideoCapture` for MP4 playback.

**Responsibilities:**
- Load video file
- Play/pause/step frame-by-frame
- Seek to arbitrary position
- Adjust playback speed (0.5x, 1x, 2x)

**Interface:**
```cpp
class VideoPlayer {
public:
    bool load(const std::string& path);
    void play();
    void pause();
    void step_forward();
    void step_backward();
    void seek(int frame_number);
    bool get_frame(cv::Mat& frame);
    int current_frame() const;
    int total_frames() const;
    double fps() const;
    
    // Error handling
    bool is_loaded() const;
    std::string last_error() const;
};
```

### MultiPromptRunner

Executes SAM3 inference for each **enabled** prompt sequentially.

**Responsibilities:**
- Load prompt-to-class mappings from config
- Run SAM3 inference once per enabled prompt (skips `enabled=false`)
- Aggregate boxes with class assignments
- Report progress for UI feedback

**Interface:**
```cpp
struct PromptConfig {
    std::string text;
    int class_id;
    std::string class_name;
    bool enabled = true;
};

struct LabeledBox {
    int class_id;
    cv::Rect2f bbox;  // normalized 0-1
    float confidence;
};

struct PredictionSet {
    std::vector<LabeledBox> boxes;
    int prompts_run = 0;
    int prompts_skipped = 0;
    std::string error;
};

class MultiPromptRunner {
public:
    void set_prompts(const std::vector<PromptConfig>& prompts);
    PredictionSet run(const cv::Mat& frame);
    int current_prompt_index() const;  // for progress display
    int total_prompts() const;
    int enabled_prompts() const;
    
    // Error handling
    bool is_initialized() const;
    std::string last_error() const;
};
```

**Performance:**
- Single prompt: ~46ms inference
- Three prompts: ~138ms total (~7 FPS)
- Acceptable for training data workflow

### BoxEditor

Handles all user interactions with bounding boxes on the canvas. Works in **pixel coordinates** internally, converts to normalized on export.

**Responsibilities:**
- Render boxes with class colors
- Select box on click
- Move box by dragging center
- Resize box by dragging corners/edges
- Delete selected box
- Create new box by click+drag

**Interface:**
```cpp
enum class EditorState { None, Hover, Selected, Moving, Resizing, Creating };

class BoxEditor {
public:
    // Render boxes on canvas
    void render(ImDrawList* draw_list, const ImVec2& canvas_pos, 
                const ImVec2& canvas_size, float zoom);
    
    // Input handling
    bool handle_mouse(const ImVec2& mouse_pos, bool clicked, bool dragging);
    bool handle_keyboard(int key);  // Returns true if key was consumed
    
    // Data access
    std::vector<LabeledBox> get_boxes_normalized(int img_width, int img_height) const;
    void set_boxes_from_normalized(const std::vector<LabeledBox>& boxes, 
                                    int img_width, int img_height);
    void clear();
    
    // Selection
    int selected_index() const;
    void delete_selected();
    
    // State for cursor display
    EditorState state() const;
    
    // Undo/redo
    bool can_undo() const;
    bool can_redo() const;
    void undo();
    void redo();
    
    // Class for new boxes
    void set_active_class(int class_id);
    int active_class() const;
};
```

**Interaction States:**
| State | Trigger | Cursor |
|-------|---------|--------|
| None | Default | Arrow |
| Hover | Mouse over box | Hand |
| Selected | Click on box | Default |
| Moving | Drag selected box center | Move |
| Resizing | Drag corner/edge | Resize arrow |
| Creating | Click+drag empty area | Crosshair |

**Keyboard Shortcuts:**
- `Delete` / `Backspace`: Remove selected box
- `Escape`: Deselect
- `Ctrl+Z`: Undo last action
- `Ctrl+Shift+Z`: Redo

### YOLOExporter

Writes frame and annotations to training dataset. Converts from pixel to normalized coordinates.

**Output Format:**
```
# labeler_001.txt
# class_id x_center y_center width height (normalized 0-1)
0 0.512345 0.345678 0.123456 0.234567
1 0.234567 0.456789 0.098765 0.123456
```

**Output Structure (matches existing game dataset):**
```
scripts/training/datasets/game/
├── images/              # Training images (default for split=train)
│   └── labeler_001.jpg
├── labels/              # Training labels (default for split=train)
│   └── labeler_001.txt
├── val/                 # Validation split
│   ├── images/
│   │   └── labeler_001.jpg
│   └── labels/
│       └── labeler_001.txt
└── classes.txt
```

**Interface:**
```cpp
enum class DatasetSplit { Train, Val };

class YOLOExporter {
public:
    bool export_frame(
        const cv::Mat& frame,
        const std::vector<LabeledBox>& boxes,
        const std::string& output_dir,
        DatasetSplit split,
        const std::string& filename_base
    );
    
    // Error handling
    std::string last_error() const;
    
    // File existence check
    static bool file_exists(const std::string& path);
    static std::string generate_filename(int counter);
};
```

**Export Trigger:**
- **Manual only** - User clicks "Save" button after reviewing/editing boxes
- Confirmation dialog before overwrite
- Log message on success/failure

### UI Panels

Follow UI_DESIGN_SPEC.md exactly.

**Capture Panel:**
- Video file browser
- Capture button (pauses + runs inference)

**Labels Panel:**
- Prompt list with enable/disable toggles
- Add/Edit/Remove prompt buttons
- Prompt editor popup

**Settings Panel:**
- Output directory browser
- Dataset split selector (train/val)
- Confidence threshold slider
- IoU threshold slider

**Log Console:**
- Inference status
- Save confirmations
- Error messages

## Coordinate System

| Stage | Coordinate System | Format |
|-------|-------------------|--------|
| VideoPlayer output | Pixel | cv::Mat (original resolution) |
| SAM3 inference | Pixel | SAM3_PCS_RESULT.boxes (original pixels) |
| BoxEditor internal | Pixel | cv::Rect (canvas coordinates) |
| BoxEditor display | Pixel | Scaled by zoom factor |
| YOLOExporter input | Normalized 0-1 | LabeledBox.bbox (float) |
| YOLO txt output | Normalized 0-1 | x_center y_center width height |

**Conversion Points:**
1. **SAM3 → BoxEditor**: MultiPromptRunner converts SAM3_PCS_RESULT.boxes (pixels) to LabeledBox (normalized), BoxEditor converts back to pixels for display
2. **BoxEditor → YOLOExporter**: YOLOExporter receives LabeledBox (normalized) directly

## Error Handling

| Scenario | Behavior |
|----------|----------|
| Video load failure | Show error dialog, disable playback controls, log error |
| Empty frame after seek | Log warning, show placeholder, advance to next valid frame |
| SAM3 engine load failure | Show error dialog, disable inference, log error |
| Inference failure | Show error dialog, keep existing boxes, log error |
| Tokenization failure | Show error dialog, check Python dependency, log error |
| Missing classes.txt | Show error dialog, use default class names, log warning |
| Export directory permissions | Show error dialog, suggest alternative path, log error |
| File exists on export | Show confirmation dialog, overwrite on confirm |

## Configuration

### prompts.yaml

```yaml
prompts:
  - text: "enemy character"
    class_id: 0
    class_name: "enemy"
    enabled: true
    
  - text: "enemy character nametag"
    class_id: 7
    class_name: "head"
    enabled: true

settings:
  sam3_engine_path: "models/sam3_fp16.engine"
  output_dir: "scripts/training/datasets/game/"
  split: "train"
  image_format: ".jpg"
```

### Class ID Mapping

Read from existing `scripts/training/datasets/game/classes.txt`:
```
player
bot
weapon
outline
dead_body
hideout_target_human
hideout_target_balls
head
smoke
fire
third_person
```

## Runtime Dependencies

| Dependency | Purpose | Required |
|------------|---------|----------|
| Python 3.12+ | Prompt tokenization (subprocess) | Yes |
| .venv/Scripts/python.exe | Virtual environment Python | Yes |

The existing SAM3 code uses `tokenize_prompt_with_python()` which spawns a Python subprocess for CLIP tokenization. This must be available at runtime.

## Implementation Phases

### Phase 1: Core Infrastructure
- Project setup (CMake, ImGui, OpenCV, SAM3 TRT)
- Main window with dark theme
- Video player with play/pause/step
- Canvas display for video frames

### Phase 2: SAM3 Integration
- SAM3 backend wrapper
- Multi-prompt sequential inference
- Box extraction and display
- Class color coding

### Phase 3: Box Editor
- Selection and highlighting
- Move and resize interactions
- Delete and create operations
- Undo/redo history

### Phase 4: Prompt Editor
- Labels panel UI
- Prompt add/edit/remove
- YAML config persistence

### Phase 5: YOLO Export
- Export logic with coordinate conversion
- File browser for output
- Manual save workflow with confirmation

### Phase 6: Polish
- Keyboard shortcuts
- Log console
- Error handling dialogs
- Help overlay

## Dependencies

| Library | Purpose | Status |
|---------|---------|--------|
| ImGui | UI framework | Available |
| OpenCV | Video playback | Available |
| yaml-cpp | Config serialization | Add required |
| fmt | String formatting | Available |
| TensorRT | SAM3 inference | Available |
| Python 3.12+ | Prompt tokenization | Runtime |

## Estimated Effort

~3000 lines of code across 4 weeks.

## Success Criteria

- Load and play MP4 video files
- Pause and run SAM3 inference with multiple enabled prompts
- Edit predicted boxes (add, remove, resize, move)
- Export to YOLO format compatible with existing training pipeline
- Persist prompt configurations across sessions
- Handle errors gracefully with user feedback