import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {miningSourceHash} from "../src/active-mining-source";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {originalEmbeddingFilterOriginalBytes, originalEmbeddingFilterStringBytes} from "../src/original-embedding-filter-workbench";
import {originalEditDistanceAllTablesCsv, originalEditDistanceSelectedRecordsJson, originalEditDistanceWireFilename} from "../src/original-edit-distance-workbench";
import {ORIGINAL_PETRI_ANALYSIS_ARTIFACTS, ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET, ORIGINAL_PETRI_ANALYSIS_DEFAULT_TRACE, ORIGINAL_PETRI_ANALYSIS_ENUMS, ORIGINAL_PETRI_ANALYSIS_OPERATIONS, ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS, ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS, ORIGINAL_PETRI_ANALYSIS_SPECS, OriginalPetriAnalysisRequestGate, defaultOriginalPetriAnalysisForm, isOriginalPetriAnalysisBytes, isOriginalPetriAnalysisNativeType, isOriginalPetriAnalysisResult, isOriginalPetriAnalysisWorkbench, originalPetriAnalysisCarrierFile, originalPetriAnalysisControls, originalPetriAnalysisEditMember, originalPetriAnalysisReport, originalPetriAnalysisRequestFile, originalPetriAnalysisResultFile, originalPetriAnalysisReusableInputs, originalPetriAnalysisTables, runOriginalPetriAnalysisWorkbench, type OriginalPetriAnalysisBytes, type OriginalPetriAnalysisOperation, type OriginalPetriAnalysisRole, type OriginalPetriAnalysisRun, type OriginalPetriAnalysisWorkbench} from "../src/original-petri-analysis-workbench";
import {OriginalPetriAnalysisWorkspace} from "../src/components/OriginalPetriAnalysisWorkspace";
import {PetriAnalysis} from "../src/components/PetriAnalysis";
import {Landing} from "../src/components/Landing";
import {TopBar} from "../src/components/TopBar";
import type {MineraExport} from "../src/types";

