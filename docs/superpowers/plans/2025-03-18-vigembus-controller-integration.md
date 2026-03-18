# ViGEmBus Controller Integration Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate ViGEmBus virtual controller emulation into sunone_aimbot_2, enabling DualSense input to be processed, optionally modified by aimbot, and output as virtual DualShock 4.

**Architecture:** Bridge Pattern with 3 threads - HID Input Thread (detection/polling), Bridge Thread (processing/aimbot injection), ViGEm Output Thread (virtual controller). Thread-safe communication via bounded MPSC queues.

**Tech Stack:** C++17, hidapi (HID access), ViGEmClient (virtual controller), Google Test (unit tests), CMake (build)

**Spec Reference:** `docs/superpowers/specs/2025-03-18-vigembus-controller-design.md`

---

## File Structure

### New Files to Create

| File | Purpose | Lines Est. |
|------|---------|------------|
| `controller/controller_state.h` | State struct definition | 50 |
| `controller/thread_safe_queue.h` | Thread-safe MPSC queue | 100 |
| `controller/ds4_button_encoding.h` | Button bitmask constants | 80 |
| `controller/axis_conversion.h` | Axis conversion utilities | 60 |
| `controller/ds4_gamepad.h` | ViGEmBus wrapper header | 60 |
| `controller/ds4_gamepad.cpp` | ViGEmBus implementation | 150 |
| `controller/hid_input_thread.h` | HID thread header | 60 |
| `controller/hid_input_thread.cpp` | HID detection/polling | 300 |
| `controller/controller_bridge.h` | Bridge header | 80 |
| `controller/controller_bridge.cpp` | Bridge implementation | 250 |
| `controller/controller_manager.h` | Manager header | 80 |
| `controller/controller_manager.cpp` | Manager implementation | 200 |
| `overlay/draw_controller.cpp` | UI overlay | 150 |
| `tests/controller_unit_tests.cpp` | Unit tests | 200 |
| **Total** | | **~1820 lines** |

### Files to Modify

| File | Changes |
|------|---------|
| `config/config.h` | Add ControllerConfig struct |
| `config/config.cpp` | Parse controller config JSON |
| `sunone_aimbot_2.cpp` | Initialize controller, integrate with aimbot |
| `sunone_aimbot_2.h` | Include controller headers |
| `mouse/mouse.cpp` | Inject aim into controller bridge |
| `CMakeLists.txt` | Add controller sources, link hidapi/ViGEm |

---

## Chunk 1: Core Data Structures

### Task 1: ControllerState Structure

**Files:**
- Create: `sunone_aimbot_2/controller/controller_state.h`

- [ ] **Step 1: Create controller directory**

```bash
mkdir -p sunone_aimbot_2/controller
```

- [ ] **Step 2: Write controller_state.h**

```cpp
// sunone_aimbot_2/controller/controller_state.h
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
    bool aimbot_modified = false;
};
```

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/controller/controller_state.h
git commit -m "feat(controller): add ControllerState structure"
```

---

### Task 2: Thread-Safe Queue

**Files:**
- Create: `sunone_aimbot_2/controller/thread_safe_queue.h`

- [ ] **Step 1: Write thread_safe_queue.h**

```cpp
// sunone_aimbot_2/controller/thread_safe_queue.h
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t max_capacity = 256)
        : max_capacity_(max_capacity) {}
    
    bool push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.size() >= max_capacity_) {
            queue_.pop();  // Drop oldest (LRU)
        }
        
        queue_.push(std::move(value));
        cv_.notify_one();
        return true;
    }
    
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    bool pop_timeout(T& value, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }
    
    size_t capacity() const { return max_capacity_; }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t max_capacity_;
};
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/thread_safe_queue.h
git commit -m "feat(controller): add ThreadSafeQueue with LRU overflow"
```

---

### Task 3: DS4 Button Encoding

**Files:**
- Create: `sunone_aimbot_2/controller/ds4_button_encoding.h`

- [ ] **Step 1: Write ds4_button_encoding.h**

```cpp
// sunone_aimbot_2/controller/ds4_button_encoding.h
#pragma once

#include <cstdint>

namespace Ds4Buttons {
    // D-Pad (bits 0-3, 0x8 = neutral)
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

// Encode dpad direction to DS4 format
inline uint16_t encodeDpad(bool up, bool down, bool left, bool right) {
    if (up && right) return Ds4Buttons::DPAD_NORTHEAST;
    if (up && left) return Ds4Buttons::DPAD_NORTHWEST;
    if (down && right) return Ds4Buttons::DPAD_SOUTHEAST;
    if (down && left) return Ds4Buttons::DPAD_SOUTHWEST;
    if (up) return Ds4Buttons::DPAD_NORTH;
    if (down) return Ds4Buttons::DPAD_SOUTH;
    if (right) return Ds4Buttons::DPAD_EAST;
    if (left) return Ds4Buttons::DPAD_WEST;
    return Ds4Buttons::DPAD_NEUTRAL;
}
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/ds4_button_encoding.h
git commit -m "feat(controller): add DS4 button encoding constants"
```

---

### Task 4: Axis Conversion Utilities

**Files:**
- Create: `sunone_aimbot_2/controller/axis_conversion.h`

- [ ] **Step 1: Write axis_conversion.h**

```cpp
// sunone_aimbot_2/controller/axis_conversion.h
#pragma once

#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdint>

// Convert normalized stick value [-1.0, 1.0] to DS4 byte [0, 255]
// Center is 0x80 (128)
inline uint8_t stick_to_u8(float value, bool invert = false) {
    // Handle NaN/Inf
    if (!std::isfinite(value)) {
        return 0x80;  // Return center on invalid input
    }
    
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
    if (!std::isfinite(value)) {
        return 0;
    }
    
    return static_cast<uint8_t>(
        std::clamp(
            static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f)),
            0, 255
        )
    );
}

// Convert DS4 byte back to normalized float [-1.0, 1.0]
inline float u8_to_stick(uint8_t value, bool invert = false) {
    float normalized = (static_cast<float>(value) / 255.0f) * 2.0f - 1.0f;
    return invert ? -normalized : normalized;
}

// Convert DS4 byte back to normalized trigger [0.0, 1.0]
inline float u8_to_trigger(uint8_t value) {
    return static_cast<float>(value) / 255.0f;
}
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/axis_conversion.h
git commit -m "feat(controller): add axis conversion utilities with NaN/Inf handling"
```

---

### Task 5: DS4_REPORT Structure

**Files:**
- Create: `sunone_aimbot_2/controller/ds4_gamepad.h` (partial, will extend later)

- [ ] **Step 1: Write DS4_REPORT structure**

```cpp
// sunone_aimbot_2/controller/ds4_gamepad.h
#pragma once

