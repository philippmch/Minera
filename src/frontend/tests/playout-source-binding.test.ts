import { afterEach, describe, expect, it, vi } from "vitest";
import { fetchBasicPlayoutVisited, fetchDfgPlayout, fetchMonteCarloPlayout, fetchPlayout, fetchProcessTreePlayoutDownload, fetchProcessTreePlayoutSet, fetchStochasticPlayoutVisited, playoutUrl } from "../src/api";

// Playout runs on the server's mined session; every request names the mined
// source the page shows so the server can refuse a replaced session.
describe("playout requests are bound to the displayed source", () => {
  afterEach(() => vi.unstubAllGlobals());

  it("appends the source identity to query URLs", () => {
    expect(playoutUrl("/api/playout?miner=dfg", "42")).toBe("/api/playout?miner=dfg&source_id=42");
    expect(playoutUrl("/api/playout", "a b")).toBe("/api/playout?source_id=a%20b");
    expect(playoutUrl("/api/playout?miner=dfg")).toBe("/api/playout?miner=dfg");
  });

  it("sends source_id from every playout entry point", async () => {
    const seen: string[] = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      seen.push(init?.body ? `${path}?${String(init.body)}` : path);
      return { ok: true, text: async () => "<log/>", json: async () => ({}) };
    }));
    await fetchPlayout("process_tree", { traces: 2 }, "7");
    await fetchPlayout("inductive", { mode: "basic", traces: 2 }, "7");
    await fetchPlayout("stochastic", { traces: 2 }, "7");
    await fetchDfgPlayout({}, "7");
    await fetchMonteCarloPlayout("inductive", {}, "7");
    await fetchBasicPlayoutVisited("inductive", { traces: 1 }, "7").catch(() => undefined);
    await fetchStochasticPlayoutVisited({ traces: 1 }, "7").catch(() => undefined);
    await fetchProcessTreePlayoutSet({}, "7").catch(() => undefined);
    expect(seen).toHaveLength(8);
    for (const request of seen) expect(request).toMatch(/[?&]source_id=7(&|$)/);
  });

  it("reads the server's truncation header with the process-tree XES download", async () => {
    const seen: string[] = [];
    let truncated = "true";
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      seen.push(path);
      return { ok: true, text: async () => "<log/>", headers: new Headers({ "X-Minera-Truncated": truncated }) };
    }));
    await expect(fetchProcessTreePlayoutDownload({ variant: "EXTENSIVE", max_trace_cap: 5 }, "7"))
      .resolves.toEqual({ xes: "<log/>", truncated: true });
    truncated = "false";
    await expect(fetchProcessTreePlayoutDownload({ variant: "EXTENSIVE" }, "7"))
      .resolves.toEqual({ xes: "<log/>", truncated: false });
    expect(seen[0]).toMatch(/^\/api\/playout\?miner=process_tree&.*max_trace_cap=5.*&source_id=7$/);
  });
});
