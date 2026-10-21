import {appendNativeMultipartField} from "./native-multipart";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isTableRepetitionEnvelope, type TableRepetitionEngine, type TableRepetitionEnvelope} from "./table-attribute-repetition";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";
import {tableBetweenSourceExample} from "./table-between-filters";

export type TableCaseOperation = "size" | "count" | "performance";
export interface TableCaseForm {
  engine: TableRepetitionEngine; operation: TableCaseOperation; minimum: string; maximum: string; maxCases: string;
  caseKeyMode: "default" | "custom"; caseKey: string;
  timestampKeyMode: "default" | "custom"; timestampKey: string; businessHours: boolean;
}
export function defaultTableCaseForm(engine: TableRepetitionEngine): TableCaseForm {
  return {engine, operation: "size", minimum: "2", maximum: "null", maxCases: "1000", caseKeyMode: "default", caseKey: "case:concept:name",
    timestampKeyMode: "default", timestampKey: "time:timestamp", businessHours: false};
}
export interface TableCaseEnvelope extends Omit<TableRepetitionEnvelope, "family" | "attribute_key"> {
  family: "TABLE_CASE_FILTER"; operation: TableCaseOperation | null; timestamp_key: ReferenceLogFilterNativeString;
}
export interface TableCaseRun {value: TableCaseEnvelope; sourceFile: File; fields: Array<[string, string]>; raw: string; responseBytes: Uint8Array<ArrayBuffer>;}
export function isTableCaseEnvelope(value: unknown): value is TableCaseEnvelope {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  if (record.family !== "TABLE_CASE_FILTER" || Object.keys(record).length !== 15 || !Object.hasOwn(record, "timestamp_key") ||
      !(record.operation === null || typeof record.operation === "string" && ["size", "count", "performance"].includes(record.operation))) return false;
  const {family: _family, operation: _operation, timestamp_key, ...rest} = record;
  return isTableRepetitionEnvelope({...rest, family: "TABLE_ATTRIBUTE_REPETITION", attribute_key: timestamp_key});
}
export function tableCaseFields(form: TableCaseForm): Array<[string, string]> {
  if (!["pandas", "polars"].includes(form.engine) || !["size", "count", "performance"].includes(form.operation)) throw new Error("Choose an engine and case filter.");
  if (![form.caseKeyMode, form.timestampKeyMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal columns.");
  const fields: Array<[string, string]> = [["engine", form.engine], ["operation", form.operation]];
  if (form.caseKeyMode === "custom") fields.push(["case_id_key", form.caseKey]);
  if (form.operation === "count") fields.push(["max_cases", form.maxCases]);
  else fields.push(["minimum", form.minimum], ["maximum", form.maximum]);
  if (form.operation === "performance") {
    if (form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
    fields.push(["business_hours", String(form.businessHours)]);
  }
  return fields;
}
export async function requestTableCase(sourceFile: File, form: TableCaseForm, signal?: AbortSignal): Promise<TableCaseRun> {
  signal?.throwIfAborted();
  if (!(sourceFile instanceof File)) throw new Error("Choose an actual native EventTable JSON file.");
  const fields = tableCaseFields(form), body = new FormData(); body.append("source_file", sourceFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await fetch("/api/reference-table-cases", {method: "POST", body, signal}); signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`Table case filtering returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try {new TextDecoder("utf-8", {fatal: true}).decode(responseBytes);} catch {throw new Error("The table case response is not safe JSON UTF-8.");}
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-table-cases", fields,
    reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isTableCaseEnvelope(value) || value.http_status !== response.status) throw new Error("The table case response has an invalid native envelope.");
  if (value.engine !== form.engine && !(value.http_status === 400 && value.engine === null) ||
      value.operation !== form.operation && !(value.http_status === 400 && value.operation === null)) throw new Error("The table case response belongs to another engine or filter.");
  return {value, sourceFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export function tableCaseArtifactFile(run: TableCaseRun, kind: "source" | "result"): File {
  const value = kind === "source" ? run.value.source_table_json : run.value.filtered_table_json;
  if (value === null || kind === "result" && (run.value.http_status !== 200 || run.value.error !== null)) throw new Error("The native table artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], `${kind === "source" ? "parsed-source" : "cases"}-${run.value.engine ?? "table"}-table.json`, {type: "application/json"});
}
export function tableCaseResponseFile(run: TableCaseRun): File {return new File([run.responseBytes], "table-cases-response.json", {type: "application/json"});}
export function tableCaseSourceExample(engine: TableRepetitionEngine): File {
  return new File([tableBetweenSourceExample(engine)], `${engine}-cases-example.json`, {type: "application/json"});
}
export interface TableCaseIdentity {source: File | null; form: TableCaseForm;}
export class TableCaseRequestGate {
  private generation = 0;
  issue(value: TableCaseIdentity) {return {generation: ++this.generation, source: value.source, form: JSON.stringify(value.form)};}
  invalidate() {++this.generation;}
  accepts(ticket: ReturnType<TableCaseRequestGate["issue"]>, value: TableCaseIdentity, signal?: AbortSignal) {
    return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.form === JSON.stringify(value.form);
  }
}
