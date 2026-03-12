# Sunone Aimbot 2 - Codebase Analysis

> **Generated for refactoring reference**  
> This document provides a comprehensive analysis of the codebase architecture, components, and recommended refactoring targets.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Directory Structure](#2-directory-structure)
3. [Key Components](#3-key-components)
4. [Build System](#4-build-system)
5. [External Dependencies](#5-external-dependencies)
6. [Hardware Abstraction](#6-hardware-abstraction-layers)
7. [Extension Points](#7-extension-points)
8. [Code Quality Assessment](#8-code-quality-assessment)
9. [Refactoring Priorities](#9-refactoring-priorities)
10. [Threading Model](#10-threading-model)

---

## 1. Architecture Overview

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     sunone_aimbot_2.exe                         │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │   Capture   │  │  Inference  │  │      Mouse Control      │ │
│  │   Thread    │→ │   Thread    │→ │       Thread            │ │
│  │             │  │  (TRT/DML)  │  │                         │ │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘ │
│         ↓                ↓                    ↓                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │  FrameQueue │  │ Detection   │  │    Input Devices        │ │
│  │             │  │   Buffer    │  │  (Arduino/GHUB/KMBOX)   │ │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘ │
│         ↓                ↓                                      │
│  ┌─────────────┐  ┌─────────────────────────────────────────┐  │
│  │Game Overlay │  │        Settings UI (ImGui)              │  │
│  │   Thread    │  │       (DirectX 11 Backend)              │  │
│  └─────────────┘  └─────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Screen Capture → Frame Preprocessing → AI Inference → 
Post-Processing (NMS) → Target Selection → 
Movement Calculation → Input Method → Mouse Driver
```

### Thread Responsibilities

| Thread | File | Purpose |
|--------|------|---------|
| Capture | `capture/capture.cpp` | Grabs frames from screen at configured FPS |
| Inference | `detector/*.cpp` | Runs YOLO detection on frames |
| Mouse | `mouse/mouse.cpp` | Calculates and sends mouse movements |
| Overlay UI | `overlay/overlay.cpp` | Renders ImGui settings window |
| Game Overlay | `overlay/Game_overlay.cpp` | In-game ESP visualization |
| Keyboard | `keyboard/keyboard_listener.cpp` | Hotkey handling |

---

## 2. Directory Structure

```
sunone_aimbot_2/
├── sunone_aimbot_2/           # Main source code
│   ├── capture/               # Screen capture implementations
│   │   ├── capture.cpp        # Main capture thread orchestrator
│   │   ├── duplication_api_capture.cpp  # DXGI Desktop Duplication
│   │   ├── winrt_capture.cpp  # Windows Runtime GraphicsCapture
│   │   ├── virtual_camera.cpp # Virtual camera support (OBS, etc.)
│   │   └── udp_capture.cpp    # Network frame streaming
│   ├── config/                # Configuration management
│   │   ├── config.cpp         # INI parser, game profiles
│   │   └── config.h           # Config struct (230+ fields)
│   ├── depth/                 # Depth estimation (CUDA only)
│   │   ├── depth_anything_trt.cpp  # Depth Anything TensorRT
│   │   └── depth_mask.cpp     # Depth-based masking
│   ├── detector/              # AI inference engines
│   │   ├── trt_detector.cpp   # TensorRT implementation
│   │   ├── dml_detector.cpp   # DirectML/ONNX Runtime
│   │   └── postProcess.cpp    # NMS, YOLO output parsing
│   ├── imgui/                 # ImGui library (embedded)
│   ├── keyboard/              # Hotkey handling
│   ├── mem/                   # Memory/GPU resource management
│   ├── modules/               # Third-party modules
│   │   ├── imgui-1.91.2/      # ImGui complete distribution
│   │   ├── makcu/             # MAKCU hardware interface
│   │   ├── serial/            # Serial port library
│   │   └── stb/               # STB image library
│   ├── mouse/                 # Mouse control implementations
│   │   ├── mouse.cpp          # Main mouse thread (~800 lines)
│   │   ├── Arduino.cpp        # Arduino Leonardo/Pro Micro
│   │   ├── ghub.cpp           # Logitech G Hub DLL
│   │   ├── kmboxA.cpp         # KMBOX A-series (USB)
│   │   ├── kmbox_net/         # KMBOX NET (WiFi/Ethernet)
│   │   └── Makcu.cpp          # MAKCU hardware
│   ├── overlay/               # UI and visualization
│   │   ├── overlay.cpp        # ImGui settings window
│   │   ├── Game_overlay.cpp   # In-game ESP overlay
│   │   └── draw_*.cpp         # UI section implementations
│   ├── scr/                   # Utility functions
│   └── tensorrt/              # TensorRT utilities
├── docs/                      # Documentation
├── tools/                     # Build/setup scripts
└── packages/                  # NuGet packages
```

---

## 3. Key Components

### 3.1 Main Entry Point (`sunone_aimbot_2.cpp`)

**Global State Management:**
```cpp
Config config;                          // Central configuration singleton
std::atomic<bool> shouldExit;           // Thread shutdown flag
std::atomic<bool> aiming;               // Aim activation state
std::atomic<bool> detectionPaused;      // Pause inference

// Change notification atomics for hot-reloading
std::atomic<bool> mouse_method_changed;
std::atomic<bool> capture_method_changed;
```

**Key Global Pointers (Lines 57-61):**
```cpp
KmboxNetConnection* kmbox_net = nullptr;
KmboxA* kmbox_a = nullptr;
Makcu* makcu = nullptr;
Arduino* arduino = nullptr;
Ghub* gHub = nullptr;
```

**Thread Spawning (Lines ~200-250):**
```cpp
std::thread captureThread(captureThread, ...);
std::thread inferenceThread(...);
std::thread mouseThread(mouseThreadFunction, ...);
std::thread overlayThread(OverlayThread);
std::thread gameOverlayThread(gameOverlayRenderLoop);
std::thread keyboardThread(keyboardListenerThread);
```

### 3.2 Configuration System (`config/config.h`, `config.cpp`)

**Config Class Structure (~230 fields):**

| Category | Key Fields |
|----------|------------|
| Capture | `capture_method`, `capture_fps`, `capture_resolution` |
| Mouse | `mouse_speed`, `fov`, `prediction_enabled`, `wind_mouse_enabled` |
| Input | `input_method`, `arduino_port`, `kmbox_net_ip`, etc. |
| AI | `ai_backend`, `model_path`, `confidence_threshold` |
| Overlay | `show_fps`, `show_detections`, `overlay_color` |

**Game Profile System:**
```cpp
struct GameProfile {
    std::string name;
    double sens;          // Mouse sensitivity
    double yaw;           // Degrees per count (horizontal)
    double pitch;         // Degrees per count (vertical)
    bool fovScaled;       // Scale aim by FOV difference
    double baseFOV;       // Reference FOV for scaling
};
```

### 3.3 Detection Components

**TensorRT Detector** (`detector/trt_detector.h`):
- CUDA stream-based inference pipeline
- CUDA Graph support for reduced CPU overhead
- Pinned memory for async transfers
- GPU preprocess path (avoids CPU→GPU copy)

**DirectML Detector** (`detector/dml_detector.h`):
- ONNX Runtime with DirectML execution provider
- Cross-vendor GPU support (AMD, Intel, NVIDIA)
- Simpler architecture than TRT (no CUDA dependencies)

**DetectionBuffer** (`detector/detection_buffer.h`):
- Thread-safe producer/consumer queue
- Versioned updates (prevents stale data consumption)

### 3.4 Mouse Control System (`mouse/mouse.h`, `mouse.cpp`)

**MouseThread Class Features:**
- **Speed Curve**: Distance-based multiplier with snap/near zones
- **Prediction**: Velocity-based target position estimation
- **WindMouse**: Human-like movement simulation with noise/gravity
- **Multi-Device**: Runtime switchable input methods

**Speed Calculation Algorithm:**
```cpp
if (distance < snapRadius)
    return minSpeed * snapBoostFactor;
if (distance < nearRadius)
    return minSpeed + (maxSpeed - minSpeed) * curve(distance/nearRadius);
return minSpeed + (maxSpeed - minSpeed) * normalizedDistance;
```

### 3.5 Screen Capture System (`capture/capture.h`, `capture.cpp`)

**Capture Methods:**
1. **Desktop Duplication API** - Fastest, Windows 8.1+, GPU-efficient
2. **WinRT GraphicsCapture** - Modern Windows API, window/monitor selection
3. **Virtual Camera** - OBS Virtual Camera, other DirectShow sources
4. **UDP Capture** - Network streaming for remote processing

---

## 4. Build System

### Dual Configuration Architecture

| Configuration | Defines | Key Files |
|---------------|---------|-----------|
| **CUDA** | `USE_CUDA` | `trt_detector.cpp`, `cuda_preprocess.cu`, `nvinf.cpp` |
| **DML** | (none) | `dml_detector.cpp` only |

### vcxproj Structure

**Conditional Compilation:**
```xml
<ClCompile Include="detector\trt_detector.cpp">
    <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='DML|x64'">true</ExcludedFromBuild>
</ClCompile>
<CudaCompile Include="detector\cuda_preprocess.cu">
    <!-- Only in CUDA config -->
</CudaCompile>
```

### CMake Build

**Key Options:**
```cmake
option(AIMBOT_USE_CUDA "Enable CUDA + TensorRT backend" OFF)
option(AIMBOT_COPY_RUNTIME_DLLS "Copy runtime DLLs to output" ON)
```

**CUDA Language Support:**
```cmake
if(AIMBOT_USE_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --allow-unsupported-compiler")
endif()
```

### NuGet Packages

| Package | Purpose | Version |
|---------|---------|---------|
| `Microsoft.ML.OnnxRuntime.DirectML` | ONNX inference + DirectML | 1.22.0 |
| `Microsoft.AI.DirectML` | DirectML runtime | 1.15.4 |
| `Microsoft.Windows.CppWinRT` | WinRT/C++ projection | 2.0.240405.15 |

---

## 5. External Dependencies

### Core Libraries

| Library | Purpose | Version | Linking |
|---------|---------|---------|---------|
| **OpenCV** | Image processing, CUDA operations | 4.13.0 | Static (world) |
| **ONNX Runtime** | Cross-platform inference | 1.22.0 | Dynamic (NuGet) |
| **TensorRT** | NVIDIA optimized inference | 10.14.1.48 | Static |
| **DirectML** | Windows ML acceleration | 1.15.4 | Dynamic (NuGet) |
| **ImGui** | Immediate-mode GUI | 1.91.2 | Source |
| **cuDNN** | CUDA deep learning primitives | 9.x | Dynamic |

### OpenCV Usage Patterns

**CUDA Builds:**
- `cv::cuda::GpuMat` for GPU frame storage
- `cv::cuda::Stream` for async operations
- `cv::cuda::resize` for GPU preprocessing

**DML Builds:**
- CPU-only `cv::Mat` operations
- Standard `cv::resize`, `cv::cvtColor`

---

## 6. Hardware Abstraction Layers

### 6.1 Mouse Input Abstraction

**Unified Interface** (via `MouseThread`):
```cpp
void sendMovementToDriver(int dx, int dy) {
    if (kmbox_net) kmbox_net->move(dx, dy);
    else if (kmbox_a) kmbox_a->move(dx, dy);
    else if (makcu) makcu->move(dx, dy);
    else if (arduino) arduino->move(dx, dy);
    else if (gHub) gHub->mouse_xy(dx, dy);
    else SendInput(1, &input, sizeof(INPUT));
}
```

**Device Implementations:**

| Device | Connection | Protocol | Location |
|--------|-----------|----------|----------|
| **Win32** | System API | `SendInput()` | `mouse/mouse.cpp` |
| **GHUB** | DLL Injection | Logitech SDK | `mouse/ghub.cpp` |
| **Arduino** | Serial (CDC) | Custom binary | `mouse/Arduino.cpp` |
| **KMBOX_NET** | UDP/TCP | Proprietary | `mouse/kmbox_net/` |
| **KMBOX_A** | USB HID | Proprietary | `mouse/kmboxA.cpp` |
| **MAKCU** | Serial | Proprietary | `mouse/Makcu.cpp` |

### 6.2 Screen Capture Abstraction

**IScreenCapture Interface:**
```cpp
class IScreenCapture {
public:
    virtual ~IScreenCapture() {}
    virtual cv::Mat GetNextFrameCpu() = 0;
};
```

**Implementations:**
- `DuplicationAPIScreenCapture` - DXGI, supports GPU read
- `WinRTScreenCapture` - Modern Windows API
- `VirtualCameraCapture` - DirectShow filter graph
- `UDPCapture` - Socket-based streaming

### 6.3 GPU Abstraction

**CUDA Path** (USE_CUDA):
- TensorRT for inference
- OpenCV CUDA modules for preprocessing
- Custom CUDA kernels (`cuda_preprocess.cu`)

**DirectML Path**:
- ONNX Runtime with DML execution provider
- CPU-based preprocessing
- Cross-vendor compatibility

---

## 7. Extension Points

### 7.1 Adding New Input Devices

**Files to modify:**
1. `mouse/mouse.h` - Add device pointer and setter
2. `sunone_aimbot_2.cpp`:
   - Add global device pointer
   - Extend `createInputDevices()` 
   - Extend `assignInputDevices()`
3. `config/config.h` - Add configuration fields
4. `config/config.cpp` - Add load/save handlers
5. `mouse/` - Create new device class

**Pattern:**
```cpp
// New device class must implement:
class NewDevice {
public:
    bool isOpen() const;
    void move(int x, int y);
    void press();
    void release();
};
```

### 7.2 Adding New Capture Methods

**Implementation:**
1. Inherit from `IScreenCapture`
2. Implement `GetNextFrameCpu()`
3. Extend `capture/capture.cpp` factory function
4. Add configuration enum value

### 7.3 Adding New Inference Backends

**Extension:**
1. Create detector class matching `TrtDetector` interface
2. Add backend enum to config
3. Conditional compilation guard (e.g., `USE_OPENVINO`)
4. Link appropriate libraries

### 7.4 Game Profile Extensions

**Config Structure:**
The `GameProfile` struct supports arbitrary fields. Add new parameters:
```cpp
struct GameProfile {
    // Existing fields...
    double recoilCompensation;  // New field
    int weaponCategory;         // New field
};
```

---

## 8. Code Quality Assessment

### 8.1 Strengths

**Thread Safety:**
- Consistent use of `std::mutex` and `std::lock_guard`
- Atomic flags for state changes
- Condition variables for thread signaling
- Proper thread join in destructors

**Error Handling:**
- Thread crash handlers (`HandleThreadCrash`)
- Exception guards around thread entry points
- Graceful degradation (e.g., input device fallback)

**Performance:**
- Lock-free queues where appropriate
- CUDA Graphs for repeated inference
- Pinned memory for async transfers
- Frame rate limiters to prevent busy-waiting

**Configuration Management:**
- Centralized config with type safety
- Hot-reload capability via atomic change flags
- Game profile system for per-title customization

### 8.2 Areas for Improvement

**Memory Management:**
- Raw pointers for device objects (lines 57-61 in `sunone_aimbot_2.cpp`)
- Consider `std::unique_ptr` for automatic lifetime management
- Global state makes unit testing difficult

**Code Organization:**
- `sunone_aimbot_2.cpp` is very large (1700+ lines)
- Aim simulation code (600+ lines) could be separate class
- Global variables create tight coupling

**Magic Numbers:**
```cpp
// Lines 1278-1279: Arbitrary constants
const int staleMs = std::clamp(2000 / fps, 25, 180);
// Line 377: Undocumented delay value
double detectionDelay = 0.05;
```

**Complexity:**
- `moveMousePivot()` is 70+ lines with multiple responsibilities
- Wind mouse algorithm duplicates code between simulation and actual movement
- Consider strategy pattern for different aim styles

**Documentation:**
- No Doxygen comments in headers
- Complex algorithms (WindMouse, speed curves) lack inline documentation
- Configuration options documented externally only

---

## 9. Refactoring Priorities

### Priority 1: Extract Global State

**Problem:** Global pointers and config create tight coupling

**Solution:** Create a `ServiceLocator` or `ApplicationContext` class:
```cpp
class ApplicationContext {
public:
    Config& config();
    InputDeviceManager& inputDevices();
    std::shared_ptr<IScreenCapture> capture();
    std::shared_ptr<IDetector> detector();
    // ...
};
```

**Files affected:** `sunone_aimbot_2.cpp`, all device classes

### Priority 2: Extract Aim Simulation

**Problem:** 600+ lines of aim simulation code in main file

**Solution:** Create `AimSimulator` class:
```cpp
class AimSimulator {
public:
    void initialize(const Config& config);
    void update(float deltaTime);
    void render(ImDrawList* drawList);
    void reset();
};
```

**Files affected:** `sunone_aimbot_2.cpp` (lines 121-1025)

### Priority 3: Split Mouse Movement Logic

**Problem:** `moveMousePivot()` is 70+ lines, multiple responsibilities

**Solution:** Decompose into strategies:
```cpp
class IMovementStrategy {
public:
    virtual MovementResult calculate(
        const Target& target,
        const Config& config,
        float deltaTime
    ) = 0;
};

class WindMouseStrategy : public IMovementStrategy { /* ... */ };
class DirectMovementStrategy : public IMovementStrategy { /* ... */ };
```

**Files affected:** `mouse/mouse.cpp`

### Priority 4: Use Smart Pointers

**Problem:** Raw pointers for device management

**Solution:** Replace with `std::unique_ptr`:
```cpp
// Before
KmboxNetConnection* kmbox_net = nullptr;
// After
std::unique_ptr<KmboxNetConnection> kmbox_net;
```

**Files affected:** `sunone_aimbot_2.cpp`, all device headers

### Priority 5: Add Unit Tests

**Problem:** No test coverage for calculation-heavy functions

**Solution:** Extract pure functions and test:
- `calculateSpeed()` - Speed curve calculations
- `predictTargetPosition()` - Prediction algorithms
- `selectBestTarget()` - Target selection logic

**Files to create:** `tests/`

### Priority 6: Document Algorithms

**Problem:** Complex algorithms lack inline documentation

**Solution:** Add comments to:
- WindMouse algorithm implementation
- Speed curve interpolation
- Target prediction calculations
- NMS (Non-Maximum Suppression)

---

## 10. Threading Model

### Thread Communication

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Capture    │────→│  Detection   │────→│    Mouse     │
│    Thread    │     │   Buffer     │     │    Thread    │
└──────────────┘     └──────────────┘     └──────────────┘
       │                                           ↑
       └───────────────────────────────────────────┘
                   (Target Feedback)
```

### Synchronization Primitives

| Primitive | Purpose | Location |
|-----------|---------|----------|
| `std::mutex` | Device access | `mouse/mouse.cpp` |
| `std::atomic<bool>` | State flags | `sunone_aimbot_2.cpp` |
| `std::condition_variable` | Frame ready signaling | `detector/detection_buffer.cpp` |
| `std::atomic<int>` | Version counters | `detector/detection_buffer.cpp` |

### Thread Safety Patterns

**Producer-Consumer (DetectionBuffer):**
```cpp
class DetectionBuffer {
    std::mutex mutex_;
    std::condition_variable cv_;
    DetectionData data_;
    std::atomic<int> version_{0};
    
public:
    void push(const DetectionData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_ = data;
        version_++;
        cv_.notify_one();
    }
    
    bool pop(DetectionData& out, int expectedVersion) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return version_ > expectedVersion || shouldExit; });
        if (shouldExit) return false;
        out = data_;
        return true;
    }
};
```

---

## Notable Technical Features

### WindMouse Algorithm
Implements Ben Land's human-like mouse movement with:
- Perlin-like noise patterns
- Variable gravity and wind resistance
- Sub-step interpolation for smooth curves

### Target Tracking
Multi-target tracker with:
- Temporal consistency (locked target persistence)
- Class-based prioritization
- Depth-aware masking (CUDA only)

### Aim Simulation
Physics-based simulation for:
- Algorithm testing without game access
- Visualization of prediction behavior
- Parameter tuning with real-time feedback

---

## Summary

Sunone Aimbot 2 represents a professionally-architected real-time AI application with strong separation of concerns, dual-backend flexibility, and comprehensive hardware abstraction.

**For Extension:**
- The codebase is well-structured for adding new capture methods, input devices, or inference backends
- Hardware abstraction layers are properly isolated

**For Refactoring:**
- Priority: Extract global state into service locator pattern
- Extract aim simulation to separate module
- Split `moveMousePivot()` into smaller functions
- Add unit tests for calculation-heavy functions
- Implement smart pointer memory management

**Architecture Score: 7/10**
- Good: Thread safety, hardware abstraction, performance optimizations
- Needs work: Global state management, code organization, testing
