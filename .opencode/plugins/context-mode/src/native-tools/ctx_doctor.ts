/**
 * Native ctx_doctor tool implementation
 */

import { tool } from "@opencode-ai/plugin"

export function createCtxDoctorTool(client: any) {
  return tool({
    description: "Diagnose installation and configuration",
    args: {},
    async execute(args: {}, context: any) {
      const { directory, sessionID } = context
      
      const lines = [
        "## Context Mode Diagnostics",
        "",
        `**Session:** ${sessionID ? sessionID.slice(0, 8) + "..." : "Not initialized"}`,
        `**Project:** ${directory || "Unknown"}`,
        "",
        "### Checks",
        "",
      ]
      
      // Check 1: Session state
      lines.push(`- [${sessionID ? "x" : " "}] Session state`)
      
      // Check 2: OpenCode version (via client)
      try {
        const health = await client.global.health()
        lines.push(`- [x] OpenCode server (v${health.data?.version || "unknown"})`)
      } catch (error) {
        lines.push(`- [ ] OpenCode server (not reachable)`)
      }
      
      lines.push("")
      lines.push("### Recommendations")
      lines.push("- Use ctx_stats to view context savings")
      lines.push("- Use ctx_upgrade to update to latest version")
      
      return lines.join("\n")
    },
  })
}
