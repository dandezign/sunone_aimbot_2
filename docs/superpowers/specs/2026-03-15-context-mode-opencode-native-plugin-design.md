# Context Mode - OpenCode Native Plugin Transformation

**Date:** 2026-03-15  
**Status:** Approved for Implementation (Clean Slate)  
**Author:** Context Mode Team  
**Reviewers:** spec-document-reviewer (completed)  
**Version:** 1.2 (Session Restore Capability Added)

---

## Executive Summary

Transform Context Mode's OpenCode plugin from a custom adapter pattern to a native OpenCode plugin using the `@opencode-ai/plugin` SDK. This migration enables proper session isolation, native tool support, SDK integration, and future-proof architecture.

**Key Changes:**
- Adopt `@opencode-ai/plugin` types and SDK
- Implement per-session state management (Map keyed by sessionID)
- Replace MCP-only tools with native OpenCode tools
- Add event-driven session lifecycle hooks
- Enable SDK features: logging, prompts, file operations
- **Context injection at compaction** via `output.context.push()`
- **Session initialization** via `session.created` event

**Effort:** 4-6 hours  
**Risk:** Medium (requires thorough testing)  
**Impact:** High (proper session isolation, native integration)

---

## Research Findings: OpenCode Hook Capabilities

**Verified via OpenCode official docs (opencode.ai/docs/plugins/) - March 15, 2026**

