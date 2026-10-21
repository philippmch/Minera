import {appendNativeMultipartField} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isTableRepetitionEnvelope, type TableRepetitionEngine, type TableRepetitionEnvelope} from "./table-attribute-repetition";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";

export const TABLE_TIMESTAMP_OPERATIONS = ["events", "contained", "intersecting", "starting", "completing", "attribute"] as const;
export type TableTimestampOperation = typeof TABLE_TIMESTAMP_OPERATIONS[number];
export const TABLE_TIMESTAMP_LABELS: Record<TableTimestampOperation, string> = {
  events: "Keep events in a time window", contained: "Keep cases contained in a time window",
  intersecting: "Keep cases intersecting a time window", starting: "Filter by case start time",
  completing: "Filter by case completion time", attribute: "Match an attribute in a time window",
};
export function tableTimestampOperations(engine: TableRepetitionEngine): TableTimestampOperation[] {
  return TABLE_TIMESTAMP_OPERATIONS.filter(operation => engine === "pandas" || operation !== "starting" && operation !== "completing");
}
export interface TableTimestampForm {
  engine: TableRepetitionEngine; operation: TableTimestampOperation; lower: string; upper: string; positive: boolean; keepMissing: boolean;
  caseKeyMode: "default" | "custom"; caseKey: string; timestampKeyMode: "default" | "custom"; timestampKey: string;
  attributeKeyMode: "default" | "custom"; attributeKey: string; targetJson: string;
}
export function defaultTableTimestampForm(engine: TableRepetitionEngine): TableTimestampForm {
  return {engine, operation: "events", lower: "2026-06-01 00:00:01", upper: "2026-06-01 00:00:03", positive: true, keepMissing: false,
    caseKeyMode: "default", caseKey: "case:concept:name", timestampKeyMode: "default", timestampKey: "time:timestamp",
    attributeKeyMode: "default", attributeKey: "concept:name", targetJson: '{"type":"string","value":"A"}'};
}
export interface TableTimestampEnvelope extends Omit<TableRepetitionEnvelope, "family"> {
  family: "TABLE_TIMESTAMP_FILTER"; operation: TableTimestampOperation | null; timestamp_key: ReferenceLogFilterNativeString;
}
export interface TableTimestampRun {value: TableTimestampEnvelope; sourceFile: File; fields: Array<[string, string]>; raw: string; responseBytes: Uint8Array<ArrayBuffer>;}
export function isTableTimestampEnvelope(value: unknown): value is TableTimestampEnvelope {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  if (record.family !== "TABLE_TIMESTAMP_FILTER" || Object.keys(record).length !== 16 ||
      !(record.operation === null || TABLE_TIMESTAMP_OPERATIONS.includes(record.operation as TableTimestampOperation)) ||
      !isOriginalEmbeddingFilterNativeString(record.timestamp_key)) return false;
  const {family: _family, operation: _operation, timestamp_key: _time, ...rest} = record;
  return isTableRepetitionEnvelope({...rest, family: "TABLE_ATTRIBUTE_REPETITION"});
}
export function tableTimestampFields(form: TableTimestampForm): Array<[string, string]> {
  if (!["pandas", "polars"].includes(form.engine) || !tableTimestampOperations(form.engine).includes(form.operation)) throw new Error("Choose an available timestamp filter.");
  if (![form.caseKeyMode, form.timestampKeyMode, form.attributeKeyMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal columns.");
  const fields: Array<[string, string]> = [["engine", form.engine], ["operation", form.operation], ["lower", form.lower], ["upper", form.upper]];
  if (form.caseKeyMode === "custom" && form.operation !== "events") fields.push(["case_id_key", form.caseKey]);
  if (form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
  if (form.operation === "attribute") {
    fields.push(["attribute_value_json", form.targetJson]);
    if (form.attributeKeyMode === "custom") fields.push(["attribute_key", form.attributeKey]);
  }
  if (form.engine === "pandas") fields.push(["keep_nan_values", String(form.keepMissing)]);
  if (form.operation === "starting" || form.operation === "completing") fields.push(["positive", String(form.positive)]);
  return fields;
}
export async function requestTableTimestamp(sourceFile: File, form: TableTimestampForm, signal?: AbortSignal): Promise<TableTimestampRun> {
  signal?.throwIfAborted(); if (!(sourceFile instanceof File)) throw new Error("Choose an actual native EventTable JSON file.");
  const fields = tableTimestampFields(form), body = new FormData(); body.append("source_file", sourceFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await fetch("/api/reference-table-timestamp", {method: "POST", body, signal}); signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`Table timestamp filtering returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try {new TextDecoder("utf-8", {fatal: true}).decode(responseBytes);} catch {throw new Error("The table timestamp response is not safe JSON UTF-8.");}
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-table-timestamp", fields, reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isTableTimestampEnvelope(value) || value.http_status !== response.status) throw new Error("The table timestamp response has an invalid native envelope.");
  if (value.engine !== form.engine && !(value.http_status === 400 && value.engine === null) || value.operation !== form.operation && !(value.http_status === 400 && value.operation === null)) throw new Error("The table timestamp response belongs to another engine or filter.");
  return {value, sourceFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export function tableTimestampArtifactFile(run: TableTimestampRun, kind: "source" | "result"): File {
  const value = kind === "source" ? run.value.source_table_json : run.value.filtered_table_json;
  if (value === null || kind === "result" && (run.value.http_status !== 200 || run.value.error !== null)) throw new Error("The native table artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], `${kind === "source" ? "parsed-source" : "timestamp"}-${run.value.engine ?? "table"}-table.json`, {type: "application/json"});
}
export function tableTimestampResponseFile(run: TableTimestampRun): File {return new File([run.responseBytes], "table-timestamp-response.json", {type: "application/json"});}
export function tableTimestampSourceExample(engine: TableRepetitionEngine): File {
  const cell = (value: string) => ({value});
  const table = {columns: ["case:concept:name", "concept:name", "time:timestamp", "signed", "unsigned"],
    dtypes: engine === "polars" ? ["String", "String", "Datetime(time_unit='us', time_zone='UTC')", "Int64", "UInt64"] : ["object", "object", "datetime64[ns, UTC]", "int64", "uint64"],
    attributes: [{key: "example", value: "Time window: complete typed table"}],
    rows: [["c1", "A"], ["c1", "X"], ["c1", "B"], ["c2", "A"], ["c2", "B"]].map(([key, activity], i) => [cell(key), cell(activity),
      {kind: "date", value: `2026-06-01T00:00:0${i}Z`, typed_value: `2026-06-01T00:00:0${i}Z`},
      {kind: "int", value: "9007199254740993", typed_value: "9007199254740993", integer_storage: "int64"},
      {kind: "int", value: "18446744073709551615", typed_value: "18446744073709551615", integer_storage: "uint64"}])};
  return new File([JSON.stringify(table, null, 2)], `${engine}-timestamp-example.json`, {type: "application/json"});
}
export interface TableTimestampIdentity {source: File | null; form: TableTimestampForm;}
export class TableTimestampRequestGate {
  private generation = 0;
  issue(value: TableTimestampIdentity) {return {generation: ++this.generation, source: value.source, form: JSON.stringify(value.form)};}
  invalidate() {++this.generation;}
  accepts(ticket: ReturnType<TableTimestampRequestGate["issue"]>, value: TableTimestampIdentity, signal?: AbortSignal) {return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.form === JSON.stringify(value.form);}
}
