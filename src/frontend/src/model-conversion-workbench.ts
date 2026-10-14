import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import type {ModelGraph} from "./model-graph";
import {isAlignmentNativeTree, processTreeAlignmentTreeGraph, type AlignmentNativeTree} from "./process-tree-alignment-workbench";
import {PETRI_ANTI_MULTI_PNML_CONTROLS, petriAntiMultiTableFileName} from "./petri-anti-multi-workbench";
import {isWorkflowInputNativeNet, workflowInputNativeBytes} from "./workflow-input-workbench";

export const MODEL_CONVERSION_OPERATIONS = ["tree_to_powl", "net_to_powl", "powl_to_tree", "net_to_tree", "powl_to_net", "reduce_petri"] as const;
export type ModelConversionOperation = typeof MODEL_CONVERSION_OPERATIONS[number];
export const MODEL_CONVERSION_FORMATS = ["auto", "ptml", "tree_json", "pnml", "powl_text"] as const;
export type ModelConversionFormat = typeof MODEL_CONVERSION_FORMATS[number];
export const MODEL_CONVERSION_PNML_CONTROLS = PETRI_ANTI_MULTI_PNML_CONTROLS;
export const MODEL_CONVERSION_REDUCTION_CONTROLS = [
  {key: "petri_reduction_initial_marking_json", label: "Initial marking (exact signed JSON integer array)", kind: "marking"},
  {key: "petri_reduction_final_marking_json", label: "Final marking (exact signed JSON integer array)", kind: "marking"},
  {key: "petri_reduction_initial_marking", label: "Initial marking (legacy comma-separated signed integers)", kind: "legacy_marking"},
  {key: "petri_reduction_final_marking", label: "Final marking (legacy comma-separated signed integers)", kind: "legacy_marking"},
  {key: "petri_reduction_parallel_place_fusion", label: "Enable parallel place fusion", kind: "boolean"},
  {key: "petri_reduction_abstraction", label: "Enable abstraction", kind: "boolean"},
  {key: "petri_reduction_preserve_join_bridges", label: "Preserve join bridges", kind: "boolean"},
] as const;
export const MODEL_CONVERSION_ARTIFACTS = ["source_pnml", "source_ptml", "source_powl", "result_pnml", "result_ptml", "result_powl"] as const;
export type ModelConversionArtifact = typeof MODEL_CONVERSION_ARTIFACTS[number];
export const MODEL_CONVERSION_POWL_KINDS = ["TRANSITION", "SILENT", "XOR", "LOOP", "STRICT_PARTIAL_ORDER"] as const;
export interface ModelConversionForm {operation: ModelConversionOperation; modelFormat: ModelConversionFormat; fields: Record<string, string>; pnmlFields: Record<string, string>;}
export const defaultModelConversionForm = (operation: ModelConversionOperation = "tree_to_powl"): ModelConversionForm => ({operation, modelFormat: "auto", fields: {}, pnmlFields: {}});
export function modelConversionFormats(operation: ModelConversionOperation): readonly ModelConversionFormat[] {
  return operation === "tree_to_powl" ? ["auto", "ptml", "tree_json"] : operation.startsWith("powl_") ? ["auto", "powl_text"] : ["auto", "pnml"];
}
export interface NativePowlNode {kind: typeof MODEL_CONVERSION_POWL_KINDS[number] | "UNKNOWN"; kind_index: number; label: string; frequency: {minimum: number; maximum: number | null}; children: NativePowlNode[]; order: Array<[number, number]>;}
export interface ModelReductionOptions {initial_marking: number[]; final_marking: number[]; enable_parallel_place_fusion: boolean; enable_abstraction: boolean; preserve_join_bridges: boolean;}
export interface ModelReductionResult {net: Record<string, unknown>; markings: {initial: number[]; final: number[]}; old_to_new_place: number[]; old_to_new_transition: number[]; changed: boolean;}
export type ModelConversionMarkingOrigin = "pnml" | "explicit_json" | "explicit_legacy" | "explicit_empty_unspecified";
export interface ModelConversionWorkbench {
  family: "MODEL_CONVERSION_WORKBENCH"; operation: ModelConversionOperation; executed: boolean; error: null | {type: string; message: string};
  input_context: {format: ModelConversionFormat; detected_format: Exclude<ModelConversionFormat, "auto">; dataset: string; original_model_bytes: number; model_unchanged: boolean; initial_marking_origin: ModelConversionMarkingOrigin | null; final_marking_origin: ModelConversionMarkingOrigin | null};
  options: ModelReductionOptions | null; pnml_import_options: {infer_initial_marking: boolean; infer_final_marking: boolean; import_stochastic_map: boolean; require_workflow_boundaries: boolean; public_marking_semantics: boolean} | null;
  model: {process_tree: AlignmentNativeTree | null; powl: NativePowlNode | null; petri_net: Record<string, unknown> | null; pnml_document: Record<string, unknown> | null; original_model: string};
  result: null | {process_tree: AlignmentNativeTree | null; powl: NativePowlNode | null; petri_net: Record<string, unknown> | null; reduction: ModelReductionResult | null};
  exports: {[K in ModelConversionArtifact]: string | null} & {[K in `${ModelConversionArtifact}_error`]: string | null};
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const unsignedMax = 18446744073709551615n, signedMin = -9223372036854775808n, signedMax = 9223372036854775807n;
function integer(holder: object, key: string | number, signed = false, bits = 64): bigint | undefined {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const text = String(nativeCell(holder, key)); if (!(signed ? /^-?(?:0|[1-9]\d*)$/ : /^(?:0|[1-9]\d*)$/).test(text)) return undefined;
  const value = BigInt(text), minimum = signed ? bits === 32 ? -2147483648n : signedMin : 0n, maximum = bits === 32 ? 2147483647n : signed ? signedMax : unsignedMax;
  return value >= minimum && value <= maximum ? value : undefined;
}
const integerArray = (value: unknown, signed = false): value is number[] => Array.isArray(value) && value.every((_, index) => integer(value, index, signed) !== undefined);
/** Validate stored getter fields without reparsing canonical POWL text or imposing output depth caps. */
export function isNativePowl(value: unknown): value is NativePowlNode {
  const pending: unknown[] = [value], seen = new Set<object>();
  while (pending.length) {
    const node = pending.pop(); if (!object(node) || seen.has(node) || typeof node.label !== "string" || !object(node.frequency) || integer(node.frequency, "minimum", true, 32) === undefined || !(node.frequency.maximum === null || integer(node.frequency, "maximum", true, 32) !== undefined) || !Array.isArray(node.children) || !Array.isArray(node.order)) return false;
    const index = integer(node, "kind_index", true, 32); if (index === undefined || node.kind !== (MODEL_CONVERSION_POWL_KINDS[Number(index)] ?? "UNKNOWN")) return false;
    if (!node.order.every(pair => Array.isArray(pair) && pair.length === 2 && integerArray(pair))) return false;
    seen.add(node); for (const child of node.children) pending.push(child);
  }
  return true;
}
export function isModelReductionOptions(value: unknown): value is ModelReductionOptions {
  return object(value) && integerArray(value.initial_marking, true) && integerArray(value.final_marking, true) && ["enable_parallel_place_fusion", "enable_abstraction", "preserve_join_bridges"].every(key => typeof value[key] === "boolean");
}
export function isModelReductionResult(value: unknown): value is ModelReductionResult {
  return object(value) && isWorkflowInputNativeNet(value.net) && object(value.markings) && integerArray(value.markings.initial, true) && integerArray(value.markings.final, true) && integerArray(value.old_to_new_place) && integerArray(value.old_to_new_transition) && typeof value.changed === "boolean";
}
function legacyMarking(raw: string): bigint[] | undefined {
  const values = raw.split(",").map(item => item.replace(/^[ \t\r\n]+|[ \t\r\n]+$/g, ""));
  if (!values.every(item => /^-?\d+$/.test(item) && BigInt(item) >= signedMin && BigInt(item) <= signedMax)) return undefined;
  return values.map(item => BigInt(item));
}
export function modelConversionFields(form: ModelConversionForm): Array<[string, string]> {
  if (!MODEL_CONVERSION_OPERATIONS.includes(form.operation) || !modelConversionFormats(form.operation).includes(form.modelFormat)) throw new Error("Choose a model format compatible with this conversion.");
  const fields: Array<[string, string]> = [["operation", form.operation], ["model_format", form.modelFormat]];
  if (form.operation.startsWith("net_") || form.operation === "reduce_petri") {
    for (const [key] of MODEL_CONVERSION_PNML_CONTROLS) {const value = form.pnmlFields[key]; if (value === undefined || value === "") continue; if (!["true", "false", "1", "0"].includes(value.toLowerCase())) throw new Error(`${key} must be true, false, 1 or 0.`); fields.push([key, value]);}
  }
  if (form.operation === "reduce_petri") {
    for (const name of ["initial", "final"]) if (form.fields[`petri_reduction_${name}_marking`] && form.fields[`petri_reduction_${name}_marking_json`]) throw new Error(`Choose one ${name} reduction marking carrier: JSON or legacy text.`);
    for (const control of MODEL_CONVERSION_REDUCTION_CONTROLS) {
      const raw = form.fields[control.key] ?? ""; if (raw === "") continue;
      if (control.kind === "boolean" && !["true", "false", "1", "0"].includes(raw)) throw new Error(`${control.label} must be true, false, 1 or 0.`);
      if (control.kind === "marking") {let value: unknown; try {value = parseNativeJson(raw);} catch {throw new Error(`${control.label} must be valid JSON.`);} if (!integerArray(value, true)) throw new Error(`${control.label} must be an exact signed int64 JSON array.`);}
      if (control.kind === "legacy_marking" && !legacyMarking(raw)) throw new Error(`${control.label} must contain comma-separated signed int64 integers.`);
      fields.push([control.key, raw]);
    }
  }
  return fields;
}
export function isModelConversionWorkbench(value: unknown): value is ModelConversionWorkbench {
  if (!object(value) || value.family !== "MODEL_CONVERSION_WORKBENCH" || !MODEL_CONVERSION_OPERATIONS.some(operation => value.operation === operation) || typeof value.executed !== "boolean" || !object(value.input_context) || !object(value.model) || !object(value.exports)) return false;
  const operation = value.operation as ModelConversionOperation, input = value.input_context, model = value.model;
  if (!modelConversionFormats(operation).some(format => input.format === format) || !modelConversionFormats(operation).some(format => format !== "auto" && input.detected_format === format) || typeof input.dataset !== "string" || integer(input, "original_model_bytes") === undefined || typeof input.model_unchanged !== "boolean" || typeof model.original_model !== "string") return false;
  if (!(model.process_tree === null || isAlignmentNativeTree(model.process_tree)) || !(model.powl === null || isNativePowl(model.powl)) || !(model.petri_net === null || isWorkflowInputNativeNet(model.petri_net)) || !(model.pnml_document === null || object(model.pnml_document))) return false;
  const pnml = input.detected_format === "pnml";
  if (pnml ? !isWorkflowInputNativeNet(model.petri_net) || !object(model.pnml_document) || !object(value.pnml_import_options) || !MODEL_CONVERSION_PNML_CONTROLS.every(([key]) => typeof (value.pnml_import_options as Record<string, unknown>)[key.slice(5)] === "boolean") : value.pnml_import_options !== null) return false;
  if (input.detected_format === "powl_text" ? !isNativePowl(model.powl) : !pnml && !isAlignmentNativeTree(model.process_tree)) return false;
  const origins = ["pnml", "explicit_json", "explicit_legacy", "explicit_empty_unspecified"];
  if (operation === "reduce_petri" ? !isModelReductionOptions(value.options) || !["initial_marking_origin", "final_marking_origin"].every(key => origins.includes(String(input[key]))) : value.options !== null || input.initial_marking_origin !== null || input.final_marking_origin !== null) return false;
  if (!MODEL_CONVERSION_ARTIFACTS.every(key => [key, `${key}_error`].every(name => (value.exports as Record<string, unknown>)[name] === null || typeof (value.exports as Record<string, unknown>)[name] === "string"))) return false;
  if (value.error !== null) return value.result === null && object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string";
  if (!value.executed || !object(value.result)) return false;
  const result = value.result;
  if (!(result.process_tree === null || isAlignmentNativeTree(result.process_tree)) || !(result.powl === null || isNativePowl(result.powl)) || !(result.petri_net === null || isWorkflowInputNativeNet(result.petri_net)) || !(result.reduction === null || isModelReductionResult(result.reduction))) return false;
  return operation === "reduce_petri" ? isModelReductionResult(result.reduction) : operation.endsWith("_powl") ? isNativePowl(result.powl) : operation.endsWith("_tree") ? isAlignmentNativeTree(result.process_tree) : isWorkflowInputNativeNet(result.petri_net);
}
function treeTables(tree: AlignmentNativeTree, prefix: string): DiagnosticTable[] {
  const rows: unknown[][] = [], records: unknown[] = [], pending = [{node: tree.root, path: "root", parent: ""}];
  while (pending.length) {const {node, path, parent} = pending.pop()!; rows.push([path, parent, node.op, nativeCell(node, "op_index"), nativeCell(node, "activity"), node.op === "ACTIVITY" ? tree.activities[node.activity] : undefined, node.children.length, node]); records.push({path, parent, node}); for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path});}
  return [{id: `${prefix}_tree_nodes`, title: `Every ${prefix} process-tree node`, columns: ["path", "parent", "operator", "operator_index", "stored_activity", "activity_label", "child_count", "complete_node"], rows, records}];
}
function powlTables(powl: NativePowlNode, prefix: string): DiagnosticTable[] {
  const rows: unknown[][] = [], records: unknown[] = [], orderRows: unknown[][] = [], orderRecords: unknown[] = [], pending = [{node: powl, path: "root", parent: ""}];
  while (pending.length) {const {node, path, parent} = pending.pop()!; rows.push([path, parent, node.kind, nativeCell(node, "kind_index"), node.label, nativeCell(node.frequency, "minimum"), nativeCell(node.frequency, "maximum"), node.children.length, node]); records.push({path, parent, node}); node.order.forEach((pair, order_index) => {orderRows.push([path, order_index, nativeCell(pair, 0), nativeCell(pair, 1), pair]); orderRecords.push({path, order_index, pair});}); for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path});}
  return [{id: `${prefix}_powl_nodes`, title: `Every ${prefix} POWL node and frequency`, columns: ["path", "parent", "kind", "kind_index", "label", "minimum", "maximum", "child_count", "complete_node"], rows, records}, {id: `${prefix}_powl_order`, title: `Every stored ${prefix} POWL order pair`, columns: ["path", "order_index", "source_child", "target_child", "complete_pair"], rows: orderRows, records: orderRecords}];
}
export function modelConversionTables(value: ModelConversionWorkbench, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = nativeObjectTables(value);
  for (const [prefix, model] of [["original", value.model], ["result", value.result]] as const) {if (model?.process_tree) tables.push(...treeTables(model.process_tree, prefix)); if (model?.powl) tables.push(...powlTables(model.powl, prefix));}
  const reduction = value.result?.reduction;
  if (reduction) for (const key of ["old_to_new_place", "old_to_new_transition"] as const) tables.push({id: `complete_${key}`, title: `Every ${key.replace(/_/g, " ")} mapping`, columns: ["original_index", "returned_index", "removed"], rows: reduction[key].map((_, index) => [index, nativeCell(reduction[key], index), integer(reduction[key], index) === unsignedMax]), records: reduction[key].map((_, index) => parseNativeJson(`{"original_index":${index},"returned_index":${nativeCell(reduction[key], index)},"removed":${integer(reduction[key], index) === unsignedMax}}`))});
  const submitted = fields ?? nativeResponseRequest(value)?.fields; if (submitted) tables.push({id: "request", title: "Exact submitted model conversion options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))});
  // A selected primitive cannot carry its array holder's numeric token sidecar.
  // Wrap it locally so selected-row JSON keeps exact markings and mapping indexes.
  for (const table of tables) {const records = table.records; if (records) table.records = records.map((record, index) => record !== null && typeof record === "object" ? record : parseNativeJson(`{"value":${typeof record === "number" ? nativeCell(records, index) : nativeJson(record)}}`));}
  return tables;
}
export function modelConversionAllTablesCsv(tables: DiagnosticTable[]): string {
  const rows: unknown[][] = []; for (const table of tables) {if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined])); table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));}
  return diagnosticCsv({id: "all_model_conversion_tables", title: "Every complete model conversion table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export function modelConversionPowlGraph(powl: NativePowlNode): ModelGraph {
  const nodes: ModelGraph["nodes"] = [], edges: ModelGraph["edges"] = [], pending = [{node: powl, path: "root", parent: "", child_index: 0}];
  while (pending.length) {const {node, path, parent, child_index} = pending.pop()!; nodes.push({id: path, label: node.kind === "TRANSITION" ? node.label : node.kind === "SILENT" ? "τ" : node.kind, start: parent === "", detail: `Kind ${node.kind} (${nativeCell(node, "kind_index")}) · label ${nativeJson(node.label)} · frequency ${nativeCell(node.frequency, "minimum")}…${node.frequency.maximum === null ? "unbounded" : nativeCell(node.frequency, "maximum")}`}); if (parent) edges.push({id: `${parent}>${path}`, source: parent, target: path, label: `child ${child_index}`, value: 1, directed: true}); node.order.forEach((pair, index) => edges.push({id: `${path}:order:${index}`, source: `${path}/${nativeCell(pair, 0)}`, target: `${path}/${nativeCell(pair, 1)}`, label: "order", value: 1, directed: true})); for (let i = node.children.length - 1; i >= 0; --i) pending.push({node: node.children[i], path: `${path}/${i}`, parent: path, child_index: i});}
  return {nodes, edges};
}
function netGraph(net: Record<string, unknown>): ModelGraph {
  const nodes: ModelGraph["nodes"] = [], edges: ModelGraph["edges"] = [];
  for (let i = 0; i < Number(net.place_count); ++i) nodes.push({id: `p:${i}`, label: `Place ${i}`, start: !!net.has_source && net.source === i, end: !!net.has_sink && net.sink === i, detail: `Native place ${i}`});
  for (const transition of net.transitions as Array<Record<string, unknown>>) {const index = Number(transition.index), id = `t:${index}`; nodes.push({id, label: transition.activity_is_null ? "τ" : String(transition.activity_name), detail: `Transition ${index} · activity ${nativeJson(transition.activity_name)} · display name ${nativeJson(transition.display_name)}`}); for (const [key, weightsKey, name, output] of [["inputs", "input_weights", "input", false], ["outputs", "output_weights", "output", true], ["reset_inputs", "reset_weights", "reset", false], ["inhibitor_inputs", "inhibitor_weights", "inhibitor", false]] as const) {const weights = transition[weightsKey] as Array<{negative: boolean; magnitude: number}>; (transition[key] as number[]).forEach((place, occurrence) => {const weight = weights[occurrence], text = weight ? (weight.negative ? "-" : "") + nativeCell(weight, "magnitude") : "1"; edges.push({id: `${name}:${index}:${occurrence}`, source: output ? id : `p:${place}`, target: output ? `p:${place}` : id, label: name === "reset" || name === "inhibitor" ? `${name} ${text}` : text, value: 1, directed: true});});}}
  return {nodes, edges};
}
export function modelConversionGraphs(value: ModelConversionWorkbench): Array<{title: string; graph: ModelGraph}> {
  const graphs: Array<{title: string; graph: ModelGraph}> = [];
  for (const [name, model] of [["Original", value.model], ["Resulting", value.result]] as const) {if (model?.process_tree) graphs.push({title: `${name} process tree`, graph: processTreeAlignmentTreeGraph(model.process_tree)}); if (model?.powl) graphs.push({title: `${name} POWL model`, graph: modelConversionPowlGraph(model.powl)}); const net = model?.petri_net ?? (model && "reduction" in model ? model.reduction?.net : null); if (net) graphs.push({title: `${name} Petri net`, graph: netGraph(net)});}
  return graphs;
}
export class ModelConversionRequestGate {private generation = 0; issue(): number {return ++this.generation;} invalidate(): void {++this.generation;} accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}}
export interface ModelConversionRun {value: ModelConversionWorkbench; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; http_status: number; input: {model: File; model_sha256: string};}
async function fileHash(file: File): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");}
export async function runModelConversionWorkbench(model: File, form: ModelConversionForm, signal?: AbortSignal): Promise<ModelConversionRun> {
  const fields = modelConversionFields(form), model_sha256 = await fileHash(model); signal?.throwIfAborted(); const body = new FormData(); body.append("model_file", model); fields.forEach(([key, value]) => body.append(key, value)); const path = "/api/model-conversion-workbench", response = await fetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!isModelConversionWorkbench(value)) {if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Model conversion failed (${response.status}).`); throw new Error("The server did not return a valid native model conversion envelope.");}
  if (value.operation !== form.operation || value.input_context.format !== form.modelFormat || value.input_context.dataset !== model.name || integer(value.input_context, "original_model_bytes") !== BigInt(model.size)) throw new Error("The model conversion response does not match the submitted original model and operation.");
  const original = new Uint8Array(await model.arrayBuffer()), captured = workflowInputNativeBytes(value.model.original_model); signal?.throwIfAborted(); if (original.length !== captured.length || original.some((byte, index) => byte !== captured[index])) throw new Error("The model conversion response does not retain the submitted original model bytes.");
  for (const [key, raw] of fields) {
    if (key.startsWith("pnml_") && value.pnml_import_options?.[key.slice(5) as keyof NonNullable<ModelConversionWorkbench["pnml_import_options"]>] !== ["true", "1"].includes(raw.toLowerCase())) throw new Error("The model conversion response does not match the PNML import options.");
    if (!key.startsWith("petri_reduction_")) continue;
    const nativeKey = key.replace(/^petri_reduction_/, "").replace(/_json$/, "").replace(/^parallel_place_fusion$/, "enable_parallel_place_fusion").replace(/^abstraction$/, "enable_abstraction");
    const actual = value.options?.[nativeKey as keyof ModelReductionOptions], control = MODEL_CONVERSION_REDUCTION_CONTROLS.find(item => item.key === key)!;
    const expected = control.kind === "boolean" ? ["true", "1"].includes(raw) : control.kind === "marking" ? (parseNativeJson(raw) as number[]).map((_, index, array) => integer(array, index, true)!) : legacyMarking(raw)!;
    const matches = control.kind === "boolean" ? actual === expected : Array.isArray(actual) && actual.length === (expected as bigint[]).length && actual.every((_, index) => integer(actual, index, true) === (expected as bigint[])[index]); if (!matches) throw new Error("The model conversion response does not match the submitted reduction options.");
  }
  if (form.operation === "reduce_petri") for (const name of ["initial", "final"] as const) {const json = form.fields[`petri_reduction_${name}_marking_json`], legacy = form.fields[`petri_reduction_${name}_marking`], expected = json ? (parseNativeJson(json) as unknown[]).length === 0 ? "explicit_empty_unspecified" : "explicit_json" : legacy ? "explicit_legacy" : "pnml"; if (value.input_context[`${name}_marking_origin`] !== expected) throw new Error("The model conversion response does not match the submitted marking origins.");}
  if (!response.ok && value.error === null) throw new Error(`The server returned a model conversion result with failure status ${response.status}.`);
  return {value, raw: nativeResponseJson(value), fields, tables: modelConversionTables(value, fields), http_status: response.status, input: {model, model_sha256}};
}
export function modelConversionReport(run: ModelConversionRun): string {return nativeJson({family: "MODEL_CONVERSION_WORKBENCH_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/model-conversion-workbench", fields: run.fields}, input: {name: run.input.model.name, size: run.input.model.size, last_modified: run.input.model.lastModified, sha256: run.input.model_sha256}, native_result: run.value}, 2);}
export function modelConversionArtifactFile(run: ModelConversionRun, artifact: ModelConversionArtifact): File {const text = run.value.exports[artifact]; if (text === null) throw new Error(run.value.exports[`${artifact}_error`] ?? `This run did not return ${artifact}.`); const format = artifact.split("_")[1]; return new File([workflowInputNativeBytes(text)], `minera-${run.value.operation}-${artifact}.${format}`, {type: format === "powl" ? "text/plain;charset=utf-8" : "application/xml"});}
export function modelConversionSummary(value: ModelConversionWorkbench): string {if (value.error) return `Conversion failed: ${value.error.type}: ${value.error.message}. Original model, native options and available exports remain inspectable.`; const reduction = value.result?.reduction; return reduction ? `Native reduction returned · changed ${reduction.changed} · ${reduction.old_to_new_place.length} original place mappings and ${reduction.old_to_new_transition.length} original transition mappings. The complete resulting net and markings are retained.` : "Native conversion returned. The original model and complete resulting model are retained separately.";}
export const modelConversionTableFileName = petriAntiMultiTableFileName;
export const MODEL_CONVERSION_TREE_EXAMPLE = '{"activities":["A","東京"],"root":{"op":"SEQUENCE","op_index":2,"activity":0,"children":[{"op":"ACTIVITY","op_index":0,"activity":0,"children":[]},{"op":"ACTIVITY","op_index":0,"activity":1,"children":[]}]}}';
export const MODEL_CONVERSION_POWL_EXAMPLE = 'PO=(nodes={A,B},order={A-->B})';
