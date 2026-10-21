import {afterEach, describe, expect, it, vi} from "vitest";
import {nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {REFERENCE_LOG_FILTER_OPERATIONS, ReferenceLogFilterRequestGate, defaultReferenceLogFilterForm,
  isReferenceLogFilterEnvelope, referenceLogFilterArtifactFile, referenceLogFilterCountText,
  referenceLogFilterFields, referenceLogFilterPickedFile, referenceLogFilterResponseFile,
  referenceLogFilterOpenActivityKey, referenceLogFilterFullBetweenFile, openReferenceLogFilter,
  requestReferenceLogFilter, type ReferenceLogFilterEnvelope, type ReferenceLogFilterForm,
  type ReferenceLogFilterRun} from "../src/reference-log-filters";

afterEach(() => vi.unstubAllGlobals());
const source = () => new File([new Uint8Array([255, 0, 237, 160, 128])], 'original"Ω.json', {lastModified: 42});
const envelope = (operation = "rework"): ReferenceLogFilterEnvelope => ({family: "REFERENCE_LOG_FILTER", operation, http_status: 200,
  phase: "complete", source_format: "native_log", source: {traces: 3, events: 8}, result: {traces: 1, events: 3},
  ...(operation === "repetition" ? {filter_attribute_key: "channel"} : operation === "between" ? {filter_attribute_key: "concept:name", case_id_key: "concept:name", full_result_json: '{"schema":"log-between-result-v1"}'} : {}),
  source_unchanged: true, activity_key: "concept:name", filtered_log_json: '{"traces":[]}', filtered_xes: '<log/>', xes_error: null, error: null});
function reply(value = envelope(), status: number = value.http_status) {
  const fetch = vi.fn(async () => new Response(nativeJson(value), {status})); vi.stubGlobal("fetch", fetch); return fetch;
}
const parts = (fetch: ReturnType<typeof reply>) => (fetch.mock.calls[0] as unknown as [string, RequestInit])[1].body as FormData;
async function field(body: FormData, key: string) {const value = body.get(key); return value instanceof Blob ? new Uint8Array(await value.arrayBuffer()) : nativeStringBytes(value ?? "");}

export function referenceFilterRun(value = envelope()): ReferenceLogFilterRun {const raw = nativeJson(value); return {value, raw, responseBytes: nativeStringBytes(raw), sourceFile: source(), fields: []};}

describe("literal reference-log filter controls", () => {
  it("sends only the criteria for each operation and keeps native defaults distinct from explicit empty keys", () => {
    for (const operation of REFERENCE_LOG_FILTER_OPERATIONS) {
      const form = defaultReferenceLogFilterForm(operation), fields = Object.fromEntries(referenceLogFilterFields(form));
      expect(fields.operation).toBe(operation); expect(fields).not.toHaveProperty("activity_key");
      expect(fields).not.toHaveProperty("case_id_column");
      if (operation === "prefix" || operation === "suffix") {expect(fields).toMatchObject({activity: "", strict: "true", occurrence: "first"}); expect(fields).not.toHaveProperty("positive");}
      else if (operation === "between") {expect(fields).toMatchObject({act1_json: '"A"', act2_json: '"B"'}); for (const key of ["positive", "activity", "case_id_key", "subcase_concat_str"]) expect(fields).not.toHaveProperty(key);}
      else if (operation === "repetition") {expect(fields).toMatchObject({target_json: form.repetitionTargetJson, min_rep: "2", max_rep: "9223372036854775807"}); expect(fields).not.toHaveProperty("positive"); expect(fields).not.toHaveProperty("activity");}
      else if (operation === "case_size") {expect(fields).toMatchObject({minimum: "2", maximum: "null"}); expect(fields).not.toHaveProperty("positive");}
      else if (operation === "case_count") {expect(fields).toMatchObject({max_cases: "1000"}); expect(fields).not.toHaveProperty("positive");}
      else if (operation === "case_performance") {expect(fields).toMatchObject({minimum: "0", maximum: "10000000000"}); expect(fields).not.toHaveProperty("positive"); expect(fields).not.toHaveProperty("timestamp_key");}
      else if (operation.startsWith("time_")) {expect(fields).toMatchObject({lower: form.timeLower, upper: form.timeUpper}); expect(Object.hasOwn(fields, "positive")).toBe(operation === "time_starting" || operation === "time_completing"); expect(Object.hasOwn(fields, "time_target_json")).toBe(operation === "time_attribute");}
      else if (operation === "variants_cumulative") expect(fields).not.toHaveProperty("positive");
      else expect(fields.positive).toBe("true");
      expect(Object.hasOwn(fields, "patterns_json")).toBe(operation === "traces");
      expect(Object.hasOwn(fields, "admitted_json")).toBe(operation === "start" || operation === "end" || operation === "variants");
      expect(Object.hasOwn(fields, "target_json")).toBe(operation === "repetition");
      expect(Object.hasOwn(fields, "min_occurrences")).toBe(operation === "rework");
      form.activityKeyMode = "custom"; form.activityKey = "";
      if (operation.startsWith("case_")) expect(Object.fromEntries(referenceLogFilterFields(form))).not.toHaveProperty("activity_key");
      else expect(referenceLogFilterFields(form)).toContainEqual(["activity_key", ""]);
    }
  });
  it("preserves exact unsigned/signed/fraction/NaN/INF minimum tokens without Number conversion", () => {
    const form = defaultReferenceLogFilterForm();
    for (const token of ["18446744073709551615", "9007199254740993", "-9223372036854775808", "2.0000000000000004", "5e-324", "NaN", "INF", "-INF", "", "null", '"2"']) {
      form.minOccurrences = token; expect(referenceLogFilterFields(form)).toContainEqual(["min_occurrences", token]);
    }
  });
  it("retains literal JSON arrays rather than splitting comma-containing or empty labels", () => {
    const raw = '\r\n[["A,B", "", "A\\u0000B", "..."]]\n', form = defaultReferenceLogFilterForm("traces");
    form.patternsJson = raw; expect(referenceLogFilterFields(form)).toContainEqual(["patterns_json", raw]);
    form.operation = "start"; form.admittedJson = '["A,B", "", " A "]';
    expect(referenceLogFilterFields(form)).toContainEqual(["admitted_json", form.admittedJson]);
  });
  it("keeps explicit CSV settings literal and omits them for XES/native logs", () => {
    const form = defaultReferenceLogFilterForm(); form.csvOptions = true;
    form.csv = {case_id_column: " case ", activity_column: "", timestamp_column: "time:key", delimiter: "\t"};
    expect(Object.fromEntries(referenceLogFilterFields(form))).toMatchObject({...form.csv, trim_fields: "false"});
    form.trimFields = true; expect(referenceLogFilterFields(form)).toContainEqual(["trim_fields", "true"]);
    for (const format of ["xes", "native_log"] as const) {form.sourceFormat = format; expect(referenceLogFilterFields(form).some(([k]) => k === "delimiter")).toBe(false);}
  });
});

describe("actual multipart request helper with unit-only HTTP doubles", () => {
  it("uploads the actual original File and native string bytes including empty/NUL/CRLF/WTF8", async () => {
    const fetch = reply(), file = source(), form = defaultReferenceLogFilterForm();
    form.activity = "\r\n\0\ud800"; form.activityKeyMode = "custom"; form.activityKey = "";
    const result = await requestReferenceLogFilter(file, form), body = parts(fetch);
    expect((fetch.mock.calls[0] as unknown as [string])[0]).toBe("/api/reference-log-filter");
    expect(body.get("source_file")).toBe(file); expect(result.sourceFile).toBe(file);
    expect(await field(body, "activity")).toEqual(nativeStringBytes(form.activity));
    expect(body.get("activity_key")).toBe(""); expect(result.fields).toContainEqual(["activity", form.activity]);
    expect(new Uint8Array(await (body.get("source_file") as File).arrayBuffer())).toEqual(new Uint8Array(await file.arrayBuffer()));
  });
  it("transports exact typed repetition targets and bounds and opens the original activity", async () => {
    const value = envelope("repetition"); value.activity_key = "work"; value.filter_attribute_key = " channel\n";
    const fetch = reply(value), form = defaultReferenceLogFilterForm("repetition");
    form.activityKeyMode = "custom"; form.activityKey = " channel\n";
    form.repetitionTargetJson = '\r\n{"type":"tuple","value":[{"type":"integer","value":9007199254740993},{"type":"null"}]}\n';
    form.minRep = "-999999999999999999999999999999999999999"; form.maxRep = "2.0000000000000004";
    const run = await requestReferenceLogFilter(source(), form), body = parts(fetch);
    for (const [key, expected] of [["activity_key", form.activityKey], ["target_json", form.repetitionTargetJson], ["min_rep", form.minRep], ["max_rep", form.maxRep]]) expect(await field(body, key)).toEqual(nativeStringBytes(expected));
    for (const key of ["positive", "activity", "min_occurrences", "patterns_json", "admitted_json", "strict"]) expect(body.has(key)).toBe(false);
    expect(run.value.filter_attribute_key).toBe(form.activityKey);
    const open = vi.fn(); openReferenceLogFilter(run, open);
    expect(open.mock.calls[0][1]).toBe("work"); expect(await open.mock.calls[0][0].text()).toBe("<log/>");
    expect(await referenceLogFilterArtifactFile(run, "json").text()).toBe(value.filtered_log_json);
    expect(isReferenceLogFilterEnvelope({...value, filter_attribute_key: undefined})).toBe(false);
    const {filter_attribute_key: ignored, ...missing} = value; void ignored;
    expect(isReferenceLogFilterEnvelope(missing)).toBe(false);
  });
  it("sends scalar/collection between endpoints and explicit empty/NUL case/separator controls without normalization", async () => {
    const value = envelope("between"); value.activity_key = "work"; reply(value); const fetch = reply(value), form = defaultReferenceLogFilterForm("between");
    form.betweenStartJson = '\r\n[" A,B ", "", "A\\u0000B"]\n'; form.betweenEndJson = '\r\n"B"\n';
    form.betweenCaseKeyMode = "custom"; form.betweenCaseKey = ""; form.betweenSeparatorMode = "custom"; form.betweenSeparator = " \r\n\0Ω ";
    const result = await requestReferenceLogFilter(source(), form), body = parts(fetch);
    for (const [key, expected] of [["act1_json", form.betweenStartJson], ["act2_json", form.betweenEndJson], ["case_id_key", ""], ["subcase_concat_str", form.betweenSeparator]]) expect(await field(body, key)).toEqual(nativeStringBytes(expected));
    expect(body.has("positive")).toBe(false);expect(body.has("min_rep")).toBe(false);
    expect(await referenceLogFilterFullBetweenFile(result).text()).toBe(value.full_result_json);
    const open = vi.fn();openReferenceLogFilter(result, open);expect(open.mock.calls[0][1]).toBe("work");
    expect(isReferenceLogFilterEnvelope({...value, full_result_json: undefined})).toBe(false);
    form.betweenSeparator = "";expect(referenceLogFilterFields(form)).toContainEqual(["subcase_concat_str", ""]);
    form.betweenSeparatorMode = "default";expect(Object.fromEntries(referenceLogFilterFields(form))).not.toHaveProperty("subcase_concat_str");
    expect(() => referenceLogFilterFullBetweenFile(referenceFilterRun())).toThrow(/unavailable/);
  });
  it("uses no client-side JSON normalization before native array validation", async () => {
    const fetch = reply(envelope("traces")), form = defaultReferenceLogFilterForm("traces"); form.patternsJson = '\r\n[[" A,B ", ""]]\r\n';
    await requestReferenceLogFilter(source(), form); expect(await field(parts(fetch), "patterns_json")).toEqual(nativeStringBytes(form.patternsJson));
  });
  it("retains structured native 400 errors, source counts, and exact byte carriers", async () => {
    const value = envelope(); value.http_status = 400; value.phase = "filter"; value.result = null; value.filtered_log_json = null; value.filtered_xes = null;
    value.error = {native_bytes: "hex-v1", data: "ff00eda080"}; reply(value);
    const run = await requestReferenceLogFilter(source(), defaultReferenceLogFilterForm());
    expect(run.value.error).toEqual(value.error); expect(run.value.source).toEqual({traces: 3, events: 8});
    expect(new TextDecoder().decode(run.responseBytes)).toBe(run.raw);
  });
  it("accepts repetition source-import failures with nullable counted-key metadata", async () => {
    const value = envelope("repetition"); Object.assign(value, {http_status: 400, phase: "source_import", source: null, result: null, source_unchanged: null, activity_key: null, filter_attribute_key: null, filtered_log_json: null, filtered_xes: null, error: "Invalid source log"});
    reply(value); const run = await requestReferenceLogFilter(source(), defaultReferenceLogFilterForm("repetition"));
    expect(run.value.phase).toBe("source_import"); expect(run.value.filter_attribute_key).toBeNull();
    expect(() => referenceLogFilterArtifactFile(run, "json")).toThrow(/unavailable/);
    expect(isReferenceLogFilterEnvelope({...envelope(), filter_attribute_key: null})).toBe(false);
  });
  it("keeps native JSON downloadable when optional XES export fails", async () => {
    const value = envelope(); value.filtered_xes = null; value.xes_error = "Cannot represent the event value in XES"; reply(value);
    const run = await requestReferenceLogFilter(source(), defaultReferenceLogFilterForm());
    expect(await referenceLogFilterArtifactFile(run, "json").text()).toBe(value.filtered_log_json);
    expect(() => referenceLogFilterArtifactFile(run, "xes")).toThrow(/unavailable/);
  });
  it("rejects malformed/foreign envelopes and HTTP status disagreement", async () => {
    for (const mutate of [(v: ReferenceLogFilterEnvelope) => {delete (v as Partial<ReferenceLogFilterEnvelope>).source_unchanged;},
      (v: ReferenceLogFilterEnvelope) => {v.operation = "prefix";}, (v: ReferenceLogFilterEnvelope) => {v.activity_key = {native_bytes: "hex-v1", data: "f"};}]) {
      const value = envelope(); mutate(value); reply(value); await expect(requestReferenceLogFilter(source(), defaultReferenceLogFilterForm())).rejects.toThrow(/envelope|operation/);
    }
    reply(envelope(), 400); await expect(requestReferenceLogFilter(source(), defaultReferenceLogFilterForm())).rejects.toThrow(/envelope/);
  });
  it("rejects invalid UTF8 and non-JSON bodies rather than accepting replacement text", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(new Uint8Array([255]), {status: 400})));
    await expect(requestReferenceLogFilter(source(), defaultReferenceLogFilterForm())).rejects.toThrow(/safe JSON/);
    vi.stubGlobal("fetch", vi.fn(async () => new Response("not JSON", {status: 400})));
    await expect(requestReferenceLogFilter(source(), defaultReferenceLogFilterForm())).rejects.toThrow(/envelope/);
  });
  it("honors early and late aborts and rejects non-File source uploads", async () => {
    const fetch = reply(), controller = new AbortController(); controller.abort();
    await expect(requestReferenceLogFilter(source(), defaultReferenceLogFilterForm(), controller.signal)).rejects.toThrow(); expect(fetch).not.toHaveBeenCalled();
    const late = new AbortController(); vi.stubGlobal("fetch", vi.fn(async () => {late.abort(); return new Response(nativeJson(envelope()));}));
    await expect(requestReferenceLogFilter(source(), defaultReferenceLogFilterForm(), late.signal)).rejects.toThrow();
    await expect(requestReferenceLogFilter("source" as unknown as File, defaultReferenceLogFilterForm())).rejects.toThrow(/actual/);
  });
});

