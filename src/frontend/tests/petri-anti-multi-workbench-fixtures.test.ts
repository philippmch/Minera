import {afterEach, describe, expect, it, vi} from "vitest";
import {
  PETRI_ANTI_MULTI_ALGORITHMS, PETRI_ANTI_MULTI_CONTROLS, PETRI_ANTI_MULTI_PNML_CONTROLS, PETRI_ANTI_MULTI_VARIANT,
  PetriAntiMultiRequestGate, defaultPetriAntiMultiForm, isPetriAntiMultiWorkbench, petriAntiMultiAllTablesCsv,
  petriAntiMultiArtifactFile, petriAntiMultiFields, petriAntiMultiGraph, petriAntiMultiNumber, petriAntiMultiReport,
  petriAntiMultiSummary, petriAntiMultiTableFileName, petriAntiMultiTables, runPetriAntiMultiWorkbench,
  type PetriAntiMultiWorkbench, type PetriAntiMultiForm, type PetriAntiMultiRun,
} from "../src/petri-anti-multi-workbench";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "../src/native-response";

// The fixture-only Node surface and independent byte/CSV/number decoders are
// shared in shape with the prior real-capture proof, without browser dependencies.
interface NativeBytes extends Uint8Array {toString(encoding?: string): string; equals(other: Uint8Array): boolean;}
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {createHash} = await nodeModule("node:crypto") as {createHash(algorithm: string): {update(data: string | Uint8Array): {digest(encoding: "hex"): string}}};
const {existsSync, mkdirSync, readFileSync, writeFileSync} = await nodeModule("node:fs") as {
  existsSync(path: string): boolean; mkdirSync(path: string, options: {recursive: boolean}): unknown;
  readFileSync: {(path: string): NativeBytes; (path: string, encoding: "utf8"): string}; writeFileSync(path: string, data: string | Uint8Array, encoding?: "utf8"): void;
};
const {dirname, isAbsolute, relative, resolve} = await nodeModule("node:path") as {dirname(path: string): string; isAbsolute(path: string): boolean; relative(from: string, to: string): string; resolve(...parts: string[]): string};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url: string): string};
const {Buffer} = await nodeModule("node:buffer") as {Buffer: {from(data: string | Uint8Array): NativeBytes}};
const environment = (globalThis as unknown as {process: {env: Record<string, string | undefined>}}).process.env;

interface CapturedFile {path: string; sha256: string; bytes: number; filename?: string;}
interface CapturedEntry {
  id: string; algorithm: PetriAntiMultiForm["algorithm"]; variant: typeof PETRI_ANTI_MULTI_VARIANT;
  input_shape: PetriAntiMultiForm["inputShape"]; input_format: PetriAntiMultiForm["inputFormat"]; model_format: PetriAntiMultiForm["modelFormat"]; result_shape: "aggregate"; executed: boolean;
  fields: Array<[string, string]>; path: string; method: string; expected_status: number;
  input: CapturedFile & {filename: string}; model: CapturedFile & {filename: string}; request_body: CapturedFile; response: CapturedFile & {status: number};
  exports: Partial<Record<"pnml" | "checked_pnml" | "source_xes", CapturedFile & {format: string}>>;
}
interface CapturedManifest {complete: boolean; feature: number; entries: CapturedEntry[]; [key: string]: unknown;}
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), ".."), root = resolve(frontend, "../..");
const captureDirectory = environment.MINERA_PETRI_ANTI_MULTI_CAPTURE_DIR;
const manifestPath = captureDirectory ? resolve(captureDirectory, "request-manifest.json") : resolve(frontend, "tests/fixtures/petri-anti-multi-workbench-request-manifest.json");
if (!existsSync(manifestPath)) throw new Error("Complete own-server Petri anti/multi alignment captures are required. Supply MINERA_PETRI_ANTI_MULTI_CAPTURE_DIR or publish the portable fixture manifest.");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as CapturedManifest;
if (manifest.complete !== true || manifest.feature !== 55 || !Array.isArray(manifest.entries) || !manifest.entries.length) throw new Error("The Petri anti/multi alignment request manifest is not a complete real capture.");
const entries = manifest.entries, exportDirectory = environment.MINERA_PETRI_ANTI_MULTI_EXPORT_DIR;