// Synthetic protocol fixtures exercise the frontend storage/ownership boundary;
// they do not assert that a native algorithm produces these arbitrary records.
const MAX_SIZE = "18446744073709551615";
const MIN_I64 = "-9223372036854775808";
const MAX_I64 = "9223372036854775807";
const HUGE = "-" + "9".repeat(400);
const BYTE_WORD = '{"native_bytes":"hex-v1","data":"ff00eda080edb080"}';
const TRACE_JSON = `["","A","A",${BYTE_WORD},{"native_bytes":"hex-v1","data":"f0908080"}]`;
const NET_JSON = `{"place_count":2,"source_storage":${MAX_SIZE},"sink_storage":7,"has_source":false,"has_sink":false,"activities":["","A","A",${BYTE_WORD}],"transitions":[{"activity":null,"inputs":[0,0],"outputs":[1],"reset_inputs":[999],"inhibitor_inputs":[99],"reset_weights":[{"magnitude":${HUGE},"negative":false}],"inhibitor_weights":[{"magnitude":0,"negative":true}],"input_weights":[],"output_weights":[{"magnitude":18446744073709551616,"negative":true}]}],"discovery_markings":{"detached_places":[{"id":${MAX_SIZE},"name":{"index":7,"value":[{"key":${BYTE_WORD},"value":{"index":4,"value":-0.0}}]}}],"initial":[{"place":{"kind":2147483647,"id":999},"tokens":${HUGE}}],"final":[]},"transition_names":[null,"",${BYTE_WORD}]}`;
const WORKFLOW_JSON = `{"variant":{"name":"petri_net","index":0},"valid":false,"source_candidates":[${MAX_SIZE},0,0],"sink_candidates":[],"offending_places":[1],"offending_transitions":[2],"unreachable_places":[3],"unreachable_transitions":[4],"reason":${BYTE_WORD}}`;
const RESULT_JSON: Record<string, string> = {
 WorkflowMarkings: `{"initial":[${MIN_I64},${MAX_I64}],"final":[0,-1]}`,
 MarkingEquationResult: `{"variant":{"name":"classic","index":0},"computed":false,"feasible":false,"exhausted":true,"solver_nodes":${MAX_SIZE},"objective":-0.0,"lower_bound":"NaN","transition_counts":["INF","-INF",-0.0],"residual":[],"activated_transitions":[${MAX_SIZE},0,0],"firing_sequence":[],"recovery_attempted":false,"reaches_final":false,"error":${BYTE_WORD}}`,
 ExtendedMarkingEquationResult: `{"variant":{"name":"classic","index":0},"computed":false,"feasible":false,"exhausted":true,"solver_nodes":${MAX_SIZE},"objective":-0.0,"lower_bound":"INF","retained_splits":[${MAX_SIZE},0,0],"segment_count":0,"transition_counts":["NaN","-INF",-0.0],"firing_sequence":[],"reaches_final":false,"error":${BYTE_WORD}}`,
 WorkflowNetResult: WORKFLOW_JSON,
 SoundnessResult: `{"variant":{"name":"soundness","index":0},"status":{"name":"unknown","index":2},"computed":false,"exhausted":true,"returned_asap":false,"workflow":${WORKFLOW_JSON},"bounded":false,"final_reachable":false,"option_to_complete":false,"proper_completion":false,"dead_transitions":[${MAX_SIZE},0,0],"dead_tasks":[],"deadlocks":[[${MIN_I64},${MAX_I64}],[]],"deadlock_witnesses":[[],[${MAX_SIZE}]],"improper_markings":[[-1,0]],"improper_witnesses":[[0,0]],"completion_witness":[],"locking_witness":[${MAX_SIZE}],"locking_scenarios":[[],[0]],"unbounded_witness":[],"reachable_marking_count":${MAX_SIZE},"coverability_node_count":${MAX_SIZE},"diagnostic_messages":["",${BYTE_WORD}],"place_invariant_basis":[["NaN","INF","-INF",-0.0],[]],"uncovered_places":[0],"uniform_place_invariants":[[]],"uncovered_places_uniform":[],"uniform_invariant_search_exhausted":true,"weighted_place_invariants":[[-0.0]],"uncovered_places_weighted":[1],"uncovered_places_s_component":[2],"left":false,"coverability_markings":[[${MIN_I64}]],"coverability_edges":[[${MAX_SIZE},0]],"restricted_coverability_tree_exhausted":true,"restricted_coverability_markings":[[]],"restricted_coverability_edges":[],"reachability_markings":[[-1]],"reachability_edges":[[]],"s_components":[],"handled_pairs":[[]],"not_well_handled_pairs":null,"structural_node_labels":[${BYTE_WORD},""],"short_circuit_valid":false,"short_circuit_strongly_connected":false,"short_circuit_transition":${MAX_SIZE},"short_circuit_diagnostic_messages":[${BYTE_WORD}],"short_circuit_reachability_exhausted":true,"short_circuit_reachability_strongly_connected":false,"short_circuit_reachability_markings":[[0]],"short_circuit_reachability_edges":[[]],"error":${BYTE_WORD}}`,
 bool: "false",
};
const EXPECTED_OPERATIONS = ["default_workflow_markings", "solve_marking_equation", "solve_extended_marking_equation", "check_workflow_net", "is_workflow_net", "analyze_soundness"] as const;
const EXPECTED_CALLS = ["default_workflow_markings(net)", "solve_marking_equation(net, options)", "solve_extended_marking_equation(trace, net, options)", "check_workflow_net(net)", "is_workflow_net(net)", "analyze_soundness(net, options)"];
const EXPECTED_FIELDS: Record<string, string[]> = {
 WorkflowMarkings: ["initial", "final"],
 MarkingEquationResult: ["variant", "computed", "feasible", "exhausted", "solver_nodes", "objective", "lower_bound", "transition_counts", "residual", "activated_transitions", "firing_sequence", "recovery_attempted", "reaches_final", "error"],
 ExtendedMarkingEquationResult: ["variant", "computed", "feasible", "exhausted", "solver_nodes", "objective", "lower_bound", "retained_splits", "segment_count", "transition_counts", "firing_sequence", "reaches_final", "error"],
 WorkflowNetResult: ["variant", "valid", "source_candidates", "sink_candidates", "offending_places", "offending_transitions", "unreachable_places", "unreachable_transitions", "reason"],
 SoundnessResult: ["variant", "status", "computed", "exhausted", "returned_asap", "workflow", "bounded", "final_reachable", "option_to_complete", "proper_completion", "dead_transitions", "dead_tasks", "deadlocks", "deadlock_witnesses", "improper_markings", "improper_witnesses", "completion_witness", "locking_witness", "locking_scenarios", "unbounded_witness", "reachable_marking_count", "coverability_node_count", "diagnostic_messages", "place_invariant_basis", "uncovered_places", "uniform_place_invariants", "uncovered_places_uniform", "uniform_invariant_search_exhausted", "weighted_place_invariants", "uncovered_places_weighted", "uncovered_places_s_component", "left", "coverability_markings", "coverability_edges", "restricted_coverability_tree_exhausted", "restricted_coverability_markings", "restricted_coverability_edges", "reachability_markings", "reachability_edges", "s_components", "handled_pairs", "not_well_handled_pairs", "structural_node_labels", "short_circuit_valid", "short_circuit_strongly_connected", "short_circuit_transition", "short_circuit_diagnostic_messages", "short_circuit_reachability_exhausted", "short_circuit_reachability_strongly_connected", "short_circuit_reachability_markings", "short_circuit_reachability_edges", "error"],
};
const asRecord = (raw: string) => parseNativeJson(raw) as Record<string, unknown>;
const fullResult = (type: string) => parseNativeJson(RESULT_JSON[type]);
const rawMember = (value: Record<string, unknown>, key: string) => typeof value[key] === "number" ? String(nativeCell(value, key)) : nativeJson(value[key]);
function revised(value: Record<string, unknown>, replacements: Record<string, string> = {}, omitted: string[] = []): Record<string, unknown> {
 const keys = [...new Set([...Object.keys(value), ...Object.keys(replacements)])].filter(key => !omitted.includes(key));
 return asRecord(`{${keys.map(key => `${JSON.stringify(key)}:${Object.hasOwn(replacements, key) ? replacements[key] : rawMember(value, key)}`).join(",")}}`);
}
const hex = (bytes: Uint8Array) => Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("");
const file = (input: string | Uint8Array, name = "input.json", lastModified = 123): File => new File([typeof input === "string" ? nativeStringBytes(input) : Uint8Array.from(input)], name, {lastModified});
async function carrier(dataset: string, input: string | Uint8Array): Promise<OriginalPetriAnalysisBytes> {
 const bytes = typeof input === "string" ? nativeStringBytes(input) : Uint8Array.from(input);
 return {dataset, encoding: "hex", data: hex(bytes), bytes: bytes.length, sha256: await miningSourceHash(new File([bytes], "hash"))};
}
const inputs = (operation: OriginalPetriAnalysisOperation): Partial<Record<OriginalPetriAnalysisRole, File>> => ({model: file(NET_JSON, "complete-net.json"), ...(operation === "solve_extended_marking_equation" ? {trace: file(TRACE_JSON, "direct-trace.json")} : {})});
function defaultOptions(type: keyof typeof ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS): Record<string, unknown> {
 return asRecord(`{${ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS[type].map(([key, raw]) => `${JSON.stringify(key)}:${raw}`).join(",")}}`);
}
async function envelope(form = defaultOriginalPetriAnalysisForm(), uploads = inputs(form.operation)): Promise<OriginalPetriAnalysisWorkbench> {
 const spec = ORIGINAL_PETRI_ANALYSIS_SPECS[form.operation], controls = originalPetriAnalysisControls(form), originals: OriginalPetriAnalysisWorkbench["originals"] = [];
 for (const control of controls) {
  const filename = control.file ? await originalEditDistanceWireFilename(control.file.name) : null;
  originals.push({field: control.name, filename, file: filename !== null, original: await carrier(control.name, control.file ? new Uint8Array(await control.file.arrayBuffer()) : control.text)});
 }
 for (const role of spec.trace ? ["model", "trace"] as const : ["model"] as const) {
  const input = uploads[role]!, filename = await originalEditDistanceWireFilename(input.name);
  originals.push({field: `${role}_file`, filename, file: filename !== null, original: await carrier(`${role}_file`, new Uint8Array(await input.arrayBuffer()))});
 }
 const submitted = form.options.supplied ? form.options.file ? await form.options.file.text() : form.options.text : null;
 const supplied = submitted === null ? {} : asRecord(submitted), base = spec.options ? defaultOptions(spec.options) : null;
 const replacements = Object.fromEntries(Object.keys(supplied).map(key => [key, rawMember(supplied, key)]));
 const configured = base ? revised(base, replacements) : null;
 const model = asRecord(await uploads.model!.text()), trace = uploads.trace ? parseNativeJson(await uploads.trace.text()) as string[] : null;
 const result = {type: spec.result, value: fullResult(spec.result)};
 const artifacts: OriginalPetriAnalysisWorkbench["artifact_bytes"] = {
  source_net_json: await carrier("source_net_json", nativeJson(model)), after_net_json: await carrier("after_net_json", nativeJson(model)),
  trace_json: trace === null ? null : await carrier("trace_json", nativeJson(trace)), configured_options_json: configured === null ? null : await carrier("configured_options_json", nativeJson(configured)),
  result_json: await carrier("result_json", nativeJson(result)), legacy_json: spec.result === "bool" || spec.result === "WorkflowMarkings" ? null : await carrier("legacy_json", "{\"legacy\":true}"),
 };
 return {
  family: "ORIGINAL_PETRI_ANALYSIS_WORKBENCH", version: "original-petri-analysis-workbench-v1", operation: form.operation,
  executed: true, native_entered: true, native_returned: true, native_calls: [{name: spec.call, stage: "primary", input_kind: spec.trace ? "NativeString[]+PetriNet" : "PetriNet", returned: true, error: null}], error: null,
  originals, parsed_controls: controls.map(control => control.name), supplied_controls: controls.map(control => ({name: control.name, origin: "multipart"})),
  input_context: {size_t_bytes: 8, model_format: "exact", trace_format: spec.trace ? "native_string_array" : null, model_supplied_members: Object.keys(model), model_unchanged: true, trace_unchanged: spec.trace ? true : null, options_unchanged: spec.options ? true : null},
  model: {before: model, after: asRecord(nativeJson(model))}, trace: trace === null ? null : {before: trace, after: parseNativeJson(nativeJson(trace)) as string[]},
  submitted_options: submitted === null ? null : await carrier("options_json", submitted), configured_options: configured, supplied_options: Object.keys(supplied).sort(), result,
  artifact_bytes: artifacts, artifact_errors: [],
 };
}
const saved = (value: OriginalPetriAnalysisWorkbench, uploads = inputs(value.operation as OriginalPetriAnalysisOperation)): OriginalPetriAnalysisRun => ({value, raw: nativeJson(value), http_status: 200, tables: originalPetriAnalysisTables(value), request: {path: "/api/petri-analysis-workbench", operation: value.operation as OriginalPetriAnalysisOperation, originals: []}, uploads, controls: []});
const respond = (value: OriginalPetriAnalysisWorkbench, status = 200) => vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value), {status})));
afterEach(() => vi.unstubAllGlobals());

