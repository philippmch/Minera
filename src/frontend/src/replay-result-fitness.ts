import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { nativeResponseJson, readNativeResponse } from "./native-response";
import { nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";

export type ReplayInputKind = "token" | "alignment";
export type ReplayResultVariant = "native" | "auto" | "token_based" | "alignment_based";
export interface ReplayResultFitnessForm { kind: ReplayInputKind; variant: ReplayResultVariant; json: string }
export interface ReplayResultFitnessValue {
  family: "REPLAY_RESULT_FITNESS";
  input_kind: ReplayInputKind;
  submitted_rows: number;
  input: unknown;
  result: {
    miner: string;
    replay_fitness: {
      variant: "auto" | "token_based" | "alignment_based";
      computed: boolean; exhausted: boolean; error: string | null;
      fitness: number | null; log_fitness: number | null;
      average_trace_fitness: number | null;
      percentage_of_fitting_traces: number | null;
      fitting_traces: number; trace_count: number; total_cost: number | null;
    };
    public_result?: unknown;
  };
  token_totals?: Record<string, unknown>;
  transition_frequencies?: unknown[];
}
export interface ReplayResultFitnessRun {
  request: { input_kind: ReplayInputKind; fitness_variant?: string; input_json: string };
  value: ReplayResultFitnessValue; raw: string; tables: DiagnosticTable[];
}
const object = (value: unknown): value is Record<string, unknown> => !!value && typeof value === "object" && !Array.isArray(value);
export const replayResultExample = (kind: ReplayInputKind): string => kind === "token"
  ? '[\n  {"trace_is_fit":true,"trace_fitness":1,"missing_tokens":0,"consumed_tokens":4,"remaining_tokens":0,"produced_tokens":4},\n  {"trace_is_fit":false,"trace_fitness":0.5,"missing_tokens":1,"consumed_tokens":4,"remaining_tokens":1,"produced_tokens":4}\n]'
  : '[\n  {"fitness":1,"cost":0,"best_worst_cost":4},\n  null,\n  {"fitness":0.5,"cost":2,"bwc":4}\n]';
export const defaultReplayResultFitnessForm = (): ReplayResultFitnessForm => ({ kind: "token", variant: "native", json: replayResultExample("token") });
export function replayResultRows(input: unknown, kind: ReplayInputKind): unknown[] {
  if (Array.isArray(input)) return input;
  if (kind === "token" && object(input)) {
    if (Array.isArray(input.trace_diagnostics)) return input.trace_diagnostics;
    if (Array.isArray(input.traces)) return input.traces;
    if (object(input.token_replay)) return replayResultRows(input.token_replay, kind);
    if (object(input.replay_fitness)) return replayResultRows(input.replay_fitness, kind);
  }
  throw new Error(kind === "token" ? "Enter token rows, or an object containing traces or trace_diagnostics." : "Enter an array of alignment replay records.");
}
export function replayResultFitnessRequest(form: ReplayResultFitnessForm) {
  if (!["token", "alignment"].includes(form.kind)) throw new Error("Select a supported replay input kind.");
  if (!["native", "auto", "token_based", "alignment_based"].includes(form.variant)) throw new Error("Select a supported native fitness variant.");
  let input: unknown;
  try { input = parseNativeJson(form.json); } catch { throw new Error("Replay results must be valid JSON."); }
  const rows = replayResultRows(input, form.kind);
  if (rows.some(row => !object(row) && !(form.kind === "alignment" && row === null))) throw new Error("Every replay row must be an object; alignment rows may also be null.");
  return { input_kind: form.kind, ...(form.variant === "native" ? {} : { fitness_variant: form.variant }), input_json: form.json };
}
export function isReplayResultFitnessValue(value: unknown): value is ReplayResultFitnessValue {
  if (!object(value) || value.family !== "REPLAY_RESULT_FITNESS" || !["token", "alignment"].includes(String(value.input_kind)) ||
      !Number.isSafeInteger(value.submitted_rows) || Number(value.submitted_rows) < 0 || !object(value.result) || !object(value.result.replay_fitness)) return false;
  const result = value.result.replay_fitness;
  if (!["auto", "token_based", "alignment_based"].includes(String(result.variant)) || typeof result.computed !== "boolean" || typeof result.exhausted !== "boolean" ||
      !(result.error === null || typeof result.error === "string") || typeof value.result.miner !== "string") return false;
  if (!["fitness", "log_fitness", "average_trace_fitness", "percentage_of_fitting_traces", "total_cost"].every(key => result[key] === null || (typeof result[key] === "number" && Number.isFinite(result[key])))) return false;
  if (!["fitting_traces", "trace_count"].every(key => Number.isSafeInteger(result[key]) && Number(result[key]) >= 0)) return false;
  try { if (replayResultRows(value.input, value.input_kind as ReplayInputKind).length !== value.submitted_rows) return false; } catch { return false; }
  return value.input_kind !== "token" || (object(value.token_totals) && Array.isArray(value.transition_frequencies));
}
export function replayResultFitnessTables(value: ReplayResultFitnessValue): DiagnosticTable[] {
  const tables = nativeObjectTables(value);
  const rows = replayResultRows(value.input, value.input_kind);
  const keys = [...new Set(rows.flatMap(row => object(row) ? Object.keys(row) : []))];
  tables.push({ id: "submitted_rows", title: "Every submitted replay record", columns: ["row_index", "row_type", ...keys],
    rows: rows.map((row, index) => [index, row === null ? "null" : "object", ...keys.map(key => object(row) ? nativeCell(row, key) : null)]), records: rows });
  return tables;
}
export async function runReplayResultFitness(form: ReplayResultFitnessForm, signal?: AbortSignal): Promise<ReplayResultFitnessRun> {
  const request = replayResultFitnessRequest(form);
  const query = new URLSearchParams({ input_kind: request.input_kind });
  if (request.fitness_variant) query.set("fitness_variant", request.fitness_variant);
  const path = `/api/evaluate-replay-results?${query}`;
  const response = await fetch(path, {
    method: "POST", headers: { "Content-Type": "application/json" }, body: request.input_json, signal,
  });
  const value = await readNativeResponse(response, { path });
  if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Replay-result fitness request failed (${response.status}).`);
  if (!isReplayResultFitnessValue(value)) throw new Error("The server did not return a valid native replay-result fitness response.");
  const expected = request.fitness_variant ?? (request.input_kind === "token" ? "token_based" : "alignment_based");
  if (value.input_kind !== request.input_kind || value.result.replay_fitness.variant !== expected ||
      nativeJson(value.input) !== nativeJson(parseNativeJson(request.input_json))) throw new Error("The native replay result does not match the submitted input, method and variant.");
  return { request, value, raw: nativeResponseJson(value) ?? nativeJson(value), tables: replayResultFitnessTables(value) };
}
