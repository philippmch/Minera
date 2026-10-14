import { afterEach, describe, expect, it, vi } from "vitest";
import {
  WORKFLOW_INPUT_CONTROLS, WORKFLOW_INPUT_FORMATS, WORKFLOW_INPUT_SHAPES, WORKFLOW_INPUT_XES_VARIANTS,
  WorkflowInputRequestGate, defaultWorkflowInputForm, isWorkflowInputDiscovery, isWorkflowInputNativeNet, isWorkflowInputTable,
  runWorkflowInput, workflowInputAllTablesCsv, workflowInputControlActive, workflowInputDfgExample, workflowInputFields,
  workflowInputGraph, workflowInputModelFile, workflowInputNativeBytes, workflowInputReport, workflowInputSummary, workflowInputTables,
  type WorkflowInputDiscovery, type WorkflowInputRun, type WorkflowInputTable,
} from "../src/workflow-input-workbench";
import { diagnosticCsv, diagnosticPage } from "../src/model-diagnostics";
import { nativeCell, nativeJson, parseNativeJson } from "../src/native-json";
import type { MineraExport } from "../src/types";

const original = () => new File(["case_id,activity,timestamp\n1,A,2024-01-01T09:00:00Z\n"], "original.csv", {type: "text/csv"});
function coreNet(): Record<string, unknown> {
  return {activity_names: ["A"], place_count: 2, has_source: true, has_sink: true, source: 0, sink: 1, discovery_markings: null,
    transitions: [{index: 0, activity_id: 0, activity_name: "A", activity_is_null: false, display_name_supplied: false, display_name: null,
      inputs: [0], outputs: [1], reset_inputs: [], inhibitor_inputs: [], input_weights: [], output_weights: [], reset_weights: [], inhibitor_weights: []}]};
}
function table(): WorkflowInputTable {
  return {columns: ["case:concept:name", "concept:name", "time:timestamp"], dtypes: ["object", "object", "datetime64[ns]"], rows: [
    [{key: "case:concept:name", kind: "string", value: "1"}, {key: "concept:name", kind: "string", value: "A"}, {key: "time:timestamp", kind: "date", value: "2024-01-01T09:00:00Z"}],
  ]};
}
function workflow(file = original()): WorkflowInputDiscovery {
  return {family: "WORKFLOW_INPUT_DISCOVERY", method: "alpha", variant: "classic", computed: true, error: null,
    input_context: {adapter: "log", format: "auto", origin: "uploaded_csv_rows", dataset: file.name, original_input_bytes: file.size, input_event_count: 1, input_case_count: 1},
    input_before: table(), input_after: table(), dfg_input: null, native_net: coreNet(), native_heuristics: null, result_error: null, alpha_discovery: null,
    result: {dataset: file.name, trace_count: 1, event_count: 1, activities: [{name: "A", count: 1, start_count: 1, end_count: 1}], edges: [], variants: [], petri_net: {place_count: 2, source: 0, sink: 1, transitions: [{activity: 0, inputs: [0], outputs: [1]}]}} as MineraExport,
    pnml: "<pnml></pnml>", bpmn: "<definitions></definitions>", ptml: null, pnml_error: null, bpmn_error: null, ptml_error: "Native PTML conversion failed"};
}
function run(value: WorkflowInputDiscovery, file = original()): WorkflowInputRun {
  const form = defaultWorkflowInputForm();
  form.family = value.method; form.shape = value.input_context.adapter; form.format = value.input_context.format;
  if (value.method === "alpha") form.alphaVariant = value.variant as "classic" | "plus"; else form.heuristicVariant = value.variant as "classic" | "plusplus";
  const fields = workflowInputFields(form, file.name);
  return {value, raw: nativeJson(value), fields, tables: workflowInputTables(value, fields), input: file, input_sha256: "fixture hash", http_status: value.computed ? 200 : 400};
}
function mutableFailure(file = original()): WorkflowInputDiscovery {
  const value = workflow(file);
  value.method = "heuristic"; value.variant = "plusplus"; value.input_context.adapter = "pandas_mutable"; value.input_context.format = "table";
  value.computed = false; value.error = {type: "AttributeError", message: "Can only use .dt accessor with datetimelike values"};
  value.result = null; value.native_net = null; value.alpha_discovery = null;
  value.pnml = value.bpmn = value.ptml = null; value.ptml_error = null;
  value.input_after = {...table(), columns: [...table().columns, "start_timestamp"], dtypes: [...table().dtypes!, "object"], rows: table().rows.map(row => [...row, {key: "start_timestamp", kind: "string" as const, value: "created before native error"}])};
  return value;
}
afterEach(() => vi.unstubAllGlobals());

