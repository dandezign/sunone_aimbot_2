# Kernel Driver Development for Anti-Cheat Bypass

## Overview

This skill provides comprehensive expertise in Windows kernel driver development with a focus on anti-cheat evasion techniques for competitive gaming environments. It covers manual mapping, stealth mechanisms, physical memory manipulation, and detection bypass strategies.

**Research Date**: March 2026  
**Target Platforms**: Windows 10/11 x64 (22H2 through 24H2)  
**Primary Anti-Cheats**: EAC (Easy Anti-Cheat), BattlEye, Riot Vanguard, FACEIT

## Core Principles

### 1. Stealth-First Architecture

Modern anti-cheats employ multi-layered detection:
- **Kernel module enumeration** (PsLoadedModuleList, PiDDB)
- **Memory integrity checks** (PatchGuard, HVCI)
- **Stack walking** (return address validation)
- **Behavioral analysis** (API call patterns, timing)
- **Hardware telemetry** (TPM, Secure Boot checks)

**Design Philosophy**: Every component must be invisible to both user-mode and kernel-mode scans.

### 2. Manual Mapping Over Traditional Loading

Traditional driver loading (`CreateService`, `ScLoadDriver`) leaves permanent traces. Manual mapping via vulnerable drivers (iqvw64e.sys, dbutil_2_3.sys) provides:
- No service registration
- No driver object creation (if cleaned properly)
- No event tracing (ETW) logs
- Immediate unload capability

**Key Tools**: kdmapper, KsDumper, gdrv, capcom

### 3. Physical Memory Over Virtual APIs

Standard virtual memory APIs (`MmCopyVirtualMemory`) are heavily monitored. Physical memory access via CR3 page table walking:
- Bypasses VAD tree monitoring
- Avoids `MmCopyVirtualMemory` hooks
- Works on demand-zero pages
- Leaves no cross-process handle traces

## Driver Architecture

### File Structure

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

### Initialization Sequence

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
    
    // STEP 4: Optional - restore original PTE on unload
    // (Driver cannot be conventionally unloaded)
    
    return STATUS_SUCCESS;
}
```

## Communication Protocol

### Hook Target Selection

**Primary Target**: `NtQueryCompositionSurfaceStatistics` in dxgkrnl.sys

**Why this function**:
- Callable from usermode via win32u.dll
- Single PVOID parameter (perfect for struct passing)
- Rarely used by legitimate applications
- No IRQL restrictions

**Alternative targets** (if primary is monitored):
- `NtUserGetThreadState` (win32kbase.sys)
- `NtGdiGetCPSBitmapInfo` (win32k.sys)
- Custom IOCTL via port driver (higher detection risk)

### Request Structure

```cpp
#define REQUEST_MAGIC 0x44524B4E  // "DRKN"

typedef struct _REQUEST_DATA {
    ULONG           magic;          // Must match REQUEST_MAGIC
    ULONG           command;        // DRIVER_COMMAND enum
    ULONG64         pid;            // Target process ID
    ULONG64         address;        // Virtual address for R/W
    ULONG64         buffer;         // Usermode buffer (cast to uint64)
    ULONG64         size;           // Bytes to R/W
    ULONG64         result;         // Output value
    ULONG           protect;        // Memory protection flags
    WCHAR           module_name[64];// Module name for CMD_MODULE_BASE
} REQUEST_DATA, *PREQUEST_DATA;
```

### Command Enumeration

```cpp
typedef enum _DRIVER_COMMAND {
    CMD_NONE          = 0,
    CMD_READ          = 1,    // Physical memory read (CR3 walk)
    CMD_WRITE         = 2,    // Physical memory write (KeStackAttach)
    CMD_MODULE_BASE   = 3,    // Get module base via PEB
    CMD_ALLOC         = 4,    // ZwAllocateVirtualMemory
    CMD_FREE          = 5,    // ZwFreeVirtualMemory
    CMD_PROTECT       = 6,    // ZwProtectVirtualMemory
    CMD_READ64        = 7,    // Virtual read (MmCopyVirtualMemory fallback)
    CMD_WRITE64       = 8,    // Virtual write (MmCopyVirtualMemory fallback)
    CMD_PING          = 99,   // Connection check
    CMD_VERIFY_PTE    = 100,  // Debug: return PTE hook state
    CMD_VERIFY_SPOOF  = 101,  // Debug: return spoof stub state
} DRIVER_COMMAND;
```

## Stealth Mechanisms

### 1. PTE Hooking (Page Table Entry)

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

**Hook State Structure**:

```cpp
typedef struct _PTE_HOOK_STATE {
    PVOID       targetVA;       // Hooked virtual address
    PPTE_ENTRY  pteAddress;     // Pointer to the PTE
    ULONG64     originalPFN;    // Original physical frame number
    ULONG64     newPFN;         // Our page's physical frame number
    PVOID       newPageVA;      // Virtual address of our allocated page
    PHYSICAL_ADDRESS newPagePA; // Physical address of our page
    BOOLEAN     active;         // Is hook active?
} PTE_HOOK_STATE;
```

### 2. Trace Cleaning

#### PiDDB Cache Table

**What it is**: Kernel Plug and Play Database caches every driver ever loaded.

**Location**: Both in ntoskrnl.exe `PAGE` section.

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

### 3. Return Address Spoofing

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

**Usage**:
```cpp
// Typed function pointers
typedef NTSTATUS (*fn_spoof_2)(PVOID, PVOID);

