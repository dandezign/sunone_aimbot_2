# Kernel Driver Development Research Sources

## EAC-Specific Research

### Complete Technical Analysis

- **EAC Kernel Driver Complete Analysis** (Internal Reference)
  - File: `references/eac_complete_analysis.md`
  - Topics: P-256 cryptography, VAD scanning, handle table detection, telemetry system, HWID collection
  - Base Address: `0xFFFFF807C1E10000` (analyzed build)
  - Binary Size: ~8 MB
  - Date: March 2026
  - Confidence: High (comprehensive reverse engineering)

### Academic Research

- **"If It Looks Like a Rootkit and Deceives Like a Rootkit"** (ACM 2024)
  - URL: https://dl.acm.org/doi/fullHtml/10.1145/3664476.3670433
  - Topics: EAC kernel driver hook detection, VAD tree scanning, physical memory analysis
  - Date: 2024
  - Confidence: High (peer-reviewed)

### Community Reverse Engineering

- **Hypercall - "Inside anti-cheat: EasyAntiCheat - Part 1"**
  - URL: https://hypercall.net/posts/EasyAntiCheat-Part1/
  - Topics: IOCTL codes, driver architecture, kernel callbacks
  - Date: 2024-2025
  - Confidence: High (detailed technical analysis)

- **UnknownCheats - "Analysing EasyAntiCheat's Cryptographic Protocol"**
  - URL: https://www.unknowncheats.me/forum/anti-cheat-bypass/738562-analysing-easyanticheats-cryptographic-protocol.html
  - Topics: P-256 ECDSA implementation, telemetry signing, challenge-response
  - Date: 2024
  - Confidence: Medium-High (community research)

- **Secret.club - "CVEAC-2020: Bypassing EasyAntiCheat integrity checks"**
  - URL: https://secret.club/2020/04/08/eac_integrity_check_bypass.html
  - Topics: Integrity check mechanisms, EPT hooking detection
  - Date: 2020
  - Confidence: High (historical baseline)

- **GitHub - girther9399-gif/EAC-REVERSED-WRITEUP**
  - URL: https://github.com/girther9399-gif/EAC-REVERSED-WRITEUP
  - Topics: Static analysis, function identification, string obfuscation
  - Date: 2024-2025
  - Confidence: Medium-High (educational purpose)

---

## Primary Documentation

### Microsoft Official
- **Windows Hardware Developer Documentation**
  - URL: https://learn.microsoft.com/en-us/windows-hardware/drivers/
  - Topics: Driver architecture, kernel APIs, memory management
  - Confidence: High (primary source)

- **Virtual and Physical Memory**
  - URL: https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/virtual-and-physical-memory
  - Topics: Memory translation, physical access in kernel debugger
  - Confidence: High

### Hardware Manuals
- **Intel® 64 and IA-32 Architectures Software Developer's Manual**
  - Volume 3A: System Programming Guide
  - Topics: Page tables, CR3, PTE structure, large pages
  - Confidence: High (hardware specification)

- **AMD64 Architecture Programmer's Manual**
  - Volume 2: System Programming
  - Topics: Paging, address translation, CR3 caching
  - Confidence: High

## Anti-Cheat Research

### Detection Vectors
- **uid000222/Anti-Cheat-Detection-Vectors** (GitHub)
  - URL: https://github.com/uid000222/Anti-Cheat-Detection-Vectors
  - Topics: EAC, BattlEye, Vanguard detection methods
  - Date: 2024-2025
  - Confidence: Medium-High (community research)

- **Anti-Cheat Systems Explained: EAC vs BattlEye vs Vanguard**
  - URL: https://injectkings.com/info/anti-cheat-explained
  - Topics: Comparison of major anti-cheat systems
  - Date: 2025
  - Confidence: Medium

### Academic Research
- **"If It Looks Like a Rootkit and Deceives Like a Rootkit"** (ACM 2024)
  - URL: https://dl.acm.org/doi/fullHtml/10.1145/3664476.3670433
  - Topics: EAC kernel driver hook detection
  - Date: 2024
  - Confidence: High (peer-reviewed)

## Manual Mapping & Driver Loading

### Detection Methods
- **Samuel Tulach - "Detecting manually mapped drivers"**
  - URL: https://tulach.cc/detecting-manually-mapped-drivers/
  - Topics: Memory scanning for manually mapped drivers
  - Date: 2024
  - Confidence: High (detailed technical analysis)

- **Anticheat.ac - "Driver Mapping: Injecting Code into Kernel Mode"**
  - URL: https://anticheat.ac/blog/mappeddrivers
  - Topics: KDMapper, driver mapping detection
  - Date: 2025
  - Confidence: Medium-High

### Tools & Implementations
- **kvmclgi/kernelmapper** (GitHub)
  - URL: https://github.com/kvmclgi/kernelmapper
  - Topics: Kernel mapper POC with PatchGuard-safe hooks
  - Confidence: Medium (POC code)

