# SAM3 Video Labeler Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone ImGui application for generating YOLO-format training data from video files using SAM3 TensorRT inference.

**Architecture:** New standalone app in `SAM3TensorRT/cpp/apps/sam3_labeler/` that reuses existing SAM3 inference code. ImGui for UI, OpenCV for video playback, yaml-cpp for config persistence.

**Tech Stack:** C++17, ImGui, OpenCV, TensorRT, yaml-cpp, fmt

**Spec:** `docs/superpowers/specs/2026-03-18-sam3-video-labeler-design.md`

---

## File Structure

```
SAM3TensorRT/
├── cpp/
│   └── apps/
│       └── sam3_labeler/
│           ├── CMakeLists.txt
│           ├── main.cpp
│           ├── app/
│           │   ├── app.h
│           │   ├── app.cpp
│           │   └── app_config.h
│           ├── video/
│           │   └── video_player.h
│           ├── inference/
│           │   ├── sam3_labeler_backend.h
│           │   └── multi_prompt_runner.h
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
│           └── export/
│               └── yolo_exporter.h
└── configs/labeler/
    └── config.yaml
```

---

## Chunk 1: Project Setup and Core Infrastructure

### Task 1.1: Create CMakeLists.txt for Labeler App

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/CMakeLists.txt`

- [ ] **Step 1: Create CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(sam3_labeler LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(OpenCV REQUIRED)
find_package(CUDA REQUIRED)
find_package(yaml-cpp REQUIRED)

# ImGui sources (relative to SAM3TensorRT root)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui")
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Labeler sources
set(LABELER_SOURCES
    main.cpp
    app/app.cpp
    video/video_player.h
    inference/sam3_labeler_backend.h
    inference/multi_prompt_runner.h
    ui/ui_renderer.h
    ui/panels/capture_panel.h
    ui/panels/labels_panel.h
    ui/panels/settings_panel.h
    ui/panels/log_console.h
    ui/canvas/video_canvas.h
    ui/canvas/box_editor.h
    export/yolo_exporter.h
)

add_executable(sam3_labeler ${LABELER_SOURCES} ${IMGUI_SOURCES})

target_include_directories(sam3_labeler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(sam3_labeler PRIVATE
    sam3_trt  # Defined in parent CMakeLists.txt - links SAM3 TensorRT library
    ${OpenCV_LIBS}
    yaml-cpp
    glfw
    OpenGL::GL
    cuda
)

# Copy config to build directory
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../configs/labeler/config.yaml"
    "${CMAKE_CURRENT_BINARY_DIR}/config.yaml"
    COPYONLY
)
```

- [ ] **Step 2: Update parent CMakeLists.txt**

Modify: `SAM3TensorRT/cpp/CMakeLists.txt` (add at end):

```cmake
add_subdirectory(apps/sam3_labeler)
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/CMakeLists.txt SAM3TensorRT/cpp/CMakeLists.txt
git commit -m "feat(labeler): add CMakeLists.txt for sam3_labeler app"
```

---

### Task 1.2: Create App Config Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/app/app_config.h`

- [ ] **Step 1: Create app_config.h**

```cpp
#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace sam3_labeler {

struct PromptConfig {
    std::string text;
    int class_id = 0;
    std::string class_name;
    bool enabled = true;
};

struct Settings {
    std::string sam3_engine_path = "models/sam3_fp16.engine";
    std::string output_dir = "scripts/training/datasets/game/";
    std::string split = "train";
    std::string image_format = ".jpg";
    float confidence_threshold = 0.45f;
    float iou_threshold = 0.35f;
};

struct AppConfig {
    std::vector<PromptConfig> prompts;
    Settings settings;
    
    static AppConfig load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/app/app_config.h
git commit -m "feat(labeler): add AppConfig struct for YAML config"
```

---

### Task 1.3: Create Main Entry Point

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/main.cpp`

- [ ] **Step 1: Create main.cpp**

```cpp
#include "app/app.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        sam3_labeler::App app;
        
        if (!app.init()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return 1;
        }
        
        app.run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/main.cpp
git commit -m "feat(labeler): add main entry point"
```

---

### Task 1.4: Create App Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/app/app.h`

- [ ] **Step 1: Create app.h**

```cpp
#pragma once

#include "app_config.h"
#include "video/video_player.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <functional>

namespace sam3_labeler {

class App {
public:
    App();
    ~App();
    
    bool init();
    void run();
    
    // Accessors for UI panels
    AppConfig& config() { return config_; }
    VideoPlayer& video_player() { return video_player_; }
    
    // Log callback
    using LogCallback = std::function<void(const std::string& level, const std::string& message)>;
    void set_log_callback(LogCallback callback) { log_callback_ = callback; }
    void log(const std::string& level, const std::string& message);
    
private:
    bool init_glfw();
    bool init_imgui();
    void shutdown();
    
    void render_frame();
    void render_ui();
    
    // Window
    GLFWwindow* window_ = nullptr;
    int window_width_ = 1014;
    int window_height_ = 730;
    
    // Config
    AppConfig config_;
    std::string config_path_ = "config.yaml";
    
    // Components
    VideoPlayer video_player_;
    
    // Callbacks
    LogCallback log_callback_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/app/app.h
git commit -m "feat(labeler): add App class header"
```

---

### Task 1.5: Create VideoPlayer Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/video/video_player.h`

- [ ] **Step 1: Create video_player.h**

```cpp
#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace sam3_labeler {

class VideoPlayer {
public:
    VideoPlayer() = default;
    ~VideoPlayer();
    
    // File operations
    bool load(const std::string& path);
    void close();
    
    // Playback control
    void play();
    void pause();
    void step_forward();
    void step_backward();
    void seek(int frame_number);
    
    // Frame access
    bool get_frame(cv::Mat& frame);
    bool is_playing() const { return playing_; }
    
    // Properties
    int current_frame() const { return current_frame_; }
    int total_frames() const { return total_frames_; }
    double fps() const { return fps_; }
    bool is_loaded() const { return loaded_; }
    std::string last_error() const { return last_error_; }
    std::string filename() const { return filename_; }
    
    // Speed control
    void set_speed(double speed) { playback_speed_ = speed; }
    double speed() const { return playback_speed_; }
    
private:
    cv::VideoCapture capture_;
    bool loaded_ = false;
    bool playing_ = false;
    int current_frame_ = 0;
    int total_frames_ = 0;
    double fps_ = 30.0;
    double playback_speed_ = 1.0;
    std::string filename_;
    std::string last_error_;
    
    // Timing for playback
    int64_t last_frame_time_ = 0;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/video/video_player.h
git commit -m "feat(labeler): add VideoPlayer header with OpenCV wrapper"
```

---

### Task 1.6: Implement App Class

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/app/app.cpp`

- [ ] **Step 1: Create app.cpp with GLFW/ImGui init**

```cpp
#include "app.h"
#include "ui/ui_renderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

namespace sam3_labeler {

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init() {
    // Load config
    try {
        config_ = AppConfig::load(config_path_);
    } catch (const std::exception& e) {
        log("WARNING", std::string("Failed to load config: ") + e.what());
    }
    
    if (!init_glfw()) {
        return false;
    }
    
    if (!init_imgui()) {
        return false;
    }
    
    log("INFO", "Application initialized");
    return true;
}

bool App::init_glfw() {
    if (!glfwInit()) {
        log("ERROR", "Failed to initialize GLFW");
        return false;
    }
    
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    window_ = glfwCreateWindow(window_width_, window_height_, 
                               "SAM3 Video Labeler", nullptr, nullptr);
    if (!window_) {
        log("ERROR", "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // Enable vsync
    
    return true;
}

bool App::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Dark theme matching UI_DESIGN_SPEC.md
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Colors from spec
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.035f, 0.043f, 1.0f);         // #09090b
    colors[ImGuiCol_ChildBg] = ImVec4(0.094f, 0.094f, 0.106f, 1.0f);         // #18181b
    colors[ImGuiCol_Header] = ImVec4(0.153f, 0.153f, 0.165f, 1.0f);          // #27272a
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.153f, 0.153f, 0.165f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.153f, 0.153f, 0.165f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.094f, 0.094f, 0.106f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.153f, 0.153f, 0.165f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.035f, 0.035f, 0.043f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.153f, 0.153f, 0.165f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.737f, 0.490f, 1.0f);          // #00bc7d
    colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.737f, 0.490f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(0.957f, 0.957f, 0.961f, 1.0f);             // #f4f4f5
    colors[ImGuiCol_TextDisabled] = ImVec4(0.620f, 0.620f, 0.667f, 1.0f);    // #9f9fa9
    colors[ImGuiCol_Border] = ImVec4(0.153f, 0.153f, 0.165f, 1.0f);
    
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 8.0f;
    
    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        log("ERROR", "Failed to init ImGui GLFW backend");
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        log("ERROR", "Failed to init ImGui OpenGL backend");
        return false;
    }
    
    return true;
}

void App::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        render_ui();
        
        ImGui::Render();
        
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.035f, 0.035f, 0.043f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window_);
    }
}

void App::render_ui() {
    // Main window - full viewport
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(window_width_, window_height_));
    ImGui::Begin("Main", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | 
                 ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse);
    
    // Three-panel layout from spec
    // Left panel: 256px
    // Right: Canvas area
    // Bottom: Log console (192px expanded, 40px collapsed)
    
    // Placeholder for now
    ImGui::Text("SAM3 Video Labeler");
    ImGui::Text("Config loaded: %zu prompts", config_.prompts.size());
    
    ImGui::End();
}