// Macro wrappers
#define SpoofCall2(fn, a1, a2)  ((fn_spoof_2)g_SpoofStub)((PVOID)(fn), (PVOID)(a1), (PVOID)(a2))

// Usage pattern (throughout driver)
PEPROCESS process;
if (g_SpoofStub) {
    SpoofCall2(PsLookupProcessByProcessId, pid, &process);
} else {
    PsLookupProcessByProcessId(pid, &process);
}
```

## Physical Memory R/W

### CR3 Acquisition

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

### CR3 Caching

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

### 4-Level Page Table Walk

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

// Similar structures for PDPT, PD, PT entries

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

### Read Implementation

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

### Write Implementation

Writes use `KeStackAttachProcess` + `RtlCopyMemory` (not physical write):

```cpp
NTSTATUS PhysicalWriteProcessMemory(HANDLE pid, PVOID virtualAddress, 
                                    PVOID userBuffer, SIZE_T size) {
    // Copy usermode → kernel buffer
    PVOID kernelBuffer = ExAllocatePoolWithTag(NonPagedPool, size, 'wPhM');
    if (!kernelBuffer) return STATUS_MEMORY_NOT_ALLOCATION;
    
    __try {
        RtlCopyMemory(kernelBuffer, userBuffer, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ExFreePoolWithTag(kernelBuffer, 'wPhM');
        return STATUS_ACCESS_VIOLATION;
    }
    
    // Get process object
    PEPROCESS process;
    NTSTATUS status = SpoofCall2(PsLookupProcessByProcessId, pid, &process);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(kernelBuffer, 'wPhM');
        return status;
    }
    
    // Attach and copy
    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);
    
    __try {
        RtlCopyMemory(virtualAddress, kernelBuffer, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        KeUnstackDetachProcess(&apc);
        ObfDereferenceObject(process);
        ExFreePoolWithTag(kernelBuffer, 'wPhM');
        return STATUS_PARTIAL_COPY;
    }
    
    KeUnstackDetachProcess(&apc);
    ObfDereferenceObject(process);
    ExFreePoolWithTag(kernelBuffer, 'wPhM');
    
    return STATUS_SUCCESS;
}
```

## Detection Vectors and Bypasses

### EAC Detection Methods (2025)

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

### PatchGuard Considerations

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

## Build Requirements

- **WDK**: Windows Driver Kit (latest)
- **Target**: Windows 10/11 x64 (22H2 through 24H2)
- **Loader**: kdmapper (iqvw64e.sys) or similar
- **Signing**: Not required (manual map bypasses DSE)
- **Pool Tags**: `'mEmK'`, `'rPhM'`, `'wPhM'`, `'etPK'`, `'fpSK'`, `'pfSK'`, `'cLnK'`

## References

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
- `references/driver_prompt.md` - Complete kernel driver build specifications
- `references/driver_stealth_mechanisms.md` - Detailed stealth technique explanations  
- `references/usermode_client_prompt.md` - Usermode overlay/client architecture
- `references/sources.md` - Research sources and citations

## When to Use

This skill is for **kernel driver development** with anti-cheat evasion. Use it when:
- ✅ Building manual-map kernel drivers
- ✅ Implementing PTE hooks for communication
- ✅ Creating physical memory R/W capabilities
- ✅ Implementing trace cleaning (PiDDB, MmUnloadedDrivers)
- ✅ Return address spoofing for API calls
- ❌ Standard C++ application development (use cpp-helper skill instead)
- ❌ User-mode game hacking (see usermode_client_prompt.md for that)

## Skill Integration

This skill works with:
- **cpp-helper** plugin - For standard C++ code and build commands
- **kernel-driver** references - Detailed specifications in references folder

For complete implementation details, see the reference documents in the `references/` folder.
