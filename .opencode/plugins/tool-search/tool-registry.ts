// Tool metadata registry - All available C++ development tools

export interface ToolInfo {
  name: string
  description: string
  useWhen: string[]
  inputSchema: Record<string, any>
  examples: Array<{
    input: Record<string, any>
    output: Record<string, any>
    explanation: string
  }>
}

export const TOOL_REGISTRY: Record<string, ToolInfo> = {
  findHeaders: {
    name: "findHeaders",
    description: "Extract and analyze #include dependencies from C++ source files. Returns a list of all header files included by the target file.",
    useWhen: [
      "You need to see what headers a C++ file depends on",
      "You're tracing include chains or dependency graphs",
      "You want to understand the file's external dependencies",
      "You're refactoring and need to know what needs to be updated",
      "You're checking for circular dependencies"
    ],
    inputSchema: {
      filePath: {
        type: "string",
        description: "Absolute or relative path to the C++ source file (.cpp, .h, .hpp)",
        required: true
      }
    },
    examples: [
      {
        input: { filePath: "src/main.cpp" },
        output: {
          filePath: "src/main.cpp",
          headersFound: 5,
          headers: ["iostream", "vector", "string", "memory", "algorithm"],
          summary: "Found 5 header includes in src/main.cpp"
        },
        explanation: "Find all standard library headers included in main.cpp"
      },
      {
        input: { filePath: "include/myclass.h" },
        output: {
          filePath: "include/myclass.h",
          headersFound: 3,
          headers: ["string", "vector", "../external/json.hpp"],
          summary: "Found 3 header includes in include/myclass.h"
        },
        explanation: "Analyze a header file's dependencies including relative includes"
      }
    ]
  },

  checkCpp: {
    name: "checkCpp",
    description: "Perform static analysis on C++ files to detect common issues: unclosed braces, TODO/FIXME comments, hardcoded Windows paths, and other code quality concerns.",
    useWhen: [
      "You want to validate a C++ file for common errors",
      "You're debugging and suspect syntax issues",
      "You need to find TODO/FIXME items in code",
      "You want to check for hardcoded paths that should be portable",
      "You're doing code review preparation"
    ],
    inputSchema: {
      filePath: {
        type: "string",
        description: "Path to the C++ file to analyze",
        required: true
      }
    },
    examples: [
      {
        input: { filePath: "src/broken.cpp" },
        output: {
          filePath: "src/broken.cpp",
          totalLines: 150,
          issuesFound: 2,
          issues: [
            { line: 45, type: "note", message: "TODO/FIXME found" },
            { line: 150, type: "error", message: "Unclosed braces: 1" }
          ],
          status: "has_issues"
        },
        explanation: "Detect an unclosed brace and a TODO comment"
      },
      {
        input: { filePath: "src/clean.cpp" },
        output: {
          filePath: "src/clean.cpp",
          totalLines: 200,
          issuesFound: 0,
          issues: [],
          status: "clean"
        },
        explanation: "A file with no detected issues"
      }
    ]
  },

  getBuildCommand: {
    name: "getBuildCommand",
    description: "Auto-detect the project's build system and return build commands for sunone_aimbot_2. Supports CUDA (TensorRT) and DML (DirectML) backends. Returns configured build commands, clean commands, and output paths.",
    useWhen: [
      "You need to compile sunone_aimbot_2 but don't know the commands",
      "You need to build the CUDA or DML backend",
      "You need to clean and rebuild the project",
      "You want to know the output executable path",
      "You're troubleshooting CUDA toolset or build issues"
    ],
    inputSchema: {
      backend: {
        type: "string",
        description: "Build backend: 'auto' (default), 'cuda', or 'dml'. Auto selects based on existing build directories.",
        required: false,
        default: "auto"
      }
    },
    examples: [
      {
        input: { backend: "cuda" },
        output: {
          buildCommand: "cmake --build build/cuda --config Release",
          rebuildCommand: "Remove-Item build/cuda -Recurse -Force; cmake -S . -B build/cuda -T \"cuda=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1\" -DAIMBOT_USE_CUDA=ON && cmake --build build/cuda --config Release",
          type: "cmake-cuda",
          output: "build/cuda/Release/ai.exe",
          configured: true
        },
        explanation: "CUDA build with TensorRT backend - uses explicit CUDA 13.1 toolset"
      },
      {
        input: { backend: "dml" },
        output: {
          buildCommand: "cmake --build build/dml --config Release",
          type: "cmake-dml",
          output: "build/dml/Release/ai.exe",
          configured: true
        },
        explanation: "DML build with DirectML backend - works on all GPUs"
      },
      {
        input: { backend: "auto" },
        output: {
          buildCommand: "cmake --build build/cuda --config Release",
          type: "cmake-cuda",
          configured: true
        },
        explanation: "Auto selects CUDA if build/cuda exists and is configured"
      }
    ]
  },

  searchSymbols: {
    name: "searchSymbols",
    description: "Search for C++ symbols (classes, functions, variables) across the codebase. Returns file locations and line numbers where symbols are defined or used.",
    useWhen: [
      "You need to find where a class or function is defined",
      "You're looking for all usages of a symbol",
      "You want to navigate to a specific symbol's definition",
      "You're refactoring and need to find all references"
    ],
    inputSchema: {
      symbol: {
        type: "string",
        description: "The symbol name to search for (class name, function name, variable name)",
        required: true
      },
      type: {
        type: "string",
        description: "Optional filter: 'class', 'function', 'variable', or 'all'",
        required: false
      }
    },
    examples: [
      {
        input: { symbol: "MyClass", type: "class" },
        output: {
          symbol: "MyClass",
          results: [
            { file: "include/myclass.h", line: 15, type: "definition" },
            { file: "src/myclass.cpp", line: 5, type: "implementation" }
          ],
          totalFound: 2
        },
        explanation: "Find where MyClass is defined and implemented"
      }
    ]
  },

  analyzeIncludes: {
    name: "analyzeIncludes",
    description: "Analyze the include structure of the entire project. Detects circular dependencies, unused includes, and suggests optimization opportunities.",
    useWhen: [
      "You want to check for circular include dependencies",
      "You're optimizing compile times",
      "You need to understand the project's include hierarchy",
      "You're refactoring header organization"
    ],
    inputSchema: {
      entryPoint: {
        type: "string",
        description: "Starting file or directory to analyze",
        required: false
      }
    },
    examples: [
      {
        input: { entryPoint: "src" },
        output: {
          totalFiles: 45,
          circularDependencies: [
            ["a.h", "b.h", "a.h"]
          ],
          unusedIncludes: [
            { file: "src/old.cpp", includes: ["deprecated.h"] }
          ],
          summary: "Found 1 circular dependency and 1 unused include"
        },
        explanation: "Analyze include structure starting from src directory"
      }
    ]
  }
}

export function getAllToolNames(): string[] {
  return Object.keys(TOOL_REGISTRY)
}

export function getToolInfo(name: string): ToolInfo | undefined {
  return TOOL_REGISTRY[name]
}
