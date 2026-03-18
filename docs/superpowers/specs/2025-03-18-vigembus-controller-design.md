# ViGEmBus Controller Integration Design Specification

**Date:** 2025-03-18  
**Status:** Approved  
**Approach:** Bridge Pattern  
**Target:** sunone_aimbot_2 C++17 Windows Project

---

## 1. Executive Summary

This specification defines the integration of ViGEmBus virtual controller emulation into sunone_aimbot_2, enabling DualSense (PS5) controller input to be processed, optionally modified by the aimbot, and output as a virtual DualShock 4 controller via ViGEmBus.

**Key Capabilities:**
- Physical DualSense detection via HID (VID: 0x054C, multiple PIDs)
- Automatic reconnection on disconnect
- Virtual DualShock 4 output via ViGEmBus driver
- Aimbot injection into controller analog stick values
- Optional macro/script transformation layer
- Real-time UI overlay showing controller status

---

## 2. Architecture Overview

### 2.1 System Architecture (Bridge Pattern)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           sunone_aimbot_2                                   │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                         DETECTION & INPUT LAYER                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    HID Input Thread                                 │   │
│  │                                                                     │   │
│  │   ┌────────────┐    ┌────────────┐    ┌────────────────────────┐   │   │
│  │   │ Enumerate  │───▶│ Filter VID │───▶│ Open Device            │   │   │
│  │   │ Devices    │    │ 0x054C     │    │ (Sony DualSense)       │   │   │
│  │   │ (2s loop)  │    │ Check PIDs │    │                        │   │   │
│  │   └────────────┘    └────────────┘    └──────────┬─────────────┘   │   │
│  │         │                                        │                 │   │
│  │         │                              ┌─────────┴────────┐        │   │
│  │         │                              │ Register         │        │   │
│  │         │                              │ Callbacks        │        │   │
│  │         │                              │ (buttons, axes)  │        │   │
│  │         │                              └─────────┬────────┘        │   │
│  │         │                                        │                 │   │
│  │         │                              ┌─────────┴────────┐        │   │
│  │         │                              │ Push to Queue    │        │   │
│  │         │                              │ ControllerState  │        │   │
│  │         │                              └──────────────────┘        │   │
│  │         │                                                           │   │
│  │   ┌─────┴─────────────────────────────────────────────────────┐    │   │
│  │   │ On Disconnect: Close → Wait 750ms → Re-enumerate          │    │   │
│  │   └─────────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                               │                                             │
│                               ▼                                             │
│                    ┌──────────────────────┐                                │
│                    │ Thread-Safe Queue    │                                │
│                    │ (MPSC)               │                                │
│                    └──────────┬───────────┘                                │
└───────────────────────────────┼─────────────────────────────────────────────┘
                                │
                                ▼
┌───────────────────────────────┼─────────────────────────────────────────────┐
│                      BRIDGE & PROCESSING LAYER                              │
│                               │                                             │
│  ┌────────────────────────────┼─────────────────────────────────────────┐   │
│  │                    Bridge Thread                                      │   │
│  │                         │                                             │   │
│  │   ┌─────────────────────┼─────────────────────────────────────────┐  │   │
│  │   │ Read from Input Queue                                          │  │   │
│  │   │ (1000Hz polling)                                               │  │   │
│  │   └─────────────────────┬─────────────────────────────────────────┘  │   │
│  │                         │                                            │   │
│  │   ┌─────────────────────┴─────────────────────────────────────────┐  │   │
│  │   │ If aimbot active: Inject aim input (modify stick positions)   │  │   │
│  │   └─────────────────────┬─────────────────────────────────────────┘  │   │
│  │                         │                                            │   │
│  │   ┌─────────────────────┴─────────────────────────────────────────┐  │   │
│  │   │ If macros enabled: Apply script transformations               │  │   │
│  │   └─────────────────────┬─────────────────────────────────────────┘  │   │
│  │                         │                                            │   │
│  │   ┌─────────────────────┴─────────────────────────────────────────┐  │   │
│  │   │ Encode to DS4_REPORT binary format                            │  │   │
│  │   └─────────────────────┬─────────────────────────────────────────┘  │   │
│  │                         │                                            │   │
│  │   ┌─────────────────────┴─────────────────────────────────────────┐  │   │
│  │   │ Push to ViGEm Queue                                           │  │   │
│  │   └───────────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                               │                                             │
└───────────────────────────────┼─────────────────────────────────────────────┘
                                │
                                ▼