#include <cstdint>
#include <string>
#include "../../controller/controller_state.h"
#include "../../controller/axis_conversion.h"
#include "../../controller/ds4_button_encoding.h"

// Forward declarations for ViGEm types (will be defined when including ViGEm headers)
typedef void* PVIGEM_CLIENT;
typedef void* PVIGEM_TARGET;

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
    
    DS4_REPORT() : thumb_lx(0x80), thumb_ly(0x80), 
                   thumb_rx(0x80), thumb_ry(0x80),
                   buttons(0x08), special(0),
                   trigger_l(0), trigger_r(0) {}
    
    void fromControllerState(const ControllerState& state) {
        thumb_lx = stick_to_u8(state.left_stick.x);
        thumb_ly = stick_to_u8(state.left_stick.y);
        thumb_rx = stick_to_u8(state.right_stick.x);
        thumb_ry = stick_to_u8(state.right_stick.y);
        trigger_l = trigger_to_u8(state.l2);
        trigger_r = trigger_to_u8(state.r2);
        buttons = state.buttons;
        special = state.special;
    }
};
#pragma pack(pop)

static_assert(sizeof(DS4_REPORT) == 9, "DS4_REPORT must be 9 bytes (packed)");

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
    PVIGEM_CLIENT client_;
    PVIGEM_TARGET target_;
    bool connected_;
    std::string last_error_;
};
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/ds4_gamepad.h
git commit -m "feat(controller): add DS4_REPORT structure and Ds4Gamepad header"
```

---

## Chunk 2: ViGEmBus Integration

### Task 6: Ds4Gamepad Implementation

**Files:**
- Create: `sunone_aimbot_2/controller/ds4_gamepad.cpp`

- [ ] **Step 1: Create ds4_gamepad.cpp with ViGEm implementation**

```cpp
// sunone_aimbot_2/controller/ds4_gamepad.cpp
#include "ds4_gamepad.h"
#include <iostream>
#include <windows.h>

// ViGEm function pointer types
typedef void* (*VigemAllocFunc)();
typedef void (*VigemFreeFunc)(void*);
typedef uint32_t (*VigemConnectFunc)(void*);
typedef void (*VigemDisconnectFunc)(void*);
typedef void* (*VigemTargetDs4AllocFunc)();
typedef void (*VigemTargetFreeFunc)(void*);
typedef uint32_t (*VigemTargetAddFunc)(void*, void*);
typedef uint32_t (*VigemTargetRemoveFunc)(void*, void*);
typedef uint32_t (*VigemTargetDs4UpdateFunc)(void*, void*, DS4_REPORT);

struct VigemFunctions {
    HMODULE module = nullptr;
    VigemAllocFunc alloc = nullptr;
    VigemFreeFunc free = nullptr;
    VigemConnectFunc connect = nullptr;
    VigemDisconnectFunc disconnect = nullptr;
    VigemTargetDs4AllocFunc targetDs4Alloc = nullptr;
    VigemTargetFreeFunc targetFree = nullptr;
    VigemTargetAddFunc targetAdd = nullptr;
    VigemTargetRemoveFunc targetRemove = nullptr;
    VigemTargetDs4UpdateFunc targetDs4Update = nullptr;
};

static bool loadVigemFunctions(VigemFunctions& fns) {
    fns.module = LoadLibraryA("ViGEmClient.dll");
    if (!fns.module) {
        return false;
    }
    
    fns.alloc = (VigemAllocFunc)GetProcAddress(fns.module, "vigem_alloc");
    fns.free = (VigemFreeFunc)GetProcAddress(fns.module, "vigem_free");
    fns.connect = (VigemConnectFunc)GetProcAddress(fns.module, "vigem_connect");
    fns.disconnect = (VigemDisconnectFunc)GetProcAddress(fns.module, "vigem_disconnect");
    fns.targetDs4Alloc = (VigemTargetDs4AllocFunc)GetProcAddress(fns.module, "vigem_target_ds4_alloc");
    fns.targetFree = (VigemTargetFreeFunc)GetProcAddress(fns.module, "vigem_target_free");
    fns.targetAdd = (VigemTargetAddFunc)GetProcAddress(fns.module, "vigem_target_add");
    fns.targetRemove = (VigemTargetRemoveFunc)GetProcAddress(fns.module, "vigem_target_remove");
    fns.targetDs4Update = (VigemTargetDs4UpdateFunc)GetProcAddress(fns.module, "vigem_target_ds4_update");
    
    return fns.alloc && fns.free && fns.connect && fns.disconnect &&
           fns.targetDs4Alloc && fns.targetFree && fns.targetAdd &&
           fns.targetRemove && fns.targetDs4Update;
}

static VigemFunctions g_vigem;

Ds4Gamepad::Ds4Gamepad()
    : client_(nullptr), target_(nullptr), connected_(false) {}

Ds4Gamepad::~Ds4Gamepad() {
    shutdown();
}

bool Ds4Gamepad::initialize() {
    if (connected_) {
        return true;
    }
    
    // Load ViGEmClient.dll
    if (!g_vigem.module) {
        if (!loadVigemFunctions(g_vigem)) {
            last_error_ = "ViGEmClient.dll not found or missing functions";
            std::cerr << "[Controller] " << last_error_ << std::endl;
            return false;
        }
    }
    
    // Allocate client
    client_ = g_vigem.alloc();
    if (!client_) {
        last_error_ = "Failed to allocate ViGEm client";
        return false;
    }
    
    // Connect to bus driver
    uint32_t err = g_vigem.connect(client_);
    if (err != 0x20000000) {  // VIGEM_ERROR_NONE
        last_error_ = "ViGEmBus driver not installed or not running";
        g_vigem.free(client_);
        client_ = nullptr;
        std::cerr << "[Controller] " << last_error_ << std::endl;
        return false;
    }
    
    // Allocate DS4 target
    target_ = g_vigem.targetDs4Alloc();
    if (!target_) {
        last_error_ = "Failed to allocate DS4 target";
        g_vigem.disconnect(client_);
        g_vigem.free(client_);
        client_ = nullptr;
        return false;
    }
    
    // Add target to client
    err = g_vigem.targetAdd(client_, target_);
    if (err != 0x20000000) {
        last_error_ = "Failed to add DS4 target";
        g_vigem.targetFree(target_);
        g_vigem.disconnect(client_);
        g_vigem.free(client_);
        target_ = nullptr;
        client_ = nullptr;
        return false;
    }
    
    connected_ = true;
    last_error_.clear();
    std::cout << "[Controller] Virtual DS4 controller initialized" << std::endl;
    return true;
}

