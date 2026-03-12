# ImGui Research Sources

## Official Documentation

### Primary Sources
- **Dear ImGui GitHub Repository** (v1.92.6, Feb 2026)
  - URL: https://github.com/ocornut/imgui
  - Core library, examples, backends
  - License: MIT

- **Dear ImGui Wiki**
  - URL: https://github.com/ocornut/imgui/wiki
  - Comprehensive documentation and guides

- **ImGui FAQ**
  - URL: https://github.com/ocornut/imgui/blob/master/docs/FAQ.md
  - Common questions and troubleshooting

### Feature Documentation
- **Docking System**
  - Wiki: https://github.com/ocornut/imgui/wiki/Docking
  - Status: Available in `docking` branch, stable
  
- **Multi-Viewports**
  - Wiki: https://github.com/ocornut/imgui/wiki/Multi-Viewports
  - Enables windows outside main window

- **Font System**
  - Docs: https://github.com/ocornut/imgui/blob/master/docs/FONTS.md
  - Icon fonts, custom fonts, glyph ranges

## Extensions and Libraries

### Plotting
- **ImPlot** (2020-2025)
  - URL: https://github.com/epezent/implot
  - 2D plotting library for ImGui
  - GPU accelerated, real-time data
  - License: MIT

- **ImPlot3D** (2024)
  - URL: https://github.com/brenocq/implot3d
  - Experimental 3D plotting extension

### Manipulation
- **ImGuizmo** (2016-2025)
  - URL: https://github.com/CedricGuillemet/ImGuizmo
  - 3D translation, rotation, scale gizmos
  - View manipulation cube
  - License: MIT

- **imGuIZMO.quat** (2020-2025)
  - URL: https://github.com/BrutPitt/imGuIZMO.quat
  - Quaternion-based 3D manipulation

### File Dialogs
- **ImFileDialog** (2021)
  - URL: https://github.com/dfranx/ImFileDialog
  - File browser with thumbnails, favorites
  - License: MIT

- **ImGuiFileDialog** (2019-2024)
  - URL: https://github.com/aiekick/ImGuiFileDialog
  - Advanced file dialog with bookmarks, styling

### UI Components
- **imgui-notify / ImGuiNotify** (2021-2024)
  - URL: https://github.com/TyomaVader/ImGuiNotify
  - Toast notifications
  - License: MIT

- **imspinner** (2022-2023)
  - URL: https://github.com/dalerank/imspinner
  - Animated loading spinners

- **imgui-knobs** (2022-2023)
  - URL: https://github.com/altschuler/imgui-knobs
  - Rotary knob widgets

### Text Editors
- **ImGuiColorTextEdit** (2017-2019, fork 2026)
  - URL: https://github.com/pthom/ImGuiColorTextEdit
  - Syntax highlighting text editor

- **Zep** (2018-2023)
  - URL: https://github.com/Rezonality/zep
  - Vim-like embeddable editor

### Node Editors
- **imgui-node-editor** (2016-2023)
  - URL: https://github.com/thedmd/imgui-node-editor
  - Blueprint-style node editor

- **imnodes** (2019-2024)
  - URL: https://github.com/Nelarius/imnodes
  - Lightweight node editor

- **ImNodeFlow** (2024-2025)
  - URL: https://github.com/Fattorino/ImNodeFlow
  - Modern node-based editor

### Memory/Hex Editors
- **imgui_memory_editor** (2017-2024)
  - URL: https://github.com/ocornut/imgui_club
  - Hexadecimal memory editor

### Markdown/Text
- **imgui_markdown** (2019-2024)
  - URL: https://github.com/juliettef/imgui_markdown
  - Markdown rendering

- **imgui_md** (2021)
  - URL: https://github.com/mekhontsev/imgui_md
  - MD4C-based markdown renderer

### Layout Tools
- **ImRAD** (2023-2025)
  - URL: https://github.com/tpecholt/imrad
  - GUI builder/code generator

- **FellowImGui** (2024)
  - URL: https://fellowimgui.dev
  - Visual editor

## Themes and Styling

### Theme Collections
- **imgui-spectrum** (Adobe)
  - URL: https://github.com/adobe/imgui
  - Adobe Spectrum design system

- **Official Gallery**
  - URL: https://github.com/ocornut/imgui/issues?q=label%3Agallery
  - User-submitted screenshots and themes

### Color Palettes
- **Dracula Theme** - https://draculatheme.com/
- **Monokai Pro** - https://monokai.pro/
- **Material Design** - https://material.io/design/color/

## Video Tutorials

### YouTube Channels
- **TheCherno** - OpenGL/ImGui series
- **Gamefromscratch** - Dear ImGui overview
- **C++ Weekly** - SFML & Dear ImGui

### Conference Talks
- **CppCon 2016** - Nicolas Guillemot "Dear ImGui"
  - URL: https://www.youtube.com/watch?v=LSRJ1jZq90k

## Articles and Guides

- **Immediate Mode GUI Paradigm**
  - Casey Muratori's original concept (2005)
  - Article: http://www.mollyrocket.com/861

- **Getting Started Guide**
  - Wiki: https://github.com/ocornut/imgui/wiki/Getting-Started

- **About Examples**
  - Docs: https://github.com/ocornut/imgui/blob/master/docs/EXAMPLES.md

- **Backend Guide**
  - Docs: https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md

## Community Resources

### Discussion Forums
- GitHub Issues: https://github.com/ocornut/imgui/issues
- GitHub Discussions: https://github.com/ocornut/imgui/discussions

### Bindings
- **cimgui** - C bindings
  - URL: https://github.com/cimgui/cimgui

- **dear_bindings** - Auto-generated bindings
  - URL: https://github.com/dearimgui/dear_bindings

- Language support: C, C#, Python, Rust, Go, Java, and many more

### Third-Party Frameworks
- **Hello ImGui** - Quick multiplatform apps
  - URL: https://github.com/pthom/hello_imgui

- **imgui_bundle** - C++/Python bindings
  - URL: https://github.com/pthom/imgui_bundle

- **Walnut** - Vulkan + ImGui framework
  - URL: https://github.com/TheCherno/Walnut

## Version Information

- **Research Date**: March 2026
- **ImGui Version**: 1.92.6 (master/docking branch)
- **ImPlot Version**: Latest from 2020-2025
- **Primary Platforms**: Windows, Linux, macOS
- **Graphics APIs**: OpenGL 3.x, DirectX 9/10/11/12, Vulkan, Metal, WebGPU

## Key Takeaways

1. **Use docking branch** for professional applications
2. **Multi-viewport** enables modern window management
3. **ID uniqueness** is critical for interactive widgets
4. **ImPlot** is essential for data visualization
5. **ImGuizmo** for 3D content creation tools
6. **Theme customization** is straightforward via ImGuiStyle
7. **Performance** requires 32-bit indices for large datasets
8. **Extensions** ecosystem is mature and well-maintained

## Research Confidence

- Official docs/examples: **High** (primary source)
- Extension libraries: **Medium-High** (community maintained)
- Theme examples: **Medium** (extracted from gallery)
- Best practices: **High** (based on FAQ and official docs)
