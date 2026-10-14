import {afterEach, describe, expect, it, vi} from "vitest";
import {
  GRAPH_ALIGNMENT_CONTROLS, GRAPH_ALIGNMENT_PNML_CONTROLS, GraphAlignmentRequestGate, defaultGraphAlignmentForm,
  graphAlignmentAllTablesCsv, graphAlignmentArtifactFile, graphAlignmentControlFields, graphAlignmentFields, graphAlignmentNativeTables,
  graphAlignmentReport, graphAlignmentSucceeded, graphAlignmentSummary, graphAlignmentTables, isGraphAlignmentWorkbench, runGraphAlignmentWorkbench,
  isDecomposedNativeAlignment, isDfgNativeLogAlignment, isDfgNativeTraceAlignment,
  type DecomposedNativeAlignment, type DfgNativeTraceAlignment, type GraphAlignmentMode, type GraphAlignmentRun, type GraphAlignmentWorkbench,
} from "../src/graph-alignment-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";
import {nativeResponseRequest} from "../src/native-response";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../src/token-replay-workbench";

const dfgTrace = (): DfgNativeTraceAlignment => ({computed: true, alignment: [{log: "A", model: "A"}], cost: 0, internal_cost: 0,
  fitness: "NaN", best_worst_cost: "NaN", visited_states: 2, closed: 2, closed_states: 2});
const decomposed = (): DecomposedNativeAlignment => ({complete: true, timed_out: false, exhausted: false, fitness: 1, best_worst_cost: 1,
  trace_count: 1, total_cost: 0, diagnostics: {decompositions: 1, components: 1, merges: 0, cache_hits: 0, cache_misses: 1,
    border_disagreements: 0, agreement_iterations: 1, visited_states: 3, queued_states: 4, traversed_arcs: 2, whole_net_fallback: false, note: ""},
  traces: [{activities: ["A"], count: 1, complete: true, timed_out: false, exhausted: false, cost: 0, fitness: 1, best_worst_cost: 1, alignment: [{log: "A", model: "A", transition: 0}]}]});
const inputFile = () => new File(["case_id,activity\n1,A\n"], "original.csv", {lastModified: 0});
const modelFile = (mode: GraphAlignmentMode = "DFG_CLASSIC") => new File([mode === "DFG_CLASSIC" ? "native DFG" : "<pnml></pnml>"], mode === "DFG_CLASSIC" ? "model.dfg" : "model.pnml", {lastModified: 0});
function workbench(mode: GraphAlignmentMode = "DFG_CLASSIC", shape: "log" | "trace" | "words" = "log", input = inputFile(), model = modelFile(mode)): GraphAlignmentWorkbench {
  return {family: "GRAPH_ALIGNMENT_WORKBENCH", mode, input_shape: shape, result_shape: mode === "DFG_CLASSIC" ? shape === "log" ? "log" : "trace" : shape === "words" ? "trace" : "log", executed: true, error: null,
    input_context: {format: shape === "words" ? "trace_words" : "auto", detected_format: "csv", dataset: input.name, original_input_bytes: input.size, input_event_count: 1, input_case_count: 1, checked_event_count: 1, checked_case_count: 1, origin: "uploaded_csv_rows", trace_index: shape === "trace" ? 0 : null, activity_attribute: "concept:name", timestamp_attribute: mode === "DFG_CLASSIC" ? "time:timestamp" : null, case_id_attribute: "case:concept:name"},
    model_context: {format: mode === "DFG_CLASSIC" ? "auto" : "pnml", detected_format: mode === "DFG_CLASSIC" ? "dfg" : "pnml", dataset: model.name, original_input_bytes: model.size, dfg: mode === "DFG_CLASSIC" ? {kind: "clean", dfg: [], start_activities: {A: 1}, end_activities: {A: 1}} : null, document: mode === "DFG_CLASSIC" ? null : {name: "supplied PNML"}, native_net: null, markings: mode === "DFG_CLASSIC" ? null : {initial_origin: "pnml", final_origin: "native_default", options_initial_marking: [1, 0], options_final_marking: [], effective_initial_marking: [1, 0], effective_final_marking: [0, 1]}},
    input_table: null, input_trace_words: shape === "words" ? ["A"] : null, input_log: shape === "words" ? null : {traces: [{case_id: "1", events: [{activity: "A"}]}]},
    result: mode === "DFG_CLASSIC" ? shape === "log" ? {alignments: [dfgTrace()], unique_variants: 1, projection: null} : dfgTrace() : shape === "words" ? decomposed().traces[0] : decomposed(),
    projection: null, projected_log: null, projected_xes: null, projection_error: null, model_export: mode === "DFG_CLASSIC" ? "native DFG" : "<pnml></pnml>", model_export_format: mode === "DFG_CLASSIC" ? "dfg" : "pnml", model_export_error: null};
}
function saved(value: GraphAlignmentWorkbench): GraphAlignmentRun {
  const input = inputFile(), model = modelFile(value.mode), form = defaultGraphAlignmentForm(value.mode); form.inputShape = value.input_shape; form.inputFormat = value.input_context.format;
  const fields = graphAlignmentFields(form, input.name);
  return {value, raw: nativeJson(value), fields, tables: graphAlignmentTables(value, fields), http_status: value.executed ? 200 : 400, inputs: {input, model, input_sha256: "input hash", model_sha256: "model hash"}};
}
afterEach(() => vi.unstubAllGlobals());

