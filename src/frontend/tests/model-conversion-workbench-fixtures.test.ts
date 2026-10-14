import {afterEach, describe, expect, it, vi} from "vitest";
import {
  MODEL_CONVERSION_ARTIFACTS, MODEL_CONVERSION_OPERATIONS, MODEL_CONVERSION_PNML_CONTROLS, MODEL_CONVERSION_REDUCTION_CONTROLS,
  ModelConversionRequestGate, defaultModelConversionForm, isModelConversionWorkbench, modelConversionAllTablesCsv,
  modelConversionArtifactFile, modelConversionFields, modelConversionGraphs, modelConversionReport, modelConversionSummary,
  modelConversionTableFileName, modelConversionTables, runModelConversionWorkbench,
  type ModelConversionArtifact, type ModelConversionForm, type ModelConversionRun, type ModelConversionWorkbench,
} from "../src/model-conversion-workbench";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";
import {diagnosticCsv, diagnosticPage, type DiagnosticTable} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "../src/native-response";

// Minimal fixture-only Node types keep browser production dependencies unchanged.
interface NativeBytes extends Uint8Array {toString(encoding?: string): string; equals(other: Uint8Array): boolean;}
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {createHash} = await nodeModule("node:crypto") as {createHash(algorithm: string): {update(data: string | Uint8Array): {digest(encoding: "hex"): string}}};
const {existsSync, mkdirSync, readFileSync, writeFileSync} = await nodeModule("node:fs") as {
  existsSync(path: string): boolean; mkdirSync(path: string, options: {recursive: boolean}): unknown;
  readFileSync: {(path: string): NativeBytes; (path: string, encoding: "utf8"): string}; writeFileSync(path: string, data: string | Uint8Array, options?: {flag: "wx"}): void;
};
const {dirname, isAbsolute, relative, resolve} = await nodeModule("node:path") as {dirname(path: string): string; isAbsolute(path: string): boolean; relative(from: string, to: string): string; resolve(...parts: string[]): string};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url: string): string};
const {Buffer} = await nodeModule("node:buffer") as {Buffer: {from(data: string | Uint8Array): NativeBytes}};
const environment = (globalThis as unknown as {process: {env: Record<string, string | undefined>}}).process.env;

