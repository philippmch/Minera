import {afterEach, describe, expect, it, vi} from "vitest";
import {
  INDUCTIVE_INPUT_CONTROLS, INDUCTIVE_INPUT_CONVERSIONS, INDUCTIVE_INPUT_FORMATS, INDUCTIVE_INPUT_SHAPES, INDUCTIVE_INPUT_VARIANTS,
  INDUCTIVE_NATIVE_DFG_EXAMPLE, INDUCTIVE_WEIGHTED_VARIANTS_EXAMPLE, InductiveInputRequestGate, defaultInductiveInputForm,
  inductiveInputAllTablesCsv, inductiveInputArtifactFile, inductiveInputControlFields, inductiveInputFields,
  inductiveInputReport, inductiveInputSummary, inductiveInputTableFileName, inductiveInputTables, inductiveInputTreeGraph,
  isInductiveInputDiscovery, isInductiveNativeDfg, isInductiveNativeTree, isInductiveWeightedVariants, runInductiveInput,
  type InductiveInputDiscovery, type InductiveInputRun, type InductiveNativeTree, type InductiveNativeTreeNode,
} from "../src/inductive-input-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseRequest} from "../src/native-response";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../src/token-replay-workbench";
import {WORKFLOW_INPUT_XES_VARIANTS} from "../src/workflow-input-workbench";

const inputFile = () => new File(["case_id,activity,timestamp\n1,A,2024-01-01T00:00:00Z\n"], "original.csv", {lastModified: 0});
const tree = (): InductiveNativeTree => ({activities: ["A", "", "unused 東京"], root: {op: "SEQUENCE", op_index: 2, activity: 7, children: [{op: "ACTIVITY", op_index: 0, activity: 0, children: []}, {op: "TAU", op_index: 1, activity: 19, children: []}]}});
function discovery(file = inputFile()): InductiveInputDiscovery {
  return {family: "INDUCTIVE_INPUT_DISCOVERY", variant: "IM", computed: true, error: null,
    input_context: {adapter: "log", format: "auto", detected_format: "csv", dataset: file.name, origin: "uploaded_csv_rows", original_input_bytes: file.size, input_event_count: 1, input_case_count: 1, weighted_variant_count: null, activity_count: null},
    options_context: {noise_threshold: 0, variant: "IM", variant_index: 0, activity_key: "concept:name", timestamp_key: "time:timestamp", case_id_key: "case:concept:name", sort_events_by_timestamp: false, disable_strict_sequence_cut: false, multiprocessing: false, disable_fallthroughs: false},
    input_before: {columns: ["activity"], rows: [[{key: "activity", kind: "string", value: "A"}]]}, input_after: null, input_log: {log: {traces: [{events: [{activity: "A"}]}]}}, input_log_error: null,
    dfg_input: null, variant_log_input: null, native_dfg: {activities: ["A"], edges: [], start_counts: [1], end_counts: [1], activity_counts: [1], rework_case_counts: [0], variants: [{activities: ["A"], count: 1}]}, native_tree: tree(), native_net: null,
    discovery_context: {requested_variant: "IM", effective_variant: "IM", direct_graph_semantics: false, prepared_event_count: 1, prepared_case_count: 1}, prepared_log: {log: {traces: [{events: [{activity: "A"}]}]}}, prepared_log_error: null,
    source_xes: "<log></log>", source_xes_error: null, conversion_context: {variant: "standard", computed: false, error: "Conversion unavailable"},
    pnml: null, pnml_error: "PNML unavailable", ptml: "<ptml></ptml>", ptml_error: null, bpmn: "<definitions></definitions>", bpmn_error: null, result: null, result_error: "Supplemental rendering unavailable"};
}
function saved(value: InductiveInputDiscovery): InductiveInputRun {
  const input = inputFile(), form = defaultInductiveInputForm(); form.shape = value.input_context.adapter; form.format = value.input_context.format; form.variant = value.variant === "IMD" ? "dfg" : value.variant === "IMF" ? "imf" : "log";
  const fields = inductiveInputFields(form, input.name); return {value, raw: nativeJson(value), fields, tables: inductiveInputTables(value, fields), input, input_sha256: "input hash", http_status: value.computed ? 200 : 400};
}
afterEach(() => vi.unstubAllGlobals());