describe("independent graph alignment controls and requests", () => {
  it("exposes every native DFG and decomposed option, including PNML import controls", () => {
    expect(GRAPH_ALIGNMENT_CONTROLS.DFG_CLASSIC.map(control => control.key)).toEqual(["alignment_case_id_attribute", "dfg_activity_attribute", "dfg_timestamp_attribute", "dfg_max_states", "dfg_max_time_ms", "dfg_sync_cost_json", "dfg_model_move_cost_json", "dfg_log_move_cost_json", "dfg_internal_log_move_cost_json"]);
    expect(GRAPH_ALIGNMENT_CONTROLS.RECOMPOS_MAXIMAL.map(control => control.key)).toEqual(["alignment_case_id_attribute", "activity_attribute", "threshold_border_agreement", "max_time_trace", "max_time_total", "max_states", "max_total_states", "use_cache", "clear_cache", "log_move_cost", "model_move_cost", "tau_model_cost", "sync_cost", "activity_log_costs_json", "activity_model_costs_json", "activity_sync_costs_json", "initial_marking_json", "final_marking_json"]);
    expect(GRAPH_ALIGNMENT_PNML_CONTROLS.map(([key]) => key)).toEqual(["pnml_infer_initial_marking", "pnml_infer_final_marking", "pnml_require_workflow_boundaries", "pnml_import_stochastic_map", "pnml_public_marking_semantics"]);
  });
  it("preserves raw numeric/JSON spellings, zero limits, exact labels, empty maps and uint64 marking arrays", () => {
    for (const mode of ["DFG_CLASSIC", "RECOMPOS_MAXIMAL"] as const) {
      const values: Record<string, string> = {};
      for (const control of GRAPH_ALIGNMENT_CONTROLS[mode]) values[control.key] = control.kind === "selector" ? " \u00a0東京,;\u0000 " : control.kind === "integer" ? " 0 " : control.kind === "number" ? " -0.000000000000000000000 " : control.kind === "map" ? ' {"":0,"  ":-0.0,"__proto__":0.10000000000000001,"東京,;":1} ' : control.kind === "marking" ? " [18446744073709551615,0] " : "false";
      const result = graphAlignmentControlFields(mode, values);
      for (const control of GRAPH_ALIGNMENT_CONTROLS[mode]) expect(control.kind === "selector" ? result.selectors[control.key] : Object.fromEntries(result.fields)[control.key]).toBe(values[control.key]);
      expect(graphAlignmentControlFields(mode, {})).toMatchObject({fields: []});
      const emptyKey = GRAPH_ALIGNMENT_CONTROLS[mode].find(control => control.kind === "map")!.key;
      expect(graphAlignmentControlFields(mode, {[emptyKey]: "{}"}).fields).toEqual([[emptyKey, "{}"]]);
    }
    expect(graphAlignmentControlFields("RECOMPOS_MAXIMAL", {initial_marking_json: "[]"}).fields).toEqual([["initial_marking_json", "[]"]]);
    expect(() => graphAlignmentControlFields("RECOMPOS_MAXIMAL", {initial_marking_json: "[-0]"})).toThrow(/unsigned/);
    expect(() => graphAlignmentControlFields("RECOMPOS_MAXIMAL", {initial_marking_json: "[18446744073709551616]"})).toThrow(/unsigned/);
    expect(() => graphAlignmentControlFields("DFG_CLASSIC", {dfg_sync_cost_json: '{"A":"NaN"}'})).toThrow(/finite/);
  });
  it("submits exact selectors through their carrier and keeps all import/PNML fields source-independent", () => {
    const form = defaultGraphAlignmentForm("RECOMPOS_MAXIMAL"); form.fields.activity_attribute = " \n東京 "; form.explicitEmptySelectors.alignment_case_id_attribute = true;
    for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) form.importFields[key] = "literal:東京";
    for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) form.importFields[key] = "false";
    for (const [key] of GRAPH_ALIGNMENT_PNML_CONTROLS) form.pnmlFields[key] = "false";
    form.importFields.xes_variant = "chunk_regex";
    const fields = Object.fromEntries(graphAlignmentFields(form, "input.tsv"));
    expect(JSON.parse(fields.alignment_selectors_json)).toEqual({alignment_case_id_attribute: "", activity_attribute: " \n東京 "});
    expect(fields.activity_attribute).toBeUndefined(); expect(fields.alignment_case_id_attribute).toBeUndefined(); expect(fields.delimiter).toBe("\t");
    for (const [key] of [...TOKEN_REPLAY_IMPORT_CONTROLS, ...TOKEN_REPLAY_XES_BOOLEAN_CONTROLS, ...GRAPH_ALIGNMENT_PNML_CONTROLS]) expect(fields[key]).toBe(form.importFields[key] ?? form.pnmlFields[key]);
    expect(fields.source_id).toBeUndefined(); expect(fields.include_source).toBeUndefined();
  });
  it("requires matching word/model/projection shapes without silently rewriting control choices", () => {
    const form = defaultGraphAlignmentForm(); form.inputFormat = "trace_words"; expect(() => graphAlignmentFields(form, "trace.json")).toThrow(/trace-word/);
    form.inputShape = "words"; form.projection = "both"; expect(() => graphAlignmentFields(form, "trace.json")).toThrow(/projection/);
    form.projection = "none"; expect(Object.fromEntries(graphAlignmentFields(form, "trace.json"))).toMatchObject({input_shape: "words", input_format: "trace_words"});
    form.modelFormat = "pnml"; expect(() => graphAlignmentFields(form, "trace.json")).toThrow(/DFG/);
    const trace = defaultGraphAlignmentForm(); trace.inputShape = "trace"; trace.traceIndex = " 18446744073709551615 ";
    expect(Object.fromEntries(graphAlignmentFields(trace, "input.csv")).trace_index).toBe(trace.traceIndex);
  });
  it("binds the original two files and native request to the independent endpoint", async () => {
    const input = inputFile(), model = modelFile(), value = workbench("DFG_CLASSIC", "log", input, model), fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(nativeJson(value)))); vi.stubGlobal("fetch", fetcher);
    const result = await runGraphAlignmentWorkbench(input, model, defaultGraphAlignmentForm());
    expect(fetcher.mock.calls[0][0]).toBe("/api/align-graph-workbench"); const body = fetcher.mock.calls[0][1].body as FormData;
    expect(body.get("input_file")).toBe(input); expect(body.get("model_file")).toBe(model); expect(body.get("source_id")).toBeNull();
    expect(result.inputs.input_sha256).toMatch(/^[0-9a-f]{64}$/); expect(result.inputs.model_sha256).toMatch(/^[0-9a-f]{64}$/);
    expect(nativeResponseRequest(result.value)?.fields).toEqual(result.fields);
    await expect(runGraphAlignmentWorkbench(new File([await input.arrayBuffer()], "other.csv"), model, defaultGraphAlignmentForm())).rejects.toThrow(/does not match/);
  });
  it("retains structured native errors and partial input/model snapshots from a 400 response", async () => {
    const value = workbench("RECOMPOS_MAXIMAL"); value.executed = false; value.result = null; value.error = {type: "KeyError", message: "literal missing case"};
    value.input_context.format = "table"; value.input_context.input_case_count = value.input_context.checked_case_count = value.input_context.checked_event_count = null;
    value.input_table = {columns: ["NA"], rows: [[{key: "NA", kind: "na", value: "<NA>", typed_value: "NA"}]], dtypes: ["object"]};
    const form = defaultGraphAlignmentForm("RECOMPOS_MAXIMAL"); form.inputFormat = "table";
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value), {status: 400})));
    const result = await runGraphAlignmentWorkbench(inputFile(), modelFile("RECOMPOS_MAXIMAL"), form);
    expect(result.http_status).toBe(400); expect(result.value.input_table).toEqual(value.input_table); expect(graphAlignmentSucceeded(result.value)).toBe(false);
    expect(graphAlignmentReport(result)).toContain("literal missing case"); expect(graphAlignmentSummary(result.value)).toContain("Alignment failed");
  });
  it("rejects late cancellation and mismatched modes, formats, sizes, exact selectors or selected traces", async () => {
    const input = inputFile(), model = modelFile(), form = defaultGraphAlignmentForm();
    for (const mutate of [
      (value: GraphAlignmentWorkbench) => {value.input_context.original_input_bytes += 1;},
      (value: GraphAlignmentWorkbench) => {value.model_context.dataset = "other.dfg";},
      (value: GraphAlignmentWorkbench) => {value.model_context.format = "dfg_json";},
    ]) {
      const value = workbench(); mutate(value); vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(value))));
      await expect(runGraphAlignmentWorkbench(input, model, form)).rejects.toThrow(/does not match/);
    }
    const literal = defaultGraphAlignmentForm(); literal.fields.dfg_activity_attribute = " literal "; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench()))));
    await expect(runGraphAlignmentWorkbench(input, model, literal)).rejects.toThrow(/literal selectors/);
    const trace = defaultGraphAlignmentForm(); trace.inputShape = "trace"; trace.traceIndex = "1"; vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(nativeJson(workbench("DFG_CLASSIC", "trace")))));
    await expect(runGraphAlignmentWorkbench(input, model, trace)).rejects.toThrow(/selected original trace/);
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(nativeJson(workbench())));}));
    await expect(runGraphAlignmentWorkbench(input, model, form, controller.signal)).rejects.toThrow(/abort/i);
  });
});

