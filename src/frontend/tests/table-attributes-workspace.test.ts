import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {describe, expect, it} from "vitest";
import {TableAttributesFilterInspection, TableAttributesFilterWorkspace} from "../src/components/TableAttributesFilterWorkspace";
import {defaultTableAttributesForm, type TableAttributesEnvelope, type TableAttributesForm, type TableAttributesRun} from "../src/table-attributes-filters";

function workspace(engine: "pandas" | "polars", patch: Partial<TableAttributesForm> = {}) {
  return renderToStaticMarkup(createElement(TableAttributesFilterWorkspace, {engine, initialForm: {...defaultTableAttributesForm(engine), ...patch}}));
}
function run(patch: Partial<TableAttributesEnvelope> = {}): TableAttributesRun {
  const value: TableAttributesEnvelope = {family: "TABLE_ATTRIBUTES_FILTER", engine: "polars", operation: "events", http_status: 200,
    call_mode:"friendly",call_json:null,controls_before_json:null,controls_after_json:null,controls_unchanged:null,
    phase: "complete", apply_returned: true, source: {rows: 12, columns: 9}, result: {rows: 4, columns: 9},
    source_unchanged: true, case_id_key: "case:concept:name", attribute_key: "literal selected role", source_table_json: '{"complete":"source"}',
    source_after_table_json: '{"complete":"after"}', filtered_table_json: '{"complete":"filtered"}', counts_table_json: null,
    result_kind: "table", error_type: null, error: null, ...patch};
  const raw = JSON.stringify(value); return {value, raw, responseBytes: new TextEncoder().encode(raw), sourceFile: new File(["{}"], "source.json"), fields: []};
}

describe("table attribute workspace displays only actual operation controls", () => {
  it("requires a genuine native table upload before enabling the action", () => {
    const html = workspace("polars"); expect(html).toContain('type="file"'); expect(html).toContain('accept=".json"');
    expect(html).toContain("including any index and metadata"); expect(html).toMatch(/<button[^>]*disabled=""[^>]*>Filter table attributes<\/button>/);
    expect(html).toContain("Download polars attributes example table");
  });
  it("explains pandas missing values as mask membership before negative selection", () => {
    const html = workspace("pandas"); expect(html).toContain("Include missing values in matching");
    expect(html).toContain("Negative selection excludes those values"); expect(html).toContain("matching cases in case mode");
    expect(html).not.toContain("Keep missing attribute values"); expect(workspace("polars")).not.toContain("Include missing values in matching");
    expect(workspace("pandas", {operation: "threshold"})).not.toContain("Include missing values in matching");
  });
  it("explains the distinct missing-value behavior of negative numeric events", () => {
    const events = workspace("pandas", {operation: "numeric_events", positive: false, keepMissing: true});
    expect(events).toContain("missing values are retained during both positive and negative numeric event selection");
    expect(events).not.toContain("Negative selection excludes those values");
    expect(workspace("pandas", {operation: "numeric_traces", positive: false, keepMissing: true})).toContain("Negative selection excludes those values");
  });
  it("renders exact typed collections and numeric tokens without confusing their controls", () => {
    const membership = workspace("polars", {valuesKind: "typed"}); expect(membership).toContain("Explicit typed collection");
    expect(membership).toContain("Integer text is preserved exactly"); expect(membership).not.toContain("Lower numeric bound");
    const numeric = workspace("polars", {operation: "numeric_events", lower: "9007199254740993", upper: "18446744073709551615"});
    expect(numeric).toContain("9007199254740993"); expect(numeric).toContain("18446744073709551615"); expect(numeric).toContain("both endpoints are included");
    expect(numeric).not.toContain("Admitted attribute values as JSON"); expect(numeric).not.toContain("Case ID column");
  });
  it("shows stream constraints only for whole-case numeric selection", () => {
    const html = workspace("polars", {operation: "numeric_traces", streamOne: true, streamTwo: true});
    for (const label of ["First exact event attribute", "Second exact event attribute", "First exact typed value as JSON", "Second exact typed value as JSON"]) expect(html).toContain(label);
    expect(workspace("polars", {operation: "numeric_events", streamOne: true})).not.toContain("Require additional event attributes");
  });
  it("explains the three relative defaults and hides the inert explicit-false case selector", () => {
    const defaults = workspace("polars", {operation: "relative"}); expect(defaults).toContain("Polars default uses event counts divided by distinct case IDs");
    expect(defaults).toContain("Case ID column"); expect(defaults).toContain("Use engine default"); expect(defaults).toContain("Selection");
    expect(workspace("polars", {operation: "relative", keepOnceMode: "false"})).not.toContain("Case ID column");
    expect(workspace("polars", {operation: "relative", keepOnceMode: "true"})).toContain("Case ID column");
  });
  it("shows real count mode separately from event/case filtering", () => {
    const html = workspace("polars", {operation: "counts"}); expect(html).toContain("Count table attribute values");
    expect(html).toContain("Attribute counts default to counting every event"); expect(html).not.toContain("Case ID column"); expect(html).not.toContain("<span>Selection</span>");
    expect(workspace("polars", {operation: "counts", keepOnceMode: "true"})).toContain("Case ID column");
    expect(workspace("pandas")).not.toContain('<option value="counts"');
  });
  it("keeps supplied typed counters/exempt activities local to absolute threshold filtering", () => {
    const html = workspace("polars", {operation: "threshold", countsMode: "supplied", mostCommonMode: "supplied", activityKeyMode: "custom"});
    for (const label of ["Frequency activity column", "Supplied counter as typed JSON", "Typed most common variant", "Keys retain their complete scalar or tuple types"]) expect(html).toContain(label);
    expect(html).not.toContain("Matched attribute column"); expect(html).not.toContain("Case ID column"); expect(html).not.toContain("<span>Selection</span>");
    expect(workspace("polars", {operation: "top_activities", countsMode: "supplied"})).not.toContain("Supplied counter as typed JSON");
    expect(workspace("pandas", {operation: "top_activities"})).toContain("preserves the table for the captured integer limits");
  });
});

