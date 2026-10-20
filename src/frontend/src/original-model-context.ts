import {appendNativeMultipartField, nativeStringBytes} from "./native-multipart";
import {nativeCell, nativeJson, parseNativeJson} from "./native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "./native-response";
import {diagnosticCsv, nativeObjectTables, type DiagnosticTable} from "./model-diagnostics";
import {isOriginalQualityNativeType} from "./original-quality-workbench";
import {isOriginalTemporalType} from "./original-temporal-workbench";
import {ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET} from "./original-petri-analysis-workbench";
import {originalEditDistanceWireFilename} from "./original-edit-distance-workbench";
import {isOriginalEmbeddingFilterNativeString as isString, originalEmbeddingFilterStringBytes as stringBytes, originalEmbeddingFilterStringText as stringText, type OriginalEmbeddingFilterNativeString as NativeString} from "./original-embedding-filter-workbench";

export const ORIGINAL_MODEL_CONTEXT_VARIANTS = ["petri_net", "declare", "log_skeleton", "temporal_profile"] as const;
export type OriginalModelContextVariant = typeof ORIGINAL_MODEL_CONTEXT_VARIANTS[number];
export const ORIGINAL_MODEL_CONTEXT_FORMATS = ["auto", "pnml", "native_net", "native_document", "declare_json", "log_skeleton_json", "temporal_profile_json"] as const;
export type OriginalModelContextFormat = typeof ORIGINAL_MODEL_CONTEXT_FORMATS[number];
export const ORIGINAL_MODEL_CONTEXT_FILE_FIELDS = ["context_options_file", "initial_marking_file", "final_marking_file", "pnml_import_options_file"] as const;
export type OriginalModelContextFileField = typeof ORIGINAL_MODEL_CONTEXT_FILE_FIELDS[number];
export const ORIGINAL_MODEL_CONTEXT_ARTIFACTS = ["source_model_json", "model_after_json", "effective_model_json", "effective_model_after_json", "initial_marking_json", "final_marking_json", "native_text_bytes", "legacy_result_json", "source_pnml", "effective_pnml"] as const;
export const ORIGINAL_MODEL_CONTEXT_IMPORT_FIELDS = ["infer_initial_marking", "infer_final_marking", "import_stochastic_map", "require_workflow_boundaries", "public_marking_semantics"] as const;
export interface OriginalModelContextForm {variant: OriginalModelContextVariant; modelFormat: OriginalModelContextFormat; fields: Record<string, string>;}
export type OriginalModelContextUploads = Partial<Record<OriginalModelContextFileField, File>>;
export const defaultOriginalModelContextForm = (variant: OriginalModelContextVariant = "petri_net"): OriginalModelContextForm => ({variant, modelFormat: "auto", fields: {}});
export const originalModelContextFormats = (variant: OriginalModelContextVariant): readonly OriginalModelContextFormat[] => variant === "petri_net" ? ["auto", "pnml", "native_net", "native_document"] : ["auto", ({declare: "declare_json", log_skeleton: "log_skeleton_json", temporal_profile: "temporal_profile_json"} as const)[variant]];
export interface OriginalModelContextPart {name: NativeString; filename: NativeString; bytes: NativeString; file: boolean;}
export interface OriginalModelContextEnvelope extends Record<string, unknown> {
 family: "LLM_CONTEXT"; operation: "original_model"; variant: NativeString; native_entered: boolean; native_returned: boolean; executed: boolean;
 error: null | {phase: string; type: NativeString; message: NativeString}; input_order: string;
 input_context: {model_format: string; detected_format: string; model_type: string | null; filename: NativeString; original_model_bytes: number};
 original_model: NativeString; options: {include_header: boolean} | null; pnml_import_options: Record<string, boolean> | null;
 snapshots: {model_before: unknown; model_after: unknown; effective_model_before: unknown; effective_model_after: unknown}; model_unchanged: boolean | null; effective_model_unchanged: boolean | null;
 markings: {initial_before: number[] | null; initial_after: number[] | null; final_before: number[] | null; final_after: number[] | null; initial_origin: string | null; final_origin: string | null; initial_override: number[] | null; final_override: number[] | null; unchanged: boolean | null};
 result: null | {variant: NativeString; text: NativeString; characters: number; truncated: false}; text_bytes: number | null; provider_text: string | null;
 original_inputs: OriginalModelContextPart[]; query_inputs: OriginalModelContextPart[]; artifacts: Record<string, NativeString | null>;
}
export interface OriginalModelContextRun {value: OriginalModelContextEnvelope; raw: string; fields: Array<[string, string]>; tables: DiagnosticTable[]; http_status: number; input: {model: File; files: Record<string, File>; sha256: Record<string, string>};}
const object = (v: unknown): v is Record<string, unknown> => v !== null && typeof v === "object" && !Array.isArray(v);
const exact = (v: Record<string, unknown>, keys: readonly string[]) => Object.keys(v).length === keys.length && keys.every(key => Object.hasOwn(v, key));
const sameBytes = (a: Uint8Array, b: Uint8Array) => a.length === b.length && a.every((byte, index) => byte === b[index]);
const nullableFlag = (v: unknown) => v === null || typeof v === "boolean";
const integer = (holder: object, key: string | number, signed = false, bits?: number): boolean => {
 if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return false;
 const token = String(nativeCell(holder, key)); if (!(signed ? /^-?(?:0|[1-9]\d*)$/ : /^(?:0|[1-9]\d*)$/).test(token)) return false;
 if (bits === undefined) return true; const number = BigInt(token), limit = 1n << BigInt(bits - (signed ? 1 : 0)); return number >= (signed ? -limit : 0n) && number < limit;
};
const marking = (v: unknown): v is number[] => Array.isArray(v) && v.every((_, index) => integer(v, index, true));
const byteKey = (v: NativeString) => Array.from(stringBytes(v), byte => byte.toString(16).padStart(2, "0")).join("");
function mapEntries(v: unknown, check: (entry: unknown) => boolean): boolean {
 if (!Array.isArray(v)) return false; const keys = new Set<string>();
 return v.every(entry => {if (!object(entry) || !exact(entry, ["key", "value"]) || !isString(entry.key) || !check(entry.value)) return false; const key = byteKey(entry.key); if (keys.has(key)) return false; keys.add(key); return true;});
}
export function isOriginalContextModel(type: string, v: unknown): boolean {
 if (type === "PetriNet" || type === "PnmlDocument") return isOriginalQualityNativeType(type, v);
 if (type === "TemporalProfile") return isOriginalTemporalType(type, v);
 if (!object(v)) return false;
 if (type === "DeclareModel") return exact(v, ["rules"]) && mapEntries(v.rules, rules => Array.isArray(rules) && rules.every(rule => object(rule) && exact(rule, ["first", "second", "binary", "support", "confidence"]) && isString(rule.first) && isString(rule.second) && typeof rule.binary === "boolean" && integer(rule, "support", true, 32) && integer(rule, "confidence", true, 32)));
 const names = ["equivalence", "always_after", "always_before", "never_together", "directly_follows", "activ_freq"];
 return type === "LogSkeletonModel" && exact(v, names) && names.slice(0, 5).every(name => Array.isArray(v[name]) && (v[name] as unknown[]).every(pair => object(pair) && exact(pair, ["source", "target"]) && isString(pair.source) && isString(pair.target))) && mapEntries(v.activ_freq, values => Array.isArray(values) && values.every((_, index) => integer(values, index, false, 64)));
}
/** Reject decoded duplicate object keys without losing native integer spellings. */
function exactJson(raw: string): unknown {
 const value = parseNativeJson(raw), tokens = raw.match(/"(?:[^"\\]|\\.)*"|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?|true|false|null|[{}\[\],:]/g) ?? []; let at = 0;
 const walk = (): void => {const token = tokens[at++]; if (token === "{") {const seen = new Set<string>(); while (tokens[at] !== "}") {const name = JSON.parse(tokens[at++]) as string; if (seen.has(name)) throw new Error(`Duplicate decoded JSON key: ${name}`); seen.add(name); ++at; walk(); if (tokens[at] !== ",") break; ++at;} ++at;} else if (token === "[") {while (tokens[at] !== "]") {walk(); if (tokens[at] !== ",") break; ++at;} ++at;}};
 walk(); return value;
}
export function originalContextHeaderOptions(raw: string): {include_header: boolean} {const v = exactJson(raw); if (!object(v) || !exact(v, ["include_header"]) || typeof v.include_header !== "boolean") throw new Error("Original model options require exactly {include_header:boolean}."); return v as {include_header: boolean};}
export function originalContextMarking(raw: string): number[] {const v = exactJson(raw); if (!marking(v)) throw new Error("Markings require exact signed JSON integer arrays, with no machine-width narrowing."); return v;}
export function originalContextImportOptions(raw: string): Record<string, boolean> {const v = exactJson(raw); if (!object(v) || !exact(v, ORIGINAL_MODEL_CONTEXT_IMPORT_FIELDS) || !ORIGINAL_MODEL_CONTEXT_IMPORT_FIELDS.every(key => typeof v[key] === "boolean")) throw new Error("PNML import options require exactly all five native boolean fields."); return v as Record<string, boolean>;}
export function originalModelContextFields(form: OriginalModelContextForm, uploads: OriginalModelContextUploads = {}): Array<[string, string]> {
 if (!ORIGINAL_MODEL_CONTEXT_VARIANTS.includes(form.variant) || !originalModelContextFormats(form.variant).includes(form.modelFormat)) throw new Error("Choose a compatible original model kind and format.");
 const allowed = ["include_header", "response_header", "context_options_json", "initial_marking_json", "final_marking_json", "pnml_import_options_json"], has = (key: string) => Object.hasOwn(form.fields, key);
 if (Object.keys(form.fields).some(key => !allowed.includes(key)) || Object.keys(uploads).some(key => !ORIGINAL_MODEL_CONTEXT_FILE_FIELDS.includes(key as OriginalModelContextFileField))) throw new Error("Unknown original model context control.");
 for (const file of Object.values(uploads)) if (!(file instanceof File)) throw new Error("Original context uploads require actual Files.");
 for (const prefix of ["context_options", "initial_marking", "final_marking", "pnml_import_options"] as const) if (has(`${prefix}_json`) && uploads[`${prefix}_file`]) throw new Error(`Choose JSON or File for ${prefix}; supplied empty fields also count.`);
 if (has("include_header") && has("response_header") || (has("context_options_json") || uploads.context_options_file) && (has("include_header") || has("response_header"))) throw new Error("Header aliases and complete header-options carriers conflict by presence.");
 for (const name of ["include_header", "response_header"]) if (has(name) && !["true", "false", "1", "0"].includes(form.fields[name])) throw new Error("Header controls require true, false, 1 or 0.");
 if (has("context_options_json")) originalContextHeaderOptions(form.fields.context_options_json);
 for (const role of ["initial", "final"] as const) if (has(`${role}_marking_json`)) originalContextMarking(form.fields[`${role}_marking_json`]);
 if (has("pnml_import_options_json")) originalContextImportOptions(form.fields.pnml_import_options_json);
 if (form.variant !== "petri_net" && (Object.keys(form.fields).some(key => key.includes("marking") || key.startsWith("pnml_")) || Object.keys(uploads).some(key => key.includes("marking") || key.startsWith("pnml_")))) throw new Error("Markings and PNML import options apply only to original Petri models.");
 if (form.modelFormat === "native_net" && !["initial", "final"].every(role => has(`${role}_marking_json`) || uploads[`${role}_marking_file` as OriginalModelContextFileField])) throw new Error("A raw native net requires both explicit initial and final marking carriers, including explicit [].");
 if (form.variant === "petri_net" && !["auto", "pnml"].includes(form.modelFormat) && (has("pnml_import_options_json") || uploads.pnml_import_options_file)) throw new Error("PNML import options require an original PNML model.");
 return [["variant", form.variant], ["model_format", form.modelFormat], ...Object.entries(form.fields)];
}
function scalarUnicode(value: string): boolean {for (let i = 0; i < value.length; ++i) {const code = value.charCodeAt(i); if (code >= 0xd800 && code <= 0xdbff) {const next = value.charCodeAt(++i); if (!(next >= 0xdc00 && next <= 0xdfff)) return false;} else if (code >= 0xdc00 && code <= 0xdfff) return false;} return true;}
export function isOriginalModelContextEnvelope(v: unknown): v is OriginalModelContextEnvelope {
 if (!object(v) || v.family !== "LLM_CONTEXT" || v.operation !== "original_model" || !isString(v.variant) || !["native_entered", "native_returned", "executed"].every(key => typeof v[key] === "boolean") || v.executed !== v.native_returned || typeof v.input_order !== "string" || !object(v.input_context) || !isString(v.original_model)) return false;
 const context = v.input_context;
 if (typeof context.model_format !== "string" || typeof context.detected_format !== "string" || !(context.model_type === null || ["DeclareModel", "LogSkeletonModel", "TemporalProfile", "PetriNet", "PnmlDocument"].includes(String(context.model_type))) || !isString(context.filename) || !integer(context, "original_model_bytes", false, 64)) return false;
 if (!(v.error === null || object(v.error) && ["transport_error", "configuration_error", "model_parse_error", "native_error"].includes(String(v.error.phase)) && isString(v.error.type) && isString(v.error.message)) || !(v.options === null || object(v.options) && exact(v.options, ["include_header"]) && typeof v.options.include_header === "boolean") || !(v.pnml_import_options === null || object(v.pnml_import_options) && exact(v.pnml_import_options, ORIGINAL_MODEL_CONTEXT_IMPORT_FIELDS) && Object.values(v.pnml_import_options).every(item => typeof item === "boolean"))) return false;
 if (!object(v.snapshots) || !nullableFlag(v.model_unchanged) || !nullableFlag(v.effective_model_unchanged) || !object(v.markings) || !nullableFlag(v.markings.unchanged)) return false;
 for (const key of ["model_before", "model_after", "effective_model_before", "effective_model_after"]) {const value = v.snapshots[key], type = key.startsWith("effective") && stringText(v.variant) === "petri_net" ? "PnmlDocument" : String(context.model_type); if (!(value === null || isOriginalContextModel(type, value))) return false;}
 for (const key of ["initial_before", "initial_after", "final_before", "final_after", "initial_override", "final_override"]) if (!(v.markings[key] === null || marking(v.markings[key]))) return false;
 for (const key of ["initial_origin", "final_origin"]) if (!(v.markings[key] === null || ["explicit_json", "explicit_file", "native_document", "pnml"].includes(String(v.markings[key])))) return false;
 for (const key of ["original_inputs", "query_inputs"]) if (!Array.isArray(v[key]) || !(v[key] as unknown[]).every(part => object(part) && isString(part.name) && isString(part.filename) && isString(part.bytes) && typeof part.file === "boolean")) return false;
 if (!object(v.artifacts) || !ORIGINAL_MODEL_CONTEXT_ARTIFACTS.every(key => Object.hasOwn(v.artifacts as object, key) && Object.hasOwn(v.artifacts as object, `${key}_error`)) || !Object.values(v.artifacts).every(item => item === null || isString(item))) return false;
 if (v.result === null) return v.native_returned === false && v.error !== null && v.text_bytes === null && v.provider_text === null;
 if (!object(v.result) || !isString(v.result.variant) || !isString(v.result.text) || !integer(v.result, "characters", false, 64) || v.result.truncated !== false || !integer(v, "text_bytes", false, 64) || !(v.provider_text === null || typeof v.provider_text === "string" && scalarUnicode(v.provider_text))) return false;
 const bytes = stringBytes(v.result.text);
 if (BigInt(String(nativeCell(v, "text_bytes"))) !== BigInt(bytes.length) || BigInt(String(nativeCell(v.result, "characters"))) !== BigInt(bytes.reduce((count, byte) => count + ((byte & 0xc0) === 0x80 ? 0 : 1), 0)) || typeof v.provider_text === "string" && !sameBytes(nativeStringBytes(v.provider_text), bytes)) return false;
 return v.native_entered === true && v.native_returned === true && v.error === null && v.options !== null;
}
const fileHash = async (bytes: Uint8Array<ArrayBuffer>) => Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), byte => byte.toString(16).padStart(2, "0")).join("");
export async function runOriginalModelContext(model: File, form: OriginalModelContextForm, uploads: OriginalModelContextUploads = {}, signal?: AbortSignal): Promise<OriginalModelContextRun> {
 if (!(model instanceof File)) throw new Error("Original model context requires the actual original model File.");
 const fields = originalModelContextFields(form, uploads), files: Record<string, File> = {model_file: model, ...uploads}, bytes: Record<string, Uint8Array<ArrayBuffer>> = {}, hashes: Record<string, string> = {};
 await Promise.all(Object.entries(files).map(async ([key, file]) => {bytes[key] = new Uint8Array(await file.arrayBuffer()); hashes[key] = await fileHash(bytes[key]);}));
 signal?.throwIfAborted(); const body = new FormData(); body.append("model_file", model); fields.forEach(([key, value]) => appendNativeMultipartField(body, key, value)); Object.entries(uploads).forEach(([key, file]) => body.append(key, file));
 const expected = new Map<string, {bytes: Uint8Array; file: boolean; filename: string}>();
 // File.name is local metadata. Compare the server echo with the browser's
 // actual Content-Disposition filename spelling and filename presence instead.
 for (const [key, entry] of body) {const filename = typeof entry === "string" ? null : await originalEditDistanceWireFilename(entry.name), raw = fields.find(([name]) => name === key)?.[1]; expected.set(key, {bytes: bytes[key] ?? nativeStringBytes(raw ?? ""), file: filename !== null, filename: filename ?? ""});}
 signal?.throwIfAborted();
 const response = await fetch("/api/llm-context", {method: "POST", body, signal}), value = await readNativeResponse(response, {path: "/api/llm-context", fields}); signal?.throwIfAborted();
 if (!isOriginalModelContextEnvelope(value)) throw new Error(`Invalid original-model context envelope (HTTP ${response.status}).`);
 if (!sameBytes(stringBytes(value.variant), nativeStringBytes(form.variant)) || value.input_context.model_format !== form.modelFormat || !sameBytes(stringBytes(value.input_context.filename), nativeStringBytes(expected.get("model_file")!.filename)) || !sameBytes(stringBytes(value.original_model), bytes.model_file) || BigInt(String(nativeCell(value.input_context, "original_model_bytes"))) !== BigInt(model.size)) throw new Error("Original context response does not match the submitted model File and format.");
 const parts = [...value.original_inputs, ...value.query_inputs];
 if (parts.length !== expected.size || [...expected].some(([name, actual]) => {const found = parts.filter(part => sameBytes(stringBytes(part.name), nativeStringBytes(name))); return found.length !== 1 || found[0].file !== actual.file || !sameBytes(stringBytes(found[0].filename), nativeStringBytes(actual.filename)) || !sameBytes(stringBytes(found[0].bytes), actual.bytes);})) throw new Error("Original model context provenance does not match every submitted File and field.");
 if (value.options && !uploads.context_options_file) {const raw = fields.find(([key]) => key === "context_options_json")?.[1], scalar = fields.find(([key]) => key === "include_header" || key === "response_header")?.[1], include = raw === undefined ? scalar === undefined || ["true", "1"].includes(scalar) : originalContextHeaderOptions(raw).include_header; if (value.options.include_header !== include) throw new Error("Original context header options do not match.");}
 for (const role of ["initial", "final"] as const) {const raw = fields.find(([key]) => key === `${role}_marking_json`)?.[1], returned = value.markings[`${role}_override`]; if (raw !== undefined && returned !== null) {const supplied = originalContextMarking(raw); if (returned.length !== supplied.length || returned.some((_, index) => BigInt(String(nativeCell(returned, index))) !== BigInt(String(nativeCell(supplied, index))))) throw new Error("Original context explicit marking does not match its exact submitted values.");}}
 if (value.result && !sameBytes(stringBytes(value.result.variant), nativeStringBytes(form.variant)) || !response.ok && value.error === null) throw new Error("Original context result/status does not match its native operation.");
 return {value, raw: nativeResponseJson(value), fields, tables: originalModelContextTables(value, fields), http_status: response.status, input: {model, files, sha256: hashes}};
}
export function originalModelContextTables(value: OriginalModelContextEnvelope, fields?: Array<[string, string]>): DiagnosticTable[] {
 const tables = nativeObjectTables(value);
 for (const [phase, model] of Object.entries(value.snapshots)) {
  if (!object(model)) continue;
  if (Array.isArray(model.rules)) {const rows: unknown[][] = [], records: unknown[] = []; for (const group of model.rules as Array<{key: NativeString; value: Array<Record<string, unknown>>}>) group.value.forEach((rule, index) => {rows.push([group.key, index, rule.first, rule.second, rule.binary, nativeCell(rule, "support"), nativeCell(rule, "confidence")]); records.push(rule);}); tables.push({id: `${phase}/all_declare_rules`, title: `${phase}: every stored DECLARE rule`, columns: ["family", "index", "first", "second", "binary", "support", "confidence"], rows, records});}
  for (const key of ["equivalence", "always_after", "always_before", "never_together", "directly_follows", "activ_freq", "entries", "transitions"]) if (Array.isArray(model[key])) for (const table of nativeObjectTables({[`${phase}/${key}`]: model[key]})) if (table.id !== "summary") tables.push(table);
 }
 if (fields) tables.push({id: "request", title: "Exact original-model context fields", columns: ["field", "value"], rows: fields, records: fields.map(([field, value]) => ({field, value}))});
 for (const table of tables) if (table.records) {const records = table.records; table.records = records.map((record, index) => record !== null && typeof record === "object" ? record : parseNativeJson(`{"value":${typeof record === "number" ? nativeCell(records, index) : nativeJson(record)}}`));}
 return tables;
}
export function originalModelContextAllTablesCsv(tables: DiagnosticTable[]): string {const rows: unknown[][] = []; for (const table of tables) {if (!table.rows.length) table.columns.forEach((column, index) => rows.push([table.id, table.title, null, index, column, null])); table.rows.forEach((row, index) => table.columns.forEach((column, cell) => rows.push([table.id, table.title, index, cell, column, row[cell]])));} return diagnosticCsv({id: "all_model_context_tables", title: "Every complete original-model context table", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});}
export function originalModelContextArtifacts(run: OriginalModelContextRun): Array<{name: string; bytes: Uint8Array<ArrayBuffer>; mime: string}> {return ORIGINAL_MODEL_CONTEXT_ARTIFACTS.flatMap(key => {const value = run.value.artifacts[key]; return isString(value) ? [{name: `minera-original-context-${key}.${key.endsWith("pnml") ? "pnml" : key === "native_text_bytes" ? "txt" : "json"}`, bytes: stringBytes(value), mime: key.endsWith("pnml") ? "application/xml" : key === "native_text_bytes" ? "application/octet-stream" : "application/json"}] : [];});}
export function originalModelContextReport(run: OriginalModelContextRun): string {return nativeJson({family: "ORIGINAL_MODEL_CONTEXT_REPORT", http_status: run.http_status, request: nativeResponseRequest(run.value) ?? {path: "/api/llm-context", fields: run.fields}, inputs: Object.fromEntries(Object.entries(run.input.files).map(([key, file]) => [key, {name: file.name, size: file.size, last_modified: file.lastModified, sha256: run.input.sha256[key]}])), native_result: run.value}, 2);}
export function originalModelContextPreview(value: OriginalModelContextEnvelope, maximum = 6000): {text: string; clipped: boolean; unicode: boolean} {const raw = value.provider_text; if (raw === null) {const bytes = value.result ? stringBytes(value.result.text) : new Uint8Array(), shown = bytes.subarray(0, Math.max(0, Math.floor(maximum / 2))); return {text: "hex:" + Array.from(shown, byte => byte.toString(16).padStart(2, "0")).join(""), clipped: bytes.length > shown.length, unicode: false};} const end = raw.length > maximum && raw.charCodeAt(maximum - 1) >= 0xd800 && raw.charCodeAt(maximum - 1) <= 0xdbff ? maximum - 1 : maximum; return {text: raw.slice(0, end), clipped: raw.length > end, unicode: true};}
export class OriginalModelContextRequestGate {
 private generation = 0;
 issue(model: File, form: OriginalModelContextForm, uploads: OriginalModelContextUploads = {}) {return {generation: ++this.generation, model, form: nativeJson(form), uploads: {...uploads}};}
 invalidate() {++this.generation;}
 accepts(ticket: ReturnType<OriginalModelContextRequestGate["issue"]>, model: File | null, form: OriginalModelContextForm, uploads: OriginalModelContextUploads = {}, signal?: AbortSignal): boolean {return !signal?.aborted && ticket.generation === this.generation && ticket.model === model && ticket.form === nativeJson(form) && ORIGINAL_MODEL_CONTEXT_FILE_FIELDS.every(key => ticket.uploads[key] === uploads[key]);}
}
export const ORIGINAL_MODEL_CONTEXT_EXAMPLES: Record<OriginalModelContextVariant, string> = {petri_net: '<pnml><net id="original"><page id="p"><place id="start"><initialMarking><text>1</text></initialMarking></place><place id="end"/><transition id="A"><name><text>A</text></name></transition><arc id="a" source="start" target="A"/><arc id="b" source="A" target="end"/></page><finalmarkings><marking><place idref="end"><text>1</text></place></marking></finalmarkings></net></pnml>', declare: '{"rules":[{"key":"response","value":[{"first":"A","second":"B","binary":true,"support":-1,"confidence":7}]}]}', log_skeleton: '{"equivalence":[{"source":"A","target":"B"}],"always_after":[],"always_before":[],"never_together":[],"directly_follows":[],"activ_freq":[{"key":"A","value":[0,2,2]}]}', temporal_profile: '{"entries":[{"source":"A","target":"B","count":18446744073709551615,"mean_seconds":-0.0,"standard_deviation_seconds":"NaN"}]}'};
export const ORIGINAL_CONTEXT_IMPORT_EXAMPLE = '{"infer_initial_marking":true,"infer_final_marking":true,"import_stochastic_map":false,"require_workflow_boundaries":true,"public_marking_semantics":false}';
export const ORIGINAL_CONTEXT_NATIVE_NET_EXAMPLE = ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET;
export const ORIGINAL_CONTEXT_NATIVE_DOCUMENT_EXAMPLE = nativeJson({net: parseNativeJson(ORIGINAL_CONTEXT_NATIVE_NET_EXAMPLE), initial_marking: [1, 0], final_marking: [0, 1], stochastic_map: null, place_ids: ["start", "end"], transition_ids: ["A"], initial_marking_keys: [true, false], final_marking_keys: [false, true], arc_occurrences: null}, 2);