┌───────────────────────────────┼─────────────────────────────────────────────┐
│                         OUTPUT & VIRTUAL LAYER                              │
│                               │                                             │
│  ┌────────────────────────────┼─────────────────────────────────────────┐   │
│  │                    ViGEm Output Thread                                │   │
│  │                         │                                             │   │
│  │   ┌─────────────────────┼─────────────────────────────────────────┐  │   │
│  │   │ Read from Queue (sync to game FPS ~60-144Hz)                  │  │   │
│  │   └─────────────────────┬─────────────────────────────────────────┘  │   │
│  │                         │                                            │   │
│  │   ┌─────────────────────┴─────────────────────────────────────────┐  │   │
│  │   │ Call vigem_target_ds4_update()                                │  │   │
│  │   │ [ViGEmClient.dll]                                             │  │   │
│  │   └─────────────────────┬─────────────────────────────────────────┘  │   │
│  │                         │                                            │   │
│  │   ┌─────────────────────┴─────────────────────────────────────────┐  │   │
│  │   │ ViGEmBus Driver → Windows → Game sees Virtual DS4             │  │   │
│  │   └───────────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Threading Model

| Thread | Frequency | Priority | Responsibilities |
|--------|-----------|----------|------------------|
| **HID Input** | 1000Hz (1ms) | Normal | Enumerate, detect, poll DualSense |
| **Bridge** | 1000Hz (1ms) | Normal | Process state, apply aimbot/macros, encode |
| **ViGEm Output** | 60-144Hz | Normal | Sync to game FPS, send to driver |
| **Aimbot** | Variable | High | Calculate aim, inject into bridge |

**Communication:** Lock-free MPSC queues using `std::atomic` + `std::mutex`

---

## 3. Component Specifications

### 3.1 HidInputThread (Detection & Polling)

**Location:** `controller/hid_input_thread.h`, `.cpp`

**Purpose:** Detects DualSense controller via HID, polls input at 1000Hz, pushes state to queue.

**Detection Algorithm:**
1. Enumerate all HID devices every 2 seconds (`hid_enumerate`)
2. Filter by Sony VID (0x054C) and PIDs (0x0CE6, 0x0DF2, 0x0E5F, 0x0F15)
3. Open device with `hid_open_path()`
4. Send "connected" state to queue
5. Start polling loop reading input reports at 1ms intervals
6. On disconnect: close device, send "disconnected" state, wait 750ms, retry

**Key Constants:**
```cpp
static constexpr uint16_t kSonyVid = 0x054C;
static constexpr std::array<uint16_t, 4> kDualSensePids = {
    0x0CE6,  // DualSense USB/Bluetooth
    0x0DF2,  // DualSense Edge
    0x0E5F,  // DualSense revision
    0x0F15   // DualSense alternate
};
static constexpr auto kRetryInterval = std::chrono::seconds(2);
static constexpr auto kReconnectDelay = std::chrono::milliseconds(750);
static constexpr int kPollingRateHz = 1000;
```

**Class Interface:**
```cpp
class HidInputThread {
public:
    void start();
    void stop();
    bool isRunning() const;
    void setOutputQueue(ThreadSafeQueue<ControllerState>* queue);
    
private:
    void run();
    std::optional<DeviceInfo> findDualSenseDevice();
    bool pollDevice(hid_device* device);
    ControllerState parseHidReport(const std::vector<uint8_t>& report);
    
    std::thread thread_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> running_{false};
    ThreadSafeQueue<ControllerState>* output_queue_ = nullptr;
};
```

**Connection States:**
- `Searching`: Looking for DualSense
- `Connected`: Device open and polling
- `Disconnected`: Was connected but lost
- `Error`: Failed to open (permissions, etc.)

---

### 3.2 ControllerState (Shared State Structure)

**Location:** `controller/controller_state.h`

**Purpose:** Normalized representation of controller input, thread-safe for queue communication.

