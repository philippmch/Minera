import {afterEach, describe, expect, it, vi} from "vitest";
import {
  TREE_ALIGNMENT_CONTROLS, TREE_ALIGNMENT_VARIANTS, ProcessTreeAlignmentRequestGate, defaultProcessTreeAlignmentForm,
  isAlignmentNativeTree, isNativeProcessTreeAlignment, isNativeTreeAlignmentOptions, isProcessTreeAlignmentWorkbench,
  processTreeAlignmentAllTablesCsv, processTreeAlignmentArtifactFile, processTreeAlignmentControlFields, processTreeAlignmentFields,
  processTreeAlignmentReport, processTreeAlignmentSucceeded, processTreeAlignmentSummary, processTreeAlignmentTableFileName,
  processTreeAlignmentTables, processTreeAlignmentTreeGraph, processTreeAlignmentMilliseconds, runProcessTreeAlignmentWorkbench,
  type AlignmentNativeTree, type NativeProcessTreeAlignment, type ProcessTreeAlignmentForm, type ProcessTreeAlignmentRun, type ProcessTreeAlignmentWorkbench, type TreeAlignmentVariant,
} from "../src/process-tree-alignment-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../src/token-replay-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseRequest} from "../src/native-response";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";

const inputFile = () => new File(["case,activity,time\n1,A,2026-10-14\n"], "original.csv");
const modelFile = () => new File(["<ptml>original model</ptml>"], "original.ptml");
const tree = (): AlignmentNativeTree => ({activities: ["A", "A", "unused 東京"], root: {op: "ACTIVITY", op_index: 0, activity: 0, children: []}});
function nativeResult(variant: TreeAlignmentVariant = "SEARCH_GRAPH_PT"): NativeProcessTreeAlignment {
  return {computed: true, completed: true, timed_out: false, exhausted: false, variant, variant_index: TREE_ALIGNMENT_VARIANTS.indexOf(variant), total_cost: 0, fitness: 1,
    diagnostics: {visited_states: 2, queued_states: 3, traversed_arcs: 1, lp_solved: 0, traces_aligned: 1, message: variant === "MILP" ? "dependency-free residual A*" : "native search"},
    traces: [{trace: ["A"], moves: [{type: "sync", type_index: 0, log_activity: "A", model_activity: "A", activity: "A"}], cost: 0, fitness: 1, completed: true, timed_out: false}]};
}
function workbench(variant: TreeAlignmentVariant = "SEARCH_GRAPH_PT", shape: ProcessTreeAlignmentForm["inputShape"] = "log", input = inputFile(), model = modelFile()): ProcessTreeAlignmentWorkbench {
  return {family: "PROCESS_TREE_ALIGNMENT_WORKBENCH", variant, input_shape: shape, result_shape: "log", executed: true, error: null,
    input_context: {format: shape === "words" ? "trace_words" : "auto", detected_format: shape === "words" ? "trace_words" : "csv", dataset: input.name, origin: "uploaded_grouped_log", original_input_bytes: input.size, input_event_count: 1, input_case_count: 1, checked_event_count: 1, checked_case_count: 1, trace_index: shape === "trace" ? 0 : null, activity_attribute: "concept:name", case_id_attribute: "case:concept:name"},
    model_context: {format: "auto", detected_format: "ptml", dataset: model.name, original_input_bytes: model.size, native_tree: tree()},
    options_context: {variant, variant_index: TREE_ALIGNMENT_VARIANTS.indexOf(variant), max_states: 0, max_time_seconds: 0, max_total_time_seconds: 0, max_trace_length: 1, max_process_tree_height: 1, max_trace_time_seconds: 0, max_per_trace_time_seconds: 0, use_global_cache: false, batch_size: 1000, clear_cache: true, activity_attribute: "concept:name"},
    input_before: null, input_after: null, input_trace_words: shape === "words" ? ["A"] : null, input_log: {traces: [{case_id: "1", events: [{activity: "A"}]}]}, input_log_error: null,
    checked_log: {traces: [{case_id: "1", events: [{activity: "A"}]}]}, checked_log_error: null, result: nativeResult(variant), native_net: null,
    conversion_context: {variant: "standard", computed: true, error: null}, source_xes: "<log>A</log>", source_xes_error: null,
    ptml: "<ptml>A</ptml>", ptml_error: null, pnml: "<pnml>A</pnml>", pnml_error: null, bpmn: "<bpmn>A</bpmn>", bpmn_error: null};
}
function saved(value: ProcessTreeAlignmentWorkbench): ProcessTreeAlignmentRun {
  const input = inputFile(), model = modelFile(), form = defaultProcessTreeAlignmentForm(); form.variant = value.variant; form.inputShape = value.input_shape; form.inputFormat = value.input_context.format;
  const fields = processTreeAlignmentFields(form, input.name);
  return {value, raw: nativeJson(value), fields, tables: processTreeAlignmentTables(value, fields), http_status: value.executed ? 200 : 400, inputs: {input, model, input_sha256: "input hash", model_sha256: "model hash"}};
}
afterEach(() => vi.unstubAllGlobals());

