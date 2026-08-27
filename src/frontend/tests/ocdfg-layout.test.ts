import { describe, expect, it } from "vitest";
import {
  aggregateOcdfgPerformance,
  computeOcdfgLayout,
  ocdfgActivityMetricLabel,
  ocdfgEdgeMetricLabel,
  type OcdfgGraphData,
} from "../src/ocdfg-layout";

function graph(overrides: Partial<OcdfgGraphData> = {}): OcdfgGraphData {
  return {
    activities: ["Create", "Pack", "Ship"],
    object_types: ["order"],
    activities_indep: {
      Create: { events: ["e1", "e2"], unique_objects: ["o1", "o2"] },
      Pack: { events: ["e3"], unique_objects: ["o1"] },
      Ship: { events: ["e4"], unique_objects: ["o1"] },
    },
    edges: {
      order: {
        "Create->Pack": {
          source_activity: "Create",
          target_activity: "Pack",
          event_couples: [["e1", "e3"]],
          unique_objects: ["o1"],
          event_objects: [["e1", "e3", "o1"]],
          total_objects: [["e1", "e3", "o1"]],
          performance: [4],
        },
        "Pack->Ship": {
          source_activity: "Pack",
          target_activity: "Ship",
          event_couples: [["e3", "e4"]],
          unique_objects: ["o1"],
          event_objects: [["e3", "e4", "o1"]],
          total_objects: [["e3", "e4", "o1"]],
          performance: [8],
        },
      },
    },
    ...overrides,
  };
}

function assertFiniteLayout(layout: ReturnType<typeof computeOcdfgLayout>): void {
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
    expect(Number.isFinite(edge.metric)).toBe(true);
    expect(Number.isFinite(edge.width)).toBe(true);
    expect(Number.isFinite(edge.opacity)).toBe(true);
  }
}

