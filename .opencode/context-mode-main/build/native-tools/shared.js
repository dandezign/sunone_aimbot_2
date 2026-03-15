/**
 * Shared utilities for native tools
 */
/**
 * Format bytes to human-readable string
 */
export function formatBytes(bytes) {
    if (bytes < 1024)
        return `${bytes} B`;
    if (bytes < 1024 * 1024)
        return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}
/**
 * Format statistics as JSON object
 */
export function formatStatsObject(stats, savings) {
    return {
        session: {
            id: stats.sessionID?.slice(0, 8) + "...",
            directory: stats.directory,
            eventCount: stats.eventCount,
            compactCount: stats.compactCount,
        },
        savings: {
            rawBytes: savings.rawBytes,
            processedBytes: savings.processedBytes,
            ratio: ((1 - savings.processedBytes / savings.rawBytes) * 100).toFixed(1) + "%",
        },
    };
}
/**
 * Format statistics as markdown text
 */
export function formatStatsText(stats, savings) {
    return [
        `## Context Mode Statistics`,
        ``,
        `**Session:** ${stats.sessionID?.slice(0, 8) || "unknown"}...`,
        `**Events Captured:** ${stats.eventCount}`,
        `**Compactions:** ${stats.compactCount}`,
        ``,
        `**Context Savings:**`,
        `- Raw: ${formatBytes(savings.rawBytes)}`,
        `- Processed: ${formatBytes(savings.processedBytes)}`,
        `- Saved: ${((1 - savings.processedBytes / savings.rawBytes) * 100).toFixed(1)}%`,
    ].join("\n");
}
