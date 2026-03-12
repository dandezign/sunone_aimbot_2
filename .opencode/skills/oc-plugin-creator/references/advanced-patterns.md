# Advanced OpenCode Plugin Patterns

Advanced techniques for complex plugin scenarios.

## Cross-Session Communication

Share data between sessions using files or external storage:

```typescript
import type { Plugin } from "@opencode-ai/plugin"
import { writeFile, readFile } from "fs/promises"
import { join } from "path"

export const CrossSessionPlugin: Plugin = async ({ directory }) => {
  const stateFile = join(directory, ".opencode", "shared-state.json")
  
  return {
    "tool.execute.after": async (input) => {
      if (input.tool === "edit") {
        // Read current shared state
        let sharedState = {}
        try {
          const data = await readFile(stateFile, "utf-8")
          sharedState = JSON.parse(data)
        } catch { /* file doesn't exist yet */ }
        
        // Update state
        sharedState[lastEdit] = input.args.filePath
        
        // Write back
        await writeFile(stateFile, JSON.stringify(sharedState, null, 2))
      }
    }
  }
}
```

## Background Tasks

Run async operations without blocking:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const BackgroundTaskPlugin: Plugin = async ({ client }) => {
  return {
    event: async ({ event }) => {
      if (event.type === "session.created") {
        const sessionId = (event as any).session_id
        
        // Fire-and-forget background analysis
        analyzeProject(sessionId, client).catch(console.error)
      }
    }
  }
}

async function analyzeProject(sessionId: string, client: any) {
  // Long-running analysis
  const files = await client.find.files({ query: { query: "*.ts" } })
  // ... do analysis
  
  // Inject results when done
  await client.session.prompt({
    path: { id: sessionId },
    body: {
      noReply: true,
      parts: [{ type: "text", text: "Analysis complete: ..." }]
    }
  })
}
```

## Conditional Hooks

Apply hooks only in specific contexts:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const ConditionalPlugin: Plugin = async ({ project }) => {
  // Only activate for specific project types
  const isNodeProject = await fileExists(`${project.path}/package.json`)
  
  if (!isNodeProject) {
    return {} // Empty hooks - plugin does nothing
  }
  
  return {
    "tool.execute.after": async (input) => {
      // Only runs for Node.js projects
      if (input.tool === "write" && input.args.filePath.endsWith(".js")) {
        // Handle JS file
      }
    }
  }
}
```

## Multi-Stage Workflows

Track and enforce multi-stage processes:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

interface WorkflowState {
  stage: "planning" | "implementing" | "testing" | "reviewing" | "complete"
  tasks: string[]
  completedTasks: string[]
}

const workflows = new Map<string, WorkflowState>()

export const WorkflowPlugin: Plugin = async ({ client }) => {
  return {
    event: async ({ event }) => {
      const sessionId = (event as any).session_id
      
      if (event.type === "session.created") {
        workflows.set(sessionId, {
          stage: "planning",
          tasks: ["Design architecture", "Write tests", "Implement", "Review"],
          completedTasks: []
        })
      }
    },
    
    stop: async (input) => {
      const sessionId = (input as any).sessionID
      const workflow = workflows.get(sessionId)
      
      if (!workflow) return
      
      if (workflow.stage !== "complete") {
        await client.session.prompt({
          path: { id: sessionId },
          body: {
            parts: [{
              type: "text",
              text: `Cannot stop yet. Current stage: ${workflow.stage}. Complete all tasks first.`
            }]
          }
        })
      }
    }
  }
}
```

## External API Integration

Call external services from plugins:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const ExternalAPIPlugin: Plugin = async ({ client }) => {
  return {
    tool: {
      searchDocs: tool({
        description: "Search external documentation",
        args: {
          query: tool.schema.string()
        },
        async execute(args) {
          const response = await fetch("https://api.docs.com/search", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ q: args.query })
          })
          
          if (!response.ok) {
            throw new Error(`API error: ${response.status}`)
          }
          
          const data = await response.json()
          return JSON.stringify(data.results)
        }
      })
    }
  }
}
```

## Rate Limiting

Prevent abuse of custom tools:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

const rateLimits = new Map<string, number[]>()

function checkRateLimit(sessionId: string, maxRequests: number, windowMs: number): boolean {
  const now = Date.now()
  const requests = rateLimits.get(sessionId) || []
  
  // Remove old requests outside window
  const recentRequests = requests.filter(t => now - t < windowMs)
  
  if (recentRequests.length >= maxRequests) {
    return false // Rate limited
  }
  
  recentRequests.push(now)
  rateLimits.set(sessionId, recentRequests)
  return true
}

