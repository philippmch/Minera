import {afterEach, describe, expect, it, vi} from "vitest";
import {
  PETRI_ANTI_MULTI_ALGORITHMS, PETRI_ANTI_MULTI_CONTROLS, PETRI_ANTI_MULTI_PNML_CONTROLS, PETRI_ANTI_MULTI_VARIANT,
  PetriAntiMultiRequestGate, defaultPetriAntiMultiForm, isNativePetriAntiMultiOptions, isNativePetriAntiMultiResult,
  isPetriAntiMultiWorkbench, petriAntiMultiAllTablesCsv, petriAntiMultiArtifactFile, petriAntiMultiControlFields,
  petriAntiMultiFields, petriAntiMultiGraph, petriAntiMultiNumber, petriAntiMultiReport, petriAntiMultiSummary,
  petriAntiMultiTableFileName, petriAntiMultiTables, runPetriAntiMultiWorkbench,
  type PetriAntiMultiAlgorithm, type PetriAntiMultiForm, type PetriAntiMultiRun, type PetriAntiMultiWorkbench,
} from "../src/petri-anti-multi-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../src/token-replay-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseRequest} from "../src/native-response";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";

const inputFile = () => new File(["case,activity,time\n1,A,2026-10-14\n"], "original.csv");
const modelFile = () => new File(["<pnml>original document</pnml>"], "original.pnml");
function nativeNet(): Record<string, unknown> {
  return {activity_names: ["A"], place_count: 2, has_source: true, has_sink: true, source: 0, sink: 1, discovery_markings: null,
    transitions: [{index: 0, activity_id: 0, activity_name: "A", activity_is_null: false, display_name_supplied: true, display_name: "Original transition", inputs: [0], outputs: [1], reset_inputs: [], inhibitor_inputs: [], input_weights: [], output_weights: [], reset_weights: [], inhibitor_weights: []}]};
}
function workbench(algorithm: PetriAntiMultiAlgorithm = "anti", shape: PetriAntiMultiForm["inputShape"] = "log", input = inputFile(), model = modelFile()): PetriAntiMultiWorkbench {
  const net = nativeNet();
  return {family: "PETRI_ANTI_MULTI_ALIGNMENT_WORKBENCH", algorithm, variant: PETRI_ANTI_MULTI_VARIANT, input_shape: shape, result_shape: "aggregate", executed: true, error: null,
    input_context: {format: shape === "words" ? "trace_words" : "auto", detected_format: shape === "words" ? "trace_words" : "csv", dataset: input.name, origin: "uploaded_grouped_log", original_input_bytes: input.size, input_event_count: 1, input_case_count: 1, checked_event_count: 1, checked_case_count: 1, trace_index: shape === "trace" ? 0 : null, activity_attribute: "concept:name", case_id_attribute: "case:concept:name"},
    model_context: {format: "auto", detected_format: "pnml", dataset: model.name, original_input_bytes: model.size, native_document: {net, raw_source: 0, raw_sink: 1, initial_marking: [1, 0], final_marking: [0, 1], place_ids: ["original p0", "original p1"], transition_ids: ["東京"], initial_marking_keys: [true, false], final_marking_keys: [true, true], arc_occurrences: null, stochastic_map: null}},
    options_context: {variant: PETRI_ANTI_MULTI_VARIANT, variant_index: 0, exponent: 2, ...(algorithm === "anti" ? {epsilon: .005} : {}), marking_limit: 100000, max_states: 0, initial_marking: [1, 0], final_marking: [0, 1], activity_attribute: "concept:name"},
    pnml_import_options: {infer_initial_marking: true, infer_final_marking: true, import_stochastic_map: false, require_workflow_boundaries: true, public_marking_semantics: false},
    marking_context: {initial_origin: "pnml", final_origin: "pnml", effective_initial: [1, 0], effective_final: [0, 1]}, native_net: net,
    input_before: null, input_after: null, input_trace_words: shape === "words" ? ["A"] : null, input_log: {log: {traces: [{case_id: "1", events: [{activity: "A"}]}]}}, input_log_error: null,
    checked_log: {log: {traces: [{case_id: "1", events: [{activity: "A"}]}]}}, checked_log_error: null,
    result: {activities: ["A"], transitions: [0], cost: algorithm === "anti" ? -.25 : .25, objective: algorithm === "anti" ? -.25 : .25, ...(algorithm === "anti" ? {precision: .75} : {max_distance_to_log: 1}), diagnostics: {visited_states: 2, queued_states: 3, traversed_arcs: 1, exhausted: false}},
    source_xes: "<log>A</log>", source_xes_error: null, pnml: "<pnml>original keys</pnml>", pnml_error: null, checked_pnml: "<pnml>resolved checked keys</pnml>", checked_pnml_error: null};
}
function saved(value: PetriAntiMultiWorkbench): PetriAntiMultiRun {
  const input = inputFile(), model = modelFile(), form = defaultPetriAntiMultiForm(value.algorithm); form.inputShape = value.input_shape; form.inputFormat = value.input_context.format;
  const fields = petriAntiMultiFields(form, input.name);
  return {value, raw: nativeJson(value), fields, tables: petriAntiMultiTables(value, fields), http_status: value.executed ? 200 : 400, inputs: {input, model, input_sha256: "input hash", model_sha256: "model hash"}};
}
afterEach(() => vi.unstubAllGlobals());