const digest = (bytes: string | Uint8Array): string => createHash("sha256").update(bytes).digest("hex");
function pathInWorkspace(path: string): string {
  const full = isAbsolute(path) ? resolve(path) : resolve(root, path), child = relative(root, full);
  if (child.startsWith("..") || isAbsolute(child)) throw new Error("Fixture and export paths must stay in the workspace.");
  return full;
}
function capturedBytes(record: CapturedFile): NativeBytes {
  const bytes = readFileSync(pathInWorkspace(record.path)); expect(digest(bytes)).toBe(record.sha256); expect(bytes.byteLength).toBe(record.bytes); return bytes;
}
function originalFile(record: CapturedFile & {filename: string}): File {return new File([new Uint8Array(capturedBytes(record)).buffer], record.filename, {lastModified: 0});}
function responseText(entry: CapturedEntry): string {return capturedBytes(entry.response).toString("utf8");}
function actual(entry: CapturedEntry): PetriAntiMultiWorkbench {
  const value = parseNativeJson(responseText(entry)); if (!isPetriAntiMultiWorkbench(value)) throw new Error(`Invalid actual Petri anti/multi alignment envelope ${entry.id}`); return value;
}
/** Independent lexical walk verifies every integer and float spelling. */
function numericPaths(raw: string): Map<string, string> {
  const tokens = raw.match(/"(?:[^"\\]|\\.)*"|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?|true|false|null|[{}\[\],:]/g) ?? [], values = new Map<string, string>(); let position = 0;
  const escape = (key: string) => key.replace(/~/g, "~0").replace(/\//g, "~1");
  const walk = (path: string): void => {
    const token = tokens[position++];
    if (token === "{") {
      while (tokens[position] !== "}") {const key = JSON.parse(tokens[position++]) as string; if (tokens[position++] !== ":") throw new Error("Invalid native object token"); walk(path + "/" + escape(key)); if (tokens[position] !== ",") break; ++position;}
      if (tokens[position++] !== "}") throw new Error("Invalid native object end");
    } else if (token === "[") {
      let i = 0; while (tokens[position] !== "]") {walk(path + "/" + i++); if (tokens[position] !== ",") break; ++position;}
      if (tokens[position++] !== "]") throw new Error("Invalid native array end");
    } else if (/^-?\d/.test(token)) values.set(path, token);
  };
  walk(""); expect(position).toBe(tokens.length); return values;
}
function holderAt(value: unknown, pointer: string): [object, string] {
  const names = pointer.slice(1).split("/").map(key => key.replace(/~1/g, "/").replace(/~0/g, "~")), key = names.pop()!; let holder = value;
  for (const name of names) holder = (holder as Record<string, unknown>)[name];
  if (holder === null || typeof holder !== "object") throw new Error("Numeric native pointer has no holder"); return [holder, key];
}
/** CSV is decoded independently of the production encoder, including multiline/quoted/NUL cells. */
function decodeCsv(raw: string): string[][] {
  const rows: string[][] = []; let row: string[] = [], cell = "", quoted = false, atStart = true;
  for (let i = 0; i < raw.length; ++i) {
    const char = raw[i];
    if (quoted) {if (char === '"') {if (raw[i + 1] === '"') {cell += '"'; ++i;} else quoted = false;} else cell += char; continue;}
    if (char === '"') {if (!atStart) throw new Error("Unexpected CSV quote"); quoted = true; atStart = false;}
    else if (char === ",") {row.push(cell); cell = ""; atStart = true;}
    else if (char === "\r" || char === "\n") {if (char === "\r" && raw[i + 1] === "\n") ++i; row.push(cell); rows.push(row); row = []; cell = ""; atStart = true;}
    else {cell += char; atStart = false;}
  }
  if (quoted) throw new Error("Unterminated CSV quoted cell"); if (row.length || cell !== "" || !atStart) {row.push(cell); rows.push(row);} return rows;
}
const expectedCell = (value: unknown): string => value === undefined ? "" : typeof value === "string" ? value : value !== null && typeof value === "object" ? nativeJson(value) : String(value);
/** Independent UTF-8/WTF-8 decoder verifies actual File/Blob bytes, including lone surrogates. */
function nativeBytesText(bytes: Uint8Array): string {
  let text = "";
  for (let i = 0; i < bytes.length;) {
    const lead = bytes[i++]; let code: number, count: number;
    if (lead < 0x80) {code = lead; count = 0;} else if (lead >= 0xc2 && lead <= 0xdf) {code = lead & 0x1f; count = 1;}
    else if (lead >= 0xe0 && lead <= 0xef) {code = lead & 0x0f; count = 2;} else if (lead >= 0xf0 && lead <= 0xf4) {code = lead & 0x07; count = 3;} else throw new Error("Invalid UTF-8 lead");
    for (let j = 0; j < count; ++j) {const tail = bytes[i++]; if (tail === undefined || tail < 0x80 || tail > 0xbf) throw new Error("Invalid UTF-8 continuation"); code = code << 6 | tail & 0x3f;}
    if (count === 1 && code < 0x80 || count === 2 && code < 0x800 || count === 3 && (code < 0x10000 || code > 0x10ffff)) throw new Error("Invalid UTF-8 code point"); text += String.fromCodePoint(code);
  }
  return text;
}
function formFromCapture(entry: CapturedEntry): PetriAntiMultiForm {
  const form = defaultPetriAntiMultiForm(entry.algorithm); form.inputShape = entry.input_shape; form.inputFormat = entry.input_format; form.modelFormat = entry.model_format;
  for (const [key, value] of entry.fields) {
    if (key === "alignment_selectors_json") for (const [name, selector] of Object.entries(JSON.parse(value) as Record<string, string>)) {form.fields[name] = selector; if (selector === "") form.explicitEmptySelectors[name] = true;}
    else if (key === "trace_index") form.traceIndex = value;
    else if (key.startsWith("pnml_")) form.pnmlFields[key] = value;
    else if (["case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter"].includes(key) || key.startsWith("xes_")) form.importFields[key] = value;
    else if (!["algorithm", "input_shape", "input_format", "model_format"].includes(key)) form.fields[key] = value;
  }
  return form;
}
async function runFromCapture(entry: CapturedEntry): Promise<PetriAntiMultiRun> {
  const raw = responseText(entry), value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields: entry.fields});
  if (!isPetriAntiMultiWorkbench(value)) throw new Error(`Invalid captured native Petri anti/multi alignment ${entry.id}`);
  return {value, raw, fields: entry.fields, tables: petriAntiMultiTables(value, entry.fields), http_status: entry.response.status,
    inputs: {input: originalFile(entry.input), model: originalFile(entry.model), input_sha256: entry.input.sha256, model_sha256: entry.model.sha256}};
}
interface ExportProof {
  id: string; verified_numeric_literals: number; tables: number; rows: number; csv_cells: number; selected_records: number;
  table_details: Array<{id: string; title: string; columns: string[]; rows: number; records: number; csv: string; records_json: string | null}>;
  artifacts: Record<string, {bytes: number; sha256: string}>;
}
const proofs: ExportProof[] = [];
function saveArtifact(id: string, name: string, bytes: Uint8Array | string, proof: ExportProof): void {
  const data = Buffer.from(typeof bytes === "string" ? workflowInputNativeBytes(bytes) : bytes);
  if (typeof bytes === "string") expect(nativeBytesText(data)).toBe(bytes); proof.artifacts[name] = {bytes: data.byteLength, sha256: digest(data)};
  if (!exportDirectory) return; if (!/^[a-z0-9][a-z0-9._-]*$/i.test(id)) throw new Error("Unsafe fixture identifier");
  const path = pathInWorkspace(resolve(exportDirectory, id, name)); mkdirSync(dirname(path), {recursive: true}); writeFileSync(path, data); expect(readFileSync(path).equals(data)).toBe(true);
}