describe("computeOcdfgLayout", () => {
  it("is deterministic and independent of JSON insertion order", () => {
    const first = graph();
    const second = graph({
      activities: ["Ship", "Create", "Pack", "Pack"],
      activities_indep: {
        Ship: first.activities_indep && (first.activities_indep as Record<string, unknown>).Ship,
        Pack: first.activities_indep && (first.activities_indep as Record<string, unknown>).Pack,
        Create: first.activities_indep && (first.activities_indep as Record<string, unknown>).Create,
      },
      edges: {
        order: {
          "Pack->Ship": (first.edges as Record<string, Record<string, unknown>>).order["Pack->Ship"],
          "Create->Pack": (first.edges as Record<string, Record<string, unknown>>).order["Create->Pack"],
        },
      },
    });
    const one = computeOcdfgLayout(first);
    const two = computeOcdfgLayout(second);
    expect(two.nodes).toEqual(one.nodes);
    expect(two.edges).toEqual(one.edges);
    expect(two.fit).toEqual(one.fit);
  });

  it("filters activities and flows using the selected metrics", () => {
    const layout = computeOcdfgLayout(graph(), {
      activityMetric: "unique_objects",
      activityThreshold: 2,
      edgeMetric: "event_objects",
      edgeThreshold: 2,
    });
    expect(layout.activityMetricLabel).toBe("unique objects");
    expect(layout.edgeMetricLabel).toBe("event-object triples");
    expect(layout.nodes.map((node) => node.name)).toEqual(["Create"]);
    expect(layout.edges).toHaveLength(0);
  });

  it("merges duplicate source-target flows and retains object-type annotations", () => {
    const data = graph({
      edges: {
        order: {
          "Create->Pack": {
            event_couples: [["e1", "e3"]],
            unique_objects: ["o1"],
            event_objects: [["e1", "e3", "o1"]],
          },
        },
        item: {
          "Create->Pack": {
            source_activity: "Create",
            target_activity: "Pack",
            event_couples: [["e1", "e3"]],
            unique_objects: ["i1"],
            event_objects: [["e1", "e3", "i1"]],
          },
        },
      },
    });
    const layout = computeOcdfgLayout(data, { edgeMetric: "unique_objects" });
    expect(layout.edges).toHaveLength(1);
    expect(layout.edges[0].metric).toBe(2);
    expect(layout.edges[0].objectTypes).toEqual(["item", "order"]);
  });

  it("supports performance aggregation and finite-value rejection", () => {
    expect(aggregateOcdfgPerformance([1, 5, 9], "median")).toBe(5);
    expect(aggregateOcdfgPerformance([1, 5, 9], "mean")).toBe(5);
    expect(aggregateOcdfgPerformance([Number.NaN, Number.POSITIVE_INFINITY, 7], "max")).toBe(7);
    expect(aggregateOcdfgPerformance([Number.NaN, Number.NEGATIVE_INFINITY], "mean")).toBeNull();
    const layout = computeOcdfgLayout(graph({
      edges: {
        order: {
          "Create->Pack": {
            performance: [Number.NaN, Number.POSITIVE_INFINITY],
          },
          "Pack->Ship": { performance: [2, 6] },
        },
      },
    }), { edgeMetric: "performance", performanceAggregation: "mean" });
    expect(layout.edges).toHaveLength(1);
    expect(layout.edges[0].metric).toBe(4);
    assertFiniteLayout(layout);
    const canonical = computeOcdfgLayout(graph({
      edges: {
        order: {
          "Create->Pack": {
            performance: [20, 30],
            performance_total_objects: [20, 30],
            performance_event_couples: [1],
          },
        },
      },
    }), { edgeMetric: "performance", performanceAggregation: "mean" });
    expect(canonical.edges[0].metric).toBe(25);
  });

  it("keeps empty and large graphs finite and bounded", () => {
    const empty = computeOcdfgLayout({ activities: [], edges: {} });
    expect(empty.nodes).toHaveLength(0);
    expect(empty.edges).toHaveLength(0);
    assertFiniteLayout(empty);

    const activities = Array.from({ length: 100 }, (_, index) => `a${index}`);
    const edges: Record<string, Record<string, unknown>> = { order: {} };
    for (let index = 0; index < 99; ++index) {
      edges.order[`a${index}->a${index + 1}`] = {
        event_couples: [[`e${index}`, `e${index + 1}`]],
      };
    }
    const large = computeOcdfgLayout({ activities, edges }, { maxNodes: 12, maxEdges: 8 });
    expect(large.nodes.length).toBeLessThanOrEqual(12);
    expect(large.edges.length).toBeLessThanOrEqual(8);
    expect(large.truncatedActivities).toBe(true);
    assertFiniteLayout(large);
  });

  it("changes rank orientation without changing semantic ordering", () => {
    const topBottom = computeOcdfgLayout(graph(), { rankDirection: "TB" });
    const leftRight = computeOcdfgLayout(graph(), { rankDirection: "LR" });
    const createTB = topBottom.nodes.find((node) => node.name === "Create")!;
    const shipTB = topBottom.nodes.find((node) => node.name === "Ship")!;
    const createLR = leftRight.nodes.find((node) => node.name === "Create")!;
    const shipLR = leftRight.nodes.find((node) => node.name === "Ship")!;
    expect(shipTB.y).toBeGreaterThan(createTB.y);
    expect(shipLR.x).toBeGreaterThan(createLR.x);
    assertFiniteLayout(leftRight);
  });

  it("keeps long labels separated across horizontal ranks", () => {
    const long = "activity-with-a-deliberately-long-label-that-needs-room";
    const data: OcdfgGraphData = {
      activities: [long, "Finish"],
      activities_indep: {
        [long]: { events: ["e1"] },
        Finish: { events: ["e2"] },
      },
      edges: {
        order: {
          [`${long}->Finish`]: {
            source_activity: long,
            target_activity: "Finish",
            event_couples: [["e1", "e2"]],
          },
        },
      },
    };
    const layout = computeOcdfgLayout(data, { rankDirection: "LR" });
    const source = layout.nodes.find((node) => node.name === long)!;
    const target = layout.nodes.find((node) => node.name === "Finish")!;
    expect(target.x - source.x).toBeGreaterThan((source.w + target.w) / 2);
    assertFiniteLayout(layout);
  });
});

describe("OCDFG metric labels", () => {
  it("names activity, flow, and performance metrics", () => {
    expect(ocdfgActivityMetricLabel("events")).toBe("events");
    expect(ocdfgActivityMetricLabel("total_objects")).toBe("total-object pairs");
    expect(ocdfgEdgeMetricLabel("event_couples")).toBe("event couples");
    expect(ocdfgEdgeMetricLabel("performance", "median")).toBe("median performance");
  });
});
