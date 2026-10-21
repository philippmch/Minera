import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {afterEach, describe, expect, it, vi} from "vitest";
import {nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {TableAttributeRepetitionInspection, TableAttributeRepetitionWorkspace} from "../src/components/TableAttributeRepetitionWorkspace";
import {TableRepetitionRequestGate, defaultTableRepetitionForm, isTableRepetitionEnvelope, requestTableRepetition,
  tableRepetitionArtifactFile, tableRepetitionCountText, tableRepetitionFields, tableRepetitionResponseFile,
  tableRepetitionSourceExample, type TableRepetitionEngine, type TableRepetitionEnvelope, type TableRepetitionRun} from "../src/table-attribute-repetition";

afterEach(() => vi.unstubAllGlobals());
const source = () => new File([new Uint8Array([255, 0, 237, 160, 128])], "complete-table.json", {lastModified: 42});
const artifact = '{"columns":["signed"],"rows":[[{"kind":"int","value":"9007199254740993","typed_value":9007199254740993,"integer_storage":"int64"}]],"attributes":[{"key":"retained","value":"original"}]}';
const envelope = (engine: TableRepetitionEngine = "pandas"): TableRepetitionEnvelope => ({family: "TABLE_ATTRIBUTE_REPETITION", engine, http_status: 200, phase: "complete", apply_returned: true,
  source: {rows: 4, columns: 4}, result: {rows: 3, columns: 4}, source_unchanged: true, case_id_key: "case:concept:name", attribute_key: "concept:name",
  source_table_json: artifact, filtered_table_json: artifact, error_type: null, error: null});
function reply(value = envelope(), status: number = value.http_status) {const fetch = vi.fn(async () => new Response(nativeJson(value), {status}));vi.stubGlobal("fetch", fetch);return fetch;}
function run(value = envelope()): TableRepetitionRun {const raw = nativeJson(value);return {value, sourceFile: source(), fields: [], raw, responseBytes: nativeStringBytes(raw)};}
const body = (fetch: ReturnType<typeof reply>) => (fetch.mock.calls[0] as unknown as [string, RequestInit])[1].body as FormData;
async function bytes(form: FormData, key: string) {const value = form.get(key);return value instanceof Blob ? new Uint8Array(await value.arrayBuffer()) : nativeStringBytes(value ?? "");}
const display = (node: Parameters<typeof renderToStaticMarkup>[0]) => renderToStaticMarkup(node);

describe("typed table repetition transport", () => {
  it("keeps default selectors omitted and explicit empty/multiline selectors literal", () => {
    for (const engine of ["pandas", "polars"] as const) {
      const form = defaultTableRepetitionForm(engine), fields = Object.fromEntries(tableRepetitionFields(form));
      expect(fields).toEqual({engine, target_json: form.targetJson, min_rep: "2", max_rep: "9223372036854775807"});
      form.caseKeyMode = "custom";form.caseKey = "";form.attributeKeyMode = "custom";form.attributeKey = " selected\n\0";
      expect(tableRepetitionFields(form)).toContainEqual(["case_id_key", ""]);expect(tableRepetitionFields(form)).toContainEqual(["attribute_key", form.attributeKey]);
      for (const token of ["9007199254740993", "18446744073709551615", "-999999999999999999999999999999999999", "2.0000000000000004", "NaN", "INF", "-INF", "null", '"2"']) {
        form.minimum = token;form.maximum = token;expect(tableRepetitionFields(form)).toContainEqual(["min_rep", token]);expect(tableRepetitionFields(form)).toContainEqual(["max_rep", token]);
      }
    }
  });
  it("uploads the original complete table File with exact typed target bytes and no log/CSV controls", async () => {
    const form = defaultTableRepetitionForm("polars"), fetch = reply(envelope("polars")), file = source();
    form.targetJson = '\r\n{"type":"integer","value":9007199254740993}\n';form.attributeKeyMode = "custom";form.attributeKey = " selected\r\n\0\ud800";
    const result = await requestTableRepetition(file, form), sent = body(fetch);
    expect((fetch.mock.calls[0] as unknown as [string])[0]).toBe("/api/reference-table-repetition");expect(sent.get("source_file")).toBe(file);expect(result.sourceFile).toBe(file);
    expect(await bytes(sent, "target_json")).toEqual(nativeStringBytes(form.targetJson));expect(await bytes(sent, "attribute_key")).toEqual(nativeStringBytes(form.attributeKey));
    expect(new Uint8Array(await (sent.get("source_file") as File).arrayBuffer())).toEqual(new Uint8Array(await file.arrayBuffer()));
    for (const key of ["operation", "source_format", "positive", "activity", "activity_key", "case_id_column", "delimiter"]) expect(sent.has(key)).toBe(false);
  });
  it("retains engine error type, apply versus collection phase and full NUL diagnostic bytes", async () => {
    for (const engine of ["pandas", "polars"] as const) {
      const value = envelope(engine);Object.assign(value, {http_status: 400, phase: engine === "pandas" ? "apply" : "collect", apply_returned: engine === "polars", result: null, filtered_table_json: null, error_type: engine === "pandas" ? "KeyError" : "ColumnNotFoundError", error: "missing\0tail\nplan"});reply(value);
      const result = await requestTableRepetition(source(), defaultTableRepetitionForm(engine));expect(result.value).toEqual(value);
      expect(await tableRepetitionArtifactFile(result, "source").text()).toBe(artifact);expect(() => tableRepetitionArtifactFile(result, "result")).toThrow(/unavailable/);
    }
  });
  it("retains a source-import failure without inventing parsed table metadata", async () => {
    const value = envelope();Object.assign(value, {http_status: 400, phase: "source_import", apply_returned: false, source: null, result: null, source_unchanged: null, source_table_json: null, filtered_table_json: null, error_type: "native_error", error: "Invalid table JSON"});reply(value);
    const result = await requestTableRepetition(source(), defaultTableRepetitionForm("pandas"));expect(result.value.source).toBeNull();expect(() => tableRepetitionArtifactFile(result, "source")).toThrow(/unavailable/);
  });
  it("downloads the complete source/result and original response bytes without JSON number rewriting", async () => {
    const value = envelope();value.source_table_json = {native_bytes: "hex-v1", data: "7b7dff00eda080"};reply(value);
    const result = await requestTableRepetition(source(), defaultTableRepetitionForm("pandas"));
    expect(await tableRepetitionArtifactFile(result, "result").text()).toBe(artifact);
    expect(new Uint8Array(await tableRepetitionArtifactFile(result, "source").arrayBuffer())).toEqual(new Uint8Array([123,125,255,0,237,160,128]));
    expect(new Uint8Array(await tableRepetitionResponseFile(result).arrayBuffer())).toEqual(result.responseBytes);
    expect(tableRepetitionArtifactFile(result, "result").name).toBe("filtered-pandas-table.json");
  });
  it("rejects foreign engines, status mismatches, unknown fields and malformed native carriers", async () => {
    for (const value of [envelope("polars"), {...envelope(), extra: 1}, {...envelope(), case_id_key: null}, {...envelope(), filtered_table_json: {native_bytes: "hex-v1", data: "f"}}]) {
      reply(value as TableRepetitionEnvelope);await expect(requestTableRepetition(source(), defaultTableRepetitionForm("pandas"))).rejects.toThrow(/engine|envelope/);
    }
    reply(envelope(), 400);await expect(requestTableRepetition(source(), defaultTableRepetitionForm("pandas"))).rejects.toThrow(/envelope/);
    vi.stubGlobal("fetch", vi.fn(async () => new Response("{}", {status: 500})));await expect(requestTableRepetition(source(), defaultTableRepetitionForm("pandas"))).rejects.toThrow(/HTTP 500/);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(new Uint8Array([255]), {status: 400})));await expect(requestTableRepetition(source(), defaultTableRepetitionForm("pandas"))).rejects.toThrow(/safe JSON/);
  });
  it("validates exact uint64 count tokens and keeps unknown envelope fields rejected", () => {
    const value = parseNativeJson(nativeJson(envelope()).replace('"rows":4', '"rows":18446744073709551615')) as TableRepetitionEnvelope;
    expect(isTableRepetitionEnvelope(value)).toBe(true);expect(tableRepetitionCountText(value.source!, "rows")).toBe("18446744073709551615");
    for (const rows of [-1, 1.5, Infinity]) expect(isTableRepetitionEnvelope({...envelope(), source: {rows, columns: 4}})).toBe(false);
    expect(isTableRepetitionEnvelope({...value, unknown: null})).toBe(false);
  });
  it("honors early/late aborts and rejects a text source rather than uploading a table placeholder", async () => {
    const controller = new AbortController(), fetch = reply();controller.abort();
    await expect(requestTableRepetition(source(), defaultTableRepetitionForm("pandas"), controller.signal)).rejects.toThrow();expect(fetch).not.toHaveBeenCalled();
    const late = new AbortController();vi.stubGlobal("fetch", vi.fn(async () => {late.abort();return new Response(nativeJson(envelope()));}));
    await expect(requestTableRepetition(source(), defaultTableRepetitionForm("pandas"), late.signal)).rejects.toThrow();
    await expect(requestTableRepetition("{}" as unknown as File, defaultTableRepetitionForm("pandas"))).rejects.toThrow(/actual native/);
  });
  it("invalidates source, engine, target, selectors and both bounds across request generations", () => {
    const identity = {source: source(), form: defaultTableRepetitionForm("pandas")}, gate = new TableRepetitionRequestGate(), ticket = gate.issue(identity);
    expect(gate.accepts(ticket, identity)).toBe(true);
    for (const patch of [{engine: "polars" as const}, {caseKeyMode: "custom" as const}, {caseKey: ""}, {attributeKeyMode: "custom" as const}, {attributeKey: "\n"}, {targetJson: '{}'}, {minimum: "0"}, {maximum: "1"}]) expect(gate.accepts(ticket, {...identity, form: {...identity.form, ...patch}})).toBe(false);
    expect(gate.accepts(ticket, {...identity, source: source()})).toBe(false);gate.issue(identity);expect(gate.accepts(ticket, identity)).toBe(false);gate.invalidate();
  });
  it("provides usable engine-specific table examples with explicit positive signed/unsigned storage", async () => {
    for (const engine of ["pandas", "polars"] as const) {
      const example = tableRepetitionSourceExample(engine), value = JSON.parse(await example.text());expect(example).toBeInstanceOf(File);expect(example.name).toContain(engine);
      expect(value.columns).toEqual(["case:concept:name", "concept:name", "signed", "unsigned"]);expect(value.rows).toHaveLength(4);
      expect(value.dtypes).toEqual(engine === "polars" ? ["String", "String", "Int64", "UInt64"] : ["object", "object", "int64", "uint64"]);
      expect(value.rows[0][2]).toMatchObject({typed_value: "9007199254740993", integer_storage: "int64"});expect(value.rows[0][3]).toMatchObject({typed_value: "18446744073709551615", integer_storage: "uint64"});
    }
  });
});

