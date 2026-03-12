# ImGui C++ Skill

## Overview

Dear ImGui is a bloat-free, immediate mode graphical user interface library for C++ with minimal dependencies. It outputs optimized vertex buffers that you can render anytime in your 3D-pipeline-enabled application.

**Core Philosophy:**
- Immediate Mode GUI (IMGUI) paradigm - UI is rebuilt every frame
- Minimize state synchronization between app and UI
- Code-driven, dynamic layouts
- Programmer-centric design for tooling and debug UIs

## When to Use ImGui

**Ideal for:**
- Debug and development tools
- Real-time data visualization
- Content creation tools
- Game engine editors
- Performance profilers
- Ad-hoc short-lived tools

**Not ideal for:**
- End-user applications requiring accessibility
- Applications needing right-to-left text support
- Complex text shaping requirements

## Latest Best Practices (v1.92+)

### 1. Version Selection

**Recommendation:** Use the `docking` branch for modern applications

```cpp
// Enable docking and multi-viewport features
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
```

**Docking Features:**
- Tab-based window management
- Split panes and docking zones
- Persistent layouts
- Multi-viewport support (windows outside main window)

### 2. Modern Window Setup

```cpp
// Standard initialization pattern
ImGui::NewFrame();

// Create fullscreen dockspace (optional but recommended)
ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), 
    ImGuiDockNodeFlags_PassthruCentralNode);

// Your UI code here
ImGui::Begin("My Window");
// ... widgets
ImGui::End();

// Multi-viewport support
ImGui::Render();
if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}
```

### 3. ID Stack System (Critical!)

**Most Common Mistake:** Using the same label for multiple widgets

```cpp
// WRONG - All buttons have same ID!
for (int i = 0; i < 3; i++) {
    if (ImGui::Button("Click")) { /* only first works */ }
}

// CORRECT - Using PushID/PopID
for (int i = 0; i < 3; i++) {
    ImGui::PushID(i);
    if (ImGui::Button("Click")) { /* works correctly */ }
    ImGui::PopID();
}

// CORRECT - Using ## suffix for unique IDs
ImGui::Button("Play##1");
ImGui::Button("Play##2");
ImGui::Button("Play##3");

// Hide label but keep ID: use ## at start
ImGui::Button("##InvisibleButton");
```

### 4. Modern Input Handling

```cpp
// Check if ImGui wants to capture input
ImGuiIO& io = ImGui::GetIO();

// Mouse
if (!io.WantCaptureMouse) {
    // Process mouse in your app
}

// Keyboard
if (!io.WantCaptureKeyboard) {
    // Process keyboard in your app
}

// Enable navigation
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
```

## Modern UI Design Patterns

### 1. Layout Patterns

```cpp
// Split layout using BeginChild
ImGui::Begin("Main Window");

// Left panel (20% width)
ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetWindowWidth() * 0.2f, 0), true);
// ... left content
ImGui::EndChild();

ImGui::SameLine();

// Right panel (remaining width)
ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
// ... right content
ImGui::EndChild();

ImGui::End();

// Table layout (modern replacement for Columns)
if (ImGui::BeginTable("MyTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Action");
    ImGui::TableHeadersRow();
    
    for (int row = 0; row < 10; row++) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Row %d", row);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Value %d", row * 10);
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("Edit")) { /* edit */ }
    }
    ImGui::EndTable();
}
```

### 2. Tree/Property Panel Pattern

```cpp
if (ImGui::Begin("Properties")) {
    if (ImGui::TreeNode("Transform")) {
        ImGui::DragFloat3("Position", &transform.pos.x);
        ImGui::DragFloat3("Rotation", &transform.rot.x);
        ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
        ImGui::TreePop();
    }
    
    if (ImGui::TreeNode("Material")) {
        ImGui::ColorEdit4("Color", &material.color.x);
        ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
        ImGui::TreePop();
    }
}
ImGui::End();
```

### 3. Toolbar Pattern

