import {afterEach, describe, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {ContextBuilder} from "../src/components/ContextBuilder";
import {OriginalModelContext, OriginalModelContextInspection} from "../src/components/OriginalModelContext";
import {nativeCell, nativeJson, parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET} from "../src/original-petri-analysis-workbench";
import {ORIGINAL_CONTEXT_IMPORT_EXAMPLE, ORIGINAL_MODEL_CONTEXT_ARTIFACTS, ORIGINAL_MODEL_CONTEXT_EXAMPLES, ORIGINAL_MODEL_CONTEXT_VARIANTS, OriginalModelContextRequestGate, defaultOriginalModelContextForm, isOriginalContextModel, isOriginalModelContextEnvelope, originalContextHeaderOptions, originalContextImportOptions, originalContextMarking, originalModelContextAllTablesCsv, originalModelContextArtifacts, originalModelContextFields, originalModelContextPreview, originalModelContextReport, originalModelContextTables, runOriginalModelContext, type OriginalModelContextEnvelope, type OriginalModelContextVariant} from "../src/original-model-context";
afterEach(() => vi.unstubAllGlobals());
const hex = (data: string) => ({native_bytes: "hex-v1" as const, data});
const byteHex = (bytes: Uint8Array) => Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("");
const documentJson = () => '{"net":'+ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET+',"initial_marking":[1,0],"final_marking":[0,1],"stochastic_map":null,"place_ids":["start","end"],"transition_ids":["A"],"initial_marking_keys":[true,false],"final_marking_keys":[false,true],"arc_occurrences":null}';
const model = (variant: OriginalModelContextVariant): unknown => parseNativeJson(variant === "petri_net" ? documentJson() : ORIGINAL_MODEL_CONTEXT_EXAMPLES[variant]);
const originalFile = (variant: OriginalModelContextVariant = "declare") => new File([ORIGINAL_MODEL_CONTEXT_EXAMPLES[variant]], `original-${variant}.${variant === "petri_net" ? "pnml" : "json"}`, {lastModified: 17});
function envelope(variant: OriginalModelContextVariant = "declare", text = "complete native model context\n"): OriginalModelContextEnvelope {
 const value = model(variant), type = {declare: "DeclareModel", log_skeleton: "LogSkeletonModel", temporal_profile: "TemporalProfile", petri_net: "PnmlDocument"}[variant], artifacts: OriginalModelContextEnvelope["artifacts"] = {};
 for (const key of ORIGINAL_MODEL_CONTEXT_ARTIFACTS) {artifacts[key] = key === "native_text_bytes" ? text : key.endsWith("pnml") ? variant === "petri_net" ? "<pnml/>" : null : nativeJson(value); artifacts[`${key}_error`] = null;}
 return {family: "LLM_CONTEXT", operation: "original_model", variant, native_entered: true, native_returned: true, executed: true, error: null, input_order: "httplib_key_order", input_context: {model_format: "auto", detected_format: variant === "petri_net" ? "pnml" : `${variant}_json`, model_type: type, filename: "", original_model_bytes: 0}, original_model: "", options: {include_header: true}, pnml_import_options: null, snapshots: {model_before: value, model_after: value, effective_model_before: value, effective_model_after: value}, model_unchanged: true, effective_model_unchanged: true, markings: {initial_before: variant === "petri_net" ? [1, 0] : null, initial_after: variant === "petri_net" ? [1, 0] : null, final_before: variant === "petri_net" ? [0, 1] : null, final_after: variant === "petri_net" ? [0, 1] : null, initial_origin: variant === "petri_net" ? "pnml" : null, final_origin: variant === "petri_net" ? "pnml" : null, initial_override: null, final_override: null, unchanged: variant === "petri_net" ? true : null}, result: {variant, text, characters: Array.from(text).length, truncated: false}, text_bytes: nativeStringBytes(text).length, provider_text: text, original_inputs: [], query_inputs: [], artifacts};
}
/** Fixed canonical native models/results; transport double captures actual request parts only. */
function reply(value: OriginalModelContextEnvelope, status = 200, change?: (value: OriginalModelContextEnvelope, body: FormData) => void) {
 const fetch = vi.fn(async (_path: string, init?: RequestInit) => {const body = init?.body as FormData, file = body.get("model_file") as File, bytes = new Uint8Array(await file.arrayBuffer()), request = new Request("http://minera.invalid/test-multipart-capture", {method: "POST", body}), boundary = /boundary=(.+)$/.exec(request.headers.get("content-type")!)![1], wire = await request.text(), headers = wire.split(`--${boundary}\r\n`).slice(1).map(part => part.slice(0, part.indexOf("\r\n\r\n")));
  value.original_model = hex(byteHex(bytes)); value.input_context.original_model_bytes = bytes.length; value.input_context.model_format = body.get("model_format") as string;
  value.original_inputs = await Promise.all([...body].map(async ([name, entry], index) => {const filename = /(?:;\s*)filename="([^"]*)"/i.exec(headers[index]); return {name, filename: filename?.[1] ?? "", file: filename !== null, bytes: typeof entry === "string" ? entry : hex(byteHex(new Uint8Array(await entry.arrayBuffer())))};})); value.input_context.filename = value.original_inputs.find(part => part.name === "model_file")!.filename; change?.(value, body); return {ok: status < 400, status, text: async () => nativeJson(value)};});
 vi.stubGlobal("fetch", fetch); return fetch;
}
function fail(value: OriginalModelContextEnvelope, phase = "native_error", entered = true): OriginalModelContextEnvelope {value.native_entered = entered; value.native_returned = false; value.executed = false; value.error = {phase, type: "native_error", message: "native rejected original model"}; value.result = null; value.text_bytes = null; value.provider_text = null; return value;}
describe("four complete original-model context workflows", () => {
 it("validates full DECLARE map membership, inactive labels, signed metadata and byte carriers", () => {
  const value = parseNativeJson('{"rules":[{"key":{"native_bytes":"hex-v1","data":"ff00"},"value":[{"first":"","second":{"native_bytes":"hex-v1","data":"eda080"},"binary":false,"support":-2147483648,"confidence":2147483647}]},{"key":"empty-unknown","value":[]}]}');
  expect(isOriginalContextModel("DeclareModel", value)).toBe(true); expect(nativeJson(value)).toContain('"support":-2147483648'); expect(nativeJson(value)).toContain("empty-unknown");
 });
 it("rejects duplicate decoded byte map keys and malformed native rule fields", () => {
  expect(isOriginalContextModel("DeclareModel", {rules: [{key: "A", value: []}, {key: hex("41"), value: []}]})).toBe(false);
  const value = model("declare") as {rules: Array<{value: Array<Record<string, unknown>>}>}; value.rules[0].value[0].support = 2147483648; expect(isOriginalContextModel("DeclareModel", value)).toBe(false);
 });
 it("retains skeleton relation order, duplicate pairs/counts and full size_t tokens", () => {
  const value = parseNativeJson('{"equivalence":[{"source":"B","target":"A"},{"source":"B","target":"A"}],"always_after":[],"always_before":[],"never_together":[],"directly_follows":[],"activ_freq":[{"key":{"native_bytes":"hex-v1","data":"ff"},"value":[2,0,2,18446744073709551615]}]}');
  expect(isOriginalContextModel("LogSkeletonModel", value)).toBe(true); expect(nativeJson(value)).toContain("[2,0,2,18446744073709551615]");
 });
 it("retains temporal entry order/count metadata, signed zero and every IEEE tag", () => {
  const value = parseNativeJson('{"entries":[{"source":{"native_bytes":"hex-v1","data":"ff"},"target":"B","count":18446744073709551615,"mean_seconds":-0.0,"standard_deviation_seconds":"NaN"},{"source":"A","target":"B","count":9007199254740992,"mean_seconds":"INF","standard_deviation_seconds":"-INF"}]}');
  expect(isOriginalContextModel("TemporalProfile", value)).toBe(true); expect(nativeJson(value)).toContain('"mean_seconds":-0.0'); expect(nativeJson(value)).toContain("18446744073709551615");
 });
 it("reuses the full native net constructor guard with inactive boundary storage and optional names", () => {
  const value = parseNativeJson(ORIGINAL_PETRI_ANALYSIS_DEFAULT_NET.replace('"source_storage":0', '"source_storage":18446744073709551615').replace('"has_source":true', '"has_source":false')) as Record<string, unknown>;
  expect(isOriginalContextModel("PetriNet", value)).toBe(true); value.transition_names = []; expect(isOriginalContextModel("PetriNet", value)).toBe(true); expect(nativeJson(value)).toContain("18446744073709551615");
 });
 it("retains complete native documents with arbitrary precision signed markings and explicit zero keys", () => {
  const value = parseNativeJson(documentJson().replace('"initial_marking":[1,0]', '"initial_marking":[-999999999999999999999999999999999999999999,0]'));
  expect(isOriginalContextModel("PnmlDocument", value)).toBe(true); expect(nativeJson(value)).toContain("-999999999999999999999999999999999999999999");
 });
 it("accepts only the actual public header option and rejects decoded duplicate keys", () => {
  expect(originalContextHeaderOptions('{"include_header":false}')).toEqual({include_header: false});
  for (const raw of ['{}', '{"include_header":null}', '{"include_header":true,"max_len":10}', '{"include_header":true,"include_\\u0068eader":false}']) expect(() => originalContextHeaderOptions(raw)).toThrow();
 });
 it("preserves arbitrary-width marking tokens without floating-point or int64 narrowing", () => {
  const raw = '[0,-1,9007199254740992,9007199254740993,-999999999999999999999999999999999999999999999999999999999999]'; expect(nativeJson(originalContextMarking(raw))).toBe(raw);
  for (const invalid of ['[1e0]', '[1.0]', '[null]', '[true]', '["1"]']) expect(() => originalContextMarking(invalid)).toThrow();
 });
 it("requires every actual PNML import boolean and preserves false values", () => {
  expect(originalContextImportOptions(ORIGINAL_CONTEXT_IMPORT_EXAMPLE).import_stochastic_map).toBe(false);
  expect(() => originalContextImportOptions('{"infer_initial_marking":true}')).toThrow(); expect(() => originalContextImportOptions(ORIGINAL_CONTEXT_IMPORT_EXAMPLE.replace('"import_stochastic_map":false', '"import_stochastic_map":0'))).toThrow();
 });
 it("rejects all full-header/scalar/alias conflicts by supplied presence", () => {
  const file = new File(['{"include_header":true}'], "options.json"), form = defaultOriginalModelContextForm("declare");
  const conflicts: Array<Record<string, string>> = [{include_header: "", response_header: "false"}, {include_header: "", context_options_json: '{"include_header":true}'}, {response_header: "", context_options_json: '{"include_header":true}'}];
  for (const fields of conflicts) expect(() => originalModelContextFields({...form, fields})).toThrow();
  expect(() => originalModelContextFields({...form, fields: {include_header: ""}}, {context_options_file: file})).toThrow(); expect(() => originalModelContextFields({...form, fields: {context_options_json: '{"include_header":true}'}}, {context_options_file: file})).toThrow();
 });
 it("rejects per-role JSON/File conflicts and incompatible Petri-only controls", () => {
  const file = new File(["[]"], "marking.json"), form = defaultOriginalModelContextForm("petri_net");
  for (const role of ["initial", "final"] as const) expect(() => originalModelContextFields({...form, fields: {[`${role}_marking_json`]: "[]"}}, {[`${role}_marking_file`]: file})).toThrow();
  expect(() => originalModelContextFields({...defaultOriginalModelContextForm("declare"), fields: {initial_marking_json: "[]"}})).toThrow();
 });
 it("requires both explicit markings for a raw net, including explicit empty vectors", () => {
  const form = {...defaultOriginalModelContextForm(), modelFormat: "native_net" as const}; expect(() => originalModelContextFields(form)).toThrow("both"); expect(() => originalModelContextFields({...form, fields: {initial_marking_json: "[]"}})).toThrow("both");
  expect(originalModelContextFields({...form, fields: {initial_marking_json: "[]", final_marking_json: "[]"}})).toContainEqual(["final_marking_json", "[]"]);
 });
 it("calls all four actual public helper branches using the original model and options File objects", async () => {
  const options = new File(['{"include_header":true}'], "same-options.json");
  for (const variant of ORIGINAL_MODEL_CONTEXT_VARIANTS) {const file = originalFile(variant), fetch = reply(envelope(variant)), run = await runOriginalModelContext(file, defaultOriginalModelContextForm(variant), {context_options_file: options}), body = fetch.mock.calls[0][1]?.body as FormData;
   expect(fetch.mock.calls[0][0]).toBe("/api/llm-context"); expect(body.get("model_file")).toBe(file); expect(body.get("context_options_file")).toBe(options); expect(body.get("variant")).toBe(variant); expect(run.input.model).toBe(file); expect(run.input.files.context_options_file).toBe(options); expect(run.input.sha256.model_file).toMatch(/^[a-f0-9]{64}$/); expect(run.input.sha256.context_options_file).toMatch(/^[a-f0-9]{64}$/); expect(run.value.native_returned).toBe(true);
  }
 });
 it("transports the actual scalar header alias and checks its native option echo", async () => {
  const value = envelope(), form = defaultOriginalModelContextForm("declare"); form.fields.response_header = "0"; value.options!.include_header = false; const fetch = reply(value); await runOriginalModelContext(originalFile(), form); expect((fetch.mock.calls[0][1]?.body as FormData).get("response_header")).toBe("0");
  reply(envelope()); await expect(runOriginalModelContext(originalFile(), form)).rejects.toThrow("header options");
 });
 it("binds every original File to actual quoted/CRLF multipart filenames while retaining local identity", async () => {
  const file = new File([ORIGINAL_MODEL_CONTEXT_EXAMPLES.petri_net], 'original "model"\r\nΩ.pnml'), options = new File(['{"include_header":true}'], 'header"\r\n.json'), initial = new File(["[1,0]"], 'initial"\r\n.json'), final = new File(["[0,1]"], 'final"\r\n.json'), imports = new File([ORIGINAL_CONTEXT_IMPORT_EXAMPLE], 'imports"\r\n.json'), uploads = {context_options_file: options, initial_marking_file: initial, final_marking_file: final, pnml_import_options_file: imports}, expected = (name: string) => name.replace(/\r/g, "%0D").replace(/\n/g, "%0A").replace(/"/g, "%22");
  const fetch = reply(envelope("petri_net")), run = await runOriginalModelContext(file, defaultOriginalModelContextForm(), uploads), body = fetch.mock.calls[0][1]?.body as FormData;
  for (const [name, original] of Object.entries({model_file: file, ...uploads})) {expect(body.get(name)).toBe(original); expect(run.input.files[name]).toBe(original); expect(run.value.original_inputs.find(part => part.name === name)?.filename).toBe(expected(original.name)); expect(run.input.sha256[name]).toMatch(/^[a-f0-9]{64}$/);}
  expect(run.value.input_context.filename).toBe(expected(file.name)); const report = parseNativeJson(originalModelContextReport(run)) as {inputs: Record<string, {name: string}>}; expect(report.inputs.model_file.name).toBe(file.name); expect(report.inputs.context_options_file.name).toBe(options.name);
  reply(envelope("petri_net"), 200, value => {value.input_context.filename = file.name;}); await expect(runOriginalModelContext(file, defaultOriginalModelContextForm(), uploads)).rejects.toThrow("model File");
  reply(envelope("petri_net"), 200, value => {value.original_inputs.find(part => part.name === "context_options_file")!.filename = options.name;}); await expect(runOriginalModelContext(file, defaultOriginalModelContextForm(), uploads)).rejects.toThrow("provenance");
 });
 it("preserves multiline complete options as byte multipart without spelling changes", async () => {
  const raw = '{\r\n "include_header" : true\r\n}', form = defaultOriginalModelContextForm("declare"); form.fields.context_options_json = raw; const fetch = reply(envelope()), run = await runOriginalModelContext(originalFile(), form), part = (fetch.mock.calls[0][1]?.body as FormData).get("context_options_json") as File;
  expect(new Uint8Array(await part.arrayBuffer())).toEqual(nativeStringBytes(raw)); expect(run.fields.at(-1)).toEqual(["context_options_json", raw]); expect(run.value.original_inputs.find(row => row.name === "context_options_json")?.file).toBe(true);
 });
 it("retains actual marking/import Files, all byte hashes and explicit File origins", async () => {
  const options = new File(['{"include_header":true}'], "header.json"), initial = new File(["[-99999999999999999999999999999999999999,0]"], "initial.json"), final = new File(["[]"], "final.json"), imports = new File([ORIGINAL_CONTEXT_IMPORT_EXAMPLE], "imports.json"), value = envelope("petri_net"); value.markings.initial_origin = "explicit_file"; value.markings.final_origin = "explicit_file";
  const fetch = reply(value), run = await runOriginalModelContext(originalFile("petri_net"), defaultOriginalModelContextForm(), {context_options_file: options, initial_marking_file: initial, final_marking_file: final, pnml_import_options_file: imports}), body = fetch.mock.calls[0][1]?.body as FormData;
  for (const [key, file] of Object.entries({context_options_file: options, initial_marking_file: initial, final_marking_file: final, pnml_import_options_file: imports})) {expect(body.get(key)).toBe(file); expect(run.input.files[key]).toBe(file); expect(run.input.sha256[key]).toMatch(/^[a-f0-9]{64}$/);}
 });
 it("returns malformed raw options File native400 without replacing invalid/WTF8 bytes", async () => {
  const file = new File([new Uint8Array([255, 0, 237, 160, 128])], ""), value = fail(envelope(), "configuration_error", false); value.options = null; const fetch = reply(value, 400), run = await runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare"), {context_options_file: file});
  expect(run.http_status).toBe(400); expect(run.input.files.context_options_file).toBe(file); expect(new Uint8Array(await ((fetch.mock.calls[0][1]?.body as FormData).get("context_options_file") as File).arrayBuffer())).toEqual(new Uint8Array([255, 0, 237, 160, 128]));
 });
 it("retains parser failure snapshots and original model bytes before native entry", async () => {
  const value = fail(envelope(), "model_parse_error", false); value.input_context.model_type = null; value.snapshots = {model_before: null, model_after: null, effective_model_before: null, effective_model_after: null}; value.model_unchanged = null; value.effective_model_unchanged = null;
  reply(value, 400); const file = new File([new Uint8Array([255, 0])], "bad-model.json"), run = await runOriginalModelContext(file, defaultOriginalModelContextForm("declare")); expect(run.value.original_model).toEqual(hex("ff00")); expect(run.value.native_entered).toBe(false); expect(run.raw).toContain('"model_before":null');
 });
 it("retains native exceptions, complete parsed models and actual byte error messages", async () => {
  const value = fail(envelope()); value.error!.message = hex("ff00eda080"); reply(value, 400); const run = await runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare")); expect(run.value.snapshots.model_before).not.toBeNull(); expect(run.value.error?.message).toEqual(hex("ff00eda080")); expect(run.value.native_entered).toBe(true);
 });
 it("rejects missing parts or incorrect actual transport file flags", async () => {
  reply(envelope(), 200, value => {value.original_inputs.pop();}); await expect(runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare"))).rejects.toThrow("provenance");
  reply(envelope(), 200, value => {value.original_inputs.find(part => part.name === "model_file")!.file = false;}); await expect(runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare"))).rejects.toThrow("provenance");
 });
 it("rejects a mismatched original model echo even when its byte count agrees", async () => {
  reply(envelope(), 200, value => {value.original_model = "wrong";}); await expect(runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare"))).rejects.toThrow("model File");
 });
 it("rejects incorrect native byte counts, truncated flags and lossy provider projections", () => {
  const value = envelope(); expect(isOriginalModelContextEnvelope(value)).toBe(true); value.text_bytes = 0; expect(isOriginalModelContextEnvelope(value)).toBe(false); value.text_bytes = nativeStringBytes(value.result!.text as string).length; value.provider_text = "different"; expect(isOriginalModelContextEnvelope(value)).toBe(false); value.provider_text = value.result!.text as string; (value.result as unknown as {truncated: boolean}).truncated = true; expect(isOriginalModelContextEnvelope(value)).toBe(false);
 });
 it("retains text beyond the former2MiB limit in raw/report/CSV/download bytes while clipping display only", async () => {
  const text = "A".repeat(2 * 1024 * 1024 + 17), value = envelope("declare", text); reply(value); const run = await runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare")), preview = originalModelContextPreview(run.value);
  expect(run.value.provider_text).toBe(text); expect(run.value.result?.truncated).toBe(false); expect(preview.clipped).toBe(true); expect(preview.text).toHaveLength(6000); expect(run.raw).toContain(text); expect(originalModelContextReport(run)).toContain(text); expect(originalModelContextAllTablesCsv(run.tables)).toContain(text); expect(originalModelContextArtifacts(run).find(artifact => artifact.name.endsWith("native_text_bytes.txt"))?.bytes.length).toBe(text.length);
 });
 it("shows non-Unicode native text as explicit byte hex without implicit replacement decoding", () => {
  const value = envelope(); value.result!.text = hex("ff00eda080"); value.result!.characters = 3; value.text_bytes = 5; value.provider_text = null; expect(isOriginalModelContextEnvelope(value)).toBe(true); expect(originalModelContextPreview(value)).toEqual({text: "hex:ff00eda080", clipped: false, unicode: false});
  value.provider_text = "\ufffd\0\ufffd"; expect(isOriginalModelContextEnvelope(value)).toBe(false);
 });
 it("downloads separate complete canonical/legacy/text artifacts with byte-exact invalid content", async () => {
  const value = envelope(); value.artifacts.legacy_result_json = hex("ff00eda080"); value.artifacts.source_pnml = null; value.artifacts.source_pnml_error = "not applicable"; reply(value); const run = await runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare")), artifacts = originalModelContextArtifacts(run), legacy = artifacts.find(item => item.name.endsWith("legacy_result_json.json"))!;
  expect(legacy.bytes).toEqual(new Uint8Array([255, 0, 237, 160, 128])); expect(legacy.mime).toBe("application/json"); expect(artifacts.find(item => item.name.endsWith("native_text_bytes.txt"))?.mime).toBe("application/octet-stream"); expect(run.value.artifacts.source_pnml_error).toBe("not applicable");
 });
 it("binds cancellation to original model/options/marking identities, variants and edits", () => {
  const file = originalFile(), form = defaultOriginalModelContextForm("declare"), options = new File(["{}"], "options.json"), gate = new OriginalModelContextRequestGate(), uploads = {context_options_file: options}, ticket = gate.issue(file, form, uploads);
  expect(gate.accepts(ticket, file, form, uploads)).toBe(true); expect(gate.accepts(ticket, new File(["same"], file.name), form, uploads)).toBe(false); expect(gate.accepts(ticket, file, {...form, variant: "log_skeleton"}, uploads)).toBe(false); expect(gate.accepts(ticket, file, {...form, fields: {include_header: "false"}}, uploads)).toBe(false); expect(gate.accepts(ticket, file, form, {context_options_file: new File(["{}"], "options.json")})).toBe(false); expect(gate.accepts(ticket, file, form, {...uploads, initial_marking_file: options})).toBe(false);
  const signal = new AbortController(); signal.abort(); expect(gate.accepts(ticket, file, form, uploads, signal.signal)).toBe(false); gate.invalidate(); expect(gate.accepts(ticket, file, form, uploads)).toBe(false);
 });
 it("checks actual request aborts before dispatch and after receiving a native response", async () => {
  const fetch = reply(envelope()), before = new AbortController(); before.abort(); await expect(runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare"), {}, before.signal)).rejects.toThrow(); expect(fetch).not.toHaveBeenCalled();
  const after = new AbortController(), actual = reply(envelope(), 200, () => after.abort()); await expect(runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare"), {}, after.signal)).rejects.toThrow(); expect(actual.mock.calls[0][1]?.signal).toBe(after.signal); expect(actual).toHaveBeenCalledTimes(1);
 });
 it("prevents a pending old response from publishing after options File replacement or header-mode edits", async () => {
  const file = originalFile(), form = defaultOriginalModelContextForm("declare"), options = new File(['{"include_header":true}'], "same.json"), gate = new OriginalModelContextRequestGate(), ticket = gate.issue(file, form, {context_options_file: options});
  const transport = reply(envelope()); let release: (() => void) | undefined; const pending = new Promise<void>(resolve => {release = resolve;}); vi.stubGlobal("fetch", async (path: string, init?: RequestInit) => {await pending; return transport(path, init);});
  const promise = runOriginalModelContext(file, form, {context_options_file: options}); const nextOptions = new File(['{"include_header":true}'], "same.json"); gate.invalidate(); release!(); const old = await promise;
  expect(old.input.files.context_options_file).toBe(options); expect(gate.accepts(ticket, file, form, {context_options_file: nextOptions})).toBe(false); expect(gate.accepts(ticket, file, {...form, fields: {response_header: "true"}}, {})).toBe(false);
 });
 it("mounts the four original-model workflows inside the existing context builder without a log", () => {
  const html = renderToStaticMarkup(createElement(ContextBuilder, {available: false, mining: true, onUse: () => {}})), child = renderToStaticMarkup(createElement(OriginalModelContext, {onUse: () => {}}));
  expect(html).toContain("Describe an original model without a log"); expect(child).toContain('value="declare"'); expect(child).toContain('value="log_skeleton"'); expect(child).toContain('value="temporal_profile"'); expect(child).toContain('value="petri_net"'); expect(child).toContain("Original model File for context"); expect(child).toContain("Original complete options File"); expect(child).toContain("initial marking"); expect(child).not.toContain('<fieldset disabled=""'); expect(child).not.toContain("max_len"); expect(child).not.toContain("2097152");
 });
 it("mounts complete helper results/errors with original/report/text downloads and direct complete Use only", async () => {
  const value = envelope(), fetch = reply(value), run = await runOriginalModelContext(originalFile(), defaultOriginalModelContextForm("declare")), onUse = vi.fn(), html = renderToStaticMarkup(createElement(OriginalModelContextInspection, {run, onUse}));
  for (const text of ["Download original model_file File", "Download complete original model context report", "native_text_bytes.txt", "legacy_result_json.json", "Use complete description", "existing editable context", "explicit Send action", "HTTP 200"]) expect(html).toContain(text); expect(html).not.toContain("Load complete Unicode text for review"); expect(onUse).not.toHaveBeenCalled(); expect(fetch).toHaveBeenCalledTimes(1);
  const bad = fail(envelope()); const failed = {...run, value: bad, raw: nativeJson(bad), tables: originalModelContextTables(bad), http_status: 400}, failedHtml = renderToStaticMarkup(createElement(OriginalModelContextInspection, {run: failed, onUse, failed: true})); expect(failedHtml).toContain("Failed original model context inspection"); expect(failedHtml).toContain("native rejected original model"); expect(failedHtml).not.toContain("Use complete description");
 });
 it("validates all150000 distinct stored DECLARE rules without a spread argument limit or slicing", () => {
  const rules = Array.from({length: 150000}, () => ({first: "A", second: "inactive", binary: false, support: 0, confidence: 0})), value = {rules: [{key: "unknown", value: rules}]}; expect(isOriginalContextModel("DeclareModel", value)).toBe(true); rules.at(-1)!.support = 2147483648; expect(isOriginalContextModel("DeclareModel", value)).toBe(false);
 });
 it("retains original document markings separately from independently overridden effective vectors", async () => {
  const value = envelope("petri_net"), form = defaultOriginalModelContextForm(); form.fields.initial_marking_json = "[-999999999999999999999999999999999999,-0]"; value.markings.initial_override = originalContextMarking(form.fields.initial_marking_json.replace(",-0]", ",0]")); value.markings.initial_before = value.markings.initial_override; value.markings.initial_after = value.markings.initial_override; value.markings.initial_origin = "explicit_json";
  const effective = parseNativeJson(documentJson().replace('"initial_marking":[1,0]', '"initial_marking":'+form.fields.initial_marking_json)); value.snapshots.effective_model_before = effective; value.snapshots.effective_model_after = effective; reply(value); const run = await runOriginalModelContext(originalFile("petri_net"), form), report = parseNativeJson(originalModelContextReport(run)) as {native_result: OriginalModelContextEnvelope};
  expect((report.native_result.snapshots.model_before as {initial_marking: number[]}).initial_marking).toEqual([1, 0]); expect(nativeCell(report.native_result.markings.initial_before!, 0)).toBe("-999999999999999999999999999999999999"); expect(run.value.markings.final_origin).toBe("pnml"); expect(run.value.markings.final_before).toEqual([0, 1]);
 });
});