describe("table workspace SSR and unit helpers (no rendered browser interaction)", () => {
  it("offers all three data models in the existing workspace and exposes separate native table controls", () => {
    const fetch = vi.fn();vi.stubGlobal("fetch", fetch);
    for (const engine of ["pandas", "polars"] as const) {
      const html = display(createElement(ReferenceLogFilterWorkspace, {initialEngine: engine}));
      for (const text of ["Event log", "pandas dataframe", "Polars lazy dataframe", "Native EventTable JSON file", "Case ID column", "Counted attribute column", "Minimum repetitions", "Maximum repetitions", "Download "+engine+" example table"]) expect(html).toContain(text);
      expect(html).not.toContain("Source format");expect(html).not.toContain("Download example CSV");expect(html).not.toContain("Open filtered log");expect(fetch).not.toHaveBeenCalled();
      expect(html).toContain(engine === "polars" ? "errors identify the stage" : "index labels, category declarations");expect(html).toContain("hexadecimal data");
    }
  });
  it("renders multiline/empty literal table keys and typed JSON criteria", () => {
    const initialForm = defaultTableRepetitionForm("pandas");initialForm.caseKeyMode = "custom";initialForm.caseKey = "";initialForm.attributeKeyMode = "custom";initialForm.attributeKey = " selected\n";
    const html = display(createElement(TableAttributeRepetitionWorkspace, {engine: "pandas", initialForm}));expect(html).toContain('rows="2"');expect(html).toContain(" selected\n");expect(html).toContain("9007199254740993");
  });
  it("renders typed table counts and complete artifact downloads without a log projection action", () => {
    const html = display(createElement(TableAttributeRepetitionInspection, {run: run()}));
    for (const text of ["Source and filtered table", "Rows", "Columns", "Download parsed source table", "Download filtered native table", "Download table filter response", "The source table was preserved", "apply returned: yes"]) expect(html).toContain(text);
    expect(html).not.toContain('disabled=""');expect(html).not.toContain("XES");expect(html).not.toContain("Open filtered log");
  });
  it("shows collect failures with parsed source available and result disabled", () => {
    const value = envelope("polars");Object.assign(value, {http_status: 400, phase: "collect", result: null, filtered_table_json: null, error_type: "ColumnNotFoundError", error: "Missing selected column"});
    const html = display(createElement(TableAttributeRepetitionInspection, {run: run(value)}));expect(html).toContain("stage: collect; apply returned: yes");expect(html).toContain('role="alert"');expect(html).toContain("ColumnNotFoundError");expect(html).toContain('disabled="">Download filtered native table');expect(html).not.toContain('disabled="">Download parsed source table');
  });
});
