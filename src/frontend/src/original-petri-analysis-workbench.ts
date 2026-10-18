import {miningSourceHash} from "./active-mining-source";
import {nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString, originalEmbeddingFilterOriginalBytes, originalEmbeddingFilterStringBytes, originalEmbeddingFilterStringText, type OriginalEmbeddingFilterBytes, type OriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {defaultOriginalEditDistanceControl, originalEditDistanceFileName, originalEditDistanceWireFilename, type OriginalEditDistanceControl, type OriginalEditDistanceRequestOriginal} from "./original-edit-distance-workbench";
import {isOriginalOcPlayoutNativeType} from "./original-object-centric-playout-workbench";

export {OriginalEditDistanceRequestGate as OriginalPetriAnalysisRequestGate} from "./original-edit-distance-workbench";
export type OriginalPetriAnalysisControl = OriginalEditDistanceControl;
export type OriginalPetriAnalysisBytes = OriginalEmbeddingFilterBytes;
export const ORIGINAL_PETRI_ANALYSIS_ARTIFACTS = ["source_net_json", "after_net_json", "trace_json", "configured_options_json", "result_json", "legacy_json"] as const;
export const ORIGINAL_PETRI_ANALYSIS_ROLES = ["model", "trace"] as const;
export type OriginalPetriAnalysisRole = typeof ORIGINAL_PETRI_ANALYSIS_ROLES[number];
export const ORIGINAL_PETRI_ANALYSIS_SPECS = {
 "default_workflow_markings": {
  "label": "default workflow markings",
  "call": "default_workflow_markings(net)",
  "options": null,
  "result": "WorkflowMarkings",
  "trace": false
 },
 "solve_marking_equation": {
  "label": "solve marking equation",
  "call": "solve_marking_equation(net, options)",
  "options": "MarkingEquationOptions",
  "result": "MarkingEquationResult",
  "trace": false
 },
 "solve_extended_marking_equation": {
  "label": "solve extended marking equation",
  "call": "solve_extended_marking_equation(trace, net, options)",
  "options": "ExtendedMarkingEquationOptions",
  "result": "ExtendedMarkingEquationResult",
  "trace": true
 },
 "check_workflow_net": {
  "label": "check workflow net",
  "call": "check_workflow_net(net)",
  "options": null,
  "result": "WorkflowNetResult",
  "trace": false
 },
 "is_workflow_net": {
  "label": "is workflow net",
  "call": "is_workflow_net(net)",
  "options": null,
  "result": "bool",
  "trace": false
 },
 "analyze_soundness": {
  "label": "analyze soundness",
  "call": "analyze_soundness(net, options)",
  "options": "SoundnessOptions",
  "result": "SoundnessResult",
  "trace": false
 }
} as const;
export type OriginalPetriAnalysisOperation = keyof typeof ORIGINAL_PETRI_ANALYSIS_SPECS;
export const ORIGINAL_PETRI_ANALYSIS_OPERATIONS = Object.keys(ORIGINAL_PETRI_ANALYSIS_SPECS) as OriginalPetriAnalysisOperation[];
export const ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS = {
 "MarkingEquationOptions": {
  "initial_marking": "Marking",
  "final_marking": "Marking",
  "transition_costs": "std::vector<double>",
  "realizability_limit": "std::size_t"
 },
 "ExtendedMarkingEquationOptions": {
  "initial_marking": "Marking",
  "final_marking": "Marking",
  "model_transition_costs": "std::vector<double>",
  "synchronous_transition_costs": "std::vector<double>",
  "log_move_costs": "std::vector<double>",
  "split_indices": "std::vector<std::size_t>",
  "max_k_value": "std::size_t",
  "full_bootstrap_required": "bool",
  "realizability_limit": "std::size_t"
 },
 "SoundnessOptions": {
  "variant": "SoundnessVariant",
  "initial_marking": "Marking",
  "final_marking": "Marking",
  "max_states": "std::size_t",
  "invariant_search_limit": "std::size_t",
  "diagnostics": "bool",
  "return_asap_when_unsound": "bool"
 },
 "WorkflowMarkings": {
  "initial": "Marking",
  "final": "Marking"
 },
 "MarkingEquationResult": {
  "variant": "MarkingEquationVariant",
  "computed": "bool",
  "feasible": "bool",
  "exhausted": "bool",
  "solver_nodes": "std::size_t",
  "objective": "double",
  "lower_bound": "double",
  "transition_counts": "std::vector<double>",
  "residual": "std::vector<double>",
  "activated_transitions": "std::vector<std::size_t>",
  "firing_sequence": "std::vector<std::size_t>",
  "recovery_attempted": "bool",
  "reaches_final": "bool",
  "error": "std::string"
 },
 "ExtendedMarkingEquationResult": {
  "variant": "ExtendedMarkingEquationVariant",
  "computed": "bool",
  "feasible": "bool",
  "exhausted": "bool",
  "solver_nodes": "std::size_t",
  "objective": "double",
  "lower_bound": "double",
  "retained_splits": "std::vector<std::size_t>",
  "segment_count": "std::size_t",
  "transition_counts": "std::vector<double>",
  "firing_sequence": "std::vector<std::size_t>",
  "reaches_final": "bool",
  "error": "std::string"
 },
 "WorkflowNetResult": {
  "variant": "WorkflowNetVariant",
  "valid": "bool",
  "source_candidates": "std::vector<std::size_t>",
  "sink_candidates": "std::vector<std::size_t>",
  "offending_places": "std::vector<std::size_t>",
  "offending_transitions": "std::vector<std::size_t>",
  "unreachable_places": "std::vector<std::size_t>",
  "unreachable_transitions": "std::vector<std::size_t>",
  "reason": "std::string"
 },
 "SoundnessResult": {
  "variant": "SoundnessVariant",
  "status": "SoundnessStatus",
  "computed": "bool",
  "exhausted": "bool",
  "returned_asap": "bool",
  "workflow": "WorkflowNetResult",
  "bounded": "bool",
  "final_reachable": "bool",
  "option_to_complete": "bool",
  "proper_completion": "bool",
  "dead_transitions": "std::vector<std::size_t>",
  "dead_tasks": "std::vector<std::size_t>",
  "deadlocks": "std::vector<Marking>",
  "deadlock_witnesses": "std::vector<std::vector<std::size_t>>",
  "improper_markings": "std::vector<Marking>",
  "improper_witnesses": "std::vector<std::vector<std::size_t>>",
  "completion_witness": "std::vector<std::size_t>",
  "locking_witness": "std::vector<std::size_t>",
  "locking_scenarios": "std::vector<std::vector<std::size_t>>",
  "unbounded_witness": "std::vector<std::size_t>",
  "reachable_marking_count": "std::size_t",
  "coverability_node_count": "std::size_t",
  "diagnostic_messages": "std::vector<std::string>",
  "place_invariant_basis": "std::vector<std::vector<double>>",
  "uncovered_places": "std::vector<std::size_t>",
  "uniform_place_invariants": "std::vector<std::vector<double>>",
  "uncovered_places_uniform": "std::vector<std::size_t>",
  "uniform_invariant_search_exhausted": "bool",
  "weighted_place_invariants": "std::vector<std::vector<double>>",
  "uncovered_places_weighted": "std::vector<std::size_t>",
  "uncovered_places_s_component": "std::vector<std::size_t>",
  "left": "std::optional<bool>",
  "coverability_markings": "std::vector<Marking>",
  "coverability_edges": "std::vector<std::vector<std::size_t>>",
  "restricted_coverability_tree_exhausted": "bool",
  "restricted_coverability_markings": "std::vector<Marking>",
  "restricted_coverability_edges": "std::vector<std::vector<std::size_t>>",
  "reachability_markings": "std::vector<Marking>",
  "reachability_edges": "std::vector<std::vector<std::size_t>>",
  "s_components": "std::optional<std::vector<std::vector<std::size_t>>>",
  "handled_pairs": "std::optional<std::vector<std::vector<std::size_t>>>",
  "not_well_handled_pairs": "std::optional<std::vector<std::vector<std::size_t>>>",
  "structural_node_labels": "std::vector<std::string>",
  "short_circuit_valid": "bool",
  "short_circuit_strongly_connected": "bool",
  "short_circuit_transition": "std::optional<std::size_t>",
  "short_circuit_diagnostic_messages": "std::vector<std::string>",
  "short_circuit_reachability_exhausted": "bool",
  "short_circuit_reachability_strongly_connected": "bool",
  "short_circuit_reachability_markings": "std::vector<Marking>",
  "short_circuit_reachability_edges": "std::vector<std::vector<std::size_t>>",
  "error": "std::string"
 }
} as const;
export const ORIGINAL_PETRI_ANALYSIS_ENUMS = {
 "MarkingEquationVariant": {
  "0": "classic"
 },
 "ExtendedMarkingEquationVariant": {
  "0": "classic"
 },
 "WorkflowNetVariant": {
  "0": "petri_net"
 },
 "SoundnessVariant": {
  "0": "soundness",
  "1": "woflan"
 },
 "SoundnessStatus": {
  "0": "sound",
  "1": "unsound",
  "2": "unknown"
 }
} as const;
export const ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS = {
 "MarkingEquationOptions": [
  [
   "initial_marking",
   "[]"
  ],
  [
   "final_marking",
   "[]"
  ],
  [
   "transition_costs",
   "[]"
  ],
  [
   "realizability_limit",
   "200000"
  ]
 ],
 "ExtendedMarkingEquationOptions": [
  [
   "initial_marking",
   "[]"
  ],
  [
   "final_marking",
   "[]"
  ],
  [
   "model_transition_costs",
   "[]"
  ],
  [
   "synchronous_transition_costs",
   "[]"
  ],
  [
   "log_move_costs",
   "[]"
  ],
  [
   "split_indices",
   "[]"
  ],
  [
   "max_k_value",
   "5"
  ],
  [
   "full_bootstrap_required",
   "true"
  ],
  [
   "realizability_limit",
   "200000"
  ]
 ],
 "SoundnessOptions": [
  [
   "variant",
   "{\"name\":\"soundness\",\"index\":0}"
  ],
  [
   "initial_marking",
   "[]"
  ],
  [
   "final_marking",
   "[]"
  ],
  [
   "max_states",
   "200000"
  ],
  [
   "invariant_search_limit",
   "100000"
  ],
  [
   "diagnostics",
   "false"
  ],
  [
   "return_asap_when_unsound",
   "false"
  ]
 ]
} as const;
export const ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET = "{\"place_count\":2,\"source_storage\":0,\"sink_storage\":1,\"has_source\":true,\"has_sink\":true,\"activities\":[\"A\"],\"transitions\":[{\"activity\":0,\"inputs\":[0],\"outputs\":[1],\"reset_inputs\":[],\"inhibitor_inputs\":[],\"reset_weights\":[],\"inhibitor_weights\":[],\"input_weights\":[{\"magnitude\":1,\"negative\":false}],\"output_weights\":[{\"magnitude\":1,\"negative\":false}]}],\"discovery_markings\":null,\"transition_names\":null}";
export const ORIGINAL_PETRI_ANALYSIS_DEFAULT_TRACE = '["A"]';
export interface OriginalPetriAnalysisForm {operation: OriginalPetriAnalysisOperation; modelFormat: "exact"; traceFormat: "native_string_array"; options: OriginalPetriAnalysisControl;}
export const defaultOriginalPetriAnalysisForm = (operation: OriginalPetriAnalysisOperation = "analyze_soundness"): OriginalPetriAnalysisForm => ({operation, modelFormat: "exact", traceFormat: "native_string_array", options: defaultOriginalEditDistanceControl("{}", ORIGINAL_PETRI_ANALYSIS_SPECS[operation].options !== null)});
export interface OriginalPetriAnalysisSubmittedControl {name: string; text: string; file: File | null;}
export function originalPetriAnalysisControls(form: OriginalPetriAnalysisForm): OriginalPetriAnalysisSubmittedControl[] {
 if (!Object.hasOwn(ORIGINAL_PETRI_ANALYSIS_SPECS, form.operation) || form.modelFormat !== "exact" || form.traceFormat !== "native_string_array") throw new Error("Choose an original Petri-analysis function and exact input formats.");
 const spec = ORIGINAL_PETRI_ANALYSIS_SPECS[form.operation], controls: OriginalPetriAnalysisSubmittedControl[] = [{name: "operation", text: form.operation, file: null}, {name: "model_format", text: form.modelFormat, file: null}];
 if (spec.trace) controls.push({name: "trace_format", text: form.traceFormat, file: null});
 if (form.options.supplied) {if (spec.options === null) throw new Error("options_json is inapplicable to this public function."); controls.push({name: "options_json", ...form.options});} return controls;
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const list = (value: unknown, predicate: (item: unknown) => boolean): value is unknown[] => Array.isArray(value) && value.every(predicate);
const strings = (value: unknown): value is string[] => list(value, item => typeof item === "string");
const nullable = (value: unknown, predicate: (item: unknown) => boolean): boolean => value === null || predicate(value);
const exact = (value: Record<string, unknown>, keys: readonly string[]): boolean => Object.keys(value).length === keys.length && keys.every(key => Object.hasOwn(value, key));
const nativeString = isOriginalEmbeddingFilterNativeString;
const real = (value: unknown): boolean => typeof value === "number" && Number.isFinite(value) || typeof value === "string" && ["NaN", "INF", "-INF"].includes(value);
const equalBytes = (a: Uint8Array, b: Uint8Array): boolean => a.length === b.length && a.every((byte, index) => byte === b[index]);
function integer(value: unknown, holder: object, key: string | number, signed = false, bits = 64): boolean {if (typeof value !== "number") return false; const token = String(nativeCell(holder, key)); if (!(signed ? /^-?(?:0|[1-9]\d*)$/ : /^(?:0|[1-9]\d*)$/).test(token)) return false; const number = BigInt(token), bound = 1n << BigInt(bits - (signed ? 1 : 0)); return number >= (signed ? -bound : 0n) && number < bound;}
/** Metadata walks full stored records; no workflow/solver invariant is imposed. */
export function isOriginalPetriAnalysisNativeType(type: string, value: unknown, holder?: object, key?: string | number, sizeBits = 64): boolean {
 if (type.startsWith("std::optional<")) return value === null || isOriginalPetriAnalysisNativeType(type.slice(14, -1), value, holder, key, sizeBits);
 if (type.startsWith("std::vector<")) return Array.isArray(value) && value.every((item, index) => isOriginalPetriAnalysisNativeType(type.slice(12, -1), item, value, index, sizeBits));
 if (type === "Marking") return Array.isArray(value) && value.every((item, index) => integer(item, value, index, true));
 if (type === "bool") return typeof value === "boolean";
 if (type === "std::string") return nativeString(value);
 if (type === "double") return real(value);
 if (type === "std::size_t") return !!holder && key !== undefined && integer(value, holder, key, false, sizeBits);
 if (type === "PetriNet") return isOriginalOcPlayoutNativeType(type, value);
 if (Object.hasOwn(ORIGINAL_PETRI_ANALYSIS_ENUMS, type)) {
  if (!object(value) || !exact(value, ["name", "index"]) || !integer(value.index, value, "index", true, 32)) return false;
  const names = ORIGINAL_PETRI_ANALYSIS_ENUMS[type as keyof typeof ORIGINAL_PETRI_ANALYSIS_ENUMS] as Record<string, string>, name = names[String(nativeCell(value, "index"))];
  return name === undefined ? value.name === null : nativeString(value.name) && equalBytes(originalEmbeddingFilterStringBytes(value.name), nativeStringBytes(name));
 }
 const schema = Object.hasOwn(ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS, type) ? (ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS as Record<string, Record<string, string>>)[type] : null;
 return !!schema && object(value) && exact(value, Object.keys(schema)) && Object.entries(schema).every(([member, memberType]) => isOriginalPetriAnalysisNativeType(memberType, value[member], value, member, sizeBits));
}
export const isOriginalPetriAnalysisResult = (type: string, value: unknown, sizeBits = 64): boolean => (type === "bool" || Object.hasOwn(ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS, type) && !type.endsWith("Options")) && isOriginalPetriAnalysisNativeType(type, value, undefined, undefined, sizeBits);
export function originalPetriAnalysisEditMember(raw: string, name: string, allowed: readonly string[], replacement?: string): string {if (!allowed.includes(name)) throw new Error("Choose a native option member."); const value = parseNativeJson(raw); if (!object(value)) throw new Error("Options must be a JSON object."); if (replacement !== undefined) parseNativeJson(replacement); const keys = Object.keys(value).filter(key => key !== name); if (replacement !== undefined) keys.push(name); return `{${keys.map(key => `${JSON.stringify(key)}:${key === name ? replacement : typeof value[key] === "number" ? nativeCell(value, key) : nativeJson(value[key])}`).join(",")}}`;}
export function isOriginalPetriAnalysisBytes(value: unknown): value is OriginalPetriAnalysisBytes {return object(value) && nativeString(value.dataset) && typeof value.data === "string" && integer(value.bytes, value, "bytes") && typeof value.sha256 === "string" && /^[a-f0-9]{64}$/.test(value.sha256) && (value.encoding === "utf8" || value.encoding === "hex" && /^(?:[a-fA-F0-9]{2})*$/.test(value.data));}
export interface OriginalPetriAnalysisError {phase: string; exception_type: string; message: OriginalEmbeddingFilterNativeString;}
const error = (value: unknown): value is OriginalPetriAnalysisError => object(value) && typeof value.phase === "string" && typeof value.exception_type === "string" && nativeString(value.message);
export interface OriginalPetriAnalysisCall {name: string; stage: string; input_kind: string; returned: boolean; error: OriginalPetriAnalysisError | null;}
const call = (value: unknown): value is OriginalPetriAnalysisCall => object(value) && ["name", "stage", "input_kind"].every(key => typeof value[key] === "string") && typeof value.returned === "boolean" && nullable(value.error, error);
export interface OriginalPetriAnalysisWorkbench extends Record<string, unknown> {
 family: "ORIGINAL_PETRI_ANALYSIS_WORKBENCH"; version: "original-petri-analysis-workbench-v1"; operation: OriginalEmbeddingFilterNativeString;
 executed: boolean; native_entered: boolean; native_returned: boolean; native_calls: OriginalPetriAnalysisCall[]; error: OriginalPetriAnalysisError | null;
 originals: Array<{field: OriginalEmbeddingFilterNativeString; filename: OriginalEmbeddingFilterNativeString | null; file: boolean; original: OriginalPetriAnalysisBytes}>;
 parsed_controls: string[]; supplied_controls: Array<{name: OriginalEmbeddingFilterNativeString; origin: "query" | "multipart"}>;
 input_context: {size_t_bytes: number; model_format: OriginalEmbeddingFilterNativeString; trace_format: OriginalEmbeddingFilterNativeString | null; model_supplied_members: string[]; model_unchanged: boolean | null; trace_unchanged: boolean | null; options_unchanged: boolean | null};
 model: {before: Record<string, unknown> | null; after: Record<string, unknown> | null}; trace: null | {before: OriginalEmbeddingFilterNativeString[] | null; after: OriginalEmbeddingFilterNativeString[] | null};
 submitted_options: OriginalPetriAnalysisBytes | null; configured_options: Record<string, unknown> | null; supplied_options: string[];
 result: null | {type: string; value: unknown}; artifact_bytes: Record<typeof ORIGINAL_PETRI_ANALYSIS_ARTIFACTS[number], OriginalPetriAnalysisBytes | null>;
 artifact_errors: Array<{name: string; error: OriginalPetriAnalysisError}>;
}
export function isOriginalPetriAnalysisWorkbench(value: unknown): value is OriginalPetriAnalysisWorkbench {
 if (!object(value) || value.family !== "ORIGINAL_PETRI_ANALYSIS_WORKBENCH" || value.version !== "original-petri-analysis-workbench-v1" || !nativeString(value.operation) || !["executed", "native_entered", "native_returned"].every(key => typeof value[key] === "boolean") || value.executed !== value.native_returned || value.native_returned && !value.native_entered || !list(value.native_calls, call) || !nullable(value.error, error) || !list(value.originals, part => object(part) && nativeString(part.field) && nullable(part.filename, nativeString) && typeof part.file === "boolean" && part.file === (part.filename !== null) && isOriginalPetriAnalysisBytes(part.original)) || !strings(value.parsed_controls) || !strings(value.supplied_options) || !list(value.supplied_controls, part => object(part) && nativeString(part.name) && ["query", "multipart"].includes(String(part.origin))) || !object(value.input_context) || !object(value.model) || !nullable(value.submitted_options, isOriginalPetriAnalysisBytes) || !nullable(value.configured_options, object) || !object(value.artifact_bytes) || !exact(value.artifact_bytes, ORIGINAL_PETRI_ANALYSIS_ARTIFACTS) || !ORIGINAL_PETRI_ANALYSIS_ARTIFACTS.every(key => nullable((value.artifact_bytes as Record<string, unknown>)[key], isOriginalPetriAnalysisBytes)) || !list(value.artifact_errors, item => object(item) && typeof item.name === "string" && error(item.error))) return false;
 const context = value.input_context, bits = context.size_t_bytes === 4 ? 32 : context.size_t_bytes === 8 ? 64 : 0;
 if (!bits || !nativeString(context.model_format) || !nullable(context.trace_format, nativeString) || !strings(context.model_supplied_members) || !["model_unchanged", "trace_unchanged", "options_unchanged"].every(key => context[key] === null || typeof context[key] === "boolean") || !["before", "after"].every(key => nullable((value.model as Record<string, unknown>)[key], item => isOriginalPetriAnalysisNativeType("PetriNet", item)))) return false;
 if (!(value.trace === null || object(value.trace) && ["before", "after"].every(key => nullable((value.trace as Record<string, unknown>)[key], item => list(item, nativeString))))) return false;
 if (!(value.result === null || object(value.result) && typeof value.result.type === "string" && isOriginalPetriAnalysisResult(value.result.type, value.result.value, bits))) return false;
 const operation = typeof value.operation === "string" && Object.hasOwn(ORIGINAL_PETRI_ANALYSIS_SPECS, value.operation) ? value.operation as OriginalPetriAnalysisOperation : null, spec = operation && ORIGINAL_PETRI_ANALYSIS_SPECS[operation];
 if (value.error !== null) return true;
 if (!spec || !value.native_entered || !value.native_returned || !object(value.result) || value.result.type !== spec.result || !isOriginalPetriAnalysisResult(spec.result, value.result.value, bits)) return false;
 const primary = (value.native_calls as OriginalPetriAnalysisCall[]).filter(item => item.stage === "primary");
 if (primary.length !== 1 || primary[0].name !== spec.call || primary[0].input_kind !== (spec.trace ? "NativeString[]+PetriNet" : "PetriNet") || !primary[0].returned || primary[0].error !== null) return false;
 if (spec.options ? !isOriginalPetriAnalysisNativeType(spec.options, value.configured_options, undefined, undefined, bits) : value.configured_options !== null || value.supplied_options.length !== 0 || value.submitted_options !== null) return false;
 const allowed = spec.options ? [...Object.keys(ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS[spec.options]), ...(spec.options === "SoundnessOptions" ? ["variant_index"] : [])] : [];
 const originals = value.originals as OriginalPetriAnalysisWorkbench["originals"];
 return new Set(value.supplied_options).size === value.supplied_options.length && value.supplied_options.every(key => allowed.includes(key)) && originals.some(part => part.field === "model_file") && (!spec.trace || originals.some(part => part.field === "trace_file")) && value.model.before !== null && value.model.after !== null && context.model_unchanged === true && (spec.trace ? object(value.trace) && Array.isArray(value.trace.before) && Array.isArray(value.trace.after) && context.trace_unchanged === true : value.trace === null);
}
export interface OriginalPetriAnalysisRun {value: OriginalPetriAnalysisWorkbench; raw: string; http_status: number; tables: DiagnosticTable[]; request: {path: "/api/petri-analysis-workbench"; operation: OriginalPetriAnalysisOperation; originals: OriginalEditDistanceRequestOriginal[]}; uploads: Partial<Record<OriginalPetriAnalysisRole, File>>; controls: OriginalPetriAnalysisSubmittedControl[];}
const hex = (bytes: Uint8Array): string => Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("");
const fromHex = (raw: string): Uint8Array<ArrayBuffer> => Uint8Array.from(raw.match(/../g) ?? [], byte => parseInt(byte, 16));
export async function runOriginalPetriAnalysisWorkbench(uploads: Partial<Record<OriginalPetriAnalysisRole, File>>, form: OriginalPetriAnalysisForm, signal?: AbortSignal): Promise<OriginalPetriAnalysisRun> {
 signal?.throwIfAborted(); const controls = originalPetriAnalysisControls(form), spec = ORIGINAL_PETRI_ANALYSIS_SPECS[form.operation], roles: OriginalPetriAnalysisRole[] = spec.trace ? ["model", "trace"] : ["model"];
 if (roles.some(role => !(uploads[role] instanceof File))) throw new Error("Choose every independent original Petri-analysis input."); if (Object.keys(uploads).some(role => !roles.includes(role as OriginalPetriAnalysisRole))) throw new Error("This function cannot accept an inapplicable input.");
 const body = new FormData(), originals: OriginalEditDistanceRequestOriginal[] = [];
 for (const control of controls) {const bytes = control.file ? new Uint8Array(await control.file.arrayBuffer()) : nativeStringBytes(control.text), file = control.file ?? new File([bytes], control.name); if (control.file) body.append(control.name, control.file, control.file.name); else appendNativeMultipartField(body, control.name, control.text); const transported = body.get(control.name); originals.push({field: control.name, filename: typeof transported === "string" || transported === null ? null : await originalEditDistanceWireFilename(transported.name), local_filename: control.file?.name ?? null, local_last_modified: control.file?.lastModified ?? null, bytes: bytes.length, sha256: await miningSourceHash(file), encoding: "hex", data: hex(bytes)}); signal?.throwIfAborted();}
 for (const role of roles) {const file = uploads[role]!; body.append(`${role}_file`, file, file.name); originals.push({field: `${role}_file`, filename: await originalEditDistanceWireFilename(file.name), local_filename: file.name, local_last_modified: file.lastModified, bytes: file.size, sha256: await miningSourceHash(file), encoding: "hex", data: hex(new Uint8Array(await file.arrayBuffer()))}); signal?.throwIfAborted();}
 const request: OriginalPetriAnalysisRun["request"] = {path: "/api/petri-analysis-workbench", operation: form.operation, originals}, response = await fetch(request.path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path: request.path, fields: controls.filter(control => !control.file).map(control => [control.name, control.text])}); signal?.throwIfAborted();
 if (!isOriginalPetriAnalysisWorkbench(value)) {if (object(value) && typeof value.error === "string") throw new Error(value.error); throw new Error(`The server did not return an owned exact Petri-analysis response (HTTP ${response.status}).`);}
 // A raw-reader/parser refusal cannot claim any unparsed body parts. The
 // attempted request is retained separately from the default error operation.
 const transportFailure = !response.ok && value.error?.phase === "configuration_error" && value.operation === "analyze_soundness" && !value.executed && !value.native_entered && !value.native_returned && value.native_calls.length === 0 && value.originals.length === 0 && value.parsed_controls.length === 0 && value.supplied_controls.length === 0 && value.configured_options === null && value.submitted_options === null && value.supplied_options.length === 0 && value.result === null && value.model.before === null && value.model.after === null && value.trace === null && value.input_context.model_format === "exact" && value.input_context.trace_format === null && value.input_context.model_supplied_members.length === 0 && [value.input_context.model_unchanged, value.input_context.trace_unchanged, value.input_context.options_unchanged].every(item => item === null) && Object.values(value.artifact_bytes).every(item => item === null) && value.artifact_errors.length === 0;
 if (!transportFailure && (value.operation !== form.operation || value.originals.length !== originals.length || value.originals.some((part, index) => part.field !== originals[index].field) || value.supplied_controls.length !== controls.length || value.supplied_controls.some((part, index) => part.origin !== "multipart" || part.name !== controls[index].name))) throw new Error("The Petri-analysis response changed the operation or original input ownership/order.");
 if (!transportFailure) for (let index = 0; index < originals.length; ++index) {const submitted = originals[index], actual = value.originals[index]; if (actual.original.sha256 !== submitted.sha256 || BigInt(String(nativeCell(actual.original, "bytes"))) !== BigInt(submitted.bytes) || !equalBytes(originalEmbeddingFilterOriginalBytes(actual.original), fromHex(submitted.data)) || actual.file !== (submitted.filename !== null) || (submitted.filename === null ? actual.filename !== null : actual.filename === null || !equalBytes(nativeStringBytes(submitted.filename), originalEmbeddingFilterStringBytes(actual.filename)))) throw new Error("The Petri-analysis response changed exact original bytes, filenames or hashes.");}
 const carriers = [...Object.values(value.artifact_bytes), value.submitted_options].filter((item): item is OriginalPetriAnalysisBytes => item !== null);
 for (const carrier of carriers) {const bytes = originalEmbeddingFilterOriginalBytes(carrier); if (BigInt(String(nativeCell(carrier, "bytes"))) !== BigInt(bytes.length) || await miningSourceHash(new File([bytes], "artifact")) !== carrier.sha256) throw new Error("The Petri-analysis response changed authoritative artifact bytes or hashes."); signal?.throwIfAborted();}
 if (!response.ok && value.error === null) throw new Error(`The server returned a Petri-analysis result with failure status ${response.status}.`);
 return {value, raw: nativeResponseJson(value), http_status: response.status, request, uploads: {...uploads}, controls: controls.map(control => ({...control})), tables: originalPetriAnalysisTables(value, originals)};
}
export function originalPetriAnalysisTables(value: OriginalPetriAnalysisWorkbench, originals?: OriginalEditDistanceRequestOriginal[]): DiagnosticTable[] {
 const tables = nativeObjectTables(value); for (const table of tables) if (table.records?.some(item => item === null || typeof item !== "object")) table.records = table.records.map((item, index) => object(item) || Array.isArray(item) ? item : parseNativeJson(`{"value":${typeof item === "number" ? nativeCell(table.records!, index) : nativeJson(item)}}`));
 if (value.result) tables.push({id: "complete_native_result", title: `Complete ${value.result.type} storage`, columns: ["type", "value"], rows: [[value.result.type, value.result.value]], records: [value.result]});
 if (originals) tables.push({id: "exact_request_originals", title: "Exact input/control bytes and wire/local filenames", columns: ["field", "wire_filename", "local_filename", "last_modified", "bytes", "sha256", "hex"], rows: originals.map(item => [item.field, item.filename, item.local_filename, item.local_last_modified, item.bytes, item.sha256, item.data]), records: originals}); return tables;
}
export const originalPetriAnalysisReport = (run: OriginalPetriAnalysisRun): string => nativeJson({family: "ORIGINAL_PETRI_ANALYSIS_WORKBENCH_REPORT", http_status: run.http_status, request: run.request, native_response: run.value}, 2);
export function originalPetriAnalysisCarrierFile(carrier: OriginalPetriAnalysisBytes): File {return new File([originalEmbeddingFilterOriginalBytes(carrier)], `minera-petri-analysis-${originalEditDistanceFileName(originalEmbeddingFilterStringText(carrier.dataset))}`, {type: carrier.encoding === "hex" ? "application/octet-stream" : "application/json"});}
export function originalPetriAnalysisRequestFile(run: OriginalPetriAnalysisRun, index: number): File {const value = run.request.originals[index]; if (!value) throw new Error("Choose an owned original input/control."); return new File([fromHex(value.data)], value.local_filename ?? value.filename ?? `${value.field}.txt`, {type: "application/octet-stream", ...(value.local_last_modified === null ? {} : {lastModified: value.local_last_modified})});}
export function originalPetriAnalysisResultFile(run: OriginalPetriAnalysisRun): File {if (!run.value.result) throw new Error("No complete native result was returned."); return new File([nativeStringBytes(nativeJson(run.value.result))], `minera-petri-analysis-${run.request.operation}-native-result.json`, {type: "application/json"});}
export interface OriginalPetriAnalysisReusable {label: string; operation: OriginalPetriAnalysisOperation; role: OriginalPetriAnalysisRole | "options"; file: File;}
export function originalPetriAnalysisReusableInputs(run: OriginalPetriAnalysisRun): OriginalPetriAnalysisReusable[] {
 const entries: OriginalPetriAnalysisReusable[] = [], artifacts = run.value.artifact_bytes;
 for (const name of ["source_net_json", "after_net_json"] as const) if (artifacts[name]) for (const operation of ORIGINAL_PETRI_ANALYSIS_OPERATIONS) entries.push({label: `Use actual ${name} with ${operation}`, operation, role: "model", file: originalPetriAnalysisCarrierFile(artifacts[name])});
 if (artifacts.trace_json) entries.push({label: "Reuse exact direct trace", operation: "solve_extended_marking_equation", role: "trace", file: originalPetriAnalysisCarrierFile(artifacts.trace_json)});
 if (artifacts.configured_options_json && ORIGINAL_PETRI_ANALYSIS_SPECS[run.request.operation].options) entries.push({label: "Reopen complete configured options and canonical variant", operation: run.request.operation, role: "options", file: originalPetriAnalysisCarrierFile(artifacts.configured_options_json)});
 return entries;
}
