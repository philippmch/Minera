import {afterEach, describe, expect, it, vi} from "vitest";
import {ORIGINAL_OC_PLAYOUT_ARTIFACTS, ORIGINAL_OC_PLAYOUT_OPERATIONS, ORIGINAL_OC_PLAYOUT_OPTION_FIELDS, isOriginalOcPlayoutWorkbench, isOriginalOcPlayoutOcpn, isOriginalOcPlayoutCausalNet, isOriginalOcPlayoutOptions, isOriginalOcPlayoutOcel, isOriginalOcPlayoutResult, originalOcPlayoutAllTablesCsv, originalOcPlayoutArtifactFile, originalOcPlayoutModelFile, originalOcPlayoutOcelFile, originalOcPlayoutOcelValue, originalOcPlayoutOriginalBytes, originalOcPlayoutReport, originalOcPlayoutResultFile, originalOcPlayoutSelectedRecordsJson, originalOcPlayoutStringBytes, originalOcPlayoutStringText, originalOcPlayoutTables, originalOcPlayoutTableFileName, defaultOriginalOcPlayoutForm, runOriginalOcPlayoutWorkbench, type OriginalOcPlayoutRun, type OriginalOcPlayoutWorkbench, type OriginalOcPlayoutBytes} from "../src/original-object-centric-playout-workbench";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "../src/native-response";
import {nativeStringBytes} from "../src/native-multipart";
import {diagnosticCsv, type DiagnosticTable} from "../src/model-diagnostics";

// Build-only template. The publisher copies these exact bytes after authorization.
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {createHash} = await nodeModule("node:crypto") as {createHash(name: string): {update(value: Uint8Array | string): {digest(kind: "hex"): string}}};
const {readFileSync} = await nodeModule("node:fs") as {readFileSync: {(path: string): Uint8Array; (path: string, encoding: "utf8"): string}};
const {dirname, resolve} = await nodeModule("node:path") as {dirname(path: string): string; resolve(...paths: string[]): string};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url: string): string};
const environment = (globalThis as unknown as {process: {env: Record<string, string | undefined>}}).process.env;
interface Bytes {encoding: "utf8" | "hex"; data: string; bytes: number; sha256: string;}
interface Part {name: string; filename: string | null; filename_attribute_present: boolean; bytes: number; sha256: string; data_hex: string;}
interface Entry {id: string; method: string; path: string; family: string; operation: unknown; coverage_tags: unknown; request: {headers: Record<string, string>; body: Bytes; parts: Part[]}; response: Bytes & {status: number}; artifact_expectations: Record<string, {bytes: number; sha256: string}>;}
interface Manifest {complete: boolean; feature: number; schema_version: number; entries: Array<{id: string; record_index: number}>; bundle: {file_name: string; bytes: number; sha256: string}; counts: {entries: number; owned_responses: number; outer_errors: number};}
const fixtureDirectory = dirname(fileURLToPath(import.meta.url));
const manifestPath = environment.MINERA_OC_PLAYOUT_FIXTURE_MANIFEST ?? resolve(fixtureDirectory, "fixtures/original-object-centric-playout-workbench-manifest.json");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as Manifest;
const bundleBytes = readFileSync(resolve(dirname(manifestPath), manifest.bundle.file_name));
const digest = (value: Uint8Array | string): string => createHash("sha256").update(value).digest("hex");
if (manifest.complete !== true || manifest.feature !== 67 || manifest.schema_version !== 1 || !manifest.entries.length || digest(bundleBytes) !== manifest.bundle.sha256 || bundleBytes.length !== manifest.bundle.bytes) throw new Error("Complete source-bound actual object-centric playout fixtures are required.");
const bundle = JSON.parse(new TextDecoder("utf-8", {fatal: true}).decode(bundleBytes)) as {schema_version: number; feature: number; entries: Entry[]};
if (bundle.schema_version !== 1 || bundle.feature !== 67 || bundle.entries.length !== manifest.entries.length || !manifest.entries.every((entry, index) => entry.record_index === index && bundle.entries[index].id === entry.id)) throw new Error("Compact actual fixture index changed.");
const entries = bundle.entries;
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const hex = (bytes: Uint8Array): string => Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("");
function fromHex(raw: string): Uint8Array {if (!/^(?:[a-f0-9]{2})*$/i.test(raw)) throw new Error("Invalid exact fixture hex bytes."); return Uint8Array.from(raw.match(/../g) ?? [], pair => parseInt(pair, 16));}
function bytesOf(value: Bytes): Uint8Array {const bytes = value.encoding === "hex" ? fromHex(value.data) : nativeStringBytes(value.data); expect(bytes.length).toBe(value.bytes); expect(digest(bytes)).toBe(value.sha256); return bytes;}
function scalarText(bytes: Uint8Array): string | null {try {return new TextDecoder("utf-8", {fatal: true, ignoreBOM: true}).decode(bytes);} catch {return null;}}
function rawResponse(entry: Entry): string {const raw = scalarText(bytesOf(entry.response)); if (raw === null) throw new Error("Actual response is not scalar UTF8 JSON."); return raw;}
function requestFields(entry: Entry): Array<[string, string]> {if (!Object.values(entry.request.headers).some(value => /multipart\/form-data;.*boundary=/i.test(value))) return []; return entry.request.parts.filter(part => part.name !== "model_file").flatMap(part => {const value = scalarText(fromHex(part.data_hex)); return value === null ? [] : [[part.name, value] as [string, string]];});}
function modelFile(entry: Entry): File | null {const part = entry.request.parts.find(item => item.name === "model_file"); return part ? new File([fromHex(part.data_hex).buffer as ArrayBuffer], part.filename ?? "", {type: "application/json"}) : null;}
function verifyCarrier(carrier: OriginalOcPlayoutBytes, bytes: Uint8Array): void {expect(hex(originalOcPlayoutOriginalBytes(carrier))).toBe(hex(bytes)); expect(String(nativeCell(carrier, "bytes"))).toBe(String(bytes.length)); expect(carrier.sha256).toBe(digest(bytes));}

