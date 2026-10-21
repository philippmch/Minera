import {afterEach, describe, expect, it, vi} from "vitest";
import {defaultTableAttributesForm, isTableAttributesEnvelope, requestTableAttributes, tableAttributesArtifactFile,
  tableAttributesFields, tableAttributesOperations, tableAttributesResponseFile, tableAttributesSourceExample,
  TableAttributesRequestGate, type TableAttributesEnvelope, type TableAttributesForm, type TableAttributesRun} from "../src/table-attributes-filters";

const source = () => new File(['{"columns":[],"rows":[]}'], "complete-source.json", {lastModified: 1234});
function envelope(patch: Partial<TableAttributesEnvelope> = {}): TableAttributesEnvelope {
  return {family: "TABLE_ATTRIBUTES_FILTER", engine: "polars", operation: "traces", http_status: 200,
    phase: "complete", apply_returned: true, source: {rows: 12, columns: 9}, result: {rows: 5, columns: 9},
    source_unchanged: true, case_id_key: "case:concept:name", attribute_key: "concept:name",
    source_table_json: '{"full":"source\\u0000Ω"}', source_after_table_json: '{"full":"after"}',
    filtered_table_json: '{"large":9007199254740993,"unsigned":18446744073709551615,"negative_zero":-0.0}',
    counts_table_json: null, result_kind: "table", error_type: null, error: null, ...patch};
}
function run(value = envelope()): TableAttributesRun {
  const raw = JSON.stringify(value); return {value, raw, responseBytes: new TextEncoder().encode(raw), sourceFile: source(), fields: []};
}
const fields = (patch: Partial<TableAttributesForm> = {}, engine: "pandas" | "polars" = "polars") =>
  Object.fromEntries(tableAttributesFields({...defaultTableAttributesForm(engine), ...patch}));
afterEach(() => vi.unstubAllGlobals());

describe("attribute controls preserve actual engine semantics", () => {
  it("offers all nine Polars modes and eight pandas modes with genuine omission defaults", () => {
    expect(tableAttributesOperations("polars")).toEqual(["traces", "events", "numeric_traces", "numeric_events", "threshold", "top_activities", "on_values", "relative", "counts"]);
    expect(tableAttributesOperations("pandas")).not.toContain("counts");
    expect(fields()).toEqual({engine: "polars", operation: "traces", values_json: '["A"]', positive: "true"});
    expect(fields({operation: "relative"})).not.toHaveProperty("keep_once_per_case");
    expect(fields({}, "pandas")).toHaveProperty("keep_nan_values", "false");
  });
  it("keeps typed membership, exact bound tokens and literal column names byte-for-byte", () => {
    const typed = '{"type":"list","value":[{"type":"integer","value":"9007199254740993"},{"type":"null"}]}';
    expect(fields({valuesKind: "typed", valuesJson: typed, caseKeyMode: "custom", caseKey: "", attributeKeyMode: "custom", attributeKey: " role\nΩ\0 "})).toMatchObject({typed_values_json: typed, case_id_key: "", attribute_key: " role\nΩ\0 "});
    const interval = fields({operation: "numeric_events", lower: "9007199254740993", upper: "18446744073709551615", valuesKind: "typed", valuesJson: typed, caseKeyMode: "custom", caseKey: "inactive"});
    expect(interval).toMatchObject({lower: "9007199254740993", upper: "18446744073709551615"});
    for (const inactive of ["case_id_key", "values_json", "typed_values_json"]) expect(interval).not.toHaveProperty(inactive);
  });
  it("retains omitted versus explicit false/true relative denominators and case-role activation", () => {
    const base = {operation: "relative" as const, caseKeyMode: "custom" as const, caseKey: " literal case\0 ", minRelative: "0.30000000000000004"};
    expect(fields(base)).toMatchObject({case_id_key: base.caseKey, criterion: base.minRelative});
    expect(fields({...base, keepOnceMode: "false"})).toMatchObject({keep_once_per_case: "false"});
    expect(fields({...base, keepOnceMode: "false"})).not.toHaveProperty("case_id_key");
    expect(fields({...base, keepOnceMode: "true"})).toMatchObject({case_id_key: base.caseKey, keep_once_per_case: "true"});
    expect(fields({...base, operation: "counts"})).not.toHaveProperty("case_id_key");
    expect(fields({...base, operation: "counts", keepOnceMode: "true"})).toHaveProperty("case_id_key", base.caseKey);
  });
  it("sends complete active stream pairs only for numeric trace filtering", () => {
    const patch = {streamOne: true, streamOneKey: " work\n\0 ", streamOneValueJson: '{"type":"null"}', streamTwo: true, streamTwoKey: "", streamTwoValueJson: '{"type":"integer","value":"9007199254740993"}'};
    expect(fields({...patch, operation: "numeric_traces"})).toMatchObject({stream_filter_key1: patch.streamOneKey, stream_filter_value1_json: patch.streamOneValueJson, stream_filter_key2: "", stream_filter_value2_json: patch.streamTwoValueJson});
    for (const operation of ["events", "numeric_events", "relative", "counts", "threshold"] as const) expect(Object.keys(fields({...patch, operation})).some(key => key.startsWith("stream_filter_"))).toBe(false);
  });
  it("uses activity roles and supplied counters only in their direct helper modes", () => {
    const counter = '{"type":"list","value":[{"type":"tuple","value":[{"type":"integer","value":"9007199254740993"},{"type":"integer","value":"2"}]}]}';
    const patch = {activityKeyMode: "custom" as const, activityKey: "", attributeKeyMode: "custom" as const, attributeKey: "inactive", caseKeyMode: "custom" as const, caseKey: "inactive", countsMode: "supplied" as const, countsJson: counter, mostCommonMode: "supplied" as const, mostCommonJson: '{"type":"null"}', positive: false, keepMissing: true};
    expect(fields({...patch, operation: "threshold"}, "pandas")).toEqual({engine: "pandas", operation: "threshold", criterion: "2", activity_key: "", counts_json: counter, most_common_json: patch.mostCommonJson});
    expect(fields({...patch, operation: "top_activities", maxActivities: "9223372036854775808"})).toEqual({engine: "polars", operation: "top_activities", criterion: "9223372036854775808", activity_key: ""});
    expect(fields({...patch, operation: "threshold", countsMode: "value", countsJson: '{"type":"null"}'})).toHaveProperty("counts_value_json", '{"type":"null"}');
    expect(fields({...patch, operation: "events"})).not.toHaveProperty("counts_json");
  });
  it("rejects unsupported engines/modes and malformed active UI discriminants", () => {
    expect(() => fields({operation: "counts"}, "pandas")).toThrow("available");
    for (const patch of [{engine: "other"}, {caseKeyMode: "other"}, {valuesKind: "other"}, {operation: "relative", keepOnceMode: "other"}, {operation: "threshold", countsMode: "other"}])
      expect(() => tableAttributesFields({...defaultTableAttributesForm("polars"), ...patch} as TableAttributesForm)).toThrow();
  });
});