- **isiddique2024/Page-Table-Injector** (GitHub)
  - URL: https://github.com/isiddique2024/Page-Table-Injector
  - Topics: Page table manipulation for injection
  - Tested: Windows 10 20H2 through Windows 11 24H2
  - Confidence: Medium-High

## Trace Cleaning

### PiDDB & MmUnloadedDrivers
- **"Clear driver loading traces"** (UnknownCheats)
  - URL: https://www.unknowncheats.me/forum/anti-cheat-bypass/361494-driver-loading-traces-mmlastunloadeddrivers-piddbcache.html
  - Topics: PiDDB cache, MmUnloadedDrivers cleaning
  - Date: 2024
  - Confidence: Medium (community knowledge)

- **hLunaaa - "Exploring CI.dll and Bigpool Cache"**
  - URL: https://hlunaaa.github.io/2025/04/18/Exploring-CI.dll-and-Bigpool-Cache.html
  - Topics: PiDDBCacheTable, MmLastUnloadedDriver, telemetry
  - Date: 2025
  - Confidence: Medium-High

- **DErDYAST1R/64KernelDriverCleaner** (GitHub)
  - URL: https://github.com/DErDYAST1R/64KernelDriverCleaner
  - Topics: Cache & structure table cleaning
  - Confidence: Medium

### Driver Hiding
- **"Hiding Drivers on Windows 10"** (Reverse Engineering Blog)
  - URL: https://revers.engineering/hiding-drivers-on-windows-10/
  - Topics: Driver object unlinking, PsLoadedModuleList
  - Date: 2024
  - Confidence: Medium-High

## Physical Memory Manipulation

### CR3 & Page Table Walking
- **"Demystifying Physical Memory Primitive Exploitation on Windows"**
  - URL: https://0dr3f.github.io/Demystifying_Physical_Memory_Primitive_Exploitation_on_Windows
  - Topics: CR3 acquisition, page table walking, kernel memory access
  - Date: 2024
  - Confidence: High

- **maxpsger/Cr3-Driver** (GitHub)
  - URL: https://github.com/maxpsger/Cr3-Driver
  - Topics: Direct physical memory manipulation via page table walking
  - Confidence: Medium-High

- **"Bypassing Windows 11 24H2 KASLR Restrictions"**
  - URL: https://undercodetesting.com/bypassing-windows-11-24h2-kaslr-restrictions-a-deep-dive-into-kernel-object-retrieval-via-physical-memory-r-w/
  - Date: 2025
  - Topics: Physical memory R/W on latest Windows
  - Confidence: Medium-High

### Address Translation
- **"Windows Address Translation Deep Dive - Part 2"**
  - URL: https://bsodtutorials.wordpress.com/2024/04/05/windows-address-translation-deep-dive-part-2/
  - Topics: PTE structure, page walk, large pages
  - Date: 2024
  - Confidence: Medium-High

- **CoreSecurity - "Making something out of Zeros"**
  - URL: https://www.coresecurity.com/core-labs/articles/making-something-out-zeros-alternative-primitive-windows-kernel-exploitation
  - Topics: CR3 walker, physical R/W primitives
  - Confidence: High (security research)

## Return Address Spoofing

### Stack Spoofing Techniques
- **Barracudach/CallStack-Spoofer** (GitHub)
  - URL: https://github.com/Barracudach/CallStack-Spoofer
  - Topics: Kernel-mode and user-mode stack spoofing
  - Confidence: Medium

- **0xROOTPLS/Spuuf** (GitHub)
  - URL: https://github.com/0xROOTPLS/Spuuf
  - Topics: Dynamic API resolution, return address spoofing, synthetic call stacks
  - Date: 2025
  - Confidence: Medium

