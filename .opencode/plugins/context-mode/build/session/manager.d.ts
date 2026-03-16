/**
 * SessionManager - Per-session state management for OpenCode native plugin.
 *
 * Manages multiple concurrent sessions using a Map keyed by sessionID.
 * Each session has its own SessionDB instance for event isolation.
 */
import { SessionDB } from "./db.js";
export interface SessionState {
    db: SessionDB;
    eventCount: number;
    compactCount: number;
    createdAt: number;
    lastEventAt: number;
}
export interface MapStats {
    activeSessions: number;
    sessions: Array<{
        sessionID: string;
        eventCount: number;
        compactCount: number;
        age: number;
    }>;
}
export declare class SessionManager {
    private sessions;
    /**
     * Get existing session state or create new one.
     */
    getOrCreate(sessionID: string, projectDir: string): SessionState;
    /**
     * Get session state by ID (returns undefined if not found).
     */
    get(sessionID: string): SessionState | undefined;
    /**
     * Delete session state and close database connection.
     */
    delete(sessionID: string): void;
    /**
     * Record an event for the session (increments eventCount).
     */
    recordEvent(sessionID: string): void;
    /**
     * Get statistics for all active sessions.
     */
    getStats(): MapStats;
    /**
     * Cleanup orphaned session databases (crash recovery).
     * Deletes session DB files older than maxAgeHours.
     */
    cleanupOrphanedSessions(maxAgeHours?: number): Promise<number>;
    /**
     * Get session directory path (creates if not exists).
     */
    private getSessionDir;
    /**
     * Get database file path for a session.
     */
    private getDBPath;
}
export declare const sessionManager: SessionManager;
