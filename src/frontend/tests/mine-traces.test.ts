import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { inlineMiningJson, inlineMiningRequest, inlineMiningValue, loadTraces, loadedTraces, markLiveTraces, retainTraces, traceFacts, traceLoad, useHasTimestamps } from "../src/mine-traces";
import { activeMiningSourceRequest, classifyMiningJson, type ActiveDatasetResult, type ActiveMiningSourceDescriptor } from "../src/active-mining-source";
import { readNativeResponse } from "../src/native-response";
import { nativeJson } from "../src/native-json";
import { initialTab, logLifecycles } from "../src/app-logic";
import { isMineraExport, type MineraExport, type Trace } from "../src/types";
import { mineActiveDataset } from "../src/api";
import { EventLogView } from "../src/components/EventLogView";
import { DottedChartView } from "../src/components/DottedChart";
import { PerformanceSpectrumView } from "../src/components/PerformanceSpectrum";
import { EmbeddingSearch, embeddingCaseSequence } from "../src/components/EmbeddingSearch";
import { ReductionTraceField } from "../src/components/Conformance";
import { AnonymizationWorkspace } from "../src/components/AnonymizationWorkspace";

const TRACES: Trace[] = [
  { case_id: "case-1", events: [
    { activity: 0, timestamp: "2024-01-01T08:00:00Z", lifecycle: "start" },
    { activity: 1, timestamp: "2024-01-01T09:00:00Z", lifecycle: "complete" },
  ] },
  { case_id: "case-2", events: [{ activity: 0, timestamp: "2024-01-02T08:00:00Z", lifecycle: "complete" }] },
];

const base = {
  dataset: "orders.csv", trace_count: 2, event_count: 3,
  activities: [
    { name: "Register", count: 2, start_count: 2, end_count: 1 },
    { name: "Ship", count: 1, start_count: 0, end_count: 1 },
  ],
  edges: [{ source: 0, target: 1, count: 1 }], variants: [],
};

const RUN = "0123456789abcdef";

/** A mining response requested with include_traces=false, as received from the backend. Each call is a new mine. */
function lazy(sourceId = "7", overrides: Partial<MineraExport["trace_summary"]> = {}): MineraExport {
  const data = { ...base, source_id: sourceId, trace_summary: {
    traces: "on_demand", trace_count: 2, event_count: 3, max_trace_length: 2,
    timestamped_trace_count: 2, has_timestamps: true, has_lifecycles: true, lifecycles: ["start", "complete"], run_id: RUN, ...overrides,
  } } as MineraExport;
  markLiveTraces(data);
  return data;
}

const inline = (): MineraExport => ({ ...base, traces: TRACES } as MineraExport);

const tracesBody = (sourceId: string, traces: unknown = TRACES, extra: object = {}) =>
  JSON.stringify({ family: "ANALYSIS_TRACES", source_id: sourceId, run_id: RUN, trace_count: 2, event_count: 3, traces, ...extra });

function respond(status: number, body: string) {
  return new Response(body, { status, headers: { "Content-Type": "application/json" } });
}

/** A fetch whose responses the test releases by hand. */
function pendingFetch() {
  const calls: Array<{ url: string; signal?: AbortSignal; resolve: (response: Response) => void }> = [];
  const fetch = vi.fn((url: string, init?: RequestInit) => new Promise<Response>((resolve, reject) => {
    const signal = init?.signal ?? undefined;
    signal?.addEventListener("abort", () => reject(new DOMException("aborted", "AbortError")), { once: true });
    calls.push({ url, signal, resolve });
  }));
  vi.stubGlobal("fetch", fetch);
  return { fetch, calls };
}

const render = (element: Parameters<typeof renderToStaticMarkup>[0]) => renderToStaticMarkup(element);

beforeEach(() => retainTraces(null));
afterEach(() => { retainTraces(null); vi.unstubAllGlobals(); });

