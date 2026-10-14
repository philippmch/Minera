import {afterEach, describe, expect, it, vi} from "vitest";
import {
  MODEL_CONVERSION_ARTIFACTS, MODEL_CONVERSION_OPERATIONS, MODEL_CONVERSION_PNML_CONTROLS, MODEL_CONVERSION_REDUCTION_CONTROLS,
  ModelConversionRequestGate, defaultModelConversionForm, isModelConversionWorkbench, isModelReductionOptions, isModelReductionResult,
  isNativePowl, modelConversionAllTablesCsv, modelConversionArtifactFile, modelConversionFields, modelConversionFormats,
  modelConversionGraphs, modelConversionPowlGraph, modelConversionReport, modelConversionSummary, modelConversionTableFileName,
  modelConversionTables, runModelConversionWorkbench,
  type ModelConversionOperation, type ModelConversionRun, type ModelConversionWorkbench, type NativePowlNode,
} from "../src/model-conversion-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseRequest} from "../src/native-response";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";

const text = "original model 東京\ud800\u0000", originalFile = () => new File([workflowInputNativeBytes(text)], "original-model.pnml");
function net(): Record<string, unknown> {return {activity_names: ["A", ""], place_count: 2, has_source: true, has_sink: true, source: 0, sink: 1, discovery_markings: null, transitions: [{index: 0, activity_id: 0, activity_name: "A", activity_is_null: false, display_name_supplied: true, display_name: "Original", inputs: [0], outputs: [1], reset_inputs: [], inhibitor_inputs: [], input_weights: [], output_weights: [], reset_weights: [], inhibitor_weights: []}]};}
function powl(label = "A"): NativePowlNode {return {kind: "TRANSITION", kind_index: 0, label, frequency: {minimum: 1, maximum: 1}, children: [], order: []};}
function workbench(operation: ModelConversionOperation = "tree_to_powl", model = originalFile()): ModelConversionWorkbench {
  const pnml = operation.startsWith("net_") || operation === "reduce_petri", tree = {activities: ["A", "unused"], root: {op: "ACTIVITY" as const, op_index: 0, activity: 0, children: []}}, originalNet = net();
  const result: NonNullable<ModelConversionWorkbench["result"]> = {process_tree: null, powl: null, petri_net: null, reduction: null};
  if (operation.endsWith("_powl")) result.powl = powl(); else if (operation.endsWith("_tree")) result.process_tree = tree; else if (operation === "powl_to_net") result.petri_net = net(); else result.reduction = {net: net(), markings: {initial: [1, 0], final: [0, 1]}, old_to_new_place: [0, 1], old_to_new_transition: [0], changed: false};
  const exports = Object.fromEntries(MODEL_CONVERSION_ARTIFACTS.flatMap(key => [[key, null], [`${key}_error`, null]])) as ModelConversionWorkbench["exports"];
  return {family: "MODEL_CONVERSION_WORKBENCH", operation, executed: true, error: null,
    input_context: {format: "auto", detected_format: pnml ? "pnml" : operation.startsWith("powl_") ? "powl_text" : "tree_json", dataset: model.name, original_model_bytes: model.size, model_unchanged: true, initial_marking_origin: operation === "reduce_petri" ? "pnml" : null, final_marking_origin: operation === "reduce_petri" ? "pnml" : null},
    options: operation === "reduce_petri" ? {initial_marking: [1, 0], final_marking: [0, 1], enable_parallel_place_fusion: false, enable_abstraction: false, preserve_join_bridges: false} : null,
    pnml_import_options: pnml ? {infer_initial_marking: true, infer_final_marking: true, import_stochastic_map: false, require_workflow_boundaries: true, public_marking_semantics: false} : null,
    model: {process_tree: pnml || operation.startsWith("powl_") ? null : tree, powl: operation.startsWith("powl_") ? powl() : null, petri_net: pnml ? originalNet : null, pnml_document: pnml ? {net: originalNet, raw_source: 0, raw_sink: 1, initial_marking: [1, 0], final_marking: [0, 1], place_ids: ["p0", "p1"], transition_ids: ["t0"], initial_marking_keys: [true, false], final_marking_keys: [false, true], arc_occurrences: null, stochastic_map: null} : null, original_model: text}, result, exports};
}
function saved(value: ModelConversionWorkbench): ModelConversionRun {const model = originalFile(), fields = modelConversionFields(defaultModelConversionForm(value.operation)); return {value, raw: nativeJson(value), fields, tables: modelConversionTables(value, fields), http_status: value.error ? 400 : 200, input: {model, model_sha256: "original hash"}};}
afterEach(() => vi.unstubAllGlobals());

