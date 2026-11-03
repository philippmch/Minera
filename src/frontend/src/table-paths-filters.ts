import {appendNativeMultipartField} from "./native-multipart";
import { backendFetch } from "./backend-fetch";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isTableRepetitionEnvelope, type TableRepetitionEngine, type TableRepetitionEnvelope} from "./table-attribute-repetition";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";
import {tableBetweenSourceExample} from "./table-between-filters";
export type TablePathsOperation = "paths" | "performance";
export interface TablePathsForm {
  engine: TableRepetitionEngine; operation: TablePathsOperation; pathsJson: string; pathJson: string; positive: boolean; minimum: string; maximum: string;
  caseKeyMode: "default" | "custom"; caseKey: string; activityKeyMode: "default" | "custom"; activityKey: string;
  targetKeyMode: "default" | "custom"; targetKey: string; timestampKeyMode: "default" | "custom"; timestampKey: string;
}
export function defaultTablePathsForm(engine: TableRepetitionEngine): TablePathsForm {
  return {engine, operation: "paths", pathsJson: '[["A", "B"]]', pathJson: '["A", "B"]', positive: true, minimum: "0", maximum: "9223372036854775807",
    caseKeyMode: "default", caseKey: "case:concept:name", activityKeyMode: "default", activityKey: "concept:name", targetKeyMode: "default", targetKey: "concept:name", timestampKeyMode: "default", timestampKey: "time:timestamp"};
}
export interface TablePathsEnvelope extends Omit<TableRepetitionEnvelope, "family" | "attribute_key"> {
  family: "TABLE_PATHS_FILTER"; operation: TablePathsOperation | null; activity_key: ReferenceLogFilterNativeString;
  target_attribute_key: ReferenceLogFilterNativeString; timestamp_key: ReferenceLogFilterNativeString;
}
export interface TablePathsRun {value: TablePathsEnvelope; sourceFile: File; fields: Array<[string, string]>; raw: string; responseBytes: Uint8Array<ArrayBuffer>;}
export function isTablePathsEnvelope(value: unknown): value is TablePathsEnvelope {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  if (record.family !== "TABLE_PATHS_FILTER" || Object.keys(record).length !== 17 ||
      !(record.operation === null || record.operation === "paths" || record.operation === "performance") ||
      !isOriginalEmbeddingFilterNativeString(record.target_attribute_key) || !isOriginalEmbeddingFilterNativeString(record.timestamp_key)) return false;
  const {family: _family, operation: _operation, activity_key, target_attribute_key: _target, timestamp_key: _time, ...rest} = record;
  return isTableRepetitionEnvelope({...rest, family: "TABLE_ATTRIBUTE_REPETITION", attribute_key: activity_key});
}
export function tablePathsFields(form: TablePathsForm): Array<[string, string]> {
  if (!["pandas", "polars"].includes(form.engine) || !["paths", "performance"].includes(form.operation)) throw new Error("Choose an engine and path filter.");
  if (![form.caseKeyMode, form.activityKeyMode, form.targetKeyMode, form.timestampKeyMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal columns.");
  const fields: Array<[string, string]> = [["engine", form.engine], ["operation", form.operation], ["positive", String(form.positive)],
    [form.operation === "paths" ? "paths_json" : "path_json", form.operation === "paths" ? form.pathsJson : form.pathJson]];
  if (form.caseKeyMode === "custom") fields.push(["case_id_key", form.caseKey]);
  if (form.activityKeyMode === "custom") fields.push(["activity_key", form.activityKey]);
  if (form.targetKeyMode === "custom" && form.operation === "paths") fields.push(["target_attribute_key", form.targetKey]);
  if (form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
  if (form.operation === "performance") {
    fields.push(["minimum", form.minimum], ["maximum", form.maximum]);
  }
  return fields;
}
export async function requestTablePaths(sourceFile: File, form: TablePathsForm, signal?: AbortSignal): Promise<TablePathsRun> {
  signal?.throwIfAborted(); if (!(sourceFile instanceof File)) throw new Error("Choose an actual native EventTable JSON file.");
  const fields = tablePathsFields(form), body = new FormData(); body.append("source_file", sourceFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await backendFetch("/api/reference-table-paths", {method: "POST", body, signal}); signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`Table path filtering returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try {new TextDecoder("utf-8", {fatal: true}).decode(responseBytes);} catch {throw new Error("The table paths response is not safe JSON UTF-8.");}
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-table-paths", fields, reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isTablePathsEnvelope(value) || value.http_status !== response.status) throw new Error("The table paths response has an invalid native envelope.");
  if (value.engine !== form.engine && !(value.http_status === 400 && value.engine === null) || value.operation !== form.operation && !(value.http_status === 400 && value.operation === null)) throw new Error("The table paths response belongs to another engine or filter.");
  return {value, sourceFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export function tablePathsArtifactFile(run: TablePathsRun, kind: "source" | "result"): File {
  const value = kind === "source" ? run.value.source_table_json : run.value.filtered_table_json;
  if (value === null || kind === "result" && (run.value.http_status !== 200 || run.value.error !== null)) throw new Error("The native table artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], `${kind === "source" ? "parsed-source" : "paths"}-${run.value.engine ?? "table"}-table.json`, {type: "application/json"});
}
export function tablePathsResponseFile(run: TablePathsRun): File {return new File([run.responseBytes], "table-paths-response.json", {type: "application/json"});}
export function tablePathsSourceExample(engine: TableRepetitionEngine): File {return new File([tableBetweenSourceExample(engine)], `${engine}-paths-example.json`, {type: "application/json"});}
export interface TablePathsIdentity {source: File | null; form: TablePathsForm;}
export class TablePathsRequestGate {
  private generation = 0;
  issue(value: TablePathsIdentity) {return {generation: ++this.generation, source: value.source, form: JSON.stringify(value.form)};}
  invalidate() {++this.generation;}
  accepts(ticket: ReturnType<TablePathsRequestGate["issue"]>, value: TablePathsIdentity, signal?: AbortSignal) {return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.form === JSON.stringify(value.form);}
}
