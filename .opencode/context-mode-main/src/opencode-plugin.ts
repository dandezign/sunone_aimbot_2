/**
 * OpenCode native plugin entry point using @opencode-ai/plugin SDK.
 *
 * Hooks provided:
 * - event: Session lifecycle (created, deleted, compacted)
 * - tool.execute.before: Routing enforcement (deny/modify/passthrough)
 * - tool.execute.after: Session event capture
 * - experimental.session.compacting: Snapshot generation with context injection
 *
 * Loaded by OpenCode via: import("context-mode/plugin").ContextModePlugin(ctx)
 */

import type { Plugin } from "@opencode-ai/plugin"
import { sessionManager } from "./session/manager.js"
import { extractEvents } from "./session/extract.js"
import type { HookInput } from "./session/extract.js"
import { buildResumeSnapshot } from "./session/snapshot.js"
import type { SessionEvent } from "./types.js"
import { OpenCodeAdapter } from "./adapters/opencode/index.js"
import { createNativeTools } from "./native-tools/index.js"

// ── Plugin Factory ────────────────────────────────────────

/**
 * OpenCode native plugin factory using @opencode-ai/plugin SDK.
 * Receives context with client, directory, $, worktree.
 * Returns hook handlers and native tools.
 */
export const ContextModePlugin: Plugin = async (ctx) => {
  const { client, directory, $, worktree } = ctx
  
  // Initialize on plugin load
  await sessionManager.cleanupOrphanedSessions(24)
  
  // Auto-write AGENTS.md on startup for OpenCode projects
  try {
    new OpenCodeAdapter().writeRoutingInstructions(directory, __dirname)
  } catch {
    // best effort — never break plugin init
  }
  
  return {
    // ── Session Lifecycle: Event hook ────────────────────
    
    event: async ({ event }) => {
      if (event.type === "session.created") {
        const sessionID = (event as any).session_id
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
        const sessionID = (event as any).session_id
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
        const sessionID = (event as any).session_id
        await client.app.log({
          body: {
            service: "context-mode",
            level: "info",
            message: "Session compacted",
            extra: { sessionID },
          },
        })
      }
    },
    
    // ── PreToolUse: Routing enforcement ─────────────────
    
    "tool.execute.before": async (input, output) => {
      const sessionID = input.sessionID
      if (!sessionID) return
      
      const state = sessionManager.get(sessionID)
      if (!state) return
      
      const toolName = input.tool
      const toolInput = output.args
      
      // Load routing module dynamically
      const routingPath = `${__dirname}/hooks/core/routing.mjs`
      const routing = await import(routingPath)
      
      let decision
      try {
        decision = routing.routePreToolUse(toolName, toolInput, directory)
      } catch (error) {
        await client.app.log({
          body: {
            service: "context-mode",
            level: "warn",
            message: "Routing failed",
            extra: { error: String(error) },
          },
        })
        return // Routing failure → allow passthrough
      }
      
      if (!decision) return // No routing match → passthrough
      
      if (decision.action === "deny" || decision.action === "ask") {
        await client.app.log({
          body: {
            service: "context-mode",
            level: "info",
            message: "Tool blocked",
            extra: { tool: toolName, reason: decision.reason },
          },
        })
        // Throw to block — OpenCode catches this and denies the tool call
        throw new Error(decision.reason ?? "Blocked by context-mode")
      }
      
      if (decision.action === "modify" && decision.updatedInput) {
        // Mutate args in place — OpenCode reads the mutated input
        Object.assign(output.args, decision.updatedInput)
      }
    },
    
    // ── PostToolUse: Session event capture ──────────────
    
    "tool.execute.after": async (input, output) => {
      const sessionID = input.sessionID
      if (!sessionID) return
      
      const state = sessionManager.get(sessionID)
      if (!state) return
      
      try {
        const hookInput: HookInput = {
          tool_name: input.tool,
          tool_input: input.args,
          tool_response: output.output,
          tool_output: undefined,
        }
        
        const events = extractEvents(hookInput)
        for (const event of events) {
          state.db.insertEvent(sessionID, event as SessionEvent, "PostToolUse")
        }
        
        sessionManager.recordEvent(sessionID)
      } catch (error) {
        // INTENTIONAL: Silent error handling (production behavior)
        // Event capture must NEVER break tool execution
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
    
    // ── PreCompact: Snapshot generation with context injection ──
    
    "experimental.session.compacting": async (input, output) => {
      const sessionID = input.sessionID
      if (!sessionID) return
      
      const state = sessionManager.get(sessionID)
      if (!state) return
      
      try {
        const events = state.db.getEvents(sessionID)
        if (events.length === 0) return
        
        const snapshot = buildResumeSnapshot(events, {
          compactCount: state.compactCount + 1,
        })
        
        // Increment compact count
        state.compactCount++
        
        // INJECT CONTEXT (new capability!)
        output.context.push(snapshot)
        
        await client.app.log({
          body: {
            service: "context-mode",
            level: "info",
            message: "Snapshot built and injected",
            extra: { events: events.length, compactCount: state.compactCount },
          },
        })
      } catch (error) {
        await client.app.log({
          body: {
            service: "context-mode",
            level: "error",
            message: "Snapshot build failed",
            extra: { error: String(error) },
          },
        })
      }
    },
    
    // ── Native Tools ────────────────────────────────────
    
    tool: createNativeTools(client, sessionManager),
  }
}
