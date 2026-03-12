# ImGui Extensions Guide

## Recommended Extensions by Use Case

### Data Visualization
**ImPlot** - Essential for charts and graphs
```cpp
// Setup
ImPlot::CreateContext();
// ... application code ...
ImPlot::DestroyContext();

// Basic line plot
if (ImPlot::BeginPlot("My Plot")) {
    ImPlot::PlotLine("Line", x_data, y_data, count);
    ImPlot::EndPlot();
}
```

**Features:**
- Line, scatter, bar plots
- Heatmaps, histograms
- Real-time plotting (10k+ points)
- Multiple Y-axes
- Log scales

**Best For:** Real-time monitoring, data analysis, scientific visualization

---

### 3D Manipulation
**ImGuizmo** - Essential for 3D editors
```cpp
// Set viewport
ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

// Draw view cube (top-right corner)
ImGuizmo::ViewManipulate(viewMatrix, camDistance, 
    ImVec2(pos.x + size.x - 128, pos.y), ImVec2(128, 128), 0x10101010);

// Object manipulation
ImGuizmo::Manipulate(viewMatrix, projMatrix, 
    ImGuizmo::TRANSLATE, ImGuizmo::WORLD, objectMatrix);
```

**Features:**
- Translation, rotation, scale gizmos
- View manipulation cube
- World/local space modes
- Snapping support

**Best For:** Level editors, 3D modeling tools, game development

---

### File Operations
**ImGuiFileDialog** - Recommended for file dialogs
```cpp
// Open file dialog
ImGuiFileDialog::Instance()->OpenDialog(
    "ChooseFileDlgKey",
    "Choose File",
    ".cpp,.h,.hpp",
    ".");

// Display
if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
        std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
        // Use filePath
    }
    ImGuiFileDialog::Instance()->Close();
}
```

**Features:**
- Thumbnails
- Bookmarks
- File filtering
- Multiple selection

**Best For:** Content creation tools, editors

---

### User Notifications
**ImGuiNotify** - Toast notifications
```cpp
// Simple notification
ImGui::InsertNotification({
    ImGuiToastType_Success,
    3000,  // Duration
    "Operation completed!"
});

// With custom title
ImGuiToast toast(ImGuiToastType_Warning, 5000);
toast.set_title("Attention");
toast.set_content("This action cannot be undone.");
ImGui::InsertNotification(toast);

// Render at end of frame
ImGui::RenderNotifications();
```

**Features:**
- Success, warning, error, info types
- Icons (Font Awesome)
- Auto-dismiss
- Stacking

**Best For:** User feedback, status updates, alerts

---

### Text Editing
**TextEditor** (ImGuiColorTextEdit fork) - Code editing
```cpp
TextEditor editor;

// Set language
auto lang = TextEditor::LanguageDefinition::CPlusPlus();
editor.SetLanguageDefinition(lang);

// Set content
editor.SetText(sourceCode);

// Render
editor.Render("Editor", ImVec2(800, 600));

// Get edited text
std::string text = editor.GetText();
```

**Features:**
- Syntax highlighting
- Error markers
- Auto-indentation
- Multiple cursors

**Best For:** Shader editors, script editors, configuration files

---

### Node Graphs
**imnodes** - Node-based editors
```cpp
ImNodes::BeginNodeEditor();

// Begin node
ImNodes::BeginNode(1);
ImNodes::BeginNodeTitleBar();
ImGui::TextUnformatted("My Node");
ImNodes::EndNodeTitleBar();

// Attributes
ImNodes::BeginInputAttribute(1);
ImGui::Text("Input");
ImNodes::EndInputAttribute();

ImNodes::BeginOutputAttribute(2);
ImGui::Text("Output");
ImNodes::EndOutputAttribute();

ImNodes::EndNode();

// Links
ImNodes::Link(0, 2, 3);

ImNodes::EndNodeEditor();
```

**Features:**
- Draggable nodes
- Connection links
- Minimap
- Context menus

**Best For:** Shader graphs, visual scripting, material editors

---

### Loading Indicators
**imspinner** - Animated spinners
```cpp
// Include imspinner.h
#include "imspinner.h"

// Various spinner styles
ImSpinner::SpinnerRotatingSegments("Loading", 16, 2, ImColor(0, 200, 255));
ImSpinner::SpinnerAng("Loading", 16, 3, ImColor(255, 100, 200));
ImSpinner::SpinnerDots("Loading", 16, 3, ImColor(200, 200, 0));
```

**Features:**
- 30+ spinner styles
- Customizable colors
- Configurable size

**Best For:** Loading states, background tasks

---

## Extension Installation

### Header-Only Extensions
Most ImGui extensions are header-only:

```
project/
├── imgui/                    # Core ImGui
│   ├── imgui.cpp
│   ├── imgui.h
│   └── ...
├── implot/                   # Extension
│   ├── implot.cpp
│   ├── implot.h
│   └── implot_demo.cpp
└── main.cpp
```

### CMake Example
```cmake
# Add ImPlot
add_library(implot STATIC
    third_party/implot/implot.cpp
    third_party/implot/implot_items.cpp
)
target_include_directories(implot PUBLIC third_party/implot)
target_link_libraries(implot PUBLIC imgui)

# Your application
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE imgui implot)
```

### vcpkg
```bash
vcpkg install imgui implot imguizmo
```

---

## Extension Compatibility

### Version Matching
- Check extension's ImGui version compatibility
- Most extensions work with 1.89+
- Some may need updates for latest ImGui

### Context Management
```cpp
// Initialize in order
ImGui::CreateContext();
ImPlot::CreateContext();
// ... other contexts

// Cleanup in reverse order
ImPlot::DestroyContext();
ImGui::DestroyContext();
```

### Backend Compatibility
All standard backends support extensions:
- OpenGL 3.x
- DirectX 9/10/11/12
- Vulkan
- Metal

---

## Performance Considerations

### ImPlot Performance
```cpp
// Limit data points for real-time
if (data.size() > 10000) {
    data.erase(data.begin(), data.begin() + 1000);
}

// Use 32-bit indices
#define ImDrawIdx unsigned int

// Use flags for optimization
ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect
```

### Text Editor Performance
```cpp
// Only render visible lines
// Use for files < 100KB
// For larger files, implement pagination
```

---

## Integration Patterns

### Modular Extension Manager
```cpp
class ExtensionManager {
public:
    void Initialize() {
        ImPlot::CreateContext();
        ImNodes::CreateContext();
        // ... other extensions
    }
    
    void Shutdown() {
        ImNodes::DestroyContext();
        ImPlot::DestroyContext();
        // ... reverse order
    }
    
    void Render() {
        // Extensions render automatically
        // via ImGui:: calls
    }
};
```

### Lazy Loading
```cpp
class LazyFileDialog {
    std::unique_ptr<ImGuiFileDialog> dialog;
    
public:
    void Open() {
        if (!dialog) dialog = std::make_unique<ImGuiFileDialog>();
        dialog->OpenDialog(...);
    }
};
```
