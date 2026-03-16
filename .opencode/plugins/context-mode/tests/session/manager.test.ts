import { strict as assert } from "node:assert";
import { afterAll, describe, test } from "vitest";
import { SessionManager } from "../../src/session/manager.js";

const cleanups: Array<() => void> = [];

afterAll(() => {
  for (const fn of cleanups) {
    try {
      fn();
    } catch {
      // ignore cleanup errors
    }
  }
});

describe("SessionManager", () => {
  test("creates new session state for unknown sessionID", () => {
    const manager = new SessionManager();
    const state = manager.getOrCreate("test-session-123", "/tmp/project");
    
    assert.ok(state, "State should be defined");
    assert.ok(state.db, "DB should be defined");
    assert.equal(state.eventCount, 0, "Event count should be 0");
    assert.equal(state.compactCount, 0, "Compact count should be 0");
  });
  
  test("returns existing session state for known sessionID", () => {
    const manager = new SessionManager();
    const state1 = manager.getOrCreate("test-session-123", "/tmp/project");
    const state2 = manager.getOrCreate("test-session-123", "/tmp/project");
    
    assert.strictEqual(state1, state2, "Should return same reference");
  });
  
  test("deletes session state on delete", () => {
    const manager = new SessionManager();
    manager.getOrCreate("test-session-123", "/tmp/project");
    manager.delete("test-session-123");
    
    const state = manager.get("test-session-123");
    assert.equal(state, undefined, "State should be undefined after delete");
  });
});
