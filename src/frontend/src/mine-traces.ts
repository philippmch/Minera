// The mined log's cases, loaded on demand.
//
// The app mines with include_traces=false: the /api/mine response leaves out
// the "traces" array (about two thirds of a large body, and most of the heap
// JSON.parse needs for it) and carries a "trace_summary" instead. The views
// that list or plot cases load them here from GET /api/traces, once per
// mined source; every other view decides what to offer from the summary.
// Analyses that still carry their traces (saved JSON exports, the CLI's
// output, a script that mines with the default) are read in place.
import { useEffect, useSyncExternalStore } from "react";
import { backendFetch } from "./backend-fetch";
import { buildDottedChart } from "./dotted-chart";
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
const emit = () => { for (const listener of listeners) listener(); };
const subscribe = (listener: () => void) => { listeners.add(listener); return () => { listeners.delete(listener); }; };

/** The summary an analysis that left its traces out carries, when it can fetch them. */
function lazySummary(data: MineraExport): TraceSummary | null {
  return !data.traces && data.trace_summary && data.source_id ? data.trace_summary : null;
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
  const sourceId = data.source_id;
  if (!sourceId) throw new Error("This analysis has no backend source to load its cases from.");
  const response = await backendFetch(`/api/traces?source_id=${encodeURIComponent(sourceId)}`, { signal });
  const value: unknown = await response.json().catch(() => null);
  signal?.throwIfAborted();
  if (!response.ok) throw new Error(traceError(response.status, value));
  const body = value as { family?: unknown; source_id?: unknown; trace_count?: unknown; event_count?: unknown; traces?: unknown } | null;
  if (!body || body.family !== "ANALYSIS_TRACES" || body.source_id !== sourceId ||
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

export function traceFacts(data: MineraExport): TraceFacts {
  const key: object | undefined = data.traces ?? lazySummary(data) ?? undefined;
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
    result = { hasCases: data.traces.length > 0, hasTimestamps: buildDottedChart(data.traces).rowCount > 0, lifecycles, maxTraceLength };
  } else {
    const summary = data.trace_summary!;
    result = { hasCases: summary.trace_count > 0, hasTimestamps: summary.has_timestamps, lifecycles: summary.lifecycles, maxTraceLength: summary.max_trace_length };
  }
  facts.set(key, result);
  return result;
}
