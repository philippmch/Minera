import {activeMiningSourceTables, miningSourceHash} from "./active-mining-source";
import { backendFetch } from "./backend-fetch";
import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {defaultTokenReplayWorkbenchForm, tokenReplayWorkbenchFields, type TokenReplayWorkbenchForm, type TokenReplayWorkbenchRun} from "./token-replay-workbench";
import {type DiagnosticTable} from "./model-diagnostics";
import {isOriginalEmbeddingFilterNativeString, originalEmbeddingFilterStringBytes} from "./original-embedding-filter-workbench";

export const REPLAY_DURATION_MODES = ["cohorts", "saved_diagnostics", "case_duration", "median_duration"] as const;
export type ReplayDurationMode = typeof REPLAY_DURATION_MODES[number];
export type ReplayDurationSavedShape = "native_rows" | "native_result" | "workbench" | "report";
export interface ReplayDurationForm {source: TokenReplayWorkbenchForm; analysisRaw: string; durationInputFormat?: "auto" | "raw_log";}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
export function replayDurationExample(mode: ReplayDurationMode): string {
  return nativeJson(mode === "cohorts" ? {mode, transition_cohorts: [{transition_index: 0, activity: "A", problem_trace_indices: [0]}], unknown_activity_cohorts: []}
    : mode === "saved_diagnostics" ? {mode, saved_input_shape: "native_rows", saved_diagnostics: []}
    : mode === "case_duration" ? {mode, trace_index: 0} : {mode, trace_indices: [0, 0, 1]}, 2);
}
export function replayDurationAnalysis(raw: string): Record<string, unknown> & {mode: ReplayDurationMode} {
  let value: unknown; try {value = parseNativeJson(raw);} catch {throw new Error("Duration analysis must be valid JSON. Exact integer tokens are retained.");}
  if (!object(value) || !REPLAY_DURATION_MODES.includes(value.mode as ReplayDurationMode)) throw new Error("Select a supported duration analysis mode in the JSON object.");
  return value as Record<string, unknown> & {mode: ReplayDurationMode};
}
export function replayDurationSourceFields(source: TokenReplayWorkbenchForm, name: string): Array<[string, string]> {
  return tokenReplayWorkbenchFields({...source, variant: "forward", replayFields: Object.fromEntries(Object.entries(source.replayFields).filter(([key]) => ["token_replay_case_id_attribute", "token_replay_max_rows_to_materialize"].includes(key)))}, name).filter(([key]) => key !== "token_replay_variant");
}
export function replayDurationFields(form: ReplayDurationForm, name: string): Array<[string, string]> {
  replayDurationAnalysis(form.analysisRaw);
  if (form.durationInputFormat !== undefined && !["auto", "raw_log"].includes(form.durationInputFormat)) throw new Error("Select a supported duration source format.");
  return [...replayDurationSourceFields(form.source, name), ...(form.durationInputFormat === undefined ? [] : [["duration_input_format", form.durationInputFormat] as [string, string]]), ["analysis", form.analysisRaw]];
}
/** Rebuild import settings from the completed request, never from mutable upload controls. */
export function replayDurationSourceFromRun(run: TokenReplayWorkbenchRun): TokenReplayWorkbenchForm {
  const form = defaultTokenReplayWorkbenchForm(); form.replayFields = {};
  for (const [key, value] of run.fields) {
    if (key === "token_replay_input_adapter") form.adapter = value as TokenReplayWorkbenchForm["adapter"];
    else if (key === "mining_input_format") form.inputFormat = value as "auto" | "table";
    else if (key === "mining_selectors_json") form.miningSelectors = parseNativeJson(value) as TokenReplayWorkbenchForm["miningSelectors"];
    else if (["token_replay_case_id_attribute", "token_replay_max_rows_to_materialize"].includes(key)) form.replayFields[key] = value;
    else if (!key.startsWith("token_replay_")) form.importFields[key] = value;
  }
  return form;
}
export function replayDurationSavedAnalysis(raw: string, shape: ReplayDurationSavedShape, previous?: string): string {
  const saved = parseNativeJson(raw), prior = previous === undefined ? {} : replayDurationAnalysis(previous);
  const value = Object.fromEntries(Object.entries(prior).filter(([key]) => ["activity_attribute", "timestamp_attribute"].includes(key)));
  return nativeJson({...value, mode: "saved_diagnostics", saved_input_shape: shape, saved_diagnostics: saved}, 2);
}
export function replayDurationSavedFileAnalysis(shape: ReplayDurationSavedShape, previous?: string): string {
  const prior = previous === undefined ? {} : replayDurationAnalysis(previous);
  return nativeJson({...Object.fromEntries(Object.entries(prior).filter(([key]) => ["activity_attribute", "timestamp_attribute"].includes(key))), mode: "saved_diagnostics", saved_input_shape: shape}, 2);
}
export function replayDurationReuse(run: TokenReplayWorkbenchRun): {log: File; model: File; saved: File; form: ReplayDurationForm} {
  const value: Record<string, unknown> = {mode: "saved_diagnostics", saved_input_shape: "workbench", activity_attribute: run.value.input_context.activity_attribute};
  const source = replayDurationSourceFromRun(run); if (source.miningSelectors?.timestamp_key !== undefined) value.timestamp_attribute = source.miningSelectors.timestamp_key;
  return {log: run.inputs.log, model: run.inputs.model, saved: new File([nativeStringBytes(run.raw)], "minera-completed-token-replay.json", {type: "application/json"}), form: {source, analysisRaw: nativeJson(value, 2)}};
}
export interface ReplayDurationWorkflow extends Record<string, unknown> {
  family: "REPLAY_DURATION_WORKFLOW"; mode: ReplayDurationMode; replay_executed: false;
  input_context: Record<string, unknown>; model_context: Record<string, unknown> | null;
  computation: {complete: boolean; error: unknown; error_type: unknown};
  result: {duration_diagnostics: Record<string, unknown> | null; case_duration_seconds: number | string | null; median_duration_seconds: number | string | null};
  presentation_errors: unknown[];
}
export function isReplayDurationWorkflow(value: unknown): value is ReplayDurationWorkflow {
  return object(value) && value.family === "REPLAY_DURATION_WORKFLOW" && REPLAY_DURATION_MODES.includes(value.mode as ReplayDurationMode) && value.replay_executed === false && object(value.input_context) && (value.model_context === null || object(value.model_context)) && object(value.computation) && typeof value.computation.complete === "boolean" && object(value.result) && ["duration_diagnostics", "case_duration_seconds", "median_duration_seconds"].every(key => Object.hasOwn(value.result as object, key)) && Array.isArray(value.presentation_errors) && isOriginalEmbeddingFilterNativeString(value.analysis_raw) && isOriginalEmbeddingFilterNativeString(value.original_log) && (value.original_model === null || isOriginalEmbeddingFilterNativeString(value.original_model)) && object(value.saved_diagnostics_context) && (value.original_saved_diagnostics === null || isOriginalEmbeddingFilterNativeString(value.original_saved_diagnostics));
}
export interface ReplayDurationRun {value: ReplayDurationWorkflow; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[];
  inputs: {log: File; model: File | null; saved: File | null; log_sha256: string; model_sha256: string | null; saved_sha256: string | null}; analysisRaw: string;}
