import { tool } from "@opencode-ai/plugin"
import { TOOL_REGISTRY, getAllToolNames } from "./tool-registry"

function formatToolOutput(payload: unknown): string {
  return JSON.stringify(payload, null, 2)
}

// Path to scripts (relative to plugin directory)
const SCRIPTS_DIR = ".opencode/plugins/tool-search/scripts"

// Tool-to-script mapping
export const TOOL_SCRIPTS: Record<string, {
  scriptPath: string
  commandTemplate: string
  description: string
  requiresAdmin: boolean
}> = {
  buildCuda: {
    scriptPath: `${SCRIPTS_DIR}/Build-Cuda.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Build-Cuda.ps1`,
    description: "Build sunone_aimbot_2 with CUDA/TensorRT backend",
    requiresAdmin: false
  },
  buildCudaRebuild: {
    scriptPath: `${SCRIPTS_DIR}/Build-Cuda.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Build-Cuda.ps1 -Rebuild`,
    description: "Clean and rebuild sunone_aimbot_2 with CUDA/TensorRT",
    requiresAdmin: false
  },
  buildCudaParallel: {
    scriptPath: `${SCRIPTS_DIR}/Build-Cuda.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Build-Cuda.ps1 -Parallel`,
    description: "Build sunone_aimbot_2 with CUDA using parallel jobs",
    requiresAdmin: false
  },
  buildDml: {
    scriptPath: `${SCRIPTS_DIR}/Build-Dml.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Build-Dml.ps1`,
    description: "Build sunone_aimbot_2 with DirectML backend",
    requiresAdmin: false
  },
  buildDmlRebuild: {
    scriptPath: `${SCRIPTS_DIR}/Build-Dml.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Build-Dml.ps1 -Rebuild`,
    description: "Clean and rebuild sunone_aimbot_2 with DirectML",
    requiresAdmin: false
  },
  findHeaders: {
    scriptPath: `${SCRIPTS_DIR}/Find-Headers.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Find-Headers.ps1 -FilePath "{{filePath}}"`,
    description: "Extract #include dependencies from a C++ file",
    requiresAdmin: false
  },
  checkCpp: {
    scriptPath: `${SCRIPTS_DIR}/Check-Cpp.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Check-Cpp.ps1 -FilePath "{{filePath}}"`,
    description: "Perform static analysis on a C++ file",
    requiresAdmin: false
  },
  searchSymbols: {
    scriptPath: `${SCRIPTS_DIR}/Search-Symbols.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Search-Symbols.ps1 -Symbol "{{symbol}}" -Type "{{type}}"`,
    description: "Search for C++ symbols across codebase",
    requiresAdmin: false
  },
  analyzeIncludes: {
    scriptPath: `${SCRIPTS_DIR}/Analyze-Includes.ps1`,
    commandTemplate: `powershell -ExecutionPolicy Bypass -File ${SCRIPTS_DIR}/Analyze-Includes.ps1 -EntryPoint "{{entryPoint}}"`,
    description: "Analyze include structure for circular dependencies",
    requiresAdmin: false
  }
}

export { TOOL_REGISTRY, getAllToolNames }