describe("native graph alignment snapshots", () => {
  it("accepts all native input/result shapes and preserves supplied marking origins separately from effective defaults", () => {
    for (const mode of ["DFG_CLASSIC", "RECOMPOS_MAXIMAL"] as const) for (const shape of ["log", "trace", "words"] as const) {
      const value = workbench(mode, shape); expect(isGraphAlignmentWorkbench(value)).toBe(true); expect(graphAlignmentSucceeded(value)).toBe(true);
      expect(isGraphAlignmentWorkbench({...value, result_shape: value.result_shape === "log" ? "trace" : "log"})).toBe(false);
    }
    const value = workbench("RECOMPOS_MAXIMAL"), markings = value.model_context.markings!;
    markings.options_final_marking = []; markings.effective_final_marking = [0, 1];
    const result = saved(value); expect(graphAlignmentReport(result)).toContain('"final_origin": "native_default"');
    expect(graphAlignmentAllTablesCsv(result.tables)).toContain("options_final_marking"); expect(graphAlignmentAllTablesCsv(result.tables)).toContain("effective_final_marking");
    const incomplete = workbench("RECOMPOS_MAXIMAL"); (incomplete.result as DecomposedNativeAlignment).complete = false;
    expect(graphAlignmentSucceeded(incomplete)).toBe(false); expect(graphAlignmentSummary(incomplete)).toContain("Incomplete");
    expect(isGraphAlignmentWorkbench({...value, error: {type: "error", message: "fake success"}})).toBe(false);
  });
  it("exports exact native wide markings, complete nested input/model metadata and raw nonfinite scores", () => {
    const value = workbench("RECOMPOS_MAXIMAL"); value.input_table = {columns: ["literal"], rows: [[{key: "literal", kind: "string", value: "東京" + "z".repeat(900)}]]};
    const raw = nativeJson(value).replace('"options_initial_marking":[1,0]', '"options_initial_marking":[18446744073709551615,0]').replace('"fitness":1', '"fitness":"NaN"');
    const parsed = parseNativeJson(raw); expect(isGraphAlignmentWorkbench(parsed)).toBe(true); if (!isGraphAlignmentWorkbench(parsed)) throw new Error("Invalid envelope");
    const result = saved(parsed); expect(graphAlignmentReport(result)).toContain("18446744073709551615");
    expect(graphAlignmentAllTablesCsv(result.tables)).toContain("18446744073709551615"); expect(graphAlignmentAllTablesCsv(result.tables)).toContain("z".repeat(900));
    expect(graphAlignmentSummary(parsed)).toContain("NaN");
    const bad = {...parsed, model_context: {...parsed.model_context, markings: {...parsed.model_context.markings!, options_initial_marking: [-1]}}}; expect(isGraphAlignmentWorkbench(bad)).toBe(false);
  });
  it("keeps same-run model and XES bytes, including lone surrogates, and exposes explicit export failures", async () => {
    const value = workbench(), text = "<log>ä 東京 🧪 \ud800 \udc00</log>";
    value.projected_xes = text; value.model_export = "A\n\ud800\nB";
    const result = saved(value), projection = graphAlignmentArtifactFile(result, "projection"), model = graphAlignmentArtifactFile(result, "model");
    expect(projection.name).toMatch(/\.xes$/); expect(model.name).toMatch(/\.dfg$/);
    expect(new Uint8Array(await projection.arrayBuffer())).toEqual(workflowInputNativeBytes(text));
    expect(new Uint8Array(await model.arrayBuffer())).toEqual(new Uint8Array([65, 10, 0xed, 0xa0, 0x80, 10, 66]));
    value.projected_xes = null; value.projection_error = "Native projection unavailable"; value.model_export = null; value.model_export_error = "Native model export unavailable";
    expect(() => graphAlignmentArtifactFile(result, "projection")).toThrow("Native projection unavailable"); expect(() => graphAlignmentArtifactFile(result, "model")).toThrow("Native model export unavailable");
  });
  it("preserves undefined raw DFG scores and every stored field of uncomputed native outcomes", () => {
    const trace = dfgTrace(); expect(isDfgNativeTraceAlignment(trace)).toBe(true);
    trace.computed = false; trace.cost = "INF"; trace.internal_cost = "-INF";
    expect(isDfgNativeTraceAlignment(trace)).toBe(true);
    const malformed = {...trace, fitness: null}; expect(isDfgNativeTraceAlignment(malformed)).toBe(false);
    expect(isDfgNativeTraceAlignment({...trace, alignment: [{log: "A", model: null}]})).toBe(false);
    const csv = graphAlignmentAllTablesCsv(graphAlignmentNativeTables({native_result: trace}, "DFG_CLASSIC", trace));
    expect(csv).toContain("NaN"); expect(csv).toContain("internal_cost"); expect(csv).toContain("-INF");
  });
  it("retains DFG projection counts, literal labels and native variant caching without flattening away zero entries", () => {
    const result = {alignments: [dfgTrace(), dfgTrace()], unique_variants: 1, projection: {dfg: [{source: "__proto__", target: "東京,;\u0000\n", sync: 1, mm: 0}], activities: [{activity: "", sync: 0, mm: 0, lm: 0}]}};
    expect(isDfgNativeLogAlignment(result)).toBe(true);
    expect(isDfgNativeLogAlignment({...result, unique_variants: 3})).toBe(false);
    expect(isDfgNativeLogAlignment({...result, projection: {...result.projection, activities: [{activity: "", sync: -1, mm: 0, lm: 0}]}})).toBe(false);
    const csv = graphAlignmentAllTablesCsv(graphAlignmentNativeTables({native_result: result}, "DFG_CLASSIC", result));
    expect(csv).toContain("__proto__"); expect(csv).toContain("東京,;\\u0000\\n"); expect(csv).toContain('"4","lm","0"');
  });
  it("accepts incomplete, exhausted and timed-out decompositions without manufacturing a successful score", () => {
    const result = decomposed(); result.complete = false; result.exhausted = true; result.timed_out = true;
    result.traces[0].complete = false; result.traces[0].exhausted = true; result.traces[0].timed_out = true; result.fitness = "NaN";
    expect(isDecomposedNativeAlignment(result)).toBe(true);
    expect(isDecomposedNativeAlignment({...result, diagnostics: {...result.diagnostics, whole_net_fallback: 0}})).toBe(false);
    expect(isDecomposedNativeAlignment({...result, traces: [{...result.traces[0], alignment: [{log: null, model: null, transition: -1}]}]})).toBe(false);
  });
  it("keeps exact native uint64 transition sentinels and numeric literals in full JSON, rows and exports", () => {
    const result = decomposed(); result.traces[0].alignment = [{log: "", model: null, transition: 1}]; result.total_cost = 1;
    const raw = JSON.stringify(result).replace('"transition":1', '"transition":18446744073709551615').replace('"total_cost":1', '"total_cost":0.10000000000000001');
    const value = parseNativeJson(raw); expect(isDecomposedNativeAlignment(value)).toBe(true); if (!isDecomposedNativeAlignment(value)) throw new Error("Invalid native result");
    const tables = graphAlignmentNativeTables({native_result: value}, "RECOMPOS_MAXIMAL", value), moves = tables.find(table => table.id === "complete_alignment_moves")!;
    expect(nativeCell(value.traces[0].alignment[0], "transition")).toBe("18446744073709551615");
    expect(moves.rows[0][4]).toBe("18446744073709551615"); expect(nativeJson(moves.records![0])).toContain("18446744073709551615");
    expect(graphAlignmentAllTablesCsv(tables)).toContain("0.10000000000000001"); expect(diagnosticCsv(moves)).toContain("18446744073709551615");
  });
  it("exports every native move after UI paging and guards cancelled or superseded requests", () => {
    const trace = dfgTrace(); trace.alignment = Array.from({length: 73}, (_, i) => ({log: `event:${i}`, model: "A"}));
    const moves = graphAlignmentNativeTables({native_result: trace}, "DFG_CLASSIC", trace).find(table => table.id === "complete_alignment_moves")!;
    expect(moves.rows).toHaveLength(73); expect(diagnosticPage(moves.rows, 2)).toHaveLength(23); expect(diagnosticCsv(moves)).toContain("event:72");
    const gate = new GraphAlignmentRequestGate(), old = gate.issue(), current = gate.issue(), controller = new AbortController();
    expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current, controller.signal)).toBe(true); controller.abort(); expect(gate.accepts(current, controller.signal)).toBe(false);
    gate.invalidate(); expect(gate.accepts(current)).toBe(false);
  });
});