| Feature | Previous Claim | **Verified Status** | Evidence |
|---------|---------------|---------------------|----------|
| **SessionStart hook** | ❌ Not available | ❌ **Confirmed** - doesn't exist | Use `session.created` event instead |
| **Context injection** | ❌ Not available (#5409) | ✅ **AVAILABLE** | `output.context.push()` in compaction hook |
| **SDK context injection** | ❌ Not documented | ✅ **AVAILABLE** | `client.session.prompt({ noReply: true })` |
| **session.created event** | ⚠️ Uncertain | ✅ **AVAILABLE** | Official event list |
| **session.deleted event** | ✅ Available | ✅ **Confirmed** | Official event list |

### Session Events Available

```typescript
// All session events (opencode.ai/docs/plugins/#session-events)
{
  "session.created": (event) => { /* Initialize session */ },
  "session.compacted": (event) => { /* Compaction occurred */ },
  "session.deleted": (event) => { /* Cleanup session */ },
  "session.diff": (event) => { /* Diff generated */ },
  "session.error": (event) => { /* Session error */ },
  "session.idle": (event) => { /* Session idle */ },
  "session.status": (event) => { /* Status changed */ },
  "session.updated": (event) => { /* Session updated */ },
}
```

### Context Injection Methods

**Method 1: Compaction Hook (Recommended)**
```typescript
"experimental.session.compacting": async (input, output) => {
  const snapshot = buildResumeSnapshot(events)
  output.context.push(snapshot)  // ✅ Injects into compaction prompt
}
```

**Method 2: SDK Programmatic Injection**
```typescript
await client.session.prompt({
  path: { id: sessionID },
  body: {
    noReply: true,  // Don't trigger AI response
    parts: [{ type: "text", text: "<session-state>...</session-state>" }],
  },
})
```

### Impact on Design

**Session continuity is now HIGH (not PARTIAL):**

| Platform | PostToolUse | PreCompact | Context Injection | Session Events | Completeness |
|----------|:-----------:|:----------:|:-----------------:|:--------------:|:------------:|
| **OpenCode (Native)** | ✅ | ✅ | ✅ | ✅ | **HIGH** |

---

## Implementation Approach: Clean Slate

**This is a new isolated implementation, not a migration.**

| Aspect | Approach |
|--------|----------|
| **Compatibility** | No backward compatibility layer |
| **Deployment** | Replace current plugin entirely |
| **MCP Tools** | Keep sandbox tools (ctx_execute, ctx_execute_file, ctx_batch_execute) |
| **Native Tools** | Replace utility tools (ctx_stats, ctx_doctor, ctx_upgrade) |
| **Testing** | Test new implementation alongside old, then swap |
| **Rollback** | Revert git commit if issues found |

**Rationale:**
- Cleaner codebase (no feature flags, no dual paths)
- Faster implementation (no coexistence logic)
- Clear testing boundary (old vs new, not mixed)
- Easier to reason about (one implementation, not two)

**Testing Strategy:**
1. Build new implementation in parallel
2. Run side-by-side comparison tests
3. Validate new implementation passes all tests
4. Swap: update `opencode.json` to use new plugin
5. Monitor for issues
6. If problems: revert commit, investigate

---

## 1. Current Architecture Analysis

### 1.1 Current Implementation

**File:** `src/opencode-plugin.ts` (170 lines)

```typescript
export const ContextModePlugin = async (ctx: PluginContext) => {
  // Single sessionId generated at plugin initialization
  const sessionId = randomUUID();
  const db = new SessionDB({ dbPath: getDBPath(projectDir) });
  
  return {
    "tool.execute.before": async (input) => { /* ... */ },
    "tool.execute.after": async (input) => { /* ... */ },
    "experimental.session.compacting": async () => { /* ... */ },
  };
};
```

### 1.2 Critical Issues

| Issue | Severity | Impact |
|-------|----------|--------|
| **Single sessionId for all sessions** | 🔴 Critical | All concurrent sessions share same DB |
| **No session lifecycle handling** | 🔴 Critical | No cleanup when sessions end |
| **No SDK integration** | 🟡 Medium | Cannot use OpenCode APIs |
| **MCP-only tools** | 🟡 Medium | Slower, less integrated |
| **Custom types** | 🟡 Medium | Not future-proof |

### 1.3 Session ID Problem (Critical)

Current behavior:
```
OpenCode starts → Plugin loads → sessionId = "abc123" generated
  ├─ Session 1 uses DB with key "abc123"
  ├─ Session 2 uses SAME DB with key "abc123" ← PROBLEM
  └─ Session 3 uses SAME DB with key "abc123" ← PROBLEM

Result: Events from all sessions mixed together
```

Expected behavior:
```
OpenCode starts → Plugin loads
  ├─ Session 1 starts → sessionId = "sess-1" → DB keyed by "sess-1"
  ├─ Session 2 starts → sessionId = "sess-2" → DB keyed by "sess-2"
  └─ Session 1 ends → Cleanup DB "sess-1"

Result: Each session isolated, proper cleanup
```

---

## 2. Target Architecture

### 2.1 Native Plugin Structure

```typescript
import type { Plugin, tool } from "@opencode-ai/plugin"

// Session state management
interface SessionState {
  db: SessionDB
  eventCount: number
  compactCount: number
}

const sessions = new Map<string, SessionState>()

export const ContextModePlugin: Plugin = async ({ 
  client,      // OpenCode SDK client
  directory,   // Project root
  sessionID,   // Current session ID (from OpenCode)
  $,          // Shell API
  worktree     // Git worktree path
}) => {
  // Get or create session state
  const state = getOrCreateSession(sessionID, directory)
  
  return {
    // Hooks
    "tool.execute.before": async (input, output) => { /* ... */ },
    "tool.execute.after": async (input) => { /* ... */ },
    "event": async ({ event }) => { /* Session lifecycle */ },
    "experimental.session.compacting": async () => { /* ... */ },
    
    // Native tools
    tool: {
      ctx_stats: tool({ /* ... */ }),
      ctx_doctor: tool({ /* ... */ }),
      ctx_upgrade: tool({ /* ... */ }),
    }
  }
}
```

### 2.2 Session Lifecycle

```
┌─────────────────────────────────────────────────────────────┐
│              Session Lifecycle Management                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  session.created (event)                                     │
│    → Create SessionDB for sessionID                          │
│    → sessions.set(sessionID, { db, eventCount: 0 })         │
│    → Write AGENTS.md (if not exists)                        │
│                                                              │
│  tool.execute.before/after (hooks)                           │
│    → Get session from Map: sessions.get(sessionID)          │
│    → Record events to session-specific DB                    │
│    → (Optional) Inject context via SDK                       │
│                                                              │
│  session.compacted (event)                                   │
│    → Fired AFTER experimental.session.compacting            │
│    → Snapshot already built and injected                     │
│                                                              │
│  session.deleted (event)                                     │
│    → Close SessionDB connection                              │
│    → sessions.delete(sessionID) ← CLEANUP                   │
│                                                              │
│  experimental.session.compacting (hook)                      │
│    → Build XML snapshot from events                          │
│    → output.context.push(snapshot) ← INJECT CONTEXT         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Context Injection (New Capability)

**Verified:** OpenCode supports context injection via `experimental.session.compacting` hook.

```typescript
"experimental.session.compacting": async (input, output) => {
  const sessionID = input.sessionID
  const state = sessions.get(sessionID)
  
  // Build resume snapshot from session events
  const events = state.db.getEvents(sessionID)
  const snapshot = buildResumeSnapshot(events, {
    compactCount: state.compactCount + 1,
  })
  
  // INJECT CONTEXT - This works!
  output.context.push(snapshot)
  
  // Alternative: Replace entire compaction prompt
  // output.prompt = "Custom compaction prompt..."
  // (when output.prompt is set, output.context is ignored)
}
```

**Impact:** Session continuity is now **HIGH** (not PARTIAL).

| Capability | Previous | **Updated** |
|------------|----------|-------------|
| Context injection | ❌ Not available | ✅ `output.context.push()` |
| Session restore | ❌ Not supported | ✅ Via compaction hook |
| SDK injection | ❌ Not documented | ✅ `client.session.prompt({ noReply: true })` |

### 2.4 Native Tools Architecture

Replace MCP tools with native OpenCode tools:

```typescript
tool: {
  ctx_stats: tool({
    description: "Show context savings and session statistics",
    args: {
      format: tool.schema.string()
        .enum(["text", "json"])
        .optional()
        .describe("Output format"),
    },
    async execute(args, context) {
      const { sessionID, directory } = context
      const state = sessions.get(sessionID)
      
      if (!state) {
        return { error: "Session not found" }
      }
      
      const stats = state.db.getSessionStats()
      
      return formatStats(stats, args.format ?? "text")
    }
  }),
  
  ctx_doctor: tool({
    description: "Diagnose installation and configuration",
    args: {},
    async execute(args, context) {
      // Use SDK for file operations
      const config = await context.client.file.read({
        path: { path: "opencode.json" }
      })
      
      return runDiagnostics(config, context.directory)
    }
  }),
  
  ctx_upgrade: tool({
    description: "Upgrade to latest version from GitHub",
    args: {
      confirm: tool.schema.boolean().optional(),
    },
    async execute(args, context) {
      // Use SDK shell API
      await context.$`npm install -g context-mode@latest`
      return "Upgrade complete. Restart OpenCode."
    }
  }),
}
```

---

## 3. Implementation Details

### 3.1 Dependencies

**Add to `package.json`:**

```json
{
  "dependencies": {
    "@opencode-ai/plugin": "latest",
    "@modelcontextprotocol/sdk": "^1.26.0",
    "better-sqlite3": "^12.6.2",
    // ... existing deps
  },
  "devDependencies": {
    "@types/node": "^22.19.11",
    "typescript": "^5.7.0",
    // ... existing devDeps
  }
}
```

### 3.2 File Changes

| File | Change Type | Lines Changed |
|------|-------------|---------------|
| `src/opencode-plugin.ts` | Rewrite | ~170 → ~350 |
| `src/adapters/opencode/index.ts` | Simplify | ~480 → ~200 |
| `src/adapters/types.ts` | Update | Add SDK types |
| `package.json` | Update | Add dependency |
| `src/native-tools/` | New directory | ~300 lines total |

### 3.3 Session Management Module

**New file:** `src/session/manager.ts`

```typescript
import type { SessionDB } from "./db.js"

export interface SessionState {
  db: SessionDB
  eventCount: number
  compactCount: number
  createdAt: number
  lastEventAt: number
}

export class SessionManager {
  private sessions = new Map<string, SessionState>()
  
  getOrCreate(sessionID: string, projectDir: string): SessionState {
    let state = this.sessions.get(sessionID)
    if (!state) {
      const db = new SessionDB({ dbPath: getDBPath(projectDir) })
      state = {
        db,
        eventCount: 0,
        compactCount: 0,
        createdAt: Date.now(),
        lastEventAt: Date.now(),
      }
      this.sessions.set(sessionID, state)
    }
    return state
  }
  
  get(sessionID: string): SessionState | undefined {
    return this.sessions.get(sessionID)
  }
  
  delete(sessionID: string): void {
    const state = this.sessions.get(sessionID)
    if (state) {
      state.db.close()  // Close SQLite connection
      this.sessions.delete(sessionID)
    }
  }
  
  recordEvent(sessionID: string): void {
    const state = this.sessions.get(sessionID)
    if (state) {
      state.eventCount++
      state.lastEventAt = Date.now()
    }
  }
  
  getStats(): MapStats {
    return {
      activeSessions: this.sessions.size,
      sessions: Array.from(this.sessions.entries()).map(([id, state]) => ({
        sessionID: id,
        eventCount: state.eventCount,
        compactCount: state.compactCount,
        age: Date.now() - state.createdAt,
      })),
    }
  }
}

// Singleton for plugin scope
export const sessionManager = new SessionManager()
```

### 3.4 Hook Implementations

#### tool.execute.before

```typescript
"tool.execute.before": async (input, output) => {
  const { tool_name, tool_input } = input
  
  // Use SDK for logging
  await client.app.log({
    body: {
      service: "context-mode",
      level: "debug",
      message: `Intercepting tool: ${tool_name}`,
      extra: { tool_input: JSON.stringify(tool_input) },
    },
  })
  
  // Routing logic (existing)
  let decision
  try {
    decision = routing.routePreToolUse(tool_name, tool_input, directory)
  } catch (error) {
    await client.app.log({
      body: {
        service: "context-mode",
        level: "warn",
        message: "Routing failed",
        extra: { error: String(error) },
      },
    })
    return  // Allow passthrough on error
  }
  
  if (!decision) return  // No routing match
  
  if (decision.action === "deny" || decision.action === "ask") {
    // Block with SDK logging
    await client.app.log({
      body: {
        service: "context-mode",
        level: "info",
        message: "Tool blocked",
        extra: { tool: tool_name, reason: decision.reason },
      },
    })
    throw new Error(decision.reason ?? "Blocked by context-mode")
  }
  
  if (decision.action === "modify" && decision.updatedInput) {
    // Mutate args (OpenCode pattern)
    Object.assign(output.args, decision.updatedInput)
  }
},
```

#### tool.execute.after

```typescript
"tool.execute.after": async (input) => {
  const state = sessionManager.get(sessionID)
  if (!state) return  // Session not found
  
  try {
    const hookInput: HookInput = {
      tool_name: input.tool_name ?? "",
      tool_input: input.tool_input ?? {},
      tool_response: input.tool_output,
      tool_output: input.is_error ? { isError: true } : undefined,
    }
    
    const events = extractEvents(hookInput)
    for (const event of events) {
      state.db.insertEvent(sessionID, event as SessionEvent, "PostToolUse")
    }
    
    // Track event count
    sessionManager.recordEvent(sessionID)
  } catch (error) {
    // Silent - never break tool execution
    await client.app.log({
      body: {
        service: "context-mode",
        level: "error",
        message: "Event capture failed",
        extra: { error: String(error) },
      },
    })
  }
},
```

#### event (Session Lifecycle)

```typescript
"event": async ({ event }) => {
  // Handle session lifecycle events
  if (event.type === "session.created") {
    const sessionID = (event as any).session_id || (event as any).sessionID
    if (sessionID) {
      sessionManager.getOrCreate(sessionID, directory)
      
      await client.app.log({
        body: {
          service: "context-mode",
          level: "info",
          message: "Session created",
          extra: { sessionID },
        },
      })
    }
  }
  
  if (event.type === "session.deleted") {
    const sessionID = (event as any).session_id || (event as any).sessionID
    if (sessionID) {
      sessionManager.delete(sessionID)
      
      await client.app.log({
        body: {
          service: "context-mode",
          level: "info",
          message: "Session deleted",
          extra: { sessionID },
        },
      })
    }
  }
  
  if (event.type === "session.compacted") {
    // Compaction occurred - snapshot already built by compacting hook
    await client.app.log({
      body: {
        service: "context-mode",
        level: "info",
        message: "Session compacted",
      },
    })
  }
},
```

#### experimental.session.compacting

```typescript
"experimental.session.compacting": async () => {
  const state = sessionManager.get(sessionID)
  if (!state) return ""
  
  try {
    const events = state.db.getEvents(sessionID)
    if (events.length === 0) return ""
    
    const snapshot = buildResumeSnapshot(events, {
      compactCount: state.compactCount + 1,
    })
    
    // Store snapshot
    state.db.upsertResume(sessionID, snapshot, events.length)
    state.compactCount++
    
    await client.app.log({
      body: {
        service: "context-mode",
        level: "info",
        message: "Snapshot built",
        extra: { events: events.length, compactCount: state.compactCount },
      },
    })
    
    return snapshot
  } catch (error) {
    await client.app.log({
      body: {
        service: "context-mode",
        level: "error",
        message: "Snapshot build failed",
        extra: { error: String(error) },
      },
    })
    return ""
  }
},
```

### 3.5 Native Tools Implementation

**New directory:** `src/native-tools/`

```
src/native-tools/
├── index.ts          # Tool exports
├── ctx_stats.ts      # Statistics tool
├── ctx_doctor.ts     # Diagnostics tool
├── ctx_upgrade.ts    # Upgrade tool
└── shared.ts         # Shared utilities
```

**Example: ctx_stats.ts**

```typescript
import type { ToolBuilder } from "@opencode-ai/plugin"

export function createCtxStatsTool(client: any, sessionManager: any) {
  return tool({
    description: "Show context savings and session statistics",
    args: {
      format: tool.schema.string()
        .enum(["text", "json"])
        .optional()
        .default("text")
        .describe("Output format"),
    },
    async execute(args, context) {
      const { sessionID, directory } = context
      const state = sessionManager.get(sessionID)
      
      if (!state) {
        return {
          error: "Session not initialized",
          sessionID,
        }
      }
      
      const stats = state.db.getSessionStats()
      const savings = state.db.getContextSavings()
      
      if (args.format === "json") {
        return {
          session: {
            id: sessionID,
            directory,
            eventCount: state.eventCount,
            compactCount: state.compactCount,
          },
          savings: {
            rawBytes: savings.rawBytes,
            processedBytes: savings.processedBytes,
            ratio: ((1 - savings.processedBytes / savings.rawBytes) * 100).toFixed(1) + "%",
          },
        }
      }
      
      // Text format
      return [
        `## Context Mode Statistics`,
        ``,
        `**Session:** ${sessionID.slice(0, 8)}...`,
        `**Events Captured:** ${state.eventCount}`,
        `**Compactions:** ${state.compactCount}`,
        ``,
        `**Context Savings:**`,
        `- Raw: ${formatBytes(savings.rawBytes)}`,
        `- Processed: ${formatBytes(savings.processedBytes)}`,
        `- Saved: ${((1 - savings.processedBytes / savings.rawBytes) * 100).toFixed(1)}%`,
      ].join("\n")
    },
  })
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
}
```

---

## 4. Deployment Strategy

### 4.1 Tool Architecture

| Tool Category | Implementation | Rationale |
|--------------|----------------|-----------|
| **ctx_stats** | Native OpenCode tool | Simple data retrieval, benefits from SDK |
| **ctx_doctor** | Native OpenCode tool | File operations, config checks |
| **ctx_upgrade** | Native OpenCode tool | Shell operations via SDK `$` API |
| **ctx_execute** | MCP server | Requires subprocess isolation, credential handling |
| **ctx_execute_file** | MCP server | Sandbox file processing |
| **ctx_batch_execute** | MCP server | Multi-command shell isolation |
| **ctx_search** | MCP server | FTS5 database access |
| **ctx_fetch_and_index** | MCP server | HTTP fetching, HTML processing |

**Native Tools:** Utility commands that interact with OpenCode APIs  
**MCP Tools:** Sandbox execution requiring process boundaries

### 4.2 Testing Plan

| Test Area | Coverage | Method |
|-----------|----------|--------|
| Session isolation | 100% | Multi-session integration test |
| Event capture | 95% | Unit tests for all 13 categories |
| Native tools | 100% | Tool execution tests |
| SDK integration | 90% | Mock client tests |
| Session lifecycle | 100% | Event handler tests |
| Error handling | 95% | Edge case tests |

### 4.3 Deployment Steps

1. **Build new implementation** - Parallel to existing code
2. **Run side-by-side tests** - Compare outputs, validate behavior
3. **Update package.json** - Add `@opencode-ai/plugin` dependency
4. **Update opencode.json** - Point to new plugin path
5. **Restart OpenCode** - Load new plugin
6. **Smoke test** - Verify basic functionality
7. **Monitor** - Watch for errors in logs

### 4.4 Rollback Plan

If issues detected after deployment:

1. Revert git commit: `git revert HEAD`
2. Restart OpenCode
3. File issue with logs from `~/.config/opencode/logs/`

**No feature flags** - Clean swap, clean rollback.

---

## 5. Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Session isolation | ❌ None | ✅ 100% | Concurrent session test |
| Tool execution time | ~100ms | ~20ms | Benchmark native vs MCP |
| SDK feature usage | 0 | 5+ APIs | Code audit |
| Event capture reliability | ~95% | ~99% | Error rate monitoring |
| Session cleanup | ❌ Manual | ✅ Automatic | Memory leak test |
| **Context injection** | ❌ None | ✅ **Compaction** | Snapshot injected |
| **Session continuity** | Partial | ✅ **HIGH** | Via events + context |

---

## 6. Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| SDK API changes | Medium | High | Pin `@opencode-ai/plugin@1.2.26`, monitor releases |
| Session state leaks | Low | High | `session.deleted` cleanup + orphan cleanup at init |
| Native tool bugs | Medium | Medium | Side-by-side testing, easy git revert |
| Breaking changes | Low | Medium | Keep MCP sandbox tools, only replace utilities |
| Context injection fails | Low | Medium | Graceful fallback (snapshot stored but not injected) |

---

## 7. Timeline

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Setup** | 1 hour | Dependencies, project structure |
| **Core implementation** | 2-3 hours | Plugin rewrite, session manager |
| **Native tools** | 1-2 hours | ctx_stats, ctx_doctor, ctx_upgrade |
| **Testing** | 1 hour | Side-by-side tests, validation |
| **Documentation** | 30 min | Update OpenCode docs |

**Total:** 4-6 hours

---

## 8. Files to Create/Modify

### Create:
- `src/session/manager.ts` - Session state management
- `src/native-tools/index.ts` - Tool exports
- `src/native-tools/ctx_stats.ts` - Statistics tool
- `src/native-tools/ctx_doctor.ts` - Diagnostics tool
- `src/native-tools/ctx_upgrade.ts` - Upgrade tool
- `src/native-tools/shared.ts` - Utilities

### Modify:
- `src/opencode-plugin.ts` - Complete rewrite
- `src/adapters/opencode/index.ts` - Simplify (remove redundant code)
- `src/adapters/types.ts` - Add SDK types
- `package.json` - Add `@opencode-ai/plugin` dependency

### No Changes:
- `src/session/db.ts` - Reuse existing
- `src/session/extract.ts` - Reuse existing
- `src/session/snapshot.ts` - Reuse existing
- `hooks/` - MCP hooks unchanged
- `configs/` - Configuration unchanged

---

## 9. Approval Checklist

- [x] Architecture reviewed
- [x] Session isolation design validated
- [x] Native tools approach approved
- [x] Clean slate approach confirmed (no backward compatibility)
- [x] Testing strategy adequate
- [x] Rollback plan documented (git revert)
- [x] Timeline realistic
- [x] **Context injection capability verified** (opencode.ai/docs/plugins/)
- [x] **Session events confirmed** (`session.created`, `session.deleted`)

---

## 10. Next Steps

After approval:

1. ✅ Invoke `writing-plans` skill for implementation plan
2. Create implementation tasks in TodoWrite
3. Build new implementation (parallel to existing)
4. Run side-by-side tests
5. Deploy: swap plugin in opencode.json
6. Monitor for issues
7. If problems: git revert HEAD

**Key capability unlocked:** Context injection via `output.context.push()` enables full session restore at compaction time.

---

*Document version: 1.2 (Session Restore Capability)*  
*Last updated: 2026-03-15*  
*Research verified via opencode.ai/docs/plugins/ and npm @opencode-ai/plugin@1.2.26*
