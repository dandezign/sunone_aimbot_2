# ImGui Theme Reference

## Complete Theme Collection

### 1. Cyberpunk Neon

```cpp
void ThemeCyberpunk() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Core palette
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.80f, 0.90f, 0.40f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.80f, 0.90f, 0.10f);
    
    // Frames
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.20f, 0.50f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.30f, 0.60f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.40f, 0.70f);
    
    // Titles
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.10f, 0.75f);
    
    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 0.60f, 0.80f, 0.60f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.70f, 0.90f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.80f, 1.00f, 0.80f);
    
    // Check mark
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    
    // Sliders
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.90f, 1.00f, 1.00f);
    
    // Buttons - Neon pink
    colors[ImGuiCol_Button] = ImVec4(0.80f, 0.20f, 0.60f, 0.60f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.30f, 0.70f, 0.80f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.40f, 0.80f, 1.00f);
    
    // Headers - Neon cyan
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.60f, 0.80f, 0.40f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.70f, 0.90f, 0.60f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.80f, 1.00f, 0.80f);
    
    // Separators
    colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.80f, 0.90f, 0.40f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.00f, 0.90f, 1.00f, 0.60f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.80f);
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.80f, 0.90f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 0.90f, 1.00f, 0.60f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.80f);
    
    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.70f, 0.90f, 0.60f);
    colors[ImGuiCol_TabActive] = ImVec4(0.00f, 0.60f, 0.80f, 0.80f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.00f, 0.50f, 0.70f, 0.60f);
    
    // Plots
    colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.80f, 0.20f, 0.60f, 0.80f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.30f, 0.70f, 1.00f);
    
    // Tables
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.50f, 0.70f, 0.60f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.80f, 0.90f, 0.50f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.60f, 0.80f, 0.30f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.08f, 0.08f, 0.15f, 0.50f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.10f, 0.20f, 0.50f);
    
    // Text selection
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.60f, 0.80f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.40f, 0.80f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.00f, 0.90f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.05f, 0.10f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.05f, 0.10f, 0.35f);
    
    // Style adjustments
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
}
```

### 2. Dracula Dark

```cpp
void ThemeDracula() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Base colors from Dracula palette
    const ImVec4 bg = ImVec4(0.18f, 0.20f, 0.27f, 1.00f);
    const ImVec4 bg_light = ImVec4(0.22f, 0.25f, 0.33f, 1.00f);
    const ImVec4 bg_lighter = ImVec4(0.30f, 0.33f, 0.42f, 1.00f);
    const ImVec4 purple = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);
    const ImVec4 pink = ImVec4(1.00f, 0.47f, 0.78f, 1.00f);
    const ImVec4 cyan = ImVec4(0.56f, 0.89f, 0.93f, 1.00f);
    const ImVec4 green = ImVec4(0.31f, 0.94f, 0.53f, 1.00f);
    const ImVec4 orange = ImVec4(1.00f, 0.55f, 0.35f, 1.00f);
    const ImVec4 yellow = ImVec4(0.95f, 0.96f, 0.54f, 1.00f);
    
    colors[ImGuiCol_Text] = ImVec4(0.96f, 0.97f, 0.99f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.59f, 1.00f);
    colors[ImGuiCol_WindowBg] = bg;
    colors[ImGuiCol_ChildBg] = bg;
    colors[ImGuiCol_PopupBg] = bg_light;
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.28f, 0.37f, 1.00f);
    colors[ImGuiCol_FrameBg] = bg_lighter;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.35f, 0.38f, 0.47f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.43f, 0.52f, 1.00f);
    colors[ImGuiCol_TitleBg] = bg;
    colors[ImGuiCol_TitleBgActive] = purple;
    colors[ImGuiCol_TitleBgCollapsed] = bg;
    colors[ImGuiCol_MenuBarBg] = bg_light;
    colors[ImGuiCol_ScrollbarBg] = bg;
    colors[ImGuiCol_ScrollbarGrab] = bg_lighter;
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.43f, 0.52f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = purple;
    colors[ImGuiCol_CheckMark] = green;
    colors[ImGuiCol_SliderGrab] = purple;
    colors[ImGuiCol_SliderGrabActive] = pink;
    colors[ImGuiCol_Button] = bg_lighter;
    colors[ImGuiCol_ButtonHovered] = purple;
    colors[ImGuiCol_ButtonActive] = pink;
    colors[ImGuiCol_Header] = bg_lighter;
    colors[ImGuiCol_HeaderHovered] = purple;
    colors[ImGuiCol_HeaderActive] = pink;
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.28f, 0.37f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = cyan;
    colors[ImGuiCol_SeparatorActive] = cyan;
    colors[ImGuiCol_ResizeGrip] = bg_lighter;
    colors[ImGuiCol_ResizeGripHovered] = purple;
    colors[ImGuiCol_ResizeGripActive] = pink;
    colors[ImGuiCol_Tab] = bg_light;
    colors[ImGuiCol_TabHovered] = purple;
    colors[ImGuiCol_TabActive] = ImVec4(0.64f, 0.48f, 0.88f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = bg;
    colors[ImGuiCol_TabUnfocusedActive] = bg_lighter;
    colors[ImGuiCol_PlotLines] = cyan;
    colors[ImGuiCol_PlotLinesHovered] = pink;
    colors[ImGuiCol_PlotHistogram] = purple;
    colors[ImGuiCol_PlotHistogramHovered] = pink;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.74f, 0.58f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = pink;
    colors[ImGuiCol_NavHighlight] = purple;
    
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 6.0f;
}
```