describe("the summary-only mining response", () => {
  it("validates with a trace summary instead of traces", () => {
    expect(isMineraExport(lazy())).toBe(true);
    expect(isMineraExport(inline())).toBe(true);
  });
  it("rejects a summary that disagrees with the analysis or stands next to traces", () => {
    expect(isMineraExport(lazy("7", { trace_count: 3 }))).toBe(false);
    expect(isMineraExport(lazy("7", { event_count: 2 }))).toBe(false);
    expect(isMineraExport(lazy("7", { has_timestamps: false }))).toBe(false);
    expect(isMineraExport(lazy("7", { has_lifecycles: false }))).toBe(false);
    expect(isMineraExport(lazy("7", { lifecycles: ["start", "start"] }))).toBe(false);
    expect(isMineraExport(lazy("7", { max_trace_length: 4 }))).toBe(false);
    expect(isMineraExport(lazy("7", { traces: "inline" as never }))).toBe(false);
    expect(isMineraExport({ ...lazy(), traces: TRACES })).toBe(false);
  });
  it("decides the log views from the summary, and from inline traces as before", () => {
    for (const data of [lazy(), inline()]) {
      expect(traceFacts(data)).toEqual({ hasCases: true, hasTimestamps: true, lifecycles: ["start", "complete"], maxTraceLength: 2 });
      expect(initialTab(data)).toBe("log");
      expect(logLifecycles(data)).toEqual(["start", "complete"]);
    }
    const untimed = lazy("7", { timestamped_trace_count: 0, has_timestamps: false, has_lifecycles: false, lifecycles: [] });
    expect(traceFacts(untimed)).toMatchObject({ hasCases: true, hasTimestamps: false, lifecycles: [] });
    const empty = { ...base, trace_count: 0, event_count: 0, source_id: "8", trace_summary: { traces: "on_demand", trace_count: 0, event_count: 0, max_trace_length: 0, timestamped_trace_count: 0, has_timestamps: false, has_lifecycles: false, lifecycles: [] } } as MineraExport;
    expect(initialTab(empty)).toBe("dfg");
    // An old export without traces or summary offers no log view.
    const old = { ...base } as MineraExport;
    expect(traceFacts(old)).toEqual({ hasCases: false, hasTimestamps: false, lifecycles: [], maxTraceLength: undefined });
    expect(initialTab(old)).toBe("dfg");
    expect(traceLoad(old).status).toBe("unavailable");
  });
});

describe("loading the traces on demand", () => {
  it("reads inline traces in place without a request", async () => {
    const { fetch } = pendingFetch();
    const data = inline();
    await loadTraces(data);
    expect(fetch).not.toHaveBeenCalled();
    expect(loadedTraces(traceLoad(data))).toBe(TRACES);
  });
  it("fetches once per source, shared by every reader", async () => {
    const fetch = vi.fn(async () => respond(200, tracesBody("7")));
    vi.stubGlobal("fetch", fetch);
    const data = lazy("7");
    expect(traceLoad(data).status).toBe("idle");
    const first = loadTraces(data);
    expect(traceLoad(data).status).toBe("loading");
    await Promise.all([first, loadTraces(data), loadTraces({ ...data })]);
    expect(fetch).toHaveBeenCalledTimes(1);
    expect(String((fetch.mock.calls[0] as unknown[])[0])).toBe(`/api/traces?source_id=7&run_id=${RUN}`);
    expect(loadedTraces(traceLoad(data))).toEqual(TRACES);
    await loadTraces(data);
    expect(fetch).toHaveBeenCalledTimes(1);
  });
  it("aborts the load of a source that was replaced, and drops it", async () => {
    const { calls } = pendingFetch();
    const first = lazy("7"), second = lazy("8");
    void loadTraces(first);
    void loadTraces(second);
    expect(calls).toHaveLength(2);
    expect(calls[0].signal?.aborted).toBe(true);
    expect(calls[1].signal?.aborted).toBe(false);
    expect(traceLoad(first).status).toBe("idle");
    calls[1].resolve(respond(200, tracesBody("8")));
    await vi.waitFor(() => expect(traceLoad(second).status).toBe("ready"));
    retainTraces(null);
    expect(traceLoad(second).status).toBe("idle");
  });
  it("aborts an unfinished load when the app leaves its analysis", () => {
    const { calls } = pendingFetch();
    const data = lazy("7");
    void loadTraces(data);
    retainTraces(data);
    expect(calls[0].signal?.aborted).toBe(false);
    retainTraces(lazy("9"));
    expect(calls[0].signal?.aborted).toBe(true);
  });
  it("reports a stale source (409) and loads again on retry", async () => {
    const fetch = vi.fn(async () => respond(409, JSON.stringify({ error: "the source log changed; reload its traces" })));
    vi.stubGlobal("fetch", fetch);
    const data = lazy("7");
    await loadTraces(data);
    const failed = traceLoad(data);
    expect(failed.status).toBe("error");
    expect(failed.status === "error" && failed.error).toMatch(/no longer holds this log/);
    fetch.mockImplementation(async () => respond(200, tracesBody("7")));
    await loadTraces(data);
    expect(fetch).toHaveBeenCalledTimes(2);
    expect(traceLoad(data).status).toBe("ready");
  });
  it("rejects traces that do not belong to the analysis", async () => {
    const cases: Array<[number, string]> = [
      [500, JSON.stringify({ error: "boom" })],
      [200, tracesBody("8")],
      [200, tracesBody("7", TRACES.slice(0, 1))],
      [200, tracesBody("7", [{ case_id: "x", events: [{ activity: 5, timestamp: "" }] }, TRACES[1]])],
      [200, tracesBody("7", TRACES, { family: "OTHER" })],
      [200, tracesBody("7", TRACES, { run_id: "fedcba9876543210" })],
      [200, "not json"],
    ];
    for (const [status, body] of cases) {
      retainTraces(null);
      vi.stubGlobal("fetch", vi.fn(async () => respond(status, body)));
      const data = lazy("7");
      await loadTraces(data);
      expect(traceLoad(data).status, body).toBe("error");
    }
  });
});

