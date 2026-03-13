# EAC Quick Reference — Addresses, Patterns, and Constants

> **Purpose**: Quick lookup reference for EAC kernel driver constants, patterns, and function addresses from the analyzed build.
> 
> **Build**: EasyAntiCheat.sys (analyzed March 2026)  
> **Base Address**: `0xFFFFF807C1E10000`  
> **Binary Size**: ~8 MB

---

## ⚠️ Important Disclaimer

**These values are BUILD-SPECIFIC and will change** with EAC updates. Use for educational purposes and as reference patterns for finding equivalent code in other builds.

---

## Encrypted Function Pointer Table

**Base Address**: `0xFFFFF807C2068E78`

| Slot | Function | Constant A | Constant B |
|------|----------|------------|------------|
| 0 | PsGetCurrentProcess equiv. | `0x936ACF702E4281A9` | `0xFA85638DCFA646E7` |
| 1 | Packet serializer | `0xE615DAFE9811D559` | `0x00A559FABE750D69` |
| 2 | PsGetProcessSessionId | `0xF3EC14C2131FEE4F` | `0xBE0DAFCD89B39CD1` |
| 3 | KeQuerySystemTime | `0xE462A05B3E35A30F` | `0x7D67C96867B51F90` |

**Decryption Formula**:
```c
fn_ptr = (slot->encrypted_ptr * constant_a) ^ constant_b
```

---

## Key Function Addresses

### Cryptography (P-256)

| Address | Size | Function | Description |
|---------|------|----------|-------------|
| `0xFFFFF807C1E21280` | 0x409 | P256_FieldMul | 9-limb polynomial multiply |
| `0xFFFFF807C1E2168C` | 0x26C | P256_FieldReduce | Modular reduction |
| `0xFFFFF807C1E226E0` | 0x31D | P256_ScalarMul | Constant-time scalar mult |
| `0xFFFFF807C1E22340` | 0x1D0 | P256_ConditionalSwap | Constant-time swap |
| `0xFFFFF807C1E22520` | 0x17E | P256_FieldInvert | Field inversion |

### NTT (Number Theoretic Transform)

| Address | Size | Function | Description |
|---------|------|----------|-------------|
| `0xFFFFF807C1E1AF00` | 0x5EB | NTT_MontgomeryReduce | Montgomery reduction |
| `0xFFFFF807C1E1AB00` | 0x3E5 | NTT_Butterfly | Cooley-Tukey butterfly |
| `0xFFFFF807C1E1AA80` | 0x68 | NTT_BitReverse | Bit-reversal permutation |

### Zstd Compression

| Address | Size | Function | Description |
|---------|------|----------|-------------|
| `0xFFFFF807C1E11C00` | 0x2D1 | Zstd_BuildFreqTable | Frequency table builder |
| `0xFFFFF807C1E11EE0` | 0x579 | Zstd_Huffman_SSE2 | SSE2 2-stream Huffman |
| `0xFFFFF807C1E12460` | 0x54D | Zstd_Huffman_AVX2_v1 | AVX2 2-stream |
| `0xFFFFF807C1E13100` | 0x577 | Zstd_Huffman_AVX2_4stream | AVX2 6-stream (fastest) |

### Hash Functions

| Address | Size | Function | Description |
|---------|------|----------|-------------|
| `0xFFFFF807C1E3A4C0` | 0x11A | HashContext_Init | Algorithm selector |
| `0xFFFFF807C1E3BB98` | varies | SHA512_Init | SHA-512 initialization |
| `0xFFFFF807C1E3BCCC` | varies | SHA384_Init | SHA-384 initialization |
| `0xFFFFF807C1E3A568` | varies | SHA256_BlockProcess | SHA-256 compression |

### Telemetry

| Address | Size | Function | Description |
|---------|------|----------|-------------|
| `0xFFFFF807C1E1DD80` | 0x844 | AssembleTelemetryPacket | Main telemetry builder |
| `0xFFFFF807C1E1E5C4` | 0xA4 | InitPacketBuffer | Packet initialization |
| `0xFFFFF807C1ED4320` | varies | DecryptFnPtr | Encrypted pointer resolver |

### Detection

