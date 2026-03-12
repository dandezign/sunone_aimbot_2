# ImGui Code Examples

## Complete Application Template

```cpp
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <cstdio>

// Forward declarations
static void glfw_error_callback(int error, const char* description);
void ApplyCyberpunkTheme();
void ShowMainDockspace();
void ShowToolbar();
void ShowPropertiesPanel();
void ShowViewport();

int main() {
    // Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui Application", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext(); // If using ImPlot
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Apply theme
    ApplyCyberpunkTheme();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Load fonts
    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 16.0f);
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Show main dockspace
        ShowMainDockspace();
        
        // Show application UI
        ShowToolbar();
        ShowPropertiesPanel();
        ShowViewport();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Multi-viewport support
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
```

## Advanced Dockspace Setup

```cpp
void ShowMainDockspace() {
    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    
    if (opt_fullscreen) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    
    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    
    if (!opt_padding)
        ImGui::PopStyleVar();
    
    if (opt_fullscreen)
        ImGui::PopStyleVar(2);
    
    // Submit the DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {}
            if (ImGui::MenuItem("Open", "Ctrl+O")) {}
            if (ImGui::MenuItem("Save", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            // View options
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    ImGui::End();
}
```

## Programmatic Docking Layout

```cpp
#include "imgui_internal.h"

void SetupDockingLayout() {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Create the dockspace node if it doesn't exist
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderAddNode(dockspace_id, 
            ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_CentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
        
        // Split into left and right
        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_id_left, dock_id_right;
        ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, 
            &dock_id_left, &dock_id_right);
        
        // Split right side into top and bottom
        ImGuiID dock_id_right_top, dock_id_right_bottom;
        ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Up, 0.7f, 
            &dock_id_right_top, &dock_id_right_bottom);
        
        // Split left side into top and bottom
        ImGuiID dock_id_left_top, dock_id_left_bottom;
        ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Up, 0.5f, 
            &dock_id_left_top, &dock_id_left_bottom);
        
        // Dock windows
        ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left_top);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_left_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dock_id_right_top);
        ImGui::DockBuilderDockWindow("Console", dock_id_right_bottom);
        
        ImGui::DockBuilderFinish(dockspace_id);
    }
}
```

## Custom Widget: Circular Progress

```cpp
void CircularProgress(const char* label, float fraction, float radius, 
                      ImVec4 color, const char* overlay_text = nullptr) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = window->DC.CursorPos;
    float diameter = radius * 2.0f;
    ImRect bb(pos, ImVec2(pos.x + diameter, pos.y + diameter));
    
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0)) return;
    
    // Background circle
    draw_list->AddCircle(
        ImVec2(pos.x + radius, pos.y + radius),
        radius - 2.0f,
        IM_COL32(50, 50, 50, 255),
        32,
        4.0f
    );
    
    // Progress arc
    float start_angle = -IM_PI / 2.0f;
    float end_angle = start_angle + (IM_PI * 2.0f * fraction);
    
    draw_list->PathArcTo(
        ImVec2(pos.x + radius, pos.y + radius),
        radius - 2.0f,
        start_angle,
        end_angle,
        32
    );
    
    draw_list->PathStroke(
        IM_COL32(
            (int)(color.x * 255),
            (int)(color.y * 255),
            (int)(color.z * 255),
            255
        ),
        false,
        4.0f
    );
    
    // Center text
    if (overlay_text) {
        ImVec2 text_size = ImGui::CalcTextSize(overlay_text);
        draw_list->AddText(
            ImVec2(
                pos.x + radius - text_size.x * 0.5f,
                pos.y + radius - text_size.y * 0.5f
            ),
            IM_COL32(255, 255, 255, 255),
            overlay_text
        );
    }
}

// Usage:
// CircularProgress("##progress", 0.75f, 50.0f, 
//     ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "75%");
```

## Real-time Data Visualization

```cpp
class RealTimePlot {
    static constexpr int MaxDataPoints = 500;
    std::deque<float> data;
    std::string name;
    float min_val, max_val;
    
public:
    RealTimePlot(const char* n, float min_v, float max_v) 
        : name(n), min_val(min_v), max_val(max_v) {}
    
    void AddPoint(float value) {
        data.push_back(value);
        if (data.size() > MaxDataPoints)
            data.pop_front();
    }
    
    void Draw() {
        if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, 150))) {
            ImPlot::SetupAxes(nullptr, nullptr, 
                ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_val, max_val, 
                ImGuiCond_Always);
            
            if (!data.empty()) {
                // Convert deque to array for plotting
                std::vector<float> x(data.size());
                std::iota(x.begin(), x.end(), 0);
                ImPlot::PlotLine(name.c_str(), x.data(), 
                    std::vector<float>(data.begin(), data.end()).data(), 
                    data.size());
            }
            
            ImPlot::EndPlot();
        }
    }
};

// Usage:
// RealTimePlot fps_plot("FPS", 0, 120);
// fps_plot.AddPoint(current_fps);
// fps_plot.Draw();
```

