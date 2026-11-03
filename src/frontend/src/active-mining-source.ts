import {cloneExactFilterFiles,exactFilterMatchesWireFilename} from "./exact-filter-controls";
import {isMineraExport, type MineraExport} from "./types";
import {isOcelJsonDocument} from "./app-logic";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {isWorkflowInputTable, workflowInputNativeBytes, type WorkflowInputTable} from "./workflow-input-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "./token-replay-workbench";
import type {MineOptions} from "./api";

export const MINING_INPUT_FORMATS = ["auto", "table"] as const;
export const MINING_INPUT_SHAPES = ["log", "stream", "dataframe", "lazyframe"] as const;
export const MINING_SOURCE_SELECTORS = [["case_id_key", "Case identifier", "case:concept:name", "case_id"], ["activity_key", "Activity", "concept:name", "activity"], ["timestamp_key", "Timestamp", "time:timestamp", "timestamp"], ["resource_key", "Resource", "org:resource", "resource"], ["lifecycle_key", "Lifecycle", "lifecycle:transition", "lifecycle"]] as const;
export type MiningInputFormat = typeof MINING_INPUT_FORMATS[number];
export type MiningInputShape = typeof MINING_INPUT_SHAPES[number];
export type MiningSourceSelector = typeof MINING_SOURCE_SELECTORS[number][0];
/** Upload candidates retain omitted native defaults; they are not canonical response tables. */
export interface MiningInputCell {first_class_role?: unknown; null?: unknown; kind?: unknown; value?: unknown; typed_value?: unknown;}
export interface MiningInputTable {columns: string[]; rows: (MiningInputCell | null)[][]; dtypes?: string[]; index?: unknown; categorical_dtypes?: unknown[]; attributes?: MiningInputCell[]; metadata?: Record<string, unknown>;}
export interface MiningInputOptions {miningInputFormat?: MiningInputFormat; miningInputShape?: MiningInputShape; miningSelectors?: Partial<Record<MiningSourceSelector, string>>; miningImportFields?: Record<string, string>; miningRequestId?: string;}
export interface MiningSourceForm {format: MiningInputFormat; shape: MiningInputShape; selectors: Partial<Record<MiningSourceSelector, string>>; supplied: Partial<Record<MiningSourceSelector, boolean>>; importFields: Record<string, string>;}
export const defaultMiningSourceForm = (): MiningSourceForm => ({format: "auto", shape: "log", selectors: {}, supplied: {}, importFields: {}});
export interface ActiveMiningSourceSnapshot extends Record<string, unknown> {
  format: MiningInputFormat; detected_format: string; shape: MiningInputShape; source_id: string; original_dataset: string; original_input_bytes: number; source_unchanged: boolean;
  source_selectors: Record<MiningSourceSelector, string>; input_before: WorkflowInputTable | null; input_after: WorkflowInputTable | null; checked_table: WorkflowInputTable | null;
  original_log: Record<string, unknown> | null; checked_log: Record<string, unknown> | null; prepared_stream: Record<string, unknown> | null; prepared_lazy_table: WorkflowInputTable | null; source_row_ordinals: number[];
  original_table_origin: string; checked_table_origin: string; checked_log_preparation_error: string | null; checked_flat_projection_error: string | null;
  representative_row_ordinals: Array<Array<number | null>>; projection_changes: Array<{checked_row: number; source_row: number; column: string; removed: boolean}>;
  /** "summary" when a mining response left the complete tables, logs and stream null; GET /api/active-source returns them. */
  detail?: "summary";
}
export interface ActiveMiningSourceDescriptor {file: File; sha256: string; format: MiningInputFormat; shape: MiningInputShape; selectors: Partial<Record<MiningSourceSelector, string>>; options: MineOptions; source_id: string; snapshot: ActiveMiningSourceSnapshot | null;}
export interface ActiveSourceMining extends Record<string, unknown> {family: "ACTIVE_SOURCE_MINING"; source_id: string; active_source: ActiveMiningSourceSnapshot; analysis: MineraExport | null; family_results: Record<string, unknown>; error: null | {type: string; message: string};}
export type ActiveDatasetResult = MineraExport | ActiveSourceMining;
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const count = (holder: object, key: string | number): boolean => typeof (holder as Record<string, unknown>)[String(key)] === "number" && /^(?:0|[1-9]\d*)$/.test(String(nativeCell(holder, key)));
/** The native input parser validates cell/default semantics after the untouched File is uploaded. */
export function isMiningInputTable(value: unknown): value is MiningInputTable {
  return object(value) && Array.isArray(value.columns) && value.columns.every(column => typeof column === "string") && Array.isArray(value.rows) && value.rows.every(row => Array.isArray(row) && row.length === (value.columns as string[]).length && row.every(cell => cell === null || object(cell))) && (value.dtypes === undefined || Array.isArray(value.dtypes) && value.dtypes.every(dtype => typeof dtype === "string")) && (value.index === undefined || value.index === null || object(value.index)) && (value.categorical_dtypes === undefined || Array.isArray(value.categorical_dtypes)) && (value.attributes === undefined || Array.isArray(value.attributes) && value.attributes.every(object)) && (value.metadata === undefined || object(value.metadata));
}
// GET /api/active-source writes a view equal to an earlier member once; a
// later member is {"same_as": "<member>"} (input_after and checked_table may
// name input_before, prepared_lazy_table checked_table, checked_log
// original_log).
const ACTIVE_SOURCE_REFERENCES: Record<string, readonly string[]> = {input_after: ["input_before"], checked_table: ["input_before"], prepared_lazy_table: ["checked_table", "input_before"], checked_log: ["original_log"]};
/** Replaces each member reference with the member it names, in place: the members then share one value. */
export function resolveActiveSourceReferences(value: Record<string, unknown>): Record<string, unknown> {
  for (const [key, targets] of Object.entries(ACTIVE_SOURCE_REFERENCES)) {const reference = value[key]; if (object(reference) && Object.keys(reference).length === 1 && typeof reference.same_as === "string" && targets.includes(reference.same_as)) value[key] = value[reference.same_as];}
  return value;
}
export function isActiveMiningSourceSnapshot(input: unknown): input is ActiveMiningSourceSnapshot {
  const value = object(input) ? resolveActiveSourceReferences({...input}) : input;
  return object(value) && MINING_INPUT_FORMATS.some(format => value.format === format) && MINING_INPUT_SHAPES.some(shape => value.shape === shape) && typeof value.detected_format === "string" && typeof value.source_id === "string" && value.source_id.length > 0 && typeof value.original_dataset === "string" && count(value, "original_input_bytes") && typeof value.source_unchanged === "boolean" && object(value.source_selectors) && MINING_SOURCE_SELECTORS.every(([key]) => typeof (value.source_selectors as Record<string, unknown>)[key] === "string") && ["input_before", "input_after", "checked_table", "prepared_lazy_table"].every(key => value[key] === null || isWorkflowInputTable(value[key])) && ["original_log", "checked_log", "prepared_stream"].every(key => value[key] === null || object(value[key])) && Array.isArray(value.source_row_ordinals) && value.source_row_ordinals.every((_, index) => count(value.source_row_ordinals as object, index)) && ["original_table_origin", "checked_table_origin"].every(key => typeof value[key] === "string") && ["checked_log_preparation_error", "checked_flat_projection_error"].every(key => value[key] === null || typeof value[key] === "string") && Array.isArray(value.representative_row_ordinals) && value.representative_row_ordinals.every(row => Array.isArray(row) && row.every((entry, index) => entry === null || count(row, index))) && Array.isArray(value.projection_changes) && value.projection_changes.every(change => object(change) && count(change, "checked_row") && count(change, "source_row") && typeof change.column === "string" && typeof change.removed === "boolean");
}
export function isActiveSourceMining(value: unknown): value is ActiveSourceMining {return object(value) && value.family === "ACTIVE_SOURCE_MINING" && typeof value.source_id === "string" && isActiveMiningSourceSnapshot(value.active_source) && value.active_source.source_id === value.source_id && (value.analysis === null || isMineraExport(value.analysis)) && object(value.family_results) && (value.error === null || object(value.error) && typeof value.error.type === "string" && typeof value.error.message === "string");}
export function activeDatasetAnalysis(value: ActiveDatasetResult): MineraExport | null {return isActiveSourceMining(value) ? value.analysis : value;}
export function miningSourceFields(options: MiningInputOptions): Array<[string, string]> {
  const fields: Array<[string, string]> = [];
  if (options.miningInputFormat !== undefined) {if (!MINING_INPUT_FORMATS.includes(options.miningInputFormat)) throw new Error("Select a supported mining input format."); fields.push(["mining_input_format", options.miningInputFormat]);}
  if (options.miningInputShape !== undefined) {if (!MINING_INPUT_SHAPES.includes(options.miningInputShape)) throw new Error("Select a supported native mining shape."); fields.push(["mining_input_shape", options.miningInputShape]);}
  if (options.miningSelectors !== undefined) {const selectors: Record<string, string> = Object.create(null); for (const [key, value] of Object.entries(options.miningSelectors)) {if (!MINING_SOURCE_SELECTORS.some(([name]) => name === key) || typeof value !== "string") throw new Error("Mining selectors require the five supported literal string keys."); selectors[key] = value;} fields.push(["mining_selectors_json", JSON.stringify(selectors)]);}
  if (options.miningRequestId !== undefined) {if (!/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/i.test(options.miningRequestId)) throw new Error("Mining request identity must be a UUID."); fields.push(["mining_request_id", options.miningRequestId]);}
  const imports = new Set<string>([...TOKEN_REPLAY_IMPORT_CONTROLS.map(([key]) => key), ...TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key]) => key), "xes_variant", "delimiter"]);
  if (options.miningInputFormat !== "table") for (const [key, value] of Object.entries(options.miningImportFields ?? {})) {if (!imports.has(key) || typeof value !== "string") throw new Error("Unknown source import option."); if (value !== "") fields.push([key, value]);}
  return fields;
}
export function miningSourceOptions(form: MiningSourceForm): MiningInputOptions {const miningSelectors: Partial<Record<MiningSourceSelector, string>> = Object.create(null); for (const [key] of MINING_SOURCE_SELECTORS) if (form.supplied[key]) miningSelectors[key] = form.selectors[key] ?? ""; return {miningInputFormat: form.format, miningInputShape: form.shape, miningSelectors, miningImportFields: {...form.importFields}};}
export function suggestMiningSourceForm(table: MiningInputTable): MiningSourceForm {
  const form = {...defaultMiningSourceForm(), format: "table" as const};
  for (const [key, , canonical, role] of MINING_SOURCE_SELECTORS) {let index = -1; for (const row of table.rows) {index = row.findIndex(cell => cell?.first_class_role === role); if (index >= 0) break;} const column = index < 0 ? table.columns.includes(canonical) ? canonical : undefined : table.columns[index]; if (column !== undefined) {form.selectors[key] = column; form.supplied[key] = true;}}
  return form;
}
export type MiningJsonClassification = {kind: "offline"; data: MineraExport} | {kind: "ocel"} | {kind: "table"; table: MiningInputTable};
export function classifyMiningJson(raw: string): MiningJsonClassification {const value = parseNativeJson(raw); if (isMineraExport(value)) return {kind: "offline", data: value}; if (isOcelJsonDocument(value)) return {kind: "ocel"}; if (isMiningInputTable(value)) return {kind: "table", table: value}; throw new Error("The JSON file is not a Minera analysis, OCEL document or native EventTable.");}
export async function miningSourceHash(file: File): Promise<string> {return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", await file.arrayBuffer())), byte => byte.toString(16).padStart(2, "0")).join("");}
/** Decode literal UTF/WTF-8 labels without File.text() replacing lone surrogates. */
export function activeMiningSourceText(input: ArrayBuffer | Uint8Array): string {const bytes = input instanceof Uint8Array ? input : new Uint8Array(input); let text = ""; for (let index = 0; index < bytes.length;) {const lead = bytes[index++]; let code: number, count: number; if (lead < 0x80) {code = lead; count = 0;} else if (lead >= 0xc2 && lead <= 0xdf) {code = lead & 0x1f; count = 1;} else if (lead >= 0xe0 && lead <= 0xef) {code = lead & 0x0f; count = 2;} else if (lead >= 0xf0 && lead <= 0xf4) {code = lead & 0x07; count = 3;} else throw new Error("The JSON source contains invalid UTF/WTF-8 bytes."); for (let tail = 0; tail < count; ++tail) {const byte = bytes[index++]; if (byte === undefined || byte < 0x80 || byte > 0xbf) throw new Error("The JSON source contains invalid UTF/WTF-8 bytes."); code = code << 6 | byte & 0x3f;} if (count === 1 && code < 0x80 || count === 2 && code < 0x800 || count === 3 && (code < 0x10000 || code > 0x10ffff)) throw new Error("The JSON source contains invalid UTF/WTF-8 bytes."); text += String.fromCodePoint(code);} return text;}
export interface MiningRequestTicket {generation: number; request_id: string;}
export class MiningSourceRequestGate {private generation = 0; issue(): MiningRequestTicket {return {generation: ++this.generation, request_id: crypto.randomUUID()};} invalidate(): void {++this.generation;} accepts(ticket: MiningRequestTicket, signal?: AbortSignal): boolean {return ticket.generation === this.generation && !signal?.aborted;}}
export function assertMiningSourceBinding(expected: string | null, current: string | null, generation: number, currentGeneration: number): void {if (!expected) throw new Error("No live backend log is loaded for this analysis. Open the original CSV or XES log (or wait for the current load to finish) to compute this."); if (expected !== current || generation !== currentGeneration) throw new Error("The active source changed while this request was pending.");}
/** What a cancel-mine settled with: `confirmed` once the backend handled the cancel (and restored its previous session), with the source id it then holds (null for none) when it reported one. A cancel that failed on the network or was refused is not confirmed: the backend may still hold the newer session. */
export interface MiningCancelOutcome {confirmed: boolean; activeSourceId?: string | null;}
/** How long a cancel-mine may take before it settles as unconfirmed. */
export const CANCEL_MINING_TIMEOUT_MS = 10_000;
/**
 * Never rejects, and settles within `timeoutMs`: a cancel whose request or
 * body stalls is aborted and reported unconfirmed, so the cancel barrier that
 * later loads wait on cannot hang behind it.
 */
export function cancelMiningRequest(requestId: string, timeoutMs = CANCEL_MINING_TIMEOUT_MS): Promise<MiningCancelOutcome> {
  const controller = new AbortController();
  let timer: ReturnType<typeof setTimeout> | undefined;
  const expired = new Promise<MiningCancelOutcome>(resolve => {timer = setTimeout(() => {controller.abort(); resolve({confirmed: false});}, timeoutMs);});
  const answered = fetch(`/api/cancel-mine?request_id=${encodeURIComponent(requestId)}`, {method: "POST", signal: controller.signal}).then(async (response): Promise<MiningCancelOutcome> => {
    if (!response.ok) return {confirmed: false};
    const value: unknown = await response.json().catch(() => undefined);
    if (!object(value) || value.family !== "ACTIVE_SOURCE_CANCEL" || value.canceled !== true) return {confirmed: false};
    const held = value.active_source_id;
    return {confirmed: true, ...(held === null || typeof held === "string" ? {activeSourceId: held} : {})};
  }, () => ({confirmed: false}));
  return Promise.race([answered, expired]).finally(() => clearTimeout(timer));
}
/** The download label for a mining response: one whose active source is only the summary (a large original) does not carry the complete source views. */
export function miningResultsLabel(capture: unknown): string {const source = object(capture) ? capture.active_source : undefined; return object(source) && source.detail === "summary" ? "Download native mining results (source summary only)" : "Download complete native mining results";}
export function miningSourceDescriptor(file: File, sha256: string, options: MineOptions, data: ActiveDatasetResult): ActiveMiningSourceDescriptor {
  const snapshot = data.active_source ?? null, source_id = data.source_id; if (!source_id) throw new Error("The mined analysis did not identify its active native source.");
  if (snapshot && (!isActiveMiningSourceSnapshot(snapshot) || snapshot.source_id !== source_id || !(exactFilterMatchesWireFilename(data,"log",snapshot.original_dataset) ?? snapshot.original_dataset === file.name) || BigInt(String(nativeCell(snapshot, "original_input_bytes"))) !== BigInt(file.size))) throw new Error("The active native source does not match the uploaded file and analysis.");
  return {file, sha256, options: cloneExactFilterFiles(options), format: snapshot?.format ?? options.miningInputFormat ?? "auto", shape: snapshot?.shape ?? options.miningInputShape ?? "log", selectors: snapshot ? {...snapshot.source_selectors} : {...options.miningSelectors}, source_id, snapshot};
}
export function miningSourceFormFromDescriptor(source: ActiveMiningSourceDescriptor): MiningSourceForm {const form = defaultMiningSourceForm(); form.format = source.format; form.shape = source.shape; form.importFields = miningSourceWorkbenchFields(source, {}).importFields; for (const [key, value] of Object.entries(source.selectors)) {form.selectors[key as MiningSourceSelector] = value; form.supplied[key as MiningSourceSelector] = true;} return form;}
/** Map actual source selectors onto a workbench's literal carrier without rewriting its completed runs. */
export function miningSourceWorkbenchFields(source: ActiveMiningSourceDescriptor, mapping: Partial<Record<MiningSourceSelector, string | string[]>>): {fields: Record<string, string>; explicitEmptySelectors: Record<string, boolean>; importFields: Record<string, string>} {
  const fields: Record<string, string> = Object.create(null), explicitEmptySelectors: Record<string, boolean> = Object.create(null); for (const [key, targets] of Object.entries(mapping)) {const value = source.selectors[key as MiningSourceSelector]; for (const target of typeof targets === "string" ? [targets] : targets ?? []) if (value !== undefined) {fields[target] = value; explicitEmptySelectors[target] = value === "";}}
  const importFields: Record<string, string> = {};
  for (const [option, key] of [["caseColumn", "case_column"], ["activityColumn", "activity_column"], ["timestampColumn", "timestamp_column"], ["delimiter", "delimiter"], ["xesVariant", "xes_variant"], ["xesCaseAttribute", "xes_case_attribute"], ["xesActivityAttribute", "xes_activity_attribute"], ["xesTimestampAttribute", "xes_timestamp_attribute"], ["xesResourceAttribute", "xes_resource_attribute"], ["xesLifecycleAttribute", "xes_lifecycle_attribute"], ["xesRequireTimestamp", "xes_require_timestamp"], ["xesTimestampSort", "xes_timestamp_sort"], ["xesReverseSort", "xes_reverse_sort"], ["xesMaxTraces", "xes_max_traces"], ["xesAttributesToRead", "xes_attributes_to_read"], ["xesMaxDecompressedBytes", "xes_max_decompressed_bytes"]]) {const value = source.options[option as keyof MineOptions]; if (value !== undefined) importFields[key] = Array.isArray(value) ? value.join(",") : String(value);}
  return {fields, explicitEmptySelectors, importFields: {...importFields, ...source.options.miningImportFields}};
}
export function activeMiningSourceTables(value: object): DiagnosticTable[] {
  const tables = nativeObjectTables(value); for (const table of tables) {const records = table.records; if (records?.some(record => record === null || typeof record !== "object")) table.records = records.map((record, index) => record !== null && typeof record === "object" ? record : parseNativeJson(`{"value":${typeof record === "number" ? nativeCell(records, index) : nativeJson(record)}}`));}
  for (const [key, original] of Object.entries(value)) if (["input_before", "input_after", "checked_table"].includes(key) && isMiningInputTable(original)) {const rows: unknown[][] = [], records: unknown[] = []; original.rows.forEach((row, row_index) => row.forEach((cell, column_index) => {rows.push([row_index, column_index, original.columns[column_index], original.dtypes?.[column_index], cell === null ? "missing" : cell.null ? "present null" : "present", cell?.kind, cell?.value, cell === null ? undefined : nativeCell(cell, "typed_value"), cell]); records.push({row_index, column_index, column: original.columns[column_index], cell});})); tables.push({id: `${key}_cells`, title: `Every ${key.replace(/_/g, " ")} cell`, columns: ["row_index", "column_index", "column", "dtype", "presence", "kind", "lexical_value", "typed_value", "complete_cell"], rows, records});}
  return tables;
}
export function activeMiningSourceAllTablesCsv(tables: DiagnosticTable[]): string {const rows: unknown[][] = []; for (const table of tables) {if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined])); table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));} return diagnosticCsv({id: "active_source_tables", title: "Every complete native source table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});}
export function activeMiningSourceReport(source: ActiveMiningSourceDescriptor, snapshot: object, mining?: ActiveDatasetResult): string {return nativeJson({family: "ACTIVE_EVENT_SOURCE_REPORT", source_id: source.source_id, original: {name: source.file.name, bytes: source.file.size, last_modified: source.file.lastModified, sha256: source.sha256}, mining_options: source.options, request: mining ? nativeResponseRequest(mining) ?? null : nativeResponseRequest(snapshot) ?? null, native_source: snapshot, mining_result: mining ?? null}, 2);}
export function activeMiningSourceRequest(source: ActiveMiningSourceDescriptor, mining?: ActiveDatasetResult): string {return nativeJson({request: mining ? nativeResponseRequest(mining) ?? null : null, original: {name: source.file.name, bytes: source.file.size, sha256: source.sha256}, source_id: source.source_id, mining_options: source.options}, 2);}
export function activeMiningSourceBytes(text: string): Uint8Array<ArrayBuffer> {return workflowInputNativeBytes(text);}
export interface ActiveMiningSourceRun {value: ActiveMiningSourceSnapshot; envelope: object; raw: string; tables: DiagnosticTable[];}
export async function requestActiveMiningSource(sourceId: string, signal?: AbortSignal): Promise<ActiveMiningSourceRun> {const path = `/api/active-source?source_id=${encodeURIComponent(sourceId)}`, response = await fetch(path, {signal}), value = await readNativeResponse(response, {path}); signal?.throwIfAborted(); if (object(value) && object(value.active_source)) resolveActiveSourceReferences(value.active_source); if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `Could not inspect the active source (${response.status}).`); if (!object(value) || value.family !== "ACTIVE_EVENT_SOURCE" || value.source_id !== sourceId || !isActiveMiningSourceSnapshot(value.active_source) || value.active_source.source_id !== sourceId) throw new Error("The source inspector response does not match the active native source."); return {value: value.active_source, envelope: value, raw: nativeResponseJson(value), tables: activeMiningSourceTables(value.active_source)};}
export async function requestActiveMiningSourceFile(source: ActiveMiningSourceDescriptor, signal?: AbortSignal): Promise<File> {const path = `/api/active-source-file?source_id=${encodeURIComponent(source.source_id)}`, response = await fetch(path, {signal}); if (!response.ok) throw new Error(`Could not download the original native source (${response.status}).`); const bytes = await response.arrayBuffer(); signal?.throwIfAborted(); const file = new File([bytes], source.file.name, {type: source.file.type, lastModified: source.file.lastModified}), sha256 = await miningSourceHash(file); signal?.throwIfAborted(); if (file.size !== source.file.size || sha256 !== source.sha256) throw new Error("The cached original source bytes do not match the active upload."); return file;}
export function activeMiningSourceTableFileName(id: string): string {let text = ""; for (let index = 0; index < id.length; ++index) text += /^[a-z0-9_.-]$/.test(id[index]) ? id[index] : "%" + id.charCodeAt(index).toString(16).padStart(4, "0"); return text;}