**Structure:**
```cpp
#pragma once
#include <chrono>
#include <string>
#include <optional>
#include <cstdint>

struct ControllerState {
    std::chrono::steady_clock::time_point last_update;
    
    struct Stick {
        float x = 0.0f;  // [-1.0, 1.0]
        float y = 0.0f;  // [-1.0, 1.0]
    };
    Stick left_stick;
    Stick right_stick;
    
    float l2 = 0.0f;  // [0.0, 1.0]
    float r2 = 0.0f;  // [0.0, 1.0]
    
    uint16_t buttons = 0x08;  // DS4 bitmask, 0x08 = dpad neutral
    uint8_t special = 0;      // PS button (0x01), Touchpad (0x02)
    
    bool connected = false;
    std::optional<std::string> error;
    bool aimbot_modified = false;  // true if aimbot changed values
};
```

**Normalization Rules:**
- Left stick: x ∈ [-1.0, 1.0] (left to right), y ∈ [-1.0, 1.0] (up to down)
- Right stick: same as left stick
- Triggers: [0.0, 1.0] (not pressed to fully pressed)
- Center position: 0.0 for sticks, 0.0 for triggers

---

### 3.3 ControllerBridge (Processing)

**Location:** `controller/controller_bridge.h`, `.cpp`

**Purpose:** Main processing logic - reads input state, applies aimbot/macros, encodes to DS4 format, outputs to ViGEm queue.

**Processing Pipeline (per frame):**
1. Read ControllerState from input queue (blocking with timeout)
2. If `connected == false`: output neutral DS4_REPORT
3. If aimbot active: modify right stick based on aim delta
4. If macros enabled: apply script transformations
5. Encode ControllerState to DS4_REPORT binary format
6. Push DS4_REPORT to output queue

**Aimbot Injection:**
```cpp
void injectAimInput(float delta_x, float delta_y) {
    // Convert aim delta to stick position
    // delta_x, delta_y are in normalized screen coordinates
    // Apply to right stick (aim stick)
    aim_delta_x_.store(delta_x);
    aim_delta_y_.store(delta_y);
    aimbot_active_.store(true);
}

void processState(ControllerState& state) {
    if (aimbot_active_.load()) {
        float dx = aim_delta_x_.load();
        float dy = aim_delta_y_.load();
        
        // Add to right stick with smoothing
        state.right_stick.x = std::clamp(state.right_stick.x + dx, -1.0f, 1.0f);
        state.right_stick.y = std::clamp(state.right_stick.y + dy, -1.0f, 1.0f);
        state.aimbot_modified = true;
    }
}
```

**Class Interface:**
```cpp
class ControllerBridge {
public:
    bool initialize();
    void shutdown();
    void start();
    void stop();
    
    // Input/Output queues
    void setInputQueue(ThreadSafeQueue<ControllerState>* input);
    void setOutputQueue(ThreadSafeQueue<DS4_REPORT>* output);
    
    // Aimbot injection
    void injectAimInput(float delta_x, float delta_y);
    void setAimbotActive(bool active);
    
    // Macro system
    void enableMacros(bool enable);
    void loadMacroScript(const std::string& path);
    
private:
    void run();
    void processState(ControllerState& state);
    void applyMacros(ControllerState& state);
    DS4_REPORT encodeToDs4(const ControllerState& state);
    
    ThreadSafeQueue<ControllerState>* input_queue_ = nullptr;
    ThreadSafeQueue<DS4_REPORT>* output_queue_ = nullptr;
    
    std::thread thread_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> running_{false};
    
    std::atomic<bool> aimbot_active_{false};
    std::atomic<float> aim_delta_x_{0.0f};
    std::atomic<float> aim_delta_y_{0.0f};
    
    std::atomic<bool> macros_enabled_{false};
    std::unique_ptr<MacroSystem> macro_system_;
};
```

---

### 3.4 Ds4Gamepad (ViGEm Output)

**Location:** `controller/ds4_gamepad.h`, `.cpp`

**Purpose:** Wraps ViGEmBus C API, manages virtual DualShock 4 controller lifecycle.