describe("mining without inline traces", () => {
  it("sends include_traces=false by default and keeps them on request", async () => {
    const sent: FormData[] = [];
    vi.stubGlobal("fetch", vi.fn(async (_url: string, init?: RequestInit) => {
      sent.push(init?.body as FormData);
      return respond(200, JSON.stringify(lazy("7")));
    }));
    const file = new File(["case_id,activity\nc,A\n"], "log.csv", { type: "text/csv" });
    const result = await mineActiveDataset(file);
    expect(sent[0].get("include_traces")).toBe("false");
    expect(result).toMatchObject({ trace_summary: { traces: "on_demand" } });
    // The response the app received may load its cases from the backend.
    expect(traceLoad(result as MineraExport).status).toBe("idle");
    await mineActiveDataset(file, { includeTraces: true });
    expect(sent[1].has("include_traces")).toBe(false);
  });
});

/** Puts the store in the loaded, error or loading state for `data`. */
async function settle(data: MineraExport, state: "ready" | "error" | "loading") {
  if (state === "loading") { pendingFetch(); void loadTraces(data); return; }
  vi.stubGlobal("fetch", vi.fn(async () => state === "ready" ? respond(200, tracesBody(data.source_id!)) : respond(409, "{}")));
  await loadTraces(data);
}

