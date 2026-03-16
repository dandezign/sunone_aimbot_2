/**
 * Native tools factory for OpenCode plugin
 *
 * Exports all native OpenCode tools (ctx_stats, ctx_doctor, ctx_upgrade).
 * MCP sandbox tools (ctx_execute, ctx_execute_file, ctx_batch_execute) remain separate.
 */
import type { ToolDefinition } from "@opencode-ai/plugin";
import type { SessionManager } from "../session/manager.js";
/**
 * Create all native OpenCode tools.
 *
 * @param client - OpenCode SDK client
 * @param sessionManager - Session state manager
 * @returns Object with all native tools
 */
export declare function createNativeTools(client: any, sessionManager: SessionManager): {
    [key: string]: ToolDefinition;
};