```cpp
void ShowToolbar() {
    ImGui::Begin("Toolbar", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar);
    
    if (ImGui::Button("New")) { /* new file */ }
    ImGui::SameLine();
    if (ImGui::Button("Open")) { /* open file */ }
    ImGui::SameLine();
    if (ImGui::Button("Save")) { /* save file */ }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0)); // Spacer
    ImGui::SameLine();
    if (ImGui::Button("Play")) { /* play */ }
    
    ImGui::End();
}
```

## Cyberpunk/Neon Theme System

### 1. Base Cyberpunk Theme

```cpp
void ApplyCyberpunkTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Core colors - Neon cyan/pink on dark
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.12f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.80f, 0.90f, 0.40f);
    
    // Headers - Cyan neon
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.60f, 0.80f, 0.40f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.70f, 0.90f, 0.60f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.80f, 1.00f, 0.80f);
    
    // Buttons - Neon pink
    colors[ImGuiCol_Button] = ImVec4(0.80f, 0.20f, 0.60f, 0.60f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.30f, 0.70f, 0.80f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.40f, 0.80f, 1.00f);
    
    // Frame backgrounds
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.20f, 0.50f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.30f, 0.60f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.40f, 0.70f);
    
    // Text
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
    
    // Highlights/accents - Neon cyan
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.90f, 1.00f, 1.00f);
    
    // Plot lines
    colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.80f, 0.20f, 0.60f, 0.80f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.30f, 0.70f, 1.00f);
    
    // Style adjustments
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    
    // Borders
    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
}
```

### 2. Neon Glow Effect (Shader-based)

```cpp
// Add glow to active elements by drawing twice with alpha
void NeonButton(const char* label, ImVec4 neonColor) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
        neonColor.x * 0.5f, 
        neonColor.y * 0.5f, 
        neonColor.z * 0.5f, 
        0.3f));
    
    ImGui::Button(label);
    
    if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
        // Draw glow effect
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        for (int i = 3; i > 0; i--) {
            float alpha = 0.3f / i;
            draw_list->AddRect(
                ImVec2(min.x - i*2, min.y - i*2),
                ImVec2(max.x + i*2, max.y + i*2),
                IM_COL32(
                    (int)(neonColor.x * 255),
                    (int)(neonColor.y * 255),
                    (int)(neonColor.z * 255),
                    (int)(alpha * 255)),
                4.0f, 0, 2.0f);
        }
    }
    
    ImGui::PopStyleColor();
}
```

### 3. Color Presets

```cpp
namespace CyberpunkColors {
    constexpr ImVec4 NeonCyan(0.0f, 0.8f, 1.0f, 1.0f);
    constexpr ImVec4 NeonPink(1.0f, 0.2f, 0.6f, 1.0f);
    constexpr ImVec4 NeonPurple(0.6f, 0.2f, 1.0f, 1.0f);
    constexpr ImVec4 NeonGreen(0.2f, 1.0f, 0.4f, 1.0f);
    constexpr ImVec4 NeonOrange(1.0f, 0.6f, 0.0f, 1.0f);
    constexpr ImVec4 NeonRed(1.0f, 0.1f, 0.2f, 1.0f);
    constexpr ImVec4 DarkBg(0.05f, 0.05f, 0.08f, 1.0f);
    constexpr ImVec4 PanelBg(0.08f, 0.08f, 0.12f, 1.0f);
}
```

## Essential Extensions

### 1. ImPlot (Data Visualization)

```cpp
// Setup
ImPlot::CreateContext();
// ...
ImPlot::DestroyContext();

// Usage
if (ImPlot::BeginPlot("My Plot", ImVec2(-1, 300))) {
    ImPlot::SetupAxes("Time", "Value");
    ImPlot::SetupAxisLimits(ImAxis_X1, 0, 10);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);
    
    ImPlot::PlotLine("Data", xs, ys, 100);
    ImPlot::PlotScatter("Points", px, py, 20);
    
    ImPlot::EndPlot();
}
```

### 2. ImGuizmo (3D Manipulation)