describe("original Petri-analysis declarations and exact storage", () => {
 it("keeps all six public calls, three actual option records and five full result records", () => {
  expect(ORIGINAL_PETRI_ANALYSIS_OPERATIONS).toEqual(EXPECTED_OPERATIONS);
  expect(ORIGINAL_PETRI_ANALYSIS_OPERATIONS.map(operation => ORIGINAL_PETRI_ANALYSIS_SPECS[operation].call)).toEqual(EXPECTED_CALLS);
  expect(Object.values(ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS).map(members => members.length)).toEqual([4, 9, 7]);
  expect(Object.values(EXPECTED_FIELDS).map(fields => fields.length)).toEqual([2, 14, 13, 9, 52]);
  for (const [type, fields] of Object.entries(EXPECTED_FIELDS)) expect(Object.keys(ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS[type as keyof typeof ORIGINAL_PETRI_ANALYSIS_RECORD_SCHEMAS])).toEqual(fields);
  expect(isOriginalPetriAnalysisNativeType("constructor", {})).toBe(false);
  expect(isOriginalPetriAnalysisNativeType("not_a_native_type", {})).toBe(false);
 });
 it.each(EXPECTED_OPERATIONS)("submits %s independently with exactly one selected public call", async operation => {
  const form = defaultOriginalPetriAnalysisForm(operation), uploads = inputs(operation), value = await envelope(form, uploads), fetcher = vi.fn(async () => new Response(nativeJson(value)));
  expect(isOriginalPetriAnalysisWorkbench(value)).toBe(true); vi.stubGlobal("fetch", fetcher);
  const run = await runOriginalPetriAnalysisWorkbench(uploads, form), [path, init] = fetcher.mock.calls[0] as unknown as [string, RequestInit], body = init.body as FormData;
  expect(path).toBe("/api/petri-analysis-workbench"); expect(init.method).toBe("POST"); expect(run.uploads.model).toBe(uploads.model);
  expect(body.get("operation")).toBe(operation); expect(body.get("model_format")).toBe("exact"); expect(body.get("model_file")).toBeInstanceOf(File);
  expect(body.has("trace_file")).toBe(operation === "solve_extended_marking_equation"); expect(body.has("trace_format")).toBe(operation === "solve_extended_marking_equation");
  expect(body.has("options_json")).toBe(ORIGINAL_PETRI_ANALYSIS_SPECS[operation].options !== null);
  for (const hidden of ["source_id", "miner", "trace_index", "model_id", "pnml", "initial_marking"]) expect(body.has(hidden)).toBe(false);
  expect(run.value.native_calls).toHaveLength(1); expect(run.value.native_calls[0].name).toBe(EXPECTED_CALLS[EXPECTED_OPERATIONS.indexOf(operation)]);
  expect(run.value.executed).toBe(true); expect(run.value.native_entered).toBe(true); expect(run.value.native_returned).toBe(true);
  expect(run.value.result?.type).toBe(ORIGINAL_PETRI_ANALYSIS_SPECS[operation].result);
 });
 it("accepts model and direct-trace uploads with controls-only supplied metadata and rejects injected upload controls", async () => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"), uploads = inputs(form.operation), value = await envelope(form, uploads);
  expect(value.originals.map(part => part.field)).toEqual(["operation", "model_format", "trace_format", "options_json", "model_file", "trace_file"]);
  expect(value.supplied_controls).toEqual(["operation", "model_format", "trace_format", "options_json"].map(name => ({name, origin: "multipart"})));
  respond(value); const run = await runOriginalPetriAnalysisWorkbench(uploads, form);
  expect(run.uploads.model).toBe(uploads.model); expect(run.uploads.trace).toBe(uploads.trace); expect(run.value.native_returned).toBe(true);
  for (const injected of ["model_file", "trace_file", "unexpected_control"]) {
   const changed = asRecord(nativeJson(value)) as OriginalPetriAnalysisWorkbench;
   changed.supplied_controls.push({name: injected, origin: "multipart"}); respond(changed);
   await expect(runOriginalPetriAnalysisWorkbench(uploads, form)).rejects.toThrow(/ownership/);
  }
 });
 it("offers a complete default net and direct trace before any mining", () => {
  expect(defaultOriginalPetriAnalysisForm().operation).toBe("analyze_soundness"); expect(isOriginalPetriAnalysisNativeType("PetriNet", parseNativeJson(ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET))).toBe(true);
  expect(parseNativeJson(ORIGINAL_PETRI_ANALYSIS_DEFAULT_TRACE)).toEqual(["A"]);
  for (const operation of ["default_workflow_markings", "check_workflow_net", "is_workflow_net"] as const) {
   const form = defaultOriginalPetriAnalysisForm(operation); expect(form.options.supplied).toBe(false); expect(originalPetriAnalysisControls(form).map(control => control.name)).toEqual(["operation", "model_format"]);
   form.options.supplied = true; expect(() => originalPetriAnalysisControls(form)).toThrow(/inapplicable/);
  }
 });
 it.each(["MarkingEquationOptions", "ExtendedMarkingEquationOptions", "SoundnessOptions"] as const)("retains every supplied member and exact defaults for %s", type => {
  const value = defaultOptions(type); expect(isOriginalPetriAnalysisNativeType(type, value)).toBe(true);
  for (const [name, raw] of ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS[type]) {
   expect(rawMember(value, name)).toBe(raw); expect(isOriginalPetriAnalysisNativeType(type, revised(value, {}, [name]))).toBe(false);
   expect(isOriginalPetriAnalysisNativeType(type, revised(value, {[name]: "null"}))).toBe(false);
  }
  expect(isOriginalPetriAnalysisNativeType(type, revised(value, {cache_id: '"unexpected"'}))).toBe(false);
 });
 it("preserves all twenty option storage domains without imposing algorithm semantics", () => {
  const classic = asRecord(`{"initial_marking":[${MIN_I64},${MAX_I64}],"final_marking":[-1],"transition_costs":["NaN","INF","-INF",-0.0,-1],"realizability_limit":0}`);
  const extended = asRecord(`{"initial_marking":[],"final_marking":[-1],"model_transition_costs":["NaN"],"synchronous_transition_costs":[-0.0],"log_move_costs":[-1,"INF"],"split_indices":[${MAX_SIZE},0,0],"max_k_value":0,"full_bootstrap_required":false,"realizability_limit":${MAX_SIZE}}`);
  const soundness = asRecord(`{"variant":{"name":null,"index":-2147483648},"initial_marking":[-1],"final_marking":[],"max_states":0,"invariant_search_limit":${MAX_SIZE},"diagnostics":true,"return_asap_when_unsound":true}`);
  expect(isOriginalPetriAnalysisNativeType("MarkingEquationOptions", classic)).toBe(true); expect(isOriginalPetriAnalysisNativeType("ExtendedMarkingEquationOptions", extended)).toBe(true); expect(isOriginalPetriAnalysisNativeType("SoundnessOptions", soundness)).toBe(true);
  expect(nativeJson(classic)).toContain(MIN_I64); expect(nativeJson(extended)).toContain(MAX_SIZE); expect(nativeJson(soundness)).toContain('"max_states":0');
  for (const invalid of ["-1", "1.0", "1e0", "18446744073709551616", '"2"']) expect(isOriginalPetriAnalysisNativeType("MarkingEquationOptions", revised(classic, {realizability_limit: invalid}))).toBe(false);
  for (const invalid of ["[-9223372036854775809]", "[9223372036854775808]", "[1.0]", "[1e0]", '["1"]']) expect(isOriginalPetriAnalysisNativeType("MarkingEquationOptions", revised(classic, {initial_marking: invalid}))).toBe(false);
  expect(isOriginalPetriAnalysisNativeType("ExtendedMarkingEquationOptions", revised(extended, {full_bootstrap_required: "0"}))).toBe(false);
  expect(isOriginalPetriAnalysisNativeType("MarkingEquationOptions", revised(classic, {transition_costs: '[{"ieee754":"8000000000000000"}]'}))).toBe(false);
 });
 it("honors native size_t width in result and option holders", async () => {
  const holder = asRecord('{"value":4294967295}'); expect(isOriginalPetriAnalysisNativeType("std::size_t", holder.value, holder, "value", 32)).toBe(true);
  const tooWide = asRecord('{"value":4294967296}'); expect(isOriginalPetriAnalysisNativeType("std::size_t", tooWide.value, tooWide, "value", 32)).toBe(false);
  const value = await envelope(); value.input_context.size_t_bytes = 4; expect(isOriginalPetriAnalysisWorkbench(value)).toBe(false);
  value.input_context.size_t_bytes = 8; expect(isOriginalPetriAnalysisWorkbench(value)).toBe(true); value.input_context.size_t_bytes = 16; expect(isOriginalPetriAnalysisWorkbench(value)).toBe(false);
 });
 it("keeps omission, empty arrays and exact editing lexemes separate", () => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"); form.options.supplied = false; expect(originalPetriAnalysisControls(form).map(control => control.name)).toEqual(["operation", "model_format", "trace_format"]);
  form.options = {supplied: true, file: null, text: '{"initial_marking":[],"max_k_value":0}'}; expect(originalPetriAnalysisControls(form).at(-1)?.text).toBe(form.options.text);
  let raw = `{"realizability_limit":${MAX_SIZE},"transition_costs":[-0.0,"NaN"]}`;
  raw = originalPetriAnalysisEditMember(raw, "initial_marking", ["initial_marking"], `[${MIN_I64}]`); expect(raw).toContain(MAX_SIZE); expect(raw).toContain("-0.0"); expect(raw).toContain(MIN_I64);
  raw = originalPetriAnalysisEditMember(raw, "realizability_limit", ["realizability_limit"]); expect(raw).not.toContain("realizability_limit");
  expect(() => originalPetriAnalysisEditMember("[]", "initial_marking", ["initial_marking"], "[]")).toThrow(/object/);
  expect(() => originalPetriAnalysisEditMember("{}", "source_id", [], "0")).toThrow(/member/);
  expect(() => originalPetriAnalysisEditMember("{}", "initial_marking", ["initial_marking"], "[")).toThrow();
 });
 it.each(Object.keys(EXPECTED_FIELDS))("requires every public field of %s without projecting or truncating", type => {
  const value = fullResult(type) as Record<string, unknown>; expect(Object.keys(value)).toEqual(EXPECTED_FIELDS[type]); expect(isOriginalPetriAnalysisResult(type, value)).toBe(true);
  for (const member of EXPECTED_FIELDS[type]) expect(isOriginalPetriAnalysisResult(type, revised(value, {}, [member]))).toBe(false);
  expect(isOriginalPetriAnalysisResult(type, revised(value, {summary: '"extra"'}))).toBe(false);
  expect(isOriginalPetriAnalysisResult(type, {family: "legacy", computed: true})).toBe(false);
 });
 it("keeps false helpers, unsupported computed=false and unknown exhausted outcomes as normal returns", async () => {
  const helper = await envelope(defaultOriginalPetriAnalysisForm("is_workflow_net")); expect(helper.result?.value).toBe(false); expect(isOriginalPetriAnalysisWorkbench(helper)).toBe(true);
  for (const operation of ["solve_marking_equation", "solve_extended_marking_equation", "analyze_soundness"] as const) {
   const form = defaultOriginalPetriAnalysisForm(operation), value = await envelope(form); respond(value); const run = await runOriginalPetriAnalysisWorkbench(inputs(operation), form), result = run.value.result!.value as Record<string, unknown>;
   expect(run.http_status).toBe(200); expect(run.value.error).toBeNull(); expect(run.value.executed).toBe(true); expect(result.computed).toBe(false); expect(result.exhausted).toBe(true);
   if (operation === "analyze_soundness") expect(result.status).toEqual({name: "unknown", index: 2});
  }
  expect(isOriginalPetriAnalysisResult("bool", false)).toBe(true); expect(isOriginalPetriAnalysisResult("bool", 0)).toBe(false); expect(isOriginalPetriAnalysisResult("bool", {value: false})).toBe(false);
 });
 it("preserves all stored enum indexes and refuses inconsistent names without relabeling", () => {
  for (const [type, names] of Object.entries(ORIGINAL_PETRI_ANALYSIS_ENUMS)) {
   for (const [index, name] of Object.entries(names)) expect(isOriginalPetriAnalysisNativeType(type, asRecord(`{"name":${JSON.stringify(name)},"index":${index}}`))).toBe(true);
   for (const index of ["-2147483648", "2147483647", "123"]) expect(isOriginalPetriAnalysisNativeType(type, asRecord(`{"name":null,"index":${index}}`))).toBe(true);
   for (const invalid of ['{"name":"wrong","index":0}', '{"name":null,"index":0}', '{"name":"soundness","index":123}', '{"name":null,"index":2147483648}', '{"name":null,"index":1.0}', '{"name":null}', '{"index":0}', '{"name":null,"index":123,"extra":true}']) expect(isOriginalPetriAnalysisNativeType(type, asRecord(invalid))).toBe(false);
  }
 });
 it("retains null, false, empty and engaged nested optional storage independently", () => {
  const result = fullResult("SoundnessResult") as Record<string, unknown>;
  for (const left of ["null", "false", "true"]) for (const pairs of ["null", "[]", "[[]]", `[[${MAX_SIZE},0]]`]) {
   const changed = revised(result, {left, s_components: pairs, handled_pairs: pairs, not_well_handled_pairs: pairs}); expect(isOriginalPetriAnalysisResult("SoundnessResult", changed)).toBe(true);
   expect(nativeJson(changed)).toContain(`"left":${left}`); expect(nativeJson(changed)).toContain(`"s_components":${pairs}`);
  }
  for (const transition of ["null", "0", MAX_SIZE]) expect(isOriginalPetriAnalysisResult("SoundnessResult", revised(result, {short_circuit_transition: transition}))).toBe(true);
  expect(isOriginalPetriAnalysisResult("SoundnessResult", revised(result, {left: "0"}))).toBe(false);
 });
 it("preserves the complete nine-field net and nine-field transitions, arbitrary signed token magnitudes and inactive storage", () => {
  const net = asRecord(NET_JSON); expect(isOriginalPetriAnalysisNativeType("PetriNet", net)).toBe(true); expect(Object.keys(net)).toHaveLength(9);
  for (const member of Object.keys(net)) expect(isOriginalPetriAnalysisNativeType("PetriNet", revised(net, {}, [member]))).toBe(false);
  const transition = (net.transitions as Array<Record<string, unknown>>)[0]; expect(Object.keys(transition)).toHaveLength(9);
  for (const member of Object.keys(transition)) expect(isOriginalPetriAnalysisNativeType("PetriNet", revised(net, {transitions: `[${nativeJson(revised(transition, {}, [member]))}]`}))).toBe(false);
  const raw = nativeJson(net); for (const token of [HUGE, MAX_SIZE, '"negative":true', '"has_source":false', '"kind":2147483647', '"transition_names":[null,""', "-0.0"]) expect(raw).toContain(token);
  expect(isOriginalPetriAnalysisNativeType("PetriNet", {places: 2, transitions: [], initial: [1, 0], final: [0, 1]})).toBe(false);
 });
 it.each(["", "ff00", "eda080", "edb080", "eda080edb080", "f0908080", "C0AF"])("preserves NativeString bytes %j without UTF8 normalization", data => {
  const changed = revised(fullResult("SoundnessResult") as Record<string, unknown>, {diagnostic_messages: `[{"native_bytes":"hex-v1","data":"${data}"}]`, structural_node_labels: `[{"native_bytes":"hex-v1","data":"${data}"}]`, error: `{"native_bytes":"hex-v1","data":"${data}"}`});
  expect(isOriginalPetriAnalysisResult("SoundnessResult", changed)).toBe(true); const message = (changed.diagnostic_messages as unknown[])[0]; expect(Array.from(originalEmbeddingFilterStringBytes(message as Parameters<typeof originalEmbeddingFilterStringBytes>[0]))).toEqual((data.match(/../g) ?? []).map(pair => parseInt(pair, 16)));
 });
 it("keeps finite/nonfinite float64 values and signed zero while making no NaN payload claim", () => {
  const value = fullResult("MarkingEquationResult") as Record<string, unknown>;
  for (const token of ["-0.0", "0.0", "1.7976931348623157e308", '"NaN"', '"INF"', '"-INF"']) expect(isOriginalPetriAnalysisResult("MarkingEquationResult", revised(value, {objective: token, transition_counts: `[${token}]`}))).toBe(true);
  for (const invalid of ['{"ieee754":"7ff8000000000001"}', '"nan"', "null", "1e400"]) expect(isOriginalPetriAnalysisResult("MarkingEquationResult", revised(value, {objective: invalid}))).toBe(false);
  expect(String(nativeCell(value, "objective"))).toBe("-0.0"); expect(nativeJson(value)).toContain('"objective":-0.0');
 });
});

