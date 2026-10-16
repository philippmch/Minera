import {appendNativeMultipartField} from "./native-multipart";
import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {nativeCell, nativeJson} from "./native-json";
import type {ModelGraph} from "./model-graph";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "./token-replay-workbench";
import {WORKFLOW_INPUT_XES_VARIANTS, isWorkflowInputNativeNet, isWorkflowInputTable, workflowInputNativeBytes, type WorkflowInputTable} from "./workflow-input-workbench";

export const INDUCTIVE_INPUT_VARIANTS = ["log", "imf", "dfg"] as const;
export type InductiveInputVariant = typeof INDUCTIVE_INPUT_VARIANTS[number];
export const INDUCTIVE_INPUT_SHAPES = ["log", "stream", "dataframe", "lazyframe", "dfg", "variants"] as const;
export const INDUCTIVE_INPUT_FORMATS = ["auto", "table", "dfg_json", "variants_json"] as const;
export const INDUCTIVE_INPUT_CONVERSIONS = ["standard", "transition_bordered"] as const;
export const INDUCTIVE_INPUT_CANONICAL_VARIANTS = ["IM", "IMF", "IMD", "UNKNOWN"] as const;
export interface InductiveInputForm {
  variant: InductiveInputVariant; shape: typeof INDUCTIVE_INPUT_SHAPES[number]; format: typeof INDUCTIVE_INPUT_FORMATS[number];
  conversion: typeof INDUCTIVE_INPUT_CONVERSIONS[number]; fields: Record<string, string>; importFields: Record<string, string>; explicitEmptySelectors: Record<string, boolean>;
}
export const defaultInductiveInputForm = (): InductiveInputForm => ({variant: "log", shape: "log", format: "auto", conversion: "standard", fields: {}, importFields: {}, explicitEmptySelectors: {}});
export const INDUCTIVE_INPUT_CONTROLS = [
  {key: "noise_threshold", label: "IMf noise threshold", kind: "number"},
  {key: "inductive_activity_key", label: "Inductive activity attribute", kind: "selector"},
  {key: "inductive_case_id_key", label: "Inductive case attribute", kind: "selector"},
  {key: "inductive_timestamp_key", label: "Inductive timestamp attribute", kind: "selector"},
  {key: "inductive_sort_events_by_timestamp", label: "Sort grouped-log events by timestamp", kind: "boolean"},
  {key: "inductive_disable_strict_sequence_cut", label: "Use the normal sequence cut", kind: "boolean"},
  {key: "inductive_multiprocessing", label: "Parallel fall-through evaluation", kind: "boolean"},
  {key: "inductive_disable_fallthroughs", label: "Disable specialized fall-throughs", kind: "boolean"},
] as const;
export type InductiveInputControl = typeof INDUCTIVE_INPUT_CONTROLS[number];
export interface InductiveWeightedVariant {activities: string[]; count: number;}
export interface InductiveNativeDfg {
  activities: string[]; edges: Array<{source: number; target: number; count: number}>; start_counts: number[]; end_counts: number[];
  activity_counts?: number[]; rework_case_counts?: number[]; variants?: InductiveWeightedVariant[];
}
export const INDUCTIVE_NATIVE_OPERATORS = ["ACTIVITY", "TAU", "SEQUENCE", "EXCLUSIVE", "INCLUSIVE_CHOICE", "PARALLEL", "INTERLEAVING", "LOOP", "UNKNOWN"] as const;
export interface InductiveNativeTreeNode {op: typeof INDUCTIVE_NATIVE_OPERATORS[number]; op_index: number; activity: number; children: InductiveNativeTreeNode[];}
export interface InductiveNativeTree {activities: string[]; root: InductiveNativeTreeNode;}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const strings = (value: unknown): value is string[] => Array.isArray(value) && value.every(item => typeof item === "string");
const unsigned = (holder: object, key: string | number): bigint | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const literal = String(nativeCell(holder, key)); if (!/^(?:0|[1-9]\d*)$/.test(literal)) return undefined;
  const value = BigInt(literal); return value <= 18446744073709551615n ? value : undefined;
};
export function isInductiveWeightedVariants(value: unknown): value is InductiveWeightedVariant[] {
  return Array.isArray(value) && value.every(row => object(row) && strings(row.activities) && unsigned(row, "count") !== undefined);
}
export function isInductiveNativeDfg(value: unknown): value is InductiveNativeDfg {
  if (!object(value) || !strings(value.activities) || new Set(value.activities).size !== value.activities.length || !Array.isArray(value.edges)) return false;
  const count = value.activities.length, pairs = new Set<string>();
  for (const edge of value.edges) {
    if (!object(edge) || unsigned(edge, "source") === undefined || unsigned(edge, "target") === undefined || unsigned(edge, "source")! >= BigInt(count) || unsigned(edge, "target")! >= BigInt(count) || unsigned(edge, "count") === undefined) return false;
    const pair = `${nativeCell(edge, "source")}/${nativeCell(edge, "target")}`; if (pairs.has(pair)) return false; pairs.add(pair);
  }
  if (!["start_counts", "end_counts"].every(key => Array.isArray(value[key]) && (value[key] as unknown[]).length === count && (value[key] as unknown[]).every((_, i) => unsigned(value[key] as object, i) !== undefined))) return false;
  if (!["activity_counts", "rework_case_counts"].every(key => value[key] === undefined || Array.isArray(value[key]) && (value[key] as unknown[]).length === count && (value[key] as unknown[]).every((_, i) => unsigned(value[key] as object, i) !== undefined))) return false;
  return value.variants === undefined || isInductiveWeightedVariants(value.variants);
}
export function isInductiveNativeTree(value: unknown): value is InductiveNativeTree {
  if (!object(value) || !strings(value.activities)) return false;
  const activities = value.activities;
  const node = (candidate: unknown): boolean => object(candidate) && INDUCTIVE_NATIVE_OPERATORS.some(op => op === candidate.op) && typeof candidate.op_index === "number" && /^-?(?:0|[1-9]\d*)$/.test(String(nativeCell(candidate, "op_index"))) && unsigned(candidate, "activity") !== undefined &&
    (candidate.op !== "ACTIVITY" || unsigned(candidate, "activity")! < BigInt(activities.length)) && Array.isArray(candidate.children) && candidate.children.every(node);
  return node(value.root);
}
/** Retain submitted strings; the native parser owns selector semantics and option defaults. */
export function inductiveInputControlFields(values: Record<string, string>, explicitEmpty: Record<string, boolean> = {}): {fields: Array<[string, string]>; selectors: Record<string, string>} {
  const fields: Array<[string, string]> = [], selectors: Record<string, string> = Object.create(null);
  for (const control of INDUCTIVE_INPUT_CONTROLS) {
    const value = values[control.key] ?? "";
    if (control.kind === "selector") {if (value !== "" || explicitEmpty[control.key]) selectors[control.key] = value; continue;}
    if (value === "") continue;
    if (control.kind === "boolean" && !["true", "false"].includes(value)) throw new Error(`${control.label} must be true or false.`);
    if (control.kind === "number" && (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(value.trim()) || !Number.isFinite(Number(value)) || Number(value) < 0 || Number(value) > 1)) throw new Error("Noise threshold must be a finite number in [0, 1].");
    fields.push([control.key, value]);
  }
  return {fields, selectors};
}
export function inductiveInputNativeTables(value: object, tree: InductiveNativeTree | null): DiagnosticTable[] {
  const tables = nativeObjectTables(value);
  if (tree) {
    const rows: unknown[][] = [], records: unknown[] = [];
    const visit = (node: InductiveNativeTreeNode, path: string, parent: string | null) => {
      rows.push([path, parent, node.op, nativeCell(node, "op_index"), nativeCell(node, "activity"), node.op === "ACTIVITY" ? tree.activities[node.activity] : undefined, node.children.length, node]);
      records.push({path, parent, node}); node.children.forEach((child, i) => visit(child, `${path}/${i}`, path));
    };
    visit(tree.root, "root", null); tables.push({id: "complete_tree_nodes", title: "Every complete native process-tree node", columns: ["path", "parent", "operator", "operator_index", "stored_activity_id", "activity_label", "child_count", "complete_node"], rows, records});
  }
  return tables;
}
/** Diagram identities follow the returned native tree; all complete nodes remain in tables and downloads. */
export function inductiveInputTreeGraph(tree: InductiveNativeTree): ModelGraph {
  const nodes: ModelGraph["nodes"] = [], edges: ModelGraph["edges"] = [];
  const visit = (node: InductiveNativeTreeNode, path: string, parent?: string) => {
    nodes.push({id: path, label: node.op === "ACTIVITY" ? tree.activities[node.activity] : node.op === "TAU" ? "τ" : node.op, detail: `Operator ${node.op} · stored activity ${nativeCell(node, "activity")}`, start: parent === undefined});
    if (parent) edges.push({id: `${parent}>${path}`, source: parent, target: path, label: "", value: 1, directed: true});
    node.children.forEach((child, i) => visit(child, `${path}/${i}`, path));
  };
  visit(tree.root, "root"); return {nodes, edges};
}
export function inductiveInputAllTablesCsv(tables: DiagnosticTable[]): string {
  const rows: unknown[][] = [];
  for (const table of tables) {
    if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined]));
    table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));
  }
  return diagnosticCsv({id: "all_inductive_input_tables", title: "Every complete Inductive discovery table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export class InductiveInputRequestGate {
  private generation = 0;
  issue(): number {return ++this.generation;}
  invalidate(): void {++this.generation;}
  accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}
}
export const INDUCTIVE_WEIGHTED_VARIANTS_EXAMPLE = '[\n  {"activities":["A","B"],"count":18446744073709551615},\n  {"activities":[],"count":0},\n  {"activities":["A","A"],"count":0}\n]';
export const INDUCTIVE_NATIVE_DFG_EXAMPLE = '{\n  "activities":["B","A","isolated 東京"],\n  "edges":[{"source":1,"target":0,"count":1},{"source":0,"target":0,"count":0}],\n  "start_counts":[0,1,0],\n  "end_counts":[1,0,0]\n}';
export interface InductiveNativeOptions {
  noise_threshold: number | "NaN" | "INF" | "-INF"; variant: typeof INDUCTIVE_INPUT_CANONICAL_VARIANTS[number]; variant_index: number;
  activity_key: string; timestamp_key: string; case_id_key: string; sort_events_by_timestamp: boolean;
  disable_strict_sequence_cut: boolean; multiprocessing: boolean; disable_fallthroughs: boolean;
}
export interface InductiveInputDiscovery {
  family: "INDUCTIVE_INPUT_DISCOVERY"; variant: typeof INDUCTIVE_INPUT_CANONICAL_VARIANTS[number]; computed: boolean; error: null | {type: string; message: string};
  input_context: {adapter: InductiveInputForm["shape"]; format: InductiveInputForm["format"]; detected_format: string; dataset: string; origin: string; original_input_bytes: number;
    input_event_count: number | null; input_case_count: number | null; weighted_variant_count: number | null; activity_count: number | null};
  options_context: InductiveNativeOptions; input_before: WorkflowInputTable | null; input_after: WorkflowInputTable | null;
  input_log: Record<string, unknown> | null; input_log_error: string | null; dfg_input: InductiveNativeDfg | null; variant_log_input: InductiveWeightedVariant[] | null;
  native_dfg: InductiveNativeDfg | null; native_tree: InductiveNativeTree | null; native_net: Record<string, unknown> | null;
  discovery_context: {requested_variant: string; effective_variant: string; direct_graph_semantics: boolean; prepared_event_count: number | null; prepared_case_count: number | null};
  prepared_log: Record<string, unknown> | null; prepared_log_error: string | null; source_xes: string | null; source_xes_error: string | null;
  conversion_context: {variant: InductiveInputForm["conversion"]; computed: boolean; error: string | null};
  ptml: string | null; ptml_error: string | null; bpmn: string | null; bpmn_error: string | null; pnml: string | null; pnml_error: string | null;
  result: Record<string, unknown> | null; result_error: string | null;
}
const countLiteral = (holder: object, key: string): boolean => typeof (holder as Record<string, unknown>)[key] === "number" && /^(?:0|[1-9]\d*)$/.test(String(nativeCell(holder, key)));
function isNativeOptions(value: unknown): value is InductiveNativeOptions {
  return object(value) && (typeof value.noise_threshold === "number" && Number.isFinite(value.noise_threshold) || value.noise_threshold === "NaN" || value.noise_threshold === "INF" || value.noise_threshold === "-INF") && INDUCTIVE_INPUT_CANONICAL_VARIANTS.some(variant => value.variant === variant) && typeof value.variant_index === "number" && /^-?(?:0|[1-9]\d*)$/.test(String(nativeCell(value, "variant_index"))) &&
    ["activity_key", "timestamp_key", "case_id_key"].every(key => typeof value[key] === "string") && ["sort_events_by_timestamp", "disable_strict_sequence_cut", "multiprocessing", "disable_fallthroughs"].every(key => typeof value[key] === "boolean");
}
export function inductiveInputFields(form: InductiveInputForm, name: string): Array<[string, string]> {
  if (!INDUCTIVE_INPUT_VARIANTS.includes(form.variant) || !INDUCTIVE_INPUT_SHAPES.includes(form.shape) || !INDUCTIVE_INPUT_FORMATS.includes(form.format) || !INDUCTIVE_INPUT_CONVERSIONS.includes(form.conversion)) throw new Error("Select supported Inductive discovery and input options.");
  if (form.shape === "dfg" ? form.format !== "dfg_json" : form.format === "dfg_json" || (form.shape === "variants" ? form.format !== "variants_json" : form.format === "variants_json")) throw new Error("Direct DFG and weighted-variant input require their matching JSON formats.");
  const fields: Array<[string, string]> = [["input_shape", form.shape], ["input_format", form.format], ["inductive_variant", form.variant], ["discovery_tree_variant", form.conversion]];
  if (form.format === "auto") {
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) {const value = form.importFields[key]; if (value !== undefined && value !== "") fields.push([key, value]);}
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) {const value = form.importFields[key]; if (value === undefined || value === "") continue; if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`); fields.push([key, value]);}
    if (form.importFields.xes_variant) {if (!WORKFLOW_INPUT_XES_VARIANTS.some(value => value === form.importFields.xes_variant)) throw new Error("Select a supported XES importer."); fields.push(["xes_variant", form.importFields.xes_variant]);}
    const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(name) ? "\t" : ""); if (delimiter) {if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character."); fields.push(["delimiter", delimiter]);}
  }
  const controls = inductiveInputControlFields(form.fields, form.explicitEmptySelectors); fields.push(...controls.fields);
  if (Object.keys(controls.selectors).length) fields.push(["discovery_selectors_json", JSON.stringify(controls.selectors)]);
  return fields;
}
/** Native tree success is independent of optional event-log rendering or model-format conversion. */
export function isInductiveInputDiscovery(value: unknown): value is InductiveInputDiscovery {
  if (!object(value) || value.family !== "INDUCTIVE_INPUT_DISCOVERY" || !INDUCTIVE_INPUT_CANONICAL_VARIANTS.some(variant => value.variant === variant) || typeof value.computed !== "boolean" || !object(value.input_context) || !object(value.discovery_context) || !object(value.conversion_context) || !isNativeOptions(value.options_context)) return false;
  const input = value.input_context, discovery = value.discovery_context, conversion = value.conversion_context;
  if (!INDUCTIVE_INPUT_SHAPES.some(shape => input.adapter === shape) || !INDUCTIVE_INPUT_FORMATS.some(format => input.format === format) || !["detected_format", "dataset", "origin"].every(key => typeof input[key] === "string") || !countLiteral(input, "original_input_bytes") || !["input_event_count", "input_case_count", "weighted_variant_count", "activity_count"].every(key => input[key] === null || countLiteral(input, key))) return false;
  if (input.adapter === "dfg" ? input.format !== "dfg_json" : input.format === "dfg_json" || (input.adapter === "variants" ? input.format !== "variants_json" : input.format === "variants_json")) return false;
  if (!["requested_variant", "effective_variant"].every(key => typeof discovery[key] === "string") || typeof discovery.direct_graph_semantics !== "boolean" || !["prepared_event_count", "prepared_case_count"].every(key => discovery[key] === null || countLiteral(discovery, key))) return false;
  if (!INDUCTIVE_INPUT_CONVERSIONS.some(variant => conversion.variant === variant) || typeof conversion.computed !== "boolean" || !(conversion.error === null || typeof conversion.error === "string")) return false;
  if (!["input_before", "input_after"].every(key => value[key] === null || isWorkflowInputTable(value[key])) || !["input_log", "prepared_log", "result"].every(key => value[key] === null || object(value[key]))) return false;
  if (value.dfg_input !== null && !isInductiveNativeDfg(value.dfg_input) || value.variant_log_input !== null && !isInductiveWeightedVariants(value.variant_log_input)) return false;
  if (value.native_dfg !== null && (!isInductiveNativeDfg(value.native_dfg) || value.native_dfg.activity_counts === undefined || value.native_dfg.rework_case_counts === undefined || value.native_dfg.variants === undefined)) return false;
  if (value.native_tree !== null && !isInductiveNativeTree(value.native_tree) || value.native_net !== null && !isWorkflowInputNativeNet(value.native_net)) return false;
  if (!["input_log_error", "prepared_log_error", "source_xes", "source_xes_error", "ptml", "ptml_error", "bpmn", "bpmn_error", "pnml", "pnml_error", "result_error"].every(key => value[key] === null || typeof value[key] === "string")) return false;
  if (!value.computed) return object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string";
  return value.error === null && value.native_tree !== null;
}
export interface InductiveInputRun {
  value: InductiveInputDiscovery; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; input: File; input_sha256: string; http_status: number;
}
export function inductiveInputTables(value: InductiveInputDiscovery, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = inductiveInputNativeTables(value, value.native_tree), submitted = fields ?? nativeResponseRequest(value)?.fields;
  if (submitted) tables.push({id: "request", title: "Exact submitted Inductive discovery options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))});
  return tables;
}
async function fileHash(file: File): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");}
export async function runInductiveInput(file: File, form: InductiveInputForm, signal?: AbortSignal): Promise<InductiveInputRun> {
  const fields = inductiveInputFields(form, file.name), hash = await fileHash(file); signal?.throwIfAborted();
  const body = new FormData(); body.append("input", file); fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value));
  const path = "/api/discover-inductive-input", response = await fetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!isInductiveInputDiscovery(value)) {if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Inductive discovery failed (${response.status}).`); throw new Error("The server did not return a valid native Inductive input discovery envelope.");}
  const expectedVariant = {log: "IM", imf: "IMF", dfg: "IMD"}[form.variant];
  if (value.variant !== expectedVariant || value.input_context.adapter !== form.shape || value.input_context.format !== form.format || value.input_context.dataset !== file.name || !countLiteral(value.input_context, "original_input_bytes") || BigInt(String(nativeCell(value.input_context, "original_input_bytes"))) !== BigInt(file.size) || value.conversion_context.variant !== form.conversion) throw new Error("The Inductive response does not match the submitted input and discovery options.");
  if (!response.ok && value.computed) throw new Error(`The server returned computed Inductive discovery with failure status ${response.status}.`);
  const selectors = inductiveInputControlFields(form.fields, form.explicitEmptySelectors).selectors;
  for (const [key, expected] of Object.entries(selectors)) {
    const nativeKey = key === "inductive_activity_key" ? "activity_key" : key === "inductive_timestamp_key" ? "timestamp_key" : "case_id_key";
    if (value.options_context[nativeKey] !== expected) throw new Error("The Inductive response does not match the submitted literal selectors.");
  }
  return {value, raw: nativeResponseJson(value), fields, tables: inductiveInputTables(value, fields), input: file, input_sha256: hash, http_status: response.status};
}
export function inductiveInputReport(run: InductiveInputRun): string {
  return nativeJson({family: "INDUCTIVE_INPUT_DISCOVERY_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/discover-inductive-input", fields: run.fields}, input: {name: run.input.name, size: run.input.size, last_modified: run.input.lastModified, sha256: run.input_sha256}, native_result: run.value}, 2);
}
export function inductiveInputArtifactFile(run: InductiveInputRun, format: "pnml" | "bpmn" | "ptml" | "source_xes"): File {
  const text = run.value[format]; if (text === null) throw new Error(run.value[`${format}_error`] ?? `This native run did not return ${format.toUpperCase()}.`);
  const extension = format === "source_xes" ? "xes" : format;
  return new File([workflowInputNativeBytes(text)], `minera-inductive-${run.value.variant.toLowerCase()}.${extension}`, {type: "application/xml"});
}
export function inductiveInputSummary(value: InductiveInputDiscovery): string {
  if (!value.computed) return `Inductive discovery failed: ${value.error!.type}: ${value.error!.message}. Original inputs and native options remain inspectable.`;
  return `${value.discovery_context.requested_variant} requested · ${value.discovery_context.effective_variant} effective · ${value.native_tree!.activities.length} native activity identities. ${value.input_context.adapter === "variants" ? "Compact weighted variants retain their original counts and ordering." : value.input_context.adapter === "dfg" ? "Direct graph discovery preserves the declared registry, topology and boundary counts." : "The complete original input and prepared discovery source remain inspectable."}`;
}
/** Lossless ASCII filename encoding works for every UTF-16 code unit, including lone surrogates. */
export function inductiveInputTableFileName(id: string): string {
  let encoded = "";
  for (let i = 0; i < id.length; ++i) encoded += /^[A-Za-z0-9_.-]$/.test(id[i]) ? id[i] : "%" + id.charCodeAt(i).toString(16).padStart(4, "0");
  return encoded;
}
