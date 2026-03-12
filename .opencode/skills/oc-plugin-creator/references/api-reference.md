# OpenCode Plugin API Reference

Complete reference for all hooks, types, and SDK methods available to plugins.

## Hook Reference

### Event Hook

```typescript
event: async ({ event }) => { }
```

**Event object structure:**
```typescript
{
  type: string,           // Event type name
  properties?: {          // Event-specific data
    message?: Message
    session_id?: string
    // ... other properties
  }
}
```

### Tool Execution Hooks

**Before execution:**
```typescript
"tool.execute.before": async (input, output) => { }
```

- `input`: `{ tool: string, args: Record<string, any>, sessionID: string }`
- `output`: Mutable object to modify arguments: `output.args.command = "new value"`
- Throw to block execution

**After execution:**
```typescript
"tool.execute.after": async (input) => { }
```

- `input`: `{ tool: string, args: Record<string, any>, result: any, sessionID: string }`

### Stop Hook

```typescript
stop: async (input) => { }
```

- `input`: `{ sessionID?: string, session_id?: string }`
- Use to inject follow-up prompts or block stopping

### System Transform Hook (Experimental)

```typescript
"experimental.chat.system.transform": async (input, output) => { }
```

- `output.system`: Array of strings to append to system prompt

### Compaction Hooks (Experimental)

```typescript
"experimental.session.compacting": async (input, output) => { }
```

- `output.context`: Array to append preserved context
- `output.prompt`: Replace entire compaction prompt (optional)

## Context Object

```typescript
interface PluginContext {
  client: SDKClient      // OpenCode SDK client
  project: Project       // Current project info
  directory: string      // Current working directory
  worktree: string       // Git worktree root
  $: Shell              // Bun shell API
}
```

## SDK Client API

### Global
- `client.global.health()` → `{ healthy: boolean, version: string }`

### App
- `client.app.log({ body: LogEntry })` → `boolean`
- `client.app.agents()` → `Agent[]`

### Project
- `client.project.list()` → `Project[]`
- `client.project.current()` → `Project`

### Path
- `client.path.get()` → `Path`

### Config
- `client.config.get()` → `Config`
- `client.config.providers()` → `{ providers: Provider[], default: Record<string, string> }`

### Sessions
- `client.session.list()` → `Session[]`
- `client.session.get({ path: { id } })` → `Session`
- `client.session.create({ body })` → `Session`
- `client.session.delete({ path: { id } })` → `boolean`
- `client.session.prompt({ path: { id }, body })` → `AssistantMessage`
- `client.session.command({ path: { id }, body })` → `{ info: AssistantMessage, parts: Part[] }`
- `client.session.shell({ path: { id }, body })` → `AssistantMessage`
- `client.session.abort({ path: { id } })` → `boolean`

### Files
- `client.find.text({ query: { pattern } })` → `TextMatch[]`
- `client.find.files({ query })` → `string[]`
- `client.file.read({ query: { path } })` → `{ type: string, content: string }`

### TUI
- `client.tui.appendPrompt({ body: { text } })` → `boolean`
- `client.tui.showToast({ body: { message, variant? } })` → `boolean`
- `client.tui.executeCommand({ body: { command } })` → `boolean`

### Events
- `client.event.subscribe()` → `{ stream: AsyncIterable<Event> }`

## Tool Schema Types

```typescript
import { tool } from "@opencode-ai/plugin"

tool.schema.string()                    // String
tool.schema.number()                    // Number
tool.schema.boolean()                   // Boolean
tool.schema.array(itemSchema)           // Array
tool.schema.object({ field: schema })   // Object
tool.schema.enum(["a", "b"])            // Enum
tool.schema.union([schema1, schema2])   // Union

// Modifiers:
.schema.type().optional()               // Optional
.schema.type().describe("text")         // Description
.schema.type().default(value)           // Default value
```

## Built-in Tools Reference

| Tool | Description |
|------|-------------|
| `read` | Read file contents |
| `write` | Write file contents |
| `edit` | Edit file contents |
| `bash` | Execute shell commands |
| `glob` | Find files by pattern |
| `grep` | Search text in files |

## Event Types Reference

### Session Events
- `session.created` - New session started
- `session.deleted` - Session removed
- `session.idle` - Agent finished responding
- `session.error` - Error occurred
- `session.compacted` - Context was compacted
- `session.updated` - Session properties changed
- `session.diff` - Session diff generated

### Message Events
- `message.updated` - Message added/changed
- `message.removed` - Message deleted
- `message.part.updated` - Message part modified

### File Events
- `file.edited` - File was edited
- `file.watcher.updated` - File watcher detected change

### Tool Events
- `tool.execute.before` - Before tool execution
- `tool.execute.after` - After tool execution

### Permission Events
- `permission.asked` - Permission requested
- `permission.replied` - Permission responded

### LSP Events
- `lsp.client.diagnostics` - LSP diagnostics received
- `lsp.updated` - LSP state updated

### Other Events
- `command.executed` - Command executed
- `server.connected` - Server connected
- `shell.env` - Shell environment event
- `todo.updated` - Todo list updated
- `tui.prompt.append` - TUI prompt appended
- `tui.command.execute` - TUI command executed
- `tui.toast.show` - Toast notification shown
- `installation.updated` - Installation status changed
