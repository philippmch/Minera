import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import dfgRaw from "./fixtures/streaming-dfg.json?raw";
import footprintsRaw from "./fixtures/streaming-footprints.json?raw";
import temporalRaw from "./fixtures/streaming-temporal.json?raw";
import tbrRaw from "./fixtures/streaming-tbr.json?raw";
import {
  defaultStreaming, invisibleBound, streamingQuery, validStreamingResult,
  streamingTables, streamingGraph, requestStreamingWorkflow, type StreamingResult,
} from "../src/streaming-workspace";
import { parseNativeJson } from "../src/native-json";
import { ocelPreparedCsv } from "../src/ocel-preparation";
import { StreamingWorkspace } from "../src/components/StreamingWorkspace";

const log = new File(["case_id,activity\n東京,A\n"], "events-東京.csv");
const model = new File(["<pnml/>"], "model-東京.pnml");
const mock = (raw: string, status = 200) => vi.fn(async () => new Response(raw, { status }));
const parse = (raw: string) => parseNativeJson(raw) as StreamingResult;
afterEach(() => vi.unstubAllGlobals());

describe("native streaming workflow delivery", () => {
  it("scopes parameters and preserves literal column names and exact limits", () => {
    const form = defaultStreaming(); form.method = "tbr"; form.caseColumn = " case,東京 ";
    form.maxInvisible = "18446744073709551615"; form.profile = "invalid unused JSON";
    const query = new URLSearchParams(streamingQuery(form));
    expect(query.get("streaming_tbr_max_invisible")).toBe("18446744073709551615");
    expect(query.get("streaming_tbr_case_id_column")).toBe(" case,東京 ");
    expect(query.get("streaming_event_order")).toBe("input");
    expect(query.has("streaming_temporal_profile")).toBe(false);
    expect(invisibleBound("0")).toBe("0");
    for (const value of ["-1", "1.5", "18446744073709551616", " 1", "1e3"])
      expect(() => invisibleBound(value)).toThrow(/unsigned/);
  });
  it("expresses omitted footprint constraints separately from required activities", () => {
    const form = defaultStreaming(); form.method = "footprints";
    form.starts = ""; form.ends = ""; form.sequence = "[]"; form.parallel = "[]";
    const query = new URLSearchParams(streamingQuery(form));
    expect(query.get("streaming_footprints_activities")).toBe("A,B,C");
    expect(query.has("streaming_footprints_start_activities")).toBe(false);
    expect(query.has("streaming_footprints_end_activities")).toBe(false);
    expect(query.has("streaming_footprints_sequence")).toBe(false);
    form.starts = "[]"; expect(() => streamingQuery(form)).toThrow(/non-empty/);
    form.starts = ""; form.activities = '["A,東京"]'; expect(() => streamingQuery(form)).toThrow(/activity names/);
  });
  it("validates temporal profiles and finite positive zeta before a request", () => {
    const form = defaultStreaming(); form.method = "temporal";
    form.profile = '[["A","B",5,0]]'; form.zeta = "2";
    expect(new URLSearchParams(streamingQuery(form)).get("streaming_temporal_profile")).toBe("A->B:5:0");
    form.zeta = "0"; expect(() => streamingQuery(form)).toThrow(/greater than zero/);
    form.zeta = ""; form.profile = '[["A","B",-1,1]]'; expect(() => streamingQuery(form)).toThrow(/mean/);
    form.profile = "bad"; expect(() => streamingQuery(form)).toThrow(/JSON array/);
  });
  it("accepts all four actual native fixtures and verifies order and variant", () => {
    for (const [method, raw] of [["dfg", dfgRaw], ["footprints", footprintsRaw], ["temporal", temporalRaw], ["tbr", tbrRaw]] as const) {
      const form = defaultStreaming(); form.method = method;
      form.finalize = method === "footprints" || method === "tbr";
      if (method === "tbr") form.variant = "backwards";
      expect(validStreamingResult(parse(raw), form)).toBe(true);
      form.order = "imported"; expect(validStreamingResult(parse(raw), form)).toBe(false);
    }
  });
  it("keeps numeric case identities independent of JavaScript property enumeration", () => {
    const form = defaultStreaming(); form.method = "tbr"; form.variant = "backwards"; form.finalize = true;
    const value = parse(tbrRaw);
    expect(Object.keys((value.prefix as { cases: object }).cases)).toEqual(["1", "2", "10"]);
    expect(value.terminated_cases?.map(row => row.case_id)).toEqual(["1", "10", "2"]);
    expect(validStreamingResult(value, form)).toBe(true);
    value.terminated_cases![2].case_id = "10"; expect(validStreamingResult(value, form)).toBe(false);
  });
  it("rejects missing prefix diagnostics, wrong variants and malformed marking tokens", () => {
    const form = defaultStreaming(); form.method = "tbr"; form.variant = "backwards"; form.finalize = true;
    let value = parse(tbrRaw); delete value.prefix; expect(validStreamingResult(value, form)).toBe(false);
    value = parse(tbrRaw); value.terminated_cases![0].marking = [-1]; expect(validStreamingResult(value, form)).toBe(false);
    value = parse(tbrRaw); form.variant = "forward"; expect(validStreamingResult(value, form)).toBe(false);
    form.method = "dfg"; form.finalize = false; expect(validStreamingResult({ ...parse(dfgRaw), prefix: {} }, form)).toBe(false);
  });
  it("exports every native termination, prefix and deviation row with exact numeric tokens", async () => {
    const footprint = streamingTables(parse(footprintsRaw));
    expect(footprint.open_cases.count).toBe(0); expect(footprint.prefix_cases.count).toBe(3);
    expect(footprint.terminated_cases.rowAt(2)).toEqual(["2", false]);
    const token = streamingTables(parse(tbrRaw));
    expect(token.terminated_cases.columns).toEqual(["case_id", "missing", "remaining", "is_fit", "marking"]);
    const csv = await ocelPreparedCsv(token.terminated_cases).text();
    expect(csv.split("\r\n")).toHaveLength(5);
    const temporal = parse(temporalRaw); const tables = streamingTables(temporal);
    expect(tables.deviations.count).toBe(1);
    expect(tables.deviations.rowAt(0)).toEqual(["10", "B", "C", 13, 8]);
    expect(streamingTables(parse(dfgRaw)).edges.count).toBe(4);
  });
  it("draws all native DFG edges and activity boundaries without changing identities", () => {
    const graph = streamingGraph(parse(dfgRaw))!;
    expect(graph.nodes.map(node => node.id)).toEqual(["A", "B", "C"]);
    expect(graph.nodes.find(node => node.id === "A")).toMatchObject({ start: true, end: true });
    expect(graph.edges).toHaveLength(4); expect(graph.edges.every(edge => edge.directed)).toBe(true);
    expect(streamingGraph(parse(tbrRaw))).toBeUndefined();
  });
  it("uploads original files and binds request hashes to a complete raw native response", async () => {
    const fetcher = mock(tbrRaw); vi.stubGlobal("fetch", fetcher);
    const form = defaultStreaming(); form.method = "tbr"; form.variant = "backwards"; form.finalize = true;
    const controller = new AbortController();
    const run = await requestStreamingWorkflow(form, log, model, controller.signal);
    const [url, options] = fetcher.mock.calls[0] as unknown as [string, RequestInit];
    expect(url).toContain("/api/streaming-tbr?"); expect(options.signal).toBe(controller.signal);
    expect(await ((options.body as FormData).get("log") as File).text()).toBe(await log.text());
    expect(await ((options.body as FormData).get("model") as File).text()).toBe(await model.text());
    expect(run.sources.map(source => source.role)).toEqual(["log", "model"]);
    expect(run.sources.every(source => /^[a-f0-9]{64}$/.test(source.sha256))).toBe(true);
    expect(run.raw).toBe(tbrRaw); expect(run.report_json).toContain(tbrRaw);
    expect(JSON.parse(run.report_json).request.parameters.streaming_tbr_terminate_all).toBe("true");
  });
  it("prevents requests without needed files and respects pre-request cancellation", async () => {
    const fetcher = mock(dfgRaw); vi.stubGlobal("fetch", fetcher);
    const form = defaultStreaming(); form.method = "tbr";
    await expect(requestStreamingWorkflow(form, log)).rejects.toThrow(/PNML/);
    form.method = "dfg";
    await expect(requestStreamingWorkflow(form, new File([""], "source.xes"))).rejects.toThrow(/CSV/);
    const controller = new AbortController(); controller.abort();
    await expect(requestStreamingWorkflow(form, log, null, controller.signal)).rejects.toThrow();
    expect(fetcher).not.toHaveBeenCalled();
  });
  it("surfaces native errors and rejects success bodies for another requested method", async () => {
    const form = defaultStreaming();
    vi.stubGlobal("fetch", mock('{"error":"CSV row 2 has an empty case id"}', 400));
    await expect(requestStreamingWorkflow(form, log)).rejects.toThrow(/empty case id/);
    vi.stubGlobal("fetch", mock(temporalRaw));
    await expect(requestStreamingWorkflow(form, log)).rejects.toThrow(/mismatched/);
  });
  it("renders an accessible source-free entry and the current original CSV handoff", () => {
    const html = renderToStaticMarkup(createElement(StreamingWorkspace, { file: log }));
    expect(html).toContain("Replay event streams"); expect(html).toContain("CSV input order");
    expect(html).toContain('aria-label="Streaming event CSV"');
    expect(html).toContain("Use current original CSV for streaming");
    expect(html).toContain("fresh native streaming monitor");
  });
});
