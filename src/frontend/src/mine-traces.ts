// The mined log's cases, loaded on demand.
//
// The app mines with include_traces=false: the /api/mine response leaves out
// the "traces" array (about two thirds of a large body, and most of the heap
// JSON.parse needs for it) and carries a "trace_summary" instead. The views
// that list or plot cases load them here from GET /api/traces, once per
// mined source; every other view decides what to offer from the summary.
// Analyses that still carry their traces (saved JSON exports, the CLI's
// output, a script that mines with the default) are read in place. Only a
// mining response this app received (markLiveTraces) loads from the backend:
// an opened JSON file never does, and one saved without its cases says so.
// Exports write the cases back in (inlineMiningJson, inlineMiningValue).
import { useEffect, useSyncExternalStore } from "react";
import { backendFetch } from "./backend-fetch";
import { parseInstant } from "./dotted-chart";
import { isMineraTrace, type MineraExport, type Trace, type TraceSummary } from "./types";

export type TraceLoad =
  /** The traces are known: inline in the analysis, or fetched. */
  | { status: "ready"; traces: Trace[] }
  /** Nothing asked for these traces yet; a view that needs them starts the load. */
  | { status: "idle" }
  /** The traces are being fetched. */
  | { status: "loading" }
  | { status: "error"; error: string }
  /** The analysis has no traces and no source to fetch them from (an old export). */
  | { status: "unavailable" };

const IDLE: TraceLoad = { status: "idle" };
const LOADING: TraceLoad = { status: "loading" };
const UNAVAILABLE: TraceLoad = { status: "unavailable" };
const NO_TRACES: Trace[] = [];

interface Entry {
  /** The summary of the analysis these traces belong to; a new mine has a new one. */
  summary: TraceSummary;
  sourceId: string;
  state: TraceLoad;
  controller: AbortController | null;
  promise: Promise<void>;
}

// One mined source at a time: loading another one aborts and drops this one.
let entry: Entry | null = null;
const listeners = new Set<() => void>();
const inline = new WeakMap<Trace[], TraceLoad>();
/** The summaries of mining responses this app received from the backend. */
const live = new WeakSet<TraceSummary>();
/** The "traces" value of a GET /api/traces body as received, by the traces parsed from it. */
const rawTraces = new WeakMap<Trace[], string>();
const emit = () => { for (const listener of listeners) listener(); };
const subscribe = (listener: () => void) => { listeners.add(listener); return () => { listeners.delete(listener); }; };

const record = (value: unknown): value is Record<string, unknown> => !!value && typeof value === "object" && !Array.isArray(value);

/** The analysis of a mining result: the classic analysis itself, or the one an ACTIVE_SOURCE_MINING envelope nests. */
function miningAnalysis(value: unknown): MineraExport | null {
  if (!record(value)) return null;
  if (value.family === "ACTIVE_SOURCE_MINING") return record(value.analysis) ? value.analysis as unknown as MineraExport : null;
  return value as unknown as MineraExport;
}

/**
 * Marks the analysis of a mining response the backend just sent as one whose
 * cases may be fetched on demand. Opened files are never marked.
 */
export function markLiveTraces(value: unknown): void {
  const summary = miningAnalysis(value)?.trace_summary;
  if (record(summary)) live.add(summary as unknown as TraceSummary);
}

/** The summary an analysis that left its traces out carries, when it can fetch them. */
function lazySummary(data: MineraExport): TraceSummary | null {
  const summary = data.trace_summary;
  return !data.traces && summary && live.has(summary) && summary.run_id && data.source_id ? summary : null;
}

/** The current trace state of an analysis, without starting a load. */
export function traceLoad(data: MineraExport): TraceLoad {
  if (data.traces) {
    let state = inline.get(data.traces);
    if (!state) inline.set(data.traces, state = { status: "ready", traces: data.traces });
    return state;
  }
  const summary = lazySummary(data);
  if (!summary) return UNAVAILABLE;
  return entry && entry.summary === summary ? entry.state : IDLE;
}

/** The loaded traces, or an empty list while they are not (yet) known. */
export function loadedTraces(load: TraceLoad): Trace[] {
  return load.status === "ready" ? load.traces : NO_TRACES;
}

function traceError(status: number, value: unknown): string {
  const message = value && typeof value === "object" && typeof (value as { error?: unknown }).error === "string"
    ? (value as { error: string }).error : null;
  if (status === 409) return "The backend no longer holds this log, so its cases cannot be loaded. Open the log again.";
  return message ? `Could not load the cases: ${message}` : `Could not load the cases (status ${status}).`;
}

