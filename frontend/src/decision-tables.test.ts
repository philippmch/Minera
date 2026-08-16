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

  it("serializes bounded tree controls and selected attributes", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe(
        "/api/decision-tables?miner=alpha&decision_max_depth=2&decision_min_split=4&decision_min_leaf=2&decision_max_categories=3&decision_max_thresholds=5&decision_min_gain=0.25&decision_include_trace_attributes=false&decision_attributes=amount%2Cregion",
      );
      return { ok: true, json: async () => payload };
    }));
    await expect(requestDecisionTables({
      miner: "alpha", max_depth: 2, min_split: 4, min_leaf: 2,
      max_categories: 3, max_thresholds: 5, min_gain: 0.25,
      include_trace_attributes: false, attributes: ["amount", "region"],
    })).resolves.toMatchObject({ family: "DECISION_TABLES" });
    vi.unstubAllGlobals();
  });

  it("rejects unsafe decision controls before fetching", async () => {
    await expect(requestDecisionTables({ max_depth: 65 })).rejects.toThrow(/at most 64/);
    await expect(requestDecisionTables({ attributes: ["amount,region"] })).rejects.toThrow(/comma-free/);
  });
});
