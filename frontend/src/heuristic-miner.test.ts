import { describe, expect, it } from "vitest";
import { discoverHeuristicsNet } from "./heuristic-miner";
import { type MineraExport } from "./types";

// Minimal export with activities and directly-follows edge counts.
function makeExport(
  names: string[],
  starts: number[],
  ends: number[],
  edges: { source: number; target: number; count: number }[],
): MineraExport {
  return {
    dataset: "test",
    trace_count: 1,
    event_count: 1,
    activities: names.map((name, id) => ({
      name,
      count: 1,
      start_count: starts.includes(id) ? 1 : 0,
      end_count: ends.includes(id) ? 1 : 0,
    })),
    edges,
    variants: [],
  };
}

function edgeFor(
  net: ReturnType<typeof discoverHeuristicsNet>,
  s: number,
  t: number,
) {
  return net.edges.find((e) => e.source === s && e.target === t) ?? null;
}

describe("discoverHeuristicsNet", () => {
  it("keeps a clean A→B→C sequence above the threshold", () => {
    // 10 identical traces: all edges have dependency ≈ 1.0.
    const data = makeExport(
      ["A", "B", "C"],
      [0],
      [2],
      [
        { source: 0, target: 1, count: 10 },
        { source: 1, target: 2, count: 10 },
      ],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.9,
      minFrequency: 1,
    });

    expect(edgeFor(net, 0, 1)).not.toBeNull();
    expect(edgeFor(net, 1, 2)).not.toBeNull();
    expect(edgeFor(net, 1, 0)).toBeNull();
    expect(net.start_activities).toContain(0);
    expect(net.end_activities).toContain(2);
  });

  it("drops a weak edge below the dependency threshold", () => {
    // A→B (10×) and B→A (9×): dependency = (10-9)/(10+9+1) = 0.05 — below 0.9.
    const data = makeExport(
      ["A", "B"],
      [0],
      [1],
      [
        { source: 0, target: 1, count: 10 },
        { source: 1, target: 0, count: 9 },
      ],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.9,
      minFrequency: 1,
      connectAll: false,
    });

    // Neither direction clears the threshold; both dropped.
    expect(edgeFor(net, 0, 1)).toBeNull();
    expect(edgeFor(net, 1, 0)).toBeNull();
  });

  it("drops edges below minFrequency even with a good dependency", () => {
    // A→B (10×) has high dependency; A→C (1×) has positive dependency but
    // fails the frequency filter.
    const data = makeExport(
      ["A", "B", "C"],
      [0],
      [1, 2],
      [
        { source: 0, target: 1, count: 10 },
        { source: 0, target: 2, count: 1 },
      ],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.0,
      minFrequency: 2,
      connectAll: false,
    });

    expect(edgeFor(net, 0, 1)).not.toBeNull();
    expect(edgeFor(net, 0, 2)).toBeNull();
  });

  it("connect_all=false leaves a dangling non-end activity with no outgoing edges", () => {
    // 5 traces A→B: dep(A,B) = 5/6 ≈ 0.833, below the 0.9 threshold.
    // A is a start activity, never an end activity.  Without connect_all the
    // threshold gap leaves A with no accepted outgoing edge.
    const data = makeExport(
      ["A", "B"],
      [0],
      [1],
      [{ source: 0, target: 1, count: 5 }],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.9,
      minFrequency: 1,
      connectAll: false,
    });

    const outA = net.edges.filter((e) => e.source === 0);
    expect(outA).toHaveLength(0);
  });

  it("connect_all=true forces the strongest outgoing edge for a dangling activity", () => {
    // Same 5-trace A→B log: dep below 0.9, but dep > 0, so connect_all
    // forces the edge even though the threshold rejected it.
    const data = makeExport(
      ["A", "B"],
      [0],
      [1],
      [{ source: 0, target: 1, count: 5 }],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.9,
      minFrequency: 1,
      connectAll: true,
    });

    const outA = net.edges.filter((e) => e.source === 0);
    expect(outA).toHaveLength(1);
    expect(outA[0].target).toBe(1);
  });

  it("detects a self-loop", () => {
    // A→A (5×) and A→B (5×).  Self-loop dependency = 5/(5+1) ≈ 0.83.
    const data = makeExport(
      ["A", "B"],
      [0],
      [1],
      [
        { source: 0, target: 0, count: 5 },
        { source: 0, target: 1, count: 5 },
      ],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.8,
      minFrequency: 1,
    });

    expect(edgeFor(net, 0, 0)).not.toBeNull();
    expect(edgeFor(net, 0, 0)!.dependency).toBeGreaterThan(0.8);
  });

  it("dependency formula matches the backend: (f-r)/(f+r+1) for non-self-loops", () => {
    // A→B 7×, B→A 3×: dep = (7-3)/(7+3+1) = 4/11 ≈ 0.3636.
    const data = makeExport(
      ["A", "B"],
      [0],
      [1],
      [
        { source: 0, target: 1, count: 7 },
        { source: 1, target: 0, count: 3 },
      ],
    );
    const net = discoverHeuristicsNet(data, {
      dependencyThreshold: 0.0,
      minFrequency: 1,
      connectAll: false,
    });

    const ab = edgeFor(net, 0, 1);
    expect(ab).not.toBeNull();
    expect(ab!.dependency).toBeCloseTo(4 / 11, 6);
  });
});
