import { monteCarloQuery } from "./api";
import { importedModelFetch } from "./imported-model-binding";
import { isLogConversionResult, isMonteCarloResult, type MonteCarloOptions, type MonteCarloResult,
  type LogConversionLog, type PetriNetModel } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import type { LogQuerySchema } from "./log-query";
import type { OcelPreparedTable } from "./ocel-preparation";

export interface SimulationWorkspace {
  family: "SIMULATION_WORKSPACE"; source_id: string; dataset: string; miner: string;
  source: { case_count: number; event_count: number }; result: MonteCarloResult;
  simulated_log: { family: "LOG_CONVERSION"; variant: "TO_EVENT_LOG"; log: LogConversionLog };
  // Null, with the writer's refusal in xes_error, when the simulated log holds
  // text XES cannot carry; the simulation result is kept.
  xes: string | null; xes_error?: string | null; underlying_pnml: string;
  model: PetriNetModel & { activities: string[]; transitions: Array<PetriNetModel["transitions"][number] & { index: number }> };
}
export interface SimulationInputs {
  options: MonteCarloOptions; initial: string; final: string; weights: string;
  durations: string; resources: string; stochasticMap: string;
}
export const defaultSimulationInputs = (): SimulationInputs => ({ options: { simulations: 100, max_steps: 1000, stop_at_final: true }, initial: "", final: "", weights: "", durations: "", resources: "", stochasticMap: "" });
function marking(text: string, name: string): number[] | undefined {
  if (!text.trim()) return undefined;
  const tokens = text.split(",").map(token => token.trim());
  if (tokens.some(token => !/^\d+$/.test(token) || !Number.isSafeInteger(Number(token)) || Number(token) > 0xffffffff))
    throw new Error(name + " must contain comma-separated uint32 token counts.");
  return tokens.map(Number);
}
function jsonInput(text: string, name: string): unknown {
  try { return JSON.parse(text); } catch { throw new Error(name + " must be valid JSON."); }
}
export function simulationOptions(inputs: SimulationInputs): MonteCarloOptions {
  const options: MonteCarloOptions = { ...inputs.options, initial_marking: marking(inputs.initial, "Initial marking"), final_marking: marking(inputs.final, "Final marking") };
  if (inputs.weights.trim()) {
    if (inputs.weights.split(",").some(part => !part.trim())) throw new Error("Transition weights must contain finite numbers.");
    options.transition_weights = inputs.weights.split(",").map(Number);
  }
  if (inputs.durations.trim()) options.transition_durations = jsonInput(inputs.durations, "Transition durations") as MonteCarloOptions["transition_durations"];
  if (inputs.resources.trim()) options.resources_per_place = jsonInput(inputs.resources, "Place resources") as MonteCarloOptions["resources_per_place"];
  if (inputs.stochasticMap.trim()) options.provided_stochastic_map = jsonInput(inputs.stochasticMap, "Stochastic map") as MonteCarloOptions["provided_stochastic_map"];
  // Preserve the existing native serializer's validation and precedence rules.
  monteCarloQuery(options);
  return options;
}
const record = (value: unknown): value is Record<string, unknown> => !!value && typeof value === "object" && !Array.isArray(value);
export function validSimulationWorkspace(value: unknown): value is SimulationWorkspace {
  if (!record(value) || value.family !== "SIMULATION_WORKSPACE" || typeof value.source_id !== "string" || !value.source_id || typeof value.dataset !== "string" || typeof value.miner !== "string" || !record(value.source) ||
      !Number.isSafeInteger(value.source.case_count) || (value.source.case_count as number) < 0 || !Number.isSafeInteger(value.source.event_count) || (value.source.event_count as number) < 0 || !isMonteCarloResult(value.result) || !isLogConversionResult(value.simulated_log) || value.simulated_log.variant !== "TO_EVENT_LOG" ||
      !(typeof value.xes === "string" && value.xes.includes("<log") && (value.xes_error === undefined || value.xes_error === null) || value.xes === null && typeof value.xes_error === "string") || typeof value.underlying_pnml !== "string" || !value.underlying_pnml.includes("<pnml") || !record(value.model)) return false;
  const model = value.model;
  if (!Number.isSafeInteger(model.place_count) || (model.place_count as number) <= 0 || !Array.isArray(model.activities) || !model.activities.every(name => typeof name === "string") || !Array.isArray(model.transitions)) return false;
  const place = (n: unknown) => Number.isSafeInteger(n) && (n as number) >= 0 && (n as number) < (model.place_count as number);
  if (!place(model.source) || !place(model.sink) || !model.transitions.every((t, i) => record(t) && t.index === i && (t.activity === null || Number.isSafeInteger(t.activity) && (t.activity as number) >= 0 && (t.activity as number) < (model.activities as unknown[]).length) &&
      [t.inputs, t.outputs, t.reset_inputs, t.inhibitor_inputs].every(ids => Array.isArray(ids) && ids.every(place)))) return false;
  const log = value.simulated_log.log;
  return log.traces.length === value.result.log.trace_count && log.traces.reduce((count, trace) => count + trace.events.length, 0) === value.result.log.event_count &&
    value.result.output_places_interval_trees.length === model.place_count && value.result.output_transitions_interval_trees.length === model.transitions.length;
}
const table = (columns: string[], count: number, rowAt: (index: number) => unknown[]): OcelPreparedTable => ({ columns, count, rowAt });
const cell = (value: object, key: string | number): unknown => { const item = nativeCell(value, key); return item && typeof item === "object" ? nativeJson(item) : item; };
function properties(value: object): OcelPreparedTable { const keys = Object.keys(value); return table(["field", "value"], keys.length, i => [keys[i], cell(value, keys[i])]); }
export function simulationTables(value: SimulationWorkspace): Record<string, OcelPreparedTable> {
  const result = value.result, log = value.simulated_log.log;
  const tables: Record<string, OcelPreparedTable> = {
    source: properties({ source_id: value.source_id, dataset: value.dataset, miner: value.miner, ...value.source }),
    diagnostics: properties(result.diagnostics), log_summary: properties(result.log),
    visible_durations: table(["generated_trace_index", "case_id", "first_to_last_visible_seconds"], result.diagnostics.cases_ex_time.length, i => [i, log.traces[i]?.case_id, cell(result.diagnostics.cases_ex_time, i)]),
    scheduler_durations: table(["simulation_index", "scheduler_elapsed_seconds"], result.diagnostics.case_execution_seconds.length, i => [i, cell(result.diagnostics.case_execution_seconds, i)]),
    log_metadata: properties({ attributes: log.attributes, metadata: log.metadata }),
    traces: table(["trace_index", "case_id", "native_trace_attributes", "native_trace_properties", "event_count"], log.traces.length, i => { const trace = log.traces[i]; return [i, trace.case_id, nativeJson(trace.attributes), trace.properties === undefined ? undefined : nativeJson(trace.properties), trace.events.length]; }),
    model: properties(value.model),
    transitions: table(["transition_index", "activity_index", "activity", "inputs", "outputs", "reset_inputs", "inhibitor_inputs"], value.model.transitions.length, i => { const t = value.model.transitions[i]; return [cell(t, "index"), cell(t, "activity"), t.activity === null ? null : value.model.activities[t.activity], nativeJson(t.inputs), nativeJson(t.outputs), nativeJson(t.reset_inputs), nativeJson(t.inhibitor_inputs)]; }),
  };
  const events = log.traces.flatMap((trace, ti) => trace.events.map((event, ei) => ({ trace, ti, event, ei })));
  const eventKeys = Array.from(new Set(events.flatMap(({ event }) => Object.keys(event))));
  tables.events = table(["trace_index", "event_index", "case_id", ...eventKeys], events.length, i => { const { trace, ti, event, ei } = events[i]; return [ti, ei, trace.case_id, ...eventKeys.map(key => cell(event, key))]; });
  for (const kind of ["places", "transitions"] as const) {
    const trees = kind === "places" ? result.output_places_interval_trees : result.output_transitions_interval_trees;
    const indexKey = kind === "places" ? "place_index" : "transition_index";
    tables[kind + "_trees"] = table([indexKey, "interval_count"], trees.length, i => [cell(trees[i], indexKey), trees[i].intervals.length]);
    const intervals = trees.flatMap(tree => tree.intervals.map((interval, i) => ({ tree, interval, i })));
    tables[kind + "_intervals"] = table([indexKey, "interval_index", "start_epoch_seconds", "end_epoch_seconds"], intervals.length, i => { const row = intervals[i]; return [cell(row.tree, indexKey), row.i, cell(row.interval, "start"), cell(row.interval, "end")]; });
  }
  return tables;
}
export interface SimulationRun { value: SimulationWorkspace; raw: string; report: string; tables: Record<string, OcelPreparedTable>; }
export async function runSimulationWorkspace(miner: string, inputs: SimulationInputs, schema: LogQuerySchema, signal?: AbortSignal): Promise<SimulationRun> {
  const q = new URLSearchParams(monteCarloQuery(simulationOptions(inputs))); q.set("miner", miner); q.set("source_id", schema.source_id); q.set("include_workspace", "true");
  const response = await importedModelFetch("/api/monte-carlo?" + q, { method: "POST", ...(signal ? { signal } : {}) });
  const raw = await response.text(); let value: unknown;
  try { value = parseNativeJson(raw); } catch { throw new Error("The backend returned invalid simulation JSON."); }
  if (record(value) && "error" in value) throw new Error(String(value.error));
  if (!response.ok) throw new Error("Simulation backend returned " + response.status + ".");
  if (!validSimulationWorkspace(value)) throw new Error("The backend returned an invalid native simulation workspace.");
  if (value.source_id !== schema.source_id || value.miner !== miner || value.source.case_count !== schema.case_count || value.source.event_count !== schema.event_count) throw new Error("Simulation result does not match the selected source and model.");
  const tables = simulationTables(value); const entries = Array.from(q);
  tables.configuration = table(["parameter", "value"], entries.length, i => entries[i]);
  return { value, raw, tables, report: '{"family":"SIMULATION_WORKSPACE_REPORT","request":' + JSON.stringify({ route: "/api/monte-carlo", parameters: Object.fromEntries(q) }) + ',"response":' + raw + '}' };
}
