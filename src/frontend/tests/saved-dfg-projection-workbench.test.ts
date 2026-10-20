import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {GraphAlignmentWorkspace} from "../src/components/GraphAlignmentWorkspace";
import {SavedDfgProjectionWorkspace} from "../src/components/SavedDfgProjectionWorkspace";
import {defaultGraphAlignmentForm, GraphAlignmentRequestGate, graphAlignmentAllTablesCsv, type GraphAlignmentRun} from "../src/graph-alignment-workbench";
import {defaultSavedDfgProjectionForm, isSavedDfgResult, isSavedDfgProjectionValue, runSavedDfgProjectionWorkbench, savedDfgProjectionFields, savedDfgProjectionReport, savedDfgProjectionReuse, type SavedDfgProjectionForm, type SavedDfgProjectionValue} from "../src/saved-dfg-projection-workbench";
import {nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";

afterEach(() => vi.unstubAllGlobals());
const source = () => new File(["case,activity\nc,A\nc,B\n"], "source.csv", {lastModified: 1});
const model = () => new File(['{"kind":"clean","dfg":[],"start_activities":[],"end_activities":[]}'], "model.json", {lastModified: 2});
const resultRaw = '{"alignments":[{"computed":true,"alignment":[{"log":"A","model":"A"},{"log":"B","model":">>"},{"log":">>","model":"B"}],"cost":"INF","internal_cost":"NaN","fitness":"-INF","best_worst_cost":0,"visited_states":18446744073709551615,"closed":9007199254740993,"closed_states":2}],"unique_variants":18446744073709551615,"projection":null}';
const config = (): SavedDfgProjectionForm => ({...defaultSavedDfgProjectionForm(), savedJson: resultRaw});
const hex = (bytes: Uint8Array) => ({native_bytes: "hex-v1" as const, data: Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("")});
async function response(input: File, dfg: File, form: SavedDfgProjectionForm, saved: File | null = null): Promise<SavedDfgProjectionValue> {
  const savedBytes = saved ? new Uint8Array(await saved.arrayBuffer()) : nativeStringBytes(form.savedJson!);
  const uploaded = parseNativeJson(new TextDecoder().decode(savedBytes)) as Record<string, unknown>;
  const envelope = form.savedInputShape === "report" ? uploaded.native_result as Record<string, unknown> : uploaded;
  const result = form.savedInputShape === "native_result" ? uploaded : envelope.result;
  return {family: "GRAPH_ALIGNMENT_WORKBENCH", operation: "project_saved_dfg", mode: "DFG_CLASSIC", input_shape: "log", result_shape: "log", executed: true, error: null, supplied_result: true, alignment_executed: false,
    input_context: {dataset: input.name, original_input_bytes: input.size, format: form.inputFormat, activity_attribute: "concept:name", timestamp_attribute: "time:timestamp", case_id_attribute: "case:concept:name"},
    model_context: {dataset: dfg.name, original_input_bytes: dfg.size, format: form.source.modelFormat, dfg: {kind: "clean", dfg: [], start_activities: [], end_activities: []}},
    saved_result_context: {source: saved ? "uploaded_file" : "inline", name: saved?.name ?? null, bytes: savedBytes.length, input_shape: form.savedInputShape},
    original_input: hex(new Uint8Array(await input.arrayBuffer())), original_model: hex(new Uint8Array(await dfg.arrayBuffer())), original_saved_result: hex(savedBytes),
    result: result as Record<string, unknown>, projection: {dfg: [], activities: []}, projected_log: {events: []}, input_raw_log: {traces: []}, projected_raw_log: {traces: []}, projected_xes: "<log />", model_export: "0\n0\n0\n0", model_export_format: "dfg",
    projection_error: null, metrics_error: null, log_projection_error: null, projected_log_error: null, projected_xes_error: null, model_export_error: null, projection_complete: true};
}
function seed(input = source(), dfg = model()): GraphAlignmentRun {
  const raw = `{"family":"GRAPH_ALIGNMENT_WORKBENCH","mode":"DFG_CLASSIC","input_shape":"log","result":${resultRaw}}`;
  return {raw, value: parseNativeJson(raw), fields: [["mode", "DFG_CLASSIC"], ["input_format", "table"], ["input_shape", "log"], ["model_format", "dfg_json"], ["projection", "none"], ["alignment_selectors_json", '{"dfg_activity_attribute":" A,B "}'], ["dfg_sync_cost_json", "{}"], ["delimiter", "|"]], inputs: {input, model: dfg, input_sha256: "a", model_sha256: "b"}, tables: [], http_status: 200} as GraphAlignmentRun;
}
describe("saved DFG result projection", () => {
  it("defaults to complete log and metrics projection with native result shape", () => {
    const form = defaultSavedDfgProjectionForm(), fields = Object.fromEntries(savedDfgProjectionFields(form, "x.tsv"));
    expect(form.source).toEqual({...defaultGraphAlignmentForm(), projection: "both"}); expect(fields).toMatchObject({operation: "project_saved_dfg", mode: "DFG_CLASSIC", input_shape: "log", input_format: "auto", projection: "both", saved_input_shape: "native_result", delimiter: "\t"}); expect(fields.saved_result_json).toBeUndefined();
  });
  it("retains exact inline JSON, explicit empty selectors, byte selectors and empty cost maps", () => {
    const form = config(); form.selectorsJson = '{"dfg_activity_attribute":{"native_bytes":"hex-v1","data":"ff0020"},"dfg_timestamp_attribute":""}'; form.source.fields.dfg_sync_cost_json = "{}";
    const fields = Object.fromEntries(savedDfgProjectionFields(form, "x.csv")); expect(fields.alignment_selectors_json).toBe(form.selectorsJson); expect(fields.saved_result_json).toBe(resultRaw); expect(fields.dfg_sync_cost_json).toBe("{}"); expect(fields.saved_result_json).toContain("18446744073709551615");
  });
  it("retains complete typed/raw-log format choice and independent explicit projection none", () => {
    for (const format of ["table", "raw_log"] as const) {const form = config(); form.inputFormat = format; form.source.projection = "none"; const fields = Object.fromEntries(savedDfgProjectionFields(form, "x.json")); expect(fields.input_format).toBe(format); expect(fields.projection).toBe("none");}
  });
  it("rejects malformed inline/selector objects and unsupported saved shape before transport", () => {
    for (const form of [{...config(), savedJson: "{"}, {...config(), selectorsJson: '{"unknown":"x"}'}, {...config(), selectorsJson: '{"dfg_activity_attribute":null}'}, {...config(), savedInputShape: "rows" as SavedDfgProjectionForm["savedInputShape"]}]) expect(() => savedDfgProjectionFields(form, "x.csv")).toThrow();
  });
  it("accepts uncomputed rows, exact uint64 counters, nonfinite scores and arbitrary byte moves as supplied data", () => {
    const value = parseNativeJson(resultRaw) as Record<string, unknown>; expect(isSavedDfgResult(value)).toBe(true);
    const row = (value.alignments as Record<string, unknown>[])[0]; row.computed = false; (row.alignment as Record<string, unknown>[])[0].log = {native_bytes: "hex-v1", data: "ff00"}; expect(isSavedDfgResult(value)).toBe(true); expect(nativeJson(value)).toContain("9007199254740993");
  });
  it("accepts saved decimal-string counters with leading zeros and validates their canonical integer values", async () => {
    const input = source(), dfg = model(), form = {...config(), savedJson: resultRaw.replace('"visited_states":18446744073709551615', '"visited_states":"18446744073709551615"').replace('"closed_states":2', '"closed_states":"0002"').replace('"unique_variants":18446744073709551615', '"unique_variants":"00018446744073709551615"')};
    const value = await response(input, dfg, form); value.result = parseNativeJson(resultRaw) as Record<string, unknown>;
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); const run = await runSavedDfgProjectionWorkbench(input, dfg, form); expect(run.savedJson).toContain('"closed_states":"0002"'); expect(savedDfgProjectionReport(run)).toContain("18446744073709551615");
  });
  it("reuses actual source/model Files and entire captured workbench rather than synthesized routes", async () => {
    const original = seed(), reused = savedDfgProjectionReuse(original); expect(reused.input).toBe(original.inputs.input); expect(reused.model).toBe(original.inputs.model); expect(await reused.saved.text()).toBe(original.raw); expect(reused.form.savedInputShape).toBe("workbench"); expect(reused.form.savedJson).toBeUndefined(); expect(reused.form.inputFormat).toBe("table"); expect(reused.form.source.projection).toBe("both"); expect(reused.form.source.importFields.delimiter).toBe("|"); expect(reused.form.selectorsJson).toBe('{"dfg_activity_attribute":" A,B "}'); expect(reused.form.source.fields.dfg_sync_cost_json).toBe("{}");
  });
  it("rejects raw trace or Petri alignment reuse", () => {
    const original = seed(); original.value.mode = "RECOMPOS_MAXIMAL"; expect(() => savedDfgProjectionReuse(original)).toThrow("grouped-log DFG");
  });
  it("reuses a complete generated report as a real File with the original source/model objects", async () => {
    const original = seed(), reused = savedDfgProjectionReuse(original, "report"); expect(reused.input).toBe(original.inputs.input); expect(reused.model).toBe(original.inputs.model); expect(reused.form.savedInputShape).toBe("report"); const raw = await reused.saved.text(); expect(raw).toContain('"family": "GRAPH_ALIGNMENT_WORKBENCH_REPORT"'); expect(raw).toContain("18446744073709551615"); expect(raw).toContain('"native_result"');
  });
  it("hands actual Files to public transport and captures full response/report without rounding", async () => {
    const original = seed(), reused = savedDfgProjectionReuse(original); reused.form.selectorsJson = "";
    const raw = nativeJson(await response(reused.input, reused.model, reused.form, reused.saved));
    const fetcher = vi.fn(async (path: string, init: RequestInit) => {expect(path).toBe("/api/align-graph-workbench"); const body = init.body as FormData; expect(body.get("input_file")).toBe(reused.input); expect(body.get("model_file")).toBe(reused.model); expect(body.get("saved_result_file")).toBe(reused.saved); expect(body.has("saved_result_json")).toBe(false); expect(await (body.get("saved_result_file") as File).text()).toBe(original.raw); return new Response(raw);}); vi.stubGlobal("fetch", fetcher);
    const run = await runSavedDfgProjectionWorkbench(reused.input, reused.model, reused.form, undefined, reused.saved); expect(fetcher).toHaveBeenCalledOnce(); expect(run.raw).toBe(raw); expect(run.inputs.saved).toBe(reused.saved); expect(run.inputs.saved_sha256).toMatch(/^[0-9a-f]{64}$/); expect(savedDfgProjectionReport(run)).toContain("18446744073709551615"); expect(graphAlignmentAllTablesCsv(run.tables)).toContain("9007199254740993");
  });
  it("rejects File/inline conflicts by presence including an explicit empty object", async () => {
    const fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher); const input = source(), dfg = model(), saved = new File(["{}"], "saved.json");
    await expect(runSavedDfgProjectionWorkbench(input, dfg, {...config(), savedJson: "{}"}, undefined, saved)).rejects.toThrow("never both"); await expect(runSavedDfgProjectionWorkbench(input, dfg, defaultSavedDfgProjectionForm())).rejects.toThrow("one saved result"); expect(fetcher).not.toHaveBeenCalled();
  });
  it("submits untrimmed multiline inline JSON as bytes and forwards cancellation signal", async () => {
    const input = source(), dfg = model(), form = {...config(), savedJson: `\n ${resultRaw}\n`}, abort = new AbortController(), value = await response(input, dfg, form);
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {expect(init.signal).toBe(abort.signal); const entry = (init.body as FormData).get("saved_result_json") as File; expect(new Uint8Array(await entry.arrayBuffer())).toEqual(nativeStringBytes(form.savedJson)); return new Response(nativeJson(value));}));
    const run = await runSavedDfgProjectionWorkbench(input, dfg, form, abort.signal); expect(run.savedJson).toBe(form.savedJson);
  });
  it("retains canonical saved result and independent native projection/export errors", async () => {
    const input = source(), dfg = model(), form = config(), value = await response(input, dfg, form); value.projection_complete = false; value.projection_error = "uncomputed row"; value.metrics_error = "count mismatch"; value.projected_xes_error = "timestamp unsupported"; value.projected_xes = null;
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); const run = await runSavedDfgProjectionWorkbench(input, dfg, form); expect(isSavedDfgProjectionValue(run.value)).toBe(true); expect(run.value.result).not.toBeNull(); expect(savedDfgProjectionReport(run)).toContain("timestamp unsupported"); expect(savedDfgProjectionReport(run)).toContain("count mismatch");
  });
  it("retains native input validation envelopes with original saved bytes and failure HTTP status", async () => {
    const input = source(), dfg = model(), form = config(), value = await response(input, dfg, form); value.result = null; value.executed = false; value.error = {type: "native_error", message: "missing computed flag"};
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value), {status: 400}))); const run = await runSavedDfgProjectionWorkbench(input, dfg, form); expect(run.http_status).toBe(400); expect(run.value.error).toEqual(value.error); expect(run.value.original_saved_result).toEqual(value.original_saved_result);
  });
  it("rejects stale file identities, changed originals and changed saved shapes", async () => {
    const input = source(), dfg = model(), form = config();
    for (const mutate of [(value: SavedDfgProjectionValue) => {value.input_context.dataset = "stale.csv";}, (value: SavedDfgProjectionValue) => {value.original_model = "changed";}, (value: SavedDfgProjectionValue) => {value.original_saved_result = "{}";}, (value: SavedDfgProjectionValue) => {value.saved_result_context.input_shape = "report";}]) {const value = await response(input, dfg, form); mutate(value); vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); await expect(runSavedDfgProjectionWorkbench(input, dfg, form)).rejects.toThrow(/match|changed|identity/);}
  });
  it("rejects changed canonical scores, routes, computed flags and exact counters", async () => {
    const input = source(), dfg = model(), form = config();
    for (const key of ["cost", "alignment", "computed", "visited_states"]) {const value = await response(input, dfg, form), trace = (value.result!.alignments as Record<string, unknown>[])[0]; if (key !== "visited_states") trace[key] = key === "cost" ? 1 : key === "alignment" ? [] : false; const raw = key === "visited_states" ? nativeJson(value).replace('"visited_states":18446744073709551615', '"visited_states":1') : nativeJson(value); vi.stubGlobal("fetch", vi.fn(async () => new Response(raw))); await expect(runSavedDfgProjectionWorkbench(input, dfg, form)).rejects.toThrow("changed supplied alignment");}
  });
  it("keeps a supplied negative zero score distinct from positive zero", async () => {
    const input = source(), dfg = model(), form = {...config(), savedJson: resultRaw.replace('"best_worst_cost":0', '"best_worst_cost":-0')}, value = await response(input, dfg, form);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); await expect(runSavedDfgProjectionWorkbench(input, dfg, form)).resolves.toBeDefined(); const raw = nativeJson(value).replace('"best_worst_cost":-0', '"best_worst_cost":0'); vi.stubGlobal("fetch", vi.fn(async () => new Response(raw))); await expect(runSavedDfgProjectionWorkbench(input, dfg, form)).rejects.toThrow("changed supplied alignment");
  });
  it("validates exact empty and byte selectors against the native response", async () => {
    const input = source(), dfg = model(), form = config(); form.selectorsJson = '{"dfg_activity_attribute":{"native_bytes":"hex-v1","data":"ff00"},"dfg_timestamp_attribute":""}'; const value = await response(input, dfg, form); value.input_context.activity_attribute = {native_bytes: "hex-v1", data: "ff00"}; value.input_context.timestamp_attribute = "";
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); await expect(runSavedDfgProjectionWorkbench(input, dfg, form)).resolves.toBeDefined(); value.input_context.timestamp_attribute = " "; await expect(runSavedDfgProjectionWorkbench(input, dfg, form)).rejects.toThrow("exact submitted selector");
  });
  it("accepts separately uploaded report envelopes without replacing their original bytes", async () => {
    const input = source(), dfg = model(), form = defaultSavedDfgProjectionForm(); form.savedInputShape = "report"; const saved = new File([` {"native_result":{"result":${resultRaw}},"opaque":{"identity":" keep "}} `], "saved-report.json"); const value = await response(input, dfg, form, saved);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); const run = await runSavedDfgProjectionWorkbench(input, dfg, form, undefined, saved); expect(run.inputs.saved).toBe(saved); expect(savedDfgProjectionReport(run)).toContain("saved-report.json");
  });
  it("retains an explicitly empty native result without introducing traces or boundaries", async () => {
    const input = source(), dfg = model(), form = {...config(), savedJson: '{"alignments":[],"unique_variants":0,"projection":null}'}, value = await response(input, dfg, form);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); const run = await runSavedDfgProjectionWorkbench(input, dfg, form); expect(run.value.result!.alignments).toEqual([]); expect(nativeJson(run.value.model_context.dfg)).toContain('"start_activities":[]'); expect(nativeJson(run.value.model_context.dfg)).toContain('"end_activities":[]');
  });
  it("honours cancellation before transport, after response and stale generation rejection", async () => {
    const input = source(), dfg = model(), form = config(), abort = new AbortController(), fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher); abort.abort(); await expect(runSavedDfgProjectionWorkbench(input, dfg, form, abort.signal)).rejects.toThrow(); expect(fetcher).not.toHaveBeenCalled();
    const later = new AbortController(), value = await response(input, dfg, form); vi.stubGlobal("fetch", vi.fn(async () => {later.abort(); return new Response(nativeJson(value));})); await expect(runSavedDfgProjectionWorkbench(input, dfg, form, later.signal)).rejects.toThrow(); const gate = new GraphAlignmentRequestGate(), ticket = gate.issue(); gate.invalidate(); expect(gate.accepts(ticket)).toBe(false);
  });
  it("renders the existing graph workspace with new alignment as default and explicit saved projection operation", () => {
    const html = renderToStaticMarkup(createElement(GraphAlignmentWorkspace, {})); expect(html).toContain('value="new_alignment" selected=""'); expect(html).toContain("Project saved DFG alignment result"); expect(html).toContain("Run independent graph alignment"); expect(html).toContain("Saved projection original source"); expect(html).toContain("Complete native raw EventLog JSON");
  });
  it("renders independent saved upload, inline edit and original completed File reuse controls", () => {
    const html = renderToStaticMarkup(createElement(SavedDfgProjectionWorkspace, {active: true, completed: seed()})); expect(html).toContain("Reuse completed DFG original Files and full result"); expect(html).toContain("Original saved alignment File"); expect(html).toContain("Enter or edit saved JSON inline"); expect(html).toContain("without running alignment search"); expect(html).toContain("Exact selectors, import settings and native cost options");
  });
});
