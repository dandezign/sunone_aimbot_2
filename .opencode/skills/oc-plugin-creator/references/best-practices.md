# OpenCode Plugin Best Practices

Guidelines for creating maintainable, effective plugins.

## Architecture Guidelines

### 1. Single Responsibility
Each plugin should do one thing well. Don't create a "kitchen sink" plugin.

**Good:**
- `commit-reminder.ts` - Only handles commit reminders
- `env-protection.ts` - Only blocks .env access

**Bad:**
- `my-plugin.ts` - Handles commits, formatting, notifications, and custom tools

### 2. State Management

**Always use session-keyed Maps:**

```typescript
// ✅ GOOD
const sessions = new Map<string, SessionState>()

function getState(sessionId: string): SessionState {
  if (!sessions.has(sessionId)) {
    sessions.set(sessionId, defaultState())
  }
  return sessions.get(sessionId)!
}
```

```typescript
// ❌ BAD - shared across all sessions!
let filesModified: string[] = []
```

**Clean up on session deletion:**

```typescript
event: async ({ event }) => {
  if (event.type === "session.deleted") {
    const sessionId = (event as any).session_id
    sessions.delete(sessionId)
  }
}
```

### 3. Error Handling

**Be selective about throwing:**
- Use throws to block dangerous operations
- Don't throw for logging/monitoring

```typescript
// ✅ GOOD - blocks dangerous operation
"tool.execute.before": async (input, output) => {
  if (isDangerous(output.args.command)) {
    throw new Error("Dangerous command blocked for safety")
  }
}
```

```typescript
// ❌ BAD - unnecessary throw
"tool.execute.after": async (input) => {
  if (input.tool === "read") {
    throw new Error("Read operation detected") // Breaks workflow!
  }
}
```

### 4. Async Patterns

**Always use async/await:**

```typescript
// ✅ GOOD
return {
  "tool.execute.after": async (input) => {
    const result = await someAsyncOperation()
    console.log(result)
  }
}
```

```typescript
// ❌ BAD - synchronous
return {
  "tool.execute.after": (input) => {
    const result = fs.readFileSync(...) // Blocking!
  }
}
```

### 5. Type Safety

**Always use TypeScript types:**

```typescript
// ✅ GOOD
import type { Plugin } from "@opencode-ai/plugin"

interface SessionState {
  filesModified: string[]
  commitMade: boolean
}

export const MyPlugin: Plugin = async ({ client }) => { }
```

```typescript
// ❌ BAD - untyped
export const MyPlugin = async (ctx) => { } // No type safety
```

## Performance Guidelines

### 1. Hook Efficiency
Hooks run on every matching event. Keep them fast.

```typescript
// ✅ GOOD - early return
"tool.execute.after": async (input) => {
  if (input.tool !== "edit") return // Skip quickly
  // ... process edit
}
```

```typescript
// ❌ BAD - unnecessary work
"tool.execute.after": async (input) => {
  const heavyData = await fetchLargeDataset() // Don't do this!
  if (input.tool === "edit") {
    // ... use heavyData
  }
}
```

### 2. Debouncing
If tracking frequent events, debounce expensive operations.

```typescript
const debouncedSave = debounce(async (data) => {
  await saveToDisk(data)
}, 1000)

"tool.execute.after": async (input) => {
  if (input.tool === "edit") {
    debouncedSave(state)
  }
}
```

### 3. Memory Management
Clean up resources to prevent memory leaks.

```typescript
event: async ({ event }) => {
  if (event.type === "session.deleted") {
    const sessionId = (event as any).session_id
    
    // Clean up
    sessions.delete(sessionId)
    timers.delete(sessionId)
    fileWatchers.delete(sessionId)
  }
}
```

## Security Guidelines

### 1. Input Validation
Always validate inputs in custom tools.