### 3. Monokai Pro

```cpp
void ThemeMonokai() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Monokai Pro palette
    const ImVec4 bg = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    const ImVec4 bg_light = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
    const ImVec4 bg_lighter = ImVec4(0.27f, 0.27f, 0.31f, 1.00f);
    const ImVec4 red = ImVec4(1.00f, 0.29f, 0.36f, 1.00f);
    const ImVec4 orange = ImVec4(1.00f, 0.59f, 0.33f, 1.00f);
    const ImVec4 yellow = ImVec4(1.00f, 0.85f, 0.40f, 1.00f);
    const ImVec4 green = ImVec4(0.66f, 0.85f, 0.35f, 1.00f);
    const ImVec4 cyan = ImVec4(0.40f, 0.85f, 0.88f, 1.00f);
    const ImVec4 blue = ImVec4(0.33f, 0.63f, 1.00f, 1.00f);
    const ImVec4 purple = ImVec4(0.70f, 0.48f, 0.93f, 1.00f);
    
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.91f, 0.88f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.47f, 0.48f, 1.00f);
    colors[ImGuiCol_WindowBg] = bg;
    colors[ImGuiCol_ChildBg] = bg;
    colors[ImGuiCol_PopupBg] = bg_light;
    colors[ImGuiCol_Border] = bg_lighter;
    colors[ImGuiCol_FrameBg] = bg_light;
    colors[ImGuiCol_FrameBgHovered] = bg_lighter;
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.37f, 0.37f, 0.41f, 1.00f);
    colors[ImGuiCol_TitleBg] = bg;
    colors[ImGuiCol_TitleBgActive] = orange;
    colors[ImGuiCol_TitleBgCollapsed] = bg;
    colors[ImGuiCol_MenuBarBg] = bg_light;
    colors[ImGuiCol_ScrollbarBg] = bg;
    colors[ImGuiCol_ScrollbarGrab] = bg_lighter;
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.47f, 0.47f, 0.51f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = orange;
    colors[ImGuiCol_CheckMark] = green;
    colors[ImGuiCol_SliderGrab] = yellow;
    colors[ImGuiCol_SliderGrabActive] = orange;
    colors[ImGuiCol_Button] = bg_lighter;
    colors[ImGuiCol_ButtonHovered] = orange;
    colors[ImGuiCol_ButtonActive] = yellow;
    colors[ImGuiCol_Header] = bg_lighter;
    colors[ImGuiCol_HeaderHovered] = orange;
    colors[ImGuiCol_HeaderActive] = yellow;
    colors[ImGuiCol_Separator] = bg_lighter;
    colors[ImGuiCol_SeparatorHovered] = cyan;
    colors[ImGuiCol_SeparatorActive] = cyan;
    colors[ImGuiCol_ResizeGrip] = bg_lighter;
    colors[ImGuiCol_ResizeGripHovered] = orange;
    colors[ImGuiCol_ResizeGripActive] = yellow;
    colors[ImGuiCol_Tab] = bg_light;
    colors[ImGuiCol_TabHovered] = orange;
    colors[ImGuiCol_TabActive] = ImVec4(0.90f, 0.49f, 0.23f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = bg;
    colors[ImGuiCol_TabUnfocusedActive] = bg_lighter;
    colors[ImGuiCol_PlotLines] = cyan;
    colors[ImGuiCol_PlotLinesHovered] = yellow;
    colors[ImGuiCol_PlotHistogram] = orange;
    colors[ImGuiCol_PlotHistogramHovered] = yellow;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.59f, 0.33f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = yellow;
    colors[ImGuiCol_NavHighlight] = orange;
    
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
}
```

