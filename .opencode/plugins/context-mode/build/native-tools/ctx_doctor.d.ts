/**
 * Native ctx_doctor tool implementation
 */
export declare function createCtxDoctorTool(client: any): {
    description: string;
    args: {};
    execute(args: Record<string, never>, context: import("@opencode-ai/plugin").ToolContext): Promise<string>;
};