describe("independent anti/multi PNML alignment", () => {
  it("exposes all stored native controls and five distinct PNML import options", () => {
    expect(PETRI_ANTI_MULTI_ALGORITHMS).toEqual(["anti", "multi"]);
    expect(PETRI_ANTI_MULTI_CONTROLS.anti.map(control => control.key)).toEqual(["alignment_case_id_attribute", "activity_attribute", "exponent", "epsilon", "marking_limit", "max_states", "initial_marking_json", "final_marking_json", "initial_marking", "final_marking"]);
    expect(PETRI_ANTI_MULTI_CONTROLS.multi.some(control => control.key === "epsilon")).toBe(false);
    expect(PETRI_ANTI_MULTI_PNML_CONTROLS.map(([key, , enabled]) => [key, enabled])).toEqual([["pnml_infer_initial_marking", true], ["pnml_infer_final_marking", true], ["pnml_import_stochastic_map", false], ["pnml_require_workflow_boundaries", true], ["pnml_public_marking_semantics", false]]);
  });
  it("retains raw native numeric domains so semantic errors can return their original snapshots", () => {
    const values = {exponent: " 0.10000000000000001 ", epsilon: "-1", marking_limit: "0", max_states: "0", activity_attribute: "\u00a0 東京,;\u0000\n", alignment_case_id_attribute: "__proto__"};
    const controls = petriAntiMultiControlFields("anti", values); expect(Object.fromEntries(controls.fields)).toEqual({exponent: values.exponent, epsilon: "-1", marking_limit: "0", max_states: "0"}); expect(controls.selectors.activity_attribute).toBe(values.activity_attribute);
    expect(petriAntiMultiControlFields("multi", values).fields.some(([key]) => key === "epsilon")).toBe(false); expect(petriAntiMultiControlFields("anti", {}).fields).toEqual([]);
    const value = workbench(); value.executed = false; value.error = {type: "native_error", message: "exponent must be >1"}; value.result = null; value.options_context.exponent = 0; value.options_context.epsilon = -1; value.options_context.max_states = value.options_context.marking_limit = 0;
    expect(isPetriAntiMultiWorkbench(value)).toBe(true); expect(petriAntiMultiSummary(value)).toContain("failed");
  });
  it("accepts exact finite C hex, signed and subnormal floats without changing request text", () => {
    for (const [text, value] of [["0x1p+20", 1048576], ["-0X1.8P+10", -1536], ["5e-324", Number.MIN_VALUE], ["-0x1p-1074", -Number.MIN_VALUE]] as const) {expect(petriAntiMultiNumber(text)).toBe(value); expect(petriAntiMultiControlFields("anti", {epsilon: text}).fields).toEqual([["epsilon", text]]);}
    expect(Object.is(petriAntiMultiNumber("-0.0"), -0)).toBe(true); expect(Object.is(petriAntiMultiNumber("-0x0p0"), -0)).toBe(true);
    for (const text of ["--0", "-+0", "NaN", "INF", "0x1p1024", "bad"]) expect(petriAntiMultiNumber(text)).toBeUndefined();
    expect(() => petriAntiMultiControlFields("anti", {epsilon: "INF"})).toThrow(/finite/);
  });
  it("distinguishes omitted, explicit empty, zero and exact uint64 markings and rejects conflicting carriers", () => {
    for (const text of ["[]", "[0,0]", " [18446744073709551615,0] "]) expect(petriAntiMultiControlFields("anti", {initial_marking_json: text}).fields).toEqual([["initial_marking_json", text]]);
    expect(petriAntiMultiControlFields("multi", {final_marking: " 18446744073709551615,0000 "}).fields).toEqual([["final_marking", " 18446744073709551615,0000 "]]);
    for (const text of ["[-0]", "[1.0]", "[1e0]", '["1"]', "[18446744073709551616]"]) expect(() => petriAntiMultiControlFields("anti", {initial_marking_json: text})).toThrow(/unsigned/);
    expect(() => petriAntiMultiControlFields("anti", {initial_marking: "1,0", initial_marking_json: "[]"})).toThrow(/one initial marking carrier/);
    expect(() => petriAntiMultiControlFields("anti", {marking_limit: "18446744073709551616"})).toThrow(/unsigned/);
  });
  it("retains all imports, PNML booleans, variant aliases, literal selectors and selected trace index", () => {
    const form = defaultPetriAntiMultiForm(); form.fields.variant = "discounted_a_star"; form.inputShape = "trace"; form.traceIndex = "18446744073709551615"; form.fields.activity_attribute = " 東京 "; form.explicitEmptySelectors.alignment_case_id_attribute = true;
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) form.importFields[key] = "literal";
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) form.importFields[key] = "false";
    for (const [key] of PETRI_ANTI_MULTI_PNML_CONTROLS) form.pnmlFields[key] = "FALSE";
    const fields = Object.fromEntries(petriAntiMultiFields(form, "original.tsv")); expect(fields).toMatchObject({variant: "discounted_a_star", delimiter: "\t", trace_index: form.traceIndex}); expect(JSON.parse(fields.alignment_selectors_json)).toEqual({activity_attribute: " 東京 ", alignment_case_id_attribute: ""});
    for (const [key] of [...TOKEN_REPLAY_IMPORT_CONTROLS, ...TOKEN_REPLAY_XES_BOOLEAN_CONTROLS]) expect(fields[key]).toBe(form.importFields[key]); for (const [key] of PETRI_ANTI_MULTI_PNML_CONTROLS) expect(fields[key]).toBe("FALSE");
    expect(fields.activity_attribute).toBeUndefined(); expect(fields.source_id).toBeUndefined(); form.inputFormat = "trace_words"; expect(() => petriAntiMultiFields(form, "words.json")).toThrow(/trace-word/); form.inputShape = "words"; expect(Object.fromEntries(petriAntiMultiFields(form, "words.json")).input_shape).toBe("words");
  });
  it("validates both aggregate native result families without adding per-trace or completion fields", () => {
    for (const algorithm of PETRI_ANTI_MULTI_ALGORITHMS) for (const shape of ["log", "trace", "words"] as const) {const value = workbench(algorithm, shape); expect(isPetriAntiMultiWorkbench(value)).toBe(true); expect(isNativePetriAntiMultiResult(value.result, algorithm)).toBe(true); expect(isPetriAntiMultiWorkbench({...value, result_shape: "log"})).toBe(false); expect(petriAntiMultiTables(value).some(table => table.id === "complete_alignment_moves")).toBe(false);}
    const value = workbench(); value.result!.diagnostics.exhausted = true; expect(isPetriAntiMultiWorkbench(value)).toBe(true); expect(petriAntiMultiSummary(value)).toContain("pruned search branches"); expect(value.result).not.toHaveProperty("completed"); expect(value.result).not.toHaveProperty("traces");
    expect(isNativePetriAntiMultiResult({...value.result, precision: null}, "anti")).toBe(false); expect(isNativePetriAntiMultiResult({...value.result, transitions: [-1]}, "anti")).toBe(false);
    expect(isNativePetriAntiMultiOptions({...value.options_context, variant: "UNKNOWN", variant_index: -7}, "anti")).toBe(true); expect(isNativePetriAntiMultiOptions({...value.options_context, variant_index: 1}, "anti")).toBe(false);
  });
  it("retains full opaque PNML metadata, arbitrary-width signed tokens and null versus empty optional carriers", () => {
    const value = workbench(); value.model_context.native_document.arc_occurrences = [{place: 0, transition: 0, input: true, type: "UNKNOWN", type_index: -7, weight: 1, negative_weight: true, id: "\ud800東京\u0000"}];
    value.model_context.native_document.stochastic_map = {transitions: [{transition: 0, timing: "UNKNOWN", timing_index: -2, distribution: "UNKNOWN", distribution_index: 99, parameter: "NaN", secondary_parameter: "INF", tertiary_parameter: "-INF", activation_weight: 0, priority: 4}]};
    const raw = nativeJson(value).replace('"weight":1', '"weight":-340282366920938463463374607431768211457').replace('"initial_marking":[1,0]', '"initial_marking":[340282366920938463463374607431768211457,0]'); const parsed = parseNativeJson(raw); expect(isPetriAntiMultiWorkbench(parsed)).toBe(true); if (!isPetriAntiMultiWorkbench(parsed)) throw new Error("Invalid snapshot");
    const report = petriAntiMultiReport(saved(parsed)); expect(report).toContain("-340282366920938463463374607431768211457"); expect(petriAntiMultiAllTablesCsv(petriAntiMultiTables(parsed))).toContain("UNKNOWN");
    parsed.model_context.native_document.arc_occurrences = []; parsed.model_context.native_document.stochastic_map = null; expect(isPetriAntiMultiWorkbench(parsed)).toBe(true); expect(petriAntiMultiReport(saved(parsed))).toContain('"arc_occurrences": []');
  });
  it("retains native errors with original typed NA/index, unknown counts and unresolved effective markings", () => {
    const value = workbench(); value.executed = false; value.result = null; value.error = {type: "native_error", message: "missing case"}; value.input_context.format = "table"; value.input_context.input_case_count = value.input_context.checked_case_count = value.input_context.checked_event_count = null;
    value.input_before = {columns: ["NA"], rows: [[{key: "NA", kind: "na", value: "<NA>", typed_value: "NA"}]], index: {class: "Index", dtype: "object", names: [null], labels: [{kind: "scalar", value: {key: "", kind: "string", value: "東京"}}]}}; value.input_after = value.input_before;
    value.marking_context.effective_initial = value.marking_context.effective_final = null; value.checked_pnml = null; value.checked_pnml_error = "Resolved checked markings are unavailable.";
    expect(isPetriAntiMultiWorkbench(value)).toBe(true); expect(petriAntiMultiGraph(value).nodes).toHaveLength(3); expect(petriAntiMultiReport(saved(value))).toContain('"kind": "na"'); expect(() => petriAntiMultiArtifactFile(saved(value), "checked_pnml")).toThrow(value.checked_pnml_error);
    expect(isPetriAntiMultiWorkbench({...value, result: workbench().result})).toBe(false);
  });
  it("renders the original net with every arc family, duplicate arcs and distinct empty/silent labels", () => {
    const value = workbench(), net = value.native_net; net.activity_names = [""];
    const transitions = net.transitions as Array<Record<string, unknown>>; transitions[0].activity_name = ""; transitions[0].inputs = [0, 0]; transitions[0].reset_inputs = [1]; transitions[0].reset_weights = []; transitions[0].inhibitor_inputs = [1]; transitions[0].inhibitor_weights = [];
    transitions.push({...transitions[0], index: 1, activity_id: null, activity_name: null, activity_is_null: true, display_name_supplied: false, display_name: null});
    expect(isPetriAntiMultiWorkbench(value)).toBe(true); const graph = petriAntiMultiGraph(value); expect(graph.nodes.find(node => node.id === "t:0")?.label).toBe(""); expect(graph.nodes.find(node => node.id === "t:1")?.label).toBe("τ"); expect(new Set(graph.edges.map(edge => edge.id)).size).toBe(graph.edges.length); expect(graph.edges).toHaveLength(10);
    expect(graph.edges.some(edge => edge.label.startsWith("reset"))).toBe(true); expect(graph.edges.some(edge => edge.label.startsWith("inhibitor"))).toBe(true);
  });
  it("submits the original two files and source-free request with exact captured hashes", async () => {
    const input = inputFile(), model = modelFile(), fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(nativeJson(workbench())))); vi.stubGlobal("fetch", fetcher);
    const run = await runPetriAntiMultiWorkbench(input, model, defaultPetriAntiMultiForm()); expect(fetcher.mock.calls[0][0]).toBe("/api/align-petri-anti-multi-workbench"); const body = fetcher.mock.calls[0][1].body as FormData;
    expect(body.get("input_file")).toBe(input); expect(body.get("model_file")).toBe(model); expect(body.get("source_id")).toBeNull(); expect(body.get("include_source")).toBeNull(); expect(run.inputs.input_sha256).toMatch(/^[a-f0-9]{64}$/); expect(run.inputs.model_sha256).toMatch(/^[a-f0-9]{64}$/); expect(nativeResponseRequest(run.value)?.fields).toEqual(run.fields);
    await expect(runPetriAntiMultiWorkbench(new File([await input.arrayBuffer()], "other.csv"), model, defaultPetriAntiMultiForm())).rejects.toThrow(/does not match/);
  });
  it("binds exact native float/uint64 options and supplied marking origins even on semantic failure", async () => {
    const form = defaultPetriAntiMultiForm(); Object.assign(form.fields, {exponent: "-0X1.8P+10", epsilon: "5e-324", marking_limit: "0", max_states: "18446744073709551615", initial_marking_json: "[]", final_marking: "0,001"});
    const value = workbench(); value.executed = false; value.error = {type: "native_error", message: "exponent must be >1"}; value.result = null; value.options_context.exponent = -1536; value.options_context.epsilon = Number.MIN_VALUE; value.options_context.marking_limit = 0; value.options_context.initial_marking = []; value.options_context.final_marking = [0, 1]; value.marking_context.initial_origin = "explicit_empty_native_default"; value.marking_context.final_origin = "explicit_legacy";
    const raw = nativeJson(value).replace('"max_states":0', '"max_states":18446744073709551615'); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(raw, {status: 400})));
    const run = await runPetriAntiMultiWorkbench(inputFile(), modelFile(), form); expect(run.http_status).toBe(400); expect(nativeCell(run.value.options_context, "max_states")).toBe("18446744073709551615"); expect(run.fields).toContainEqual(["initial_marking_json", "[]"]);
    value.marking_context.initial_origin = "pnml"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(raw.replace('"initial_origin":"explicit_empty_native_default"', '"initial_origin":"pnml"'), {status: 400}))); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), form)).rejects.toThrow(/marking origins/);
  });
  it("rejects mismatched algorithms/files/formats, PNML options, literal selectors and selected traces", async () => {
    for (const mutate of [(value: PetriAntiMultiWorkbench) => {value.model_context.dataset = "other.pnml";}, (value: PetriAntiMultiWorkbench) => {value.input_context.original_input_bytes++;}, (value: PetriAntiMultiWorkbench) => {value.model_context.format = "pnml";}]) {const value = workbench(); mutate(value); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value)))); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), defaultPetriAntiMultiForm())).rejects.toThrow(/does not match/);}
    const pnml = defaultPetriAntiMultiForm(); pnml.pnmlFields.pnml_infer_initial_marking = "0"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench())))); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), pnml)).rejects.toThrow(/PNML import/);
    const selector = defaultPetriAntiMultiForm(); selector.fields.activity_attribute = " literal "; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench())))); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), selector)).rejects.toThrow(/literal selectors/);
    const trace = defaultPetriAntiMultiForm(); trace.inputShape = "trace"; trace.traceIndex = "1"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench("anti", "trace"))))); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), trace)).rejects.toThrow(/selected original trace/);
  });
  it("retains structured native 400s, surfaces generic transport failures and prevents late/cancelled runs", async () => {
    const value = workbench(); value.executed = false; value.result = null; value.error = {type: "native_error", message: "unreachable final marking"}; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value), {status: 400}))); const run = await runPetriAntiMultiWorkbench(inputFile(), modelFile(), defaultPetriAntiMultiForm()); expect(run.value.error?.message).toBe(value.error.message);
    value.executed = true; value.error.message = "native result serialization failed"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value), {status: 400}))); const writerFailure = await runPetriAntiMultiWorkbench(inputFile(), modelFile(), defaultPetriAntiMultiForm()); expect(writerFailure.value.executed).toBe(true); expect(writerFailure.value.result).toBeNull(); expect(petriAntiMultiSummary(writerFailure.value)).toContain(value.error.message);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response('{"error":"invalid PNML"}', {status: 400}))); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), defaultPetriAntiMultiForm())).rejects.toThrow("invalid PNML");
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(nativeJson(workbench())));})); await expect(runPetriAntiMultiWorkbench(inputFile(), modelFile(), defaultPetriAntiMultiForm(), controller.signal)).rejects.toThrow(/abort/i);
    const gate = new PetriAntiMultiRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false); expect(gate.accepts(gate.issue(), controller.signal)).toBe(false);
  });
  it("exports every aggregate sequence and typed cell beyond row/column paging with exact native scalars", () => {
    const value = workbench(); value.result!.activities = Array.from({length: 73}, (_, i) => `東京:${i}`); value.result!.transitions = [1]; value.result!.objective = .1; value.input_before = {columns: ["literal"], rows: [[{key: "literal", kind: "string", value: "z".repeat(900)}]]};
    const parsed = parseNativeJson(nativeJson(value).replace('"transitions":[1]', '"transitions":[18446744073709551615]').replace('"objective":0.1', '"objective":0.10000000000000001')); expect(isPetriAntiMultiWorkbench(parsed)).toBe(true); if (!isPetriAntiMultiWorkbench(parsed)) throw new Error("Invalid snapshot");
    const run = saved(parsed), sequence = run.tables.find(table => table.id === "complete_activities")!, cells = run.tables.find(table => table.id === "input_before_cells")!; expect(diagnosticPage(sequence.rows, 2)).toHaveLength(23); expect(diagnosticPage(cells.columns, 1, 8)).toHaveLength(1); expect(diagnosticCsv(sequence)).toContain("東京:72"); expect(nativeJson(sequence.records![72])).toContain("東京:72");
    expect(petriAntiMultiAllTablesCsv(run.tables)).toContain("18446744073709551615"); expect(petriAntiMultiReport(run)).toContain("0.10000000000000001"); expect(nativeJson(cells.records![0])).toContain("z".repeat(900));
    value.result!.cost = "-INF"; value.result!.objective = "NaN"; value.result!.precision = "INF"; expect(isNativePetriAntiMultiResult(value.result, "anti")).toBe(true);
  });
  it("keeps original/checked PNML and source XES byte-distinct, WTF-8 exact and safe to name", async () => {
    const value = workbench(); value.pnml = "<pnml>original 🧪 \ud800</pnml>"; value.checked_pnml = "<pnml>checked 東京 \udc00</pnml>"; value.source_xes = "<log>source\u0000</log>"; const run = saved(value);
    for (const format of ["pnml", "checked_pnml", "source_xes"] as const) expect(new Uint8Array(await petriAntiMultiArtifactFile(run, format).arrayBuffer())).toEqual(workflowInputNativeBytes(value[format]!));
    expect(await petriAntiMultiArtifactFile(run, "pnml").text()).not.toBe(await petriAntiMultiArtifactFile(run, "checked_pnml").text()); value.checked_pnml = null; value.checked_pnml_error = "Resolved checked markings are unavailable."; expect(() => petriAntiMultiArtifactFile(run, "checked_pnml")).toThrow(value.checked_pnml_error);
    const id = "東京/\ud800/\udc00/%\u0000", encoded = petriAntiMultiTableFileName(id); expect(encoded).toMatch(/^[A-Za-z0-9_.%\-]+$/); expect(encoded.replace(/%([a-f0-9]{4})/g, (_, code: string) => String.fromCharCode(parseInt(code, 16)))).toBe(id);
  });
});