void Ds4Gamepad::shutdown() {
    if (!connected_) {
        return;
    }
    
    if (target_ && client_) {
        g_vigem.targetRemove(client_, target_);
    }
    if (target_) {
        g_vigem.targetFree(target_);
    }
    if (client_) {
        g_vigem.disconnect(client_);
        g_vigem.free(client_);
    }
    
    target_ = nullptr;
    client_ = nullptr;
    connected_ = false;
    std::cout << "[Controller] Virtual DS4 controller shutdown" << std::endl;
}

bool Ds4Gamepad::update(const DS4_REPORT& report) {
    if (!connected_ || !client_ || !target_) {
        return false;
    }
    
    uint32_t err = g_vigem.targetDs4Update(client_, target_, report);
    return err == 0x20000000;
}
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/ds4_gamepad.cpp
git commit -m "feat(controller): implement Ds4Gamepad with ViGEmBus integration"
```

---

## Chunk 3: HID Input Thread

### Task 7: HID Input Thread Header

**Files:**
- Create: `sunone_aimbot_2/controller/hid_input_thread.h`

- [ ] **Step 1: Write hid_input_thread.h**

```cpp
// sunone_aimbot_2/controller/hid_input_thread.h
#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <optional>
#include <vector>
#include <cstdint>
#include "controller_state.h"
#include "thread_safe_queue.h"

struct DeviceInfo {
    std::string path;
    std::string serial_number;
    std::string product_string;
};

class HidInputThread {
public:
    HidInputThread();
    ~HidInputThread();
    
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    void setOutputQueue(ThreadSafeQueue<ControllerState>* queue) { output_queue_ = queue; }
    
private:
    void run();
    std::optional<DeviceInfo> findDualSenseDevice();
    bool pollDevice(void* device);  // hid_device* as void* to avoid header dependency
    std::optional<ControllerState> parseHidReport(const std::vector<uint8_t>& report);
    
    std::thread thread_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> running_{false};
    ThreadSafeQueue<ControllerState>* output_queue_ = nullptr;
    
    // Sony VID and DualSense PIDs
    static constexpr uint16_t kSonyVid = 0x054C;
    static constexpr uint16_t kDualSensePids[4] = {
        0x0CE6,  // DualSense USB/Bluetooth
        0x0DF2,  // DualSense Edge
        0x0E5F,  // DualSense revision
        0x0F15   // DualSense alternate
    };
    static constexpr auto kRetryInterval = std::chrono::seconds(2);
    static constexpr auto kReconnectDelay = std::chrono::milliseconds(750);
};
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/hid_input_thread.h
git commit -m "feat(controller): add HidInputThread header"
```

---

### Task 8: HID Input Thread Implementation

**Files:**
- Create: `sunone_aimbot_2/controller/hid_input_thread.cpp`

- [ ] **Step 1: Write hid_input_thread.cpp (Part 1 - Detection)**

```cpp
// sunone_aimbot_2/controller/hid_input_thread.cpp
#include "hid_input_thread.h"
#include <iostream>
#include <chrono>
#include <cstring>

// hidapi header
#include <hidapi.h>

HidInputThread::HidInputThread() = default;

HidInputThread::~HidInputThread() {
    stop();
}

void HidInputThread::start() {
    if (running_.load()) {
        return;
    }
    
    stop_flag_.store(false);
    thread_ = std::thread(&HidInputThread::run, this);
}