**DS4_REPORT Binary Format:**
```cpp
#pragma pack(push, 1)
struct DS4_REPORT {
    uint8_t thumb_lx;      // Left stick X (0x00-0xFF, center=0x80)
    uint8_t thumb_ly;      // Left stick Y
    uint8_t thumb_rx;      // Right stick X
    uint8_t thumb_ry;      // Right stick Y
    uint16_t buttons;      // Button bitmask
    uint8_t special;       // PS + Touchpad
    uint8_t trigger_l;     // L2 analog
    uint8_t trigger_r;     // R2 analog
};
#pragma pack(pop)

static_assert(sizeof(DS4_REPORT) == 9, "DS4_REPORT must be 9 bytes");
```

**ViGEm Initialization Sequence:**
1. `vigem_alloc()` - Allocate client
2. `vigem_connect(client)` - Connect to bus driver
3. `vigem_target_ds4_alloc()` - Allocate DS4 target
4. `vigem_target_add(client, target)` - Add target to client
5. `vigem_target_ds4_update(client, target, report)` - Send report (every frame)

**Cleanup Sequence (reverse order):**
1. `vigem_target_remove(client, target)`
2. `vigem_target_free(target)`
3. `vigem_disconnect(client)`
4. `vigem_free(client)`

**Class Interface:**
```cpp
class Ds4Gamepad {
public:
    Ds4Gamepad();
    ~Ds4Gamepad();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Update virtual controller
    bool update(const DS4_REPORT& report);
    
    // Status
    bool isConnected() const { return connected_; }
    std::string getLastError() const { return last_error_; }
    
private:
    PVIGEM_CLIENT client_ = nullptr;
    PVIGEM_TARGET target_ = nullptr;
    bool connected_ = false;
    std::string last_error_;
};
```

---

### 3.5 Button Encoding

**Location:** `controller/ds4_button_encoding.h`, `.cpp`

**DS4 Button Bitmask Layout:**
```cpp
namespace Ds4Buttons {
    // D-Pad (bits 0-3)
    constexpr uint16_t DPAD_NEUTRAL = 0x08;
    constexpr uint16_t DPAD_NORTH = 0x00;
    constexpr uint16_t DPAD_NORTHEAST = 0x01;
    constexpr uint16_t DPAD_EAST = 0x02;
    constexpr uint16_t DPAD_SOUTHEAST = 0x03;
    constexpr uint16_t DPAD_SOUTH = 0x04;
    constexpr uint16_t DPAD_SOUTHWEST = 0x05;
    constexpr uint16_t DPAD_WEST = 0x06;
    constexpr uint16_t DPAD_NORTHWEST = 0x07;
    
    // Action buttons
    constexpr uint16_t SQUARE = (1 << 4);
    constexpr uint16_t CROSS = (1 << 5);
    constexpr uint16_t CIRCLE = (1 << 6);
    constexpr uint16_t TRIANGLE = (1 << 7);
    
    // Shoulder buttons
    constexpr uint16_t L1 = (1 << 8);
    constexpr uint16_t R1 = (1 << 9);
    constexpr uint16_t L2_DIGITAL = (1 << 10);
    constexpr uint16_t R2_DIGITAL = (1 << 11);
    
    // System buttons
    constexpr uint16_t SHARE = (1 << 12);
    constexpr uint16_t OPTIONS = (1 << 13);
    constexpr uint16_t L3 = (1 << 14);  // Left stick press
    constexpr uint16_t R3 = (1 << 15);  // Right stick press
}

namespace Ds4Special {
    constexpr uint8_t PS_BUTTON = 0x01;
    constexpr uint8_t TOUCHPAD = 0x02;
}
```

**Encoding Function:**
```cpp
std::pair<uint16_t, uint8_t> encodeButtons(const ControllerState& state);
```

---

### 3.6 Axis Conversion

**Location:** `controller/axis_conversion.h`

