import {afterEach, describe, expect, it, vi} from "vitest";
import {
  TREE_ALIGNMENT_CONTROLS, TREE_ALIGNMENT_VARIANTS, ProcessTreeAlignmentRequestGate, defaultProcessTreeAlignmentForm,
  isProcessTreeAlignmentWorkbench, processTreeAlignmentAllTablesCsv, processTreeAlignmentArtifactFile,
  processTreeAlignmentFields, processTreeAlignmentMilliseconds, processTreeAlignmentReport, processTreeAlignmentSucceeded,
  processTreeAlignmentSummary, processTreeAlignmentTableFileName, processTreeAlignmentTables, processTreeAlignmentTreeGraph,
  runProcessTreeAlignmentWorkbench, type ProcessTreeAlignmentWorkbench, type ProcessTreeAlignmentForm, type ProcessTreeAlignmentRun,
} from "../src/process-tree-alignment-workbench";
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
  id: string; variant: ProcessTreeAlignmentForm["variant"];
  input_shape: ProcessTreeAlignmentForm["inputShape"]; input_format: ProcessTreeAlignmentForm["inputFormat"]; model_format: ProcessTreeAlignmentForm["modelFormat"]; result_shape: "log"; executed: boolean;
  fields: Array<[string, string]>; path: string; method: string; expected_status: number;
  input: CapturedFile & {filename: string}; model: CapturedFile & {filename: string}; request_body: CapturedFile; response: CapturedFile & {status: number};
  exports: Partial<Record<"pnml" | "bpmn" | "ptml" | "source_xes", CapturedFile & {format: string}>>;
}
interface CapturedManifest {complete: boolean; feature: number; entries: CapturedEntry[]; [key: string]: unknown;}
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), ".."), root = resolve(frontend, "../..");
const captureDirectory = environment.MINERA_PROCESS_TREE_ALIGNMENT_CAPTURE_DIR;
const manifestPath = captureDirectory ? resolve(captureDirectory, "request-manifest.json") : resolve(frontend, "tests/fixtures/process-tree-alignment-workbench-request-manifest.json");
if (!existsSync(manifestPath)) throw new Error("Complete own-server process-tree alignment captures are required. Supply MINERA_PROCESS_TREE_ALIGNMENT_CAPTURE_DIR or publish the portable fixture manifest.");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as CapturedManifest;
if (manifest.complete !== true || manifest.feature !== 54 || !Array.isArray(manifest.entries) || !manifest.entries.length) throw new Error("The process-tree alignment request manifest is not a complete real capture.");
const entries = manifest.entries, exportDirectory = environment.MINERA_PROCESS_TREE_ALIGNMENT_EXPORT_DIR;

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
function actual(entry: CapturedEntry): ProcessTreeAlignmentWorkbench {
  const value = parseNativeJson(responseText(entry)); if (!isProcessTreeAlignmentWorkbench(value)) throw new Error(`Invalid actual process-tree alignment envelope ${entry.id}`); return value;
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
function formFromCapture(entry: CapturedEntry): ProcessTreeAlignmentForm {
  const form = defaultProcessTreeAlignmentForm(); form.variant = entry.variant; form.inputShape = entry.input_shape; form.inputFormat = entry.input_format; form.modelFormat = entry.model_format;
  for (const [key, value] of entry.fields) {
    if (key === "alignment_selectors_json") for (const [name, selector] of Object.entries(JSON.parse(value) as Record<string, string>)) {form.fields[name] = selector; if (selector === "") form.explicitEmptySelectors[name] = true;}
    else if (key === "discovery_tree_variant") form.conversion = value as ProcessTreeAlignmentForm["conversion"];
    else if (key === "trace_index") form.traceIndex = value;
    else if (["case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter"].includes(key) || key.startsWith("xes_")) form.importFields[key] = value;
    else if (!["pt_alignment_variant", "input_shape", "input_format", "model_format"].includes(key)) form.fields[key] = value;
  }
  return form;
}
async function runFromCapture(entry: CapturedEntry): Promise<ProcessTreeAlignmentRun> {
  const raw = responseText(entry), value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields: entry.fields});
  if (!isProcessTreeAlignmentWorkbench(value)) throw new Error(`Invalid captured native process-tree alignment ${entry.id}`);
  return {value, raw, fields: entry.fields, tables: processTreeAlignmentTables(value, entry.fields), http_status: entry.response.status,
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

describe("actual own-server independent original-model process-tree alignment fixtures", () => {
  it("requires all four algorithms over CSV/XES/table grouped logs, selected traces and literal words", () => {
    for (const variant of TREE_ALIGNMENT_VARIANTS) {
      for (const shape of ["log", "trace", "words"] as const) expect(entries.some(entry => entry.variant === variant && entry.input_shape === shape && entry.executed)).toBe(true);
      for (const shape of ["log", "trace"] as const) for (const format of ["auto", "table"] as const) expect(entries.some(entry => entry.variant === variant && entry.input_shape === shape && entry.input_format === format && entry.executed)).toBe(true);
      for (const extension of [/\.csv$/i, /\.xes(?:\.gz)?$/i]) expect(entries.some(entry => entry.variant === variant && extension.test(entry.input.filename) && entry.executed)).toBe(true);
    }
    for (const model of ["ptml", "tree_json"]) expect(entries.some(entry => actual(entry).model_context.detected_format === model)).toBe(true);
    expect(entries.some(entry => entry.response.status === 400 && !entry.executed)).toBe(true);
    expect(entries.some(entry => entry.executed && !processTreeAlignmentSucceeded(actual(entry)))).toBe(true);
  });
  for (const entry of entries) it(`${entry.id} preserves every original/native field, numeric spelling and complete export byte`, async () => {
    expect(entry.path).toBe("/api/align-process-tree-workbench"); expect(entry.method).toBe("POST"); expect(entry.response.status).toBe(entry.expected_status);
    const raw = responseText(entry), captured = await runFromCapture(entry), value = captured.value;
    expect(value.variant).toBe(entry.variant); expect(value.input_shape).toBe(entry.input_shape); expect(value.result_shape).toBe("log"); expect(value.result_shape).toBe(entry.result_shape); expect(value.executed).toBe(entry.executed);
    expect(value.input_context.format).toBe(entry.input_format); expect(value.model_context.format).toBe(entry.model_format);
    for (const [context, file] of [[value.input_context, entry.input], [value.model_context, entry.model]] as const) {expect(context.dataset).toBe(file.filename); expect(context.original_input_bytes).toBe(file.bytes);}
    expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(entry.fields); expect(value.options_context.variant).toBe(entry.variant);
    const numeric = numericPaths(raw); for (const [pointer, literal] of numeric) {const [holder, key] = holderAt(value, pointer); expect(String(nativeCell(holder, key)), pointer).toBe(literal);} expect(numericPaths(nativeJson(value))).toEqual(numeric);
    const proof: ExportProof = {id: entry.id, verified_numeric_literals: numeric.size, tables: captured.tables.length, rows: 0, csv_cells: 0, selected_records: 0, table_details: [], artifacts: {}};
    saveArtifact(entry.id, "native.json", captured.raw, proof); expect(digest(workflowInputNativeBytes(captured.raw))).toBe(entry.response.sha256);
    const reportText = processTreeAlignmentReport(captured), report = parseNativeJson(reportText) as {native_result: unknown; inputs: {input: {name: string; size: number; sha256: string}; model: {name: string; size: number; sha256: string}}; request: {fields: Array<[string, string]>}};
    expect(report.inputs.input).toMatchObject({name: entry.input.filename, size: entry.input.bytes, sha256: entry.input.sha256}); expect(report.inputs.model).toMatchObject({name: entry.model.filename, size: entry.model.bytes, sha256: entry.model.sha256}); expect(report.request.fields).toEqual(entry.fields); expect(nativeJson(report.native_result)).toBe(nativeJson(value));
    expect(new Map([...numericPaths(reportText)].filter(([path]) => path.startsWith("/native_result/")).map(([path, token]) => [path.slice("/native_result".length), token]))).toEqual(numeric); saveArtifact(entry.id, "report.json", reportText, proof);
    const requestText = nativeJson(nativeResponseRequest(value), 2); expect(JSON.parse(requestText).fields).toEqual(entry.fields); saveArtifact(entry.id, "request.json", requestText, proof); saveArtifact(entry.id, "request.bin", capturedBytes(entry.request_body), proof);
    for (const [name, file, record] of [["original-input.bin", captured.inputs.input, entry.input], ["original-model.bin", captured.inputs.model, entry.model]] as const) {const bytes = new Uint8Array(await file.arrayBuffer()); expect(Buffer.from(bytes).equals(capturedBytes(record))).toBe(true); saveArtifact(entry.id, name, bytes, proof);}
    for (const format of ["pnml", "bpmn", "ptml", "source_xes"] as const) {
      if (value[format] === null) {expect(() => processTreeAlignmentArtifactFile(captured, format)).toThrow(); expect(entry.exports[format]).toBeUndefined(); continue;}
      const file = processTreeAlignmentArtifactFile(captured, format), bytes = new Uint8Array(await file.arrayBuffer()), observed = entry.exports[format]; expect(observed).toBeDefined(); if (!observed) throw new Error("Missing captured native process-tree artifact");
      expect(nativeBytesText(bytes)).toBe(value[format]); expect(Buffer.from(bytes).equals(capturedBytes(observed))).toBe(true); expect(digest(bytes)).toBe(observed.sha256); expect(file.name.endsWith("." + observed.format)).toBe(true); saveArtifact(entry.id, format + "." + observed.format, bytes, proof);
    }
    for (const [tableIndex, table] of captured.tables.entries()) {
      const csv = diagnosticCsv(table), decoded = decodeCsv(csv), bytes = new Uint8Array(await new Blob([workflowInputNativeBytes(csv)], {type: "text/csv;charset=utf-8"}).arrayBuffer());
      expect(decodeCsv(nativeBytesText(bytes))).toEqual(decoded); expect(decoded[0], table.id).toEqual(table.columns); expect(decoded.slice(1), table.id).toEqual(table.rows.map(row => row.map(expectedCell))); expect(decoded.slice(1).every(row => row.length === table.columns.length)).toBe(true);
      const fileName = processTreeAlignmentTableFileName(table.id); expect(fileName).toMatch(/^[A-Za-z0-9_.%\-]+$/); expect(fileName.replace(/%([a-f0-9]{4})/g, (_, code: string) => String.fromCharCode(Number.parseInt(code, 16)))).toBe(table.id);
      proof.rows += table.rows.length; proof.csv_cells += decoded.reduce((sum, row) => sum + row.length, 0); proof.selected_records += table.records?.length ?? 0;
      const csvName = `tables/${String(tableIndex).padStart(4, "0")}.csv`, recordsName = table.records ? `records/${String(tableIndex).padStart(4, "0")}.json` : null;
      proof.table_details.push({id: table.id, title: table.title, columns: table.columns, rows: table.rows.length, records: table.records?.length ?? 0, csv: csvName, records_json: recordsName}); saveArtifact(entry.id, csvName, csv, proof);
      if (table.records && recordsName) {const text = nativeJson(table.records, 2); expect(nativeJson(parseNativeJson(text))).toBe(nativeJson(table.records)); saveArtifact(entry.id, recordsName, text, proof);}
    }
    const allCsv = processTreeAlignmentAllTablesCsv(captured.tables), decodedAll = decodeCsv(allCsv), expected: string[][] = [];
    expect(decodeCsv(nativeBytesText(new Uint8Array(await new Blob([workflowInputNativeBytes(allCsv)]).arrayBuffer())))).toEqual(decodedAll);
    for (const table of captured.tables) {
      if (!table.rows.length) table.columns.forEach((column, column_index) => expected.push([table.id, table.title, "", String(column_index), column, ""]));
      table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => expected.push([table.id, table.title, String(row_index), String(column_index), column, expectedCell(row[column_index])])));
    }
    expect(decodedAll[0]).toEqual(["table_id", "table_title", "row_index", "column_index", "column", "value"]); expect(decodedAll.slice(1)).toEqual(expected); saveArtifact(entry.id, "all-tables.csv", allCsv, proof); proofs.push(proof);
  }, 30000);
  it("retains every original tree identity, checked/source log and before/after typed cell beyond paging", () => {
    let cellsChecked = 0, nodesChecked = 0, movesChecked = 0, pagedRows = 0, pagedColumns = 0;
    for (const entry of entries) {
      const value = actual(entry), tables = processTreeAlignmentTables(value, entry.fields), tree = value.model_context.native_tree;
      if (value.input_before && value.input_after) expect(nativeJson(value.input_after)).toBe(nativeJson(value.input_before));
      for (const name of ["input_before", "input_after"] as const) {
        const original = value[name]; if (!original) continue;
        const rows = tables.find(table => table.id === `/${name}/rows`)!, cells = tables.find(table => table.id === `${name}_cells`)!; expect(rows.records).toBe(original.rows); expect(cells.records).toHaveLength(original.rows.reduce((sum, row) => sum + row.length, 0));
        original.rows.forEach((row, i) => {expect(nativeJson(rows.records![i])).toBe(nativeJson(row)); cellsChecked += row.length;});
        for (const record of cells.records ?? []) {const cell = record as {row_index: number; column_index: number; cell: unknown}; expect(cell.cell).toBe(original.rows[cell.row_index][cell.column_index]);}
      }
      const nodes = tables.find(table => table.id === "complete_tree_nodes")!, graph = processTreeAlignmentTreeGraph(tree); expect(graph.nodes).toHaveLength(nodes.rows.length);
      expect(graph.nodes.map(node => node.id)).toEqual(nodes.rows.map(row => row[0])); expect(graph.edges).toHaveLength(nodes.rows.length - 1);
      for (const record of nodes.records ?? []) {const node = record as {path: string; node: unknown}; expect(nativeJson(node.node)).toBe(nativeJson(nodes.rows.find(row => row[0] === node.path)![7])); ++nodesChecked;}
      if (value.model_context.detected_format === "tree_json") expect(nativeJson(tree)).toBe(nativeJson(parseNativeJson(capturedBytes(entry.model).toString("utf8"))));
      const moves = tables.find(table => table.id === "complete_alignment_moves")!, traces = value.result?.traces ?? [];
      expect(moves.rows).toHaveLength(traces.reduce((sum, trace) => sum + trace.moves.length, 0));
      for (const record of moves.records ?? []) {const move = record as {trace_index: number; move_index: number; move: unknown}; expect(move.move).toBe(traces[move.trace_index].moves[move.move_index]); ++movesChecked;}
      for (const key of ["input_log", "checked_log"] as const) if (value[key]) {const summary = tables.find(table => table.id === `/${key}`)!; expect(summary.records!.some(record => nativeJson(record).includes('"traces"'))).toBe(true);}
      for (const table of tables) {
        if (table.rows.length > 25) {expect(diagnosticPage(table.rows, 1)).toHaveLength(Math.min(25, table.rows.length - 25)); expect(nativeJson(table.records?.at(-1))).toBeTruthy(); ++pagedRows;}
        if (table.columns.length > 8) {expect(diagnosticPage(table.columns, 1, 8)).toHaveLength(Math.min(8, table.columns.length - 8)); ++pagedColumns;}
      }
    }
    expect(cellsChecked).toBeGreaterThan(0); expect(nodesChecked).toBeGreaterThan(0); expect(movesChecked).toBeGreaterThan(0); expect(pagedRows).toBeGreaterThan(0); expect(pagedColumns).toBeGreaterThan(0);
  });
  it("keeps configured native options, milliseconds/seconds, exact selectors, raw enums and incomplete/error state", () => {
    let nativeErrors = 0, incomplete = 0, emptySides = 0, hexadecimalTimes = 0, wideValues = 0, negativeZero = 0;
    for (const entry of entries) {
      const value = actual(entry), options = value.options_context; expect(options.variant_index).toBe(TREE_ALIGNMENT_VARIANTS.indexOf(entry.variant));
      for (const [key, text] of entry.fields) {
        if (key === "alignment_selectors_json") for (const [name, literal] of Object.entries(JSON.parse(text) as Record<string, string>)) expect(value.input_context[name === "alignment_case_id_attribute" ? "case_id_attribute" : "activity_attribute"]).toBe(literal);
        const control = TREE_ALIGNMENT_CONTROLS.find(control => control.key === key); if (!control || control.kind === "selector") continue;
        const nativeKey = key.replace(/^pt_alignment_/, "").replace(/_ms$/, "_seconds"), actualValue = options[nativeKey as keyof typeof options];
        if (control.kind === "boolean") expect(actualValue).toBe(text === "true" || text === "1");
        else if (control.kind === "number") {expect(Object.is(actualValue, processTreeAlignmentMilliseconds(text)! / 1000)).toBe(true); if (/0x/i.test(text)) ++hexadecimalTimes; if (Object.is(actualValue, -0)) ++negativeZero;}
        else {expect(String(nativeCell(options, nativeKey))).toBe(BigInt(text).toString()); if (BigInt(text) > BigInt(Number.MAX_SAFE_INTEGER)) ++wideValues;}
      }
      if (!value.executed) {expect(value.result).toBeNull(); expect(value.error?.type).toBe("native_error"); expect(processTreeAlignmentSummary(value)).toContain("failed"); ++nativeErrors; continue;}
      expect(value.error).toBeNull(); const result = value.result!; expect(result.variant).toBe(entry.variant); expect(result.variant_index).toBe(options.variant_index);
      if (!processTreeAlignmentSucceeded(value)) {expect(processTreeAlignmentSummary(value)).toContain("Incomplete"); ++incomplete;}
      if (entry.variant === "MILP") expect(result.diagnostics.lp_solved).toBe(0);
      for (const trace of result.traces) for (const move of trace.moves) {
        expect(move.type).toBe(["sync", "log", "model"][move.type_index] ?? "UNKNOWN");
        if (move.type === "log") {expect(move.model_activity).toBe(""); ++emptySides;}
        if (move.type === "model") {expect(move.log_activity).toBe(""); ++emptySides;}
      }
    }
    expect(nativeErrors).toBeGreaterThan(0); expect(incomplete).toBeGreaterThan(0); expect(emptySides).toBeGreaterThan(0); expect(hexadecimalTimes).toBeGreaterThan(0); expect(wideValues).toBeGreaterThan(0); expect(negativeZero).toBeGreaterThan(0);
  });
  it("binds actual original files/options to the source-free route and blocks mismatched or late responses", async () => {
    const entry = entries.find(entry => entry.executed && entry.input_shape === "trace")!, input = originalFile(entry.input), model = originalFile(entry.model), form = formFromCapture(entry), raw = responseText(entry);
    const fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))); vi.stubGlobal("fetch", fetcher);
    const run = await runProcessTreeAlignmentWorkbench(input, model, form); expect(run.inputs.input_sha256).toBe(entry.input.sha256); expect(run.inputs.model_sha256).toBe(entry.model.sha256); expect(run.raw).toBe(raw);
    expect(fetcher.mock.calls[0][0]).toBe("/api/align-process-tree-workbench"); expect(fetcher.mock.calls[0][1].body.get("input_file")).toBe(input); expect(fetcher.mock.calls[0][1].body.get("model_file")).toBe(model); expect(fetcher.mock.calls[0][1].body.get("source_id")).toBeNull(); expect(fetcher.mock.calls[0][1].body.get("include_source")).toBeNull();
    expect(Object.fromEntries(processTreeAlignmentFields(form, input.name)).pt_alignment_variant).toBe(entry.variant);
    await expect(runProcessTreeAlignmentWorkbench(new File([await input.arrayBuffer()], "other-input.csv"), model, form)).rejects.toThrow(/does not match/);
    await expect(runProcessTreeAlignmentWorkbench(input, new File([await model.arrayBuffer()], "other-model.ptml"), form)).rejects.toThrow(/does not match/);
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(raw));})); await expect(runProcessTreeAlignmentWorkbench(input, model, form, controller.signal)).rejects.toThrow(/abort/i);
    const gate = new ProcessTreeAlignmentRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false); expect(gate.accepts(gate.issue(), controller.signal)).toBe(false);
  });
  it("publishes the complete independently decoded export proof when requested by the frozen recipe", () => {
    expect(proofs.length).toBe(entries.length); if (!exportDirectory) return;
    const path = pathInWorkspace(resolve(exportDirectory, "export-integrity.json")); mkdirSync(dirname(path), {recursive: true});
    writeFileSync(path, JSON.stringify({complete: true, manifest: manifestPath, manifest_sha256: digest(readFileSync(manifestPath)), entries: proofs,
      verified_numeric_literals: proofs.reduce((sum, proof) => sum + proof.verified_numeric_literals, 0), csv_cells: proofs.reduce((sum, proof) => sum + proof.csv_cells, 0), selected_records: proofs.reduce((sum, proof) => sum + proof.selected_records, 0)}, null, 2) + "\n", "utf8");
  });
});
