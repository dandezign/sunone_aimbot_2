# SAM3TensorRT UI Design Specification

## Overview

This document describes the visual design and layout of the SAM3TensorRT image annotation editor UI. The specification is framework-agnostic and can be implemented in any UI system (ImGui, Qt, web, etc.).

---

## Application Type

- **Purpose:** Image/video annotation editor for computer vision workflows
- **Theme:** Dark theme (deep gray with emerald accents)
- **Window Size:** 1014 x 730 pixels (fixed)
- **Layout Style:** Three-panel master-detail layout with collapsible sidebar

---

## Layout Structure

### Main Window Layout

The application window is divided into three horizontal sections:

- TOP SECTION: Main Content Area (538 pixels height)
  - LEFT: Left Panel (256 pixels width) - Collapsible sidebar with configuration
  - RIGHT: Canvas Area (758 pixels width) - Image/video editing workspace
- BOTTOM SECTION: Log Console (192px expanded / 40px collapsed) - System feedback

---

## Color Palette

### Background Colors

| Name | Hex Code | RGB Values | Usage |
|------|----------|------------|-------|
| Primary Background | #09090b | rgb(9, 9, 11) | Main window, canvas area, log panel background |
| Secondary Background | #18181b | rgb(24, 24, 27) | Toolbar, panel headers, log header |
| Tertiary Background | #27272a | rgb(39, 39, 42) | Hover states, expanded sections, borders |
| Input Background | #09090b | rgb(9, 9, 11) | Form input fields |

### Text Colors

| Name | Hex Code | RGB Values | Usage |
|------|----------|------------|-------|
| Primary Text | #f4f4f5 | rgb(244, 244, 245) | Headings, important labels |
| Secondary Text | #d4d4d8 | rgb(212, 212, 216) | Body text, log messages |
| Muted Text | #9f9fa9 | rgb(159, 159, 169) | Navigation labels, icons, less important info |
| Meta Text | #71717b | rgb(113, 113, 123) | Metadata, dimension labels |
| Timestamp Text | #52525c | rgb(82, 82, 92) | Log timestamps |

### Accent Colors

| Name | Hex Code | RGB Values | Usage |
|------|----------|------------|-------|
| Success Green | #05df72 | rgb(5, 223, 114) | Success log messages |
| Active Green | #00bc7d | rgb(0, 188, 125) | Active toggle buttons, slider thumbs, focus borders |
| Warning Yellow | #fdc700 | rgb(253, 199, 0) | Warning log messages |
| Info Blue | #51a2ff | rgb(81, 162, 255) | Info log messages |

### Border Colors

| Name | Hex Code | RGB Values | Usage |
|------|----------|------------|-------|
| Primary Border | #27272a | rgb(39, 39, 42) | All panel dividers, borders |
| Input Border | #3f3f46 | rgb(63, 63, 70) | Form input borders |

---

## Typography

### Font Family
- **Primary Font:** Inter
- **Fallbacks:** System sans-serif fonts
- **Monospace:** System monospace (for log console)

### Font Weights Used
- 400 (Regular) - body text, metadata
- 500 (Medium) - labels, navigation
- 600 (SemiBold) - section headers
- 700 (Bold) - log levels, emphasis

### Text Sizes

| Element | Size | Weight | Color |
|---------|------|--------|-------|
| Section Headers | 14px | SemiBold | Primary Text |
| Panel Titles | 14px | Medium | Primary Text |
| Navigation Labels | 14px | Medium | Muted Text |
| Form Labels | 12px | Medium | Muted Text |
| Body Text | 12px | Regular | Secondary Text |
| Log Timestamps | 12px | Regular | Timestamp Text |
| Log Levels | 12px | Bold | Level-specific |
| Log Messages | 12px | Regular | Secondary Text |
| Metadata | 12px | Regular | Meta Text |

---

## Component Specifications

### 1. Left Panel

**Dimensions:**
- Width: 256 pixels
- Height: Fills available space (538 pixels)

