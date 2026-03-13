# Easy Anti-Cheat (EAC) Kernel Driver — Complete Technical Analysis

> Comprehensive reverse engineering documentation covering cryptography, detection methods, vulnerabilities, and architectural analysis of the EAC kernel driver (`EasyAntiCheat.sys`).
> 
> **Research Date**: March 2026  
> **Base Address**: `0xFFFFF807C1E10000` (analyzed build)  
> **Binary Size**: ~8 MB (Code: ~7 MB, Data: ~1 MB)  
> **Primary Target**: Rust, Fortnite, Apex Legends, Dead by Daylight

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Cryptography & Obfuscation](#2-cryptography--obfuscation)
3. [Detection Methods](#3-detection-methods)
4. [External Cheat Detection](#4-external-cheat-detection)
5. [Internal Cheat Detection](#5-internal-cheat--injection-detection)
6. [Hardware ID & Spoofer Detection](#6-hardware-id--spoofer-detection)
7. [Telemetry System](#7-telemetry-system)
8. [User-Mode Component (Ring 3)](#8-user-mode-component-ring-3)
9. [User-Mode Techniques & Gaps](#9-user-mode-techniques--gaps)
10. [Vulnerabilities & Detection Gaps](#10-vulnerabilities--detection-gaps)
11. [Function Reference](#11-function-reference)
12. [Global Data Structures](#12-global-data-structures)

---

## 1. Architecture Overview

### 1.1 Ring 0 vs Ring 3 Architecture

```
User Mode (Ring 3)                    Kernel Mode (Ring 0)
─────────────────────────────────    ─────────────────────────────────
Game.exe (Protected Process)              EasyAntiCheat.sys
    │                                       │
    ├── GameModule.dll (EAC SDK)            ├── DriverEntry
    │       │                               ├── IOCTL Dispatch
    │       └── DeviceIoControl             ├── Process/Thread Callbacks
    │               │                       ├── VAD Tree Scanner
    │               ▼                       ├── Handle Table Walker
EasyAntiCheat.exe (Service)               ├── Cryptographic Engine
    │                                         (P-256, Zstd, SHA)
    ├── Window/Overlay Detection            ├── Hardware ID Collection
    ├── Anti-Debug Checks                   └── Telemetry Assembler
    └── Network Client (HTTPS)
            │
            ▼
    EAC Backend Servers
    (*.easyanticheat.net)
```

### 1.2 Key Design Principles

- **No Import Table**: All Windows API calls resolved via encrypted function pointer table
- **String Obfuscation**: No plaintext strings; runtime XOR decoding
- **Signed Telemetry**: All reports cryptographically signed with P-256 ECDSA
- **Kernel-First**: Critical detection logic runs at Ring 0; user-mode is secondary
- **Multi-Source Validation**: Hardware IDs cross-checked across 6+ independent sources

### 1.3 EPROCESS Offsets Used by EAC

| Field | Offset | Purpose |
|-------|--------|---------|
| UniqueProcessId | +56 | Token/PID verification |
| InheritedFromUniqueProcessId | +64 | Parent chain analysis |
| ImageFileName | +96 | 15-char name hash |
| Peb/VadRoot | +240 | Memory map traversal |
| ObjectTable | +376 | Handle table access |
| Protection flags | +556 | PS_PROTECTION validation |

---

## 2. Cryptography & Obfuscation

### 2.1 P-256 Elliptic Curve Cryptography

EAC implements a full **secp256r1 / NIST P-256** stack for:
- Telemetry packet signing
- Challenge-response authentication with backend
- Code integrity verification

#### Layer 1: Field Arithmetic (9-Limb 30-Bit Radix)

**Function**: `sub_FFFFF807C1E21280` (size: 0x409)

256-bit field elements stored as 9 × 30-bit limbs to prevent 64-bit overflow:

```c
// P-256 field element representation
typedef struct {
    uint32_t limbs[9];  // 9 × 30-bit = 270 bits (top 14 bits unused)
} P256_FieldElement;

// Prime: p = 2^256 - 2^224 + 2^192 + 2^96 - 1

// Polynomial multiplication with carry propagation
void P256_FieldMul(uint32_t* result, uint32_t* a, uint32_t* b) {
    uint64_t temp[18] = {0};
    
    // Schoolbook multiply (9×9 = 81 partial products)
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            temp[i + j] += (uint64_t)a[i] * b[j];
        }
    }
    
    // Carry propagation with 30-bit masking
    uint64_t carry = 0;
    for (int i = 0; i < 17; i++) {
        uint64_t sum = temp[i] + carry;
        result[i] = sum & 0x3FFFFFFF;  // Keep bottom 30 bits
        carry = sum >> 30;              // Propagate top bits
    }
    result[17] = carry;
}
```

#### Layer 2: Constant-Time Scalar Multiplication

**Function**: `sub_FFFFF807C1E226E0` (Montgomery Ladder, size: 0x31D)

Implements **2-bit NAF (Non-Adjacent Form)** window method:
- Processes 2 bits per iteration (128 rounds for 256-bit scalar)
- Constant-time conditional swap prevents timing side-channels:

```c
// Constant-time conditional swap (no branches)
void ConditionalSwap(uint8_t* a, uint8_t* b, int condition) {
    uint8_t mask = -(condition);  // 0xFF if true, 0x00 if false
    for (int i = 0; i < 32; i++) {
        uint8_t diff = (a[i] ^ b[i]) & mask;
        a[i] ^= diff;
        b[i] ^= diff;
    }
}
```

**Verification Research**: This implementation matches the standard NIST P-256 specification and uses constant-time operations to prevent timing side-channel attacks. The 9-limb 30-bit representation is a well-known optimization for avoiding 64-bit overflow during field arithmetic.

### 2.2 Number Theoretic Transform (NTT)

**Function**: `sub_FFFFF807C1E1AF00` (shared with Montgomery)

- Uses 62-bit prime field (`& 0x3FFFFFFFFFFFFFFF`)
- FFT over modular arithmetic for polynomial multiplication
- Applications: Lattice-based crypto, zero-knowledge proofs, RSA acceleration

**Key Operations**:
- Butterfly: `sub_FFFFF807C1E1AB00` (Cooley-Tukey)
- Bit-reversal permutation: `sub_FFFFF807C1E1AA80`
- Finalize (divide by n): `sub_FFFFF807C1E1A7C0`

### 2.3 Zstd Compression Engine

Complete Zstandard implementation with three performance tiers:

| Tier | Function | Size | Features |
|------|----------|------|----------|
| SSE2 | `sub_FFFFF807C1E11EE0` | 0x579 | 2-stream Huffman, 128-bit XMM |
| AVX2 v1 | `sub_FFFFF807C1E12460` | 0x54D | 256-bit YMM, 2 independent streams |
| AVX2 v2 | `sub_FFFFF807C1E13100` | 0x577 | 6 parallel streams, 15 bytes/iteration |

**Key AVX2 Instructions Used**:
- `vpinsrb` — Insert byte into XMM
- `vmovdqu` — Unaligned 256-bit move
- `vpaddq` — Parallel 64-bit add

### 2.4 Hash Algorithm Suite

**Function**: `sub_FFFFF807C1E3A4C0` (Selector-based initialization)

| Selector | Algorithm | Output | Initialization Constants |
|----------|-----------|--------|-------------------------|
| 1 | SHA-1 | 20 bytes | `0x67452301`, `0xEFCDAB89`, `0x98BADCFE`, `0x10325476`, `0xC3D2E1F0` |
| 2 | MD5 | 16 bytes | `1732584193`, `-271733879`, `-1732584194`, `271733878` |
| 4 | SHA-224 | 28 bytes | `0xC1059ED8`, `0x367CD507`, `0x3070DD17`, `0xF70E5939`... |
| 5 | SHA-256 | 32 bytes | `0x6A09E667`, `0xBB67AE85`, `0x3C6EF372`, `0xA54FF53A`... |
| 6 | SHA-384 | 48 bytes | → `sub_FFFFF807C1E3BCCC` |
| 7 | SHA-512 | 64 bytes | → `sub_FFFFF807C1E3BB98` |

**Usage**:
- SHA-256: Module file integrity
- SHA-512: Telemetry authentication tags
- MD5: Fast process memory uniqueness checks
- SHA-1: Legacy game file verification

### 2.5 Encrypted Function Pointer Dispatch

EAC has **no import table**. All kernel API calls resolved via encrypted slots:

**Resolver**: `sub_FFFFF807C1ED4320`

```c
// Encrypted pointer table at 0xFFFFF807C2068E78
typedef struct {
    uint64_t encrypted_ptr;
    uint64_t constant_a;
    uint64_t constant_b;
} EncryptedFnSlot;

// Decryption formula:
// fn_ptr = (slot->encrypted_ptr * constant_a) ^ constant_b

// Example slots:
// Slot 0: PsGetCurrentProcess equivalent
//   A: 0x936ACF702E4281A9, B: 0xFA85638DCFA646E7
// Slot 1: Packet serializer  
//   A: 0xE615DAFE9811D559, B: 0x00A559FABE750D69
// Slot 2: PsGetProcessSessionId
//   A: 0xF3EC14C2131FEE4F, B: 0xBE0DAFCD89B39CD1
// Slot 3: KeQuerySystemTime
//   A: 0xE462A05B3E35A30F, B: 0x7D67C96867B51F90
```

**Key Table Layout**:
```
0xFFFFF807C2068E78  → Slot 0: PsGetCurrentProcess
0xFFFFF807C2068E88  → Slot 1: Packet serializer
0xFFFFF807C2068EC8  → Slot 2: PsGetProcessSessionId
0xFFFFF807C2068EE8  → Slot 3: KeQuerySystemTime
... +0x10 increments ...
```

### 2.6 String Obfuscation

- No plaintext strings in binary
- Globals named `a41`, `a42`, ... `a7e` in IDA
- Rolling XOR cipher with position-dependent key
- API names only appear in RAM during execution

---

## 3. Detection Methods

### 3.1 Process Scanning via EPROCESS

EAC reads `EPROCESS` offsets directly (bypassing user-mode hooks):

| Field | Offset | Purpose |
|-------|--------|---------|
| UniqueProcessId | +56 | Token/PID verification |
| InheritedFromUniqueProcessId | +64 | Parent chain analysis |
| ImageFileName | +96 | 15-char name hash |
| Peb/VadRoot | +240 | Memory map traversal |
| ObjectTable | +376 | Handle table access |
| Protection flags | +556 | PS_PROTECTION validation |

**Function**: `sub_FFFFF807C1E1DD80` (Telemetry assembler)

### 3.2 VAD Tree Inspection

**Virtual Address Descriptor** tree at `EPROCESS+240` — tracks every mapped memory region.

**Targets**:
1. **Manually Mapped PE Headers**: Private memory + executable + no file backing
2. **RWX Memory**: `PAGE_EXECUTE_READWRITE` (0x40) — immediate red flag
3. **Suspicious Sizes**: Tiny executable regions (<4KB) outside module boundaries

**VAD Node Structure**:
```c
// EAC reads at offsets: 60, 64, 52, 461, 41 bytes
// Corresponding to MMVAD_SHORT, UNICODE_STRING paths, binary fingerprints

typedef struct _MMVAD {
    ULONG_PTR StartingVpn;      // >> PAGE_SHIFT for address
    ULONG_PTR EndingVpn;
    union {
        struct {
            ULONG Protection : 5;  // PAGE_EXECUTE_* etc.
            ULONG VadType : 3;     // 0=Private, 1=Mapped, 2=Image
        } VadFlags;
    } u;
    PVOID SubSection;           // File backing (NULL = private)
} MMVAD;
```

**Suspicious Indicators**:
- `VadType == 0` (Private) + `Protection & PAGE_EXECUTE` + `SubSection == NULL`
- `Protection == PAGE_EXECUTE_READWRITE` (0x40)
- Image type but not in PEB module list

### 3.3 Handle Table Walking

EAC walks kernel handle tables directly (not `NtQuerySystemInformation`):

**Suspicious Access Masks**:
| Mask | Value | Trigger |
|------|-------|---------|
| PROCESS_VM_READ | 0x0010 | 🔴 Immediate flag |
| PROCESS_VM_WRITE | 0x0020 | 🔴 Immediate flag |
| PROCESS_VM_OPERATION | 0x0008 | 🔴 Immediate flag |
| PROCESS_ALL_ACCESS | 0x1FFFFF | 🔴 Immediate flag |
| PROCESS_QUERY_INFORMATION | 0x0400 | 🟠 Flag + watchlist |

**Method**: Read `_EPROCESS.ObjectTable` → Walk `HANDLE_TABLE` tree (L1→L2→L3) → Decode `HANDLE_TABLE_ENTRY`:
```c
OBJECT_HEADER* header = (OBJECT_HEADER*)(entry.ObjectPointerBits << 4);
DWORD access = entry.GrantedAccessBits << 2;
```

### 3.4 Thread Monitoring

**Callback**: `PsSetCreateThreadNotifyRoutine`

**Detects**:
- Remote thread injection (`CreateRemoteThread`)
- Thread hijacking (suspicious `RIP` register values)
- Start addresses outside known modules

**Detection Logic**:
```c
VOID ThreadNotifyCallback(HANDLE pid, HANDLE tid, BOOLEAN create) {
    if (!create || pid != gamePID) return;
    
    PVOID startAddr = PsGetThreadStartAddress(thread);
    
    // Check if start address is inside known module
    if (!IsInKnownModule(startAddr)) {
        flag_suspicious_thread(tid, startAddr);
    }
    
    // Classic LoadLibrary injection signature
    if (startAddr == cached_LoadLibraryA || 
        startAddr == cached_LoadLibraryW) {
        flag_loadlibrary_injection(pid, tid);
    }
}
```

### 3.5 Kernel Module Enumeration

**Primary**: Walk `PsLoadedModuleList` (doubly-linked list of `LDR_DATA_TABLE_ENTRY`)

**Per-Module Checks**:
- Digital signature (Authenticode)
- Path on disk (temp folders = suspicious)
- Module name hash (blacklist comparison)
- Dispatch routine pointers (hook detection)

**DKOM Counter-Measure**:
- Scan physical memory pages for `MZ`/`PE` headers
- Compare discovered PE images against `PsLoadedModuleList`
- Unlisted PE images = hidden driver

---

## 4. External Cheat Detection

### 4.1 Handle-Based Detection (Primary Vector)

External cheats must open handles to the game process. EAC enumerates all handles system-wide.

**Legitimate Exceptions** (Whitelist):
- `WerFault.exe` (Windows Error Reporting)
- Antivirus products (path hash matched)
- NVIDIA overlay (`nvcontainer.exe`)
- Steam overlay (`GameOverlayRenderer64.dll`)

**Detection Flow**:
```c
// For every process P:
//   For every handle H in P's handle table:
//     If H.ObjectType == Process && H.Object == GameEPROCESS:
//        Check H.GrantedAccess

if ((access & (PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_ALL_ACCESS)) 
    && ownerProcess != gameProcess) {
    flag_external_cheat(ownerProcess);
}
```

### 4.2 MiLookupSystemHandle

EAC accesses kernel handle table structure directly:
```c
// HANDLE_TABLE structure via EPROCESS.ObjectTable
// TableCode indicates 1/2/3 level table
// Each HANDLE_TABLE_ENTRY contains:
//   - ObjectPointerBits (59-bit encoded)
//   - GrantedAccessBits (25-bit mask)
```

### 4.3 Cross-Process Memory Access Patterns

**Process Creation Callback** (`PsSetCreateProcessNotifyRoutine`):
- Checks process name against blacklist
- Validates parent process (spawning by cheat launcher = flag)
- Monitors if process opens game handle

### 4.4 Window & UI Overlay Detection

**User-Mode Checks** (EAC.exe):
```c
// Suspicious window styles:
#define OVERLAY_STYLE (WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED)

// DWM composition abuse:
DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
// Check for windows positioned exactly over game rect
```

**Detection**:
- `EnumWindows` + `GetWindowLongPtr` for style analysis
- `WS_EX_TOOLWINDOW` (hides from Alt+Tab) = suspicious

### 4.5 Known External Cheat Driver Signatures

Blacklist at `aBin` / `aBin_0` (`0xFFFFF807C1FFEE10`, `0xFFFFF807C1FFEDF0`):

| Driver Type | Bypass Method | Detection |
|-------------|---------------|-----------|
| Physical memory mapper | `\Device\PhysicalMemory` direct read | Module name hash |
| MmCopyMemory wrapper | Kernel `MmCopyMemory` via vulnerable driver | Dispatch hook |
| CR3 mapper | Read target CR3, map pages directly | CPUID/MSR patterns |
| SSDT patcher | Patches `NtReadVirtualMemory` | SSDT hash check |

---

## 5. Internal Cheat & Injection Detection

### 5.1 LoadLibrary Injection Detection

**Vector 1**: Thread creation callback catches `LoadLibraryA`/`W` as start address.

**Vector 2**: Handle inspection catches `PROCESS_CREATE_THREAD | PROCESS_VM_WRITE`.

**Vector 3**: Post-load PEB module list scan checks:
- Is module on disk?
- Is it signed?
- Does `DiskHash == InMemoryHash`?

### 5.2 Manual Mapping Detection

**Manual Map Process**:
1. Allocate RWX memory in target
2. Copy PE headers + sections
3. Apply relocations manually
4. Resolve imports manually
5. Call `DllMain` directly
6. Erase PE headers (optional)

**Detection Methods**:

**VAD Tree Scan (Primary)**:
```c
// Walk VAD tree via EPROCESS+240
if (vad->u.VadFlags.VadType == 0 &&  // Private
    (vad->u.VadFlags.Protection & PAGE_EXECUTE) &&
    vad->SubSection == NULL) {         // No file backing
    flag_manual_map(vad->StartingVpn);
}
```

**PE Header Scan in Private Memory**:
- Section boundaries at page-aligned offsets
- Import table fingerprints (resolved function pointer arrays)
- `.text` section entropy patterns
- Export directory remnants

**RWX Region Flagging**:
- `PAGE_EXECUTE_READWRITE` (0x40) = immediate report
- Legitimate code is `RX` or `RW`, never both

### 5.3 VAD Tree Deep Dive

**Structure Validation**:
```c
ForEachVADNode(gameProcess) {
    CheckProtection();      // RWX = critical
    CheckVadType();         // Private + Exec = suspicious
    CheckSubSection();      // NULL = no file backing
    CheckSize();            // >10MB private exec = flag
    CrossReferencePEB();    // VAD exists but not in PEB = hidden
}
```

**Why Hard to Beat**:
- Removing VAD node requires kernel privileges
- Kernel privileges → EAC driver scanner finds attacker's driver
- Circular dependency for attacker

### 5.4 PEB Module List Integrity

**Three Lists**:
- `InLoadOrderModuleList` (load time)
- `InMemoryOrderModuleList` (memory address)
- `InInitializationOrderModuleList` (init order)

**Cross-Validation**:
```c
// Walk PEB list, verify each entry has corresponding VAD node
// Walk VAD tree, verify each image node has PEB entry
if (PEBEntryExistsButNoVAD()) flag_peb_tampering();
if (VADImageExistsButNoPEB()) flag_hidden_module();
```

---

## 6. Hardware ID & Spoofer Detection

### 6.1 Hardware ID Sources (6 Independent Paths)

| Source | Method | Spoof Difficulty |
|--------|--------|------------------|
| ATA Drive Serial | `IOCTL_ATA_PASS_THROUGH` | Very Hard (firmware reflash) |
| Storage Query Property | `IOCTL_STORAGE_QUERY_PROPERTY` | Hard |
| Volume GUID | Registry + IOCTL | Medium (offline reg edit) |
| Permanent MAC | `OID_802_3_PERMANENT_ADDRESS` | Hard (EEPROM write) |
| GPU PnP Instance ID | `IoGetDeviceProperty` | Medium (registry) |
| SMBIOS Firmware | `SystemFirmwareTableInformation` | Very Hard (BIOS reflash) |

### 6.2 Cross-Source Comparison Logic

```c
composite_id = hash(
    ata_serial,
    storage_query_serial,
    volume_guid,
    permanent_mac_1,
    permanent_mac_2,
    gpu_instance_id,
    smbios_system_uuid,
    smbios_board_serial
);

// Any inconsistency between sources = spoofing detected
if (ata_serial != storage_query_serial) {
    flag_spoofing_attempt();
}
```

### 6.3 Spoofer Driver Detection

**Why Spoofers Need Drivers**:
- Must intercept kernel-mode IOCTLs
- Modern Windows requires WHQL cert (revoked if leaked)
- Alternative: BYOVD (Bring Your Own Vulnerable Driver)

**EAC Checks**:
1. DSE status (`g_CiOptions` != 0x6 = suspicious)
2. Signature verification on all drivers
3. Known-vulnerable driver list (ene.sys, dbutil_2_3.sys, etc.)

### 6.4 Storage Driver Hook Detection

```c
// Check disk.sys MajorFunction table
PDRIVER_OBJECT disk = get_driver_object(L"\\Driver\\Disk");
for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
    if (!is_in_module_range(disk->MajorFunction[i], 
                           disk->DriverStart, 
                           disk->DriverSize)) {
        report_storage_hook(i, disk->MajorFunction[i]);
    }
}
```

### 6.5 Timing Anomaly Detection

```c
t0 = KUSER_SHARED_DATA.TickCountLow;
result = query_disk_serial();
t1 = KUSER_SHARED_DATA.TickCountLow;
latency = t1 - t0;

// Expected: 1-10ms (hardware)
// Hook intercept: 50-500ms (additional processing)
// Cached/static: <0.1ms (too fast)
// All three scenarios flagged
```

### 6.6 VM Detection Matrix

| Signal | Suspicious Value |
|--------|------------------|
| System uptime | < 60 seconds |
| CPUID hypervisor bit | Bit 31 of ECX set |
| Hypervisor vendor | VMware, VirtualBox, QEMU |
| SMBIOS manufacturer | VMware, VBOX, QEMU, Microsoft Corporation |
| Disk model | VBOX HARDDISK, VMware, QEMU HARDDISK |
| MAC OUI | 00:0C:29 (VMware), 08:00:27 (VirtualBox) |
| PCI IDs | VMware SVGA II (0x0405), VirtualBox GA (0xBEEF) |

---

## 7. Telemetry System

### 7.1 Pipeline Overview

```
EPROCESS Structures
        ↓
Telemetry Assembler (sub_FFFFF807C1E1DD80)
        ↓
Raw Binary Packet (184+ bytes)
        ↓
XOR Obfuscation (0x90 key on selected fields)
        ↓
Zstd Compression (60-80% reduction)
        ↓
P-256 ECDSA Signature (64 bytes appended)
        ↓
User-Mode Relay (IOCTL)
        ↓
HTTPS POST to *.easyanticheat.net
```

### 7.2 Telemetry Assembler Function

**Function**: `sub_FFFFF807C1E1DD80` (size: 0x844)

**Process**:
```c
// 1. Validate EPROCESS
if (!a1) return;

// 2. Decrypt PsGetCurrentProcess, compare tokens
uint64_t v3 = DecryptFnPtr(&unk_FFFFF807C2068E78);
if (ResolveAndCall(v3) != *(EPROCESS**)(a1 + 56)) return;

// 3. Collect protection flags & session ID
int sessionId = DecryptAndCall(PsGetProcessSessionId);

// 4. Initialize 184-byte packet buffer
InitPacketBuffer(&packet);

// 5. Collect timestamps
uint64_t tickCount = *(uint64_t*)0xFFFFF78000000014;  // KUSER_SHARED_DATA
uint64_t systemTime = DecryptAndCall(KeQuerySystemTime);

// 6. Serialize fields
WriteField(packet, 0x20087C0, EPROCESS+556, 4);  // Protection flags
WriteField(packet, 0x20086F8, &systemTime, 8);   // System time
WriteField(packet, 0x20086C0, &tickCount, 8);    // Tick count
WriteField(packet, 0x2008688, &sessionId, 4);    // Session ID
WriteField(packet, 0x2008658, &imageHash, 4);    // Image name hash
WriteField(packet, 0x2008628, EPROCESS+240, 8);  // VAD/PEB pointer
```

### 7.3 Packet Structure (184 bytes)

| Offset | Size | Field | Source |
|--------|------|-------|--------|
| 0x00 | 4 | Packet version | Hardcoded |
| 0x04 | 4 | Protection flags | EPROCESS+556 |
| 0x08 | 4 | EPROCESS field [0] | EPROCESS+0 |
| 0x0C | 4 | EPROCESS field [4] | EPROCESS+4 |
| 0x10 | 4 | Parent PID | EPROCESS+64 |
| 0x14 | 4 | Image name hash | EPROCESS+96 (hashed) |
| 0x18 | 8 | System time | KeQuerySystemTime |
| 0x20 | 8 | Tick count | KUSER_SHARED_DATA+0x14 |
| 0x28 | 4 | Session ID | PsGetProcessSessionId |
| 0x30 | 8 | VAD root / PEB | EPROCESS+240 |
| 0x38 | 4 | Module count | Module walk |
| 0x3C | 32 | Module bases 1-4 | Module list entry[0..3] |
| 0x78 | 461 | Module full path | UNICODE_STRING (461 bytes) |
| 0x... | 41 | Binary fingerprint | Custom 41-byte hash |

### 7.4 XOR Obfuscation Layer

```c
// Applied to linked-list node data
*((BYTE*)&decoded + i) = *(BYTE*)(node + 4*i + 28) ^ 0x90;

// Purpose: Tamper-evidence
// If driver patched to skip XOR, server detects invalid telemetry
```

### 7.5 Compression & Encryption

```c
// Zstd compression
Zstd_BuildFreqTable();      // sub_FFFFF807C1E11C00
Zstd_Huffman_AVX2_4stream(); // sub_FFFFF807C1E13100

// P-256 ECDSA signing
P256_ScalarMul();           // sub_FFFFF807C1E226E0

// Output: [compressed_payload | 64_byte_signature]
```

### 7.6 Reporting Frequency

| Event | Interval |
|-------|----------|
| Heartbeat | ~5 seconds |
| Suspicious Event | Immediate |
| Full System Scan | ~30 seconds |
| Game Start | Comprehensive |
| Periodic Callback | Timer DPC |

---

## 8. User-Mode Component (Ring 3)

### 8.1 Two-Layer Architecture

```
Game.exe
    ↓ (EAC SDK)
GameModule.dll
    ↓ (DeviceIoControl)
EasyAntiCheat.exe (or EasyAntiCheat_EOS.exe)
    ↓ (HTTPS)
EAC Backend Servers
```

**Executables**:
- `EasyAntiCheat.exe`: Legacy (pre-EOS)
- `EasyAntiCheat_EOS.exe`: Epic Online Services (post-2021)
- `EasyAntiCheat_Launcher.exe`: Wrapper

### 8.2 Startup Sequence

1. Game starts, loads GameModule.dll
2. SDK spawns EasyAntiCheat.exe child process
3. EAC.exe verifies own signature
4. Loads kernel driver (`NtLoadDriver`)
5. Opens device (`CreateFile`)
6. Handshake IOCTL (Game ID, version, nonce)
7. Registers game PID
8. Starts heartbeat loop (~5s)

### 8.3 Ring3 → Ring0 Communication

**Method**: `DeviceIoControl` with `METHOD_BUFFERED`
- Input: Copied from user to kernel pool
- Output: Kernel writes to pool, copied back to user
- Prevents direct pointer passing

### 8.4 User-Mode Anti-Debug

| Check | Method | Bypass |
|-------|--------|--------|
| Debugger present | `IsDebuggerPresent()` | Patch PEB.BeingDebugged |
| Debug port | `NtQueryInformationProcess(ProcessDebugPort)` | Return 0 |
| Heap flags | `NtGlobalFlag` (PEB+0x100) | Clear flags |
| Heap validation | `GetProcessHeap() + 0x70` | Patch flags |
| Timing | `__rdtsc()` + `NtDelayExecution` | Hypervisor intercept |

### 8.5 Authenticode Verification

**Function**: `sub_FFFFF807C1EAD280` (size: 0x264A / ~9.8KB)

Full X.509/DER parser in kernel:
1. Validates PE magic (`MZ` / `0x5A4D`)
2. Checks `IMAGE_NT_SIGNATURE` (`PE\0\0` / `0x4550`)
3. Validates optional header magic (0x10B=PE32, 0x20B=PE32+)
4. Walks certificate data directory (index 4)
5. Parses WIN_CERTIFICATE (revision 0x0200, type 0x0002)
6. ASN.1/DER decode PKCS#7 SignedData
7. Verifies chain to Microsoft Root CA

**Why In-Kernel**: Prevents `WinVerifyTrust` hooking; no user-mode interception point.

---

## 9. User-Mode Techniques & Gaps

### 9.1 Detection Status Key

| Badge | Meaning |
|-------|---------|
| 🟢 | Undetected — no driver vector |
| 🟡 | Server-side only — behavioral detection possible |
| 🟠 | Partially monitored — low risk with care |

### 9.2 KUSER_SHARED_DATA Timing

**Address**: `0x7FFE0000` (user-mode), `0xFFFFF78000000000` (kernel mirror)

```c
#define KUSER_SHARED_DATA 0x7FFE0000

// Tick count (updates ~15ms)
volatile ULONG* TickCount = (ULONG*)(KUSER_SHARED_DATA + 0x320);

// System time (100ns since 1601)
volatile LARGE_INTEGER* SystemTime = (LARGE_INTEGER*)(KUSER_SHARED_DATA + 0x14);

// Interrupt time (100ns since boot)
volatile LARGE_INTEGER* InterruptTime = (LARGE_INTEGER*)(KUSER_SHARED_DATA + 0x08);
```

**Use**: High-res timing, frame sync, load detection.

### 9.3 Game Base Address Without VM_READ

```c
// PROCESS_QUERY_LIMITED_INFORMATION (0x1000) — not flagged by EAC
HANDLE hGame = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

PROCESS_BASIC_INFORMATION pbi;
NtQueryInformationProcess(hGame, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);
// pbi.PebBaseAddress → game PEB address

// Also available:
// ProcessImageFileName (exe path)
// ProcessCommandLine (launch args)
// ProcessTimes (CPU usage)
```

### 9.4 ETW (Event Tracing for Windows)

**Passive subscription** — no detection vector.

| Provider | Data |
|----------|------|
| `DxgKrnl` | Frame start/end, GPU queue |
| `Win32k` | Input events |
| `Kernel-Process` | DLL loads |
| `DXGI` | SwapChain Present() |
| `Heap` | Large allocations |

### 9.5 RawInput Interception

```c
RAWINPUTDEVICE rid = {0};
rid.usUsagePage = 0x01;  // Generic Desktop
rid.usUsage = 0x02;      // Mouse (0x06 for keyboard)
rid.dwFlags = RIDEV_INPUTSINK;  // Receive without focus
rid.hwndTarget = hwnd;

RegisterRawInputDevices(&rid, 1, sizeof(rid));
// WM_INPUT receives all mouse/keyboard events
```

**Advantage**: Not a hook (WH_KEYBOARD_LL), not monitored by EAC.

---

## 10. Vulnerabilities & Detection Gaps

### 10.1 Severity Legend

| Rating | Meaning |
|--------|---------|
| 🔴 Critical | Complete bypass, practical exploit |
| 🟠 High | Specific layer bypass, requires skill |
| 🟡 Medium | Partial evasion |
| 🟢 Low | Theoretical/exotic hardware |

### 10.2 External Cheat Gaps

#### 🔴 GAP-EXT-01: DMA Hardware Attacks
**Issue**: PCIe device reads physical RAM directly; no software on victim machine.

**Why Missed**: No driver, no handle, no network footprint. EAC has no anti-DMA enumeration.

**Mitigation**: Server-side behavior analysis.

#### 🔴 GAP-EXT-02: Kernel Memory Read (No Handle)
**Issue**: `MmCopyMemory` with EPROCESS pointer requires no handle entry.

**Why Missed**: Handle scanner finds nothing; if driver uses DKOM (unlinked from module list), invisible to module scanner.

**Mitigation**: Physical memory scan for hidden PE images.

#### 🟠 GAP-EXT-03: Handle Pre-Open
**Issue**: Open handle before EAC initializes, close after reading. EAC scan misses window.

**Difficulty**: Medium (requires precise timing).

### 10.3 Internal Cheat Gaps

#### 🔴 GAP-INJ-01: VAD Node DKOM
**Issue**: Kernel driver manipulates VAD tree directly — unlinks injected region or changes type to "mapped image".

**Difficulty**: Very high (BSOD risk), but demonstrated possible.

**Impact**: Defeats primary internal detection mechanism.

#### 🟠 GAP-INJ-02: Early Injection Window
**Issue**: Inject between Windows loader mapping and EAC first scan. If injected DLL is legitimate signed binary with secondary payload, initial scan passes.

#### 🟠 GAP-INJ-03: Thread Pool Hijacking
**Issue**: Queue work to existing thread pool thread (whitelisted start address in ntdll). EAC thread creation callback never fires.

**Mitigation**: Periodic thread IP scanning (polling gap exists).

### 10.4 Spoofer Gaps

#### 🔴 GAP-SPF-01: DMA Firmware Reprogramming
**Issue**: Physically reflash storage firmware to return custom serial via ATA IDENTIFY.

**Why Missed**: EAC trusts ATA response; no firmware integrity check.

#### 🟠 GAP-SPF-02: SMBIOS Hypervisor Emulation
**Issue**: Type-1 hypervisor intercepts `SystemFirmwareTableInformation` and `MmMapIoSpace` via EPT.

**Difficulty**: Requires custom hypervisor setup (HVPP, SimpleSvm).

### 10.5 User-Mode Gaps

#### 🔴 GAP-UM-01: Ring3 Anti-Debug Trivially Bypassed
**Issue**: All user-mode anti-debug checks patchable (`PEB.BeingDebugged`, `NtGlobalFlag`, etc.).

**Impact**: EAC.exe freely debuggable for IOCTL protocol analysis.

#### 🟠 GAP-UM-02: Process Suspension Within Heartbeat Window
**Issue**: Hypervisor pauses EAC for 4.99s (heartbeat is 5s), performs scan, resumes before deadline.

### 10.6 Kernel Architecture Gaps

#### 🟠 GAP-KRN-01: PatchGuard Timing Window
**Issue**: PatchGuard validates every 3-10 minutes. Between validations, kernel structures modifiable.

**Attack**: Modify VAD between EAC scan and PatchGuard validation, restore after.

#### 🟡 GAP-KRN-02: Static API Constants
**Issue**: Encrypted dispatch constants (e.g., `0x936ACF702E4281A9`) are compile-time static per build.

**Impact**: Once extracted from one build, API mapping known until next rebuild.

---

## 11. Function Reference

### 11.1 Cryptographic Functions

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E21280` | 0x409 | `P256_FieldMul` | 9-limb × 9-limb polynomial multiply |
| `0xFFFFF807C1E2168C` | 0x26C | `P256_FieldReduce` | Modular reduction to P-256 field |
| `0xFFFFF807C1E21900` | 0x91 | `P256_PointAdd_Prep` | Jacobian coordinate conversion |
| `0xFFFFF807C1E21994` | 0x137 | `P256_FieldAdd` | Field addition with carry |
| `0xFFFFF807C1E21ACC` | 0x175 | `P256_FieldSub` | Field subtraction with borrow |
| `0xFFFFF807C1E21C44` | 0x175 | `P256_FieldNeg` | Field negation (p - x) |
| `0xFFFFF807C1E21DC0` | 0x81 | `P256_FieldSqr` | Field squaring (optimized) |
| `0xFFFFF807C1E21E60` | 0x15B | `P256_PointDouble` | Jacobian point doubling |
| `0xFFFFF807C1E21FC0` | 0x17E | `P256_ConvertToAffine` | Jacobian to affine conversion |
| `0xFFFFF807C1E22140` | 0x1F2 | `P256_PointAdd` | Full Jacobian point addition |
| `0xFFFFF807C1E22340` | 0x1D0 | `P256_ConditionalSwap` | Constant-time conditional swap |
| `0xFFFFF807C1E22520` | 0x17E | `P256_FieldInvert` | Field inversion (Fermat's theorem) |
| `0xFFFFF807C1E226A0` | 0x35 | `P256_ScalarMul_Entry` | Scalar multiplication dispatch |
| `0xFFFFF807C1E226E0` | 0x31D | `P256_ScalarMul` | 2-bit NAF constant-time multiply |
| `0xFFFFF807C1E1AF00` | 0x5EB | `NTT_MontgomeryReduce` | NTT with Montgomery reduction |
| `0xFFFFF807C1E1AB00` | 0x3E5 | `NTT_Butterfly` | Cooley-Tukey butterfly |
| `0xFFFFF807C1E1AA80` | 0x68 | `NTT_BitReverse` | Bit-reversal permutation |
| `0xFFFFF807C1E1A7C0` | 0x88 | `NTT_Finalize` | Post-NTT normalization |
| `0xFFFFF807C1E1A850` | 0x149 | `NTT_Setup` | Twiddle factor computation |
| `0xFFFFF807C1E1A640` | 0x17D | `ECDSA_Sign` | ECDSA signing |
| `0xFFFFF807C1E1A490` | 0x1A8 | `ECDSA_Verify` | ECDSA verification |

### 11.2 Compression Functions

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E11C00` | 0x2D1 | `Zstd_BuildFreqTable` | Huffman frequency builder |
| `0xFFFFF807C1E11EE0` | 0x579 | `Zstd_Huffman_SSE2` | SSE2 2-stream Huffman |
| `0xFFFFF807C1E12460` | 0x54D | `Zstd_Huffman_AVX2_v1` | AVX2 2-stream Huffman |
| `0xFFFFF807C1E13100` | 0x577 | `Zstd_Huffman_AVX2_4stream` | AVX2 6-stream (15 bytes/iter) |
| `0xFFFFF807C1E129C0` | 0x23A | `Zstd_BlockDecompress` | Block decompression |
| `0xFFFFF807C1E12C00` | 0x2E9 | `Zstd_SequenceDecode` | Literal + match decode |
| `0xFFFFF807C1E12F00` | 0x1FA | `Zstd_FSE_Decode` | Finite State Entropy decode |
| `0xFFFFF807C1E13680` | 0x2DC | `Zstd_BuildHuffmanTree` | Huffman tree construction |
| `0xFFFFF807C1E13960` | 0x28B | `Zstd_AssignCodeLengths` | Canonical code length assignment |
| `0xFFFFF807C1E13C00` | 0x274 | `Zstd_FSE_BuildTable` | FSE table construction |
| `0xFFFFF807C1E13E80` | 0x256 | `Zstd_CompressBlock` | Block compression |
| `0xFFFFF807C1E140E0` | 0x255 | `Zstd_CompressLiterals` | Literal compression |
| `0xFFFFF807C1E30B00` | 0x10C | `Zstd_HeapSiftDown` | Min-heap sift-down |
| `0xFFFFF807C1E30C20` | 0x30F | `Zstd_Quicksort` | Introsort implementation |

### 11.3 Hash Functions

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E3A4C0` | 0x11A | `HashContext_Init` | Algorithm selector |
| `0xFFFFF807C1E3BB98` | varies | `SHA512_Init` | SHA-512 initialization |
| `0xFFFFF807C1E3BCCC` | varies | `SHA384_Init` | SHA-384 initialization |
| `0xFFFFF807C1E3A568` | varies | `SHA256_BlockProcess` | SHA-256 compression |
| `0xFFFFF807C1E8D840` | varies | `HashImageName` | Process name hashing |
| `0xFFFFF807C1E28DA0` | 0xC3 | `VerifyModuleSignature` | Authenticode verification |
| `0xFFFFF807C1E29540` | 0x3B | `CheckSignatureResult` | Signature validation |
| `0xFFFFF807C1E2A4E0` | 0xCF | `ComputeModuleFingerprint` | 41-byte fingerprint |

### 11.4 Telemetry & Detection

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E1DD80` | 0x844 | `AssembleTelemetryPacket` | Main telemetry builder |
| `0xFFFFF807C1E1E5C4` | 0xA4 | `InitPacketBuffer` | Packet initialization |
| `0xFFFFF807C1E1E668` | varies | `SerializeField_*` | Field serialization (multiple) |
| `0xFFFFF807C1ED4320` | varies | `DecryptFnPtr` | Encrypted pointer resolver |
| `0xFFFFF807C1EBF800` | varies | `SafeStructRead` | Safe kernel memory read |
| `0xFFFFF807C1E17CF0` | 0xF2 | `ModuleEnum_IterNext` | Module list iterator |
| `0xFFFFF807C1E173E0` | 0xFB | `ProcessEnum_IterNext` | Process list iterator |
| `0xFFFFF807C1E16EF0` | 0xF0 | `DispatchTable_Check` | Driver dispatch validation |
| `0xFFFFF807C1E16FE0` | 0xF7 | `SSDT_Validate` | SSDT hook detection |
| `0xFFFFF807C1E18190` | 0x4E | `VAD_WalkNext` | VAD tree traversal |

---

## 12. Global Data Structures

### 12.1 Encrypted Function Pointer Table

**Address**: `0xFFFFF807C2068E78`

```c
// Each slot: 24 bytes (encrypted_ptr + constant_a + constant_b)
// Total slots: ~50+ (all kernel API calls)

struct EncryptedFnSlot {
    uint64_t encrypted_ptr;
    uint64_t constant_a;  // Multiplier
    uint64_t constant_b;  // XOR constant
};
```

### 12.2 String Obfuscation Globals

**Names**: `a41`, `a42`, ... `a7e` in IDA

```c
// Rolling XOR cipher
// Key derived from string position
// Decoded at runtime only
```

### 12.3 Driver State Block

**Address**: Global pointer (encrypted)

```c
struct EAC_DriverState {
    uint64_t canary;              // 0xBC44A31CA74B4AAF
    PEPROCESS gameProcess;        // Protected game
    DWORD gamePID;                // Game process ID
    uint64_t telemetryBuffer;     // Telemetry assembly buffer
    uint64_t cryptoContext;       // P-256 context
    uint64_t hwidCache;           // Hardware ID cache
};
```

---

## Validation Notes

### Research Methodology

This analysis was performed through:
1. **Static Reverse Engineering**: IDA Pro analysis of EasyAntiCheat.sys binary
2. **Dynamic Analysis**: WinDbg kernel debugging with live game sessions
3. **Cross-Reference Validation**: Comparison with academic papers and security research
4. **Web Research**: Verified against multiple independent sources (UnknownCheats, secret.club, ACM papers)

### Confidence Levels

| Component | Confidence | Validation Method |
|-----------|-----------|-------------------|
| P-256 Implementation | High | Matches NIST spec, verified constants |
| VAD Tree Scanning | High | Multiple independent confirmations |
| Handle Table Detection | High | Well-documented in research |
| Telemetry Structure | Medium-High | Reverse engineered, partially verified |
| Cryptographic Keys | Medium | Extracted from binary, not externally verified |
| Function Addresses | Medium | Build-specific, may vary |

### Build Variability

**Important**: Function addresses and offsets are **build-specific**. The analyzed build represents a snapshot from early 2025. EAC updates frequently (weekly patches), so:
- Function addresses will change
- Encryption constants may rotate
- Detection logic evolves
- New anti-bypass measures added

Always verify against the current build before implementing bypasses.

---

## References

### Primary Research Sources

1. **"If It Looks Like a Rootkit and Deceives Like a Rootkit"** (ACM 2024)
   - URL: https://dl.acm.org/doi/fullHtml/10.1145/3664476.3670433
   - Peer-reviewed analysis of EAC kernel driver hook detection

2. **Hypercall - "Inside anti-cheat: EasyAntiCheat - Part 1"**
   - URL: https://hypercall.net/posts/EasyAntiCheat-Part1/
   - Detailed IOCTL and kernel driver analysis

3. **UnknownCheats - "Analysing EasyAntiCheat's Cryptographic Protocol"**
   - URL: https://www.unknowncheats.me/forum/anti-cheat-bypass/738562-analysing-easyanticheats-cryptographic-protocol.html
   - Community reverse engineering of P-256 implementation

4. **Secret.club - "CVEAC-2020: Bypassing EasyAntiCheat integrity checks"**
   - URL: https://secret.club/2020/04/08/eac_integrity_check_bypass.html
   - Historical analysis of EAC integrity check vulnerabilities

### Hardware & Architecture References

5. **Intel® 64 and IA-32 Architectures Software Developer's Manual**
   - Volume 3A: System Programming Guide
   - Page tables, CR3, PTE structure

6. **Microsoft Windows Driver Documentation**
   - URL: https://learn.microsoft.com/en-us/windows-hardware/drivers/
   - EPROCESS, VAD, handle table structures

### Community Resources

7. **UnknownCheats Kernel Subforum**
   - URL: https://www.unknowncheats.me/forum/anti-cheat-bypass/
   - Active development and discussion

8. **GitHub - girther9399-gif/EAC-REVERSED-WRITEUP**
   - URL: https://github.com/girther9399-gif/EAC-REVERSED-WRITEUP
   - Educational reverse engineering writeup

---

**Document Version**: 1.0  
**Last Updated**: March 2026  
**Classification**: Educational/Research Purpose Only
