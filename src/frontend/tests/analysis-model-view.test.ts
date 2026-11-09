import { describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import sampleRaw from "../public/sample-data.json?raw";
import { AnalysisModelView, minerVariantOf, workflowVariantOf } from "../src/components/ProcessModel";
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

  it("shows the workflow net, with the toggle, when the export lacks the tree or POWL model (review AA1-4)", () => {
    const withoutTree = { ...data, process_tree: undefined } as MineraExport;
    const html = renderToStaticMarkup(createElement(AnalysisModelView, { minerId: "inductive", title: "Inductive · analysis", data: withoutTree, layouts: modelLayouts(withoutTree) }));
    expect(html).toContain('aria-label="Model view"');
    expect(html).toMatch(/aria-pressed="false" disabled=""[^>]*>Tree<\/button>/);
    expect(html).toMatch(/aria-pressed="true"[^>]*>Workflow net<\/button>/);
    expect(html).toContain('aria-label="Net annotation"');
    expect(html).not.toContain("No process tree is available");
  });

  it("maps miner cards to workflow variants and back (review AA1-4)", () => {
    expect(workflowVariantOf("inductive", "basic")).toBe("log");
    expect(workflowVariantOf("powl", "configured")).toBe("maximal");
    expect(workflowVariantOf("alpha", "plus")).toBe("plus");
    expect(minerVariantOf("inductive", "log")).toBe("basic");
    expect(minerVariantOf("powl", "maximal")).toBe("configured");
    expect(minerVariantOf("heuristic", "plusplus")).toBe("plusplus");
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
    // Without a CSV or XES source the Discover button is disabled, and the note says what enables it (review AA1-4).
    expect(html).toContain("open a CSV or XES source to mine IMf · infrequent");
    expect(html).not.toContain("press “Discover selected workflow”");
    expect(html).not.toContain("Saved Inductive model");
  });
});