describe("independent Inductive input controls", () => {
  it("exposes every native variant, input adapter, scalar selector/flag and conversion", () => {
    expect(INDUCTIVE_INPUT_VARIANTS).toEqual(["log", "imf", "dfg"]); expect(INDUCTIVE_INPUT_SHAPES).toEqual(["log", "stream", "dataframe", "lazyframe", "dfg", "variants"]);
    expect(INDUCTIVE_INPUT_FORMATS).toEqual(["auto", "table", "dfg_json", "variants_json"]); expect(INDUCTIVE_INPUT_CONVERSIONS).toEqual(["standard", "transition_bordered"]);
    expect(INDUCTIVE_INPUT_CONTROLS.map(control => control.key)).toEqual(["noise_threshold", "inductive_activity_key", "inductive_case_id_key", "inductive_timestamp_key", "inductive_sort_events_by_timestamp", "inductive_disable_strict_sequence_cut", "inductive_multiprocessing", "inductive_disable_fallthroughs"]);
  });
  it("preserves exact noise/selector spellings and distinguishes omitted from explicit-empty selectors", () => {
    const values: Record<string, string> = {};
    for (const control of INDUCTIVE_INPUT_CONTROLS) values[control.key] = control.kind === "selector" ? " \t東京,;\u0000 \ud800 " : control.kind === "number" ? " 0.10000000000000000001 " : "false";
    const controls = inductiveInputControlFields(values); for (const control of INDUCTIVE_INPUT_CONTROLS) expect(control.kind === "selector" ? controls.selectors[control.key] : Object.fromEntries(controls.fields)[control.key]).toBe(values[control.key]);
    expect(inductiveInputControlFields({}).fields).toEqual([]); expect(Object.keys(inductiveInputControlFields({}).selectors)).toEqual([]);
    expect(inductiveInputControlFields({}, {inductive_activity_key: true}).selectors.inductive_activity_key).toBe("");
    const form = defaultInductiveInputForm(); form.fields = values;
    const fields = Object.fromEntries(inductiveInputFields(form, "input.csv")); expect(fields.noise_threshold).toBe(values.noise_threshold); expect(JSON.parse(fields.discovery_selectors_json)).toEqual(controls.selectors); expect(fields.inductive_activity_key).toBeUndefined();
    expect(() => inductiveInputControlFields({noise_threshold: "1.1"})).toThrow(/\[0, 1\]/); expect(() => inductiveInputControlFields({noise_threshold: "NaN"})).toThrow(/finite/);
  });
  it("preserves every CSV/XES import field while keeping all four adapters independent of cached sources", () => {
    for (const shape of ["log", "stream", "dataframe", "lazyframe"] as const) {
      const form = defaultInductiveInputForm(); form.shape = shape;
      for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) form.importFields[key] = " literal:東京 "; for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) form.importFields[key] = "false";
      form.importFields.xes_variant = WORKFLOW_INPUT_XES_VARIANTS[5];
      const fields = Object.fromEntries(inductiveInputFields(form, "input.tsv")); expect(fields.input_shape).toBe(shape); expect(fields.delimiter).toBe("\t");
      for (const [key] of [...TOKEN_REPLAY_IMPORT_CONTROLS, ...TOKEN_REPLAY_XES_BOOLEAN_CONTROLS]) expect(fields[key]).toBe(form.importFields[key]); expect(fields.source_id).toBeUndefined(); expect(fields.include_source).toBeUndefined();
    }
  });
  it("requires matching direct-input formats without rewriting the requested variant or noise", () => {
    const form = defaultInductiveInputForm(); form.shape = "dfg"; expect(() => inductiveInputFields(form, "graph.json")).toThrow(/matching/);
    form.format = "dfg_json"; form.variant = "imf"; form.fields.noise_threshold = "0.75";
    expect(Object.fromEntries(inductiveInputFields(form, "graph.json"))).toMatchObject({inductive_variant: "imf", noise_threshold: "0.75", input_shape: "dfg"});
    form.shape = "variants"; expect(() => inductiveInputFields(form, "variants.json")).toThrow(/matching/); form.format = "variants_json";
    expect(Object.fromEntries(inductiveInputFields(form, "variants.json"))).toMatchObject({input_shape: "variants", input_format: "variants_json"});
  });
});

