import {afterEach,describe,expect,it,vi} from "vitest";
import {defaultLogVariantsForm,LOG_VARIANTS_OPERATIONS,LOG_VARIANTS_KEYS,logVariantsFields,logVariantsOriginalExample,logVariantsCallFileText,isLogVariantsEnvelope,requestLogVariants,logVariantsArtifactFile,logVariantsResponseFile,logVariantsSourceExample,logVariantsCountText,LogVariantsRequestGate,type LogVariantsEnvelope,type LogVariantsRun} from "../src/log-variants-filters";

const source=()=>new File(['{"traces":[]}'],"actual-source.json",{type:"application/json"});
function envelope(call=defaultLogVariantsForm().callJson):LogVariantsEnvelope{return {family:"REFERENCE_LOG_VARIANTS",operation:"apply",http_status:200,phase:"complete",source_format:"native_log",source:{traces:3,events:6},result:{traces:2,events:4},source_unchanged:true,call_mode:"original",call_json:call,controls_before_json:'{"actual":"before"}',controls_after_json:'{"actual":"after"}',controls_unchanged:true,source_log_json:'{"traces":[]}',source_after_log_json:'{"traces":[]}',filtered_log_json:'{"traces":[]}',filtered_xes:"<log/>",xes_error:null,value_json:null,error_type:null,error:null};}
function run(value=envelope()):LogVariantsRun{return {value,sourceFile:source(),fields:logVariantsFields(defaultLogVariantsForm()),raw:JSON.stringify(value),responseBytes:new TextEncoder().encode(JSON.stringify(value))};}
function respond(value:LogVariantsEnvelope){vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status,headers:{"Content-Type":"application/json"}})));}
afterEach(()=>vi.unstubAllGlobals());