## Drag and Drop

```cpp
void ShowDragDropExample() {
    // Source
    ImGui::Button("Drag Me", ImVec2(100, 50));
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        int payload = 42;
        ImGui::SetDragDropPayload("DEMO_PAYLOAD", &payload, sizeof(int));
        ImGui::Text("Dragging %d", payload);
        ImGui::EndDragDropSource();
    }
    
    // Target
    ImGui::Button("Drop Here", ImVec2(100, 50));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DEMO_PAYLOAD")) {
            int data = *(const int*)payload->Data;
            printf("Received: %d\n", data);
        }
        ImGui::EndDragDropTarget();
    }
}

// Tree node drag and drop (reordering)
void ShowTreeDragDrop() {
    static std::vector<std::string> items = {"Item 1", "Item 2", "Item 3"};
    
    for (int i = 0; i < items.size(); i++) {
        ImGui::PushID(i);
        
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | 
                                   ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(items[i].c_str(), flags);
        
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("TREE_NODE", &i, sizeof(int));
            ImGui::Text("Moving %s", items[i].c_str());
            ImGui::EndDragDropSource();
        }
        
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TREE_NODE")) {
                int src_idx = *(const int*)payload->Data;
                std::swap(items[src_idx], items[i]);
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::PopID();
    }
}
```

## Multi-select with Checkboxes

```cpp
template<typename T>
struct SelectableList {
    struct Item {
        T data;
        bool selected;
    };
    
    std::vector<Item> items;
    int last_selected_index = -1;
    
    void Draw(const char* label, std::function<void(T&)> on_double_click = nullptr) {
        if (ImGui::BeginListBox(label, ImVec2(-FLT_MIN, 200))) {
            for (int i = 0; i < items.size(); i++) {
                auto& item = items[i];
                
                ImGui::PushID(i);
                
                bool is_selected = item.selected;
                if (ImGui::Selectable("##selectable", is_selected, 
                    ImGuiSelectableFlags_SpanAllColumns)) {
                    
                    if (ImGui::GetIO().KeyCtrl) {
                        // Ctrl+Click: toggle
                        item.selected = !item.selected;
                    } else if (ImGui::GetIO().KeyShift && last_selected_index != -1) {
                        // Shift+Click: range select
                        int start = std::min(last_selected_index, i);
                        int end = std::max(last_selected_index, i);
                        for (int j = start; j <= end; j++)
                            items[j].selected = true;
                    } else {
                        // Click: single select
                        for (auto& it : items) it.selected = false;
                        item.selected = true;
                    }
                    last_selected_index = i;
                }
                
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && on_double_click) {
                    on_double_click(item.data);
                }
                
                ImGui::SameLine();
                ImGui::Checkbox("##check", &item.selected);
                ImGui::SameLine();
                ImGui::Text("Item %d", i);
                
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
    }
    
    std::vector<T> GetSelected() const {
        std::vector<T> selected;
        for (const auto& item : items) {
            if (item.selected) selected.push_back(item.data);
        }
        return selected;
    }
};
```

## Modal Dialogs

```cpp
void ShowSaveConfirmationDialog(bool* p_open) {
    if (!*p_open) return;
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Save Changes?", p_open, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("You have unsaved changes.\n\nSave before closing?");
        ImGui::Separator();
        
        float button_width = 120.0f;
        ImGui::SetItemDefaultFocus();
        
        if (ImGui::Button("Save", ImVec2(button_width, 0))) {
            // Save logic
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Don't Save", ImVec2(button_width, 0))) {
            // Discard logic
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

// Usage:
// static bool show_dialog = true;
// if (show_dialog) ImGui::OpenPopup("Save Changes?");
// ShowSaveConfirmationDialog(&show_dialog);
```

## Context Menus

```cpp
void ShowContextMenuExample() {
    ImGui::Text("Right-click for context menu");
    
    if (ImGui::BeginPopupContextItem("item_context")) {
        if (ImGui::MenuItem("Copy")) {}
        if (ImGui::MenuItem("Paste")) {}
        ImGui::Separator();
        if (ImGui::BeginMenu("More")) {
            if (ImGui::MenuItem("Option 1")) {}
            if (ImGui::MenuItem("Option 2")) {}
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    
    // Window context menu
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Close")) {}
        ImGui::EndPopup();
    }
}
```