**Conversion Functions:**
```cpp
// Convert normalized stick value [-1.0, 1.0] to DS4 byte [0, 255]
// Center is 0x80 (128)
inline uint8_t stick_to_u8(float value, bool invert = false) {
    float normalized = std::clamp(value, -1.0f, 1.0f);
    if (invert) normalized = -normalized;
    return static_cast<uint8_t>(
        std::clamp(
            static_cast<int>(std::round((normalized + 1.0f) * 0.5f * 255.0f)),
            0, 255
        )
    );
}

// Convert normalized trigger [0.0, 1.0] to DS4 byte [0, 255]
inline uint8_t trigger_to_u8(float value) {
    return static_cast<uint8_t>(
        std::clamp(
            static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f)),
            0, 255
        )
    );
}

// Convert DS4 byte back to normalized float
inline float u8_to_stick(uint8_t value, bool invert = false) {
    float normalized = (static_cast<float>(value) / 255.0f) * 2.0f - 1.0f;
    return invert ? -normalized : normalized;
}

inline float u8_to_trigger(uint8_t value) {
    return static_cast<float>(value) / 255.0f;
}
```

---

### 3.7 Thread-Safe Queue

**Location:** `controller/thread_safe_queue.h`

**Purpose:** Lock-free MPSC queue for inter-thread communication.

**Interface:**
```cpp
template<typename T>
class ThreadSafeQueue {
public:
    void push(T value);
    bool try_pop(T& value);
    bool pop_timeout(T& value, std::chrono::milliseconds timeout);
    bool empty() const;
    size_t size() const;
    void clear();
    
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
```

---

### 3.8 ControllerManager (Top-Level Manager)

**Location:** `controller/controller_manager.h`, `.cpp`

**Purpose:** Manages all controller components, provides unified interface to main application.

**Class Interface:**
```cpp
class ControllerManager {
public:
    static ControllerManager& instance();
    
    // Lifecycle
    bool initialize(const ControllerConfig& config);
    void shutdown();
    void start();
    void stop();
    
    // Status
    bool isConnected() const;
    ControllerState getCurrentState() const;
    ControllerError getLastError() const;
    
    // Aimbot integration
    void injectAimInput(float delta_x, float delta_y);
    void setAimbotActive(bool active);
    
    // Configuration
    void updateConfig(const ControllerConfig& config);
    
private:
    ControllerManager() = default;
    ~ControllerManager() = default;
    
    std::unique_ptr<HidInputThread> hid_thread_;
    std::unique_ptr<ControllerBridge> bridge_;
    std::unique_ptr<Ds4Gamepad> gamepad_;
    
    ThreadSafeQueue<ControllerState> hid_to_bridge_queue_;
    ThreadSafeQueue<DS4_REPORT> bridge_to_vigem_queue_;
    
    ControllerConfig config_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
};
```

---

## 4. Error Handling

### 4.1 Error Types

```cpp
enum class ControllerError {
    None,                           // No error
    ViGEmDriverNotInstalled,        // User needs to install ViGEmBus
    ViGEmClientDllNotFound,         // DLL missing from PATH
    HidDeviceNotFound,              // No DualSense connected
    HidDeviceDisconnected,          // Cable unplugged/BT lost
    HidOpenFailed,                  // Permission issue
    BridgeThreadCrashed,            // Internal error
    InvalidButtonMapping,           // Config error
    MacroScriptNotFound,            // Script file missing
    MacroScriptParseError           // Invalid script syntax
};
```

### 4.2 Error Propagation

Errors are propagated via:
1. **Console log**: Immediate error message with context
2. **UI overlay**: Visual indicator showing error state
3. **Config persistence**: Error stored for debugging
4. **Auto-retry**: Some errors trigger automatic retry (e.g., HidDeviceNotFound)

### 4.3 Recovery Strategies

| Error | Strategy | Retry Delay |
|-------|----------|-------------|
| ViGEmDriverNotInstalled | Log error, disable controller | None (manual fix) |
| HidDeviceNotFound | Continue polling | 2 seconds |
| HidDeviceDisconnected | Close, wait, re-enumerate | 750ms |
| HidOpenFailed | Log error, continue polling | 500ms |
| BridgeThreadCrashed | Restart bridge thread | Immediate |

---

## 5. Dependencies

### 5.1 Required Libraries

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| **hidapi** | 0.14.0+ | HID device access | BSD-3-Clause |
| **ViGEmClient** | 1.21.0+ | Virtual controller | BSD-3-Clause |

