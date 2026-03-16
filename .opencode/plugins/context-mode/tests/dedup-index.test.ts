import { describe, it, expect, beforeEach, afterEach } from "vitest";
import { ContentStore } from "../src/store.js";

describe("Index deduplication (issue #67)", () => {
  let store: ContentStore;

  beforeEach(() => {
    store = new ContentStore(":memory:");
  });

  afterEach(() => {
    store.cleanup();
  });

  it("re-indexing with same label replaces previous content", () => {
    // First build: error A
    store.index({
      content: "# Build Output\nERROR: Module not found 'foo'",
      source: "execute:shell:npm run build",
    });

    // Verify error A is searchable
    const results1 = store.search("Module not found foo");
    expect(results1.length).toBeGreaterThan(0);
    expect(results1[0].content).toContain("Module not found");

    // Second build: error A fixed, new error B
    store.index({
      content: "# Build Output\nERROR: Type 'string' is not assignable to type 'number'",
      source: "execute:shell:npm run build",
    });

    // Error B should be searchable
    const results2 = store.search("Type string not assignable number");
    expect(results2.length).toBeGreaterThan(0);
    expect(results2[0].content).toContain("not assignable");

    // Error A should NO LONGER be searchable
    const results3 = store.search("Module not found foo");
    expect(results3.length).toBe(0);
  });

  it("different labels are NOT deduped", () => {
    store.index({
      content: "# Test Output\n5 tests passed",
      source: "execute:shell:npm test",
    });
    store.index({
      content: "# Build Output\nBuild successful",
      source: "execute:shell:npm run build",
    });

    // Both should be searchable
    const testResults = store.search("tests passed");
    expect(testResults.length).toBeGreaterThan(0);

    const buildResults = store.search("Build successful");
    expect(buildResults.length).toBeGreaterThan(0);
  });

  it("sources list shows only one entry per label after dedup", () => {
    store.index({ content: "# Run 1\nfail", source: "execute:shell:make" });
    store.index({ content: "# Run 2\nfail", source: "execute:shell:make" });
    store.index({ content: "# Run 3\npass", source: "execute:shell:make" });

    const sources = store.listSources();
    const makeEntries = sources.filter((s) => s.label === "execute:shell:make");
    expect(makeEntries.length).toBe(1);
    expect(makeEntries[0].chunkCount).toBeGreaterThan(0);
  });

  it("dedup works with indexPlainText too", () => {
    store.indexPlainText("error: old failure", "build-output");
    store.indexPlainText("success: all good", "build-output");

    const oldResults = store.search("old failure");
    expect(oldResults.length).toBe(0);

    const newResults = store.search("all good");
    expect(newResults.length).toBeGreaterThan(0);
  });

  it("dedup works with indexJSON too", () => {
    store.indexJSON(
      JSON.stringify({ status: "error", message: "connection refused" }),
      "api-response",
    );
    store.indexJSON(
      JSON.stringify({ status: "ok", data: [1, 2, 3] }),
      "api-response",
    );

    const oldResults = store.search("connection refused");
    expect(oldResults.length).toBe(0);

    const newResults = store.searchWithFallback("ok", 5);
    expect(newResults.length).toBeGreaterThan(0);
  });

  it("trigram search also returns only latest content after dedup", () => {
    store.index({
      content: "# Output\nxyz123oldvalue",
      source: "execute:shell:check",
    });
    store.index({
      content: "# Output\nabc456newvalue",
      source: "execute:shell:check",
    });

    // Trigram search for old unique substring
    const oldResults = store.searchWithFallback("xyz123oldvalue", 5);
    expect(oldResults.length).toBe(0);

    // Trigram search for new unique substring
    const newResults = store.searchWithFallback("abc456newvalue", 5);
    expect(newResults.length).toBeGreaterThan(0);
  });
});
