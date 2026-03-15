/**
 * SessionManager tests
 */

import { strict as assert } from "node:assert"
import { afterAll, describe, test } from "vitest"
import { SessionManager } from "../../src/session/manager.js"

const cleanups: Array<() => void> = []

afterAll(() => {
  for (const cleanup of cleanups) {
    cleanup()
  }
})

describe("SessionManager", () => {
  test("creates new session state for unknown sessionID", () => {
    const manager = new SessionManager()
    const state = manager.getOrCreate("test-session-123", "/tmp/project")
    
    assert.ok(state, "state should be defined")
    assert.ok(state.db, "db should be defined")
    assert.strictEqual(state.eventCount, 0, "eventCount should start at 0")
    assert.strictEqual(state.compactCount, 0, "compactCount should start at 0")
    
    cleanups.push(() => manager.delete("test-session-123"))
  })
  
  test("returns existing session state for known sessionID", () => {
    const manager = new SessionManager()
    const state1 = manager.getOrCreate("test-session-456", "/tmp/project")
    const state2 = manager.getOrCreate("test-session-456", "/tmp/project")
    
    assert.strictEqual(state1, state2, "should return same reference")
    
    cleanups.push(() => manager.delete("test-session-456"))
  })
  
  test("deletes session state on delete", () => {
    const manager = new SessionManager()
    manager.getOrCreate("test-session-789", "/tmp/project")
    manager.delete("test-session-789")
    
    const state = manager.get("test-session-789")
    assert.strictEqual(state, undefined, "session should be undefined after delete")
  })
  
  test("records events correctly", () => {
    const manager = new SessionManager()
    manager.getOrCreate("test-session-events", "/tmp/project")
    
    manager.recordEvent("test-session-events")
    manager.recordEvent("test-session-events")
    manager.recordEvent("test-session-events")
    
    const state = manager.get("test-session-events")
    assert.strictEqual(state?.eventCount, 3, "eventCount should be 3")
    
    cleanups.push(() => manager.delete("test-session-events"))
  })
  
  test("getStats returns active sessions", () => {
    const manager = new SessionManager()
    manager.getOrCreate("stats-1", "/tmp/project")
    manager.getOrCreate("stats-2", "/tmp/project")
    
    const stats = manager.getStats()
    assert.strictEqual(stats.activeSessions, 2, "should have 2 active sessions")
    assert.strictEqual(stats.sessions.length, 2, "sessions array should have 2 items")
    
    cleanups.push(() => {
      manager.delete("stats-1")
      manager.delete("stats-2")
    })
  })
})
