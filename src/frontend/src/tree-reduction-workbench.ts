import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import { backendFetch } from "./backend-fetch";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {isOriginalDeclareTrace} from "./original-declare-workbench";
import {isOriginalEmbeddingFilterNativeString as isString, originalEmbeddingFilterStringBytes as stringBytes, originalEmbeddingFilterStringText as stringText, type OriginalEmbeddingFilterNativeString as NativeString} from "./original-embedding-filter-workbench";
import {TREE_ALIGNMENT_OPERATORS} from "./process-tree-alignment-workbench";
import type {ModelGraph} from "./model-graph";

export interface ReductionTreeNode {op: string; op_index: number; activity: number; children: ReductionTreeNode[];}
export interface ReductionTree {activities: NativeString[]; root: ReductionTreeNode;}
export interface TreeReductionForm {modelFormat: "auto" | "ptml" | "tree_json"; traceFormat: "native_trace" | "trace_words"; fields: Record<string, string>;}
export interface TreeReductionUploads {options?: File; activityKey?: File;}
export const defaultTreeReductionForm = (): TreeReductionForm => ({modelFormat: "auto", traceFormat: "native_trace", fields: {}});
export interface TreeReductionOptions {variant: "TREE_TR_BASED" | "UNKNOWN"; variant_index: number; activity_key: NativeString;}
export interface TreeReductionPart {name: NativeString; filename: NativeString | null; bytes: NativeString; file: boolean;}
export interface TreeReductionWorkbench extends Record<string, unknown> {
  family: "MODEL_CONVERSION_WORKBENCH"; operation: "reduce_tree"; executed: boolean; native_entered: boolean; native_returned: boolean;
  error: null | {type: NativeString; message: NativeString};
  model: {process_tree: ReductionTree | null; original_model: NativeString};
  trace: {native_trace: Record<string, unknown> | null; original_trace: NativeString};
  result: null | {process_tree: ReductionTree}; options: TreeReductionOptions | null;
  input_context: {format: string; detected_format: string; dataset: NativeString; original_model_bytes: number; model_unchanged: boolean | null};
  trace_context: {format: string; dataset: NativeString; original_trace_bytes: number; event_count: number | null; trace_unchanged: boolean | null};
  snapshots: {model_before: ReductionTree | null; model_after: ReductionTree | null; trace_before: Record<string, unknown> | null; trace_after: Record<string, unknown> | null};
  original_inputs: TreeReductionPart[]; query_inputs: TreeReductionPart[]; input_order: string;
  exports: Record<string, NativeString | null>; artifacts: Record<string, NativeString | null>;
}
export interface TreeReductionRun {
  value: TreeReductionWorkbench; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; http_status: number;
  input: {model: File; trace: File; options?: File; activityKey?: File; model_sha256: string; trace_sha256: string; options_sha256?: string; activityKey_sha256?: string};
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const nullableString = (value: unknown) => value === null || isString(value);
function integer(holder: object, key: string | number, signed = false): bigint | undefined {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const token = String(nativeCell(holder, key));
  if (!(signed ? /^-?(?:0|[1-9]\d*)$/ : /^(?:0|[1-9]\d*)$/).test(token)) return undefined;
  const value = BigInt(token);
  return value >= (signed ? -2147483648n : 0n) && value <= (signed ? 2147483647n : 18446744073709551615n) ? value : undefined;
}
/** Stored trees may contain inactive indexes and unknown operators; native validation reports semantic errors. */
export function isReductionTree(value: unknown): value is ReductionTree {
  if (!object(value) || !Array.isArray(value.activities) || !value.activities.every(isString)) return false;
  const pending: unknown[] = [value.root], seen = new Set<object>();
  while (pending.length) {
    const node = pending.pop();
    if (!object(node) || seen.has(node) || !Array.isArray(node.children) || integer(node, "activity") === undefined) return false;
    const index = integer(node, "op_index", true);
    if (index === undefined || node.op !== (TREE_ALIGNMENT_OPERATORS[Number(index)] ?? "UNKNOWN")) return false;
    seen.add(node); for (const child of node.children) pending.push(child);
  }
  return true;
}
/** Validation-only view: replace byte carriers with display strings, preserving every numeric token. */
function traceValidationJson(value: unknown): string {
  if (isString(value)) return JSON.stringify(stringText(value));
  if (!value || typeof value !== "object") return nativeJson(value);
  const array = Array.isArray(value), keys = array ? value.map((_, i) => String(i)) : Object.keys(value);
  return (array ? "[" : "{") + keys.map(key => {
    const item = (value as Record<string, unknown>)[key], encoded = typeof item === "number" ? String(nativeCell(value, key)) : traceValidationJson(item);
    return (array ? "" : JSON.stringify(key) + ":") + encoded;
  }).join(",") + (array ? "]" : "}");
}
export function isReductionTrace(value: unknown): value is Record<string, unknown> {
  try {return isOriginalDeclareTrace(parseNativeJson(traceValidationJson(value)));} catch {return false;}
}
/** JSON.parse loses duplicate decoded keys; reject them before accepting complete options. */
function uniqueJson(raw: string): unknown {
  const parsed = parseNativeJson(raw), tokens = raw.match(/"(?:[^"\\]|\\.)*"|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?|true|false|null|[{}\[\],:]/g) ?? [];
  let cursor = 0;
  const walk = (): void => {
    const token = tokens[cursor++];
    if (token === "{") {const seen = new Set<string>(); while (tokens[cursor] !== "}") {const key = JSON.parse(tokens[cursor++]) as string; if (seen.has(key)) throw new Error(`Duplicate JSON key: ${key}`); seen.add(key); ++cursor; walk(); if (tokens[cursor] !== ",") break; ++cursor;} ++cursor;}
    else if (token === "[") {while (tokens[cursor] !== "]") {walk(); if (tokens[cursor] !== ",") break; ++cursor;} ++cursor;}
  };
  walk(); return parsed;
}
export function treeReductionOptions(raw: string): {variant_index: number; activity_key: NativeString} {
  const value = uniqueJson(raw);
  if (!object(value) || Object.keys(value).length !== 2 || integer(value, "variant_index", true) === undefined || !isString(value.activity_key)) throw new Error("Complete tree reduction options require exactly variant_index (signed int32) and activity_key (NativeString).");
  return value as {variant_index: number; activity_key: NativeString};
}
export function treeReductionFields(form: TreeReductionForm, uploads: TreeReductionUploads = {}): Array<[string, string]> {
  if (!["auto", "ptml", "tree_json"].includes(form.modelFormat) || !["native_trace", "trace_words"].includes(form.traceFormat)) throw new Error("Choose compatible tree and trace formats.");
  const allowed = ["tree_reduction_options_json", "tree_reduction_variant", "tree_reduction_activity_key"], keys = Object.keys(form.fields);
  if (keys.some(key => !allowed.includes(key))) throw new Error("Unknown tree reduction control.");
  const full = Object.hasOwn(form.fields, "tree_reduction_options_json"), scalar = keys.some(key => key !== "tree_reduction_options_json");
  if ((full && uploads.options) || ((full || uploads.options) && (scalar || uploads.activityKey)) || (Object.hasOwn(form.fields, "tree_reduction_activity_key") && uploads.activityKey)) throw new Error("Choose one complete options carrier, or scalar variant and one activity-key carrier; supplied empty fields also count.");
  if (full) treeReductionOptions(form.fields.tree_reduction_options_json);
  if (Object.hasOwn(form.fields, "tree_reduction_variant") && !["TREE_TR_BASED", "tree_tr_based"].includes(form.fields.tree_reduction_variant)) throw new Error("Unknown scalar tree reduction variant.");
  return [["operation", "reduce_tree"], ["model_format", form.modelFormat], ["trace_format", form.traceFormat], ...keys.map(key => [key, form.fields[key]] as [string, string])];
}
const options = (value: unknown): value is TreeReductionOptions => object(value) && integer(value, "variant_index", true) !== undefined && value.variant === (value.variant_index === 0 ? "TREE_TR_BASED" : "UNKNOWN") && isString(value.activity_key);
const part = (value: unknown): value is TreeReductionPart => object(value) && isString(value.name) && nullableString(value.filename) && isString(value.bytes) && typeof value.file === "boolean";
export function isTreeReductionWorkbench(value: unknown): value is TreeReductionWorkbench {
  if (!object(value) || value.family !== "MODEL_CONVERSION_WORKBENCH" || value.operation !== "reduce_tree" || !["executed", "native_entered", "native_returned"].every(key => typeof value[key] === "boolean") || !object(value.model) || !object(value.trace) || !object(value.input_context) || !object(value.trace_context) || !object(value.snapshots)) return false;
  if (!(value.error === null || object(value.error) && isString(value.error.type) && isString(value.error.message)) || !(value.options === null || options(value.options))) return false;
  if (!isString(value.model.original_model) || !isString(value.trace.original_trace) || !(value.model.process_tree === null || isReductionTree(value.model.process_tree)) || !(value.trace.native_trace === null || isReductionTrace(value.trace.native_trace)) || !(value.result === null || object(value.result) && isReductionTree(value.result.process_tree))) return false;
  const input = value.input_context, trace = value.trace_context;
  if (typeof input.format !== "string" || typeof input.detected_format !== "string" || !isString(input.dataset) || integer(input, "original_model_bytes") === undefined || !(input.model_unchanged === null || typeof input.model_unchanged === "boolean") || typeof trace.format !== "string" || !isString(trace.dataset) || integer(trace, "original_trace_bytes") === undefined || !(trace.event_count === null || integer(trace, "event_count") !== undefined) || !(trace.trace_unchanged === null || typeof trace.trace_unchanged === "boolean")) return false;
  const snapshots = value.snapshots;
  if (!["model_before", "model_after"].every(key => snapshots[key] === null || isReductionTree(snapshots[key])) || !["trace_before", "trace_after"].every(key => snapshots[key] === null || isReductionTrace(snapshots[key]))) return false;
  if (!Array.isArray(value.original_inputs) || !value.original_inputs.every(part) || !Array.isArray(value.query_inputs) || !value.query_inputs.every(part) || typeof value.input_order !== "string") return false;
  if (!["exports", "artifacts"].every(key => object(value[key]) && Object.values(value[key]).every(nullableString))) return false;
  for (const [key, names] of [["exports", ["source_pnml", "source_ptml", "source_powl", "result_pnml", "result_ptml", "result_powl"]], ["artifacts", ["source_tree_json", "result_tree_json", "source_trace_json", "legacy_source_tree_json", "legacy_result_tree_json"]]] as const) {
    const entries = value[key] as Record<string, unknown>;
    if (!names.every(name => Object.hasOwn(entries, name) && Object.hasOwn(entries, `${name}_error`))) return false;
  }
  if (value.executed !== value.native_returned) return false;
  return value.native_returned ? value.native_entered === true && value.result !== null && value.error === null && value.options !== null && value.model.process_tree !== null && value.trace.native_trace !== null : value.result === null && value.error !== null;
}
const equalBytes = (left: Uint8Array, right: Uint8Array) => left.length === right.length && left.every((byte, index) => byte === right[index]);
const hash = async (bytes: Uint8Array<ArrayBuffer>) => Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), byte => byte.toString(16).padStart(2, "0")).join("");
export async function runTreeReductionWorkbench(model: File, trace: File, form: TreeReductionForm, uploads: TreeReductionUploads = {}, signal?: AbortSignal): Promise<TreeReductionRun> {
  const fields = treeReductionFields(form, uploads), modelBytes = new Uint8Array(await model.arrayBuffer()), traceBytes = new Uint8Array(await trace.arrayBuffer());
  const optional = await Promise.all((["options", "activityKey"] as const).map(async key => {const file = uploads[key]; return file ? {key, file, bytes: new Uint8Array(await file.arrayBuffer())} : null;}));
  const body = new FormData(); body.append("model_file", model); body.append("trace_file", trace);
  fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value));
  if (uploads.options) body.append("tree_reduction_options_file", uploads.options);
  if (uploads.activityKey) body.append("tree_reduction_activity_key_file", uploads.activityKey);
  const response = await backendFetch("/api/model-conversion-workbench", {method: "POST", body, signal}), value = await readNativeResponse(response, {path: "/api/model-conversion-workbench", fields});
  if (!isTreeReductionWorkbench(value)) throw new Error(`Invalid tree reduction envelope (HTTP ${response.status}).`);
  if (value.input_context.format !== form.modelFormat || value.trace_context.format !== form.traceFormat || !equalBytes(stringBytes(value.model.original_model), modelBytes) || !equalBytes(stringBytes(value.trace.original_trace), traceBytes) || integer(value.input_context, "original_model_bytes") !== BigInt(modelBytes.length) || integer(value.trace_context, "original_trace_bytes") !== BigInt(traceBytes.length)) throw new Error("Tree reduction response does not match both original input Files and formats.");
  if (!equalBytes(stringBytes(value.input_context.dataset), nativeStringBytes(model.name)) || !equalBytes(stringBytes(value.trace_context.dataset), nativeStringBytes(trace.name)) || value.trace.native_trace !== null && integer(value.trace_context, "event_count") !== BigInt((value.trace.native_trace.events as unknown[]).length)) throw new Error("Tree reduction input names or complete Trace event count do not match.");
  const supplied = new Map<string, {bytes: Uint8Array; file: boolean; filename?: string}>();
  supplied.set("model_file", {bytes: modelBytes, file: true, filename: model.name}); supplied.set("trace_file", {bytes: traceBytes, file: true, filename: trace.name});
  fields.forEach(([key, raw]) => {const entry = body.get(key); supplied.set(key, {bytes: nativeStringBytes(raw), file: typeof entry !== "string", filename: typeof entry !== "string" && entry !== null ? entry.name : ""});});
  optional.forEach(row => {if (row) supplied.set(row.key === "options" ? "tree_reduction_options_file" : "tree_reduction_activity_key_file", {bytes: row.bytes, file: true, filename: row.file.name});});
  const captured = [...value.original_inputs, ...value.query_inputs];
  if (captured.length !== supplied.size || [...supplied].some(([name, expected]) => {const matches = captured.filter(row => equalBytes(stringBytes(row.name), nativeStringBytes(name))); return matches.length !== 1 || !equalBytes(stringBytes(matches[0].bytes), expected.bytes) || matches[0].file !== expected.file || expected.filename !== undefined && (matches[0].filename === null || !equalBytes(stringBytes(matches[0].filename), nativeStringBytes(expected.filename)));})) throw new Error("Tree reduction provenance does not match the complete submitted fields and Files.");
  if (value.options && !uploads.options) {
    const raw = fields.find(([key]) => key === "tree_reduction_options_json")?.[1], expected = raw === undefined ? null : treeReductionOptions(raw);
    const keyBytes = expected ? stringBytes(expected.activity_key) : optional.find(row => row?.key === "activityKey")?.bytes ?? nativeStringBytes(fields.find(([key]) => key === "tree_reduction_activity_key")?.[1] ?? "");
    if (value.options.variant_index !== (expected?.variant_index ?? 0) || !equalBytes(stringBytes(value.options.activity_key), keyBytes)) throw new Error("Tree reduction response does not match the submitted native options.");
  }
  if (!response.ok && value.error === null) throw new Error(`Unexpected tree reduction failure status ${response.status}.`);
  const input: TreeReductionRun["input"] = {model, trace, model_sha256: await hash(modelBytes), trace_sha256: await hash(traceBytes)};
  for (const row of optional) if (row) {input[row.key] = row.file; input[row.key === "options" ? "options_sha256" : "activityKey_sha256"] = await hash(row.bytes);}
  return {value, raw: nativeResponseJson(value), fields, tables: treeReductionTables(value, fields), http_status: response.status, input};
}
export function treeReductionTables(value: TreeReductionWorkbench, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = nativeObjectTables(value);
  for (const [prefix, tree] of [["source", value.model.process_tree], ["result", value.result?.process_tree]] as const) {
    if (!tree) continue;
    tables.push({id: `${prefix}_activities`, title: `Every ${prefix} activity registry entry`, columns: ["index", "native_string"], rows: tree.activities.map((label, index) => [index, label]), records: tree.activities.map((label, index) => ({index, label}))});
    const rows: unknown[][] = [], records: unknown[] = [], pending = [{node: tree.root, path: "root", parent: ""}];
    while (pending.length) {const {node, path, parent} = pending.pop()!; rows.push([path, parent, node.op, nativeCell(node, "op_index"), nativeCell(node, "activity"), node.children.length, node]); records.push(node); for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path});}
    tables.push({id: `${prefix}_tree_nodes`, title: `Every ${prefix} stored tree node`, columns: ["path", "parent", "operator", "operator_index", "stored_activity_index", "child_count", "complete_node"], rows, records});
  }
  if (fields) tables.push({id: "request", title: "Exact submitted tree reduction fields", columns: ["field", "value"], rows: fields, records: fields.map(([field, value]) => ({field, value}))});
  for (const table of tables) if (table.records) {const records = table.records; table.records = records.map((record, index) => record !== null && typeof record === "object" ? record : parseNativeJson(`{"value":${typeof record === "number" ? nativeCell(records, index) : nativeJson(record)}}`));}
  return tables;
}
export function treeReductionAllTablesCsv(tables: DiagnosticTable[]): string {
  const rows: unknown[][] = []; for (const table of tables) {if (!table.rows.length) table.columns.forEach((column, index) => rows.push([table.id, table.title, null, index, column, null])); table.rows.forEach((row, index) => table.columns.forEach((column, cell) => rows.push([table.id, table.title, index, cell, column, row[cell]])));}
  return diagnosticCsv({id: "all_tree_reduction", title: "Every complete tree reduction table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export function treeReductionGraph(tree: ReductionTree): ModelGraph {
  const graph: ModelGraph = {nodes: [], edges: []}, pending = [{node: tree.root, path: "root", parent: "", index: 0}];
  while (pending.length) {const {node, path, parent, index} = pending.pop()!, activity = integer(node, "activity"), label = activity !== undefined && activity < BigInt(tree.activities.length) ? tree.activities[Number(activity)] : undefined;
    graph.nodes.push({id: path, label: node.op === "ACTIVITY" ? label === undefined ? `Activity ${nativeCell(node, "activity")}` : stringText(label) : node.op === "TAU" ? "τ" : node.op, start: !parent, detail: `Operator ${nativeCell(node, "op_index")} · stored activity ${nativeCell(node, "activity")} · ${node.children.length} ordered children`});
    if (parent) graph.edges.push({id: `${parent}>${path}`, source: parent, target: path, label: `child ${index}`, value: 1, directed: true});
    for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path, index: i});
  } return graph;
}
export function treeReductionArtifacts(run: TreeReductionRun): Array<{name: string; bytes: Uint8Array<ArrayBuffer>; mime: string}> {
  const result: ReturnType<typeof treeReductionArtifacts> = [];
  for (const key of ["source_tree_json", "result_tree_json", "source_trace_json", "legacy_source_tree_json", "legacy_result_tree_json"]) {const value = run.value.artifacts[key]; if (isString(value)) result.push({name: `minera-reduce_tree-${key}.json`, bytes: stringBytes(value), mime: "application/json"});}
  for (const key of ["source_ptml", "result_ptml"]) {const value = run.value.exports[key]; if (isString(value)) result.push({name: `minera-reduce_tree-${key}.ptml`, bytes: stringBytes(value), mime: "application/xml"});}
  return result;
}
export function treeReductionReport(run: TreeReductionRun): string {
  const inputs = Object.fromEntries((["model", "trace", "options", "activityKey"] as const).flatMap(key => {const file = run.input[key]; return file ? [[key, {name: file.name, size: file.size, last_modified: file.lastModified, sha256: run.input[`${key}_sha256`]}]] : [];}));
  return nativeJson({family: "TREE_REDUCTION_WORKBENCH_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/model-conversion-workbench", fields: run.fields}, input: inputs, native_result: run.value}, 2);
}
/** A request is bound to both original Files, option File identities and the exact form revision. */
export class TreeReductionRequestGate {
  private revision = 0;
  issue(model: File, trace: File, form: TreeReductionForm, uploads: TreeReductionUploads = {}) {return {revision: ++this.revision, model, trace, options: uploads.options, activityKey: uploads.activityKey, form: nativeJson(form)};}
  invalidate() {++this.revision;}
  accepts(ticket: ReturnType<TreeReductionRequestGate["issue"]>, model: File | null, trace: File | null, form: TreeReductionForm, uploads: TreeReductionUploads = {}, signal?: AbortSignal) {return !signal?.aborted && ticket.revision === this.revision && ticket.model === model && ticket.trace === trace && ticket.options === uploads.options && ticket.activityKey === uploads.activityKey && ticket.form === nativeJson(form);}
}
export const TREE_REDUCTION_MODEL_EXAMPLE = '{"activities":["A","B","unused"],"root":{"op":"SEQUENCE","op_index":2,"activity":0,"children":[{"op":"EXCLUSIVE","op_index":3,"activity":0,"children":[{"op":"TAU","op_index":1,"activity":0,"children":[]},{"op":"ACTIVITY","op_index":0,"activity":0,"children":[]}]},{"op":"ACTIVITY","op_index":0,"activity":1,"children":[]}]}}';
const exampleEvent = (activity: string, order: number) => ({case_id: "case-1", activity, timestamp: "", resource: "", lifecycle: "", case_id_state_index: 0, activity_state_index: 0, timestamp_state_index: 0, resource_state_index: 0, lifecycle_state_index: 0, case_id_source_key: "", activity_source_key: "", timestamp_source_key: "", resource_source_key: "", lifecycle_source_key: "", input_order: order, attributes: [], activity_attribute: null, timestamp_attribute: null, resource_attribute: null, lifecycle_attribute: null});
export const TREE_REDUCTION_TRACE_EXAMPLE = nativeJson({case_id: "case-1", case_id_state_index: 0, case_id_source_key: "", case_id_attribute: null, attributes: [], properties: [], events: [exampleEvent("B", 0), exampleEvent("B", 1)]}, 2);
export const TREE_REDUCTION_OPTIONS_EXAMPLE = '{"variant_index":0,"activity_key":"concept:name"}';
