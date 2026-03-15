/**
 * Native tools factory for OpenCode plugin
 *
 * Exports all native OpenCode tools (ctx_stats, ctx_doctor, ctx_upgrade).
 * MCP sandbox tools (ctx_execute, ctx_execute_file, ctx_batch_execute) remain separate.
 */
import { createCtxStatsTool } from "./ctx_stats.js";
import { createCtxDoctorTool } from "./ctx_doctor.js";
import { createCtxUpgradeTool } from "./ctx_upgrade.js";
/**
 * Create all native OpenCode tools.
 *
 * @param client - OpenCode SDK client
 * @param sessionManager - Session state manager
 * @returns Object with all native tools
 */
export function createNativeTools(client, sessionManager) {
    return {
        ctx_stats: createCtxStatsTool(client, sessionManager),
        ctx_doctor: createCtxDoctorTool(client),
        ctx_upgrade: createCtxUpgradeTool(client),
    };
}