describe("original Petri-analysis multipart ownership and native failures", () => {
 it.each(['quote"name\r\n.json', "", "literal%22.json"])("uses real FormData filename serialization for %j and keeps local identity", async name => {
  const form = defaultOriginalPetriAnalysisForm(), uploads = {model: file(NET_JSON + "\r\n", name, 1700000000123)}; form.options.file = file("{}\r\n", name, 456); const value = await envelope(form, uploads), escaped = await originalEditDistanceWireFilename(name);
  if (escaped === null) {value.executed = value.native_entered = value.native_returned = false; value.native_calls = []; value.result = null; value.error = {phase: "configuration_error", exception_type: "invalid_argument", message: "model_file requires an explicit multipart filename"};}
  vi.stubGlobal("fetch", vi.fn(async (_path: unknown, init: RequestInit) => {
   const raw = await new Request("http://minera.invalid/petri-analysis-multipart-test", {method: "POST", body: init.body}).text(); expect(raw).toContain('name="model_file"'); if (escaped !== null) expect(raw).toContain(`filename="${escaped}"`);
   expect(raw).toContain("{}\r\n"); return new Response(nativeJson(value), {status: escaped === null ? 400 : 200});
  }));
  const run = await runOriginalPetriAnalysisWorkbench(uploads, form), index = run.request.originals.findIndex(part => part.field === "model_file"), original = run.request.originals[index], reopened = originalPetriAnalysisRequestFile(run, index);
  expect(original.filename).toBe(escaped); expect(original.local_filename).toBe(name); expect(original.local_last_modified).toBe(1700000000123); expect(reopened.name).toBe(name); expect(reopened.lastModified).toBe(1700000000123); expect(new Uint8Array(await reopened.arrayBuffer())).toEqual(new Uint8Array(await uploads.model.arrayBuffer()));
  if (name.includes('"')) expect(escaped).toBe("quote%22name%0D%0A.json"); if (name === "") expect(run.value.error?.phase).toBe("configuration_error");
  expect(() => originalPetriAnalysisRequestFile(run, -1)).toThrow(/owned original/);
 });
 it("keeps exact text/file controls in physical order and sorted supplied option members", async () => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"); form.options.file = file(`{"realizability_limit":${MAX_SIZE},"initial_marking":[]}`, "options\"\n.json");
  const value = await envelope(form), fetcher = vi.fn(async () => new Response(nativeJson(value))); vi.stubGlobal("fetch", fetcher); const run = await runOriginalPetriAnalysisWorkbench(inputs(form.operation), form);
  const body = (fetcher.mock.calls[0] as unknown as [string, RequestInit])[1].body as FormData;
  expect([...body.keys()]).toEqual(["operation", "model_format", "trace_format", "options_json", "model_file", "trace_file"]);
  expect(run.value.supplied_controls.map(control => control.name)).toEqual(run.controls.map(control => control.name)); expect(run.value.supplied_controls.every(control => control.origin === "multipart")).toBe(true);
  expect(run.value.supplied_options).toEqual(["initial_marking", "realizability_limit"]); expect(originalEmbeddingFilterOriginalBytes(run.value.submitted_options!)).toEqual(new Uint8Array(await form.options.file.arrayBuffer())); expect(nativeJson(run.value.configured_options)).toContain(MAX_SIZE);
 });
 it.each(["operation", "part_order", "part_presence", "control_order", "query_origin", "bytes", "count", "hash", "filename", "file_presence"])("rejects changed original %s ownership", async mutation => {
  const form = defaultOriginalPetriAnalysisForm(), value = await envelope(form);
  if (mutation === "operation") value.operation = "check_workflow_net";
  if (mutation === "part_order") value.originals.reverse(); if (mutation === "part_presence") value.originals.pop(); if (mutation === "control_order") value.supplied_controls.reverse(); if (mutation === "query_origin") value.supplied_controls[0].origin = "query";
  if (mutation === "bytes") value.originals[0].original.data += "00"; if (mutation === "count") value.originals[0].original.bytes += 1; if (mutation === "hash") value.originals[0].original.sha256 = "0".repeat(64);
  if (mutation === "filename") value.originals.at(-1)!.filename = "changed.json"; if (mutation === "file_presence") {value.originals.at(-1)!.filename = null; value.originals.at(-1)!.file = false;}
  respond(value); await expect(runOriginalPetriAnalysisWorkbench(inputs(form.operation), form)).rejects.toThrow(/owned exact Petri-analysis|ownership|original bytes/);
 });
 it.each(["configuration_error", "preparation_error", "native_error", "result_serialization_error"])("retains the reached %s prefix and exact entry/return flags", async phase => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"), value = await envelope(form), entered = phase === "native_error" || phase === "result_serialization_error", returned = phase === "result_serialization_error";
  value.error = {phase, exception_type: "invalid_argument", message: {native_bytes: "hex-v1", data: "ff00"}}; value.native_entered = entered; value.native_returned = value.executed = returned; value.result = null; value.artifact_bytes.result_json = value.artifact_bytes.legacy_json = null;
  value.native_calls = entered ? [{name: ORIGINAL_PETRI_ANALYSIS_SPECS[form.operation].call, stage: "primary", input_kind: "NativeString[]+PetriNet", returned, error: returned ? null : value.error}] : [];
  if (!entered) {value.model = {before: null, after: null}; value.trace = {before: null, after: null}; value.input_context.model_unchanged = value.input_context.trace_unchanged = null; value.artifact_bytes.source_net_json = value.artifact_bytes.after_net_json = value.artifact_bytes.trace_json = null; value.parsed_controls = phase === "configuration_error" ? ["operation"] : ["operation", "model_format", "trace_format", "options_json"];}
  if (phase === "configuration_error") {value.configured_options = null; value.artifact_bytes.configured_options_json = null; value.input_context.options_unchanged = null;}
  expect(isOriginalPetriAnalysisWorkbench(value)).toBe(true); respond(value, returned ? 200 : 400); const run = await runOriginalPetriAnalysisWorkbench(inputs(form.operation), form);
  expect(run.value.error?.phase).toBe(phase); expect(run.value.native_entered).toBe(entered); expect(run.value.native_returned).toBe(returned); expect(run.value.executed).toBe(returned); expect(run.value.originals).toHaveLength(6); expect(run.value.result).toBeNull(); expect(originalPetriAnalysisReport(run)).toContain("ff00"); expect(() => originalPetriAnalysisResultFile(run)).toThrow(/No complete/);
 });
 it("accepts a raw MIME refusal only with the actual unparsed default-operation prefix", async () => {
  const attempted = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"), value = await envelope();
  value.operation = "analyze_soundness"; value.executed = value.native_entered = value.native_returned = false; value.native_calls = []; value.error = {phase: "configuration_error", exception_type: "invalid_argument", message: "incomplete physical multipart"}; value.originals = []; value.supplied_controls = []; value.parsed_controls = []; value.supplied_options = []; value.submitted_options = null; value.configured_options = null; value.result = null; value.model = {before: null, after: null}; value.trace = null;
  value.input_context.model_supplied_members = []; value.input_context.model_unchanged = value.input_context.trace_unchanged = value.input_context.options_unchanged = null; for (const key of ORIGINAL_PETRI_ANALYSIS_ARTIFACTS) value.artifact_bytes[key] = null;
  expect(isOriginalPetriAnalysisWorkbench(value)).toBe(true); respond(value, 413); const run = await runOriginalPetriAnalysisWorkbench(inputs(attempted.operation), attempted);
  expect(run.http_status).toBe(413); expect(run.request.operation).toBe("solve_extended_marking_equation"); expect(run.value.operation).toBe("analyze_soundness"); expect(run.value.originals).toEqual([]); expect(run.request.originals).toHaveLength(6);
  for (const mutation of ["http_200", "parsed_prefix", "query_origin", "configured_options", "model_members", "artifact"]) {
   const changed = asRecord(nativeJson(value)) as OriginalPetriAnalysisWorkbench;
   if (mutation === "parsed_prefix") changed.parsed_controls = ["operation"];
   if (mutation === "query_origin") changed.supplied_controls = [{name: "operation", origin: "query"}];
   if (mutation === "configured_options") changed.configured_options = defaultOptions("SoundnessOptions");
   if (mutation === "model_members") changed.input_context.model_supplied_members = ["place_count"];
   if (mutation === "artifact") changed.artifact_bytes.source_net_json = await carrier("invented_source", "{}");
   respond(changed, mutation === "http_200" ? 200 : 400); await expect(runOriginalPetriAnalysisWorkbench(inputs(attempted.operation), attempted)).rejects.toThrow(/ownership/);
  }
 });
 it("does not turn a returned result or non-primary helper into a selected failed public call", async () => {
  const form = defaultOriginalPetriAnalysisForm(), value = await envelope(form);
  value.native_calls.push({name: "serialize result", stage: "serialization", input_kind: "SoundnessResult", returned: true, error: null}); expect(isOriginalPetriAnalysisWorkbench(value)).toBe(true);
  const changed = asRecord(nativeJson(value)) as OriginalPetriAnalysisWorkbench; changed.native_calls[0].stage = "serialization"; expect(isOriginalPetriAnalysisWorkbench(changed)).toBe(false);
  const wrong = asRecord(nativeJson(value)) as OriginalPetriAnalysisWorkbench; wrong.native_calls[0].name = "check_workflow_net(net)"; expect(isOriginalPetriAnalysisWorkbench(wrong)).toBe(false);
  const duplicate = asRecord(nativeJson(value)) as OriginalPetriAnalysisWorkbench; duplicate.native_calls.push({...duplicate.native_calls[0]}); expect(isOriginalPetriAnalysisWorkbench(duplicate)).toBe(false);
  respond(value, 400); await expect(runOriginalPetriAnalysisWorkbench(inputs(form.operation), form)).rejects.toThrow(/failure status 400/);
 });
 it("retains an independent legacy-export failure after the complete result has returned", async () => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"), value = await envelope(form); value.artifact_bytes.legacy_json = null; value.artifact_errors = [{name: "legacy_json", error: {phase: "export_error", exception_type: "invalid_argument", message: {native_bytes: "hex-v1", data: "ff00"}}}]; respond(value);
  const run = await runOriginalPetriAnalysisWorkbench(inputs(form.operation), form); expect(run.value.native_returned).toBe(true); expect(run.value.error).toBeNull(); expect(run.value.result?.value).not.toBeNull(); expect(run.value.artifact_bytes.result_json).not.toBeNull(); expect(originalPetriAnalysisReport(run)).toContain("legacy_json"); expect(originalPetriAnalysisReport(run)).toContain("ff00");
 });
});

