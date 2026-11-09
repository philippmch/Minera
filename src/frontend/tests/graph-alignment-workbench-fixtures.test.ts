import {afterEach, describe, expect, it, vi} from "vitest";
import {
  GraphAlignmentRequestGate, defaultGraphAlignmentForm, graphAlignmentAllTablesCsv, graphAlignmentArtifactFile,
  graphAlignmentFields, graphAlignmentReport, graphAlignmentSucceeded, graphAlignmentSummary, graphAlignmentTables,
  isGraphAlignmentWorkbench, runGraphAlignmentWorkbench,
  type GraphAlignmentForm, type GraphAlignmentMode, type GraphAlignmentRun, type GraphAlignmentWorkbench,
} from "../src/graph-alignment-workbench";
import {workflowInputNativeBytes} from "../src/workflow-input-workbench";
import {diagnosticCsv, diagnosticPage} from "../src/model-diagnostics";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeResponseJson, nativeResponseRequest, readNativeResponse} from "../src/native-response";

// Fixture-only Node types keep the browser project's dependencies unchanged.
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
  id: string; mode: GraphAlignmentMode; input_format: GraphAlignmentForm["inputFormat"]; input_shape: GraphAlignmentForm["inputShape"];
  model_format: GraphAlignmentForm["modelFormat"]; result_shape: "trace" | "log"; executed: boolean; error: unknown;
  fields: Array<[string, string]>; path: string; method: string; expected_status: number; headers: Record<string, string>;
  input: CapturedFile & {filename: string}; model: CapturedFile & {filename: string}; request_body: CapturedFile;
  response: CapturedFile & {status: number}; exports: Partial<Record<"model" | "projected_xes", CapturedFile & {format: string}>>;
}
interface CapturedManifest {complete: boolean; feature: number; entries: CapturedEntry[]; [key: string]: unknown;}
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), ".."), root = resolve(frontend, "../..");
const captureDirectory = environment.MINERA_GRAPH_ALIGNMENT_CAPTURE_DIR;
const manifestPath = captureDirectory ? resolve(captureDirectory, "request-manifest.json") : resolve(frontend, "tests/fixtures/graph-alignment-workbench-request-manifest.json");
if (!existsSync(manifestPath)) throw new Error("Complete own-server graph alignment captures are required. Supply MINERA_GRAPH_ALIGNMENT_CAPTURE_DIR or publish the portable fixture manifest.");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as CapturedManifest;
if (manifest.complete !== true || manifest.feature !== 52 || !Array.isArray(manifest.entries) || !manifest.entries.length) throw new Error("The graph alignment request manifest is not a complete real capture.");
// A model file that is not well-formed XML, or a PTML tag the reference does not know, is refused by
// the model reader before the workbench runs (Review AH2): a plain 400 error, not a workbench envelope.
const readerRefusal = (entry: {expected_status: number; error?: unknown}) => entry.expected_status === 400 &&
  typeof entry.error === "string" && /^(XML is not well-formed|PTML unknown tag)/.test(entry.error);
