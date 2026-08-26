import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import type { MineraExport } from "../types";
import { PerformanceSpectrumView } from "./PerformanceSpectrum";

const data = (variant: "connected" | "disconnected"): MineraExport => ({
  dataset: "fixture", trace_count: 1, event_count: 2,
  activities: [
    { name: "A", count: 1, start_count: 1, end_count: 0 },
    { name: "B", count: 1, start_count: 0, end_count: 1 },
  ], edges: [], variants: [],
  performance_spectrum: {
    variant, activities: ["A", "B"],
    observations: [{ case_id: "case-1", activities: ["A", "B"], instants: [1700000000, 1700000010], pattern_offset: 0 }],
  },
});

describe("PerformanceSpectrumView server results", () => {
  it("renders server-returned observations when no client pattern is selected", () => {
    const html = renderToStaticMarkup(createElement(PerformanceSpectrumView, { data: data("connected") }));
    expect(html).toContain("Server spectrum (connected)");
    expect(html).toContain("flow time");
    expect(html).toContain("A → B");
  });

  it("labels disconnected observations as partial subpatterns", () => {
    const disconnected = data("disconnected");
    disconnected.performance_spectrum = {
      variant: "disconnected", activities: ["A", "B", "A", "B"],
      observations: [{ case_id: "case-2", activities: ["A", "B"], instants: [1700000010, 1700000020], pattern_offset: 2 }],
    };
    const html = renderToStaticMarkup(createElement(PerformanceSpectrumView, { data: disconnected }));
    expect(html).toContain("Partial lines");
    expect(html).toContain("A → B");
    expect(html).toContain("10,218 870,314");
  });

  it("uses nonzero offsets even when activity labels repeat", () => {
    const repeated = data("disconnected");
    repeated.performance_spectrum = {
      variant: "disconnected", activities: ["A", "A", "B"],
      observations: [{ case_id: "case-3", activities: ["A", "B"], instants: [1700000000, 1700000010], pattern_offset: 1 }],
    };
    const html = renderToStaticMarkup(createElement(PerformanceSpectrumView, { data: repeated }));
    expect(html).toContain("10,122 870,218");
  });
});
