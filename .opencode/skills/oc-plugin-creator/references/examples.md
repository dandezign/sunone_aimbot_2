# OpenCode Plugin Examples

Complete, working examples of common plugin patterns.

## Example 1: Commit Reminder Plugin

Reminds users to commit when they have uncommitted changes.

```typescript
// .opencode/plugins/commit-reminder.ts
import type { Plugin } from "@opencode-ai/plugin"

interface SessionState {
  filesModified: string[]
  commitMade: boolean
}

const sessions = new Map<string, SessionState>()

export const CommitReminder: Plugin = async ({ client }) => {
  return {
    event: async ({ event }) => {
      const sessionId = (event as any).session_id
      
      if (event.type === "session.created" && sessionId) {
        sessions.set(sessionId, { filesModified: [], commitMade: false })
      }
      
      if (event.type === "session.deleted" && sessionId) {
        sessions.delete(sessionId)
      }
    },

    "tool.execute.after": async (input) => {
      const state = sessions.get(input.sessionID)
      if (!state) return

      if (input.tool === "edit" || input.tool === "write") {
        const path = input.args?.filePath as string
        if (path && !state.filesModified.includes(path)) {
          state.filesModified.push(path)
        }
      }

      if (input.tool === "bash") {
        const cmd = input.args?.command as string
        if (/git\s+commit/.test(cmd)) {
          state.commitMade = true
        }
      }
    },

    stop: async (input) => {
      const sessionId = (input as any).sessionID || (input as any).session_id
      if (!sessionId) return

      const state = sessions.get(sessionId)
      if (!state) return

      if (state.filesModified.length > 0 && !state.commitMade) {
        await client.session.prompt({
          path: { id: sessionId },
          body: {
            parts: [{
              type: "text",
              text: `## Uncommitted Changes\n\nYou modified ${state.filesModified.length} file(s) but haven't committed. Please commit before stopping.`
            }]
          }
        })
      }
    }
  }
}
```

## Example 2: Environment Protection

Prevents reading sensitive `.env` files.

```typescript
// .opencode/plugins/env-protection.ts
import type { Plugin } from "@opencode-ai/plugin"

export const EnvProtection: Plugin = async () => {
  return {
    "tool.execute.before": async (input, output) => {
      if (input.tool === "read" && output.args.filePath.includes(".env")) {
        throw new Error("Do not read .env files")
      }
    }
  }
}
```

## Example 3: Custom Tool - Database Query

Adds a tool for querying project database.

```typescript
// .opencode/plugins/database-tool.ts
import { type Plugin, tool } from "@opencode-ai/plugin"

export const DatabaseToolPlugin: Plugin = async () => {
  return {
    tool: {
      queryDatabase: tool({
        description: "Query the project SQLite database",
        args: {
          query: tool.schema.string().describe("SQL SELECT query"),
        },
        async execute(args, context) {
          const { worktree } = context
          const dbPath = `${worktree}/data/app.db`
          
          // Example using Bun's shell
          const result = await Bun.$`sqlite3 ${dbPath} "${args.query}"`.text()
          return result
        }
      })
    }
  }
}
```

## Example 4: Notification Plugin

Sends system notifications on session completion.

```typescript
// .opencode/plugins/notifications.ts
import type { Plugin } from "@opencode-ai/plugin"

export const NotificationPlugin: Plugin = async ({ $ }) => {
  return {
    event: async ({ event }) => {
      // Send notification on session completion
      if (event.type === "session.idle") {
        // macOS
        await $`osascript -e 'display notification "Session completed!" with title "opencode"'`
      }
      
      if (event.type === "session.error") {
        await $`osascript -e 'display notification "Session error!" with title "opencode"' sound name "Basso"'`
      }
    }
  }
}
```

## Example 5: Auto-format on Save

Runs formatter after file edits.

```typescript
// .opencode/plugins/auto-format.ts
import type { Plugin } from "@opencode-ai/plugin"