describe("complete native Inductive carriers", () => {
  it("accepts weighted zero/MAX counts, repeated records and literal empty/Unicode labels without expansion", () => {
    const raw = '[{"activities":["","A","A","東京,;\\n"],"count":18446744073709551615},{"activities":[],"count":0},{"activities":["A"],"count":1},{"activities":["A"],"count":1}]';
    const variants = parseNativeJson(raw); expect(isInductiveWeightedVariants(variants)).toBe(true); if (!isInductiveWeightedVariants(variants)) throw new Error("Invalid weighted fixture");
    expect(variants).toHaveLength(4); expect(nativeCell(variants[0], "count")).toBe("18446744073709551615"); expect(nativeJson(variants)).toContain("18446744073709551615");
    expect(isInductiveWeightedVariants([{activities: ["A"], count: -1}])).toBe(false); expect(isInductiveWeightedVariants([{activities: [null], count: 1}])).toBe(false);
    expect(isInductiveWeightedVariants(parseNativeJson(INDUCTIVE_WEIGHTED_VARIANTS_EXAMPLE))).toBe(true);
  });
  it("retains full native DFG registry/edge order and zero topology while rejecting invalid constructor identities", () => {
    const graph = parseNativeJson(INDUCTIVE_NATIVE_DFG_EXAMPLE); expect(isInductiveNativeDfg(graph)).toBe(true); if (!isInductiveNativeDfg(graph)) throw new Error("Invalid graph fixture");
    expect(graph.activities).toEqual(["B", "A", "isolated 東京"]); expect(graph.edges[1].count).toBe(0);
    expect(isInductiveNativeDfg({...graph, edges: [...graph.edges, graph.edges[0]]})).toBe(false); expect(isInductiveNativeDfg({...graph, activities: ["A", "A", "B"]})).toBe(false);
    expect(isInductiveNativeDfg({...graph, edges: [{source: 3, target: 0, count: 1}]})).toBe(false); expect(isInductiveNativeDfg({...graph, start_counts: [1]})).toBe(false);
    expect(isInductiveNativeDfg({...graph, activity_counts: [0, 0, 0], rework_case_counts: [0, 0, 0], variants: []})).toBe(true);
  });
  it("validates and exports complete trees including unused registry/latent node activity values and unknown operators", () => {
    const value = tree(); value.root.children[1].activity = 1; value.root.children.push({op: "UNKNOWN", op_index: 99, activity: 0, children: []});
    const raw = nativeJson(value).replace('"op":"TAU","op_index":1,"activity":1', '"op":"TAU","op_index":1,"activity":18446744073709551615');
    const parsed = parseNativeJson(raw); expect(isInductiveNativeTree(parsed)).toBe(true); if (!isInductiveNativeTree(parsed)) throw new Error("Invalid tree fixture");
    const result = discovery(); result.native_tree = parsed; const run = saved(result), nodes = run.tables.find(table => table.id === "complete_tree_nodes")!;
    expect(nodes.rows).toHaveLength(4); expect(nodes.rows[2][4]).toBe("18446744073709551615"); expect(nativeJson(nodes.records![2])).toContain("18446744073709551615");
    expect(inductiveInputAllTablesCsv(run.tables)).toContain("unused 東京"); expect(inductiveInputReport(run)).toContain("18446744073709551615");
    const graph = inductiveInputTreeGraph(parsed); expect(graph.nodes.map(node => node.id)).toEqual(["root", "root/0", "root/1", "root/2"]); expect(graph.nodes[2].detail).toContain("18446744073709551615"); expect(graph.nodes[3].label).toBe("UNKNOWN");
    expect(isInductiveNativeTree({...parsed, root: {...parsed.root, op: "other"}})).toBe(false); expect(isInductiveNativeTree({activities: [], root: {op: "ACTIVITY", op_index: 0, activity: 0, children: []}})).toBe(false);
  });
  it("keeps complete long/deep trees beyond visual paging and all actual DFG observables", () => {
    const value = discovery(); let node: InductiveNativeTreeNode = {op: "TAU", op_index: 1, activity: 999, children: []};
    for (let i = 0; i < 70; ++i) node = {op: "SEQUENCE", op_index: 2, activity: i, children: [node]}; value.native_tree = {activities: ["unused"], root: node};
    expect(isInductiveInputDiscovery(value)).toBe(true); const run = saved(value), nodes = run.tables.find(table => table.id === "complete_tree_nodes")!;
    expect(nodes.rows).toHaveLength(71); expect(diagnosticPage(nodes.rows, 2)).toHaveLength(21); expect(diagnosticCsv(nodes)).toContain("999"); expect(inductiveInputTreeGraph(value.native_tree).nodes).toHaveLength(71);
    expect(inductiveInputAllTablesCsv(run.tables)).toContain("rework_case_counts"); expect(inductiveInputReport(run)).toContain("prepared_log");
  });
  it("accepts native-only success and conversion failures without invented compact input counts or legacy renderers", () => {
    const value = discovery(); value.input_context.adapter = "variants"; value.input_context.format = "variants_json";
    value.input_context.input_case_count = value.input_context.input_event_count = null; value.input_context.weighted_variant_count = 2;
    value.variant_log_input = [{activities: ["A"], count: 1}, {activities: [], count: 0}]; value.native_dfg = null; value.input_before = value.input_after = null; value.prepared_log = null; value.source_xes = null;
    value.result = null; value.result_error = "Legacy event-log analysis is unavailable for compact variants";
    expect(isInductiveInputDiscovery(value)).toBe(true); expect(inductiveInputSummary(value)).toContain("Compact weighted"); expect(inductiveInputTreeGraph(value.native_tree!).nodes).toHaveLength(3);
    const direct = discovery(), declared = parseNativeJson(INDUCTIVE_NATIVE_DFG_EXAMPLE); if (!isInductiveNativeDfg(declared)) throw new Error("Invalid declared DFG fixture");
    direct.input_context.adapter = "dfg"; direct.input_context.format = "dfg_json"; direct.dfg_input = declared; direct.native_dfg = {...declared, activity_counts: [0, 0, 0], rework_case_counts: [0, 0, 0], variants: []};
    direct.discovery_context.direct_graph_semantics = true; direct.discovery_context.effective_variant = "IMD"; direct.options_context.noise_threshold = 0.75;
    expect(isInductiveInputDiscovery(direct)).toBe(true); expect(direct.options_context.variant).toBe("IM"); expect(inductiveInputSummary(direct)).toContain("IM requested · IMD effective");
  });
  it("retains native failures with full typed NA/index metadata and unknown prepared counts", () => {
    const value = discovery(); value.computed = false; value.error = {type: "native_error", message: "Missing selected timestamp"}; value.native_tree = null;
    value.input_before = {columns: ["NA"], rows: [[{key: "NA", kind: "na", value: "<NA>", typed_value: "NA"}]], dtypes: ["object"], index: {class: "Index", dtype: "object", names: [null], labels: [{kind: "scalar", value: {key: "index", kind: "string", value: "literal"}}]}};
    value.discovery_context.prepared_case_count = value.discovery_context.prepared_event_count = null;
    expect(isInductiveInputDiscovery(value)).toBe(true); expect(inductiveInputReport(saved(value))).toContain("Missing selected timestamp"); expect(inductiveInputSummary(value)).toContain("failed");
    expect(isInductiveInputDiscovery({...value, error: null})).toBe(false); expect(isInductiveInputDiscovery({...discovery(), native_tree: null})).toBe(false);
  });
});

