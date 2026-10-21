import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {defaultTableTimestampForm, tableTimestampFields, tableTimestampOperations, requestTableTimestamp, isTableTimestampEnvelope,
  tableTimestampArtifactFile, tableTimestampResponseFile, tableTimestampSourceExample, TableTimestampRequestGate, type TableTimestampEnvelope, type TableTimestampRun} from "../src/table-timestamp-filters";
import {TableTimestampFilterWorkspace, TableTimestampFilterInspection} from "../src/components/TableTimestampFilterWorkspace";
import {TableAttributeRepetitionWorkspace} from "../src/components/TableAttributeRepetitionWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {defaultReferenceLogFilterForm, referenceLogFilterFields, requestReferenceLogFilter, referenceLogFilterSourceFile, openReferenceLogFilter, isReferenceLogFilterEnvelope} from "../src/reference-log-filters";
const source = () => new File(['{"columns":[],"rows":[]}'], "source.json");
function envelope(): TableTimestampEnvelope {
  return {family: "TABLE_TIMESTAMP_FILTER", engine: "pandas", operation: "events", http_status: 200, phase: "complete", apply_returned: true,
    source: {rows: 5, columns: 3}, result: {rows: 3, columns: 3}, source_unchanged: true, case_id_key: "case:concept:name", attribute_key: "concept:name", timestamp_key: "time:timestamp",
    source_table_json: '{"complete":"source"}', filtered_table_json: '{"rows":[[9007199254740993,-0.0,"Ω\\u0000"]],"context":"retained"}', error_type: null, error: null};
}
function run(value = envelope()): TableTimestampRun {const raw = JSON.stringify(value); return {value, raw, responseBytes: new TextEncoder().encode(raw), sourceFile: source(), fields: []};}
afterEach(() => vi.unstubAllGlobals());
describe("timestamp workflow transport and complete artifacts", () => {
  it("sends exact endpoints and only active role controls", () => {
    const form = defaultTableTimestampForm("pandas");
    expect(Object.fromEntries(tableTimestampFields({...form, caseKeyMode: "custom", caseKey: "inactive", timestampKeyMode: "custom", timestampKey: " time\nΩ\0 ", lower: " literal bound ", upper: "2026-06-01 00:00:03"}))).toEqual({engine: "pandas", operation: "events", lower: " literal bound ", upper: "2026-06-01 00:00:03", timestamp_key: " time\nΩ\0 ", keep_nan_values: "false"});
    expect(Object.fromEntries(tableTimestampFields({...form, operation: "attribute", caseKeyMode: "custom", caseKey: "", attributeKeyMode: "custom", attributeKey: "selected\n\0", targetJson: '{"type":"integer","value":"9007199254740993"}'}))).toMatchObject({case_id_key: "", attribute_key: "selected\n\0", attribute_value_json: '{"type":"integer","value":"9007199254740993"}'});
  });
  it("restricts engine-specific functions and optional controls", () => {
    expect(tableTimestampOperations("polars")).toEqual(["events", "contained", "intersecting", "attribute"]);
    const form = defaultTableTimestampForm("polars"); expect(Object.fromEntries(tableTimestampFields({...form, keepMissing: true}))).not.toHaveProperty("keep_nan_values");
    expect(() => tableTimestampFields({...form, operation: "starting"})).toThrow("available");
    expect(Object.fromEntries(tableTimestampFields({...defaultTableTimestampForm("pandas"), operation: "completing", positive: false, keepMissing: true}))).toMatchObject({positive: "false", keep_nan_values: "true"});
    expect(Object.fromEntries(tableTimestampFields(defaultTableTimestampForm("pandas")))).not.toHaveProperty("positive");
  });
  it("validates the full success and failure envelope", () => {
    expect(isTableTimestampEnvelope(envelope())).toBe(true);
    for (const patch of [{operation: "wrong"}, {family: "TABLE_PATHS_FILTER"}, {extra: true}, {timestamp_key: null}, {result: {rows: -1, columns: 2}}, {apply_returned: "yes"}]) expect(isTableTimestampEnvelope({...envelope(), ...patch})).toBe(false);
    expect(isTableTimestampEnvelope({...envelope(), engine: "polars", http_status: 400, phase: "collect", result: null, error: "missing\0column", error_type: "ColumnNotFoundError"})).toBe(true);
  });
  it("posts a genuine File and rejects a mismatched engine or operation", async () => {
    let answer = envelope(); const fetch = vi.fn(async (_path: string, init: RequestInit) => {const body = init.body as FormData;
      expect(body.get("source_file")).toBeInstanceOf(File); expect(body.get("lower")).toBe("2026-06-01 00:00:01"); expect(body.has("positive")).toBe(false); return new Response(JSON.stringify(answer));});
    vi.stubGlobal("fetch", fetch); expect((await requestTableTimestamp(source(), defaultTableTimestampForm("pandas"))).value.operation).toBe("events"); expect(fetch.mock.calls[0][0]).toBe("/api/reference-table-timestamp");
    answer = {...answer, operation: "contained"}; await expect(requestTableTimestamp(source(), defaultTableTimestampForm("pandas"))).rejects.toThrow("another engine or filter");
  });
  it("downloads every byte of the result, source and response", async () => {
    const value = run(); expect(await tableTimestampArtifactFile(value, "result").text()).toBe(value.value.filtered_table_json);
    expect(await tableTimestampArtifactFile(value, "source").text()).toBe(value.value.source_table_json);
    expect(new Uint8Array(await tableTimestampResponseFile(value).arrayBuffer())).toEqual(value.responseBytes);
    expect(() => tableTimestampArtifactFile(run({...envelope(), http_status: 400, error: "failure"}), "result")).toThrow();
  });
  it("invalidates late requests after source, mode, role or endpoint edits", () => {
    const gate = new TableTimestampRequestGate(), identity = {source: source(), form: defaultTableTimestampForm("pandas")}, ticket = gate.issue(identity);
    expect(gate.accepts(ticket, identity)).toBe(true);
    for (const patch of [{lower: ""}, {upper: "changed"}, {operation: "attribute" as const}, {positive: false}, {keepMissing: true}, {caseKey: ""}, {timestampKey: ""}, {attributeKey: ""}, {targetJson: "{}"}]) expect(gate.accepts(ticket, {...identity, form: {...identity.form, ...patch}})).toBe(false);
    expect(gate.accepts(ticket, {...identity, source: source()})).toBe(false); const abort = new AbortController(); abort.abort(); expect(gate.accepts(ticket, identity, abort.signal)).toBe(false); gate.invalidate(); expect(gate.accepts(ticket, identity)).toBe(false);
  });
  it("provides a complete UTC example for both engines", async () => {
    for (const engine of ["pandas", "polars"] as const) {
      const example = JSON.parse(await tableTimestampSourceExample(engine).text()); expect(example.dtypes[2]).toContain("UTC"); expect(example.rows).toHaveLength(5);
      expect(example.rows[0][2].value).toBe("2026-06-01T00:00:00Z"); expect(example.rows[0][3].typed_value).toBe("9007199254740993"); expect(example.rows[0][4].typed_value).toBe("18446744073709551615");
    }
  });
  it("renders real engine controls, strict intersection and complete downloads", () => {
    const pandas = renderToStaticMarkup(createElement(TableTimestampFilterWorkspace, {engine: "pandas"}));
    for (const label of ["Lower endpoint", "Timestamp column", "case start time", "without temporal values", "Filter table timestamps"]) expect(pandas).toContain(label);
    const polars = renderToStaticMarkup(createElement(TableTimestampFilterWorkspace, {engine: "polars", initialForm: {...defaultTableTimestampForm("polars"), operation: "intersecting"}}));
    expect(polars).toContain("strict boundary comparisons"); expect(polars).not.toContain("case start time"); expect(polars).not.toContain("without temporal values");
    expect(renderToStaticMarkup(createElement(TableAttributeRepetitionWorkspace, {engine: "pandas"}))).toContain("Filter a time window");
    expect(renderToStaticMarkup(createElement(TableTimestampFilterInspection, {run: run()}))).toContain("Download filtered native table");
  });
  it("sends log timestamp criteria without inactive selection or attribute controls", () => {
    const form = defaultReferenceLogFilterForm("time_events"); expect(Object.fromEntries(referenceLogFilterFields(form))).toMatchObject({operation: "time_events", lower: form.timeLower, upper: form.timeUpper});
    expect(Object.fromEntries(referenceLogFilterFields(form))).not.toHaveProperty("positive"); expect(Object.fromEntries(referenceLogFilterFields(form))).not.toHaveProperty("time_target_json");
    expect(Object.fromEntries(referenceLogFilterFields({...form, operation: "time_attribute", timeAttributeKeyMode: "custom", timeAttributeKey: "", timestampKeyMode: "custom", timestampKey: " time\n\0 ", timeTargetJson: '{"type":"null"}'}))).toMatchObject({time_attribute_key: "", timestamp_key: " time\n\0 ", time_target_json: '{"type":"null"}'});
    expect(Object.fromEntries(referenceLogFilterFields({...form, operation: "time_starting", positive: false}))).toHaveProperty("positive", "false");
  });
  it("retains log source mutations and complete snapshots through reopening", async () => {
    const form = defaultReferenceLogFilterForm("time_events"), value = {family: "REFERENCE_LOG_FILTER", operation: "time_events", http_status: 200, phase: "complete", source_format: "native_log", source: {traces: 1, events: 2}, result: {traces: 1, events: 1}, source_unchanged: false, activity_key: "work", timestamp_key: "when", time_attribute_key: "work", source_log_json: '{"before":"Ω\\u0000"}', source_after_log_json: '{"after":9007199254740993}', filtered_log_json: '{"traces":[]}', filtered_xes: "<log/>", xes_error: null, error: null};
    expect(isReferenceLogFilterEnvelope(value)).toBe(true); expect(isReferenceLogFilterEnvelope({...value, source_after_log_json: 1})).toBe(false);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(value)))); const result = await requestReferenceLogFilter(source(), form);
    expect(await referenceLogFilterSourceFile(result).text()).toBe(value.source_log_json); expect(await referenceLogFilterSourceFile(result, true).text()).toBe(value.source_after_log_json);
    const open = vi.fn(); openReferenceLogFilter(result, open); expect(open.mock.calls[0][1]).toBe("work");
  });
  it("renders seven log time modes and converter effects", () => {
    for (const operation of ["time_events", "time_contained", "time_intersecting", "time_starting", "time_completing", "time_attribute", "time_trace_attribute"] as const) {
      const html = renderToStaticMarkup(createElement(ReferenceLogFilterWorkspace, {initialOperation: operation})); expect(html).toContain("Lower endpoint");
      if (operation === "time_events") expect(html).toContain("source snapshots before and after");
      if (operation === "time_trace_attribute") expect(html).toContain("Trace timestamp attribute");
      if (operation === "time_attribute") expect(html).toContain("Matched event attribute");
    }
  });
});
