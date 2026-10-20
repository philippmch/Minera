import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {TokenReplayWorkspace} from "../src/components/TokenReplayWorkspace";
import {defaultTokenReplayWorkbenchForm, type TokenReplayWorkbenchRun} from "../src/token-replay-workbench";
import {nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {replayDurationAnalysis, replayDurationExample, replayDurationFields, replayDurationReport, replayDurationReuse, replayDurationSavedAnalysis, replayDurationSavedFileAnalysis, replayDurationSourceFromRun, runReplayDurationWorkbench, type ReplayDurationForm, type ReplayDurationWorkflow} from "../src/replay-duration-workbench";
import {diagnosticCsv} from "../src/model-diagnostics";

afterEach(() => vi.unstubAllGlobals());
const log = () => new File(["original log"], "original.csv", {lastModified: 7});
const model = () => new File(["original pnml"], "original.pnml", {lastModified: 8});
const form = (raw = replayDurationExample("cohorts")): ReplayDurationForm => ({source: defaultTokenReplayWorkbenchForm(), analysisRaw: raw});
function response(input: File, pnml: File | null, raw: string, saved: File | null = null, savedRaw = ""): ReplayDurationWorkflow {
  return {family: "REPLAY_DURATION_WORKFLOW", mode: replayDurationAnalysis(raw).mode, replay_executed: false, analysis_raw: raw, input: parseNativeJson(raw), original_log: "original log", original_model: pnml ? "original pnml" : null,
    input_context: {dataset: input.name, log_bytes: input.size, model: pnml?.name ?? null, model_bytes: pnml?.size ?? 0}, model_context: pnml ? {places: [], transitions: []} : null,
    computation: {complete: true, error: null, error_type: null}, result: {duration_diagnostics: {transitions: [], unknown_activities: []}, case_duration_seconds: null, median_duration_seconds: null}, presentation_errors: [], original_saved_diagnostics: saved ? savedRaw : null, saved_diagnostics_context: {source: saved ? "uploaded_file" : "none", name: saved?.name ?? null, bytes: saved?.size ?? 0, input_shape: replayDurationAnalysis(raw).saved_input_shape ?? "native_rows"}};
}
function completed(): TokenReplayWorkbenchRun {
  const raw = '{"family":"TOKEN_REPLAY_WORKFLOW","variant":"forward","input_context":{"activity_attribute":" name "},"diagnostics":{"family":"TOKEN_REPLAY","traces":[{"trace_index":0,"produced":18446744073709551617,"remaining":-9999999999999999999999,"event_diagnostics":[{"event_index":0,"transitions_with_problems":[7]}],"unknown_event_indices":[1],"reached_marking":[18446744073709551617,0],"reached_marking_keys":[true,true],"reached_marking_order":[1,0],"marking_domain":{"places":[]}}]},"full_result":{"opaque":"preserved"}}';
  return {raw, value: parseNativeJson(raw), fields: [["token_replay_input_adapter", "lazyframe"], ["mining_input_format", "table"], ["mining_selectors_json", '{"timestamp_key":" stamp "}'], ["token_replay_case_id_attribute", " case "], ["token_replay_max_rows_to_materialize", "0"], ["token_replay_stop_immediately_unfit", "true"]], inputs: {log: log(), model: model(), log_sha256: "a", model_sha256: "b"}, tables: []} as TokenReplayWorkbenchRun;
}
describe("caller-supplied replay duration analysis", () => {
  it("retains exact large indices, repeated median weights and byte selector carriers", () => {
    const raw = ' {"mode":"median_duration","trace_indices":[18446744073709551615,0,0],"timestamp_attribute":{"native_bytes":"hex-v1","data":"ff0020"}} ';
    const input = form(raw); expect(Object.fromEntries(replayDurationFields(input, "x.csv")).analysis).toBe(raw);
    expect(nativeJson(replayDurationAnalysis(raw))).toContain("18446744073709551615,0,0");
  });
  it("retains null and empty labels and empty unknown cohorts that affect baselines", () => {
    const raw = '{"mode":"cohorts","transition_cohorts":[{"transition_index":7,"activity":null,"problem_trace_indices":[2,2]}],"unknown_activity_cohorts":[{"activity":"","activity_is_null":false,"problem_trace_indices":[]},{"activity":"X,Y","activity_is_null":false,"problem_trace_indices":[2]}]}';
    expect(replayDurationFields(form(raw), "x.csv").at(-1)).toEqual(["analysis", raw]);
  });
  it("shares typed-source imports without sending replay operations", () => {
    const input = form(); input.source.inputFormat = "table"; input.source.adapter = "dataframe"; input.source.miningSelectors = {activity_key: "", case_id_key: " a,b "}; input.source.replayFields.token_replay_max_rows_to_materialize = "0";
    const fields = Object.fromEntries(replayDurationFields(input, "x.tsv")); expect(fields.mining_input_format).toBe("table"); expect(fields.mining_selectors_json).toBe('{"activity_key":"","case_id_key":" a,b "}'); expect(fields.token_replay_max_rows_to_materialize).toBe("0"); expect(fields.token_replay_variant).toBeUndefined(); expect(fields.token_replay_enable_pltr_fitness).toBeUndefined(); expect(fields.delimiter).toBeUndefined();
  });
  it("exposes typed raw-log source transport separately from native EventTable imports", () => {
    const input = {...form(), durationInputFormat: "raw_log" as const}; expect(Object.fromEntries(replayDurationFields(input, "x.json")).duration_input_format).toBe("raw_log");
  });
  it("retains explicit false import flags and literal CSV mappings", () => {
    const input = form(); input.source.importFields = {xes_require_timestamp: "false", xes_timestamp_sort: "false", activity_column: "A,B", case_column: " case "};
    const fields = Object.fromEntries(replayDurationFields(input, "x.tsv")); expect(fields.xes_require_timestamp).toBe("false"); expect(fields.activity_column).toBe("A,B"); expect(fields.case_column).toBe(" case "); expect(fields.delimiter).toBe("\t");
  });
  it("rejects malformed JSON and unsupported modes before transport", () => {
    for (const raw of ["{", "[]", '{"mode":"replay"}']) expect(() => replayDurationFields(form(raw), "x.csv")).toThrow();
  });
  it("reuses the completed run's actual original Files and full diagnostics", async () => {
    const replay = completed(), reused = replayDurationReuse(replay); expect(reused.log).toBe(replay.inputs.log); expect(reused.model).toBe(replay.inputs.model);
    expect(await reused.saved.text()).toBe(replay.raw); expect(replay.raw).toContain("18446744073709551617"); expect(replay.raw).toContain("-9999999999999999999999"); expect(replay.raw).toContain('"reached_marking_order"'); expect(replay.raw).toContain('"full_result"'); expect(Object.hasOwn(replayDurationAnalysis(reused.form.analysisRaw), "saved_diagnostics")).toBe(false);
    expect(reused.form.source.adapter).toBe("lazyframe"); expect(reused.form.source.inputFormat).toBe("table"); expect(reused.form.source.replayFields.token_replay_stop_immediately_unfit).toBeUndefined(); expect(replayDurationAnalysis(reused.form.analysisRaw).activity_attribute).toBe(" name "); expect(replayDurationAnalysis(reused.form.analysisRaw).timestamp_attribute).toBe(" stamp ");
  });
  it("reconstructs import settings from the completed request rather than a current form", () => {
    const replay = completed(); replay.fields = [["token_replay_input_adapter", "stream"], ["activity_column", "literal,activity"], ["xes_timestamp_sort", "false"], ["delimiter", "|"]]; const source = replayDurationSourceFromRun(replay);
    expect(source.adapter).toBe("stream"); expect(source.importFields).toEqual({activity_column: "literal,activity", xes_timestamp_sort: "false", delimiter: "|"});
  });
  it("loads complete saved envelopes without rounding nested token integers or retaining unrelated mode fields", () => {
    const raw = '{"native_result":{"diagnostics":{"traces":[{"produced":18446744073709551617,"event_diagnostics":[{"event_index":0,"transitions_with_problems":[7]}]}]}}}';
    const analysis = replayDurationSavedAnalysis(raw, "report", '{"mode":"median_duration","trace_indices":[0,0],"activity_attribute":" A "}');
    expect(analysis).toContain("18446744073709551617"); expect(analysis).not.toContain("trace_indices"); expect(replayDurationAnalysis(analysis).activity_attribute).toBe(" A "); expect(replayDurationAnalysis(analysis).saved_input_shape).toBe("report");
  });
  it("submits the actual saved diagnostic File without an inline replacement and retains its byte identity", async () => {
    const replay = completed(), reused = replayDurationReuse(replay), raw = nativeJson(response(reused.log, reused.model, reused.form.analysisRaw, reused.saved, replay.raw));
    const fetcher = vi.fn(async (_path: string, init: RequestInit) => {const body = init.body as FormData; expect(body.get("saved_diagnostics_file")).toBe(reused.saved); expect(body.get("log")).toBe(reused.log); expect(body.get("model")).toBe(reused.model); expect(await (body.get("saved_diagnostics_file") as File).text()).toBe(replay.raw); return new Response(raw);}); vi.stubGlobal("fetch", fetcher);
    const run = await runReplayDurationWorkbench(reused.log, reused.model, reused.form, undefined, reused.saved); expect(run.inputs.saved).toBe(reused.saved); expect(run.inputs.saved_sha256).toMatch(/^[a-f0-9]{64}$/); expect(replayDurationReport(run)).toContain("18446744073709551617"); expect(run.raw).toBe(raw);
  });
  it("rejects file and inline diagnostic conflicts by presence, including an empty inline array", async () => {
    const input = log(), pnml = model(), saved = new File(["[]"], "saved.json"), fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher);
    await expect(runReplayDurationWorkbench(input, pnml, form(replayDurationExample("saved_diagnostics")), undefined, saved)).rejects.toThrow("no inline saved_diagnostics"); expect(fetcher).not.toHaveBeenCalled();
    expect(Object.hasOwn(replayDurationAnalysis(replayDurationSavedFileAnalysis("report", '{"mode":"cohorts","activity_attribute":" ","transition_cohorts":[]}')), "saved_diagnostics")).toBe(false);
  });
  it("rejects changed saved diagnostic bytes and mismatched saved envelope identity", async () => {
    const replay = completed(), reused = replayDurationReuse(replay);
    for (const change of [(value: ReplayDurationWorkflow) => {value.original_saved_diagnostics = "[]";}, (value: ReplayDurationWorkflow) => {(value.saved_diagnostics_context as Record<string, unknown>).input_shape = "report";}]) {const value = response(reused.log, reused.model, reused.form.analysisRaw, reused.saved, replay.raw); change(value); vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); await expect(runReplayDurationWorkbench(reused.log, reused.model, reused.form, undefined, reused.saved)).rejects.toThrow(/changed original|File identity/);}
  });
  it("sends original File objects, raw analysis bytes and cancellation signal without requiring a model for cohorts", async () => {
    const input = log(), config = form(), abort = new AbortController(), raw = nativeJson(response(input, null, config.analysisRaw));
    const fetcher = vi.fn(async (_path: string, init: RequestInit) => {const body = init.body as FormData; expect(body.get("log")).toBe(input); expect(body.has("model")).toBe(false); expect(init.signal).toBe(abort.signal); const field = body.get("analysis") as File; expect(new Uint8Array(await field.arrayBuffer())).toEqual(nativeStringBytes(config.analysisRaw)); return new Response(raw);}); vi.stubGlobal("fetch", fetcher);
    const run = await runReplayDurationWorkbench(input, null, config, abort.signal); expect(run.raw).toBe(raw); expect(run.inputs.log).toBe(input); expect(run.inputs.model).toBeNull(); expect(run.inputs.log_sha256).toMatch(/^[a-f0-9]{64}$/); expect(fetcher).toHaveBeenCalledOnce();
  });
  it("retains native computation errors and optional presentation errors in downloadable results", async () => {
    const input = log(), config = form(), value = response(input, null, config.analysisRaw); value.computation = {complete: false, error: "missing timestamp", error_type: "out_of_range"}; value.presentation_errors.push("optional source projection"); value.result.duration_diagnostics = null;
    vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); const run = await runReplayDurationWorkbench(input, null, config); expect(run.value.computation.complete).toBe(false); expect(replayDurationReport(run)).toContain("missing timestamp"); expect(replayDurationReport(run)).toContain("optional source projection");
  });
  it("keeps exact scalar tokens and complete native tables in reports and CSV", async () => {
    const input = log(), config = form(replayDurationExample("median_duration")), value = response(input, null, config.analysisRaw), raw = nativeJson(value).replace('"median_duration_seconds":null', '"median_duration_seconds":18446744073709551617');
    vi.stubGlobal("fetch", vi.fn(async () => new Response(raw))); const run = await runReplayDurationWorkbench(input, null, config); expect(replayDurationReport(run)).toContain("18446744073709551617"); expect(run.tables.map(diagnosticCsv).join("\n")).toContain("18446744073709551617"); expect(run.tables.find(table => table.id === "request")?.rows.at(-1)).toEqual(["analysis", config.analysisRaw]);
  });
  it("refuses missing original models for saved diagnostics before making a request", async () => {
    const fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher); await expect(runReplayDurationWorkbench(log(), null, form(replayDurationExample("saved_diagnostics")))).rejects.toThrow("original PNML"); expect(fetcher).not.toHaveBeenCalled();
  });
  it("rejects stale or changed native input identities and original bytes", async () => {
    const input = log(), config = form(); for (const change of [(value: ReturnType<typeof response>) => {value.input_context.dataset = "other.csv";}, (value: ReturnType<typeof response>) => {value.analysis_raw = config.analysisRaw + " ";}, (value: ReturnType<typeof response>) => {value.original_log = "changed log";}]) {const value = response(input, null, config.analysisRaw); change(value); vi.stubGlobal("fetch", vi.fn(async () => new Response(nativeJson(value)))); await expect(runReplayDurationWorkbench(input, null, config)).rejects.toThrow(/match|changed original/);}
  });
  it("honours cancellation before transport and immediately after the response", async () => {
    const input = log(), config = form(), abort = new AbortController(), fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher); abort.abort(); await expect(runReplayDurationWorkbench(input, null, config, abort.signal)).rejects.toThrow(); expect(fetcher).not.toHaveBeenCalled();
    const during = new AbortController(); vi.stubGlobal("fetch", vi.fn(async () => {during.abort(); return new Response(nativeJson(response(input, null, config.analysisRaw)));})); await expect(runReplayDurationWorkbench(input, null, config, during.signal)).rejects.toThrow();
  });
  it("renders independent duration operations without an uploaded log or a cached replay", () => {
    const html = renderToStaticMarkup(createElement(TokenReplayWorkspace, {file: null}));
    expect(html).toContain('aria-label="Independent replay duration analysis"');
    expect(html).toContain("Analyse replay durations");
    expect(html).toContain("Duration original log");
    expect(html).toContain("Duration original PNML (optional)");
    for (const mode of ["cohorts", "saved_diagnostics", "case_duration", "median_duration"]) expect(html).toContain(`value="${mode}"`);
    expect(html).toContain("Full native EventTable JSON");
    expect(html).toContain("Full native typed raw-log JSON");
    expect(html).toContain("Complete duration analysis JSON");
    expect(html).toMatch(/<button[^>]*disabled=""[^>]*>Run duration analysis<\/button>/);
    expect(html).toContain("Run independent token replay");
    expect(html).not.toContain("Use completed replay original files and full diagnostics");
    expect(html).not.toContain('aria-label="Complete duration analysis inspection"');
  });
});