export async function runReplayDurationWorkbench(log: File, model: File | null, form: ReplayDurationForm, signal?: AbortSignal, saved: File | null = null): Promise<ReplayDurationRun> {
  const analysis = replayDurationAnalysis(form.analysisRaw), fields = replayDurationFields(form, log.name);
  if (analysis.mode === "saved_diagnostics" && model === null) throw new Error("Saved diagnostic reanalysis requires its original PNML model.");
  if (saved !== null && (analysis.mode !== "saved_diagnostics" || Object.hasOwn(analysis, "saved_diagnostics"))) throw new Error("Saved diagnostic file input requires saved_diagnostics mode and no inline saved_diagnostics member. Remove the file or edit it inline explicitly.");
  const [logHash, modelHash, savedHash] = await Promise.all([miningSourceHash(log), model ? miningSourceHash(model) : null, saved ? miningSourceHash(saved) : null]);
  signal?.throwIfAborted(); const body = new FormData(); body.append("log", log); if (model) body.append("model", model); if (saved) body.append("saved_diagnostics_file", saved); fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value));
  const path = "/api/analyze-replay-duration-workbench", response = await backendFetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Duration analysis failed (${response.status}).`);
  if (!isReplayDurationWorkflow(value)) throw new Error("The server did not return a complete native duration analysis workflow.");
  const actual = originalEmbeddingFilterStringBytes(value.analysis_raw as Parameters<typeof originalEmbeddingFilterStringBytes>[0]), expected = nativeStringBytes(form.analysisRaw);
  if (value.mode !== analysis.mode || actual.length !== expected.length || actual.some((byte, index) => byte !== expected[index]) || value.input_context.dataset !== log.name || BigInt(String(nativeCell(value.input_context, "log_bytes"))) !== BigInt(log.size) || value.input_context.model !== (model?.name ?? null) || BigInt(String(nativeCell(value.input_context, "model_bytes"))) !== BigInt(model?.size ?? 0)) throw new Error("The duration result does not match the exact submitted analysis and original Files.");
  for (const [file, carrier] of [[log, value.original_log], [model, value.original_model], [saved, value.original_saved_diagnostics]] as const) {
    if (file === null) {if (carrier !== null) throw new Error("The duration response introduced an unsubmitted original file."); continue;}
    if (!isOriginalEmbeddingFilterNativeString(carrier)) throw new Error("The duration response lost an original file byte carrier.");
    const bytes = originalEmbeddingFilterStringBytes(carrier), sourceBytes = new Uint8Array(await file.arrayBuffer());
    if (bytes.length !== sourceBytes.length || bytes.some((byte, index) => byte !== sourceBytes[index])) throw new Error("The duration response changed original file bytes.");
  }
  const savedContext = value.saved_diagnostics_context as Record<string, unknown>;
  if (saved && (savedContext.source !== "uploaded_file" || savedContext.name !== saved.name || BigInt(String(nativeCell(savedContext, "bytes"))) !== BigInt(saved.size) || savedContext.input_shape !== (analysis.saved_input_shape ?? "native_rows"))) throw new Error("The duration response does not retain the submitted saved diagnostic File identity and shape.");
  signal?.throwIfAborted();
  const tables = activeMiningSourceTables(value); tables.push({id: "request", title: "Submitted duration analysis and source configuration", columns: ["field", "value"], rows: fields});
  return {value, raw: nativeResponseJson(value), fields, tables, inputs: {log, model, saved, log_sha256: logHash, model_sha256: modelHash, saved_sha256: savedHash}, analysisRaw: form.analysisRaw};
}
export function replayDurationReport(run: ReplayDurationRun): string {
  const metadata = (file: File | null, sha256: string | null) => file ? {name: file.name, size: file.size, last_modified: file.lastModified, sha256} : null;
  return nativeJson({family: "REPLAY_DURATION_WORKFLOW_REPORT", request: nativeResponseRequest(run.value), analysis_raw: run.analysisRaw, inputs: {log: metadata(run.inputs.log, run.inputs.log_sha256), model: metadata(run.inputs.model, run.inputs.model_sha256), saved_diagnostics: metadata(run.inputs.saved, run.inputs.saved_sha256)}, native_result: run.value}, 2);
}
