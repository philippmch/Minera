import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {afterEach, describe, expect, it, vi} from "vitest";
import {ocelFilterQuery, requestOcelFilter} from "../src/api";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {isOcelFilterResult} from "../src/types";
import {DEFAULT_OCEL_FILTER, OCEL_FILTERS, ocelPreparationFilterQuery, preparedOcelFile,
  requestOcelPreparation, type OcelFilterForm} from "../src/ocel-preparation";
import {OcelExactFilterControls, OcelPreparationWorkspace} from "../src/components/OcelPreparationWorkspace";
import {classifyMiningJson} from "../src/active-mining-source";
import {isOcelJsonDocument} from "../src/app-logic";

const exactVariants = ["event_attribute_exact", "object_attribute_exact", "timestamp_exact", "object_count_exact"] as const;
const classic = '{"ocel:events":{"e1":{"ocel:activity":"A","ocel:timestamp":"2026-01-01T00:00:00.000000001Z","ocel:omap":["o1","o1"],"ocel:typedOmap":[{"ocel:oid":"o1","ocel:qualifier":"first"},{"ocel:oid":"o1","ocel:qualifier":"second"}],"ocel:vmap":{"n":9223372036854775807}}},"ocel:objects":{"o1":{"ocel:type":"Item","ocel:ovmap":{},"ocel:o2o":[]}},"ocel:objectChanges":[],"ocel:e2e":[]}';
const openLog = '{"ocelVersion":"2.0","objects":[{"id":"o1","type":"Item","attributes":[{"name":"n","value":9223372036854775807}]}],"events":[{"id":"e1","type":"A","time":"2026-01-01T00:00:00.000000001Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":"first"},{"objectId":"o1","qualifier":"second"}]}]}';
const response = (variant: string) => '{"family":"OCEL_FILTER","variant":' + JSON.stringify(variant.toUpperCase()) + ',"log":' + classic + ',"open_log":' + openLog + '}';
const source = () => new File([openLog], "original-ocel.json", {type: "application/json"});
const display = (node: Parameters<typeof renderToStaticMarkup>[0]) => renderToStaticMarkup(node);
afterEach(() => vi.unstubAllGlobals());