describe("Inductive request binding and exact downloads", () => {
  it("sends the original file and literal options to the independent endpoint with no host source fields", async () => {
    const file = inputFile(), value = discovery(file), form = defaultInductiveInputForm(); form.fields.inductive_activity_key = " literal 東京 "; value.options_context.activity_key = form.fields.inductive_activity_key;
    const fetcher = vi.fn().mockResolvedValue(new Response(nativeJson(value))); vi.stubGlobal("fetch", fetcher);
    const run = await runInductiveInput(file, form); expect(fetcher.mock.calls[0][0]).toBe("/api/discover-inductive-input"); expect(fetcher.mock.calls[0][1].body.get("input")).toBe(file); expect(fetcher.mock.calls[0][1].body.get("source_id")).toBeNull(); expect(run.input_sha256).toMatch(/^[0-9a-f]{64}$/);
    expect(nativeResponseRequest(run.value)?.fields).toEqual(run.fields); expect(inductiveInputReport(run)).toContain(run.input_sha256);
  });
  it("retains structured 400 failures and rejects mismatched file, adapter, variant, conversion or literal selectors", async () => {
    const value = discovery(); value.computed = false; value.error = {type: "native_error", message: "Empty activity selector"}; value.native_tree = null;
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value), {status: 400}))); expect((await runInductiveInput(inputFile(), defaultInductiveInputForm())).http_status).toBe(400);
    for (const change of [(result: InductiveInputDiscovery) => {result.input_context.dataset = "other.csv";}, (result: InductiveInputDiscovery) => {result.input_context.adapter = "stream";}, (result: InductiveInputDiscovery) => {result.variant = "IMF";}, (result: InductiveInputDiscovery) => {result.conversion_context.variant = "transition_bordered";}]) {
      const result = discovery(); change(result); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(result)))); await expect(runInductiveInput(inputFile(), defaultInductiveInputForm())).rejects.toThrow(/does not match/);
    }
    const form = defaultInductiveInputForm(); form.fields.inductive_activity_key = "literal"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(discovery())))); await expect(runInductiveInput(inputFile(), form)).rejects.toThrow(/literal selectors/);
  });
  it("rejects cancelled and superseded requests after asynchronous response decoding", async () => {
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(nativeJson(discovery())));}));
    await expect(runInductiveInput(inputFile(), defaultInductiveInputForm(), controller.signal)).rejects.toThrow(/abort/i);
    const gate = new InductiveInputRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false);
  });
  it("exports same-tree model/source bytes exactly, including lone UTF-16 surrogates", async () => {
    const value = discovery(); value.ptml = "<ptml>\ud800|\udc00|🧪</ptml>"; const run = saved(value), file = inductiveInputArtifactFile(run, "ptml");
    const prefix = [...new TextEncoder().encode("<ptml>")], suffix = [...new TextEncoder().encode("</ptml>")];
    expect(new Uint8Array(await file.arrayBuffer())).toEqual(new Uint8Array([...prefix, 0xed, 0xa0, 0x80, 124, 0xed, 0xb0, 0x80, 124, 0xf0, 0x9f, 0xa7, 0xaa, ...suffix]));
    expect(inductiveInputArtifactFile(run, "source_xes").name).toMatch(/\.xes$/); expect(() => inductiveInputArtifactFile(run, "pnml")).toThrow("PNML unavailable");
  });
  it("encodes every filename code unit losslessly while keeping full CSV cell data unchanged", () => {
    const id = "/東京/\ud800/%/🧪"; const encoded = inductiveInputTableFileName(id); expect(encoded).toMatch(/^[A-Za-z0-9_.%\-]+$/);
    const decoded = encoded.replace(/%([a-f0-9]{4})/g, (_, digits: string) => String.fromCharCode(Number.parseInt(digits, 16))); expect(decoded).toBe(id);
    expect(diagnosticCsv({id, title: id, columns: ["literal"], rows: [[id]]})).toContain(id);
  });
});