describe("original Petri-analysis complete exports, reuse and cancellation", () => {
 it("retains numeric holders in native result downloads, complete records, CSV and reports", async () => {
  const value = await envelope(), run = saved(value), raw = await originalPetriAnalysisResultFile(run).text(), records = originalEditDistanceSelectedRecordsJson(run.tables), csv = originalEditDistanceAllTablesCsv(run.tables), report = originalPetriAnalysisReport(run);
  for (const output of [raw, records, csv, report]) {expect(output).toContain(MAX_SIZE); expect(output).toContain(MIN_I64); expect(output).toContain("-0.0");}
  expect(raw).toContain('"type":"SoundnessResult"'); expect(report).toContain(HUGE); expect(records).toContain("structural_node_labels"); expect(csv).toContain("short_circuit_reachability_edges");
  const result = value.result!.value as Record<string, unknown>; expect(String(nativeCell(result, "reachable_marking_count"))).toBe(MAX_SIZE); expect(nativeJson(result)).toContain('"weighted_place_invariants":[[-0.0]]');
  const helper = saved(await envelope(defaultOriginalPetriAnalysisForm("is_workflow_net"))); expect(await originalPetriAnalysisResultFile(helper).text()).toBe('{"type":"bool","value":false}');
 });
 it("verifies and downloads every authoritative artifact with its real byte domain", async () => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"), value = await envelope(form), bytes = Uint8Array.from([0xff, 0, 0xed, 0xa0, 0x80, 0xed, 0xb0, 0x80]); value.artifact_bytes.legacy_json = await carrier("legacy/returned.bin", bytes); respond(value);
  const run = await runOriginalPetriAnalysisWorkbench(inputs(form.operation), form); expect(Object.keys(run.value.artifact_bytes)).toEqual(["source_net_json", "after_net_json", "trace_json", "configured_options_json", "result_json", "legacy_json"]);
  for (const key of ORIGINAL_PETRI_ANALYSIS_ARTIFACTS) {const item = run.value.artifact_bytes[key]!; expect(isOriginalPetriAnalysisBytes(item)).toBe(true); expect(new Uint8Array(await originalPetriAnalysisCarrierFile(item).arrayBuffer())).toEqual(originalEmbeddingFilterOriginalBytes(item));}
  expect(new Uint8Array(await originalPetriAnalysisCarrierFile(run.value.artifact_bytes.legacy_json!).arrayBuffer())).toEqual(bytes); expect(run.value.artifact_errors).toEqual([]);
 });
 it.each(ORIGINAL_PETRI_ANALYSIS_ARTIFACTS)("rejects mismatched authoritative %s count, hash or bytes", async key => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation");
  for (const mutation of ["count", "hash", "bytes"]) {const value = await envelope(form), artifact = value.artifact_bytes[key]!; if (mutation === "count") artifact.bytes += 1; if (mutation === "hash") artifact.sha256 = "0".repeat(64); if (mutation === "bytes") artifact.data += "00"; respond(value); await expect(runOriginalPetriAnalysisWorkbench(inputs(form.operation), form)).rejects.toThrow(/artifact bytes or hashes/);}
 });
 it("accepts actual scalar UTF8 artifact bytes and rejects malformed carrier spellings", async () => {
  const value = await envelope(), raw = "\u0000exact\r\n\u2603", item = await carrier("actual_utf8", raw); item.encoding = "utf8"; item.data = raw; value.artifact_bytes.legacy_json = item; respond(value); const run = await runOriginalPetriAnalysisWorkbench(inputs("analyze_soundness"), defaultOriginalPetriAnalysisForm()); expect(await originalPetriAnalysisCarrierFile(run.value.artifact_bytes.legacy_json!).text()).toBe(raw);
  for (const mutation of [{encoding: "base64"}, {encoding: "hex", data: "0"}, {sha256: "bad"}, {bytes: -1}]) expect(isOriginalPetriAnalysisBytes({...item, ...mutation})).toBe(false);
 });
 it("reopens both complete net snapshots across all six functions without mutating a cached model", async () => {
  const value = await envelope(), before = nativeJson(value), run = saved(value), reusable = originalPetriAnalysisReusableInputs(run), modelEntries = reusable.filter(item => item.role === "model");
  expect(modelEntries).toHaveLength(12); expect(modelEntries.slice(0, 6).map(item => item.operation)).toEqual(EXPECTED_OPERATIONS); expect(modelEntries.slice(6).map(item => item.operation)).toEqual(EXPECTED_OPERATIONS);
  for (const item of modelEntries) {expect(await item.file.text()).toBe(nativeJson(value.model.before)); expect(isOriginalPetriAnalysisNativeType("PetriNet", parseNativeJson(await item.file.text()))).toBe(true); expect(item.file).not.toBe(run.uploads.model);}
  expect(nativeJson(value)).toBe(before); expect(defaultOriginalPetriAnalysisForm().modelFormat).toBe("exact");
 });
 it("reuses the exact direct byte trace and complete canonical option file with the same operation", async () => {
  const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation"), value = await envelope(form), run = saved(value), entries = originalPetriAnalysisReusableInputs(run), trace = entries.find(item => item.role === "trace")!, options = entries.find(item => item.role === "options")!;
  expect(trace.operation).toBe("solve_extended_marking_equation"); expect(await trace.file.text()).toBe(TRACE_JSON); expect(options.operation).toBe(form.operation); expect(await options.file.text()).toBe(nativeJson(value.configured_options));
  const sound = await envelope(), canonical = revised(defaultOptions("SoundnessOptions"), {variant: '{"name":null,"index":-2147483648}', max_states: MAX_SIZE}); sound.configured_options = canonical; sound.artifact_bytes.configured_options_json = await carrier("configured_options_json", nativeJson(canonical));
  const option = originalPetriAnalysisReusableInputs(saved(sound)).find(item => item.role === "options")!; expect(await option.file.text()).toContain('"variant":{"name":null,"index":-2147483648}'); expect(await option.file.text()).toContain(MAX_SIZE);
  const next = defaultOriginalPetriAnalysisForm(option.operation); next.options = {supplied: true, file: option.file, text: "ignored editor text"}; const controls = originalPetriAnalysisControls(next); expect(controls.at(-1)?.file).toBe(option.file); expect(controls.at(-1)?.name).toBe("options_json");
 });
 it("does not invent trace/options reuse for functions that have no such input", async () => {
  for (const operation of ["default_workflow_markings", "check_workflow_net", "is_workflow_net"] as const) {const run = saved(await envelope(defaultOriginalPetriAnalysisForm(operation))); expect(originalPetriAnalysisReusableInputs(run).every(item => item.role === "model")).toBe(true); expect(run.value.trace).toBeNull(); expect(run.value.configured_options).toBeNull();}
 });
 it("rejects missing/inapplicable roles and aborted requests before dispatch", async () => {
  const fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher); const form = defaultOriginalPetriAnalysisForm("solve_extended_marking_equation");
  await expect(runOriginalPetriAnalysisWorkbench({}, form)).rejects.toThrow(/independent/); await expect(runOriginalPetriAnalysisWorkbench({model: file(NET_JSON)}, form)).rejects.toThrow(/independent/);
  await expect(runOriginalPetriAnalysisWorkbench({model: file(NET_JSON), trace: file(TRACE_JSON)}, defaultOriginalPetriAnalysisForm())).rejects.toThrow(/inapplicable/);
  const abort = new AbortController(); abort.abort(); await expect(runOriginalPetriAnalysisWorkbench(inputs(form.operation), form, abort.signal)).rejects.toThrow(); expect(fetcher).not.toHaveBeenCalled();
 });
 it("drops a response aborted during fetch and invalidates previous request generations", async () => {
  const form = defaultOriginalPetriAnalysisForm(), value = await envelope(form), abort = new AbortController(); vi.stubGlobal("fetch", vi.fn(async () => {abort.abort(); return new Response(nativeJson(value));}));
  await expect(runOriginalPetriAnalysisWorkbench(inputs(form.operation), form, abort.signal)).rejects.toThrow();
  const gate = new OriginalPetriAnalysisRequestGate(), first = gate.issue(), second = gate.issue(); expect(gate.accepts(first)).toBe(false); expect(gate.accepts(second)).toBe(true); gate.invalidate(); expect(gate.accepts(second)).toBe(false); expect(gate.accepts(gate.issue(), abort.signal)).toBe(false);
 });
});