describe("original LOG variants wire and byte identity",()=>{
  it("exposes all seven public operations, source-first examples and original Packed dictionaries",()=>{
    expect(LOG_VARIANTS_OPERATIONS).toHaveLength(7);
    for(const operation of LOG_VARIANTS_OPERATIONS){const raw=logVariantsOriginalExample(operation),example=JSON.parse(raw);expect(Object.keys(example)).toEqual(["schema","args","keyword_entries"]);expect(example.schema).toBe("minera-log-variants-original-call-v1");expect(example.keyword_entries).toEqual([]);expect(logVariantsFields({operation,callJson:raw})).toEqual([["operation",operation],["call_json",raw]]);}
    const helper=JSON.parse(logVariantsOriginalExample("find_auto_threshold"));expect(helper.args).toHaveLength(2);expect(helper.args[0].type).toBe("dict");expect(helper.args[0].items[0][0].type).toBe("tuple");expect(helper.args[0].items[0][1].values).toEqual([{type:"trace_ref",source_trace_index:0},{type:"trace_ref",source_trace_index:2}]);
    expect(JSON.parse(logVariantsOriginalExample("apply")).args[0].values[0].type).toBe("tuple");
  });
  it("forwards raw malformed/empty call bytes exclusively and keeps large tokens and CRLF",async()=>{
    const raw='\r\n{"schema":"minera-log-variants-original-call-v1","args":[{"type":"int","value":9007199254740993}],"keyword_entries":[]}\r\n';
    const value=envelope(raw);respond(value);const file=source(),result=await requestLogVariants(file,{operation:"apply",callJson:raw});
    const mock=vi.mocked(fetch),init=mock.mock.calls[0][1]!,body=init.body as FormData;
    expect(mock.mock.calls[0][0]).toBe("/api/reference-log-variants");expect([...body.keys()]).toEqual(["source_file","operation","call_json"]);expect(body.get("source_file")).toBe(file);const callFile=body.get("call_json");expect(callFile).toBeInstanceOf(File);expect((callFile as File).name).toBe("call_json.txt");expect((callFile as File).type).toBe("application/octet-stream");expect(new Uint8Array(await (callFile as File).arrayBuffer())).toEqual(new TextEncoder().encode(raw));expect(result.value.call_json).toBe(raw);expect(logVariantsFields({operation:"apply",callJson:""})).toEqual([["operation","apply"],["call_json",""]]);
    expect(logVariantsFields({operation:"apply",callJson:"not JSON"})[1][1]).toBe("not JSON");
  });
  it("requires an actual source File even for the scalar helper",async()=>{
    const mock=vi.fn();vi.stubGlobal("fetch",mock);await expect(requestLogVariants(null as unknown as File,defaultLogVariantsForm("find_auto_threshold"))).rejects.toThrow("Every operation requires its source");expect(mock).not.toHaveBeenCalled();
  });
  it("imports strict UTF8 without dropping BOM, CRLF or number tokens and rejects replacement decoding",async()=>{
    const raw='\uFEFF{\r\n"value":9007199254740993\r\n}\r\n';expect(await logVariantsCallFileText(new File([raw],"call.json"))).toBe(raw);
    await expect(logVariantsCallFileText(new File([new Uint8Array([0xff,0xfe])],"invalid.json"))).rejects.toThrow("valid UTF-8");
  });
  it("accepts exactly21 fields and distinguishes complete log, scalar, error and secondary-XES results",()=>{
    const value=envelope();expect(Object.keys(value).sort()).toEqual([...LOG_VARIANTS_KEYS].sort());expect(isLogVariantsEnvelope(value)).toBe(true);
    expect(isLogVariantsEnvelope({...value,unexpected:null})).toBe(false);const missing={...value} as Partial<LogVariantsEnvelope>;delete missing.controls_after_json;expect(isLogVariantsEnvelope(missing)).toBe(false);
    expect(isLogVariantsEnvelope({...value,source:{traces:-1,events:0}})).toBe(false);expect(isLogVariantsEnvelope({...value,source:{traces:Infinity,events:0}})).toBe(false);expect(isLogVariantsEnvelope({...value,phase:"collect"})).toBe(false);
    expect(isLogVariantsEnvelope({...value,phase:{toString:"complete"}})).toBe(false);
    const scalar={...value,operation:"find_auto_threshold" as const,result:null,filtered_log_json:null,filtered_xes:null,value_json:'{"columns":["value"],"rows":[]}'};expect(isLogVariantsEnvelope(scalar)).toBe(true);expect(isLogVariantsEnvelope({...scalar,filtered_log_json:"{}"})).toBe(false);expect(isLogVariantsEnvelope({...scalar,value_json:null})).toBe(false);
    expect(isLogVariantsEnvelope({...value,filtered_xes:null,xes_error:"unsupported projection"})).toBe(true);expect(isLogVariantsEnvelope({...value,filtered_xes:null})).toBe(false);
    const failure={...value,http_status:400 as const,phase:"filter" as const,result:null,filtered_log_json:null,filtered_xes:null,error_type:"TypeError",error:"complete diagnostic"};expect(isLogVariantsEnvelope(failure)).toBe(true);expect(isLogVariantsEnvelope({...failure,value_json:"{}"})).toBe(false);
  });
  it("matches exact native call echo, operation, mode and HTTP status with only the early-control null exception",async()=>{
    const form=defaultLogVariantsForm();respond({...envelope(),call_json:form.callJson+" "});await expect(requestLogVariants(source(),form)).rejects.toThrow("another original call");
    respond({...envelope(),operation:"filter_variants_top_k"});await expect(requestLogVariants(source(),form)).rejects.toThrow("another operation");
    const failure:LogVariantsEnvelope={...envelope(),operation:null,http_status:400,phase:"control",call_mode:null,call_json:null,source_format:null,source:null,result:null,source_unchanged:null,controls_before_json:null,controls_after_json:null,controls_unchanged:null,source_log_json:null,source_after_log_json:null,filtered_log_json:null,filtered_xes:null,error_type:"ValueError",error:"early malformed field"};respond(failure);expect((await requestLogVariants(source(),form)).value.call_mode).toBeNull();
    respond({...failure,phase:"source_import"});await expect(requestLogVariants(source(),form)).rejects.toThrow("another operation");
  });
  it("retains exact uint64 count tokens from actual response bytes",async()=>{
    const raw=JSON.stringify(envelope()).replace('"traces":3','"traces":18446744073709551615');vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw,{status:200})));const result=await requestLogVariants(source(),defaultLogVariantsForm());expect(logVariantsCountText(result.value.source!,"traces")).toBe("18446744073709551615");
  });
  it("preserves native hex artifacts and actual response bytes; errors retain source/control downloads",async()=>{
    const value=envelope();value.filtered_log_json={native_bytes:"hex-v1",data:"ff0041"};const result=run(value);expect(new Uint8Array(await logVariantsArtifactFile(result,"filtered_log_json").arrayBuffer())).toEqual(new Uint8Array([255,0,65]));
    result.responseBytes=new Uint8Array([0xef,0xbb,0xbf,123,125,13,10]);expect(new Uint8Array(await logVariantsResponseFile(result).arrayBuffer())).toEqual(result.responseBytes);
    const failed=run({...envelope(),http_status:400,phase:"filter",filtered_log_json:null,filtered_xes:null,result:null,error_type:"KeyError",error:"'missing'"});expect(await logVariantsArtifactFile(failed,"source_after_log_json").text()).toBe(failed.value.source_after_log_json);expect(await logVariantsArtifactFile(failed,"controls_before_json").text()).toBe(failed.value.controls_before_json);expect(()=>logVariantsArtifactFile(failed,"filtered_log_json")).toThrow("unavailable");
  });
  it("reuses the full actual filtered native File as the next source without rebuilding its JSON",async()=>{
    const raw='{\r\n"traces":[],"metadata":{"large":9007199254740993}\r\n}';const first=run({...envelope(),filtered_log_json:raw});const reusable=logVariantsArtifactFile(first,"filtered_log_json");respond(envelope());const next=await requestLogVariants(reusable,defaultLogVariantsForm());expect(next.sourceFile).toBe(reusable);expect((vi.mocked(fetch).mock.calls[0][1]!.body as FormData).get("source_file")).toBe(reusable);expect(await reusable.text()).toBe(raw);
  });
  it("invalidates the whole call/source identity and aborts stale completions",()=>{
    const gate=new LogVariantsRequestGate(),file=source(),form=defaultLogVariantsForm(),identity={source:file,form},ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);
    expect(gate.accepts(ticket,{source:source(),form})).toBe(false);expect(gate.accepts(ticket,{source:file,form:{...form,callJson:form.callJson+" "}})).toBe(false);expect(gate.accepts(ticket,{source:file,form:defaultLogVariantsForm("find_auto_threshold")})).toBe(false);
    const abort=new AbortController();abort.abort();expect(gate.accepts(ticket,identity,abort.signal)).toBe(false);gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);
  });
  it("ships a genuine native example with separate full traces for all source references",async()=>{
    const file=logVariantsSourceExample(),log=JSON.parse(await file.text());expect(file).toBeInstanceOf(File);expect(log.traces.map((t:{events:{activity:string}[]})=>t.events.map(e=>e.activity))).toEqual([["A","B"],["A","C"],["A","B"]]);expect(log.traces.every((t:{properties:unknown[]})=>t.properties.length===1)).toBe(true);expect(log.metadata.properties).toHaveLength(1);
  });
});
