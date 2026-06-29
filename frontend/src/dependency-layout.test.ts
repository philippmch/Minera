import { describe, expect, it } from "vitest";
import { computeDependencyLayout } from "./dependency-layout";
import { edgeKey } from "./layout";
import { type ActivityStat, type HeuristicsNet, type MineraExport } from "./types";

function data(activities: ActivityStat[]): MineraExport {
  return {
    dataset: "t",
    trace_count: 1,
    event_count: 1,
    activities,
    edges: [],
    variants: [],
  };
}

const act = (name: string): ActivityStat => ({
  name,
  count: 1,
  start_count: 0,
  end_count: 0,
});

describe("computeDependencyLayout", () => {
  it("lays out the net and exposes each edge's dependency", () => {
    const net: HeuristicsNet = {
      edges: [
        { source: 0, target: 1, dependency: 0.9, frequency: 8 },
        { source: 1, target: 2, dependency: 0.8, frequency: 6 },
      ],
      start_activities: [0],
      end_activities: [2],
    };
    const { graph, dependency } = computeDependencyLayout(net, data([act("a"), act("b"), act("c")]));

    expect(graph.nodes).toHaveLength(3);
    expect(dependency.get(edgeKey(0, 1))).toBe(0.9);
    expect(dependency.get(edgeKey(1, 2))).toBe(0.8);
    // The net's own start/end activities drive the terminal edges.
    expect(graph.edges.some((e) => e.key === edgeKey(-1, 0))).toBe(true);
    expect(graph.edges.some((e) => e.key === edgeKey(2, -2))).toBe(true);
  });

  it("handles a net with no edges", () => {
    const net: HeuristicsNet = { edges: [], start_activities: [], end_activities: [] };
    const { graph, dependency } = computeDependencyLayout(net, data([act("a")]));
    expect(dependency.size).toBe(0);
    expect(graph.nodes).toHaveLength(1);
    expect(Number.isFinite(graph.fit.w)).toBe(true);
  });
});
