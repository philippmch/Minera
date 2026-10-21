import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {afterEach, describe, expect, it, vi} from "vitest";
import {ReferenceLogFilterInspection, ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {OriginalLogQueryWorkspace} from "../src/components/OriginalLogQueryWorkspace";
import {defaultReferenceLogFilterForm, type ReferenceLogFilterEnvelope, type ReferenceLogFilterRun} from "../src/reference-log-filters";
import {nativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
const display = (node: Parameters<typeof renderToStaticMarkup>[0]) => renderToStaticMarkup(node);
afterEach(() => vi.unstubAllGlobals());
function result(): ReferenceLogFilterRun {
  const value: ReferenceLogFilterEnvelope = {family: "REFERENCE_LOG_FILTER", operation: "rework", http_status: 200, phase: "complete", source_format: "csv", source: {traces: 8, events: 40}, result: {traces: 2, events: 6}, source_unchanged: true, activity_key: "activity", filtered_log_json: '{"traces":[]}', filtered_xes: '<log/>', xes_error: null, error: null};
  const raw = nativeJson(value); return {value, raw, responseBytes: nativeStringBytes(raw), fields: [], sourceFile: new File([], "source.csv")};
}
describe("filter workspace SSR controls and results (no browser event/effect execution)", () => {
  it("renders source then criteria, all seven operations, and empty-label guidance without eager requests", () => {
    const fetch = vi.fn(); vi.stubGlobal("fetch", fetch); const html = display(createElement(ReferenceLogFilterWorkspace));
    for (const label of ["Source", "Criteria", "Event log file", "Native log JSON", "Exact activity label", "Empty labels are valid", "Minimum occurrences", "Apply log filter"]) expect(html).toContain(label);
    for (const value of ["traces", "prefix", "suffix", "start", "end", "rework", "repetition"]) expect(html).toContain(`value="${value}"`);
    expect(html.indexOf("<legend>Source")).toBeLessThan(html.indexOf("<legend>Criteria")); expect(fetch).not.toHaveBeenCalled();
    expect(html).toContain("Choose a source file to apply this filter"); expect(html).toContain('disabled=""');
    expect(html).toContain('accept=".csv,.xes,.xes.gz,.json"'); expect(html).not.toContain(".tsv");
  });
  it("renders typed repetition, both inclusive bounds and a literal multiline key without unrelated controls", () => {
    const initialForm = defaultReferenceLogFilterForm("repetition"); initialForm.activityKeyMode = "custom"; initialForm.activityKey = " channel\n";
    const html = display(createElement(ReferenceLogFilterWorkspace, {initialForm}));
    for (const text of ["Counted attribute", "Exact typed value as JSON", "Minimum repetitions", "Maximum repetitions", "Both bounds are inclusive", "Missing attributes do not count", "Zero counts and empty cases can match", "frozenset", "9007199254740993"]) expect(html).toContain(text);
    expect(html).toContain('rows="2"'); expect(html).toContain(" channel\n");
    for (const text of ["Exact activity label", "Minimum occurrences", "Keep matching cases", "First occurrence"]) expect(html).not.toContain(text);
    const run = result(); run.value.operation = "repetition"; run.value.filter_attribute_key = "channel";
    const inspected = display(createElement(ReferenceLogFilterInspection, {run}));
    expect(inspected).toContain("Counted attribute: channel"); expect(inspected).toContain("activity attribute: activity");
  });
  it("shows strict and first/last controls only for prefix/suffix", () => {
    for (const operation of ["prefix", "suffix"] as const) {
      const html = display(createElement(ReferenceLogFilterWorkspace, {initialOperation: operation}));
      expect(html).toContain("First occurrence"); expect(html).toContain("Last occurrence"); expect(html).toContain("Exclude the activity itself"); expect(html).toContain("Include the activity itself");
      expect(html).not.toContain("Minimum occurrences"); expect(html).not.toContain("Keep matching cases");
    }
  });
  it("explains literal arrays, comma-containing labels, and sequence wildcards", () => {
    expect(display(createElement(ReferenceLogFilterWorkspace, {initialOperation: "traces"}))).toContain("Activity sequences as JSON");
    const html = display(createElement(ReferenceLogFilterWorkspace, {initialOperation: "start"})); expect(html).toContain("Commas inside a quoted label"); expect(html).toContain("Empty traces are omitted"); expect(html).not.toContain("Exact activity label");
  });
  it("renders explicit empty key input and advanced CSV selectors without trimming source settings", () => {
    const initialForm = defaultReferenceLogFilterForm(); initialForm.activityKeyMode = "custom"; initialForm.csvOptions = true; initialForm.csv.activity_column = "";
    const html = display(createElement(ReferenceLogFilterWorkspace, {initialForm}));
    for (const label of ["Exact activity key", "including an empty key", "Case ID column", "Activity column", "Timestamp column", "Delimiter", "Trim CSV field whitespace"]) expect(html).toContain(label);
  });
  it("preserves the six original query operations and adds an entry button", () => {
    const html = display(createElement(OriginalLogQueryWorkspace)); expect(html).toContain("Filter a CSV, XES, or native log");
    for (const value of ["log_query_table", "query_log_sqlite", "filter_query_events", "log_query_json", "log_query_table_json", "sqlite_log_query_available"]) expect(html).toContain(`value="${value}"`);
  });
  it("renders source/result counts and enables original artifact download/open actions", () => {
    const html = display(createElement(ReferenceLogFilterInspection, {run: result(), onOpenLog: vi.fn()}));
    for (const text of ["Source and result", "Cases", "Events", "Download native log JSON", "Download filtered XES", "Open filtered log", "The source log was preserved"]) expect(html).toContain(text);
    expect(html).not.toContain('disabled=""');
  });
  it("keeps native JSON available and disables XES/open when optional export fails", () => {
    const run = result(); run.value.filtered_xes = null; run.value.xes_error = "unsupported typed event value";
    const html = display(createElement(ReferenceLogFilterInspection, {run, onOpenLog: vi.fn()})); expect(html).toContain("unsupported typed event value");
    expect(html).toContain('disabled="">Download filtered XES'); expect(html).toContain('disabled="">Open filtered log'); expect(html).not.toContain('disabled="">Download native log JSON');
  });
  it("labels native-log XES as a projection and explains retained JSON versus projected types", () => {
    const run = result(); run.value.source_format = "native_log";
    const html = display(createElement(ReferenceLogFilterInspection, {run, onOpenLog: vi.fn()}));
    expect(html).toContain("XES can project bytes, tuples, and sets to text");
    expect(html).toContain("Native log JSON preserves all stored types and values");
    expect(html).toContain("Open XES projection"); expect(html).not.toContain("Open filtered log");
    for (const format of ["csv", "xes"]) {
      run.value.source_format = format;
      const ordinary = display(createElement(ReferenceLogFilterInspection, {run, onOpenLog: vi.fn()}));
      expect(ordinary).toContain("Open filtered log"); expect(ordinary).not.toContain("Open XES projection");
    }
  });
  it("renders primary errors as alerts and distinguishes a successful empty result", () => {
    const run = result(); run.value.result = {traces: 0, events: 0}; expect(display(createElement(ReferenceLogFilterInspection, {run}))).toContain("No cases met this selection");
    run.value.http_status = 400; run.value.error = "The selected activity key is missing";
    const html = display(createElement(ReferenceLogFilterInspection, {run})); expect(html).toContain('role="alert"'); expect(html).toContain("Filter could not be applied"); expect(html).toContain('disabled="">Open filtered log');
  });
});
