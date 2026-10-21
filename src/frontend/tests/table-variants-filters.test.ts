import {afterEach, expect, it, vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {defaultTableVariantsForm, tableVariantsFields, tableVariantsOperations, requestTableVariants, isTableVariantsEnvelope,
  tableVariantsArtifactFile, tableVariantsResponseFile, tableVariantsMappingExample, TableVariantsRequestGate, type TableVariantsEnvelope, type TableVariantsRun} from "../src/table-variants-filters";
import {TableVariantsFilterWorkspace, TableVariantsFilterInspection} from "../src/components/TableVariantsFilterWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {defaultReferenceLogFilterForm, referenceLogFilterFields, isReferenceLogFilterEnvelope, openReferenceLogFilter, type ReferenceLogFilterRun} from "../src/reference-log-filters";
const source = () => new File(['{"columns":[],"rows":[]}'], "source.json");
function envelope(): TableVariantsEnvelope {return {family: "TABLE_VARIANTS_FILTER", engine: "pandas", operation: "membership", http_status: 200, phase: "complete", apply_returned: true,
  source: {rows: 5, columns: 3}, result: {rows: 2, columns: 3}, source_unchanged: true, case_id_key: "case:concept:name", attribute_key: "concept:name", timestamp_key: "time:timestamp",
  source_table_json: '{"full":"source"}', filtered_table_json: '{"exact":9007199254740993,"label":"Ω\\u0000"}', variants_table_json: null, variants_source_unchanged: null, error_type: null, error: null};}
function run(value=envelope()): TableVariantsRun {const raw=JSON.stringify(value);return {value,raw,responseBytes:new TextEncoder().encode(raw),sourceFile:source(),variantsFile:null,fields:[]};}
afterEach(() => vi.unstubAllGlobals());
it("sends explicit tuple interpretation and literal roles without numeric conversion", () => {
  const form=defaultTableVariantsForm("pandas"); expect(Object.fromEntries(tableVariantsFields(form))).toMatchObject({admitted_json:'[["A", "B"]]',sequence_kind:"tuple",positive:"true"});
  expect(Object.fromEntries(tableVariantsFields({...form,operation:"top_k",criterion:"9007199254740993",caseKeyMode:"custom",caseKey:"",activityKeyMode:"custom",activityKey:" role\nΩ\0 "}))).toMatchObject({criterion:"9007199254740993",case_id_key:"",activity_key:" role\nΩ\0 "});
});
it("offers only working engine modes and active timestamp controls", () => {
  expect(tableVariantsOperations("polars")).toEqual(["membership","top_k"]);
  const form={...defaultTableVariantsForm("polars"),timestampKeyMode:"custom" as const,timestampKey:" rank\0 "};
  expect(Object.fromEntries(tableVariantsFields(form))).not.toHaveProperty("timestamp_key");
  expect(Object.fromEntries(tableVariantsFields({...form,operation:"top_k"}))).toHaveProperty("timestamp_key"," rank\0 ");
  expect(() => tableVariantsFields({...form,operation:"auto"})).toThrow("available");
});
it("validates complete envelopes and mapping preservation", () => {
  expect(isTableVariantsEnvelope(envelope())).toBe(true);
  for(const patch of [{extra:1},{variants_table_json:42},{variants_source_unchanged:"true"},{timestamp_key:null},{operation:"wrong"}])expect(isTableVariantsEnvelope({...envelope(),...patch})).toBe(false);
  expect(isTableVariantsEnvelope({...envelope(),http_status:400,phase:"collect",result:null,error:"failure",error_type:"ColumnNotFoundError"})).toBe(true);
});
it("uploads both genuine files and rejects inappropriate auxiliary files", async () => {
  const mapping=source();vi.stubGlobal("fetch",vi.fn(async (_url:string,init:RequestInit) => {const body=init.body as FormData;expect(body.get("source_file")).toBeInstanceOf(File);expect(body.get("variants_file")).toBeInstanceOf(File);expect(body.get("sequence_kind")).toBe("tuple");return new Response(JSON.stringify(envelope()));}));
  expect((await requestTableVariants(source(),defaultTableVariantsForm("pandas"),mapping)).variantsFile).toBe(mapping);
  await expect(requestTableVariants(source(),defaultTableVariantsForm("polars"),mapping)).rejects.toThrow("pandas variants");
});
it("rejects responses for another operation", async () => {
  vi.stubGlobal("fetch",vi.fn(async () => new Response(JSON.stringify({...envelope(),operation:"top_k"}))));
  await expect(requestTableVariants(source(),defaultTableVariantsForm("pandas"))).rejects.toThrow("another engine or filter");
});
for(const operation of ["top_k","min_coverage","max_coverage"] as const)it(`uploads a supplied mapping for pandas ${operation}`,async () => {
  const mapping=source();vi.stubGlobal("fetch",vi.fn(async (_url:string,init:RequestInit) => {const body=init.body as FormData;expect(body.get("variants_file")).toBeInstanceOf(File);expect(body.get("operation")).toBe(operation);expect(body.has("admitted_json")).toBe(false);return new Response(JSON.stringify({...envelope(),operation}));}));
  expect((await requestTableVariants(source(),{...defaultTableVariantsForm("pandas"),operation},mapping)).variantsFile).toBe(mapping);
  expect(renderToStaticMarkup(createElement(TableVariantsFilterWorkspace,{engine:"pandas",initialForm:{...defaultTableVariantsForm("pandas"),operation}}))).toContain("Optional precomputed");
});
it("keeps complete source/result/mapping and raw response bytes downloadable", async () => {
  const result=run({...envelope(),variants_table_json:'{"mapping":"full"}',variants_source_unchanged:true});
  for(const kind of ["source","result","variants"] as const)expect(await tableVariantsArtifactFile(result,kind).text()).toBe(kind==="source"?result.value.source_table_json:kind==="result"?result.value.filtered_table_json:result.value.variants_table_json);
  expect(new Uint8Array(await tableVariantsResponseFile(result).arrayBuffer())).toEqual(result.responseBytes);
  expect(() => tableVariantsArtifactFile(run({...envelope(),http_status:400,error:"failure"}),"result")).toThrow();
});
it("includes real indexed tuple cells in the supplied mapping example", async () => {
  const example=JSON.parse(await tableVariantsMappingExample().text());expect(example.index.names).toEqual(["case:concept:name"]);expect(example.index.labels[0]).toEqual({kind:"scalar",value:{value:"c1"}});expect(example.rows[0][0].container).toBe("tuple");
});
it("invalidates late responses for either file and every control change", () => {
  const gate=new TableVariantsRequestGate(),identity={source:source(),variants:null,form:defaultTableVariantsForm("pandas")},ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);
  expect(gate.accepts(ticket,{...identity,variants:source()})).toBe(false);expect(gate.accepts(ticket,{...identity,source:source()})).toBe(false);
  for(const patch of [{sequenceKind:"list" as const},{admittedJson:"[]"},{criterion:"2"},{operation:"top_k" as const},{positive:false},{caseKey:""},{activityKey:""},{timestampKey:""}])expect(gate.accepts(ticket,{...identity,form:{...identity.form,...patch}})).toBe(false);
  gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);
});
it("renders engine differences and complete downloads", () => {
  const pandas=renderToStaticMarkup(createElement(TableVariantsFilterWorkspace,{engine:"pandas"}));for(const label of ["Inner sequence type","Optional precomputed","contiguous slices","Filter table variants"])expect(pandas).toContain(label);
  const polars=renderToStaticMarkup(createElement(TableVariantsFilterWorkspace,{engine:"polars"}));expect(polars).toContain("canonical time:timestamp");expect(polars).toContain("AttributeError");expect(polars).not.toContain("Optional precomputed");
  expect(renderToStaticMarkup(createElement(TableVariantsFilterInspection,{run:run()}))).toContain("Download filtered native table");
});
for(const operation of ["variants","variants_top_k","variants_min_coverage","variants_max_coverage","variants_cumulative"] as const)it(`exposes log ${operation} with active controls`, () => {
  const form=defaultReferenceLogFilterForm(operation),fields=Object.fromEntries(referenceLogFilterFields(form));expect(fields.operation).toBe(operation);
  if(operation==="variants")expect(fields).toMatchObject({admitted_json:'[["A", "B"]]',sequence_kind:"tuple"});else expect(fields.criterion).toBe("1");
  if(operation==="variants_cumulative")expect(fields).not.toHaveProperty("positive");else expect(fields.positive).toBe("true");
  expect(renderToStaticMarkup(createElement(ReferenceLogFilterWorkspace,{initialOperation:operation}))).toContain(operation==="variants"?"Inner sequence type":operation==="variants_top_k"?"Number of variants":"Coverage fraction");
});
it("retains the original log opening activity separately from variant matching", () => {
  const value={family:"REFERENCE_LOG_FILTER" as const,operation:"variants",http_status:200 as const,phase:"complete",source_format:"native_log",source:{traces:2,events:4},result:{traces:1,events:2},source_unchanged:true,activity_key:"work",filter_attribute_key:"selected",filtered_log_json:'{"traces":[]}',filtered_xes:"<log/>",xes_error:null,error:null};
  expect(isReferenceLogFilterEnvelope(value)).toBe(true);const raw=JSON.stringify(value),result:ReferenceLogFilterRun={value,raw,responseBytes:new TextEncoder().encode(raw),sourceFile:source(),fields:[]};const open=vi.fn();openReferenceLogFilter(result,open);expect(open.mock.calls[0][1]).toBe("work");
});
