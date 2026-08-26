import { describe, expect, it } from "vitest";
import { computeTreeLayout } from "./tree-layout";
import { type ActivityStat, type MineraExport, type ProcessTreeNode } from "./types";

const act = (name: string): ActivityStat => ({
  name,
  count: 1,
  start_count: 0,
  end_count: 0,
});

function data(names: string[]): MineraExport {
  return {
    dataset: "t",
    trace_count: 1,
    event_count: 1,
    activities: names.map(act),
    edges: [],
    variants: [],
  };
}

describe("computeTreeLayout", () => {
  it("places an operator above its leaf children", () => {
    const root: ProcessTreeNode = {
      operator: "sequence",
      children: [
        { operator: "activity", activity: 0 },
        { operator: "activity", activity: 1 },
      ],
    };
    const layout = computeTreeLayout(root, data(["a", "b"]));

    expect(layout.nodes).toHaveLength(3);
    expect(layout.edges).toHaveLength(2);
    const op = layout.nodes.find((n) => n.kind === "operator")!;
    const leaves = layout.nodes.filter((n) => n.kind === "activity");
    expect(op.label).toBe("→");
    expect(leaves.map((l) => l.label).sort()).toEqual(["a", "b"]);
    // The operator sits above (smaller y) its children.
    for (const leaf of leaves) expect(op.y).toBeLessThan(leaf.y);
    for (const v of [layout.fit.x, layout.fit.y, layout.fit.w, layout.fit.h]) {
      expect(Number.isFinite(v)).toBe(true);
    }
  });

  it("renders an inclusive OR operator with a distinct glyph", () => {
    const layout = computeTreeLayout({
      operator: "inclusive_or",
      children: [
        { operator: "activity", activity: 0 },
        { operator: "activity", activity: 1 },
      ],
    }, data(["a", "b"]));
    const op = layout.nodes.find((node) => node.kind === "operator")!;
    expect(op.label).toBe("⊻");
    expect(op.operator).toBe("inclusive_or");
  });

  it("labels a tau leaf and a single-leaf tree", () => {
    const layout = computeTreeLayout({ operator: "tau" }, data([]));
    expect(layout.nodes).toHaveLength(1);
    expect(layout.nodes[0].kind).toBe("tau");
    expect(layout.nodes[0].label).toBe("τ");
  });

  it("falls back to '?' for an activity index out of range", () => {
    // A malformed tree (activity id past the end of the names) must render a
    // placeholder rather than crash on an undefined lookup.
    const layout = computeTreeLayout({ operator: "activity", activity: 9 }, data(["a"]));
    expect(layout.nodes[0].label).toBe("?");
  });

  it("marks the redo branches of a loop", () => {
    const root: ProcessTreeNode = {
      operator: "loop",
      children: [
        { operator: "activity", activity: 0 },
        { operator: "activity", activity: 1 },
      ],
    };
    const layout = computeTreeLayout(root, data(["a", "b"]));
    // First child is the "do" body (not redo); the rest are redo paths.
    expect(layout.edges.map((e) => e.redo)).toEqual([false, true]);
  });
});
