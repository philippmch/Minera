import {afterEach,describe,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {LogTimestampOriginalCallWorkspace,LogTimestampInspection} from "../src/components/LogTimestampOriginalCallWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {logTimestampCallFile} from "../src/log-timestamp-original-call";
import {nativeStringBytes} from "../src/native-multipart";
import {LOG_TIMESTAMP_OPERATIONS,LOG_TIMESTAMP_KEYS,LOG_TIMESTAMP_PREDICATES,defaultLogTimestampForm,logTimestampExample,logTimestampFields,logTimestampCallText,logTimestampSourceExample,isLogTimestampEnvelope,isLogTimestampControls,logTimestampBoolean,requestLogTimestamp,logTimestampCanReuse,logTimestampArtifactFile,logTimestampResponseFile,logTimestampCountText,LogTimestampRequestGate,type LogTimestampEnvelope,type LogTimestampRun} from "../src/log-timestamp-original-call";

const source=()=>new File(['{"traces":[]}'],"native-source.json");
const controls=JSON.stringify({schema:"minera-log-timestamp-native-controls-v1",source_argument:{kind:"log",source_trace_index:null,log_json:'{"traces":[]}',trace_json:null},arguments:[],options:{timestamp_key_json:"{}",enum_timestamp_key_json:"{}",positive_json:"{}"}});
const boolCell=(v:boolean)=>JSON.stringify({columns:[""],dtypes:["object"],rows:[[{key:"",kind:"boolean",value:String(v),typed_value:v}]]});
function envelope(call=defaultLogTimestampForm().callJson):LogTimestampEnvelope{return {family:"REFERENCE_LOG_TIMESTAMP",operation:"apply_events",http_status:200,phase:"complete",source_format:"native_log",source:{traces:3,events:6},result:{traces:2,events:4},source_unchanged:true,call_mode:"original",call_json:call,controls_before_json:controls,controls_after_json:controls,controls_unchanged:true,source_log_json:'{"traces":[]}',source_after_log_json:'{"traces":[]}',filtered_log_json:'{"traces":[]}',filtered_xes:"<log/>",xes_error:null,value_json:null,error_type:null,error:null,native_returned:true,result_kind:"log",call_projection_json:"{}",inspection_error:null};}
function run(value=envelope()):LogTimestampRun{return {value,sourceFile:source(),fields:[],raw:JSON.stringify(value),responseBytes:new TextEncoder().encode(JSON.stringify(value))};}
function failure():LogTimestampEnvelope{return {...envelope(),http_status:400,phase:"native_call",result:null,filtered_log_json:null,filtered_xes:null,error_type:"KeyError",error:"'missing'",native_returned:false,result_kind:null,source_after_log_json:'{"traces":[{"changed":true}]}',source_unchanged:false,controls_after_json:controls.replace('"traces":[]','"traces":[{}]'),controls_unchanged:false};}
function respond(value:LogTimestampEnvelope){vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));}
afterEach(()=>vi.unstubAllGlobals());
describe("timestamp original calls and actual return artifacts",()=>{
  it("exposes all13 calls with exact non-source arities and real trace selection",()=>{
    const arities=[0,0,2,2,4,2,2,2,2,5,3,3,3];expect(LOG_TIMESTAMP_OPERATIONS).toHaveLength(13);
    LOG_TIMESTAMP_OPERATIONS.forEach((op,i)=>{const f=logTimestampExample(op),v=JSON.parse(f.callJson);expect(Object.keys(v)).toEqual(["schema","args","keyword_entries","source_trace_index"]);expect(v.schema).toBe("minera-log-timestamp-original-call-v1");expect(v.args).toHaveLength(arities[i]);expect(v.source_trace_index).toBe(LOG_TIMESTAMP_PREDICATES.includes(op)?0:null);expect(logTimestampFields(f)).toEqual([["operation",op],["call_json",f.callJson]]);});
  });
  it("imports BOM, CRLF and wide integer bytes without JSON parsing and rejects invalid UTF8",async()=>{
    const raw='\uFEFF{\r\n"wide":18446744073709551615\r\n}\r\n';expect(await logTimestampCallText(new File([raw],"call.json"))).toBe(raw);
    await expect(logTimestampCallText(new File([new Uint8Array([255])],"bad.json"))).rejects.toThrow("valid UTF-8");
    expect(logTimestampFields({operation:"apply",callJson:"malformed"})[1][1]).toBe("malformed");
  });
  it("downloads the same diagnostic bytes as submission for edited lone surrogates",async()=>{
    const callJson='\ud800\r\n',file=logTimestampCallFile({operation:"apply",callJson});expect(new Uint8Array(await file.arrayBuffer())).toEqual(nativeStringBytes(callJson));
  });
  it("requires the actual source and sends only the three frozen fields as byte controls",async()=>{
    const mock=vi.fn();vi.stubGlobal("fetch",mock);await expect(requestLogTimestamp(null as unknown as File,logTimestampExample("is_contained"))).rejects.toThrow("Predicates also require");expect(mock).not.toHaveBeenCalled();
    const raw='\uFEFF{\r\n"n":9007199254740993\r\n}',form={operation:"apply_events" as const,callJson:raw};respond(envelope(raw));const file=source();await requestLogTimestamp(file,form);
    const call=vi.mocked(fetch).mock.calls[0];expect(call[0]).toBe("/api/reference-log-timestamp");const body=call[1]!.body as FormData;expect([...body.keys()]).toEqual(["source_file","operation","call_json"]);expect(body.get("source_file")).toBe(file);const uploaded=body.get("call_json") as File;expect(uploaded.name).toBe("call_json.txt");expect(new Uint8Array(await uploaded.arrayBuffer())).toEqual(new TextEncoder().encode(raw));
  });
  it("validates exact25 fields, actual controls, source captures and normal-return state",()=>{
    const v=envelope();expect(Object.keys(v).sort()).toEqual([...LOG_TIMESTAMP_KEYS].sort());expect(isLogTimestampEnvelope(v)).toBe(true);expect(isLogTimestampEnvelope({...v,extra:null})).toBe(false);expect(isLogTimestampEnvelope({...v,native_returned:false})).toBe(false);expect(isLogTimestampEnvelope({...v,source:{traces:-1,events:0}})).toBe(false);expect(isLogTimestampEnvelope({...v,controls_after_json:null})).toBe(false);expect(isLogTimestampEnvelope({...v,controls_before_json:'{"raw":"descriptor mirror"}'})).toBe(false);expect(isLogTimestampEnvelope({...v,filtered_xes:null})).toBe(false);expect(isLogTimestampEnvelope({...v,filtered_xes:null,xes_error:"secondary XES failure"})).toBe(true);
    const c=JSON.parse(controls);c.source_argument={kind:"trace",source_trace_index:0,log_json:null,trace_json:"{}"};expect(isLogTimestampControls(JSON.stringify(c))).toBe(true);c.source_argument.log_json="{}";expect(isLogTimestampControls(JSON.stringify(c))).toBe(false);
  });
  it.each([true,false])("retains the explicit %s result without inventing a filtered log",value=>{
    const v={...envelope(),operation:"is_contained" as const,result_kind:"bool" as const,result:null,filtered_log_json:null,filtered_xes:null,value_json:boolCell(value)};expect(isLogTimestampEnvelope(v)).toBe(true);expect(logTimestampBoolean(v.value_json)).toBe(value);expect(logTimestampCanReuse(v)).toBe(false);expect(isLogTimestampEnvelope({...v,filtered_log_json:"{}"})).toBe(false);expect(isLogTimestampEnvelope({...v,value_json:null})).toBe(false);
    const html=renderToStaticMarkup(createElement(LogTimestampInspection,{run:run(v),onUseLog:()=>{}}));expect(html).toContain(`>${String(value)}</strong>`);expect(html).toContain("Download typed Boolean result");expect(html).not.toContain("Use returned native log as source");
  });
  it("decodes a raw full native bool cell, including false",()=>{
    const raw={native_byte_strings:"hex-v1",native_schema:"event-table-raw-v1",value:{columns:[""],rows:[[{typed_value:{index:3,value:false}}]]}};expect(logTimestampBoolean(JSON.stringify(raw))).toBe(false);expect(logTimestampBoolean('{"rows":[[{"typed_value":0}]]}')).toBeNull();
  });
  it("retains mutating-error snapshots and 500 inspection failures independently",async()=>{
    const v=failure();expect(isLogTimestampEnvelope(v)).toBe(true);expect(logTimestampCanReuse(v)).toBe(false);respond(v);const result=await requestLogTimestamp(source(),defaultLogTimestampForm());expect(await logTimestampArtifactFile(result,"source_after_log_json").text()).toContain("changed");expect(await logTimestampArtifactFile(result,"controls_after_json").text()).toBe(v.controls_after_json);
    const internal={...v,http_status:500 as const,inspection_error:"controls capture failed",controls_after_json:null,controls_unchanged:null};expect(isLogTimestampEnvelope(internal)).toBe(true);respond(internal);expect((await requestLogTimestamp(source(),defaultLogTimestampForm())).value.error).toBe("'missing'");
    const afterReturn={...envelope(),http_status:500 as const,phase:"result_export" as const,error_type:"internal_error",error:"export failed",inspection_error:"control snapshot failed"};expect(isLogTimestampEnvelope(afterReturn)).toBe(true);const result500=run(afterReturn);expect(await logTimestampArtifactFile(result500,"filtered_log_json").text()).toBe(afterReturn.filtered_log_json);expect(logTimestampCanReuse(afterReturn)).toBe(false);
  });
  it("matches the full call, operation and HTTP status, allowing only early-control nulls",async()=>{
    respond({...envelope(),call_json:defaultLogTimestampForm().callJson+" "});await expect(requestLogTimestamp(source(),defaultLogTimestampForm())).rejects.toThrow("another original call");
    respond({...envelope(),operation:"apply"});await expect(requestLogTimestamp(source(),defaultLogTimestampForm())).rejects.toThrow("another operation");
    const early={...failure(),phase:"control" as const,operation:null,call_mode:null,call_json:null};respond(early);expect((await requestLogTimestamp(source(),defaultLogTimestampForm())).value.call_mode).toBeNull();
  });
  it("preserves native hex artifacts and exact response downloads and allows genuine log reuse",async()=>{
    const v={...envelope(),filtered_log_json:{native_bytes:"hex-v1" as const,data:"ff0041"}};const r=run(v);expect(new Uint8Array(await logTimestampArtifactFile(r,"filtered_log_json").arrayBuffer())).toEqual(new Uint8Array([255,0,65]));r.responseBytes=new Uint8Array([239,187,191,123,125,13,10]);expect(new Uint8Array(await logTimestampResponseFile(r).arrayBuffer())).toEqual(r.responseBytes);
    expect(logTimestampCanReuse(v)).toBe(true);const next=logTimestampArtifactFile(r,"filtered_log_json");respond(envelope());await requestLogTimestamp(next,defaultLogTimestampForm());expect((vi.mocked(fetch).mock.calls[0][1]!.body as FormData).get("source_file")).toBe(next);
  });
  it("retains uint64 count tokens and invalidates stale source/call/operation generations",async()=>{
    const raw=JSON.stringify(envelope()).replace('"traces":3','"traces":18446744073709551615');vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw)));expect(logTimestampCountText((await requestLogTimestamp(source(),defaultLogTimestampForm())).value.source!,"traces")).toBe("18446744073709551615");
    const gate=new LogTimestampRequestGate(),identity={source:source(),form:defaultLogTimestampForm()},ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);expect(gate.accepts(ticket,{...identity,source:source()})).toBe(false);expect(gate.accepts(ticket,{...identity,form:{...identity.form,callJson:identity.form.callJson+" "}})).toBe(false);const controller=new AbortController();controller.abort();expect(gate.accepts(ticket,identity,controller.signal)).toBe(false);gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);
  });
  it("provides actual stored dates in the raw-log example",async()=>{
    const log=JSON.parse(await logTimestampSourceExample().text());const date=log.traces[0].events[0].attributes[1];expect(date.kind_index).toBe(1);expect(date.typed_value).toEqual({index:4,value:{value:"2026-06-01T00:00:01",resolution_index:0,timezone:null}});expect(log.traces[0].properties).toHaveLength(1);
  });
  it("uses UTC-aware bounds for the four directly compared predicate examples",()=>{
    for(const operation of LOG_TIMESTAMP_PREDICATES){const args=JSON.parse(logTimestampExample(operation).callJson).args;const bounds=operation==="has_attribute_in_timeframe"?args.slice(2,4):args.slice(0,2);expect(bounds.map((v:{iso:string;offset_seconds:number})=>[v.iso,v.offset_seconds])).toEqual([["2026-06-01T00:00:01Z",0],["2026-06-01T00:00:03Z",0]]);}
  });
  it("adds a dedicated family while preserving friendly filters and offers all13 operations",()=>{
    const html=renderToStaticMarkup(createElement(LogTimestampOriginalCallWorkspace));for(const operation of LOG_TIMESTAMP_OPERATIONS)expect(html).toContain(`(${operation})`);expect(html).toContain("Download current timestamp call");expect(html).toContain('disabled=""');expect(html).toContain("Imported or edited text stays unchanged");
    expect(renderToStaticMarkup(createElement(ReferenceLogFilterWorkspace,{initialLogFamily:"timestamp"}))).toContain("Complete original timestamp call JSON");expect(renderToStaticMarkup(createElement(ReferenceLogFilterWorkspace))).toContain("Apply log filter");
  });
  it("shows all available error artifacts and preserves an empty native diagnostic",()=>{
    const v={...failure(),error:""};const html=renderToStaticMarkup(createElement(LogTimestampInspection,{run:run(v),onUseLog:()=>{}}));expect(html).toContain("KeyError: ");expect(html).toContain("Download complete source after processing");expect(html).toContain("Download actual controls after processing");expect(html).toContain("changed; inspect");expect(html).not.toContain("Use returned native log as source");
  });
});
