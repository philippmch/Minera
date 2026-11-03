import {miningSourceHash} from "./active-mining-source";
import { backendFetch } from "./backend-fetch";
import {defaultGraphAlignmentForm, graphAlignmentFields, graphAlignmentReport, type GraphAlignmentForm, type GraphAlignmentRun} from "./graph-alignment-workbench";
import {nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString, originalEmbeddingFilterStringBytes, type OriginalEmbeddingFilterNativeString as NativeString} from "./original-embedding-filter-workbench";

export const SAVED_DFG_INPUT_SHAPES = ["native_result", "workbench", "report"] as const;
export interface SavedDfgProjectionForm {
  source: GraphAlignmentForm; inputFormat: "auto" | "table" | "raw_log";
  savedInputShape: typeof SAVED_DFG_INPUT_SHAPES[number]; savedJson?: string; selectorsJson: string;
}
export const defaultSavedDfgProjectionForm = (): SavedDfgProjectionForm => ({source: {...defaultGraphAlignmentForm(), projection: "both"}, inputFormat: "auto", savedInputShape: "native_result", selectorsJson: ""});
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
function unsigned(holder: object, key: string | number): boolean {const value = (holder as Record<string, unknown>)[String(key)], raw = String(nativeCell(holder, key)); return (typeof value === "number" || typeof value === "string") && /^\d+$/.test(raw) && BigInt(raw) <= 18446744073709551615n;}
const double = (value: unknown) => typeof value === "number" && Number.isFinite(value) || typeof value === "string" && ["NaN", "INF", "-INF"].includes(value);
export function isSavedDfgResult(value: unknown): value is Record<string, unknown> {
  return object(value) && unsigned(value, "unique_variants") && Array.isArray(value.alignments) && value.alignments.every(trace => object(trace) && typeof trace.computed === "boolean" && ["cost", "internal_cost", "fitness", "best_worst_cost"].every(key => double(trace[key])) && ["visited_states", "closed", "closed_states"].every(key => unsigned(trace, key)) && Array.isArray(trace.alignment) && trace.alignment.every(move => object(move) && isOriginalEmbeddingFilterNativeString(move.log) && isOriginalEmbeddingFilterNativeString(move.model)));
}
export function savedDfgProjectionFields(form: SavedDfgProjectionForm, inputName: string): Array<[string, string]> {
  if (!["auto", "table", "raw_log"].includes(form.inputFormat) || !SAVED_DFG_INPUT_SHAPES.includes(form.savedInputShape)) throw new Error("Choose a supported original input and saved result shape.");
  const source = {...form.source, mode: "DFG_CLASSIC" as const, inputShape: "log" as const, inputFormat: form.inputFormat === "raw_log" ? "auto" as const : form.inputFormat};
  const fields = graphAlignmentFields(source, inputName).filter(([key]) => !(form.inputFormat === "raw_log" && key === "input_format") && !(form.selectorsJson !== "" && key === "alignment_selectors_json"));
  if (form.inputFormat === "raw_log") fields.push(["input_format", "raw_log"]);
  fields.push(["operation", "project_saved_dfg"], ["saved_input_shape", form.savedInputShape]);
  if (form.selectorsJson !== "") {
    const selectors = parseNativeJson(form.selectorsJson);
    if (!object(selectors) || Object.entries(selectors).some(([key, value]) => !["alignment_case_id_attribute", "dfg_activity_attribute", "dfg_timestamp_attribute"].includes(key) || !isOriginalEmbeddingFilterNativeString(value))) throw new Error("Exact selectors must be a JSON object of literal NativeString selectors.");
    fields.push(["alignment_selectors_json", form.selectorsJson]);
  }
  if (form.savedJson !== undefined) {parseNativeJson(form.savedJson); fields.push(["saved_result_json", form.savedJson]);}
  return fields;
}
export interface SavedDfgProjectionValue extends Record<string, unknown> {
  family: "GRAPH_ALIGNMENT_WORKBENCH"; operation: "project_saved_dfg"; alignment_executed: false; supplied_result: true;
  input_context: Record<string, unknown>; model_context: Record<string, unknown>; saved_result_context: Record<string, unknown>;
  result: Record<string, unknown> | null; projection: unknown; projected_log: unknown; projected_raw_log: unknown;
  original_input: NativeString; original_model: NativeString; original_saved_result: NativeString;
  projected_xes: NativeString | null; model_export: NativeString | null;
}
export function isSavedDfgProjectionValue(value: unknown): value is SavedDfgProjectionValue {
  return object(value) && value.family === "GRAPH_ALIGNMENT_WORKBENCH" && value.operation === "project_saved_dfg" && value.alignment_executed === false && value.supplied_result === true && object(value.input_context) && object(value.model_context) && object(value.saved_result_context) && (value.result === null || isSavedDfgResult(value.result)) && ["original_input", "original_model", "original_saved_result"].every(key => isOriginalEmbeddingFilterNativeString(value[key])) && ["projected_xes", "model_export"].every(key => value[key] === null || isOriginalEmbeddingFilterNativeString(value[key])) && ["projection_error", "metrics_error", "log_projection_error", "projected_log_error", "projected_xes_error", "model_export_error"].every(key => value[key] === null || isOriginalEmbeddingFilterNativeString(value[key]));
}
export interface SavedDfgProjectionRun {value: SavedDfgProjectionValue; raw: string; fields: Array<[string, string]>; http_status: number; tables: DiagnosticTable[]; inputs: {input: File; model: File; saved: File | null; input_sha256: string; model_sha256: string; saved_sha256: string | null}; savedJson?: string;}
function equalBytes(a: Uint8Array, b: Uint8Array): boolean {return a.length === b.length && a.every((byte, index) => byte === b[index]);}
function sameSavedResult(supplied: Record<string, unknown>, canonical: Record<string, unknown>): boolean {
  if (BigInt(String(nativeCell(supplied, "unique_variants"))) !== BigInt(String(nativeCell(canonical, "unique_variants")))) return false;
  const before = supplied.alignments as Record<string, unknown>[], after = canonical.alignments as Record<string, unknown>[];
  return before.length === after.length && before.every((trace, index) => {
    const result = after[index];
    if (trace.computed !== result.computed || ["cost", "internal_cost", "fitness", "best_worst_cost"].some(key => !Object.is(trace[key], result[key])) || ["visited_states", "closed", "closed_states"].some(key => BigInt(String(nativeCell(trace, key))) !== BigInt(String(nativeCell(result, key))))) return false;
    const moves = trace.alignment as {log: NativeString; model: NativeString}[], returned = result.alignment as {log: NativeString; model: NativeString}[];
    return moves.length === returned.length && moves.every((move, at) => ["log", "model"].every(key => equalBytes(originalEmbeddingFilterStringBytes(move[key as "log" | "model"]), originalEmbeddingFilterStringBytes(returned[at][key as "log" | "model"]))));
  });
}
export async function runSavedDfgProjectionWorkbench(input: File, model: File, form: SavedDfgProjectionForm, signal?: AbortSignal, saved: File | null = null): Promise<SavedDfgProjectionRun> {
  if ((saved !== null) === (form.savedJson !== undefined)) throw new Error("Supply one saved result File or an inline saved result, never both. Edit the File inline explicitly to replace it.");
  const fields = savedDfgProjectionFields(form, input.name), [inputHash, modelHash, savedHash] = await Promise.all([miningSourceHash(input), miningSourceHash(model), saved ? miningSourceHash(saved) : null]); signal?.throwIfAborted();
  const body = new FormData(); body.append("input_file", input); body.append("model_file", model); if (saved) body.append("saved_result_file", saved); fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value));
  const path = "/api/align-graph-workbench", response = await backendFetch(path, {method: "POST", body, signal}), value = await readNativeResponse(response, {path, fields}); signal?.throwIfAborted();
  if (!isSavedDfgProjectionValue(value)) throw new Error(object(value) && typeof value.error === "string" ? value.error : "The server did not return a complete saved DFG projection envelope.");
  if (value.input_context.dataset !== input.name || value.model_context.dataset !== model.name || String(nativeCell(value.input_context, "original_input_bytes")) !== String(input.size) || String(nativeCell(value.model_context, "original_input_bytes")) !== String(model.size) || value.input_context.format !== form.inputFormat || value.model_context.format !== form.source.modelFormat) throw new Error("The projection response does not match the original input and model Files.");
  for (const [file, carrier] of [[input, value.original_input], [model, value.original_model]] as const) if (!equalBytes(new Uint8Array(await file.arrayBuffer()), originalEmbeddingFilterStringBytes(carrier))) throw new Error("The projection response changed original input or model bytes.");
  const savedBytes = saved ? new Uint8Array(await saved.arrayBuffer()) : nativeStringBytes(form.savedJson!);
  if (!equalBytes(savedBytes, originalEmbeddingFilterStringBytes(value.original_saved_result))) throw new Error("The projection response changed the supplied saved result bytes.");
  const context = value.saved_result_context;
  if (context.input_shape !== form.savedInputShape || context.source !== (saved ? "uploaded_file" : "inline") || (saved && context.name !== saved.name) || String(nativeCell(context, "bytes")) !== String(savedBytes.length)) throw new Error("The projection response does not retain the supplied saved result identity.");
  if (value.result !== null) {
    const uploaded = parseNativeJson(new TextDecoder("utf-8", {fatal: true}).decode(savedBytes));
    const envelope = form.savedInputShape === "report" && object(uploaded) ? uploaded.native_result : uploaded;
    const supplied = form.savedInputShape === "native_result" ? uploaded : object(envelope) ? envelope.result : null;
    if (!isSavedDfgResult(supplied) || !sameSavedResult(supplied, value.result)) throw new Error("The projection response changed supplied alignment routes, scores, flags or counters.");
  }
  const submittedSelectors = fields.find(([key]) => key === "alignment_selectors_json");
  if (submittedSelectors && value.executed === true) for (const [key, expected] of Object.entries(parseNativeJson(submittedSelectors[1]) as Record<string, NativeString>)) {
    const actual = value.input_context[key === "alignment_case_id_attribute" ? "case_id_attribute" : key === "dfg_activity_attribute" ? "activity_attribute" : "timestamp_attribute"];
    if (!isOriginalEmbeddingFilterNativeString(actual) || !equalBytes(originalEmbeddingFilterStringBytes(expected), originalEmbeddingFilterStringBytes(actual))) throw new Error("The projection response changed an exact submitted selector.");
  }
  signal?.throwIfAborted();
  const tables = nativeObjectTables(value); tables.push({id: "request", title: "Exact saved projection request", columns: ["field", "value"], rows: fields});
  return {value, raw: nativeResponseJson(value), fields, http_status: response.status, tables, inputs: {input, model, saved, input_sha256: inputHash, model_sha256: modelHash, saved_sha256: savedHash}, savedJson: form.savedJson};
}
/** Original File objects and the entire captured workbench remain the new operation's inputs. */
export function savedDfgProjectionReuse(run: GraphAlignmentRun, shape: "workbench" | "report" = "workbench"): {input: File; model: File; saved: File; form: SavedDfgProjectionForm} {
  if (run.value.mode !== "DFG_CLASSIC" || run.value.input_shape !== "log" || !run.value.result || !("alignments" in run.value.result)) throw new Error("Choose a completed grouped-log DFG alignment result.");
  const form = defaultSavedDfgProjectionForm(); form.savedInputShape = shape;
  for (const [key, value] of run.fields) {
    if (key === "input_format") {if (value !== "auto" && value !== "table") throw new Error("Saved projection requires an original grouped log."); form.inputFormat = value;}
    else if (key === "model_format") form.source.modelFormat = value as GraphAlignmentForm["modelFormat"];
    else if (key === "alignment_selectors_json") form.selectorsJson = value;
    else if (key.startsWith("dfg_") || key === "alignment_case_id_attribute") form.source.fields[key] = value;
    else if (!["mode", "input_shape", "trace_index", "projection", "operation"].includes(key)) form.source.importFields[key] = value;
  }
  return {input: run.inputs.input, model: run.inputs.model, saved: new File([nativeStringBytes(shape === "report" ? graphAlignmentReport(run) : run.raw)], `minera-completed-dfg-alignment-${shape}.json`, {type: "application/json"}), form};
}
export function savedDfgProjectionReport(run: SavedDfgProjectionRun): string {
  const metadata = (file: File | null, hash: string | null) => file ? {name: file.name, size: file.size, last_modified: file.lastModified, sha256: hash} : null;
  return nativeJson({family: "SAVED_DFG_PROJECTION_REPORT", request: nativeResponseRequest(run.value), http_status: run.http_status, saved_inline_json: run.savedJson, inputs: {input: metadata(run.inputs.input, run.inputs.input_sha256), model: metadata(run.inputs.model, run.inputs.model_sha256), saved_result: metadata(run.inputs.saved, run.inputs.saved_sha256)}, native_result: run.value}, 2);
}