describe("independent original-model process-tree alignment", () => {
  it("exposes all four variants and all twelve native options plus typed-table grouping", () => {
    expect(TREE_ALIGNMENT_VARIANTS).toEqual(["SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP", "APPROXIMATED_ORIGINAL"]);
    expect(TREE_ALIGNMENT_CONTROLS.map(control => control.key)).toEqual(["alignment_case_id_attribute", "pt_alignment_activity_attribute", "pt_alignment_max_states", "pt_alignment_max_time_ms", "pt_alignment_max_total_time_ms", "pt_alignment_max_trace_length", "pt_alignment_max_process_tree_height", "pt_alignment_max_trace_time_ms", "pt_alignment_max_per_trace_time_ms", "pt_alignment_use_global_cache", "pt_alignment_batch_size", "pt_alignment_clear_cache"]);
    expect(TREE_ALIGNMENT_CONTROLS.filter(control => control.key.endsWith("_ms"))).toHaveLength(4);
  });
  it("preserves raw numeric strings, unlimited zeros and literal selectors without converting request text", () => {
    const values: Record<string, string> = {};
    for (const control of TREE_ALIGNMENT_CONTROLS) values[control.key] = control.kind === "selector" ? "\u00a0 東京,;\u0000\n" : control.kind === "integer" ? " 0 " : control.kind === "positive" ? " 18446744073709551615 " : control.kind === "number" ? " -0.000000000000000000000 " : "false";
    const result = processTreeAlignmentControlFields(values);
    for (const control of TREE_ALIGNMENT_CONTROLS) expect(control.kind === "selector" ? result.selectors[control.key] : Object.fromEntries(result.fields)[control.key]).toBe(values[control.key]);
    expect(processTreeAlignmentControlFields({}).fields).toEqual([]);
    expect(processTreeAlignmentControlFields({}, {alignment_case_id_attribute: true, pt_alignment_activity_attribute: true}).selectors).toEqual({alignment_case_id_attribute: "", pt_alignment_activity_attribute: ""});
    const form = defaultProcessTreeAlignmentForm(); form.fields = values; const fields = Object.fromEntries(processTreeAlignmentFields(form, "original.csv"));
    expect(JSON.parse(fields.alignment_selectors_json)).toEqual(result.selectors); expect(fields.pt_alignment_activity_attribute).toBeUndefined(); expect(fields.source_id).toBeUndefined();
  });
  it("rejects malformed limits while keeping zero approximation thresholds distinct from positive batch size", () => {
    for (const key of ["pt_alignment_max_states", "pt_alignment_max_trace_length", "pt_alignment_max_process_tree_height"]) expect(processTreeAlignmentControlFields({[key]: "0"}).fields).toEqual([[key, "0"]]);
    expect(() => processTreeAlignmentControlFields({pt_alignment_batch_size: "0"})).toThrow(/positive/);
    expect(() => processTreeAlignmentControlFields({pt_alignment_max_states: "18446744073709551616"})).toThrow(/unsigned/);
    expect(() => processTreeAlignmentControlFields({pt_alignment_max_trace_length: "1.5"})).toThrow(/unsigned/);
    for (const value of ["-1", "NaN", "INF", "1e999"]) expect(() => processTreeAlignmentControlFields({pt_alignment_max_time_ms: value})).toThrow(/finite/);
    expect(processTreeAlignmentMilliseconds("0x1p+20")).toBe(1048576); expect(processTreeAlignmentMilliseconds("0X1.8P+10")).toBe(1536);
    expect(processTreeAlignmentMilliseconds("0x1fffffffffffffp-52")).toBe(2 - 2 ** -52); expect(processTreeAlignmentMilliseconds("0x1fffffffffffff8p-56")).toBe(2);
    expect(processTreeAlignmentMilliseconds("0x1.fffffffffffffp1023")).toBe(Number.MAX_VALUE); expect(processTreeAlignmentMilliseconds("0x1p1024")).toBeUndefined();
    expect(() => processTreeAlignmentControlFields({pt_alignment_clear_cache: "maybe"})).toThrow(/true or false/);
  });
  it("retains every import field, independent model format, conversion and exact selected-trace index", () => {
    const form = defaultProcessTreeAlignmentForm(); form.variant = "APPROXIMATED_ORIGINAL"; form.modelFormat = "tree_json"; form.conversion = "transition_bordered"; form.inputShape = "trace"; form.traceIndex = " 18446744073709551615 ";
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) form.importFields[key] = "literal:東京";
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) form.importFields[key] = "false";
    form.importFields.xes_variant = "chunk_regex";
    const fields = Object.fromEntries(processTreeAlignmentFields(form, "original.tsv"));
    expect(fields).toMatchObject({pt_alignment_variant: form.variant, model_format: "tree_json", discovery_tree_variant: "transition_bordered", trace_index: form.traceIndex, delimiter: "\t"});
    for (const [key] of [...TOKEN_REPLAY_IMPORT_CONTROLS, ...TOKEN_REPLAY_XES_BOOLEAN_CONTROLS]) expect(fields[key]).toBe(form.importFields[key]);
    form.inputFormat = "trace_words"; expect(() => processTreeAlignmentFields(form, "words.json")).toThrow(/trace-word/);
    form.inputShape = "words"; expect(Object.fromEntries(processTreeAlignmentFields(form, "words.json"))).toMatchObject({input_shape: "words", input_format: "trace_words"});
  });
  it("accepts every variant and convenience input shape with the native log result family", () => {
    for (const variant of TREE_ALIGNMENT_VARIANTS) for (const shape of ["log", "trace", "words"] as const) {
      const value = workbench(variant, shape); expect(isProcessTreeAlignmentWorkbench(value)).toBe(true); expect(processTreeAlignmentSucceeded(value)).toBe(true);
      expect(isProcessTreeAlignmentWorkbench({...value, result_shape: "trace"})).toBe(false);
    }
    const value = workbench("MILP"); expect(value.result!.diagnostics.lp_solved).toBe(0); expect(processTreeAlignmentSummary(value)).toContain("MILP");
    expect(processTreeAlignmentReport(saved(value))).toContain("dependency-free residual A*");
  });
  it("keeps arbitrary native tree shapes, duplicate/unused identities, latent indices and unknown raw enum values", () => {
    const value = parseNativeJson('{"activities":["A","A","unused"],"root":{"op":"UNKNOWN","op_index":99,"activity":18446744073709551615,"children":[{"op":"ACTIVITY","op_index":0,"activity":18446744073709551615,"children":[]}]}}');
    expect(isAlignmentNativeTree(value)).toBe(true); if (!isAlignmentNativeTree(value)) throw new Error("Invalid native tree");
    expect(nativeCell(value.root, "activity")).toBe("18446744073709551615"); expect(processTreeAlignmentTreeGraph(value).nodes[1].detail).toContain("18446744073709551615");
    expect(isAlignmentNativeTree({...value, root: {...value.root, op_index: 0}})).toBe(false);
    let root = tree().root; for (let i = 0; i < 350; ++i) root = {op: "SEQUENCE", op_index: 2, activity: i, children: [root]};
    expect(isAlignmentNativeTree({activities: ["A"], root})).toBe(true); expect(processTreeAlignmentTreeGraph({activities: ["A"], root}).nodes).toHaveLength(351);
  });
  it("retains every stored field of uncomputed, incomplete, timed-out and exhausted native results", () => {
    const value = workbench(); value.result!.computed = false; value.result!.completed = false; value.result!.timed_out = true; value.result!.exhausted = true;
    value.result!.fitness = "NaN"; value.result!.traces[0].fitness = "-INF"; value.result!.traces[0].completed = false; value.result!.traces[0].timed_out = true;
    expect(isProcessTreeAlignmentWorkbench(value)).toBe(true); expect(processTreeAlignmentSucceeded(value)).toBe(false); expect(processTreeAlignmentSummary(value)).toContain("Incomplete");
    expect(processTreeAlignmentReport(saved(value))).toContain('"fitness": "-INF"');
    expect(isNativeProcessTreeAlignment({...value.result, fitness: null})).toBe(false); expect(isNativeProcessTreeAlignment({...value.result, total_cost: -1})).toBe(false);
  });
  it("preserves empty unused move-side strings, literal markers and move type indices without normalization", () => {
    const value = nativeResult(); value.traces[0].moves = [
      {type: "log", type_index: 1, log_activity: ">>", model_activity: "", activity: ">>"},
      {type: "model", type_index: 2, log_activity: "", model_activity: "", activity: ""},
      {type: "UNKNOWN", type_index: -7, log_activity: "\u0000東京", model_activity: "\ud800", activity: ""},
    ];
    expect(isNativeProcessTreeAlignment(value)).toBe(true);
    expect(isNativeProcessTreeAlignment({...value, traces: [{...value.traces[0], moves: [{...value.traces[0].moves[0], model_activity: null}]}]})).toBe(false);
    expect(isNativeProcessTreeAlignment({...value, traces: [{...value.traces[0], moves: [{...value.traces[0].moves[0], type_index: 0}]}]})).toBe(false);
    const envelope = workbench(); envelope.result = value; const moves = processTreeAlignmentTables(envelope).find(table => table.id === "complete_alignment_moves")!;
    expect(moves.rows[0][5]).toBe(""); expect(moves.rows[1][4]).toBe(""); expect(nativeJson(moves.records![2])).toContain('"type_index":-7');
  });
  it("retains typed NA/index metadata, unknown counts, original tree and model exports on native errors", () => {
    const value = workbench(); value.executed = false; value.result = null; value.error = {type: "native_error", message: "Unknown process-tree operator"};
    value.model_context.native_tree.root.op = "UNKNOWN"; value.model_context.native_tree.root.op_index = 90;
    value.input_context.input_case_count = value.input_context.checked_event_count = value.input_context.checked_case_count = null;
    value.input_before = {columns: ["NA"], rows: [[{key: "NA", kind: "na", value: "<NA>", typed_value: "NA"}]], dtypes: ["object"], index: {class: "Index", dtype: "object", names: [null], labels: [{kind: "scalar", value: {key: "", kind: "string", value: "東京"}}]}}; value.input_after = value.input_before;
    expect(isProcessTreeAlignmentWorkbench(value)).toBe(true); expect(processTreeAlignmentSummary(value)).toContain("Alignment failed");
    expect(processTreeAlignmentReport(saved(value))).toContain('"kind": "na"'); expect(processTreeAlignmentTreeGraph(value.model_context.native_tree).nodes[0].label).toBe("UNKNOWN");
    expect(processTreeAlignmentArtifactFile(saved(value), "ptml").name).toMatch(/\.ptml$/);
    expect(isProcessTreeAlignmentWorkbench({...value, result: nativeResult()})).toBe(false);
  });
  it("submits two original files, records their hashes and keeps the request independent of host source state", async () => {
    const input = inputFile(), model = modelFile(), fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(nativeJson(workbench())))); vi.stubGlobal("fetch", fetcher);
    const result = await runProcessTreeAlignmentWorkbench(input, model, defaultProcessTreeAlignmentForm());
    expect(fetcher.mock.calls[0][0]).toBe("/api/align-process-tree-workbench"); const body = fetcher.mock.calls[0][1].body as FormData;
    expect(body.get("input_file")).toBe(input); expect(body.get("model_file")).toBe(model); expect(body.get("source_id")).toBeNull(); expect(body.get("include_source")).toBeNull();
    expect(result.inputs.input_sha256).toMatch(/^[0-9a-f]{64}$/); expect(result.inputs.model_sha256).toMatch(/^[0-9a-f]{64}$/); expect(nativeResponseRequest(result.value)?.fields).toEqual(result.fields);
    await expect(runProcessTreeAlignmentWorkbench(new File([await input.arrayBuffer()], "other.csv"), model, defaultProcessTreeAlignmentForm())).rejects.toThrow(/does not match/);
  });
  it("binds all supplied native options including four millisecond controls to actual seconds snapshots", async () => {
    const form = defaultProcessTreeAlignmentForm(); form.variant = "APPROXIMATED_ORIGINAL";
    Object.assign(form.fields, {pt_alignment_max_states: "18446744073709551615", pt_alignment_max_time_ms: "0X1.8P+10", pt_alignment_max_total_time_ms: "2e3", pt_alignment_max_trace_time_ms: "-0.0", pt_alignment_max_per_trace_time_ms: "0.10000000000000001", pt_alignment_max_trace_length: "0", pt_alignment_max_process_tree_height: "0", pt_alignment_use_global_cache: "true", pt_alignment_clear_cache: "false", pt_alignment_batch_size: "2"});
    const value = workbench(form.variant); Object.assign(value.options_context, {max_time_seconds: 1.536, max_total_time_seconds: 2, max_trace_time_seconds: -0, max_per_trace_time_seconds: Number(form.fields.pt_alignment_max_per_trace_time_ms) / 1000, max_trace_length: 0, max_process_tree_height: 0, use_global_cache: true, clear_cache: false, batch_size: 2});
    const raw = nativeJson(value).replace('"max_states":0', '"max_states":18446744073709551615').replace('"max_trace_time_seconds":0', '"max_trace_time_seconds":-0.0'); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(raw)));
    const result = await runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), form); expect(result.fields).toContainEqual(["pt_alignment_max_per_trace_time_ms", "0.10000000000000001"]); expect(nativeCell(result.value.options_context, "max_states")).toBe("18446744073709551615");
    value.options_context.max_time_seconds = 125; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value)))); await expect(runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), form)).rejects.toThrow(/native options/);
  });
  it("rejects mismatched mode, files, format, conversion, literal selectors and selected original traces", async () => {
    for (const mutate of [
      (value: ProcessTreeAlignmentWorkbench) => {value.model_context.dataset = "other.ptml";},
      (value: ProcessTreeAlignmentWorkbench) => {value.input_context.original_input_bytes++;},
      (value: ProcessTreeAlignmentWorkbench) => {value.model_context.format = "tree_json";},
      (value: ProcessTreeAlignmentWorkbench) => {value.conversion_context.variant = "transition_bordered";},
    ]) {const value = workbench(); mutate(value); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value)))); await expect(runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), defaultProcessTreeAlignmentForm())).rejects.toThrow(/does not match/);}
    const selectors = defaultProcessTreeAlignmentForm(); selectors.fields.pt_alignment_activity_attribute = " literal "; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench())))); await expect(runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), selectors)).rejects.toThrow(/literal selectors/);
    const trace = defaultProcessTreeAlignmentForm(); trace.inputShape = "trace"; trace.traceIndex = "1"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench("SEARCH_GRAPH_PT", "trace"))))); await expect(runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), trace)).rejects.toThrow(/selected original trace/);
    expect(isNativeTreeAlignmentOptions({...workbench().options_context, variant_index: 1})).toBe(false);
  });
  it("returns inspectable 400 envelopes and blocks cancellation or superseded file/request generations", async () => {
    const value = workbench(); value.executed = false; value.result = null; value.error = {type: "native_error", message: "missing activity"}; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value), {status: 400})));
    const result = await runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), defaultProcessTreeAlignmentForm()); expect(result.http_status).toBe(400); expect(result.value.error?.message).toBe("missing activity");
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(nativeJson(workbench())));})); await expect(runProcessTreeAlignmentWorkbench(inputFile(), modelFile(), defaultProcessTreeAlignmentForm(), controller.signal)).rejects.toThrow(/abort/i);
    const gate = new ProcessTreeAlignmentRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false);
    const fresh = gate.issue(); expect(gate.accepts(fresh, controller.signal)).toBe(false);
  });
  it("exports every row/column and selected native record after paging with exact uint64 and floating tokens", () => {
    const value = workbench(); value.result!.total_cost = 1; value.result!.fitness = .1;
    value.result!.traces[0].moves = Array.from({length: 73}, (_, i) => ({type: "log" as const, type_index: 1, log_activity: `東京:${i}`, model_activity: "", activity: "z".repeat(600)}));
    value.input_before = {columns: ["NA"], rows: [[{key: "NA", kind: "na", value: "<NA>"}]]};
    const raw = nativeJson(value).replace('"total_cost":1', '"total_cost":18446744073709551615').replace('"fitness":0.1', '"fitness":0.10000000000000001'); const parsed = parseNativeJson(raw);
    expect(isProcessTreeAlignmentWorkbench(parsed)).toBe(true); if (!isProcessTreeAlignmentWorkbench(parsed)) throw new Error("Invalid native envelope");
    const result = saved(parsed), moves = result.tables.find(table => table.id === "complete_alignment_moves")!, options = result.tables.find(table => table.columns.length > 8)!;
    expect(diagnosticPage(moves.rows, 2)).toHaveLength(23); expect(diagnosticPage(options.columns, 1, 8)).not.toHaveLength(0);
    expect(diagnosticCsv(moves)).toContain("東京:72"); expect(nativeJson(moves.records![72])).toContain("z".repeat(600)); expect(processTreeAlignmentAllTablesCsv(result.tables)).toContain("18446744073709551615"); expect(processTreeAlignmentReport(result)).toContain("0.10000000000000001");
    const graph = processTreeAlignmentTreeGraph(tree()); expect(graph.nodes[0].id).toBe("root"); expect(graph.nodes[0].label).toBe("A");
  });
  it("preserves same-run model/source WTF-8 bytes, explicit export errors and safe literal-key filenames", async () => {
    const value = workbench(), text = "<model>ä 東京 🧪 \ud800 x\udc00</model>"; value.ptml = value.pnml = value.bpmn = value.source_xes = text;
    const result = saved(value);
    for (const format of ["ptml", "pnml", "bpmn", "source_xes"] as const) expect(new Uint8Array(await processTreeAlignmentArtifactFile(result, format).arrayBuffer())).toEqual(workflowInputNativeBytes(text));
    value.pnml = null; value.pnml_error = "Native original-tree conversion unavailable"; expect(() => processTreeAlignmentArtifactFile(result, "pnml")).toThrow(value.pnml_error);
    const id = "東京/\ud800/\udc00/%\u0000", encoded = processTreeAlignmentTableFileName(id); expect(encoded).toMatch(/^[\x20-\x7e]+$/); expect(encoded.replace(/%([0-9a-f]{4})/g, (_, code: string) => String.fromCharCode(parseInt(code, 16)))).toBe(id);
  });
});