void App::shutdown() {
    // Save config
    try {
        config_.save(config_path_);
    } catch (...) {}
    
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

void App::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, message);
    }
    std::cout << "[" << level << "] " << message << std::endl;
}

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/app/app.cpp
git commit -m "feat(labeler): implement App class with GLFW/ImGui init and dark theme"
```

---

### Task 1.7: Create Config File

**Files:**
- Create: `SAM3TensorRT/configs/labeler/config.yaml`

- [ ] **Step 1: Create config.yaml**

```yaml
prompts:
  - text: "enemy character"
    class_id: 0
    class_name: "player"
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
  confidence_threshold: 0.45
  iou_threshold: 0.35
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/configs/labeler/config.yaml
git commit -m "feat(labeler): add default config.yaml"
```

---

### Task 1.8: Implement AppConfig YAML Serialization

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/app/app_config.h`

- [ ] **Step 1: Add YAML serialization to app_config.h**

Append after the struct definitions:

```cpp
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace sam3_labeler {

// YAML conversion for PromptConfig
inline YAML::Emitter& operator<<(YAML::Emitter& out, const PromptConfig& p) {
    out << YAML::BeginMap;
    out << YAML::Key << "text" << YAML::Value << p.text;
    out << YAML::Key << "class_id" << YAML::Value << p.class_id;
    out << YAML::Key << "class_name" << YAML::Value << p.class_name;
    out << YAML::Key << "enabled" << YAML::Value << p.enabled;
    out << YAML::EndMap;
    return out;
}

// Implementation of AppConfig methods
inline AppConfig AppConfig::load(const std::string& path) {
    AppConfig config;
    YAML::Node node = YAML::LoadFile(path);
    
    if (node["prompts"]) {
        for (const auto& p : node["prompts"]) {
            PromptConfig pc;
            pc.text = p["text"].as<std::string>();
            pc.class_id = p["class_id"].as<int>();
            pc.class_name = p["class_name"].as<std::string>();
            pc.enabled = p["enabled"].as<bool>(true);
            config.prompts.push_back(pc);
        }
    }
    
    if (node["settings"]) {
        auto& s = node["settings"];
        config.settings.sam3_engine_path = s["sam3_engine_path"].as<std::string>("models/sam3_fp16.engine");
        config.settings.output_dir = s["output_dir"].as<std::string>("scripts/training/datasets/game/");
        config.settings.split = s["split"].as<std::string>("train");
        config.settings.image_format = s["image_format"].as<std::string>(".jpg");
        config.settings.confidence_threshold = s["confidence_threshold"].as<float>(0.45f);
        config.settings.iou_threshold = s["iou_threshold"].as<float>(0.35f);
    }
    
    return config;
}

inline bool AppConfig::save(const std::string& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    
    out << YAML::Key << "prompts" << YAML::Value << YAML::BeginSeq;
    for (const auto& p : prompts) {
        out << p;
    }
    out << YAML::EndSeq;
    
    out << YAML::Key << "settings" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "sam3_engine_path" << YAML::Value << settings.sam3_engine_path;
    out << YAML::Key << "output_dir" << YAML::Value << settings.output_dir;
    out << YAML::Key << "split" << YAML::Value << settings.split;
    out << YAML::Key << "image_format" << YAML::Value << settings.image_format;
    out << YAML::Key << "confidence_threshold" << YAML::Value << settings.confidence_threshold;
    out << YAML::Key << "iou_threshold" << YAML::Value << settings.iou_threshold;
    out << YAML::EndMap;
    
    out << YAML::EndMap;
    
    std::ofstream fout(path);
    fout << out.c_str();
    return fout.good();
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/app/app_config.h
git commit -m "feat(labeler): add YAML serialization for AppConfig"
```

---

### Task 1.9: Create UI Renderer Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/ui_renderer.h`

- [ ] **Step 1: Create ui_renderer.h**

```cpp
#pragma once

#include "panels/capture_panel.h"
#include "panels/labels_panel.h"
#include "panels/settings_panel.h"
#include "panels/log_console.h"
#include "canvas/video_canvas.h"

namespace sam3_labeler {

class App;

class UIRenderer {
public:
    UIRenderer(App& app);
    
    void render();
    
    // Panel state
    bool capture_expanded_ = true;
    bool labels_expanded_ = false;
    bool settings_expanded_ = false;
    bool log_expanded_ = true;
    
private:
    void render_left_panel();
    void render_toolbar();
    void render_canvas();
    void render_log_console();
    
    App& app_;
    
    // Panels
    CapturePanel capture_panel_;
    LabelsPanel labels_panel_;
    SettingsPanel settings_panel_;
    LogConsole log_console_;
    VideoCanvas video_canvas_;
    
    // Layout constants from UI_DESIGN_SPEC.md
    static constexpr int LEFT_PANEL_WIDTH = 256;
    static constexpr int LOG_EXPANDED_HEIGHT = 192;
    static constexpr int LOG_COLLAPSED_HEIGHT = 40;
    static constexpr int TOOLBAR_HEIGHT = 48;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/ui_renderer.h
git commit -m "feat(labeler): add UIRenderer header with layout constants"
```

---

### Task 1.10: Create Panel Headers (Stubs)

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/capture_panel.h`
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/labels_panel.h`
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/settings_panel.h`
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/log_console.h`
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/video_canvas.h`

- [ ] **Step 1: Create capture_panel.h**

```cpp
#pragma once

