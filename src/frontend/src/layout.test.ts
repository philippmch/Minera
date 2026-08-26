import { describe, expect, it } from "vitest";
import { computeLayout, edgeKey } from "./layout";
import { END, START, type ActivityStat, type EdgeStat, type MineraExport } from "./types";

function act(name: string, count: number, start = 0, end = 0): ActivityStat {
  return { name, count, start_count: start, end_count: end };
}

function exp(activities: ActivityStat[], edges: EdgeStat[]): MineraExport {
  return {
    dataset: "t",
    trace_count: 1,
    event_count: 1,
    activities,
    edges,
    variants: [],
  };
}

/** Every coordinate the renderer consumes must be a real number. */
function assertFinite(layout: ReturnType<typeof computeLayout>): void {
  const { fit } = layout;
  for (const v of [fit.x, fit.y, fit.w, fit.h]) expect(Number.isFinite(v)).toBe(true);
  expect(fit.w).toBeGreaterThan(0);
  expect(fit.h).toBeGreaterThan(0);
  for (const node of layout.nodes) {
    for (const v of [node.x, node.y, node.w, node.h]) expect(Number.isFinite(v)).toBe(true);
  }
  for (const edge of layout.edges) {
    expect(edge.path).not.toMatch(/NaN|Infinity/);
    expect(edge.headPoints).not.toMatch(/NaN|Infinity/);
    expect(Number.isFinite(edge.width)).toBe(true);
    expect(Number.isFinite(edge.label.x)).toBe(true);
    expect(Number.isFinite(edge.label.y)).toBe(true);
  }
}

describe("computeLayout", () => {
  it("lays out an empty log as just the two terminals", () => {
    // No activities and no edges must still yield a finite, renderable frame
    // rather than NaN bounds — the landing state before any real discovery.
    const layout = computeLayout(exp([], []));
    expect(layout.nodes).toHaveLength(0);
    expect(layout.terminals.map((t) => t.id)).toEqual([START, END]);
    expect(layout.edges).toHaveLength(0);
    assertFinite(layout);
  });

  it("connects a single activity between START and END", () => {
    const layout = computeLayout(exp([act("a", 3, 3, 3)], []));
    expect(layout.nodes).toHaveLength(1);
    const keys = layout.edges.map((e) => e.key);
    expect(keys).toContain(edgeKey(START, 0));
    expect(keys).toContain(edgeKey(0, END));
    assertFinite(layout);
  });

  it("routes a self-loop without producing NaN geometry", () => {
    const layout = computeLayout(exp([act("a", 5, 5, 5)], [{ source: 0, target: 0, count: 4 }]));
    expect(layout.edges.some((e) => e.source === 0 && e.target === 0)).toBe(true);
    assertFinite(layout);
  });

  it("detects a cycle as a back edge and stays finite", () => {
    // a -> b -> c -> a: the DFS must mark the returning edge as a back edge so
    // layering terminates, and the right-margin loop sweep keeps the frame finite.
    const data = exp(
      [act("a", 2, 2), act("b", 2), act("c", 2, 0, 2)],
      [
        { source: 0, target: 1, count: 2 },
        { source: 1, target: 2, count: 2 },
        { source: 2, target: 0, count: 1 },
      ],
    );
    const layout = computeLayout(data);
    expect(layout.edges.some((e) => e.back)).toBe(true);
    assertFinite(layout);
  });

  it("hides edge labels once the graph grows past the threshold", () => {
    const small = computeLayout(exp([act("a", 1, 1, 1)], []));
    expect(small.showLabels).toBe(true);

    // Build a dense graph with more than 80 edges (a 10-clique is 90).
    const activities = Array.from({ length: 10 }, (_, i) => act(`n${i}`, 1, 1, 1));
    const edges: EdgeStat[] = [];
    for (let i = 0; i < 10; i++) {
      for (let j = 0; j < 10; j++) {
        if (i !== j) edges.push({ source: i, target: j, count: 1 });
      }
    }
    const dense = computeLayout(exp(activities, edges));
    expect(dense.edges.length).toBeGreaterThan(80);
    expect(dense.showLabels).toBe(false);
    assertFinite(dense);
  });
});