const readerRefusals = manifest.entries.filter(readerRefusal);
const entries = manifest.entries.filter(entry => !readerRefusal(entry));
const exportDirectory = environment.MINERA_GRAPH_ALIGNMENT_EXPORT_DIR;
const digest = (bytes: string | Uint8Array): string => createHash("sha256").update(bytes).digest("hex");
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
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
function actual(entry: CapturedEntry): GraphAlignmentWorkbench {
  const value = parseNativeJson(responseText(entry)); if (!isGraphAlignmentWorkbench(value)) throw new Error(`Invalid actual graph alignment envelope ${entry.id}`); return value;
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
function formFromCapture(entry: CapturedEntry): GraphAlignmentForm {
  const form = defaultGraphAlignmentForm(entry.mode); form.inputFormat = entry.input_format; form.inputShape = entry.input_shape; form.modelFormat = entry.model_format;
  for (const [key, value] of entry.fields) {
    if (key === "alignment_selectors_json") for (const [name, selector] of Object.entries(JSON.parse(value) as Record<string, string>)) {form.fields[name] = selector; if (selector === "") form.explicitEmptySelectors[name] = true;}
    else if (key === "projection") form.projection = value as GraphAlignmentForm["projection"];
    else if (key === "trace_index") form.traceIndex = value;
    else if (key.startsWith("pnml_")) form.pnmlFields[key] = value;
    else if (["case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter"].includes(key) || key.startsWith("xes_")) form.importFields[key] = value;
    else if (!["mode", "input_shape", "input_format", "model_format"].includes(key)) form.fields[key] = value;
  }
  return form;
}
async function runFromCapture(entry: CapturedEntry): Promise<GraphAlignmentRun> {
  const raw = responseText(entry), value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields: entry.fields});
  if (!isGraphAlignmentWorkbench(value)) throw new Error(`Invalid captured native graph alignment ${entry.id}`);
  return {value, raw, fields: entry.fields, tables: graphAlignmentTables(value, entry.fields), http_status: entry.response.status,
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

describe("actual own-server independent graph alignment fixtures", () => {
  it("requires both algorithms over original CSV/XES/table and direct native trace words", () => {
    expect(entries.length).toBeGreaterThan(0);
    for (const mode of ["DFG_CLASSIC", "RECOMPOS_MAXIMAL"] as const) for (const shape of ["log", "trace", "words"] as const) expect(entries.some(entry => entry.mode === mode && entry.input_shape === shape && entry.executed)).toBe(true);
    for (const mode of ["DFG_CLASSIC", "RECOMPOS_MAXIMAL"] as const) for (const format of ["auto", "table", "trace_words"] as const) expect(entries.some(entry => entry.mode === mode && entry.input_format === format)).toBe(true);
    expect(entries.some(entry => entry.response.status === 400 && !entry.executed)).toBe(true);
  });
  for (const entry of entries) it(`${entry.id} preserves both source files, every native scalar and every complete export`, async () => {
    expect(entry.path).toBe("/api/align-graph-workbench"); expect(entry.method).toBe("POST"); expect(entry.response.status).toBe(entry.expected_status);
    const raw = responseText(entry), captured = await runFromCapture(entry), value = captured.value;
    expect(value.mode).toBe(entry.mode); expect(value.input_shape).toBe(entry.input_shape); expect(value.result_shape).toBe(entry.result_shape); expect(value.executed).toBe(entry.executed);
    expect(value.input_context.format).toBe(entry.input_format); expect(value.model_context.format).toBe(entry.model_format);
    for (const [context, file] of [[value.input_context, entry.input], [value.model_context, entry.model]] as const) {expect(context.dataset).toBe(file.filename); expect(context.original_input_bytes).toBe(file.bytes);}
    expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(entry.fields);
    const options = (value as unknown as Record<string, unknown>).options_context; expect(object(options)).toBe(true);
    const numeric = numericPaths(raw); for (const [pointer, literal] of numeric) {const [holder, key] = holderAt(value, pointer); expect(String(nativeCell(holder, key)), pointer).toBe(literal);}
    expect(numericPaths(nativeJson(value))).toEqual(numeric);
    const proof: ExportProof = {id: entry.id, verified_numeric_literals: numeric.size, tables: captured.tables.length, rows: 0, csv_cells: 0, selected_records: 0, table_details: [], artifacts: {}};
    saveArtifact(entry.id, "native.json", captured.raw, proof); expect(digest(workflowInputNativeBytes(captured.raw))).toBe(entry.response.sha256);
    const reportText = graphAlignmentReport(captured), report = parseNativeJson(reportText) as {native_result: unknown; inputs: {input: {name: string; size: number; sha256: string}; model: {name: string; size: number; sha256: string}}; request: {fields: Array<[string, string]>}};
    expect(report.inputs.input).toMatchObject({name: entry.input.filename, size: entry.input.bytes, sha256: entry.input.sha256}); expect(report.inputs.model).toMatchObject({name: entry.model.filename, size: entry.model.bytes, sha256: entry.model.sha256}); expect(report.request.fields).toEqual(entry.fields);
    expect(nativeJson(report.native_result)).toBe(nativeJson(value));
    const reportNumbers = new Map([...numericPaths(reportText)].filter(([path]) => path.startsWith("/native_result/")).map(([path, token]) => [path.slice("/native_result".length), token])); expect(reportNumbers).toEqual(numeric);
    saveArtifact(entry.id, "report.json", reportText, proof);
    const requestText = nativeJson(nativeResponseRequest(value), 2); expect(JSON.parse(requestText).fields).toEqual(entry.fields); saveArtifact(entry.id, "request.json", requestText, proof);
    saveArtifact(entry.id, "request.bin", capturedBytes(entry.request_body), proof);
    for (const [name, file, record] of [["original-input.bin", captured.inputs.input, entry.input], ["original-model.bin", captured.inputs.model, entry.model]] as const) {const bytes = new Uint8Array(await file.arrayBuffer()); expect(Buffer.from(bytes).equals(capturedBytes(record))).toBe(true); saveArtifact(entry.id, name, bytes, proof);}
    for (const [kind, key, text] of [["model", "model", value.model_export], ["projection", "projected_xes", value.projected_xes]] as const) {
      if (text === null) {expect(() => graphAlignmentArtifactFile(captured, kind)).toThrow(); expect(entry.exports[key]).toBeUndefined(); continue;}
      const file = graphAlignmentArtifactFile(captured, kind), bytes = new Uint8Array(await file.arrayBuffer()), observed = entry.exports[key]; expect(observed).toBeDefined(); if (!observed) throw new Error("Missing captured native model/projection export");
      expect(nativeBytesText(bytes)).toBe(text); expect(Buffer.from(bytes).equals(capturedBytes(observed))).toBe(true); expect(digest(bytes)).toBe(observed.sha256); expect(file.name.endsWith("." + observed.format)).toBe(true); saveArtifact(entry.id, key + "." + observed.format, bytes, proof);
    }
    for (const [tableIndex, table] of captured.tables.entries()) {
      const csv = diagnosticCsv(table), decoded = decodeCsv(csv), delivered = new Uint8Array(await new Blob([workflowInputNativeBytes(csv)], {type: "text/csv;charset=utf-8"}).arrayBuffer());
      expect(decodeCsv(nativeBytesText(delivered))).toEqual(decoded); expect(decoded[0], table.id).toEqual(table.columns); expect(decoded.slice(1), table.id).toEqual(table.rows.map(row => row.map(expectedCell))); expect(decoded.slice(1).every(row => row.length === table.columns.length)).toBe(true);
      proof.rows += table.rows.length; proof.csv_cells += decoded.reduce((sum, row) => sum + row.length, 0); proof.selected_records += table.records?.length ?? 0;
      const csvName = `tables/${String(tableIndex).padStart(4, "0")}.csv`, recordsName = table.records ? `records/${String(tableIndex).padStart(4, "0")}.json` : null;
      proof.table_details.push({id: table.id, title: table.title, columns: table.columns, rows: table.rows.length, records: table.records?.length ?? 0, csv: csvName, records_json: recordsName}); saveArtifact(entry.id, csvName, csv, proof);
      if (table.records && recordsName) {const text = nativeJson(table.records, 2); expect(nativeJson(parseNativeJson(text))).toBe(nativeJson(table.records)); saveArtifact(entry.id, recordsName, text, proof);}
    }
    const allCsv = graphAlignmentAllTablesCsv(captured.tables), decodedAll = decodeCsv(allCsv), expected: string[][] = [];
    expect(decodeCsv(nativeBytesText(new Uint8Array(await new Blob([workflowInputNativeBytes(allCsv)]).arrayBuffer())))).toEqual(decodedAll);
    for (const table of captured.tables) {
      if (!table.rows.length) table.columns.forEach((column, column_index) => expected.push([table.id, table.title, "", String(column_index), column, ""]));
      table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => expected.push([table.id, table.title, String(row_index), String(column_index), column, expectedCell(row[column_index])])));
    }
    expect(decodedAll[0]).toEqual(["table_id", "table_title", "row_index", "column_index", "column", "value"]); expect(decodedAll.slice(1)).toEqual(expected); saveArtifact(entry.id, "all-tables.csv", allCsv, proof); proofs.push(proof);
  });
  it("retains every original typed cell, full nested move and selected record beyond UI paging", () => {
    let checkedCells = 0, checkedMoves = 0, pagedTables = 0;
    for (const entry of entries) {
      const value = actual(entry), tables = graphAlignmentTables(value, entry.fields), table = value.input_table;
      if (table) {
        const rows = tables.find(item => item.id === "/input_table/rows")!; expect(rows.records).toBe(table.rows);
        table.rows.forEach((row, i) => {expect(nativeJson(rows.records![i])).toBe(nativeJson(row)); checkedCells += row.length;});
        const dtypes = tables.find(item => item.id === "/input_table/dtypes"); if (table.dtypes) expect(dtypes?.records).toBe(table.dtypes);
      }
      const native = value.result, traces = native === null ? [] : "alignments" in native ? native.alignments : "traces" in native ? native.traces : [native];
      const moves = tables.find(item => item.id === "complete_alignment_moves")!; expect(moves.rows.length).toBe(traces.reduce((sum, trace) => sum + trace.alignment.length, 0));
      for (const record of moves.records ?? []) {const row = record as {trace_index: number; move_index: number; move: unknown}; expect(row.move).toBe(traces[row.trace_index].alignment[row.move_index]); ++checkedMoves;}
      for (const candidate of tables) if (candidate.rows.length > 25) {expect(diagnosticPage(candidate.rows, 1)).toHaveLength(Math.min(25, candidate.rows.length - 25)); expect(nativeJson(candidate.records?.at(-1))).toBeTruthy(); ++pagedTables;}
    }
    expect(checkedCells).toBeGreaterThan(0); expect(checkedMoves).toBeGreaterThan(0); expect(pagedTables).toBeGreaterThan(0);
  });
  it("retains raw undefined scores, all native options, explicit map flags and incomplete/error outcomes", () => {
    let undefinedScores = 0, incomplete = 0, nativeErrors = 0; const explicitEmptyMaps = new Set<string>();
    for (const entry of entries) {
      const value = actual(entry), options = (value as unknown as Record<string, unknown>).options_context; if (!object(options)) throw new Error("Missing actual native options");
      expect(options.variant).toBe(entry.mode === "DFG_CLASSIC" ? "CLASSIC" : "RECOMPOS_MAXIMAL"); expect(options.variant_index).toBe(0);
      for (const [key, text] of entry.fields) if (key.endsWith("_cost_json") || key.endsWith("_costs_json")) {
        const nativeKey = key.startsWith("dfg_") ? key.slice(4, -5) : key.slice(0, -5), submitted = parseNativeJson(text);
        expect(options[nativeKey]).toEqual(submitted); if (key.startsWith("dfg_")) {expect(options[nativeKey + "_supplied"]).toBe(true); if (Object.keys(submitted as object).length === 0) explicitEmptyMaps.add(nativeKey);}
      }
      for (const name of ["initial", "final"] as const) {
        const field = entry.fields.find(([key]) => key === name + "_marking_json"); if (!field) continue;
        const configured = value.model_context.markings!; expect(nativeJson(configured[`options_${name}_marking`])).toBe(nativeJson(parseNativeJson(field[1]))); expect(nativeJson(options[name + "_marking"])).toBe(nativeJson(configured[`options_${name}_marking`]));
      }
      if (!value.executed) {expect(value.result).toBeNull(); expect(value.error).not.toBeNull(); expect(graphAlignmentSucceeded(value)).toBe(false); ++nativeErrors; continue;}
      if (!graphAlignmentSucceeded(value)) {expect(graphAlignmentSummary(value)).toMatch(/not computed|Incomplete/); ++incomplete;}
      if (value.mode === "DFG_CLASSIC" && value.result_shape === "trace") {const trace = value.result! as {fitness: unknown; best_worst_cost: unknown}; expect(trace.fitness).toBe("NaN"); expect(trace.best_worst_cost).toBe("NaN"); ++undefinedScores;}
    }
    expect(undefinedScores).toBeGreaterThan(0); expect(incomplete).toBeGreaterThan(0); expect(nativeErrors).toBeGreaterThan(0); expect([...explicitEmptyMaps].sort()).toEqual(["internal_log_move_cost", "log_move_cost", "model_move_cost", "sync_cost"]);
  });
  it("binds actual files, selectors and trace selection to the independent route and rejects late requests", async () => {
    const entry = entries.find(entry => entry.executed && entry.input_shape === "trace")!, input = originalFile(entry.input), model = originalFile(entry.model), form = formFromCapture(entry), raw = responseText(entry);
    const fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))); vi.stubGlobal("fetch", fetcher);
    const run = await runGraphAlignmentWorkbench(input, model, form); expect(run.inputs.input_sha256).toBe(entry.input.sha256); expect(run.inputs.model_sha256).toBe(entry.model.sha256); expect(run.raw).toBe(raw);
    expect(fetcher.mock.calls[0][0]).toBe("/api/align-graph-workbench"); expect(fetcher.mock.calls[0][1].body.get("source_id")).toBeNull(); expect(fetcher.mock.calls[0][1].body.get("include_source")).toBeNull();
    expect(Object.fromEntries(graphAlignmentFields(form, input.name)).trace_index).toBe(form.traceIndex);
    await expect(runGraphAlignmentWorkbench(new File([await input.arrayBuffer()], "other-input.csv"), model, form)).rejects.toThrow(/does not match/);
    await expect(runGraphAlignmentWorkbench(input, new File([await model.arrayBuffer()], "other-model"), form)).rejects.toThrow(/does not match/);
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(raw));}));
    await expect(runGraphAlignmentWorkbench(input, model, form, controller.signal)).rejects.toThrow(/abort/i);
    const gate = new GraphAlignmentRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false);
  });
  it("publishes a complete independently decoded export proof when requested by the standalone recipe", () => {
    expect(proofs.length).toBe(entries.length); if (!exportDirectory) return;
    const path = pathInWorkspace(resolve(exportDirectory, "export-integrity.json")); mkdirSync(dirname(path), {recursive: true});
    writeFileSync(path, JSON.stringify({complete: true, manifest: manifestPath, manifest_sha256: digest(readFileSync(manifestPath)), entries: proofs,
      verified_numeric_literals: proofs.reduce((sum, proof) => sum + proof.verified_numeric_literals, 0), csv_cells: proofs.reduce((sum, proof) => sum + proof.csv_cells, 0), selected_records: proofs.reduce((sum, proof) => sum + proof.selected_records, 0)}, null, 2) + "\n", "utf8");
  });
});

describe("model reader refusals", () => {
  for (const entry of readerRefusals) it(`${entry.id} is refused by the strict model reader`, () => {
    expect(entry.response.status).toBe(400);
    expect(JSON.parse(responseText(entry))).toEqual({error: entry.error});
  });
});