describe("exact response artifacts and request lifetime", () => {
  it("hands off the actual exported XES File and literal selected activity key", async () => {
    const run = referenceFilterRun(), onOpen = vi.fn();
    for (const key of ["activity", " custom\r\nkey ", "", "\ufeffA"]) {
      run.value.activity_key = key;
      expect(referenceLogFilterOpenActivityKey(run)).toBe(key);
      openReferenceLogFilter(run, onOpen);
      const [file, selectedKey] = onOpen.mock.calls.at(-1)!;
      expect(file).toBeInstanceOf(File); expect(await file.text()).toBe("<log/>");
      expect(selectedKey).toBe(key);
    }
  });
  it("decodes valid native key bytes and omits unavailable/invalid keys without replacement text", () => {
    const run = referenceFilterRun(), onOpen = vi.fn();
    run.value.activity_key = {native_bytes: "hex-v1", data: "206b65790d0a"};
    expect(referenceLogFilterOpenActivityKey(run)).toBe(" key\r\n");
    for (const key of [null, {native_bytes: "hex-v1" as const, data: "ffeda080"}, "\ud800"]) {
      run.value.activity_key = key; expect(referenceLogFilterOpenActivityKey(run)).toBeUndefined();
      openReferenceLogFilter(run, onOpen); expect(onOpen.mock.calls.at(-1)).toHaveLength(1);
    }
  });
  it("creates the actual handoff XES File and native JSON File from authoritative byte carriers", async () => {
    const value = envelope(); value.filtered_xes = {native_bytes: "hex-v1", data: "3c6c6f672f3eff00eda080"}; value.filtered_log_json = '{"large":18446744073709551615}';
    const run = referenceFilterRun(value), xes = referenceLogFilterArtifactFile(run, "xes");
    expect(xes).toBeInstanceOf(File); expect(xes.name).toBe("filtered-rework.xes");
    expect(new Uint8Array(await xes.arrayBuffer())).toEqual(new Uint8Array([60,108,111,103,47,62,255,0,237,160,128]));
    expect(await referenceLogFilterArtifactFile(run, "json").text()).toBe(value.filtered_log_json);
    expect(new Uint8Array(await referenceLogFilterResponseFile(run).arrayBuffer())).toEqual(run.responseBytes);
  });
  it("validates exactly the declared fields and keeps uint64 count tokens intact", () => {
    const raw = nativeJson(envelope()).replace('"events":8', '"events":18446744073709551615'), value = parseNativeJson(raw) as ReferenceLogFilterEnvelope;
    expect(isReferenceLogFilterEnvelope(value)).toBe(true); expect(referenceLogFilterCountText(value.source!, "events")).toBe("18446744073709551615");
    expect(isReferenceLogFilterEnvelope({...value, extra: 1})).toBe(false);
    for (const count of [-1, 1.5, Infinity]) expect(isReferenceLogFilterEnvelope({...envelope(), result: {traces: count, events: 0}})).toBe(false);
  });
  it("invalidates every source/criteria/CSV change and rejects previous requests after edits/unmount", () => {
    const identity = {source: source(), form: defaultReferenceLogFilterForm()}, gate = new ReferenceLogFilterRequestGate(), first = gate.issue(identity);
    expect(gate.accepts(first, {source: identity.source, form: {...identity.form, csv: {...identity.form.csv}}})).toBe(true);
    for (const patch of [{minCaseSize: "3"}, {maxCaseSize: "0"}, {maxCases: "-1"}, {minCaseSeconds: "1"}, {maxCaseSeconds: "NaN"}, {timestampKeyMode: "custom"}, {timestampKey: ""}, {activity: "A"}, {positive: false}, {strict: false}, {occurrence: "last"}, {sourceFormat: "xes"}, {activityKeyMode: "custom"}, {activityKey: "" + "x"}, {minOccurrences: "NaN"}, {minRep: "0"}, {maxRep: "1"}, {betweenStartJson: "[]"}, {betweenEndJson: "[]"}, {betweenCaseKeyMode: "custom"}, {betweenCaseKey: ""}, {betweenSeparatorMode: "custom"}, {betweenSeparator: ""}, {repetitionTargetJson: '{"type":"null"}'}, {admittedJson: "[]"}, {patternsJson: "[]"}, {csvOptions: true}, {trimFields: true}, {csv: {...identity.form.csv, activity_column: ""}}])
      expect(gate.accepts(first, {source: identity.source, form: {...identity.form, ...patch} as ReferenceLogFilterForm})).toBe(false);
    expect(gate.accepts(first, {...identity, source: source()})).toBe(false);
    const second = gate.issue(identity); expect(gate.accepts(first, identity)).toBe(false); expect(gate.accepts(second, identity)).toBe(true);
    const controller = new AbortController(); controller.abort(); expect(gate.accepts(second, identity, controller.signal)).toBe(false);
    gate.invalidate(); expect(gate.accepts(second, identity)).toBe(false);
  });
  it("resets the file picker after taking the original File so the same file can be selected again", () => {
    const file = source(), input = {files: [file], value: "same"}; expect(referenceLogFilterPickedFile(input)).toBe(file); expect(input.value).toBe("");
  });
});
