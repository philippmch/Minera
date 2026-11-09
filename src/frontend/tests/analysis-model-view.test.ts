import { describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import sampleRaw from "../public/sample-data.json?raw";
import { AnalysisModelView } from "../src/components/ProcessModel";
import { WorkflowDiscovery, workflowAutoStarts } from "../src/components/WorkflowDiscovery";
import { modelLayouts } from "../src/model-layouts";
import type { MineraExport } from "../src/types";

const data = JSON.parse(sampleRaw) as MineraExport;
const view = (minerId: "alpha" | "inductive" | "heuristic" | "powl") =>
  renderToStaticMarkup(createElement(AnalysisModelView, { minerId, title: "Miner · mined with this analysis (analysis settings)", data, layouts: modelLayouts(data) }));

describe("the analysis model view", () => {
  it("offers the replay decoration modes on the analysis workflow net", () => {
    expect(data.petri_net_decoration?.decorated_traces).toBeGreaterThan(0);
    const html = view("alpha");
    expect(html).toContain('aria-label="Net annotation"');
    for (const mode of ["Plain", "Frequency"]) expect(html).toContain(`>${mode}</button>`);
    expect(html).toMatch(/aria-pressed="true"[^>]*>Plain|>Plain/);
    expect(html).toContain("mined with this analysis");
  });

  it("flips a tree to its workflow net and a dependency graph to its net", () => {
    const tree = view("inductive");
    expect(tree).toContain('aria-label="Model view"');
    expect(tree).toMatch(/aria-pressed="true"[^>]*>Tree<\/button>/);
    expect(tree).toMatch(/aria-pressed="false"[^>]*>Workflow net<\/button>/);
  });
});

describe("variant discovery from the variant card", () => {
  it("starts once the source is known and nothing else runs", () => {
    const ready = { armed: true, schema: true, run: false, pending: false, mining: false, available: true };
    expect(workflowAutoStarts(ready)).toBe(true);
    for (const change of [{ armed: false }, { schema: false }, { run: true }, { pending: true }, { mining: true }, { available: false }])
      expect(workflowAutoStarts({ ...ready, ...change })).toBe(false);
  });

  it("introduces the analysis model as such instead of under the selected variant", () => {
    const html = renderToStaticMarkup(createElement(WorkflowDiscovery, { family: "inductive", variant: "imf", data, available: false, mining: false, savedView: createElement("div", { id: "analysis-model" }) }));
    expect(html).toContain('id="analysis-model"');
    expect(html).toContain("not a IMf · infrequent discovery");
    expect(html).not.toContain("Saved Inductive model");
  });
});