export const AutoFormat: Plugin = async ({ $ }) => {
  return {
    "tool.execute.after": async (input) => {
      if (input.tool === "edit" || input.tool === "write") {
        const filePath = input.args.filePath as string
        
        if (filePath.endsWith(".rs")) {
          await $`cargo fmt`.quiet()
        } else if (filePath.endsWith(".ts") || filePath.endsWith(".js")) {
          await $`prettier --write ${filePath}`.quiet()
        } else if (filePath.endsWith(".py")) {
          await $`black ${filePath}`.quiet()
        }
      }
    }
  }
}
```

## Example 6: Test Before Commit Guard

Enforces running tests before git commit.

```typescript
// .opencode/plugins/test-guard.ts
import type { Plugin } from "@opencode-ai/plugin"

interface SessionState {
  testsPassed: boolean
}

const sessions = new Map<string, SessionState>()

export const TestGuard: Plugin = async () => {
  return {
    event: async ({ event }) => {
      const sessionId = (event as any).session_id
      
      if (event.type === "session.created" && sessionId) {
        sessions.set(sessionId, { testsPassed: false })
      }
      
      if (event.type === "session.deleted" && sessionId) {
        sessions.delete(sessionId)
      }
    },

    "tool.execute.after": async (input) => {
      const state = sessions.get(input.sessionID)
      if (!state) return

      if (input.tool === "bash") {
        const cmd = input.args?.command as string
        // Track if tests were run and passed
        if (/npm test|cargo test|pytest|jest/.test(cmd)) {
          // In real implementation, check exit code
          state.testsPassed = true
        }
      }
    },

    "tool.execute.before": async (input, output) => {
      const state = sessions.get(input.sessionID)
      if (!state) return

      if (input.tool === "bash") {
        const cmd = output.args?.command as string
        if (/git\s+commit/.test(cmd) && !state.testsPassed) {
          throw new Error("Tests must pass before committing! Run your test suite first.")
        }
      }
    }
  }
}
```

## Example 7: Custom Compaction

Preserves important context during session compaction.

```typescript
// .opencode/plugins/smart-compaction.ts
import type { Plugin } from "@opencode-ai/plugin"

interface SessionState {
  decisions: string[]
  activeFiles: string[]
}

const sessions = new Map<string, SessionState>()

export const SmartCompaction: Plugin = async () => {
  return {
    event: async ({ event }) => {
      const sessionId = (event as any).session_id
      
      if (event.type === "session.created" && sessionId) {
        sessions.set(sessionId, { decisions: [], activeFiles: [] })
      }
    },

    "tool.execute.after": async (input) => {
      const state = sessions.get(input.sessionID)
      if (!state) return

      if (input.tool === "edit" || input.tool === "write") {
        const path = input.args?.filePath as string
        if (path && !state.activeFiles.includes(path)) {
          state.activeFiles.push(path)
        }
      }
    },

    "experimental.session.compacting": async (input, output) => {
      const sessionId = (input as any).sessionID || (input as any).session_id
      const state = sessions.get(sessionId)
      
      if (state && state.activeFiles.length > 0) {
        output.context.push(`## Currently Active Files\n${state.activeFiles.map(f => `- ${f}`).join("\n")}`)
      }
    }
  }
}
```

## Example 8: Multi-Tool Plugin

Creates multiple related tools in one plugin.

```typescript
// .opencode/plugins/project-tools.ts
import { type Plugin, tool } from "@opencode-ai/plugin"

export const ProjectTools: Plugin = async () => {
  return {
    tool: {
      getPackageVersion: tool({
        description: "Get version from package.json",
        args: {
          field: tool.schema.string().optional().describe("Field to read (default: version)")
        },
        async execute(args, context) {
          const { worktree } = context
          const pkg = await import(`${worktree}/package.json`)
          return pkg[args.field || "version"] || "not found"
        }
      }),
      
      countLines: tool({
        description: "Count lines of code in a file",
        args: {
          path: tool.schema.string().describe("File path")
        },
        async execute(args, context) {
          const { $, worktree } = context
          const result = await $`wc -l ${worktree}/${args.path}`.text()
          return result.trim()
        }
      })
    }
  }
}
```