### 5.2 Installation Requirements

**ViGEmBus Driver (System-wide, install once):**
1. Download from: https://github.com/nefarius/ViGEmBus/releases
2. Run `ViGEmBusSetup.exe` as administrator
3. Reboot if prompted

**Files to Distribute:**
- `hidapi.dll` (runtime)
- `ViGEmClient.dll` (runtime, must be on PATH or next to executable)

### 5.3 CMake Integration

Add to `CMakeLists.txt`:
```cmake
# Find ViGEmClient
find_library(VIGEM_CLIENT_LIB ViGEmClient.lib
    PATHS "${CMAKE_SOURCE_DIR}/third_party/vigem/lib"
)
find_path(VIGEM_CLIENT_INCLUDE ViGEm/Client.h
    PATHS "${CMAKE_SOURCE_DIR}/third_party/vigem/include"
)

# Find hidapi
find_library(HIDAPI_LIB hidapi.lib
    PATHS "${CMAKE_SOURCE_DIR}/third_party/hidapi/lib"
)
find_path(HIDAPI_INCLUDE hidapi.h
    PATHS "${CMAKE_SOURCE_DIR}/third_party/hidapi/include"
)

# Link to ai target
target_link_libraries(ai PRIVATE
    ${VIGEM_CLIENT_LIB}
    ${HIDAPI_LIB}
)

# Include directories
target_include_directories(ai PRIVATE
    ${VIGEM_CLIENT_INCLUDE}
    ${HIDAPI_INCLUDE}
)

# Copy DLLs to output
add_custom_command(TARGET ai POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/third_party/vigem/bin/ViGEmClient.dll"
    $<TARGET_FILE_DIR:ai>
)
add_custom_command(TARGET ai POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/third_party/hidapi/bin/hidapi.dll"
    $<TARGET_FILE_DIR:ai>
)
```

---

## 6. Configuration

### 6.1 Config Structure

Add to `sunone_aimbot_2/config/config.h`:
```cpp
struct ControllerConfig {
    bool enabled = false;
    bool auto_connect = true;           // Auto-detect on startup
    int polling_rate_hz = 1000;         // HID poll frequency
    bool enable_aimbot_injection = true; // Allow aimbot to modify input
    bool enable_macros = false;          // Enable macro/script layer
    std::string macro_script_path = "";  // Path to macro script
    bool show_overlay = true;           // Show controller status in UI
    bool invert_left_stick_y = false;   // Y-axis inversion
    bool invert_right_stick_y = false;  // Y-axis inversion
    float aimbot_sensitivity = 1.0f;    // Aimbot input scaling
};

// Add to main Config struct
struct Config {
    // ... existing config ...
    ControllerConfig controller;
};
```

### 6.2 Config File Format

```json
{
    "controller": {
        "enabled": true,
        "auto_connect": true,
        "polling_rate_hz": 1000,
        "enable_aimbot_injection": true,
        "enable_macros": false,
        "macro_script_path": "",
        "show_overlay": true,
        "invert_left_stick_y": false,
        "invert_right_stick_y": false,
        "aimbot_sensitivity": 1.0
    }
}
```

---

## 7. UI Integration

### 7.1 Overlay Panel

Add to overlay drawing code:
```cpp
void DrawControllerPanel() {
    if (!config.controller.enabled) return;
    
    ImGui::Begin("Controller");
    
    auto& mgr = ControllerManager::instance();
    
    if (mgr.isConnected()) {
        auto state = mgr.getCurrentState();
        
        // Status indicator
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "🎮 DualSense Connected");
        
        // Analog sticks (visual bars)
        ImGui::Text("Left Stick:");
        ImGui::ProgressBar((state.left_stick.x + 1.0f) * 0.5f, ImVec2(100, 0), "X");
        ImGui::SameLine();
        ImGui::ProgressBar((state.left_stick.y + 1.0f) * 0.5f, ImVec2(100, 0), "Y");
        
        ImGui::Text("Right Stick:");
        ImGui::ProgressBar((state.right_stick.x + 1.0f) * 0.5f, ImVec2(100, 0), "X");
        ImGui::SameLine();
        ImGui::ProgressBar((state.right_stick.y + 1.0f) * 0.5f, ImVec2(100, 0), "Y");
        
        // Triggers
        ImGui::Text("Triggers: L2=%.0f%% R2=%.0f%%", 
            state.l2 * 100.0f, state.r2 * 100.0f);
        
        // Active buttons
        if (state.buttons != 0x08) {
            ImGui::Text("Buttons: Active");
        }
        
        // Aimbot indicator
        if (state.aimbot_modified) {
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "⚡ Aimbot Active");
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "🎮 Controller Disconnected");
        ImGui::Text("Connect DualSense via USB or Bluetooth");
        
        auto error = mgr.getLastError();
        if (error != ControllerError::None) {
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), 
                "Error: %s", controllerErrorToString(error));
        }
    }
    
    ImGui::End();
}
```