/** GET /api/traces for the source of `data`, checked against its summary. */
export async function requestTraces(data: MineraExport, signal?: AbortSignal): Promise<Trace[]> {
  const sourceId = data.source_id, runId = data.trace_summary?.run_id;
  if (!sourceId || !runId) throw new Error("This analysis has no backend source to load its cases from.");
  const response = await backendFetch(`/api/traces?source_id=${encodeURIComponent(sourceId)}&run_id=${encodeURIComponent(runId)}`, { signal });
  const text = await response.text().catch(() => "");
  let value: unknown = null;
  try { value = JSON.parse(text); } catch { value = null; }
  signal?.throwIfAborted();
  if (!response.ok) throw new Error(traceError(response.status, value));
  const body = value as { family?: unknown; source_id?: unknown; run_id?: unknown; trace_count?: unknown; event_count?: unknown; traces?: unknown } | null;
  if (!body || body.family !== "ANALYSIS_TRACES" || body.source_id !== sourceId || body.run_id !== runId ||
      body.trace_count !== data.trace_count || body.event_count !== data.event_count || !Array.isArray(body.traces))
    throw new Error("The backend's cases do not belong to this analysis. Open the log again.");
  const traces = body.traces as unknown[];
  const activities = data.activities.length;
  let events = 0;
  for (const trace of traces) {
    if (!isMineraTrace(trace, activities)) throw new Error("The backend returned unreadable cases.");
    events += trace.events.length;
  }
  if (traces.length !== data.trace_count || events !== data.event_count)
    throw new Error("The backend's cases do not belong to this analysis. Open the log again.");
  // The "traces" member is the body's last: keep its text for exact exports.
  const key = text.indexOf(',"traces":');
  if (key >= 0 && text.endsWith("}")) rawTraces.set(traces as Trace[], text.slice(key + 10, -1));
  return traces as Trace[];
}

/**
 * Starts loading the traces of `data` unless they are inline, loaded or
 * already loading; a failed load is tried again. Loading another source
 * aborts the one in flight.
 */
export function loadTraces(data: MineraExport): Promise<void> {
  const summary = lazySummary(data);
  if (!summary) return Promise.resolve();
  if (entry && entry.summary === summary && entry.state.status !== "error") return entry.promise;
  entry?.controller?.abort();
  const controller = new AbortController();
  const current: Entry = { summary, sourceId: data.source_id!, state: LOADING, controller, promise: Promise.resolve() };
  entry = current;
  current.promise = requestTraces(data, controller.signal).then(
    (traces) => { if (entry === current) { current.state = { status: "ready", traces }; current.controller = null; emit(); } },
    (cause: unknown) => {
      if (entry !== current || controller.signal.aborted) return;
      current.state = { status: "error", error: cause instanceof Error ? cause.message : String(cause) };
      current.controller = null;
      emit();
    });
  emit();
  return current.promise;
}

/**
 * Drops (and aborts) every loaded or loading traces except those of `data`,
 * so the cases of a log the app no longer shows do not stay in memory.
 */
export function retainTraces(data: MineraExport | null): void {
  if (!entry || (data && lazySummary(data) === entry.summary)) return;
  entry.controller?.abort();
  entry = null;
  emit();
}

/** The traces of `data`, loading them when they are on demand; throws when they cannot be had. */
async function requireTraces(data: MineraExport): Promise<Trace[]> {
  if (data.traces) return data.traces;
  if (data.trace_summary && !lazySummary(data))
    throw new Error("This analysis was saved without its cases, and they cannot be loaded from the backend.");
  await loadTraces(data);
  const state = traceLoad(data);
  if (state.status === "ready") return state.traces;
  throw new Error(state.status === "error" ? state.error : "The cases of this analysis could not be loaded.");
}

/** `analysis` with its cases where its trace summary stood, every other key in place. */
function withTraces(analysis: MineraExport, traces: Trace[]): MineraExport {
  const out: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(analysis)) {
    if (key === "trace_summary") out.traces = traces;
    else out[key] = value;
  }
  return out as unknown as MineraExport;
}

/**
 * A mining result (classic analysis or ACTIVE_SOURCE_MINING envelope) with
 * its analysis' cases inline, loaded first when they are on demand. A result
 * that carries its cases (or has no analysis) is returned as it is.
 */
export async function inlineMiningValue<T extends object>(value: T): Promise<T> {
  const analysis = miningAnalysis(value);
  if (!analysis || analysis.traces || !analysis.trace_summary) return value;
  const filled = withTraces(analysis, await requireTraces(analysis));
  return (analysis === (value as unknown) ? filled : { ...value, analysis: filled }) as T;
}

const SUMMARY_LINE = '\n  "trace_summary": {"traces": "on_demand"';

