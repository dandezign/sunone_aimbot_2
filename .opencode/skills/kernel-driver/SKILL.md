# Kernel Driver Development for Anti-Cheat Bypass

## Overview

This skill provides comprehensive expertise in Windows kernel driver development with a focus on anti-cheat evasion techniques, specifically targeting **Easy Anti-Cheat (EAC)** systems. It covers EAC's internal architecture, cryptography, detection methods, vulnerabilities, and complete bypass strategies including manual mapping, stealth mechanisms, physical memory manipulation, and detection bypass.

**Research Date**: March 2026  
**EAC Driver Build Analyzed**: EasyAntiCheat.sys (~8 MB binary)  
**Base Address**: `0xFFFFF807C1E10000` (analyzed build)  
**Target Platforms**: Windows 10/11 x64 (22H2 through 24H2)  
**Primary Anti-Cheats**: EAC (Easy Anti-Cheat), BattlEye, Riot Vanguard, FACEIT

---

## Table of Contents

1. [EAC Architecture Overview](#1-eac-architecture-overview)
2. [EAC Cryptography & Obfuscation](#2-eac-cryptography--obfuscation)
3. [EAC Detection Methods](#3-eac-detection-methods)
4. [External Cheat Detection](#4-external-cheat-detection)
5. [Internal Cheat & Injection Detection](#5-internal-cheat--injection-detection)
6. [Hardware ID & Spoofer Detection](#6-hardware-id--spoofer-detection)
7. [EAC Telemetry System](#7-eac-telemetry-system)
8. [Driver Architecture](#8-driver-architecture)
9. [Stealth Mechanisms](#9-stealth-mechanisms)
10. [Physical Memory R/W](#10-physical-memory-rw)
11. [Detection Vectors and Bypasses](#11-detection-vectors-and-bypasses)
12. [EAC Vulnerabilities & Exploitation Gaps](#12-eac-vulnerabilities--exploitation-gaps)
13. [Function Reference](#13-function-reference)
14. [Build Requirements](#14-build-requirements)
15. [References](#15-references)

---

## 1. EAC Architecture Overview

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

## 2. EAC Cryptography & Obfuscation

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

### 2.2 Zstd Compression Engine

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

### 2.3 Hash Algorithm Suite

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

### 2.4 Encrypted Function Pointer Dispatch

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

### 2.5 String Obfuscation

- No plaintext strings in binary
- Globals named `a41`, `a42`, ... `a7e` in IDA
- Rolling XOR cipher with position-dependent key
- API names only appear in RAM during execution

---

## 3. EAC Detection Methods

### 3.1 Process Scanning via EPROCESS

EAC reads `EPROCESS` offsets directly (bypassing user-mode hooks):

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

### 4.3 Window & UI Overlay Detection

**User-Mode Checks** (EAC.exe):
```c
// Suspicious window styles:
#define OVERLAY_STYLE (WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED)

// DWM composition abuse:
DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
// Check for windows positioned exactly over game rect
```

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

**VAD Tree Scan (Primary)**:
```c
// Walk VAD tree via EPROCESS+240
if (vad->u.VadFlags.VadType == 0 &&  // Private
    (vad->u.VadFlags.Protection & PAGE_EXECUTE) &&
    vad->SubSection == NULL) {         // No file backing
    flag_manual_map(vad->StartingVpn);
}
```

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

### 6.4 VM Detection Matrix

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

## 7. EAC Telemetry System

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
```

### 7.3 Packet Structure (184 bytes)

| Offset | Size | Field | Source |
|--------|------|-------|--------|
| 0x00 | 4 | Packet version | Hardcoded |
| 0x04 | 4 | Protection flags | EPROCESS+556 |
| 0x10 | 4 | Parent PID | EPROCESS+64 |
| 0x14 | 4 | Image name hash | EPROCESS+96 (hashed) |
| 0x18 | 8 | System time | KeQuerySystemTime |
| 0x20 | 8 | Tick count | KUSER_SHARED_DATA+0x14 |
| 0x28 | 4 | Session ID | PsGetProcessSessionId |
| 0x30 | 8 | VAD root / PEB | EPROCESS+240 |
| 0x38 | 4 | Module count | Module walk |

### 7.4 Reporting Frequency

| Event | Interval |
|-------|----------|
| Heartbeat | ~5 seconds |
| Suspicious Event | Immediate |
| Full System Scan | ~30 seconds |
| Game Start | Comprehensive |
| Periodic Callback | Timer DPC |

---

## 8. Driver Architecture

### 8.1 File Structure

```
driver/
├── driver.cpp          # DriverEntry, initialization sequence
├── driver.h            # Core includes and definitions
├── definitions.h       # Kernel imports, undocumented structures
├── shared.h            # User-kernel protocol (REQUEST_DATA)
├── hook.cpp            # PTE hook installation and handler
├── hook.h              # Hook function prototypes
├── pte_hook.h          # PTE manipulation engine
├── physical_memory.h   # CR3 caching, page table walking
├── spoof_call.h        # Return address spoofing
├── cleaner.h           # Trace removal (PiDDB, MmUnloadedDrivers)
├── memory.cpp          # Virtual memory operations (fallback)
└── memory.h            # Memory function prototypes
```

### 8.2 Initialization Sequence

**CRITICAL ORDER** (verified against EAC 2025 detection patterns):

```cpp
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    // STEP 1: Install communication hook FIRST
    // - Needs dxgkrnl exports to be resolvable
    // - Must complete before any trace cleaning
    if (!Hook::Install(&Hook::Handler)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                   "[!] Failed to install hook\n");
        return STATUS_UNSUCCESSFUL;
    }
    
    // STEP 2: Initialize spoofing infrastructure
    // - Find gadget in ntoskrnl BEFORE cleaning removes symbols
    InitSpoofCall();
    
    // STEP 3: Clean ALL traces
    // - PiDDB: Remove iqvw64e.sys load record
    // - MmUnloadedDrivers: Remove unload record
    // - DriverObject: Unlink and NULL fields
    CleanAllTraces(DriverObject);
    
    return STATUS_SUCCESS;
}
```

---

## 9. Stealth Mechanisms

### 9.1 PTE Hooking (Page Table Entry)

**Why PTE over inline**: PatchGuard scans physical memory for code modifications. PTE hook keeps original physical page clean.

**Installation Process**:

```cpp
// Step 1: Find MiGetPteAddress in ntoskrnl
// Pattern (31 bytes):
BYTE pattern[] = {
    0x48, 0xC1, 0xE9, 0x09,              // shr rcx, 9
    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <MASK>
    0x48, 0x23, 0xC8,                    // and rcx, rax
    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <PTE_BASE>
    0x48, 0x03, 0xC1,                    // add rax, rcx
    0xC3                                 // ret
};

PVOID MiGetPteAddress = FindPatternInModule("ntoskrnl.exe", pattern, sizeof(pattern));

// Step 2: Check for large page (2MB)
PPDE pde = GetPdeAddress(hookTarget);
if (pde->LargePage) {
    SplitLargePage(pde);  // Convert to 512 × 4KB pages
}

// Step 3: Allocate new physical page
PPTE pte = ((PMiGetPteAddress)(hookTarget));
PHYSICAL_ADDRESS newPagePA = MmAllocateContiguousMemorySpecifyCache(
    PAGE_SIZE, LowAddress, HighAddress, LowAddress, MmCached);

// Step 4: Copy original page
PVOID originalPageVA = MmMapIoSpace(pte->PageFrameNumber * PAGE_SIZE, PAGE_SIZE, MmCached);
RtlCopyMemory(newPageVA, originalPageVA, PAGE_SIZE);
MmUnmapIoSpace(originalPageVA, PAGE_SIZE);

// Step 5: Apply trampoline at offset
BYTE trampoline[] = {
    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, handler
    0xFF, 0xE0  // jmp rax
};
*(PVOID*)&trampoline[2] = handlerAddress;
RtlCopyMemory((PVOID)((ULONG64)newPageVA + offset), trampoline, sizeof(trampoline));

// Step 6: Atomically swap PFN
ULONG64 newPFN = newPagePA.QuadPart >> PAGE_SHIFT;
InterlockedExchange64(&pte->PageFrameNumber, newPFN);
__invlpg(hookTarget);  // Invalidate TLB entry
```

### 9.2 Trace Cleaning

#### PiDDB Cache Table

**What it is**: Kernel Plug and Play Database caches every driver ever loaded.

**Patterns**:
```
PiDDBLockPtr:  48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? 33 DB
PiDDBTablePtr: 48 8D 0D ? ? ? ? E8 ? ? ? ? 3D ? ? ? ? 0F 83 ? ? ? ?
```

**Cleaning Code**:
```cpp
typedef struct PiDDBCacheEntry {
    LIST_ENTRY      List;
    UNICODE_STRING  DriverName;
    ULONG           TimeDateStamp;
    NTSTATUS        LoadStatus;
    char            _pad[16];
} PIDCacheobj;

void CleanPiDDBCache() {
    // Scan for PiDDBLock and PiDDBCacheTable
    PVOID piDDBLockPtr = FindPatternInKernel(
        "\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\x0D\x00\x00\x00\x00\x33\xDB",
        "xxx????x???xxxx????x");
    
    PVOID piDDBTablePtr = FindPatternInKernel(
        "\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x3D\x00\x00\x00\x00\x0F\x83\x00\x00\x00\x00",
        "xxx????x???xx????xx???");
    
    // Resolve RIP-relative addresses
    PEX_RESOURCE PiDDBLock = ResolveRipRelative(piDDBLockPtr);
    PRTL_AVL_TABLE PiDDBCacheTable = ResolveRipRelative(piDDBTablePtr);
    
    // Create lookup entry
    PIDCacheobj lookupEntry = {};
    lookupEntry.DriverName.Buffer = L"iqvw64e.sys";
    lookupEntry.DriverName.Length = 22;
    lookupEntry.DriverName.MaximumLength = 24;
    lookupEntry.TimeDateStamp = 0x5284EAC3;  // iqvw64e.sys timestamp
    
    // Search and remove
    ExAcquireResourceExclusiveLite(PiDDBLock, TRUE);
    PIDCacheobj* pFound = (PIDCacheobj*)RtlLookupElementGenericTableAvl(
        PiDDBCacheTable, &lookupEntry);
    
    if (pFound) {
        RemoveEntryList(&pFound->List);
        RtlDeleteElementGenericTableAvl(PiDDBCacheTable, pFound);
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
                   "[+] PiDDB entry removed\n");
    }
    ExReleaseResourceLite(PiDDBLock);
}
```

#### MmUnloadedDrivers

**What it is**: Circular buffer of recently unloaded drivers (typically 50 entries).

**Pattern**:
```
MmUnloadedDrivers: 4C 8B ? ? ? ? ? 4C 8B ? ? 4D 85 ? ? 74 ?
```

**Cleaning Code**:
```cpp
typedef struct _MM_UNLOADED_DRIVER {
    UNICODE_STRING Name;
    PVOID          StartAddress;
    PVOID          EndAddress;
    ULONG          TimeDateStamp;
    ULONG          _pad;
} MM_UNLOADED_DRIVER, *PMM_UNLOADED_DRIVER;

void CleanMmUnloadedDrivers() {
    // Scan for MmUnloadedDrivers
    PVOID mmUnloadedDriversPtr = FindPatternInKernel(
        "\x4C\x8B\x00\x00\x00\x00\x00\x4C\x8B\x00\x00\x4D\x85\x00\x00\x74\x00",
        "xx?????xx?xx??x?");
    
    PMM_UNLOADED_DRIVER MmUnloadedDrivers = 
        *(PMM_UNLOADED_DRIVER*)ResolveRipRelative(mmUnloadedDriversPtr);
    
    // Iterate and clean (typically 50 entries)
    for (ULONG i = 0; i < 50; i++) {
        PMM_UNLOADED_DRIVER entry = &MmUnloadedDrivers[i];
        if (entry->Name.Buffer && 
            wcsstr(entry->Name.Buffer, L"iqvw64e") != NULL) {
            RtlZeroMemory(entry->Name.Buffer, entry->Name.MaximumLength);
            RtlZeroMemory(entry, sizeof(MM_UNLOADED_DRIVER));
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
                       "[+] MmUnloadedDrivers entry %lu cleaned\n", i);
            break;
        }
    }
}
```

#### Driver Object Hiding

```cpp
void HideDriverObject(PDRIVER_OBJECT DriverObject) {
    // Step 1: Clear DriverName buffer
    if (DriverObject->DriverName.Buffer) {
        RtlZeroMemory(DriverObject->DriverName.Buffer, 
                      DriverObject->DriverName.MaximumLength);
    }
    
    // Step 2: Unlink from PsLoadedModuleList
    PLDR_DATA_TABLE_ENTRY entry = (PLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;
    if (entry && entry->InLoadOrderLinks.Flink && entry->InLoadOrderLinks.Blink) {
        PLDR_DATA_TABLE_ENTRY prev = (PLDR_DATA_TABLE_ENTRY)entry->InLoadOrderLinks.Blink;
        PLDR_DATA_TABLE_ENTRY next = (PLDR_DATA_TABLE_ENTRY)entry->InLoadOrderLinks.Flink;
        prev->InLoadOrderLinks.Flink = next;
        next->InLoadOrderLinks.Blink = prev;
    }
    
    // Step 3: Erase PE headers
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)DriverObject->DriverStart;
    if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(
            (ULONG64)DriverObject->DriverStart + dosHeader->e_lfanew);
        if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
            RtlZeroMemory(DriverObject->DriverStart, 
                         ntHeaders->OptionalHeader.SizeOfHeaders);
        }
    }
    
    // Step 4: NULL critical fields
    DriverObject->DriverSection = NULL;
    DriverObject->DriverInit = NULL;
    DriverObject->DriverStart = NULL;
    DriverObject->DriverSize = 0;
    DriverObject->DriverUnload = NULL;
    
    // Step 5: NULL all MajorFunction pointers
    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = NULL;
    }
}
```

### 9.3 Return Address Spoofing

**Problem**: When driver calls kernel APIs, return address points to manually-mapped code (unmapped/non-module memory). EAC stack walks detect this.

**Solution**: Route calls through gadget in ntoskrnl.exe.

**Gadget Pattern**:
```
48 83 C4 28 C3    add rsp, 0x28; ret    (common epilogue)
```

**Shellcode Stub** (52 bytes):
```asm
41 5B                 pop r11              ; save real return address
48 8B C1              mov rax, rcx         ; rax = target function
48 8B CA              mov rcx, rdx         ; shift arg1 → rcx
49 8B D0              mov rdx, r8          ; shift arg2 → rdx
4D 8B C1              mov r8, r9           ; shift arg3 → r8
4C 8B 4C 24 20        mov r9, [rsp+0x20]   ; shift arg4 → r9
48 83 EC 38           sub rsp, 0x38        ; allocate shadow space
49 BA [8-byte gadget] mov r10, <gadget>    ; load gadget address
4C 89 14 24           mov [rsp], r10       ; [rsp] = gadget (fake return)
4C 89 5C 24 30        mov [rsp+0x30], r11  ; [rsp+0x30] = real return
FF E0                 jmp rax              ; jump to target
```

---

## 10. Physical Memory R/W

### 10.1 CR3 Acquisition

**Problem**: `EPROCESS+0x28` (DirectoryTableBase) encrypted/stripped by EAC in recent versions.

**Solution**: Attach to process and read CR3 from hardware register:

```cpp
ULONG64 GetProcessCR3(PEPROCESS process) {
    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);
    ULONG64 cr3 = __readcr3();  // Read from CPU directly
    KeUnstackDetachProcess(&apc);
    return cr3;
}
```

### 10.2 CR3 Caching

EAC may trash CR3 early during process creation. Aggressive caching required:

```cpp
typedef struct _CR3_CACHE_ENTRY {
    volatile ULONG64 cr3;
    volatile HANDLE  pid;
    volatile ULONG   callCount;
    volatile BOOLEAN validated;
} CR3_CACHE_ENTRY;

#define CR3_CACHE_THRESHOLD  500  // Don't trust until 500+ calls

CR3_CACHE_ENTRY g_Cr3Cache[16] = {};

ULONG64 GetCachedCR3(HANDLE pid) {
    // Check cache first
    for (ULONG i = 0; i < 16; i++) {
        if (g_Cr3Cache[i].pid == pid && g_Cr3Cache[i].callCount > CR3_CACHE_THRESHOLD) {
            return g_Cr3Cache[i].cr3;
        }
    }
    
    // Cache miss - acquire and cache
    PEPROCESS process;
    if (NT_SUCCESS(SpoofCall2(PsLookupProcessByProcessId, pid, &process))) {
        ULONG64 cr3 = GetProcessCR3(process);
        
        // Validate: check first PML4 entry is present
        PMML4_ENTRY pml4 = (PMML4_ENTRY)(ULONG64)(cr3 & 0xFFFFFFFFFFF000);
        if (pml4->Present) {
            // Find empty slot
            for (ULONG i = 0; i < 16; i++) {
                if (g_Cr3Cache[i].pid == 0) {
                    g_Cr3Cache[i].pid = pid;
                    g_Cr3Cache[i].cr3 = cr3;
                    g_Cr3Cache[i].validated = TRUE;
                    break;
                }
            }
        }
        
        ObfDereferenceObject(process);
        return cr3;
    }
    return 0;
}
```

### 10.3 4-Level Page Table Walk

```cpp
typedef struct _MML4_ENTRY {
    ULONG64 Present : 1;
    ULONG64 ReadWrite : 1;
    ULONG64 UserSupervisor : 1;
    ULONG64 PageWriteThrough : 1;
    ULONG64 PageCacheDisable : 1;
    ULONG64 Accessed : 1;
    ULONG64 _reserved0 : 1;
    ULONG64 _reserved1 : 1;
    ULONG64 _reserved2 : 4;
    ULONG64 PageFrameNumber : 36;
    ULONG64 _reserved3 : 4;
} MML4_ENTRY, *PMML4_ENTRY;

ULONG64 TranslateVirtualAddress(ULONG64 cr3, ULONG64 virtualAddress) {
    ULONG64 pml4_idx = (virtualAddress >> 39) & 0x1FF;   // Bits 39-47
    ULONG64 pdpt_idx = (virtualAddress >> 30) & 0x1FF;   // Bits 30-38
    ULONG64 pd_idx   = (virtualAddress >> 21) & 0x1FF;   // Bits 21-29
    ULONG64 pt_idx   = (virtualAddress >> 12) & 0x1FF;   // Bits 12-20
    ULONG64 offset   = virtualAddress & 0xFFF;            // Bits 0-11
    
    // Read PML4 entry
    PMML4_ENTRY pml4 = (PMML4_ENTRY)(cr3 & 0xFFFFFFFFFFF000);
    PMML4_ENTRY pml4_entry = &pml4[pml4_idx];
    if (!pml4_entry->Present) return 0;
    
    // Read PDPT entry
    PPDPT_ENTRY pdpt = (PPDPT_ENTRY)(pml4_entry->PageFrameNumber * PAGE_SIZE);
    PPDPT_ENTRY pdpt_entry = &pdpt[pdpt_idx];
    if (!pdpt_entry->Present) return 0;
    
    // Check for 1GB large page
    if (pdpt_entry->LargePage) {
        return (pdpt_entry->PageFrameNumber * PAGE_SIZE) + 
               ((pdpt_idx & 0x1FFFFF) << 30) + (virtualAddress & 0x3FFFFFFF);
    }
    
    // Read PD entry
    PPD_ENTRY pd = (PPD_ENTRY)(pdpt_entry->PageFrameNumber * PAGE_SIZE);
    PPD_ENTRY pd_entry = &pd[pd_idx];
    if (!pd_entry->Present) return 0;
    
    // Check for 2MB large page
    if (pd_entry->LargePage) {
        return (pd_entry->PageFrameNumber * PAGE_SIZE) + 
               ((pd_idx & 0x1FFFFF) << 21) + (virtualAddress & 0x1FFFFF);
    }
    
    // Read PT entry
    PPT_ENTRY pt = (PPT_ENTRY)(pd_entry->PageFrameNumber * PAGE_SIZE);
    PPT_ENTRY pt_entry = &pt[pt_idx];
    if (!pt_entry->Present) return 0;
    
    // 4KB page
    return (pt_entry->PageFrameNumber * PAGE_SIZE) + offset;
}
```

### 10.4 Read Implementation

**CRITICAL**: Never write directly to usermode from `MmCopyMemory`. Use intermediate kernel buffer.

```cpp
NTSTATUS PhysicalReadProcessMemory(HANDLE pid, PVOID virtualAddress, 
                                   PVOID userBuffer, SIZE_T size) {
    ULONG64 cr3 = GetCachedCR3(pid);
    if (!cr3) return STATUS_NOT_FOUND;
    
    // Allocate kernel intermediate buffer
    PVOID kernelBuffer = ExAllocatePoolWithTag(NonPagedPool, size, 'mEmK');
    if (!kernelBuffer) return STATUS_MEMORY_NOT_ALLOCATED;
    
    SIZE_T totalRead = 0;
    PVOID currentVA = virtualAddress;
    PVOID currentBuf = kernelBuffer;
    
    // Loop over page boundaries
    while (totalRead < size) {
        SIZE_T chunkSize = min(size - totalRead, PAGE_SIZE - 
                              ((ULONG64)currentVA & 0xFFF));
        
        ULONG64 physAddr = TranslateVirtualAddress(cr3, (ULONG64)currentVA);
        if (!physAddr) {
            ExFreePoolWithTag(kernelBuffer, 'mEmK');
            return STATUS_PARTIAL_COPY;
        }
        
        SIZE_T bytesRead = 0;
        PHYSICAL_ADDRESS physAddrStruct = {.QuadPart = physAddr};
        
        NTSTATUS status = MmCopyMemory(currentBuf, physAddrStruct, chunkSize, 
                                       MM_COPY_MEMORY_PHYSICAL, &bytesRead);
        if (!NT_SUCCESS(status) || bytesRead == 0) {
            ExFreePoolWithTag(kernelBuffer, 'mEmK');
            return status;
        }
        
        currentVA = (PVOID)((ULONG64)currentVA + bytesRead);
        currentBuf = (PVOID)((ULONG64)currentBuf + bytesRead);
        totalRead += bytesRead;
    }
    
    // Copy kernel buffer → usermode with exception handling
    __try {
        RtlCopyMemory(userBuffer, kernelBuffer, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ExFreePoolWithTag(kernelBuffer, 'mEmK');
        return STATUS_ACCESS_VIOLATION;
    }
    
    ExFreePoolWithTag(kernelBuffer, 'mEmK');
    return STATUS_SUCCESS;
}
```

---

## 11. Detection Vectors and Bypasses

### 11.1 EAC Detection Methods (2025)

| Vector | Detection Method | Bypass |
|--------|------------------|--------|
| **Code Integrity** | Physical memory scan for hooks | PTE hook (original page clean) |
| **Stack Walking** | Return address in non-module memory | Return address spoofing via gadget |
| **MmCopyVirtualMemory** | API call monitoring | Physical memory R/W via CR3 |
| **PiDDB Cache** | Driver load history | Clean on init |
| **MmUnloadedDrivers** | Driver unload history | Clean on init |
| **Driver Enumeration** | PsLoadedModuleList walk | Unlink + NULL fields |
| **PE Header Analysis** | Memory scan for valid PE | Zero headers after load |
| **CR3 Monitoring** | EPROCESS DirTableBase strip | `__readcr3()` via attach |
| **Large Page Detection** | PDE LargePage bit check | Split to 4KB pages before hook |
| **Pool Tag Scanning** | Look for suspicious tags | Use common tags ('mEmK', 'rPhM') |
| **Gadget Detection** | ROP gadget in ntoskrnl | Use common epilogue (`add rsp, 0x28; ret`) |
| **Timing Analysis** | Hook handler execution time | Keep handler minimal, fast return |

### 11.2 PatchGuard Considerations

PatchGuard (Kernel Patch Protection) runs periodically and checks:
- SSDT/IDT modifications
- Kernel code section integrity
- Critical structure modifications
- GDT/LDT modifications

**Bypass Strategies**:
1. **PTE hooks**: Original physical page untouched
2. **No inline patches**: Use PTE method exclusively
3. **Disable PG**: Requires additional exploit (not covered)
4. **Timing**: Complete initialization before first PG check (~1-5 minutes)

---

## 12. EAC Vulnerabilities & Exploitation Gaps

### 12.1 Severity Legend

| Rating | Meaning |
|--------|---------|
| 🔴 Critical | Complete bypass, practical exploit |
| 🟠 High | Specific layer bypass, requires skill |
| 🟡 Medium | Partial evasion |
| 🟢 Low | Theoretical/exotic hardware |

### 12.2 External Cheat Gaps

#### 🔴 GAP-EXT-01: DMA Hardware Attacks
**Issue**: PCIe device reads physical RAM directly; no software on victim machine.

**Why Missed**: No driver, no handle, no network footprint. EAC has no anti-DMA enumeration.

**Mitigation**: Server-side behavior analysis.

#### 🔴 GAP-EXT-02: Kernel Memory Read (No Handle)
**Issue**: `MmCopyMemory` with EPROCESS pointer requires no handle entry.

**Why Missed**: Handle scanner finds nothing; if driver uses DKOM (unlinked from module list), invisible to module scanner.

**Mitigation**: Physical memory scan for hidden PE images.

### 12.3 Internal Cheat Gaps

#### 🔴 GAP-INJ-01: VAD Node DKOM
**Issue**: Kernel driver manipulates VAD tree directly — unlinks injected region or changes type to "mapped image".

**Difficulty**: Very high (BSOD risk), but demonstrated possible.

**Impact**: Defeats primary internal detection mechanism.

#### 🟠 GAP-INJ-02: Early Injection Window
**Issue**: Inject between Windows loader mapping and EAC first scan. If injected DLL is legitimate signed binary with secondary payload, initial scan passes.

### 12.4 User-Mode Gaps

#### 🔴 GAP-UM-01: Ring3 Anti-Debug Trivially Bypassed
**Issue**: All user-mode anti-debug checks patchable (`PEB.BeingDebugged`, `NtGlobalFlag`, etc.).

**Impact**: EAC.exe freely debuggable for IOCTL protocol analysis.

---

## 13. Function Reference

### 13.1 Cryptographic Functions

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E21280` | 0x409 | `P256_FieldMul` | 9-limb × 9-limb polynomial multiply |
| `0xFFFFF807C1E2168C` | 0x26C | `P256_FieldReduce` | Modular reduction to P-256 field |
| `0xFFFFF807C1E226E0` | 0x31D | `P256_ScalarMul` | 2-bit NAF constant-time multiply |
| `0xFFFFF807C1E1AF00` | 0x5EB | `NTT_MontgomeryReduce` | NTT with Montgomery reduction |
| `0xFFFFF807C1E1AB00` | 0x3E5 | `NTT_Butterfly` | Cooley-Tukey butterfly |

### 13.2 Compression Functions

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E11C00` | 0x2D1 | `Zstd_BuildFreqTable` | Huffman frequency builder |
| `0xFFFFF807C1E11EE0` | 0x579 | `Zstd_Huffman_SSE2` | SSE2 2-stream Huffman |
| `0xFFFFF807C1E12460` | 0x54D | `Zstd_Huffman_AVX2_v1` | AVX2 2-stream Huffman |
| `0xFFFFF807C1E13100` | 0x577 | `Zstd_Huffman_AVX2_4stream` | AVX2 6-stream (15 bytes/iter) |

### 13.3 Hash Functions

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E3A4C0` | 0x11A | `HashContext_Init` | Algorithm selector |
| `0xFFFFF807C1E3BB98` | varies | `SHA512_Init` | SHA-512 initialization |
| `0xFFFFF807C1E3A568` | varies | `SHA256_BlockProcess` | SHA-256 compression |

### 13.4 Telemetry & Detection

| Address | Size | Name | Description |
|---------|------|------|-------------|
| `0xFFFFF807C1E1DD80` | 0x844 | `AssembleTelemetryPacket` | Main telemetry builder |
| `0xFFFFF807C1ED4320` | varies | `DecryptFnPtr` | Encrypted pointer resolver |
| `0xFFFFF807C1E17CF0` | 0xF2 | `ModuleEnum_IterNext` | Module list iterator |
| `0xFFFFF807C1E173E0` | 0xFB | `ProcessEnum_IterNext` | Process list iterator |

---

## 14. Build Requirements

- **WDK**: Windows Driver Kit (latest)
- **Target**: Windows 10/11 x64 (22H2 through 24H2)
- **Loader**: kdmapper (iqvw64e.sys) or similar
- **Signing**: Not required (manual map bypasses DSE)
- **Pool Tags**: `'mEmK'`, `'rPhM'`, `'wPhM'`, `'etPK'`, `'fpSK'`, `'pfSK'`, `'cLnK'`

---

## 15. References

### Primary Sources
- Microsoft Windows Driver Documentation: https://learn.microsoft.com/en-us/windows-hardware/drivers/
- Intel® 64 and IA-32 Architectures Software Developer's Manual
- AMD64 Architecture Programmer's Manual

### Research Papers
- "If It Looks Like a Rootkit and Deceives Like a Rootkit" (ACM 2024)
- "Detecting manually mapped drivers" - Samuel Tulach (2024)
- "Demystifying Physical Memory Primitive Exploitation on Windows" (2024)

### Community Resources
- UnknownCheats Kernel Subforum
- Anti-Cheat.ac Blog
- Secret.club Research

### See Also
- `references/eac_complete_analysis.md` - Complete EAC reverse engineering documentation
- `references/driver_prompt.md` - Complete kernel driver build specifications
- `references/driver_stealth_mechanisms.md` - Detailed stealth technique explanations  
- `references/usermode_client_prompt.md` - Usermode overlay/client architecture
- `references/sources.md` - Research sources and citations

---

## When to Use

This skill is for **kernel driver development** with anti-cheat evasion. Use it when:
- ✅ Building manual-map kernel drivers
- ✅ Implementing PTE hooks for communication
- ✅ Creating physical memory R/W capabilities
- ✅ Implementing trace cleaning (PiDDB, MmUnloadedDrivers)
- ✅ Return address spoofing for API calls
- ✅ Bypassing EAC detection mechanisms
- ❌ Standard C++ application development (use cpp-helper skill instead)
- ❌ User-mode game hacking (see usermode_client_prompt.md for that)

## Skill Integration

This skill works with:
- **cpp-helper** plugin - For standard C++ code and build commands
- **kernel-driver** references - Detailed specifications in references folder

For complete implementation details, see the reference documents in the `references/` folder.