afterEach(() => vi.unstubAllGlobals());

describe("actual own-server independent Petri anti/multi PNML fixtures", () => {
  it("requires both objectives over original CSV/XES/table logs, selected traces and literal words", () => {
    for (const algorithm of PETRI_ANTI_MULTI_ALGORITHMS) {
      for (const shape of ["log", "trace", "words"] as const) expect(entries.some(entry => entry.algorithm === algorithm && entry.input_shape === shape && entry.executed && actual(entry).error === null)).toBe(true);
      for (const shape of ["log", "trace"] as const) for (const format of ["auto", "table"] as const) expect(entries.some(entry => entry.algorithm === algorithm && entry.input_shape === shape && entry.input_format === format && entry.executed && actual(entry).error === null)).toBe(true);
      for (const extension of [/\.csv$/i, /\.xes(?:\.gz)?$/i]) expect(entries.some(entry => entry.algorithm === algorithm && extension.test(entry.input.filename) && entry.executed && actual(entry).error === null)).toBe(true);
    }
    expect(entries.some(entry => entry.response.status === 400 && actual(entry).error !== null)).toBe(true);
    expect(entries.some(entry => actual(entry).result?.diagnostics.exhausted)).toBe(true);
  });
  for (const entry of entries) it(`${entry.id} preserves every original/native field, numeric spelling and complete export byte`, async () => {
    expect(entry.path).toBe("/api/align-petri-anti-multi-workbench"); expect(entry.method).toBe("POST"); expect(entry.response.status).toBe(entry.expected_status);
    const raw = responseText(entry), captured = await runFromCapture(entry), value = captured.value;
    expect(value.algorithm).toBe(entry.algorithm); expect(value.variant).toBe(entry.variant); expect(value.input_shape).toBe(entry.input_shape); expect(value.result_shape).toBe("aggregate"); expect(value.result_shape).toBe(entry.result_shape); expect(value.executed).toBe(entry.executed);
    expect(value.input_context.format).toBe(entry.input_format); expect(value.model_context.format).toBe(entry.model_format);
    for (const [context, file] of [[value.input_context, entry.input], [value.model_context, entry.model]] as const) {expect(context.dataset).toBe(file.filename); expect(context.original_input_bytes).toBe(file.bytes);}
    expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(entry.fields); expect(value.options_context.variant).toBe(entry.variant);
    vi.stubGlobal("fetch", vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))));
    const bound = await runPetriAntiMultiWorkbench(captured.inputs.input, captured.inputs.model, formFromCapture(entry)); expect(bound.raw).toBe(raw); expect(bound.inputs.input_sha256).toBe(entry.input.sha256); expect(bound.inputs.model_sha256).toBe(entry.model.sha256);
    const numeric = numericPaths(raw); for (const [pointer, literal] of numeric) {const [holder, key] = holderAt(value, pointer); expect(String(nativeCell(holder, key)), pointer).toBe(literal);} expect(numericPaths(nativeJson(value))).toEqual(numeric);
    const proof: ExportProof = {id: entry.id, verified_numeric_literals: numeric.size, tables: captured.tables.length, rows: 0, csv_cells: 0, selected_records: 0, table_details: [], artifacts: {}};
    saveArtifact(entry.id, "native.json", captured.raw, proof); expect(digest(workflowInputNativeBytes(captured.raw))).toBe(entry.response.sha256);
    const reportText = petriAntiMultiReport(captured), report = parseNativeJson(reportText) as {native_result: unknown; inputs: {input: {name: string; size: number; sha256: string}; model: {name: string; size: number; sha256: string}}; request: {fields: Array<[string, string]>}};
    expect(report.inputs.input).toMatchObject({name: entry.input.filename, size: entry.input.bytes, sha256: entry.input.sha256}); expect(report.inputs.model).toMatchObject({name: entry.model.filename, size: entry.model.bytes, sha256: entry.model.sha256}); expect(report.request.fields).toEqual(entry.fields); expect(nativeJson(report.native_result)).toBe(nativeJson(value));
    expect(new Map([...numericPaths(reportText)].filter(([path]) => path.startsWith("/native_result/")).map(([path, token]) => [path.slice("/native_result".length), token]))).toEqual(numeric); saveArtifact(entry.id, "report.json", reportText, proof);
    const requestText = nativeJson(nativeResponseRequest(value), 2); expect(JSON.parse(requestText).fields).toEqual(entry.fields); saveArtifact(entry.id, "request.json", requestText, proof); saveArtifact(entry.id, "request.bin", capturedBytes(entry.request_body), proof);
    for (const [name, file, record] of [["original-input.bin", captured.inputs.input, entry.input], ["original-model.bin", captured.inputs.model, entry.model]] as const) {const bytes = new Uint8Array(await file.arrayBuffer()); expect(Buffer.from(bytes).equals(capturedBytes(record))).toBe(true); saveArtifact(entry.id, name, bytes, proof);}
    for (const format of ["pnml", "checked_pnml", "source_xes"] as const) {
      if (value[format] === null) {expect(() => petriAntiMultiArtifactFile(captured, format)).toThrow(); expect(entry.exports[format]).toBeUndefined(); continue;}
      const file = petriAntiMultiArtifactFile(captured, format), bytes = new Uint8Array(await file.arrayBuffer()), observed = entry.exports[format]; expect(observed).toBeDefined(); if (!observed) throw new Error("Missing captured native Petri anti/multi artifact");
      expect(nativeBytesText(bytes)).toBe(value[format]); expect(Buffer.from(bytes).equals(capturedBytes(observed))).toBe(true); expect(digest(bytes)).toBe(observed.sha256); expect(file.name.endsWith("." + observed.format)).toBe(true); saveArtifact(entry.id, format + "." + observed.format, bytes, proof);
    }
    for (const [tableIndex, table] of captured.tables.entries()) {
      const csv = diagnosticCsv(table), decoded = decodeCsv(csv), bytes = new Uint8Array(await new Blob([workflowInputNativeBytes(csv)], {type: "text/csv;charset=utf-8"}).arrayBuffer());
      expect(decodeCsv(nativeBytesText(bytes))).toEqual(decoded); expect(decoded[0], table.id).toEqual(table.columns); expect(decoded.slice(1), table.id).toEqual(table.rows.map(row => row.map(expectedCell))); expect(decoded.slice(1).every(row => row.length === table.columns.length)).toBe(true);
      const fileName = petriAntiMultiTableFileName(table.id); expect(fileName).toMatch(/^[A-Za-z0-9_.%\-]+$/); expect(fileName.replace(/%([a-f0-9]{4})/g, (_, code: string) => String.fromCharCode(Number.parseInt(code, 16)))).toBe(table.id);
      proof.rows += table.rows.length; proof.csv_cells += decoded.reduce((sum, row) => sum + row.length, 0); proof.selected_records += table.records?.length ?? 0;
      const csvName = `tables/${String(tableIndex).padStart(4, "0")}.csv`, recordsName = table.records ? `records/${String(tableIndex).padStart(4, "0")}.json` : null;
      proof.table_details.push({id: table.id, title: table.title, columns: table.columns, rows: table.rows.length, records: table.records?.length ?? 0, csv: csvName, records_json: recordsName}); saveArtifact(entry.id, csvName, csv, proof);
      if (table.records && recordsName) {const text = nativeJson(table.records, 2); expect(nativeJson(parseNativeJson(text))).toBe(nativeJson(table.records)); saveArtifact(entry.id, recordsName, text, proof);}
    }
    const allCsv = petriAntiMultiAllTablesCsv(captured.tables), decodedAll = decodeCsv(allCsv), expected: string[][] = [];
    expect(decodeCsv(nativeBytesText(new Uint8Array(await new Blob([workflowInputNativeBytes(allCsv)]).arrayBuffer())))).toEqual(decodedAll);
    for (const table of captured.tables) {
      if (!table.rows.length) table.columns.forEach((column, column_index) => expected.push([table.id, table.title, "", String(column_index), column, ""]));
      table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => expected.push([table.id, table.title, String(row_index), String(column_index), column, expectedCell(row[column_index])])));
    }
    expect(decodedAll[0]).toEqual(["table_id", "table_title", "row_index", "column_index", "column", "value"]); expect(decodedAll.slice(1)).toEqual(expected); saveArtifact(entry.id, "all-tables.csv", allCsv, proof); proofs.push(proof);
  }, 30000);
  it("retains all original document metadata, native net arcs, checked logs, typed cells and aggregate sequences beyond paging", () => {
    let cellsChecked = 0, sequenceItems = 0, pagedRows = 0, pagedColumns = 0;
    for (const entry of entries) {
      const value = actual(entry), tables = petriAntiMultiTables(value, entry.fields), document = value.model_context.native_document;
      for (const key of ["net", "raw_source", "raw_sink", "initial_marking", "final_marking", "place_ids", "transition_ids", "initial_marking_keys", "final_marking_keys", "arc_occurrences", "stochastic_map"]) expect(document).toHaveProperty(key);
      expect(nativeJson(document.net)).toBe(nativeJson(value.native_net));
      for (const key of ["initial_marking", "final_marking", "place_ids", "transition_ids", "initial_marking_keys", "final_marking_keys", "arc_occurrences"] as const) if (Array.isArray(document[key])) expect(tables.find(table => table.id === `/model_context/native_document/${key}`)?.records).toBe(document[key]);
      const graph = petriAntiMultiGraph(value), transitions = value.native_net.transitions as Array<Record<string, unknown>>;
      expect(graph.nodes).toHaveLength(Number(value.native_net.place_count) + transitions.length); expect(graph.edges).toHaveLength(transitions.reduce((sum, transition) => sum + ["inputs", "outputs", "reset_inputs", "inhibitor_inputs"].reduce((count, key) => count + (transition[key] as unknown[]).length, 0), 0));
      expect(new Set(graph.nodes.map(node => node.id)).size).toBe(graph.nodes.length); expect(new Set(graph.edges.map(edge => edge.id)).size).toBe(graph.edges.length);
      if (value.input_before && value.input_after) expect(nativeJson(value.input_after)).toBe(nativeJson(value.input_before));
      for (const key of ["input_before", "input_after"] as const) {
        const original = value[key]; if (!original) continue;
        const rows = tables.find(table => table.id === `/${key}/rows`)!, cells = tables.find(table => table.id === `${key}_cells`)!; expect(rows.records).toBe(original.rows); expect(cells.records).toHaveLength(original.rows.reduce((sum, row) => sum + row.length, 0));
        original.rows.forEach((row, i) => {expect(nativeJson(rows.records![i])).toBe(nativeJson(row)); cellsChecked += row.length;});
        for (const record of cells.records ?? []) {const cell = record as {row_index: number; column_index: number; cell: unknown}; expect(cell.cell).toBe(original.rows[cell.row_index][cell.column_index]);}
      }
      for (const key of ["input_log", "checked_log"] as const) if (value[key]) expect(tables.find(table => table.id === `/${key}`)?.records?.some(record => nativeJson(record).includes('"traces"'))).toBe(true);
      if (value.result) for (const key of ["activities", "transitions"] as const) {
        const sequence = tables.find(table => table.id === `complete_${key}`)!; expect(sequence.records).toBe(value.result[key]); expect(sequence.rows).toHaveLength(value.result[key].length);
        value.result[key].forEach((_, i) => {expect(sequence.rows[i][1]).toBe(nativeCell(value.result![key], i)); ++sequenceItems;});
      }
      expect(tables.some(table => table.id === "complete_alignment_moves")).toBe(false);
      for (const table of tables) {
        if (table.rows.length > 25) {expect(diagnosticPage(table.rows, 1)).toHaveLength(Math.min(25, table.rows.length - 25)); expect(nativeJson(table.records?.at(-1))).toBeTruthy(); ++pagedRows;}
        if (table.columns.length > 8) {expect(diagnosticPage(table.columns, 1, 8)).toHaveLength(Math.min(8, table.columns.length - 8)); ++pagedColumns;}
      }
    }
    expect(cellsChecked).toBeGreaterThan(0); expect(sequenceItems).toBeGreaterThan(0); expect(pagedRows).toBeGreaterThan(0); expect(pagedColumns).toBeGreaterThan(0);
  });
  it("keeps every configured native option, marking presence/origin, PNML flag and returned/pruned/error state", () => {
    const origins = new Set<string>(); let nativeErrors = 0, pruning = 0, hexadecimal = 0, subnormal = 0, negativeZero = 0, wideInteger = 0;
    for (const entry of entries) {
      const value = actual(entry), options = value.options_context; expect(options.variant).toBe(PETRI_ANTI_MULTI_VARIANT); expect(options.variant_index).toBe(0);
      for (const key of ["variant", "variant_index", "exponent", "marking_limit", "max_states", "initial_marking", "final_marking", "activity_attribute"]) expect(options).toHaveProperty(key);
      expect(Object.hasOwn(options, "epsilon")).toBe(entry.algorithm === "anti");
      for (const [key, text] of entry.fields) {
        if (key === "alignment_selectors_json") for (const [name, literal] of Object.entries(JSON.parse(text) as Record<string, string>)) expect(value.input_context[name === "alignment_case_id_attribute" ? "case_id_attribute" : "activity_attribute"]).toBe(literal);
        if (PETRI_ANTI_MULTI_PNML_CONTROLS.some(([name]) => name === key)) expect(value.pnml_import_options[key.slice(5) as keyof typeof value.pnml_import_options]).toBe(["true", "1"].includes(text.toLowerCase()));
        const control = PETRI_ANTI_MULTI_CONTROLS[entry.algorithm].find(control => control.key === key); if (!control || control.kind === "selector") continue;
        const nativeKey = key.replace(/_json$/, ""), actualValue = options[nativeKey as keyof typeof options];
        if (control.kind === "number") {expect(Object.is(actualValue, petriAntiMultiNumber(text))).toBe(true); if (/0x/i.test(text)) ++hexadecimal; if (typeof actualValue === "number" && actualValue !== 0 && Math.abs(actualValue) < 2 ** -1022) ++subnormal; if (Object.is(actualValue, -0)) ++negativeZero;}
        else if (control.kind === "integer") {expect(String(nativeCell(options, nativeKey))).toBe(BigInt(text).toString()); if (BigInt(text) > BigInt(Number.MAX_SAFE_INTEGER)) ++wideInteger;}
        else {const expected = control.kind === "marking" ? nativeJson(parseNativeJson(text)) : `[${text.split(",").map(item => BigInt(item.trim()).toString()).join(",")}]`; expect(nativeJson(actualValue)).toBe(expected);}
      }
      for (const name of ["initial", "final"] as const) {
        const json = entry.fields.find(([key]) => key === `${name}_marking_json`), legacy = entry.fields.find(([key]) => key === `${name}_marking`);
        const origin = json ? (parseNativeJson(json[1]) as unknown[]).length ? "explicit_json" : "explicit_empty_native_default" : legacy ? "explicit_legacy" : "pnml";
        expect(value.marking_context[`${name}_origin`]).toBe(origin); origins.add(origin);
      }
      if (value.error !== null) {expect(value.result).toBeNull(); expect(value.error.type).toBe("native_error"); expect(petriAntiMultiSummary(value)).toContain("failed"); ++nativeErrors; continue;}
      expect(value.executed).toBe(true); const result = value.result!; expect(nativeCell(result, "cost")).toBe(nativeCell(result, "objective"));
      for (const key of ["computed", "completed", "timed_out", "variant", "variant_index", "traces", "moves", "alignment"]) expect(result).not.toHaveProperty(key);
      expect(Object.hasOwn(result, "precision")).toBe(entry.algorithm === "anti"); expect(Object.hasOwn(result, "max_distance_to_log")).toBe(entry.algorithm === "multi");
      if (result.diagnostics.exhausted) {expect(petriAntiMultiSummary(value)).toContain("pruned search branches"); ++pruning;}
    }
    expect([...origins].sort()).toEqual(["explicit_empty_native_default", "explicit_json", "explicit_legacy", "pnml"]); expect(nativeErrors).toBeGreaterThan(0); expect(pruning).toBeGreaterThan(0); expect(hexadecimal).toBeGreaterThan(0); expect(subnormal).toBeGreaterThan(0); expect(negativeZero).toBeGreaterThan(0); expect(wideInteger).toBeGreaterThan(0);
  });
  it("binds the two original uploads and blocks source-independent mismatches, cancellation and stale generations", async () => {
    const entry = entries.find(entry => entry.input_shape === "trace" && actual(entry).error === null)!, input = originalFile(entry.input), model = originalFile(entry.model), form = formFromCapture(entry), raw = responseText(entry);
    const fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))); vi.stubGlobal("fetch", fetcher);
    const run = await runPetriAntiMultiWorkbench(input, model, form); expect(run.inputs.input_sha256).toBe(entry.input.sha256); expect(run.inputs.model_sha256).toBe(entry.model.sha256); expect(run.raw).toBe(raw);
    expect(fetcher.mock.calls[0][0]).toBe("/api/align-petri-anti-multi-workbench"); expect(fetcher.mock.calls[0][1].body.get("input_file")).toBe(input); expect(fetcher.mock.calls[0][1].body.get("model_file")).toBe(model); expect(fetcher.mock.calls[0][1].body.get("source_id")).toBeNull(); expect(fetcher.mock.calls[0][1].body.get("include_source")).toBeNull();
    expect(Object.fromEntries(petriAntiMultiFields(form, input.name)).algorithm).toBe(entry.algorithm);
    await expect(runPetriAntiMultiWorkbench(new File([await input.arrayBuffer()], "other-input.csv"), model, form)).rejects.toThrow(/does not match/); await expect(runPetriAntiMultiWorkbench(input, new File([await model.arrayBuffer()], "other-model.pnml"), form)).rejects.toThrow(/does not match/);
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(raw));})); await expect(runPetriAntiMultiWorkbench(input, model, form, controller.signal)).rejects.toThrow(/abort/i);
    const gate = new PetriAntiMultiRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false); expect(gate.accepts(gate.issue(), controller.signal)).toBe(false);
  });
  it("publishes the complete independently decoded export proof when requested by the frozen recipe", () => {
    expect(proofs.length).toBe(entries.length); if (!exportDirectory) return;
    const path = pathInWorkspace(resolve(exportDirectory, "export-integrity.json")); mkdirSync(dirname(path), {recursive: true});
    writeFileSync(path, JSON.stringify({complete: true, manifest: manifestPath, manifest_sha256: digest(readFileSync(manifestPath)), entries: proofs,
      verified_numeric_literals: proofs.reduce((sum, proof) => sum + proof.verified_numeric_literals, 0), csv_cells: proofs.reduce((sum, proof) => sum + proof.csv_cells, 0), selected_records: proofs.reduce((sum, proof) => sum + proof.selected_records, 0)}, null, 2) + "\n", "utf8");
  });
});