// Tool Search Tool - Returns PowerShell commands/scripts
export const toolSearch = tool({
  description: `Search for C++ development commands and scripts.

This tool returns PowerShell commands/scripts that the agent can execute via bash.
Instead of calling individual tools, you receive actual command strings to run.

Returns:
- PowerShell commands to execute
- Script file paths
- Parameter substitutions needed
- Execution instructions

The agent should execute the returned commands using the bash tool.

Examples:
- "build the project" → returns: powershell -ExecutionPolicy Bypass -File .opencode/plugins/tool-search/scripts/Build-Cuda.ps1
- "find headers in main.cpp" → returns: powershell ...Find-Headers.ps1 -FilePath "main.cpp"
- "check for errors" → returns: powershell ...Check-Cpp.ps1 -FilePath "..."`,
  args: {
    query: tool.schema.string().describe(
      "Natural language description of what you want to accomplish. " +
      "Examples: 'build with CUDA', 'find headers in src/main.cpp', 'check cpp errors', " +
      "'search for class MyClass', 'detect circular includes'"
    ),
    parameters: tool.schema.object({
      filePath: tool.schema.string().optional(),
      symbol: tool.schema.string().optional(),
      type: tool.schema.string().optional(),
      entryPoint: tool.schema.string().optional(),
      backend: tool.schema.string().optional()
    }).optional().describe(
      "Optional parameters for the command (filePath, symbol, type, entryPoint, backend)"
    ),
    loadIntoContext: tool.schema.boolean().optional().default(true).describe(
      "Reserved for compatibility with plugin wrappers that cache commands"
    ),
    context: tool.schema.string().optional().describe(
      "Additional context about your task (optional)"
    )
  },
  async execute(args) {
    const { query, parameters = {} } = args as {
      query: string
      parameters?: { filePath?: string; symbol?: string; type?: string; entryPoint?: string; backend?: string }
    }
    const queryLower = query.toLowerCase()
    
    // Command matching logic
    const commandMatches: Array<{
      command: typeof TOOL_SCRIPTS[keyof typeof TOOL_SCRIPTS]
      relevance: number
      key: string
      parameters: Record<string, string>
    }> = []

    // Intent mapping
    const intents: Record<string, { commands: string[], boost: number }> = {
      // Build intents
      "build cuda": { commands: ["buildCuda", "buildCudaParallel"], boost: 20 },
      "cuda build": { commands: ["buildCuda"], boost: 20 },
      "build with cuda": { commands: ["buildCuda"], boost: 20 },
      "rebuild cuda": { commands: ["buildCudaRebuild"], boost: 25 },
      "clean cuda": { commands: ["buildCudaRebuild"], boost: 20 },
      "build dml": { commands: ["buildDml"], boost: 20 },
      "dml build": { commands: ["buildDml"], boost: 20 },
      "directml build": { commands: ["buildDml"], boost: 20 },
      "rebuild dml": { commands: ["buildDmlRebuild"], boost: 25 },
      "build project": { commands: ["buildCuda", "buildDml"], boost: 18 },
      "compile": { commands: ["buildCuda", "buildDml"], boost: 18 },
      
      // Header analysis
      "find headers": { commands: ["findHeaders"], boost: 20 },
      "get includes": { commands: ["findHeaders"], boost: 18 },
      "dependencies": { commands: ["findHeaders", "analyzeIncludes"], boost: 15 },
      "includes": { commands: ["findHeaders", "analyzeIncludes"], boost: 15 },
      
      // Code checking
      "check cpp": { commands: ["checkCpp"], boost: 20 },
      "check file": { commands: ["checkCpp"], boost: 18 },
      "static analysis": { commands: ["checkCpp"], boost: 18 },
      "find errors": { commands: ["checkCpp"], boost: 15 },
      "lint": { commands: ["checkCpp"], boost: 15 },
      "validate": { commands: ["checkCpp"], boost: 15 },
      
      // Symbol search
      "find class": { commands: ["searchSymbols"], boost: 20 },
      "find function": { commands: ["searchSymbols"], boost: 20 },
      "search symbol": { commands: ["searchSymbols"], boost: 18 },
      "find symbol": { commands: ["searchSymbols"], boost: 18 },
      "definition": { commands: ["searchSymbols"], boost: 15 },
      
      // Include analysis
      "circular": { commands: ["analyzeIncludes"], boost: 20 },
      "circular dependency": { commands: ["analyzeIncludes"], boost: 22 },
      "analyze includes": { commands: ["analyzeIncludes"], boost: 20 },
      "include structure": { commands: ["analyzeIncludes"], boost: 18 }
    }

    // Match intents
    for (const [intent, config] of Object.entries(intents)) {
      if (queryLower.includes(intent)) {
        for (const cmdKey of config.commands) {
          const cmd = TOOL_SCRIPTS[cmdKey]
          if (cmd) {
            const existing = commandMatches.find(m => m.key === cmdKey)
            if (existing) {
              existing.relevance += config.boost
            } else {
              commandMatches.push({
                command: cmd,
                relevance: config.boost,
                key: cmdKey,
                parameters: {}
              })
            }
          }
        }
      }
    }

    // Fill in parameters
    for (const match of commandMatches) {
      // Extract parameters from the query using regex
      const fileMatch = query.match(/(?:in|for|of)\s+["']?([^"']+\.(?:cpp|h|hpp|c))["']?/i)
      const symbolMatch = query.match(/(?:class|function|symbol)\s+["']?(\w+)["']?/i)
      
      // Apply provided parameters
      if (parameters.filePath || fileMatch) {
        match.parameters.filePath = parameters.filePath || fileMatch?.[1] || ""
      }
      if (parameters.symbol || symbolMatch) {
        match.parameters.symbol = parameters.symbol || symbolMatch?.[1] || ""
      }
      if (parameters.type) {
        match.parameters.type = parameters.type
      }
      if (parameters.entryPoint) {
        match.parameters.entryPoint = parameters.entryPoint
      }
      if (parameters.backend) {
        match.parameters.backend = parameters.backend
      }
    }

    // Sort by relevance
    commandMatches.sort((a, b) => b.relevance - a.relevance)

    // Build response
    if (commandMatches.length === 0) {
      return formatToolOutput({
        success: false,
        message: "No matching commands found for your query.",
        query,
        availableCommands: Object.keys(TOOL_SCRIPTS),
        suggestion: "Try: 'build cuda', 'find headers in file.cpp', 'check cpp errors', 'search for class MyClass', 'detect circular includes'"
      })
    }

    // Generate executable commands
    const topMatches = commandMatches.slice(0, 3)
    const executableCommands = topMatches.map(match => {
      let command = match.command.commandTemplate
      
      // Replace placeholders
      for (const [key, value] of Object.entries(match.parameters)) {
        command = command.replace(new RegExp(`{{${key}}}`, 'g'), value)
      }
      
      // Remove unused placeholders
      command = command.replace(/\s+-\w+\s+"?{{\w+}}"?/g, '')
      
      return {
        key: match.key,
        description: match.command.description,
        command,
        scriptPath: match.command.scriptPath,
        relevance: match.relevance,
        parameters: match.parameters,
        requiresAdmin: match.command.requiresAdmin
      }
    })

    return formatToolOutput({
      success: true,
      query,
      matchesFound: executableCommands.length,
      recommendation: executableCommands.length > 0
        ? `Best match: ${executableCommands[0].key} (${executableCommands[0].relevance} relevance)`
        : null,
      commands: executableCommands,
      executionInstructions: {
        method: "Use the bash tool to execute the PowerShell command",
        example: executableCommands.length > 0
          ? {
              tool: "bash",
              args: {
                command: executableCommands[0].command,
                description: `Execute ${executableCommands[0].key}`
              }
            }
          : null,
        note: "All commands use -ExecutionPolicy Bypass for script execution"
      },
      nextSteps: executableCommands.map((cmd, idx) => ({
        priority: idx + 1,
        action: `Execute: ${cmd.command}`,
        description: cmd.description
      }))
    })
  }
})

export const allTools = { toolSearch }

export default toolSearch
