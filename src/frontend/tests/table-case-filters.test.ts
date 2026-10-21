import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {defaultTableCaseForm, tableCaseFields, requestTableCase, isTableCaseEnvelope, tableCaseArtifactFile,
  tableCaseResponseFile, TableCaseRequestGate, type TableCaseEnvelope, type TableCaseRun} from "../src/table-case-filters";
import {TableCaseFilterWorkspace, TableCaseFilterInspection} from "../src/components/TableCaseFilterWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {defaultReferenceLogFilterForm, referenceLogFilterFields} from "../src/reference-log-filters";
const source = () => new File(['{"columns":[],"rows":[]}'], "source.json");
function envelope(): TableCaseEnvelope {
  return {family: "TABLE_CASE_FILTER", engine: "pandas", operation: "size", http_status: 200, phase: "complete", apply_returned: true,
    source: {rows: 4, columns: 2}, result: {rows: 2, columns: 2}, source_unchanged: true, case_id_key: "case:concept:name", timestamp_key: "time:timestamp",
    source_table_json: '{"full":"source"}', filtered_table_json: '{"rows":[[9007199254740993,-0.0,"Ω\\u0000"]],"context":"retained"}', error_type: null, error: null};
}
function run(value = envelope()): TableCaseRun {const raw = JSON.stringify(value); return {value, raw, responseBytes: new TextEncoder().encode(raw), sourceFile: source(), fields: []};}
afterEach(() => vi.unstubAllGlobals());
describe("complete case workflows", () => {
  it("sends operation-specific exact bounds and literal selectors", () => {
    const form = defaultTableCaseForm("pandas");
    expect(tableCaseFields(form)).toEqual([["engine", "pandas"], ["operation", "size"], ["minimum", "2"], ["maximum", "null"]]);
    expect(tableCaseFields({...form, operation: "count", maxCases: "-9007199254740993"})).toEqual([["engine", "pandas"], ["operation", "count"], ["max_cases", "-9007199254740993"]]);
    expect(Object.fromEntries(tableCaseFields({...form, operation: "performance", minimum: "-0.0", maximum: "2.0000000000000004", caseKeyMode: "custom", caseKey: "", timestampKeyMode: "custom", timestampKey: " \nΩ\0 ", businessHours: true}))).toEqual({engine: "pandas", operation: "performance", minimum: "-0.0", maximum: "2.0000000000000004", case_id_key: "", timestamp_key: " \nΩ\0 ", business_hours: "true"});
  });
  it("validates full envelopes including operation and engine errors", () => {
    expect(isTableCaseEnvelope(envelope())).toBe(true);
    for (const patch of [{operation: "wrong"}, {family: "TABLE_BETWEEN_FILTER"}, {extra: true}, {result: {rows: -1, columns: 2}}, {timestamp_key: 1}]) expect(isTableCaseEnvelope({...envelope(), ...patch})).toBe(false);
    expect(isTableCaseEnvelope({...envelope(), operation: null, engine: null, http_status: 400, phase: "control", result: null, error: "choose filter"})).toBe(true);
  });
  it("retains complete artifact and response bytes", async () => {
    const value = run();
    expect(await tableCaseArtifactFile(value, "result").text()).toBe(value.value.filtered_table_json);
    expect(await tableCaseArtifactFile(value, "source").text()).toBe(value.value.source_table_json);
    expect(new Uint8Array(await tableCaseResponseFile(value).arrayBuffer())).toEqual(value.responseBytes);
    expect(() => tableCaseArtifactFile(run({...envelope(), http_status: 400, error: "failed"}), "result")).toThrow();
  });
  it("uploads real multipart and rejects a different operation", async () => {
    let response = envelope();
    const fetch = vi.fn(async (_path: string, init: RequestInit) => {const body = init.body as FormData; expect(body.get("source_file")).toBeInstanceOf(File); expect(body.get("operation")).toBe("size"); expect(body.has("max_cases")).toBe(false); return new Response(JSON.stringify(response));});
    vi.stubGlobal("fetch", fetch);
    expect((await requestTableCase(source(), defaultTableCaseForm("pandas"))).value.operation).toBe("size");
    expect(fetch.mock.calls[0][0]).toBe("/api/reference-table-cases");
    response = {...response, operation: "count"}; await expect(requestTableCase(source(), defaultTableCaseForm("pandas"))).rejects.toThrow("another engine or filter");
  });
  it("invalidates late requests on criteria, file and cancellation changes", () => {
    const gate = new TableCaseRequestGate(), identity = {source: source(), form: defaultTableCaseForm("polars")}, ticket = gate.issue(identity);
    expect(gate.accepts(ticket, identity)).toBe(true);
    for (const patch of [{minimum: "3"}, {maximum: "2"}, {maxCases: "0"}, {operation: "count" as const}, {businessHours: true}, {timestampKey: ""}, {caseKey: ""}]) expect(gate.accepts(ticket, {...identity, form: {...identity.form, ...patch}})).toBe(false);
    expect(gate.accepts(ticket, {...identity, source: source()})).toBe(false);
    const controller = new AbortController(); controller.abort(); expect(gate.accepts(ticket, identity, controller.signal)).toBe(false);
    gate.invalidate(); expect(gate.accepts(ticket, identity)).toBe(false);
  });
  it("renders source, case criteria, duration and complete downloads", () => {
    const html = renderToStaticMarkup(createElement(TableCaseFilterWorkspace, {engine: "pandas", initialForm: {...defaultTableCaseForm("pandas"), operation: "performance"}}));
    for (const label of ["Case duration", "Minimum seconds", "Maximum seconds", "Timestamp column", "Measure business hours", "Filter table cases"]) expect(html).toContain(label);
    const result = renderToStaticMarkup(createElement(TableCaseFilterInspection, {run: run()}));
    expect(result).toContain("Download filtered native table"); expect(result).toContain("source table was preserved");
    const failed = renderToStaticMarkup(createElement(TableCaseFilterInspection, {run: run({...envelope(), http_status: 400, phase: "collect", error_type: "InvalidOperationError", error: "invalid bound", result: null, filtered_table_json: null})}));
    expect(failed).toContain("InvalidOperationError"); expect(failed).toContain("stage: collect");
  });
  it("offers log size/count/performance without irrelevant selection controls", () => {
    for (const operation of ["case_size", "case_count", "case_performance"] as const) {
      const html = renderToStaticMarkup(createElement(ReferenceLogFilterWorkspace, {initialOperation: operation}));
      expect(html).toContain("empty log-level context"); expect(html).not.toContain("Keep matching cases"); expect(html).not.toContain("Activity attribute");
      expect(Object.fromEntries(referenceLogFilterFields(defaultReferenceLogFilterForm(operation)))).not.toHaveProperty("positive");
    }
    const form = defaultReferenceLogFilterForm("case_performance"); form.timestampKeyMode = "custom"; form.timestampKey = " \nΩ\0 ";
    expect(referenceLogFilterFields(form)).toContainEqual(["timestamp_key", form.timestampKey]);
  });
});