### 4. Material Ocean

```cpp
void ThemeMaterialOcean() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Material Ocean palette
    const ImVec4 bg = ImVec4(0.09f, 0.12f, 0.16f, 1.00f);
    const ImVec4 bg_light = ImVec4(0.13f, 0.17f, 0.22f, 1.00f);
    const ImVec4 bg_lighter = ImVec4(0.18f, 0.23f, 0.30f, 1.00f);
    const ImVec4 blue = ImVec4(0.25f, 0.55f, 0.95f, 1.00f);
    const ImVec4 cyan = ImVec4(0.00f, 0.75f, 0.85f, 1.00f);
    const ImVec4 teal = ImVec4(0.00f, 0.55f, 0.65f, 1.00f);
    const ImVec4 green = ImVec4(0.30f, 0.80f, 0.60f, 1.00f);
    
    colors[ImGuiCol_Text] = ImVec4(0.87f, 0.88f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.52f, 1.00f);
    colors[ImGuiCol_WindowBg] = bg;
    colors[ImGuiCol_ChildBg] = bg;
    colors[ImGuiCol_PopupBg] = bg_light;
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBg] = bg_light;
    colors[ImGuiCol_FrameBgHovered] = bg_lighter;
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.33f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg] = bg;
    colors[ImGuiCol_TitleBgActive] = blue;
    colors[ImGuiCol_TitleBgCollapsed] = bg;
    colors[ImGuiCol_MenuBarBg] = bg_light;
    colors[ImGuiCol_ScrollbarBg] = bg;
    colors[ImGuiCol_ScrollbarGrab] = bg_lighter;
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.43f, 0.50f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = blue;
    colors[ImGuiCol_CheckMark] = cyan;
    colors[ImGuiCol_SliderGrab] = teal;
    colors[ImGuiCol_SliderGrabActive] = cyan;
    colors[ImGuiCol_Button] = bg_lighter;
    colors[ImGuiCol_ButtonHovered] = blue;
    colors[ImGuiCol_ButtonActive] = cyan;
    colors[ImGuiCol_Header] = bg_lighter;
    colors[ImGuiCol_HeaderHovered] = blue;
    colors[ImGuiCol_HeaderActive] = cyan;
    colors[ImGuiCol_Separator] = ImVec4(0.13f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = teal;
    colors[ImGuiCol_SeparatorActive] = cyan;
    colors[ImGuiCol_ResizeGrip] = bg_lighter;
    colors[ImGuiCol_ResizeGripHovered] = blue;
    colors[ImGuiCol_ResizeGripActive] = cyan;
    colors[ImGuiCol_Tab] = bg_light;
    colors[ImGuiCol_TabHovered] = blue;
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.50f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotLines] = cyan;
    colors[ImGuiCol_PlotLinesHovered] = blue;
    colors[ImGuiCol_PlotHistogram] = teal;
    colors[ImGuiCol_PlotHistogramHovered] = cyan;
    
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowRounding = 4.0f;
}
```

### 5. Synthwave/Retrowave