void HidInputThread::stop() {
    stop_flag_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void HidInputThread::run() {
    running_.store(true);
    std::cout << "[Controller] HID input thread started" << std::endl;
    
    while (!stop_flag_.load()) {
        // Step 1: Find DualSense device
        auto device_info = findDualSenseDevice();
        
        if (!device_info) {
            // Not found - wait and retry
            if (output_queue_) {
                ControllerState state;
                state.connected = false;
                state.error = "DualSense not found";
                output_queue_->push(state);
            }
            
            for (int i = 0; i < 20 && !stop_flag_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        std::cout << "[Controller] Found DualSense at " << device_info->path << std::endl;
        
        // Step 2: Open device
        hid_device* device = hid_open_path(device_info->path.c_str());
        if (!device) {
            std::cerr << "[Controller] Failed to open device" << std::endl;
            std::this_thread::sleep_for(kReconnectDelay);
            continue;
        }
        
        // Step 3: Send connected state
        if (output_queue_) {
            ControllerState state;
            state.connected = true;
            state.error.reset();
            output_queue_->push(state);
        }
        
        std::cout << "[Controller] DualSense connected" << std::endl;
        
        // Step 4: Poll loop
        while (!stop_flag_.load() && pollDevice(device)) {
            // Continue polling at 1ms intervals
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Step 5: Disconnected - cleanup
        hid_close(device);
        
        if (output_queue_) {
            ControllerState state;
            state.connected = false;
            state.error = "Controller disconnected";
            output_queue_->push(state);
        }
        
        std::cout << "[Controller] DualSense disconnected" << std::endl;
        std::this_thread::sleep_for(kReconnectDelay);
    }
    
    running_.store(false);
    std::cout << "[Controller] HID input thread stopped" << std::endl;
}
```

- [ ] **Step 2: Write hid_input_thread.cpp (Part 2 - Device Discovery)**

```cpp
// Add to hid_input_thread.cpp

std::optional<DeviceInfo> HidInputThread::findDualSenseDevice() {
    // Enumerate all HID devices
    struct hid_device_info* devs = hid_enumerate(kSonyVid, 0);
    if (!devs) {
        return std::nullopt;
    }
    
    struct hid_device_info* cur_dev = devs;
    std::optional<DeviceInfo> result;
    
    while (cur_dev) {
        // Check if PID matches any DualSense variant
        for (uint16_t pid : kDualSensePids) {
            if (cur_dev->product_id == pid) {
                DeviceInfo info;
                info.path = cur_dev->path ? cur_dev->path : "";
                info.serial_number = cur_dev->serial_number ? 
                    reinterpret_cast<const char*>(cur_dev->serial_number) : "unknown";
                info.product_string = cur_dev->product_string ? 
                    reinterpret_cast<const char*>(cur_dev->product_string) : "DualSense";
                
                result = info;
                break;
            }
        }
        
        if (result) break;
        cur_dev = cur_dev->next;
    }
    
    hid_free_enumeration(devs);
    return result;
}
```

- [ ] **Step 3: Write hid_input_thread.cpp (Part 3 - Polling and Parsing)**

```cpp
// Add to hid_input_thread.cpp

bool HidInputThread::pollDevice(void* device_ptr) {
    hid_device* device = static_cast<hid_device*>(device_ptr);
    
    // Read HID report
    std::vector<uint8_t> report(64);
    int bytes_read = hid_read(device, report.data(), report.size());
    
    if (bytes_read < 0) {
        // Fatal HID error - device disconnected
        std::cerr << "[Controller] HID read error: " << bytes_read << std::endl;
        return false;
    }
    
    if (bytes_read == 0) {
        // Timeout - no data available
        return true;
    }
    
    // Resize to actual bytes read
    report.resize(bytes_read);
    
    // Parse report
    auto state = parseHidReport(report);
    if (state && output_queue_) {
        output_queue_->push(*state);
    }
    
    return true;
}

std::optional<ControllerState> HidInputThread::parseHidReport(const std::vector<uint8_t>& report) {
    // Check minimum size
    if (report.size() < 64) {
        // Report too small - skip silently (common for feature reports)
        return std::nullopt;
    }
    
    // Check report ID (DualSense uses 0x01 for standard input)
    if (report[0] != 0x01) {
        // Wrong report type - skip silently
        return std::nullopt;
    }
    
    ControllerState state;
    state.connected = true;
    state.last_update = std::chrono::steady_clock::now();
    
    // Parse stick values (DualSense report format)
    // Note: Report format differs between USB and Bluetooth
    // This is a simplified parsing - real implementation needs more work
    
    // Left stick: bytes 1-2 (USB) or 2-3 (Bluetooth)
    // Right stick: bytes 3-4 (USB) or 4-5 (Bluetooth)
    // For simplicity, assuming USB mode here
    
    state.left_stick.x = u8_to_stick(report[1]);
    state.left_stick.y = u8_to_stick(report[2], true);  // Invert Y
    state.right_stick.x = u8_to_stick(report[3]);
    state.right_stick.y = u8_to_stick(report[4], true);
    
    // Triggers: bytes 8-9 (USB)
    state.l2 = u8_to_trigger(report[8]);
    state.r2 = u8_to_trigger(report[9]);
    
    // Buttons: bytes 5-7 (complex bit packing)
    // This is simplified - real implementation needs full button parsing
    uint16_t buttons = (report[5] & 0xF0) | ((report[6] & 0xFF) << 8);
    state.buttons = buttons | Ds4Buttons::DPAD_NEUTRAL;  // Ensure dpad neutral
    state.special = report[7] & 0x03;
    
    return state;
}
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/controller/hid_input_thread.cpp
git commit -m "feat(controller): implement HID input thread with DualSense detection"
```

---

## Chunk 4: Controller Bridge

### Task 9: Controller Bridge Header

**Files:**
- Create: `sunone_aimbot_2/controller/controller_bridge.h`

- [ ] **Step 1: Write controller_bridge.h**

```cpp
// sunone_aimbot_2/controller/controller_bridge.h
#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include "controller_state.h"
#include "ds4_gamepad.h"
#include "thread_safe_queue.h"

class ControllerBridge {
public:
    ControllerBridge();
    ~ControllerBridge();
    
    bool initialize();
    void shutdown();
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Input/Output queues
    void setInputQueue(ThreadSafeQueue<ControllerState>* input) { input_queue_ = input; }
    void setOutputQueue(ThreadSafeQueue<DS4_REPORT>* output) { output_queue_ = output; }
    
    // Aimbot injection
    void injectAimInput(float delta_x, float delta_y);
    void setAimbotActive(bool active) { aimbot_active_.store(active); }
    
private:
    void run();
    void processState(ControllerState& state);
    DS4_REPORT encodeToDs4(const ControllerState& state);
    
    ThreadSafeQueue<ControllerState>* input_queue_ = nullptr;
    ThreadSafeQueue<DS4_REPORT>* output_queue_ = nullptr;
    
    std::thread thread_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> running_{false};
    
    std::atomic<bool> aimbot_active_{false};
    std::atomic<float> aim_delta_x_{0.0f};
    std::atomic<float> aim_delta_y_{0.0f};
    
    // Crash recovery
    int crash_count_ = 0;
    std::chrono::steady_clock::time_point last_crash_time_;
};
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/controller_bridge.h
git commit -m "feat(controller): add ControllerBridge header"
```

---

### Task 10: Controller Bridge Implementation

**Files:**
- Create: `sunone_aimbot_2/controller/controller_bridge.cpp`

- [ ] **Step 1: Write controller_bridge.cpp**

```cpp
// sunone_aimbot_2/controller/controller_bridge.cpp
#include "controller_bridge.h"
#include <iostream>
#include <chrono>

ControllerBridge::ControllerBridge() = default;

ControllerBridge::~ControllerBridge() {
    stop();
}

bool ControllerBridge::initialize() {
    return true;
}

void ControllerBridge::shutdown() {
    stop();
}

void ControllerBridge::start() {
    if (running_.load()) {
        return;
    }
    
    stop_flag_.store(false);
    thread_ = std::thread(&ControllerBridge::run, this);
}

void ControllerBridge::stop() {
    stop_flag_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ControllerBridge::run() {
    running_.store(true);
    std::cout << "[Controller] Bridge thread started" << std::endl;
    
    while (!stop_flag_.load()) {
        try {
            ControllerState state;
            
            // Read from input queue with timeout
            if (input_queue_ && input_queue_->pop_timeout(state, std::chrono::milliseconds(10))) {
                // Process state (apply aimbot injection)
                processState(state);
                
                // Encode to DS4 report
                DS4_REPORT report = encodeToDs4(state);
                
                // Push to output queue
                if (output_queue_) {
                    output_queue_->push(report);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[Controller] Bridge exception: " << e.what() << std::endl;
            
            // Crash recovery with backoff
            auto now = std::chrono::steady_clock::now();
            auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_crash_time_).count();
            
            if (time_since_last < 5000) {  // Within 5 seconds
                crash_count_++;
            } else {
                crash_count_ = 1;
            }
            last_crash_time_ = now;
            
            if (crash_count_ > 10) {
                std::cerr << "[Controller] Bridge crashed 10 times, stopping" << std::endl;
                break;
            }
            
            // Exponential backoff
            int delay_ms = std::min(100 * (1 << (crash_count_ - 1)), 5000);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
    
    running_.store(false);
    std::cout << "[Controller] Bridge thread stopped" << std::endl;
}

void ControllerBridge::injectAimInput(float delta_x, float delta_y) {
    // Validate inputs - reject NaN or Inf
    if (!std::isfinite(delta_x) || !std::isfinite(delta_y)) {
        return;
    }
    
    aim_delta_x_.store(delta_x);
    aim_delta_y_.store(delta_y);
}

void ControllerBridge::processState(ControllerState& state) {
    if (!state.connected) {
        return;
    }
    
    if (aimbot_active_.load()) {
        float dx = aim_delta_x_.load();
        float dy = aim_delta_y_.load();
        
        // Add to right stick with clamping
        state.right_stick.x = std::clamp(state.right_stick.x + dx, -1.0f, 1.0f);
        state.right_stick.y = std::clamp(state.right_stick.y + dy, -1.0f, 1.0f);
        state.aimbot_modified = true;
    }
}

DS4_REPORT ControllerBridge::encodeToDs4(const ControllerState& state) {
    DS4_REPORT report;
    
    if (!state.connected) {
        // Return neutral report for disconnected state
        return report;
    }
    
    // Convert analog values
    report.thumb_lx = stick_to_u8(state.left_stick.x);
    report.thumb_ly = stick_to_u8(state.left_stick.y, true);  // Invert Y
    report.thumb_rx = stick_to_u8(state.right_stick.x);
    report.thumb_ry = stick_to_u8(state.right_stick.y, true);
    report.trigger_l = trigger_to_u8(state.l2);
    report.trigger_r = trigger_to_u8(state.r2);
    
    // Copy buttons
    report.buttons = state.buttons;
    report.special = state.special;
    
    return report;
}
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/controller_bridge.cpp
git commit -m "feat(controller): implement ControllerBridge with aimbot injection"
```

---

## Chunk 5: Controller Manager

### Task 11: Controller Manager Header

**Files:**
- Create: `sunone_aimbot_2/controller/controller_manager.h`

- [ ] **Step 1: Write controller_manager.h**

```cpp
// sunone_aimbot_2/controller/controller_manager.h
#pragma once

#include <memory>
#include <mutex>
#include "controller_state.h"
#include "hid_input_thread.h"
#include "controller_bridge.h"
#include "ds4_gamepad.h"
#include "thread_safe_queue.h"

enum class ControllerError {
    None,
    ViGEmDriverNotInstalled,
    ViGEmClientDllNotFound,
    HidDeviceNotFound,
    HidDeviceDisconnected,
    HidOpenFailed,
    BridgeThreadCrashed,
    InvalidButtonMapping
};

inline const char* controllerErrorToString(ControllerError error) {
    switch (error) {
        case ControllerError::None: return "No error";
        case ControllerError::ViGEmDriverNotInstalled: return "ViGEmBus driver not installed";
        case ControllerError::ViGEmClientDllNotFound: return "ViGEmClient.dll not found";
        case ControllerError::HidDeviceNotFound: return "DualSense not found";
        case ControllerError::HidDeviceDisconnected: return "DualSense disconnected";
        case ControllerError::HidOpenFailed: return "Failed to open HID device";
        case ControllerError::BridgeThreadCrashed: return "Controller bridge crashed";
        case ControllerError::InvalidButtonMapping: return "Invalid button mapping";
        default: return "Unknown error";
    }
}

struct ControllerConfig {
    bool enabled = false;
    bool auto_connect = true;
    int polling_rate_hz = 1000;
    bool enable_aimbot_injection = true;
    bool show_overlay = true;
    bool invert_left_stick_y = false;
    bool invert_right_stick_y = false;
    float aimbot_sensitivity = 1.0f;
};

class ControllerManager {
public:
    // Thread-safe singleton access (Meyers' pattern)
    static ControllerManager& instance() {
        static ControllerManager instance;
        return instance;
    }
    
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
    ~ControllerManager() { shutdown(); }
    ControllerManager(const ControllerManager&) = delete;
    ControllerManager& operator=(const ControllerManager&) = delete;
    
    std::unique_ptr<HidInputThread> hid_thread_;
    std::unique_ptr<ControllerBridge> bridge_;
    std::unique_ptr<Ds4Gamepad> gamepad_;
    
    ThreadSafeQueue<ControllerState> hid_to_bridge_queue_{256};
    ThreadSafeQueue<DS4_REPORT> bridge_to_vigem_queue_{256};
    
    ControllerConfig config_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    mutable std::mutex state_mutex_;
    ControllerState current_state_;
    ControllerError last_error_ = ControllerError::None;
};
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/controller_manager.h
git commit -m "feat(controller): add ControllerManager header with singleton pattern"
```

---

### Task 12: Controller Manager Implementation

**Files:**
- Create: `sunone_aimbot_2/controller/controller_manager.cpp`

- [ ] **Step 1: Write controller_manager.cpp**

```cpp
// sunone_aimbot_2/controller/controller_manager.cpp
#include "controller_manager.h"
#include <iostream>

bool ControllerManager::initialize(const ControllerConfig& config) {
    if (initialized_.load()) {
        return true;
    }
    
    config_ = config;
    
    if (!config_.enabled) {
        return true;
    }
    
    std::cout << "[Controller] Initializing..." << std::endl;
    
    // Initialize ViGEm gamepad
    gamepad_ = std::make_unique<Ds4Gamepad>();
    if (!gamepad_->initialize()) {
        last_error_ = ControllerError::ViGEmDriverNotInstalled;
        std::cerr << "[Controller] Failed to initialize ViGEm: " 
                  << gamepad_->getLastError() << std::endl;
        return false;
    }
    
    // Create bridge
    bridge_ = std::make_unique<ControllerBridge>();
    bridge_->setInputQueue(&hid_to_bridge_queue_);
    bridge_->setOutputQueue(&bridge_to_vigem_queue_);
    
    if (!bridge_->initialize()) {
        last_error_ = ControllerError::BridgeThreadCrashed;
        return false;
    }
    
    // Create HID thread
    hid_thread_ = std::make_unique<HidInputThread>();
    hid_thread_->setOutputQueue(&hid_to_bridge_queue_);
    
    initialized_.store(true);
    std::cout << "[Controller] Initialized successfully" << std::endl;
    return true;
}

void ControllerManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    stop();
    
    hid_thread_.reset();
    bridge_.reset();
    gamepad_.reset();
    
    initialized_.store(false);
    std::cout << "[Controller] Shutdown complete" << std::endl;
}

void ControllerManager::start() {
    if (!initialized_.load() || running_.load()) {
        return;
    }
    
    if (hid_thread_) hid_thread_->start();
    if (bridge_) bridge_->start();
    
    running_.store(true);
    std::cout << "[Controller] Started" << std::endl;
}

void ControllerManager::stop() {
    if (!running_.load()) {
        return;
    }
    
    if (hid_thread_) hid_thread_->stop();
    if (bridge_) bridge_->stop();
    
    running_.store(false);
    std::cout << "[Controller] Stopped" << std::endl;
}

bool ControllerManager::isConnected() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_.connected;
}

ControllerState ControllerManager::getCurrentState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
}

ControllerError ControllerManager::getLastError() const {
    return last_error_;
}

void ControllerManager::injectAimInput(float delta_x, float delta_y) {
    if (bridge_ && config_.enable_aimbot_injection) {
        bridge_->injectAimInput(delta_x * config_.aimbot_sensitivity, 
                                delta_y * config_.aimbot_sensitivity);
    }
}

void ControllerManager::setAimbotActive(bool active) {
    if (bridge_) {
        bridge_->setAimbotActive(active);
    }
}

void ControllerManager::updateConfig(const ControllerConfig& config) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    config_ = config;
}
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/controller/controller_manager.cpp
git commit -m "feat(controller): implement ControllerManager lifecycle and aimbot integration"
```

---

## Chunk 6: Unit Tests

### Task 13: Unit Tests for Core Components

**Files:**
- Create: `tests/controller_unit_tests.cpp`

- [ ] **Step 1: Write unit tests**

```cpp
// tests/controller_unit_tests.cpp
#include <gtest/gtest.h>
#include "../sunone_aimbot_2/controller/controller_state.h"
#include "../sunone_aimbot_2/controller/ds4_button_encoding.h"
#include "../sunone_aimbot_2/controller/axis_conversion.h"
#include "../sunone_aimbot_2/controller/ds4_gamepad.h"
#include "../sunone_aimbot_2/controller/thread_safe_queue.h"

// Test axis conversion
TEST(AxisConversion, CenterPosition) {
    EXPECT_EQ(stick_to_u8(0.0f), 0x80);
}

TEST(AxisConversion, FullRange) {
    EXPECT_EQ(stick_to_u8(-1.0f), 0x00);
    EXPECT_EQ(stick_to_u8(1.0f), 0xFF);
}

TEST(AxisConversion, Invert) {
    EXPECT_EQ(stick_to_u8(-1.0f, true), 0xFF);
    EXPECT_EQ(stick_to_u8(1.0f, true), 0x00);
}

TEST(AxisConversion, NaNHandling) {
    EXPECT_EQ(stick_to_u8(std::numeric_limits<float>::quiet_NaN()), 0x80);
}

TEST(AxisConversion, InfHandling) {
    EXPECT_EQ(stick_to_u8(std::numeric_limits<float>::infinity()), 0xFF);
    EXPECT_EQ(stick_to_u8(-std::numeric_limits<float>::infinity()), 0x00);
}

TEST(TriggerConversion, FullRange) {
    EXPECT_EQ(trigger_to_u8(0.0f), 0);
    EXPECT_EQ(trigger_to_u8(1.0f), 255);
}

TEST(TriggerConversion, NaNHandling) {
    EXPECT_EQ(trigger_to_u8(std::numeric_limits<float>::quiet_NaN()), 0);
}

// Test DS4_REPORT
TEST(Ds4Report, SizeCheck) {
    EXPECT_EQ(sizeof(DS4_REPORT), 9);
}

TEST(Ds4Report, DefaultValues) {
    DS4_REPORT report;
    EXPECT_EQ(report.thumb_lx, 0x80);
    EXPECT_EQ(report.thumb_ly, 0x80);
    EXPECT_EQ(report.buttons, 0x08);
}

TEST(Ds4Report, FromControllerState) {
    ControllerState state;
    state.left_stick.x = 0.0f;
    state.left_stick.y = 0.0f;
    state.right_stick.x = 1.0f;
    state.right_stick.y = -1.0f;
    state.l2 = 0.5f;
    state.r2 = 1.0f;
    state.connected = true;
    
    DS4_REPORT report;
    report.fromControllerState(state);
    
    EXPECT_EQ(report.thumb_lx, 0x80);
    EXPECT_EQ(report.thumb_rx, 0xFF);
    EXPECT_EQ(report.trigger_l, 128);  // ~0.5 * 255
    EXPECT_EQ(report.trigger_r, 255);
}

// Test button encoding
TEST(ButtonEncoding, CrossButton) {
    uint16_t buttons = Ds4Buttons::CROSS;
    EXPECT_TRUE(buttons & Ds4Buttons::CROSS);
    EXPECT_FALSE(buttons & Ds4Buttons::CIRCLE);
}

TEST(ButtonEncoding, DpadEncoding) {
    EXPECT_EQ(encodeDpad(true, false, false, false), Ds4Buttons::DPAD_NORTH);
    EXPECT_EQ(encodeDpad(true, false, false, true), Ds4Buttons::DPAD_NORTHEAST);
    EXPECT_EQ(encodeDpad(false, false, false, false), Ds4Buttons::DPAD_NEUTRAL);
}

// Test ThreadSafeQueue
TEST(ThreadSafeQueue, PushPop) {
    ThreadSafeQueue<int> queue;
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    
    int val;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 2);
}

TEST(ThreadSafeQueue, CapacityRespected) {
    ThreadSafeQueue<int> queue(3);
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_TRUE(queue.push(4));  // Should drop oldest
    
    int val;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 2);  // 1 was dropped
}

TEST(ThreadSafeQueue, Empty) {
    ThreadSafeQueue<int> queue;
    EXPECT_TRUE(queue.empty());
    
    int val;
    EXPECT_FALSE(queue.try_pop(val));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 2: Commit**

```bash
git add tests/controller_unit_tests.cpp
git commit -m "test(controller): add unit tests for core components"
```

---

## Chunk 7: Configuration Integration

### Task 14: Add ControllerConfig to Config

**Files:**
- Modify: `sunone_aimbot_2/config/config.h`
- Modify: `sunone_aimbot_2/config/config.cpp`

- [ ] **Step 1: Add ControllerConfig to config.h**

Find the Config struct and add the controller member:

```cpp
// In sunone_aimbot_2/config/config.h
// Add this include at the top:
#include "../controller/controller_manager.h"

// Add this member to the Config struct (after existing members):
    ControllerConfig controller;
```

- [ ] **Step 2: Add JSON parsing for controller config in config.cpp**

Find where other config values are parsed and add:

```cpp
// In sunone_aimbot_2/config/config.cpp
// Add in parseConfig() function:

    // Controller config
    if (json.contains("controller")) {
        auto& ctrl = json["controller"];
        controller.enabled = ctrl.value("enabled", false);
        controller.auto_connect = ctrl.value("auto_connect", true);
        controller.polling_rate_hz = ctrl.value("polling_rate_hz", 1000);
        controller.enable_aimbot_injection = ctrl.value("enable_aimbot_injection", true);
        controller.show_overlay = ctrl.value("show_overlay", true);
        controller.invert_left_stick_y = ctrl.value("invert_left_stick_y", false);
        controller.invert_right_stick_y = ctrl.value("invert_right_stick_y", false);
        controller.aimbot_sensitivity = ctrl.value("aimbot_sensitivity", 1.0f);
    }

// Add in saveConfig() function:

    // Controller config
    json["controller"] = {
        {"enabled", controller.enabled},
        {"auto_connect", controller.auto_connect},
        {"polling_rate_hz", controller.polling_rate_hz},
        {"enable_aimbot_injection", controller.enable_aimbot_injection},
        {"show_overlay", controller.show_overlay},
        {"invert_left_stick_y", controller.invert_left_stick_y},
        {"invert_right_stick_y", controller.invert_right_stick_y},
        {"aimbot_sensitivity", controller.aimbot_sensitivity}
    };
```

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/config/config.h sunone_aimbot_2/config/config.cpp
git commit -m "feat(config): add ControllerConfig parsing and serialization"
```

---

## Chunk 8: Main Application Integration

### Task 15: Initialize Controller in Main

**Files:**
- Modify: `sunone_aimbot_2/sunone_aimbot_2.cpp`
- Modify: `sunone_aimbot_2/sunone_aimbot_2.h`

- [ ] **Step 1: Add controller include in sunone_aimbot_2.h**

```cpp
// In sunone_aimbot_2/sunone_aimbot_2.h
// Add this include:
#include "controller/controller_manager.h"
```

- [ ] **Step 2: Initialize controller in sunone_aimbot_2.cpp**

Find where other input devices are initialized (around `createInputDevices()`) and add:

```cpp
// In sunone_aimbot_2/sunone_aimbot_2.cpp
// Add after createInputDevices() call in main():

    // Initialize controller
    if (config.controller.enabled) {
        if (ControllerManager::instance().initialize(config.controller)) {
            ControllerManager::instance().start();
        }
    }

// Add in cleanup section (before return 0):
    ControllerManager::instance().shutdown();
```

- [ ] **Step 3: Commit**

```bash
git add sunone_aimbot_2/sunone_aimbot_2.h sunone_aimbot_2/sunone_aimbot_2.cpp
git commit -m "feat(main): integrate controller initialization"
```

---

### Task 16: Integrate Aimbot with Controller

**Files:**
- Modify: `sunone_aimbot_2/mouse/mouse.cpp`

- [ ] **Step 1: Add aimbot injection in mouse.cpp**

Find where aimbot calculates movement and add controller injection:

```cpp
// In sunone_aimbot_2/mouse/mouse.cpp
// Add at the top:
#include "../controller/controller_manager.h"

// Find where mouse movement is calculated (in moveMousePivot or similar)
// Add after calculating dx, dy:

    // Inject into controller if enabled
    if (config.controller.enabled && config.controller.enable_aimbot_injection) {
        // Convert pixel delta to normalized stick values
        // This is approximate - adjust based on your game's sensitivity
        float stick_x = dx / 100.0f;  // Adjust divisor for sensitivity
        float stick_y = dy / 100.0f;
        
        ControllerManager::instance().injectAimInput(stick_x, stick_y);
    }

// Add where aiming flag is set:
    ControllerManager::instance().setAimbotActive(aiming);

// Add where aiming flag is cleared:
    ControllerManager::instance().setAimbotActive(false);
```

- [ ] **Step 2: Commit**

```bash
git add sunone_aimbot_2/mouse/mouse.cpp
git commit -m "feat(aimbot): integrate controller aim injection"
```

---

## Chunk 9: CMake Integration

### Task 17: Update CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add controller sources to CMakeLists.txt**

Find the `AIMBOT_SOURCES` section and add:

```cmake
# In CMakeLists.txt
# Add to AIMBOT_SOURCES list:

    "${SRC_DIR}/controller/ds4_gamepad.cpp"
    "${SRC_DIR}/controller/hid_input_thread.cpp"
    "${SRC_DIR}/controller/controller_bridge.cpp"
    "${SRC_DIR}/controller/controller_manager.cpp"
```

- [ ] **Step 2: Add hidapi and ViGEm to CMakeLists.txt**

Add after the existing dependency section:

```cmake
# Add after target_link_libraries section

# Controller dependencies
set(HIDAPI_ROOT "${CMAKE_SOURCE_DIR}/third_party/hidapi" CACHE PATH "Path to hidapi")
set(VIGEM_ROOT "${CMAKE_SOURCE_DIR}/third_party/vigem" CACHE PATH "Path to ViGEmClient")

# hidapi
find_path(HIDAPI_INCLUDE_DIR hidapi.h
    PATHS "${HIDAPI_ROOT}/include"
)
find_library(HIDAPI_LIBRARY hidapi
    PATHS "${HIDAPI_ROOT}/lib"
)

if(NOT HIDAPI_INCLUDE_DIR OR NOT HIDAPI_LIBRARY)
    message(WARNING "hidapi not found. Controller support will be disabled.")
else()
    target_include_directories(ai PRIVATE "${HIDAPI_INCLUDE_DIR}")
    target_link_libraries(ai PRIVATE "${HIDAPI_LIBRARY}")
endif()

# ViGEmClient
find_path(VIGEM_INCLUDE_DIR ViGEm/Client.h
    PATHS "${VIGEM_ROOT}/include"
)
find_library(VIGEM_LIBRARY ViGEmClient
    PATHS "${VIGEM_ROOT}/lib"
)

if(NOT VIGEM_INCLUDE_DIR OR NOT VIGEM_LIBRARY)
    message(WARNING "ViGEmClient not found. Controller support will be disabled.")
else()
    target_include_directories(ai PRIVATE "${VIGEM_INCLUDE_DIR}")
    target_link_libraries(ai PRIVATE "${VIGEM_LIBRARY}")
endif()

# Copy DLLs
if(EXISTS "${HIDAPI_ROOT}/bin/hidapi.dll")
    add_custom_command(TARGET ai POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${HIDAPI_ROOT}/bin/hidapi.dll" $<TARGET_FILE_DIR:ai>
    )
endif()

if(EXISTS "${VIGEM_ROOT}/bin/ViGEmClient.dll")
    add_custom_command(TARGET ai POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VIGEM_ROOT}/bin/ViGEmClient.dll" $<TARGET_FILE_DIR:ai>
    )
endif()
```

- [ ] **Step 3: Add controller unit tests to CMakeLists.txt**

```cmake
# Add after training_labeling_tests definition

add_executable(controller_unit_tests
    "${CMAKE_SOURCE_DIR}/tests/controller_unit_tests.cpp"
    "${SRC_DIR}/controller/controller_state.h"
    "${SRC_DIR}/controller/ds4_button_encoding.h"
    "${SRC_DIR}/controller/axis_conversion.h"
    "${SRC_DIR}/controller/ds4_gamepad.h"
    "${SRC_DIR}/controller/ds4_gamepad.cpp"
    "${SRC_DIR}/controller/thread_safe_queue.h"
)

target_include_directories(controller_unit_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${SRC_DIR}"
)

if(MSVC)
    target_compile_options(controller_unit_tests PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W3>
        $<$<COMPILE_LANGUAGE:CXX>:/sdl>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    )
    set_target_properties(controller_unit_tests PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# Link Google Test (assuming it's available)
find_package(GTest QUIET)
if(GTest_FOUND)
    target_link_libraries(controller_unit_tests PRIVATE GTest::gtest GTest::gtest_main)
else()
    message(WARNING "Google Test not found. Controller unit tests will not build.")
endif()
```

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build(cmake): add controller sources and dependencies"
```

---

## Chunk 10: UI Overlay

### Task 18: Controller Status Overlay

**Files:**
- Create: `sunone_aimbot_2/overlay/draw_controller.cpp`

- [ ] **Step 1: Create draw_controller.cpp**

```cpp
// sunone_aimbot_2/overlay/draw_controller.cpp
#include "overlay.h"
#include "../controller/controller_manager.h"
#include "../config/config.h"
#include "imgui.h"

void drawControllerStatus() {
    if (!config.controller.enabled || !config.controller.show_overlay) {
        return;
    }
    
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin("Controller", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    
    auto& mgr = ControllerManager::instance();
    
    if (mgr.isConnected()) {
        auto state = mgr.getCurrentState();
        
        // Status indicator
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "DualSense Connected");
        
        // Analog sticks (visual bars)
        ImGui::Text("Left Stick:");
        ImGui::SameLine();
        ImGui::ProgressBar((state.left_stick.x + 1.0f) * 0.5f, ImVec2(60, 0), "X");
        ImGui::SameLine();
        ImGui::ProgressBar((state.left_stick.y + 1.0f) * 0.5f, ImVec2(60, 0), "Y");
        
        ImGui::Text("Right Stick:");
        ImGui::SameLine();
        ImGui::ProgressBar((state.right_stick.x + 1.0f) * 0.5f, ImVec2(60, 0), "X");
        ImGui::SameLine();
        ImGui::ProgressBar((state.right_stick.y + 1.0f) * 0.5f, ImVec2(60, 0), "Y");
        
        // Triggers
        ImGui::Text("Triggers: L2=%.0f%% R2=%.0f%%", 
            state.l2 * 100.0f, state.r2 * 100.0f);
        
        // Aimbot indicator
        if (state.aimbot_modified) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Aimbot Active");
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Controller Disconnected");
        ImGui::Text("Connect DualSense via USB or Bluetooth");
        
        auto error = mgr.getLastError();
        if (error != ControllerError::None) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), 
                "Error: %s", controllerErrorToString(error));
        }
    }
    
    ImGui::End();
}

void drawControllerSettings() {
    ImGui::Checkbox("Enable Controller", &config.controller.enabled);
    
    if (config.controller.enabled) {
        ImGui::Checkbox("Auto-Connect", &config.controller.auto_connect);
        ImGui::SliderInt("Polling Rate (Hz)", 
            &config.controller.polling_rate_hz, 250, 1000);
        ImGui::Checkbox("Aimbot Injection", 
            &config.controller.enable_aimbot_injection);
        ImGui::Checkbox("Show Overlay", &config.controller.show_overlay);
        ImGui::Checkbox("Invert Left Stick Y", &config.controller.invert_left_stick_y);
        ImGui::Checkbox("Invert Right Stick Y", &config.controller.invert_right_stick_y);
        ImGui::SliderFloat("Aimbot Sensitivity", 
            &config.controller.aimbot_sensitivity, 0.1f, 3.0f);
    }
}
```

- [ ] **Step 2: Add to overlay.h**

```cpp
// In sunone_aimbot_2/overlay/overlay.h
// Add declarations:

void drawControllerStatus();
void drawControllerSettings();
```

- [ ] **Step 3: Call from main overlay loop**

```cpp
// In the main overlay draw function, add:
drawControllerStatus();
```

- [ ] **Step 4: Commit**

```bash
git add sunone_aimbot_2/overlay/draw_controller.cpp sunone_aimbot_2/overlay/overlay.h
git commit -m "feat(ui): add controller status overlay"
```

---

## Final Steps

### Task 19: Build and Test

- [ ] **Step 1: Configure build**

```bash
cmake -S . -B build/cuda -G "Visual Studio 18 2026" -A x64 -T "cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1" -DAIMBOT_USE_CUDA=ON -DCMAKE_CUDA_FLAGS="--allow-unsupported-compiler"
```

- [ ] **Step 2: Build**

```bash
cmake --build build/cuda --config Release
```

- [ ] **Step 3: Run unit tests**

```bash
./build/cuda/Release/controller_unit_tests.exe
```

- [ ] **Step 4: Test with ViGEmBus**

1. Install ViGEmBus driver
2. Run `joy.cpl` (Windows Game Controllers)
3. Verify virtual DS4 appears
4. Connect DualSense
5. Verify physical input passes through to virtual controller

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat: complete ViGEmBus controller integration"
```

---

## Summary

| Chunk | Tasks | Files Created | Files Modified |
|-------|-------|---------------|----------------|
| 1 | 5 | 5 headers | 0 |
| 2 | 1 | 1 cpp | 0 |
| 3 | 2 | 1 h, 1 cpp | 0 |
| 4 | 2 | 1 h, 1 cpp | 0 |
| 5 | 2 | 1 h, 1 cpp | 0 |
| 6 | 1 | 1 cpp | 0 |
| 7 | 1 | 0 | 2 |
| 8 | 2 | 0 | 3 |
| 9 | 1 | 0 | 1 |
| 10 | 1 | 1 cpp | 1 |
| **Total** | **18** | **14 files** | **7 files** |

**Estimated Implementation Time:** 9-14 days (as per spec)