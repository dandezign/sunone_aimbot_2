/**
 * Native ctx_stats tool implementation
 */
import type { ToolDefinition } from "@opencode-ai/plugin";
import type { SessionManager } from "../session/manager.js";
export declare function createCtxStatsTool(client: any, sessionManager: SessionManager): ToolDefinition;
