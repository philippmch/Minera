import {afterEach,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {LogAttributesFilterWorkspace,LogAttributesFilterInspection} from "../src/components/LogAttributesFilterWorkspace";
import {defaultLogAttributesForm,logAttributesOriginalExample,logAttributesCallFileText,logAttributesFields,requestLogAttributes,logAttributesArtifactFile,LogAttributesRequestGate,type LogAttributesEnvelope,type LogAttributesRun,LOG_ATTRIBUTES_OPERATIONS} from "../src/log-attributes-filters";
import {nativeStringBytes} from "../src/native-multipart";

afterEach(()=>vi.unstubAllGlobals());
const call=' {\r\n"schema":"minera-log-attributes-original-call-v1","args":[{"type":"integer","value":"9007199254740993"}],"parameter_entries":[]\r\n} ';
function envelope(raw:string):LogAttributesEnvelope{return {family:"LOG_ATTRIBUTES_FILTER",operation:"top_activities",http_status:400,phase:"apply",source_format:"native_log",source:{traces:1,events:1},result:null,source_unchanged:true,activity_key:"work",filter_attribute_key:null,source_log_json:"source",source_after_log_json:"source",parameters_before_json:"[]",parameters_after_json:"[injected]",parameters_unchanged:false,variants_source_log_json:null,variants_source_after_log_json:null,variants_source_unchanged:null,threshold_counts_json:null,threshold_variants_map_json:null,variant_counts_json:null,filtered_log_json:null,filtered_xes:null,xes_error:null,error_type:"TypeError",error:"original diagnostic",call_mode:"original",call_json:raw,threshold_cache_before_json:null,threshold_cache_after_json:null};}
it("uploads exact original call bytes and actual source without friendly defaults",async()=>{
 const source=new File(["actual native log"],"source.json"),form={...defaultLogAttributesForm("top_activities"),callMode:"original"as const,callJson:call,csvOptions:true,positive:false};
 expect(logAttributesFields(form)).toEqual([["operation","top_activities"],["call_json",call]]);
 vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{const body=init.body as FormData;expect([...body.keys()].sort()).toEqual(["call_json","operation","source_file"]);expect(await(body.get("source_file")as File).text()).toBe("actual native log");const raw=body.get("call_json")as File;expect(raw.name).toBe("call_json.txt");expect(new Uint8Array(await raw.arrayBuffer())).toEqual(nativeStringBytes(call));return new Response(JSON.stringify(envelope(call)),{status:400});}));
 const run=await requestLogAttributes(source,form);expect(run.value.parameters_unchanged).toBe(false);expect(await logAttributesArtifactFile(run,"call_json").text()).toBe(call);
});
it("rejects an unrelated exact call, friendly mode and selector projection",async()=>{
 const source=new File(["source"],"source.json"),form={...defaultLogAttributesForm("top_activities"),callMode:"original"as const,callJson:call};
 for(const patch of [{call_json:call+" "},{call_mode:"friendly",call_json:null},{filter_attribute_key:"concept:name"}]){
  vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify({...envelope(call),...patch}),{status:400})));
  await expect(requestLogAttributes(source,form)).rejects.toThrow(/another|friendly selector/);
 }
});
it("allows a threshold call using source traces without auxiliary File",async()=>{
 const source=new File(["source"],"source.json"),form={...defaultLogAttributesForm("threshold"),callMode:"original"as const,callJson:logAttributesOriginalExample("threshold")};
 vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{expect((init.body as FormData).has("variants_file")).toBe(false);return new Response(JSON.stringify({...envelope(form.callJson),operation:"threshold",threshold_cache_before_json:"full cache",threshold_cache_after_json:"full cache"}),{status:400});}));
 const run=await requestLogAttributes(source,form);expect(await logAttributesArtifactFile(run,"threshold_cache_after_json").text()).toBe("full cache");
});
it("imports UTF8 without normalizing BOM, CRLF or large integer tokens",async()=>{
 const file=new File([new Uint8Array([239,187,191]),call],"call.json");expect(await logAttributesCallFileText(file)).toBe("\ufeff"+call);
 await expect(logAttributesCallFileText(new File([new Uint8Array([255])],"invalid.json"))).rejects.toThrow("valid UTF-8");
});
it("renders original editor for every operation with optional full external cache",()=>{
 for(const operation of LOG_ATTRIBUTES_OPERATIONS){const html=renderToStaticMarkup(createElement(LogAttributesFilterWorkspace,{initialForm:{...defaultLogAttributesForm(operation),callMode:"original"}}));expect(html).toContain("Complete original attributes call JSON");expect(html).toContain("Import original attributes call JSON file");expect(html).not.toContain("Attributes source format");expect(html).not.toContain("Allowed string values as JSON");expect(html).not.toContain("Inclusive lower bound");if(operation==="threshold"){expect(html).toContain("auxiliary_trace_index");expect(html).toContain("source_trace_index");}}
});
it("keeps exact call and both full materialized cache artifacts available after an error",()=>{
 const value={...envelope(call),threshold_cache_before_json:"complete before",threshold_cache_after_json:"complete after"},run:LogAttributesRun={value,sourceFile:new File(["x"],"x"),variantsFile:null,fields:[],raw:"",responseBytes:new Uint8Array()};
 const html=renderToStaticMarkup(createElement(LogAttributesFilterInspection,{run}));for(const text of ["exact original attributes call","complete threshold cache before filtering","complete threshold cache after filtering","Parameters changed","original diagnostic"])expect(html).toContain(text);
});
it("invalidates response identities on call edits and mode changes",()=>{
 const gate=new LogAttributesRequestGate(),identity={source:new File(["x"],"x"),variants:null,form:{...defaultLogAttributesForm(),callMode:"original"as const,callJson:call}},ticket=gate.issue(identity);
 expect(gate.accepts(ticket,{...identity,form:{...identity.form,callJson:call+" "}})).toBe(false);expect(gate.accepts(ticket,{...identity,form:{...identity.form,callMode:"friendly"}})).toBe(false);
});
