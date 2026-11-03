import { afterEach, describe, expect, it, vi } from "vitest";
import { fetchPlayout } from "../src/api";
import { SIMULATED_LOG_TRACES, simulatedLogOptions } from "../src/components/ProcessModel";

afterEach(() => vi.unstubAllGlobals());

describe("Simulate log (XES)", () => {
  it("asks Petri miners for pm4py's 1000 traces explicitly", () => {
    expect(SIMULATED_LOG_TRACES).toBe(1000);
    for (const miner of ["inductive", "alpha", "heuristic", "ilp", "powl", "imported"])
      expect(simulatedLogOptions(miner)).toEqual({ traces: 1000 });
    for (const miner of ["declare", "dfg", "process_tree", "stochastic", "monte_carlo"])
      expect(simulatedLogOptions(miner)).toEqual({});
  });

  it("sends the explicit count on the playout request", async () => {
    const paths: string[] = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      paths.push(path);
      return new Response("<log></log>", { headers: { "Content-Type": "application/xml" } });
    }));
    await fetchPlayout("inductive", simulatedLogOptions("inductive"), "7");
    expect(paths).toHaveLength(1);
    const url = new URL(paths[0], "http://localhost");
    expect(url.pathname).toBe("/api/playout");
    expect(url.searchParams.get("miner")).toBe("inductive");
    expect(url.searchParams.get("traces")).toBe("1000");
    expect(url.searchParams.get("source_id")).toBe("7");
  });
});