```typescript
tool: {
  runCommand: tool({
    description: "Run safe command",
    args: {
      command: tool.schema.string().describe("Command to run")
    },
    async execute(args) {
      // Validate before executing
      if (args.command.includes("rm -rf /")) {
        throw new Error("Dangerous command rejected")
      }
      // ... execute
    }
  })
}
```

### 2. Path Traversal Prevention
Validate file paths to prevent directory traversal.

```typescript
async execute(args, context) {
  const { worktree } = context
  const requestedPath = path.resolve(worktree, args.path)
  
  // Ensure path is within worktree
  if (!requestedPath.startsWith(worktree)) {
    throw new Error("Path outside project directory")
  }
  
  // ... safe to proceed
}
```

### 3. Sensitive Data
Don't log sensitive information.

```typescript
// ✅ GOOD
await client.app.log({
  body: {
    service: "my-plugin",
    level: "info",
    message: "API call completed",
    extra: { endpoint: "/api/users" } // Safe data only
  }
})
```

```typescript
// ❌ BAD
await client.app.log({
  body: {
    message: "API call completed",
    extra: { apiKey: secretKey, password: userPass } // NEVER!
  }
})
```

## Testing Plugins

### 1. Manual Testing Steps

1. Place plugin in `.opencode/plugins/`
2. Restart OpenCode
3. Check console for initialization message
4. Trigger the event/tool the plugin hooks into
5. Verify expected behavior

### 2. Debug Logging

Add temporary logging during development:

```typescript
export const DebugPlugin: Plugin = async ({ client }) => {
  console.log("Plugin loaded!")
  
  return {
    event: async ({ event }) => {
      console.log("Event:", event.type, event.properties)
    },
    "tool.execute.before": async (input) => {
      console.log("Tool before:", input.tool, input.args)
    }
  }
}
```

### 3. Common Issues

**Plugin not loading:**
- Check TypeScript syntax: `npx tsc --noEmit plugin.ts`
- Verify file extension is `.ts` or `.js`
- Check OpenCode version compatibility

**Hooks not firing:**
- Verify exact hook name (case-sensitive)
- Check that event type matches
- Add console.log to verify registration

**State not persisting:**
- Ensure using session-keyed Maps
- Verify session_id extraction is correct
- Check cleanup isn't happening too early

## Distribution Guidelines

### 1. NPM Package Structure

```
my-opencode-plugin/
├── package.json
├── README.md
├── src/
│   └── index.ts
└── dist/
    └── index.js
```

**package.json:**
```json
{
  "name": "my-opencode-plugin",
  "version": "1.0.0",
  "main": "dist/index.js",
  "types": "dist/index.d.ts",
  "peerDependencies": {
    "@opencode-ai/plugin": "^1.0.0"
  },
  "keywords": ["opencode", "plugin"]
}
```

### 2. Documentation

Include in README:
- What the plugin does
- Installation instructions
- Configuration options
- Example usage
- Requirements

### 3. Versioning

Follow semantic versioning:
- MAJOR: Breaking changes
- MINOR: New features
- PATCH: Bug fixes

## Maintenance

### 1. Keep Dependencies Minimal
Only depend on what you absolutely need.

### 2. Monitor OpenCode Updates
Plugins may need updates when OpenCode releases new versions.

### 3. Handle Deprecation
If a hook becomes deprecated:
- Update to new API
- Add backwards compatibility
- Document migration path

## Anti-Patterns Summary

❌ Global variables for session state  
❌ Synchronous file operations  
❌ Unnecessary throws in hooks  
❌ Heavy operations in frequently-fired hooks  
❌ Logging sensitive data  
❌ Uncaught promise rejections  
❌ No type safety  
❌ Kitchen-sink plugins that do everything  

## Patterns Summary

✅ Session-keyed Maps for state  
✅ Async/await for all operations  
✅ Selective throwing for blocking  
✅ Early returns for efficiency  
✅ TypeScript with proper types  
✅ Input validation  
✅ Path traversal protection  
✅ Clean up on session deletion  
✅ Single responsibility per plugin  