describe("original Petri-analysis standalone and cached entry surfaces", () => {
 it.each(EXPECTED_OPERATIONS)("exposes %s and all applicable original controls before mining", operation => {
  const html = renderToStaticMarkup(createElement<{standalone?: boolean; initialOperation?: OriginalPetriAnalysisOperation}>(OriginalPetriAnalysisWorkspace, {initialOperation: operation, standalone: true})), spec = ORIGINAL_PETRI_ANALYSIS_SPECS[operation];
  expect(html).toContain('open=""'); expect(html).toContain("before mining"); expect(html).toContain('aria-label="Original Petri-analysis workbench"'); expect(html).toContain('aria-label="Original Petri-analysis model file"'); expect(html).toContain('aria-label="Original Petri-analysis model text"'); expect(html).toContain("Use these exact model editor bytes"); expect(html).toContain("exact");
  for (const name of EXPECTED_OPERATIONS) expect(html).toContain(`value="${name}"`);
  if (spec.trace) {expect(html).toContain('aria-label="Original Petri-analysis trace file"'); expect(html).toContain('aria-label="Original Petri-analysis trace text"'); expect(html).toContain("native_string_array");} else expect(html).not.toContain('aria-label="Original Petri-analysis trace file"');
  if (spec.options) {
   expect(html).toContain("Supply options_json"); expect(html).toContain('aria-label="Original Petri-analysis options file"'); expect(html).toContain('aria-label="Original Petri-analysis option value"');
   for (const [member] of ORIGINAL_PETRI_ANALYSIS_OPTION_MEMBERS[spec.options]) expect(html).toContain(member);
  } else {expect(html).toContain("no options argument"); expect(html).not.toContain('aria-label="Original Petri-analysis options file"');}
  if (operation === "analyze_soundness") {expect(html).toContain("Use native soundness variant"); expect(html).toContain("Use native woflan variant"); expect(html).toContain("variant_index");}
  expect(html).toContain("NaN payload/sign/signalling-bit identity is not claimed"); expect(html).toContain("Run original Petri-analysis function");
 });
 it("offers independent Landing and TopBar access with no loaded source", () => {
  const noop = () => {}, landing = renderToStaticMarkup(createElement(Landing, {onOpenFile: noop, onLoadSample: noop, onLoadOcelSample: noop, onLoadResourceSample: noop, error: null, mining: false, onOriginalPetriAnalysis: noop})), top = renderToStaticMarkup(createElement(TopBar, {dataset: "current", onOpenFile: noop, onHome: noop, onOriginalPetriAnalysis: noop}));
  expect(landing).toContain("Inspect original Petri analysis from a complete net"); expect(top).toContain("Original Petri analysis");
 });
 it("keeps all five cached families and a separate default invariant budget beside the state limit", () => {
  const data = {dataset: "cached models", conformance: {alpha: {}}} as unknown as MineraExport, html = renderToStaticMarkup(createElement(PetriAnalysis, {data}));
  for (const family of ["soundness", "woflan", "workflow_net", "marking_equation", "extended_marking_equation"]) expect(html).toContain(`value="${family}"`);
  expect(html).toContain('State limit<input type="number" min="1" value="200000"'); expect(html).toContain('Invariant search limit<input type="number" min="0" value="100000"');
  expect(html).toContain('aria-label="Original Petri-analysis workbench"'); expect(html).toContain("before mining"); expect(html).toContain("Use native woflan variant");
 });
});

describe("native multipart model and trace roles without filename attributes", () => {
 it.each(EXPECTED_OPERATIONS)("accepts the complete owned %s response and retains absence", async operation => {
  const value = await envelope(defaultOriginalPetriAnalysisForm(operation));
  const roles = ORIGINAL_PETRI_ANALYSIS_SPECS[operation].trace ? ["model_file", "trace_file"] : ["model_file"];
  for (const role of roles) {
   const original = value.originals.find(part => part.field === role)!;
   original.filename = null; original.file = false; original.original.dataset = role;
  }
  expect(isOriginalPetriAnalysisWorkbench(value)).toBe(true);
  for (const role of roles) {
   const original = value.originals.find(part => part.field === role)!;
   expect(original.filename).toBeNull(); expect(original.file).toBe(false);
   original.file = true;
   expect(isOriginalPetriAnalysisWorkbench(value)).toBe(false);
   original.file = false; original.field = "missing_role";
   expect(isOriginalPetriAnalysisWorkbench(value)).toBe(false);
   original.field = role;
  }
 });
});
