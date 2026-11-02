import {afterEach, describe, expect, it, vi} from "vitest";
import {
  MINING_INPUT_SHAPES, MiningSourceRequestGate, activeDatasetAnalysis, activeMiningSourceAllTablesCsv,
  activeMiningSourceBytes, activeMiningSourceReport, activeMiningSourceTableFileName, activeMiningSourceTables,
  activeMiningSourceText, assertMiningSourceBinding, classifyMiningJson, defaultMiningSourceForm,
  isActiveMiningSourceSnapshot, isActiveSourceMining, isMiningInputTable, miningSourceDescriptor, miningSourceFields,
  miningSourceFormFromDescriptor, miningSourceHash, miningSourceOptions, miningSourceWorkbenchFields,
  requestActiveMiningSource, requestActiveMiningSourceFile, suggestMiningSourceForm,
  type ActiveMiningSourceSnapshot, type ActiveSourceMining,
} from "../src/active-mining-source";
import {mineActiveDataset, mineCsv, mineFormEntries} from "../src/api";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseJson, nativeResponseRequest} from "../src/native-response";
import {diagnosticCsv} from "../src/model-diagnostics";
import {isLogConversionResult} from "../src/types";
import {isLogQuerySchema, requestLogQuery, requestQueryFilter, type LogQuerySchema} from "../src/log-query";
import {defaultTokenReplayWorkbenchForm, tokenReplayWorkbenchFields} from "../src/token-replay-workbench";
import {defaultPetriAlignmentWorkbenchForm, petriAlignmentWorkbenchFields} from "../src/petri-alignment-workbench";
import {defaultWorkflowInputForm, isWorkflowInputTable, workflowInputFields} from "../src/workflow-input-workbench";
import {defaultInductiveInputForm, inductiveInputFields} from "../src/inductive-input-workbench";
import {defaultGraphAlignmentForm, graphAlignmentFields} from "../src/graph-alignment-workbench";
import {defaultProcessTreeAlignmentForm, processTreeAlignmentFields} from "../src/process-tree-alignment-workbench";
import {defaultPetriAntiMultiForm, petriAntiMultiFields} from "../src/petri-anti-multi-workbench";

const signed = '{"key":"signed","kind":"int","value":"-9223372036854775808","typed_value":-9223372036854775808,"integer_storage":"int64"}';
const unsigned = '{"key":"unsigned","kind":"int","value":"18446744073709551615","typed_value":18446744073709551615,"integer_storage":"uint64"}';
const na = {key: "NA", kind: "na", container: "scalar", value: "<NA>", typed_value: "NA"};
function table() {
  return parseNativeJson(`{"columns":["","phase:東京","clock","missing","null","NA","signed","unsigned"],"dtypes":["object","object","datetime64[ns]","object","object","object","int64","uint64"],"rows":[[{"key":"case","kind":"string","value":"c1","first_class_role":"case_id","first_class_state":"present","first_class_source_key":"old-case"},{"key":"phase","kind":"string","value":"A","first_class_role":"activity","first_class_state":"present"},{"key":"clock","kind":"date","value":"2024-01-01T00:00:00Z","date_resolution":"ns","date_timezone":"UTC","first_class_role":"timestamp"},null,{"key":"null","kind":"string","value":"","null":true},${JSON.stringify(na)},${signed},${unsigned}]],"index":{"class":"LiteralNativeIndex","dtype":"object","names":[" index "],"labels":[{"kind":"scalar","value":${unsigned}}]},"attributes":[${signed}],"metadata":{"extensions":[],"classifiers":[{"name":"selected","keys":["phase:東京"]}],"trace_globals":[],"event_globals":[${JSON.stringify(na)}],"properties":[${unsigned}],"globals":[{"scope":"event","attributes":[${JSON.stringify(na)}]}]}}`);
}
function snapshot(bytes = 0): ActiveMiningSourceSnapshot {
  const original = table(); if (!isWorkflowInputTable(original)) throw new Error("Invalid own native table fixture");
  return {format: "table", detected_format: "table", shape: "dataframe", source_id: "18446744073709551615", original_dataset: "typed.json", original_input_bytes: bytes, source_unchanged: true,
    source_selectors: {case_id_key: "", activity_key: "phase:東京", timestamp_key: "clock", resource_key: " resource ", lifecycle_key: ""},
    input_before: original, input_after: original, checked_table: original, original_log: null, checked_log: null, prepared_stream: null, prepared_lazy_table: original,
    checked_log_preparation_error: "Native grouped conversion rejected an absent field.", checked_flat_projection_error: null,
    original_table_origin: "uploaded_typed_table", checked_table_origin: "original_typed_table", source_row_ordinals: [0, 0], representative_row_ordinals: [[0, null]], projection_changes: [{checked_row: 1, source_row: 0, column: "phase:東京", removed: false}]};
}
function outcome(bytes = 0): ActiveSourceMining {return {family: "ACTIVE_SOURCE_MINING", source_id: "18446744073709551615", active_source: snapshot(bytes), analysis: null,
  family_results: {alpha: {executed: true, native_input_kind: "dataframe", result: null, error: {type: "native_error", message: "Missing selected column"}}, profiles: {executed: true, native_input_kind: "dataframe", result: {value: "NaN"}, error: null}}, error: null};}
