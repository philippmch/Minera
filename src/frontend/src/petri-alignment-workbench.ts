import {activeMiningSourceTables} from "./active-mining-source";
import {isWorkflowInputTable, type WorkflowInputTable} from "./workflow-input-workbench";
import type { Alignments, AlignmentVariant } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "./native-response";
import { type DiagnosticTable } from "./model-diagnostics";
import { tokenReplayInputFields, TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS, type TokenReplayAdapter } from "./token-replay-workbench";

export const PETRI_ALIGNMENT_VARIANTS = [
  "state_equation_a_star", "dijkstra_no_heuristics", "dijkstra_less_memory", "discounted_a_star",
  "generator_dijkstra_no_heuristics", "generator_dijkstra_less_memory",
] as const satisfies readonly AlignmentVariant[];
export const PETRI_ALIGNMENT_XES_VARIANTS = ["iterparse", "line_by_line", "iterparse_20", "iterparse_mem_compressed", "rustxes", "chunk_regex"] as const;
export type PetriAlignmentAdapter = TokenReplayAdapter;
export interface PetriAlignmentWorkbenchForm {
  variant: AlignmentVariant;
  adapter: PetriAlignmentAdapter;
  inputFormat?: "auto" | "table";
  miningSelectors?: Partial<Record<"case_id_key" | "activity_key" | "timestamp_key" | "resource_key" | "lifecycle_key", string>>;
  importFields: Record<string, string>;
  alignmentFields: Record<string, string>;
}
/** The kind and scope govern both editable controls and request serialization. */
export const PETRI_ALIGNMENT_CONTROLS = [
  ["alignment_activity_attribute", "Alignment activity attribute", "text", "all"],
  ["alignment_case_id_attribute", "Alignment case attribute (flat inputs)", "text", "all"],
  ["alignment_max_rows_to_materialize", "Alignment event cap", "integer", "all"],
  ["max_states", "Search state cap", "integer", "all"],
  ["max_align_time", "Overall alignment time limit (seconds)", "number", "all"],
  ["max_align_time_trace", "Per-trace alignment time limit (seconds)", "number", "all"],
  ["max_alignments", "Maximum optimal alignments", "integer", "generator"],
  ["exponent", "Discount exponent", "number", "discounted"],
  ["synchronous_dijkstra", "Synchronous Dijkstra", "boolean", "discounted"],
  ["cost_profile", "Cost profile", "profile", "all"],
  ["trace_cost", "Log move cost", "number", "all"],
  ["model_cost", "Visible model move cost", "number", "all"],
  ["silent_model_cost", "Silent model move cost", "number", "all"],
  ["sync_cost", "Synchronous move cost", "number", "all"],
  ["alignment_trace_costs", "Log costs by activity (JSON object)", "map", "all"],
  ["alignment_model_costs", "Model costs by activity (JSON object)", "map", "all"],
  ["alignment_sync_costs", "Synchronous costs by activity (JSON object)", "map", "all"],
  ["alignment_trace_costs_by_position", "Log costs by event position (JSON array)", "array", "all"],
  ["alignment_model_costs_by_transition", "Model costs by transition index (JSON array)", "array", "all"],
  ["alignment_sync_costs_by_transition", "Synchronous costs by transition index (JSON array)", "array", "all"],
  ["alignment_initial_marking", "Initial marking (comma-separated place tokens)", "marking", "all"],
  ["alignment_final_marking", "Final marking (comma-separated place tokens)", "marking", "all"],
] as const;
export const defaultPetriAlignmentWorkbenchForm = (): PetriAlignmentWorkbenchForm => ({
  variant: "dijkstra_less_memory", adapter: "log", importFields: {}, alignmentFields: {},
});
export function petriAlignmentControlActive(scope: typeof PETRI_ALIGNMENT_CONTROLS[number][3], variant: AlignmentVariant): boolean {
  return scope === "all" || (scope === "generator" && variant.startsWith("generator_")) || (scope === "discounted" && variant === "discounted_a_star");
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const index = (value: unknown): value is number => typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
const integer = (holder: object, key: string | number): boolean => {
  const value = nativeCell(holder, key);
  return typeof value === "string" ? /^-?\d+$/.test(value) : typeof value === "number" && Number.isFinite(value) && Number.isInteger(value);
};
const numeric = (holder: object, key: string | number): boolean => {
  const value = nativeCell(holder, key);
  return typeof value === "string" ? /^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?$/.test(value) : typeof value === "number" && Number.isFinite(value);
};
const supportedVariant = (value: unknown): value is AlignmentVariant => PETRI_ALIGNMENT_VARIANTS.some(variant => value === variant);
export function petriAlignmentWorkbenchFields(form: PetriAlignmentWorkbenchForm, logName: string): Array<[string, string]> {
  if (!supportedVariant(form.variant)) throw new Error("Select a supported Petri alignment variant.");
  if (!["log", "stream", "dataframe", "lazyframe"].includes(form.adapter)) throw new Error("Select a supported alignment input shape.");
  const fields: Array<[string, string]> = [["alignment_variant", form.variant], ["alignment_input_adapter", form.adapter]];
  fields.push(...tokenReplayInputFields(form));
  if (form.inputFormat !== "table") {
  for (const [key] of TOKEN_REPLAY_IMPORT_CONTROLS) {
    const value = form.importFields[key];
    if (value !== undefined && value !== "") fields.push([key, value]);
  }
  for (const [key] of TOKEN_REPLAY_XES_BOOLEAN_CONTROLS) {
    const value = form.importFields[key];
    if (!value) continue;
    if (!["true", "false"].includes(value)) throw new Error(`${key} must be true or false.`);
    fields.push([key, value]);
  }
  if (form.importFields.xes_variant) {
    if (!PETRI_ALIGNMENT_XES_VARIANTS.some(value => value === form.importFields.xes_variant)) throw new Error("Select a supported XES importer.");
    fields.push(["xes_variant", form.importFields.xes_variant]);
  }
  const delimiter = form.importFields.delimiter || (/\.tsv$/i.test(logName) ? "\t" : "");
  if (delimiter) {
    if (delimiter.length !== 1) throw new Error("The CSV delimiter must be one character.");
    fields.push(["delimiter", delimiter]);
  }
  }
  for (const [key, label, kind, scope] of PETRI_ALIGNMENT_CONTROLS) {
    if (!petriAlignmentControlActive(scope, form.variant)) continue;
    const value = form.alignmentFields[key];
    if (value === undefined || value === "") continue;
    if (kind === "boolean" && !["true", "false"].includes(value)) throw new Error(`${label} must be true or false.`);
    if (kind === "profile" && !["legacy", "standard"].includes(value)) throw new Error("Select a supported alignment cost profile.");
    if (kind === "integer" && !/^\d+$/.test(value.trim())) throw new Error(`${label} must be a non-negative integer.`);
    if (kind === "map" || kind === "array") {
      let parsed: unknown;
      try { parsed = parseNativeJson(value); } catch { throw new Error(`${label} must be valid JSON.`); }
      if (kind === "map" ? !object(parsed) : !Array.isArray(parsed)) throw new Error(`${label} must be a JSON ${kind === "map" ? "object" : "array"}.`);
    }
    // Keep original numeric and JSON text; native parsing decides supported costs and bounds.
    fields.push([key, value]);
  }
  return fields;
}

export interface PetriAlignmentCompleteMove {
  type: "sync" | "log" | "model";
  activity_name: string | null;
  transition: number | null;
}
export interface PetriAlignmentWorkflow {
  family: "PETRI_ALIGNMENT_WORKFLOW";
  input_before?: WorkflowInputTable | null; input_after?: WorkflowInputTable | null; prepared_log?: Record<string, unknown>;
  input_context: {
    adapter: PetriAlignmentAdapter; origin: "uploaded_csv_rows" | "uploaded_grouped_log" | "uploaded_typed_table"; dataset: string; model: string;
    log_bytes: number; model_bytes: number; source_case_count: number; source_event_count: number;
    evaluated_case_count: number; evaluated_event_count: number; activity_attribute: string; case_id_attribute: string;
  };
  model_context: {
    places: Array<{index: number; id: string; pnml_initial_tokens: number; pnml_final_tokens: number;
      effective_initial_tokens: number; effective_final_tokens: number; initial_key: boolean; final_key: boolean}>;
    transitions: Array<{index: number; id: string; label: string | null}>;
  };
  activities: string[];
  result: Alignments;
  variant_details: Array<{variant_index: number; worst_cost: number; complete_moves: PetriAlignmentCompleteMove[];
    projected_model_moves: Array<{activity_name: string | null; transition: number | null; marking_before: number[]; marking_after: number[]}>;
    optimal_alignments: PetriAlignmentCompleteMove[][]}>;
  trace_groups: Array<{trace_index: number; case_id: string; event_count: number; activities: string[]; variant_index: number | null}>;
}
/** Validate required native containers and index relationships without replacing or rounding their values. */
export function isPetriAlignmentWorkflow(value: unknown): value is PetriAlignmentWorkflow {
  if (!object(value) || value.family !== "PETRI_ALIGNMENT_WORKFLOW" || !object(value.input_context) || !object(value.model_context) || !object(value.result)) return false;
  const input = value.input_context, model = value.model_context, result = value.result;
  if (["input_before", "input_after"].some(key => value[key] !== undefined && value[key] !== null && !isWorkflowInputTable(value[key])) || value.prepared_log !== undefined && !object(value.prepared_log)) return false;
  if (!["log", "stream", "dataframe", "lazyframe"].includes(String(input.adapter)) || !["uploaded_csv_rows", "uploaded_grouped_log", "uploaded_typed_table"].includes(String(input.origin)) ||
    !["dataset", "model", "activity_attribute", "case_id_attribute"].every(key => typeof input[key] === "string") ||
    !["log_bytes", "model_bytes", "source_case_count", "source_event_count", "evaluated_case_count", "evaluated_event_count"].every(key => index(input[key]))) return false;
  if (!Array.isArray(model.places) || !model.places.every((row, i) => object(row) && row.index === i && typeof row.id === "string" &&
    ["pnml_initial_tokens", "pnml_final_tokens", "effective_initial_tokens", "effective_final_tokens"].every(key => integer(row, key)) &&
    typeof row.initial_key === "boolean" && typeof row.final_key === "boolean") ||
    !Array.isArray(model.transitions) || !model.transitions.every((row, i) => object(row) && row.index === i && typeof row.id === "string" && (row.label === null || typeof row.label === "string"))) return false;
  if (!Array.isArray(value.activities) || !value.activities.every(activity => typeof activity === "string")) return false;
  const activities = value.activities, transitions = model.transitions, places = model.places;
  const activityIndex = (activity: unknown) => activity === null || (index(activity) && activity < activities.length);
  const transitionIndex = (transition: unknown) => transition === null || (index(transition) && transition < transitions.length);
  const move = (item: unknown): boolean => object(item) && ["sync", "log", "model"].includes(String(item.type)) && activityIndex(item.activity) &&
    (item.activity_name === undefined || item.activity_name === null || typeof item.activity_name === "string") && (item.silent === undefined || typeof item.silent === "boolean");
  const completeMove = (item: unknown): boolean => object(item) && ["sync", "log", "model"].includes(String(item.type)) &&
    (item.activity_name === null || typeof item.activity_name === "string") && transitionIndex(item.transition) &&
    (item.type === "log" ? item.transition === null : item.transition !== null);
  const marking = (items: unknown): boolean => Array.isArray(items) && items.length === places.length && items.every((_, i) => integer(items, i));
  const diagnostics = (item: unknown): boolean => item === undefined || (object(item) && Object.keys(item).every(key => integer(item, key)));
  if (typeof result.computed !== "boolean" || typeof result.exhausted !== "boolean" || !numeric(result, "fitness") ||
    (result.precision !== undefined && !numeric(result, "precision")) || !index(result.fitting_traces) || !index(result.trace_count) ||
    result.fitting_traces > result.trace_count || !numeric(result, "total_cost") || !supportedVariant(result.variant) ||
    (result.alignment_variant !== undefined && result.alignment_variant !== result.variant) || typeof result.activity_attribute !== "string" ||
    result.activity_attribute !== input.activity_attribute || !diagnostics(result.diagnostics) || !Array.isArray(result.variants)) return false;
  const variants = result.variants;
  if (!variants.every(row => object(row) && Array.isArray(row.activities) && row.activities.every(activityIndex) && index(row.count) &&
    (row.activity_names === undefined || (Array.isArray(row.activity_names) && row.activity_names.length === row.activities.length && row.activity_names.every(name => typeof name === "string"))) &&
    Array.isArray(row.moves) && row.moves.every(move) && numeric(row, "cost") && numeric(row, "fitness") && diagnostics(row.diagnostics) &&
    ["alternatives", "optimal_alignments"].every(key => row[key] === undefined || (Array.isArray(row[key]) && (row[key] as unknown[]).every(items => Array.isArray(items) && items.every(move)))))) return false;
  if (!Array.isArray(value.variant_details) || !value.variant_details.every((row, i) => object(row) && row.variant_index === i && i < variants.length && numeric(row, "worst_cost") &&
    Array.isArray(row.complete_moves) && row.complete_moves.every(completeMove) && Array.isArray(row.optimal_alignments) && row.optimal_alignments.every(items => Array.isArray(items) && items.every(completeMove)) &&
    Array.isArray(row.projected_model_moves) && row.projected_model_moves.every(item => object(item) && (item.activity_name === null || typeof item.activity_name === "string") &&
      transitionIndex(item.transition) && marking(item.marking_before) && marking(item.marking_after)))) return false;
  if (!Array.isArray(value.trace_groups) || value.trace_groups.length !== input.evaluated_case_count || !value.trace_groups.every((row, i) => object(row) && row.trace_index === i &&
    typeof row.case_id === "string" && index(row.event_count) && Array.isArray(row.activities) && row.activities.length === row.event_count && row.activities.every(name => typeof name === "string") &&
    (row.variant_index === null || (index(row.variant_index) && row.variant_index < variants.length)))) return false;
  if (value.trace_groups.reduce((count, row) => count + (row as Record<string, number>).event_count, 0) !== input.evaluated_event_count) return false;
  if (result.computed && (result.trace_count !== input.evaluated_case_count || value.variant_details.length !== variants.length ||
    variants.reduce((count, row) => count + (row as Record<string, number>).count, 0) !== result.trace_count || value.trace_groups.some(row => (row as Record<string, unknown>).variant_index === null))) return false;
  return true;
}

export function petriAlignmentWorkbenchTables(value: PetriAlignmentWorkflow, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = activeMiningSourceTables(value);
  const moveRecords: unknown[] = [], moveRows: unknown[][] = [], alternativeRecords: unknown[] = [], alternativeRows: unknown[][] = [];
  const projectionRecords: unknown[] = [], projectionRows: unknown[][] = [];
  for (const detail of value.variant_details) {
    detail.complete_moves.forEach((move, move_index) => {
      moveRecords.push({variant_index: detail.variant_index, move_index, move});
      moveRows.push([detail.variant_index, move_index, move.type, move.activity_name, move.transition, move]);
    });
    detail.optimal_alignments.forEach((moves, alignment_index) => {
      alternativeRecords.push({variant_index: detail.variant_index, source: "variant_details", alignment_index, moves});
      alternativeRows.push([detail.variant_index, "variant_details", alignment_index, moves.length, moves]);
    });
    detail.projected_model_moves.forEach((move, move_index) => {
      projectionRecords.push({variant_index: detail.variant_index, move_index, move});
      projectionRows.push([detail.variant_index, move_index, move.activity_name, move.transition, move.marking_before, move.marking_after, move]);
    });
  }
  value.result.variants.forEach((variant, variant_index) => {
    for (const source of ["alternatives", "optimal_alignments"] as const) variant[source]?.forEach((moves, alignment_index) => {
      alternativeRecords.push({variant_index, source: `result.${source}`, alignment_index, moves});
      alternativeRows.push([variant_index, `result.${source}`, alignment_index, moves.length, moves]);
    });
  });
  tables.push({id: "complete_moves", title: "Every complete alignment move", columns: ["variant_index", "move_index", "type", "activity_name", "transition", "complete_move"], rows: moveRows, records: moveRecords});
  tables.push({id: "optimal_alignments", title: "Every returned optimal alignment (including empty alternatives)", columns: ["variant_index", "source", "alignment_index", "move_count", "complete_moves"], rows: alternativeRows, records: alternativeRecords});
  tables.push({id: "projected_model_moves", title: "Every model projection and visited marking", columns: ["variant_index", "move_index", "activity_name", "transition", "marking_before", "marking_after", "complete_projection"], rows: projectionRows, records: projectionRecords});
  const submitted = fields ?? nativeResponseRequest(value)?.fields;
  if (submitted) tables.push({id: "request", title: "Submitted alignment options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, value]) => ({field, value}))});
  return tables;
}
export interface PetriAlignmentWorkbenchRun {
  value: PetriAlignmentWorkflow; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[];
  inputs: {log: File; model: File; log_sha256: string; model_sha256: string};
}
async function fileHash(file: File): Promise<string> {
  return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");
}
export async function runPetriAlignmentWorkbench(log: File, model: File, form: PetriAlignmentWorkbenchForm, signal?: AbortSignal): Promise<PetriAlignmentWorkbenchRun> {
  const fields = petriAlignmentWorkbenchFields(form, log.name), submitted = Object.fromEntries(fields);
  const [logHash, modelHash] = await Promise.all([fileHash(log), fileHash(model)]);
  signal?.throwIfAborted();
  const body = new FormData(); body.append("log", log); body.append("model", model);
  fields.forEach(([key, value]) => body.append(key, value));
  const path = "/api/align-petri-workbench";
  const response = await fetch(path, {method: "POST", body, signal});
  const value = await readNativeResponse(response, {path, fields});
  signal?.throwIfAborted();
  if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Petri alignment failed (${response.status}).`);
  if (!isPetriAlignmentWorkflow(value)) throw new Error("The server did not return a valid native Petri alignment workflow.");
  const input = value.input_context;
  if (value.result.variant !== submitted.alignment_variant || input.adapter !== submitted.alignment_input_adapter || input.dataset !== log.name || input.model !== model.name ||
    input.log_bytes !== log.size || input.model_bytes !== model.size ||
    (submitted.alignment_activity_attribute !== undefined && input.activity_attribute !== submitted.alignment_activity_attribute.trim()) ||
    (submitted.alignment_case_id_attribute !== undefined && input.case_id_attribute !== submitted.alignment_case_id_attribute.trim())) throw new Error("The alignment response does not match the submitted inputs and options.");
  if (form.inputFormat === "table" && input.origin !== "uploaded_typed_table") throw new Error("The native result did not retain the uploaded typed source.");
  return {value, raw: nativeResponseJson(value), fields, tables: petriAlignmentWorkbenchTables(value, fields), inputs: {log, model, log_sha256: logHash, model_sha256: modelHash}};
}
export function petriAlignmentWorkbenchReport(run: PetriAlignmentWorkbenchRun): string {
  const metadata = (file: File, sha256: string) => ({name: file.name, size: file.size, last_modified: file.lastModified, sha256});
  return nativeJson({family: "PETRI_ALIGNMENT_WORKFLOW_REPORT", request: nativeResponseRequest(run.value) ?? {path: "/api/align-petri-workbench", fields: run.fields},
    inputs: {log: metadata(run.inputs.log, run.inputs.log_sha256), model: metadata(run.inputs.model, run.inputs.model_sha256)}, native_result: run.value}, 2);
}
export function petriAlignmentResultSummary(result: Alignments): string {
  return result.computed ? `Fitness ${nativeCell(result, "fitness")} · ${result.precision === undefined ? "Precision absent" : `Precision ${nativeCell(result, "precision")}`} · ${result.fitting_traces}/${result.trace_count} fitting traces · total cost ${nativeCell(result, "total_cost")}`
    : `Alignment was not computed${result.exhausted ? ": the search bound was reached" : ""}. Fitness, precision and cost fields are not valid measurements for this result.`;
}