namespace sam3_labeler {

class App;

class CapturePanel {
public:
    CapturePanel(App& app) : app_(app) {}
    void render();
    
private:
    App& app_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Create labels_panel.h**

```cpp
#pragma once

namespace sam3_labeler {

class App;

class LabelsPanel {
public:
    LabelsPanel(App& app) : app_(app) {}
    void render();
    
private:
    App& app_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 3: Create settings_panel.h**

```cpp
#pragma once

namespace sam3_labeler {

class App;

class SettingsPanel {
public:
    SettingsPanel(App& app) : app_(app) {}
    void render();
    
private:
    App& app_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 4: Create log_console.h**

```cpp
#pragma once

#include <string>
#include <deque>
#include <chrono>

namespace sam3_labeler {

struct LogEntry {
    std::string timestamp;
    std::string level;
    std::string message;
};

class LogConsole {
public:
    void render(bool expanded);
    void add_entry(const std::string& level, const std::string& message);
    
private:
    std::deque<LogEntry> entries_;
    static constexpr size_t MAX_ENTRIES = 100;
    
    std::string format_timestamp();
};

}  // namespace sam3_labeler
```

- [ ] **Step 5: Create video_canvas.h**

```cpp
#pragma once

#include <opencv2/opencv.hpp>

namespace sam3_labeler {

class App;

class VideoCanvas {
public:
    VideoCanvas(App& app) : app_(app) {}
    void render();
    
    void set_frame(const cv::Mat& frame);
    float zoom() const { return zoom_; }
    void set_zoom(float z) { zoom_ = z; }
    
private:
    App& app_;
    cv::Mat current_frame_;
    unsigned int texture_id_ = 0;
    float zoom_ = 1.0f;
    
    void update_texture();
};

}  // namespace sam3_labeler
```

- [ ] **Step 6: Commit all panel headers**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/
git commit -m "feat(labeler): add UI panel stub headers"
```

---

### Task 1.11: Build and Test

- [ ] **Step 1: Configure CMake (if not already configured)**

```bash
cd I:\CppProjects\sunone_aimbot_2
cmake -S SAM3TensorRT -B build/sam3 -G "Visual Studio 18 2026" -A x64 `
  -T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1" `
  -DCMAKE_CUDA_FLAGS="--allow-unsupported-compiler"
```

Note: This uses the same configuration as the existing SAM3 build. If `build/sam3` already exists with SAM3 configured, you can skip this step.

- [ ] **Step 2: Build the labeler**

```bash
cmake --build build/sam3 --config Release --target sam3_labeler
```

Expected: Build succeeds with no errors

- [ ] **Step 3: Run the labeler**

```bash
.\build\sam3\Release\sam3_labeler.exe
```

Expected: Window opens with dark theme, shows "SAM3 Video Labeler" text

- [ ] **Step 4: Commit if working**

```bash
git add -A
git commit -m "feat(labeler): chunk 1 complete - basic app window with ImGui"
```

---

## Chunk 2: SAM3 Integration

### Task 2.1: Create SAM3 Labeler Backend Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/inference/sam3_labeler_backend.h`

- [ ] **Step 1: Create sam3_labeler_backend.h**

```cpp
#pragma once

#include <sam3.cuh>  // Existing SAM3 TRT interface
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>

namespace sam3_labeler {

struct DetectedBox {
    int class_id;
    cv::Rect2f bbox;  // Normalized 0-1
    float confidence;
};

class Sam3LabelerBackend {
public:
    Sam3LabelerBackend() = default;
    ~Sam3LabelerBackend() = default;
    
    bool initialize(const std::string& engine_path);
    std::vector<DetectedBox> infer(const cv::Mat& frame, const std::string& prompt);
    
    bool is_initialized() const { return initialized_; }
    std::string last_error() const { return last_error_; }
    
private:
    std::unique_ptr<SAM3_PCS> sam3_;
    bool initialized_ = false;
    std::string last_error_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/inference/sam3_labeler_backend.h
git commit -m "feat(labeler): add Sam3LabelerBackend wrapper header"
```

---

### Task 2.2: Create Multi-Prompt Runner Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/inference/multi_prompt_runner.h`

- [ ] **Step 1: Create multi_prompt_runner.h**

```cpp
#pragma once

#include "sam3_labeler_backend.h"
#include "../app/app_config.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <functional>

namespace sam3_labeler {

struct PredictionSet {
    std::vector<DetectedBox> boxes;
    int prompts_run = 0;
    int prompts_skipped = 0;
    std::string error;
    bool success = false;
};

using ProgressCallback = std::function<void(int current, int total, const std::string& prompt)>;

class MultiPromptRunner {
public:
    MultiPromptRunner() = default;
    
    bool initialize(const std::string& engine_path);
    PredictionSet run(const cv::Mat& frame, 
                      const std::vector<PromptConfig>& prompts);
    
    void set_progress_callback(ProgressCallback cb) { progress_cb_ = cb; }
    
    bool is_initialized() const { return backend_.is_initialized(); }
    std::string last_error() const { return last_error_; }
    
private:
    Sam3LabelerBackend backend_;
    ProgressCallback progress_cb_;
    std::string last_error_;
    
    // Merge overlapping boxes from different prompts (optional)
    std::vector<DetectedBox> merge_boxes(const std::vector<DetectedBox>& boxes);
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Implement multi_prompt_runner.h**

Add implementation after the class definition:

```cpp
// Implementation

inline bool MultiPromptRunner::initialize(const std::string& engine_path) {
    if (!backend_.initialize(engine_path)) {
        last_error_ = backend_.last_error();
        return false;
    }
    return true;
}

inline PredictionSet MultiPromptRunner::run(
    const cv::Mat& frame,
    const std::vector<PromptConfig>& prompts) {
    
    PredictionSet result;
    
    if (!backend_.is_initialized()) {
        result.error = "Backend not initialized";
        return result;
    }
    
    std::vector<DetectedBox> all_boxes;
    int prompt_idx = 0;
    
    for (const auto& prompt : prompts) {
        if (!prompt.enabled) {
            result.prompts_skipped++;
            prompt_idx++;
            continue;
        }
        
        if (progress_cb_) {
            progress_cb_(prompt_idx, static_cast<int>(prompts.size()), prompt.text);
        }
        
        auto boxes = backend_.infer(frame, prompt.text);
        
        // Assign class_id from prompt config
        for (auto& box : boxes) {
            box.class_id = prompt.class_id;
        }
        
        all_boxes.insert(all_boxes.end(), boxes.begin(), boxes.end());
        result.prompts_run++;
        prompt_idx++;
    }
    
    // Optional: merge overlapping boxes
    result.boxes = merge_boxes(all_boxes);
    result.success = true;
    
    return result;
}

inline std::vector<DetectedBox> MultiPromptRunner::merge_boxes(
    const std::vector<DetectedBox>& boxes) {
    // For now, just return all boxes
    // TODO: Implement NMS if needed
    return boxes;
}

}  // namespace sam3_labeler
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/inference/multi_prompt_runner.h
git commit -m "feat(labeler): add MultiPromptRunner for sequential inference"
```

---

### Task 2.3: Create Box Editor Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/box_editor.h`

- [ ] **Step 1: Create box_editor.h**

```cpp
#pragma once

#include "../inference/sam3_labeler_backend.h"
#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <vector>
#include <deque>

namespace sam3_labeler {

enum class EditorState {
    None,
    Hover,
    Selected,
    Moving,
    Resizing,
    Creating
};

struct EditableBox {
    int class_id = 0;
    cv::Rect2f rect;  // Pixel coordinates
    float confidence = 1.0f;
};

class BoxEditor {
public:
    BoxEditor() = default;
    
    // Render boxes on canvas
    void render(ImDrawList* draw_list, const ImVec2& canvas_pos, 
                const ImVec2& canvas_size, float zoom, int img_width, int img_height);
    
    // Input handling
    bool handle_mouse(const ImVec2& mouse_pos, bool clicked, bool dragging, 
                      const ImVec2& canvas_pos, float zoom);
    bool handle_keyboard(int key);
    
    // Data access
    std::vector<DetectedBox> get_boxes_normalized(int img_width, int img_height) const;
    void set_boxes_from_normalized(const std::vector<DetectedBox>& boxes, 
                                    int img_width, int img_height);
    void clear();
    bool empty() const { return boxes_.empty(); }
    size_t count() const { return boxes_.size(); }
    
    // Selection
    int selected_index() const { return selected_idx_; }
    void delete_selected();
    
    // State
    EditorState state() const { return state_; }
    
    // Undo/redo
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }
    void undo();
    void redo();
    
    // Class for new boxes
    void set_active_class(int class_id) { active_class_ = class_id; }
    int active_class() const { return active_class_; }
    
    // Class colors (matches game.yaml)
    static ImU32 get_class_color(int class_id);
    
private:
    std::vector<EditableBox> boxes_;
    int selected_idx_ = -1;
    EditorState state_ = EditorState::None;
    int active_class_ = 0;
    
    // For resize/move
    int resize_handle_ = -1;  // 0-3: corners, 4-7: edges
    cv::Rect2f drag_start_rect_;
    ImVec2 drag_start_pos_;
    
    // Undo/redo stacks
    std::deque<std::vector<EditableBox>> undo_stack_;
    std::deque<std::vector<EditableBox>> redo_stack_;
    static constexpr size_t MAX_HISTORY = 50;
    
    void push_undo();
    int hit_test(const ImVec2& pos, const cv::Rect2f& rect, float zoom);
    int hit_test_handle(const ImVec2& pos, const cv::Rect2f& rect, float zoom);
    cv::Rect2f pixel_to_canvas(const cv::Rect2f& rect, const ImVec2& canvas_pos, float zoom);
    ImVec2 canvas_to_pixel(const ImVec2& pos, const ImVec2& canvas_pos, float zoom);
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/box_editor.h
git commit -m "feat(labeler): add BoxEditor header with selection/undo support"
```

---

## Chunk 3: YOLO Export

### Task 3.1: Create YOLO Exporter Header

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/export/yolo_exporter.h`

- [ ] **Step 1: Create yolo_exporter.h**

```cpp
#pragma once

#include "../inference/sam3_labeler_backend.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace sam3_labeler {

enum class DatasetSplit { Train, Val };

class YOLOExporter {
public:
    bool export_frame(
        const cv::Mat& frame,
        const std::vector<DetectedBox>& boxes,
        const std::string& output_dir,
        DatasetSplit split,
        const std::string& filename_base
    );
    
    std::string last_error() const { return last_error_; }
    
    static bool file_exists(const std::string& path);
    static std::string generate_filename(int counter);
    
private:
    std::string last_error_;
    
