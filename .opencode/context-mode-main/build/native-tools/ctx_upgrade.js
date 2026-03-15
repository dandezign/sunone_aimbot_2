/**
 * Native ctx_upgrade tool implementation
 */
import { tool } from "@opencode-ai/plugin";
export function createCtxUpgradeTool(client) {
    return tool({
        description: "Upgrade to latest version from GitHub",
        args: {
            confirm: tool.schema.boolean().optional(),
        },
        async execute(args, context) {
            const { $ } = context;
            try {
                await $ `npm install -g context-mode@latest`;
                return "Upgrade complete. Restart OpenCode for changes to take effect.";
            }
            catch (error) {
                return `Upgrade failed: ${String(error)}. Try running: npm install -g context-mode@latest`;
            }
        },
    });
}
