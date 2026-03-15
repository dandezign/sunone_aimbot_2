/**
 * Shared utilities for native tools
 */
/**
 * Format bytes to human-readable string
 */
export declare function formatBytes(bytes: number): string;
/**
 * Format statistics as JSON object
 */
export declare function formatStatsObject(stats: any, savings: any): {
    session: {
        id: string;
        directory: any;
        eventCount: any;
        compactCount: any;
    };
    savings: {
        rawBytes: any;
        processedBytes: any;
        ratio: string;
    };
};
/**
 * Format statistics as markdown text
 */
export declare function formatStatsText(stats: any, savings: any): string;