- **"An Introduction into Stack Spoofing"** (Nigerald's Blog)
  - URL: https://dtsec.us/2023-09-15-StackSpoofin/
  - Topics: Stack spoofing fundamentals, detection opportunities
  - Date: 2023
  - Confidence: Medium-High

### Advanced Techniques
- **"SilentMoonwalk: Implementing a dynamic Call Stack Spoofer"**
  - URL: https://klezvirus.github.io/posts/Stackmoonwalk/
  - Topics: ROP gadgets, stack frame manipulation
  - Date: 2024
  - Confidence: Medium-High

- **IBM X-Force - "Reflective call stack detections and evasions"**
  - URL: https://www.ibm.com/think/x-force/reflective-call-stack-detections-evasions
  - Topics: Return address spoofing, ROP gadget detection
  - Date: 2024
  - Confidence: High (security vendor research)

## Community Resources

### Forums & Discussions
- **UnknownCheats Kernel Subforum**
  - URL: https://www.unknowncheats.me/forum/anti-cheat-bypass/
  - Topics: Community discussions, POCs, detection methods
  - Confidence: Variable (community-sourced)

- **Secret.club Research**
  - URL: https://secret.club/
  - Topics: Anti-cheat internals, HV detection, emulation detection
  - Confidence: Medium-High

### Vulnerable Drivers
- **CVE-2021-21551 (DBUTIL)**
  - URL: https://www.mitchellzakocs.com/blog/dbutil
  - Topics: Dell BIOS utility driver vulnerabilities
  - Confidence: High (CVE analysis)

## Version Information

- **Research Date**: March 2026
- **Windows Versions**: 10/11 x64 (22H2 through 24H2)
- **Anti-Cheat Versions**: EAC (2025), BattlEye (2025), Vanguard (2025)
- **Primary Tools**: kdmapper, iqvw64e.sys (timestamp: 0x5284EAC3)

## Research Confidence Summary

### EAC-Specific Research

| Component | Confidence | Primary Sources | Validation Status |
|-----------|-----------|-----------------|-------------------|
| **P-256 Cryptography** | High | ACM paper, UnknownCheats analysis | ✅ Verified against NIST spec |
| **VAD Tree Scanning** | High | Multiple independent confirmations | ✅ Consistent across builds |
| **Handle Table Detection** | High | Microsoft docs, community research | ✅ Well-documented |
| **Telemetry Structure** | Medium-High | Reverse engineered | ⚠️ Build-specific |
| **HWID Collection Methods** | High | Driver analysis, spoofer research | ✅ Validated |
| **Encrypted Function Dispatch** | High | Binary analysis | ⚠️ Constants rotate |
| **Zstd Compression** | High | Function pattern matching | ✅ Verified |
| **Thread/Process Callbacks** | High | Microsoft + community | ✅ Standard technique |
| **String Obfuscation** | Medium | Static analysis | ⚠️ Algorithm may vary |
| **Detection Gaps** | Medium | Theoretical + limited testing | ⚠️ Requires validation |

### General Kernel Driver Research

| Topic | Confidence Level | Primary Sources |
|-------|-----------------|-----------------|
| PTE Hooking | High | Microsoft docs, GitHub POCs |
| Trace Cleaning | High | Multiple independent implementations |
| Physical Memory R/W | High | Academic papers, security research |
| Return Address Spoofing | Medium-High | Security vendor research |
| Anti-Cheat Detection | Medium | Community research, some academic |
| PatchGuard Bypass | Medium | Limited public information |

## Key EAC-Specific Takeaways

### Cryptographic Implementation

1. **P-256 is standard NIST secp256r1**
   - 9-limb 30-bit radix representation
   - Constant-time Montgomery ladder
   - 2-bit NAF window method
   - Used for telemetry signing, not encryption

2. **No Import Table**
   - All APIs resolved via encrypted function pointer table
   - Decryption: `(encrypted_ptr * constant_a) ^ constant_b`
   - ~50+ slots at `0xFFFFF807C2068E78`
   - Build-specific constants

3. **String Obfuscation**
   - Rolling XOR cipher with position-dependent key
   - No plaintext strings in binary
   - Decoded at runtime only

### Detection Mechanisms

4. **VAD Tree is Primary Internal Detection**
   - Scans for private + executable + no file backing
   - RWX regions flagged immediately
   - Cross-references with PEB module list
   - **Bypass**: Requires kernel driver to manipulate VAD nodes

5. **Handle Table Walking**
   - Direct kernel structure access (not NtQuerySystemInformation)
   - PROCESS_VM_READ/WRITE = immediate flag
   - Whitelist for legitimate software (NVIDIA, Steam)
   - **Bypass**: Don't open handles to game process

6. **HWID Collection**
   - 6+ independent sources (ATA serial, MAC, SMBIOS, etc.)
   - Cross-source comparison for inconsistency detection
   - Timing anomaly detection for hook interception
   - **Bypass**: Firmware-level spoofing or hypervisor interception

7. **Telemetry Pipeline**
   - 184-byte packet structure
   - XOR obfuscation (0x90 key)
   - Zstd compression (60-80% reduction)
   - P-256 ECDSA signature (64 bytes)
   - HTTPS to *.easyanticheat.net
   - **Bypass**: Driver modification or hypervisor EPT hook

### Exploitation Gaps

8. **Critical Vulnerabilities**
   - DMA attacks (no software footprint)
   - VAD node DKOM (unlinking injected regions)
   - Early injection window (before EAC first scan)
   - Ring3 anti-debug trivially bypassed

9. **High-Value Gaps**
   - Kernel memory read without handles
   - SMBIOS hypervisor emulation
   - Thread pool hijacking
   - PatchGuard timing window (3-10 minutes)

### Build Considerations

10. **Variability**
    - Function addresses change per build
    - Encryption constants rotate
    - Detection logic evolves weekly
    - Always verify against current build

---

## Key Takeaways (General)

1. **PTE hooks are stealthier than inline** - Original physical page remains clean for integrity checks
2. **Physical memory R/W bypasses most monitoring** - CR3 page walking avoids `MmCopyVirtualMemory` hooks
3. **Trace cleaning is mandatory** - PiDDB, MmUnloadedDrivers, and DriverObject must all be cleaned
4. **Return address spoofing prevents stack walk detection** - Use common epilogue gadgets
5. **CR3 caching is critical** - EAC strips DirTableBase, acquire via `__readcr3()` + attach
6. **Order matters** - Hook first, then clean traces
7. **No conventional unload** - Driver stays resident, cannot use DriverUnload