describe("independent original-model conversion and reduction", () => {
  it("exposes the five converters and reduction with compatible original formats", () => {
    expect(MODEL_CONVERSION_OPERATIONS).toEqual(["tree_to_powl", "net_to_powl", "powl_to_tree", "net_to_tree", "powl_to_net", "reduce_petri"]);
    for (const operation of MODEL_CONVERSION_OPERATIONS) for (const modelFormat of modelConversionFormats(operation)) expect(modelConversionFields({...defaultModelConversionForm(operation), modelFormat})).toEqual([["operation", operation], ["model_format", modelFormat]]);
    expect(() => modelConversionFields({...defaultModelConversionForm("tree_to_powl"), modelFormat: "pnml"})).toThrow(/compatible/);
    expect(modelConversionFormats("powl_to_net")).toEqual(["auto", "powl_text"]);
  });
  it("transmits all five native reduction options only for reduction and PNML controls only for net inputs", () => {
    expect(MODEL_CONVERSION_REDUCTION_CONTROLS.map(control => control.key)).toEqual(["petri_reduction_initial_marking_json", "petri_reduction_final_marking_json", "petri_reduction_initial_marking", "petri_reduction_final_marking", "petri_reduction_parallel_place_fusion", "petri_reduction_abstraction", "petri_reduction_preserve_join_bridges"]);
    const form = defaultModelConversionForm("reduce_petri"); form.fields = {petri_reduction_initial_marking_json: "[]", petri_reduction_final_marking: "-1,0", petri_reduction_parallel_place_fusion: "true", petri_reduction_abstraction: "false", petri_reduction_preserve_join_bridges: "1"}; for (const [key] of MODEL_CONVERSION_PNML_CONTROLS) form.pnmlFields[key] = "FALSE";
    expect(Object.fromEntries(modelConversionFields(form))).toMatchObject(form.fields);
    for (const operation of MODEL_CONVERSION_OPERATIONS.filter(value => value !== "reduce_petri")) {const fields = modelConversionFields({...form, operation}); expect(fields.some(([key]) => key.startsWith("petri_reduction_"))).toBe(false); expect(fields.filter(([key]) => key.startsWith("pnml_")).length).toBe(operation.startsWith("net_") ? 5 : 0);}
    expect(modelConversionFields(defaultModelConversionForm()).some(([key]) => key === "source_id")).toBe(false);
  });
  it("preserves exact signed int64 JSON and legacy marking text, including empty arrays and negative zero", () => {
    const form = defaultModelConversionForm("reduce_petri");
    for (const raw of ["[]", " [0,-0,-9223372036854775808,9223372036854775807] "]) {form.fields = {petri_reduction_initial_marking_json: raw}; expect(modelConversionFields(form).at(-1)).toEqual(["petri_reduction_initial_marking_json", raw]);}
    form.fields = {petri_reduction_final_marking: " -9223372036854775808,0000,-0,9223372036854775807 "}; expect(modelConversionFields(form).at(-1)?.[1]).toBe(form.fields.petri_reduction_final_marking);
    for (const raw of ["[9223372036854775808]", "[-9223372036854775809]", "[1.0]", "[1e0]", '["1"]', "[[1]]"]) {form.fields = {petri_reduction_initial_marking_json: raw}; expect(() => modelConversionFields(form)).toThrow(/signed int64/);}
    form.fields = {petri_reduction_initial_marking_json: "[]", petri_reduction_initial_marking: "1"}; expect(() => modelConversionFields(form)).toThrow(/one initial/); form.fields = {petri_reduction_initial_marking: "+1"}; expect(() => modelConversionFields(form)).toThrow(/signed int64/);
  });
  it("applies the distinct native Boolean contracts without altering request text", () => {
    const form = defaultModelConversionForm("reduce_petri"); for (const [key] of MODEL_CONVERSION_PNML_CONTROLS) form.pnmlFields[key] = "TRUE"; form.fields.petri_reduction_abstraction = "0";
    const fields = Object.fromEntries(modelConversionFields(form)); expect(fields.petri_reduction_abstraction).toBe("0"); for (const [key] of MODEL_CONVERSION_PNML_CONTROLS) expect(fields[key]).toBe("TRUE");
    for (const raw of ["yes", "TRUE", "FaLsE"]) {form.fields.petri_reduction_abstraction = raw; expect(() => modelConversionFields(form)).toThrow(/true, false/);}
    expect(MODEL_CONVERSION_PNML_CONTROLS.map(([, , enabled]) => enabled)).toEqual([true, true, false, true, false]);
  });
  it("validates every POWL getter and keeps empty transitions separate from silent nodes and canonical text", () => {
    const node = powl(""); node.frequency = {minimum: 0, maximum: null}; const silent = {...powl("stored latent"), kind: "SILENT" as const, kind_index: 1}; const root: NativePowlNode = {...powl("unused root"), kind: "STRICT_PARTIAL_ORDER", kind_index: 4, children: [node, silent, powl("@12")], order: [[0, 2], [1, 2]]};
    expect(isNativePowl(root)).toBe(true); const graph = modelConversionPowlGraph(root); expect(graph.nodes[1].label).toBe(""); expect(graph.nodes[2].label).toBe("τ"); expect(graph.edges.filter(edge => edge.label === "order")).toHaveLength(2);
    const value = workbench("powl_to_tree"); value.model.powl = root; value.exports.source_powl = "PO=(nodes={@12,12},order={@12-->12})"; expect(isModelConversionWorkbench(value)).toBe(true);
    expect(isNativePowl({...node, kind: "UNKNOWN", kind_index: -7})).toBe(true); expect(isNativePowl({...node, kind_index: 1})).toBe(false); expect(isNativePowl({...node, frequency: {minimum: 1.5, maximum: null}})).toBe(false);
  });
  it("accepts complete native output beyond input depth limits and rejects cycles without truncating", () => {
    const root = powl("root"); let current = root; for (let index = 0; index < 90; ++index) {const child = powl(`node ${index}`); current.children.push(child); current = child;} expect(isNativePowl(root)).toBe(true);
    const value = workbench("powl_to_tree"); value.model.powl = root; expect(modelConversionTables(value).find(table => table.id === "original_powl_nodes")?.rows).toHaveLength(91); expect(modelConversionAllTablesCsv(modelConversionTables(value))).toContain("node 89"); current.children.push(root); expect(isNativePowl(root)).toBe(false);
  });
  it("retains full constructor registries, unknown enums and latent activity indexes on native errors", () => {
    const value = workbench(); value.executed = false; value.result = null; value.error = {type: "native_error", message: "activity index outside registry"}; value.model.process_tree = parseNativeJson('{"activities":["duplicate","duplicate","unused"],"root":{"op":"UNKNOWN","op_index":-7,"activity":18446744073709551615,"children":[{"op":"ACTIVITY","op_index":0,"activity":18446744073709551615,"children":[]}]}}') as typeof value.model.process_tree;
    expect(isModelConversionWorkbench(value)).toBe(true); const table = modelConversionTables(value).find(item => item.id === "original_tree_nodes")!; expect(table.rows[0][4]).toBe("18446744073709551615"); expect(modelConversionGraphs(value)[0].graph.nodes).toHaveLength(2); expect(modelConversionSummary(value)).toContain("failed");
  });
  it("preserves signed markings, UINT64_MAX removed mappings and opaque PNML numeric storage", () => {
    const value = workbench("reduce_petri"), raw = nativeJson(value).replace('"old_to_new_place":[0,1]', '"old_to_new_place":[18446744073709551615,1]').replace('"initial_marking":[1,0]', '"initial_marking":[-9223372036854775808,9223372036854775807]'); const parsed = parseNativeJson(raw) as ModelConversionWorkbench;
    expect(isModelConversionWorkbench(parsed)).toBe(true); expect(isModelReductionOptions(parsed.options)).toBe(true); expect(isModelReductionResult(parsed.result!.reduction)).toBe(true); expect(nativeCell(parsed.options!.initial_marking, 0)).toBe("-9223372036854775808");
    const tables = modelConversionTables(parsed), table = tables.find(item => item.id === "complete_old_to_new_place")!; expect(table.rows[0]).toEqual([0, "18446744073709551615", true]); expect(nativeJson(table.records![0])).toBe('{"original_index":0,"returned_index":18446744073709551615,"removed":true}'); const markingTable = tables.find(item => item.id === "/options/initial_marking")!; expect(nativeJson(markingTable.records![0])).toBe('{"value":-9223372036854775808}'); expect(nativeJson(markingTable.records![1])).toBe('{"value":9223372036854775807}'); expect(modelConversionAllTablesCsv(tables)).toContain("-9223372036854775808");
    parsed.model.pnml_document!.stochastic_map = parseNativeJson('{"transitions":[{"weight":1.0000000000000002,"timing":-0.0,"overflow":"INF","tokens":340282366920938463463374607431768211455}]}'); const report = modelConversionReport(saved(parsed)); for (const literal of ["1.0000000000000002", "-0.0", "340282366920938463463374607431768211455"]) expect(report).toContain(literal);
    expect(isModelReductionResult({...parsed.result!.reduction, old_to_new_place: [-1]})).toBe(false);
  });
  it("accepts all six complete result families and explicit errors after or before the native call", () => {
    for (const operation of MODEL_CONVERSION_OPERATIONS) {const value = workbench(operation); expect(isModelConversionWorkbench(value)).toBe(true); expect(isModelConversionWorkbench({...value, executed: false})).toBe(false); value.error = {type: "native_error", message: "native failure"}; value.result = null; expect(isModelConversionWorkbench(value)).toBe(true); value.executed = false; expect(isModelConversionWorkbench(value)).toBe(true);}
    const value = workbench("net_to_tree"); value.input_context.detected_format = "tree_json"; expect(isModelConversionWorkbench(value)).toBe(false);
  });
  it("inspects rebuilt changed-false nets independently and preserves all arc types and exact weights", () => {
    const value = workbench("reduce_petri"), result = value.result!.reduction!; const transition = (result.net.transitions as Array<Record<string, unknown>>)[0]; transition.display_name = null; transition.display_name_supplied = false; transition.reset_inputs = [0]; transition.inhibitor_inputs = [1]; transition.reset_weights = [{negative: true, magnitude: 2}]; transition.inhibitor_weights = [{negative: false, magnitude: 3}];
    expect(isModelConversionWorkbench(value)).toBe(true); expect(result.changed).toBe(false); expect(nativeJson(result.net)).not.toBe(nativeJson(value.model.petri_net)); const graphs = modelConversionGraphs(value); expect(graphs.map(item => item.title)).toEqual(["Original Petri net", "Resulting Petri net"]); expect(graphs[1].graph.edges.some(edge => edge.label === "reset -2")).toBe(true); expect(graphs[1].graph.edges.some(edge => edge.label === "inhibitor 3")).toBe(true);
  });
  it("retains every row, column, nested record and POWL order pair beyond a display page", () => {
    const value = workbench("powl_to_net"), root = {...powl(), kind: "STRICT_PARTIAL_ORDER" as const, kind_index: 4, children: Array.from({length: 32}, (_, index) => powl(`東京,blue;\"line\n${index}`)), order: Array.from({length: 31}, (_, index) => [index, index + 1] as [number, number])}; value.model.powl = root;
    const tables = modelConversionTables(value), nodes = tables.find(table => table.id === "original_powl_nodes")!, order = tables.find(table => table.id === "original_powl_order")!; expect(nodes.rows).toHaveLength(33); expect(order.rows).toHaveLength(31); expect(diagnosticPage(nodes.rows, 1)).toHaveLength(8); expect(diagnosticCsv(nodes)).toContain("31"); expect(nativeJson(nodes.records!.at(-1))).toContain("31");
    const csv = modelConversionAllTablesCsv(tables); expect(csv).toContain("original_powl_order"); expect(csv).toContain('""line'); expect(csv).toContain("source_pnml_error"); expect(nodes.records).toHaveLength(nodes.rows.length);
  });
  it("uploads original bytes and binds exact supplied signed options, origins, booleans and file hash", async () => {
    const model = originalFile(), form = defaultModelConversionForm("reduce_petri"), value = workbench("reduce_petri", model); form.fields = {petri_reduction_initial_marking_json: " [-0,9223372036854775807] ", petri_reduction_final_marking: "-9223372036854775808,0", petri_reduction_parallel_place_fusion: "1"}; form.pnmlFields.pnml_import_stochastic_map = "1";
    value.options = parseNativeJson('{"initial_marking":[0,9223372036854775807],"final_marking":[-9223372036854775808,0],"enable_parallel_place_fusion":true,"enable_abstraction":false,"preserve_join_bridges":false}') as typeof value.options; value.input_context.initial_marking_origin = "explicit_json"; value.input_context.final_marking_origin = "explicit_legacy"; value.pnml_import_options!.import_stochastic_map = true;
    const raw = nativeJson(value), mock = vi.fn(async (_: string, init: RequestInit) => {const body = init.body as FormData; expect(await (body.get("model_file") as File).arrayBuffer()).toEqual(await model.arrayBuffer()); expect(body.get("petri_reduction_initial_marking_json")).toBe(form.fields.petri_reduction_initial_marking_json); return new Response(raw, {status: 200});}); vi.stubGlobal("fetch", mock);
    const run = await runModelConversionWorkbench(model, form); expect(run.raw).toBe(raw); expect(run.input.model_sha256).toMatch(/^[a-f0-9]{64}$/); expect(nativeResponseRequest(run.value)?.path).toBe("/api/model-conversion-workbench"); expect(modelConversionReport(run)).toContain(run.input.model_sha256);
  });
  it("rejects mismatched operation, file content, options and origins while retaining native error envelopes", async () => {
    const model = originalFile(), form = defaultModelConversionForm("reduce_petri"); form.fields.petri_reduction_initial_marking_json = "[]";
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(workbench("net_to_tree", model))))); await expect(runModelConversionWorkbench(model, form)).rejects.toThrow(/does not match/);
    for (const change of [(value: ModelConversionWorkbench) => {value.input_context.dataset = "other.pnml";}, (value: ModelConversionWorkbench) => {value.model.original_model = "different bytes";}, (value: ModelConversionWorkbench) => {value.options!.initial_marking = [1];}, (value: ModelConversionWorkbench) => {value.input_context.initial_marking_origin = "pnml";}]) {const value = workbench("reduce_petri", model); value.options!.initial_marking = []; value.input_context.initial_marking_origin = "explicit_empty_unspecified"; change(value); vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); await expect(runModelConversionWorkbench(model, form)).rejects.toThrow(/does not/);}
    const value = workbench("reduce_petri", model); value.result = null; value.executed = false; value.error = {type: "native_error", message: "signed marking width"}; value.options!.initial_marking = []; value.input_context.initial_marking_origin = "explicit_empty_unspecified"; vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value), {status: 400}))); expect((await runModelConversionWorkbench(model, form)).value.error?.message).toBe("signed marking width");
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"error":"invalid transport"}', {status: 400}))); await expect(runModelConversionWorkbench(model, form)).rejects.toThrow("invalid transport");
  });
  it("prevents canceled and invalidated file generations from publishing a late response", async () => {
    const gate = new ModelConversionRequestGate(), first = gate.issue(), abort = new AbortController(); expect(gate.accepts(first)).toBe(true); gate.invalidate(); expect(gate.accepts(first)).toBe(false); const next = gate.issue(); abort.abort(); expect(gate.accepts(next, abort.signal)).toBe(false);
    const fetchMock = vi.fn(); vi.stubGlobal("fetch", fetchMock); await expect(runModelConversionWorkbench(originalFile(), defaultModelConversionForm(), abort.signal)).rejects.toThrow(); expect(fetchMock).not.toHaveBeenCalled();
    const active = new AbortController(); vi.stubGlobal("fetch", vi.fn(async () => {active.abort(); return new Response(nativeJson(workbench()));})); await expect(runModelConversionWorkbench(originalFile(), defaultModelConversionForm(), active.signal)).rejects.toThrow();
  });
  it("exports every same-run artifact with exact UTF/WTF bytes and lossless ASCII table filenames", async () => {
    const value = workbench(), run = saved(value), content = "ASCII 東京🧪 \ud800 lone \udc00 mixed\u0000\n"; for (const artifact of MODEL_CONVERSION_ARTIFACTS) {value.exports[artifact] = content; expect(new Uint8Array(await modelConversionArtifactFile(run, artifact).arrayBuffer())).toEqual(workflowInputNativeBytes(content));} value.exports.result_powl = null; value.exports.result_powl_error = "Native serialization failed"; expect(() => modelConversionArtifactFile(run, "result_powl")).toThrow(value.exports.result_powl_error);
    const id = "__proto__/東京/\ud800/\udc00/%\u0000", encoded = modelConversionTableFileName(id); expect(encoded).toMatch(/^[A-Za-z0-9_.%\-]+$/); expect(encoded.replace(/%([a-f0-9]{4})/g, (_, code: string) => String.fromCharCode(parseInt(code, 16)))).toBe(id); expect(new Uint8Array(await run.input.model.arrayBuffer())).toEqual(workflowInputNativeBytes(text));
  });
});