**Visual Properties:**
- Background: Secondary Background (#18181b)
- Right border: 1 pixel, Primary Border color

**Header Section:**
- Height: 53 pixels
- Padding: 16 pixels all sides
- Bottom border: 1 pixel, Primary Border color
- Contains: "Editor Options" title (14px, SemiBold, Primary Text)

**Navigation Section:**
- Top padding: 12 pixels
- Left/Right padding: 12 pixels
- Vertical gap between items: 4 pixels

**Navigation Item (each):**
- Height: 40 pixels
- Left/Right padding: 12 pixels
- Border radius: 8 pixels (rounded corners)
- Background when inactive: Transparent
- Background when hovered: Tertiary Background
- Background when active/expanded: Tertiary Background

**Navigation Item Contents:**
- Icon: 20x20 pixels, left side, Muted Text color
- Label: 14px Medium, Muted Text color, positioned after icon with 12px gap
- Expand indicator: 16x16 pixel chevron icon, right side, rotates 90 degrees when expanded

**Expanded Section Content:**
- Background: Tertiary Background
- Border radius: 8 pixels
- Padding: 12 pixels
- Top margin: 8 pixels
- Contains form controls specific to each section

---

### 2. Capture Section (Expanded Content)

**Capture Mode Dropdown:**
- Label above: "Capture Mode" (12px Medium, Muted Text), 6px gap below
- Dropdown width: 100%
- Dropdown height: 32 pixels (approximately)
- Options: Window, Fullscreen, Region

**Interval Slider:**
- Label: "Interval: Xms" (12px Medium, Muted Text)
- Slider track: 4 pixels height, Input Background color
- Slider thumb: Circular, Active Green color
- Range: 50ms to 500ms
- Step: 10ms

**Auto Capture Toggle Button:**
- Width: 100%
- Height: 32 pixels (approximately)
- Border radius: 4 pixels

**Toggle Button States:**
- ON state: Background Active Green, Text Primary Background color
- OFF state: Background Input Background, Text Muted Text, 1 pixel border in Input Border color
- Text: "Auto Capture: On" or "Auto Capture: Off" (12px Medium)

---

### 3. Labels Section (Expanded Content)

**Label Class Dropdown:**
- Label: "Label Class" (12px Medium, Muted Text)
- Options: Person, Vehicle, Object
- Same styling as Capture Mode dropdown

**Preset Dropdown:**
- Label: "Preset" (12px Medium, Muted Text)
- Options: Default, Training, Debug
- Same styling as other dropdowns

---

### 4. Settings Section (Expanded Content)

**Confidence Slider:**
- Label: "Confidence: 0.XX" (12px Medium, Muted Text)
- Range: 0.10 to 0.99
- Step: 0.01
- Default: 0.45

**IoU Threshold Slider:**
- Label: "IoU Threshold: 0.XX" (12px Medium, Muted Text)
- Range: 0.10 to 0.95
- Step: 0.01
- Default: 0.35

**Live Preview Toggle:**
- Same styling as Auto Capture toggle
- Text: "Live Preview: On/Off"

**Snap To Grid Toggle:**
- Same styling as other toggles
- Text: "Snap To Grid: On/Off"

---

### 5. Toolbar (Main Canvas Area)

**Dimensions:**
- Height: 48 pixels
- Width: Fills canvas area (758 pixels)
- Background: Secondary Background
- Bottom border: 1 pixel, Primary Border color
- Left/Right padding: 16 pixels

**Left Side Contents:**
- "Canvas" title: 14px Medium, Primary Text
- Gap: 8 pixels
- Dimensions display: "1920 x 1080" (12px Regular, Meta Text)

**Right Side Contents (Zoom Controls):**

**Zoom Out Button:**
- Size: 32x32 pixels
- Border radius: 4 pixels
- Icon: 16x16 pixels, minus symbol
- Background when hovered: Tertiary Background

**Zoom Percentage Text:**
- Width: 64 pixels
- Text alignment: Center
- Font: 14px Regular, Muted Text
- Shows current zoom (e.g., "25%")

**Zoom In Button:**
- Size: 32x32 pixels
- Border radius: 4 pixels
- Icon: 16x16 pixels, plus symbol
- Background when hovered: Tertiary Background

**Fit Button:**
- Size: 32x32 pixels
- Border radius: 4 pixels
- Icon: 16x16 pixels, expand/arrows symbol
- Background when hovered: Tertiary Background

**Zoom Control Spacing:**
- Gap between elements: 4 pixels

---

### 6. Canvas Area

**Dimensions:**
- Width: Fills remaining space (758 pixels)
- Height: Fills remaining space (490 pixels after toolbar)
- Background: Primary Background
- Top padding: 48 pixels (toolbar offset)

**Canvas Element:**
- Size: 640x640 pixels (placeholder)
- Position: Centered with 32px top margin, 52px left margin
- Background: Primary Background
- Shadow: Soft drop shadow (20px blur, 25px offset, 10% black opacity)
- Transform: Scales based on zoom percentage (origin: top-left corner)

**Canvas Interaction:**
- Supports zoom scaling (10% minimum, 200% maximum)
- Zoom step: 5% per click

---

### 7. Log Console Panel

**Dimensions:**
- Width: Full window width (1014 pixels)
- Height: 192 pixels (expanded) or 40 pixels (collapsed)
- Background: Primary Background
- Top border: 1 pixel, Primary Border color
- Transition: Height animates over 200ms when collapsing/expanding

**Header Section:**
- Height: 40 pixels
- Background: Secondary Background
- Bottom border: 1 pixel, Primary Border color (only when expanded)
- Left/Right padding: 16 pixels

**Header Left Contents:**
- Log icon: 16x16 pixels, Muted Text color
- Gap: 8 pixels
- "Log Console" title: 14px Medium, Primary Text
- Gap: 8 pixels
- Entry count: "(X entries)" (12px Regular, Meta Text)

**Header Right Contents:**
- Collapse/Expand button: 24x24 pixels
- Icon: X symbol (16x16 centered)
- Background when hovered: Tertiary Background
- Border radius: 4 pixels

**Log Entries Container:**
- Height: 152 pixels (192 total - 40 header)
- Scrollable when more entries exist
- Padding: 12 pixels left/right

**Each Log Entry:**
- Height: 32 pixels
- Left/Right padding: 12 pixels
- Bottom border: 1 pixel, Primary Border color (except last entry)
- Layout: Horizontal row with three columns

**Log Entry Columns:**
1. Timestamp column: 75 pixels width, Monospace 12px, Timestamp Text color
2. Level column: 70 pixels width, Monospace 12px Bold, Uppercase
   - INFO: Info Blue color
   - SUCCESS: Success Green color
   - WARNING: Warning Yellow color
3. Message column: Remaining width, Monospace 12px, Secondary Text color, truncated if too long

**Log Buffer:**
- Maximum displayed entries: 8 (older entries removed FIFO)

---

## Form Control Styles

### Dropdown/Select

**Dimensions:**
- Height: 32 pixels (approximately)
- Width: 100% of container
- Left/Right padding: 8 pixels
- Top/Bottom padding: 6 pixels

**Visual Properties:**
- Background: Input Background
- Text color: Secondary Text
- Border: 1 pixel, Input Border color
- Border radius: 4 pixels
- Border on focus: Active Green color

**Dropdown Arrow:**
- Standard system dropdown indicator

---

### Range Slider

**Track:**
- Height: 4 pixels
- Background: Input Background
- Border radius: Fully rounded (cylindrical)
- Width: 100% of container

**Thumb/Handle:**
- Shape: Circle
- Size: 16x16 pixels
- Color: Active Green
- No border

**Behavior:**
- Smooth dragging
- Updates label in real-time

---

### Toggle Button

**Dimensions:**
- Height: 32 pixels (approximately)
- Width: 100% of container
- Left/Right padding: 8 pixels
- Top/Bottom padding: 6 pixels
- Border radius: 4 pixels

**ON State:**
- Background: Active Green
- Text: Primary Background color (dark)
- No border

**OFF State:**
- Background: Input Background
- Text: Muted Text color
- Border: 1 pixel, Input Border color

**Interaction:**
- Click to toggle state
- Immediate visual feedback
- Smooth color transition

---

## Icons

All icons use a consistent stroke-based style.

### Icon Specifications

| Icon Name | Size | Stroke Width | Color |
|-----------|------|--------------|-------|
| Capture | 20x20 px | 1.2 px | Muted Text |
| Labels | 20x20 px | 1.2 px | Muted Text |
| Settings | 20x20 px | 1.2 px | Muted Text |
| Zoom In | 16x16 px | 1.2 px | Muted Text |
| Zoom Out | 16x16 px | 1.2 px | Muted Text |
| Fit | 16x16 px | 1.2 px | Muted Text |
| Log | 16x16 px | 1.2 px | Muted Text |
| Close | 16x16 px | 1.2 px | Muted Text |
| Chevron | 16x16 px | 1.4 px | Muted Text |

### Icon Descriptions

**Capture Icon:**
- Rectangle with rounded corners (camera viewport)
- Small triangle on right side (recording indicator)

**Labels Icon:**
- Tag shape with pointed end
- Small circle near top (attachment point)

**Settings Icon:**
- Circle in center
- Radiating lines (gear teeth)

**Zoom In:**
- Circle outline
- Plus symbol inside

**Zoom Out:**
- Circle outline
- Minus symbol inside

**Fit Icon:**
- Four arrows pointing outward from corners

**Log Icon:**
- Terminal/console window shape
- Small lines representing text

**Close Icon:**
- X symbol

**Chevron Icon:**
- Right-pointing arrow
- Rotates 90 degrees clockwise when section expands

---

## Spacing System

| Spacing Token | Value | Common Usage |
|---------------|-------|--------------|
| Extra Small | 4 px | Tight gaps between buttons |
| Small | 8 px | Icon gaps, button padding |
| Medium | 12 px | Section content padding, log entry padding |
| Large | 16 px | Standard panel padding, toolbar padding |
| Extra Large | 24 px | Large gaps |
| 2X Large | 32 px | Canvas margins, button sizes |

---

## Border Specifications

### Border Widths
- Standard borders: 1 pixel
- Some design elements use 0.8 pixels (thinner than standard)

### Border Radius
- Small radius: 4 pixels (buttons, inputs)
- Medium radius: 8 pixels (panels, navigation items)
- Full radius: Circular (slider thumbs, icon backgrounds on hover)

---

## Shadows

### Canvas Shadow
- Type: Drop shadow
- Blur: 20 pixels
- Offset: 0px horizontal, 25px vertical
- Color: Black at 10% opacity
- Secondary shadow: Blur 8px, offset 10px, same color

---

## Animations and Transitions

### Transition Durations
- Standard transitions: 200 milliseconds
- Easing: Smooth (ease-in-out equivalent)

### Animated Elements
- Log panel collapse/expand: Height animates over 200ms
- Chevron rotation: Rotate 90 degrees over 200ms
- Hover state changes: Background color transitions over 200ms

---

## Component Interaction States

### Buttons (General)
| State | Background | Text Color | Border |
|-------|------------|------------|--------|
| Normal | Transparent or specified | Muted Text | None |
| Hovered | Tertiary Background | Muted Text | None |
| Active/Pressed | Tertiary Background | Muted Text | None |

### Toggle Buttons
| State | Background | Text Color | Border |
|-------|------------|------------|--------|
| OFF | Input Background | Muted Text | 1px Input Border |
| ON | Active Green | Primary Background | None |

### Dropdowns
| State | Border Color |
|-------|--------------|
| Normal | Input Border |
| Focused | Active Green |
| Hovered | Input Border |

### Navigation Items
| State | Background |
|-------|------------|
| Normal | Transparent |
| Hovered | Tertiary Background |
| Active/Expanded | Tertiary Background |

---

## User Interaction Patterns

### Section Navigation (Accordion Pattern)
1. Only one section can be expanded at a time
2. Clicking a collapsed section expands it
3. Clicking an expanded section collapses it
4. Chevron rotates 90 degrees when expanded
5. Log entry is generated: "Opened [Section] panel"

### Zoom Controls
1. Zoom In: Increases zoom by 5%, max 200%
2. Zoom Out: Decreases zoom by 5%, min 10%
3. Fit: Resets zoom to default (25%)
4. Canvas scales via transform (zoom/100)
5. Log entry generated for each zoom action

### Log Console
1. Shows last 8 entries (FIFO buffer)
2. Can be collapsed to header only (40px)
3. Can be expanded to show entries (192px)
4. Entry count displayed in header
5. Each entry has timestamp, level, message

### Toggle Buttons
1. Click to toggle between ON and OFF
2. Immediate visual state change
3. Log entry generated: "[Feature] enabled/disabled"

---

## Complete Color Reference

### Color Swatches (for Copy-Paste)



---

## Implementation Notes

### Critical Requirements
1. Use Inter font family (or similar sans-serif)
2. Maintain exact color values for dark theme consistency
3. Implement accordion behavior for left panel sections
4. Support zoom scaling on canvas area
5. Implement FIFO log buffer (8 entries max)

### Suggested Implementation Order
1. Set up main window layout (3 regions)
2. Implement left panel with static structure
3. Add accordion expand/collapse behavior
4. Add form controls to each section
5. Implement toolbar with zoom controls
6. Add canvas placeholder
7. Implement log console with collapse
8. Add all interactions and state management

---

## Summary

This UI features a clean, dark-themed design optimized for image annotation workflows. The three-panel layout provides:

- **Left Panel:** Configuration controls organized in collapsible sections
- **Center Canvas:** Main workspace for image/video editing with zoom controls
- **Bottom Log:** Real-time system feedback with color-coded entries

The design uses a limited color palette (zinc grays + emerald accents) with consistent spacing, typography, and interaction patterns throughout.
