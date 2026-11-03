import { afterEach, describe, expect, it, vi } from "vitest";
import {
  defaultWorkflowInputForm, isWorkflowInputDiscovery, runWorkflowInput, workflowInputAllTablesCsv,
  workflowInputModelFile, workflowInputNativeBytes, workflowInputReport, workflowInputSummary, workflowInputTables,
  type WorkflowInputDiscovery, type WorkflowInputForm, type WorkflowInputRun,
} from "../src/workflow-input-workbench";
import { diagnosticCsv, diagnosticPage } from "../src/model-diagnostics";
import { nativeCell, nativeJson, parseNativeJson } from "../src/native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "../src/native-response";

// This browser project has no @types/node dependency. Keep the fixture-only
// Node surface explicit; production code and compiler configuration stay shared.
interface NativeBytes extends Uint8Array {toString(encoding?: string): string; equals(other: Uint8Array): boolean;}
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {createHash} = await nodeModule("node:crypto") as {createHash(algorithm: string): {update(data: string | Uint8Array): {digest(encoding: "hex"): string}}};
const {existsSync, mkdirSync, readFileSync, writeFileSync} = await nodeModule("node:fs") as {
  existsSync(path: string): boolean; mkdirSync(path: string, options: {recursive: boolean}): unknown;
  readFileSync: { (path: string): NativeBytes; (path: string, encoding: "utf8"): string };
  writeFileSync(path: string, data: string | Uint8Array, encoding?: "utf8"): void;
};
const {dirname, isAbsolute, relative, resolve} = await nodeModule("node:path") as {
  dirname(path: string): string; isAbsolute(path: string): boolean; relative(from: string, to: string): string; resolve(...parts: string[]): string;
};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url: string): string};
const {Buffer} = await nodeModule("node:buffer") as {Buffer: {from(data: string | Uint8Array): NativeBytes}};
const environment = (globalThis as unknown as {process: {env: Record<string, string | undefined>}}).process.env;

