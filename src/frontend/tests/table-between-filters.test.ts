import {afterEach, describe, expect, it, vi} from "vitest";
import {renderToStaticMarkup} from "react-dom/server";
import {createElement} from "react";
import {defaultTableBetweenForm, isTableBetweenEnvelope, requestTableBetween, tableBetweenArtifactFile,
  tableBetweenFields, tableBetweenResponseFile, tableBetweenSourceExample, TableBetweenRequestGate,
  type TableBetweenEnvelope, type TableBetweenRun} from "../src/table-between-filters";
import {TableBetweenFilterInspection, TableBetweenFilterWorkspace} from "../src/components/TableBetweenFilterWorkspace";
import {TableAttributeRepetitionWorkspace} from "../src/components/TableAttributeRepetitionWorkspace";

const source = () => new File(['{"columns":[],"rows":[]}'], "source.json", {type: "application/json"});
function envelope(engine: "pandas" | "polars" = "pandas"): TableBetweenEnvelope {
  return {family: "TABLE_BETWEEN_FILTER", engine, http_status: 200, phase: "complete", apply_returned: true,
    source: {rows: 4, columns: 2}, result: {rows: 2, columns: 2}, source_unchanged: true,
    case_id_key: "case:concept:name", activity_key: "concept:name", source_table_json: '{"source":"complete"}',
    filtered_table_json: '{"rows":[["Ω\\u0000",9007199254740993,-0.0]],"metadata":{"last":"retained"}}',
    error_type: null, error: null};
}
function run(value = envelope()): TableBetweenRun {
  const raw = JSON.stringify(value);
  return {value, sourceFile: source(), fields: [], raw, responseBytes: new TextEncoder().encode(raw)};
}
afterEach(() => vi.unstubAllGlobals());

