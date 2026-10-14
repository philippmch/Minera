import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import type {ModelGraph} from "./model-graph";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {processTreeAlignmentMilliseconds} from "./process-tree-alignment-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "./token-replay-workbench";
import {WORKFLOW_INPUT_XES_VARIANTS, isWorkflowInputNativeNet, isWorkflowInputTable, workflowInputNativeBytes, type WorkflowInputTable} from "./workflow-input-workbench";

export const PETRI_ANTI_MULTI_ALGORITHMS = ["anti", "multi"] as const;
export type PetriAntiMultiAlgorithm = typeof PETRI_ANTI_MULTI_ALGORITHMS[number];
export const PETRI_ANTI_MULTI_VARIANT = "VERSION_DISCOUNTED_A_STAR";
export const PETRI_ANTI_MULTI_VARIANT_ALIASES = [PETRI_ANTI_MULTI_VARIANT, "version_discounted_a_star", "discounted_a_star"] as const;
export const PETRI_ANTI_MULTI_INPUT_FORMATS = ["auto", "table", "trace_words"] as const;
export const PETRI_ANTI_MULTI_INPUT_SHAPES = ["log", "trace", "words"] as const;
export const PETRI_ANTI_MULTI_MODEL_FORMATS = ["auto", "pnml"] as const;
export const PETRI_ANTI_MULTI_PNML_CONTROLS = [
  ["pnml_infer_initial_marking", "Infer initial marking", true], ["pnml_infer_final_marking", "Infer final marking", true],
  ["pnml_import_stochastic_map", "Import stochastic transition metadata", false], ["pnml_require_workflow_boundaries", "Require workflow boundaries", true],
  ["pnml_public_marking_semantics", "Use public marking semantics", false],
] as const;
export interface PetriAntiMultiForm {
  algorithm: PetriAntiMultiAlgorithm; inputFormat: typeof PETRI_ANTI_MULTI_INPUT_FORMATS[number]; inputShape: typeof PETRI_ANTI_MULTI_INPUT_SHAPES[number];
  modelFormat: typeof PETRI_ANTI_MULTI_MODEL_FORMATS[number]; traceIndex: string; fields: Record<string, string>; importFields: Record<string, string>;
  pnmlFields: Record<string, string>; explicitEmptySelectors: Record<string, boolean>;
}
export const defaultPetriAntiMultiForm = (algorithm: PetriAntiMultiAlgorithm = "anti"): PetriAntiMultiForm => ({algorithm, inputFormat: "auto", inputShape: "log", modelFormat: "auto", traceIndex: "", fields: {}, importFields: {}, pnmlFields: {}, explicitEmptySelectors: {}});
export interface PetriAntiMultiControl {key: string; label: string; kind: "selector" | "integer" | "number" | "marking" | "legacy_marking";}
const commonControls: readonly PetriAntiMultiControl[] = [
  {key: "alignment_case_id_attribute", label: "Typed-table case attribute", kind: "selector"},
  {key: "activity_attribute", label: "Alignment activity attribute", kind: "selector"},
  {key: "exponent", label: "Discount exponent", kind: "number"},
  {key: "marking_limit", label: "Closed-marking revisit limit", kind: "integer"},
  {key: "max_states", label: "Closed-marking revisit limit override (0 uses marking limit)", kind: "integer"},
  {key: "initial_marking_json", label: "Initial marking (exact JSON unsigned integer array)", kind: "marking"},
  {key: "final_marking_json", label: "Final marking (exact JSON unsigned integer array)", kind: "marking"},
  {key: "initial_marking", label: "Initial marking (legacy comma-separated integers)", kind: "legacy_marking"},
  {key: "final_marking", label: "Final marking (legacy comma-separated integers)", kind: "legacy_marking"},
];
export const PETRI_ANTI_MULTI_CONTROLS: Record<PetriAntiMultiAlgorithm, readonly PetriAntiMultiControl[]> = {
  anti: [...commonControls.slice(0, 3), {key: "epsilon", label: "Anti-alignment length divisor epsilon", kind: "number"}, ...commonControls.slice(3)], multi: commonControls,
};
export type PetriAntiMultiDouble = number | "NaN" | "INF" | "-INF";
export interface NativePetriAntiMultiResult {
  activities: string[]; transitions: number[]; cost: PetriAntiMultiDouble; objective: PetriAntiMultiDouble; precision?: PetriAntiMultiDouble; max_distance_to_log?: PetriAntiMultiDouble;
  diagnostics: {visited_states: number; queued_states: number; traversed_arcs: number; exhausted: boolean};
}
export interface NativePetriAntiMultiOptions {
  variant: typeof PETRI_ANTI_MULTI_VARIANT | "UNKNOWN"; variant_index: number; exponent: PetriAntiMultiDouble; epsilon?: PetriAntiMultiDouble;
  marking_limit: number; max_states: number; initial_marking: number[]; final_marking: number[]; activity_attribute: string;
}
export type PetriAntiMultiMarkingOrigin = "pnml" | "explicit_empty_native_default" | "explicit_json" | "explicit_legacy";
export interface PetriAntiMultiWorkbench {
  family: "PETRI_ANTI_MULTI_ALIGNMENT_WORKBENCH"; algorithm: PetriAntiMultiAlgorithm; variant: typeof PETRI_ANTI_MULTI_VARIANT;
  input_shape: PetriAntiMultiForm["inputShape"]; result_shape: "aggregate"; executed: boolean; error: null | {type: string; message: string};
  input_context: {format: PetriAntiMultiForm["inputFormat"]; detected_format: string; dataset: string; origin: string; original_input_bytes: number; input_event_count: number;
    input_case_count: number | null; checked_event_count: number | null; checked_case_count: number | null; trace_index: number | null; activity_attribute: string; case_id_attribute: string};
  model_context: {format: PetriAntiMultiForm["modelFormat"]; detected_format: "pnml"; dataset: string; original_input_bytes: number; native_document: Record<string, unknown>};
  options_context: NativePetriAntiMultiOptions;
  pnml_import_options: {infer_initial_marking: boolean; infer_final_marking: boolean; import_stochastic_map: boolean; require_workflow_boundaries: boolean; public_marking_semantics: boolean};
  marking_context: {initial_origin: PetriAntiMultiMarkingOrigin; final_origin: PetriAntiMultiMarkingOrigin; effective_initial: number[] | null; effective_final: number[] | null};
  native_net: Record<string, unknown>; input_before: WorkflowInputTable | null; input_after: WorkflowInputTable | null; input_trace_words: string[] | null;
  input_log: Record<string, unknown> | null; input_log_error: string | null; checked_log: Record<string, unknown> | null; checked_log_error: string | null;
  result: NativePetriAntiMultiResult | null; source_xes: string | null; source_xes_error: string | null;
  pnml: string | null; pnml_error: string | null; checked_pnml: string | null; checked_pnml_error: string | null;
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const strings = (value: unknown): value is string[] => Array.isArray(value) && value.every(item => typeof item === "string");
const unsigned = (holder: object, key: string | number): bigint | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const text = String(nativeCell(holder, key)); if (!/^(?:0|[1-9]\d*)$/.test(text)) return undefined; const value = BigInt(text); return value <= 18446744073709551615n ? value : undefined;
};
const marking = (value: unknown): value is number[] => Array.isArray(value) && value.every((_, i) => unsigned(value, i) !== undefined);
const nativeDouble = (value: unknown): value is PetriAntiMultiDouble => typeof value === "number" && Number.isFinite(value) || value === "NaN" || value === "INF" || value === "-INF";
/** Parse finite C numeric syntax without narrowing native semantic domains such as exponent <= 1. */
export function petriAntiMultiNumber(raw: string): number | undefined {
  const text = raw.trim(), negative = text.startsWith("-"); if (negative && /^[+-]/.test(text.slice(1))) return undefined;
  const value = processTreeAlignmentMilliseconds(negative ? text.slice(1) : text);
  return value === undefined ? undefined : negative ? -value : value;
}
export function isNativePetriAntiMultiResult(value: unknown, algorithm: PetriAntiMultiAlgorithm): value is NativePetriAntiMultiResult {
  return object(value) && strings(value.activities) && marking(value.transitions) && nativeDouble(value.cost) && nativeDouble(value.objective) && nativeDouble(value[algorithm === "anti" ? "precision" : "max_distance_to_log"]) && object(value.diagnostics) &&
    ["visited_states", "queued_states", "traversed_arcs"].every(key => unsigned(value.diagnostics as object, key) !== undefined) && typeof value.diagnostics.exhausted === "boolean";
}
export function isNativePetriAntiMultiOptions(value: unknown, algorithm: PetriAntiMultiAlgorithm): value is NativePetriAntiMultiOptions {
  if (!object(value) || typeof value.variant_index !== "number" || !/^-?(?:0|[1-9]\d*)$/.test(String(nativeCell(value, "variant_index")))) return false;
  const index = BigInt(String(nativeCell(value, "variant_index"))); if (index < -2147483648n || index > 2147483647n || value.variant !== (index === 0n ? PETRI_ANTI_MULTI_VARIANT : "UNKNOWN")) return false;
  return nativeDouble(value.exponent) && (algorithm === "anti" ? nativeDouble(value.epsilon) : value.epsilon === undefined) && ["marking_limit", "max_states"].every(key => unsigned(value, key) !== undefined) && marking(value.initial_marking) && marking(value.final_marking) && typeof value.activity_attribute === "string";
}
function legacyMarking(text: string): string[] | undefined {
  const items = text.split(",").map(item => item.trim()); return items.every(item => /^\d+$/.test(item) && BigInt(item) <= 18446744073709551615n) ? items.map(item => BigInt(item).toString()) : undefined;
}
export function petriAntiMultiControlFields(algorithm: PetriAntiMultiAlgorithm, values: Record<string, string>, explicitEmpty: Record<string, boolean> = {}): {fields: Array<[string, string]>; selectors: Record<string, string>} {
  if (!PETRI_ANTI_MULTI_ALGORITHMS.includes(algorithm)) throw new Error("Select anti-alignment or multi-alignment.");
  const fields: Array<[string, string]> = [], selectors: Record<string, string> = Object.create(null);
  for (const name of ["initial", "final"]) if (values[name + "_marking"] && values[name + "_marking_json"]) throw new Error(`Choose one ${name} marking carrier: JSON or legacy text.`);
  for (const control of PETRI_ANTI_MULTI_CONTROLS[algorithm]) {
    const value = values[control.key] ?? "";
    if (control.kind === "selector") {if (value !== "" || explicitEmpty[control.key]) selectors[control.key] = value; continue;}
    if (value === "") continue;
    if (control.kind === "integer" && (!/^\d+$/.test(value.trim()) || BigInt(value.trim()) > 18446744073709551615n)) throw new Error(`${control.label} must be an unsigned integer.`);
    if (control.kind === "number" && petriAntiMultiNumber(value) === undefined) throw new Error(`${control.label} must be a finite number.`);
    if (control.kind === "marking") {let valueObject: unknown; try {valueObject = parseNativeJson(value);} catch {throw new Error(`${control.label} must be valid JSON.`);} if (!marking(valueObject)) throw new Error(`${control.label} must be an exact JSON unsigned integer array.`);}
    if (control.kind === "legacy_marking" && !legacyMarking(value)) throw new Error(`${control.label} must contain comma-separated unsigned integers.`);
    fields.push([control.key, value]);
  }
  return {fields, selectors};
}
export function petriAntiMultiFields(form: PetriAntiMultiForm, inputName: string): Array<[string, string]> {
  if (!PETRI_ANTI_MULTI_ALGORITHMS.includes(form.algorithm) || !PETRI_ANTI_MULTI_INPUT_FORMATS.includes(form.inputFormat) || !PETRI_ANTI_MULTI_INPUT_SHAPES.includes(form.inputShape) || !PETRI_ANTI_MULTI_MODEL_FORMATS.includes(form.modelFormat)) throw new Error("Select supported anti/multi alignment inputs.");
  if (form.inputFormat === "trace_words" ? form.inputShape !== "words" : form.inputShape === "words") throw new Error("Literal trace words require the trace-word input format and shape.");
  const variant = form.fields.variant || PETRI_ANTI_MULTI_VARIANT; if (!PETRI_ANTI_MULTI_VARIANT_ALIASES.some(value => value === variant)) throw new Error("Select the discounted A* variant.");
  const fields: Array<[string, string]> = [["algorithm", form.algorithm], ["variant", variant], ["input_format", form.inputFormat], ["input_shape", form.inputShape], ["model_format", form.modelFormat]];
  if (form.inputShape === "trace" && form.traceIndex !== "") {if (!/^\d+$/.test(form.traceIndex.trim()) || BigInt(form.traceIndex.trim()) > 18446744073709551615n) throw new Error("Trace index must be an unsigned integer."); fields.push(["trace_index", form.traceIndex]);}
  if (form.inputFormat === "auto") {
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) {const value = form.importFields[key]; if (value !== undefined && value !== "") fields.push([key, value]);}
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) {const value = form.importFields[key]; if (value === undefined || value === "") continue; if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`); fields.push([key, value]);}
    if (form.importFields.xes_variant) {if (!WORKFLOW_INPUT_XES_VARIANTS.some(value => value === form.importFields.xes_variant)) throw new Error("Select a supported XES importer."); fields.push(["xes_variant", form.importFields.xes_variant]);}
    const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(inputName) ? "\t" : ""); if (delimiter) {if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character."); fields.push(["delimiter", delimiter]);}
  }
  for (const [key] of PETRI_ANTI_MULTI_PNML_CONTROLS) {const value = form.pnmlFields[key]; if (value === undefined || value === "") continue; if (!["true", "false", "1", "0"].includes(value.toLowerCase())) throw new Error(`${key} must be true, false, 1 or 0.`); fields.push([key, value]);}
  const controls = petriAntiMultiControlFields(form.algorithm, form.fields, form.explicitEmptySelectors); fields.push(...controls.fields); if (Object.keys(controls.selectors).length) fields.push(["alignment_selectors_json", JSON.stringify(controls.selectors)]); return fields;
}
const countLiteral = (holder: object, key: string): boolean => typeof (holder as Record<string, unknown>)[key] === "number" && /^(?:0|[1-9]\d*)$/.test(String(nativeCell(holder, key)));
export function isPetriAntiMultiWorkbench(value: unknown): value is PetriAntiMultiWorkbench {
  if (!object(value) || value.family !== "PETRI_ANTI_MULTI_ALIGNMENT_WORKBENCH" || !PETRI_ANTI_MULTI_ALGORITHMS.some(algorithm => value.algorithm === algorithm) || value.variant !== PETRI_ANTI_MULTI_VARIANT || !PETRI_ANTI_MULTI_INPUT_SHAPES.some(shape => value.input_shape === shape) || value.result_shape !== "aggregate" || typeof value.executed !== "boolean" || !object(value.input_context) || !object(value.model_context) || !object(value.marking_context) || !object(value.pnml_import_options)) return false;
  const algorithm = value.algorithm as PetriAntiMultiAlgorithm, input = value.input_context, model = value.model_context;
  if (!isNativePetriAntiMultiOptions(value.options_context, algorithm) || value.options_context.variant !== value.variant || !isWorkflowInputNativeNet(value.native_net)) return false;
  if (!PETRI_ANTI_MULTI_INPUT_FORMATS.some(format => input.format === format) || !["detected_format", "dataset", "origin", "activity_attribute", "case_id_attribute"].every(key => typeof input[key] === "string") || !["original_input_bytes", "input_event_count"].every(key => countLiteral(input, key)) || !["input_case_count", "checked_event_count", "checked_case_count", "trace_index"].every(key => input[key] === null || countLiteral(input, key))) return false;
  if (input.format === "trace_words" ? value.input_shape !== "words" : value.input_shape === "words") return false;
  if (!PETRI_ANTI_MULTI_MODEL_FORMATS.some(format => model.format === format) || model.detected_format !== "pnml" || typeof model.dataset !== "string" || !countLiteral(model, "original_input_bytes") || !object(model.native_document)) return false;
  if (!PETRI_ANTI_MULTI_PNML_CONTROLS.every(([key]) => typeof (value.pnml_import_options as Record<string, unknown>)[key.slice(5)] === "boolean")) return false;
  if (!["initial_origin", "final_origin"].every(key => ["pnml", "explicit_empty_native_default", "explicit_json", "explicit_legacy"].includes(String((value.marking_context as Record<string, unknown>)[key]))) || !["effective_initial", "effective_final"].every(key => (value.marking_context as Record<string, unknown>)[key] === null || marking((value.marking_context as Record<string, unknown>)[key]))) return false;
  if (!["input_before", "input_after"].every(key => value[key] === null || isWorkflowInputTable(value[key])) || !(value.input_trace_words === null || strings(value.input_trace_words)) || !["input_log", "checked_log"].every(key => value[key] === null || object(value[key]))) return false;
  if (!["input_log_error", "checked_log_error", "source_xes", "source_xes_error", "pnml", "pnml_error", "checked_pnml", "checked_pnml_error"].every(key => value[key] === null || typeof value[key] === "string")) return false;
  if (value.error !== null) return value.result === null && object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string";
  return value.executed && isNativePetriAntiMultiResult(value.result, algorithm);
}
export function petriAntiMultiTables(value: PetriAntiMultiWorkbench, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = nativeObjectTables(value), result = value.result;
  if (result) {
    for (const key of ["activities", "transitions"] as const) tables.push({id: `complete_${key}`, title: key === "activities" ? "Every returned visible model activity" : "Every returned native transition (includes silent transitions)", columns: ["sequence_index", "value"], rows: result[key].map((_, i) => [i, nativeCell(result[key], i)]), records: result[key]});
  }
  for (const key of ["input_before", "input_after"] as const) {
    const table = value[key]; if (!table) continue; const rows: unknown[][] = [], records: unknown[] = [];
    table.rows.forEach((row, row_index) => row.forEach((cell, column_index) => {rows.push([row_index, column_index, table.columns[column_index], table.dtypes?.[column_index], cell === null ? "missing" : cell.null ? "present null" : "present", cell?.kind, cell?.value, cell === null ? undefined : nativeCell(cell, "typed_value"), cell]); records.push({row_index, column_index, column: table.columns[column_index], dtype: table.dtypes?.[column_index], cell});}));
    tables.push({id: `${key}_cells`, title: `Every ${key === "input_before" ? "original" : "resulting"} typed input cell`, columns: ["row_index", "column_index", "column", "dtype", "presence", "kind", "lexical_value", "typed_value", "complete_cell"], rows, records});
  }
  const submitted = fields ?? nativeResponseRequest(value)?.fields; if (submitted) tables.push({id: "request", title: "Exact submitted anti/multi alignment options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))}); return tables;
}
export function petriAntiMultiAllTablesCsv(tables: DiagnosticTable[]): string {
  const rows: unknown[][] = [];
  for (const table of tables) {if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined])); table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));}
  return diagnosticCsv({id: "all_petri_anti_multi_tables", title: "Every complete anti/multi alignment table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
/** Render the captured original structural net without requiring a successful alignment. */
export function petriAntiMultiGraph(value: PetriAntiMultiWorkbench): ModelGraph {
  const net = value.native_net, count = Number(net.place_count), nodes: ModelGraph["nodes"] = [], edges: ModelGraph["edges"] = [];
  for (let i = 0; i < count; ++i) nodes.push({id: `p:${i}`, label: `Place ${i}`, detail: `Original PNML place ${i}${net.has_source && net.source === i ? " · source" : ""}${net.has_sink && net.sink === i ? " · sink" : ""}`, start: !!net.has_source && net.source === i, end: !!net.has_sink && net.sink === i});
  for (const transition of net.transitions as Array<Record<string, unknown>>) {
    const index = Number(transition.index), id = `t:${index}`; nodes.push({id, label: transition.activity_is_null ? "τ" : String(transition.activity_name), detail: `Transition ${index} · ${transition.activity_is_null ? "silent" : "activity " + nativeJson(transition.activity_name)} · display name ${nativeJson(transition.display_name)}`});
    for (const [key, weightsKey, name, output] of [["inputs", "input_weights", "input", false], ["outputs", "output_weights", "output", true], ["reset_inputs", "reset_weights", "reset", false], ["inhibitor_inputs", "inhibitor_weights", "inhibitor", false]] as const) {
      const weights = transition[weightsKey] as Array<{negative: boolean; magnitude: number}>;
      (transition[key] as number[]).forEach((place, i) => {const weight = weights[i], text = weight ? (weight.negative ? "-" : "") + nativeCell(weight, "magnitude") : "1"; edges.push({id: `${name}:${index}:${i}`, source: output ? id : `p:${place}`, target: output ? `p:${place}` : id, label: name === "reset" || name === "inhibitor" ? `${name} ${text}` : text, value: 1, directed: true});});
    }
  }
  return {nodes, edges};
}
export class PetriAntiMultiRequestGate {private generation = 0; issue(): number {return ++this.generation;} invalidate(): void {++this.generation;} accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}}
export interface PetriAntiMultiRun {value: PetriAntiMultiWorkbench; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; http_status: number; inputs: {input: File; model: File; input_sha256: string; model_sha256: string};}
async function fileHash(file: File): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");}
export async function runPetriAntiMultiWorkbench(input: File, model: File, form: PetriAntiMultiForm, signal?: AbortSignal): Promise<PetriAntiMultiRun> {
  const fields = petriAntiMultiFields(form, input.name), [input_sha256, model_sha256] = await Promise.all([fileHash(input), fileHash(model)]); signal?.throwIfAborted();
  const body = new FormData(); body.append("input_file", input); body.append("model_file", model); fields.forEach(([key, value]) => body.append(key, value)); const path = "/api/align-petri-anti-multi-workbench", response = await fetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!isPetriAntiMultiWorkbench(value)) {if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Anti/multi alignment failed (${response.status}).`); throw new Error("The server did not return a valid native anti/multi alignment envelope.");}
  if (value.algorithm !== form.algorithm || value.input_shape !== form.inputShape || value.input_context.format !== form.inputFormat || value.model_context.format !== form.modelFormat || value.input_context.dataset !== input.name || value.model_context.dataset !== model.name || BigInt(String(nativeCell(value.input_context, "original_input_bytes"))) !== BigInt(input.size) || BigInt(String(nativeCell(value.model_context, "original_input_bytes"))) !== BigInt(model.size)) throw new Error("The anti/multi alignment response does not match the submitted files and inputs.");
  const controls = petriAntiMultiControlFields(form.algorithm, form.fields, form.explicitEmptySelectors);
  for (const [key, expected] of Object.entries(controls.selectors)) if (value.input_context[key === "alignment_case_id_attribute" ? "case_id_attribute" : "activity_attribute"] !== expected || key === "activity_attribute" && value.options_context.activity_attribute !== expected) throw new Error("The anti/multi alignment response does not match the submitted literal selectors.");
  for (const [key, raw] of controls.fields) {
    const kind = PETRI_ANTI_MULTI_CONTROLS[form.algorithm].find(control => control.key === key)!.kind, nativeKey = key.replace(/_json$/, ""), actual = value.options_context[nativeKey as keyof NativePetriAntiMultiOptions];
    const matches = kind === "number" ? Object.is(actual, petriAntiMultiNumber(raw)) : kind === "integer" ? unsigned(value.options_context, key) === BigInt(raw.trim()) : nativeJson(actual) === (kind === "marking" ? nativeJson(parseNativeJson(raw)) : `[${legacyMarking(raw)!.join(",")}]`);
    if (!matches) throw new Error("The anti/multi alignment response does not match the submitted native options.");
  }
  for (const name of ["initial", "final"] as const) {
    const json = form.fields[`${name}_marking_json`], legacy = form.fields[`${name}_marking`];
    const expected = json ? (parseNativeJson(json) as unknown[]).length === 0 ? "explicit_empty_native_default" : "explicit_json" : legacy ? "explicit_legacy" : "pnml";
    if (value.marking_context[`${name}_origin`] !== expected) throw new Error("The anti/multi alignment response does not match the submitted marking origins.");
  }
  for (const [key] of PETRI_ANTI_MULTI_PNML_CONTROLS) if (form.pnmlFields[key] && value.pnml_import_options[key.slice(5) as keyof PetriAntiMultiWorkbench["pnml_import_options"]] !== ["true", "1"].includes(form.pnmlFields[key].toLowerCase())) throw new Error("The anti/multi alignment response does not match the PNML import options.");
  if (form.inputShape === "trace" && form.traceIndex !== "" && unsigned(value.input_context, "trace_index") !== BigInt(form.traceIndex.trim())) throw new Error("The anti/multi alignment response does not match the selected original trace.");
  if (!response.ok && value.error === null) throw new Error(`The server returned an anti/multi result with failure status ${response.status}.`);
  return {value, raw: nativeResponseJson(value), fields, tables: petriAntiMultiTables(value, fields), http_status: response.status, inputs: {input, model, input_sha256, model_sha256}};
}
export function petriAntiMultiReport(run: PetriAntiMultiRun): string {const metadata = (file: File, sha256: string) => ({name: file.name, size: file.size, last_modified: file.lastModified, sha256}); return nativeJson({family: "PETRI_ANTI_MULTI_ALIGNMENT_WORKBENCH_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/align-petri-anti-multi-workbench", fields: run.fields}, inputs: {input: metadata(run.inputs.input, run.inputs.input_sha256), model: metadata(run.inputs.model, run.inputs.model_sha256)}, native_result: run.value}, 2);}
export function petriAntiMultiArtifactFile(run: PetriAntiMultiRun, format: "pnml" | "checked_pnml" | "source_xes"): File {const text = run.value[format]; if (text === null) throw new Error(run.value[`${format}_error`] ?? `This run did not return ${format}.`); return new File([workflowInputNativeBytes(text)], `minera-${run.value.algorithm}-${format}.${format === "source_xes" ? "xes" : "pnml"}`, {type: "application/xml"});}
export function petriAntiMultiSummary(value: PetriAntiMultiWorkbench): string {
  if (value.error !== null) return `Alignment failed: ${value.error.type}: ${value.error.message}. Original inputs, model and native options remain inspectable.`;
  const result = value.result!; return `${value.algorithm === "anti" ? "Anti-alignment" : "Multi-alignment"} aggregate result · objective ${nativeCell(result, "objective")} · ${value.algorithm === "anti" ? "precision " + nativeCell(result, "precision") : "maximum ordinary distance " + nativeCell(result, "max_distance_to_log")} · ${result.activities.length} visible activities and ${result.transitions.length} transitions.${result.diagnostics.exhausted ? " The closed-marking revisit bound pruned search branches." : ""}`;
}
export function petriAntiMultiTableFileName(id: string): string {let encoded = ""; for (let i = 0; i < id.length; ++i) encoded += /^[A-Za-z0-9_.-]$/.test(id[i]) ? id[i] : "%" + id.charCodeAt(i).toString(16).padStart(4, "0"); return encoded;}
export const PETRI_ANTI_MULTI_TRACE_EXAMPLE = nativeJson(["A", "A", "", "東京,blue;line\nnext", ">>"], 2);
