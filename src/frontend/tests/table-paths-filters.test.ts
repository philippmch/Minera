import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {defaultTablePathsForm, tablePathsFields, requestTablePaths, isTablePathsEnvelope, tablePathsArtifactFile,
  tablePathsResponseFile, TablePathsRequestGate, type TablePathsEnvelope, type TablePathsRun} from "../src/table-paths-filters";
import {TablePathsFilterWorkspace, TablePathsFilterInspection} from "../src/components/TablePathsFilterWorkspace";
import {TableAttributeRepetitionWorkspace} from "../src/components/TableAttributeRepetitionWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {defaultReferenceLogFilterForm, referenceLogFilterFields, requestReferenceLogFilter, openReferenceLogFilter} from "../src/reference-log-filters";
const source = () => new File(['{"columns":[],"rows":[]}'], "source.json");
function envelope(): TablePathsEnvelope {
  return {family: "TABLE_PATHS_FILTER", engine: "pandas", operation: "paths", http_status: 200, phase: "complete", apply_returned: true,
    source: {rows: 5, columns: 3}, result: {rows: 2, columns: 3}, source_unchanged: true, case_id_key: "case:concept:name", activity_key: "concept:name", target_attribute_key: "concept:name", timestamp_key: "time:timestamp",
    source_table_json: '{"full":"source"}', filtered_table_json: '{"rows":[[9007199254740993,-0.0,"Ω\\u0000"]],"context":"retained"}', error_type: null, error: null};
}
function run(value = envelope()): TablePathsRun {const raw = JSON.stringify(value); return {value, raw, responseBytes: new TextEncoder().encode(raw), sourceFile: source(), fields: []};}
afterEach(() => vi.unstubAllGlobals());
describe("complete adjacent path workflows", () => {
  it("retains exact paths, literal roles and active timestamps in ordinary mode", () => {
    const form = defaultTablePathsForm("pandas");
    expect(tablePathsFields(form)).toEqual([["engine", "pandas"], ["operation", "paths"], ["positive", "true"], ["paths_json", '[["A", "B"]]']]);
    expect(Object.fromEntries(tablePathsFields({...form, pathsJson: '[[" A,B ", "\\u0000"]]', positive: false, caseKeyMode: "custom", caseKey: "", activityKeyMode: "custom", activityKey: " source\nΩ\0 ", targetKeyMode: "custom", targetKey: "", timestampKeyMode: "custom", timestampKey: "time\0"}))).toEqual({engine: "pandas", operation: "paths", positive: "false", paths_json: '[[" A,B ", "\\u0000"]]', case_id_key: "", activity_key: " source\nΩ\0 ", target_attribute_key: "", timestamp_key: "time\0"});
  });
  it("keeps performance bounds exact and omits the inactive target selector", () => {
    const form = {...defaultTablePathsForm("polars"), operation: "performance" as const, minimum: "2.0000000000000004", maximum: "9223372036854775807", targetKeyMode: "custom" as const, targetKey: "ignored"};
    const fields = Object.fromEntries(tablePathsFields(form)); expect(fields).toMatchObject({path_json: '["A", "B"]', minimum: form.minimum, maximum: form.maximum});
    expect(fields).not.toHaveProperty("target_attribute_key"); expect(fields).not.toHaveProperty("paths_json");
  });
  it("validates every envelope field and preserves native byte values", () => {
    expect(isTablePathsEnvelope(envelope())).toBe(true);
    for (const patch of [{operation: "wrong"}, {family: "TABLE_CASE_FILTER"}, {extra: true}, {timestamp_key: 1}, {target_attribute_key: null}, {result: {rows: -1, columns: 2}}]) expect(isTablePathsEnvelope({...envelope(), ...patch})).toBe(false);
    expect(isTablePathsEnvelope({...envelope(), http_status: 400, phase: "collect", result: null, error: "missing\0column", error_type: "ColumnNotFoundError"})).toBe(true);
  });
  it("downloads the complete table and untouched response bytes", async () => {
    const value = run(); expect(await tablePathsArtifactFile(value, "result").text()).toBe(value.value.filtered_table_json); expect(await tablePathsArtifactFile(value, "source").text()).toBe(value.value.source_table_json);
    expect(new Uint8Array(await tablePathsResponseFile(value).arrayBuffer())).toEqual(value.responseBytes);
    expect(() => tablePathsArtifactFile(run({...envelope(), http_status: 400, error: "failure"}), "result")).toThrow();
  });
  it("posts real FormData and rejects another operation or engine", async () => {
    let response = envelope();
    const fetch = vi.fn(async (_path: string, init: RequestInit) => {const body = init.body as FormData; expect(body.get("source_file")).toBeInstanceOf(File); expect(body.get("paths_json")).toBe('[["A", "B"]]'); expect(body.has("minimum")).toBe(false); return new Response(JSON.stringify(response));});
    vi.stubGlobal("fetch", fetch); expect((await requestTablePaths(source(), defaultTablePathsForm("pandas"))).value.operation).toBe("paths"); expect(fetch.mock.calls[0][0]).toBe("/api/reference-table-paths");
    response = {...response, operation: "performance"}; await expect(requestTablePaths(source(), defaultTablePathsForm("pandas"))).rejects.toThrow("another engine or filter");
  });
  it("invalidates late requests on pair, role, operation and source edits", () => {
    const gate = new TablePathsRequestGate(), identity = {source: source(), form: defaultTablePathsForm("polars")}, ticket = gate.issue(identity);
    expect(gate.accepts(ticket, identity)).toBe(true);
    for (const patch of [{pathsJson: "[]"}, {pathJson: '["X","Y"]'}, {minimum: "3"}, {maximum: "2"}, {operation: "performance" as const}, {positive: false}, {targetKey: ""}, {timestampKey: ""}, {caseKey: ""}]) expect(gate.accepts(ticket, {...identity, form: {...identity.form, ...patch}})).toBe(false);
    expect(gate.accepts(ticket, {...identity, source: source()})).toBe(false); const controller = new AbortController(); controller.abort(); expect(gate.accepts(ticket, identity, controller.signal)).toBe(false);
    gate.invalidate(); expect(gate.accepts(ticket, identity)).toBe(false);
  });
  it("renders timestamp sorting, distinct target control and full result inspection", () => {
    const ordinary = renderToStaticMarkup(createElement(TablePathsFilterWorkspace, {engine: "pandas"}));
    for (const label of ["Adjacent pairs as JSON", "Target activity column", "Timestamp column", "sort by case and timestamp", "Filter table paths"]) expect(ordinary).toContain(label);
    const timed = renderToStaticMarkup(createElement(TablePathsFilterWorkspace, {engine: "polars", initialForm: {...defaultTablePathsForm("polars"), operation: "performance"}}));
    expect(timed).toContain("Minimum seconds"); expect(timed).not.toContain("Target activity column");
    expect(renderToStaticMarkup(createElement(TableAttributeRepetitionWorkspace, {engine: "pandas"}))).toContain("Match adjacent paths");
    const result = renderToStaticMarkup(createElement(TablePathsFilterInspection, {run: run()})); expect(result).toContain("Download filtered native table"); expect(result).toContain("source table was preserved");
  });
  it("transports literal log pairs and reopens using the original activity", async () => {
    const form = defaultReferenceLogFilterForm("path_performance"); form.pathJson = '["start,Ω", "end\\u0000"]'; form.activityKeyMode = "custom"; form.activityKey = " channel\n\0 "; form.timestampKeyMode = "custom"; form.timestampKey = "";
    const value = {family: "REFERENCE_LOG_FILTER", operation: "path_performance", http_status: 200, phase: "complete", source_format: "native_log", source: {traces: 1, events: 2}, result: {traces: 1, events: 2}, source_unchanged: true, activity_key: "work", filter_attribute_key: form.activityKey, filtered_log_json: '{"traces":[]}', filtered_xes: "<log/>", xes_error: null, error: null};
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {const body = init.body as FormData; expect(body.get("path_json")).toBe(form.pathJson); expect(body.get("timestamp_key")).toBe(""); expect(body.has("paths_json")).toBe(false); return new Response(JSON.stringify(value));}));
    const result = await requestReferenceLogFilter(source(), form), open = vi.fn(); openReferenceLogFilter(result, open); expect(open.mock.calls[0][1]).toBe("work"); expect(await open.mock.calls[0][0].text()).toBe("<log/>");
    for (const operation of ["paths", "path_performance"] as const) expect(renderToStaticMarkup(createElement(ReferenceLogFilterWorkspace, {initialOperation: operation}))).toContain(operation === "paths" ? "Adjacent pairs as JSON" : "Minimum pair seconds");
    expect(Object.fromEntries(referenceLogFilterFields(form))).toMatchObject({minimum: "0", maximum: "9223372036854775807", positive: "true"});
  });
});