// An independent lexical walk checks every original JSON numeric token by path.
type Numeric = {path: Array<string | number>; token: string};
function numericTokens(raw: string): Numeric[] {
 const result: Numeric[] = []; let position = 0;
 const whitespace = () => {while (/\s/.test(raw[position] ?? "") && position < raw.length) ++position;};
 const string = (): string => {const start = position++; while (position < raw.length) {const char = raw[position++]; if (char === "\\") ++position; else if (char === '"') return JSON.parse(raw.slice(start, position)) as string;} throw new Error("Unterminated fixture JSON string.");};
 const value = (path: Array<string | number>): void => {whitespace(); const char = raw[position]; if (char === '"') {string(); return;} if (char === "{") {++position; whitespace(); if (raw[position] === "}") {++position; return;} for (;;) {whitespace(); const key = string(); whitespace(); if (raw[position++] !== ":") throw new Error("Invalid fixture object separator."); value([...path, key]); whitespace(); const separator = raw[position++]; if (separator === "}") return; if (separator !== ",") throw new Error("Invalid fixture object separator.");}} if (char === "[") {++position; whitespace(); if (raw[position] === "]") {++position; return;} let index = 0; for (;;) {value([...path, index++]); whitespace(); const separator = raw[position++]; if (separator === "]") return; if (separator !== ",") throw new Error("Invalid fixture array separator.");}} const tail = raw.slice(position), literal = /^(?:true|false|null)/.exec(tail); if (literal) {position += literal[0].length; return;} const numeric = /^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/.exec(tail); if (!numeric) throw new Error("Invalid fixture JSON value."); result.push({path, token: numeric[0]}); position += numeric[0].length;};
 value([]); whitespace(); if (position !== raw.length) throw new Error("Trailing fixture JSON bytes."); return result;
}
function numericAt(value: unknown, path: Array<string | number>): string {let holder: unknown = value; for (const key of path.slice(0, -1)) {if (!holder || typeof holder !== "object") throw new Error("Numeric path lost its native holder."); holder = (holder as Record<string, unknown>)[key];} if (!holder || typeof holder !== "object") throw new Error("Numeric path lost its native holder."); return String(nativeCell(holder, path[path.length - 1]));}
function verifyNumeric(raw: string, value: unknown): number {const tokens = numericTokens(raw); for (const item of tokens) expect(numericAt(value, item.path), JSON.stringify(item.path)).toBe(item.token); expect(numericTokens(nativeJson(value))).toEqual(tokens); return tokens.length;}
function sameNative(actual: unknown, expected: unknown): void {expect(nativeJson(actual)).toBe(nativeJson(expected));}
function parseCsv(raw: string): string[][] {const rows: string[][] = []; let row: string[] = [], cell = "", quoted = false; for (let index = 0; index < raw.length; ++index) {const char = raw[index]; if (quoted) {if (char === '"' && raw[index + 1] === '"') {cell += '"'; ++index;} else if (char === '"') quoted = false; else cell += char;} else if (char === '"') quoted = true; else if (char === ",") {row.push(cell); cell = "";} else if (char === "\r" && raw[index + 1] === "\n") {row.push(cell); rows.push(row); row = []; cell = ""; ++index;} else cell += char;} if (quoted || cell || row.length) throw new Error("Incomplete CSV cell or row."); return rows;}
const cellText = (value: unknown): string => value === undefined ? "" : typeof value === "string" ? value : typeof value === "object" ? nativeJson(value) : String(value);
function verifyTables(value: OriginalOcPlayoutWorkbench, tables: DiagnosticTable[]): void {
 const selected = parseNativeJson(originalOcPlayoutSelectedRecordsJson(tables)); if (!object(selected)) throw new Error("Selected records must be one object containing every table.");
 expect(Object.keys(selected)).toEqual(tables.map(table => table.id));
 const flattened: string[][] = [["table_id", "table_title", "row_index", "column_index", "column", "value"]];
 for (const table of tables) {
  sameNative(selected[table.id], table.records ?? []);
  expect(parseCsv(diagnosticCsv(table))).toEqual([table.columns, ...table.rows.map(row => row.map(cellText))]);
  const fileName = originalOcPlayoutTableFileName(table.id); expect(fileName).not.toMatch(/[\\/\r\n\0]/);
  if (!table.rows.length) table.columns.forEach((column, index) => flattened.push([table.id, table.title, "", String(index), column, ""]));
  table.rows.forEach((row, rowIndex) => table.columns.forEach((column, columnIndex) => flattened.push([table.id, table.title, String(rowIndex), String(columnIndex), column, cellText(row[columnIndex])])));
 }
 expect(parseCsv(originalOcPlayoutAllTablesCsv(tables))).toEqual(flattened);
 const model = value.model?.ocpn ?? value.model?.causal_net;
 if (model) {const table = tables.find(item => item.id === "complete_original_model"); expect(table).toBeDefined(); sameNative(table!.records, [model]);}
 const log = originalOcPlayoutOcelValue(value);
 if (log) for (const [key, records] of Object.entries(log)) if (Array.isArray(records)) {const table = tables.find(item => item.id === `complete_ocel_${key}`); expect(table).toBeDefined(); sameNative(table!.rows, records.map((record, index) => [index, record]));}
 if (value.result?.type === "ocpn_playout_result") for (const key of ["sequences", "traces"] as const) {const table = tables.find(item => item.id === `complete_${key}`); expect(table).toBeDefined(); sameNative(table!.rows, (value.result.value[key] as unknown[]).map((record, index) => [index, record]));}
}
function verifyNativeStrings(value: unknown): void {if (Array.isArray(value)) {value.forEach(verifyNativeStrings); return;} if (!object(value)) return; if (value.native_bytes === "hex-v1") {expect(typeof value.data).toBe("string"); expect(hex(originalOcPlayoutStringBytes(value as {native_bytes: "hex-v1"; data: string}))).toBe((value.data as string).toLowerCase());} else Object.values(value).forEach(verifyNativeStrings);}
function verifyPhysicalRequest(entry: Entry): void {
 const body = bytesOf(entry.request.body), contentType = Object.entries(entry.request.headers).find(([key]) => key.toLowerCase() === "content-type")?.[1] ?? "";
 const boundary = /boundary=(?:"([^"]+)"|([^;]+))/.exec(contentType);
 for (const part of entry.request.parts) {const bytes = fromHex(part.data_hex); expect(bytes.length).toBe(part.bytes); expect(digest(bytes)).toBe(part.sha256); expect(typeof part.filename === "string" || part.filename === null).toBe(true);}
 if (!boundary) return; // Retained prepared part metadata is not physically present in an overridden nonmultipart body.
 // Latin1 is a bijection used solely to split the captured physical MIME bytes.
 const latin1Of = (bytes: Uint8Array): string => Array.from(bytes, byte => String.fromCharCode(byte)).join("");
 const latin1 = latin1Of(body), delimiter = `--${boundary[1] ?? boundary[2]}`;
 const physical = latin1.split(delimiter); expect(physical[0]).toBe(""); expect(physical.at(-1)).toBe("--\r\n"); const parts = physical.slice(1, -1); expect(parts).toHaveLength(entry.request.parts.length);
 parts.forEach((part, index) => {expect(part.startsWith("\r\n")).toBe(true); expect(part.endsWith("\r\n")).toBe(true); const split = part.indexOf("\r\n\r\n"); expect(split).toBeGreaterThan(0); const headers = part.slice(2, split), expected = entry.request.parts[index]; expect(headers).toContain(`name="${latin1Of(nativeStringBytes(expected.name))}"`); expect(/(?:^|;)\s*filename=/.test(headers)).toBe(expected.filename_attribute_present); if (expected.filename_attribute_present) expect(headers).toContain(`filename="${latin1Of(nativeStringBytes(expected.filename ?? ""))}"`); expect(hex(Uint8Array.from(part.slice(split + 4, -2), char => char.charCodeAt(0)))).toBe(expected.data_hex.toLowerCase());});
}
function runForInspection(entry: Entry, value: OriginalOcPlayoutWorkbench, raw: string, tables: DiagnosticTable[]): OriginalOcPlayoutRun {const file = modelFile(entry) ?? new File([], ""); return {value, raw, fields: requestFields(entry), tables, http_status: entry.response.status, input: {model: file, model_sha256: digest(new Uint8Array(entry.request.parts.find(part => part.name === "model_file") ? fromHex(entry.request.parts.find(part => part.name === "model_file")!.data_hex) : []))}};}
afterEach(() => vi.unstubAllGlobals());