describe("complete native envelopes, requests and artifacts", () => {
  it("requires the full fixed envelope shape and valid typed artifact strings/counts", () => {
    expect(isTableAttributesEnvelope(envelope())).toBe(true);
    expect(isTableAttributesEnvelope(envelope({operation: "counts", result_kind: "counts", filtered_table_json: null, counts_table_json: '{"typed":"counts"}'}))).toBe(true);
    for (const patch of [{extra: true}, {operation: "wrong"}, {result_kind: "mapping"}, {counts_table_json: 1}, {source_after_table_json: {}}, {source: {rows: -1, columns: 1}}, {result: {rows: 1.5, columns: 2}}, {apply_returned: "yes"}, {case_id_key: null}, {http_status: 500}]) expect(isTableAttributesEnvelope({...envelope(), ...patch})).toBe(false);
    const {source_after_table_json: _after, ...incomplete} = envelope(); expect(isTableAttributesEnvelope(incomplete)).toBe(false);
    expect(isTableAttributesEnvelope(envelope({http_status: 400, engine: null, operation: null, result: null, result_kind: null, phase: "apply", error_type: "TypeError", error: "complete\0diagnostic"}))).toBe(true);
  });
  it("uploads an actual File and preserves binary-safe literal fields and request identity", async () => {
    const file = source(), form = {...defaultTableAttributesForm("polars"), attributeKeyMode: "custom" as const, attributeKey: " exact\r\nΩ\0 "};
    const fetch = vi.fn(async (url: string, init: RequestInit) => {
      expect(url).toBe("/api/reference-table-attributes"); expect(init.method).toBe("POST");
      const body = init.body as FormData, uploaded = body.get("source_file") as File;
      expect(uploaded).toBeInstanceOf(File); expect(uploaded.name).toBe(file.name); expect(await uploaded.text()).toBe(await file.text());
      expect(await (body.get("attribute_key") as Blob).text()).toBe(form.attributeKey); expect(body.has("keep_once_per_case")).toBe(false);
      return new Response(JSON.stringify(envelope()));
    }); vi.stubGlobal("fetch", fetch);
    const value = await requestTableAttributes(file, form); expect(value.sourceFile).toBe(file); expect(fetch).toHaveBeenCalledOnce();
    await expect(requestTableAttributes({name: "fake.json"} as File, form)).rejects.toThrow("actual native"); expect(fetch).toHaveBeenCalledOnce();
  });
  it("accepts complete native 400 diagnostics while rejecting response status/engine/mode mismatches", async () => {
    let value = envelope({http_status: 400, phase: "collect", result: null, error_type: "ColumnNotFoundError", error: "literal\0failure"});
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(value), {status: value.http_status})));
    expect((await requestTableAttributes(source(), defaultTableAttributesForm("polars"))).value.error).toBe("literal\0failure");
    value = envelope({engine: "pandas"}); await expect(requestTableAttributes(source(), defaultTableAttributesForm("polars"))).rejects.toThrow("another engine");
    value = envelope({operation: "counts"}); await expect(requestTableAttributes(source(), defaultTableAttributesForm("polars"))).rejects.toThrow("another engine");
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(envelope()), {status: 400})));
    await expect(requestTableAttributes(source(), defaultTableAttributesForm("polars"))).rejects.toThrow("invalid native envelope");
  });
  it("rejects malformed UTF-8, invalid envelopes and unexpected HTTP statuses", async () => {
    for (const response of [new Response(Uint8Array.of(255)), new Response('{"family":"TABLE_ATTRIBUTES_FILTER"}'), new Response("down", {status: 503})]) {
      vi.stubGlobal("fetch", vi.fn(async () => response)); await expect(requestTableAttributes(source(), defaultTableAttributesForm("polars"))).rejects.toThrow();
    }
  });
  it("cancels before upload and rejects a response arriving after abort", async () => {
    const controller = new AbortController(); controller.abort(); const fetch = vi.fn(); vi.stubGlobal("fetch", fetch);
    await expect(requestTableAttributes(source(), defaultTableAttributesForm("polars"), controller.signal)).rejects.toMatchObject({name: "AbortError"}); expect(fetch).not.toHaveBeenCalled();
    const late = new AbortController(); vi.stubGlobal("fetch", vi.fn(async () => {late.abort(); return new Response(JSON.stringify(envelope()));}));
    await expect(requestTableAttributes(source(), defaultTableAttributesForm("polars"), late.signal)).rejects.toMatchObject({name: "AbortError"});
  });
  it("exports full source/before/after/result bytes and keeps counts a separate typed projection", async () => {
    const result = run();
    for (const kind of ["source", "source_after", "result"] as const) expect(await tableAttributesArtifactFile(result, kind).text()).toBe(kind === "source" ? result.value.source_table_json : kind === "source_after" ? result.value.source_after_table_json : result.value.filtered_table_json);
    expect(new Uint8Array(await tableAttributesResponseFile(result).arrayBuffer())).toEqual(result.responseBytes);
    const counts = run(envelope({operation: "counts", result_kind: "counts", filtered_table_json: null, counts_table_json: '{"rows":[[{"kind":"int","value":"9007199254740993"},{"kind":"int","value":"2"}]]}'}));
    expect(await tableAttributesArtifactFile(counts, "counts").text()).toBe(counts.value.counts_table_json); expect(() => tableAttributesArtifactFile(counts, "result")).toThrow("unavailable");
    expect(() => tableAttributesArtifactFile(result, "counts")).toThrow("unavailable");
    const failed = run(envelope({http_status: 400, error: "failure"})); expect(() => tableAttributesArtifactFile(failed, "result")).toThrow("unavailable");
    expect(await tableAttributesArtifactFile(failed, "source").text()).toBe(failed.value.source_table_json);
  });
  it("downloads native byte envelopes without Unicode replacement", async () => {
    const result = run(envelope({filtered_table_json: {native_bytes: "hex-v1", data: "ff00c328"}}));
    expect(new Uint8Array(await tableAttributesArtifactFile(result, "result").arrayBuffer())).toEqual(Uint8Array.of(255, 0, 195, 40));
  });
  it("invalidates stale requests on any control edit, file identity change, cancellation or newer request", () => {
    const gate = new TableAttributesRequestGate(), form = defaultTableAttributesForm("polars"), identity = {source: source(), form}, ticket = gate.issue(identity);
    expect(gate.accepts(ticket, identity)).toBe(true);
    for (const key of Object.keys(form) as Array<keyof TableAttributesForm>) {
      const old = form[key], changed = typeof old === "boolean" ? !old : `${old}\0changed`;
      expect(gate.accepts(ticket, {...identity, form: {...form, [key]: changed} as TableAttributesForm})).toBe(false);
    }
    expect(gate.accepts(ticket, {...identity, source: source()})).toBe(false);
    const controller = new AbortController(); controller.abort(); expect(gate.accepts(ticket, identity, controller.signal)).toBe(false);
    gate.issue(identity); expect(gate.accepts(ticket, identity)).toBe(false); const fresh = gate.issue(identity); gate.invalidate(); expect(gate.accepts(fresh, identity)).toBe(false);
  });
  it("provides a complete real typed table example for either engine", async () => {
    for (const engine of ["pandas", "polars"] as const) {
      const file = tableAttributesSourceExample(engine), value = JSON.parse(await file.text());
      expect(file).toBeInstanceOf(File); expect(file.name).toBe(`${engine}-attributes-example.json`); expect(value.rows).toHaveLength(4);
      expect(value.attributes).not.toHaveLength(0); expect(value.rows[0][2].typed_value).toBe("9007199254740993"); expect(value.rows[0][3].typed_value).toBe("18446744073709551615");
      expect(value.dtypes).toEqual(engine === "polars" ? ["String", "String", "Int64", "UInt64"] : ["object", "object", "int64", "uint64"]);
    }
  });
});
