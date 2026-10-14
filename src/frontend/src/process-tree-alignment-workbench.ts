import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {nativeCell, nativeJson} from "./native-json";
import type {ModelGraph} from "./model-graph";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "./token-replay-workbench";
import {WORKFLOW_INPUT_XES_VARIANTS, isWorkflowInputNativeNet, isWorkflowInputTable, workflowInputNativeBytes, type WorkflowInputTable} from "./workflow-input-workbench";

export const TREE_ALIGNMENT_VARIANTS = ["SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP", "APPROXIMATED_ORIGINAL"] as const;
export type TreeAlignmentVariant = typeof TREE_ALIGNMENT_VARIANTS[number];
export const TREE_ALIGNMENT_INPUT_FORMATS = ["auto", "table", "trace_words"] as const;
export const TREE_ALIGNMENT_INPUT_SHAPES = ["log", "trace", "words"] as const;
export const TREE_ALIGNMENT_MODEL_FORMATS = ["auto", "ptml", "tree_json"] as const;
export const TREE_ALIGNMENT_CONVERSIONS = ["standard", "transition_bordered"] as const;
export interface ProcessTreeAlignmentForm {
  variant: TreeAlignmentVariant; inputFormat: typeof TREE_ALIGNMENT_INPUT_FORMATS[number]; inputShape: typeof TREE_ALIGNMENT_INPUT_SHAPES[number];
  modelFormat: typeof TREE_ALIGNMENT_MODEL_FORMATS[number]; conversion: typeof TREE_ALIGNMENT_CONVERSIONS[number]; traceIndex: string;
  fields: Record<string, string>; importFields: Record<string, string>; explicitEmptySelectors: Record<string, boolean>;
}
export const defaultProcessTreeAlignmentForm = (): ProcessTreeAlignmentForm => ({variant: "SEARCH_GRAPH_PT", inputFormat: "auto", inputShape: "log", modelFormat: "auto", conversion: "standard", traceIndex: "", fields: {}, importFields: {}, explicitEmptySelectors: {}});
export const TREE_ALIGNMENT_CONTROLS = [
  {key: "alignment_case_id_attribute", label: "Typed-table case attribute", kind: "selector"},
  {key: "pt_alignment_activity_attribute", label: "Alignment activity attribute", kind: "selector"},
  {key: "pt_alignment_max_states", label: "Visited-state limit (0 for unlimited)", kind: "integer"},
  {key: "pt_alignment_max_time_ms", label: "Per-trace time limit in milliseconds (0 for unlimited)", kind: "number"},
  {key: "pt_alignment_max_total_time_ms", label: "Overall time limit in milliseconds (0 for unlimited)", kind: "number"},
  {key: "pt_alignment_max_trace_length", label: "Approximation trace-length threshold (0 for unlimited)", kind: "integer"},
  {key: "pt_alignment_max_process_tree_height", label: "Approximation tree-height threshold (0 for unlimited)", kind: "integer"},
  {key: "pt_alignment_max_trace_time_ms", label: "Approximation subtrace time limit in milliseconds (0 for unlimited)", kind: "number"},
  {key: "pt_alignment_max_per_trace_time_ms", label: "Approximation per-trace time limit in milliseconds (0 for unlimited)", kind: "number"},
  {key: "pt_alignment_use_global_cache", label: "Reuse the global alignment cache", kind: "boolean"},
  {key: "pt_alignment_batch_size", label: "Alignment batch size", kind: "positive"},
  {key: "pt_alignment_clear_cache", label: "Clear the global alignment cache", kind: "boolean"},
] as const;
export type TreeAlignmentControl = typeof TREE_ALIGNMENT_CONTROLS[number];
export type TreeAlignmentDouble = number | "NaN" | "INF" | "-INF";
export const TREE_ALIGNMENT_OPERATORS = ["ACTIVITY", "TAU", "SEQUENCE", "EXCLUSIVE", "INCLUSIVE_CHOICE", "PARALLEL", "INTERLEAVING", "LOOP"] as const;
export interface AlignmentNativeTreeNode {op: typeof TREE_ALIGNMENT_OPERATORS[number] | "UNKNOWN"; op_index: number; activity: number; children: AlignmentNativeTreeNode[];}
export interface AlignmentNativeTree {activities: string[]; root: AlignmentNativeTreeNode;}
export interface NativeTreeAlignmentMove {type: "sync" | "log" | "model" | "UNKNOWN"; type_index: number; log_activity: string; model_activity: string; activity: string;}
export interface NativeTreeTraceAlignment {trace: string[]; moves: NativeTreeAlignmentMove[]; cost: number; fitness: TreeAlignmentDouble; completed: boolean; timed_out: boolean;}
export interface NativeProcessTreeAlignment {
  computed: boolean; completed: boolean; timed_out: boolean; exhausted: boolean; variant: TreeAlignmentVariant | "UNKNOWN"; variant_index: number;
  total_cost: number; fitness: TreeAlignmentDouble;
  diagnostics: {visited_states: number; queued_states: number; traversed_arcs: number; lp_solved: number; traces_aligned: number; message: string};
  traces: NativeTreeTraceAlignment[];
}
export interface NativeTreeAlignmentOptions {
  variant: TreeAlignmentVariant | "UNKNOWN"; variant_index: number; max_states: number; max_time_seconds: TreeAlignmentDouble; max_total_time_seconds: TreeAlignmentDouble;
  max_trace_length: number; max_process_tree_height: number; max_trace_time_seconds: TreeAlignmentDouble; max_per_trace_time_seconds: TreeAlignmentDouble;
  use_global_cache: boolean; batch_size: number; clear_cache: boolean; activity_attribute: string;
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const strings = (value: unknown): value is string[] => Array.isArray(value) && value.every(item => typeof item === "string");
const unsigned = (holder: object, key: string | number): bigint | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const literal = String(nativeCell(holder, key)); if (!/^(?:0|[1-9]\d*)$/.test(literal)) return undefined;
  const value = BigInt(literal); return value <= 18446744073709551615n ? value : undefined;
};
const signedEnum = (holder: object, key: string): number | undefined => {
  if (typeof (holder as Record<string, unknown>)[key] !== "number") return undefined;
  const literal = String(nativeCell(holder, key)); if (!/^-?(?:0|[1-9]\d*)$/.test(literal)) return undefined;
  const value = BigInt(literal); return value >= -2147483648n && value <= 2147483647n ? Number(value) : undefined;
};
const enumeration = (holder: Record<string, unknown>, key: string, indexKey: string, names: readonly string[]): boolean => {
  const index = signedEnum(holder, indexKey); return index !== undefined && holder[key] === (names[index] ?? "UNKNOWN");
};
const nativeDouble = (value: unknown): value is TreeAlignmentDouble => typeof value === "number" && Number.isFinite(value) || value === "NaN" || value === "INF" || value === "-INF";
const flags = (value: Record<string, unknown>, keys: readonly string[]) => keys.every(key => typeof value[key] === "boolean");
/** C strtod also accepts hexadecimal floats; retain their request spelling and binary value. */
export function processTreeAlignmentMilliseconds(raw: string): number | undefined {
  const text = raw.trim();
  if (/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(text)) {const value = Number(text); return Number.isFinite(value) && value >= 0 ? value : undefined;}
  const match = /^([+-]?)0x([0-9a-f]*)(?:\.([0-9a-f]*))?(?:p([+-]?\d+))?$/i.exec(text);
  if (!match || !match[2] && !match[3]) return undefined;
  const significand = BigInt("0x" + (match[2] + (match[3] ?? ""))), negative = match[1] === "-";
  if (!significand) return negative ? -0 : 0;
  if (negative) return undefined;
  const exponent = Number(match[4] ?? 0) - 4 * (match[3]?.length ?? 0), bits = significand.toString(2).length;
  if (bits - 1 + exponent > 1023) return undefined;
  if (bits - 1 + exponent < -1075) return 0;
  const roundedShift = (value: bigint, shift: number) => {
    if (shift <= 0) return value << BigInt(-shift);
    const integral = value >> BigInt(shift), remainder = value - (integral << BigInt(shift)), midpoint = 1n << BigInt(shift - 1);
    return integral + (remainder > midpoint || remainder === midpoint && integral % 2n !== 0n ? 1n : 0n);
  };
  const value = bits - 1 + exponent < -1022 ? Number(roundedShift(significand, -exponent - 1074)) * 2 ** -1074 : Number(roundedShift(significand, bits - 53)) * 2 ** (bits - 53 + exponent);
  return Number.isFinite(value) ? value : undefined;
}
/** Validate stored native fields without imposing miner-only tree shapes or registry bounds. */
export function isAlignmentNativeTree(value: unknown): value is AlignmentNativeTree {
  if (!object(value) || !strings(value.activities)) return false;
  const pending: unknown[] = [value.root], seen = new Set<object>();
  while (pending.length) {
    const node = pending.pop(); if (!object(node) || seen.has(node) || !enumeration(node, "op", "op_index", TREE_ALIGNMENT_OPERATORS) || unsigned(node, "activity") === undefined || !Array.isArray(node.children)) return false;
    seen.add(node); for (const child of node.children) pending.push(child);
  }
  return true;
}
export function isNativeProcessTreeAlignment(value: unknown): value is NativeProcessTreeAlignment {
  if (!object(value) || !flags(value, ["computed", "completed", "timed_out", "exhausted"]) || !enumeration(value, "variant", "variant_index", TREE_ALIGNMENT_VARIANTS) || unsigned(value, "total_cost") === undefined || !nativeDouble(value.fitness) || !object(value.diagnostics) || !Array.isArray(value.traces)) return false;
  if (!["visited_states", "queued_states", "traversed_arcs", "lp_solved", "traces_aligned"].every(key => unsigned(value.diagnostics as object, key) !== undefined) || typeof value.diagnostics.message !== "string") return false;
  return value.traces.every(trace => object(trace) && strings(trace.trace) && unsigned(trace, "cost") !== undefined && nativeDouble(trace.fitness) && flags(trace, ["completed", "timed_out"]) && Array.isArray(trace.moves) && trace.moves.every(move => object(move) && enumeration(move, "type", "type_index", ["sync", "log", "model"]) && ["log_activity", "model_activity", "activity"].every(key => typeof move[key] === "string")));
}
export function isNativeTreeAlignmentOptions(value: unknown): value is NativeTreeAlignmentOptions {
  return object(value) && enumeration(value, "variant", "variant_index", TREE_ALIGNMENT_VARIANTS) && ["max_states", "max_trace_length", "max_process_tree_height", "batch_size"].every(key => unsigned(value, key) !== undefined) &&
    ["max_time_seconds", "max_total_time_seconds", "max_trace_time_seconds", "max_per_trace_time_seconds"].every(key => nativeDouble(value[key])) && flags(value, ["use_global_cache", "clear_cache"]) && typeof value.activity_attribute === "string";
}
export function processTreeAlignmentControlFields(values: Record<string, string>, explicitEmpty: Record<string, boolean> = {}): {fields: Array<[string, string]>; selectors: Record<string, string>} {
  const fields: Array<[string, string]> = [], selectors: Record<string, string> = Object.create(null);
  for (const control of TREE_ALIGNMENT_CONTROLS) {
    const value = values[control.key] ?? "";
    if (control.kind === "selector") {if (value !== "" || explicitEmpty[control.key]) selectors[control.key] = value; continue;}
    if (value === "") continue;
    if ((control.kind === "integer" || control.kind === "positive") && (!/^\d+$/.test(value.trim()) || BigInt(value.trim()) > 18446744073709551615n || control.kind === "positive" && BigInt(value.trim()) === 0n)) throw new Error(`${control.label} must be ${control.kind === "positive" ? "a positive" : "an unsigned"} integer.`);
    if (control.kind === "number" && processTreeAlignmentMilliseconds(value) === undefined) throw new Error(`${control.label} must be a finite non-negative number.`);
    if (control.kind === "boolean" && !["true", "false"].includes(value)) throw new Error(`${control.label} must be true or false.`);
    fields.push([control.key, value]);
  }
  return {fields, selectors};
}
export function processTreeAlignmentNativeTables(value: object, tree: AlignmentNativeTree | null, result: NativeProcessTreeAlignment | null): DiagnosticTable[] {
  const tables = nativeObjectTables(value), rows: unknown[][] = [], records: unknown[] = [];
  result?.traces.forEach((trace, trace_index) => trace.moves.forEach((move, move_index) => {
    rows.push([trace_index, move_index, move.type, nativeCell(move, "type_index"), move.log_activity, move.model_activity, move.activity, move]); records.push({trace_index, move_index, move});
  }));
  tables.push({id: "complete_alignment_moves", title: "Every complete process-tree alignment move", columns: ["trace_index", "move_index", "type", "type_index", "log_activity", "model_activity", "activity", "complete_move"], rows, records});
  if (tree) {
    const nodeRows: unknown[][] = [], nodeRecords: unknown[] = [], pending: Array<{node: AlignmentNativeTreeNode; path: string; parent: string | null}> = [{node: tree.root, path: "root", parent: null}];
    while (pending.length) {
      const {node, path, parent} = pending.pop()!;
      nodeRows.push([path, parent, node.op, nativeCell(node, "op_index"), nativeCell(node, "activity"), node.op === "ACTIVITY" ? tree.activities[node.activity] : undefined, node.children.length, node]); nodeRecords.push({path, parent, node});
      for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path});
    }
    tables.push({id: "complete_tree_nodes", title: "Every original native process-tree node", columns: ["path", "parent", "operator", "operator_index", "stored_activity_id", "activity_label", "child_count", "complete_node"], rows: nodeRows, records: nodeRecords});
  }
  return tables;
}
export function processTreeAlignmentTreeGraph(tree: AlignmentNativeTree): ModelGraph {
  const nodes: ModelGraph["nodes"] = [], edges: ModelGraph["edges"] = [], pending = [{node: tree.root, path: "root", parent: ""}];
  while (pending.length) {
    const {node, path, parent} = pending.pop()!;
    nodes.push({id: path, label: node.op === "ACTIVITY" ? tree.activities[node.activity] ?? `Activity ${nativeCell(node, "activity")}` : node.op === "TAU" ? "τ" : node.op, detail: `Operator ${node.op} (${nativeCell(node, "op_index")}) · stored activity ${nativeCell(node, "activity")}`, start: parent === ""});
    if (parent) edges.push({id: `${parent}>${path}`, source: parent, target: path, label: "", value: 1, directed: true});
    for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path});
  }
  return {nodes, edges};
}
export function processTreeAlignmentAllTablesCsv(tables: DiagnosticTable[]): string {
  const rows: unknown[][] = [];
  for (const table of tables) {
    if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined]));
    table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));
  }
  return diagnosticCsv({id: "all_process_tree_alignment_tables", title: "Every complete process-tree alignment table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export class ProcessTreeAlignmentRequestGate {
  private generation = 0;
  issue(): number {return ++this.generation;}
  invalidate(): void {++this.generation;}
  accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}
}
export const TREE_ALIGNMENT_TRACE_EXAMPLE = nativeJson(["A", "A", "", "東京,blue;line\nnext", ">>"], 2);
export const TREE_ALIGNMENT_MODEL_EXAMPLE = '{\n  "activities":["A","B","unused 東京"],\n  "root":{"op":"SEQUENCE","op_index":2,"activity":18446744073709551615,"children":[\n    {"op":"ACTIVITY","op_index":0,"activity":0,"children":[]},\n    {"op":"ACTIVITY","op_index":0,"activity":1,"children":[]}\n  ]}\n}';
/** Lossless ASCII filename encoding also accepts native lone-surrogate table keys. */
export function processTreeAlignmentTableFileName(id: string): string {
  let encoded = ""; for (let i = 0; i < id.length; ++i) encoded += /^[A-Za-z0-9_.-]$/.test(id[i]) ? id[i] : "%" + id.charCodeAt(i).toString(16).padStart(4, "0"); return encoded;
}
export interface ProcessTreeAlignmentWorkbench {
  family: "PROCESS_TREE_ALIGNMENT_WORKBENCH"; variant: TreeAlignmentVariant; input_shape: ProcessTreeAlignmentForm["inputShape"]; result_shape: "log";
  executed: boolean; error: null | {type: string; message: string};
  input_context: {format: ProcessTreeAlignmentForm["inputFormat"]; detected_format: string; dataset: string; origin: string; original_input_bytes: number; input_event_count: number;
    input_case_count: number | null; checked_event_count: number | null; checked_case_count: number | null; trace_index: number | null; activity_attribute: string; case_id_attribute: string};
  model_context: {format: ProcessTreeAlignmentForm["modelFormat"]; detected_format: string; dataset: string; original_input_bytes: number; native_tree: AlignmentNativeTree};
  options_context: NativeTreeAlignmentOptions; input_before: WorkflowInputTable | null; input_after: WorkflowInputTable | null; input_trace_words: string[] | null;
  input_log: Record<string, unknown> | null; input_log_error: string | null; checked_log: Record<string, unknown> | null; checked_log_error: string | null;
  result: NativeProcessTreeAlignment | null; native_net: Record<string, unknown> | null;
  conversion_context: {variant: ProcessTreeAlignmentForm["conversion"]; computed: boolean; error: string | null};
  source_xes: string | null; source_xes_error: string | null; ptml: string | null; ptml_error: string | null;
  pnml: string | null; pnml_error: string | null; bpmn: string | null; bpmn_error: string | null;
}
const countLiteral = (holder: object, key: string): boolean => typeof (holder as Record<string, unknown>)[key] === "number" && /^(?:0|[1-9]\d*)$/.test(String(nativeCell(holder, key)));
export function processTreeAlignmentFields(form: ProcessTreeAlignmentForm, inputName: string): Array<[string, string]> {
  if (!TREE_ALIGNMENT_VARIANTS.includes(form.variant) || !TREE_ALIGNMENT_INPUT_FORMATS.includes(form.inputFormat) || !TREE_ALIGNMENT_INPUT_SHAPES.includes(form.inputShape) || !TREE_ALIGNMENT_MODEL_FORMATS.includes(form.modelFormat) || !TREE_ALIGNMENT_CONVERSIONS.includes(form.conversion)) throw new Error("Select supported process-tree alignment inputs and options.");
  if (form.inputFormat === "trace_words" ? form.inputShape !== "words" : form.inputShape === "words") throw new Error("Literal trace words require the trace-word input format and shape.");
  const fields: Array<[string, string]> = [["pt_alignment_variant", form.variant], ["input_format", form.inputFormat], ["input_shape", form.inputShape], ["model_format", form.modelFormat], ["discovery_tree_variant", form.conversion]];
  if (form.inputShape === "trace" && form.traceIndex !== "") {
    if (!/^\d+$/.test(form.traceIndex.trim()) || BigInt(form.traceIndex.trim()) > 18446744073709551615n) throw new Error("Trace index must be an unsigned integer."); fields.push(["trace_index", form.traceIndex]);
  }
  if (form.inputFormat === "auto") {
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) {const value = form.importFields[key]; if (value !== undefined && value !== "") fields.push([key, value]);}
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) {const value = form.importFields[key]; if (value === undefined || value === "") continue; if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`); fields.push([key, value]);}
    if (form.importFields.xes_variant) {if (!WORKFLOW_INPUT_XES_VARIANTS.some(value => value === form.importFields.xes_variant)) throw new Error("Select a supported XES importer."); fields.push(["xes_variant", form.importFields.xes_variant]);}
    const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(inputName) ? "\t" : ""); if (delimiter) {if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character."); fields.push(["delimiter", delimiter]);}
  }
  const controls = processTreeAlignmentControlFields(form.fields, form.explicitEmptySelectors); fields.push(...controls.fields);
  if (Object.keys(controls.selectors).length) fields.push(["alignment_selectors_json", JSON.stringify(controls.selectors)]);
  return fields;
}
/** Preserve every returned field of native errors, incomplete calls and successful alignments. */
export function isProcessTreeAlignmentWorkbench(value: unknown): value is ProcessTreeAlignmentWorkbench {
  if (!object(value) || value.family !== "PROCESS_TREE_ALIGNMENT_WORKBENCH" || !TREE_ALIGNMENT_VARIANTS.some(variant => value.variant === variant) || !TREE_ALIGNMENT_INPUT_SHAPES.some(shape => value.input_shape === shape) || value.result_shape !== "log" || typeof value.executed !== "boolean" || !object(value.input_context) || !object(value.model_context) || !object(value.conversion_context) || !isNativeTreeAlignmentOptions(value.options_context)) return false;
  const input = value.input_context, model = value.model_context, conversion = value.conversion_context;
  if (!TREE_ALIGNMENT_INPUT_FORMATS.some(format => input.format === format) || !["detected_format", "dataset", "origin", "activity_attribute", "case_id_attribute"].every(key => typeof input[key] === "string") || !["original_input_bytes", "input_event_count"].every(key => countLiteral(input, key)) || !["input_case_count", "checked_event_count", "checked_case_count", "trace_index"].every(key => input[key] === null || countLiteral(input, key))) return false;
  if (input.format === "trace_words" ? value.input_shape !== "words" : value.input_shape === "words") return false;
  if (!TREE_ALIGNMENT_MODEL_FORMATS.some(format => model.format === format) || !["detected_format", "dataset"].every(key => typeof model[key] === "string") || !countLiteral(model, "original_input_bytes") || !isAlignmentNativeTree(model.native_tree)) return false;
  if (!TREE_ALIGNMENT_CONVERSIONS.some(variant => conversion.variant === variant) || typeof conversion.computed !== "boolean" || !(conversion.error === null || typeof conversion.error === "string")) return false;
  if (!["input_before", "input_after"].every(key => value[key] === null || isWorkflowInputTable(value[key])) || !(value.input_trace_words === null || strings(value.input_trace_words)) || !["input_log", "checked_log"].every(key => value[key] === null || object(value[key]))) return false;
  if (value.native_net !== null && !isWorkflowInputNativeNet(value.native_net) || !["input_log_error", "checked_log_error", "source_xes", "source_xes_error", "ptml", "ptml_error", "pnml", "pnml_error", "bpmn", "bpmn_error"].every(key => value[key] === null || typeof value[key] === "string")) return false;
  if (value.options_context.variant !== value.variant) return false;
  if (!value.executed) return value.result === null && object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string";
  return value.error === null && isNativeProcessTreeAlignment(value.result) && value.result.variant === value.variant;
}
export interface ProcessTreeAlignmentRun {
  value: ProcessTreeAlignmentWorkbench; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; http_status: number;
  inputs: {input: File; model: File; input_sha256: string; model_sha256: string};
}
export function processTreeAlignmentTables(value: ProcessTreeAlignmentWorkbench, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = processTreeAlignmentNativeTables(value, value.model_context.native_tree, value.result), submitted = fields ?? nativeResponseRequest(value)?.fields;
  for (const key of ["input_before", "input_after"] as const) {
    const table = value[key]; if (!table) continue;
    const rows: unknown[][] = [], records: unknown[] = [];
    table.rows.forEach((row, row_index) => row.forEach((cell, column_index) => {
      rows.push([row_index, column_index, table.columns[column_index], table.dtypes?.[column_index], cell === null ? "missing" : cell.null ? "present null" : "present", cell?.kind, cell?.value, cell === null ? undefined : nativeCell(cell, "typed_value"), cell]);
      records.push({row_index, column_index, column: table.columns[column_index], dtype: table.dtypes?.[column_index], cell});
    }));
    tables.push({id: `${key}_cells`, title: key === "input_before" ? "Every original alignment input table cell" : "Every resulting alignment input table cell", columns: ["row_index", "column_index", "column", "dtype", "presence", "kind", "lexical_value", "typed_value", "complete_cell"], rows, records});
  }
  if (submitted) tables.push({id: "request", title: "Exact submitted process-tree alignment options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))}); return tables;
}
async function fileHash(file: File): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");}
export async function runProcessTreeAlignmentWorkbench(input: File, model: File, form: ProcessTreeAlignmentForm, signal?: AbortSignal): Promise<ProcessTreeAlignmentRun> {
  const fields = processTreeAlignmentFields(form, input.name), [input_sha256, model_sha256] = await Promise.all([fileHash(input), fileHash(model)]); signal?.throwIfAborted();
  const body = new FormData(); body.append("input_file", input); body.append("model_file", model); fields.forEach(([key, value]) => body.append(key, value));
  const path = "/api/align-process-tree-workbench", response = await fetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!isProcessTreeAlignmentWorkbench(value)) {if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Process-tree alignment failed (${response.status}).`); throw new Error("The server did not return a valid native process-tree alignment envelope.");}
  if (value.variant !== form.variant || value.input_shape !== form.inputShape || value.input_context.format !== form.inputFormat || value.model_context.format !== form.modelFormat || value.input_context.dataset !== input.name || value.model_context.dataset !== model.name || BigInt(String(nativeCell(value.input_context, "original_input_bytes"))) !== BigInt(input.size) || BigInt(String(nativeCell(value.model_context, "original_input_bytes"))) !== BigInt(model.size) || value.conversion_context.variant !== form.conversion) throw new Error("The process-tree alignment response does not match the submitted inputs and options.");
  const controls = processTreeAlignmentControlFields(form.fields, form.explicitEmptySelectors);
  for (const [key, expected] of Object.entries(controls.selectors)) if (value.input_context[key === "alignment_case_id_attribute" ? "case_id_attribute" : "activity_attribute"] !== expected || key === "pt_alignment_activity_attribute" && value.options_context.activity_attribute !== expected) throw new Error("The process-tree alignment response does not match the submitted literal selectors.");
  for (const [key, raw] of controls.fields) {
    const nativeKey = key.replace(/^pt_alignment_/, "").replace(/_ms$/, "_seconds"), actual = value.options_context[nativeKey as keyof NativeTreeAlignmentOptions], kind = TREE_ALIGNMENT_CONTROLS.find(control => control.key === key)!.kind;
    const matches = kind === "boolean" ? actual === (raw === "true") : kind === "number" ? Object.is(actual, processTreeAlignmentMilliseconds(raw)! / 1000) : unsigned(value.options_context, nativeKey) === BigInt(raw.trim());
    if (!matches) throw new Error("The process-tree alignment response does not match the submitted native options.");
  }
  if (form.inputShape === "trace" && form.traceIndex !== "" && unsigned(value.input_context, "trace_index") !== BigInt(form.traceIndex.trim())) throw new Error("The process-tree alignment response does not match the selected original trace.");
  if (!response.ok && value.executed) throw new Error(`The server returned an executed process-tree alignment with failure status ${response.status}.`);
  return {value, raw: nativeResponseJson(value), fields, tables: processTreeAlignmentTables(value, fields), http_status: response.status, inputs: {input, model, input_sha256, model_sha256}};
}
export function processTreeAlignmentReport(run: ProcessTreeAlignmentRun): string {
  const metadata = (file: File, sha256: string) => ({name: file.name, size: file.size, last_modified: file.lastModified, sha256});
  return nativeJson({family: "PROCESS_TREE_ALIGNMENT_WORKBENCH_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/align-process-tree-workbench", fields: run.fields}, inputs: {input: metadata(run.inputs.input, run.inputs.input_sha256), model: metadata(run.inputs.model, run.inputs.model_sha256)}, native_result: run.value}, 2);
}
export function processTreeAlignmentArtifactFile(run: ProcessTreeAlignmentRun, format: "ptml" | "pnml" | "bpmn" | "source_xes"): File {
  const text = run.value[format]; if (text === null) throw new Error(run.value[`${format}_error`] ?? `This run did not return ${format.toUpperCase()}.`);
  return new File([workflowInputNativeBytes(text)], `minera-tree-alignment-${run.value.variant.toLowerCase()}.${format === "source_xes" ? "xes" : format}`, {type: "application/xml"});
}
export function processTreeAlignmentSucceeded(value: ProcessTreeAlignmentWorkbench): boolean {return value.executed && value.result !== null && value.result.computed && value.result.completed;}
export function processTreeAlignmentSummary(value: ProcessTreeAlignmentWorkbench): string {
  if (!value.executed) return `Alignment failed: ${value.error!.type}: ${value.error!.message}. Original inputs, model and native options remain inspectable.`;
  const result = value.result!;
  if (!result.computed || !result.completed) return `Incomplete process-tree alignment${result.timed_out ? " · time limit reached" : ""}${result.exhausted ? " · search bound reached" : ""}. Every stored score, move and diagnostic remains inspectable.`;
  return `${result.variant} · ${result.traces.length} returned trace alignments · total cost ${nativeCell(result, "total_cost")} · fitness ${nativeCell(result, "fitness")}.`;
}
