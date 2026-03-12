---
name: oc-plugin-creator
description: Use when creating OpenCode plugins - provides plugin architecture, hook API specifications, event handling patterns, custom tool creation, TypeScript/JavaScript module structure, and distribution guidance for extending OpenCode with event-driven plugins.
---

# OpenCode Plugin Creator

Create powerful plugins to extend OpenCode's behavior through event hooks, custom tools, and session management.

## Philosophy: Event-Driven Extension

OpenCode plugins are **event-driven modules** that hook into the agent's lifecycle. Think of them as interceptors that can observe, modify, or block operations.

**Before creating a plugin, ask:**
- What event or lifecycle stage should trigger my plugin?
- Should I observe (log/react) or intercept (modify/block)?
- Does this need state across the session or work statelessly?
- Will this be distributed via npm or remain local?

**Core principles:**
1. **Hook into events, don't replace behavior**: Plugins extend, they don't override core functionality
2. **Fail gracefully**: Throwing in hooks can break workflows - use with purpose
3. **Session-aware state**: Use Maps keyed by sessionID, not global variables
4. **Async everywhere**: All hooks are async - use `await` for operations
5. **Type safety first**: Use `@opencode-ai/plugin` for types and validation

## Plugin Fundamentals

### Basic Structure

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const MyPlugin: Plugin = async ({ client, project, $, directory, worktree }) => {
  console.log("Plugin initialized!")
  
  return {
    // Hook implementations go here
  }
}
```

**Context object properties:**
- `client`: SDK Client for OpenCode API calls
- `project`: Current project information
- `directory`: string - Current working directory
- `worktree`: string - Git worktree path
- `$`: Bun's shell API for executing commands

**CRITICAL**: The plugin function receives a **context object**, not individual parameters:

```typescript
// ✅ CORRECT - destructure what you need
export const MyPlugin: Plugin = async ({ client, project, $ }) => { }

// ❌ WRONG - treating context as client
export const MyPlugin: Plugin = async (client) => { } // FAILS
```

### Installation Locations

Plugins load from two locations (automatically at startup):

| Location | Scope | Path |
|----------|-------|------|
| Project | Local to current project | `.opencode/plugins/` |
| Global | All projects | `~/.config/opencode/plugins/` |

### NPM Distribution

For wider distribution, publish to npm and add to `opencode.json`:

```json
{
  "$schema": "https://opencode.ai/config.json",
  "plugin": ["opencode-helicone-session", "@my-org/custom-plugin"]
}
```

NPM plugins are auto-installed using Bun at startup. Dependencies are cached in `~/.cache/opencode/node_modules/`.

## Available Hooks

### Event Hook (Catch-all)

Subscribe to all system events:

```typescript
return {
  event: async ({ event }) => {
    if (event.type === "session.idle") {
      // Agent finished responding
    }
    if (event.type === "file.edited") {
      // File was modified
    }
  }
}
```

**Event categories:**

| Category | Events |
|----------|--------|
| Session | `session.created`, `session.deleted`, `session.idle`, `session.error`, `session.compacted`, `session.updated`, `session.diff` |
| Message | `message.updated`, `message.removed`, `message.part.updated` |
| Tool | `tool.execute.before`, `tool.execute.after` |
| File | `file.edited`, `file.watcher.updated` |
| Permission | `permission.asked`, `permission.replied` |
| LSP | `lsp.client.diagnostics`, `lsp.updated` |
| Shell | `shell.env` |
| TUI | `tui.prompt.append`, `tui.command.execute`, `tui.toast.show` |
| Todo | `todo.updated` |
| Command | `command.executed` |
| Server | `server.connected` |
| Installation | `installation.updated` |

### Tool Execution Hooks

Intercept tool calls before/after execution:

```typescript
return {
  "tool.execute.before": async (input, output) => {
    // Modify or block tool execution
    if (input.tool === "bash" && output.args.command.includes("rm -rf")) {
      throw new Error("Dangerous command blocked")
    }
  },
  
  "tool.execute.after": async (input) => {
    // React to completed tool calls
    if (input.tool === "edit") {
      console.log(`File edited: ${input.args.filePath}`)
    }
  }
}
```

### Stop Hook

Intercept when agent attempts to stop (session goes idle):

```typescript
return {
  stop: async (input) => {
    const sessionId = input.sessionID || input.session_id
    
    // Check if work is complete
    if (!workComplete) {
      // Prompt agent to continue
      await client.session.prompt({
        path: { id: sessionId },
        body: {
          parts: [{ type: "text", text: "Please complete X before stopping." }]
        }
      })
    }
  }
}
```

### System Prompt Transform (Experimental)

Inject context into the system prompt:

```typescript
return {
  "experimental.chat.system.transform": async (input, output) => {
    output.system.push(`<custom-context>
      Important project rules go here.
    </custom-context>`)
  }
}
```

### Compaction Hooks (Experimental)

Preserve state when sessions are compacted:

```typescript
return {
  "experimental.session.compacting": async (input, output) => {
    // Add context to preserve
    output.context.push(`<preserved-state>
      Task progress: 75%
      Files modified: src/main.ts
    </preserved-state>`)
    
    // Or replace entire compaction prompt
    output.prompt = "Custom compaction instructions..."
  }
}
```

## Custom Tools

Plugins can add tools the LLM can call:

```typescript
import { type Plugin, tool } from "@opencode-ai/plugin"

