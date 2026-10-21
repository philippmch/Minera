import {nativeCell} from "./native-json";
import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString, originalEmbeddingFilterStringBytes, originalEmbeddingFilterStringText, type OriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";

export const REFERENCE_LOG_FILTER_OPERATIONS = ["traces", "prefix", "suffix", "start", "end", "rework", "repetition", "between", "case_size", "case_count", "case_performance", "paths", "path_performance", "time_events", "time_contained", "time_intersecting", "time_starting", "time_completing", "time_attribute", "time_trace_attribute", "variants", "variants_top_k", "variants_min_coverage", "variants_max_coverage", "variants_cumulative"] as const;
export type ReferenceLogFilterOperation = typeof REFERENCE_LOG_FILTER_OPERATIONS[number];
export const REFERENCE_LOG_FILTER_LABELS: Record<ReferenceLogFilterOperation, string> = {
  variants: "Match complete activity variants", variants_top_k: "Keep the top k variants", variants_min_coverage: "Keep variants above a coverage threshold", variants_max_coverage: "Keep variants below a coverage threshold", variants_cumulative: "Keep variants to cumulative coverage",
  traces: "Match an activity sequence", prefix: "Keep a prefix before an activity",
  suffix: "Keep a suffix after an activity", start: "Match the first activity",
  end: "Match the last activity", rework: "Find repeated activity", repetition: "Count an exact attribute value", between: "Extract subtraces between activities",
  case_size: "Filter by case event count", case_count: "Limit the number of cases", case_performance: "Filter by case duration",
  paths: "Match adjacent activity pairs", path_performance: "Filter by adjacent-pair duration",
  time_events: "Keep events in a time window", time_contained: "Keep traces contained in a time window", time_intersecting: "Keep traces intersecting a time window",
  time_starting: "Filter by first-event time", time_completing: "Filter by last-event time", time_attribute: "Match an attribute in a time window", time_trace_attribute: "Filter a trace timestamp attribute",
};
export const REFERENCE_LOG_FILTER_CSV_FIELDS = [
  ["case_id_column", "Case ID column", "case_id"], ["activity_column", "Activity column", "activity"],
  ["timestamp_column", "Timestamp column", "timestamp"], ["delimiter", "Delimiter", ","],
] as const;
export type ReferenceLogFilterCsvKey = typeof REFERENCE_LOG_FILTER_CSV_FIELDS[number][0];
export type ReferenceLogFilterSourceFormat = "auto" | "csv" | "xes" | "native_log";
export type ReferenceLogFilterNativeString = OriginalEmbeddingFilterNativeString;
export interface ReferenceLogFilterForm {
  operation: ReferenceLogFilterOperation; sourceFormat: ReferenceLogFilterSourceFormat;
  activity: string; activityKeyMode: "default" | "custom"; activityKey: string;
  positive: boolean; strict: boolean; occurrence: "first" | "last";
  variantsJson: string; variantsCriterion: string; variantsSequenceKind: "list" | "tuple";
  admittedJson: string; patternsJson: string; minOccurrences: string;
  repetitionTargetJson: string; minRep: string; maxRep: string;
  minCaseSize: string; maxCaseSize: string; maxCases: string; minCaseSeconds: string; maxCaseSeconds: string;
  pathsJson: string; pathJson: string; minPathSeconds: string; maxPathSeconds: string;
  timeLower: string; timeUpper: string; timeAttributeKeyMode: "default" | "custom"; timeAttributeKey: string; timeTargetJson: string;
  timestampKeyMode: "default" | "custom"; timestampKey: string;
  betweenStartJson: string; betweenEndJson: string; betweenCaseKeyMode: "default" | "custom"; betweenCaseKey: string; betweenSeparatorMode: "default" | "custom"; betweenSeparator: string;
  csvOptions: boolean; csv: Record<ReferenceLogFilterCsvKey, string>; trimFields: boolean;
}
export function defaultReferenceLogFilterForm(operation: ReferenceLogFilterOperation = "rework"): ReferenceLogFilterForm {
  return {variantsJson: '[["A", "B"]]', variantsCriterion: "1", variantsSequenceKind: "tuple", operation, sourceFormat: "auto", activity: "", activityKeyMode: "default", activityKey: "",
    minCaseSize: "2", maxCaseSize: "null", maxCases: "1000", minCaseSeconds: "0", maxCaseSeconds: "10000000000", timestampKeyMode: "default", timestampKey: "time:timestamp",
    pathsJson: '[["A", "B"]]', pathJson: '["A", "B"]', minPathSeconds: "0", maxPathSeconds: "9223372036854775807",
    timeLower: "2026-06-01 00:00:01", timeUpper: "2026-06-01 00:00:03", timeAttributeKeyMode: "default", timeAttributeKey: "concept:name", timeTargetJson: '{"type":"string","value":"A"}',
    positive: true, strict: true, occurrence: "first", admittedJson: '["A"]', patternsJson: '[["A", "...", "B"]]',
    betweenStartJson: '"A"', betweenEndJson: '"B"', betweenCaseKeyMode: "default", betweenCaseKey: "concept:name", betweenSeparatorMode: "default", betweenSeparator: "##@@",
    minOccurrences: "2", repetitionTargetJson: '{"type":"string","value":"web"}', minRep: "2", maxRep: "9223372036854775807", csvOptions: false, csv: {case_id_column: "case_id", activity_column: "activity", timestamp_column: "timestamp", delimiter: ","}, trimFields: false};
}
export interface ReferenceLogFilterCounts {traces: number; events: number;}
export interface ReferenceLogFilterEnvelope {
  family: "REFERENCE_LOG_FILTER"; operation: string | null; http_status: 200 | 400; phase: string;
  source_format: string | null; source: ReferenceLogFilterCounts | null; result: ReferenceLogFilterCounts | null;
  filter_attribute_key?: ReferenceLogFilterNativeString | null;
  case_id_key?: ReferenceLogFilterNativeString | null; full_result_json?: ReferenceLogFilterNativeString | null;
  timestamp_key?: ReferenceLogFilterNativeString; time_attribute_key?: ReferenceLogFilterNativeString;
  source_log_json?: ReferenceLogFilterNativeString | null; source_after_log_json?: ReferenceLogFilterNativeString | null;
  source_unchanged: boolean | null; activity_key: ReferenceLogFilterNativeString | null;
  filtered_log_json: ReferenceLogFilterNativeString | null; filtered_xes: ReferenceLogFilterNativeString | null;
  xes_error: ReferenceLogFilterNativeString | null; error: ReferenceLogFilterNativeString | null;
}
export interface ReferenceLogFilterRun {
  value: ReferenceLogFilterEnvelope; sourceFile: File; fields: Array<[string, string]>;
  raw: string; responseBytes: Uint8Array<ArrayBuffer>;
}
const object = (v: unknown): v is Record<string, unknown> => v !== null && typeof v === "object" && !Array.isArray(v);
const keys = (v: Record<string, unknown>, names: readonly string[]) => Object.keys(v).length === names.length && names.every(k => Object.hasOwn(v, k));
const nullableString = (v: unknown) => v === null || isOriginalEmbeddingFilterNativeString(v);
function counts(v: unknown): v is ReferenceLogFilterCounts | null {
  return v === null || object(v) && keys(v, ["traces", "events"]) && ["traces", "events"].every(key => {
    const value = v[key], token = String(nativeCell(v, key));
    return typeof value === "number" && Number.isFinite(value) && /^(?:0|[1-9]\d*)$/.test(token) && BigInt(token) <= 18446744073709551615n;
  });
}
export function isReferenceLogFilterEnvelope(v: unknown): v is ReferenceLogFilterEnvelope {
  return object(v) && keys(v, ["family", "operation", "http_status", "phase", "source_format", "source", "result", "source_unchanged", "activity_key", "filtered_log_json", "filtered_xes", "xes_error", "error", ...((["repetition", "paths", "path_performance"].includes(String(v.operation)) || String(v.operation).startsWith("variants")) ? ["filter_attribute_key"] : v.operation === "between" ? ["filter_attribute_key", "case_id_key", "full_result_json"] : String(v.operation).startsWith("time_") ? ["timestamp_key", "time_attribute_key", "source_log_json", "source_after_log_json"] : [])])
    && v.family === "REFERENCE_LOG_FILTER" && (v.operation === null || typeof v.operation === "string")
    && (v.http_status === 200 || v.http_status === 400) && typeof v.phase === "string"
    && (v.source_format === null || typeof v.source_format === "string") && counts(v.source) && counts(v.result)
    && (v.source_unchanged === null || typeof v.source_unchanged === "boolean")
    && (!(["repetition", "between", "paths", "path_performance"].includes(String(v.operation)) || String(v.operation).startsWith("variants")) || nullableString(v.filter_attribute_key))
    && (v.operation !== "between" || nullableString(v.case_id_key) && nullableString(v.full_result_json))
    && (!String(v.operation).startsWith("time_") || isOriginalEmbeddingFilterNativeString(v.timestamp_key) && isOriginalEmbeddingFilterNativeString(v.time_attribute_key) && nullableString(v.source_log_json) && nullableString(v.source_after_log_json))
    && ["activity_key", "filtered_log_json", "filtered_xes", "xes_error", "error"].every(key => nullableString(v[key]));
}
/** Fields remain literal; native validation owns JSON arrays and scalar tokens. */
export function referenceLogFilterFields(form: ReferenceLogFilterForm): Array<[string, string]> {
  if (!(REFERENCE_LOG_FILTER_OPERATIONS as readonly string[]).includes(form.operation)) throw new Error("Choose a log filter.");
  if (!["auto", "csv", "xes", "native_log"].includes(form.sourceFormat)) throw new Error("Choose a source format.");
  if (!["default", "custom"].includes(form.activityKeyMode)) throw new Error("Choose an activity key mode.");
  const fields: Array<[string, string]> = [["operation", form.operation], ["source_format", form.sourceFormat]];
  if (form.activityKeyMode === "custom" && !form.operation.startsWith("case_")) fields.push(["activity_key", form.activityKey]);
  if (form.operation.startsWith("variants")) {
    if (form.operation === "variants") {
      if (!["list", "tuple"].includes(form.variantsSequenceKind)) throw new Error("Choose list or tuple sequences.");
      fields.push(["admitted_json", form.variantsJson], ["sequence_kind", form.variantsSequenceKind]);
    } else fields.push(["criterion", form.variantsCriterion]);
    if (form.operation !== "variants_cumulative") fields.push(["positive", String(form.positive)]);
  } else if (form.operation.startsWith("time_")) {
    if (![form.timestampKeyMode, form.timeAttributeKeyMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal timestamp attributes.");
    fields.push(["lower", form.timeLower], ["upper", form.timeUpper]);
    if (form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
    if (form.operation === "time_attribute") {
      fields.push(["time_target_json", form.timeTargetJson]);
      if (form.timeAttributeKeyMode === "custom") fields.push(["time_attribute_key", form.timeAttributeKey]);
    }
    if (form.operation === "time_starting" || form.operation === "time_completing") fields.push(["positive", String(form.positive)]);
  } else if (form.operation === "paths" || form.operation === "path_performance") {
    fields.push(["positive", String(form.positive)], [form.operation === "paths" ? "paths_json" : "path_json", form.operation === "paths" ? form.pathsJson : form.pathJson]);
    if (form.operation === "path_performance") {
      if (!["default", "custom"].includes(form.timestampKeyMode)) throw new Error("Choose a timestamp key mode.");
      fields.push(["minimum", form.minPathSeconds], ["maximum", form.maxPathSeconds]);
      if (form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
    }
  } else if (form.operation === "case_count") fields.push(["max_cases", form.maxCases]);
  else if (form.operation === "case_size") fields.push(["minimum", form.minCaseSize], ["maximum", form.maxCaseSize]);
  else if (form.operation === "case_performance") {
    if (!["default", "custom"].includes(form.timestampKeyMode)) throw new Error("Choose a timestamp key mode.");
    fields.push(["minimum", form.minCaseSeconds], ["maximum", form.maxCaseSeconds]);
    if (form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
  } else if (form.operation === "prefix" || form.operation === "suffix") {
    if (!["first", "last"].includes(form.occurrence)) throw new Error("Choose the first or last occurrence.");
    fields.push(["activity", form.activity], ["strict", String(form.strict)], ["occurrence", form.occurrence]);
  } else if (form.operation === "between") {
    if (![form.betweenCaseKeyMode, form.betweenSeparatorMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal between controls.");
    fields.push(["act1_json", form.betweenStartJson], ["act2_json", form.betweenEndJson]);
    if (form.betweenCaseKeyMode === "custom") fields.push(["case_id_key", form.betweenCaseKey]);
    if (form.betweenSeparatorMode === "custom") fields.push(["subcase_concat_str", form.betweenSeparator]);
  } else if (form.operation === "repetition") {
    fields.push(["target_json", form.repetitionTargetJson], ["min_rep", form.minRep], ["max_rep", form.maxRep]);
  } else {
    fields.push(["positive", String(form.positive)]);
    if (form.operation === "rework") fields.push(["activity", form.activity], ["min_occurrences", form.minOccurrences]);
    else if (form.operation === "traces") fields.push(["patterns_json", form.patternsJson]);
    else fields.push(["admitted_json", form.admittedJson]);
  }
  if (form.csvOptions && (form.sourceFormat === "auto" || form.sourceFormat === "csv")) {
    for (const [key] of REFERENCE_LOG_FILTER_CSV_FIELDS) fields.push([key, form.csv[key]]);
    fields.push(["trim_fields", String(form.trimFields)]);
  }
  return fields;
}
export async function requestReferenceLogFilter(sourceFile: File, form: ReferenceLogFilterForm, signal?: AbortSignal): Promise<ReferenceLogFilterRun> {
  signal?.throwIfAborted();
  if (!(sourceFile instanceof File)) throw new Error("Choose an actual CSV, XES, or native log file.");
  const fields = referenceLogFilterFields(form), body = new FormData();
  body.append("source_file", sourceFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await fetch("/api/reference-log-filter", {method: "POST", body, signal});
  signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`The log filter returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try {new TextDecoder("utf-8", {fatal: true}).decode(responseBytes);} catch {throw new Error("The log filter response is not safe JSON UTF-8.");}
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-log-filter", fields, reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isReferenceLogFilterEnvelope(value) || value.http_status !== response.status) throw new Error("The log filter response has an invalid native envelope.");
  if (value.operation !== form.operation && !(value.http_status === 400 && value.operation === null)) throw new Error("The log filter response belongs to another operation.");
  return {value, sourceFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export const referenceLogFilterStringBytes = originalEmbeddingFilterStringBytes;
export const referenceLogFilterStringText = originalEmbeddingFilterStringText;
export function referenceLogFilterSourceFile(run: ReferenceLogFilterRun, after = false): File {
  const value = after ? run.value.source_after_log_json : run.value.source_log_json;
  if (value === undefined || value === null) throw new Error("The complete parsed source artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], after ? "timestamp-source-after.json" : "timestamp-source-before.json", {type: "application/json"});
}
export function referenceLogFilterArtifactFile(run: ReferenceLogFilterRun, kind: "json" | "xes"): File {
  const value = kind === "json" ? run.value.filtered_log_json : run.value.filtered_xes;
  if (run.value.http_status !== 200 || run.value.error !== null || value === null || kind === "xes" && run.value.xes_error !== null) throw new Error(`The filtered ${kind === "xes" ? "XES" : "native JSON"} is unavailable.`);
  return new File([referenceLogFilterStringBytes(value)], `filtered-${run.value.operation ?? "log"}.${kind}`, {type: kind === "xes" ? "application/xml" : "application/json"});
}
export function referenceLogFilterFullBetweenFile(run: ReferenceLogFilterRun): File {
  const value = run.value.full_result_json;
  if (run.value.operation !== "between" || run.value.http_status !== 200 || run.value.error !== null || value == null) throw new Error("The full between result is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], "filtered-between-full-result.json", {type: "application/json"});
}
export type ReferenceLogFilterOpenLog = (file: File, activityKey?: string) => void;
export function referenceLogFilterOpenActivityKey(run: ReferenceLogFilterRun): string | undefined {
  const key = run.value.activity_key;
  if (key === null) return undefined;
  try {return new TextDecoder("utf-8", {fatal: true, ignoreBOM: true}).decode(referenceLogFilterStringBytes(key));}
  catch {return undefined;}
}
/** Carry the actual exported File and its literal selector into the loader. */
export function openReferenceLogFilter(run: ReferenceLogFilterRun, onOpenLog: ReferenceLogFilterOpenLog): void {
  const file = referenceLogFilterArtifactFile(run, "xes"), key = referenceLogFilterOpenActivityKey(run);
  if (key === undefined) onOpenLog(file);
  else onOpenLog(file, key);
}
export interface ReferenceLogFilterIdentity {source: File | null; form: ReferenceLogFilterForm;}
export class ReferenceLogFilterRequestGate {
  private generation = 0;
  issue(value: ReferenceLogFilterIdentity) {return {generation: ++this.generation, source: value.source, form: JSON.stringify(value.form)};}
  invalidate() {++this.generation;}
  accepts(ticket: ReturnType<ReferenceLogFilterRequestGate["issue"]>, value: ReferenceLogFilterIdentity, signal?: AbortSignal) {
    return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.form === JSON.stringify(value.form);
  }
}
export function referenceLogFilterPickedFile(input: {files: ArrayLike<File> | null; value: string}): File | null {
  const file = input.files?.[0] ?? null; input.value = ""; return file;
}
export function referenceLogFilterCountText(value: ReferenceLogFilterCounts, key: "traces" | "events"): string {return String(nativeCell(value, key));}
export function referenceLogFilterResponseFile(run: ReferenceLogFilterRun): File {return new File([run.responseBytes], "log-filter-response.json", {type: "application/json"});}
export function referenceLogFilterSourceExample(): File {return new File([nativeStringBytes("case_id,activity,timestamp\ncase-1,A,2026-01-01T00:00:00Z\ncase-1,B,2026-01-01T00:00:01Z\ncase-1,A,2026-01-01T00:00:02Z\ncase-2,B,2026-01-01T00:00:03Z\n")], "log-filter-example.csv", {type: "text/csv"});}
