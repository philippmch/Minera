import { describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { GaugeTile, precisionGauge } from "../src/components/Conformance";
import { petriAlignmentResultSummary } from "../src/petri-alignment-workbench";
import type { Alignments } from "../src/types";

const render = (precision: number | null, incomplete?: boolean) =>
  renderToStaticMarkup(createElement(GaugeTile, { metric: precisionGauge(precision, incomplete, "hint") }));

describe("precision the backend could not compute", () => {
  it("renders as unavailable, never as a graded 0.0%", () => {
    const html = render(null, true);
    expect(html).toContain("conf-gauge unavailable");
    expect(html).toContain("—");
    expect(html).toContain("Not computed");
    expect(html).not.toContain("0.0%");
    expect(html).not.toContain("poor");
    expect(render(null)).toContain("unavailable");
  });

  it("names the causes that apply to each precision source", () => {
    const alignment = precisionGauge(null, true, "hint").hint;
    const token = precisionGauge(null, true, "hint", "token_replay").hint;
    expect(alignment).toContain("silent transitions");
    expect(alignment).not.toContain("backwards");
    expect(token).toContain("silent transitions");
    expect(token).toContain("backwards token replay");
  });

  it("keeps genuine numbers, including a real zero", () => {
    expect(render(0)).toContain("conf-gauge poor");
    expect(render(0)).toContain("0.0%");
    expect(render(0.97)).toContain("conf-gauge good");
    expect(render(0.97)).toContain("97.0%");
  });

  it("summarizes alignment results without a number", () => {
    const base = { computed: true, exhausted: false, fitness: 1, fitting_traces: 1, trace_count: 1, total_cost: 0 } as unknown as Alignments;
    expect(petriAlignmentResultSummary({ ...base, precision: null, precision_incomplete: true } as Alignments)).toContain("Precision not computed");
    expect(petriAlignmentResultSummary(base)).toContain("Precision absent");
  });
});
