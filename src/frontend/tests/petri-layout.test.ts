import { describe, expect, it } from "vitest";
import { computePetriLayout } from "../src/petri-layout";
import { type ActivityStat, type MineraExport, type PetriNetModel } from "../src/types";

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

function assertFinite(layout: ReturnType<typeof computePetriLayout>): void {
  for (const v of [layout.fit.x, layout.fit.y, layout.fit.w, layout.fit.h]) {
    expect(Number.isFinite(v)).toBe(true);
  }
  for (const p of layout.places) {
    expect(Number.isFinite(p.x) && Number.isFinite(p.y)).toBe(true);
  }
  for (const t of layout.transitions) {
    for (const v of [t.x, t.y, t.w, t.h]) expect(Number.isFinite(v)).toBe(true);
  }
  for (const a of layout.arcs) expect(a.path).not.toMatch(/NaN|Infinity/);
}

describe("computePetriLayout", () => {
  it("draws one labelled arc per arc object", () => {
    // a consumes a weight-2 arc (two repeated endpoints) from the source and
    // produces two parallel weight-1 arcs plus a weight-3 arc (compact side).
    const net: PetriNetModel = {
      place_count: 3,
      source: 0,
      sink: 2,
      transitions: [
        { activity: 0, inputs: [0, 0], outputs: [1, 1, 2], output_weights: [1, 1, 3] },
        { activity: 1, inputs: [1], outputs: [2] },
      ],
    };
    const layout = computePetriLayout(net, data(["a", "b"]));
    const ofA = layout.arcs.filter((arc) => arc.transition === 0);
    expect(ofA.filter((arc) => arc.input).map((arc) => arc.weight)).toEqual(["2"]);
    expect(ofA.filter((arc) => !arc.input).map((arc) => [arc.place, arc.weight])).toEqual([[1, null], [1, null], [2, "3"]]);
    expect(layout.arcs.filter((arc) => arc.transition === 1).every((arc) => arc.weight === null)).toBe(true);
    assertFinite(layout);
    for (const arc of layout.arcs) expect(Number.isFinite(arc.labelX) && Number.isFinite(arc.labelY)).toBe(true);
  });

  it("lays out a simple sequence net with no unconnected lane", () => {
    // source -[a]-> p1 -[b]-> sink
    const net: PetriNetModel = {
      place_count: 3,
      source: 0,
      sink: 2,
      transitions: [
        { activity: 0, inputs: [0], outputs: [1] },
        { activity: 1, inputs: [1], outputs: [2] },
      ],
    };
    const layout = computePetriLayout(net, data(["a", "b"]));

    expect(layout.places).toHaveLength(3);
    expect(layout.transitions).toHaveLength(2);
    expect(layout.arcs).toHaveLength(4); // one per input/output place
    expect(layout.unconnectedY).toBeNull();
    assertFinite(layout);
  });

  it("drops a transition with no arcs into the unconnected lane", () => {
    const net: PetriNetModel = {
      place_count: 3,
      source: 0,
      sink: 2,
      transitions: [
        { activity: 0, inputs: [0], outputs: [1] },
        { activity: 1, inputs: [1], outputs: [2] },
        { activity: 2, inputs: [], outputs: [] }, // short-loop activity, no arcs
      ],
    };
    const layout = computePetriLayout(net, data(["a", "b", "c"]));

    expect(layout.unconnectedY).not.toBeNull();
    expect(Number.isFinite(layout.unconnectedY!)).toBe(true);
    assertFinite(layout);
  });

  it("renders a silent transition as tau", () => {
    const net: PetriNetModel = {
      place_count: 2,
      source: 0,
      sink: 1,
      transitions: [{ activity: null, inputs: [0], outputs: [1] }],
    };
    const layout = computePetriLayout(net, data([]));
    expect(layout.transitions[0].label).toBe("τ");
    assertFinite(layout);
  });
});
