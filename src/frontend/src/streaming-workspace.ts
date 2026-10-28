import { streamingDfgQuery, streamingFootprintsQuery, streamingTemporalQuery, streamingTbrQuery } from "./api";
import {
  isStreamingDfgResult, isStreamingFootprintsResult, isStreamingTemporalResult,
  isStreamingTbrResult, type StreamingDfgResult, type StreamingFootprintsResult,
  type StreamingTemporalResult, type StreamingTbrResult,
} from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { type OcelPreparedTable } from "./ocel-preparation";
import { type ModelGraph } from "./model-graph";
import { isDeclareStreamResult,declareStreamTables,type DeclareStreamResult } from "./declare-stream-workspace";

export const STREAMING_METHODS = [
  ["dfg", "Discover a streaming DFG"], ["footprints", "Check footprint prefixes"],
  ["temporal", "Check temporal prefixes"], ["tbr", "Replay tokens over prefixes"],
  ["declare", "Check Declare prefixes"],
] as const;
export type StreamingMethod = typeof STREAMING_METHODS[number][0];
export interface StreamingForm {
  method: StreamingMethod;
  order: "input" | "imported";
  caseColumn: string;
  activityColumn: string;
  timestampColumn: string;
  startColumn: string;
  activities: string;
  starts: string;
  ends: string;
  sequence: string;
  parallel: string;
  profile: string;
  zeta: string;
  finalize: boolean;
  variant: "forward" | "backwards";
  maxInvisible: string;
  stopImmediately: boolean;
}
export function defaultStreaming(): StreamingForm {
  return {
    method: "dfg", order: "input", caseColumn: "", activityColumn: "",
    timestampColumn: "", startColumn: "", activities: '["A","B","C"]',
    starts: '["A"]', ends: '["C"]', sequence: '[["A","B"],["B","C"]]',
    parallel: "", profile: '[["A","B",5,1],["B","C",5,1]]',
    zeta: "", finalize: false, variant: "forward", maxInvisible: "",
    stopImmediately: false,
  };
}
function jsonList<T>(text: string, name: string): T[] {
  let value: unknown;
  try { value = JSON.parse(text); } catch { throw new Error(name + " must be a JSON array."); }
  if (!Array.isArray(value)) throw new Error(name + " must be a JSON array.");
  return value as T[];
}
/** A finite number; the reference accepts zero and negative zeta values. */
function finiteNumber(text: string, name: string): number | undefined {
  if (!text) return undefined;
  const value = Number(text);
  if (!text.trim() || !Number.isFinite(value))
    throw new Error(name + " must be a finite number.");
  return value;
}
/** Preserve safety bounds as decimal text rather than rounding through Number. */
export function invisibleBound(text: string): string {
  if (!/^\d+$/.test(text) || BigInt(text) > 18446744073709551615n)
    throw new Error("Invisible search limit must be an unsigned 64-bit integer.");
  return text;
}
export function streamingQuery(form: StreamingForm): string {
  let query: string;
  if (form.method === "dfg") query = streamingDfgQuery();
  else if (form.method === "footprints") query = streamingFootprintsQuery({
    streaming_footprints_activities: jsonList<string>(form.activities, "Activities"),
    ...(form.starts ? { streaming_footprints_start_activities: jsonList<string>(form.starts, "Start activities") } : {}),
    ...(form.ends ? { streaming_footprints_end_activities: jsonList<string>(form.ends, "End activities") } : {}),
    ...(form.sequence && form.sequence.trim() !== "[]" ? { streaming_footprints_sequence: jsonList<[string, string]>(form.sequence, "Sequence pairs") } : {}),
    ...(form.parallel && form.parallel.trim() !== "[]" ? { streaming_footprints_parallel: jsonList<[string, string]>(form.parallel, "Parallel pairs") } : {}),
    streaming_footprints_terminate_all: form.finalize,
  });
  else if (form.method === "temporal") query = streamingTemporalQuery({
    streaming_temporal_profile: jsonList<[string, string, number, number]>(form.profile, "Temporal profile"),
    streaming_temporal_zeta: finiteNumber(form.zeta, "Zeta"),
  });
  else if(form.method==="declare")query="";
  else query = streamingTbrQuery({
    streaming_tbr_variant: form.variant,
    streaming_tbr_stop_immediately: form.stopImmediately,
  });
  const params = new URLSearchParams(query);
  const prefix = form.method === "dfg" ? "streaming_" : "streaming_" + form.method + "_";
  // Column selectors are literal; the native adapter applies its own aliases.
  if (form.caseColumn) params.set(prefix + "case_id_column", form.caseColumn);
  if (form.activityColumn) params.set(prefix + "activity_column", form.activityColumn);
  if (form.method === "temporal"||form.method==="declare") {
    if (form.timestampColumn) params.set(prefix + "timestamp_column", form.timestampColumn);
    if (form.method==="temporal"&&form.startColumn) params.set(prefix + "start_timestamp_column", form.startColumn);
  }
  if (form.method === "tbr") {
    if (form.maxInvisible) params.set("streaming_tbr_max_invisible", invisibleBound(form.maxInvisible));
    params.set("streaming_tbr_terminate_all", String(form.finalize));
  }
  params.set("streaming_event_order", form.order);
  params.set("streaming_include_diagnostics", "true");
  return params.toString();
}

