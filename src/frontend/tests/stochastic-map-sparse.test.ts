import { describe, expect, it, vi } from "vitest";
import { fetchStochasticMap, monteCarloQuery, stochasticMapQuery } from "../src/api";
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
  it("can reuse signed map metadata with an explicit simulation duration", () => {
    for (const distribution of ["normal", "uniform"] as const) {
      const query = monteCarloQuery({
        provided_stochastic_map: [{ transition: 0, timing: "timed", distribution,
          parameter: -4, secondary_parameter: distribution === "uniform" ? -1 : 1,
          tertiary_parameter: 0, activation_weight: 1, priority: 0 }],
        transition_durations: [{ distribution: "deterministic", value_seconds: 5 }],
      });
      expect(new URLSearchParams(query).get("monte_carlo_provided_stochastic_map"))
        .toContain(`0:timed:${distribution}:-4:`);
      expect(new URLSearchParams(query).get("monte_carlo_transition_durations"))
        .toContain("deterministic:5");
    }
  });

  it("requests automatic log fitting and keeps signed distribution statistics", () => {
    expect(stochasticMapQuery({ distribution: "automatic" })).toContain("stochastic_map_distribution=automatic");
    expect(() => stochasticMapQuery({ source: "performance_dfg", distribution: "automatic" }))
      .toThrow(/source log/);
    const base = payload([0]);
    const result = (distribution: string, parameter: number, secondary: number) => ({
      ...base,
      map: { ...base.map, transitions: [{ ...base.map.transitions[0],
        timing: "timed", distribution, parameter, secondary_parameter: secondary, priority: 0,
      }] },
      reachability: { ...base.reachability, states: [], generator: [], exhausted: true,
        diagnostic: "CTMC unavailable: fitted mean is negative" },
    });
    expect(isStochasticMapResult(result("normal", -2, 1))).toBe(true);
    expect(isStochasticMapResult(result("uniform", -4, -1))).toBe(true);
    expect(isStochasticMapResult(result("normal", -2, -1))).toBe(false);
    expect(isStochasticMapResult(result("uniform", -1, -4))).toBe(false);
    expect(isStochasticMapResult(result("exponential", -2, 0))).toBe(false);
  });

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
