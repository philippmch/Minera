import {appendNativeMultipartField} from "./native-multipart";
import { backendFetch } from "./backend-fetch";
import {nativeResponseJson, readNativeResponse} from "./native-response";
import {isTableRepetitionEnvelope, type TableRepetitionEngine, type TableRepetitionEnvelope} from "./table-attribute-repetition";
import {isOriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {referenceLogFilterStringBytes, type ReferenceLogFilterNativeString} from "./reference-log-filters";
import {tableTimestampSourceExample} from "./table-timestamp-filters";

export const TABLE_VARIANTS_OPERATIONS = ["membership", "top_k", "min_coverage", "max_coverage", "from_list", "auto"] as const;
export type TableVariantsOperation = typeof TABLE_VARIANTS_OPERATIONS[number];
export const TABLE_VARIANTS_LABELS: Record<TableVariantsOperation, string> = {
  membership: "Match complete activity sequences", top_k: "Keep the top k variants", min_coverage: "Keep variants above a coverage threshold",
  max_coverage: "Keep variants below a coverage threshold", from_list: "Match serialized variants", auto: "Automatically select variants",
};
export function tableVariantsOperations(engine: TableRepetitionEngine): TableVariantsOperation[] {
  return engine === "pandas" ? ["membership", "top_k", "min_coverage", "max_coverage"] : ["membership", "top_k"];
}
export interface TableVariantsForm {
  engine: TableRepetitionEngine; operation: TableVariantsOperation; admittedJson: string; criterion: string; positive: boolean; sequenceKind: "list" | "tuple";
  caseKeyMode: "default" | "custom"; caseKey: string; activityKeyMode: "default" | "custom"; activityKey: string;
  timestampKeyMode: "default" | "custom"; timestampKey: string;
}
export function defaultTableVariantsForm(engine: TableRepetitionEngine): TableVariantsForm {
  return {engine, operation: "membership", admittedJson: '[["A", "B"]]', criterion: "1", positive: true, sequenceKind: "tuple",
    caseKeyMode: "default", caseKey: "case:concept:name", activityKeyMode: "default", activityKey: "concept:name", timestampKeyMode: "default", timestampKey: "time:timestamp"};
}
export interface TableVariantsEnvelope extends Omit<TableRepetitionEnvelope, "family"> {
  family: "TABLE_VARIANTS_FILTER"; operation: TableVariantsOperation | null; timestamp_key: ReferenceLogFilterNativeString;
  variants_table_json: ReferenceLogFilterNativeString | null; variants_source_unchanged: boolean | null;
}
export interface TableVariantsRun {value: TableVariantsEnvelope; sourceFile: File; variantsFile: File | null; fields: Array<[string, string]>; raw: string; responseBytes: Uint8Array<ArrayBuffer>;}
export function isTableVariantsEnvelope(value: unknown): value is TableVariantsEnvelope {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  if (record.family !== "TABLE_VARIANTS_FILTER" || Object.keys(record).length !== 18 ||
    !(record.operation === null || TABLE_VARIANTS_OPERATIONS.includes(record.operation as TableVariantsOperation)) ||
    !isOriginalEmbeddingFilterNativeString(record.timestamp_key) ||
    !(record.variants_table_json === null || isOriginalEmbeddingFilterNativeString(record.variants_table_json)) ||
    !(record.variants_source_unchanged === null || typeof record.variants_source_unchanged === "boolean")) return false;
  const {family: _family, operation: _operation, timestamp_key: _time, variants_table_json: _variants, variants_source_unchanged: _unchanged, ...rest} = record;
  return isTableRepetitionEnvelope({...rest, family: "TABLE_ATTRIBUTE_REPETITION"});
}
export function tableVariantsFields(form: TableVariantsForm): Array<[string, string]> {
  if (!["pandas", "polars"].includes(form.engine) || !tableVariantsOperations(form.engine).includes(form.operation)) throw new Error("Choose an available variants filter.");
  if (![form.caseKeyMode, form.activityKeyMode, form.timestampKeyMode].every(mode => mode === "default" || mode === "custom")) throw new Error("Choose default or literal columns.");
  const fields: Array<[string, string]> = [["engine", form.engine], ["operation", form.operation], ["positive", String(form.positive)]];
  if (form.caseKeyMode === "custom") fields.push(["case_id_key", form.caseKey]);
  if (form.activityKeyMode === "custom") fields.push(["activity_key", form.activityKey]);
  if (form.operation === "membership") {
    if (!["list", "tuple"].includes(form.sequenceKind)) throw new Error("Choose list or tuple sequences.");
    fields.push(["admitted_json", form.admittedJson], ["sequence_kind", form.sequenceKind]);
  } else fields.push(["criterion", form.criterion]);
  if (form.engine === "polars" && form.operation === "top_k" && form.timestampKeyMode === "custom") fields.push(["timestamp_key", form.timestampKey]);
  return fields;
}
export async function requestTableVariants(sourceFile: File, form: TableVariantsForm, variantsFile: File | null = null, signal?: AbortSignal): Promise<TableVariantsRun> {
  signal?.throwIfAborted(); if (!(sourceFile instanceof File)) throw new Error("Choose an actual native EventTable JSON file.");
  if (variantsFile !== null && (!(variantsFile instanceof File) || form.engine !== "pandas")) throw new Error("A supplied variants table requires pandas variants filtering.");
  const fields = tableVariantsFields(form), body = new FormData(); body.append("source_file", sourceFile);
  if (variantsFile) body.append("variants_file", variantsFile);
  for (const [key, value] of fields) appendNativeMultipartField(body, key, value);
  const response = await backendFetch("/api/reference-table-variants", {method: "POST", body, signal}); signal?.throwIfAborted();
  if (response.status !== 200 && response.status !== 400) throw new Error(`Table variants filtering returned HTTP ${response.status}.`);
  const responseBytes = new Uint8Array(await response.arrayBuffer());
  try {new TextDecoder("utf-8", {fatal: true}).decode(responseBytes);} catch {throw new Error("The table variants response is not safe JSON UTF-8.");}
  const value = await readNativeResponse(new Response(responseBytes), {path: "/api/reference-table-variants", fields, reference: {name: sourceFile.name, size: sourceFile.size, last_modified: sourceFile.lastModified}});
  signal?.throwIfAborted();
  if (!isTableVariantsEnvelope(value) || value.http_status !== response.status) throw new Error("The table variants response has an invalid native envelope.");
  if (value.engine !== form.engine && !(value.http_status === 400 && value.engine === null) || value.operation !== form.operation && !(value.http_status === 400 && value.operation === null)) throw new Error("The table variants response belongs to another engine or filter.");
  return {value, sourceFile, variantsFile, fields, raw: nativeResponseJson(value), responseBytes};
}
export function tableVariantsArtifactFile(run: TableVariantsRun, kind: "source" | "result" | "variants"): File {
  const value = kind === "source" ? run.value.source_table_json : kind === "variants" ? run.value.variants_table_json : run.value.filtered_table_json;
  if (value === null || kind === "result" && (run.value.http_status !== 200 || run.value.error !== null)) throw new Error("The native table artifact is unavailable.");
  return new File([referenceLogFilterStringBytes(value)], `${kind}-${run.value.engine ?? "table"}-variants-table.json`, {type: "application/json"});
}
export function tableVariantsResponseFile(run: TableVariantsRun): File {return new File([run.responseBytes], "table-variants-response.json", {type: "application/json"});}
export function tableVariantsSourceExample(engine: TableRepetitionEngine): File {return tableTimestampSourceExample(engine);}
export function tableVariantsMappingExample(): File {
  const tuple = (values: string[]) => ({container: "tuple", children: values.map((value, i) => ({key: String(i), value}))});
  return new File([JSON.stringify({columns: ["variant"], dtypes: ["object"], rows: [[tuple(["A", "X", "B"])], [tuple(["A", "B"])]],
    index: {class: "Index", dtype: "object", names: ["case:concept:name"], labels: ["c1", "c2"].map(value => ({kind: "scalar", value: {value}}))}}, null, 2)], "pandas-case-variant-mapping.json", {type: "application/json"});
}
export interface TableVariantsIdentity {source: File | null; variants: File | null; form: TableVariantsForm;}
export class TableVariantsRequestGate {
  private generation = 0;
  issue(value: TableVariantsIdentity) {return {generation: ++this.generation, source: value.source, variants: value.variants, form: JSON.stringify(value.form)};}
  invalidate() {++this.generation;}
  accepts(ticket: ReturnType<TableVariantsRequestGate["issue"]>, value: TableVariantsIdentity, signal?: AbortSignal) {return !signal?.aborted && ticket.generation === this.generation && ticket.source === value.source && ticket.variants === value.variants && ticket.form === JSON.stringify(value.form);}
}