export const CustomToolsPlugin: Plugin = async (ctx) => {
  return {
    tool: {
      myTool: tool({
        description: "Does something useful",
        args: {
          input: tool.schema.string(),
          count: tool.schema.number().optional(),
        },
        async execute(args, context) {
          const { directory, worktree, agent, sessionID, messageID } = context
          return `Processed: ${args.input}`
        }
      })
    }
  }
}
```

**Tool schema types:**
- `tool.schema.string()` - String values
- `tool.schema.number()` - Numeric values
- `tool.schema.boolean()` - True/false
- `tool.schema.array(itemType)` - Arrays
- `tool.schema.object({...})` - Objects
- `.optional()` - Make any type optional
- `.describe("text")` - Add description for LLM

## Session State Management

Track state across a session using Maps keyed by session ID:

```typescript
interface SessionState {
  filesModified: string[]
  commitMade: boolean
}

const sessions = new Map<string, SessionState>()

function getState(sessionId: string): SessionState {
  let state = sessions.get(sessionId)
  if (!state) {
    state = { filesModified: [], commitMade: false }
    sessions.set(sessionId, state)
  }
  return state
}

export const MyPlugin: Plugin = async ({ client }) => {
  return {
    event: async ({ event }) => {
      const sessionId = (event as any).session_id || (event as any).sessionID
      
      if (event.type === "session.created" && sessionId) {
        sessions.set(sessionId, { filesModified: [], commitMade: false })
      }
      
      if (event.type === "session.deleted" && sessionId) {
        sessions.delete(sessionId)
      }
    },
    
    "tool.execute.after": async (input) => {
      const state = getState(input.sessionID)
      
      if (input.tool === "edit" || input.tool === "write") {
        state.filesModified.push(input.args.filePath as string)
      }
    }
  }
}
```

## Dependencies

Local plugins can use external npm packages. Add a `package.json` to your config directory:

```json
{
  "dependencies": {
    "@opencode-ai/plugin": "latest",
    "shescape": "^2.1.0"
  }
}
```

OpenCode runs `bun install` at startup.

**Example with external dependency:**

```typescript
import { escape } from "shescape"

export const MyPlugin = async (ctx) => {
  return {
    "tool.execute.before": async (input, output) => {
      if (input.tool === "bash") {
        output.args.command = escape(output.args.command)
      }
    }
  }
}
```

## SDK Integration

Use the SDK for programmatic control:

```typescript
import type { Plugin } from "@opencode-ai/plugin"