/**
 * The text of a mining result whose cases are inline: the response bytes as
 * received with the trace summary line replaced by the fetched "traces" bytes,
 * which is what the backend sends with include_traces=true. `raw` is the
 * received text; without it (or when it is not the backend's layout) the
 * result is serialized with `serialize`.
 */
export async function inlineMiningJson(value: object, raw: string | undefined, serialize: (value: object) => string): Promise<string> {
  const analysis = miningAnalysis(value);
  if (!analysis || analysis.traces || !analysis.trace_summary) return raw ?? serialize(value);
  const traces = await requireTraces(analysis);
  const bytes = rawTraces.get(traces);
  const start = raw?.indexOf(SUMMARY_LINE) ?? -1;
  // The summary is one line (its strings are escaped), ending at ",\n".
  const end = start >= 0 && raw!.indexOf(SUMMARY_LINE, start + 1) < 0 ? raw!.indexOf(",\n", start + SUMMARY_LINE.length) : -1;
  if (raw && bytes !== undefined && end >= 0)
    return raw.slice(0, start) + '\n  "traces": ' + bytes + raw.slice(end);
  return serialize(await inlineMiningValue(value));
}

/**
 * The traces of `data` for a view: inline ones at once, otherwise fetched
 * when the view mounts (and `enabled`), shared by every view of the source.
 */
export function useTraces(data: MineraExport, enabled = true): TraceLoad & { retry: () => void } {
  const state = useSyncExternalStore(subscribe, () => traceLoad(data), () => traceLoad(data));
  useEffect(() => { if (enabled) void loadTraces(data); }, [data, enabled]);
  // An enabled view starts the load right after rendering: show it loading.
  return { ...(enabled && state.status === "idle" ? LOADING : state), retry: () => void loadTraces(data) };
}

/**
 * What the views need to know about the cases without loading them: read
 * from the traces when the analysis carries them, from its summary otherwise.
 */
export interface TraceFacts {
  /** Whether the analysis has cases to list (the Event log view). */
  hasCases: boolean;
  /** Whether any case has a timestamped event (the Dotted chart). */
  hasTimestamps: boolean;
  /** Distinct lifecycle transitions, in first-seen order. */
  lifecycles: string[];
  /** Events of the longest case; undefined when unknown. */
  maxTraceLength: number | undefined;
}

const facts = new WeakMap<object, TraceFacts>();

/** Whether some case has an event whose timestamp Date.parse reads (a dotted chart row). */
const plotted = new WeakMap<Trace[], boolean>();
function hasPlottableEvent(traces: Trace[]): boolean {
  let known = plotted.get(traces);
  if (known === undefined) {
    known = traces.some((trace) => trace.events.some((event) => parseInstant(event.timestamp) !== null));
    plotted.set(traces, known);
  }
  return known;
}

/**
 * Whether the analysis has timestamps to plot: the summary's word until its
 * cases load, then whether any of them parses in this browser.
 */
export function useHasTimestamps(data: MineraExport | null): boolean {
  const state = useSyncExternalStore(subscribe, () => data ? traceLoad(data) : UNAVAILABLE, () => data ? traceLoad(data) : UNAVAILABLE);
  if (!data) return false;
  return state.status === "ready" ? hasPlottableEvent(state.traces) : traceFacts(data).hasTimestamps;
}

export function traceFacts(data: MineraExport): TraceFacts {
  // A summary saved in a file still describes its cases; the views then say
  // they were not saved (traceLoad answers "unavailable").
  const key: object | undefined = data.traces ?? data.trace_summary ?? undefined;
  if (!key) return { hasCases: false, hasTimestamps: false, lifecycles: [], maxTraceLength: undefined };
  const known = facts.get(key);
  if (known) return known;
  let result: TraceFacts;
  if (data.traces) {
    const lifecycles: string[] = [];
    const seen = new Set<string>();
    let maxTraceLength = 0;
    for (const trace of data.traces) {
      maxTraceLength = Math.max(maxTraceLength, trace.events.length);
      for (const event of trace.events)
        if (event.lifecycle && !seen.has(event.lifecycle)) { seen.add(event.lifecycle); lifecycles.push(event.lifecycle); }
    }
    result = { hasCases: data.traces.length > 0, hasTimestamps: hasPlottableEvent(data.traces), lifecycles, maxTraceLength };
  } else {
    const summary = data.trace_summary!;
    result = { hasCases: summary.trace_count > 0, hasTimestamps: summary.has_timestamps, lifecycles: summary.lifecycles, maxTraceLength: summary.max_trace_length };
  }
  facts.set(key, result);
  return result;
}
