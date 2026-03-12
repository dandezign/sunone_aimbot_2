import { type Plugin, tool } from "@opencode-ai/plugin"
import { TOOL_SCRIPTS } from "./tool-search/tool-search"

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
  loadedAt: number
}

const sessions = new Map<string, SessionState>()

function formatToolOutput(payload: unknown): string {
  return JSON.stringify(payload, null, 2)
}

function getState(sessionId: string): SessionState {
  let state = sessions.get(sessionId)
  if (!state) {
    state = { 
      filesModified: [], 
      lastBuildStatus: true,
      loadedCommands: new Map()
    }
    sessions.set(sessionId, state)
  }
  return state
}

export const CppHelperPlugin: Plugin = async ({ client, $, directory, worktree }) => {
  console.log("C++ Helper Plugin initialized!")
  console.log("Mode: toolSearch-only (commands loaded on-demand)")

  return {
    // ============================================
    // ONLY TOOL: toolSearch (loads commands into session)
    // ============================================
    tool: {
      toolSearch: tool({
        description: `Search for C++ development commands and scripts.

This is the ONLY tool you need to call initially. It searches for relevant PowerShell commands/scripts and loads them into your session context.

After calling toolSearch, you can use the returned commands directly via bash tool.

Parameters:
- query: What you want to accomplish (e.g., "build with CUDA", "find headers in main.cpp")
- parameters: Optional values like filePath, symbol, type, entryPoint, backend
- loadIntoContext: If true (default), loads top 3 commands for direct reuse

Returns:
- PowerShell commands ready to execute
- Commands are loaded into session for quick access
- Use bash tool to execute the returned commands`,
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
            "Optional parameters for the command"
          ),
          loadIntoContext: tool.schema.boolean().optional().default(true).describe(
            "If true, loads top 3 commands into session context for direct reuse"
          )
        },
        async execute(args, context) {
          const { query, parameters = {}, loadIntoContext = true } = args as {
            query: string
            parameters?: { filePath?: string; symbol?: string; type?: string; entryPoint?: string; backend?: string }
            loadIntoContext?: boolean
          }
          const queryLower = query.toLowerCase()
          
          // Command matching logic (same as before)
          const commandMatches: Array<{
            command: typeof TOOL_SCRIPTS[keyof typeof TOOL_SCRIPTS]
            relevance: number
            key: string
            parameters: Record<string, string>
          }> = []

          // Intent mapping
          const intents: Record<string, { commands: string[], boost: number }> = {
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
            "find headers": { commands: ["findHeaders"], boost: 20 },
            "get includes": { commands: ["findHeaders"], boost: 18 },
            "dependencies": { commands: ["findHeaders", "analyzeIncludes"], boost: 15 },
            "includes": { commands: ["findHeaders", "analyzeIncludes"], boost: 15 },
            "check cpp": { commands: ["checkCpp"], boost: 20 },
            "check file": { commands: ["checkCpp"], boost: 18 },
            "static analysis": { commands: ["checkCpp"], boost: 18 },
            "find errors": { commands: ["checkCpp"], boost: 15 },
            "lint": { commands: ["checkCpp"], boost: 15 },
            "validate": { commands: ["checkCpp"], boost: 15 },
            "find class": { commands: ["searchSymbols"], boost: 20 },
            "find function": { commands: ["searchSymbols"], boost: 20 },
            "search symbol": { commands: ["searchSymbols"], boost: 18 },
            "find symbol": { commands: ["searchSymbols"], boost: 18 },
            "definition": { commands: ["searchSymbols"], boost: 15 },
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
            const fileMatch = query.match(/(?:in|for|of)\s+["']?([^"']+\.(?:cpp|h|hpp|c))["']?/i)
            const symbolMatch = query.match(/(?:class|function|symbol)\s+["']?(\w+)["']?/i)
            
            if (parameters.filePath || fileMatch) {
              match.parameters.filePath = parameters.filePath || fileMatch?.[1] || ""
            }
            if (parameters.symbol || symbolMatch) {
              match.parameters.symbol = parameters.symbol || symbolMatch?.[1] || ""
            }
            if (parameters.type) match.parameters.type = parameters.type
            if (parameters.entryPoint) match.parameters.entryPoint = parameters.entryPoint
            if (parameters.backend) match.parameters.backend = parameters.backend
          }

          // Sort by relevance
          commandMatches.sort((a, b) => b.relevance - a.relevance)

          if (commandMatches.length === 0) {
            return formatToolOutput({
              success: false,
              message: "No matching commands found for your query.",
              query,
              availableCommands: Object.keys(TOOL_SCRIPTS),
              suggestion: "Try: 'build cuda', 'find headers in file.cpp', 'check cpp errors', 'search for class MyClass', 'detect circular includes'"
            })
          }

          // Get top matches
          const topMatches = commandMatches.slice(0, 5)
          
          const state = loadIntoContext ? getState(context.sessionID) : null

          // Load into session context if requested
          if (state) {
            topMatches.forEach(match => {
              let command = match.command.commandTemplate
              for (const [key, value] of Object.entries(match.parameters)) {
                command = command.replace(new RegExp(`{{${key}}}`, 'g'), value)
              }
              command = command.replace(/\s+-\w+\s+"?{{\w+}}"?/g, '')
              
              state.loadedCommands.set(match.key, {
                key: match.key,
                description: match.command.description,
                command,
                scriptPath: match.command.scriptPath,
                parameters: match.parameters,
                loadedAt: Date.now()
              })
            })
          }

          // Generate executable commands
          const executableCommands = topMatches.map(match => {
            let command = match.command.commandTemplate
            for (const [key, value] of Object.entries(match.parameters)) {
              command = command.replace(new RegExp(`{{${key}}}`, 'g'), value)
            }
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
            loadedIntoContext: loadIntoContext,
            recommendation: executableCommands.length > 0
              ? `Best match: ${executableCommands[0].key} (${executableCommands[0].relevance} relevance)`
              : null,
            commands: executableCommands,
            loadedCommands: state
              ? Array.from(state.loadedCommands.values()).map(c => ({
                  key: c.key,
                  description: c.description,
                  command: c.command
                }))
              : [],
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
              note: "Commands loaded into context can be executed directly via bash"
            },
            nextSteps: executableCommands.map((cmd, idx) => ({
              priority: idx + 1,
              action: `Execute: ${cmd.command}`,
              description: cmd.description
            }))
          })
        }
      })
    },

    // ============================================
    // HOOKS - Session and Event Management
    // ============================================
    
    "tool.execute.after": async (input) => {
      const state = getState(input.sessionID)
      
      if (input.tool === "edit" || input.tool === "write") {
        const filePath = input.args.filePath as string
        if (filePath && /\.(cpp|h|hpp)$/.test(filePath)) {
          state.filesModified.push(filePath)
        }
      }

      if (input.tool === "bash") {
        const cmd = (input.args.command as string)?.toLowerCase() || ""
        if (cmd.includes("cmake") && (cmd.includes("--build build/cuda") || cmd.includes("--build build/dml"))) {
          state.lastBuildStatus = true
        }
      }
    },

    event: async ({ event }) => {
      const sessionId = (event as any).session_id || (event as any).sessionID
      
      if (event.type === "session.created" && sessionId) {
        sessions.set(sessionId, { 
          filesModified: [], 
          lastBuildStatus: true,
          loadedCommands: new Map()
        })
      }
      
      if (event.type === "session.deleted" && sessionId) {
        sessions.delete(sessionId)
      }

      if (event.type === "file.edited") {
        const filePath = (event as any).properties?.filePath
        if (filePath && /\.(cpp|h|hpp)$/.test(filePath)) {
          await client.app.log({
            body: {
              service: "cpp-helper",
              level: "info",
              message: `C++ file modified: ${filePath}`,
              extra: { filePath, timestamp: Date.now() }
            }
          })
        }
      }
    }
  }
}

export default CppHelperPlugin
