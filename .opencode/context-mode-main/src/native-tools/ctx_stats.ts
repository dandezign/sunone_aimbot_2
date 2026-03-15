/**
 * Native ctx_stats tool implementation
 */

import { tool } from "@opencode-ai/plugin"
import type { ToolDefinition } from "@opencode-ai/plugin"
import type { SessionManager } from "../session/manager.js"
import { formatStatsText } from "./shared.js"

export function createCtxStatsTool(client: any, sessionManager: SessionManager): ToolDefinition {
  return tool({
    description: "Show context savings and session statistics",
    args: {
      format: tool.schema.string().optional().default("text"),
    },
    async execute(args: { format?: string }, context: any) {
      const sessionID = context.sessionID
      const directory = context.directory
      const state = sessionManager.get(sessionID)
      
      if (!state) {
        return "Error: Session not initialized"
      }
      
      const stats = state.db.getSessionStats(sessionID)
      const savings = { rawBytes: 100000, processedBytes: 2000 } // Placeholder
      
      if (args.format === "json") {
        return JSON.stringify({
          session: {
            id: sessionID?.slice(0, 8) + "...",
            directory,
            eventCount: stats?.event_count ?? 0,
            compactCount: stats?.compact_count ?? 0,
          },
          savings: {
            rawBytes: savings.rawBytes,
            processedBytes: savings.processedBytes,
            ratio: "98.0%",
          },
        })
      }
      
      return formatStatsText({ sessionID, directory, ...stats }, savings)
    },
  })
}
