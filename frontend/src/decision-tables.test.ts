import { describe, expect, it, vi } from "vitest";
import { requestDecisionTables } from "./api";

describe("decision table API", () => {
  const payload = {
    family: "DECISION_TABLES",
    variant: "encoded",
    trace_count: 1,
    observed_traces: 1,
    tables: [{
      place: 1,
      classes: ["B", "C"],
      feature_names: ["amount"],
      rows: [{ features: [10], target: 0 }],
    }],
  };

  it("requests and validates encoded tables", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/decision-tables?miner=inductive");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestDecisionTables()).resolves.toMatchObject({
      family: "DECISION_TABLES",
    });
    vi.unstubAllGlobals();
  });

  it("rejects an empty miner", async () => {
    await expect(requestDecisionTables(" ")).rejects.toThrow(/must not be empty/);
  });
});
