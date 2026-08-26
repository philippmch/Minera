import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import type { MineraExport } from "../../src/types";
import { Batches } from "../../src/components/Batches";

const baseData = (): MineraExport => ({
  dataset: "fixture", trace_count: 1, event_count: 5,
  activities: [{ name: "A", count: 5, start_count: 5, end_count: 5 }],
  edges: [], variants: [],
});

describe("Batches", () => {
  it("renders empty states for absent and empty analyses", () => {
    expect(renderToStaticMarkup(createElement(Batches, { data: baseData() }))).toContain("No batch analysis");
    const data = baseData();
    data.batch_discovery = { groups: [] };
    expect(renderToStaticMarkup(createElement(Batches, { data }))).toContain("No batches found");
  });

  it("renders every batch category, evidence, counts, and missing resource", () => {
    const data = baseData();
    data.batch_discovery = {
      groups: [{
        activity: 0, resource: "", batch_count: 5, event_count: 5,
        batches: (["simultaneous", "batching_on_start", "batching_on_end", "sequential", "concurrent"] as const).map((type, index) => ({
          type, start: 1700000000 + index, complete: 1700000001 + index,
          events: [{ start: 1700000000, complete: 1700000001, case_id: `c${index}`, trace_index: index, event_index: 0 }],
        })),
      }],
    };
    const html = renderToStaticMarkup(createElement(Batches, { data }));
    for (const label of ["Simultaneous", "Batching on start", "Batching on end", "Sequential", "Concurrent"]) expect(html).toContain(label);
    expect(html).toContain("(missing resource)");
    expect(html).toContain("case c0");
    expect(html).toContain("5 batches");
    expect(html).toContain("5 events");
    expect(html).toContain("2023");
  });
});
