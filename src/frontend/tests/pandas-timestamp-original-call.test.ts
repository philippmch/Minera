import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {afterEach,describe,expect,it,vi} from "vitest";
import {nativeCell} from "../src/native-json";
import {TableTimestampFilterWorkspace} from "../src/components/TableTimestampFilterWorkspace";
import {PandasTimestampOriginalCallWorkspace,PandasTimestampOriginalInspection} from "../src/components/PandasTimestampOriginalCallWorkspace";
import {PANDAS_TIMESTAMP_ORIGINAL_OPERATIONS,PANDAS_TIMESTAMP_ORIGINAL_LABELS,PANDAS_TIMESTAMP_ORIGINAL_KEYS,defaultPandasTimestampOriginalForm,pandasTimestampOriginalExample,pandasTimestampOriginalFields,pandasTimestampOriginalCallText,pandasTimestampOriginalCallFile,isPandasTimestampOriginalEnvelope,requestPandasTimestampOriginal,pandasTimestampOriginalArtifactFile,pandasTimestampOriginalResponseFile,PandasTimestampOriginalRequestGate,type PandasTimestampOriginalEnvelope,type PandasTimestampOriginalRun} from "../src/pandas-timestamp-original-call";
const source=()=>new File(['{"columns":[],"rows":[]}'],"source.json",{type:"application/json"});
function envelope(call=defaultPandasTimestampOriginalForm().callJson):PandasTimestampOriginalEnvelope{return {family:"TABLE_TIMESTAMP_FILTER",engine:"pandas",operation:"apply_events",http_status:200,phase:"complete",apply_returned:true,source:{rows:5,columns:5},result:{rows:2,columns:5},source_unchanged:true,case_id_key:null,attribute_key:null,timestamp_key:null,source_table_json:'{"actual":"source"}',filtered_table_json:'{"actual":"filtered"}',error_type:null,error:null,call_mode:"original",call_json:call,controls_before_json:'{"actual":"before"}',controls_after_json:'{"actual":"after"}',controls_unchanged:true,source_after_table_json:'{"actual":"source-after"}',call_projection_json:'{"native_function":"filter_pandas_timestamp_events"}'};}
function result(value=envelope()):PandasTimestampOriginalRun{return {value,sourceFile:source(),fields:[],raw:JSON.stringify(value),responseBytes:new TextEncoder().encode(JSON.stringify(value))};}
const respond=(value:PandasTimestampOriginalEnvelope)=>vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));
const render=(node:Parameters<typeof renderToStaticMarkup>[0])=>renderToStaticMarkup(node);
afterEach(()=>vi.unstubAllGlobals());
describe("pandas original timestamp complete wire and byte identity",()=>{
  it("covers all8 public functions with their exact positional arity",()=>{
    expect(PANDAS_TIMESTAMP_ORIGINAL_OPERATIONS).toHaveLength(8);
    for(const operation of PANDAS_TIMESTAMP_ORIGINAL_OPERATIONS){const form=pandasTimestampOriginalExample(operation),v=JSON.parse(form.callJson);expect(Object.keys(v)).toEqual(["schema","args","keyword_entries"]);expect(v.schema).toBe("minera-pandas-timestamp-original-call-v1");
      expect(v.args).toHaveLength(operation.startsWith("apply")&&operation!=="apply_events"?0:operation==="filter_traces_attribute_in_timeframe"?4:2);expect(pandasTimestampOriginalFields(form)).toEqual([["engine","pandas"],["operation",operation],["call_json",form.callJson]]);}
  });
  it("offers typed bounds, string mappings, ordered enum keys and raw parameters",()=>{
    expect(JSON.parse(pandasTimestampOriginalExample("apply_events","typed").callJson).args[0]).toEqual({type:"pandas.Timestamp",iso:"2026-06-01T00:00:01+00:00",resolution:"ns",timezone:"UTC"});
    expect(JSON.parse(pandasTimestampOriginalExample("apply_events","mapping").callJson).keyword_entries[0].value.items.every((pair:[{type:string}])=>pair[0].type==="str")).toBe(true);
    const entries=JSON.parse(pandasTimestampOriginalExample("filter_traces_contained","enum").callJson).keyword_entries[0].value.items;expect(entries[0][0].type).toBe("str");expect(entries[1][0]).toMatchObject({type:"enum",module:"pm4py.algo.filtering.pandas.timestamp.timestamp_filter",name:"TIMESTAMP_KEY"});
    expect(JSON.parse(pandasTimestampOriginalExample("apply","raw").callJson).keyword_entries[0].value.type).toBe("str");
  });
  it("sends exact BOM/CRLF/wide tokens in a canonical File and preserves the source File",async()=>{
    const raw='﻿\r\n{"schema":"minera-pandas-timestamp-original-call-v1","args":[{"type":"str","value":"x"},{"type":"int","value":9007199254740993},{"type":"str","value":"a"},{"type":"str","value":"b"}],"keyword_entries":[]}\r\n';
    respond({...envelope(raw),operation:"filter_traces_attribute_in_timeframe"});const file=source();const run=await requestPandasTimestampOriginal(file,{operation:"filter_traces_attribute_in_timeframe",callJson:raw});
    const mock=vi.mocked(fetch),body=mock.mock.calls[0][1]!.body as FormData;expect(mock.mock.calls[0][0]).toBe("/api/reference-table-timestamp");expect([...body.keys()]).toEqual(["source_file","engine","operation","call_json"]);expect(body.get("source_file")).toBe(file);
    const callFile=body.get("call_json") as File;expect(callFile.name).toBe("call_json.txt");expect(new Uint8Array(await callFile.arrayBuffer())).toEqual(new TextEncoder().encode(raw));expect(run.value.call_json).toBe(raw);
    expect(new Uint8Array(await pandasTimestampOriginalCallFile({operation:"apply",callJson:raw}).arrayBuffer())).toEqual(new TextEncoder().encode(raw));
  });
  it("imports strict UTF8 without BOM stripping and forwards malformed text for native diagnostics",async()=>{
    const raw='﻿{\r\n"integer":18446744073709551616\r\n}\r\n';expect(await pandasTimestampOriginalCallText(new File([raw],"call.json"))).toBe(raw);
    await expect(pandasTimestampOriginalCallText(new File([new Uint8Array([0xff])],"bad.json"))).rejects.toThrow("valid UTF-8");
    const failure:PandasTimestampOriginalEnvelope={...envelope("{"),http_status:400,phase:"control",apply_returned:false,source:null,result:null,source_table_json:null,source_after_table_json:null,filtered_table_json:null,controls_before_json:null,controls_after_json:null,source_unchanged:null,controls_unchanged:null,call_projection_json:null,error_type:"native_error",error:"original pandas timestamp call_json: trailing JSON content"};
    respond(failure);expect((await requestPandasTimestampOriginal(source(),{operation:"apply_events",callJson:"{"})).value.error).toBe(failure.error);
  });
  it("validates all23 fields and rejects friendly roles, foreign engines and missing projection",()=>{
    const value=envelope();expect(Object.keys(value).sort()).toEqual([...PANDAS_TIMESTAMP_ORIGINAL_KEYS].sort());expect(isPandasTimestampOriginalEnvelope(value)).toBe(true);
    for(const changed of [{engine:"polars"},{case_id_key:"case:concept:name"},{timestamp_key:""},{extra:null},{call_projection_json:null},{phase:"collect"},{source:{rows:-1,columns:2}}])expect(isPandasTimestampOriginalEnvelope({...value,...changed})).toBe(false);
    const native:PandasTimestampOriginalEnvelope={...value,http_status:400,phase:"apply",apply_returned:false,result:null,filtered_table_json:null,error_type:"KeyError",error:"'time:timestamp'"};expect(isPandasTimestampOriginalEnvelope(native)).toBe(true);
    expect(isPandasTimestampOriginalEnvelope({...native,filtered_table_json:'{"stale":true}'})).toBe(false);expect(isPandasTimestampOriginalEnvelope({...native,error:null})).toBe(false);
  });
  it("rejects identity drift, keeps exact count tokens and honors abort",async()=>{
    const form=defaultPandasTimestampOriginalForm();for(const changed of [{operation:"filter_traces_contained"},{call_json:form.callJson+" "}]){respond({...envelope(),...changed} as PandasTimestampOriginalEnvelope);await expect(requestPandasTimestampOriginal(source(),form)).rejects.toThrow();}
    const raw=JSON.stringify(envelope()).replace('"rows":5','"rows":9007199254740993');vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw,{status:200})));expect(String(nativeCell((await requestPandasTimestampOriginal(source(),form)).value.source!,"rows"))).toBe("9007199254740993");
    const fetcher=vi.fn();vi.stubGlobal("fetch",fetcher);const controller=new AbortController();controller.abort();await expect(requestPandasTimestampOriginal(source(),form,controller.signal)).rejects.toThrow();expect(fetcher).not.toHaveBeenCalled();
  });
  it("downloads opaque artifacts byte-exact and withholds the result after native errors",async()=>{
    const run=result();run.value.controls_after_json={native_bytes:"hex-v1",data:"ff000a"};expect(new Uint8Array(await pandasTimestampOriginalArtifactFile(run,"controls_after_json").arrayBuffer())).toEqual(new Uint8Array([255,0,10]));expect(new Uint8Array(await pandasTimestampOriginalResponseFile(run).arrayBuffer())).toEqual(run.responseBytes);
    run.value={...run.value,http_status:400,phase:"apply",apply_returned:false,error_type:"TypeError",error:"full message"};expect(()=>pandasTimestampOriginalArtifactFile(run,"filtered_table_json")).toThrow("unavailable");expect(await pandasTimestampOriginalArtifactFile(run,"source_after_table_json").text()).toBe(run.value.source_after_table_json);
  });
  it("binds source, operation and descriptor identity to each request",()=>{
    const form=defaultPandasTimestampOriginalForm(),identity={source:source(),form},gate=new PandasTimestampOriginalRequestGate(),ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);
    for(const changed of [{...identity,source:source()},{...identity,form:{...form,operation:"apply" as const}},{...identity,form:{...form,callJson:form.callJson+" "}}])expect(gate.accepts(ticket,changed)).toBe(false);gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);
  });
});
describe("pandas original timestamp rendering (SSR only; browser events accepted separately)",()=>{
  it("keeps the friendly default and isolates the original editor to pandas",()=>{
    const friendly=render(createElement(TableTimestampFilterWorkspace,{engine:"pandas"}));expect(friendly).toContain("Pandas timestamp call mode");expect(friendly).not.toContain("Complete original pandas timestamp call JSON");
    const polars=render(createElement(TableTimestampFilterWorkspace,{engine:"polars",initialCallMode:"original"}));expect(polars).not.toContain("Pandas timestamp call mode");expect(polars).not.toContain("Complete original pandas timestamp call JSON");
    const html=render(createElement(TableTimestampFilterWorkspace,{engine:"pandas",initialCallMode:"original"}));for(const operation of PANDAS_TIMESTAMP_ORIGINAL_OPERATIONS)expect(html).toContain(PANDAS_TIMESTAMP_ORIGINAL_LABELS[operation]);
    for(const label of ["Original pandas timestamp source table","Complete original pandas timestamp call JSON","Import original pandas timestamp call JSON file","Typed UTC Timestamp bounds","Ordered enum and string keys","Download current pandas timestamp call","Run original pandas timestamp call"])expect(html).toContain(label);expect(html).toContain('disabled=""');
    expect(render(createElement(PandasTimestampOriginalCallWorkspace,{initialForm:pandasTimestampOriginalExample("apply")}))).toContain("&quot;args&quot;: []");
  });
  it("offers every native artifact and genuine filtered-table reuse only on success",()=>{
    const html=render(createElement(PandasTimestampOriginalInspection,{run:result(),onUseTable:()=>{}}));for(const label of ["Download exact original pandas timestamp call","Download native timestamp call projection","Download complete original timestamp source table","Download original timestamp source after processing","Download complete original timestamp controls before processing","Download complete original timestamp controls after processing","Download original filtered native timestamp table","Use filtered timestamp table as source","Download complete original pandas timestamp response"])expect(html).toContain(label);
    const failed=result();failed.value={...failed.value,http_status:400,phase:"apply",apply_returned:false,result:null,filtered_table_json:null,error_type:"Exception",error:"apply method not available for timestamp filter"};
    const error=render(createElement(PandasTimestampOriginalInspection,{run:failed,onUseTable:()=>{}}));expect(error).toContain("Exception: apply method not available");expect(error).not.toContain("Use filtered timestamp table as source");expect(error).toContain("Download original timestamp source after processing");
    const changed=result();changed.value={...changed.value,source_unchanged:false,controls_unchanged:false};const flags=render(createElement(PandasTimestampOriginalInspection,{run:changed}));expect(flags).toContain("Complete source table changed.");expect(flags).toContain("Complete caller controls changed.");
  });
});