### 7.2 Settings Panel

Add to settings overlay:
```cpp
void DrawControllerSettings() {
    ImGui::Checkbox("Enable Controller", &config.controller.enabled);
    
    if (config.controller.enabled) {
        ImGui::Checkbox("Auto-Connect", &config.controller.auto_connect);
        ImGui::SliderInt("Polling Rate (Hz)", 
            &config.controller.polling_rate_hz, 250, 1000);
        ImGui::Checkbox("Aimbot Injection", 
            &config.controller.enable_aimbot_injection);
        ImGui::Checkbox("Show Overlay", &config.controller.show_overlay);
        
        if (ImGui::Button("Test Virtual Controller")) {
            ControllerManager::instance().runTestPattern();
        }
    }
}
```

---

## 8. Testing Strategy

### 8.1 Unit Tests

**File:** `tests/controller_unit_tests.cpp`

```cpp
// Test button encoding
TEST(ButtonEncoding, CrossButton) {
    ControllerState state;
    state.buttons = Ds4Buttons::CROSS;
    auto [buttons, special] = encodeButtons(state);
    EXPECT_EQ(buttons & Ds4Buttons::CROSS, Ds4Buttons::CROSS);
}

// Test axis conversion
TEST(AxisConversion, CenterPosition) {
    uint8_t center = stick_to_u8(0.0f);
    EXPECT_EQ(center, 0x80);
}

TEST(AxisConversion, FullRange) {
    EXPECT_EQ(stick_to_u8(-1.0f), 0x00);
    EXPECT_EQ(stick_to_u8(1.0f), 0xFF);
}

// Test DS4_REPORT packing
TEST(Ds4Report, SizeCheck) {
    EXPECT_EQ(sizeof(DS4_REPORT), 9);
}
```

### 8.2 Integration Tests

```cpp
// Test ViGEmBus connection
TEST(ViGEmIntegration, CanConnect) {
    Ds4Gamepad gamepad;
    EXPECT_TRUE(gamepad.initialize());
    gamepad.shutdown();
}

// Test HID enumeration
TEST(HidIntegration, CanEnumerate) {
    auto devices = hid_enumerate(0, 0);
    EXPECT_NE(devices, nullptr);
    hid_free_enumeration(devices);
}
```

### 8.3 Smoke Tests

1. **Virtual Controller Visibility:**
   - Run `joy.cpl` (Windows Game Controllers)
   - Verify virtual DS4 appears as "DualShock 4 Controller"

2. **Online Tester:**
   - Visit https://gamepad-tester.com/
   - Verify virtual controller detected
   - Test all buttons and axes

### 8.4 Functional Tests

1. **Input Passthrough:**
   - Connect DualSense
   - Open test game
   - Verify physical input reaches game via virtual controller

2. **Aimbot Injection:**
   - Enable aimbot
   - Verify right stick moves when aimbot calculates delta
   - Verify smooth transition between manual and aimbot control

3. **Reconnection:**
   - Unplug DualSense
   - Verify "Disconnected" state in UI
   - Replug DualSense
   - Verify automatic reconnection

---

## 9. File Structure