```cpp
void ThemeSynthwave() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Synthwave palette
    const ImVec4 bg = ImVec4(0.06f, 0.02f, 0.12f, 1.00f);
    const ImVec4 bg_light = ImVec4(0.12f, 0.04f, 0.20f, 1.00f);
    const ImVec4 bg_lighter = ImVec4(0.20f, 0.08f, 0.30f, 1.00f);
    const ImVec4 pink = ImVec4(1.00f, 0.20f, 0.60f, 1.00f);
    const ImVec4 purple = ImVec4(0.60f, 0.20f, 0.90f, 1.00f);
    const ImVec4 cyan = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    const ImVec4 yellow = ImVec4(1.00f, 0.90f, 0.00f, 1.00f);
    const ImVec4 orange = ImVec4(1.00f, 0.40f, 0.20f, 1.00f);
    
    colors[ImGuiCol_Text] = ImVec4(1.00f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.40f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg] = bg;
    colors[ImGuiCol_ChildBg] = bg;
    colors[ImGuiCol_PopupBg] = bg_light;
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.10f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.60f, 0.20f, 0.90f, 0.15f);
    colors[ImGuiCol_FrameBg] = bg_light;
    colors[ImGuiCol_FrameBgHovered] = bg_lighter;
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.12f, 0.45f, 1.00f);
    colors[ImGuiCol_TitleBg] = bg;
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.40f, 0.10f, 0.70f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = bg;
    colors[ImGuiCol_MenuBarBg] = bg_light;
    colors[ImGuiCol_ScrollbarBg] = bg;
    colors[ImGuiCol_ScrollbarGrab] = bg_lighter;
    colors[ImGuiCol_ScrollbarGrabHovered] = purple;
    colors[ImGuiCol_ScrollbarGrabActive] = pink;
    colors[ImGuiCol_CheckMark] = cyan;
    colors[ImGuiCol_SliderGrab] = pink;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.40f, 0.80f, 1.00f);
    colors[ImGuiCol_Button] = bg_lighter;
    colors[ImGuiCol_ButtonHovered] = purple;
    colors[ImGuiCol_ButtonActive] = pink;
    colors[ImGuiCol_Header] = bg_lighter;
    colors[ImGuiCol_HeaderHovered] = purple;
    colors[ImGuiCol_HeaderActive] = pink;
    colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.15f, 0.60f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = cyan;
    colors[ImGuiCol_SeparatorActive] = cyan;
    colors[ImGuiCol_ResizeGrip] = purple;
    colors[ImGuiCol_ResizeGripHovered] = pink;
    colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.30f, 0.70f, 1.00f);
    colors[ImGuiCol_Tab] = bg_light;
    colors[ImGuiCol_TabHovered] = purple;
    colors[ImGuiCol_TabActive] = ImVec4(0.50f, 0.15f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotLines] = cyan;
    colors[ImGuiCol_PlotLinesHovered] = pink;
    colors[ImGuiCol_PlotHistogram] = purple;
    colors[ImGuiCol_PlotHistogramHovered] = pink;
    
    style.FrameRounding = 0.0f; // Sharp edges for retro feel
    style.GrabRounding = 0.0f;
    style.WindowRounding = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 2.0f;
}
```

### 6. High Contrast Light

```cpp
void ThemeHighContrastLight() {
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // High contrast palette
    colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    
    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
}
```

