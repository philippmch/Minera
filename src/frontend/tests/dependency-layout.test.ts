import { describe, expect, it } from "vitest";
import { computeDependencyLayout } from "../src/dependency-layout";
import { edgeKey } from "../src/layout";
import { type ActivityStat, type HeuristicsNet, type MineraExport } from "../src/types";

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

  it("omits classic activities that were not materialized as heuristic nodes", () => {
    const net: HeuristicsNet = {
      variant: "classic",
      edges: [{ source: 0, target: 2, dependency: 0.75, frequency: 3 }],
      start_activities: [0, 1],
      end_activities: [1, 2],
      active_activities: [0, 2],
    };
    const { graph, dependency } = computeDependencyLayout(
      net,
      data([act("a"), act("filtered"), act("c")]),
    );
    expect(graph.nodes.map((node) => node.name)).toEqual(["a", "c"]);
    expect(dependency.get(edgeKey(0, 1))).toBe(0.75);
    expect(graph.edges.some((edge) => edge.key === edgeKey(-1, 0))).toBe(true);
    expect(graph.edges.some((edge) => edge.key === edgeKey(1, -2))).toBe(true);
  });

  it("uses a local label table for alternate activity fields", () => {
    const net: HeuristicsNet = {
      variant: "classic",
      activity_ids_are_local: true,
      activities: ["phase-a", "phase-b"],
      active_activities: [0, 1],
      edges: [{ source: 0, target: 1, dependency: 0.8, frequency: 2 }],
      start_activities: [0],
      end_activities: [1],
      activity_statistics: [
        { activity: 0, occurrences: 2, start_occurrences: 1, end_occurrences: 0, mean_sojourn_seconds: 0 },
        { activity: 1, occurrences: 2, start_occurrences: 0, end_occurrences: 1, mean_sojourn_seconds: 0 },
      ],
    };
    const { graph, dependency } = computeDependencyLayout(
      net,
      data([act("canonical-a"), act("canonical-b")]),
    );
    expect(graph.nodes.map((node) => node.name)).toEqual(["phase-a", "phase-b"]);
    expect(dependency.get(edgeKey(0, 1))).toBe(0.8);
  });
});