const options = {miningInputFormat: "table" as const, miningInputShape: "dataframe" as const, miningSelectors: {case_id_key: "", activity_key: "phase:東京", timestamp_key: "clock", resource_key: " resource ", lifecycle_key: ""}};
afterEach(() => {vi.unstubAllGlobals(); vi.restoreAllMocks();});

describe("active original native source transport", () => {
  it("classifies full typed tables losslessly and keeps signed/unsigned identity, missing/null/NA and arbitrary index classes", () => {
    const original = table(), classified = classifyMiningJson(nativeJson(original)); expect(classified.kind).toBe("table");
    if (classified.kind !== "table") throw new Error("Expected typed source");
    expect(nativeCell(classified.table.rows[0][6]!, "typed_value")).toBe("-9223372036854775808"); expect(nativeCell(classified.table.rows[0][7]!, "typed_value")).toBe("18446744073709551615");
    expect(classified.table.rows[0][3]).toBeNull(); expect(classified.table.rows[0][4]?.null).toBe(true); expect(classified.table.rows[0][5]?.kind).toBe("na");
    expect(nativeJson(classified.table)).toContain('"integer_storage":"int64"'); expect(nativeJson(classified.table)).toContain('"class":"LiteralNativeIndex"');
  });
  it("recognizes native upload defaults without treating omitted metadata/cell fields as canonical response state", () => {
    const raw = '{"columns":["", "phase"],"rows":[[{}, {"kind":"int","typed_value":18446744073709551615,"integer_storage":"uint64","first_class_role":"activity"}]],"attributes":[{}],"metadata":{}}', parsed = parseNativeJson(raw);
    expect(isMiningInputTable(parsed)).toBe(true); expect(isWorkflowInputTable(parsed)).toBe(false);
    const classified = classifyMiningJson(raw); if (classified.kind !== "table") throw new Error("Expected native upload candidate");
    expect(nativeJson(classified.table)).toBe(nativeJson(parsed)); expect(nativeCell(classified.table.rows[0][1]!, "typed_value")).toBe("18446744073709551615");
    expect(suggestMiningSourceForm(classified.table).selectors.activity_key).toBe("phase"); expect(classified.table.rows[0][0]).toEqual({}); expect(classified.table.metadata).toEqual({});
    for (const suffix of ["", ',"metadata":{}', ',"metadata":{"event_globals":[{"kind":"na"}]}']) expect(classifyMiningJson('{"columns":["x"],"rows":[[{}]]' + suffix + '}').kind).toBe("table");
    expect(isMiningInputTable({columns: ["x"], rows: [[false]]})).toBe(false); expect(isMiningInputTable({columns: ["x"], rows: [[{}]], metadata: []})).toBe(false);
    // Domain errors remain the native parser's responsibility; no JS numeric conversion is used.
    expect(isMiningInputTable({columns: ["x"], rows: [[{kind: "native-invalid"}]], metadata: {}})).toBe(true);
  });
  it("keeps offline exports and both OCEL document conventions distinct even with stale source IDs", () => {
    const offline = {dataset: "saved", trace_count: 0, event_count: 0, activities: [], edges: [], variants: [], source_id: "stale"};
    expect(classifyMiningJson(JSON.stringify(offline)).kind).toBe("offline"); expect(classifyMiningJson('{"ocel:events":{},"ocel:objects":{}}').kind).toBe("ocel"); expect(classifyMiningJson('{"events":{},"objects":{}}').kind).toBe("ocel");
    expect(() => classifyMiningJson('{"columns":["x"],"rows":[[false]]}')).toThrow(/native EventTable/);
  });
  it("suggests physical role columns including an empty case label without changing stale stored source-key metadata", () => {
    const original = table(); if (!isWorkflowInputTable(original)) throw new Error("Invalid table"); const before = nativeJson(original), form = suggestMiningSourceForm(original);
    expect(form.selectors).toMatchObject({case_id_key: "", activity_key: "phase:東京", timestamp_key: "clock"}); expect(form.supplied.case_id_key).toBe(true); expect(nativeJson(original)).toBe(before);
  });
  it("preserves every literal source selector, explicit empty maps and all four shapes independently of miner overrides", () => {
    for (const shape of MINING_INPUT_SHAPES) {
      const fields = mineFormEntries({...options, miningInputShape: shape, alphaActivityAttribute: "miner-phase"});
      expect(fields).toContainEqual(["mining_input_shape", shape]); expect(fields).toContainEqual(["alpha_activity_attribute", "miner-phase"]); expect(fields).toContainEqual(["mining_selectors_json", JSON.stringify(options.miningSelectors)]);
    }
    expect(miningSourceFields({miningSelectors: {}})).toEqual([["mining_selectors_json", "{}"]]); expect(() => miningSourceFields({miningSelectors: {unknown: "x"} as never})).toThrow(/supported/);
    const form = defaultMiningSourceForm(); form.supplied.activity_key = true; form.selectors.activity_key = " \u0000\ud800 "; expect(miningSourceOptions(form).miningSelectors).toEqual({activity_key: " \u0000\ud800 "});
  });
  it("retains raw source import controls and omits inapplicable imports for native table input", () => {
    expect(mineFormEntries({miningImportFields: {xes_max_traces: "18446744073709551615", xes_activity_attribute: "phase", delimiter: "\t"}})).toContainEqual(["xes_max_traces", "18446744073709551615"]);
    expect(mineFormEntries({...options, xesActivityAttribute: "", delimiter: "\t", miningImportFields: {xes_max_traces: "18446744073709551615"}}).some(([key]) => key.startsWith("xes_") || key === "delimiter")).toBe(false);
  });
  it("accepts source-only outcomes with independent native results while rejecting source identity substitution", () => {
    const value = outcome(); expect(isActiveSourceMining(value)).toBe(true); expect(activeDatasetAnalysis(value)).toBeNull(); expect(isActiveMiningSourceSnapshot(value.active_source)).toBe(true);
    expect(isActiveSourceMining({...value, source_id: "other"})).toBe(false); expect(isActiveMiningSourceSnapshot({...value.active_source, input_before: {columns: ["x"], rows: [[false]]}})).toBe(false);
  });
  it("uploads the exact original File bytes and captures native numeric text without table reserialization", async () => {
    const bytes = activeMiningSourceBytes(' {"literal":"\ud800","count":18446744073709551615}\n'), file = new File([bytes], "typed.json"), value = outcome(file.size), raw = nativeJson(value);
    let submitted: FormData | undefined; const abort = new AbortController(); vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {expect(init.signal).toBe(abort.signal); submitted = init.body as FormData; return new Response(raw, {status: 200});}));
    const returned = await mineActiveDataset(file, options, abort.signal); expect(nativeResponseJson(returned)).toBe(raw); expect(nativeResponseRequest(returned)?.fields).toContainEqual(["mining_input_format", "table"]);
    const sent = submitted!.get("log") as File; expect(Array.from(new Uint8Array(await sent.arrayBuffer()))).toEqual(Array.from(bytes)); expect(submitted!.get("mining_request_id")).toMatch(/^[a-f0-9-]{36}$/);
    await expect(mineCsv(file, options, abort.signal)).rejects.toThrow(/no classic analysis/);
  });
  it("rejects late replacement, canceled upload and stale cached results while retaining string source identities", () => {
    const gate = new MiningSourceRequestGate(), first = gate.issue(), signal = new AbortController(); expect(gate.accepts(first)).toBe(true); const second = gate.issue(); expect(gate.accepts(first)).toBe(false); expect(gate.accepts(second)).toBe(true);
    signal.abort(); expect(gate.accepts(second, signal.signal)).toBe(false); gate.invalidate(); expect(gate.accepts(second)).toBe(false);
    expect(() => assertMiningSourceBinding("18446744073709551615", "18446744073709551615", 4, 4)).not.toThrow(); expect(() => assertMiningSourceBinding("old", "new", 4, 4)).toThrow(/changed/); expect(() => assertMiningSourceBinding(null, null, 4, 4)).toThrow(/No live backend log is loaded/); expect(() => assertMiningSourceBinding("same", "same", 4, 5)).toThrow(/changed/);
  });
  it("rejects an aborted mining response even when a transport completes after cancellation", async () => {
    const abort = new AbortController(), file = new File(["original"], "typed.json"); vi.stubGlobal("fetch", vi.fn(async () => {abort.abort(); return new Response(nativeJson(outcome(file.size)));})); await expect(mineActiveDataset(file, options, abort.signal)).rejects.toThrow();
  });
  it("binds descriptors to original byte sizes and keeps the parsed snapshot numeric sidecars on re-mining", async () => {
    const file = new File([activeMiningSourceBytes("raw \ud800")], "typed.json"), hash = await miningSourceHash(file), value = outcome(file.size), descriptor = miningSourceDescriptor(file, hash, {...options, dependencyThreshold: 0.6}, value);
    expect(descriptor.file).toBe(file); expect(descriptor.snapshot).toBe(value.active_source); expect(miningSourceFormFromDescriptor(descriptor).selectors).toEqual(value.active_source.source_selectors);
    expect(() => miningSourceDescriptor(file, hash, options, outcome(file.size + 1))).toThrow(/does not match/); expect(() => miningSourceDescriptor(new File(["x"], "other.json"), hash, options, value)).toThrow(/does not match/);
    const report = activeMiningSourceReport(descriptor, value.active_source, value); expect(report).toMatch(/"typed_value"\s*:\s*18446744073709551615/); expect(report).toContain('"family_results"'); expect(report).toContain(hash);
  });
  it("maps exact active selectors into all five existing standalone workbench request carriers", () => {
    const source = miningSourceDescriptor(new File([], "typed.json"), "hash", options, outcome()), mapped = miningSourceWorkbenchFields(source, {case_id_key: ["alpha_case_id_attribute", "heuristic_case_id_attribute"], activity_key: ["alpha_activity_attribute", "heuristic_activity_attribute"], timestamp_key: ["alpha_timestamp_attribute", "heuristic_timestamp_attribute"]});
    const workflow = {...defaultWorkflowInputForm(), format: source.format, shape: source.shape, ...mapped}; expect(Object.fromEntries(workflowInputFields(workflow, source.file.name)).discovery_selectors_json).toContain('"alpha_case_id_attribute":""');
    const carriers: Array<[Record<string,string>, Array<[string,string]>]> = [];
    const inductive = miningSourceWorkbenchFields(source, {case_id_key: "inductive_case_id_key", activity_key: "inductive_activity_key", timestamp_key: "inductive_timestamp_key"}); carriers.push([inductive.fields, inductiveInputFields({...defaultInductiveInputForm(), format: source.format, shape: source.shape, ...inductive}, "typed.json")]);
    const graph = miningSourceWorkbenchFields(source, {case_id_key: "alignment_case_id_attribute", activity_key: "dfg_activity_attribute", timestamp_key: "dfg_timestamp_attribute"}); carriers.push([graph.fields, graphAlignmentFields({...defaultGraphAlignmentForm(), inputFormat: source.format, ...graph}, "typed.json")]);
    const tree = miningSourceWorkbenchFields(source, {case_id_key: "alignment_case_id_attribute", activity_key: "pt_alignment_activity_attribute"}); carriers.push([tree.fields, processTreeAlignmentFields({...defaultProcessTreeAlignmentForm(), inputFormat: source.format, ...tree}, "typed.json")]);
    const anti = miningSourceWorkbenchFields(source, {case_id_key: "alignment_case_id_attribute", activity_key: "activity_attribute"}); carriers.push([anti.fields, petriAntiMultiFields({...defaultPetriAntiMultiForm(), inputFormat: source.format, ...anti}, "typed.json")]);
    for (const [expected, fields] of carriers) {const carrier = fields.find(([key]) => key.endsWith("selectors_json"))!; expect(JSON.parse(carrier[1])).toEqual(expected); expect(fields).toContainEqual(["input_format", "table"]);}
  });
  it("propagates native table format, source selectors and independent adapters through legacy Token/Petri requests", () => {
    for (const adapter of MINING_INPUT_SHAPES) {
      const replay = tokenReplayWorkbenchFields({...defaultTokenReplayWorkbenchForm(), adapter, inputFormat: "table", miningSelectors: options.miningSelectors, importFields: {delimiter: "bad"}}, "typed.json"), petri = petriAlignmentWorkbenchFields({...defaultPetriAlignmentWorkbenchForm(), adapter, inputFormat: "table", miningSelectors: options.miningSelectors, importFields: {delimiter: "bad"}}, "typed.json");
      for (const fields of [replay, petri]) {expect(fields).toContainEqual(["mining_input_format", "table"]); expect(fields).toContainEqual(["mining_selectors_json", JSON.stringify(options.miningSelectors)]); expect(fields.some(([key]) => key === "delimiter")).toBe(false);}
    }
  });
  it("preserves full CSV cells and every selected primitive/object row beyond safe integer precision", () => {
    const value = parseNativeJson(`{"input_before":${nativeJson(table())},"source_row_ordinals":[18446744073709551615,0,0],"signed":[-9223372036854775808,9223372036854775807]}`) as object, tables = activeMiningSourceTables(value), cells = tables.find(item => item.id === "input_before_cells")!;
    expect(cells.rows).toHaveLength(8); expect(diagnosticCsv(cells)).toContain("18446744073709551615"); expect(nativeJson(cells.records![6])).toContain('"typed_value":-9223372036854775808'); expect(tables.find(item => item.id === "/input_before/rows")!.records).toBe((value as {input_before: {rows: unknown[]}}).input_before.rows);
    expect(nativeJson(tables.find(item => item.id === "/source_row_ordinals")!.records![0])).toBe('{"value":18446744073709551615}'); expect(nativeJson(tables.find(item => item.id === "/signed")!.records![1])).toBe('{"value":9223372036854775807}');
    expect(activeMiningSourceAllTablesCsv(tables)).toContain("-9223372036854775808"); expect(activeMiningSourceAllTablesCsv(tables)).toContain("18446744073709551615");
  });
  it("accepts actual legacy NA in stream/table conversion envelopes without accepting unknown native kinds", () => {
    expect(isLogConversionResult({family: "LOG_CONVERSION", variant: "TO_EVENT_STREAM", stream: {attributes: [na], events: [{fields: [na]}]}})).toBe(true);
    expect(isLogConversionResult({family: "LOG_CONVERSION", variant: "TO_DATA_FRAME", table: {columns: ["NA"], rows: [[na]]}})).toBe(true);
    expect(isLogConversionResult({family: "LOG_CONVERSION", variant: "TO_DATA_FRAME", table: {columns: ["NA"], rows: [[{...na, kind: "unknown"}]]}})).toBe(false);
  });
  it("retains unknown case counts in source-only schemas rather than inventing zero cases", () => {
    const schema = {family: "LOG_QUERY_SCHEMA", source_id: "id", dataset: "typed.json", event_count: 1, case_count: null, table_name: "dataframe", event_index_column: "ordinal", sqlite_available: true, duckdb_configured: false, columns: ["ordinal"]};
    expect(isLogQuerySchema(schema)).toBe(true); expect(isLogQuerySchema({...schema, case_count: -1})).toBe(false); expect(isLogQuerySchema({...schema, case_count: undefined})).toBe(false);
  });
  it("submits native SQL query and table-export bodies as exact WTF-8 bytes with the supplied source and cancellation signal", async () => {
    const schema: LogQuerySchema = {family: "LOG_QUERY_SCHEMA", source_id: "exact-id", event_count: 1, case_count: null, table_name: "dataframe", event_index_column: "ordinal", sqlite_available: true, duckdb_configured: false, columns: ["ordinal", "\ud800", "\udc00", "😀"]}, sql = 'SELECT "\ud800", "\udc00", "😀", "ordinal" FROM dataframe', abort = new AbortController(), observed: Array<{path: string; bytes: number[]}> = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string, init: RequestInit) => {expect(init.signal).toBe(abort.signal); expect(init.body).toBeInstanceOf(Uint8Array); const bytes = Array.from(init.body as Uint8Array); observed.push({path, bytes}); return path.startsWith("/api/log-query/export") ? new Response(nativeJson(table()), {headers: {"Content-Type": "application/json"}}) : new Response(JSON.stringify({family: "LOG_QUERY", engine: "sqlite", source_id: schema.source_id, sql, event_index_column: "ordinal", columns: ["ordinal"], rows: [[0]], output_truncated: false, can_filter: true, selected_event_indices: [0]}));}));
    await requestLogQuery(schema, "sqlite", sql, 100, abort.signal); const exported = await requestQueryFilter(schema, "sqlite", sql, "events", "json", abort.signal); expect(isWorkflowInputTable(parseNativeJson(await exported.text()))).toBe(true);
    expect(observed).toHaveLength(2); for (const request of observed) {expect(request.path).toContain("source_id=exact-id"); expect(request.bytes).toEqual(Array.from(activeMiningSourceBytes(sql))); expect(request.bytes).toContain(0xed); expect(activeMiningSourceText(new Uint8Array(request.bytes))).toBe(sql);}
  });
  it("keeps native WTF-8 file bytes and lossless ASCII filenames for empty/NUL/lone-surrogate labels", () => {
    const text = "東京\u0000\ud800x\udc00😀", bytes = activeMiningSourceBytes(text); expect(activeMiningSourceText(bytes)).toBe(text); expect(Array.from(bytes).slice(-4)).toEqual([240,159,152,128]); expect(() => activeMiningSourceText(new Uint8Array([0xc0,0x80]))).toThrow(/invalid/);
    const first = activeMiningSourceTableFileName("/\ud800"), second = activeMiningSourceTableFileName("/%d800"); expect(first).not.toBe(second); expect(first).toBe("%002f%d800"); expect(activeMiningSourceTableFileName("/A").toLowerCase()).not.toBe(activeMiningSourceTableFileName("/a").toLowerCase());
  });
  it("binds source inspection and verified original downloads to the exact source and original hash", async () => {
    const file = new File([activeMiningSourceBytes("original \ud800")], "typed.json"), source = miningSourceDescriptor(file, await miningSourceHash(file), options, outcome(file.size)), envelope = {family: "ACTIVE_EVENT_SOURCE", source_id: source.source_id, active_source: source.snapshot};
    vi.stubGlobal("fetch", vi.fn(async (path: string) => path.startsWith("/api/active-source-file") ? new Response(await file.arrayBuffer()) : new Response(nativeJson(envelope))));
    expect((await requestActiveMiningSource(source.source_id)).value.source_id).toBe(source.source_id); expect(await miningSourceHash(await requestActiveMiningSourceFile(source))).toBe(source.sha256);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson({...envelope, source_id: "other"})))); await expect(requestActiveMiningSource(source.source_id)).rejects.toThrow(/does not match/);
    vi.stubGlobal("fetch", vi.fn(async () => new Response("different original"))); await expect(requestActiveMiningSourceFile(source)).rejects.toThrow(/do not match/);
  });
});
