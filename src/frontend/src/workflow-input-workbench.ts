import {hasExactFilters,hasFullHeuristic,exactFilterFields,appendExactFilterFiles,readExactFilterResponse,OwnedExactFilterResult,type ExactFilterControls,type ExactFilterRun} from "./exact-filter-controls";
import { backendFetch } from "./backend-fetch";
import {originalEditDistanceWireFilename} from "./original-edit-distance-workbench";
import {appendNativeMultipartField, nativeStringBytes as workflowInputNativeBytes} from "./native-multipart";
export {workflowInputNativeBytes};
import type { MineraExport, LogConversionTableCell } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "./native-response";
import { diagnosticCsv, nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";
import { TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS } from "./token-replay-workbench";
import type { ModelGraph } from "./model-graph";

export const WORKFLOW_INPUT_SHAPES = ["log", "stream", "dataframe", "lazyframe", "dfg", "pandas_mutable"] as const;
export const WORKFLOW_INPUT_FORMATS = ["auto", "table", "dfg_json"] as const;
export const WORKFLOW_INPUT_XES_VARIANTS = ["iterparse", "line_by_line", "iterparse_20", "iterparse_mem_compressed", "rustxes", "chunk_regex"] as const;
export type WorkflowInputShape = typeof WORKFLOW_INPUT_SHAPES[number];
export type WorkflowInputFormat = typeof WORKFLOW_INPUT_FORMATS[number];
export type WorkflowInputFamily = "alpha" | "heuristic";
export interface WorkflowInputForm {
  exact?: ExactFilterControls;
  family: WorkflowInputFamily; shape: WorkflowInputShape; format: WorkflowInputFormat;
  alphaVariant: "classic" | "plus"; heuristicVariant: "classic" | "plusplus";
  importFields: Record<string, string>; fields: Record<string, string>; explicitEmptySelectors: Record<string, boolean>;
}
export const defaultWorkflowInputForm = (): WorkflowInputForm => ({family: "alpha", shape: "log", format: "auto", alphaVariant: "classic", heuristicVariant: "classic", importFields: {}, fields: {}, explicitEmptySelectors: {}});
export interface WorkflowInputControl {
  key: string; label: string; kind: "selector" | "text" | "integer" | "number" | "native_number" | "boolean" | "enum" | "json";
  choices?: readonly string[]; plusplus?: boolean;
}
export const WORKFLOW_INPUT_CONTROLS: Record<WorkflowInputFamily, readonly WorkflowInputControl[]> = {
  alpha: [
    {key: "alpha_activity_attribute", label: "Alpha activity attribute", kind: "selector"},
    {key: "alpha_case_id_attribute", label: "Alpha flat-input case attribute", kind: "selector"},
    {key: "alpha_timestamp_attribute", label: "Alpha table completion timestamp attribute", kind: "selector"},
    {key: "alpha_start_timestamp_attribute", label: "Alpha table start timestamp attribute", kind: "selector"},
    {key: "alpha_sort_caseid_required", label: "Alpha Classic table case sorting", kind: "boolean"},
    {key: "alpha_sort_timestamp_along_case_id", label: "Alpha Classic table timestamp sorting", kind: "boolean"},
    {key: "alpha_remove_unconnected", label: "Remove unconnected Alpha+ transitions", kind: "boolean"},
    {key: "alpha_max_rows_to_materialize", label: "Alpha flat-input materialization cap", kind: "integer"},
  ],
  heuristic: [
    {key: "heuristic_activity_attribute", label: "Heuristic activity attribute", kind: "selector"},
    {key: "heuristic_case_id_attribute", label: "Heuristic flat-input case attribute", kind: "selector"},
    {key: "heuristic_timestamp_attribute", label: "Heuristic completion timestamp attribute", kind: "selector"},
    {key: "heuristic_start_timestamp_attribute", label: "Heuristic start timestamp attribute", kind: "selector"},
    {key: "heuristic_lifecycle_instance_attribute", label: "Heuristic lifecycle instance attribute", kind: "selector"},
    {key: "dependency_threshold", label: "Heuristic dependency threshold", kind: "number"},
    {key: "and_threshold", label: "Heuristic AND threshold", kind: "number"},
    {key: "min_edge_frequency", label: "Heuristic minimum edge frequency", kind: "integer"},
    {key: "heuristic_min_activity_count", label: "Heuristic minimum activity count", kind: "integer"},
    {key: "heuristic_connect_all", label: "Connect strongest dependencies", kind: "boolean"},
    {key: "heuristic_decoration", label: "Heuristic decoration", kind: "enum", choices: ["frequency", "performance"]},
    {key: "heuristic_performance_aggregation", label: "Heuristic performance aggregation", kind: "enum", choices: ["mean", "median", "min", "max", "stdev", "sum", "raw_values", "all"]},
    {key: "heuristic_business_hours", label: "Heuristic business hours", kind: "boolean"},
    {key: "heuristic_business_hour_slots", label: "Heuristic weekly business-hour slots (none or start:end pairs)", kind: "text"},
    {key: "heuristic_dfg_pre_cleaning_noise_threshold", label: "Heuristic DFG pre-cleaning noise threshold", kind: "number"},
    {key: "heuristic_loop_length_two_threshold", label: "Heuristic length-two-loop threshold", kind: "number"},
    {key: "heuristic_keep_once_per_case", label: "Count each path once per case", kind: "boolean"},
    {key: "heuristic_most_common_paths", label: "Heuristic most common paths", kind: "text"},
    {key: "heuristic_plusplus_decoration_selector", label: "Heuristics++ literal decoration selector", kind: "selector", plusplus: true},
    {key: "heuristic_plusplus_decoration_value", label: "Heuristics++ decoration value (any JSON value)", kind: "json", plusplus: true},
    {key: "heuristic_plusplus_min_activity_count", label: "Heuristics++ numeric activity cutoff (including NaN/Inf)", kind: "native_number", plusplus: true},
    {key: "heuristic_plusplus_min_frequency", label: "Heuristics++ numeric frequency cutoff (including NaN/Inf)", kind: "native_number", plusplus: true},
  ],
};
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const index = (value: unknown): value is number => typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
const strings = (value: unknown): value is string[] => Array.isArray(value) && value.every(item => typeof item === "string");
const unsigned = (holder: object, key: string | number): bigint | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const value = String(nativeCell(holder, key));
  if (!/^(?:0|[1-9]\d*)$/.test(value)) return undefined;
  const parsed = BigInt(value); return parsed <= 18446744073709551615n ? parsed : undefined;
};
const integerLiteral = (holder: object, key: string | number, signed = false): boolean => typeof (holder as Record<string, unknown>)[String(key)] === "number" && (signed ? /^-?(?:0|[1-9]\d*)$/ : /^(?:0|[1-9]\d*)$/).test(String(nativeCell(holder, key)));
export function workflowInputControlActive(control: WorkflowInputControl, form: WorkflowInputForm): boolean {
  return !control.plusplus || form.heuristicVariant === "plusplus";
}
/** Preserve every numeric, selector and JSON spelling. Blank values omit native options unless explicitly supplied. */
export function workflowInputFields(form: WorkflowInputForm, name: string): Array<[string, string]> {
  if (!["alpha", "heuristic"].includes(form.family) || !WORKFLOW_INPUT_SHAPES.includes(form.shape) || !WORKFLOW_INPUT_FORMATS.includes(form.format)) throw new Error("Select a supported discovery family, input shape and format.");
  if (!["classic", "plus"].includes(form.alphaVariant) || !["classic", "plusplus"].includes(form.heuristicVariant)) throw new Error("Select a supported native discovery variant.");
  const full = form.family === "heuristic" && hasFullHeuristic(form.exact??{});
  const variant = form.family === "alpha" ? form.alphaVariant : form.heuristicVariant;
  if (form.shape === "dfg" && (form.format !== "dfg_json" || !full && variant !== "classic")) throw new Error("Structured DFG input requires Classic discovery and the DFG JSON format.");
  if (form.format === "dfg_json" && form.shape !== "dfg") throw new Error("DFG JSON requires the DFG input shape.");
  if (form.shape === "pandas_mutable" && (form.family !== "heuristic" || !full && variant !== "plusplus")) throw new Error("Mutable pandas input requires Heuristics++.");
  const fields: Array<[string, string]> = [["discovery_family", form.family], ["input_shape", form.shape], ["input_format", form.format]];
  if (!full) fields.push([form.family === "alpha" ? "alpha_variant" : "heuristic_variant", variant]);
  if (form.format === "auto") {
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) {const value = form.importFields[key]; if (value !== undefined && value !== "") fields.push([key, value]);}
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) {
      const value = form.importFields[key]; if (value === undefined || value === "") continue;
      if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`);
      fields.push([key, value]);
    }
    if (form.importFields.xes_variant) {
      if (!WORKFLOW_INPUT_XES_VARIANTS.some(value => value === form.importFields.xes_variant)) throw new Error("Select a supported XES importer.");
      fields.push(["xes_variant", form.importFields.xes_variant]);
    }
    const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(name) ? "\t" : "");
    if (delimiter) {if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character."); fields.push(["delimiter", delimiter]);}
  }
  const selectors: Record<string, string> = Object.create(null);
  for (const control of WORKFLOW_INPUT_CONTROLS[form.family]) {
    if (!workflowInputControlActive(control, form)) continue;
    const value = form.fields[control.key] ?? "";
    if (control.kind === "selector") {if (value !== "" || form.explicitEmptySelectors[control.key]) selectors[control.key] = value; continue;}
    if (value === "") continue;
    if (control.kind === "boolean" && !["true", "false"].includes(value)) throw new Error(`${control.label} must be true or false.`);
    if (control.kind === "enum" && !control.choices?.includes(value)) throw new Error(`${control.label}: select a native value.`);
    if (control.kind === "integer" && !/^\d+$/.test(value.trim())) throw new Error(`${control.label} must be an unsigned integer.`);
    if (control.kind === "number" && (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(value.trim()) || !Number.isFinite(Number(value)))) throw new Error(`${control.label} must be a finite number.`);
    if (control.kind === "native_number" && !/^[+-]?(?:(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?|inf(?:inity)?|nan)$/i.test(value.trim())) throw new Error(`${control.label} must be numeric text, NaN, Inf or -Inf.`);
    if (control.kind === "json") {try {parseNativeJson(value);} catch {throw new Error(`${control.label} must be valid JSON.`);}}
    fields.push([control.key, value]);
  }
  if (Object.keys(selectors).length) fields.push(["discovery_selectors_json", JSON.stringify(selectors)]);
  if (form.exact && hasExactFilters(form.exact)) {
    if (form.family !== "heuristic" || Object.entries(form.exact).some(([k,v])=>k.startsWith("ltl_")&&v!==undefined)) throw new Error("Independent workflow exact controls are Heuristic path pairs only.");
    const legacy = full ? [...fields,...Object.entries(form.fields),...Object.entries(form.explicitEmptySelectors).filter(([,v])=>v).map(([k])=>[k,""] as [string,string])] : Object.hasOwn(form.fields,"heuristic_most_common_paths") && !fields.some(([key])=>key==="heuristic_most_common_paths") ? [...fields,["heuristic_most_common_paths",form.fields.heuristic_most_common_paths] as [string,string]] : fields;
    fields.push(...exactFilterFields(form.exact,legacy));
  }
  return fields;
}
export interface WorkflowInputCell extends Omit<LogConversionTableCell, "kind" | "children"> {
  kind: LogConversionTableCell["kind"] | "na";
  children?: WorkflowInputCell[];
  integer_storage?: "int64" | "uint64"; date_resolution?: "us" | "ns"; date_timezone?: string;
}
export interface WorkflowInputTable {
  columns: string[]; rows: (WorkflowInputCell | null)[][]; attributes?: WorkflowInputCell[];
  metadata?: Record<string, unknown>; lazy?: boolean; dtypes?: string[]; index?: unknown; categorical_dtypes?: unknown[];
}
export interface WorkflowInputDiscovery {
  family: "WORKFLOW_INPUT_DISCOVERY"; method: WorkflowInputFamily; variant: "classic" | "plus" | "plusplus"; computed: boolean;
  error: null | {type: string; message: string};
  input_context: {adapter: WorkflowInputShape; format: WorkflowInputFormat; origin: string; dataset: string; original_input_bytes: number; input_event_count: number; input_case_count: number | null};
  discovery_context?: {case_count: number; event_count: number} | null;
  input_before: WorkflowInputTable | null; input_after: WorkflowInputTable | null; dfg_input: Record<string, unknown> | null;
  result: MineraExport | null; alpha_discovery: Record<string, unknown> | null;
  result_error: string | null; native_net: Record<string, unknown> | null; native_heuristics?: Record<string, unknown> | null;
  pnml: string | null; bpmn: string | null; ptml: string | null;
  pnml_error: string | null; bpmn_error: string | null; ptml_error: string | null;
}
/** The original-input codec includes NativeNA and index class metadata absent from historical log-conversion guards. */
function workflowInputCell(value: unknown, requireKey = false): value is WorkflowInputCell {
  if (!object(value) || typeof value.value !== "string" || !["string", "date", "int", "float", "boolean", "id", "bytes", "na"].includes(String(value.kind))) return false;
  if (requireKey ? typeof value.key !== "string" : value.key !== undefined && typeof value.key !== "string") return false;
  if (value.container !== undefined && !["scalar", "list", "container", "tuple", "set", "frozenset"].includes(String(value.container))) return false;
  if (!["null", "scalar_wrapper", "scalar_children_list"].every(key => value[key] === undefined || typeof value[key] === "boolean")) return false;
  if (value.typed_value !== undefined && value.typed_value !== null && typeof value.typed_value !== "string" && typeof value.typed_value !== "boolean" && !(typeof value.typed_value === "number" && Number.isFinite(value.typed_value))) return false;
  if (value.integer_storage !== undefined && (value.kind !== "int" || !["int64", "uint64"].includes(String(value.integer_storage)) || !integerLiteral(value, "typed_value", value.integer_storage === "int64"))) return false;
  if (value.date_resolution !== undefined && (value.kind !== "date" || !["us", "ns"].includes(String(value.date_resolution)))) return false;
  if (value.date_timezone !== undefined && (value.kind !== "date" || typeof value.date_timezone !== "string")) return false;
  if (value.first_class_role !== undefined && !["none", "case_id", "activity", "timestamp", "resource", "lifecycle"].includes(String(value.first_class_role))) return false;
  if (value.first_class_state !== undefined && !["missing", "present", "null"].includes(String(value.first_class_state))) return false;
  if (value.first_class_source_key !== undefined && typeof value.first_class_source_key !== "string") return false;
  return value.children === undefined || Array.isArray(value.children) && value.children.every(child => workflowInputCell(child, true));
}
const workflowInputAttributes = (value: unknown): boolean => Array.isArray(value) && value.every(attribute => workflowInputCell(attribute, true));
function workflowInputMetadata(value: unknown): boolean {
  if (!object(value) || !Array.isArray(value.extensions) || !value.extensions.every(extension => object(extension) && ["name", "prefix", "uri"].every(key => typeof extension[key] === "string")) || !Array.isArray(value.classifiers) || !value.classifiers.every(classifier => object(classifier) && typeof classifier.name === "string" && strings(classifier.keys))) return false;
  if (!["trace_globals", "event_globals", "properties"].every(key => workflowInputAttributes(value[key]))) return false;
  if (!["trace_global_details", "event_global_details", "property_details"].every(key => value[key] === undefined || workflowInputAttributes(value[key]))) return false;
  return value.globals === undefined || Array.isArray(value.globals) && value.globals.every(global => object(global) && typeof global.scope === "string" && workflowInputAttributes(global.attributes));
}
function workflowInputIndexLabel(value: unknown): boolean {
  return object(value) && (value.kind === "scalar" ? workflowInputCell(value.value, true) : value.kind === "tuple" && Array.isArray(value.items) && value.items.every(workflowInputIndexLabel));
}
export function isWorkflowInputTable(value: unknown): value is WorkflowInputTable {
  if (!object(value) || !strings(value.columns) || !Array.isArray(value.rows) || !value.rows.every(row => Array.isArray(row) && row.length === (value.columns as string[]).length && row.every(cell => cell === null || workflowInputCell(cell)))) return false;
  if (value.attributes !== undefined && !workflowInputAttributes(value.attributes) || value.metadata !== undefined && !workflowInputMetadata(value.metadata) || value.lazy !== undefined && typeof value.lazy !== "boolean") return false;
  if (value.dtypes !== undefined && (!strings(value.dtypes) || value.dtypes.length !== 0 && value.dtypes.length !== value.columns.length)) return false;
  if (value.index !== undefined && value.index !== null && (!object(value.index) || typeof value.index.class !== "string" || typeof value.index.dtype !== "string" || !Array.isArray(value.index.names) || !value.index.names.every(name => name === null || typeof name === "string") || !Array.isArray(value.index.labels) || !value.index.labels.every(workflowInputIndexLabel))) return false;
  return value.categorical_dtypes === undefined || Array.isArray(value.categorical_dtypes) && (value.categorical_dtypes.length === 0 || value.categorical_dtypes.length === value.columns.length) && value.categorical_dtypes.every(dtype => dtype === null || object(dtype) && typeof dtype.categories_dtype === "string" && typeof dtype.ordered === "boolean" && workflowInputAttributes(dtype.categories));
}
function nativeNet(value: unknown, activities: number): boolean {
  if (!object(value) || !index(value.place_count) || unsigned(value, "source") === undefined || unsigned(value, "sink") === undefined || !Array.isArray(value.transitions)) return false;
  const places = (items: unknown) => Array.isArray(items) && items.every(item => index(item) && item < Number(value.place_count));
  return value.transitions.every(item => object(item) && (item.activity === null || index(item.activity) && item.activity < activities) && places(item.inputs) && places(item.outputs) && ["reset_inputs", "inhibitor_inputs"].every(key => item[key] === undefined || places(item[key])));
}
function alphaPlaceName(value: unknown): boolean {
  return object(value) && (typeof value.boundary === "string" || Array.isArray(value.before) && value.before.every(name => name === null || typeof name === "string") && Array.isArray(value.after) && value.after.every(name => name === null || typeof name === "string"));
}
function alphaResult(value: unknown): boolean {
  if (!object(value) || value.family !== "ALPHA_PLUS" || !Array.isArray(value.places) || !Array.isArray(value.transitions)) return false;
  const count = value.places.length;
  if (!value.places.every((place, i) => object(place) && place.id === i && alphaPlaceName(place.name) && unsigned(place, "initial") !== undefined && unsigned(place, "final") !== undefined)) return false;
  if (!value.transitions.every((transition, i) => object(transition) && transition.id === i && (transition.label === null || typeof transition.label === "string") && ["inputs", "outputs"].every(key => Array.isArray(transition[key]) && (transition[key] as unknown[]).every(place => index(place) && place < count)))) return false;
  return ["detached_initial_marking", "detached_final_marking"].every(key => Array.isArray(value[key]) && (value[key] as unknown[]).every(marking => object(marking) && alphaPlaceName(marking.name) && unsigned(marking, "tokens") !== undefined && unsigned(marking, "tokens")! > 0n));
}
function typedStructuredValue(value: unknown): boolean {
  if (!object(value)) return false;
  switch (value.type) {
    case "null": return value.value === null;
    case "boolean": return typeof value.value === "boolean";
    case "string": return typeof value.value === "string";
    case "int64": return integerLiteral(value, "value", true);
    case "uint64": return unsigned(value, "value") !== undefined;
    case "float": return typeof value.value === "number" && Number.isFinite(value.value) || ["NaN", "INF", "-INF"].includes(String(value.value));
    case "array": return Array.isArray(value.value) && value.value.every(typedStructuredValue);
    case "object": return object(value.value) && Object.values(value.value).every(typedStructuredValue);
    default: return false;
  }
}
/** Structural net carrier keeps absent boundaries and arbitrary-width weights/markings outside legacy renderer assumptions. */
export function isWorkflowInputNativeNet(value: unknown): value is Record<string, unknown> {
  if (!object(value) || !strings(value.activity_names) || !index(value.place_count) || typeof value.has_source !== "boolean" || typeof value.has_sink !== "boolean" || !Array.isArray(value.transitions)) return false;
  const count = value.place_count;
  for (const [flag, key] of [["has_source", "source"], ["has_sink", "sink"]]) if (value[flag] ? !index(value[key]) || Number(value[key]) >= count : value[key] !== null) return false;
  for (let i = 0; i < value.transitions.length; ++i) {
    const transition = value.transitions[i];
    if (!object(transition) || transition.index !== i || typeof transition.activity_is_null !== "boolean" || typeof transition.display_name_supplied !== "boolean" || !(transition.display_name === null || typeof transition.display_name === "string")) return false;
    if (transition.activity_is_null ? transition.activity_id !== null || transition.activity_name !== null : !index(transition.activity_id) || transition.activity_id >= value.activity_names.length || transition.activity_name !== value.activity_names[transition.activity_id]) return false;
    for (const [endpoints, weights] of [["inputs", "input_weights"], ["outputs", "output_weights"], ["reset_inputs", "reset_weights"], ["inhibitor_inputs", "inhibitor_weights"]]) {
      const places = transition[endpoints], values = transition[weights];
      if (!Array.isArray(places) || !places.every(place => index(place) && place < count) || !Array.isArray(values) || values.length !== 0 && values.length !== places.length || !values.every(weight => object(weight) && typeof weight.negative === "boolean" && integerLiteral(weight, "magnitude"))) return false;
    }
  }
  if (value.discovery_markings === null) return true;
  const markings = value.discovery_markings;
  if (!object(markings) || !Array.isArray(markings.detached_places)) return false;
  const ids = new Set<number>();
  for (const place of markings.detached_places) {if (!object(place) || !index(place.id) || ids.has(place.id) || !typedStructuredValue(place.name)) return false; ids.add(place.id);}
  return ["initial", "final"].every(key => Array.isArray(markings[key]) && (markings[key] as unknown[]).every(marking => object(marking) && index(marking.place_id) && integerLiteral(marking, "tokens", true) && (marking.namespace === "root" ? marking.place_id < count : marking.namespace === "detached" && ids.has(marking.place_id))));
}
/** Native discovery errors retain their complete mutable-input envelope. No success result or model is invented. */
export function isWorkflowInputDiscovery(value: unknown): value is WorkflowInputDiscovery {
  if (!object(value) || value.family !== "WORKFLOW_INPUT_DISCOVERY" || !["alpha", "heuristic"].includes(String(value.method)) || typeof value.computed !== "boolean" || !object(value.input_context)) return false;
  const input = value.input_context;
  if (!WORKFLOW_INPUT_SHAPES.some(shape => shape === input.adapter) || !WORKFLOW_INPUT_FORMATS.some(format => format === input.format) || typeof input.origin !== "string" || typeof input.dataset !== "string" || !["original_input_bytes", "input_event_count"].every(key => unsigned(input, key) !== undefined) || input.input_case_count !== null && unsigned(input, "input_case_count") === undefined) return false;
  if (value.discovery_context !== undefined && value.discovery_context !== null && (!object(value.discovery_context) || unsigned(value.discovery_context, "case_count") === undefined || unsigned(value.discovery_context, "event_count") === undefined)) return false;
  if (!(value.method === "alpha" ? ["classic", "plus"] : ["classic", "plusplus"]).includes(String(value.variant))) return false;
  if (input.adapter === "dfg" && (input.format !== "dfg_json" || value.variant !== "classic" || value.computed && !object(value.dfg_input))) return false;
  if (input.adapter === "pandas_mutable" && (value.method !== "heuristic" || value.variant !== "plusplus")) return false;
  if (value.input_before !== null && !isWorkflowInputTable(value.input_before) || value.input_after !== null && !isWorkflowInputTable(value.input_after)) return false;
  if (input.adapter === "pandas_mutable" && (value.input_before === null || value.input_after === null)) return false;
  if (value.dfg_input !== null && !object(value.dfg_input)) return false;
  if (value.native_net !== null && !isWorkflowInputNativeNet(value.native_net) || value.native_heuristics !== undefined && value.native_heuristics !== null && !object(value.native_heuristics)) return false;
  if (value.result_error !== null && typeof value.result_error !== "string") return false;
  for (const [key, marker] of [["pnml", "<pnml"], ["bpmn", "definitions"], ["ptml", "<ptml"]]) if (value[key] !== null && (typeof value[key] !== "string" || !(value[key] as string).includes(marker))) return false;
  if (!["pnml_error", "bpmn_error", "ptml_error"].every(key => value[key] === null || typeof value[key] === "string")) return false;
  if (!value.computed) return object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string" && value.result === null && value.alpha_discovery === null && value.native_net === null && value.pnml === null && value.bpmn === null && value.ptml === null;
  if (value.error !== null || value.native_net === null && value.alpha_discovery === null && !object(value.native_heuristics)) return false;
  if (value.alpha_discovery !== null && !alphaResult(value.alpha_discovery)) return false;
  if (value.method === "heuristic" && value.alpha_discovery !== null || value.method === "alpha" && (value.variant === "plus" ? value.alpha_discovery === null : value.alpha_discovery !== null)) return false;
  if (value.result === null) return typeof value.result_error === "string";
  if (!object(value.result) || typeof value.result.dataset !== "string" || unsigned(value.result, "trace_count") === undefined || unsigned(value.result, "event_count") === undefined || !Array.isArray(value.result.activities) || !value.result.activities.every(activity => object(activity) && typeof activity.name === "string" && unsigned(activity, "count") !== undefined) || !Array.isArray(value.result.edges) || !Array.isArray(value.result.variants)) return false;
  const activityCount = value.result.activities.length, result = value.result;
  if (!nativeNet(value.method === "alpha" ? result.petri_net : result.heuristic_petri_net, activityCount)) return false;
  if (value.method === "alpha") return value.variant === "plus" ? alphaResult(value.alpha_discovery) : value.alpha_discovery === null;
  return value.alpha_discovery === null && object(result.heuristics_net) && result.heuristics_net.variant === value.variant && Array.isArray(result.heuristics_net.edges) && Array.isArray(result.heuristics_net.start_activities) && Array.isArray(result.heuristics_net.end_activities);
}
function tableCells(value: WorkflowInputTable, id: string): DiagnosticTable {
  const rows: unknown[][] = [], records: unknown[] = [];
  value.rows.forEach((row, row_index) => row.forEach((cell, column_index) => {
    rows.push([row_index, column_index, value.columns[column_index], value.dtypes?.[column_index], cell === null ? "missing" : cell.null ? "present null" : "present", cell?.kind, cell?.value, cell === null ? undefined : nativeCell(cell, "typed_value"), cell]);
    records.push({row_index, column_index, column: value.columns[column_index], dtype: value.dtypes?.[column_index], cell});
  }));
  return {id, title: id === "input_before_cells" ? "Every original input table cell" : "Every resulting input table cell", columns: ["row_index", "column_index", "column", "dtype", "presence", "kind", "lexical_value", "typed_value", "complete_cell"], rows, records};
}
export function workflowInputTables(value: WorkflowInputDiscovery, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = nativeObjectTables(value);
  if (value.input_before) tables.push(tableCells(value.input_before, "input_before_cells"));
  if (value.input_after) tables.push(tableCells(value.input_after, "input_after_cells"));
  const submitted = fields ?? nativeResponseRequest(value)?.fields;
  if (submitted) tables.push({id: "request", title: "Exact submitted discovery options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))});
  return tables;
}
export interface WorkflowInputRun {
  exact?: ExactFilterRun;
  value: WorkflowInputDiscovery; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[];
  input: File; input_sha256: string; http_status: number;
}
async function fileHash(file: File): Promise<string> {
  return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");
}
export async function runWorkflowInput(file: File, form: WorkflowInputForm, signal?: AbortSignal): Promise<WorkflowInputRun> {
  const fields = workflowInputFields(form, file.name), submitted = Object.fromEntries(fields), hash = await fileHash(file);
  signal?.throwIfAborted();
  const body = new FormData(); body.append("input", file); fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value));
  appendExactFilterFiles(body,form.exact??{});
  const path = "/api/discover-workflow-input", response = await backendFetch(path, {method: "POST", body, signal});
  const exact = form.exact && hasExactFilters(form.exact) ? await readExactFilterResponse(response,path,body,fields,signal) : undefined;
  const value = exact?.value ?? await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (exact && (!response.ok || exact.value.family === "EXACT_FILTER_DISCOVERY" || hasFullHeuristic(form.exact??{}))) throw new OwnedExactFilterResult(exact);
  if (!isWorkflowInputDiscovery(value)) {
    if (exact) throw new OwnedExactFilterResult(exact);
    if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Workflow input discovery failed (${response.status}).`);
    throw new Error("The server did not return a valid native workflow input discovery envelope.");
  }
  const input = value.input_context;
  if (value.method !== submitted.discovery_family || value.variant !== (submitted.alpha_variant ?? submitted.heuristic_variant) || input.adapter !== submitted.input_shape || input.format !== submitted.input_format || input.dataset !== (exact ? await originalEditDistanceWireFilename(file.name) : file.name) || unsigned(input, "original_input_bytes") !== BigInt(file.size)) throw new Error("The workflow response does not match the submitted file, input shape and discovery options.");
  if (!response.ok && value.computed) throw new Error(`The server returned a computed workflow with failure status ${response.status}.`);
  return {value, raw: nativeResponseJson(value), fields, tables: workflowInputTables(value, fields), input: file, input_sha256: hash, http_status: response.status, exact};
}
export class WorkflowInputRequestGate {
  private generation = 0;
  issue(): number {return ++this.generation;}
  invalidate(): void {++this.generation;}
  accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}
}
export function workflowInputReport(run: WorkflowInputRun): string {
  return nativeJson({family: "WORKFLOW_INPUT_DISCOVERY_REPORT", http_status: run.http_status,
    request: nativeResponseRequest(run.value) ?? {path: "/api/discover-workflow-input", fields: run.fields},
    input: {name: run.input.name, size: run.input.size, last_modified: run.input.lastModified, sha256: run.input_sha256}, native_result: run.value}, 2);
}
export function workflowInputAllTablesCsv(run: WorkflowInputRun): string {
  const rows: unknown[][] = [];
  for (const table of run.tables) {
    if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined]));
    table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));
  }
  return diagnosticCsv({id: "all_tables", title: "Every complete native discovery table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export function workflowInputModelFile(run: WorkflowInputRun, format: "pnml" | "bpmn" | "ptml"): File {
  const raw = run.value[format]; if (raw === null) throw new Error(run.value[`${format}_error`] ?? `This native run did not return ${format.toUpperCase()}.`);
  return new File([workflowInputNativeBytes(raw)], `minera-input-${run.value.method}-${run.value.variant}.${format}`, {type: "application/xml"});
}
/** The graph is a display projection only; all native markings and detached names remain in the tables and exports. */
export function workflowInputGraph(value: WorkflowInputDiscovery): ModelGraph | null {
  if (!value.computed || !value.result) return null;
  const net = value.method === "alpha" ? value.result.petri_net : value.result.heuristic_petri_net; if (!net || !index(net.source) || !index(net.sink) || net.source >= net.place_count || net.sink >= net.place_count) return null;
  const alpha = value.alpha_discovery, places = object(alpha) && Array.isArray(alpha.places) ? alpha.places : [];
  const edges: ModelGraph["edges"] = [];
  net.transitions.forEach((transition, transition_index) => {
    transition.inputs.forEach((place, arc_index) => edges.push({id: `in/${transition_index}/${arc_index}`, source: `p/${place}`, target: `t/${transition_index}`, label: "", value: 1, directed: true}));
    transition.outputs.forEach((place, arc_index) => edges.push({id: `out/${transition_index}/${arc_index}`, source: `t/${transition_index}`, target: `p/${place}`, label: "", value: 1, directed: true}));
  });
  return {nodes: [
    ...Array.from({length: net.place_count}, (_, i) => {
      const place = places[i], marked = object(place);
      return {id: `p/${i}`, label: `Place ${i}`, detail: marked ? `Name ${nativeJson(place.name)} · initial ${nativeCell(place, "initial")} · final ${nativeCell(place, "final")}` : undefined,
        start: marked ? unsigned(place, "initial")! > 0n : net.source === i, end: marked ? unsigned(place, "final")! > 0n : net.sink === i};
    }),
    ...net.transitions.map((transition, i) => ({id: `t/${i}`, label: transition.activity === null ? "τ" : value.result!.activities[transition.activity].name, detail: `Native transition ${i}`})),
  ], edges};
}
export function workflowInputSummary(run: WorkflowInputRun): string {
  const value = run.value, input = value.input_context;
  return `${value.method} · ${value.variant} · ${input.adapter} · ${input.input_case_count === null ? "Unknown input case count" : `${nativeCell(input, "input_case_count")} input cases`} · ${nativeCell(input, "input_event_count")} input events. ${value.computed ? "Native discovery completed." : `${value.error?.type}: ${value.error?.message}`}`;
}
export const WORKFLOW_INPUT_TABLE_EXAMPLE = nativeJson({columns: ["case:concept:name", "concept:name", "time:timestamp"], dtypes: ["object", "object", "datetime64[ns]"], rows: [
  [{key: "case:concept:name", kind: "string", value: "case 1"}, {key: "concept:name", kind: "string", value: "A"}, {key: "time:timestamp", kind: "date", value: "2024-01-01T09:00:00Z"}],
  [{key: "case:concept:name", kind: "string", value: "case 1"}, {key: "concept:name", kind: "string", value: "B"}, {key: "time:timestamp", kind: "date", value: "2024-01-01T10:00:00Z"}],
]}, 2);
export function workflowInputDfgExample(family: WorkflowInputFamily): string {
  const dfg = [{source: "A", target: "B", count: 2}];
  return family === "alpha" ? nativeJson({kind: "sparse", dfg}, 2) : nativeJson({dfg, activities: ["A", "B"], activity_counts: {A: 2, B: 2}, start_counts: {A: 2}, end_counts: {B: 2}, window_two_dfg: [], frequency_triples: [], start_activities: ["A"], end_activities: ["B"]}, 2);
}

export async function runExactWorkflowInput(file:File,form:WorkflowInputForm,signal?:AbortSignal):Promise<ExactFilterRun>{if(!form.exact||!hasExactFilters(form.exact))throw new Error("Supply exact Heuristic path controls.");try{const run=await runWorkflowInput(file,form,signal);if(!run.exact)throw new Error("Missing exact workflow inspection.");return run.exact;}catch(cause){if(cause instanceof OwnedExactFilterResult)return cause.run;throw cause;}}
