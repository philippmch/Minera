import { describe, expect, it, vi } from "vitest";
import { modelLayouts, type LayoutBuilders } from "../src/model-layouts";
import { type GraphLayout } from "../src/layout";
import { type PetriLayout } from "../src/petri-layout";
import { type TreeLayout } from "../src/tree-layout";
import { type MineraExport, type PetriNetModel, type ProcessTreeNode } from "../src/types";

const net = (label: string) => ({ label } as unknown as PetriNetModel);
const analysis = (extra: Partial<MineraExport> = {}): MineraExport => ({
  dataset: "t", trace_count: 1, event_count: 1, activities: [], edges: [], variants: [], ...extra,
});

function counting() {
  const builders = {
    processMap: vi.fn((data: MineraExport) => ({ data } as unknown as GraphLayout)),
    tree: vi.fn((root: ProcessTreeNode) => ({ root } as unknown as TreeLayout)),
    net: vi.fn((model: PetriNetModel) => ({ model } as unknown as PetriLayout)),
  } satisfies LayoutBuilders;
  return builders;
}

describe("modelLayouts", () => {
  const full = analysis({
    process_tree: { operator: "sequence", children: [] } as unknown as ProcessTreeNode,
    petri_net: net("alpha"), inductive_petri_net: net("inductive"), heuristic_petri_net: net("heuristic"), powl_petri_net: net("powl"),
  });

  it("lays nothing out when an analysis loads", () => {
    const builders = counting();
    const layouts = modelLayouts(full, builders);
    // Asking whether a model exists (the model view's toggles and export panels) lays nothing out either.
    expect([layouts.hasTree(), layouts.hasNet("alpha"), layouts.hasNet("inductive"), layouts.hasNet("heuristic"), layouts.hasNet("powl")]).toEqual([true, true, true, true, true]);
    expect(builders.processMap).not.toHaveBeenCalled();
    expect(builders.tree).not.toHaveBeenCalled();
    expect(builders.net).not.toHaveBeenCalled();
  });

  it("lays out only the diagram asked for, once per analysis", () => {
    const builders = counting();
    const layouts = modelLayouts(full, builders);
    const map = layouts.processMap();
    expect(layouts.processMap()).toBe(map);
    expect(builders.processMap).toHaveBeenCalledTimes(1);
    expect(builders.net).not.toHaveBeenCalled();

    const heuristic = layouts.net("heuristic");
    expect(layouts.net("heuristic")).toBe(heuristic);
    expect(builders.net).toHaveBeenCalledTimes(1);
    expect(builders.net).toHaveBeenLastCalledWith(full.heuristic_petri_net, full);
    expect(builders.tree).not.toHaveBeenCalled();

    const tree = layouts.tree();
    expect(layouts.tree()).toBe(tree);
    expect(builders.tree).toHaveBeenCalledTimes(1);
    for (const kind of ["alpha", "inductive", "powl"] as const) layouts.net(kind);
    for (const kind of ["alpha", "inductive", "heuristic", "powl"] as const) layouts.net(kind);
    expect(builders.net).toHaveBeenCalledTimes(4);
    expect(builders.net.mock.calls.map(([model]) => (model as unknown as { label: string }).label)).toEqual(["heuristic", "alpha", "inductive", "powl"]);
  });

  it("answers null without laying out a model the analysis does not carry, and remembers it", () => {
    const builders = counting();
    const layouts = modelLayouts(analysis({ petri_net: net("alpha") }), builders);
    expect(layouts.hasTree()).toBe(false);
    expect(layouts.tree()).toBeNull();
    expect(layouts.hasNet("powl")).toBe(false);
    expect(layouts.net("powl")).toBeNull();
    expect(layouts.net("powl")).toBeNull();
    expect(builders.tree).not.toHaveBeenCalled();
    expect(builders.net).not.toHaveBeenCalled();
  });

  it("starts over for a new analysis (a new log or run)", () => {
    const builders = counting();
    const first = modelLayouts(full, builders);
    const map = first.processMap();
    const next = modelLayouts({ ...full }, builders);
    expect(builders.processMap).toHaveBeenCalledTimes(1);
    expect(next.processMap()).not.toBe(map);
    expect(builders.processMap).toHaveBeenCalledTimes(2);
  });

  it("builds real layouts with the default layout functions", () => {
    const layouts = modelLayouts(analysis({ activities: [{ name: "a", count: 1, start_count: 1, end_count: 1 }], edges: [] }));
    expect(layouts.processMap().nodes.length).toBeGreaterThan(0);
    expect(layouts.net("alpha")).toBeNull();
  });
});