interface CapturedFile {path: string; sha256: string;}
interface CapturedEntry {
  id: string; family: "alpha" | "heuristic"; variant: "classic" | "plus" | "plusplus";
  adapter: WorkflowInputForm["shape"]; format: WorkflowInputForm["format"];
  fields: Array<[string, string]>; path: string; method: string; expected_status: number;
  input: CapturedFile & {filename: string; bytes: number};
  response: CapturedFile & {status: number}; computed?: boolean; error?: unknown;
  exports?: Partial<Record<"pnml" | "bpmn" | "ptml", CapturedFile>>;
}
interface CapturedManifest {entries: CapturedEntry[]; [key: string]: unknown;}
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const root = resolve(frontend, "../..");
const captureDirectory = environment.MINERA_WORKFLOW_INPUT_CAPTURE_DIR;
const manifestPath = captureDirectory ? resolve(captureDirectory, "request-manifest.json") : resolve(frontend, "tests/fixtures/workflow-input-workbench-request-manifest.json");
if (!existsSync(manifestPath)) throw new Error("Complete own-server workflow input captures are required. Supply MINERA_WORKFLOW_INPUT_CAPTURE_DIR or publish the portable fixture manifest.");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as CapturedManifest;
if (!Array.isArray(manifest.entries) || !manifest.entries.length) throw new Error("The workflow input request manifest has no real captured entries.");
const entries = manifest.entries.filter(entry => entry.path === "/api/discover-workflow-input");
const exportDirectory = environment.MINERA_WORKFLOW_INPUT_EXPORT_DIR;
const digest = (bytes: string | Uint8Array): string => createHash("sha256").update(bytes).digest("hex");
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
function pathInWorkspace(path: string): string {
  const full = isAbsolute(path) ? resolve(path) : resolve(root, path), child = relative(root, full);
  if (child.startsWith("..") || isAbsolute(child)) throw new Error("Capture and export paths must stay in the workspace.");
  return full;
}
function capturedBytes(file: CapturedFile): Uint8Array {
  const bytes = readFileSync(pathInWorkspace(file.path));
  expect(digest(bytes)).toBe(file.sha256); return bytes;
}
function inputFile(entry: CapturedEntry): File {
  const bytes = capturedBytes(entry.input); expect(bytes.byteLength).toBe(entry.input.bytes);
  return new File([new Uint8Array(bytes).buffer], entry.input.filename, {lastModified: 0});
}
function responseText(entry: CapturedEntry): string {return Buffer.from(capturedBytes(entry.response)).toString("utf8");}
function actual(entry: CapturedEntry): WorkflowInputDiscovery {
  const value = parseNativeJson(responseText(entry));
  if (!isWorkflowInputDiscovery(value)) throw new Error(`Invalid actual native workflow envelope ${entry.id}`);
  return value;
}
/** Independent lexical walk verifies every native numeric spelling, including arbitrary-width integer tokens. */
function numericPaths(raw: string): Map<string, string> {
  const tokens = raw.match(/"(?:[^"\\]|\\.)*"|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?|true|false|null|[{}\[\],:]/g) ?? [];
  const values = new Map<string, string>(); let position = 0;
  const escape = (key: string) => key.replace(/~/g, "~0").replace(/\//g, "~1");
  const walk = (path: string): void => {
    const token = tokens[position++];
    if (token === "{") {
      while (tokens[position] !== "}") {const key = JSON.parse(tokens[position++]) as string; if (tokens[position++] !== ":") throw new Error("Invalid capture lexical object"); walk(path + "/" + escape(key)); if (tokens[position] !== ",") break; ++position;}
      if (tokens[position++] !== "}") throw new Error("Invalid capture lexical object end");
    } else if (token === "[") {
      let i = 0; while (tokens[position] !== "]") {walk(path + "/" + i++); if (tokens[position] !== ",") break; ++position;}
      if (tokens[position++] !== "]") throw new Error("Invalid capture lexical array end");
    } else if (/^-?\d/.test(token)) values.set(path, token);
  };
  walk(""); expect(position).toBe(tokens.length); return values;
}
function holderAt(value: unknown, pointer: string): [object, string] {
  const names = pointer.slice(1).split("/").map(key => key.replace(/~1/g, "/").replace(/~0/g, "~")), key = names.pop()!;
  let holder = value;
  for (const name of names) holder = (holder as Record<string, unknown>)[name];
  if (holder === null || typeof holder !== "object") throw new Error("Numeric native pointer has no holder");
  return [holder, key];
}
/** Strict decoder checks complete cells independently of the production CSV encoder. */
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
  if (quoted) throw new Error("Unterminated CSV quoted cell");
  if (row.length || cell !== "" || !atStart) {row.push(cell); rows.push(row);}
  return rows;
}
const expectedCell = (value: unknown): string => value === undefined ? "" : typeof value === "string" ? value : value !== null && typeof value === "object" ? nativeJson(value) : String(value);
/** Independent native UTF-8/WTF-8 decoder verifies the bytes delivered by File and Blob. */
function nativeBytesText(bytes: Uint8Array): string {
  let text = "";
  for (let i = 0; i < bytes.length;) {
    const lead = bytes[i++]; let code: number, count: number;
    if (lead < 0x80) {code = lead; count = 0;}
    else if (lead >= 0xc2 && lead <= 0xdf) {code = lead & 0x1f; count = 1;}
    else if (lead >= 0xe0 && lead <= 0xef) {code = lead & 0x0f; count = 2;}
    else if (lead >= 0xf0 && lead <= 0xf4) {code = lead & 0x07; count = 3;}
    else throw new Error("Invalid native UTF-8 lead byte");
    for (let j = 0; j < count; ++j) {const tail = bytes[i++]; if (tail === undefined || tail < 0x80 || tail > 0xbf) throw new Error("Invalid native UTF-8 continuation"); code = code << 6 | tail & 0x3f;}
    if (count === 1 && code < 0x80 || count === 2 && code < 0x800 || count === 3 && (code < 0x10000 || code > 0x10ffff)) throw new Error("Invalid native UTF-8 code point");
    text += String.fromCodePoint(code);
  }
  return text;
}
function formFromCapture(entry: CapturedEntry): WorkflowInputForm {
  const form = defaultWorkflowInputForm(); form.family = entry.family; form.shape = entry.adapter; form.format = entry.format;
  if (entry.family === "alpha") form.alphaVariant = entry.variant as "classic" | "plus"; else form.heuristicVariant = entry.variant as "classic" | "plusplus";
  for (const [key, value] of entry.fields) {
    if (key === "discovery_selectors_json") {
      const selectors = JSON.parse(value) as Record<string, string>;
      for (const [name, selector] of Object.entries(selectors)) {form.fields[name] = selector; if (selector === "") form.explicitEmptySelectors[name] = true;}
    } else if (["case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter"].includes(key) || key.startsWith("xes_")) form.importFields[key] = value;
    else if (!["discovery_family", "input_shape", "input_format", "alpha_variant", "heuristic_variant"].includes(key)) form.fields[key] = value;
  }
  return form;
}
async function runFromCapture(entry: CapturedEntry): Promise<WorkflowInputRun> {
  const raw = responseText(entry), fields = entry.fields;
  const value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields});
  if (!isWorkflowInputDiscovery(value)) throw new Error(`Invalid captured native workflow ${entry.id}`);
  return {value, raw, fields, tables: workflowInputTables(value, fields), input: inputFile(entry), input_sha256: entry.input.sha256, http_status: entry.response.status};
}
interface ExportProof {
  id: string; verified_numeric_literals: number; tables: number; rows: number; csv_cells: number; selected_records: number;
  table_details: Array<{id: string; title: string; columns: string[]; rows: number; records: number; csv: string; records_json: string | null}>;
  artifacts: Record<string, {bytes: number; sha256: string}>;
}
const proofs: ExportProof[] = [];
function saveArtifact(id: string, name: string, bytes: Uint8Array | string, proof: ExportProof): void {
  const data = typeof bytes === "string" ? Buffer.from(workflowInputNativeBytes(bytes)) : Buffer.from(bytes);
  if (typeof bytes === "string") expect(nativeBytesText(data)).toBe(bytes);
  proof.artifacts[name] = {bytes: data.byteLength, sha256: digest(data)};
  if (!exportDirectory) return;
  if (!/^[a-z0-9][a-z0-9._-]*$/i.test(id)) throw new Error("Unsafe captured fixture ID");
  const path = pathInWorkspace(resolve(exportDirectory, id, name)); mkdirSync(dirname(path), {recursive: true}); writeFileSync(path, data);
  expect(readFileSync(path).equals(data)).toBe(true);
}
afterEach(() => vi.unstubAllGlobals());