export const SDKPlugin: Plugin = async ({ client }) => {
  // Log structured messages
  await client.app.log({
    body: {
      service: "my-plugin",
      level: "info", // debug, info, warn, error
      message: "Plugin initialized",
      extra: { key: "value" }
    }
  })
  
  // Send prompt to session
  await client.session.prompt({
    path: { id: sessionId },
    body: {
      parts: [{ type: "text", text: "Message to agent" }]
    }
  })
  
  return { /* hooks */ }
}
```

**SDK APIs available:**
- `client.global.health()` - Check server health
- `client.app.log()` - Structured logging
- `client.app.agents()` - List available agents
- `client.session.*` - Session management
- `client.find.text()` / `client.find.files()` - Search
- `client.file.read()` - Read files
- `client.tui.*` - Control TUI interface
- `client.event.subscribe()` - Listen to real-time events

## Common Patterns

### File Modification Tracker

```typescript
"tool.execute.after": async (input) => {
  if (input.tool === "edit" || input.tool === "write") {
    const filePath = input.args.filePath as string
    // Track the modification
  }
}
```

### Command Interceptor

```typescript
"tool.execute.before": async (input, output) => {
  if (input.tool === "bash" && /git commit/.test(output.args.command as string)) {
    if (!state.testsRan) {
      throw new Error("Run tests before committing!")
    }
  }
}
```

### Image Detection

```typescript
event: async ({ event }) => {
  if (event.type === "message.updated") {
    const message = (event as any).properties?.message
    if (message?.role === "user") {
      const content = JSON.stringify(message.content || "")
      if (content.includes("image/") || /\.(png|jpg|jpeg|gif|webp)/i.test(content)) {
        // User provided an image
      }
    }
  }
}
```

## Anti-Patterns to Avoid

❌ **Wrong destructuring**: Treating context as client directly
```typescript
// WRONG
export const BadPlugin = async (client) => {
  await client.session.prompt() // FAILS
}
```

❌ **Global state**: Using global variables instead of session-keyed Maps
```typescript
// WRONG
let filesModified = [] // Shared across ALL sessions!
```

❌ **Blocking without reason**: Throwing in hooks unnecessarily breaks workflows
```typescript
// WRONG - be careful with throwing
"tool.execute.before": async (input) => {
  throw new Error("Blocked!") // Stops everything
}
```

❌ **Synchronous operations**: Hooks must be async
```typescript
// WRONG
return {
  "tool.execute.after": (input) => { // Missing async
    const result = fs.readFileSync(...) // Blocking!
  }
}
```

❌ **Missing type imports**: Not using `@opencode-ai/plugin` types
```typescript
// WRONG - no type safety
export const UntypedPlugin = async (ctx) => { }
```

## Load Order

Plugins load in this sequence:

1. Global config (`~/.config/opencode/opencode.json`)
2. Project config (`opencode.json`)
3. Global plugin directory (`~/.config/opencode/plugins/`)
4. Project plugin directory (`.opencode/plugins/`)

All hooks from all plugins run sequentially.

## Debugging Tips

1. **Plugin not loading?** Check for TypeScript errors - syntax errors prevent loading
2. **Hooks not firing?** Verify hook name matches exactly (case-sensitive)
3. **State not persisting?** Use session-keyed Maps, not global variables
4. **SDK calls failing?** Check destructuring: `async ({ client })` not `async (client)`
5. **Dependencies not found?** Ensure `package.json` is in config directory, not plugin directory

## Variation Guidance

Plugin implementations should vary based on use case:

- **Observation plugins** (logging, metrics): Use `event` hook and `tool.execute.after`
- **Interception plugins** (blocking, modifying): Use `tool.execute.before` with validation
- **Workflow plugins** (enforcing processes): Use `stop` hook with state management
- **Integration plugins** (external services): Use SDK client for external APIs
- **Tool plugins** (new capabilities): Export tools with clear, focused purposes

Avoid creating plugins that duplicate built-in functionality or add unnecessary complexity.

## Remember

OpenCode plugins unlock powerful customization through event-driven architecture. Master the hook system, respect session boundaries, and build extensions that enhance the agent experience without getting in the way.

**Start simple**: Log events. Then intercept. Then transform. Build up complexity as needed.

See `references/` for detailed API documentation, advanced patterns, and example implementations.