| Address | Size | Function | Description |
|---------|------|----------|-------------|
| `0xFFFFF807C1E17CF0` | 0xF2 | ModuleEnum_IterNext | Module list iterator |
| `0xFFFFF807C1E173E0` | 0xFB | ProcessEnum_IterNext | Process list iterator |
| `0xFFFFF807C1E16EF0` | 0xF0 | DispatchTable_Check | Driver dispatch validation |
| `0xFFFFF807C1E16FE0` | 0xF7 | SSDT_Validate | SSDT hook detection |
| `0xFFFFF807C1E18190` | 0x4E | VAD_WalkNext | VAD tree traversal |

---

## Pattern Signatures

### PiDDB Cleaning Patterns

**PiDDBLockPtr**:
```
48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? 33 DB
```

**PiDDBTablePtr**:
```
48 8D 0D ? ? ? ? E8 ? ? ? ? 3D ? ? ? ? 0F 83 ? ? ? ?
```

### MmUnloadedDrivers Pattern

```
4C 8B ? ? ? ? ? 4C 8B ? ? 4D 85 ? ? 74 ?
```

### MiGetPteAddress Pattern (for PTE Hook)

```
48 C1 E9 09              shr rcx, 9
48 B8 [8 bytes MASK]     mov rax, <MASK>
48 23 C8                 and rcx, rax
48 B8 [8 bytes PTE_BASE] mov rax, <PTE_BASE>
48 03 C1                 add rax, rcx
C3                       ret
```

**Full Pattern (31 bytes)**:
```
48 C1 E9 09 48 B8 ? ? ? ? ? ? ? ? 48 23 C8 48 B8 ? ? ? ? ? ? ? ? 48 03 C1 C3
```

### Return Address Spoofing Gadget

**Pattern**: `48 83 C4 28 C3`
```asm
add rsp, 0x28
ret
```

**Use**: Common function epilogue in ntoskrnl for stack spoofing.

---

## SHA Initialization Constants

### SHA-1
```
0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
```

### SHA-256
```
0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
```

### MD5
```
1732584193 (0x67452301)
-271733879 (0xEFCDAB89)
-1732584194 (0x98BADCFE)
271733878 (0x10325476)
```

---

## EPROCESS Offsets (Windows 10/11 x64)

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| UniqueProcessId | +56 | 8 | Process ID |
| InheritedFromUniqueProcessId | +64 | 8 | Parent PID |
| ImageFileName | +96 | 15 | Process name |
| Peb/VadRoot | +240 | 8 | VAD tree root |
| ObjectTable | +376 | 8 | Handle table |
| Protection flags | +556 | 4 | PS_PROTECTION |

---

## KUSER_SHARED_DATA Addresses

| Address | Type | Description |
|---------|------|-------------|
| `0x7FFE0000` | User | KUSER_SHARED_DATA base (user-mode) |
| `0xFFFFF78000000000` | Kernel | KUSER_SHARED_DATA base (kernel) |
| `0xFFFFF78000000014` | Kernel | TickCountLow (updated ~15ms) |

---

## Telemetry Packet Structure

**Size**: 184 bytes minimum

| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 4 | Packet version |
| 0x04 | 4 | Protection flags (EPROCESS+556) |
| 0x10 | 4 | Parent PID |
| 0x14 | 4 | Image name hash |
| 0x18 | 8 | System time |
| 0x20 | 8 | Tick count |
| 0x28 | 4 | Session ID |
| 0x30 | 8 | VAD root / PEB |
| 0x38 | 4 | Module count |
| 0x3C | 32 | Module bases (4 × 8 bytes) |
| 0x78 | 461 | Module full path (UNICODE_STRING) |
| ... | 41 | Binary fingerprint |

**XOR Key**: `0x90` (applied to linked-list node data)

---

## HWID Collection IOCTLs

### Disk Serial Numbers

```c
// ATA direct (raw command)
IOCTL_ATA_PASS_THROUGH → ATA IDENTIFY DEVICE (0xEC)
// Serial at bytes [20..39] (20 ASCII chars)

// High-level property query
IOCTL_STORAGE_QUERY_PROPERTY → StorageDeviceProperty
```

### Network MAC

```c
// Permanent (hardware-burned) MAC
IOCTL_NDIS_QUERY_GLOBAL_STATS → OID_802_3_PERMANENT_ADDRESS
```

### Volume GUID

```c
IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS
```