describe("actual own-server workflow input discovery fixtures", () => {
  it("requires all four native algorithms over original CSV, XES and typed-table input adapters", () => {
    expect(entries.length).toBeGreaterThan(0);
    for (const prefix of ["csv", "xes", "typed"]) for (const method of ["alpha-classic", "alpha-plus", "heuristic-classic", "heuristic-plusplus"]) for (const shape of ["log", "stream", "dataframe", "lazyframe"]) expect(entries.some(entry => entry.id === `${prefix}-${method}-${shape}`)).toBe(true);
    for (const id of ["mutable-success", "mutable-datetime-failure", "alpha-plus-detached", "typed-alpha-classic-null-blank", "typed-alpha-plus-null-blank", "alpha-dfg-sparse", "alpha-dfg-explicit-empty", "alpha-dfg-clean", "alpha-dfg-graph", "alpha-dfg-wide", "heuristic-dfg-full", "heuristic-dfg-empty", "heuristic-dfg-wide", "heuristic-dfg-nonfinite", "heuristic-dfg-deep"]) expect(entries.some(entry => entry.id === id)).toBe(true);
  });
  for (const entry of entries) it(`${entry.id} preserves the actual response, original bytes, every numeric literal and complete native exports`, async () => {
    const raw = responseText(entry), parsed = parseNativeJson(raw);
    expect(entry.method).toBe("POST"); expect(entry.response.status).toBe(entry.expected_status);
    if (!isWorkflowInputDiscovery(parsed)) {
      expect(entry.response.status).toBeGreaterThanOrEqual(400);
      expect(object(parsed) && typeof parsed.error === "string").toBe(true); expect(entry.id.startsWith("reject-")).toBe(true);
      capturedBytes(entry.input); return;
    }
    const captured = await runFromCapture(entry), value = captured.value;
    expect(value.method).toBe(entry.family); expect(value.variant).toBe(entry.variant);
    expect(value.input_context.adapter).toBe(entry.adapter); expect(value.input_context.format).toBe(entry.format);
    expect(value.input_context.dataset).toBe(entry.input.filename); expect(value.input_context.original_input_bytes).toBe(entry.input.bytes);
    expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(entry.fields);
    const numeric = numericPaths(raw);
    for (const [pointer, literal] of numeric) {const [holder, key] = holderAt(value, pointer); expect(String(nativeCell(holder, key)), pointer).toBe(literal);}
    expect(numericPaths(nativeJson(value))).toEqual(numeric);
    const proof: ExportProof = {id: entry.id, verified_numeric_literals: numeric.size, tables: captured.tables.length, rows: 0, csv_cells: 0, selected_records: 0, table_details: [], artifacts: {}};
    saveArtifact(entry.id, "native.json", captured.raw, proof);
    expect(digest(captured.raw)).toBe(entry.response.sha256);
    const reportText = workflowInputReport(captured), report = parseNativeJson(reportText) as {native_result: unknown; input: {name: string; size: number; sha256: string}; request: {fields: Array<[string, string]>}};
    expect(report.input).toMatchObject({name: entry.input.filename, size: entry.input.bytes, sha256: entry.input.sha256}); expect(report.request.fields).toEqual(entry.fields);
    expect(nativeJson(report.native_result)).toBe(nativeJson(value));
    const reportNumbers = new Map([...numericPaths(reportText)].filter(([path]) => path.startsWith("/native_result/")).map(([path, token]) => [path.slice("/native_result".length), token]));
    expect(reportNumbers).toEqual(numeric);
    saveArtifact(entry.id, "report.json", reportText, proof);
    const requestText = nativeJson(nativeResponseRequest(value), 2); expect(JSON.parse(requestText).fields).toEqual(entry.fields); saveArtifact(entry.id, "request.json", requestText, proof);
    const originalBytes = new Uint8Array(await captured.input.arrayBuffer()); expect(digest(originalBytes)).toBe(entry.input.sha256); saveArtifact(entry.id, "original-input.bin", originalBytes, proof);
    for (const format of ["pnml", "bpmn", "ptml"] as const) {
      if (value[format] === null) {expect(() => workflowInputModelFile(captured, format)).toThrow(); continue;}
      const file = workflowInputModelFile(captured, format), bytes = new Uint8Array(await file.arrayBuffer());
      expect(nativeBytesText(bytes)).toBe(value[format]);
      const observed = entry.exports?.[format]; expect(observed).toBeDefined(); if (!observed) throw new Error("Missing actual captured model export");
      expect(Buffer.from(bytes).equals(capturedBytes(observed))).toBe(true); expect(digest(bytes)).toBe(observed.sha256); saveArtifact(entry.id, "model." + format, bytes, proof);
    }
    for (const [tableIndex, table] of captured.tables.entries()) {
      const csv = diagnosticCsv(table), decoded = decodeCsv(csv);
      const csvFile = new Blob([workflowInputNativeBytes(csv)], {type: "text/csv;charset=utf-8"});
      expect(decodeCsv(nativeBytesText(new Uint8Array(await csvFile.arrayBuffer())))).toEqual(decoded);
      expect(decoded[0], table.id).toEqual(table.columns); expect(decoded.slice(1), table.id).toEqual(table.rows.map(row => row.map(expectedCell)));
      expect(decoded.slice(1).every(row => row.length === table.columns.length), table.id).toBe(true);
      proof.rows += table.rows.length; proof.csv_cells += decoded.reduce((sum, row) => sum + row.length, 0); proof.selected_records += table.records?.length ?? 0;
      const csvName = `tables/${String(tableIndex).padStart(4, "0")}.csv`, recordsName = table.records ? `records/${String(tableIndex).padStart(4, "0")}.json` : null;
      proof.table_details.push({id: table.id, title: table.title, columns: table.columns, rows: table.rows.length, records: table.records?.length ?? 0, csv: csvName, records_json: recordsName});
      saveArtifact(entry.id, csvName, csv, proof);
      if (table.records && recordsName) {const text = nativeJson(table.records, 2); expect(nativeJson(parseNativeJson(text))).toBe(nativeJson(table.records)); saveArtifact(entry.id, recordsName, text, proof);}
    }
    const allCsv = workflowInputAllTablesCsv(captured), decodedAll = decodeCsv(allCsv), expected: string[][] = [];
    expect(decodeCsv(nativeBytesText(new Uint8Array(await new Blob([workflowInputNativeBytes(allCsv)]).arrayBuffer())))).toEqual(decodedAll);
    for (const table of captured.tables) {
      if (!table.rows.length) table.columns.forEach((column, column_index) => expected.push([table.id, table.title, "", String(column_index), column, ""]));
      table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => expected.push([table.id, table.title, String(row_index), String(column_index), column, expectedCell(row[column_index])])));
    }
    expect(decodedAll[0]).toEqual(["table_id", "table_title", "row_index", "column_index", "column", "value"]); expect(decodedAll.slice(1)).toEqual(expected);
    saveArtifact(entry.id, "all-tables.csv", allCsv, proof); proofs.push(proof);
  });
  // About 20,000 cells with seven exact checks each. Each check is the
  // Object.is comparison toBe makes, collected into one assertion: a vitest
  // expect() per check made this test take seconds and time out under load.
  it("retains every before/after table cell, presence state, dtype and complete selected record", () => {
    let cellsChecked = 0, pagesChecked = 0, mismatchCount = 0;
    const mismatches: string[] = [];
    const same = (actual: unknown, expected: unknown, where: string) => {
      if (Object.is(actual, expected)) return;
      const show = (value: unknown) => {try {return value === undefined ? "undefined" : nativeJson(value);} catch {return String(value);}};
      if (++mismatchCount <= 20) mismatches.push(`${where}: ${show(actual)} is not ${show(expected)}`);
    };
    for (const entry of entries) {
      const parsed = parseNativeJson(responseText(entry)); if (!isWorkflowInputDiscovery(parsed)) continue;
      const tables = workflowInputTables(parsed, entry.fields);
      for (const name of ["input_before", "input_after"] as const) {
        const table = parsed[name]; if (!table) continue;
        const flat = tables.find(item => item.id === name + "_cells")!;
        expect(flat.rows).toHaveLength(table.rows.length * table.columns.length);
        expect(flat.records).toHaveLength(flat.rows.length);
        table.rows.forEach((row, row_index) => row.forEach((cell, column_index) => {
          const i = row_index * table.columns.length + column_index, record = flat.records![i] as {cell: unknown}, where = `${entry.id} ${name} row ${row_index} column ${column_index}`;
          same(flat.rows[i][0], row_index, where + " row index"); same(flat.rows[i][1], column_index, where + " column index"); same(flat.rows[i][2], table.columns[column_index], where + " column");
          same(flat.rows[i][3], table.dtypes?.[column_index], where + " dtype"); same(flat.rows[i][4], cell === null ? "missing" : cell.null ? "present null" : "present", where + " presence");
          same(record.cell, cell, where + " record cell"); same(nativeJson(record.cell), nativeJson(cell), where + " record cell JSON"); ++cellsChecked;
        }));
        if (flat.rows.length > 25) {expect(diagnosticPage(flat.rows, 1)).toHaveLength(Math.min(25, flat.rows.length - 25)); ++pagesChecked;}
      }
    }
    expect(mismatches).toEqual([]); expect(mismatchCount).toBe(0);
    expect(cellsChecked).toBeGreaterThan(0); expect(pagesChecked).toBeGreaterThan(0);
  }, 30_000);
  it("retains failed mutable tables and completed detached/native-only results as distinct real outcomes", () => {
    const failed = actual(entries.find(entry => entry.id === "mutable-datetime-failure")!), detached = actual(entries.find(entry => entry.id === "alpha-plus-detached")!);
    expect(failed.computed).toBe(false); expect(failed.error).not.toBeNull(); expect(failed.input_before).not.toBeNull(); expect(failed.input_after).not.toBeNull(); expect(failed.result).toBeNull();
    expect(detached.computed).toBe(true); expect(detached.alpha_discovery).not.toBeNull(); expect(detached.native_net).not.toBeNull();
    expect((detached.alpha_discovery!.detached_initial_marking as unknown[]).length + (detached.alpha_discovery!.detached_final_marking as unknown[]).length).toBeGreaterThan(0);
    if (failed.input_context.input_case_count === null) expect(workflowInputSummary({value: failed} as WorkflowInputRun)).toContain("Unknown input case count");
  });
  it("uses only the independent source-free route and rejects a detached file response", async () => {
    const entry = entries.find(entry => entry.id === "csv-alpha-classic-log")!, file = inputFile(entry), form = formFromCapture(entry), raw = responseText(entry);
    const fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))); vi.stubGlobal("fetch", fetcher);
    const result = await runWorkflowInput(file, form); expect(result.input_sha256).toBe(entry.input.sha256); expect(result.raw).toBe(raw);
    expect(fetcher).toHaveBeenCalledTimes(1); expect(fetcher.mock.calls[0][0]).toBe("/api/discover-workflow-input");
    expect(fetcher.mock.calls[0][1].body.get("source_id")).toBeNull();
    await expect(runWorkflowInput(new File([await file.arrayBuffer()], "other-file.csv"), form)).rejects.toThrow(/does not match/);
  });
  it("records the independently decoded export proof when the standalone validation recipe requests artifacts", () => {
    expect(proofs.length).toBe(entries.filter(entry => isWorkflowInputDiscovery(parseNativeJson(responseText(entry)))).length);
    if (!exportDirectory) return;
    const path = pathInWorkspace(resolve(exportDirectory, "export-integrity.json")); mkdirSync(dirname(path), {recursive: true});
    writeFileSync(path, JSON.stringify({complete: true, manifest: manifestPath, manifest_sha256: digest(readFileSync(manifestPath)), entries: proofs,
      verified_numeric_literals: proofs.reduce((sum, proof) => sum + proof.verified_numeric_literals, 0), csv_cells: proofs.reduce((sum, proof) => sum + proof.csv_cells, 0), selected_records: proofs.reduce((sum, proof) => sum + proof.selected_records, 0)}, null, 2) + "\n", "utf8");
  });
});