type BaseResult = StreamingDfgResult | StreamingFootprintsResult | StreamingTemporalResult | StreamingTbrResult | DeclareStreamResult;
type TerminatedCase = { case_id: string; is_fit: boolean; missing?: number; remaining?: number; marking?: number[] };
export type StreamingResult = BaseResult & {
  input: { event_order: "input" | "imported"; parsed_events: number };
  prefix?: StreamingFootprintsResult | StreamingTbrResult;
  terminated_cases?: TerminatedCase[];
};
const record = (value: unknown): value is Record<string, unknown> =>
  !!value && typeof value === "object" && !Array.isArray(value);
const count = (value: unknown) => Number.isSafeInteger(value) && (value as number) >= 0;
export function validStreamingResult(value: unknown, form: StreamingForm): value is StreamingResult {
  const guard = { dfg: isStreamingDfgResult, footprints: isStreamingFootprintsResult,
    temporal: isStreamingTemporalResult, tbr: isStreamingTbrResult,declare:isDeclareStreamResult }[form.method];
  if (!guard(value) || !record(value) || !record(value.input) ||
      value.input.event_order !== form.order || !count(value.input.parsed_events)) return false;
  if (form.method === "tbr" && value.variant !== form.variant.toUpperCase()) return false;
  if ((form.method === "footprints" || form.method === "tbr") && form.finalize) {
    const prefixGuard = form.method === "footprints" ? isStreamingFootprintsResult : isStreamingTbrResult;
    if (!prefixGuard(value.prefix) || !Array.isArray(value.terminated_cases)) return false;
    const prefix = value.prefix as StreamingFootprintsResult | StreamingTbrResult;
    if (form.method === "tbr" && (prefix as StreamingTbrResult).variant !== value.variant) return false;
    const prefixIds = Array.isArray(prefix.cases) ? prefix.cases.map(c => c.case_id) : Object.keys(prefix.cases);
    const terminated = value.terminated_cases;
    if (terminated.length !== prefixIds.length ||
      new Set(terminated.map(entry => record(entry) ? entry.case_id : undefined)).size !== prefixIds.length ||
      !terminated.every(entry =>
      record(entry) && prefixIds.includes(entry.case_id as string) && typeof entry.is_fit === "boolean" &&
      (form.method !== "tbr" || (count(entry.missing) && count(entry.remaining) &&
        Array.isArray(entry.marking) && entry.marking.every(count))))) return false;
    const cases = (value as unknown as { cases: unknown }).cases;
    if (!cases || Object.keys(cases).length) return false;
  } else if (value.prefix !== undefined || value.terminated_cases !== undefined) return false;
  return true;
}
export interface StreamingRun {
  method: StreamingMethod;
  query: string;
  raw: string;
  report_json: string;
  result: StreamingResult;
  sources: { role: "log" | "model"; name: string; size: number; sha256: string }[];
  tables: Record<string, OcelPreparedTable>;
}
function rows(columns: string[], values: unknown[][]): OcelPreparedTable {
  return { columns, count: values.length, rowAt: index => values[index] };
}
function recordTable(items: Record<string, unknown>[]): OcelPreparedTable {
  const columns = [...new Set(items.flatMap(item => Object.keys(item)))];
  return rows(columns, items.map(item => columns.map(key => nativeCell(item, key))));
}
export function streamingTables(result: StreamingResult): Record<string, OcelPreparedTable> {
  const tables: Record<string, OcelPreparedTable> = {};
  const addCases = (value: StreamingFootprintsResult | StreamingTbrResult | StreamingTemporalResult | DeclareStreamResult, name: string) => {
    if (Array.isArray(value.cases)) {
      const columns = ["case_id", "events", "deviations", "is_fit", "first_activity", "last_activity"];
      tables[name] = rows(columns, value.cases.map(item => columns.map(key => nativeCell(item, key))));
    } else {
      const columns = value.family === "STREAMING_TBR"
        ? ["case_id", "events", "missing", "remaining", "is_fit", "marking"]
        : ["case_id", "events", "deviations"];
      // A Declare stream keeps the null case apart from the named cases
      // (including the empty-string case); it is listed with a null id.
      const entries: [string | null, unknown][] = Object.entries(value.cases);
      if ("null_case" in value && value.null_case !== undefined) entries.push([null, value.null_case]);
      tables[name] = rows(columns, entries.map(([id, item]) =>
        [id, ...columns.slice(1).map(key => key === "marking" ? nativeJson((item as StreamingTbrResult["cases"][string]).marking) : nativeCell(item as object, key))]));
    }
  };
  if (result.family === "STREAMING_DFG") {
    tables.edges = rows(["source", "target", "count"], result.graph.map(edge =>
      [edge.source, edge.target, nativeCell(edge, "count")]));
    for (const key of ["activities", "start_activities", "end_activities"] as const)
      tables[key] = rows(["activity", "count"], Object.keys(result[key]).map(id => [id, nativeCell(result[key], id)]));
    tables.open_cases = rows(["case_id", "last_activity"], Object.entries(result.open_cases));
  } else {
    addCases(result, "open_cases");
    if (result.family === "STREAMING_TEMPORAL") tables.deviations = rows(
      ["case_id", "source", "target", "elapsed_seconds", "standard_deviation_distance"],
      Object.entries(result.deviations).flatMap(([id, entries]) => entries.map(entry =>
        [id, entry.source, entry.target, nativeCell(entry, "elapsed_seconds"), nativeCell(entry, "standard_deviation_distance")])));
    if (result.prefix) addCases(result.prefix, "prefix_cases");
    if (result.terminated_cases) tables.terminated_cases = result.family === "STREAMING_TBR"
      ? rows(["case_id", "missing", "remaining", "is_fit", "marking"], result.terminated_cases.map(item =>
        [item.case_id, nativeCell(item, "missing"), nativeCell(item, "remaining"), item.is_fit, nativeJson(item.marking)]))
      : rows(["case_id", "is_fit"], result.terminated_cases.map(item => [item.case_id, item.is_fit]));
  }
  const summaryKeys = Object.keys(result).filter(key => typeof (result as unknown as Record<string, unknown>)[key] !== "object");
  if(result.family==="STREAMING_DECLARE")Object.assign(tables,declareStreamTables(result));
  tables.summary = rows(summaryKeys, [summaryKeys.map(key => nativeCell(result, key))]);
  tables.input = recordTable([result.input]);
  return tables;
}
export function streamingGraph(result: StreamingResult): ModelGraph | undefined {
  if (result.family !== "STREAMING_DFG") return undefined;
  return {
    nodes: Object.keys(result.activities).map(id => ({ id, label: id,
      detail: String(nativeCell(result.activities, id)) + " events",
      start: Object.hasOwn(result.start_activities, id), end: Object.hasOwn(result.end_activities, id) })),
    edges: result.graph.map((edge, i) => ({ id: String(i), source: edge.source,
      target: edge.target, value: edge.count, label: String(nativeCell(edge, "count")), directed: true })),
  };
}
export async function requestStreamingWorkflow(
  form: StreamingForm, log: File, model?: File | null, signal?: AbortSignal,
): Promise<StreamingRun> {
  if (!/\.csv$/i.test(log.name)&&!(form.method==="declare"&&/\.xes(?:\.gz)?$/i.test(log.name))) throw new Error("Choose a CSV event file, or XES for Declare prefixes.");
  if (form.method === "tbr" && !model) throw new Error("Choose a PNML workflow net for token replay.");
  if (form.method === "declare" && !model) throw new Error("Choose a Declare model JSON document.");
  const query = streamingQuery(form);
  const sources: StreamingRun["sources"] = [];
  const files: ["log" | "model", File][] = [["log", log]];
  if (form.method === "tbr"||form.method==="declare") files.push(["model", model!]);
  for (const [role, file] of files) {
    const hash = await crypto.subtle.digest("SHA-256", await file.arrayBuffer());
    sources.push({ role, name: file.name, size: file.size,
      sha256: [...new Uint8Array(hash)].map(byte => byte.toString(16).padStart(2, "0")).join("") });
  }
  signal?.throwIfAborted();
  const body = new FormData(); body.append("log", log, log.name);
  if (form.method === "tbr"||form.method==="declare") body.append("model", model!, model!.name);
  const params=new URLSearchParams(query);if(form.method==="declare")params.set("format",/\.xes(?:\.gz)?$/i.test(log.name)?"xes":"csv");
  const response = await fetch("/api/streaming-" + form.method + "?" + params, { method: "POST", body, signal });
  const raw = await response.text(); signal?.throwIfAborted();
  let result: unknown;
  try { result = parseNativeJson(raw); } catch { throw new Error("The backend returned invalid streaming JSON."); }
  if (!response.ok || (record(result) && "error" in result))
    throw new Error(record(result) && typeof result.error === "string" ? result.error : "Streaming request failed (" + response.status + ").");
  if (!validStreamingResult(result, form)) throw new Error("The backend returned an invalid or mismatched streaming result.");
  const report_json = '{"family":"STREAMING_WORKFLOW_REPORT","request":' +
    JSON.stringify({ method: form.method, query:params.toString(), parameters: Object.fromEntries(params), sources,
      session: "fresh monitor per request" }) + ',"response":' + raw + '}';
  return { method: form.method, query:params.toString(), raw, report_json, result, sources, tables: streamingTables(result) };
}