describe("independent workflow input controls", () => {
  it("exposes every native Alpha selector/sort/cap and all Heuristic thresholds, decorations and timing controls", () => {
    expect(WORKFLOW_INPUT_SHAPES).toEqual(["log", "stream", "dataframe", "lazyframe", "dfg", "pandas_mutable"]);
    expect(WORKFLOW_INPUT_FORMATS).toEqual(["auto", "table", "dfg_json"]);
    expect(WORKFLOW_INPUT_CONTROLS.alpha.map(control => control.key)).toEqual(["alpha_activity_attribute", "alpha_case_id_attribute", "alpha_timestamp_attribute", "alpha_start_timestamp_attribute", "alpha_sort_caseid_required", "alpha_sort_timestamp_along_case_id", "alpha_remove_unconnected", "alpha_max_rows_to_materialize"]);
    expect(WORKFLOW_INPUT_CONTROLS.heuristic.map(control => control.key)).toEqual(["heuristic_activity_attribute", "heuristic_case_id_attribute", "heuristic_timestamp_attribute", "heuristic_start_timestamp_attribute", "heuristic_lifecycle_instance_attribute", "dependency_threshold", "and_threshold", "min_edge_frequency", "heuristic_min_activity_count", "heuristic_connect_all", "heuristic_decoration", "heuristic_performance_aggregation", "heuristic_business_hours", "heuristic_business_hour_slots", "heuristic_dfg_pre_cleaning_noise_threshold", "heuristic_loop_length_two_threshold", "heuristic_keep_once_per_case", "heuristic_most_common_paths", "heuristic_plusplus_decoration_selector", "heuristic_plusplus_decoration_value", "heuristic_plusplus_min_activity_count", "heuristic_plusplus_min_frequency"]);
  });
  it("preserves exact uint64/float/JSON/selector text and only omits blank native defaults", () => {
    for (const family of ["alpha", "heuristic"] as const) {
      const form = defaultWorkflowInputForm(); form.family = family; form.heuristicVariant = "plusplus";
      for (const control of WORKFLOW_INPUT_CONTROLS[family]) form.fields[control.key] = control.kind === "selector" ? "  literal:東京,;  " : control.kind === "integer" ? "18446744073709551615" : control.kind === "number" ? " 0.650000000000000000001 " : control.kind === "native_number" ? "-Inf" : control.kind === "enum" ? control.choices![0] : control.kind === "boolean" ? "false" : control.kind === "json" ? ' {"__proto__": [], "東京":18446744073709551615} ' : "none";
      const submitted = Object.fromEntries(workflowInputFields(form, "input.csv")), selectors = JSON.parse(submitted.discovery_selectors_json);
      for (const control of WORKFLOW_INPUT_CONTROLS[family]) expect(control.kind === "selector" ? selectors[control.key] : submitted[control.key]).toBe(form.fields[control.key]);
      expect(submitted.source_id).toBeUndefined(); expect(submitted.include_source).toBeUndefined();
      expect(workflowInputFields(defaultWorkflowInputForm(), "input.csv")).toEqual([["discovery_family", "alpha"], ["input_shape", "log"], ["input_format", "auto"], ["alpha_variant", "classic"]]);
    }
  });
  it("sends literal empty and whitespace selectors through the exact carrier without normalizing them", () => {
    const form = defaultWorkflowInputForm(); form.family = "heuristic"; form.shape = "pandas_mutable"; form.heuristicVariant = "plusplus";
    form.explicitEmptySelectors = {heuristic_activity_attribute: true, heuristic_start_timestamp_attribute: true};
    form.fields = {heuristic_case_id_attribute: "\u00a0", heuristic_timestamp_attribute: "  ", heuristic_plusplus_decoration_value: "null"};
    const fields = Object.fromEntries(workflowInputFields(form, "input.csv"));
    expect(JSON.parse(fields.discovery_selectors_json)).toEqual({heuristic_activity_attribute: "", heuristic_case_id_attribute: "\u00a0", heuristic_timestamp_attribute: "  ", heuristic_start_timestamp_attribute: ""});
    expect(fields.heuristic_plusplus_decoration_value).toBe("null");
    expect(fields.heuristic_activity_attribute).toBeUndefined();
  });
  it("retains inactive PLUSPLUS controls and submits native nonfinite cutoff text only for PLUSPLUS", () => {
    const form = defaultWorkflowInputForm(); form.family = "heuristic";
    form.fields = {heuristic_plusplus_min_activity_count: "NaN", heuristic_plusplus_min_frequency: "-Infinity", heuristic_plusplus_decoration_value: "[]"};
    expect(Object.fromEntries(workflowInputFields(form, "input.csv")).heuristic_plusplus_min_activity_count).toBeUndefined();
    form.heuristicVariant = "plusplus";
    expect(Object.fromEntries(workflowInputFields(form, "input.csv"))).toMatchObject(form.fields);
    for (const value of ["nan", "+Inf", "-Inf", "-.5", "1e-3"]) {form.fields.heuristic_plusplus_min_frequency = value; expect(Object.fromEntries(workflowInputFields(form, "input.csv")).heuristic_plusplus_min_frequency).toBe(value);}
    expect(workflowInputControlActive(WORKFLOW_INPUT_CONTROLS.heuristic.at(-1)!, form)).toBe(true);
  });
  it("supports all original log adapters, typed tables, import variants and explicit empty JSON", () => {
    const form = defaultWorkflowInputForm();
    form.importFields = {case_column: "case", activity_column: "task", xes_max_traces: "0", xes_require_timestamp: "false", xes_max_decompressed_bytes: "18446744073709551615"};
    for (const shape of ["log", "stream", "dataframe", "lazyframe"] as const) {
      form.shape = shape; expect(Object.fromEntries(workflowInputFields(form, "input.TSV")).delimiter).toBe("\t");
      form.format = "table"; expect(Object.fromEntries(workflowInputFields(form, "input.json")).input_format).toBe("table");
      expect(Object.fromEntries(workflowInputFields(form, "input.json")).case_column).toBeUndefined(); form.format = "auto";
    }
    for (const variant of WORKFLOW_INPUT_XES_VARIANTS) {form.importFields.xes_variant = variant; expect(Object.fromEntries(workflowInputFields(form, "input.xes.gz")).xes_variant).toBe(variant);}
    form.family = "heuristic"; form.heuristicVariant = "plusplus";
    for (const text of ["{}", "[]", "null", "false", '"frequency"']) {form.fields.heuristic_plusplus_decoration_value = text; expect(Object.fromEntries(workflowInputFields(form, "input.xes")).heuristic_plusplus_decoration_value).toBe(text);}
  });
  it("requires Classic for source-free DFG and PLUSPLUS for mutable pandas without silently rewriting choices", () => {
    const form = defaultWorkflowInputForm(); form.shape = "dfg"; form.format = "dfg_json";
    expect(Object.fromEntries(workflowInputFields(form, "input.json"))).toMatchObject({input_shape: "dfg", input_format: "dfg_json"});
    form.alphaVariant = "plus"; expect(() => workflowInputFields(form, "input.json")).toThrow(/Classic/);
    form.shape = "pandas_mutable"; form.format = "table"; expect(() => workflowInputFields(form, "input.json")).toThrow(/Heuristics/);
    form.family = "heuristic"; form.heuristicVariant = "plusplus"; expect(() => workflowInputFields(form, "input.json")).not.toThrow();
    form.fields.dependency_threshold = "NaN"; expect(() => workflowInputFields(form, "input.json")).toThrow(/finite/);
    expect(JSON.parse(workflowInputDfgExample("alpha"))).toMatchObject({kind: "sparse"}); expect(JSON.parse(workflowInputDfgExample("heuristic"))).toHaveProperty("frequency_triples", []);
  });
});