describe("the readers of the cases", () => {
  it("EventLogView: loading, loaded and error", async () => {
    let data = lazy("7");
    let html = render(createElement(EventLogView, { data }));
    expect(html).toContain("Loading 2 cases");
    expect(html).not.toContain("No cases match");
    await settle(data = lazy("7"), "ready");
    html = render(createElement(EventLogView, { data }));
    expect(html).toContain("case-1");
    expect(html).toContain("case-2");
    expect(html).not.toContain("Loading");
    retainTraces(null);
    await settle(data = lazy("7"), "error");
    html = render(createElement(EventLogView, { data }));
    expect(html).toContain('role="alert"');
    expect(html).toContain("no longer holds this log");
    expect(html).toContain("Try again");
  });
  it("EventLogView keeps showing inline traces", () => {
    const html = render(createElement(EventLogView, { data: inline() }));
    expect(html).toContain("case-1");
    expect(html).not.toContain("Loading");
  });
  it("DottedChartView: loading, loaded and error", async () => {
    let data = lazy("7");
    expect(render(createElement(DottedChartView, { data }))).toContain("Loading the cases");
    await settle(data = lazy("7"), "ready");
    const html = render(createElement(DottedChartView, { data }));
    expect(html).toContain("2 cases");
    expect(html).toContain("3 events");
    retainTraces(null);
    await settle(data = lazy("7"), "error");
    expect(render(createElement(DottedChartView, { data }))).toContain("no longer holds this log");
  });
  it("PerformanceSpectrumView: needs no cases for the server spectrum, loads them for a pattern", async () => {
    let data = lazy("7");
    const idle = render(createElement(PerformanceSpectrumView, { data }));
    expect(idle).toContain("Pick two or more activities");
    expect(traceLoad(data).status).toBe("idle");
    expect(render(createElement(PerformanceSpectrumView, { data, initialPattern: [0, 1] }))).toContain("Loading the cases");
    await settle(data = lazy("7"), "ready");
    const loaded = render(createElement(PerformanceSpectrumView, { data, initialPattern: [0, 1] }));
    expect(loaded).toContain("1 occurrence");
    expect(loaded).toContain("polyline");
    retainTraces(null);
    await settle(data = lazy("7"), "error");
    expect(render(createElement(PerformanceSpectrumView, { data, initialPattern: [0, 1] }))).toContain("no longer holds this log");
  });
  it("EmbeddingSearch: case sequences while loading, once loaded and on error", async () => {
    const data = lazy("7");
    // Rendering the search does not load the cases.
    render(createElement(EmbeddingSearch, { data, available: true, mining: false }));
    expect(traceLoad(data).status).toBe("idle");
    expect(embeddingCaseSequence(data, { status: "loading" }, undefined)).toBe("Loading the sequence…");
    expect(embeddingCaseSequence(data, { status: "ready", traces: TRACES }, TRACES[0])).toBe("Register → Ship");
    expect(embeddingCaseSequence(data, { status: "error", error: "x" }, undefined)).toMatch(/could not be loaded/);
    expect(embeddingCaseSequence(data, { status: "unavailable" }, undefined)).toMatch(/not included/);
  });
  it("Conformance reduction: lists the cases once loaded, a trace number before, and the error", async () => {
    let data = lazy("7");
    const props = (data: MineraExport) => ({ data, traceIndex: 0, onChange: () => undefined });
    let html = render(createElement(ReductionTraceField, props(data)));
    expect(html).toContain("#1</option>");
    expect(html).not.toContain("case-1");
    expect(traceLoad(data).status).toBe("idle");
    await settle(data, "loading");
    expect(render(createElement(ReductionTraceField, props(data)))).toContain("Loading 2 cases");
    retainTraces(null);
    await settle(data = lazy("7"), "ready");
    html = render(createElement(ReductionTraceField, props(data)));
    expect(html).toContain("#1 · case-1");
    expect(html).toContain("#2 · case-2");
    retainTraces(null);
    await settle(data = lazy("7"), "error");
    expect(render(createElement(ReductionTraceField, props(data)))).toContain("no longer holds this log");
    expect(render(createElement(ReductionTraceField, props(inline())))).toContain("#2 · case-2");
  });
  it("AnonymizationWorkspace seeds the variant length from the summary without loading the cases", () => {
    const data = lazy("7", { max_trace_length: 2 });
    const html = render(createElement(AnonymizationWorkspace, { data, available: false, mining: false, onOpenReleased: () => undefined }));
    expect(html).toMatch(/value="2"/);
    expect(traceLoad(data).status).toBe("idle");
  });
});

describe("opened files never load cases from the backend", () => {
  const saved = () => (classifyMiningJson(JSON.stringify(lazy("7"))) as { data: MineraExport }).data;
  it("an opened summary-only file is unavailable and sends no request", async () => {
    const fetch = vi.fn(async () => respond(200, tracesBody("7")));
    vi.stubGlobal("fetch", fetch);
    const data = saved();
    expect(traceLoad(data).status).toBe("unavailable");
    await loadTraces(data);
    expect(fetch).not.toHaveBeenCalled();
    // The views it describes say that its cases were not saved.
    expect(traceFacts(data)).toMatchObject({ hasCases: true, hasTimestamps: true });
    expect(render(createElement(EventLogView, { data }))).toContain("saved without its cases");
    expect(render(createElement(DottedChartView, { data }))).toContain("saved without its cases");
    await expect(inlineMiningValue(data)).rejects.toThrow(/saved without its cases/);
    expect(fetch).not.toHaveBeenCalled();
  });
  it("a summary without the server run cannot load", async () => {
    const fetch = vi.fn(async () => respond(200, tracesBody("7")));
    vi.stubGlobal("fetch", fetch);
    const data = lazy("7", { run_id: undefined });
    expect(traceLoad(data).status).toBe("unavailable");
    await loadTraces(data);
    expect(fetch).not.toHaveBeenCalled();
  });
  it("an opened file with inline traces reads them in place", () => {
    const data = (classifyMiningJson(JSON.stringify(inline())) as { data: MineraExport }).data;
    expect(loadedTraces(traceLoad(data))).toEqual(TRACES);
  });
});

