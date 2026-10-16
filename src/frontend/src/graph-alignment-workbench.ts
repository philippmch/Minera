import {appendNativeMultipartField} from "./native-multipart";
import { diagnosticCsv, nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "./token-replay-workbench";
import {WORKFLOW_INPUT_XES_VARIANTS, isWorkflowInputNativeNet, isWorkflowInputTable, workflowInputNativeBytes, type WorkflowInputTable} from "./workflow-input-workbench";

export const GRAPH_ALIGNMENT_MODES = ["DFG_CLASSIC", "RECOMPOS_MAXIMAL"] as const;
export type GraphAlignmentMode = typeof GRAPH_ALIGNMENT_MODES[number];
export const GRAPH_ALIGNMENT_INPUT_SHAPES = ["log", "trace", "words"] as const;
export const GRAPH_ALIGNMENT_INPUT_FORMATS = ["auto", "table", "trace_words"] as const;
export const GRAPH_ALIGNMENT_MODEL_FORMATS = ["auto", "dfg_json", "dfg", "pnml"] as const;
export const GRAPH_ALIGNMENT_PROJECTIONS = ["none", "log", "metrics", "both"] as const;
export const GRAPH_ALIGNMENT_PNML_CONTROLS = [
  ["pnml_infer_initial_marking", "Infer initial marking"], ["pnml_infer_final_marking", "Infer final marking"],
  ["pnml_require_workflow_boundaries", "Require workflow boundaries"], ["pnml_import_stochastic_map", "Import stochastic transition metadata"],
  ["pnml_public_marking_semantics", "Use public marking semantics"],
] as const;
export interface GraphAlignmentForm {
  mode: GraphAlignmentMode; inputFormat: typeof GRAPH_ALIGNMENT_INPUT_FORMATS[number]; inputShape: typeof GRAPH_ALIGNMENT_INPUT_SHAPES[number];
  modelFormat: typeof GRAPH_ALIGNMENT_MODEL_FORMATS[number]; projection: typeof GRAPH_ALIGNMENT_PROJECTIONS[number]; traceIndex: string;
  fields: Record<string, string>; importFields: Record<string, string>; pnmlFields: Record<string, string>; explicitEmptySelectors: Record<string, boolean>;
}
export const defaultGraphAlignmentForm = (mode: GraphAlignmentMode = "DFG_CLASSIC"): GraphAlignmentForm => ({mode, inputFormat: "auto", inputShape: "log", modelFormat: mode === "DFG_CLASSIC" ? "auto" : "pnml", projection: "none", traceIndex: "", fields: {}, importFields: {}, pnmlFields: {}, explicitEmptySelectors: {}});
export interface GraphAlignmentControl {key: string; label: string; kind: "selector" | "integer" | "number" | "boolean" | "map" | "marking";}
export const GRAPH_ALIGNMENT_CONTROLS: Record<GraphAlignmentMode, readonly GraphAlignmentControl[]> = {
  DFG_CLASSIC: [
    {key: "alignment_case_id_attribute", label: "Typed-table case attribute", kind: "selector"},
    {key: "dfg_activity_attribute", label: "DFG activity attribute", kind: "selector"},
    {key: "dfg_timestamp_attribute", label: "Projected-log timestamp attribute", kind: "selector"},
    {key: "dfg_max_states", label: "DFG visited-state limit (0 for unlimited)", kind: "integer"},
    {key: "dfg_max_time_ms", label: "DFG time limit in milliseconds (0 for unlimited)", kind: "integer"},
    {key: "dfg_sync_cost_json", label: "Synchronous costs by activity (JSON object)", kind: "map"},
    {key: "dfg_model_move_cost_json", label: "Model-move costs by activity (JSON object)", kind: "map"},
    {key: "dfg_log_move_cost_json", label: "Log-move costs by activity (JSON object)", kind: "map"},
    {key: "dfg_internal_log_move_cost_json", label: "Internal log-move costs by activity (JSON object)", kind: "map"},
  ],
  RECOMPOS_MAXIMAL: [
    {key: "alignment_case_id_attribute", label: "Typed-table case attribute", kind: "selector"},
    {key: "activity_attribute", label: "Decomposed alignment activity attribute", kind: "selector"},
    {key: "threshold_border_agreement", label: "Border agreement threshold", kind: "number"},
    {key: "max_time_trace", label: "Per-trace time limit in seconds (0 for unlimited)", kind: "number"},
    {key: "max_time_total", label: "Overall time limit in seconds (0 for unlimited)", kind: "number"},
    {key: "max_states", label: "Component search state limit", kind: "integer"},
    {key: "max_total_states", label: "Total search state limit", kind: "integer"},
    {key: "use_cache", label: "Reuse component alignments", kind: "boolean"},
    {key: "clear_cache", label: "Clear component alignment cache", kind: "boolean"},
    {key: "log_move_cost", label: "Log-move cost", kind: "number"},
    {key: "model_move_cost", label: "Visible model-move cost", kind: "number"},
    {key: "tau_model_cost", label: "Silent model-move cost", kind: "number"},
    {key: "sync_cost", label: "Synchronous move cost", kind: "number"},
    {key: "activity_log_costs_json", label: "Log costs by activity (JSON object)", kind: "map"},
    {key: "activity_model_costs_json", label: "Model costs by activity (JSON object)", kind: "map"},
    {key: "activity_sync_costs_json", label: "Synchronous costs by activity (JSON object)", kind: "map"},
    {key: "initial_marking_json", label: "Initial marking (JSON integer array in model place order)", kind: "marking"},
    {key: "final_marking_json", label: "Final marking (JSON integer array in model place order)", kind: "marking"},
  ],
};
export type GraphAlignmentDouble = number | "NaN" | "INF" | "-INF";
export interface DfgNativeAlignmentMove {log: string; model: string;}
export interface DfgNativeTraceAlignment {
  computed: boolean; alignment: DfgNativeAlignmentMove[];
  cost: GraphAlignmentDouble; internal_cost: GraphAlignmentDouble; fitness: GraphAlignmentDouble; best_worst_cost: GraphAlignmentDouble;
  visited_states: number; closed: number; closed_states: number;
}
export interface DfgNativeLogAlignment {
  alignments: DfgNativeTraceAlignment[]; unique_variants: number;
  projection: null | {dfg: Array<{source: string; target: string; sync: number; mm: number}>; activities: Array<{activity: string; sync: number; mm: number; lm: number}>};
}
export interface DecomposedNativeAlignmentMove {log: string | null; model: string | null; transition: number;}
export interface DecomposedNativeTraceAlignment {
  activities: string[]; count: number; complete: boolean; timed_out: boolean; exhausted: boolean;
  cost: GraphAlignmentDouble; fitness: GraphAlignmentDouble; best_worst_cost: GraphAlignmentDouble; alignment: DecomposedNativeAlignmentMove[];
}
export interface DecomposedNativeAlignment {
  complete: boolean; timed_out: boolean; exhausted: boolean; fitness: GraphAlignmentDouble; best_worst_cost: GraphAlignmentDouble;
  trace_count: number; total_cost: GraphAlignmentDouble;
  diagnostics: {decompositions: number; components: number; merges: number; cache_hits: number; cache_misses: number;
    border_disagreements: number; agreement_iterations: number; visited_states: number; queued_states: number; traversed_arcs: number; whole_net_fallback: boolean; note: string};
  traces: DecomposedNativeTraceAlignment[];
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const strings = (value: unknown): value is string[] => Array.isArray(value) && value.every(item => typeof item === "string");
const unsigned = (holder: object, key: string | number): bigint | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const literal = String(nativeCell(holder, key)); if (!/^(?:0|[1-9]\d*)$/.test(literal)) return undefined;
  const parsed = BigInt(literal); return parsed <= 18446744073709551615n ? parsed : undefined;
};
const nativeDouble = (value: unknown): value is GraphAlignmentDouble => typeof value === "number" && Number.isFinite(value) || value === "NaN" || value === "INF" || value === "-INF";
const flags = (value: Record<string, unknown>, keys: readonly string[]) => keys.every(key => typeof value[key] === "boolean");
const doubles = (value: Record<string, unknown>, keys: readonly string[]) => keys.every(key => nativeDouble(value[key]));
/** Return literal selectors separately for the route's exact-selector carrier. */
export function graphAlignmentControlFields(mode: GraphAlignmentMode, values: Record<string, string>, explicitEmpty: Record<string, boolean> = {}): {fields: Array<[string, string]>; selectors: Record<string, string>} {
  if (!GRAPH_ALIGNMENT_MODES.includes(mode)) throw new Error("Select a supported graph alignment mode.");
  const fields: Array<[string, string]> = [], selectors: Record<string, string> = Object.create(null);
  for (const control of GRAPH_ALIGNMENT_CONTROLS[mode]) {
    const value = values[control.key] ?? "";
    if (control.kind === "selector") {if (value !== "" || explicitEmpty[control.key]) selectors[control.key] = value; continue;}
    if (value === "") continue;
    if (control.kind === "integer" && (!/^\d+$/.test(value.trim()) || BigInt(value.trim()) > 18446744073709551615n)) throw new Error(`${control.label} must be an unsigned integer.`);
    if (control.kind === "number" && (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(value.trim()) || !Number.isFinite(Number(value)) || Number(value) < 0)) throw new Error(`${control.label} must be a finite non-negative number.`);
    if (control.kind === "boolean" && !["true", "false"].includes(value)) throw new Error(`${control.label} must be true or false.`);
    if (control.kind === "map" || control.kind === "marking") {
      let parsed: unknown; try {parsed = parseNativeJson(value);} catch {throw new Error(`${control.label} must be valid JSON.`);}
      if (control.kind === "map" ? !object(parsed) || !Object.values(parsed).every(cost => typeof cost === "number" && Number.isFinite(cost) && cost >= 0) : !Array.isArray(parsed) || !parsed.every((_, i) => unsigned(parsed, i) !== undefined)) throw new Error(`${control.label} must be a JSON ${control.kind === "map" ? "object of finite non-negative costs" : "array of unsigned integers"}.`);
    }
    fields.push([control.key, value]);
  }
  return {fields, selectors};
}
export function isDfgNativeTraceAlignment(value: unknown): value is DfgNativeTraceAlignment {
  return object(value) && typeof value.computed === "boolean" && Array.isArray(value.alignment) && value.alignment.every(move => object(move) && typeof move.log === "string" && typeof move.model === "string") &&
    doubles(value, ["cost", "internal_cost", "fitness", "best_worst_cost"]) && ["visited_states", "closed", "closed_states"].every(key => unsigned(value, key) !== undefined);
}
export function isDfgNativeLogAlignment(value: unknown): value is DfgNativeLogAlignment {
  if (!object(value) || !Array.isArray(value.alignments) || !value.alignments.every(isDfgNativeTraceAlignment) || unsigned(value, "unique_variants") === undefined || unsigned(value, "unique_variants")! > BigInt(value.alignments.length)) return false;
  if (value.projection === null) return true;
  const projection = value.projection;
  return object(projection) && Array.isArray(projection.dfg) && projection.dfg.every(edge => object(edge) && typeof edge.source === "string" && typeof edge.target === "string" && ["sync", "mm"].every(key => unsigned(edge, key) !== undefined)) &&
    Array.isArray(projection.activities) && projection.activities.every(activity => object(activity) && typeof activity.activity === "string" && ["sync", "mm", "lm"].every(key => unsigned(activity, key) !== undefined));
}
export function isDecomposedNativeTraceAlignment(value: unknown): value is DecomposedNativeTraceAlignment {
  return object(value) && strings(value.activities) && unsigned(value, "count") !== undefined && flags(value, ["complete", "timed_out", "exhausted"]) && doubles(value, ["cost", "fitness", "best_worst_cost"]) &&
    Array.isArray(value.alignment) && value.alignment.every(move => object(move) && (move.log === null || typeof move.log === "string") && (move.model === null || typeof move.model === "string") && unsigned(move, "transition") !== undefined);
}
export function isDecomposedNativeAlignment(value: unknown): value is DecomposedNativeAlignment {
  if (!object(value) || !flags(value, ["complete", "timed_out", "exhausted"]) || !doubles(value, ["fitness", "best_worst_cost", "total_cost"]) || unsigned(value, "trace_count") === undefined || !Array.isArray(value.traces) || !value.traces.every(isDecomposedNativeTraceAlignment)) return false;
  const diagnostics = value.diagnostics;
  return object(diagnostics) && ["decompositions", "components", "merges", "cache_hits", "cache_misses", "border_disagreements", "agreement_iterations", "visited_states", "queued_states", "traversed_arcs"].every(key => unsigned(diagnostics, key) !== undefined) && typeof diagnostics.whole_net_fallback === "boolean" && typeof diagnostics.note === "string";
}
/** Every returned native move is available independently of the selected table page. */
export function graphAlignmentNativeTables(value: object, mode: GraphAlignmentMode, result: GraphAlignmentNativeResult | null): DiagnosticTable[] {
  const tables = nativeObjectTables(value), rows: unknown[][] = [], records: unknown[] = [];
  const traces = result === null ? [] : mode === "RECOMPOS_MAXIMAL" ? "traces" in result ? result.traces : [result as DecomposedNativeTraceAlignment] : "alignments" in result ? result.alignments : [result as DfgNativeTraceAlignment];
  traces.forEach((trace, trace_index) => trace.alignment.forEach((move, move_index) => {
    rows.push([trace_index, move_index, move.log, move.model, "transition" in move ? nativeCell(move, "transition") : undefined, move]);
    records.push({trace_index, move_index, move});
  }));
  tables.push({id: "complete_alignment_moves", title: "Every complete alignment move", columns: ["trace_index", "move_index", "log", "model", "transition", "complete_move"], rows, records});
  return tables;
}
export function graphAlignmentAllTablesCsv(tables: DiagnosticTable[]): string {
  const rows: unknown[][] = [];
  for (const table of tables) {
    if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined]));
    table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));
  }
  return diagnosticCsv({id: "all_graph_alignment_tables", title: "Every complete graph alignment table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export class GraphAlignmentRequestGate {
  private generation = 0;
  issue(): number {return ++this.generation;}
  invalidate(): void {++this.generation;}
  accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}
}
export const GRAPH_ALIGNMENT_TRACE_EXAMPLE = nativeJson(["A", "", "A", "東京,blue;line\nnext", ">>"], 2);
export const GRAPH_ALIGNMENT_DFG_EXAMPLE = '{\n  "kind": "clean",\n  "dfg": [{"source":"A","target":"B","count":1}],\n  "start_activities": {"A":1},\n  "end_activities": {"B":1}\n}';
export type GraphAlignmentNativeResult = DfgNativeTraceAlignment | DfgNativeLogAlignment | DecomposedNativeAlignment | DecomposedNativeTraceAlignment;
export interface GraphAlignmentWorkbench {
  family: "GRAPH_ALIGNMENT_WORKBENCH"; mode: GraphAlignmentMode; input_shape: GraphAlignmentForm["inputShape"]; result_shape: "trace" | "log";
  executed: boolean; error: null | {type: string; message: string};
  input_context: {format: GraphAlignmentForm["inputFormat"]; detected_format: string; dataset: string; original_input_bytes: number;
    input_event_count: number; input_case_count: number | null; checked_event_count: number | null; checked_case_count: number | null;
    origin: string; trace_index: number | null; activity_attribute: string; timestamp_attribute: string | null; case_id_attribute: string};
  model_context: {format: GraphAlignmentForm["modelFormat"]; detected_format: string; dataset: string; original_input_bytes: number;
    dfg: Record<string, unknown> | null; document: Record<string, unknown> | null; native_net: Record<string, unknown> | null;
    markings: null | {initial_origin: "pnml" | "override" | "native_default"; final_origin: "pnml" | "override" | "native_default";
      options_initial_marking: number[]; options_final_marking: number[]; effective_initial_marking: number[]; effective_final_marking: number[]}};
  input_table: WorkflowInputTable | null; input_trace_words: string[] | null; input_log: Record<string, unknown> | null;
  result: GraphAlignmentNativeResult | null; projection: DfgNativeLogAlignment["projection"];
  projected_log: Record<string, unknown> | null; projected_xes: string | null; projection_error: string | null;
  model_export: string | null; model_export_format: "dfg" | "pnml"; model_export_error: string | null;
}
export function graphAlignmentFields(form: GraphAlignmentForm, name: string): Array<[string, string]> {
  if (!GRAPH_ALIGNMENT_MODES.includes(form.mode) || !GRAPH_ALIGNMENT_INPUT_SHAPES.includes(form.inputShape) || !GRAPH_ALIGNMENT_INPUT_FORMATS.includes(form.inputFormat) || !GRAPH_ALIGNMENT_MODEL_FORMATS.includes(form.modelFormat) || !GRAPH_ALIGNMENT_PROJECTIONS.includes(form.projection)) throw new Error("Select supported alignment inputs, model and projection formats.");
  if (form.inputFormat === "trace_words" ? form.inputShape !== "words" : form.inputShape === "words") throw new Error("Literal trace words require the trace-word format and input shape.");
  if (form.mode === "RECOMPOS_MAXIMAL" ? !["auto", "pnml"].includes(form.modelFormat) : form.modelFormat === "pnml") throw new Error("DFG alignment requires a DFG model; decomposed alignment requires PNML.");
  if (form.projection !== "none" && (form.mode !== "DFG_CLASSIC" || form.inputShape !== "log")) throw new Error("DFG projection requires grouped-log DFG alignment.");
  const fields: Array<[string, string]> = [["mode", form.mode], ["input_format", form.inputFormat], ["input_shape", form.inputShape], ["model_format", form.modelFormat]];
  if (form.mode === "DFG_CLASSIC") fields.push(["projection", form.projection]);
  if (form.inputShape === "trace" && form.traceIndex !== "") {
    if (!/^\d+$/.test(form.traceIndex.trim()) || BigInt(form.traceIndex.trim()) > 18446744073709551615n) throw new Error("Trace index must be an unsigned integer.");
    fields.push(["trace_index", form.traceIndex]);
  }
  if (form.inputFormat === "auto") {
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) {const value = form.importFields[key]; if (value !== undefined && value !== "") fields.push([key, value]);}
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) {const value = form.importFields[key]; if (value === undefined || value === "") continue; if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`); fields.push([key, value]);}
    if (form.importFields.xes_variant) {if (!WORKFLOW_INPUT_XES_VARIANTS.some(value => value === form.importFields.xes_variant)) throw new Error("Select a supported XES importer."); fields.push(["xes_variant", form.importFields.xes_variant]);}
    const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(name) ? "\t" : "");
    if (delimiter) {if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character."); fields.push(["delimiter", delimiter]);}
  }
  if (form.mode === "RECOMPOS_MAXIMAL") for (const [key] of GRAPH_ALIGNMENT_PNML_CONTROLS) {
    const value = form.pnmlFields[key]; if (value === undefined || value === "") continue;
    if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`); fields.push([key, value]);
  }
  const controls = graphAlignmentControlFields(form.mode, form.fields, form.explicitEmptySelectors); fields.push(...controls.fields);
  if (Object.keys(controls.selectors).length) fields.push(["alignment_selectors_json", JSON.stringify(controls.selectors)]);
  return fields;
}
/** Validate the route's native carriers while preserving full opaque log and document snapshots. */
export function isGraphAlignmentWorkbench(value: unknown): value is GraphAlignmentWorkbench {
  if (!object(value) || value.family !== "GRAPH_ALIGNMENT_WORKBENCH" || !GRAPH_ALIGNMENT_MODES.some(mode => value.mode === mode) || !GRAPH_ALIGNMENT_INPUT_SHAPES.some(shape => value.input_shape === shape) || typeof value.executed !== "boolean" || !object(value.input_context) || !object(value.model_context)) return false;
  const input = value.input_context, model = value.model_context;
  if (!GRAPH_ALIGNMENT_INPUT_FORMATS.some(format => input.format === format) || !GRAPH_ALIGNMENT_MODEL_FORMATS.some(format => model.format === format) || ![input, model].every(context => typeof context.dataset === "string" && typeof context.detected_format === "string" && unsigned(context, "original_input_bytes") !== undefined) || unsigned(input, "input_event_count") === undefined || !["input_case_count", "checked_event_count", "checked_case_count", "trace_index"].every(key => input[key] === null || unsigned(input, key) !== undefined) || !["origin", "activity_attribute", "case_id_attribute"].every(key => typeof input[key] === "string") || !(input.timestamp_attribute === null || typeof input.timestamp_attribute === "string")) return false;
  if (input.format === "trace_words" ? value.input_shape !== "words" : value.input_shape === "words") return false;
  const resultShape = value.mode === "DFG_CLASSIC" ? value.input_shape === "log" ? "log" : "trace" : value.input_shape === "words" ? "trace" : "log";
  if (value.result_shape !== resultShape || value.model_export_format !== (value.mode === "DFG_CLASSIC" ? "dfg" : "pnml")) return false;
  if (value.input_table !== null && !isWorkflowInputTable(value.input_table) || value.input_trace_words !== null && !strings(value.input_trace_words)) return false;
  if (!["input_log", "projected_log"].every(key => value[key] === null || object(value[key])) || !["dfg", "document"].every(key => model[key] === null || object(model[key])) || model.native_net !== null && !isWorkflowInputNativeNet(model.native_net)) return false;
  if (model.markings !== null) {
    const markings = model.markings;
    if (!object(markings) || !["initial_origin", "final_origin"].every(key => ["pnml", "override", "native_default"].includes(String(markings[key]))) || !["options_initial_marking", "options_final_marking", "effective_initial_marking", "effective_final_marking"].every(key => Array.isArray(markings[key]) && (markings[key] as unknown[]).every((_, i) => unsigned(markings[key] as object, i) !== undefined))) return false;
  }
  if (!["projected_xes", "projection_error", "model_export", "model_export_error"].every(key => value[key] === null || typeof value[key] === "string")) return false;
  if (value.projection !== null && !isDfgNativeLogAlignment({alignments: [], unique_variants: 0, projection: value.projection})) return false;
  if (!value.executed) return object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string" && value.result === null;
  if (value.error !== null) return false;
  return value.mode === "DFG_CLASSIC" ? resultShape === "log" ? isDfgNativeLogAlignment(value.result) : isDfgNativeTraceAlignment(value.result) : resultShape === "log" ? isDecomposedNativeAlignment(value.result) : isDecomposedNativeTraceAlignment(value.result);
}
export interface GraphAlignmentRun {
  value: GraphAlignmentWorkbench; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; http_status: number;
  inputs: {input: File; model: File; input_sha256: string; model_sha256: string};
}
export function graphAlignmentTables(value: GraphAlignmentWorkbench, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = graphAlignmentNativeTables(value, value.mode, value.result), submitted = fields ?? nativeResponseRequest(value)?.fields;
  if (submitted) tables.push({id: "request", title: "Exact submitted alignment options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))});
  return tables;
}
async function fileHash(file: File): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");}
export async function runGraphAlignmentWorkbench(input: File, model: File, form: GraphAlignmentForm, signal?: AbortSignal): Promise<GraphAlignmentRun> {
  const fields = graphAlignmentFields(form, input.name), [input_sha256, model_sha256] = await Promise.all([fileHash(input), fileHash(model)]); signal?.throwIfAborted();
  const body = new FormData(); body.append("input_file", input); body.append("model_file", model); fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value));
  const path = "/api/align-graph-workbench", response = await fetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!isGraphAlignmentWorkbench(value)) {if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Graph alignment failed (${response.status}).`); throw new Error("The server did not return a valid native graph alignment envelope.");}
  if (value.mode !== form.mode || value.input_shape !== form.inputShape || value.input_context.format !== form.inputFormat || value.model_context.format !== form.modelFormat || value.input_context.dataset !== input.name || value.model_context.dataset !== model.name || unsigned(value.input_context, "original_input_bytes") !== BigInt(input.size) || unsigned(value.model_context, "original_input_bytes") !== BigInt(model.size)) throw new Error("The alignment response does not match the submitted input, model and alignment mode.");
  if (value.executed) {
    const controls = graphAlignmentControlFields(form.mode, form.fields, form.explicitEmptySelectors);
    for (const [key, expected] of Object.entries(controls.selectors)) {
      const contextKey = key === "alignment_case_id_attribute" ? "case_id_attribute" : key === "dfg_timestamp_attribute" ? "timestamp_attribute" : "activity_attribute";
      if (value.input_context[contextKey] !== expected) throw new Error("The alignment response does not match the submitted literal selectors.");
    }
    if (form.inputShape === "trace" && form.traceIndex !== "" && unsigned(value.input_context, "trace_index") !== BigInt(form.traceIndex.trim())) throw new Error("The alignment response does not match the selected original trace.");
  }
  if (!response.ok && value.executed) throw new Error(`The server returned an executed alignment with failure status ${response.status}.`);
  return {value, raw: nativeResponseJson(value), fields, tables: graphAlignmentTables(value, fields), http_status: response.status, inputs: {input, model, input_sha256, model_sha256}};
}
export function graphAlignmentReport(run: GraphAlignmentRun): string {
  const metadata = (file: File, sha256: string) => ({name: file.name, size: file.size, last_modified: file.lastModified, sha256});
  return nativeJson({family: "GRAPH_ALIGNMENT_WORKBENCH_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/align-graph-workbench", fields: run.fields}, inputs: {input: metadata(run.inputs.input, run.inputs.input_sha256), model: metadata(run.inputs.model, run.inputs.model_sha256)}, native_result: run.value}, 2);
}
export function graphAlignmentArtifactFile(run: GraphAlignmentRun, kind: "model" | "projection"): File {
  const text = kind === "model" ? run.value.model_export : run.value.projected_xes;
  if (text === null) throw new Error((kind === "model" ? run.value.model_export_error : run.value.projection_error) ?? `This run did not return a ${kind} export.`);
  const extension = kind === "model" ? run.value.model_export_format : "xes";
  return new File([workflowInputNativeBytes(text)], `minera-${run.value.mode.toLowerCase()}-${kind}.${extension}`, {type: extension === "dfg" ? "text/plain" : "application/xml"});
}
export function graphAlignmentSummary(value: GraphAlignmentWorkbench): string {
  if (!value.executed) return `Alignment failed: ${value.error!.type}: ${value.error!.message}. Complete input and model snapshots remain available.`;
  const result = value.result!;
  if (value.mode === "DFG_CLASSIC") {
    if ("alignments" in result) return `${result.alignments.filter(trace => trace.computed).length}/${result.alignments.length} computed DFG trace alignments · ${nativeCell(result, "unique_variants")} searched variants. Per-trace scores and projections remain in the complete tables.`;
    const trace = result as DfgNativeTraceAlignment;
    return trace.computed ? `DFG trace alignment cost ${nativeCell(trace, "cost")} · internal cost ${nativeCell(trace, "internal_cost")}. Raw trace fitness and best/worst cost are undefined.` : "DFG trace alignment was not computed. Native stored values remain in the complete tables.";
  }
  const trace = result as DecomposedNativeAlignment | DecomposedNativeTraceAlignment;
  return trace.complete ? `Complete decomposed alignment · fitness ${nativeCell(trace, "fitness")} · ${"total_cost" in trace ? "total cost " + nativeCell(trace, "total_cost") : "cost " + nativeCell(trace, "cost")}.`
    : `Incomplete decomposed alignment${trace.timed_out ? " · time limit reached" : ""}${trace.exhausted ? " · search bound reached" : ""}. Native scores, moves and diagnostics remain inspectable.`;
}
export function graphAlignmentSucceeded(value: GraphAlignmentWorkbench): boolean {
  if (!value.executed || !value.result) return false;
  return value.mode === "DFG_CLASSIC" ? "alignments" in value.result ? value.result.alignments.every(trace => trace.computed) : (value.result as DfgNativeTraceAlignment).computed : (value.result as DecomposedNativeAlignment | DecomposedNativeTraceAlignment).complete;
}