describe("complete original input, failures and request identity", () => {
  it("captures original file/hash, exact multipart fields and every native response without cached-source calls", async () => {
    const file = original(), form = defaultWorkflowInputForm(), controller = new AbortController();
    form.fields.alpha_max_rows_to_materialize = "18446744073709551615";
    const raw = JSON.stringify(workflow(file)), fetcher = vi.fn().mockResolvedValue(new Response(raw)); vi.stubGlobal("fetch", fetcher);
    const result = await runWorkflowInput(file, form, controller.signal), [path, request] = fetcher.mock.calls[0];
    expect(path).toBe("/api/discover-workflow-input"); expect(request.method).toBe("POST"); expect(request.signal).toBe(controller.signal);
    expect(request.body.get("input").name).toBe(file.name); expect(request.body.get("alpha_max_rows_to_materialize")).toBe("18446744073709551615");
    expect(result.raw).toBe(raw); expect(result.input).toBe(file); expect(result.input_sha256).toMatch(/^[0-9a-f]{64}$/);
    expect(workflowInputReport(result)).toContain(result.input_sha256); expect(fetcher).toHaveBeenCalledTimes(1);
    form.fields.alpha_max_rows_to_materialize = "0"; expect(Object.fromEntries(result.fields).alpha_max_rows_to_materialize).toBe("18446744073709551615");
  });
  it("returns structured HTTP400 native failures with before/after mutation tables and exact report", async () => {
    const file = new File(["{}"], "typed-table.json"), value = mutableFailure(file), form = defaultWorkflowInputForm();
    form.family = "heuristic"; form.heuristicVariant = "plusplus"; form.shape = "pandas_mutable"; form.format = "table";
    const raw = JSON.stringify(value); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(raw, {status: 400})));
    const result = await runWorkflowInput(file, form);
    expect(result.http_status).toBe(400); expect(result.value.computed).toBe(false); expect(result.value.error?.type).toBe("AttributeError");
    expect(result.tables.find(item => item.id === "input_before_cells")!.rows).toHaveLength(3); expect(result.tables.find(item => item.id === "input_after_cells")!.rows).toHaveLength(4);
    expect(workflowInputAllTablesCsv(result)).toContain("created before native error"); expect(workflowInputReport(result)).toContain('"http_status": 400');
  });
  it("rejects mismatched files/shapes/variants and generic transport failures while not fabricating model results", async () => {
    for (const mutate of [(value: WorkflowInputDiscovery) => {value.input_context.dataset = "other";}, (value: WorkflowInputDiscovery) => {++value.input_context.original_input_bytes;}, (value: WorkflowInputDiscovery) => {value.input_context.adapter = "stream";}]) {
      const file = original(), value = workflow(file); mutate(value); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify(value))));
      await expect(runWorkflowInput(file, defaultWorkflowInputForm())).rejects.toThrow(/does not match/);
    }
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response('{"error":"bad typed input"}', {status: 400})));
    await expect(runWorkflowInput(original(), defaultWorkflowInputForm())).rejects.toThrow("bad typed input");
    expect(workflowInputGraph(mutableFailure())).toBeNull(); expect(workflowInputSummary(run(mutableFailure()))).toContain("AttributeError");
  });
  it("blocks cancellation even if transport ignores the signal and invalidates late file/request identities", async () => {
    const file = original(), controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(async () => {controller.abort(); return new Response(JSON.stringify(workflow(file)));}));
    await expect(runWorkflowInput(file, defaultWorkflowInputForm(), controller.signal)).rejects.toThrow();
    const gate = new WorkflowInputRequestGate(), old = gate.issue(), active = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(active)).toBe(true);
    gate.invalidate(); expect(gate.accepts(active)).toBe(false); const cancelled = gate.issue(); expect(gate.accepts(cancelled, controller.signal)).toBe(false);
  });
  it("keeps same-model export failures in the native run and never substitutes a different model", async () => {
    const fixture = run(workflow()); expect((await workflowInputModelFile(fixture, "pnml").text())).toBe(fixture.value.pnml);
    expect(() => workflowInputModelFile(fixture, "ptml")).toThrow("Native PTML conversion failed");
    expect(workflowInputReport(fixture)).toContain("Native PTML conversion failed");
  });
  it("preserves lone high/low surrogates, valid pairs and mixed Unicode in actual model File bytes", async () => {
    const encode = (value: string) => Array.from(new TextEncoder().encode(value));
    const mixed = "Latin ä 東京 🧪 |\ud800|\udc00|\ud800\udc00";
    const expected = new Uint8Array([...encode("Latin ä 東京 🧪 |"), 0xed, 0xa0, 0x80, ...encode("|"), 0xed, 0xb0, 0x80, ...encode("|𐀀")]);
    expect(workflowInputNativeBytes(mixed)).toEqual(expected);
    expect(workflowInputNativeBytes("\ud800")).toEqual(new Uint8Array([0xed, 0xa0, 0x80]));
    expect(workflowInputNativeBytes("\udc00")).toEqual(new Uint8Array([0xed, 0xb0, 0x80]));
    expect(workflowInputNativeBytes("\ud800\udc00")).toEqual(new Uint8Array([0xf0, 0x90, 0x80, 0x80]));
    const fixture = run(workflow()); fixture.value.pnml = `<pnml>${mixed}</pnml>`;
    const model = workflowInputModelFile(fixture, "pnml");
    expect(new Uint8Array(await model.arrayBuffer())).toEqual(new Uint8Array([...encode("<pnml>"), ...expected, ...encode("</pnml>")]));
    expect(new Uint8Array(await new Blob([workflowInputNativeBytes(mixed)]).arrayBuffer())).toEqual(expected);
  });
});