describe("exports carry the cases", () => {
  // The backend's layout: one top-level key per line, the summary on one line.
  const summaryLine = `  "trace_summary": {"traces": "on_demand", "trace_count": 2, "event_count": 3, "max_trace_length": 2, "timestamped_trace_count": 2, "has_timestamps": true, "has_lifecycles": true, "lifecycles": ["start", "complete"], "run_id": "${RUN}"},\n`;
  const tracesText = '[\n    {"case_id": "case-1", "events": [{"activity": 0, "timestamp": "2024-01-01T08:00:00Z", "lifecycle": "start"}, {"activity": 1, "timestamp": "2024-01-01T09:00:00Z", "lifecycle": "complete"}]},\n    {"case_id": "case-2", "events": [{"activity": 0, "timestamp": "2024-01-02T08:00:00Z", "lifecycle": "complete"}]}\n  ]';
  const head = '{\n  "dataset": "orders.csv",\n  "trace_count": 2,\n  "event_count": 3,\n';
  const tail = '  "activities": [{"name": "Register", "count": 2, "start_count": 2, "end_count": 1}, {"name": "Ship", "count": 1, "start_count": 0, "end_count": 1}],\n  "edges": [{"source": 0, "target": 1, "count": 1}],\n  "variants": []\n,"source_id":"7"}';
  const lazyRaw = head + summaryLine + tail;
  const fullRaw = head + `  "traces": ${tracesText},\n` + tail;
  const serve = () => {
    const fetch = vi.fn(async () => respond(200, `{"family":"ANALYSIS_TRACES","source_id":"7","run_id":"${RUN}","trace_count":2,"event_count":3,"traces":${tracesText}}`));
    vi.stubGlobal("fetch", fetch);
    return fetch;
  };
  it("writes the received bytes with the fetched traces, as include_traces=true sends them", async () => {
    const fetch = serve();
    const value = JSON.parse(lazyRaw) as MineraExport;
    markLiveTraces(value);
    expect(await inlineMiningJson(value, lazyRaw, (v) => nativeJson(v, 2))).toBe(fullRaw);
    expect(fetch).toHaveBeenCalledTimes(1);
    // The cases are loaded once and shared with the views.
    expect(loadedTraces(traceLoad(value))).toEqual(TRACES);
  });
  it("nests the traces into an ACTIVE_SOURCE_MINING envelope", async () => {
    serve();
    // The backend nests the bound analysis (with its source_id) as it is.
    const envelopeRaw = `{"family":"ACTIVE_SOURCE_MINING","source_id":"7","analysis":${lazyRaw},"family_results":{}}`;
    const envelope = JSON.parse(envelopeRaw) as { analysis: MineraExport };
    markLiveTraces(envelope);
    const text = await inlineMiningJson(envelope, envelopeRaw, (v) => nativeJson(v, 2));
    expect(text).toBe(envelopeRaw.replace(summaryLine, `  "traces": ${tracesText},\n`));
    const filled = await inlineMiningValue(envelope);
    expect(filled.analysis.traces).toEqual(TRACES);
    expect(filled.analysis.trace_summary).toBeUndefined();
    expect(envelope.analysis.trace_summary).toBeDefined();
  });
  it("echoes include_traces=true in the envelope, as the full result's response does", async () => {
    serve();
    // The backend's echo: the multipart fields in name order, then the query
    // fields; a selector value may hold escaped quotes and braces.
    const controls = (includeTraces: string) => `{"case_column":"a\\\"}\\\\","include_traces":"${includeTraces}","mining_input_format":"auto","mining_request_id":"r1","mining_selectors_json":"{\\"case_id_key\\":\\"include_traces\\"}"}`;
    const envelopeRaw = (analysis: string, includeTraces: string) => `{"family":"ACTIVE_SOURCE_MINING","source_id":"7","active_source":{"submitted_controls":{"include_traces":"false"}},"analysis":${analysis},"family_results":{},"submitted_controls":${controls(includeTraces)},"inductive_options":{"noise_threshold":0},"error":null}`;
    const lazyEnvelope = envelopeRaw(lazyRaw, "false");
    const envelope = JSON.parse(lazyEnvelope) as { analysis: MineraExport; submitted_controls: Record<string, string> };
    markLiveTraces(envelope);
    const text = await inlineMiningJson(envelope, lazyEnvelope, (v) => nativeJson(v, 2));
    expect(text).toBe(envelopeRaw(fullRaw, "true"));
    expect(JSON.parse(text).submitted_controls.case_column).toBe('a"}\\');
    const filled = await inlineMiningValue(envelope);
    expect(filled.submitted_controls).toEqual({ ...envelope.submitted_controls, include_traces: "true" });
    expect(envelope.submitted_controls.include_traces).toBe("false");
    // Without the received text the serialized envelope says the same.
    expect(JSON.parse(await inlineMiningJson(envelope, undefined, (v) => nativeJson(v, 2))).submitted_controls.include_traces).toBe("true");
  });
  it("records the request that replays to the full result", async () => {
    const fields: Array<[string, string]> = [["mining_request_id", "r1"], ["include_traces", "false"], ["case_column", "include_traces"]];
    const request = { path: "/api/mine", fields };
    expect(inlineMiningRequest(request)).toEqual({ path: "/api/mine", fields: [["mining_request_id", "r1"], ["include_traces", "true"], ["case_column", "include_traces"]] });
    expect(request.fields[1]).toEqual(["include_traces", "false"]);
    const exact = { path: "/api/mine", fields: [["mining_request_id", "r1"]] as Array<[string, string]> };
    expect(inlineMiningRequest(exact)).toBe(exact);
    // The "Download exact source mining request" file.
    const mining = await readNativeResponse(respond(200, '{"family":"ACTIVE_SOURCE_MINING"}'), request) as ActiveDatasetResult;
    const source = { file: new File(["x"], "log.csv"), sha256: "00", source_id: "7", options: {} } as unknown as ActiveMiningSourceDescriptor;
    expect(JSON.parse(activeMiningSourceRequest(source, mining)).request.fields).toEqual([["mining_request_id", "r1"], ["include_traces", "true"], ["case_column", "include_traces"]]);
  });
  it("serializes in key order when the received text is not available", async () => {
    serve();
    const value = lazy("7");
    const text = await inlineMiningJson(value, undefined, (v) => nativeJson(v, 2));
    const parsed = JSON.parse(text) as Record<string, unknown>;
    expect(Object.keys(parsed)).toEqual(Object.keys(value).map((key) => key === "trace_summary" ? "traces" : key));
    expect(parsed.traces).toEqual(TRACES);
  });
  it("leaves a result that carries its traces as it is", async () => {
    const fetch = serve();
    const value = inline();
    expect(await inlineMiningValue(value)).toBe(value);
    expect(await inlineMiningJson(value, "raw", () => "serialized")).toBe("raw");
    expect(fetch).not.toHaveBeenCalled();
  });
  it("refuses to save without the cases when they cannot be loaded", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => respond(409, JSON.stringify({ error: "the backend restarted since this log was mined; mine it again" }))));
    await expect(inlineMiningJson(lazy("7"), lazyRaw, (v) => nativeJson(v, 2))).rejects.toThrow(/no longer holds this log/);
  });
});

describe("the Timeline decides from parseable timestamps", () => {
  const Probe = ({ data }: { data: MineraExport }) => createElement("i", null, String(useHasTimestamps(data)));
  it("follows the summary until the cases load, then whether any parses", async () => {
    const untimed: Trace[] = [{ case_id: "case-1", events: [{ activity: 0, timestamp: "not a date" }, { activity: 1, timestamp: "" }] },
      { case_id: "case-2", events: [{ activity: 0, timestamp: "soon" }] }];
    const data = lazy("7");
    expect(render(createElement(Probe, { data }))).toBe("<i>true</i>");
    vi.stubGlobal("fetch", vi.fn(async () => respond(200, tracesBody("7", untimed))));
    await loadTraces(data);
    expect(traceLoad(data).status).toBe("ready");
    expect(render(createElement(Probe, { data }))).toBe("<i>false</i>");
    expect(render(createElement(Probe, { data: inline() }))).toBe("<i>true</i>");
    expect(render(createElement(Probe, { data: { ...base, traces: untimed } as MineraExport }))).toBe("<i>false</i>");
  });
});
