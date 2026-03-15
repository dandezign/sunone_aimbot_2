# SAM3 Dynamic Prompt Preset System Design

**Date:** 2026-03-15  
**Status:** Draft → Review → Approved  
**Author:** Build Agent  
**Reviewers:** User

---

## 1. Overview

### 1.1 Problem Statement

The current SAM3 integration has a hardcoded "person" preset with fixed CLIP token IDs. This limits flexibility:

- Cannot easily add new detection classes (helmet, weapon, etc.)
- No support for multi-word prompts like "level 3 helmet"
- Adding new classes requires C++ code changes and recompilation
- Users cannot customize or share preset configurations

### 1.2 Goals

Build a **dynamic preset system** that:

1. Supports multi-word class names (e.g., "level 3 helmet", "upper body")
2. Provides a Python-based preset builder tool
3. Loads presets from JSON files at runtime
4. Offers UI dropdown + text override for preset selection
5. Hot-reloads preset changes without app restart
6. Loads one selected preset file at a time

### 1.3 Non-Goals

- Embedded C++ CLIP tokenizer (Python dependency is acceptable)
- Merging multiple preset files simultaneously (single-file focus for now)
- Real-time preset switching during active inference (reinit on selection change is acceptable)

---

## 2. Architecture

### 2.1 System Components

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SAM3 PRESET MANAGEMENT SYSTEM                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 1. PRESET BUILDER (scripts/build_sam3_presets.py)               │   │
│  │    - Tokenizes class names using Python CLIP tokenizer          │   │
│  │    - Outputs JSON preset files                                  │   │
│  │    - Interactive and batch modes                                │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                   │                                     │
│                                   ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 2. PRESET FILES (models/presets/*.json)                         │   │
│  │    - default.json (built-in classes)                            │   │
│  │    - custom.json (user-defined)                                 │   │
│  │    - Schema: {version, presets: {name: {ids[], mask[]}}}        │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                   │                                     │
│                                   ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 3. RUNTIME LOADER (training_sam3_labeler.cpp)                   │   │
│  │    - Loads selected preset file at startup                      │   │
│  │    - Provides preset lookup by name                             │   │
│  │    - Hot-reload via file watcher                                │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                   │                                     │
│                                   ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ 4. UI INTEGRATION (draw_training.cpp)                           │   │
│  │    - Dropdown: select from available presets                    │   │
│  │    - Text field: manual override (uses Python fallback)         │   │
│  │    - Config persistence: which preset file + selected preset    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```
User creates preset → Python tokenizer → JSON file
                                               │
                                               ▼
App startup → Load preset file → Populate dropdown
                                               │
                                               ▼
User selects preset → Update config → Trigger reinit
                                               │
                                               ▼
SAM3 runtime → Load new tokens → Infer with new class
```

---

## 3. Component Specifications

### 3.1 Preset Builder Tool

**File:** `scripts/build_sam3_presets.py`

**Purpose:** Generate JSON preset files from class names.

**Usage:**

```powershell
# Batch mode: specify classes directly
py scripts/build_sam3_presets.py --class "head" --class "body" --class "weapon" --output models/presets/default.json

# Interactive mode: prompted for class names
py scripts/build_sam3_presets.py --interactive --output models/presets/custom.json

# From file: read class names from text file
py scripts/build_sam3_presets.py --input classes.txt --output models/presets/my_game.json
```

**Output Format:**

```json
{
  "version": 1,
  "created_at": "2026-03-15T10:30:00Z",
  "source": "build_sam3_presets.py",
  "presets": {
    "head": {
      "ids": [49406, 1234, 49407, 49407, ...],
      "mask": [1, 1, 1, 0, 0, ...]
    },
    "upper body": {
      "ids": [49406, 2345, 5678, 49407, ...],
      "mask": [1, 1, 1, 1, 0, ...]
    }
  }
}
```

**Dependencies:**

- Python 3.12+
- `transformers` library (CLIPTokenizer from `openai/clip-vit-base-patch32`)

**Implementation Notes:**

- Reuse existing `SAM3TensorRT/python/tokenize_prompt.py` tokenizer logic
- Validate token sequences (must be exactly 32 tokens)
- Handle special characters in class names (spaces, underscores)

---

### 3.2 Preset File Schema

**Location:** `models/presets/`

**Schema:**

```typescript
interface PresetFile {
  version: number;           // Schema version (start with 1)
  created_at?: string;       // ISO 8601 timestamp
  source?: string;           // Tool that created this file
  presets: {
    [className: string]: {
      ids: int64[32];        // CLIP input_ids
      mask: int64[32];       // CLIP attention_mask
    }
  }
}
```

**Constraints:**

- Class names: 1-64 characters, alphanumeric + spaces + underscores
- IDs: exactly 32 integers, first must be 49406 (BOS)
- Mask: exactly 32 integers (0 or 1), at least one 1

**Default Presets:** `models/presets/default.json`

| Class Name | Description |
|------------|-------------|
| `head` | Head detection |
| `body` | Full body / torso |
| `weapon` | Weapons / guns |
| `upper body` | Upper torso only |
| `lower body` | Lower torso / legs |

---

### 3.3 Runtime Loader

**File:** `sunone_aimbot_2/training/training_sam3_labeler.cpp` (new)

**Responsibilities:**

1. Load preset file from configured path
2. Parse JSON into in-memory lookup table
3. Provide `GetPresetTokens(className)` function
4. Watch file for changes, trigger reload on modify

**Interface:**

```cpp
namespace training {

class Sam3PresetLoader {
public:
    // Load preset file from path. Returns true on success.
    bool LoadFromFile(const std::filesystem::path& path);
    
    // Get tokens for a preset class. Returns nullptr if not found.
    const Sam3PromptPreset* GetPreset(const std::string& className) const;
    
    // Get list of all available preset names
    std::vector<std::string> GetPresetNames() const;
    
    // Enable/disable hot reload watching
    void EnableHotReload(bool enabled);
    
    // Check if file changed and reload if needed
    void CheckForChanges();

private:
    std::unordered_map<std::string, Sam3PromptPreset> presets_;
    std::filesystem::path presetPath_;
    std::filesystem::file_time_type lastModified_;
    bool hotReloadEnabled_ = false;
};

}  // namespace training
```

**Hot Reload Implementation:**

- Store file's `last_modified` timestamp
- On each preset lookup (or timer tick), check if file changed
- If changed, reload and notify runtime to reinitialize

**Error Handling:**

- Missing file: log warning, use empty preset list
- Invalid JSON: log error, fall back to last valid state
- Corrupt token data: skip that preset, continue loading others

---

### 3.4 UI Integration

**File:** `sunone_aimbot_2/overlay/draw_training.cpp`

**Changes:**

1. Add preset file selector (dropdown or text field)
2. Add preset class selector (dropdown populated from loaded presets)
3. Keep existing prompt text field as manual override
4. Show hot reload status indicator

**Layout:**

```
┌────────────────────────────────────────────────────────────┐
│ SAM3 Detection Controls                                    │
├────────────────────────────────────────────────────────────┤
│ Preset File:  [default.json ▼]  [Reload]                  │
│ Preset Class: [head ▼]         (5 presets loaded)         │
│ Prompt:       [head____________]  [Clear]                  │
│                                                            │
│ ☑ Hot Reload Enabled                                       │
│                                                            │
│ [Rest of SAM3 controls: thresholds, etc.]                 │
└────────────────────────────────────────────────────────────┘
```

**Behavior:**

| Action | Result |
|--------|--------|
| Select preset file | Load file, populate class dropdown |
| Select preset class | Fill prompt field, trigger reinit |
| Edit prompt manually | Use text as-is (Python fallback if not preset) |
| Click Reload | Force reload preset file |
| Toggle hot reload | Enable/disable file watching |

**Config Persistence:**

```cpp
// Config fields to add:
std::string training_sam3_preset_file;      // e.g., "default.json"
std::string training_sam3_preset_class;     // e.g., "head"
bool training_sam3_preset_hot_reload;       // true/false
```

---

### 3.5 Runtime Reinitialization

**File:** `sunone_aimbot_2/training/training_sam3_runtime.cpp`

**Changes:**

1. Detect preset class change in `UpdateSettings()`
2. Invalidate current backend
3. Reinitialize with new prompt tokens
4. Preserve other settings (thresholds, etc.)

**Reinit Flow:**

```
Preset class changed
        │
        ▼
UpdateSettings() detects prompt change
        │
        ▼
Set initRequested_ = true
        │
        ▼
WorkerLoop() processes init request
        │
        ▼
Shutdown old backend
        │
        ▼
Initialize new backend with new tokens
        │
        ▼
Status updated: backend.ready = true
        │
        ▼
Preview resumes with new class
```

**Expected Latency:**

- Backend shutdown: < 100ms
- Backend initialization: 30-60 seconds (TensorRT deserialization)
- Total: ~30-60 seconds (dominated by engine load)

---

## 4. Configuration

### 4.1 New Config Fields

**File:** `sunone_aimbot_2/config/config.h` and `config.cpp`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `training_sam3_preset_file` | string | `"default.json"` | Preset file to load |
| `training_sam3_preset_class` | string | `"head"` | Selected preset class |
| `training_sam3_preset_hot_reload` | bool | `true` | Enable file watching |
| `training_sam3_presets_dir` | string | `"models/presets"` | Preset directory |

### 4.2 Config UI

**File:** `sunone_aimbot_2/overlay/draw_settings.h` (possibly)

Add preset configuration to Settings tab if not in Training tab.

---

## 5. Error Handling

### 5.1 Preset File Errors

| Error | Behavior |
|-------|----------|
| File not found | Log warning, use empty preset list, UI shows "0 presets" |
| Invalid JSON | Log error, show error in UI, keep previous valid state |
| Missing tokens | Skip that preset, continue loading others |
| Token count mismatch | Skip that preset, log warning |

### 5.2 Hot Reload Errors

| Error | Behavior |
|-------|----------|
| File deleted while watching | Stop watching, log warning, keep loaded presets |
| File locked by another process | Retry on next check, log if persistent |
| Corrupt update | Reject update, keep previous valid state, log error |

### 5.3 Reinitialization Errors

| Error | Behavior |
|-------|----------|
| Backend init fails | Show error in UI, revert to previous preset, status shows error |
| Timeout during init | Allow cancellation, show progress indicator |

---

## 6. Testing

### 6.1 Unit Tests

**File:** `tests/training_labeling_tests.cpp`

| Test | Description |
|------|-------------|
| `PresetLoader_LoadsValidFile` | Load default.json, verify preset count |
| `PresetLoader_GetPresetTokens` | Lookup "head", verify token sequence |
| `PresetLoader_HandlesMissingFile` | Load non-existent file, verify graceful fallback |
| `PresetLoader_HandlesInvalidJson` | Load corrupt JSON, verify error handling |
| `PresetLoader_HotReloadDetectsChange` | Modify file, verify reload triggered |
| `PresetLoader_MultiWordClassNames` | Load "upper body", verify correct lookup |

### 6.2 Integration Tests

| Test | Description |
|------|-------------|
| `Builder_CreatesValidPresetFile` | Run builder, load output, verify tokens |
| `UI_PresetSelectionUpdatesPrompt` | Select preset, verify prompt field updates |
| `UI_PresetFileChangeReloads` | Change file, verify dropdown repopulates |
| `Runtime_ReinitOnPresetChange` | Change preset, verify backend reinitializes |
| `HotReload_AutoReloadsOnFileChange` | Modify preset file, verify auto-reload |

### 6.3 Manual Tests

1. Create custom preset file with new class
2. Select file in UI, verify class appears in dropdown
3. Select class, verify SAM3 detects new class
4. Modify preset file, verify hot reload triggers
5. Test multi-word class names work correctly

---

## 7. Migration

### 7.1 Existing Behavior

- Current: Hardcoded "person" preset in `training_sam3_labeler.h`
- No preset file loading
- No hot reload

### 7.2 Migration Steps

1. Create `models/presets/default.json` with default classes
2. Update preset lookup to check loaded presets first
3. Fall back to hardcoded "person" preset if file not loaded (temporary)
4. Remove hardcoded presets once file loading is stable

### 7.3 Backward Compatibility

- Existing config without preset fields: use defaults
- Missing preset file: app continues with empty preset list
- Manual prompt text still works (Python fallback)

---

## 8. Future Enhancements

| Enhancement | Description |
|-------------|-------------|
| Multiple preset file merging | Load and merge multiple files simultaneously |
| Preset sharing UI | Import/export preset files, share with community |
| Preset validation tool | Verify preset tokens produce expected results |
| Embedded tokenizer | Remove Python dependency with C++ tokenizer |
| Preset categories | Organize presets by game, body part, equipment type |
| Preset preview | Show sample detections before applying |

---

## 9. Implementation Phases

### Phase 1: Preset Builder + Default File
- Create `scripts/build_sam3_presets.py`
- Generate `models/presets/default.json`
- Test tokenizer output manually

### Phase 2: Runtime Loader
- Create `training_sam3_labeler.cpp` with `Sam3PresetLoader`
- Add JSON parsing (use existing JSON library in project)
- Add preset lookup integration

### Phase 3: UI Integration
- Add preset file selector to Training tab
- Add preset class dropdown
- Add hot reload toggle + status

### Phase 4: Hot Reload
- Implement file watcher
- Add change detection + auto-reload
- Add manual reload button

### Phase 5: Runtime Reinit
- Wire preset change to backend reinitialization
- Add progress indicator during reinit
- Add error handling + rollback

### Phase 6: Testing + Polish
- Add unit + integration tests
- Manual validation with real SAM3 inference
- Documentation + user guide

---

## 10. Dependencies

| Dependency | Source | Purpose |
|------------|--------|---------|
| Python 3.12+ | System | Preset builder tool |
| `transformers` | PyPI | CLIP tokenizer |
| JSON parser | Existing (nlohmann/json?) | Parse preset files |
| Filesystem watcher | Existing or `std::filesystem` | Hot reload detection |

---

## 11. Acceptance Criteria

- [ ] Preset builder creates valid JSON with correct token sequences
- [ ] Default preset file includes: head, body, weapon, upper body, lower body
- [ ] App loads preset file at startup
- [ ] UI shows preset file selector + class dropdown
- [ ] Selecting preset class triggers SAM3 reinitialization
- [ ] Hot reload detects file changes and reloads automatically
- [ ] Multi-word class names work correctly
- [ ] Error handling graceful for missing/corrupt files
- [ ] All unit + integration tests pass
- [ ] Manual validation confirms detections work for each preset class