describe("explicit exact OCEL filter protocol", () => {
  it("retains all old selectors and adds the four implemented exact variants", () => {
    const selectors = OCEL_FILTERS.map(([id]) => id);
    for (const old of ["event_attribute", "object_attribute", "object_types", "objects", "events", "timestamp", "activity_type_matching", "object_count", "start_events", "end_events", "activities_object_type", "cc_object", "cc_length", "cc_object_type", "cc_activity"])
      expect(selectors).toContain(old);
    for (const exact of exactVariants) expect(selectors).toContain(exact);
    expect(selectors).toHaveLength(19);
  });
  it("passes exact typed membership JSON and literal empty keys unchanged", () => {
    const raw = '\r\n[{"type":"integer","value":9007199254740993},{"type":"decimal","value":"NaN"},{"type":"timestamp","value":"2026-01-01T00:00:00.000000001+00:00"},{"type":"string","value":" A,B:C;D|E "},{"type":"null"}]\r\n';
    const form: OcelFilterForm = {...DEFAULT_OCEL_FILTER, variant: "event_attribute_exact", values: raw, key: "", columnKind: "timestamp", positive: false, counts: "stale", from: "stale"};
    expect(Object.fromEntries(new URLSearchParams(ocelPreparationFilterQuery(form)))).toEqual({variant: "event_attribute_exact", positive: "false", attribute_key: "", attribute_values_json: raw, column_kind: "timestamp"});
    for (const columnKind of ["auto", "scalar", "timestamp"] as const)
      expect(new URLSearchParams(ocelPreparationFilterQuery({...form, columnKind})).get("column_kind")).toBe(columnKind);
  });
  it("uses the same exact membership controls for objects and keeps literal canonical selectors", () => {
    const values = '[{"type":"integer","value":9007199254740993},{"type":"nat"},{"type":"string","value":" A,B "}]';
    for (const key of ["", "ocel:oid", "ocel:type", " literal attribute "]) {
      const form: OcelFilterForm = {...DEFAULT_OCEL_FILTER, variant: "object_attribute_exact", key, values, columnKind: "scalar", positive: false, level: "999", names: "stale"};
      expect(Object.fromEntries(new URLSearchParams(ocelPreparationFilterQuery(form)))).toEqual({variant: "object_attribute_exact", positive: "false", attribute_key: key, attribute_values_json: values, column_kind: "scalar"});
      expect(new URLSearchParams(ocelFilterQuery({variant: "object_attribute_exact", attribute_key: key, attribute_values_json: values, positive: false})).get("attribute_values_json")).toBe(values);
    }
  });
  it("preserves typed timestamp bound syntax, timezone awareness and all fractional digits", () => {
    const fromJson = '{"type":"string","value":"2026-01-01 00:00:00"}', toJson = '\n{"type":"timestamp","value":"2026-01-01T00:00:00.000000001+01:00"}\n';
    const query = new URLSearchParams(ocelPreparationFilterQuery({...DEFAULT_OCEL_FILTER, variant: "timestamp_exact", timestampKey: " time\r\n ", fromJson, toJson, positive: false}));
    expect(Object.fromEntries(query)).toEqual({variant: "timestamp_exact", timestamp_key: " time\r\n ", from_json: fromJson, to_json: toJson});
    expect(query.has("from")).toBe(false); expect(query.has("positive")).toBe(false);
  });
  it("transports signed arbitrary integers, decimal/special thresholds and empty type names without parsing, sorting or clamping", () => {
    const counts = '\r\n{"Z":-999999999999999999999999999999999999999999,"A":1.5,"":9007199254740993,"x,y:z":"NaN","inf":"INF","neg":"-INF"}\r\n';
    const query = new URLSearchParams(ocelPreparationFilterQuery({...DEFAULT_OCEL_FILTER, variant: "object_count_exact", counts, positive: false, level: "999"}));
    expect(Object.fromEntries(query)).toEqual({variant: "object_count_exact", minimum_counts_json: counts});
    expect(new URLSearchParams(ocelPreparationFilterQuery({...DEFAULT_OCEL_FILTER, variant: "object_count_exact", counts: "not JSON"})).get("minimum_counts_json")).toBe("not JSON");
  });
  it("extends the public API options without changing convenience serialization", () => {
    const values = '[{"type":"integer","value":9223372036854775807}]', count = '{"":184467440737095516150000,"A":-1,"B":"NaN"}';
    const query = new URLSearchParams(ocelFilterQuery({variant: "event_attribute_exact", attribute_key: "", attribute_values_json: values, column_kind: "scalar", positive: false}));
    expect(query.get("attribute_values_json")).toBe(values); expect(query.get("attribute_key")).toBe(""); expect(query.get("column_kind")).toBe("scalar");
    expect(new URLSearchParams(ocelFilterQuery({variant: "object_count_exact", minimum_counts_json: count})).get("minimum_counts_json")).toBe(count);
    expect(ocelFilterQuery({variant: "object_count", minimum_counts: {item: 2}})).toBe("variant=object_count&minimum_counts=item%3A2");
  });
});

