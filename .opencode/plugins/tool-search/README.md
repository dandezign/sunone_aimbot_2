# Tool Search Plugin - Command Loading Architecture

## Overview

This plugin implements the **Tool Search pattern** where:
1. **Only `toolSearch` is exposed** initially
2. Agent calls `toolSearch` with keywords/query
3. Relevant commands are **loaded into session context**
4. Agent can then execute loaded commands directly via bash tool

## Architecture Flow

```
Agent: "I need to build with CUDA"
         │
         ▼
    ┌─────────────────┐
    │   toolSearch    │  (ONLY tool available)
    │   - query       │
    │   - parameters  │
    └─────────────────┘
         │
         ▼
    Returns + Loads:
    {
      "commands": [
        {
          "key": "buildCuda",
          "command": "powershell -File .../Build-Cuda.ps1"
        }
      ],
      "loadedCommands": [
        { "key": "buildCuda", ... },
        { "key": "buildCudaParallel", ... },
        { "key": "buildCudaRebuild", ... }
      ]
    }
         │
         ▼
    Agent executes via bash:
    bash({ command: "powershell -File ...", ... })
```

## Usage Pattern

### Step 1: Search for Commands

```typescript
// Call toolSearch with your task
const result = await toolSearch({
  query: "build with CUDA",
  parameters: { backend: "cuda" },
  loadIntoContext: true  // Load commands into session (default)
})
```

### Step 2: Execute Returned Commands

```typescript
// Use bash tool with returned command
await bash({
  command: result.commands[0].command,
  description: "Build CUDA project"
})
```

### Step 3: Reuse Loaded Commands (Optional)

After loading, commands are available for the session. You can reference them by key:

```typescript
// Previously loaded commands can be reused
// The command is already in session context
await bash({
  command: "powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1"
})
```

## Available Commands

| Key | Description | Parameters |
|-----|-------------|------------|
| `buildCuda` | Build with CUDA/TensorRT | none |
| `buildCudaRebuild` | Clean + rebuild CUDA | none |
| `buildCudaParallel` | Build CUDA with parallel jobs | none |
| `buildDml` | Build with DirectML | none |
| `buildDmlRebuild` | Clean + rebuild DML | none |
| `findHeaders` | Extract #include deps | `filePath` |
| `checkCpp` | Static analysis | `filePath` |
| `searchSymbols` | Find class/function | `symbol`, `type` |
| `analyzeIncludes` | Detect circular deps | `entryPoint` |

## Examples

### Build Project

```typescript
// 1. Search and load
const buildResult = await toolSearch({
  query: "build with CUDA"
})

// 2. Execute
await bash({
  command: buildResult.commands[0].command,
  description: "Build CUDA"
})
```

### Find Headers in File

```typescript
// 1. Search with parameters
const headersResult = await toolSearch({
  query: "find headers in src/main.cpp",
  parameters: { filePath: "src/main.cpp" }
})

// 2. Execute
await bash({
  command: headersResult.commands[0].command,
  description: "Find headers"
})
```

### Check File for Errors

```typescript
const checkResult = await toolSearch({
  query: "check cpp errors",
  parameters: { filePath: "src/myfile.cpp" }
})

await bash({
  command: checkResult.commands[0].command,
  description: "Static analysis"
})
```

### Search for Class Definition

```typescript
const symbolResult = await toolSearch({
  query: "find class MyClass",
  parameters: { symbol: "MyClass", type: "class" }
})

await bash({
  command: symbolResult.commands[0].command,
  description: "Search for MyClass"
})
```

## Response Format

```typescript
{
  success: true,
  query: "build with CUDA",
  matchesFound: 3,
  loadedIntoContext: true,
  recommendation: "Best match: buildCuda (20 relevance)",
  commands: [
    {
      key: "buildCuda",
      description: "Build with CUDA/TensorRT backend",
      command: "powershell -ExecutionPolicy Bypass -File .../Build-Cuda.ps1",
      scriptPath: ".opencode/plugins/tool-search/scripts/Build-Cuda.ps1",
      relevance: 20,
      parameters: {},
      requiresAdmin: false
    }
  ],
  loadedCommands: [
    { "key": "buildCuda", ... },
    { "key": "buildCudaParallel", ... },
    { "key": "buildCudaRebuild", ... }
  ],
  executionInstructions: {
    method: "Use the bash tool to execute the PowerShell command",
    example: { tool: "bash", args: { command: "..." } },
    note: "Commands loaded into context can be executed directly via bash"
  },
  nextSteps: [
    { priority: 1, action: "Execute: powershell ...", description: "Build with CUDA" }
  ]
}
```

## Intent Keywords

The `toolSearch` recognizes these intents:

| Intent | Commands Loaded | Relevance Boost |
|--------|----------------|-----------------|
| "build cuda" | buildCuda, buildCudaParallel | 20 |
| "rebuild cuda" | buildCudaRebuild | 25 |
| "build dml" | buildDml | 20 |
| "find headers" | findHeaders | 20 |
| "check cpp" | checkCpp | 20 |
| "find class" | searchSymbols | 20 |
| "circular" | analyzeIncludes | 20 |
| "dependencies" | findHeaders, analyzeIncludes | 15 |

## Session Management

Commands are loaded **per session**:
- Each session has its own `loadedCommands` Map
- Commands persist for the session lifetime
- Commands are cleared when session is deleted

```typescript
// Session state structure
interface SessionState {
  filesModified: string[]
  lastBuildStatus: boolean
  loadedCommands: Map<string, LoadedCommand>
}

interface LoadedCommand {
  key: string
  description: string
  command: string
  scriptPath: string
  parameters: Record<string, string>
  loadedAt: number  // Timestamp
}
```

## Benefits

1. **Minimal Context Usage**: Only `toolSearch` loaded initially
2. **On-Demand Loading**: Commands loaded only when needed
3. **Reusable**: Once loaded, commands available for session
4. **Discoverable**: Intent-based search with relevance ranking
5. **Flexible**: Can load new commands anytime via toolSearch

## Directory Structure

```
tool-search/
├── tool-search.ts          # toolSearch + command loading logic
├── tool-registry.ts        # Metadata and descriptions
├── scripts/                # PowerShell scripts
│   ├── Build-Cuda.ps1
│   ├── Build-Dml.ps1
│   ├── Find-Headers.ps1
│   ├── Check-Cpp.ps1
│   ├── Search-Symbols.ps1
│   └── Analyze-Includes.ps1
└── tools/                  # Legacy (not used in new flow)
```

## Specialized Skills

For **kernel driver development**, see the `kernel-driver` skill:
- Location: `.opencode/skills/kernel-driver/`
- Use for: Driver development, physical memory R/W, PTE hooks, trace cleaning
- References: `.opencode/skills/kernel-driver/references/`

Use **cpp-helper** for:
- Standard C++ application development
- Build systems (CMake, Make)
- Code analysis and refactoring

## Related Skills

- **kernel-driver**: Specialized skill for Windows kernel driver development
  - Location: `.opencode/skills/kernel-driver/`
  - References: `.opencode/skills/kernel-driver/references/`
