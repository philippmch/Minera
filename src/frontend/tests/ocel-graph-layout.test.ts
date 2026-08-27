import { describe, expect, it } from "vitest";
import {
  computeOcelGraphLayout,
  type EtotGraphData,
  type OtgGraphData,
} from "../src/ocel-graph-layout";

function etot(overrides: Partial<EtotGraphData> = {}): EtotGraphData {
  return {
    activities: ["Create", "Ship"],
    object_types: ["order", "item"],
    edges: [
      { activity: "Create", object_type: "order", frequency: 4 },
      { activity: "Create", object_type: "item", frequency: 2 },
      { activity: "Ship", object_type: "order", frequency: 3 },
    ],
    ...overrides,
  };
}

function otg(overrides: Partial<OtgGraphData> = {}): OtgGraphData {
  return {
    object_types: ["order", "item", "invoice"],
    edges: [
      { source_type: "order", relation: "object_interaction", target_type: "item", frequency: 4 },
      { source_type: "item", relation: "object_descendants", target_type: "invoice", frequency: 2 },
      { source_type: "invoice", relation: "object_interaction", target_type: "order", frequency: 1 },
    ],
    ...overrides,
  };
}

function assertFinite(layout: ReturnType<typeof computeOcelGraphLayout>): void {
  for (const value of [layout.fit.x, layout.fit.y, layout.fit.w, layout.fit.h]) {
    expect(Number.isFinite(value)).toBe(true);
  }
  for (const node of layout.nodes) {
    for (const value of [node.x, node.y, node.w, node.h, node.metric]) {
      expect(Number.isFinite(value)).toBe(true);
    }
  }
  for (const edge of layout.edges) {
    expect(edge.path).not.toMatch(/NaN|Infinity/);
    expect(edge.headPoints).not.toMatch(/NaN|Infinity/);
    expect(Number.isFinite(edge.frequency)).toBe(true);
    expect(Number.isFinite(edge.width)).toBe(true);
    expect(Number.isFinite(edge.opacity)).toBe(true);
  }
}

describe("computeOcelGraphLayout", () => {
  it("lays out ETOT nodes and keeps insertion order irrelevant", () => {
    const first = computeOcelGraphLayout(etot(), { kind: "etot" });
    const second = computeOcelGraphLayout(etot({
      activities: ["Ship", "Create", "Create"],
      object_types: ["item", "order"],
      edges: [
        { activity: "Ship", object_type: "order", frequency: 3 },
        { activity: "Create", object_type: "item", frequency: 2 },
        { activity: "Create", object_type: "order", frequency: 4 },
      ],
    }), { kind: "etot" });
    expect(second.nodes).toEqual(first.nodes);
    expect(second.edges).toEqual(first.edges);
    expect(second.fit).toEqual(first.fit);
    expect(first.nodes.map((node) => node.name)).toEqual(["Create", "Ship", "item", "order"]);
    expect(first.edges.map((edge) => edge.frequency)).toEqual([4, 3, 2]);
    expect(first.nodes.filter((node) => node.kind === "activity")).toHaveLength(2);
    assertFinite(first);
  });

  it("applies frequency thresholds and node/edge bounds", () => {
    const layout = computeOcelGraphLayout(etot(), {
      kind: "etot",
      frequencyThreshold: 3,
      maxNodes: 3,
      maxEdges: 1,
    });
    expect(layout.edges).toHaveLength(1);
    expect(layout.edges[0].frequency).toBe(4);
    expect(layout.truncatedEdges).toBe(true);
    expect(layout.nodes.length).toBeLessThanOrEqual(3);
    expect(layout.truncatedNodes).toBe(true);
    assertFinite(layout);
  });

  it("renders cyclic OTG relations with a visible return edge", () => {
    const topBottom = computeOcelGraphLayout(otg(), { kind: "otg" });
    const leftRight = computeOcelGraphLayout(otg(), { kind: "otg", rankDirection: "LR" });
    expect(topBottom.edges.map((edge) => edge.relation)).toEqual([
      "object_interaction", "object_descendants", "object_interaction",
    ]);
    expect(topBottom.edges.some((edge) => edge.back)).toBe(true);
    const orderTB = topBottom.nodes.find((node) => node.name === "order")!;
    const invoiceTB = topBottom.nodes.find((node) => node.name === "invoice")!;
    const orderLR = leftRight.nodes.find((node) => node.name === "order")!;
    const invoiceLR = leftRight.nodes.find((node) => node.name === "invoice")!;
    expect(invoiceTB.y).not.toBe(orderTB.y);
    expect(invoiceLR.x).not.toBe(orderLR.x);
    assertFinite(leftRight);
  });

  it("handles empty and self-loop graphs without non-finite geometry", () => {
    const empty = computeOcelGraphLayout({ object_types: [], edges: [] }, { kind: "otg" });
    expect(empty.nodes).toHaveLength(0);
    expect(empty.edges).toHaveLength(0);
    assertFinite(empty);
    const loop = computeOcelGraphLayout(otg({
      object_types: ["case"],
      edges: [{ source_type: "case", relation: "self", target_type: "case", frequency: 2 }],
    }), { kind: "otg" });
    expect(loop.edges).toHaveLength(1);
    expect(loop.edges[0].path).not.toMatch(/NaN|Infinity/);
    assertFinite(loop);
  });
});