describe("same-workspace exact OCEL results with unit-only HTTP doubles", () => {
  it("keeps marked literal empty names in same-run standard OCEL2 download/open/reupload Files", async () => {
    const literalClassic = '{"ocel:events":{"":{"ocel:activity":"","ocel:timestamp":"2026-01-01T00:00:00.000000001Z","ocel:omap":["",""],"ocel:typedOmap":[{"ocel:oid":"","ocel:qualifier":"first"},{"ocel:oid":"","ocel:qualifier":"second"}],"ocel:vmap":{"":9223372036854775807}}},"ocel:objects":{"":{"ocel:type":"","ocel:ovmap":{"":9223372036854775807},"ocel:o2o":[]}},"ocel:objectChanges":[],"ocel:e2e":[],"minera:literalNames":true}';
    const literalOpen = '{"ocelVersion":"2.0","objectTypes":[{"name":"","attributes":[{"name":"","type":"int"}]}],"eventTypes":[{"name":"","attributes":[{"name":"","type":"int"}]}],"events":[{"id":"","type":"","time":"2026-01-01T00:00:00.000000001Z","attributes":[{"name":"","value":9223372036854775807}],"relationships":[{"objectId":"","qualifier":"first"},{"objectId":"","qualifier":"second"}]}],"objects":[{"id":"","type":"","attributes":[{"name":"","time":"1970-01-01T00:00:00Z","value":9223372036854775807}],"relationships":[]}],"minera:literalNames":true}';
    const raw = '{"family":"OCEL_FILTER","variant":"EVENT_ATTRIBUTE_EXACT","log":' + literalClassic + ',"open_log":' + literalOpen + '}';
    const fetch = vi.fn(async () => new Response(raw)); vi.stubGlobal("fetch", fetch);
    const query = ocelPreparationFilterQuery({...DEFAULT_OCEL_FILTER, variant: "event_attribute_exact", key: "", values: '[{"type":"integer","value":9223372036854775807}]'});
    const run = await requestOcelPreparation("filter", source(), query);
    expect(run.outputs[0].log["minera:literalNames"]).toBe(true);
    expect(run.outputs[0].tables.events.rowAt(0).slice(0, 3)).toEqual([0, "", ""]);
    expect(run.outputs[0].tables.objects.rowAt(0).slice(0, 3)).toEqual([0, "", ""]);
    expect(run.outputs[0].tables.relations.count).toBe(2);
    const file = preparedOcelFile(run), text = await file.text();
    expect(text).toBe(literalOpen); expect(classifyMiningJson(text)).toEqual({kind: "ocel"});
    const opened = parseNativeJson(text) as {"minera:literalNames": boolean; events: Array<{id: string; type: string; attributes: Array<{name: string}>; relationships: unknown[]}>};
    expect(isOcelJsonDocument(opened)).toBe(true); expect(opened["minera:literalNames"]).toBe(true);
    expect(opened.events[0]).toMatchObject({id: "", type: "", attributes: [{name: ""}]});
    expect(opened.events[0].relationships).toHaveLength(2);
    await requestOcelPreparation("filter", file, query);
    const [, next] = fetch.mock.calls[1] as unknown as [string, RequestInit];
    expect(await ((next.body as FormData).get("log") as File).text()).toBe(literalOpen);
  });
  it("guards each canonical exact response and rejects unimplemented variants", () => {
    for (const variant of exactVariants) expect(isOcelFilterResult(parseNativeJson(response(variant)))).toBe(true);
    expect(isOcelFilterResult(parseNativeJson(response("unsupported_exact")))).toBe(false);
    expect(isOcelFilterResult({family: "OCEL_FILTER", variant: "TIMESTAMP_EXACT", log: {}})).toBe(false);
  });
  it("uploads the original OCEL and preserves complete openable/downloadable same-run results for all four selectors", async () => {
    for (const variant of exactVariants) {
      const file = source(), raw = response(variant), fetch = vi.fn(async () => new Response(raw, {status: 200})); vi.stubGlobal("fetch", fetch);
      const form = {...DEFAULT_OCEL_FILTER, variant, values: '[{"type":"integer","value":9007199254740993}]', counts: '{"":-1,"Item":9007199254740993}'};
      const query = ocelPreparationFilterQuery(form), run = await requestOcelPreparation("filter", file, query);
      const [url, init] = fetch.mock.calls[0] as unknown as [string, RequestInit], sent = new URLSearchParams(url.split("?")[1]);
      expect(sent.get("include_open_source")).toBe("true"); expect(sent.get("variant")).toBe(variant);
      if (variant === "event_attribute_exact" || variant === "object_attribute_exact") expect(sent.get("attribute_values_json")).toBe(form.values);
      if (variant === "object_count_exact") expect(sent.get("minimum_counts_json")).toBe(form.counts);
      expect(await ((init.body as FormData).get("log") as File).text()).toBe(await file.text());
      expect(run.raw).toBe(raw); expect(run.report_json).toContain('"response":' + raw);
      expect(run.outputs).toHaveLength(1); expect(run.outputs[0].tables.relations.count).toBe(2);
      const prepared = preparedOcelFile(run); expect(prepared).toBeInstanceOf(File);
      expect(await prepared.text()).toBe(openLog); expect(await prepared.text()).toContain("9223372036854775807");
      expect((parseNativeJson(await prepared.text()) as {events: Array<{relationships: unknown[]}>}).events[0].relationships).toHaveLength(2);
      expect(run.source.sha256).toMatch(/^[a-f0-9]{64}$/);
    }
  });
  it("parses actual public API response text with exact integer tokens instead of response.json rounding", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(response("event_attribute_exact"))));
    const result = await requestOcelFilter(source(), {variant: "event_attribute_exact", attribute_key: "n", attribute_values_json: '[{"type":"integer","value":9223372036854775807}]'});
    const events = result.log["ocel:events"] as Record<string, Record<string, unknown>>, attributes = events.e1["ocel:vmap"] as Record<string, unknown>;
    expect(String(nativeCell(attributes, "n"))).toBe("9223372036854775807"); expect(nativeJson(result)).toContain("9223372036854775807");
  });
  it("retains native validation errors and refuses foreign response variants or missing opening sources", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"error":"timestamp bound is invalid"}', {status: 400})));
    await expect(requestOcelPreparation("filter", source(), "variant=timestamp_exact&from_json=bad")).rejects.toThrow("timestamp bound is invalid");
    vi.stubGlobal("fetch", vi.fn(async () => new Response(response("object_count_exact"))));
    await expect(requestOcelPreparation("filter", source(), "variant=timestamp_exact")).rejects.toThrow(/different/);
    const missing = response("timestamp_exact").replace(',"open_log":' + openLog, "");
    vi.stubGlobal("fetch", vi.fn(async () => new Response(missing)));
    await expect(requestOcelPreparation("filter", source(), "variant=timestamp_exact")).rejects.toThrow(/opening|source/);
  });
});

