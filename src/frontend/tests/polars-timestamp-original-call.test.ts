import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {afterEach,describe,expect,it,vi} from "vitest";
import {TableTimestampFilterWorkspace} from "../src/components/TableTimestampFilterWorkspace";
import {PolarsTimestampOriginalInspection} from "../src/components/PolarsTimestampOriginalCallWorkspace";
import {POLARS_TIMESTAMP_ORIGINAL_OPERATIONS,POLARS_TIMESTAMP_ORIGINAL_LABELS,POLARS_TIMESTAMP_ORIGINAL_KEYS,defaultPolarsTimestampOriginalForm,polarsTimestampOriginalExample,polarsTimestampOriginalFields,polarsTimestampOriginalCallText,isPolarsTimestampOriginalEnvelope,requestPolarsTimestampOriginal,polarsTimestampOriginalArtifactFile,polarsTimestampOriginalResponseFile,PolarsTimestampOriginalRequestGate,type PolarsTimestampOriginalEnvelope,type PolarsTimestampOriginalRun} from "../src/polars-timestamp-original-call";
const source=()=>new File(['{"columns":[],"rows":[]}'],"source.json",{type:"application/json"});
function envelope(call=defaultPolarsTimestampOriginalForm().callJson):PolarsTimestampOriginalEnvelope{return {family:"TABLE_TIMESTAMP_FILTER",engine:"polars",operation:"apply_events",http_status:200,phase:"complete",apply_returned:true,source:{rows:5,columns:5},result:{rows:3,columns:5},source_unchanged:true,case_id_key:null,attribute_key:null,timestamp_key:null,source_table_json:'{"actual":"source"}',filtered_table_json:'{"actual":"filtered"}',error_type:null,error:null,call_mode:"original",call_json:call,controls_before_json:'{"actual":"before"}',controls_after_json:'{"actual":"after"}',controls_unchanged:true,source_after_table_json:'{"actual":"source-after"}',call_projection_json:'{"native_function":"filter_polars_timestamp_events_parameter_value"}'};}
function result(value=envelope()):PolarsTimestampOriginalRun{return {value,sourceFile:source(),fields:[],raw:JSON.stringify(value),responseBytes:new TextEncoder().encode(JSON.stringify(value))};}
const respond=(value:PolarsTimestampOriginalEnvelope)=>vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));
const render=(node:Parameters<typeof renderToStaticMarkup>[0])=>renderToStaticMarkup(node);
afterEach(()=>vi.unstubAllGlobals());
describe("Polars original timestamp keyword wire and byte identity",()=>{
  it("covers all6 public functions with ordered keyword operands and a frame kind",()=>{
    expect(POLARS_TIMESTAMP_ORIGINAL_OPERATIONS).toHaveLength(6);
    for(const operation of POLARS_TIMESTAMP_ORIGINAL_OPERATIONS){const form=polarsTimestampOriginalExample(operation),v=JSON.parse(form.callJson);expect(Object.keys(v)).toEqual(["schema","source_kind","keyword_entries"]);expect(v.source_kind).toBe("LazyFrame");
      expect(v.keyword_entries.map((e:{key:string})=>e.key)).toEqual(operation.startsWith("apply")&&operation!=="apply_events"?[]:operation==="filter_traces_attribute_in_timeframe"?["attribute","attribute_value","dt1","dt2"]:["dt1","dt2"]);
      expect(polarsTimestampOriginalFields(form)).toEqual([["engine","polars"],["operation",operation],["call_json",form.callJson]]);}
    expect(JSON.parse(polarsTimestampOriginalExample("apply_events","eager").callJson).source_kind).toBe("DataFrame");
    expect(JSON.parse(polarsTimestampOriginalExample("apply_events","typed").callJson).keyword_entries[0].value).toEqual({type:"datetime",iso:"2026-06-01T00:00:01+00:00",fold:0,offset_seconds:0});
    const enums=JSON.parse(polarsTimestampOriginalExample("filter_traces_contained","enum").callJson).keyword_entries[2].value.items;expect(enums[1][0]).toMatchObject({type:"enum",module:"pm4py.algo.filtering.polars.timestamp.timestamp_filter",name:"TIMESTAMP_KEY"});
  });
  it("sends exact BOM/CRLF/wide tokens and keeps imported text byte-exact",async()=>{
    const raw='﻿\r\n{"schema":"minera-polars-timestamp-original-call-v1","source_kind":"LazyFrame","keyword_entries":[{"key":"dt2","value":{"type":"int","value":18446744073709551615}}]}\r\n';
    respond(envelope(raw));const file=source();const run=await requestPolarsTimestampOriginal(file,{operation:"apply_events",callJson:raw});
    const body=vi.mocked(fetch).mock.calls[0][1]!.body as FormData;expect([...body.keys()]).toEqual(["source_file","engine","operation","call_json"]);expect(body.get("source_file")).toBe(file);expect(body.get("engine")).toBe("polars");
    expect(new Uint8Array(await (body.get("call_json") as File).arrayBuffer())).toEqual(new TextEncoder().encode(raw));expect(run.value.call_json).toBe(raw);
    expect(await polarsTimestampOriginalCallText(new File([raw],"call.json"))).toBe(raw);await expect(polarsTimestampOriginalCallText(new File([new Uint8Array([0xff])],"bad.json"))).rejects.toThrow("valid UTF-8");
  });
  it("validates all23 fields with apply and collect phases",()=>{
    const value=envelope();expect(Object.keys(value).sort()).toEqual([...POLARS_TIMESTAMP_ORIGINAL_KEYS].sort());expect(isPolarsTimestampOriginalEnvelope(value)).toBe(true);
    for(const changed of [{engine:"pandas"},{case_id_key:"x"},{extra:null},{call_projection_json:null},{operation:"filter_traces_starting_in_timeframe"}])expect(isPolarsTimestampOriginalEnvelope({...value,...changed})).toBe(false);
    const collect:PolarsTimestampOriginalEnvelope={...value,http_status:400,phase:"collect",apply_returned:true,result:null,filtered_table_json:null,error_type:"SchemaError",error:"could not evaluate comparison"};expect(isPolarsTimestampOriginalEnvelope(collect)).toBe(true);
    expect(isPolarsTimestampOriginalEnvelope({...collect,apply_returned:false})).toBe(false);
    const apply:PolarsTimestampOriginalEnvelope={...collect,phase:"apply",apply_returned:false};expect(isPolarsTimestampOriginalEnvelope(apply)).toBe(true);expect(isPolarsTimestampOriginalEnvelope({...apply,apply_returned:true})).toBe(false);expect(isPolarsTimestampOriginalEnvelope({...apply,filtered_table_json:"{}"})).toBe(false);
  });
  it("rejects identity drift, withholds results after errors and binds request identity",async()=>{
    const form=defaultPolarsTimestampOriginalForm();respond({...envelope(),operation:"filter_traces_contained"});await expect(requestPolarsTimestampOriginal(source(),form)).rejects.toThrow();
    const run=result();run.value={...run.value,http_status:400,phase:"collect",result:null,filtered_table_json:null,error_type:"SchemaError",error:"x"};expect(()=>polarsTimestampOriginalArtifactFile(run,"filtered_table_json")).toThrow("unavailable");
    expect(new Uint8Array(await polarsTimestampOriginalResponseFile(run).arrayBuffer())).toEqual(run.responseBytes);
    const identity={source:source(),form},gate=new PolarsTimestampOriginalRequestGate(),ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);expect(gate.accepts(ticket,{...identity,form:{...form,callJson:form.callJson+" "}})).toBe(false);
  });
});
describe("Polars original timestamp rendering (SSR only; browser events accepted separately)",()=>{
  it("keeps the friendly default and exposes all6 operations in original mode",()=>{
    const friendly=render(createElement(TableTimestampFilterWorkspace,{engine:"polars"}));expect(friendly).toContain("Polars timestamp call mode");expect(friendly).not.toContain("Complete original Polars timestamp call JSON");
    const html=render(createElement(TableTimestampFilterWorkspace,{engine:"polars",initialCallMode:"original"}));for(const operation of POLARS_TIMESTAMP_ORIGINAL_OPERATIONS)expect(html).toContain(POLARS_TIMESTAMP_ORIGINAL_LABELS[operation]);
    for(const label of ["Original Polars timestamp source table","Complete original Polars timestamp call JSON","Eager DataFrame source","Typed UTC datetime bounds","collect phase","Run original Polars timestamp call"])expect(html).toContain(label);expect(html).not.toContain("pandas");
  });
  it("offers artifacts, shows collect diagnostics and reuses only successful frames",()=>{
    const html=render(createElement(PolarsTimestampOriginalInspection,{run:result(),onUseTable:()=>{}}));for(const label of ["Download native timestamp call projection","Download original filtered native timestamp table","Use filtered timestamp table as source","Download complete original Polars timestamp response"])expect(html).toContain(label);
    const failed=result();failed.value={...failed.value,http_status:400,phase:"collect",result:null,filtered_table_json:null,error_type:"SchemaError",error:"could not evaluate"};const error=render(createElement(PolarsTimestampOriginalInspection,{run:failed,onUseTable:()=>{}}));expect(error).toContain("SchemaError: could not evaluate");expect(error).toContain("phase: collect");expect(error).not.toContain("Use filtered timestamp table as source");
  });
});
