import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import pnmlRaw from "./fixtures/model-exchange-pnml.json?raw";
import bpmnRaw from "./fixtures/model-exchange-bpmn.json?raw";
import ptmlRaw from "./fixtures/model-exchange-ptml.json?raw";
import dfgRaw from "./fixtures/model-exchange-dfg.json?raw";
import hugeRaw from "./fixtures/model-exchange-huge.json?raw";
import failureRaw from "./fixtures/model-exchange-failure.json?raw";
import { defaultModelExchange, modelExchangeQuery, validModelExchange, modelExchangeTables,
  modelExchangeGraph, modelExchangeTreeGraph, requestModelExchange, exchangedPnml,
  type ModelExchangeResult } from "../src/model-exchange";
import { parseNativeJson, nativeJson } from "../src/native-json";
import { ocelPreparedCsv } from "../src/ocel-preparation";
import { checkModel } from "../src/api";
import { ModelExchangeWorkspace } from "../src/components/ModelExchangeWorkspace";
const parse = (raw: string) => parseNativeJson(raw) as ModelExchangeResult;
const source = new File(["<pnml/>"], "net-東京.pnml");
afterEach(() => vi.unstubAllGlobals());
describe("independent native model exchange", () => {
  it("scopes import and conversion options to their applicable source variants", () => {
    const form = defaultModelExchange(); form.startActivity = "Start, 東京";
    expect(modelExchangeQuery(form)).not.toContain("artificial_start_activity");
    form.format = "dfg";
    expect(modelExchangeQuery(form)).not.toContain("pnml_infer");
    expect(modelExchangeQuery(form)).not.toContain("artificial_start_activity");
    form.dfgVariant = "invisibles_no_duplicates";
    expect(new URLSearchParams(modelExchangeQuery(form)).get("artificial_start_activity")).toBe("Start, 東京");
    form.format = "ptml"; form.treeVariant = "transition_bordered";
    expect(new URLSearchParams(modelExchangeQuery(form)).get("tree_variant")).toBe("transition_bordered");
  });
  it("accepts all four actual native source inspector responses", () => {
    for (const [format, raw] of [["pnml", pnmlRaw], ["bpmn", bpmnRaw], ["ptml", ptmlRaw], ["dfg", dfgRaw]] as const) {
      const form = defaultModelExchange(); form.format = format;
      expect(validModelExchange(parse(raw), form)).toBe(true);
      form.output = "ptml"; expect(validModelExchange(parse(raw), form)).toBe(false);
    }
  });
  it("preserves arbitrary precision native integer tokens through tables, graphs and CSV", async () => {
    const result = parse(hugeRaw), form = defaultModelExchange(); form.output = "pnml";
    expect(validModelExchange(result, form)).toBe(true);
    expect(result.net!.initial_marking[0]).toBe(Infinity);
    const huge = "1" + "0".repeat(500), tables = modelExchangeTables(result);
    expect(tables.places.rowAt(0)[2]).toBe(huge);
    expect(tables.arcs.rowAt(0)[6]).toBe(huge);
    expect(await ocelPreparedCsv(tables.arcs).text()).toContain('"' + huge + '"');
    expect(nativeJson(result)).toContain('"weight":' + huge);
    expect(modelExchangeGraph(result).edges[0].label).toContain(huge);
  });
  it("keeps failed conversion data, native PNML and complete source inspection usable", () => {
    const form = defaultModelExchange(); form.output = "bpmn";
    const result = parse(failureRaw);
    expect(validModelExchange(result, form)).toBe(true);
    expect(result.exported).toBeNull(); expect(result.conversion_error).toContain("markings");
    expect(result.pnml).toContain("<pnml");
    expect(modelExchangeTables(result).places.count).toBe(2);
  });
  it("rejects mismatched formats, malformed arc references and fractional token counts", () => {
    const form = defaultModelExchange();
    let result = parse(pnmlRaw); result.format = "bpmn"; expect(validModelExchange(result, form)).toBe(false);
    result = parse(pnmlRaw); result.arcs[0].place = 99; expect(validModelExchange(result, form)).toBe(false);
    result = parse(pnmlRaw); result.net!.initial_marking[0] = 0.5; expect(validModelExchange(result, form)).toBe(false);
    result = parse(pnmlRaw); result.arcs[0].weight = -0.5; expect(validModelExchange(result, form)).toBe(false);
    result = parse(pnmlRaw); result.net!.transition_count = 999; expect(validModelExchange(result, form)).toBe(false);
  });
  it("retains distinct XML identities and repeated native arc occurrences", () => {
    const result = parse(pnmlRaw), tables = modelExchangeTables(result), graph = modelExchangeGraph(result);
    expect(tables.places.columns).toEqual(["place_index", "place_id", "initial_tokens", "initial_key", "final_tokens", "final_key"]);
    expect(tables.transitions.count).toBe(3); expect(tables.arcs.count).toBe(6);
    result.arcs.push({ ...result.arcs[0], id: "parallel second arc" });
    expect(modelExchangeGraph(result).edges).toHaveLength(7);
    expect(new Set(graph.nodes.map(node => node.id)).size).toBe(graph.nodes.length);
    expect(graph.nodes.find(node => node.id === "t/0")?.label).toBe("A");
  });
  it("distinguishes process-tree child positions from net transition identities", () => {
    const result = parse(ptmlRaw), tree = result.tree!;
    const tables = modelExchangeTables(result), graph = modelExchangeTreeGraph(tree);
    expect(tables.tree_nodes.count).toBe(5);
    expect(tables.tree_nodes.rowAt(1)).toEqual(["root/0", "root", 0, "activity", 0, "A", 0]);
    expect(tables.tree_nodes.rowAt(2)).toEqual(["root/1", "root", 1, "sequence", null, null, 2]);
    expect(graph.edges.map(edge => edge.label)).toEqual(["0", "1", "0", "1"]);
    tree.root.children![0].activity = 99;
    const form = defaultModelExchange(); form.format = "ptml";
    expect(validModelExchange(result, form)).toBe(false);
  });
  it("exports complete DFG source maps without requiring a net conversion", async () => {
    const result = parse(dfgRaw), tables = modelExchangeTables(result);
    expect(result.net).toBeNull(); expect(result.pnml).toBeNull();
    expect(tables.dfg_edges.rowAt(0)).toEqual(["A", "B", 3]);
    expect(tables.start_activities.rowAt(0)).toEqual(["A", 3]);
    expect((await ocelPreparedCsv(tables.dfg_edges).text()).split("\r\n")).toHaveLength(4);
    expect(modelExchangeGraph(result).nodes.map(node => node.id)).toEqual(["A", "B", "C"]);
  });
  it("uploads original model bytes and records the native response without numeric rewriting", async () => {
    const form = defaultModelExchange(); form.output = "pnml";
    const fetcher = vi.fn(async () => new Response(hugeRaw)); vi.stubGlobal("fetch", fetcher);
    const controller = new AbortController();
    const run = await requestModelExchange(source, form, controller.signal);
    const [url, init] = fetcher.mock.calls[0] as unknown as [string, RequestInit];
    expect(url).toContain("/api/model-exchange?");
    expect(init.signal).toBe(controller.signal);
    expect(await ((init.body as FormData).get("model") as File).text()).toBe(await source.text());
    expect(run.raw).toBe(hugeRaw); expect(run.report_json).toContain(hugeRaw);
    expect(run.source.name).toBe(source.name); expect(run.source.size).toBe(source.size);
    expect(run.source.sha256).toMatch(/^[a-f0-9]{64}$/);
    expect(await exchangedPnml(run).text()).toBe(run.result.pnml);
  });
  it("checks cached models with explicit legacy marking inference disabled", async () => {
    const fetcher = vi.fn(async () => new Response('{"activities":[],"model":{}}')); vi.stubGlobal("fetch", fetcher);
    await checkModel(source, { sourceId: "42", pnmlInferLegacyBoundaries: false });
    const [url] = fetcher.mock.calls[0] as unknown as [string];
    const params = new URLSearchParams(url.split("?")[1]);
    expect(params.get("source_id")).toBe("42");
    expect(params.get("pnml_infer_legacy_boundaries")).toBe("false");
  });
  it("respects cancellation before publication and exposes native parse errors", async () => {
    const fetcher = vi.fn(async () => new Response(pnmlRaw)); vi.stubGlobal("fetch", fetcher);
    const controller = new AbortController(); controller.abort();
    await expect(requestModelExchange(source, defaultModelExchange(), controller.signal)).rejects.toThrow();
    expect(fetcher).not.toHaveBeenCalled();
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"error":"dangling PNML arc"}', { status: 400 })));
    await expect(requestModelExchange(source, defaultModelExchange())).rejects.toThrow(/dangling/);
  });
  it("renders accessible model uploads and all source-free format choices", () => {
    const html = renderToStaticMarkup(createElement(ModelExchangeWorkspace));
    expect(html).toContain("Inspect and convert model files");
    expect(html).toContain('aria-label="Model document"');
    expect(html).toContain("BPMN"); expect(html).toContain("PTML"); expect(html).toContain("DFG");
    expect(html).toContain("Preserve general PNML markings");
    expect(html).toContain("no event log is required");
  });
});
