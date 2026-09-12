import { describe, expect, it, vi } from "vitest";
import { fetchStochasticMap } from "../src/api";
import { isStochasticMapResult } from "../src/types";

function payload(indices: number[]) {
  return {
    family: "STOCHASTIC_MAP", source: "log",
    map: { family: "STOCHASTIC_MAP", transitions: indices.map((transition) => ({
      transition, timing: "immediate", distribution: "immediate",
      parameter: 0, secondary_parameter: 0, tertiary_parameter: 0,
      activation_weight: 1, priority: 1,
    })) },
    diagnostics: { replayed_traces: 1, skipped_traces: 0 },
    reachability: {
      family: "STOCHASTIC_PETRI", variant: "TANGIBLE_CTMC", exhausted: false,
      diagnostic: "", states: [[0, 1]], edges: [], generator: [[0]],
    },
  };
}

describe("sparse fitted stochastic maps", () => {
  it("keeps transition IDs when consuming a sparse API result", async () => {
    const result = payload([2, 5]);
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => result })));
    try {
      await expect(fetchStochasticMap("imported")).resolves.toEqual(result);
    } finally {
      vi.unstubAllGlobals();
    }
  });

  it("accepts empty and dense maps and rejects ambiguous or invalid IDs", () => {
    expect(isStochasticMapResult(payload([]))).toBe(true);
    expect(isStochasticMapResult(payload([0, 1]))).toBe(true);
    for (const indices of [[2, 2], [5, 2], [-1], [0.5], [Number.MAX_SAFE_INTEGER + 1]])
      expect(isStochasticMapResult(payload(indices))).toBe(false);
  });
});