interface CapturedFile {path: string; sha256: string; bytes: number; filename?: string;}
interface CapturedEntry {
  id: string; family: "model_conversion"; operation: ModelConversionForm["operation"]; model_format: ModelConversionForm["modelFormat"]; executed: boolean;
  error: null | {type: string; message: string}; fields: Array<[string, string]>; path: string; method: string; expected_status: number;
  model: CapturedFile & {filename: string}; request_body: CapturedFile; response: CapturedFile & {status: number};
  exports: Partial<Record<ModelConversionArtifact, CapturedFile & {format: string}>>; coverage_tags?: Record<string, unknown>;
}
interface CapturedManifest {complete: boolean; feature: number; entries: CapturedEntry[]; [key: string]: unknown;}
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), ".."), root = resolve(frontend, "../..");
const captureDirectory = environment.MINERA_MODEL_CONVERSION_CAPTURE_DIR;
const manifestPath = captureDirectory ? resolve(captureDirectory, "request-manifest.json") : resolve(frontend, "tests/fixtures/model-conversion-workbench-request-manifest.json");
if (!existsSync(manifestPath)) throw new Error("Complete own-server model conversion captures are required. Supply MINERA_MODEL_CONVERSION_CAPTURE_DIR or publish the portable fixture manifest.");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as CapturedManifest;
if (manifest.complete !== true || manifest.feature !== 56 || !Array.isArray(manifest.entries) || !manifest.entries.length) throw new Error("The model conversion manifest is not a complete real native capture.");
const entries = manifest.entries, exportDirectory = environment.MINERA_MODEL_CONVERSION_EXPORT_DIR;
if (new Set(entries.map(entry => entry.id.toLowerCase())).size !== entries.length) throw new Error("Native request identifiers must be casefold unique.");
const digest = (bytes: string | Uint8Array): string => createHash("sha256").update(bytes).digest("hex");
function pathInWorkspace(path: string): string {const full = isAbsolute(path) ? resolve(path) : resolve(root, path), child = relative(root, full); if (child.startsWith("..") || isAbsolute(child)) throw new Error("Fixture and export paths must stay in the workspace."); return full;}
function capturedBytes(record: CapturedFile): NativeBytes {const bytes = readFileSync(pathInWorkspace(record.path)); expect(digest(bytes)).toBe(record.sha256); expect(bytes.byteLength).toBe(record.bytes); return bytes;}
function originalFile(record: CapturedFile & {filename: string}): File {return new File([new Uint8Array(capturedBytes(record)).buffer], record.filename, {lastModified: 0});}
function responseText(entry: CapturedEntry): string {return capturedBytes(entry.response).toString("utf8");}
function actual(entry: CapturedEntry): ModelConversionWorkbench {const value = parseNativeJson(responseText(entry)); if (!isModelConversionWorkbench(value)) throw new Error(`Invalid actual model conversion envelope ${entry.id}`); return value;}
/** Independent lexical walk checks every integer and floating-point token. */
function numericPaths(raw: string): Map<string, string> {
  const tokens = raw.match(/"(?:[^"\\]|\\.)*"|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?|true|false|null|[{}\[\],:]/g) ?? [], values = new Map<string, string>(); let position = 0;
  const escape = (key: string) => key.replace(/~/g, "~0").replace(/\//g, "~1");
  const walk = (path: string): void => {const token = tokens[position++]; if (token === "{") {while (tokens[position] !== "}") {const key = JSON.parse(tokens[position++]) as string; if (tokens[position++] !== ":") throw new Error("Invalid native object token"); walk(path + "/" + escape(key)); if (tokens[position] !== ",") break; ++position;} if (tokens[position++] !== "}") throw new Error("Invalid native object end");} else if (token === "[") {let index = 0; while (tokens[position] !== "]") {walk(path + "/" + index++); if (tokens[position] !== ",") break; ++position;} if (tokens[position++] !== "]") throw new Error("Invalid native array end");} else if (/^-?\d/.test(token)) values.set(path, token);};
  walk(""); expect(position).toBe(tokens.length); return values;
}
function valueAt(value: unknown, pointer: string): unknown {if (!pointer || pointer === "summary") return value; let current = value; for (const name of pointer.slice(1).split("/").map(key => key.replace(/~1/g, "/").replace(/~0/g, "~"))) current = (current as Record<string, unknown>)[name]; return current;}
function holderAt(value: unknown, pointer: string): [object, string] {const slash = pointer.lastIndexOf("/"), holder = valueAt(value, pointer.slice(0, slash)), key = pointer.slice(slash + 1).replace(/~1/g, "/").replace(/~0/g, "~"); if (holder === null || typeof holder !== "object") throw new Error("Numeric native pointer has no holder"); return [holder, key];}
/** CSV decoding is independent of the production encoder, including quoted multiline/NUL cells. */
function decodeCsv(raw: string): string[][] {
  const rows: string[][] = []; let row: string[] = [], cell = "", quoted = false, atStart = true;
  for (let index = 0; index < raw.length; ++index) {const char = raw[index]; if (quoted) {if (char === '"') {if (raw[index + 1] === '"') {cell += '"'; ++index;} else quoted = false;} else cell += char; continue;} if (char === '"') {if (!atStart) throw new Error("Unexpected CSV quote"); quoted = true; atStart = false;} else if (char === ",") {row.push(cell); cell = ""; atStart = true;} else if (char === "\r" || char === "\n") {if (char === "\r" && raw[index + 1] === "\n") ++index; row.push(cell); rows.push(row); row = []; cell = ""; atStart = true;} else {cell += char; atStart = false;}}
  if (quoted) throw new Error("Unterminated CSV quoted cell"); if (row.length || cell !== "" || !atStart) {row.push(cell); rows.push(row);} return rows;
}
const expectedCell = (value: unknown): string => value === undefined ? "" : typeof value === "string" ? value : value !== null && typeof value === "object" ? nativeJson(value) : String(value);
/** Independent UTF-8/WTF-8 decoder checks actual download bytes, including lone surrogates. */
function nativeBytesText(bytes: Uint8Array): string {
  let text = "";
  for (let index = 0; index < bytes.length;) {const lead = bytes[index++]; let code: number, count: number; if (lead < 0x80) {code = lead; count = 0;} else if (lead >= 0xc2 && lead <= 0xdf) {code = lead & 0x1f; count = 1;} else if (lead >= 0xe0 && lead <= 0xef) {code = lead & 0x0f; count = 2;} else if (lead >= 0xf0 && lead <= 0xf4) {code = lead & 0x07; count = 3;} else throw new Error("Invalid UTF-8 lead"); for (let offset = 0; offset < count; ++offset) {const tail = bytes[index++]; if (tail === undefined || tail < 0x80 || tail > 0xbf) throw new Error("Invalid UTF-8 continuation"); code = code << 6 | tail & 0x3f;} if (count === 1 && code < 0x80 || count === 2 && code < 0x800 || count === 3 && (code < 0x10000 || code > 0x10ffff)) throw new Error("Invalid UTF-8 code point"); text += String.fromCodePoint(code);}
  return text;
}
function formFromCapture(entry: CapturedEntry): ModelConversionForm {const form = defaultModelConversionForm(entry.operation); form.modelFormat = entry.model_format; for (const [key, value] of entry.fields) {if (key.startsWith("pnml_")) form.pnmlFields[key] = value; else if (key.startsWith("petri_reduction_")) form.fields[key] = value;} return form;}
async function runFromCapture(entry: CapturedEntry): Promise<ModelConversionRun> {const raw = responseText(entry), value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields: entry.fields}); if (!isModelConversionWorkbench(value)) throw new Error(`Invalid captured model conversion ${entry.id}`); return {value, raw, fields: entry.fields, tables: modelConversionTables(value, entry.fields), http_status: entry.response.status, input: {model: originalFile(entry.model), model_sha256: entry.model.sha256}};}
interface ExportProof {
  id: string; verified_numeric_literals: number; tables: number; rows: number; csv_cells: number; selected_records: number; selected_numeric_literals: number;
  table_details: Array<{id: string; title: string; columns: string[]; rows: number; records: number; csv: string; records_json: string | null}>;
  artifacts: Record<string, {bytes: number; sha256: string}>;
}
const proofs: ExportProof[] = [], written = new Map<string, {path: string; sha256: string}>();
function immutableWrite(path: string, data: NativeBytes): void {const key = path.toLowerCase(), existing = written.get(key), hash = digest(data); if (existing && (existing.path !== path || existing.sha256 !== hash)) throw new Error("Casefold export filename collision"); written.set(key, {path, sha256: hash}); mkdirSync(dirname(path), {recursive: true}); if (existsSync(path)) {if (!readFileSync(path).equals(data)) throw new Error("An immutable export path contains different bytes");} else writeFileSync(path, data, {flag: "wx"}); expect(readFileSync(path).equals(data)).toBe(true);}
function saveArtifact(id: string, name: string, bytes: Uint8Array | string, proof: ExportProof): void {const data = Buffer.from(typeof bytes === "string" ? workflowInputNativeBytes(bytes) : bytes); if (typeof bytes === "string") expect(nativeBytesText(data)).toBe(bytes); proof.artifacts[name] = {bytes: data.byteLength, sha256: digest(data)}; if (!exportDirectory) return; if (!/^[a-z0-9][a-z0-9._-]*$/i.test(id) || /[. ]$/.test(id) || /^(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(id)) throw new Error("Unsafe fixture identifier"); immutableWrite(pathInWorkspace(resolve(exportDirectory, id, name)), data);}
/** Compare each selected generic record to its original holder, preserving primitive numeric tokens. */
function selectedRecords(value: ModelConversionWorkbench, table: DiagnosticTable): number {
  const records = table.records; if (!records) return 0; let checked = 0;
  if (table.id === "summary" || table.id.startsWith("/")) {
    const source = valueAt(value, table.id);
    if (Array.isArray(source)) {expect(records).toHaveLength(source.length); source.forEach((record, index) => {const selected = records[index]; if (record !== null && typeof record === "object") expect(nativeJson(selected)).toBe(nativeJson(record)); else {expect(selected).toHaveProperty("value"); const encoded = typeof record === "number" ? String(nativeCell(source, index)) : nativeJson(record); expect(nativeJson(selected)).toBe(`{"value":${encoded}}`);} checked += numericPaths(nativeJson(selected)).size;});}
    else if (source && typeof source === "object") {const object = source as Record<string, unknown>, keys = Object.keys(object); expect(records).toHaveLength(keys.length); keys.forEach((key, index) => {const encoded = typeof object[key] === "number" ? String(nativeCell(object, key)) : nativeJson(object[key]); expect(nativeJson(records[index])).toBe(`{"field":${JSON.stringify(key)},"value":${encoded}}`); checked += numericPaths(nativeJson(records[index])).size;});}
  } else if (table.id.startsWith("complete_old_to_new_")) {const key = table.id.slice("complete_".length) as "old_to_new_place" | "old_to_new_transition", source = value.result!.reduction![key]; expect(records).toHaveLength(source.length); source.forEach((_, index) => {const literal = String(nativeCell(source, index)); expect(nativeJson(records[index])).toBe(`{"original_index":${index},"returned_index":${literal},"removed":${literal === "18446744073709551615"}}`); checked += numericPaths(nativeJson(records[index])).size;});}
  else for (const record of records) checked += numericPaths(nativeJson(record)).size;
  return checked;
}
afterEach(() => vi.unstubAllGlobals());

describe("actual own-server original-model conversion and reduction fixtures", () => {
  it("requires all converters/reduction, native errors, independent model formats and complete coverage", () => {
    for (const operation of MODEL_CONVERSION_OPERATIONS) {expect(entries.some(entry => entry.operation === operation && actual(entry).error === null)).toBe(true); expect(entries.some(entry => entry.operation === operation && entry.model_format === "auto")).toBe(true);}
    for (const model_format of ["ptml", "tree_json", "pnml", "powl_text"] as const) expect(entries.some(entry => entry.model_format === model_format)).toBe(true);
    expect(entries.some(entry => entry.response.status === 400 && actual(entry).error !== null)).toBe(true); expect(entries.every(entry => !Object.hasOwn(entry, "input"))).toBe(true);
    for (const artifact of MODEL_CONVERSION_ARTIFACTS) expect(entries.some(entry => entry.exports[artifact])).toBe(true);
  });
  for (const entry of entries) it(`${entry.id} preserves all native scalars, selected records and complete exported bytes`, async () => {
    expect(entry.family).toBe("model_conversion"); expect(entry.path).toBe("/api/model-conversion-workbench"); expect(entry.method).toBe("POST"); expect(entry.response.status).toBe(entry.expected_status);
    const raw = responseText(entry), captured = await runFromCapture(entry), value = captured.value;
    expect(value.operation).toBe(entry.operation); expect(value.executed).toBe(entry.executed); expect(value.error).toEqual(entry.error); expect(value.input_context.format).toBe(entry.model_format); expect(value.input_context.dataset).toBe(entry.model.filename); expect(value.input_context.original_model_bytes).toBe(entry.model.bytes); expect(value.input_context.model_unchanged).toBe(true);
    expect(Buffer.from(workflowInputNativeBytes(value.model.original_model)).equals(capturedBytes(entry.model))).toBe(true); expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(entry.fields);
    vi.stubGlobal("fetch", vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status})))); const bound = await runModelConversionWorkbench(captured.input.model, formFromCapture(entry)); expect(bound.raw).toBe(raw); expect(bound.input.model_sha256).toBe(entry.model.sha256);
    const numeric = numericPaths(raw); for (const [pointer, literal] of numeric) {const [holder, key] = holderAt(value, pointer); expect(String(nativeCell(holder, key)), pointer).toBe(literal);} expect(numericPaths(nativeJson(value))).toEqual(numeric);
    const proof: ExportProof = {id: entry.id, verified_numeric_literals: numeric.size, tables: captured.tables.length, rows: 0, csv_cells: 0, selected_records: 0, selected_numeric_literals: 0, table_details: [], artifacts: {}};
    saveArtifact(entry.id, "native.json", captured.raw, proof); expect(digest(workflowInputNativeBytes(captured.raw))).toBe(entry.response.sha256);
    const reportText = modelConversionReport(captured), report = parseNativeJson(reportText) as {native_result: unknown; input: {name: string; size: number; sha256: string}; request: {fields: Array<[string, string]>}};
    expect(report.input).toMatchObject({name: entry.model.filename, size: entry.model.bytes, sha256: entry.model.sha256}); expect(report.request.fields).toEqual(entry.fields); expect(nativeJson(report.native_result)).toBe(nativeJson(value)); expect(new Map([...numericPaths(reportText)].filter(([path]) => path.startsWith("/native_result/")).map(([path, token]) => [path.slice("/native_result".length), token]))).toEqual(numeric); saveArtifact(entry.id, "report.json", reportText, proof);
    const requestText = nativeJson(nativeResponseRequest(value), 2); expect(JSON.parse(requestText).fields).toEqual(entry.fields); saveArtifact(entry.id, "request.json", requestText, proof); saveArtifact(entry.id, "request.bin", capturedBytes(entry.request_body), proof);
    const original = new Uint8Array(await captured.input.model.arrayBuffer()); expect(Buffer.from(original).equals(capturedBytes(entry.model))).toBe(true); saveArtifact(entry.id, "original-model.bin", original, proof);
    for (const artifact of MODEL_CONVERSION_ARTIFACTS) {if (value.exports[artifact] === null) {expect(() => modelConversionArtifactFile(captured, artifact)).toThrow(); expect(entry.exports[artifact]).toBeUndefined(); continue;} const file = modelConversionArtifactFile(captured, artifact), bytes = new Uint8Array(await file.arrayBuffer()), observed = entry.exports[artifact]; expect(observed).toBeDefined(); if (!observed) throw new Error("Missing captured native model artifact"); expect(nativeBytesText(bytes)).toBe(value.exports[artifact]); expect(Buffer.from(bytes).equals(capturedBytes(observed))).toBe(true); expect(digest(bytes)).toBe(observed.sha256); expect(file.name.endsWith("." + observed.format)).toBe(true); saveArtifact(entry.id, artifact + "." + observed.format, bytes, proof);}
    const fileNames = new Set<string>();
    for (const [tableIndex, table] of captured.tables.entries()) {
      const csv = diagnosticCsv(table), decoded = decodeCsv(csv), bytes = new Uint8Array(await new Blob([workflowInputNativeBytes(csv)], {type: "text/csv;charset=utf-8"}).arrayBuffer()); expect(decodeCsv(nativeBytesText(bytes))).toEqual(decoded); expect(decoded[0], table.id).toEqual(table.columns); expect(decoded.slice(1), table.id).toEqual(table.rows.map(row => row.map(expectedCell))); expect(decoded.slice(1).every(row => row.length === table.columns.length)).toBe(true);
      const escaped = modelConversionTableFileName(table.id); expect(escaped).toMatch(/^[A-Za-z0-9_.%\-]+$/); expect(escaped.replace(/%([a-f0-9]{4})/g, (_, code: string) => String.fromCharCode(Number.parseInt(code, 16)))).toBe(table.id);
      const indexed = String(tableIndex).padStart(4, "0"), csvName = `tables/${indexed}.csv`, recordsName = table.records ? `records/${indexed}.json` : null; expect(fileNames.has(csvName.toLowerCase())).toBe(false); fileNames.add(csvName.toLowerCase()); proof.rows += table.rows.length; proof.csv_cells += decoded.reduce((sum, row) => sum + row.length, 0); proof.selected_records += table.records?.length ?? 0; proof.selected_numeric_literals += selectedRecords(value, table);
      proof.table_details.push({id: table.id, title: table.title, columns: table.columns, rows: table.rows.length, records: table.records?.length ?? 0, csv: csvName, records_json: recordsName}); saveArtifact(entry.id, csvName, csv, proof);
      if (table.records && recordsName) {const text = nativeJson(table.records, 2); expect(nativeJson(parseNativeJson(text))).toBe(nativeJson(table.records)); for (const record of table.records) expect(numericPaths(nativeJson(parseNativeJson(nativeJson(record))))).toEqual(numericPaths(nativeJson(record))); saveArtifact(entry.id, recordsName, text, proof);}
    }
    const allCsv = modelConversionAllTablesCsv(captured.tables), decodedAll = decodeCsv(allCsv), expected: string[][] = []; expect(decodeCsv(nativeBytesText(new Uint8Array(await new Blob([workflowInputNativeBytes(allCsv)]).arrayBuffer())))).toEqual(decodedAll);
    for (const table of captured.tables) {if (!table.rows.length) table.columns.forEach((column, column_index) => expected.push([table.id, table.title, "", String(column_index), column, ""])); table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => expected.push([table.id, table.title, String(row_index), String(column_index), column, expectedCell(row[column_index])])));}
    expect(decodedAll[0]).toEqual(["table_id", "table_title", "row_index", "column_index", "column", "value"]); expect(decodedAll.slice(1)).toEqual(expected); saveArtifact(entry.id, "all-tables.csv", allCsv, proof); proofs.push(proof);
  }, 30000);
  it("retains complete original/result models, raw constructor state, order/frequency and every graph node beyond paging", () => {
    const kinds = new Set<string>(), frequencies = new Set<string>(); let pagedRows = 0, pagedColumns = 0, deepest = 0, visibleEmpty = 0, silent = 0, canonicalAmbiguity = 0;
    for (const entry of entries) {
      const value = actual(entry), tables = modelConversionTables(value, entry.fields), expectedGraphs: Array<{title: string; nodes: number; edges: number}> = [];
      if (value.input_context.detected_format === "tree_json") expect(nativeJson(parseNativeJson(value.model.original_model))).toBe(nativeJson(value.model.process_tree));
      if (value.model.pnml_document) {const document = value.model.pnml_document; for (const key of ["net", "raw_source", "raw_sink", "initial_marking", "final_marking", "place_ids", "transition_ids", "initial_marking_keys", "final_marking_keys", "arc_occurrences", "stochastic_map"]) expect(document).toHaveProperty(key); expect(nativeJson(document.net)).toBe(nativeJson(value.model.petri_net));}
      for (const [prefix, model] of [["original", value.model], ["result", value.result]] as const) {
        if (!model) continue; const title = prefix === "original" ? "Original" : "Resulting";
        if (model.process_tree) {const pending = [model.process_tree.root]; let count = 0; while (pending.length) {const node = pending.pop()!; ++count; pending.push(...node.children);} const table = tables.find(table => table.id === `${prefix}_tree_nodes`)!; expect(table.rows).toHaveLength(count); expect(table.records).toHaveLength(count); expect(nativeJson(table.records![0])).toContain(nativeJson(model.process_tree.root)); expectedGraphs.push({title: `${title} process tree`, nodes: count, edges: count - 1});}
        if (model.powl) {const pending = [{node: model.powl, depth: 0}]; let count = 0, relations = 0; while (pending.length) {const {node, depth} = pending.pop()!; ++count; relations += node.order.length; deepest = Math.max(deepest, depth); kinds.add(node.kind); frequencies.add(nativeJson(node.frequency)); if (node.kind === "TRANSITION" && node.label === "") ++visibleEmpty; if (node.kind === "SILENT") ++silent; node.children.forEach(child => pending.push({node: child, depth: depth + 1}));} expect(tables.find(table => table.id === `${prefix}_powl_nodes`)?.rows).toHaveLength(count); expect(tables.find(table => table.id === `${prefix}_powl_order`)?.rows).toHaveLength(relations); expectedGraphs.push({title: `${title} POWL model`, nodes: count, edges: count - 1 + relations});}
        const originalNet = model.petri_net; if (originalNet) {const transitions = originalNet.transitions as Array<Record<string, unknown>>; expectedGraphs.push({title: `${title} Petri net`, nodes: Number(originalNet.place_count) + transitions.length, edges: transitions.reduce((sum, transition) => sum + ["inputs", "outputs", "reset_inputs", "inhibitor_inputs"].reduce((count, key) => count + (transition[key] as unknown[]).length, 0), 0)});}
      }
      if (value.result?.reduction) {const net = value.result.reduction.net, transitions = net.transitions as Array<Record<string, unknown>>; expect(value.result.petri_net).toBeNull(); expectedGraphs.push({title: "Resulting Petri net", nodes: Number(net.place_count) + transitions.length, edges: transitions.reduce((sum, transition) => sum + ["inputs", "outputs", "reset_inputs", "inhibitor_inputs"].reduce((count, key) => count + (transition[key] as unknown[]).length, 0), 0)});}
      const graphs = modelConversionGraphs(value); expect(graphs).toHaveLength(expectedGraphs.length); graphs.forEach(({title, graph}, index) => {expect(title).toBe(expectedGraphs[index].title); expect(graph.nodes).toHaveLength(expectedGraphs[index].nodes); expect(graph.edges).toHaveLength(expectedGraphs[index].edges); expect(new Set(graph.nodes.map(node => node.id)).size).toBe(graph.nodes.length); expect(new Set(graph.edges.map(edge => edge.id)).size).toBe(graph.edges.length);});
      if (["numeric-label", "at-digit-label"].includes(String(entry.coverage_tags?.powl_case))) {expect(value.exports.source_powl).toContain("9-->B"); expect(value.error).toBeNull(); ++canonicalAmbiguity;}
      for (const table of tables) {if (table.rows.length > 25) {expect(diagnosticPage(table.rows, 1)).toHaveLength(Math.min(25, table.rows.length - 25)); expect(nativeJson(table.records?.at(-1))).toBeTruthy(); ++pagedRows;} if (table.columns.length > 8) {expect(diagnosticPage(table.columns, 1, 8)).toHaveLength(Math.min(8, table.columns.length - 8)); ++pagedColumns;}}
    }
    for (const kind of ["TRANSITION", "SILENT", "XOR", "LOOP", "STRICT_PARTIAL_ORDER"]) expect(kinds.has(kind)).toBe(true);
    for (const minimum of [0, 1]) for (const maximum of [1, null]) expect(frequencies.has(nativeJson({minimum, maximum}))).toBe(true);
    expect(deepest).toBeGreaterThan(64); expect(visibleEmpty).toBeGreaterThan(0); expect(silent).toBeGreaterThan(0); expect(canonicalAmbiguity).toBeGreaterThan(0); expect(pagedRows).toBeGreaterThan(0); expect(pagedColumns).toBeGreaterThan(0);
  });
  it("preserves all native reduction options/origins, signed extremes and selected mapping tokens without inventing identity", () => {
    const origins = new Set<string>(), selectedTokens = new Set<string>(); let errors = 0, rebuiltUnchanged = 0, configuredBooleans = 0, unspecified = 0;
    for (const entry of entries) {
      const value = actual(entry), tables = modelConversionTables(value, entry.fields), fields = modelConversionFields(formFromCapture(entry));
      if (value.input_context.detected_format === "pnml") {expect(value.pnml_import_options).not.toBeNull(); for (const [key] of MODEL_CONVERSION_PNML_CONTROLS) {const supplied = entry.fields.find(([name]) => name === key); if (supplied) expect(value.pnml_import_options![key.slice(5) as keyof NonNullable<ModelConversionWorkbench["pnml_import_options"]>]).toBe(["true", "1"].includes(supplied[1].toLowerCase()));}} else {expect(value.pnml_import_options).toBeNull(); expect(fields.some(([key]) => key.startsWith("pnml_"))).toBe(false);}
      if (entry.operation !== "reduce_petri") {expect(value.options).toBeNull(); expect(value.input_context.initial_marking_origin).toBeNull(); expect(value.input_context.final_marking_origin).toBeNull(); expect(fields.some(([key]) => key.startsWith("petri_reduction_"))).toBe(false);}
      else {
        const options = value.options!; expect(Object.keys(options).sort()).toEqual(["initial_marking", "final_marking", "enable_parallel_place_fusion", "enable_abstraction", "preserve_join_bridges"].sort());
        for (const control of MODEL_CONVERSION_REDUCTION_CONTROLS) {const supplied = entry.fields.find(([key]) => key === control.key); if (!supplied) continue; const raw = supplied[1], key = control.key.replace(/^petri_reduction_/, "").replace(/_json$/, "").replace(/^parallel_place_fusion$/, "enable_parallel_place_fusion").replace(/^abstraction$/, "enable_abstraction"); if (control.kind === "boolean") {expect(options[key as keyof typeof options]).toBe(["true", "1"].includes(raw)); ++configuredBooleans;} else {const expected = control.kind === "marking" ? (parseNativeJson(raw) as number[]).map((_, index, array) => BigInt(String(nativeCell(array, index))).toString()) : raw.split(",").map(item => BigInt(item.trim()).toString()), observed = options[key as "initial_marking" | "final_marking"]; expect(observed.map((_, index) => String(nativeCell(observed, index)))).toEqual(expected);}}
        for (const name of ["initial", "final"] as const) {const json = entry.fields.find(([key]) => key === `petri_reduction_${name}_marking_json`), legacy = entry.fields.find(([key]) => key === `petri_reduction_${name}_marking`), origin = json ? (parseNativeJson(json[1]) as unknown[]).length ? "explicit_json" : "explicit_empty_unspecified" : legacy ? "explicit_legacy" : "pnml"; expect(value.input_context[`${name}_marking_origin`]).toBe(origin); origins.add(origin); if (origin === "explicit_empty_unspecified") {expect(options[`${name}_marking`]).toEqual([]); ++unspecified;}}
        if (value.result?.reduction) {const reduced = value.result.reduction, original = value.model.petri_net!; expect(reduced.old_to_new_place).toHaveLength(Number(original.place_count)); expect(reduced.old_to_new_transition).toHaveLength((original.transitions as unknown[]).length); if (!reduced.changed && nativeJson(reduced.net) !== nativeJson(original)) ++rebuiltUnchanged; expect(modelConversionSummary(value)).toContain(String(reduced.changed));}
      }
      if (value.error) {expect(value.result).toBeNull(); expect(value.error.type).toBe("native_error"); expect(modelConversionSummary(value)).toContain("failed"); ++errors;} else {expect(value.executed).toBe(true); for (const key of ["computed", "completed", "timed_out", "fitness", "traces", "alignment"]) expect(value.result).not.toHaveProperty(key);}
      for (const table of tables) for (const record of table.records ?? []) for (const token of numericPaths(nativeJson(record)).values()) selectedTokens.add(token);
    }
    expect([...origins].sort()).toEqual(["explicit_empty_unspecified", "explicit_json", "explicit_legacy", "pnml"]); for (const token of ["18446744073709551615", "-9223372036854775808", "9223372036854775807", "340282366920938463463374607431768211456"]) expect(selectedTokens.has(token)).toBe(true);
    expect(errors).toBeGreaterThan(0); expect(rebuiltUnchanged).toBeGreaterThan(0); expect(configuredBooleans).toBeGreaterThan(0); expect(unspecified).toBeGreaterThan(0);
  });
  it("binds independent original bytes and blocks mismatches, cancellation and stale request generations", async () => {
    const entry = entries.find(entry => entry.operation === "reduce_petri" && actual(entry).error === null)!, model = originalFile(entry.model), form = formFromCapture(entry), raw = responseText(entry), fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))); vi.stubGlobal("fetch", fetcher);
    const run = await runModelConversionWorkbench(model, form); expect(run.input.model_sha256).toBe(entry.model.sha256); expect(run.raw).toBe(raw); expect(fetcher.mock.calls[0][0]).toBe("/api/model-conversion-workbench"); const body = fetcher.mock.calls[0][1].body as FormData; expect(body.get("model_file")).toBe(model); for (const key of ["input_file", "source_id", "include_source"]) expect(body.get(key)).toBeNull();
    await expect(runModelConversionWorkbench(new File([await model.arrayBuffer()], "different-model.pnml"), form)).rejects.toThrow(/does not match/); const changed = new Uint8Array(await model.arrayBuffer()); changed[0] ^= 1; await expect(runModelConversionWorkbench(new File([changed], model.name), form)).rejects.toThrow(/original model bytes/);
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(raw));})); await expect(runModelConversionWorkbench(model, form, controller.signal)).rejects.toThrow(/abort/i);
    const gate = new ModelConversionRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false); expect(gate.accepts(gate.issue(), controller.signal)).toBe(false);
  });
  it("publishes the complete independently decoded native/CSV/selected-record export proof when requested", () => {
    expect(proofs.map(proof => proof.id)).toEqual(entries.map(entry => entry.id)); if (!exportDirectory) return;
    const integrity = {complete: true, manifest: manifestPath, manifest_sha256: digest(readFileSync(manifestPath)), entries: proofs, verified_numeric_literals: proofs.reduce((sum, proof) => sum + proof.verified_numeric_literals, 0), csv_cells: proofs.reduce((sum, proof) => sum + proof.csv_cells, 0), selected_records: proofs.reduce((sum, proof) => sum + proof.selected_records, 0), selected_numeric_literals: proofs.reduce((sum, proof) => sum + proof.selected_numeric_literals, 0)};
    immutableWrite(pathInWorkspace(resolve(exportDirectory, "export-integrity.json")), Buffer.from(JSON.stringify(integrity, null, 2) + "\n"));
  });
});