```cpp
// In your 3D view rendering
ImGuizmo::SetOrthographic(false);
ImGuizmo::SetDrawlist();
ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

// View manipulation cube
ImGuizmo::ViewManipulate(viewMatrix, projMatrix, 
    ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, 
    objectMatrix, NULL, 128.0f, ImVec2(10, 10));

// Transform manipulation
static ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
if (ImGui::RadioButton("Translate", op == ImGuizmo::TRANSLATE)) op = ImGuizmo::TRANSLATE;
ImGui::SameLine();
if (ImGui::RadioButton("Rotate", op == ImGuizmo::ROTATE)) op = ImGuizmo::ROTATE;
ImGui::SameLine();
if (ImGui::RadioButton("Scale", op == ImGuizmo::SCALE)) op = ImGuizmo::SCALE;

ImGuizmo::Manipulate(viewMatrix, projMatrix, op, ImGuizmo::MODE::LOCAL, objectMatrix);
```

### 3. Notifications

```cpp
// Include imgui-notify or similar
ImGui::InsertNotification({
    ImGuiToastType_Success, 
    3000, // duration ms
    "Operation completed successfully!"
});

// At end of frame
ImGui::RenderNotifications();
```

## Performance Best Practices

### 1. Minimize Draw Calls

```cpp
// Batch multiple items
ImDrawList* draw_list = ImGui::GetWindowDrawList();
draw_list->PushClipRect(min, max, true);

// Draw many items at once
for (const auto& item : items) {
    draw_list->AddRectFilled(item.min, item.max, item.color);
}

draw_list->PopClipRect();
```

### 2. Use 32-bit Indices for Large Data

```cpp
// In imconfig.h before including imgui.h
#define ImDrawIdx unsigned int
```

### 3. Avoid Rebuilding Unchanged UI

```cpp
// Cache expensive operations
static bool initialized = false;
static std::vector<SomeData> cachedData;

if (!initialized) {
    cachedData = ComputeExpensiveData();
    initialized = true;
}

// Use cachedData for rendering
```

## Common Widget Patterns

### Custom Combo with Icons

```cpp
void IconCombo(const char* label, int* current_item, 
               const char* const items[], int items_count) {
    if (ImGui::BeginCombo(label, items[*current_item])) {
        for (int i = 0; i < items_count; i++) {
            const bool is_selected = (*current_item == i);
            
            // Add icon based on type
            if (i == 0) ImGui::Text(ICON_FA_FILE); 
            else if (i == 1) ImGui::Text(ICON_FA_FOLDER);
            ImGui::SameLine();
            
            if (ImGui::Selectable(items[i], is_selected)) {
                *current_item = i;
            }
            
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}
```

### Progress Bar with Text

```cpp
void ProgressBarWithText(float fraction, const char* text) {
    ImVec2 barSize = ImVec2(ImGui::GetContentRegionAvail().x, 25.0f);
    ImGui::ProgressBar(fraction, barSize, "");
    
    // Overlay text
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 size = ImGui::GetItemRectSize();
    ImVec2 textSize = ImGui::CalcTextSize(text);
    
    ImGui::SetCursorScreenPos(ImVec2(
        pos.x + (size.x - textSize.x) * 0.5f,
        pos.y + (size.y - textSize.y) * 0.5f));
    ImGui::Text("%s", text);
}
```

## Anti-Patterns to Avoid

❌ **Storing widget state between frames**
```cpp
// WRONG
static bool isOpen = ImGui::Button("Open"); // This makes no sense
```

❌ **Calling ImGui functions outside Begin/End**
```cpp
// WRONG
ImGui::Button("Outside"); // No window context!
ImGui::Begin("Window");
ImGui::End();
```

❌ **Ignoring return values**
```cpp
// WRONG - misses the button click
ImGui::Button("Click"); 

// CORRECT
if (ImGui::Button("Click")) {
    DoSomething();
}
```

❌ **Using same ID for different widgets**
```cpp
// WRONG
ImGui::Button("Action");
ImGui::Button("Action"); // Same ID!

// CORRECT
ImGui::Button("Action##1");
ImGui::Button("Action##2");
```

## See Also

- `references/examples.md` - Complete code examples
- `references/themes.md` - Theme definitions and color schemes
- `references/extensions.md` - Recommended extensions guide
- Official: https://github.com/ocornut/imgui
- ImPlot: https://github.com/epezent/implot
- ImGuizmo: https://github.com/CedricGuillemet/ImGuizmo