export const RateLimitPlugin: Plugin = async () => {
  return {
    tool: {
      expensiveOperation: tool({
        description: "Expensive API call",
        args: { query: tool.schema.string() },
        async execute(args, context) {
          if (!checkRateLimit(context.sessionID, 5, 60000)) {
            throw new Error("Rate limit exceeded. Try again in a minute.")
          }
          
          // Perform expensive operation
          return "Result"
        }
      })
    }
  }
}
```

## Dynamic Tool Registration

Add tools based on project context:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const DynamicToolsPlugin: Plugin = async ({ directory }) => {
  const tools: Record<string, any> = {}
  
  // Detect project type and add relevant tools
  const hasPackageJson = await fileExists(`${directory}/package.json`)
  const hasCargoToml = await fileExists(`${directory}/Cargo.toml`)
  
  if (hasPackageJson) {
    tools.npmInstall = tool({
      description: "Install npm package",
      args: { package: tool.schema.string() },
      async execute(args, { $ }) {
        await $`npm install ${args.package}`
        return `Installed ${args.package}`
      }
    })
  }
  
  if (hasCargoToml) {
    tools.cargoAdd = tool({
      description: "Add Cargo dependency",
      args: { crate: tool.schema.string() },
      async execute(args, { $ }) {
        await $`cargo add ${args.crate}`
        return `Added ${args.crate}`
      }
    })
  }
  
  return { tool: tools }
}
```

## File Watching Integration

React to file system changes:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const FileWatchPlugin: Plugin = async ({ client, directory }) => {
  return {
    "file.watcher.updated": async ({ event }) => {
      const filePath = (event as any).properties?.path
      
      if (filePath?.endsWith(".env")) {
        await client.tui.showToast({
          body: {
            message: "Environment file changed - restart may be needed",
            variant: "warning"
          }
        })
      }
    }
  }
}
```

## Permission Middleware

Intercept and augment permission requests:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const PermissionPlugin: Plugin = async ({ client }) => {
  return {
    "permission.asked": async ({ event }) => {
      const permission = (event as any).properties?.permission
      
      // Auto-approve safe permissions
      if (isSafePermission(permission)) {
        // Note: Cannot auto-reply, but can log or prepare context
        await client.app.log({
          body: {
            service: "permission-plugin",
            level: "info",
            message: `Auto-approvable: ${permission.type}`
          }
        })
      }
    }
  }
}

function isSafePermission(permission: any): boolean {
  const safeTools = ["read", "glob", "grep"]
  return safeTools.includes(permission.tool)
}
```

## Session Persistence

Save and restore session state:

```typescript
import type { Plugin } from "@opencode-ai/plugin"
import { writeFile, readFile, mkdir } from "fs/promises"
import { join } from "path"

export const PersistencePlugin: Plugin = async ({ directory, worktree }) => {
  const stateDir = join(worktree, ".opencode", "session-states")
  await mkdir(stateDir, { recursive: true })
  
  return {
    event: async ({ event }) => {
      const sessionId = (event as any).session_id
      
      if (event.type === "session.created") {
        // Try to restore previous state
        try {
          const saved = await readFile(
            join(stateDir, `${sessionId}.json`),
            "utf-8"
          )
          const state = JSON.parse(saved)
          // Restore state...
        } catch { /* No saved state */ }
      }
      
      if (event.type === "session.compacted") {
        // Save important state before compaction
        const state = getState(sessionId)
        await writeFile(
          join(stateDir, `${sessionId}.json`),
          JSON.stringify(state)
        )
      }
    }
  }
}
```

## Error Recovery

Graceful error handling:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const ErrorRecoveryPlugin: Plugin = async ({ client }) => {
  return {
    event: async ({ event }) => {
      if (event.type === "session.error") {
        const error = (event as any).properties?.error
        const sessionId = (event as any).session_id
        
        // Log error details
        await client.app.log({
          body: {
            service: "error-recovery",
            level: "error",
            message: error.message,
            extra: { stack: error.stack }
          }
        })
        
        // Attempt recovery
        await client.session.prompt({
          path: { id: sessionId },
          body: {
            parts: [{
              type: "text",
              text: "An error occurred. Would you like to retry or try an alternative approach?"
            }]
          }
        })
      }
    }
  }
}
```

## Combining Multiple Patterns

Complex plugin with multiple features:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const AdvancedPlugin: Plugin = async (ctx) => {
  const { client, $, directory, worktree } = ctx
  
  // Initialize state
  const state = initState()
  
  return {
    // Event monitoring
    event: async ({ event }) => {
      await handleEvent(event, state, client)
    },
    
    // Tool interception
    "tool.execute.before": async (input, output) => {
      await validateTool(input, output, state)
    },
    
    // Tool reaction
    "tool.execute.after": async (input) => {
      await recordToolUse(input, state)
    },
    
    // Stop handling
    stop: async (input) => {
      await handleStop(input, state, client)
    },
    
    // Custom tools
    tool: {
      customFeature: createCustomTool(ctx, state)
    }
  }
}
```