describe("complete table attribute result inspection", () => {
  it("offers full parsed source, after snapshot, filtered native artifact and exact response", () => {
    const html = renderToStaticMarkup(createElement(TableAttributesFilterInspection, {run: run()}));
    for (const label of ["Filtered table attributes", "Download complete parsed source table", "Download source after processing", "Download filtered native table", "Download attributes response", "The complete source table was preserved", "literal selected role"]) expect(html).toContain(label);
    expect(html).not.toContain("Download complete typed attribute counts");
  });
  it("identifies count results as typed projections with no filtered-table download", () => {
    const html = renderToStaticMarkup(createElement(TableAttributesFilterInspection, {run: run({operation: "counts", result_kind: "counts", filtered_table_json: null, counts_table_json: '{"typed":"count map"}', result: {rows: 6, columns: 2}})}));
    expect(html).toContain("Complete attribute counts"); expect(html).toContain("typed value/count pairs in a native table projection");
    expect(html).toContain("Dictionary entry order is not asserted"); expect(html).toContain("Download complete typed attribute counts");
    expect(html).not.toContain("Download filtered native table");
  });
  it("renders exact failed phase/class/message while retaining available source evidence only", () => {
    const html = renderToStaticMarkup(createElement(TableAttributesFilterInspection, {run: run({http_status: 400, phase: "collect", apply_returned: true, result: null, result_kind: null, error_type: "InvalidOperationError", error: "full diagnostic Ω\0tail"})}));
    expect(html).toContain("Attribute operation could not be completed"); expect(html).toContain("InvalidOperationError"); expect(html).toContain("full diagnostic Ω");
    expect(html).toContain("stage: collect"); expect(html).toContain("apply returned: yes"); expect(html).toContain("Download complete parsed source table");
    expect(html).not.toContain("Download filtered native table"); expect(html).not.toContain("Download complete typed attribute counts");
  });
});
