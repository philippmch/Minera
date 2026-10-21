import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isTableRepetitionEnvelope, type TableRepetitionCounts, type TableRepetitionEngine, type TableRepetitionEnvelope} from "./table-attribute-repetition";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";

export interface TableBetweenForm {
  engine: TableRepetitionEngine;
  startJson: string; endJson: string;
  caseKeyMode: "default" | "custom"; caseKey: string;
  activityKeyMode: "default" | "custom"; activityKey: string;
  separatorMode: "default" | "custom"; separator: string;
}
export function defaultTableBetweenForm(engine: TableRepetitionEngine): TableBetweenForm {
  return {engine, startJson: '"A"', endJson: '"B"', caseKeyMode: "default", caseKey: "case:concept:name",
    activityKeyMode: "default", activityKey: "concept:name", separatorMode: "default", separator: "##@@"};
}
export interface TableBetweenEnvelope extends Omit<TableRepetitionEnvelope, "family" | "attribute_key"> {
  family: "TABLE_BETWEEN_FILTER"; activity_key: ReferenceLogFilterNativeString;
}
export interface TableBetweenRun {
  value: TableBetweenEnvelope; sourceFile: File; fields: Array<[string, string]>;
  raw: string; responseBytes: Uint8Array<ArrayBuffer>;
}
export function isTableBetweenEnvelope(value: unknown): value is TableBetweenEnvelope {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  if (record.family !== "TABLE_BETWEEN_FILTER" || Object.keys(record).length !== 14 || !Object.hasOwn(record, "activity_key")) return false;
  const {family: _family, activity_key, ...rest} = record;
  return isTableRepetitionEnvelope({...rest, family: "TABLE_ATTRIBUTE_REPETITION", attribute_key: activity_key});
}
export function tableBetweenFields(form: TableBetweenForm): Array<[string, string]> {
  if (!["pandas", "polars"].includes(form.engine)) throw new Error("Choose pandas or Polars table filtering.");
  if (![form.caseKeyMode, form.activityKeyMode, form.separatorMode].every(mode => mode === "default" || mode === "custom"))
    throw new Error("Choose default or literal column and separator controls.");
  const fields: Array<[string, string]> = [["engine", form.engine], ["act1_json", form.startJson], ["act2_json", form.endJson]];
  if (form.caseKeyMode === "custom") fields.push(["case_id_key", form.caseKey]);
  if (form.activityKeyMode === "custom") fields.push(["activity_key", form.activityKey]);
  if (form.separatorMode === "custom") fields.push(["subcase_concat_str", form.separator]);
  return fields;
}
export async function requestTableBetween(sourceFile: File, form: TableBetweenForm, signal?: AbortSignal): Promise<TableBetweenRun> {
  signal?.throwIfAborted();
  if (!(sourceFile instanceof File)) throw new Error("Choose an actual native EventTable JSON file.");
  const fields = tableBetweenFields(form), body = new FormData(); body.append("source_file", sourceFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await fetch("/api/reference-table-between", {method: "POST", body, signal}); signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`Table between filtering returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try { new TextDecoder("utf-8", {fatal: true}).decode(responseBytes); }
  catch { throw new Error("The table between response is not safe JSON UTF-8."); }
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-table-between", fields,
    reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isTableBetweenEnvelope(value) || value.http_status !== response.status) throw new Error("The table between response has an invalid native envelope.");
  if (value.engine !== form.engine && !(value.http_status === 400 && value.engine === null)) throw new Error("The table between response belongs to another engine.");
  return {value, sourceFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export function tableBetweenArtifactFile(run: TableBetweenRun, kind: "source" | "result"): File {
  const value = kind === "source" ? run.value.source_table_json : run.value.filtered_table_json;
  if (value === null || kind === "result" && (run.value.http_status !== 200 || run.value.error !== null))
    throw new Error("The native table artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], `${kind === "source" ? "parsed-source" : "between"}-${run.value.engine ?? "table"}-table.json`, {type: "application/json"});
}
export function tableBetweenResponseFile(run: TableBetweenRun): File {
  return new File([run.responseBytes], "table-between-response.json", {type: "application/json"});
}
export interface TableBetweenIdentity {source: File | null; form: TableBetweenForm;}
export class TableBetweenRequestGate {
  private generation = 0;
  issue(value: TableBetweenIdentity) { return {generation: ++this.generation, source: value.source, form: JSON.stringify(value.form)}; }
  invalidate() { ++this.generation; }
  accepts(ticket: ReturnType<TableBetweenRequestGate["issue"]>, value: TableBetweenIdentity, signal?: AbortSignal) {
    return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.form === JSON.stringify(value.form);
  }
}
export function tableBetweenSourceExample(engine: TableRepetitionEngine): File {
  const cell = (value: string) => ({value});
  const table = {columns: ["case:concept:name", "concept:name", "time:timestamp", "signed", "unsigned"],
    dtypes: engine === "polars" ? ["String", "String", "Datetime(time_unit='us', time_zone=None)", "Int64", "UInt64"] : ["object", "object", "datetime64[ns]", "int64", "uint64"],
    attributes: [{key: "example", value: "Between activities: complete typed table"}],
    rows: [["c1", "A"], ["c1", "X"], ["c1", "B"], ["c2", "A"], ["c2", "B"]].map(([key, activity], i) => [cell(key), cell(activity),
      {kind: "date", value: `2026-06-01T00:00:0${i}`, typed_value: `2026-06-01T00:00:0${i}`},
      {kind: "int", value: "9007199254740993", typed_value: "9007199254740993", integer_storage: "int64"},
      {kind: "int", value: "18446744073709551615", typed_value: "18446744073709551615", integer_storage: "uint64"}])};
  return new File([nativeStringBytes(JSON.stringify(table, null, 2))], `${engine}-between-example.json`, {type: "application/json"});
}
export type TableBetweenCounts = TableRepetitionCounts;
