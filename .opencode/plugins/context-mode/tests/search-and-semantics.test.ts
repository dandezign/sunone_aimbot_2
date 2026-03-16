/**
 * Search AND semantics test — proves quality improvement from issue #23.
 *
 * Before: sanitizeQuery joined with OR → "useEffect cleanup function"
 *         matched ANY chunk with ANY of those words.
 * After:  sanitizeQuery joins with AND → only chunks with ALL words match.
 *         OR is used as fallback when AND returns nothing.
 */

import { describe, test, expect } from "vitest";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { ContentStore } from "../src/store.js";

function createStore(): ContentStore {
  const path = join(
    tmpdir(),
    `context-mode-test-${Date.now()}-${Math.random().toString(36).slice(2)}.db`,
  );
  return new ContentStore(path);
}

describe("AND semantics (issue #23)", () => {
  test("multi-word query excludes irrelevant single-word matches", () => {
    const store = createStore();

    // Index two documents — one relevant, one only matches on "function"
    store.index({
      content: "## useEffect cleanup\nReturn a cleanup function from useEffect to avoid memory leaks.\nAlways clean up subscriptions and timers in the cleanup function.",
      source: "React Hooks Guide",
    });
    store.index({
      content: "## What is a function\nA function is a reusable block of code that performs a specific task.\nFunctions accept parameters and return values.",
      source: "JavaScript Basics",
    });

    // AND search: only the React chunk should match (has all 3 terms)
    const andResults = store.search("useEffect cleanup function", 5);
    expect(andResults.length).toBe(1);
    expect(andResults[0].source).toBe("React Hooks Guide");

    // OR search: both chunks match (JS Basics matches on "function" alone)
    const orResults = store.search("useEffect cleanup function", 5, undefined, "OR");
    expect(orResults.length).toBe(2);

    store.close();
  });

  test("searchWithFallback uses AND by default, falls back to OR", () => {
    const store = createStore();

    store.index({
      content: "## useEffect cleanup\nReturn a cleanup function from useEffect to avoid memory leaks.",
      source: "React Hooks Guide",
    });
    store.index({
      content: "## What is a function\nA function is a reusable block of code.",
      source: "JavaScript Basics",
    });

    // searchWithFallback should use AND first — only React chunk matches
    const results = store.searchWithFallback("useEffect cleanup function", 5);
    expect(results.length).toBe(1);
    expect(results[0].source).toBe("React Hooks Guide");

    store.close();
  });

  test("AND with no results falls back to OR gracefully", () => {
    const store = createStore();

    store.index({
      content: "## React components\nComponents are the building blocks of React applications.",
      source: "React Guide",
    });
    store.index({
      content: "## Vue components\nVue uses a template-based component system.",
      source: "Vue Guide",
    });

    // "React useState hooks" — AND would match nothing (no chunk has all 3),
    // searchWithFallback should fall back to OR and find the React chunk
    const results = store.searchWithFallback("React useState hooks", 5);
    expect(results.length).toBeGreaterThan(0);
    expect(results[0].source).toBe("React Guide");

    store.close();
  });

  test("single-word queries work the same in AND and OR", () => {
    const store = createStore();

    store.index({
      content: "## Authentication\nJWT tokens provide stateless authentication.",
      source: "Auth Guide",
    });

    const andResults = store.search("authentication", 5);
    const orResults = store.search("authentication", 5, undefined, "OR");
    expect(andResults.length).toBe(orResults.length);

    store.close();
  });

  test("trigram search also uses AND semantics", () => {
    const store = createStore();

    store.index({
      content: "## useEffect cleanup pattern\nReturn a cleanup function from useEffect.",
      source: "React Hooks",
    });
    store.index({
      content: "## JavaScript function basics\nA function is a reusable block of code.",
      source: "JS Basics",
    });

    // Trigram AND: partial match "useEff clean func" should only match React chunk
    const andResults = store.searchTrigram("useEffect cleanup function", 5, undefined, "AND");
    // With AND, only the chunk containing ALL terms should match
    for (const r of andResults) {
      expect(r.source).toBe("React Hooks");
    }

    store.close();
  });
});