describe("between table controls and full artifacts", () => {
  it("sends canonical omission and exact raw endpoint JSON", () => {
    expect(tableBetweenFields(defaultTableBetweenForm("pandas"))).toEqual([
      ["engine", "pandas"], ["act1_json", '"A"'], ["act2_json", '"B"'],
    ]);
    const form = {...defaultTableBetweenForm("polars"), startJson: '["", " A,\\nΩ\\u0000 "]', endJson: "[]",
      caseKeyMode: "custom" as const, caseKey: "", activityKeyMode: "custom" as const, activityKey: " work\nΩ\0 ",
      separatorMode: "custom" as const, separator: ""};
    expect(tableBetweenFields(form)).toEqual([["engine", "polars"], ["act1_json", form.startJson], ["act2_json", "[]"],
      ["case_id_key", ""], ["activity_key", form.activityKey], ["subcase_concat_str", ""]]);
    expect(() => tableBetweenFields({...form, engine: "other" as "polars"})).toThrow();
    expect(() => tableBetweenFields({...form, separatorMode: "invalid" as "custom"})).toThrow();
  });
  it("rejects incompatible envelopes and alias fields without losing counts", () => {
    expect(isTableBetweenEnvelope(envelope())).toBe(true);
    expect(isTableBetweenEnvelope({...envelope(), family: "TABLE_ATTRIBUTE_REPETITION"})).toBe(false);
    expect(isTableBetweenEnvelope({...envelope(), attribute_key: "extra"})).toBe(false);
    expect(isTableBetweenEnvelope({...envelope(), source: {rows: -1, columns: 2}})).toBe(false);
    expect(isTableBetweenEnvelope({...envelope(), error: 1})).toBe(false);
    const failure = {...envelope("polars"), http_status: 400, phase: "collect", result: null,
      filtered_table_json: null, error_type: "ColumnNotFoundError", error: "missing\0column"};
    expect(isTableBetweenEnvelope(failure)).toBe(true);
  });
  it("downloads complete table JSON and untouched response bytes", async () => {
    const value = run();
    expect(await tableBetweenArtifactFile(value, "result").text()).toBe(value.value.filtered_table_json);
    expect(await tableBetweenArtifactFile(value, "source").text()).toBe(value.value.source_table_json);
    expect(new Uint8Array(await tableBetweenResponseFile(value).arrayBuffer())).toEqual(value.responseBytes);
    expect(() => tableBetweenArtifactFile(run({...envelope(), http_status: 400, error: "failure"}), "result")).toThrow();
  });
  it("gates stale files, endpoints, separator edits, engine changes and cancellation", () => {
    const gate = new TableBetweenRequestGate(), identity = {source: source(), form: defaultTableBetweenForm("pandas")};
    const ticket = gate.issue(identity);
    expect(gate.accepts(ticket, identity)).toBe(true);
    expect(gate.accepts(ticket, {...identity, source: source()})).toBe(false);
    expect(gate.accepts(ticket, {...identity, form: {...identity.form, startJson: "[]"}})).toBe(false);
    expect(gate.accepts(ticket, {...identity, form: {...identity.form, engine: "polars"}})).toBe(false);
    expect(gate.accepts(ticket, {...identity, form: {...identity.form, separator: ""}})).toBe(false);
    const controller = new AbortController(); controller.abort();
    expect(gate.accepts(ticket, identity, controller.signal)).toBe(false);
    gate.invalidate(); expect(gate.accepts(ticket, identity)).toBe(false);
  });
  it("provides complete engine examples with fixed timestamps and exact integer storage", async () => {
    for (const engine of ["pandas", "polars"] as const) {
      const table = JSON.parse(await tableBetweenSourceExample(engine).text());
      expect(table.columns).toEqual(["case:concept:name", "concept:name", "time:timestamp", "signed", "unsigned"]);
      expect(table.rows).toHaveLength(5);
      expect(table.rows[4][3]).toMatchObject({typed_value: "9007199254740993", integer_storage: "int64"});
      expect(table.rows[4][4]).toMatchObject({typed_value: "18446744073709551615", integer_storage: "uint64"});
      expect(table.dtypes[2]).toContain(engine === "polars" ? "Datetime" : "datetime64");
    }
  });
  it("routes real multipart fields and validates the selected engine", async () => {
    const fetch = vi.fn(async (_path: string, init: RequestInit) => {
      const body = init.body as FormData;
      expect(body.get("source_file")).toBeInstanceOf(File);
      expect(body.get("act1_json")).toBe('"A"');
      expect(body.get("min_rep")).toBeNull();
      return new Response(JSON.stringify(envelope()), {status: 200});
    });
    vi.stubGlobal("fetch", fetch);
    const result = await requestTableBetween(source(), defaultTableBetweenForm("pandas"));
    expect(fetch.mock.calls[0][0]).toBe("/api/reference-table-between");
    expect(result.value.family).toBe("TABLE_BETWEEN_FILTER");
    await expect(requestTableBetween(source(), defaultTableBetweenForm("polars"))).rejects.toThrow("another engine");
  });
  it("renders usable source, literal controls, complete downloads and stage errors", () => {
    const criteria = renderToStaticMarkup(createElement(TableBetweenFilterWorkspace, {engine: "polars"}));
    expect(criteria).toContain("time:timestamp"); expect(criteria).toContain("Start activities as JSON");
    expect(criteria).toContain("End activities as JSON"); expect(criteria).toContain("Subcase separator");
    expect(criteria).toContain("Extract table intervals"); expect(criteria).not.toContain("Minimum repetitions");
    const wrapper = renderToStaticMarkup(createElement(TableAttributeRepetitionWorkspace, {engine: "pandas"}));
    expect(wrapper).toContain("Extract intervals between activities");
    const result = renderToStaticMarkup(createElement(TableBetweenFilterInspection, {run: run()}));
    expect(result).toContain("Download extracted native table"); expect(result).toContain("source table was preserved");
    const error = renderToStaticMarkup(createElement(TableBetweenFilterInspection, {run: run({...envelope("polars"),
      http_status: 400, phase: "collect", filtered_table_json: null, result: null, error_type: "ColumnNotFoundError", error: "missing timestamp"})}));
    expect(error).toContain("stage: collect"); expect(error).toContain("ColumnNotFoundError");
  });
});
