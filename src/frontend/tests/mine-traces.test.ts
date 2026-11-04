import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { loadTraces, loadedTraces, retainTraces, traceFacts, traceLoad } from "../src/mine-traces";
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

/** A mining response requested with include_traces=false. Each call is a new mine. */
function lazy(sourceId = "7", overrides: Partial<MineraExport["trace_summary"]> = {}): MineraExport {
  return { ...base, source_id: sourceId, trace_summary: {
    traces: "on_demand", trace_count: 2, event_count: 3, max_trace_length: 2,
    timestamped_trace_count: 2, has_timestamps: true, has_lifecycles: true, lifecycles: ["start", "complete"], ...overrides,
  } } as MineraExport;
}

const inline = (): MineraExport => ({ ...base, traces: TRACES } as MineraExport);

const tracesBody = (sourceId: string, traces: unknown = TRACES, extra: object = {}) =>
  JSON.stringify({ family: "ANALYSIS_TRACES", source_id: sourceId, trace_count: 2, event_count: 3, traces, ...extra });

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
    expect(String((fetch.mock.calls[0] as unknown[])[0])).toBe("/api/traces?source_id=7");
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
