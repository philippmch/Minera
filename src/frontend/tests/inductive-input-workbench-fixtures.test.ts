import {afterEach, describe, expect, it, vi} from "vitest";
import {
  InductiveInputRequestGate, defaultInductiveInputForm, inductiveInputAllTablesCsv, inductiveInputArtifactFile,
  inductiveInputFields, inductiveInputReport, inductiveInputSummary, inductiveInputTableFileName, inductiveInputTables,
  inductiveInputTreeGraph, isInductiveInputDiscovery, runInductiveInput,
  type InductiveInputDiscovery, type InductiveInputForm, type InductiveInputRun,
} from "../src/inductive-input-workbench";
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
  id: string; variant: InductiveInputForm["variant"]; canonical_variant: InductiveInputDiscovery["variant"];
  input_shape: InductiveInputForm["shape"]; input_format: InductiveInputForm["format"]; computed: boolean;
  fields: Array<[string, string]>; path: string; method: string; expected_status: number;
  input: CapturedFile & {filename: string}; request_body: CapturedFile; response: CapturedFile & {status: number};
  exports: Partial<Record<"pnml" | "bpmn" | "ptml" | "source_xes", CapturedFile & {format: string}>>;
}
interface CapturedManifest {complete: boolean; feature: number; entries: CapturedEntry[]; [key: string]: unknown;}
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), ".."), root = resolve(frontend, "../..");
const captureDirectory = environment.MINERA_INDUCTIVE_INPUT_CAPTURE_DIR;
const manifestPath = captureDirectory ? resolve(captureDirectory, "request-manifest.json") : resolve(frontend, "tests/fixtures/inductive-input-workbench-request-manifest.json");
if (!existsSync(manifestPath)) throw new Error("Complete own-server Inductive input captures are required. Supply MINERA_INDUCTIVE_INPUT_CAPTURE_DIR or publish the portable fixture manifest.");
const manifest = JSON.parse(readFileSync(manifestPath, "utf8")) as CapturedManifest;
if (manifest.complete !== true || manifest.feature !== 53 || !Array.isArray(manifest.entries) || !manifest.entries.length) throw new Error("The Inductive input request manifest is not a complete real capture.");
const entries = manifest.entries, exportDirectory = environment.MINERA_INDUCTIVE_INPUT_EXPORT_DIR;

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
function actual(entry: CapturedEntry): InductiveInputDiscovery {
  const value = parseNativeJson(responseText(entry)); if (!isInductiveInputDiscovery(value)) throw new Error(`Invalid actual Inductive input envelope ${entry.id}`); return value;
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
function formFromCapture(entry: CapturedEntry): InductiveInputForm {
  const form = defaultInductiveInputForm(); form.variant = entry.variant; form.shape = entry.input_shape; form.format = entry.input_format;
  for (const [key, value] of entry.fields) {
    if (key === "discovery_selectors_json") for (const [name, selector] of Object.entries(JSON.parse(value) as Record<string, string>)) {form.fields[name] = selector; if (selector === "") form.explicitEmptySelectors[name] = true;}
    else if (key === "discovery_tree_variant") form.conversion = value as InductiveInputForm["conversion"];
    else if (["case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter"].includes(key) || key.startsWith("xes_")) form.importFields[key] = value;
    else if (!["inductive_variant", "input_shape", "input_format"].includes(key)) form.fields[key] = value;
  }
  return form;
}
async function runFromCapture(entry: CapturedEntry): Promise<InductiveInputRun> {
  const raw = responseText(entry), value = await readNativeResponse(new Response(raw, {status: entry.response.status}), {path: entry.path, fields: entry.fields});
  if (!isInductiveInputDiscovery(value)) throw new Error(`Invalid captured native Inductive discovery ${entry.id}`);
  return {value, raw, fields: entry.fields, tables: inductiveInputTables(value, entry.fields), http_status: entry.response.status, input: originalFile(entry.input), input_sha256: entry.input.sha256};
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

describe("actual own-server independent Inductive input fixtures", () => {
  it("requires all three variants over original CSV/XES/table adapters and both compact input families", () => {
    for (const prefix of ["csv", "xes", "typed"]) for (const variant of ["log", "imf", "dfg"] as const) for (const shape of ["log", "stream", "dataframe", "lazyframe"] as const) expect(entries.some(entry => entry.id === `${prefix}-${variant}-${shape}` && entry.computed)).toBe(true);
    for (const variant of ["log", "imf", "dfg"] as const) for (const shape of ["dfg", "variants"] as const) expect(entries.some(entry => entry.variant === variant && entry.input_shape === shape && entry.computed)).toBe(true);
    expect(entries.some(entry => !entry.computed && entry.response.status === 400)).toBe(true);
  });
  for (const entry of entries) it(`${entry.id} preserves original input, complete native scalars and all actual export bytes`, async () => {
    expect(entry.path).toBe("/api/discover-inductive-input"); expect(entry.method).toBe("POST"); expect(entry.response.status).toBe(entry.expected_status);
    const raw = responseText(entry), captured = await runFromCapture(entry), value = captured.value;
    expect(value.variant).toBe(entry.canonical_variant); expect(value.computed).toBe(entry.computed); expect(value.input_context.adapter).toBe(entry.input_shape); expect(value.input_context.format).toBe(entry.input_format);
    expect(value.input_context.dataset).toBe(entry.input.filename); expect(value.input_context.original_input_bytes).toBe(entry.input.bytes);
    expect(nativeResponseJson(value)).toBe(raw); expect(nativeResponseRequest(value)?.fields).toEqual(entry.fields);
    expect(value.options_context.variant).toBe(entry.canonical_variant); expect(value.discovery_context.requested_variant).toBe(entry.canonical_variant); expect(value.discovery_context.effective_variant).toBe(entry.input_shape === "dfg" ? "IMD" : entry.canonical_variant);
    const numeric = numericPaths(raw); for (const [pointer, literal] of numeric) {const [holder, key] = holderAt(value, pointer); expect(String(nativeCell(holder, key)), pointer).toBe(literal);} expect(numericPaths(nativeJson(value))).toEqual(numeric);
    const proof: ExportProof = {id: entry.id, verified_numeric_literals: numeric.size, tables: captured.tables.length, rows: 0, csv_cells: 0, selected_records: 0, table_details: [], artifacts: {}};
    saveArtifact(entry.id, "native.json", captured.raw, proof); expect(digest(workflowInputNativeBytes(captured.raw))).toBe(entry.response.sha256);
    const reportText = inductiveInputReport(captured), report = parseNativeJson(reportText) as {native_result: unknown; input: {name: string; size: number; sha256: string}; request: {fields: Array<[string, string]>}};
    expect(report.input).toMatchObject({name: entry.input.filename, size: entry.input.bytes, sha256: entry.input.sha256}); expect(report.request.fields).toEqual(entry.fields); expect(nativeJson(report.native_result)).toBe(nativeJson(value));
    expect(new Map([...numericPaths(reportText)].filter(([path]) => path.startsWith("/native_result/")).map(([path, token]) => [path.slice("/native_result".length), token]))).toEqual(numeric);
    saveArtifact(entry.id, "report.json", reportText, proof);
    const requestText = nativeJson(nativeResponseRequest(value), 2); expect(JSON.parse(requestText).fields).toEqual(entry.fields); saveArtifact(entry.id, "request.json", requestText, proof); saveArtifact(entry.id, "request.bin", capturedBytes(entry.request_body), proof);
    const original = new Uint8Array(await captured.input.arrayBuffer()); expect(Buffer.from(original).equals(capturedBytes(entry.input))).toBe(true); saveArtifact(entry.id, "original-input.bin", original, proof);
    for (const format of ["pnml", "bpmn", "ptml", "source_xes"] as const) {
      if (value[format] === null) {expect(() => inductiveInputArtifactFile(captured, format)).toThrow(); expect(entry.exports[format]).toBeUndefined(); continue;}
      const file = inductiveInputArtifactFile(captured, format), bytes = new Uint8Array(await file.arrayBuffer()), observed = entry.exports[format]; expect(observed).toBeDefined(); if (!observed) throw new Error("Missing actual Inductive export capture");
      expect(nativeBytesText(bytes)).toBe(value[format]); expect(Buffer.from(bytes).equals(capturedBytes(observed))).toBe(true); expect(digest(bytes)).toBe(observed.sha256); expect(file.name.endsWith("." + observed.format)).toBe(true); saveArtifact(entry.id, format + "." + observed.format, bytes, proof);
    }
    for (const [tableIndex, table] of captured.tables.entries()) {
      const csv = diagnosticCsv(table), decoded = decodeCsv(csv), bytes = new Uint8Array(await new Blob([workflowInputNativeBytes(csv)]).arrayBuffer());
      expect(decodeCsv(nativeBytesText(bytes))).toEqual(decoded); expect(decoded[0], table.id).toEqual(table.columns); expect(decoded.slice(1), table.id).toEqual(table.rows.map(row => row.map(expectedCell))); expect(decoded.slice(1).every(row => row.length === table.columns.length)).toBe(true);
      const fileName = inductiveInputTableFileName(table.id); expect(fileName).toMatch(/^[A-Za-z0-9_.%\-]+$/); expect(fileName.replace(/%([a-f0-9]{4})/g, (_, code: string) => String.fromCharCode(Number.parseInt(code, 16)))).toBe(table.id);
      proof.rows += table.rows.length; proof.csv_cells += decoded.reduce((sum, row) => sum + row.length, 0); proof.selected_records += table.records?.length ?? 0;
      const csvName = `tables/${String(tableIndex).padStart(4, "0")}.csv`, recordsName = table.records ? `records/${String(tableIndex).padStart(4, "0")}.json` : null;
      proof.table_details.push({id: table.id, title: table.title, columns: table.columns, rows: table.rows.length, records: table.records?.length ?? 0, csv: csvName, records_json: recordsName}); saveArtifact(entry.id, csvName, csv, proof);
      if (table.records && recordsName) {const text = nativeJson(table.records, 2); expect(nativeJson(parseNativeJson(text))).toBe(nativeJson(table.records)); saveArtifact(entry.id, recordsName, text, proof);}
    }
    const allCsv = inductiveInputAllTablesCsv(captured.tables), decodedAll = decodeCsv(allCsv), expected: string[][] = [];
    expect(decodeCsv(nativeBytesText(new Uint8Array(await new Blob([workflowInputNativeBytes(allCsv)]).arrayBuffer())))).toEqual(decodedAll);
    for (const table of captured.tables) {
      if (!table.rows.length) table.columns.forEach((column, column_index) => expected.push([table.id, table.title, "", String(column_index), column, ""]));
      table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => expected.push([table.id, table.title, String(row_index), String(column_index), column, expectedCell(row[column_index])])));
    }
    expect(decodedAll[0]).toEqual(["table_id", "table_title", "row_index", "column_index", "column", "value"]); expect(decodedAll.slice(1)).toEqual(expected); saveArtifact(entry.id, "all-tables.csv", allCsv, proof); proofs.push(proof);
  });
  it("retains every tree node and native graph observable alongside complete original typed cells", () => {
    let nodesChecked = 0, cellsChecked = 0, pagedTables = 0;
    for (const entry of entries) {
      const value = actual(entry), tables = inductiveInputTables(value, entry.fields);
      if (value.input_before && value.input_after) expect(nativeJson(value.input_after)).toBe(nativeJson(value.input_before));
      for (const name of ["input_before", "input_after"] as const) {
        const table = value[name]; if (!table) continue; const rows = tables.find(item => item.id === `/${name}/rows`)!; expect(rows.records).toBe(table.rows);
        table.rows.forEach((row, i) => {expect(nativeJson(rows.records![i])).toBe(nativeJson(row)); cellsChecked += row.length;});
      }
      if (value.native_tree) {
        const tree = value.native_tree, nodes = tables.find(table => table.id === "complete_tree_nodes")!, graph = inductiveInputTreeGraph(tree); expect(graph.nodes.length).toBe(nodes.rows.length);
        for (const record of nodes.records ?? []) {const row = record as {path: string; node: {op: string; activity: number}}; expect(graph.nodes.find(node => node.id === row.path)).toBeDefined(); expect(nativeJson(row.node)).toContain('"activity":'); ++nodesChecked;}
        expect(tables.find(table => table.id === "/native_tree/activities")?.records).toBe(tree.activities);
      }
      if (value.native_dfg) for (const key of ["activity_counts", "rework_case_counts", "variants"] as const) expect(tables.find(table => table.id === "/native_dfg/" + key)?.records).toBe(value.native_dfg[key]);
      for (const table of tables) if (table.rows.length > 25) {expect(diagnosticPage(table.rows, 1)).toHaveLength(Math.min(25, table.rows.length - 25)); ++pagedTables;}
    }
    expect(nodesChecked).toBeGreaterThan(0); expect(cellsChecked).toBeGreaterThan(0); expect(pagedTables).toBeGreaterThan(0);
  });
  it("keeps configured options, effective graph dispatch and compact source counts faithful", () => {
    let nativeErrors = 0, compact = 0, direct = 0;
    for (const entry of entries) {
      const value = actual(entry), fields = Object.fromEntries(entry.fields);
      expect(value.conversion_context.variant).toBe(fields.discovery_tree_variant ?? "standard");
      if (fields.noise_threshold !== undefined) expect(value.options_context.noise_threshold).toBe(Number(fields.noise_threshold));
      if (fields.discovery_selectors_json) for (const [key, expected] of Object.entries(JSON.parse(fields.discovery_selectors_json) as Record<string, string>)) {const nativeKey = key === "inductive_activity_key" ? "activity_key" : key === "inductive_timestamp_key" ? "timestamp_key" : "case_id_key"; expect(value.options_context[nativeKey]).toBe(expected);}
      for (const name of ["sort_events_by_timestamp", "disable_strict_sequence_cut", "disable_fallthroughs", "multiprocessing"] as const) if (fields["inductive_" + name] !== undefined) expect(value.options_context[name]).toBe(["true", "1"].includes(fields["inductive_" + name]));
      if (entry.input_shape === "variants" || entry.input_shape === "dfg") {expect(value.input_context.input_case_count).toBeNull(); expect(value.input_context.input_event_count).toBeNull(); expect(value.prepared_log).toBeNull(); expect(value.source_xes).toBeNull(); expect(value.result).toBeNull();}
      if (entry.input_shape === "variants") {expect(value.native_dfg).toBeNull(); expect(value.variant_log_input).not.toBeNull(); expect(value.input_context.weighted_variant_count).toBe(value.variant_log_input!.length); ++compact;}
      if (entry.input_shape === "dfg") {expect(value.discovery_context.direct_graph_semantics).toBe(true); expect(value.discovery_context.effective_variant).toBe("IMD"); expect(value.options_context.variant).toBe(entry.canonical_variant); if (value.native_dfg) {expect(value.native_dfg.activities).toEqual(value.dfg_input!.activities); expect(value.native_dfg.edges).toEqual(value.dfg_input!.edges); expect(value.native_dfg.activity_counts!.every(count => count === 0)).toBe(true); expect(value.native_dfg.rework_case_counts!.every(count => count === 0)).toBe(true); expect(value.native_dfg.variants).toEqual([]);} ++direct;}
      if (!value.computed) {expect(value.error).not.toBeNull(); expect(inductiveInputSummary(value)).toContain("failed"); ++nativeErrors;} else expect(value.native_tree).not.toBeNull();
    }
    expect(compact).toBeGreaterThan(0); expect(direct).toBeGreaterThan(0); expect(nativeErrors).toBeGreaterThan(0);
  });
  it("binds actual file/options to the independent route and rejects mismatched or cancelled responses", async () => {
    const entry = entries.find(entry => entry.id === "csv-log-log")!, file = originalFile(entry.input), form = formFromCapture(entry), raw = responseText(entry);
    const fetcher = vi.fn().mockImplementation(() => Promise.resolve(new Response(raw, {status: entry.response.status}))); vi.stubGlobal("fetch", fetcher);
    const run = await runInductiveInput(file, form); expect(run.input_sha256).toBe(entry.input.sha256); expect(run.raw).toBe(raw); expect(fetcher.mock.calls[0][0]).toBe("/api/discover-inductive-input"); expect(fetcher.mock.calls[0][1].body.get("source_id")).toBeNull(); expect(fetcher.mock.calls[0][1].body.get("include_source")).toBeNull();
    expect(Object.fromEntries(inductiveInputFields(form, file.name)).inductive_variant).toBe(entry.variant);
    await expect(runInductiveInput(new File([await file.arrayBuffer()], "other-source.csv"), form)).rejects.toThrow(/does not match/);
    const controller = new AbortController(); vi.stubGlobal("fetch", vi.fn().mockImplementation(() => {controller.abort(); return Promise.resolve(new Response(raw));})); await expect(runInductiveInput(file, form, controller.signal)).rejects.toThrow(/abort/i);
    const gate = new InductiveInputRequestGate(), old = gate.issue(), current = gate.issue(); expect(gate.accepts(old)).toBe(false); expect(gate.accepts(current)).toBe(true); gate.invalidate(); expect(gate.accepts(current)).toBe(false);
  });
  it("publishes the independently decoded complete export proof when requested", () => {
    expect(proofs.length).toBe(entries.length); if (!exportDirectory) return;
    const path = pathInWorkspace(resolve(exportDirectory, "export-integrity.json")); mkdirSync(dirname(path), {recursive: true});
    writeFileSync(path, JSON.stringify({complete: true, manifest: manifestPath, manifest_sha256: digest(readFileSync(manifestPath)), entries: proofs,
      verified_numeric_literals: proofs.reduce((sum, proof) => sum + proof.verified_numeric_literals, 0), csv_cells: proofs.reduce((sum, proof) => sum + proof.csv_cells, 0), selected_records: proofs.reduce((sum, proof) => sum + proof.selected_records, 0)}, null, 2) + "\n", "utf8");
  });
});