### GPU Instance ID

```c
IoGetDeviceProperty(..., DEVPKEY_Device_InstanceId, ...);
```

### SMBIOS Data

```c
// Raw ACPI tables
NtQuerySystemInformation(SystemFirmwareTableInformation, 'RSMB')
```

---

## VAD Node Structure

```c
typedef struct _MMVAD {
    ULONG_PTR StartingVpn;      // >> PAGE_SHIFT for address
    ULONG_PTR EndingVpn;
    union {
        struct {
            ULONG Protection : 5;  // PAGE_EXECUTE_*
            ULONG VadType : 3;     // 0=Private, 1=Mapped, 2=Image
        } VadFlags;
    } u;
    PVOID SubSection;           // File backing (NULL = private)
} MMVAD;
```

**Suspicious Indicators**:
- `VadType == 0` (Private) + `Protection & PAGE_EXECUTE` + `SubSection == NULL`
- `Protection == PAGE_EXECUTE_READWRITE` (0x40)

---

## Handle Access Masks

| Mask | Value | Risk Level |
|------|-------|------------|
| PROCESS_VM_READ | 0x0010 | 🔴 Critical |
| PROCESS_VM_WRITE | 0x0020 | 🔴 Critical |
| PROCESS_VM_OPERATION | 0x0008 | 🔴 Critical |
| PROCESS_ALL_ACCESS | 0x1FFFFF | 🔴 Critical |
| PROCESS_QUERY_INFORMATION | 0x0400 | 🟠 Medium |
| PROCESS_QUERY_LIMITED_INFORMATION | 0x1000 | 🟢 Low |

---

## P-256 Field Element

**Representation**: 9 × 30-bit limbs (270 bits total, top 14 unused)

```c
typedef struct {
    uint32_t limbs[9];
} P256_FieldElement;

// Prime: p = 2^256 - 2^224 + 2^192 + 2^96 - 1
```

**Constant-Time Swap**:
```c
void ConditionalSwap(uint8_t* a, uint8_t* b, int condition) {
    uint8_t mask = -(condition);  // 0xFF if true, 0x00 if false
    for (int i = 0; i < 32; i++) {
        uint8_t diff = (a[i] ^ b[i]) & mask;
        a[i] ^= diff;
        b[i] ^= diff;
    }
}
```

---

## Driver State Canary

**Value**: `0xBC44A31CA74B4AAF`

**Usage**: Checked in DriverUnload before cleanup:
```c
if (*(uint64_t*)g_pStateBlock == 0xBC44A31CA74B4AAF && g_pStateBlock) {
    ObjectCleanup(g_pStateBlock);
    FreePoolWrapper(v1);
}
```

---

## iqvw64e.sys Timestamp

**Value**: `0x5284EAC3`

**Usage**: PiDDB cache lookup for cleaning:
```cpp
lookupEntry.TimeDateStamp = 0x5284EAC3;
```

---

## Pool Tag Conventions

| Tag | Usage |
|-----|-------|
| `'mEmK'` | Kernel memory buffer |
| `'rPhM'` | Read physical memory |
| `'wPhM'` | Write physical memory |
| `'etPK'` | PTE hook state |
| `'fpSK'` | Spoof stub allocation |
| `'pfSK'` | PTE hook split |
| `'cLnK'` | Cleaner operations |

---

## Common Windows Kernel Structures

### HANDLE_TABLE_ENTRY

```c
typedef struct _HANDLE_TABLE_ENTRY {
    union {
        ULONG64 ObjectPointerBits : 59;  // << 4 for actual address
        ULONG64 LowValue;
    };
    ULONG32 GrantedAccessBits : 25;      // << 2 for actual mask
    // ...
} HANDLE_TABLE_ENTRY;
```

### LDR_DATA_TABLE_ENTRY

```c
typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    // ...
} LDR_DATA_TABLE_ENTRY;
```

---

## Reporting Frequency

| Event | Interval |
|-------|----------|
| Heartbeat | ~5 seconds |
| Suspicious Event | Immediate |
| Full System Scan | ~30 seconds |
| Game Start | Comprehensive |
| Periodic Callback | Timer DPC |

---

## Last Updated

**Document Version**: 1.0  
**Date**: March 2026  
**Note**: All addresses and constants are build-specific. Verify against current build before use.