    std::string get_image_path(const std::string& output_dir, DatasetSplit split, 
                                const std::string& filename, const std::string& ext);
    std::string get_label_path(const std::string& output_dir, DatasetSplit split,
                                const std::string& filename);
    bool ensure_directory_exists(const std::string& path);
};

// Implementation

inline bool YOLOExporter::export_frame(
    const cv::Mat& frame,
    const std::vector<DetectedBox>& boxes,
    const std::string& output_dir,
    DatasetSplit split,
    const std::string& filename_base) {
    
    // Determine paths based on split
    std::string img_ext = ".jpg";  // TODO: get from config
    std::string img_path = get_image_path(output_dir, split, filename_base, img_ext);
    std::string label_path = get_label_path(output_dir, split, filename_base);
    
    // Ensure directories exist
    if (!ensure_directory_exists(img_path) || !ensure_directory_exists(label_path)) {
        last_error_ = "Failed to create output directories";
        return false;
    }
    
    // Check for existing file
    if (file_exists(img_path)) {
        // TODO: Prompt user for overwrite confirmation
        // For now, just overwrite
    }
    
    // Write image
    if (!cv::imwrite(img_path, frame)) {
        last_error_ = "Failed to write image: " + img_path;
        return false;
    }
    
    // Write label file
    std::ofstream fout(label_path);
    if (!fout.is_open()) {
        last_error_ = "Failed to open label file: " + label_path;
        return false;
    }
    
    for (const auto& box : boxes) {
        // YOLO format: class_id x_center y_center width height
        fout << box.class_id << " "
             << box.bbox.x << " "
             << box.bbox.y << " "
             << box.bbox.width << " "
             << box.bbox.height << "\n";
    }
    
    fout.close();
    return true;
}

inline std::string YOLOExporter::get_image_path(
    const std::string& output_dir, DatasetSplit split,
    const std::string& filename, const std::string& ext) {
    
    if (split == DatasetSplit::Train) {
        return output_dir + "/images/" + filename + ext;
    } else {
        return output_dir + "/val/images/" + filename + ext;
    }
}

inline std::string YOLOExporter::get_label_path(
    const std::string& output_dir, DatasetSplit split,
    const std::string& filename) {
    
    if (split == DatasetSplit::Train) {
        return output_dir + "/labels/" + filename + ".txt";
    } else {
        return output_dir + "/val/labels/" + filename + ".txt";
    }
}

inline bool YOLOExporter::ensure_directory_exists(const std::string& filepath) {
    // Extract directory from filepath
    size_t last_slash = filepath.find_last_of("/\\");
    if (last_slash == std::string::npos) return true;
    
    std::string dir = filepath.substr(0, last_slash);
    
    // Create directory if needed (platform-specific)
#ifdef _WIN32
    return _mkdir(dir.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

inline bool YOLOExporter::file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

inline std::string YOLOExporter::generate_filename(int counter) {
    char buf[32];
    snprintf(buf, sizeof(buf), "labeler_%06d", counter);
    return std::string(buf);
}

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/export/yolo_exporter.h
git commit -m "feat(labeler): add YOLOExporter with train/val split support"
```

---

## Chunk 4: Panel Implementations

### Task 4.1: Implement Capture Panel

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/capture_panel.h`

- [ ] **Step 1: Implement capture panel render**

Replace stub with implementation:

```cpp
#pragma once

#include <imgui.h>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace sam3_labeler {

class App;

class CapturePanel {
public:
    CapturePanel(App& app) : app_(app) {}
    void render();
    
private:
    App& app_;
    
    void render_video_controls();
    void render_file_browser();
    std::string open_file_dialog();
};

inline void CapturePanel::render() {
    // Video file section
    ImGui::Text("Video File");
    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
    if (ImGui::Button("Browse...")) {
        std::string path = open_file_dialog();
        if (!path.empty()) {
            // Load video via app
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Video controls
    render_video_controls();
}

inline void CapturePanel::render_video_controls() {
    // Play/Pause button
    if (ImGui::Button("Play")) {
        // app_.video_player().play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) {
        // app_.video_player().pause();
    }
    
    // Frame stepping
    ImGui::SameLine();
    if (ImGui::Button("<")) {
        // app_.video_player().step_backward();
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        // app_.video_player().step_forward();
    }
    
    ImGui::Spacing();
    
    // Frame counter
    // ImGui::Text("Frame: %d / %d", app_.video_player().current_frame(), app_.video_player().total_frames());
    
    // Seek bar
    // float progress = (float)current_frame / total_frames;
    // ImGui::SliderFloat("##seek", &progress, 0.0f, 1.0f, "");
    
    ImGui::Spacing();
    
    // Speed selector
    const char* speeds[] = { "0.5x", "1.0x", "2.0x" };
    static int speed_idx = 1;
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("Speed", &speed_idx, speeds, IM_ARRAYSIZE(speeds));
}

inline std::string CapturePanel::open_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Video Files\0*.mp4;*.avi;*.mkv\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/capture_panel.h
git commit -m "feat(labeler): implement CapturePanel with video controls"
```

---

### Task 4.2: Implement Labels Panel

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/labels_panel.h`

- [ ] **Step 1: Implement labels panel**

```cpp
#pragma once

#include <imgui.h>
#include <vector>

namespace sam3_labeler {

class App;
struct PromptConfig;

class LabelsPanel {
public:
    LabelsPanel(App& app) : app_(app) {}
    void render();
    
private:
    App& app_;
    
    void render_prompt_list();
    void render_prompt_editor();
    
    // Editor state
    bool show_editor_ = false;
    int editing_idx_ = -1;
    char prompt_text_buf_[256] = "";
    int selected_class_idx_ = 0;
    bool editor_enabled_ = true;
};

inline void LabelsPanel::render() {
    // Prompt list
    render_prompt_list();
    
    ImGui::Spacing();
    
    // Add/Edit/Remove buttons
    if (ImGui::Button("Add")) {
        editing_idx_ = -1;
        prompt_text_buf_[0] = '\0';
        selected_class_idx_ = 0;
        editor_enabled_ = true;
        show_editor_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Edit")) {
        // Open editor for selected prompt
        show_editor_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        // Remove selected prompt
    }
    
    // Prompt editor popup
    if (show_editor_) {
        render_prompt_editor();
    }
}

inline void LabelsPanel::render_prompt_list() {
    // ImGui::Text("Prompts");
    // ImGui::Separator();
    
    // For each prompt in config:
    // - Checkbox for enabled
    // - Text: "prompt_text -> class_name"
    // - Click to select
    
    // Placeholder
    ImGui::Text("No prompts configured");
    ImGui::Text("Click 'Add' to create a prompt");
}

inline void LabelsPanel::render_prompt_editor() {
    ImGui::OpenPopup("Edit Prompt");
    
    if (ImGui::BeginPopupModal("Edit Prompt", &show_editor_)) {
        ImGui::Text("Prompt Text:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##prompt", prompt_text_buf_, sizeof(prompt_text_buf_));
        
        ImGui::Spacing();
        
        ImGui::Text("Class:");
        // Class dropdown from classes.txt
        const char* classes[] = { "player", "bot", "weapon", "outline", 
                                  "dead_body", "hideout_target_human", 
                                  "hideout_target_balls", "head", 
                                  "smoke", "fire", "third_person" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##class", &selected_class_idx_, classes, IM_ARRAYSIZE(classes));
        
        ImGui::Spacing();
        
        ImGui::Checkbox("Enabled", &editor_enabled_);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("OK", ImVec2(80, 0))) {
            // Save prompt
            show_editor_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            show_editor_ = false;
        }
        
        ImGui::EndPopup();
    }
}

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/labels_panel.h
git commit -m "feat(labeler): implement LabelsPanel with prompt editor"
```

---

### Task 4.3: Implement Settings Panel

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/settings_panel.h`

- [ ] **Step 1: Implement settings panel**

```cpp
#pragma once

#include <imgui.h>

namespace sam3_labeler {

class App;

class SettingsPanel {
public:
    SettingsPanel(App& app) : app_(app) {}
    void render();
    
private:
    App& app_;
    
    char output_dir_buf_[512] = "";
};

inline void SettingsPanel::render() {
    // Output directory
    ImGui::Text("Output Directory:");
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##output", output_dir_buf_, sizeof(output_dir_buf_));
    ImGui::SameLine();
    if (ImGui::Button("...##output_browse")) {
        // Open folder browser
    }
    
    ImGui::Spacing();
    
    // Dataset split
    ImGui::Text("Dataset Split:");
    const char* splits[] = { "train", "val" };
    static int split_idx = 0;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##split", &split_idx, splits, IM_ARRAYSIZE(splits));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Confidence threshold
    static float confidence = 0.45f;
    ImGui::Text("Confidence: %.2f", confidence);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##confidence", &confidence, 0.1f, 0.99f, "");
    
    // IoU threshold
    static float iou = 0.35f;
    ImGui::Text("IoU Threshold: %.2f", iou);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##iou", &iou, 0.1f, 0.95f, "");
    
    ImGui::Spacing();
    
    // Image format
    ImGui::Text("Image Format:");
    const char* formats[] = { ".jpg", ".png" };
    static int format_idx = 0;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##format", &format_idx, formats, IM_ARRAYSIZE(formats));
}

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/settings_panel.h
git commit -m "feat(labeler): implement SettingsPanel with threshold sliders"
```

---

### Task 4.4: Implement Log Console

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/log_console.h`

- [ ] **Step 1: Implement log console**

```cpp
#pragma once

#include <imgui.h>
#include <string>
#include <deque>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace sam3_labeler {

struct LogEntry {
    std::string timestamp;
    std::string level;
    std::string message;
};

class LogConsole {
public:
    void render(bool expanded) {
        // Header
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.094f, 0.094f, 0.106f, 1.0f));
        
        float height = expanded ? 192.0f : 40.0f;
        ImGui::BeginChild("LogConsole", ImVec2(0, height), true);
        
        // Header row
        ImGui::Text("Log Console");
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu entries)", entries_.size());
        
        ImGui::SameLine(ImGui::GetWindowWidth() - 50);
        const char* btn_text = expanded ? "v" : "^";
        if (ImGui::Button(btn_text, ImVec2(24, 0))) {
            // Toggle expanded (handled by parent)
        }
        
        if (expanded) {
            ImGui::Separator();
            
            // Log entries
            ImGui::BeginChild("LogEntries", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            
            for (const auto& entry : entries_) {
                // Timestamp
                ImGui::TextColored(ImVec4(0.322f, 0.322f, 0.361f, 1.0f), "[%s]", entry.timestamp.c_str());
                ImGui::SameLine();
                
                // Level with color
                ImVec4 level_color;
                if (entry.level == "INFO") {
                    level_color = ImVec4(0.318f, 0.635f, 1.0f, 1.0f);  // Blue
                } else if (entry.level == "SUCCESS") {
                    level_color = ImVec4(0.02f, 0.875f, 0.447f, 1.0f);  // Green
                } else if (entry.level == "WARNING") {
                    level_color = ImVec4(0.992f, 0.780f, 0.0f, 1.0f);  // Yellow
                } else if (entry.level == "ERROR") {
                    level_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
                } else {
                    level_color = ImVec4(0.831f, 0.831f, 0.847f, 1.0f);
                }
                
                ImGui::TextColored(level_color, "%s", entry.level.c_str());
                ImGui::SameLine();
                
                // Message
                ImGui::Text("%s", entry.message.c_str());
            }
            
            // Auto-scroll to bottom
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            
            ImGui::EndChild();
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    
    void add_entry(const std::string& level, const std::string& message) {
        LogEntry entry;
        entry.timestamp = format_timestamp();
        entry.level = level;
        entry.message = message;
        
        entries_.push_back(entry);
        
        // Keep only last MAX_ENTRIES
        while (entries_.size() > MAX_ENTRIES) {
            entries_.pop_front();
        }
    }
    
private:
    std::deque<LogEntry> entries_;
    static constexpr size_t MAX_ENTRIES = 100;
    
    std::string format_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S");
        return ss.str();
    }
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/panels/log_console.h
git commit -m "feat(labeler): implement LogConsole with colored levels"
```

---

## Chunk 5: VideoPlayer and VideoCanvas Implementation

### Task 5.1: Implement VideoPlayer

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/video/video_player.h`

- [ ] **Step 1: Add VideoPlayer implementation**

Replace the stub with full implementation:

```cpp
#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>

namespace sam3_labeler {

class VideoPlayer {
public:
    VideoPlayer() = default;
    ~VideoPlayer() { close(); }
    
    bool load(const std::string& path) {
        close();
        
        capture_.open(path);
        if (!capture_.isOpened()) {
            last_error_ = "Failed to open video: " + path;
            return false;
        }
        
        total_frames_ = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_COUNT));
        fps_ = capture_.get(cv::CAP_PROP_FPS);
        current_frame_ = 0;
        filename_ = path;
        loaded_ = true;
        
        // Extract filename for display
        size_t last_sep = path.find_last_of("/\\");
        if (last_sep != std::string::npos) {
            display_name_ = path.substr(last_sep + 1);
        } else {
            display_name_ = path;
        }
        
        return true;
    }
    
    void close() {
        if (capture_.isOpened()) {
            capture_.release();
        }
        loaded_ = false;
        playing_ = false;
        current_frame_ = 0;
        total_frames_ = 0;
        filename_.clear();
        display_name_.clear();
    }
    
    void play() {
        if (loaded_) {
            playing_ = true;
            last_frame_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        }
    }
    
    void pause() {
        playing_ = false;
    }
    
    void step_forward() {
        if (!loaded_) return;
        pause();
        if (current_frame_ < total_frames_ - 1) {
            current_frame_++;
            capture_.set(cv::CAP_PROP_POS_FRAMES, current_frame_);
        }
    }
    
    void step_backward() {
        if (!loaded_) return;
        pause();
        if (current_frame_ > 0) {
            current_frame_--;
            capture_.set(cv::CAP_PROP_POS_FRAMES, current_frame_);
        }
    }
    
    void seek(int frame_number) {
        if (!loaded_) return;
        pause();
        frame_number = std::max(0, std::min(frame_number, total_frames_ - 1));
        current_frame_ = frame_number;
        capture_.set(cv::CAP_PROP_POS_FRAMES, current_frame_);
    }
    
    bool get_frame(cv::Mat& frame) {
        if (!loaded_) return false;
        
        if (playing_) {
            // Check timing for playback speed
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            
            double frame_duration_ms = (1000.0 / fps_) / playback_speed_;
            
            if (now - last_frame_time_ >= frame_duration_ms) {
                if (current_frame_ < total_frames_ - 1) {
                    current_frame_++;
                    capture_.read(frame);
                    last_frame_time_ = now;
                } else {
                    playing_ = false;  // End of video
                    return false;
                }
            } else {
                // Return cached frame
                capture_.set(cv::CAP_PROP_POS_FRAMES, current_frame_);
                capture_.read(frame);
            }
        } else {
            // Paused - read current frame
            capture_.set(cv::CAP_PROP_POS_FRAMES, current_frame_);
            capture_.read(frame);
        }
        
        return !frame.empty();
    }
    
    // Properties
    bool is_playing() const { return playing_; }
    int current_frame() const { return current_frame_; }
    int total_frames() const { return total_frames_; }
    double fps() const { return fps_; }
    bool is_loaded() const { return loaded_; }
    std::string last_error() const { return last_error_; }
    std::string filename() const { return filename_; }
    std::string display_name() const { return display_name_; }
    cv::Size frame_size() const {
        if (!loaded_) return cv::Size();
        return cv::Size(
            static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH)),
            static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT))
        );
    }
    
    // Speed control
    void set_speed(double speed) { playback_speed_ = std::max(0.25, std::min(4.0, speed)); }
    double speed() const { return playback_speed_; }
    
private:
    cv::VideoCapture capture_;
    bool loaded_ = false;
    bool playing_ = false;
    int current_frame_ = 0;
    int total_frames_ = 0;
    double fps_ = 30.0;
    double playback_speed_ = 1.0;
    std::string filename_;
    std::string display_name_;
    std::string last_error_;
    int64_t last_frame_time_ = 0;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/video/video_player.h
git commit -m "feat(labeler): implement VideoPlayer with playback control"
```

---

### Task 5.2: Implement VideoCanvas with Texture Rendering

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/video_canvas.h`

- [ ] **Step 1: Implement VideoCanvas with OpenGL texture**

```cpp
#pragma once

#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <GL/gl3w.h>

namespace sam3_labeler {

class App;

class VideoCanvas {
public:
    VideoCanvas(App& app) : app_(app) {}
    ~VideoCanvas() {
        if (texture_id_) {
            glDeleteTextures(1, &texture_id_);
        }
    }
    
    void render() {
        // Render video frame texture
        if (texture_id_ && !current_frame_.empty()) {
            ImGui::Image(
                reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture_id_)),
                ImVec2(current_frame_.cols * zoom_, current_frame_.rows * zoom_)
            );
        } else {
            // Placeholder when no video loaded
            ImGui::BeginChild("NoVideo", ImVec2(640 * zoom_, 480 * zoom_), true);
            ImGui::Text("No video loaded");
            ImGui::Text("Use the Capture panel to load a video file");
            ImGui::EndChild();
        }
    }
    
    void set_frame(const cv::Mat& frame) {
        if (frame.empty()) return;
        
        // Convert BGR to RGB
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        
        // Create or update texture
        if (!texture_id_) {
            glGenTextures(1, &texture_id_);
        }
        
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 
                     0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
        
        current_frame_ = frame.clone();
    }
    
    float zoom() const { return zoom_; }
    void set_zoom(float z) { zoom_ = std::max(0.1f, std::min(4.0f, z)); }
    void zoom_in() { set_zoom(zoom_ * 1.25f); }
    void zoom_out() { set_zoom(zoom_ / 1.25f); }
    void zoom_fit(int canvas_width, int canvas_height) {
        if (current_frame_.empty()) return;
        float scale_x = static_cast<float>(canvas_width) / current_frame_.cols;
        float scale_y = static_cast<float>(canvas_height) / current_frame_.rows;
        set_zoom(std::min(scale_x, scale_y));
    }
    
    cv::Size frame_size() const {
        return current_frame_.empty() ? cv::Size() : current_frame_.size();
    }
    
private:
    App& app_;
    cv::Mat current_frame_;
    unsigned int texture_id_ = 0;
    float zoom_ = 1.0f;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Add gl3w to CMakeLists.txt**

Modify `SAM3TensorRT/cpp/apps/sam3_labeler/CMakeLists.txt` to add gl3w:

```cmake
# Add gl3w for OpenGL loading
set(GL3W_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gl3w")
set(GL3W_SOURCES
    "${GL3W_DIR}/src/gl3w.c"
)

add_executable(sam3_labeler ${LABELER_SOURCES} ${IMGUI_SOURCES} ${GL3W_SOURCES})

target_include_directories(sam3_labeler PRIVATE
    ${GL3W_DIR}/include
    # ... existing includes
)
```

- [ ] **Step 3: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/video_canvas.h SAM3TensorRT/cpp/apps/sam3_labeler/CMakeLists.txt
git commit -m "feat(labeler): implement VideoCanvas with OpenGL texture rendering"
```

---

## Chunk 6: SAM3LabelerBackend Full Implementation

### Task 6.1: Implement SAM3LabelerBackend with SAM3_PCS Integration

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/inference/sam3_labeler_backend.h`

- [ ] **Step 1: Implement SAM3LabelerBackend**

```cpp
#pragma once

#include <sam3.cuh>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace sam3_labeler {

struct DetectedBox {
    int class_id;
    cv::Rect2f bbox;  // Normalized 0-1
    float confidence;
};

class Sam3LabelerBackend {
public:
    Sam3LabelerBackend() = default;
    ~Sam3LabelerBackend() = default;
    
    bool initialize(const std::string& engine_path) {
        // Resolve path relative to project root
        std::string resolved_path = engine_path;
        if (!std::filesystem::exists(resolved_path)) {
            // Try relative to build directory
            resolved_path = "../../../" + engine_path;
        }
        
        try {
            sam3_ = std::make_unique<SAM3_PCS>();
            if (!sam3_->init(resolved_path)) {
                last_error_ = "Failed to initialize SAM3 with engine: " + resolved_path;
                sam3_.reset();
                return false;
            }
            initialized_ = true;
            return true;
        } catch (const std::exception& e) {
            last_error_ = std::string("SAM3 init exception: ") + e.what();
            return false;
        }
    }
    
    std::vector<DetectedBox> infer(const cv::Mat& frame, const std::string& prompt) {
        std::vector<DetectedBox> result;
        
        if (!initialized_ || !sam3_) {
            last_error_ = "Backend not initialized";
            return result;
        }
        
        if (frame.empty()) {
            last_error_ = "Empty frame";
            return result;
        }
        
        try {
            // Run SAM3 inference
            SAM3_PCS_RESULT sam3_result;
            sam3_->infer_on_image(frame, prompt, sam3_result, SAM3_VISUALIZATION::VIS_NONE);
            
            // Convert to normalized boxes
            int img_width = frame.cols;
            int img_height = frame.rows;
            
            for (const auto& box : sam3_result.boxes) {
                DetectedBox db;
                db.class_id = 0;  // Will be set by caller
                db.confidence = box.confidence;
                
                // Convert pixel coordinates to normalized 0-1
                db.bbox.x = (box.x1 + box.x2) / 2.0f / img_width;  // x_center
                db.bbox.y = (box.y1 + box.y2) / 2.0f / img_height; // y_center
                db.bbox.width = (box.x2 - box.x1) / img_width;
                db.bbox.height = (box.y2 - box.y1) / img_height;
                
                result.push_back(db);
            }
            
            last_error_.clear();
            
        } catch (const std::exception& e) {
            last_error_ = std::string("Inference exception: ") + e.what();
        }
        
        return result;
    }
    
    bool is_initialized() const { return initialized_; }
    std::string last_error() const { return last_error_; }
    
private:
    std::unique_ptr<SAM3_PCS> sam3_;
    bool initialized_ = false;
    std::string last_error_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/inference/sam3_labeler_backend.h
git commit -m "feat(labeler): implement SAM3LabelerBackend with SAM3_PCS integration"
```

---

## Chunk 7: BoxEditor Full Implementation

### Task 7.1: Implement BoxEditor with Full Interaction Support

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/box_editor.h`

- [ ] **Step 1: Implement BoxEditor**

```cpp
#pragma once

#include "../inference/sam3_labeler_backend.h"
#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <vector>
#include <deque>
#include <algorithm>

namespace sam3_labeler {

enum class EditorState {
    None,
    Hover,
    Selected,
    Moving,
    Resizing_NW, Resizing_NE, Resizing_SW, Resizing_SE,
    Resizing_N, Resizing_S, Resizing_W, Resizing_E,
    Creating
};

struct EditableBox {
    int class_id = 0;
    cv::Rect2f rect;  // Pixel coordinates
    float confidence = 1.0f;
};

class BoxEditor {
public:
    BoxEditor() = default;
    
    void render(ImDrawList* draw_list, const ImVec2& canvas_pos, 
                const ImVec2& canvas_size, float zoom, int img_width, int img_height) {
        
        if (img_width <= 0 || img_height <= 0) return;
        
        // Draw all boxes
        for (size_t i = 0; i < boxes_.size(); i++) {
            const auto& box = boxes_[i];
            
            // Convert to canvas coordinates
            ImVec2 p1(canvas_pos.x + box.rect.x * zoom, 
                      canvas_pos.y + box.rect.y * zoom);
            ImVec2 p2(canvas_pos.x + (box.rect.x + box.rect.width) * zoom,
                      canvas_pos.y + (box.rect.y + box.rect.height) * zoom);
            
            ImU32 color = get_class_color(box.class_id);
            float thickness = (i == selected_idx_) ? 3.0f : 2.0f;
            
            draw_list->AddRect(p1, p2, color, 0.0f, 0, thickness);
            
            // Draw class label
            ImVec2 text_pos(p1.x + 4, p1.y + 2);
            draw_list->AddText(text_pos, color, get_class_name(box.class_id).c_str());
            
            // Draw resize handles for selected box
            if (i == selected_idx_) {
                draw_resize_handles(draw_list, p1, p2, zoom);
            }
        }
    }
    
    bool handle_mouse(const ImVec2& mouse_pos, bool clicked, bool dragging,
                      const ImVec2& canvas_pos, float zoom, int img_width, int img_height) {
        
        if (img_width <= 0 || img_height <= 0) return false;
        
        // Convert mouse to pixel coordinates
        float px = (mouse_pos.x - canvas_pos.x) / zoom;
        float py = (mouse_pos.y - canvas_pos.y) / zoom;
        
        if (clicked && state_ == EditorState::None) {
            // Check for handle hit first (if box selected)
            if (selected_idx_ >= 0 && selected_idx_ < boxes_.size()) {
                int handle = hit_test_handle(mouse_pos, boxes_[selected_idx_].rect, canvas_pos, zoom);
                if (handle >= 0) {
                    state_ = static_cast<EditorState>(static_cast<int>(EditorState::Resizing_NW) + handle);
                    drag_start_rect_ = boxes_[selected_idx_].rect;
                    drag_start_pos_ = mouse_pos;
                    push_undo();
                    return true;
                }
            }
            
            // Check for box hit
            for (int i = static_cast<int>(boxes_.size()) - 1; i >= 0; i--) {
                if (boxes_[i].rect.contains(cv::Point2f(px, py))) {
                    selected_idx_ = i;
                    state_ = EditorState::Selected;
                    return true;
                }
            }
            
            // Start creating new box
            if (px >= 0 && px < img_width && py >= 0 && py < img_height) {
                push_undo();
                EditableBox new_box;
                new_box.class_id = active_class_;
                new_box.rect = cv::Rect2f(px, py, 0, 0);
                boxes_.push_back(new_box);
                selected_idx_ = static_cast<int>(boxes_.size()) - 1;
                state_ = EditorState::Creating;
                create_start_ = cv::Point2f(px, py);
                return true;
            }
            
            // Clicked outside - deselect
            selected_idx_ = -1;
            return true;
        }
        
        if (dragging) {
            if (state_ == EditorState::Creating && selected_idx_ >= 0) {
                // Update box dimensions
                float x1 = std::min(create_start_.x, px);
                float y1 = std::min(create_start_.y, py);
                float x2 = std::max(create_start_.x, px);
                float y2 = std::max(create_start_.y, py);
                
                boxes_[selected_idx_].rect = cv::Rect2f(x1, y1, x2 - x1, y2 - y1);
                return true;
            }
            
            if (state_ == EditorState::Selected) {
                // Started dragging selected box
                state_ = EditorState::Moving;
                drag_start_pos_ = mouse_pos;
                drag_start_rect_ = boxes_[selected_idx_].rect;
                return true;
            }
            
            if (state_ == EditorState::Moving && selected_idx_ >= 0) {
                // Move the box
                float dx = (mouse_pos.x - drag_start_pos_.x) / zoom;
                float dy = (mouse_pos.y - drag_start_pos_.y) / zoom;
                boxes_[selected_idx_].rect = drag_start_rect_ + cv::Point2f(dx, dy);
                return true;
            }
            
            if (is_resize_state() && selected_idx_ >= 0) {
                // Handle resize
                handle_resize(mouse_pos, zoom, img_width, img_height);
                return true;
            }
        }
        
        if (!dragging && !clicked) {
            // Mouse released
            if (state_ == EditorState::Creating) {
                // Remove if too small
                if (boxes_[selected_idx_].rect.width < 10 || boxes_[selected_idx_].rect.height < 10) {
                    boxes_.erase(boxes_.begin() + selected_idx_);
                    selected_idx_ = -1;
                }
            }
            state_ = EditorState::None;
        }
        
        return false;
    }
    
    bool handle_keyboard(int key) {
        if (key == ImGuiKey_Delete || key == ImGuiKey_Backspace) {
            delete_selected();
            return true;
        }
        if (key == ImGuiKey_Escape) {
            selected_idx_ = -1;
            state_ = EditorState::None;
            return true;
        }
        if (key == ImGuiKey_Z && ImGui::GetIO().KeyCtrl) {
            if (ImGui::GetIO().KeyShift) {
                redo();
            } else {
                undo();
            }
            return true;
        }
        return false;
    }
    
    std::vector<DetectedBox> get_boxes_normalized(int img_width, int img_height) const {
        std::vector<DetectedBox> result;
        for (const auto& box : boxes_) {
            DetectedBox db;
            db.class_id = box.class_id;
            db.confidence = box.confidence;
            db.bbox.x = (box.rect.x + box.rect.width / 2.0f) / img_width;   // x_center
            db.bbox.y = (box.rect.y + box.rect.height / 2.0f) / img_height; // y_center
            db.bbox.width = box.rect.width / img_width;
            db.bbox.height = box.rect.height / img_height;
            result.push_back(db);
        }
        return result;
    }
    
    void set_boxes_from_normalized(const std::vector<DetectedBox>& boxes, 
                                    int img_width, int img_height) {
        push_undo();
        boxes_.clear();
        for (const auto& db : boxes) {
            EditableBox eb;
            eb.class_id = db.class_id;
            eb.confidence = db.confidence;
            eb.rect.x = (db.bbox.x - db.bbox.width / 2.0f) * img_width;
            eb.rect.y = (db.bbox.y - db.bbox.height / 2.0f) * img_height;
            eb.rect.width = db.bbox.width * img_width;
            eb.rect.height = db.bbox.height * img_height;
            boxes_.push_back(eb);
        }
    }
    
    void clear() { push_undo(); boxes_.clear(); selected_idx_ = -1; }
    bool empty() const { return boxes_.empty(); }
    size_t count() const { return boxes_.size(); }
    int selected_index() const { return selected_idx_; }
    
    void delete_selected() {
        if (selected_idx_ >= 0 && selected_idx_ < boxes_.size()) {
            push_undo();
            boxes_.erase(boxes_.begin() + selected_idx_);
            selected_idx_ = -1;
        }
    }
    
    EditorState state() const { return state_; }
    
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }
    
    void undo() {
        if (!undo_stack_.empty()) {
            redo_stack_.push_back(boxes_);
            boxes_ = undo_stack_.back();
            undo_stack_.pop_back();
            selected_idx_ = -1;
        }
    }
    
    void redo() {
        if (!redo_stack_.empty()) {
            undo_stack_.push_back(boxes_);
            boxes_ = redo_stack_.back();
            redo_stack_.pop_back();
            selected_idx_ = -1;
        }
    }
    
    void set_active_class(int class_id) { active_class_ = class_id; }
    int active_class() const { return active_class_; }
    
    static ImU32 get_class_color(int class_id) {
        static const ImU32 colors[] = {
            IM_COL32(255, 50, 50, 255),     // 0: player (red)
            IM_COL32(50, 255, 50, 255),     // 1: bot (green)
            IM_COL32(50, 50, 255, 255),     // 2: weapon (blue)
            IM_COL32(255, 255, 50, 255),    // 3: outline (yellow)
            IM_COL32(128, 128, 128, 255),   // 4: dead_body (gray)
            IM_COL32(255, 128, 0, 255),     // 5: hideout_target_human (orange)
            IM_COL32(128, 0, 255, 255),     // 6: hideout_target_balls (purple)
            IM_COL32(255, 0, 255, 255),     // 7: head (magenta)
            IM_COL32(150, 150, 150, 255),   // 8: smoke (light gray)
            IM_COL32(255, 100, 0, 255),     // 9: fire (orange-red)
            IM_COL32(0, 255, 255, 255),     // 10: third_person (cyan)
        };
        return colors[class_id % 11];
    }
    
    static std::string get_class_name(int class_id) {
        static const char* names[] = {
            "player", "bot", "weapon", "outline", "dead_body",
            "hideout_target_human", "hideout_target_balls", "head",
            "smoke", "fire", "third_person"
        };
        return names[class_id % 11];
    }
    
private:
    std::vector<EditableBox> boxes_;
    int selected_idx_ = -1;
    EditorState state_ = EditorState::None;
    int active_class_ = 0;
    
    cv::Point2f create_start_;
    cv::Rect2f drag_start_rect_;
    ImVec2 drag_start_pos_;
    
    std::deque<std::vector<EditableBox>> undo_stack_;
    std::deque<std::vector<EditableBox>> redo_stack_;
    static constexpr size_t MAX_HISTORY = 50;
    
    void push_undo() {
        undo_stack_.push_back(boxes_);
        if (undo_stack_.size() > MAX_HISTORY) undo_stack_.pop_front();
        redo_stack_.clear();
    }
    
    bool is_resize_state() const {
        return state_ >= EditorState::Resizing_NW && state_ <= EditorState::Resizing_E;
    }
    
    int hit_test_handle(const ImVec2& mouse_pos, const cv::Rect2f& rect, 
                        const ImVec2& canvas_pos, float zoom) {
        float handle_size = 8.0f;
        
        // Calculate handle positions (corners and edges)
        float x1 = canvas_pos.x + rect.x * zoom;
        float y1 = canvas_pos.y + rect.y * zoom;
        float x2 = canvas_pos.x + (rect.x + rect.width) * zoom;
        float y2 = canvas_pos.y + (rect.y + rect.height) * zoom;
        float xc = (x1 + x2) / 2;
        float yc = (y1 + y2) / 2;
        
        // Check handles: 0=NW, 1=NE, 2=SW, 3=SE, 4=N, 5=S, 6=W, 7=E
        struct Handle { float x, y; };
        Handle handles[] = {
            {x1, y1}, {x2, y1}, {x1, y2}, {x2, y2},  // corners
            {xc, y1}, {xc, y2}, {x1, yc}, {x2, yc}   // edges
        };
        
        for (int i = 0; i < 8; i++) {
            if (std::abs(mouse_pos.x - handles[i].x) < handle_size &&
                std::abs(mouse_pos.y - handles[i].y) < handle_size) {
                return i;
            }
        }
        return -1;
    }
    
    void draw_resize_handles(ImDrawList* draw_list, const ImVec2& p1, const ImVec2& p2, float zoom) {
        float size = 6.0f;
        ImU32 color = IM_COL32(255, 255, 255, 255);
        
        // Corners
        draw_list->AddRectFilled(ImVec2(p1.x - size/2, p1.y - size/2), 
                                  ImVec2(p1.x + size/2, p1.y + size/2), color);
        draw_list->AddRectFilled(ImVec2(p2.x - size/2, p1.y - size/2), 
                                  ImVec2(p2.x + size/2, p1.y + size/2), color);
        draw_list->AddRectFilled(ImVec2(p1.x - size/2, p2.y - size/2), 
                                  ImVec2(p1.x + size/2, p2.y + size/2), color);
        draw_list->AddRectFilled(ImVec2(p2.x - size/2, p2.y - size/2), 
                                  ImVec2(p2.x + size/2, p2.y + size/2), color);
    }
    
    void handle_resize(const ImVec2& mouse_pos, float zoom, int img_width, int img_height) {
        if (selected_idx_ < 0) return;
        
        float dx = (mouse_pos.x - drag_start_pos_.x) / zoom;
        float dy = (mouse_pos.y - drag_start_pos_.y) / zoom;
        
        cv::Rect2f r = drag_start_rect_;
        
        // Apply resize based on handle
        switch (state_) {
            case EditorState::Resizing_NW:  // Top-left
                r.x += dx; r.y += dy;
                r.width -= dx; r.height -= dy;
                break;
            case EditorState::Resizing_NE:  // Top-right
                r.y += dy;
                r.width += dx; r.height -= dy;
                break;
            case EditorState::Resizing_SW:  // Bottom-left
                r.x += dx;
                r.width -= dx; r.height += dy;
                break;
            case EditorState::Resizing_SE:  // Bottom-right
                r.width += dx; r.height += dy;
                break;
            case EditorState::Resizing_N:   // Top
                r.y += dy; r.height -= dy;
                break;
            case EditorState::Resizing_S:   // Bottom
                r.height += dy;
                break;
            case EditorState::Resizing_W:   // Left
                r.x += dx; r.width -= dx;
                break;
            case EditorState::Resizing_E:   // Right
                r.width += dx;
                break;
            default:
                break;
        }
        
        // Clamp to image bounds
        r.x = std::max(0.0f, r.x);
        r.y = std::max(0.0f, r.y);
        r.width = std::min(r.width, img_width - r.x);
        r.height = std::min(r.height, img_height - r.y);
        
        // Ensure minimum size
        if (r.width > 5 && r.height > 5) {
            boxes_[selected_idx_].rect = r;
        }
    }
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/canvas/box_editor.h
git commit -m "feat(labeler): implement BoxEditor with full interaction support"
```

---

## Chunk 8: UIRenderer Integration and Keyboard Shortcuts

### Task 8.1: Implement UIRenderer with Full Layout

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/ui_renderer.h`

- [ ] **Step 1: Implement UIRenderer**

```cpp
#pragma once

#include "panels/capture_panel.h"
#include "panels/labels_panel.h"
#include "panels/settings_panel.h"
#include "panels/log_console.h"
#include "canvas/video_canvas.h"
#include "canvas/box_editor.h"

namespace sam3_labeler {

class App;

class UIRenderer {
public:
    UIRenderer(App& app) 
        : app_(app),
          capture_panel_(app),
          labels_panel_(app),
          settings_panel_(app),
          video_canvas_(app) {}
    
    void render() {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(window_width_, window_height_));
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Main", nullptr, 
                     ImGuiWindowFlags_NoTitleBar | 
                     ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();
        
        render_left_panel();
        ImGui::SameLine();
        render_main_area();
        
        ImGui::End();
    }
    
    // Panel state (accordion)
    bool capture_expanded_ = true;
    bool labels_expanded_ = false;
    bool settings_expanded_ = false;
    bool log_expanded_ = true;
    
    VideoCanvas& video_canvas() { return video_canvas_; }
    BoxEditor& box_editor() { return box_editor_; }
    LogConsole& log_console() { return log_console_; }
    
private:
    App& app_;
    
    CapturePanel capture_panel_;
    LabelsPanel labels_panel_;
    SettingsPanel settings_panel_;
    LogConsole log_console_;
    VideoCanvas video_canvas_;
    BoxEditor box_editor_;
    
    // Layout from UI_DESIGN_SPEC.md
    static constexpr int window_width_ = 1014;
    static constexpr int window_height_ = 730;
    static constexpr int LEFT_PANEL_WIDTH = 256;
    static constexpr int LOG_EXPANDED_HEIGHT = 192;
    static constexpr int LOG_COLLAPSED_HEIGHT = 40;
    static constexpr int TOOLBAR_HEIGHT = 48;
    
    void render_left_panel() {
        ImGui::BeginChild("LeftPanel", ImVec2(LEFT_PANEL_WIDTH, 0), true);
        
        // Header
        ImGui::Text("Editor Options");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Accordion sections
        render_accordion_section("Capture", capture_expanded_, [this]() {
            capture_panel_.render();
        });
        
        render_accordion_section("Labels", labels_expanded_, [this]() {
            labels_panel_.render();
        });
        
        render_accordion_section("Settings", settings_expanded_, [this]() {
            settings_panel_.render();
        });
        
        ImGui::EndChild();
    }
    
    template<typename Func>
    void render_accordion_section(const char* name, bool& expanded, Func render_content) {
        ImGui::PushID(name);
        
        // Header with expand indicator
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.153f, 0.153f, 0.165f, 1.0f));
        if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_AllowItemOverlap)) {
            expanded = true;
        } else {
            expanded = false;
        }
        ImGui::PopStyleColor();
        
        if (expanded) {
            ImGui::Indent(8);
            render_content();
            ImGui::Unindent(8);
        }
        
        ImGui::PopID();
    }
    
    void render_main_area() {
        float log_height = log_expanded_ ? LOG_EXPANDED_HEIGHT : LOG_COLLAPSED_HEIGHT;
        float main_height = window_height_ - log_height;
        
        // Toolbar + Canvas
        ImGui::BeginChild("MainArea", ImVec2(0, main_height), false);
        
        render_toolbar();
        
        // Canvas area
        ImGui::BeginChild("Canvas", ImVec2(0, 0), false);
        video_canvas_.render();
        ImGui::EndChild();
        
        ImGui::EndChild();
        
        // Log console
        log_console_.render(log_expanded_);
    }
    
    void render_toolbar() {
        ImGui::BeginChild("Toolbar", ImVec2(0, TOOLBAR_HEIGHT), true);
        
        // Left: Title
        ImGui::Text("Canvas");
        ImGui::SameLine();
        ImGui::TextDisabled("(1920 x 1080)");  // TODO: actual frame size
        
        // Right: Zoom controls
        float right_x = ImGui::GetWindowWidth() - 200;
        ImGui::SameLine(right_x);
        
        if (ImGui::Button("-##zoomout", ImVec2(32, 0))) {
            video_canvas_.zoom_out();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom Out");
        
        ImGui::SameLine();
        ImGui::Text("%d%%", static_cast<int>(video_canvas_.zoom() * 100));
        
        ImGui::SameLine();
        if (ImGui::Button("+##zoomin", ImVec2(32, 0))) {
            video_canvas_.zoom_in();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom In");
        
        ImGui::SameLine();
        if (ImGui::Button("Fit", ImVec2(32, 0))) {
            // Fit to canvas
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fit to Window");
        
        ImGui::EndChild();
    }
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/ui_renderer.h
git commit -m "feat(labeler): implement UIRenderer with full layout"
```

---

### Task 8.2: Add Classes.txt Loader

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/app/class_loader.h`

- [ ] **Step 1: Create class_loader.h**

```cpp
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

namespace sam3_labeler {

struct ClassInfo {
    int id;
    std::string name;
};

class ClassLoader {
public:
    bool load(const std::string& path) {
        classes_.clear();
        
        std::ifstream file(path);
        if (!file.is_open()) {
            last_error_ = "Failed to open classes file: " + path;
            return false;
        }
        
        std::string line;
        int id = 0;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                classes_.push_back({id++, line});
            }
        }
        
        return !classes_.empty();
    }
    
    const std::vector<ClassInfo>& classes() const { return classes_; }
    std::string class_name(int id) const {
        if (id >= 0 && id < classes_.size()) {
            return classes_[id].name;
        }
        return "unknown";
    }
    int class_count() const { return classes_.size(); }
    std::string last_error() const { return last_error_; }
    
    // Try to find classes.txt in common locations
    static std::string find_classes_file(const std::string& output_dir) {
        std::vector<std::string> candidates = {
            output_dir + "/labels/classes.txt",
            output_dir + "/classes.txt",
            "scripts/training/datasets/game/labels/classes.txt"
        };
        
        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
        return "";
    }
    
private:
    std::vector<ClassInfo> classes_;
    std::string last_error_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/app/class_loader.h
git commit -m "feat(labeler): add ClassLoader for classes.txt"
```

---

## Chunk 9: Final Integration and Testing

### Task 9.1: App Integration - Wire Everything Together

**Files:**
- Modify: `SAM3TensorRT/cpp/apps/sam3_labeler/app/app.cpp`

- [ ] **Step 1: Update App class to use UIRenderer**

Replace render_ui() with full integration:

```cpp
void App::render_ui() {
    // Delegate to UIRenderer
    ui_renderer_->render();
}
```

Add to init():

```cpp
bool App::init() {
    // ... existing init code ...
    
    // Load classes
    std::string classes_path = ClassLoader::find_classes_file(config_.settings.output_dir);
    if (!classes_path.empty()) {
        class_loader_.load(classes_path);
    }
    
    // Create UI renderer
    ui_renderer_ = std::make_unique<UIRenderer>(*this);
    
    // Set up log callback
    ui_renderer_->log_console().add_entry("INFO", "Application initialized");
    
    return true;
}
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/app/app.cpp
git commit -m "feat(labeler): integrate UIRenderer into App"
```

---

### Task 9.2: Implement Error Dialogs

**Files:**
- Create: `SAM3TensorRT/cpp/apps/sam3_labeler/ui/error_dialog.h`

- [ ] **Step 1: Create error_dialog.h**

```cpp
#pragma once

#include <imgui.h>
#include <string>
#include <functional>

namespace sam3_labeler {

class ErrorDialog {
public:
    void show(const std::string& title, const std::string& message) {
        show_ = true;
        title_ = title;
        message_ = message;
    }
    
    void render() {
        if (!show_) return;
        
        ImGui::OpenPopup(title_.c_str());
        
        if (ImGui::BeginPopupModal(title_.c_str(), &show_, 
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", message_.c_str());
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(80, 0))) {
                show_ = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }
    
private:
    bool show_ = false;
    std::string title_;
    std::string message_;
};

class ConfirmDialog {
public:
    using Callback = std::function<void(bool)>;
    
    void show(const std::string& title, const std::string& message, Callback callback) {
        show_ = true;
        title_ = title;
        message_ = message;
        callback_ = callback;
    }
    
    void render() {
        if (!show_) return;
        
        ImGui::OpenPopup(title_.c_str());
        
        if (ImGui::BeginPopupModal(title_.c_str(), &show_, 
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", message_.c_str());
            ImGui::Spacing();
            
            if (ImGui::Button("Yes", ImVec2(80, 0))) {
                if (callback_) callback_(true);
                show_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 0))) {
                if (callback_) callback_(false);
                show_ = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }
    
private:
    bool show_ = false;
    std::string title_;
    std::string message_;
    Callback callback_;
};

}  // namespace sam3_labeler
```

- [ ] **Step 2: Commit**

```bash
git add SAM3TensorRT/cpp/apps/sam3_labeler/ui/error_dialog.h
git commit -m "feat(labeler): add ErrorDialog and ConfirmDialog"
```

---

### Task 9.3: End-to-End Integration Test

- [ ] **Step 1: Build the complete application**

```bash
cmake --build build/sam3 --config Release --target sam3_labeler
```

Expected: Build succeeds with no errors

- [ ] **Step 2: Run the labeler**

```bash
.\build\sam3\Release\sam3_labeler.exe
```

Expected: Window opens with dark theme

- [ ] **Step 3: Test video loading**

- Use Capture panel to browse for an MP4 file
- Verify video loads and displays
- Test play/pause/step controls
- Test seek bar

Expected: Video plays correctly, controls work

- [ ] **Step 4: Test SAM3 inference**

- Load a video and pause on a frame
- Ensure prompts are configured in config.yaml
- Run inference (trigger from UI)
- Verify bounding boxes appear on frame

Expected: Boxes appear with correct class colors

- [ ] **Step 5: Test box editing**

- Click on a box to select it
- Drag to move the box
- Drag corners to resize
- Press Delete to remove a box
- Use Ctrl+Z to undo
- Click+drag on empty area to create new box

Expected: All editing operations work correctly

- [ ] **Step 6: Test YOLO export**

- Click Save button
- Choose output directory
- Verify image and label files created
- Check YOLO format is correct

Expected: Files created with correct format

- [ ] **Step 7: Commit if all tests pass**

```bash
git add -A
git commit -m "feat(labeler): complete integration - all tests passing"
```

---

### Task 9.4: Fix Any Remaining TODOs

- [ ] **Step 1: Replace TODO in MultiPromptRunner**

Replace `// TODO: Implement NMS if needed` with actual NMS or remove if not needed for this version.

- [ ] **Step 2: Replace TODO in YOLOExporter**

Replace `// TODO: get from config` with:
```cpp
std::string img_ext = config_.settings.image_format;
```

Replace `// TODO: Prompt user for overwrite confirmation` with:
```cpp
if (YOLOExporter::file_exists(img_path)) {
    // Use ConfirmDialog from error_dialog.h
    confirm_dialog_.show("Overwrite?", 
        "File already exists. Overwrite?", 
        [this, img_path, label_path, frame, boxes](bool confirmed) {
            if (confirmed) {
                do_export(img_path, label_path, frame, boxes);
            }
        });
}
```

- [ ] **Step 3: Replace TODO in UIRenderer toolbar**

Replace `// TODO: actual frame size` with:
```cpp
auto size = video_canvas_.frame_size();
if (size.width > 0) {
    ImGui::TextDisabled("(%d x %d)", size.width, size.height);
} else {
    ImGui::TextDisabled("(no video)");
}
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "fix(labeler): replace all TODO markers with implementations"
```

---

## Summary

This expanded plan covers the full implementation of the SAM3 Video Labeler:

1. **Chunk 1**: Project setup, CMake, App class, ImGui init, dark theme
2. **Chunk 2**: SAM3 integration headers
3. **Chunk 3**: YOLOExporter
4. **Chunk 4**: UI panel stubs
5. **Chunk 5**: VideoPlayer and VideoCanvas with OpenGL textures
6. **Chunk 6**: SAM3LabelerBackend with SAM3_PCS integration
7. **Chunk 7**: BoxEditor with full interaction (select, move, resize, create, undo/redo)
8. **Chunk 8**: UIRenderer integration, ClassLoader
9. **Chunk 9**: Final integration, error dialogs, end-to-end testing

Each task has exact file paths, complete code, and commit commands. Follow the tasks in order, committing after each completed step.

**Execution**: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan.