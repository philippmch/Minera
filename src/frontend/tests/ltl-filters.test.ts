import {afterEach,expect,it,vi} from "vitest";
import {defaultLtlFilterForm,ltlFilterFields,ltlFilterOperations,ltlOriginalCallJson,ltlOriginalCallFileText,isLtlFilterEnvelope,requestLtlFilter,ltlFilterArtifactFile,ltlFilterResponseFile,LtlFilterRequestGate,openLtlFilteredLog,type LtlFilterEnvelope,type LtlFilterRun} from "../src/ltl-filters";
const file=()=>new File(['{"complete":"source"}'],"source.json",{lastModified:123});
function envelope(patch:Partial<LtlFilterEnvelope>={}):LtlFilterEnvelope{return {family:"REFERENCE_LTL_FILTER",call_mode:"friendly",call_json:null,engine:"polars",input_kind:patch.engine&&patch.engine!=="polars"?null:"LazyFrame",operation:"eventually",result_kind:"table",http_status:200,phase:"complete",apply_returned:true,source_format:"native_table",source:{rows:4,columns:5},result:{rows:2,columns:5},source_unchanged:true,activity_key:null,case_id_key:"case:concept:name",attribute_key:"concept:name",resource_key:"org:resource",timestamp_key:"time:timestamp",source_json:'{"all":"before"}',source_after_json:'{"all":"after"}',result_json:'{"large":9007199254740993,"unsigned":18446744073709551615,"zero":-0.0}',filtered_xes:null,xes_error:null,helper_result_json:null,error_type:null,error:null,...patch};}
function run(value=envelope()):LtlFilterRun{const raw=JSON.stringify(value);return {value,raw,responseBytes:new TextEncoder().encode(raw),sourceFile:file(),fields:[]};}
const fields=(engine:"log"|"pandas"|"polars",operation:"eventually"|"next"|"four_eyes"|"different_persons"|"timestamp_ge"|"timestamp_le"="eventually")=>Object.fromEntries(ltlFilterFields(defaultLtlFilterForm(engine,operation)));
afterEach(()=>vi.unstubAllGlobals());
it("uses each public operand shape and keeps helpers exclusive to logs",()=>{
 expect(ltlFilterOperations("log")).toHaveLength(6);expect(ltlFilterOperations("pandas")).toHaveLength(4);expect(ltlFilterOperations("polars")).toHaveLength(4);
 for(const engine of ["log","pandas"]as const){expect(fields(engine,"next")).toMatchObject({a_json:'{"type":"string","value":"A"}',b_json:'{"type":"string","value":"B"}',c_json:'{"type":"string","value":"C"}'});expect(fields(engine,"next")).not.toHaveProperty("values_json");expect(fields(engine,"four_eyes")).not.toHaveProperty("c_json");}
 for(const operation of ["eventually","next","four_eyes"]as const){expect(fields("polars",operation)).toHaveProperty("values_json");expect(fields("polars",operation)).not.toHaveProperty("a_json");}
 expect(fields("polars","different_persons")).toHaveProperty("target_json");expect(()=>fields("polars","timestamp_ge")).toThrow("available");
});
it("preserves exact typed operands and literal selectors with active controls only",()=>{
 const typed='{"type":"list","value":[{"type":"integer","value":"9007199254740993"},{"type":"null"}]}';
 const form={...defaultLtlFilterForm("polars"),valuesKind:"typed"as const,typedValuesJson:typed,attributeKeyMode:"custom"as const,attributeKey:" \r\nΩ\0 ",caseKeyMode:"custom"as const,caseKey:"",resourceKeyMode:"custom"as const,resourceKey:"inactive",timestampKeyMode:"custom"as const,timestampKey:"inactive"};
 expect(Object.fromEntries(ltlFilterFields(form))).toEqual({engine:"polars",operation:"eventually",input_kind:"LazyFrame",positive:"true",typed_values_json:typed,attribute_key:form.attributeKey,case_id_key:"",timestamp_key:"inactive"});
 const helper={...form,engine:"log"as const,operation:"timestamp_ge"as const};expect(Object.keys(Object.fromEntries(ltlFilterFields(helper)))).toEqual(["engine","operation","helper_a_json","helper_b_json"]);
});
it("keeps omission and explicit timestamp boundaries distinct",()=>{
 const form={...defaultLtlFilterForm("pandas"),boundariesMode:"supplied"as const,boundariesJson:'{"type":"null"}',timestampKeyMode:"custom"as const,timestampKey:""};
 expect(Object.fromEntries(ltlFilterFields(form))).not.toHaveProperty("enable_timestamp");
 const enabled=Object.fromEntries(ltlFilterFields({...form,enableTimestamp:"true"}));expect(enabled).toMatchObject({enable_timestamp:"true",timestamp_key:"",timestamp_diff_boundaries_json:form.boundariesJson});
 const disabled=Object.fromEntries(ltlFilterFields({...form,enableTimestamp:"false"}));expect(disabled.enable_timestamp).toBe("false");expect(disabled.timestamp_diff_boundaries_json).toBe(form.boundariesJson);
});
it("requires the full fixed native envelope and exact nonnegative count ranges",()=>{
 expect(isLtlFilterEnvelope(envelope())).toBe(true);
 for(const patch of [{extra:1},{source:{rows:-1,columns:1}},{source:{rows:1.5,columns:2}},{source:{rows:1,columns:2,events:3}},{source:{rows:18446744073709551616,columns:1}},{apply_returned:"yes"},{error:{}},{result_kind:"mapping"},{engine:"bad"},{http_status:500}])expect(isLtlFilterEnvelope({...envelope(),...patch})).toBe(false);
 const {source_after_json:_missing,...incomplete}=envelope();expect(isLtlFilterEnvelope(incomplete)).toBe(false);
});
it("uploads real Files, preserves binary-safe literal fields and validates request identity",async()=>{
 const source=file(),form={...defaultLtlFilterForm("polars"),attributeKeyMode:"custom"as const,attributeKey:" exact\r\nΩ\0 "};
 const fetch=vi.fn(async(url:string,init:RequestInit)=>{expect(url).toBe("/api/reference-ltl-filter");const body=init.body as FormData;expect(body.get("source_file")).toBeInstanceOf(File);expect(await(body.get("source_file")as File).text()).toBe(await source.text());expect(await(body.get("attribute_key")as Blob).text()).toBe(form.attributeKey);return new Response(JSON.stringify(envelope()));});vi.stubGlobal("fetch",fetch);
 expect((await requestLtlFilter(source,form)).sourceFile).toBe(source);await expect(requestLtlFilter({name:"fake"}as File,form)).rejects.toThrow("actual source File");expect(fetch).toHaveBeenCalledOnce();
});
it("runs helpers without a source upload or irrelevant controls",async()=>{
 const form=defaultLtlFilterForm("log","timestamp_ge");vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{expect((init.body as FormData).has("source_file")).toBe(false);expect((init.body as FormData).has("positive")).toBe(false);return new Response(JSON.stringify(envelope({engine:"log",operation:"timestamp_ge",source:null,result:null,source_format:null,source_json:null,source_after_json:null,result_json:null,result_kind:"helper",helper_result_json:'{"typed":"complete result"}'})));}));
 expect((await requestLtlFilter(file(),form)).sourceFile).toBeNull();
});
it("keeps complete 400 diagnostics and rejects cross-request envelopes",async()=>{
 let value=envelope({http_status:400,phase:"collect",result:null,result_json:null,error_type:"ColumnNotFoundError",error:"complete\0error"});vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));
 expect((await requestLtlFilter(file(),defaultLtlFilterForm("polars"))).value.error).toBe(value.error);
 value=envelope({engine:"pandas"});await expect(requestLtlFilter(file(),defaultLtlFilterForm("polars"))).rejects.toThrow("another engine");value=envelope({operation:"next"});await expect(requestLtlFilter(file(),defaultLtlFilterForm("polars"))).rejects.toThrow("another engine");
});
it("retains whole artifact bytes and prevents failed result downloads",async()=>{
 const result=run();expect(await ltlFilterArtifactFile(result,"result_json").text()).toBe(result.value.result_json);expect(await ltlFilterResponseFile(result).text()).toBe(result.raw);
 result.value.http_status=400;result.value.error="failed";expect(()=>ltlFilterArtifactFile(result,"result_json")).toThrow("unavailable");expect(await ltlFilterArtifactFile(result,"source_after_json").text()).toBe(result.value.source_after_json);
});
it("rejects stale results after edits, cancellation or source identity changes",()=>{
 const gate=new LtlFilterRequestGate(),identity={source:file(),form:defaultLtlFilterForm("pandas")},first=gate.issue(identity);expect(gate.accepts(first,identity)).toBe(true);expect(gate.accepts(first,{...identity,source:file()})).toBe(false);expect(gate.accepts(first,{...identity,form:{...identity.form,positive:false}})).toBe(false);gate.invalidate();expect(gate.accepts(first,identity)).toBe(false);const next=gate.issue(identity),controller=new AbortController();controller.abort();expect(gate.accepts(next,identity,controller.signal)).toBe(false);
});

it("opens actual exported XES with the original activity role",async()=>{
 const value=run(envelope({engine:"log",result_kind:"log",activity_key:" literal\nΩ\0 ",filtered_xes:"<log>complete</log>"})),open=vi.fn();openLtlFilteredLog(value,open);expect(open).toHaveBeenCalledOnce();const [exported,key]=open.mock.calls[0];expect(exported).toBeInstanceOf(File);expect(await(exported as File).text()).toBe("<log>complete</log>");expect(key).toBe(value.value.activity_key);
 value.value.xes_error="projection failed";expect(()=>openLtlFilteredLog(value,open)).toThrow("unavailable");
});
it("rejects unsafe UTF-8 and malformed JSON before accepting response artifacts",async()=>{
 vi.stubGlobal("fetch",vi.fn(async()=>new Response(new Uint8Array([0xff,0x7b,0x7d]))));await expect(requestLtlFilter(file(),defaultLtlFilterForm("polars"))).rejects.toThrow("safe JSON UTF-8");
 vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"family":')));await expect(requestLtlFilter(file(),defaultLtlFilterForm("polars"))).rejects.toThrow("invalid native envelope");
});

it("exposes Polars timestamp ordering and ignores inert difference options",()=>{
 for(const operation of ["eventually","next"]as const){const selected=Object.fromEntries(ltlFilterFields({...defaultLtlFilterForm("polars",operation),timestampKeyMode:"custom",timestampKey:" exact\n\0 ",enableTimestamp:"true",boundariesMode:"supplied"}));expect(selected.timestamp_key).toBe(" exact\n\0 ");expect(selected).not.toHaveProperty("enable_timestamp");expect(selected).not.toHaveProperty("timestamp_diff_boundaries_json");}
 const social=Object.fromEntries(ltlFilterFields({...defaultLtlFilterForm("polars","four_eyes"),timestampKeyMode:"custom",timestampKey:"inactive"}));expect(social).not.toHaveProperty("timestamp_key");
});

it("transports the selected Polars input type and ignores it for other engines",()=>{
 expect(fields("polars").input_kind).toBe("LazyFrame");
 const eager={...defaultLtlFilterForm("polars"),polarsInputKind:"DataFrame"as const};
 expect(Object.fromEntries(ltlFilterFields(eager)).input_kind).toBe("DataFrame");
 for(const engine of ["log","pandas"]as const)expect(Object.fromEntries(ltlFilterFields({...eager,engine}))).not.toHaveProperty("input_kind");
 expect(()=>ltlFilterFields({...eager,polarsInputKind:"invalid"as never})).toThrow("Polars DataFrame or LazyFrame");
});

it("rejects a response from the other input type on success or apply failure",async()=>{
 const eager={...defaultLtlFilterForm("polars"),polarsInputKind:"DataFrame"as const};
 let value=envelope({input_kind:"DataFrame"});
 vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{expect((init.body as FormData).get("input_kind")).toBe("DataFrame");return new Response(JSON.stringify(value),{status:value.http_status});}));
 expect((await requestLtlFilter(file(),eager)).value.input_kind).toBe("DataFrame");
 value=envelope();await expect(requestLtlFilter(file(),eager)).rejects.toThrow("another Polars input type");
 value=envelope({http_status:400,phase:"apply",input_kind:"LazyFrame",error_type:"AttributeError",error:"other input",result:null,result_json:null});
 await expect(requestLtlFilter(file(),eager)).rejects.toThrow("another Polars input type");
 value=envelope({http_status:400,phase:"control",input_kind:null,error_type:"native_error",error:"control failure",result:null,result_json:null});
 expect((await requestLtlFilter(file(),eager)).value.error).toBe("control failure");
 expect(isLtlFilterEnvelope(envelope({input_kind:"other"as never}))).toBe(false);
 expect(isLtlFilterEnvelope(envelope({engine:"pandas",input_kind:"DataFrame"}))).toBe(false);
});

it("invalidates pending results when the Polars input type changes",()=>{
 const gate=new LtlFilterRequestGate(),identity={source:file(),form:defaultLtlFilterForm("polars")};
 const ticket=gate.issue(identity);
 expect(gate.accepts(ticket,{...identity,form:{...identity.form,polarsInputKind:"DataFrame"}})).toBe(false);
});

const originalForm=()=>({...defaultLtlFilterForm("polars"),callMode:"original"as const});
it("keeps original calls exclusive and distinguishes omitted from explicit null operands",()=>{
 const form={...originalForm(),originalOperand:"omitted"as const,positive:false,attributeKeyMode:"custom"as const,attributeKey:"inert"};
 expect(Object.fromEntries(ltlFilterFields(form))).toEqual({engine:"polars",operation:"eventually",input_kind:"LazyFrame",call_json:'{"schema":"minera-polars-ltl-original-call-v1"}'});
 expect(ltlOriginalCallJson({...form,originalOperand:"provided",originalOperandJson:'{"type":"null"}'})).toBe('{"schema":"minera-polars-ltl-original-call-v1","values":{"type":"null"}}');
 for(const engine of ["log","pandas"]as const)expect(()=>ltlFilterFields({...form,engine})).toThrow("require the Polars");
 expect(()=>ltlFilterFields({...form,callMode:"unknown"as never})).toThrow("call");
 expect(()=>ltlOriginalCallJson({...form,originalOperand:"unknown"as never})).toThrow("operand and parameter modes");
});

it("retains original editor fragments and ordered enum/string entries without rewriting numbers",()=>{
 const operand=' \r\n{"type":"integer","value":9007199254740993} ',raw=' {"type":"boolean","value":false}\n';
 const entries='[ {"key":"positive","enum_key":false,"value":{"type":"integer","value":"0"}}, {"key":"positive","enum_key":true,"value":{"type":"integer","value":"1"}} ]';
 const form={...originalForm(),originalOperandJson:operand,originalParameters:"raw"as const,originalParameterJson:raw};
 expect(ltlOriginalCallJson(form)).toBe(`{"schema":"minera-polars-ltl-original-call-v1","values":${operand},"parameter_value":${raw}}`);
 expect(ltlOriginalCallJson({...form,originalParameters:"entries",originalEntriesJson:entries})).toBe(`{"schema":"minera-polars-ltl-original-call-v1","values":${operand},"parameter_entries":${entries}}`);
 expect(ltlOriginalCallJson({...form,originalParameters:"entries",originalEntriesJson:"[]"})).toContain('"parameter_entries":[]');
 expect(ltlOriginalCallJson({...form,originalParameters:"omitted"})).not.toContain('"parameter_');
 const malformed=' {"schema":"minera-polars-ltl-original-call-v1","values":9007199254740993,"values":null}\r\n';
 expect(ltlOriginalCallJson({...form,originalEditor:"json",originalCallJson:malformed})).toBe(malformed);
 expect(ltlOriginalCallJson({...form,originalEditor:"json",originalCallJson:""})).toBe("");
});

it("imports original JSON Files with exact UTF8 text and rejects replacement decoding",async()=>{
 const bytes=new Uint8Array([0xef,0xbb,0xbf,...new TextEncoder().encode(' {"values":9007199254740993}\r\n')]);
 expect(await ltlOriginalCallFileText(new File([bytes],"original.json"))).toBe('\ufeff {"values":9007199254740993}\r\n');
 await expect(ltlOriginalCallFileText(new File([new Uint8Array([0xff])],"invalid.json"))).rejects.toThrow("valid UTF-8");
 await expect(ltlOriginalCallFileText({name:"fake"}as File)).rejects.toThrow("actual");
});

it("uploads only original call controls with canonical byte fields and exact response identity",async()=>{
 const call='\r\n{"schema":"minera-polars-ltl-original-call-v1","values":{"type":"integer","value":9007199254740993}}\n';
 const form={...originalForm(),originalEditor:"json"as const,originalCallJson:call};
 let value=envelope({call_mode:"original",call_json:call});
 vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{
  const body=init.body as FormData,control=body.get("call_json")as File;
  expect(control).toBeInstanceOf(File);expect(control.name).toBe("call_json.txt");expect(await control.text()).toBe(call);
  expect([...body.keys()].sort()).toEqual(["call_json","engine","input_kind","operation","source_file"]);
  return new Response(JSON.stringify(value),{status:value.http_status});
 }));
 expect((await requestLtlFilter(file(),form)).value.call_json).toBe(call);
 value=envelope();await expect(requestLtlFilter(file(),form)).rejects.toThrow("another call mode");
 value=envelope({call_mode:"original",call_json:call.trim()});await expect(requestLtlFilter(file(),form)).rejects.toThrow("another original call");
 value=envelope({call_mode:"original",call_json:call,http_status:400,phase:"control",result:null,result_json:null,error:"invalid original call"});
 expect((await requestLtlFilter(file(),form)).value.error).toBe("invalid original call");
 value=envelope({call_mode:null,call_json:null,http_status:400,phase:"control",result:null,result_json:null,error:"early rejection"});
 expect((await requestLtlFilter(file(),form)).value.error).toBe("early rejection");
});

it("requires the complete27field envelope and rejects cross-mode/error call echoes",async()=>{
 const call=ltlOriginalCallJson(originalForm()),valid=envelope({call_mode:"original",call_json:call});
 expect(isLtlFilterEnvelope(valid)).toBe(true);expect(Object.keys(valid)).toHaveLength(27);
 for(const patch of [{call_mode:"unknown"},{call_json:undefined},{call_mode:"original",call_json:null},{call_mode:"friendly",call_json:call},{call_mode:null,call_json:call},{engine:"pandas",input_kind:null}])expect(isLtlFilterEnvelope({...valid,...patch})).toBe(false);
 const {call_mode:_mode,...old}=valid;expect(isLtlFilterEnvelope(old)).toBe(false);
 let value=valid;vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));
 await expect(requestLtlFilter(file(),defaultLtlFilterForm("polars"))).rejects.toThrow("another call mode");
 value=envelope({call_mode:null,http_status:400,phase:"apply",result:null,result_json:null,error:"lost mode"});
 await expect(requestLtlFilter(file(),originalForm())).rejects.toThrow("another call mode");
 value=envelope({call_mode:"original",call_json:call+" ",http_status:400,phase:"apply",result:null,result_json:null,error:"wrong call"});
 await expect(requestLtlFilter(file(),originalForm())).rejects.toThrow("another original call");
});

it("keeps original call downloads available on failure and invalidates all original editor changes",async()=>{
 const call=' {"schema":"minera-polars-ltl-original-call-v1"}\r\n';
 const failed=run(envelope({call_mode:"original",call_json:call,http_status:400,phase:"apply",error:"missing operand",result:null,result_json:null}));
 expect(await ltlFilterArtifactFile(failed,"call_json").text()).toBe(call);
 expect(()=>ltlFilterArtifactFile(failed,"result_json")).toThrow("unavailable");
 const gate=new LtlFilterRequestGate(),identity={source:file(),form:originalForm()},ticket=gate.issue(identity);
 for(const patch of [{callMode:"friendly"as const},{originalEditor:"json"as const},{originalOperand:"omitted"as const},{originalOperandJson:'{"type":"null"}'},{originalParameters:"raw"as const},{originalParameterJson:'{"type":"boolean","value":false}'},{originalEntriesJson:'[ ]'},{originalCallJson:'{}'}])expect(gate.accepts(ticket,{...identity,form:{...identity.form,...patch}})).toBe(false);
});

it("compares and downloads original native byte echoes without replacement text",async()=>{
 const form={...originalForm(),originalEditor:"json"as const,originalCallJson:"\ud800"};
 const value=envelope({call_mode:"original",call_json:{native_bytes:"hex-v1",data:"eda080"},http_status:400,phase:"control",result:null,result_json:null,error:"invalid JSON"});
 vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{
  const control=(init.body as FormData).get("call_json")as File;
  expect(control.name).toBe("call_json.txt");expect([...new Uint8Array(await control.arrayBuffer())]).toEqual([0xed,0xa0,0x80]);
  return new Response(JSON.stringify(value),{status:400});
 }));
 const result=await requestLtlFilter(file(),form);
 expect([...new Uint8Array(await ltlFilterArtifactFile(result,"call_json").arrayBuffer())]).toEqual([0xed,0xa0,0x80]);
});