describe("compact actual original object-centric playout responses", () => {
 it("retains every final composite entry once and all four successful native surfaces", () => {
  expect(new Set(entries.map(entry => entry.id.toLowerCase())).size).toBe(entries.length); expect(entries.length).toBe(manifest.counts.entries);
  const values = entries.map(entry => parseNativeJson(rawResponse(entry))), owned = values.filter(isOriginalOcPlayoutWorkbench);
  expect(owned.length).toBe(manifest.counts.owned_responses); expect(values.length - owned.length).toBe(manifest.counts.outer_errors);
  for (const operation of ORIGINAL_OC_PLAYOUT_OPERATIONS) expect(owned.some(value => value.operation === operation && value.executed && !value.error && value.result?.type === (operation.endsWith("_result") ? "ocpn_playout_result" : "ocel"))).toBe(true);
  for (const artifact of ORIGINAL_OC_PLAYOUT_ARTIFACTS) expect(owned.some(value => value.artifact_originals[artifact] !== null)).toBe(true);
 });
 for (const entry of entries) it(`${entry.id}: exact captured model/options, full native storage, exports and inspection`, async () => {
  expect(entry.family).toBe("original_object_centric_playout"); expect(entry.method).toBe("POST"); expect(entry.path).toBe("/api/object-centric-playout-workbench"); verifyPhysicalRequest(entry);
  const raw = rawResponse(entry), value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields: requestFields(entry)});
  if (!isOriginalOcPlayoutWorkbench(value)) {
   expect(entry.response.status).toBeGreaterThanOrEqual(400); if (!object(value) || !Object.hasOwn(value, "error")) throw new Error("Actual outer transport failure lacks error diagnostics.");
   expect(typeof value.error === "string" || object(value.error)).toBe(true);
   const reference = entries.map(modelFile).find((file): file is File => file !== null)!; vi.stubGlobal("fetch", vi.fn(async () => new Response(raw, {status: entry.response.status})));
   await expect(runOriginalOcPlayoutWorkbench(reference, defaultOriginalOcPlayoutForm())).rejects.toThrow(typeof value.error === "string" ? value.error : originalOcPlayoutStringText(value.error as {native_bytes: "hex-v1"; data: string}));
   return;
  }
  expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(requestFields(entry)); verifyNumeric(raw, value); verifyNativeStrings(value);
  if (value.model?.ocpn) expect(isOriginalOcPlayoutOcpn(value.model.ocpn)).toBe(true); if (value.model?.causal_net) expect(isOriginalOcPlayoutCausalNet(value.model.causal_net)).toBe(true);
  if (value.configured_options) {expect(isOriginalOcPlayoutOptions(value.configured_options)).toBe(true); expect(Object.keys(value.configured_options).sort()).toEqual(ORIGINAL_OC_PLAYOUT_OPTION_FIELDS.map(([name]) => name).sort());}
  if (value.result) {expect(isOriginalOcPlayoutResult(value.result)).toBe(true); expect(value.executed).toBe(true); expect(value.error).toBeNull();}
  const modelPart = entry.request.parts.find(part => part.name === "model_file"), upload = value.input_context.uploads.model_file;
  if (value.model) {if (!modelPart || !upload) throw new Error("Owned model lost physical upload provenance."); const bytes = fromHex(modelPart.data_hex); verifyCarrier(value.model.original_model, bytes); expect(hex(originalOcPlayoutStringBytes(value.model.original_model.dataset))).toBe(hex(nativeStringBytes(modelPart.filename ?? ""))); expect(upload.sha256).toBe(modelPart.sha256); expect(upload.unchanged).not.toBe(false); if (upload.before !== null) sameNative(upload.after, upload.before);}
  if (value.options_context) {expect(value.options_context.unchanged).toBe(true); sameNative(value.options_context.before, value.options_context.after); sameNative(value.options_context.after, value.configured_options);}
  const physicalControls = entry.request.parts.filter(part => part.name !== "model_file").map(part => `${hex(nativeStringBytes(part.name))}:${part.data_hex.toLowerCase()}`).sort();
  const returnedControls = value.raw_controls.map(control => {const bytes = originalOcPlayoutOriginalBytes(control.original); verifyCarrier(control.original, bytes); return `${hex(originalOcPlayoutStringBytes(control.name))}:${hex(bytes)}`;}).sort(); expect(returnedControls).toEqual(physicalControls);
  const optionParts = entry.request.parts.filter(part => part.name === "options_json"); if (optionParts.length <= 1) expect(value.submitted_options).toBe(optionParts.length ? scalarText(fromHex(optionParts[0].data_hex)) : null);
  const tables = originalOcPlayoutTables(value, requestFields(entry)); verifyTables(value, tables); const run = runForInspection(entry, value, raw, tables), report = parseNativeJson(originalOcPlayoutReport(run)); if (!object(report)) throw new Error("Complete inspection report missing."); sameNative(report.native_result, value); verifyNumeric(nativeJson(value), report.native_result);
  for (const artifact of ORIGINAL_OC_PLAYOUT_ARTIFACTS) {
   const original = value.artifact_originals[artifact], expected = entry.artifact_expectations[artifact];
   if (!original) {expect(expected).toBeUndefined(); expect(() => originalOcPlayoutArtifactFile(run, artifact)).toThrow(); continue;}
   expect(expected).toBeDefined(); const bytes = originalOcPlayoutOriginalBytes(original); verifyCarrier(original, bytes); expect(bytes.length).toBe(expected.bytes); expect(digest(bytes)).toBe(expected.sha256);
   const file = originalOcPlayoutArtifactFile(run, artifact); expect(hex(new Uint8Array(await file.arrayBuffer()))).toBe(hex(bytes)); expect(value.exports[`${artifact}_error`]).toBeNull();
   if (value.exports[artifact] !== null) expect(hex(nativeStringBytes(value.exports[artifact]!))).toBe(hex(bytes));
   if (artifact === "model_json") {sameNative(parseNativeJson(scalarText(bytes)!), value.model?.ocpn ?? value.model?.causal_net); expect(hex(new Uint8Array(await originalOcPlayoutModelFile(run).arrayBuffer()))).toBe(hex(bytes));}
   if (artifact === "options_json") sameNative(parseNativeJson(scalarText(bytes)!), value.configured_options);
   if (artifact === "result_json") {sameNative(parseNativeJson(scalarText(bytes)!), value.result?.value); expect(hex(new Uint8Array(await originalOcPlayoutResultFile(run).arrayBuffer()))).toBe(hex(bytes));}
  }
  const log = originalOcPlayoutOcelValue(value); if (log) {const reopened = parseNativeJson(await originalOcPlayoutOcelFile(run).text()); expect(isOriginalOcPlayoutOcel(reopened)).toBe(true); sameNative(reopened, log);} else expect(() => originalOcPlayoutOcelFile(run)).toThrow();
 });
});
