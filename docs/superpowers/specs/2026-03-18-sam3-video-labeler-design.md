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
Video File → VideoPlayer → Frame Buffer
                                 ↓
                         Pause Frame
                                 ↓
                    MultiPromptRunner
                    (sequential inference)
                                 ↓
                    AllPredictions
                    (boxes per class)
                                 ↓
                         BoxEditor
                    (user adjustments)
                                 ↓
                       YOLOExporter
                                 ↓
                scripts/training/datasets/game/
                ├── images/train/labeler_001.jpg
                └── labels/train/labeler_001.txt
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
};
```

### MultiPromptRunner

Executes SAM3 inference for each configured prompt sequentially.

**Responsibilities:**
- Load prompt-to-class mappings from config
- Run SAM3 inference once per prompt
- Aggregate boxes with class assignments
- Report progress for UI feedback

**Interface:**
```cpp
struct PromptConfig {
    std::string text;
    int class_id;
    std::string class_name;
    bool enabled;
};

struct LabeledBox {
    int class_id;
    cv::Rect2f bbox;  // normalized 0-1
    float confidence;
};

class MultiPromptRunner {
public:
    void set_prompts(const std::vector<PromptConfig>& prompts);
    std::vector<LabeledBox> run(const cv::Mat& frame);
    int current_prompt_index() const;  // for progress display
    int total_prompts() const;
};
```

**Performance:**
- Single prompt: ~46ms inference
- Three prompts: ~138ms total (~7 FPS)
- Acceptable for training data workflow

### BoxEditor

Handles all user interactions with bounding boxes on the canvas.

**Responsibilities:**
- Render boxes with class colors
- Select box on click
- Move box by dragging center
- Resize box by dragging corners/edges
- Delete selected box
- Create new box by click+drag

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

Writes frame and annotations to training dataset.

**Output Format:**
```
# labeler_001.txt
# class_id x_center y_center width height (normalized 0-1)
0 0.512345 0.345678 0.123456 0.234567
1 0.234567 0.456789 0.098765 0.123456
```

**Output Structure:**
```
scripts/training/datasets/game/
├── images/
│   ├── train/labeler_001.jpg
│   └── val/labeler_001.jpg
├── labels/
│   ├── train/labeler_001.txt
│   └── val/labeler_001.txt
└── classes.txt
```

**Interface:**
```cpp
class YOLOExporter {
public:
    bool export_frame(
        const cv::Mat& frame,
        const std::vector<LabeledBox>& boxes,
        const std::string& output_dir,
        DatasetSplit split
    );
};
```

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
- Export logic
- File browser for output
- Save workflow

### Phase 6: Polish
- Keyboard shortcuts
- Log console
- Error handling
- Help overlay

## Dependencies

| Library | Purpose | Status |
|---------|---------|--------|
| ImGui | UI framework | Available |
| OpenCV | Video playback | Available |
| yaml-cpp | Config serialization | Add required |
| fmt | String formatting | Available |
| TensorRT | SAM3 inference | Available |

## Estimated Effort

~3000 lines of code across 4 weeks.

## Success Criteria

- Load and play MP4 video files
- Pause and run SAM3 inference with multiple prompts
- Edit predicted boxes (add, remove, resize, move)
- Export to YOLO format compatible with existing training pipeline
- Persist prompt configurations across sessions