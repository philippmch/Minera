import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";

import type {
  CorrelationMinerAnalysis,
  MineraExport,
} from "../../src/types";
import { CorrelationMining } from "../../src/components/CorrelationMining";

const baseData = (): MineraExport => ({
  dataset: "fixture",
  trace_count: 1,
  event_count: 2,
  activities: [
    { name: "A", count: 1, start_count: 1, end_count: 0 },
    { name: "B", count: 1, start_count: 0, end_count: 1 },
  ],
  edges: [],
  variants: [],
});

const analysis = (
  variant: CorrelationMinerAnalysis["variant"],
): CorrelationMinerAnalysis => ({
  variant,
  activity_counts: [
    { activity: 0, count: 2 },
    { activity: 1, count: 2 },
  ],
  frequency_dfg: [
    { source: 0, target: 0, count: 1 },
    { source: 1, target: 0, count: 1 },
  ],
  performance_dfg: [
    { source: 0, target: 0, seconds: 0 },
    { source: 1, target: 0, seconds: 5 },
  ],
  precede_succeed_probability: [
    { source: 0, target: 0, value: 0 },
    { source: 0, target: 1, value: 0.25 },
    { source: 1, target: 0, value: 0.75 },
    { source: 1, target: 1, value: 0 },
  ],
  duration_matrix: [
    { source: 0, target: 0, value: 0 },
    { source: 0, target: 1, value: 3 },
    { source: 1, target: 0, value: 5 },
    { source: 1, target: 1, value: 0 },
  ],
});

describe("CorrelationMining", () => {
  it("renders nothing for absent or empty analyses", () => {
    expect(
      renderToStaticMarkup(
        createElement(CorrelationMining, { data: baseData() }),
      ),
    ).toBe("");
    const data = baseData();
    data.correlation_mining = { variants: [] };
    expect(
      renderToStaticMarkup(createElement(CorrelationMining, { data })),
    ).toBe("");
  });

  it("renders multiple variants, forced edges, and diagnostics", () => {
    const data = baseData();
    data.correlation_mining = {
      variants: [analysis("classic"), analysis("trace_based")],
    };
    const html = renderToStaticMarkup(
      createElement(CorrelationMining, { data }),
    );
    expect(html).toContain("2 variants");
    expect(html).toContain("classic");
    expect(html).toContain("trace based");
    expect(html).toContain("Probability and duration diagnostics");
    expect(html).toContain("0s");
    expect(html).toContain("5s");
    expect(html).toContain("0.2500");
    expect(html).toContain("0.7500");
  });

  it("does not disguise missing values as genuine zero durations", () => {
    const malformed = analysis("classic_split");
    malformed.duration_matrix = malformed.duration_matrix.filter(
      (entry) => !(entry.source === 0 && entry.target === 1),
    );
    malformed.performance_dfg = malformed.performance_dfg.filter(
      (entry) => !(entry.source === 0 && entry.target === 0),
    );
    const data = baseData();
    data.correlation_mining = { variants: [malformed] };
    const html = renderToStaticMarkup(
      createElement(CorrelationMining, { data }),
    );
    expect(html.match(/—/g)).toHaveLength(2);
  });
});