## Style Variables Quick Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `Alpha` | 1.0 | Global transparency |
| `WindowPadding` | (8,8) | Padding inside windows |
| `WindowRounding` | 0.0 | Window corner rounding |
| `WindowBorderSize` | 1.0 | Window border thickness |
| `WindowMinSize` | (32,32) | Minimum window size |
| `WindowTitleAlign` | (0,0.5) | Title bar alignment |
| `ChildRounding` | 0.0 | Child window rounding |
| `ChildBorderSize` | 1.0 | Child window border |
| `PopupRounding` | 0.0 | Popup rounding |
| `PopupBorderSize` | 1.0 | Popup border |
| `FramePadding` | (4,3) | Frame internal padding |
| `FrameRounding` | 0.0 | Frame rounding |
| `FrameBorderSize` | 0.0 | Frame border |
| `ItemSpacing` | (8,4) | Item gap |
| `ItemInnerSpacing` | (4,4) | Inner item gap |
| `CellPadding` | (4,2) | Table cell padding |
| `TouchExtraPadding` | (0,0) | Touch hit area |
| `IndentSpacing` | 21.0 | Tree indent |
| `ColumnsMinSpacing` | 6.0 | Column min gap |
| `ScrollbarSize` | 14.0 | Scrollbar width |
| `ScrollbarRounding` | 9.0 | Scrollbar rounding |
| `GrabMinSize` | 10.0 | Min grab size |
| `GrabRounding` | 0.0 | Grab rounding |
| `TabRounding` | 4.0 | Tab rounding |
| `TabBorderSize` | 0.0 | Tab border |
| `TabMinWidthForCloseButton` | 0.0 | Tab close button |
| `ColorButtonPosition` | Right | Color button side |
| `ButtonTextAlign` | (0.5,0.5) | Button text align |
| `SelectableTextAlign` | (0,0) | Selectable text |
| `DisplayWindowPadding` | (19,19) | Display padding |
| `DisplaySafeAreaPadding` | (3,3) | Safe area |
| `MouseCursorScale` | 1.0 | Cursor scale |
| `AntiAliasedLines` | true | Line AA |
| `AntiAliasedLinesUseTex` | true | Line AA texture |
| `AntiAliasedFill` | true | Fill AA |
| `CurveTessellationTol` | 1.25 | Curve quality |
| `CircleTessellationMaxError` | 0.30 | Circle quality |

## Color Index Reference

| Index | Name | Usage |
|-------|------|-------|
| 0 | Text | All text |
| 1 | TextDisabled | Disabled text |
| 2 | WindowBg | Window background |
| 3 | ChildBg | Child window bg |
| 4 | PopupBg | Popup/menu bg |
| 5 | Border | Border color |
| 6 | BorderShadow | Border shadow |
| 7 | FrameBg | Frame bg (input, button) |
| 8 | FrameBgHovered | Frame hover |
| 9 | FrameBgActive | Frame active |
| 10 | TitleBg | Title bar |
| 11 | TitleBgActive | Active title |
| 12 | TitleBgCollapsed | Collapsed title |
| 13 | MenuBarBg | Menu bar |
| 14 | ScrollbarBg | Scrollbar bg |
| 15 | ScrollbarGrab | Scrollbar grip |
| 16 | ScrollbarGrabHovered | Scrollbar hover |
| 17 | ScrollbarGrabActive | Scrollbar active |
| 18 | CheckMark | Checkbox mark |
| 19 | SliderGrab | Slider grip |
| 20 | SliderGrabActive | Slider active |
| 21 | Button | Button bg |
| 22 | ButtonHovered | Button hover |
| 23 | ButtonActive | Button active |
| 24 | Header | Header (tree, selectable) |
| 25 | HeaderHovered | Header hover |
| 26 | HeaderActive | Header active |
| 27 | Separator | Separator line |
| 28 | SeparatorHovered | Separator hover |
| 29 | SeparatorActive | Separator active |
| 30 | ResizeGrip | Resize grip |
| 31 | ResizeGripHovered | Resize hover |
| 32 | ResizeGripActive | Resize active |
| 33 | Tab | Tab bg |
| 34 | TabHovered | Tab hover |
| 35 | TabActive | Active tab |
| 36 | TabUnfocused | Unfocused tab |
| 37 | TabUnfocusedActive | Unfocused active |
| 38 | PlotLines | Plot lines |
| 39 | PlotLinesHovered | Plot line hover |
| 40 | PlotHistogram | Plot histogram |
| 41 | PlotHistogramHovered | Histogram hover |
| 42 | TableHeaderBg | Table header |
| 43 | TableBorderStrong | Table outer border |
| 44 | TableBorderLight | Table inner border |
| 45 | TableRowBg | Table row |
| 46 | TableRowBgAlt | Alt table row |
| 47 | TextSelectedBg | Selected text |
| 48 | DragDropTarget | Drag target |
| 49 | NavHighlight | Navigation highlight |
| 50 | NavWindowingHighlight | Windowing highlight |
| 51 | NavWindowingDimBg | Windowing dim |
| 52 | ModalWindowDimBg | Modal dim |