```
sunone_aimbot_2/
├── controller/
│   ├── controller_state.h              # State struct
│   ├── controller_state.cpp
│   ├── ds4_gamepad.h                   # ViGEm wrapper
│   ├── ds4_gamepad.cpp
│   ├── ds4_button_encoding.h           # Button constants
│   ├── ds4_button_encoding.cpp
│   ├── axis_conversion.h               # Axis helpers
│   ├── controller_bridge.h             # Main bridge
│   ├── controller_bridge.cpp
│   ├── hid_input_thread.h              # HID detection/polling
│   ├── hid_input_thread.cpp
│   ├── macro_system.h                  # Optional macros
│   ├── macro_system.cpp
│   ├── controller_manager.h            # Top-level manager
│   ├── controller_manager.cpp
│   └── thread_safe_queue.h             # Lock-free queue
│
├── config/
│   ├── config.h                        # Modified to add ControllerConfig
│   └── config.cpp                      # Modified to parse controller config
│
├── overlay/
│   ├── draw_controller.cpp             # New: Controller overlay
│   └── draw_settings.cpp               # Modified: Add controller settings
│
├── sunone_aimbot_2.cpp                 # Modified: Initialize controller
├── sunone_aimbot_2.h                   # Modified: Add controller includes
│
├── mouse/
│   └── mouse.cpp                       # Modified: Inject aim into controller
│
├── CMakeLists.txt                      # Modified: Add ViGEm + hidapi
│
├── third_party/                        # New: Dependencies
│   ├── vigem/
│   │   ├── include/ViGEm/
│   │   ├── lib/ViGEmClient.lib
│   │   └── bin/ViGEmClient.dll
│   └── hidapi/
│       ├── include/hidapi.h
│       ├── lib/hidapi.lib
│       └── bin/hidapi.dll
│
├── tests/
│   └── controller_unit_tests.cpp       # New: Unit tests
│
└── docs/
    └── superpowers/
        └── specs/
            └── 2025-03-18-vigembus-controller-design.md  # This file
```

---

## 10. Performance Requirements

### 10.1 Latency Budget

| Component | Target Latency | Maximum |
|-----------|---------------|---------|
| HID Poll | 1ms | 2ms |
| Bridge Processing | 0.5ms | 1ms |
| ViGEm Output | 1ms | 2ms |
| **Total** | **2.5ms** | **5ms** |

### 10.2 CPU Usage

- HID Thread: < 1% CPU
- Bridge Thread: < 1% CPU  
- ViGEm Thread: < 0.5% CPU
- **Total**: < 2.5% CPU on modern processors

### 10.3 Memory Usage

- ControllerState: ~64 bytes
- Thread queues: < 1MB each
- ViGEm client: ~100KB
- **Total**: < 5MB additional memory

---

## 11. Security Considerations

### 11.1 HID Access
- Requires no special privileges (user-mode HID access)
- Uses standard Windows HID APIs via hidapi

### 11.2 ViGEmBus Driver
- ViGEmBus is a signed kernel driver
- Requires admin privileges to install (one-time)
- Runtime access requires no elevation

### 11.3 Anti-Cheat Compatibility
- ViGEmBus is widely used and generally compatible
- Virtual controller appears as genuine hardware to games
- Some competitive anti-cheats may flag - test before use

---

## 12. Future Extensions

### 12.1 Planned Features
- [ ] Macro/script system (Python/Lua integration)
- [ ] Xbox 360 controller output mode
- [ ] Multiple controller support
- [ ] Haptic feedback passthrough
- [ ] Touchpad emulation
- [ ] Motion sensor (gyro) support

### 12.2 Potential Enhancements
- Adaptive stick sensitivity curves
- Button remapping profiles
- Combo recording/playback
- Integration with training mode

---

## 13. Approval

**Design Status:** ✅ Approved  
**Date:** 2025-03-18  
**Approved By:** User  

**Next Step:** Write implementation plan using writing-plans skill

---

## 14. References

1. **ViGEmBus Documentation:** https://docs.nefarius.at/projects/ViGEm/
2. **ViGEmClient GitHub:** https://github.com/nefarius/ViGEmClient
3. **hidapi Documentation:** https://github.com/libusb/hidapi
4. **DS4 Report Format:** https://www.psdevwiki.com/ps4/DS4-USB
5. **VirtualGamepad (Reference Implementation):** I:\RustProjects\VirtualGamepad
