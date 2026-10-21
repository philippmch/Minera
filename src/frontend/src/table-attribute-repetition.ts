import {nativeCell} from "./native-json";
import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";

export type TableRepetitionEngine = "pandas" | "polars";
export interface TableRepetitionForm {
  engine: TableRepetitionEngine;
  caseKeyMode: "default" | "custom"; caseKey: string;
  attributeKeyMode: "default" | "custom"; attributeKey: string;
  targetJson: string; minimum: string; maximum: string;
}
export function defaultTableRepetitionForm(engine: TableRepetitionEngine): TableRepetitionForm {
  return {engine, caseKeyMode: "default", caseKey: "case:concept:name", attributeKeyMode: "default", attributeKey: "concept:name",
    targetJson: '{"type":"string","value":"A"}', minimum: "2", maximum: "9223372036854775807"};
}
export interface TableRepetitionCounts {rows: number; columns: number;}
export interface TableRepetitionEnvelope {
  family: "TABLE_ATTRIBUTE_REPETITION"; engine: TableRepetitionEngine | null; http_status: 200 | 400;
  phase: string; apply_returned: boolean; source: TableRepetitionCounts | null; result: TableRepetitionCounts | null;
  source_unchanged: boolean | null; case_id_key: ReferenceLogFilterNativeString; attribute_key: ReferenceLogFilterNativeString;
  source_table_json: ReferenceLogFilterNativeString | null; filtered_table_json: ReferenceLogFilterNativeString | null;
  error_type: ReferenceLogFilterNativeString | null; error: ReferenceLogFilterNativeString | null;
}
export interface TableRepetitionRun {
  value: TableRepetitionEnvelope; sourceFile: File; fields: Array<[string, string]>; raw: string; responseBytes: Uint8Array<ArrayBuffer>;
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const keys = (value: Record<string, unknown>, names: readonly string[]) => Object.keys(value).length === names.length && names.every(key => Object.hasOwn(value, key));
function counts(value: unknown): value is TableRepetitionCounts | null {
  return value === null || object(value) && keys(value, ["rows", "columns"]) && ["rows", "columns"].every(key => {
    const cell = value[key], token = String(nativeCell(value, key));
    return typeof cell === "number" && Number.isFinite(cell) && /^(?:0|[1-9]\d*)$/.test(token) && BigInt(token) <= 18446744073709551615n;
  });
}
export function isTableRepetitionEnvelope(value: unknown): value is TableRepetitionEnvelope {
  return object(value) && keys(value, ["family", "engine", "http_status", "phase", "apply_returned", "source", "result", "source_unchanged", "case_id_key", "attribute_key", "source_table_json", "filtered_table_json", "error_type", "error"])
    && value.family === "TABLE_ATTRIBUTE_REPETITION" && (value.engine === null || value.engine === "pandas" || value.engine === "polars")
    && (value.http_status === 200 || value.http_status === 400) && typeof value.phase === "string" && typeof value.apply_returned === "boolean"
    && counts(value.source) && counts(value.result) && (value.source_unchanged === null || typeof value.source_unchanged === "boolean")
    && isOriginalEmbeddingFilterNativeString(value.case_id_key) && isOriginalEmbeddingFilterNativeString(value.attribute_key)
    && ["source_table_json", "filtered_table_json", "error_type", "error"].every(key => value[key] === null || isOriginalEmbeddingFilterNativeString(value[key]));
}
/** Typed target, bound tokens and literal selectors are validated by the native workbench. */
export function tableRepetitionFields(form: TableRepetitionForm): Array<[string, string]> {
  if (!["pandas", "polars"].includes(form.engine)) throw new Error("Choose pandas or Polars table filtering.");
  if (![form.caseKeyMode, form.attributeKeyMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal column selectors.");
  const fields: Array<[string, string]> = [["engine", form.engine], ["target_json", form.targetJson], ["min_rep", form.minimum], ["max_rep", form.maximum]];
  if (form.caseKeyMode === "custom") fields.push(["case_id_key", form.caseKey]);
  if (form.attributeKeyMode === "custom") fields.push(["attribute_key", form.attributeKey]);
  return fields;
}
export async function requestTableRepetition(sourceFile: File, form: TableRepetitionForm, signal?: AbortSignal): Promise<TableRepetitionRun> {
  signal?.throwIfAborted();
  if (!(sourceFile instanceof File)) throw new Error("Choose an actual native EventTable JSON file.");
  const fields = tableRepetitionFields(form), body = new FormData(); body.append("source_file", sourceFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await fetch("/api/reference-table-repetition", {method: "POST", body, signal}); signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`Table repetition returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try {new TextDecoder("utf-8", {fatal: true}).decode(responseBytes);} catch {throw new Error("The table repetition response is not safe JSON UTF-8.");}
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-table-repetition", fields, reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isTableRepetitionEnvelope(value) || value.http_status !== response.status) throw new Error("The table repetition response has an invalid native envelope.");
  if (value.engine !== form.engine && !(value.http_status === 400 && value.engine === null)) throw new Error("The table repetition response belongs to another engine.");
  return {value, sourceFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export function tableRepetitionArtifactFile(run: TableRepetitionRun, kind: "source" | "result"): File {
  const value = kind === "source" ? run.value.source_table_json : run.value.filtered_table_json;
  if (value === null || kind === "result" && (run.value.http_status !== 200 || run.value.error !== null)) throw new Error("The native table artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], `${kind === "source" ? "parsed-source" : "filtered"}-${run.value.engine ?? "table"}-table.json`, {type: "application/json"});
}
export function tableRepetitionResponseFile(run: TableRepetitionRun): File {return new File([run.responseBytes], "table-repetition-response.json", {type: "application/json"});}
export function tableRepetitionCountText(value: TableRepetitionCounts, key: "rows" | "columns"): string {return String(nativeCell(value, key));}
export interface TableRepetitionIdentity {source: File | null; form: TableRepetitionForm;}
export class TableRepetitionRequestGate {
  private generation = 0;
  issue(value: TableRepetitionIdentity) {return {generation: ++this.generation, source: value.source, form: JSON.stringify(value.form)};}
  invalidate() {++this.generation;}
  accepts(ticket: ReturnType<TableRepetitionRequestGate["issue"]>, value: TableRepetitionIdentity, signal?: AbortSignal) {
    return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.form === JSON.stringify(value.form);
  }
}
export function tableRepetitionSourceExample(engine: TableRepetitionEngine): File {
  const cell = (value: string) => ({value});
  const signed = () => ({value: "9007199254740993", kind: "int", typed_value: "9007199254740993", integer_storage: "int64"});
  const unsigned = () => ({value: "18446744073709551615", kind: "int", typed_value: "18446744073709551615", integer_storage: "uint64"});
  const table = {columns: ["case:concept:name", "concept:name", "signed", "unsigned"], dtypes: engine === "polars" ? ["String", "String", "Int64", "UInt64"] : ["object", "object", "int64", "uint64"],
    attributes: [{key: "example", value: "Original typed table"}], rows: [["c1", "A"], ["c2", "B"], ["c1", "A"], ["c1", "B"]].map(([key, value]) => [cell(key), cell(value), signed(), unsigned()])};
  return new File([nativeStringBytes(JSON.stringify(table, null, 2))], `${engine}-repetition-example.json`, {type: "application/json"});
}