describe("native structural fidelity and complete inspection", () => {
  it("accepts mutable failure envelopes and rejects absent containers, mismatched shapes and fake success", () => {
    expect(isWorkflowInputDiscovery(workflow())).toBe(true); expect(isWorkflowInputDiscovery(mutableFailure())).toBe(true);
    const failure = mutableFailure(); failure.result = workflow().result; expect(isWorkflowInputDiscovery(failure)).toBe(false);
    const malformed = workflow(); malformed.input_after = {columns: ["a"], rows: [[null, null]]}; expect(isWorkflowInputDiscovery(malformed)).toBe(false);
    expect(isWorkflowInputDiscovery({family: "WORKFLOW_INPUT_DISCOVERY"})).toBe(false); expect(isWorkflowInputDiscovery(null)).toBe(false);
    expect(isWorkflowInputTable({columns: ["a"], rows: [[null], [{key: "a", kind: "string", value: "", null: true}]], dtypes: ["object"]})).toBe(true);
  });
  it("retains an unknown original typed-table case count without inventing zero or replacing discovery counts", () => {
    const value = mutableFailure(); value.input_context.origin = "uploaded_typed_table"; value.input_context.input_case_count = null;
    value.discovery_context = null;
    expect(isWorkflowInputDiscovery(value)).toBe(true); expect(workflowInputSummary(run(value))).toContain("Unknown input case count");
    expect(workflowInputSummary(run(value))).not.toContain("0 input cases");
    const successful = workflow(); successful.input_context.input_case_count = null; successful.discovery_context = {case_count: 7, event_count: 1};
    expect(isWorkflowInputDiscovery(successful)).toBe(true); expect(workflowInputReport(run(successful))).toContain('"case_count": 7');
  });
  it("validates native NA recursively in class-based indices, categories and metadata while preserving exact cells", () => {
    const na = {key: "NA", kind: "na", container: "scalar", value: "<NA>", typed_value: "NA"};
    const signed = {key: "same", kind: "int", value: "1", typed_value: 1, integer_storage: "int64"};
    const unsigned = {key: "same", kind: "int", value: "18446744073709551615", typed_value: 1, integer_storage: "uint64"};
    let nested: Record<string, unknown> = na;
    for (let depth = 0; depth < 70; ++depth) nested = {key: "nested", kind: "string", container: "tuple", value: "", children: [nested]};
    const typed = parseNativeJson(JSON.stringify({columns: ["literal", "typed"], rows: [[na, nested]], dtypes: ["category", "object"],
      index: {class: "MultiIndex", dtype: "object", names: [null, " idx "], labels: [{kind: "tuple", items: [{kind: "scalar", value: signed}, {kind: "tuple", items: [{kind: "scalar", value: na}]}]}]},
      categorical_dtypes: [{categories_dtype: "object", ordered: true, categories: [na, unsigned]}, null],
      attributes: [{key: "wrapped", kind: "string", value: "", scalar_wrapper: true, scalar_children_list: true, children: [signed, unsigned, na]}],
      metadata: {extensions: [{name: "literal", prefix: "", uri: ""}], classifiers: [{name: "duplicates", keys: ["same", "same"]}], trace_globals: [na], event_globals: [nested], properties: [unsigned], globals: [{scope: "event", attributes: [na]}]},
    }).replaceAll('"typed_value":1,"integer_storage":"uint64"', '"typed_value":18446744073709551615,"integer_storage":"uint64"'));
    expect(isWorkflowInputTable(typed)).toBe(true); if (!isWorkflowInputTable(typed)) throw new Error("Invalid native table fixture");
    expect(nativeJson(typed)).toContain('"typed_value":18446744073709551615');
    const value = mutableFailure(); value.input_before = typed; value.input_after = typed;
    expect(isWorkflowInputDiscovery(value)).toBe(true);
    const cells = workflowInputTables(value).find(item => item.id === "input_before_cells")!;
    expect(cells.rows[0].slice(4, 8)).toEqual(["present", "na", "<NA>", "NA"]); expect(nativeJson(cells.records![1])).toContain('"container":"tuple"');
    const badIndex = {...typed, index: {class: "MultiIndex", dtype: "object", names: [null], labels: [{kind: "tuple", items: [{kind: "scalar", value: {...na, kind: "unknown"}}]}]}};
    expect(isWorkflowInputTable(badIndex)).toBe(false);
    expect(isWorkflowInputTable({...typed, categorical_dtypes: [{categories_dtype: "object", ordered: true, categories: [{...na, children: [false]}]}, null]})).toBe(false);
  });
  it("preserves completed native Alpha+ results with detached markings when the analysis projection is unavailable", () => {
    const value = workflow(); value.variant = "plus"; value.result = null; value.result_error = "No live source boundary";
    value.alpha_discovery = {family: "ALPHA_PLUS", places: [{id: 0, name: {boundary: "end"}, initial: 0, final: 1}], transitions: [{id: 0, label: "A", inputs: [], outputs: [0]}], detached_initial_marking: [{name: {before: ["A"], after: [null]}, tokens: 1}], detached_final_marking: []};
    const native = value.native_net!; native.place_count = 1; native.has_source = false; native.source = null; native.sink = 0;
    (native.transitions as Array<Record<string, unknown>>)[0].inputs = []; (native.transitions as Array<Record<string, unknown>>)[0].outputs = [0];
    native.discovery_markings = {detached_places: [{id: 0, name: {type: "object", value: {before: {type: "array", value: [{type: "string", value: "A"}]}}}}], initial: [{namespace: "detached", place_id: 0, tokens: 1}], final: [{namespace: "root", place_id: 0, tokens: 1}]};
    expect(isWorkflowInputDiscovery(value)).toBe(true); expect(workflowInputGraph(value)).toBeNull();
    const inspected = run(value); expect(workflowInputReport(inspected)).toContain('"namespace": "detached"'); expect(workflowInputAllTablesCsv(inspected)).toContain("detached_initial_marking");
  });
  it("validates original native transition names, implicit/explicit arc weights and separate marking namespaces", () => {
    const net = coreNet(), transition = (net.transitions as Array<Record<string, unknown>>)[0];
    transition.input_weights = [{negative: true, magnitude: 0}]; transition.display_name_supplied = true; transition.display_name = "";
    expect(isWorkflowInputNativeNet(net)).toBe(true); transition.activity_name = "wrong"; expect(isWorkflowInputNativeNet(net)).toBe(false); transition.activity_name = "A";
    transition.input_weights = [{negative: false, magnitude: 1}, {negative: false, magnitude: 1}]; expect(isWorkflowInputNativeNet(net)).toBe(false);
    transition.input_weights = []; net.discovery_markings = {detached_places: [], initial: [{namespace: "detached", place_id: 0, tokens: 1}], final: []}; expect(isWorkflowInputNativeNet(net)).toBe(false);
  });
  it("preserves arbitrary-width weight/marking literals, typed cells and complete long nested values in every export", () => {
    const value = workflow(), net = value.native_net!, transition = (net.transitions as Array<Record<string, unknown>>)[0];
    transition.input_weights = [{negative: false, magnitude: 1}]; net.discovery_markings = {detached_places: [], initial: [{namespace: "root", place_id: 0, tokens: 1}], final: []};
    value.input_before = {columns: ["int", "missing", "null", "large"], rows: [[{key: "int", kind: "int", value: "9007199254740993", typed_value: 1, integer_storage: "uint64"} as never, null, {key: "null", kind: "string", value: "", null: true}, {key: "large", kind: "string", value: "東京" + "z".repeat(700)}]], dtypes: ["uint64", "object", "object", "object"]};
    const raw = JSON.stringify(value).replace('"magnitude":1', '"magnitude":184467440737095516160000000000000000000').replace('"tokens":1', '"tokens":-184467440737095516160000000000000000000').replace('"typed_value":1', '"typed_value":9007199254740993');
    const parsed = parseNativeJson(raw); expect(isWorkflowInputDiscovery(parsed)).toBe(true); if (!isWorkflowInputDiscovery(parsed)) throw new Error("Invalid fixture");
    const inspected = run(parsed), cells = inspected.tables.find(item => item.id === "input_before_cells")!;
    expect(cells.rows[1][4]).toBe("missing"); expect(cells.rows[2][4]).toBe("present null");
    expect(diagnosticCsv(cells)).toContain("9007199254740993"); expect(nativeJson(cells.records![0])).toContain('"integer_storage":"uint64"');
    expect(workflowInputAllTablesCsv(inspected)).toContain("184467440737095516160000000000000000000"); expect(workflowInputReport(inspected)).toContain("-184467440737095516160000000000000000000");
    expect(workflowInputAllTablesCsv(inspected)).toContain("z".repeat(700)); expect(nativeCell(parsed.input_before!.rows[0][0]!, "typed_value")).toBe("9007199254740993");
  });
  it("exports all typed table rows/columns beyond paging and complete opaque heuristic performance metadata", () => {
    const value = workflow(); value.method = "heuristic"; value.variant = "classic";
    value.result!.heuristic_petri_net = value.result!.petri_net; value.result!.heuristics_net = {variant: "classic", edges: [], start_activities: [], end_activities: []};
    value.native_heuristics = {activities: ["A"], selected_activity: "__proto__", performance: [{type: "samples", value: ["NaN", "INF", "-INF", 0.1]}], metadata: {activity_counts_supplied: true, activity_counts: {}, performance_values: []}};
    value.input_after = {columns: Array.from({length: 12}, (_, column) => `column:${column}`), rows: Array.from({length: 63}, (_, row) => Array.from({length: 12}, (_, column) => ({kind: "string" as const, value: `value:${row}:${column}`})))};
    expect(isWorkflowInputDiscovery(value)).toBe(true);
    const inspected = run(value), cells = inspected.tables.find(item => item.id === "input_after_cells")!;
    expect(cells.rows).toHaveLength(756); expect(diagnosticPage(cells.rows, 30)).toHaveLength(6);
    expect(workflowInputAllTablesCsv(inspected)).toContain("value:62:11"); expect(workflowInputReport(inspected)).toContain('"-INF"'); expect(workflowInputAllTablesCsv(inspected)).toContain("activity_counts_supplied");
  });
});
