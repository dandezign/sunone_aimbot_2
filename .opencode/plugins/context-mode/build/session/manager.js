/**
 * SessionManager - Per-session state management for OpenCode native plugin.
 *
 * Manages multiple concurrent sessions using a Map keyed by sessionID.
 * Each session has its own SessionDB instance for event isolation.
 */
import { mkdirSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";
import { createHash } from "node:crypto";
import { SessionDB } from "./db.js";
export class SessionManager {
    sessions = new Map();
    /**
     * Get existing session state or create new one.
     */
    getOrCreate(sessionID, projectDir) {
        let state = this.sessions.get(sessionID);
        if (!state) {
            const db = new SessionDB({ dbPath: this.getDBPath(projectDir, sessionID) });
            state = {
                db,
                eventCount: 0,
                compactCount: 0,
                createdAt: Date.now(),
                lastEventAt: Date.now(),
            };
            this.sessions.set(sessionID, state);
        }
        return state;
    }
    /**
     * Get session state by ID (returns undefined if not found).
     */
    get(sessionID) {
        return this.sessions.get(sessionID);
    }
    /**
     * Delete session state and close database connection.
     */
    delete(sessionID) {
        const state = this.sessions.get(sessionID);
        if (state) {
            state.db.close();
            this.sessions.delete(sessionID);
        }
    }
    /**
     * Record an event for the session (increments eventCount).
     */
    recordEvent(sessionID) {
        const state = this.sessions.get(sessionID);
        if (state) {
            state.eventCount++;
            state.lastEventAt = Date.now();
        }
    }
    /**
     * Get statistics for all active sessions.
     */
    getStats() {
        return {
            activeSessions: this.sessions.size,
            sessions: Array.from(this.sessions.entries()).map(([id, state]) => ({
                sessionID: id,
                eventCount: state.eventCount,
                compactCount: state.compactCount,
                age: Date.now() - state.createdAt,
            })),
        };
    }
    /**
     * Cleanup orphaned session databases (crash recovery).
     * Deletes session DB files older than maxAgeHours.
     */
    async cleanupOrphanedSessions(maxAgeHours = 24) {
        const fs = await import("node:fs/promises");
        const sessionDir = this.getSessionDir();
        const now = Date.now();
        const maxAgeMs = maxAgeHours * 60 * 60 * 1000;
        let cleaned = 0;
        try {
            const dbFiles = await fs.readdir(sessionDir);
            for (const file of dbFiles) {
                if (!file.endsWith('.db'))
                    continue;
                const dbPath = join(sessionDir, file);
                const stats = await fs.stat(dbPath);
                const ageMs = now - stats.mtimeMs;
                if (ageMs > maxAgeMs) {
                    await fs.unlink(dbPath);
                    cleaned++;
                }
            }
        }
        catch (error) {
            console.error(`Failed to cleanup orphaned sessions:`, error);
        }
        return cleaned;
    }
    /**
     * Get session directory path (creates if not exists).
     */
    getSessionDir() {
        const dir = join(homedir(), ".config", "opencode", "context-mode", "sessions");
        mkdirSync(dir, { recursive: true });
        return dir;
    }
    /**
     * Get database file path for a session.
     */
    getDBPath(projectDir, sessionID) {
        const hash = createHash("sha256")
            .update(projectDir)
            .digest("hex")
            .slice(0, 16);
        return join(this.getSessionDir(), `${hash}-${sessionID.slice(0, 8)}.db`);
    }
}
// Singleton instance for plugin use
export const sessionManager = new SessionManager();