describe("existing OCEL preparation UI exact criteria (SSR only)", () => {
  it("keeps exact selectors in the existing preparation workspace", () => {
    const html = display(createElement(OcelPreparationWorkspace, {file: source(), busy: false, onOpen: vi.fn()}));
    for (const variant of exactVariants) expect(html).toContain('value="' + variant + '"');
    expect(html).toContain("Prepare object-centric data");
  });
  it("explains scalar versus timestamp membership and retains literal JSON in the editor", () => {
    const filter: OcelFilterForm = {...DEFAULT_OCEL_FILTER, variant: "event_attribute_exact", values: '[{"type":"integer","value":9007199254740993}]'};
    const html = display(createElement(OcelExactFilterControls, {filter, onChange: vi.fn()}));
    expect(html).toContain("Event column interpretation"); expect(html).toContain("Timestamp entries are parsed instants"); expect(html).toContain("9007199254740993");
    for (const kind of ["auto", "scalar", "timestamp"]) expect(html).toContain('value="' + kind + '"');
  });
  it("shows exact object interpretation, canonical keys and unused-object retention", () => {
    const html = display(createElement(OcelExactFilterControls, {filter: {...DEFAULT_OCEL_FILTER, variant: "object_attribute_exact", values: '[{"type":"integer","value":9007199254740993},{"type":"nat"}]'}, onChange: vi.fn()}));
    expect(html).toContain("Literal object attribute name"); expect(html).toContain("Object column interpretation");
    expect(html).toContain("ocel:oid"); expect(html).toContain("ocel:type"); expect(html).toContain("Matching unused objects remain"); expect(html).toContain("9007199254740993");
    expect(html).toContain("type&quot;:&quot;nat"); expect(html).toContain("missing datetime cells");
  });
  it("explains required bounds and UTC reinterpretation of the local clock", () => {
    const html = display(createElement(OcelExactFilterControls, {filter: {...DEFAULT_OCEL_FILTER, variant: "timestamp_exact"}, onChange: vi.fn()}));
    expect(html).toContain("From bound (typed JSON)"); expect(html).toContain("Both bounds are inclusive and required"); expect(html).toContain("Source timestamps must be timezone-aware");
    expect(html).toContain("interpret the local clock as UTC"); expect(html).toContain("offsets are replaced without converting the clock");
    expect(html).toContain("Fractional digits are retained"); expect(html).not.toContain("timezone awareness are retained");
    expect(html).not.toContain("unbounded");
  });
  it("explains repeated-relation counts and the required-type condition at zero/negative thresholds", () => {
    const html = display(createElement(OcelExactFilterControls, {filter: {...DEFAULT_OCEL_FILTER, variant: "object_count_exact"}, onChange: vi.fn()}));
    expect(html).toContain("Repeated event-object relationships each contribute"); expect(html).toContain("A required type must occur"); expect(html).toContain("complete precision");
  });
});
