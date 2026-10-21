import {afterEach,expect,it,vi} from "vitest";
import {defaultLogAttributesForm,LOG_ATTRIBUTES_OPERATIONS,isLogAttributesEnvelope,logAttributesFields,requestLogAttributes,logAttributesArtifactFile,logAttributesResponseFile,openLogAttributes,LogAttributesRequestGate,type LogAttributesEnvelope,type LogAttributesRun,LOG_ATTRIBUTES_ARTIFACTS} from "../src/log-attributes-filters";
export function attributesEnvelope():LogAttributesEnvelope {return {family:"LOG_ATTRIBUTES_FILTER",operation:"traces",http_status:200,phase:"complete",source_format:"native_log",source:{traces:2,events:3},result:{traces:1,events:2},source_unchanged:true,activity_key:"concept:name",filter_attribute_key:"kind",filtered_log_json:'{"large":9007199254740993}',filtered_xes:"<log/>",xes_error:null,error:null,source_log_json:'{"source":"complete"}',source_after_log_json:'{"source":"complete"}',parameters_before_json:'{"params":"before"}',parameters_after_json:'{"params":"after"}',parameters_unchanged:false,variants_source_log_json:null,variants_source_after_log_json:null,variants_source_unchanged:null,threshold_counts_json:null,threshold_variants_map_json:null,variant_counts_json:null,error_type:null};}
export function attributesRun(value=attributesEnvelope()):LogAttributesRun {const raw=JSON.stringify(value);return {value,raw,sourceFile:new File(["source"],"source.json"),variantsFile:null,fields:[],responseBytes:new TextEncoder().encode(raw)};}
afterEach(()=>vi.unstubAllGlobals());
it("validates the exact26field native envelope including all error snapshots",()=>{
 expect(Object.keys(attributesEnvelope())).toHaveLength(26);expect(isLogAttributesEnvelope(attributesEnvelope())).toBe(true);
 for(const patch of [{extra:1},{operation:"wrong"},{parameters_unchanged:1},{variants_source_log_json:4},{source:{traces:-1,events:2}},{error_type:{native_bytes:"hex-v1",data:"f"}},{result:{traces:1.5,events:2}}])expect(isLogAttributesEnvelope({...attributesEnvelope(),...patch})).toBe(false);
 const e=attributesEnvelope();for(const key of ["source_log_json","parameters_before_json","threshold_counts_json"]as const){const copy={...e};delete(copy as Partial<LogAttributesEnvelope>)[key];expect(isLogAttributesEnvelope(copy)).toBe(false);}
 expect(isLogAttributesEnvelope({...e,http_status:400,phase:"apply",error_type:"TypeError",error:{native_bytes:"hex-v1",data:"ff00eda080"},result:null})).toBe(true);
});
for(const operation of LOG_ATTRIBUTES_OPERATIONS)it(`sends only active ${operation} controls`,()=>{
 const f={...defaultLogAttributesForm(operation),activityKeyMode:"custom"as const,activityKey:" role\n\0Ω ",attributeKeyMode:"custom"as const,attributeKey:"",caseKeyMode:"custom"as const,caseKey:" custom\0 ",glueMode:"custom"as const,glue:"",once:"false"as const,stream1:true,stream2:true};
 const fields=Object.fromEntries(logAttributesFields(f));expect(fields.activity_key).toBe(f.activityKey);expect(fields.attribute_key).toBe(operation==="top_activities"?undefined:"");expect(fields.positive).toBe(operation==="threshold"?undefined:"true");
 expect(fields.case_id_key).toBe(["events","numeric_traces","numeric_events","relative","top_activities"].includes(operation)?f.caseKey:undefined);
 expect(fields.case_id_glue).toBe(operation==="numeric_traces"?"":undefined);expect(fields.stream_filter_key1).toBe(operation==="numeric_traces"?f.streamKey1:undefined);
 expect(fields.keep_once_per_case).toBe(operation==="relative"?"false":undefined);
 expect(fields.counts_json).toBe(operation==="threshold"?f.countsJson:undefined);
 expect(fields.lower).toBe(operation.startsWith("numeric_")?"0":undefined);
 expect(fields.values_json).toBe(["traces","events","trace_values"].includes(operation)?'["A"]':undefined);
});
it("preserves exactly one values collection and precision without eager JSON normalization",()=>{
 const f=defaultLogAttributesForm();const typed='{"type":"list","value":[{"type":"integer","value":"9007199254740993"},{"type":"na"}]}';const fields=Object.fromEntries(logAttributesFields({...f,valuesMode:"typed",typedValuesJson:typed}));expect(fields.typed_values_json).toBe(typed);expect(fields).not.toHaveProperty("values_json");
 expect(Object.fromEntries(logAttributesFields({...f,operation:"numeric_events",lower:"9007199254740993",upper:'{"type":"integer","value":"18446744073709551615"}'}))).toMatchObject({lower:"9007199254740993",upper:'{"type":"integer","value":"18446744073709551615"}'});
});
it("omits inactive CSV controls and retains empty literal CSV settings",()=>{const f={...defaultLogAttributesForm(),csvOptions:true,csv:{case_id_column:"",activity_column:" work\n",timestamp_column:"time",delimiter:";"},trimFields:false};expect(Object.fromEntries(logAttributesFields(f))).toMatchObject({...f.csv,trim_fields:"false"});expect(Object.fromEntries(logAttributesFields({...f,sourceFormat:"xes"}))).not.toHaveProperty("delimiter");});
it("uploads actual source and auxiliary Files with unchanged threshold controls",async()=>{
 const source=new File(["actual source"],"source.json"),aux=new File(["actual cached traces"],"aux.json"),form=defaultLogAttributesForm("threshold");
 vi.stubGlobal("fetch",vi.fn(async(url:string,init:RequestInit)=>{expect(url).toBe("/api/reference-log-attributes");const body=init.body as FormData;expect(await(body.get("source_file")as File).text()).toBe("actual source");expect(await(body.get("variants_file")as File).text()).toBe("actual cached traces");expect(body.get("variant_traces_json")).toBe(form.variantTracesJson);return new Response(JSON.stringify({...attributesEnvelope(),operation:"threshold"}));}));
 const result=await requestLogAttributes(source,form,aux);expect(result.sourceFile).toBe(source);expect(result.variantsFile).toBe(aux);
 await expect(requestLogAttributes(source,form)).rejects.toThrow("auxiliary");
});
it("preserves source and mutable parameters on400 and rejects wrong operation/status",async()=>{
 const source=new File(["x"],"x.csv"),form=defaultLogAttributesForm();
 const failure={...attributesEnvelope(),http_status:400,phase:"apply",error:"failure",error_type:"TypeError",result:null};
 vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(failure),{status:400})));expect((await requestLogAttributes(source,form)).value.source_log_json).toBe(failure.source_log_json);
 vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify({...attributesEnvelope(),operation:"events"}))));await expect(requestLogAttributes(source,form)).rejects.toThrow("another filter");
 vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(failure))));await expect(requestLogAttributes(source,form)).rejects.toThrow("invalid native envelope");
});
it("rejects invalid UTF8 and unsupported HTTP transport",async()=>{const f=new File(["x"],"x.csv");vi.stubGlobal("fetch",vi.fn(async()=>new Response(new Uint8Array([255]))));await expect(requestLogAttributes(f,defaultLogAttributesForm())).rejects.toThrow("safe JSON UTF-8");vi.stubGlobal("fetch",vi.fn(async()=>new Response("down",{status:503})));await expect(requestLogAttributes(f,defaultLogAttributesForm())).rejects.toThrow("HTTP 503");});
it("downloads exact complete native bytes and all source/cache/control artifacts on errors",async()=>{
 const v=attributesEnvelope();for(const key of LOG_ATTRIBUTES_ARTIFACTS)v[key]={native_bytes:"hex-v1",data:"ff00eda080"};const run=attributesRun(v);
 for(const key of LOG_ATTRIBUTES_ARTIFACTS)expect(new Uint8Array(await logAttributesArtifactFile(run,key).arrayBuffer())).toEqual(new Uint8Array([255,0,237,160,128]));
 expect(new Uint8Array(await logAttributesResponseFile(run).arrayBuffer())).toEqual(run.responseBytes);
 run.value.http_status=400;run.value.error="failed";expect(()=>logAttributesArtifactFile(run,"filtered_log_json")).toThrow();expect(()=>logAttributesArtifactFile(run,"source_after_log_json")).not.toThrow();
});
it("reopens the actual exported XES with literal activity selector and avoids lossy keys",()=>{const run=attributesRun();run.value.activity_key=" work\0Ω ";const open=vi.fn();openLogAttributes(run,open);expect(open.mock.calls[0][0]).toBeInstanceOf(File);expect(open.mock.calls[0][1]).toBe(" work\0Ω ");run.value.activity_key={native_bytes:"hex-v1",data:"ff"};open.mockClear();openLogAttributes(run,open);expect(open.mock.calls[0]).toHaveLength(1);});
it("invalidates late results on either upload, hidden controls, edits, cancellation and abort",()=>{const gate=new LogAttributesRequestGate(),identity={source:new File(["x"],"s"),variants:null,form:defaultLogAttributesForm()},ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);expect(gate.accepts(ticket,{...identity,source:new File(["x"],"s")})).toBe(false);expect(gate.accepts(ticket,{...identity,variants:new File(["x"],"v")})).toBe(false);for(const key of ["criterion","variantCountsJson","streamValue1","valuesJson","caseKey"]as const)expect(gate.accepts(ticket,{...identity,form:{...identity.form,[key]:"changed"}})).toBe(false);const controller=new AbortController();controller.abort();expect(gate.accepts(ticket,identity,controller.signal)).toBe(false);gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);});
it("uses genuine typed list-of-tuple pairs for threshold defaults",()=>{const form=defaultLogAttributesForm("threshold");for(const value of [form.countsJson,form.variantTracesJson]){const parsed=JSON.parse(value);expect(parsed.type).toBe("list");expect(parsed.value[0].type).toBe("tuple");expect(parsed.value[0].value).toHaveLength(2);}expect(JSON.parse(form.variantTracesJson).value[0].value[1]).toEqual({type:"list",value:[{type:"integer",value:"0"}]});});
