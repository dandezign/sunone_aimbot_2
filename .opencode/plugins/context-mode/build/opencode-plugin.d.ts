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
import type { Plugin } from "@opencode-ai/plugin";
/**
 * OpenCode native plugin factory using @opencode-ai/plugin SDK.
 * Receives context with client, directory, $, worktree.
 * Returns hook handlers and native tools.
 */
export declare const ContextModePlugin: Plugin;
