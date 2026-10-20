import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {ModelConversionWorkspace} from "../src/components/ModelConversionWorkspace";
import {TreeReductionInspection, TreeReductionWorkspace} from "../src/components/TreeReductionWorkspace";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {originalEmbeddingFilterStringBytes} from "../src/original-embedding-filter-workbench";
import {MODEL_CONVERSION_OPERATIONS, defaultModelConversionForm, modelConversionFields} from "../src/model-conversion-workbench";
import {TREE_REDUCTION_MODEL_EXAMPLE, TREE_REDUCTION_TRACE_EXAMPLE, TreeReductionRequestGate, defaultTreeReductionForm, isReductionTrace, isReductionTree, isTreeReductionWorkbench, runTreeReductionWorkbench, treeReductionAllTablesCsv, treeReductionArtifacts, treeReductionFields, treeReductionGraph, treeReductionOptions, treeReductionReport, treeReductionTables, type ReductionTree, type TreeReductionForm, type TreeReductionWorkbench} from "../src/tree-reduction-workbench";

afterEach(() => vi.unstubAllGlobals());
const hex = (data: string) => ({native_bytes: "hex-v1" as const, data});
const sourceTree = () => parseNativeJson(TREE_REDUCTION_MODEL_EXAMPLE) as ReductionTree;
const sourceTrace = () => parseNativeJson(TREE_REDUCTION_TRACE_EXAMPLE) as Record<string, unknown>;
const files = () => ({model: new File([TREE_REDUCTION_MODEL_EXAMPLE], "original-tree.json", {lastModified: 7}), trace: new File([TREE_REDUCTION_TRACE_EXAMPLE], "original-trace.json", {lastModified: 9})});
const attr = (typed: string) => parseNativeJson('{"key":{"native_bytes":"hex-v1","data":"ff00"},"value":"inactive\\u0000","first_class_source_key":{"native_bytes":"hex-v1","data":"eda080"},"kind_index":-7,"container_index":123,"first_class_role_index":-1,"first_class_state_index":2147483647,"null":false,"scalar_wrapper":true,"scalar_children_list":false,"children":[],"typed_value":'+typed+'}');
function envelope(): TreeReductionWorkbench {
  const tree = sourceTree(), trace = sourceTrace();
  const exports: Record<string, string | null> = {}; for (const key of ["source_pnml", "source_ptml", "source_powl", "result_pnml", "result_ptml", "result_powl"]) {exports[key] = key.endsWith("ptml") ? "<ptml/>" : null; exports[key + "_error"] = null;}
  const artifacts: Record<string, string | null> = {}; for (const key of ["source_tree_json", "result_tree_json", "source_trace_json", "legacy_source_tree_json", "legacy_result_tree_json"]) {artifacts[key] = key.includes("trace") ? nativeJson(trace) : nativeJson(tree); artifacts[key + "_error"] = null;}
  return {family: "MODEL_CONVERSION_WORKBENCH", operation: "reduce_tree", executed: true, native_entered: true, native_returned: true, error: null, model: {process_tree: tree, original_model: TREE_REDUCTION_MODEL_EXAMPLE}, trace: {native_trace: trace, original_trace: TREE_REDUCTION_TRACE_EXAMPLE}, result: {process_tree: tree}, options: {variant: "TREE_TR_BASED", variant_index: 0, activity_key: ""}, input_context: {format: "auto", detected_format: "tree_json", dataset: "original-tree.json", original_model_bytes: nativeStringBytes(TREE_REDUCTION_MODEL_EXAMPLE).length, model_unchanged: true}, trace_context: {format: "native_trace", dataset: "original-trace.json", original_trace_bytes: nativeStringBytes(TREE_REDUCTION_TRACE_EXAMPLE).length, event_count: 2, trace_unchanged: true}, snapshots: {model_before: tree, model_after: tree, trace_before: trace, trace_after: trace}, original_inputs: [], query_inputs: [], input_order: "httplib_key_order", exports, artifacts};
}
/** Transport double only: fixed native snapshot, actual submitted File/byte-part provenance. */
function reply(value: TreeReductionWorkbench, status = 200, change?: (body: FormData, value: TreeReductionWorkbench) => void) {
  const fetch = vi.fn(async (_path: string, init?: RequestInit) => {
    const body = init?.body as FormData;
    value.original_inputs = await Promise.all([...body].map(async ([name, entry]) => ({name, filename: typeof entry === "string" ? "" : entry.name, file: typeof entry !== "string", bytes: typeof entry === "string" ? entry : hex(Array.from(new Uint8Array(await entry.arrayBuffer()), byte => byte.toString(16).padStart(2, "0")).join(""))})));
    change?.(body, value);
    return {ok: status < 400, status, text: async () => nativeJson(value)};
  });
  vi.stubGlobal("fetch", fetch); return fetch;
}
describe("independent native tree reduction", () => {
  it("keeps duplicate and unused byte labels and latent maximum size_t activity indexes", () => {
    const tree = parseNativeJson('{"activities":["",{"native_bytes":"hex-v1","data":"ff00eda080"},""],"root":{"op":"TAU","op_index":1,"activity":18446744073709551615,"children":[]}}');
    expect(isReductionTree(tree)).toBe(true); expect(nativeJson(tree)).toContain('18446744073709551615');
    const typed = tree as ReductionTree; expect(typed.activities).toHaveLength(3); expect(treeReductionGraph(typed).nodes[0].detail).toContain("18446744073709551615");
  });
  it("preserves unknown signed operator indexes for a native failure snapshot", () => {
    expect(isReductionTree(parseNativeJson('{"activities":[],"root":{"op":"UNKNOWN","op_index":-2147483648,"activity":9007199254740992,"children":[]}}'))).toBe(true);
  });
  it("validates every distinct leaf in a 150000-child native tree without a host argument limit", () => {
    const tree: ReductionTree = {activities: [""], root: {op: "SEQUENCE", op_index: 2, activity: 0, children: Array.from({length: 150000}, () => ({op: "ACTIVITY", op_index: 0, activity: 0, children: []}))}};
    expect(isReductionTree(tree)).toBe(true);
    tree.root.children.at(-1)!.activity = -1;
    expect(isReductionTree(tree)).toBe(false);
  });
  it("rejects inconsistent operator names, negative activity indexes and uint64 overflow", () => {
    for (const [op, index, activity] of [["ACTIVITY", "1", "0"], ["TAU", "1", "-1"], ["TAU", "1", "18446744073709551616"], ["UNKNOWN", "2147483648", "0"]]) expect(isReductionTree(parseNativeJson(`{"activities":[],"root":{"op":"${op}","op_index":${index},"activity":${activity},"children":[]}}`))).toBe(false);
  });
  it("validates the complete Trace example with ordered duplicate events", () => {
    const trace = sourceTrace(); expect(isReductionTrace(trace)).toBe(true); expect((trace.events as Array<Record<string, unknown>>).map(event => event.activity)).toEqual(["B", "B"]);
  });
  it("retains nested attributes, inactive payloads, full typed values and exact integer tokens", () => {
    const trace = sourceTrace();
    trace.attributes = ['{"index":0,"value":null}', '{"index":1,"value":-9223372036854775808}', '{"index":2,"value":"NaN"}', '{"index":3,"value":false}', '{"index":4,"value":{"value":{"native_bytes":"hex-v1","data":"ff"},"resolution_index":-9,"timezone":{"native_bytes":"hex-v1","data":"eda080"}}}', '{"index":5,"value":18446744073709551615}', '{"index":6,"value":"NA"}'].map(attr);
    const event = (trace.events as Array<Record<string, unknown>>)[0]; event.activity = hex("ff00eda080"); event.activity_state_index = -2147483648; event.activity_attribute = (trace.attributes as unknown[])[1];
    expect(isReductionTrace(trace)).toBe(true); expect(nativeJson(trace)).toContain("18446744073709551615"); expect(nativeJson(trace)).toContain("-9223372036854775808"); expect(event.activity).toEqual(hex("ff00eda080"));
  });
  it("rejects invalid typed payloads and incomplete native output states", () => {
    const trace = sourceTrace(); trace.properties = [attr('{"index":5,"value":18446744073709551616}')]; expect(isReductionTrace(trace)).toBe(false);
    trace.properties = []; delete (trace.events as Array<Record<string, unknown>>)[0].activity_state_index; expect(isReductionTrace(trace)).toBe(false);
  });
  it("accepts the entire signed variant domain and exact arbitrary key bytes", () => {
    const raw = '{"variant_index":-2147483648,"activity_key":{"native_bytes":"hex-v1","data":"ff00eda080"}}';
    expect(treeReductionOptions(raw)).toEqual({variant_index: -2147483648, activity_key: hex("ff00eda080")});
    expect(treeReductionOptions('{"variant_index":2147483647,"activity_key":""}').activity_key).toBe("");
  });
  it("rejects decoded duplicate keys, unknown fields and invalid complete-option types", () => {
    for (const raw of ['{"variant_index":0,"variant_\\u0069ndex":1,"activity_key":""}', '{"variant_index":0,"activity_key":"","extra":0}', '{"variant_index":0}', '{"variant_index":2147483648,"activity_key":""}', '{"variant_index":1e0,"activity_key":""}', '{"variant_index":true,"activity_key":null}', '{"variant_index":0,"activity_key":{"native_bytes":"hex-v1","data":"f"}}']) expect(() => treeReductionOptions(raw)).toThrow();
  });
  it("distinguishes omitted selectors from an explicit empty literal", () => {
    const form = defaultTreeReductionForm(); expect(treeReductionFields(form)).not.toContainEqual(["tree_reduction_activity_key", ""]);
    form.fields.tree_reduction_activity_key = ""; expect(treeReductionFields(form)).toContainEqual(["tree_reduction_activity_key", ""]);
    form.fields.tree_reduction_activity_key = " raw\r\n\0\ud800 "; expect(treeReductionFields(form).at(-1)).toEqual(["tree_reduction_activity_key", " raw\r\n\0\ud800 "]);
  });
  it("rejects every conflicting carrier by presence, including an empty scalar", () => {
    const file = new File(["{}"], "options.json"), form = defaultTreeReductionForm(); form.fields.tree_reduction_options_json = '{"variant_index":0,"activity_key":""}';
    expect(() => treeReductionFields(form, {options: file})).toThrow();
    for (const key of ["tree_reduction_variant", "tree_reduction_activity_key"]) expect(() => treeReductionFields({...form, fields: {...form.fields, [key]: ""}})).toThrow();
    expect(() => treeReductionFields(form, {activityKey: file})).toThrow(); expect(() => treeReductionFields({ ...defaultTreeReductionForm(), fields: {tree_reduction_activity_key: ""}}, {activityKey: file})).toThrow();
  });
  it("uses the actual two original Files and byte multipart controls in the real request helper", async () => {
    const {model, trace} = files(), value = envelope(), form = defaultTreeReductionForm(), literal = " leading\r\n\0\ud800 trailing "; form.fields.tree_reduction_activity_key = literal; value.options!.activity_key = hex(Array.from(nativeStringBytes(literal), byte => byte.toString(16).padStart(2, "0")).join(""));
    const fetch = reply(value), abort = new AbortController(), run = await runTreeReductionWorkbench(model, trace, form, {}, abort.signal), call = fetch.mock.calls[0];
    expect(call[0]).toBe("/api/model-conversion-workbench"); expect(call[1]?.method).toBe("POST"); expect(call[1]?.signal).toBe(abort.signal);
    const body = call[1]?.body as FormData; expect(body.get("model_file")).toBe(model); expect(body.get("trace_file")).toBe(trace);
    const key = body.get("tree_reduction_activity_key") as File; expect(new Uint8Array(await key.arrayBuffer())).toEqual(nativeStringBytes(literal));
    expect(run.input.model).toBe(model); expect(run.input.trace).toBe(trace); expect(run.input.model_sha256).toMatch(/^[a-f0-9]{64}$/); expect(run.input.trace_sha256).toMatch(/^[a-f0-9]{64}$/); expect(run.raw).toContain('"input_order":"httplib_key_order"');
  });
  it("retains original options File identity and raw invalid/WTF8 bytes without decoding", async () => {
    const {model, trace} = files(), options = new File([new Uint8Array([0xff, 0, 0xed, 0xa0, 0x80])], "", {lastModified: 4}), value = envelope(); value.executed = false; value.native_entered = false; value.native_returned = false; value.error = {type: "native_error", message: "invalid JSON"}; value.options = null; value.result = null;
    const fetch = reply(value, 400), run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm(), {options});
    expect((fetch.mock.calls[0][1]?.body as FormData).get("tree_reduction_options_file")).toBe(options); expect(run.input.options).toBe(options); expect(run.input.options_sha256).toMatch(/^[a-f0-9]{64}$/); expect(run.http_status).toBe(400); expect(run.value.native_entered).toBe(false); expect(run.value.options).toBeNull();
  });
  it("sends complete raw options JSON as byte multipart and keeps unknown-variant native failures", async () => {
    const {model, trace} = files(), value = envelope(), raw = '{\r\n"variant_index":-1,\r\n"activity_key":"\ud800"\r\n}', form = defaultTreeReductionForm(); form.fields.tree_reduction_options_json = raw;
    value.executed = false; value.native_returned = false; value.error = {type: "native_error", message: "unknown variant"}; value.result = null; value.options = {variant: "UNKNOWN", variant_index: -1, activity_key: hex("eda080")};
    const fetch = reply(value, 400), run = await runTreeReductionWorkbench(model, trace, form), part = (fetch.mock.calls[0][1]?.body as FormData).get("tree_reduction_options_json") as File;
    expect(new Uint8Array(await part.arrayBuffer())).toEqual(nativeStringBytes(raw)); expect(run.fields.at(-1)).toEqual(["tree_reduction_options_json", raw]); expect(run.value.options?.variant_index).toBe(-1); expect(run.value.native_entered).toBe(true); expect(run.value.snapshots.model_before).not.toBeNull();
  });
  it("keeps a valid complete options File containing raw WTF8 JSON bytes", async () => {
    const {model, trace} = files(), value = envelope(), raw = '{"variant_index":0,"activity_key":"\ud800"}', options = new File([nativeStringBytes(raw)], "options-wtf8.json"); value.options!.activity_key = hex("eda080");
    const fetch = reply(value), run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm(), {options});
    expect((fetch.mock.calls[0][1]?.body as FormData).get("tree_reduction_options_file")).toBe(options); expect(run.input.options).toBe(options); expect(run.value.options?.activity_key).toEqual(hex("eda080")); expect(new Uint8Array(await run.input.options!.arrayBuffer())).toEqual(nativeStringBytes(raw));
  });
  it("retains an original raw key File and explicit native exception snapshots", async () => {
    const {model, trace} = files(), activityKey = new File([new Uint8Array([0xff, 0])], "raw-key.bin"), value = envelope(); value.executed = false; value.native_entered = true; value.native_returned = false; value.error = {type: "native_error", message: hex("ff00")}; value.options!.activity_key = hex("ff00"); value.result = null;
    const fetch = reply(value, 400), run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm(), {activityKey});
    expect((fetch.mock.calls[0][1]?.body as FormData).get("tree_reduction_activity_key_file")).toBe(activityKey); expect(run.input.activityKey).toBe(activityKey); expect(run.value.snapshots.trace_before).not.toBeNull(); expect(run.value.error?.message).toEqual(hex("ff00"));
  });
  it("preserves both original byte echoes and nullable parsed state on parser failure", async () => {
    const {model, trace} = files(), value = envelope(); value.executed = false; value.native_entered = false; value.native_returned = false; value.error = {type: "native_error", message: "parse failed"}; value.model.process_tree = null; value.trace.native_trace = null; value.options = null; value.result = null; value.input_context.model_unchanged = null; value.trace_context.trace_unchanged = null; value.trace_context.event_count = null; value.snapshots = {model_before: null, model_after: null, trace_before: null, trace_after: null};
    reply(value, 400); const run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm()); expect(run.value.model.original_model).toBe(TREE_REDUCTION_MODEL_EXAMPLE); expect(run.value.trace.original_trace).toBe(TREE_REDUCTION_TRACE_EXAMPLE); expect(run.raw).toContain('"model_before":null');
  });
  it("rejects mismatched original trace bytes despite a valid model echo", async () => {
    const {model, trace} = files(), value = envelope(); value.trace.original_trace = "different"; reply(value); await expect(runTreeReductionWorkbench(model, trace, defaultTreeReductionForm())).rejects.toThrow("both original");
  });
  it("rejects a native options echo inconsistent with the submitted scalar selector", async () => {
    const {model, trace} = files(), value = envelope(), form = defaultTreeReductionForm(); form.fields.tree_reduction_activity_key = "literal"; reply(value); await expect(runTreeReductionWorkbench(model, trace, form)).rejects.toThrow("native options");
  });
  it("rejects missing original options provenance and wrong transport filename flags", async () => {
    const {model, trace} = files(), value = envelope(); reply(value, 200, (_body, result) => {result.original_inputs = result.original_inputs.filter(part => part.name !== "trace_format");}); await expect(runTreeReductionWorkbench(model, trace, defaultTreeReductionForm())).rejects.toThrow("provenance");
    reply(envelope(), 200, (_body, result) => {result.original_inputs.find(part => part.name === "model_file")!.file = false;}); await expect(runTreeReductionWorkbench(model, trace, defaultTreeReductionForm())).rejects.toThrow("provenance");
  });
  it("retains exact raw response integer spellings in reports and complete CSV tables", async () => {
    const {model, trace} = files(), value = envelope(), tree = parseNativeJson('{"activities":["A"],"root":{"op":"TAU","op_index":1,"activity":18446744073709551615,"children":[]}}') as ReductionTree;
    value.model.process_tree = tree; value.snapshots.model_before = tree; reply(value); const run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm());
    expect(run.raw).toContain("18446744073709551615"); expect(treeReductionReport(run)).toContain("18446744073709551615"); expect(treeReductionAllTablesCsv(run.tables)).toContain("18446744073709551615"); expect(run.tables.find(table => table.id === "source_tree_nodes")?.rows[0][4]).toBe("18446744073709551615");
    const report = parseNativeJson(treeReductionReport(run)) as {native_result: TreeReductionWorkbench}; expect(nativeCell(report.native_result.model.process_tree!.root, "activity")).toBe("18446744073709551615");
  });
  it("exports byte-exact safe/legacy canonical tree, Trace and PTML artifacts separately", async () => {
    const {model, trace} = files(), value = envelope(); value.artifacts.legacy_source_tree_json = hex("ff00eda080"); value.exports.source_ptml = hex("ff003c3e"); value.exports.result_ptml = null; value.exports.result_ptml_error = "PTML failed"; reply(value);
    const run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm()), artifacts = treeReductionArtifacts(run);
    expect(artifacts.map(item => item.name)).toEqual(["source_tree_json", "result_tree_json", "source_trace_json", "legacy_source_tree_json", "legacy_result_tree_json"].map(key => `minera-reduce_tree-${key}.json`).concat("minera-reduce_tree-source_ptml.ptml"));
    expect(artifacts[3].bytes).toEqual(new Uint8Array([255, 0, 237, 160, 128])); expect(artifacts[5].bytes).toEqual(new Uint8Array([255, 0, 60, 62])); expect(artifacts[5].mime).toBe("application/xml"); expect(run.value.exports.result_ptml_error).toBe("PTML failed");
  });
  it("renders byte labels as explicit hex in graphs without replacement decoding", () => {
    const tree = sourceTree(); tree.activities[0] = hex("ff00eda080"); const graph = treeReductionGraph(tree); expect(graph.nodes.some(node => node.label === "hex:ff00eda080")).toBe(true); expect(graph.edges.map(edge => edge.label)).toContain("child 1");
    expect(originalEmbeddingFilterStringBytes(tree.activities[0])).toEqual(new Uint8Array([255, 0, 237, 160, 128]));
  });
  it("binds request tickets to both File identities, all option revisions and aborts", () => {
    const {model, trace} = files(), gate = new TreeReductionRequestGate(), form = defaultTreeReductionForm(), options = new File(["{}"], "options.json"), uploads = {options}, ticket = gate.issue(model, trace, form, uploads);
    expect(gate.accepts(ticket, model, trace, form, uploads)).toBe(true); expect(gate.accepts(ticket, new File([TREE_REDUCTION_MODEL_EXAMPLE], model.name), trace, form, uploads)).toBe(false); expect(gate.accepts(ticket, model, new File([TREE_REDUCTION_TRACE_EXAMPLE], trace.name), form, uploads)).toBe(false);
    expect(gate.accepts(ticket, model, trace, {...form, fields: {tree_reduction_activity_key: ""}}, uploads)).toBe(false); expect(gate.accepts(ticket, model, trace, form, {options: new File(["{}"], "options.json")})).toBe(false);
    const abort = new AbortController(); abort.abort(); expect(gate.accepts(ticket, model, trace, form, uploads, abort.signal)).toBe(false); gate.invalidate(); expect(gate.accepts(ticket, model, trace, form, uploads)).toBe(false);
  });
  it("does not let an older request replace a newer tree/trace pair", () => {
    const {model, trace} = files(), gate = new TreeReductionRequestGate(), form = defaultTreeReductionForm(), old = gate.issue(model, trace, form), next = gate.issue(model, trace, form); expect(gate.accepts(old, model, trace, form)).toBe(false); expect(gate.accepts(next, model, trace, form)).toBe(true);
  });
  it("mounts existing model navigation and full native Trace/File controls without a separate tool", () => {
    const parent = renderToStaticMarkup(createElement(ModelConversionWorkspace)), workspace = renderToStaticMarkup(createElement(TreeReductionWorkspace, {active: true}));
    expect(parent).toContain('value="reduce_tree"'); expect(parent).toContain('value="reduce_petri"'); expect(parent).toContain('value="tree_to_powl"'); expect(workspace).toContain("Original process-tree File"); expect(workspace).toContain("Original trace File"); expect(workspace).toContain("Full native Trace constructor"); expect(workspace).toContain("Complete original options File"); expect(workspace).toContain("case_id_state_index"); expect(workspace).toContain("Original raw activity-key File");
  });
  it("mounts a complete actual-helper run with both original downloads and every result export", async () => {
    const {model, trace} = files(), value = envelope(); reply(value); const run = await runTreeReductionWorkbench(model, trace, defaultTreeReductionForm()), html = renderToStaticMarkup(createElement(TreeReductionInspection, {run}));
    for (const text of ["original-tree.json", "original-trace.json", "Download original model File", "Download original trace File", "Download complete native tree reduction JSON", "Download tree reduction report", "Download every tree reduction table CSV", "source_trace_json.json", "legacy_source_tree_json.json", "result_ptml.ptml", "Reduced process tree", "HTTP 200"]) expect(html).toContain(text);
  });
  it("mounts failed snapshots separately while preserving original and artifact-error evidence", () => {
    const value = envelope(); value.executed = false; value.native_returned = false; value.result = null; value.error = {type: "native_error", message: "unknown variant"}; value.artifacts.legacy_source_tree_json = null; value.artifacts.legacy_source_tree_json_error = hex("ff00");
    expect(isTreeReductionWorkbench(value)).toBe(true); const {model, trace} = files(), html = renderToStaticMarkup(createElement(TreeReductionInspection, {failed: true, run: {value, raw: nativeJson(value), fields: [], http_status: 400, tables: treeReductionTables(value), input: {model, trace, model_sha256: "model-hash", trace_sha256: "trace-hash"}}}));
    expect(html).toContain("Failed original tree reduction inspection"); expect(html).toContain("unknown variant"); expect(html).toContain("hex:ff00"); expect(html).toContain("Original process tree before reduction"); expect(html).not.toContain("Reduced process tree"); expect(html).toContain("model-hash"); expect(html).toContain("trace-hash");
  });
  it("keeps the existing six operation fields and validators independent", () => {
    expect(MODEL_CONVERSION_OPERATIONS).toEqual(["tree_to_powl", "net_to_powl", "powl_to_tree", "net_to_tree", "powl_to_net", "reduce_petri"]);
    for (const operation of MODEL_CONVERSION_OPERATIONS) expect(modelConversionFields(defaultModelConversionForm(operation))[0]).toEqual(["operation", operation]);
    const form: TreeReductionForm = defaultTreeReductionForm(); form.fields.tree_reduction_variant = "tree_tr_based"; expect(treeReductionFields(form)).toContainEqual(["tree_reduction_variant", "tree_tr_based"]);
  });
});
