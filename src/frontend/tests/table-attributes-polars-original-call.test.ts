import {afterEach,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {TableAttributesFilterWorkspace,TableAttributesFilterInspection} from "../src/components/TableAttributesFilterWorkspace";
import {defaultTableAttributesForm,tableAttributesOriginalExample,tableAttributesCallFileText,tableAttributesFields,requestTableAttributes,tableAttributesArtifactFile,tableAttributesResponseFile,tableAttributesOperations,TableAttributesRequestGate,type TableAttributesEnvelope,type TableAttributesRun,type TableAttributesOperation} from "../src/table-attributes-filters";
import {nativeStringBytes} from "../src/native-multipart";

afterEach(()=>vi.unstubAllGlobals());
const raw=' {\r\n"schema":"minera-polars-attributes-original-call-v1","input_kind":"DataFrame","keyword_entries":[{"key":"int1","value":{"type":"integer","value":9007199254740993}},{"key":"int2","value":{"type":"integer","value":"18446744073709551615"}}]\r\n} ';
const form=(operation:TableAttributesOperation="numeric_events")=>({...defaultTableAttributesForm("polars"),operation,callMode:"original"as const,callJson:raw});
function envelope(call:string,patch:Partial<TableAttributesEnvelope>={}):TableAttributesEnvelope{return {family:"TABLE_ATTRIBUTES_FILTER",engine:"polars",operation:"numeric_events",http_status:400,phase:"collect",apply_returned:true,source:{rows:2,columns:3},result:null,source_unchanged:true,case_id_key:"",attribute_key:"",source_table_json:"complete source",source_after_table_json:"complete source",filtered_table_json:null,counts_table_json:null,result_kind:null,error_type:"InvalidOperationError",error:"complete collect diagnostic Ω\0tail",call_mode:"original",call_json:call,controls_before_json:"actual native controls",controls_after_json:"actual native controls",controls_unchanged:true,...patch};}
it("keeps both engines friendly by default with their own original schema examples",()=>{
 for(const engine of ["pandas","polars"]as const){const f=defaultTableAttributesForm(engine);expect(f.callMode).toBe("friendly");expect(JSON.parse(f.callJson).schema).toBe(`minera-${engine}-attributes-original-call-v1`);}
});
it("provides all nine Polars keyword examples without pandas positional arguments",()=>{
 const keys:Record<TableAttributesOperation,string[]>={traces:["values"],events:["values"],numeric_traces:["int1","int2","parameters"],numeric_events:["int1","int2","parameters"],threshold:["thresh"],top_activities:["max_no_activities"],on_values:["values"],relative:["min_relative_stake"],counts:["attribute_key"]};
 expect(tableAttributesOperations("polars")).toHaveLength(9);
 for(const operation of tableAttributesOperations("polars")){
  const call=JSON.parse(tableAttributesOriginalExample(operation,"polars"));expect(Object.keys(call)).toEqual(["schema","input_kind","keyword_entries"]);expect(call.schema).toBe("minera-polars-attributes-original-call-v1");expect(call.input_kind).toBe("LazyFrame");expect(call.keyword_entries.map((entry:{key:string})=>entry.key)).toEqual(keys[operation]);
  if(operation.startsWith("numeric_")){expect(call.keyword_entries[0].value.value).toBe("9007199254740992");expect(call.keyword_entries[2].value).toEqual({type:"parameter_entries",entries:[{key:"pm4py:param:attribute_key",enum_key:false,value:{type:"string",value:"signed"}}]});}
 }
});
it("forwards exact eager/lazy, keyword order and malformed descriptors without frontend coercion",()=>{
 for(const call of [raw,raw.replace("DataFrame","LazyFrame"),"",'{"schema":"wrong","args":[]}',"\ufeff"+raw])expect(tableAttributesFields({...form(),callJson:call,positive:false,countsMode:"supplied",caseKeyMode:"custom",caseKey:"inactive"})).toEqual([["engine","polars"],["operation","numeric_events"],["call_json",call]]);
});
it("uploads only the original Polars four fields and actual source bytes",async()=>{
 const source=new File(["whole source Ω\0\r\n"],"source.json");
 vi.stubGlobal("fetch",vi.fn(async(url:string,init:RequestInit)=>{expect(url).toBe("/api/reference-table-attributes");const body=init.body as FormData;expect([...body.keys()].sort()).toEqual(["call_json","engine","operation","source_file"]);expect(await(body.get("source_file")as File).text()).toBe("whole source Ω\0\r\n");const call=body.get("call_json")as File;expect(call.name).toBe("call_json.txt");expect(new Uint8Array(await call.arrayBuffer())).toEqual(nativeStringBytes(raw));return new Response(JSON.stringify(envelope(raw)),{status:400});}));
 const run=await requestTableAttributes(source,form());expect(run.sourceFile).toBe(source);expect(run.value.phase).toBe("collect");expect(run.value.apply_returned).toBe(true);expect(await tableAttributesArtifactFile(run,"call").text()).toBe(raw);
});
it("rejects original Polars responses from another engine, call or friendly selector",async()=>{
 for(const patch of [{engine:"pandas"as const},{call_json:raw+" "},{call_mode:"friendly"as const,call_json:null},{case_id_key:"case:concept:name"},{attribute_key:"concept:name"}]){vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(envelope(raw,patch)),{status:400})));await expect(requestTableAttributes(new File(["source"],"source.json"),form())).rejects.toThrow(/another|friendly selector/);}
});
it("accepts early control rejection with exact captured echo or null early collection identity",async()=>{
 for(const patch of [{phase:"control",apply_returned:false,source:null,source_unchanged:null,source_table_json:null,source_after_table_json:null,controls_before_json:null,controls_after_json:null,controls_unchanged:null},{phase:"control",apply_returned:false,engine:null,operation:null,call_mode:null,call_json:null,source:null,source_unchanged:null,source_table_json:null,source_after_table_json:null,controls_before_json:null,controls_after_json:null,controls_unchanged:null}]as Partial<TableAttributesEnvelope>[]){vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(envelope(raw,patch)),{status:400})));expect((await requestTableAttributes(new File(["source"],"source.json"),form())).value.phase).toBe("control");}
});
it("retains complete controls/source/call/response after a native Polars diagnostic",async()=>{
 const value=envelope(raw),responseBytes=new TextEncoder().encode(JSON.stringify(value)),run:TableAttributesRun={value,sourceFile:new File(["source"],"source.json"),fields:[],raw:"",responseBytes};
 for(const [kind,wanted]of [["source_after","complete source"],["controls_before","actual native controls"],["controls_after","actual native controls"],["call",raw]]as const)expect(await tableAttributesArtifactFile(run,kind).text()).toBe(wanted);
 expect(new Uint8Array(await tableAttributesResponseFile(run).arrayBuffer())).toEqual(responseBytes);const html=renderToStaticMarkup(createElement(TableAttributesFilterInspection,{run}));expect(html).toContain("Download exact original polars attributes call");expect(html).toContain("stage: collect");expect(html).not.toContain("Use filtered table as source");
});
it("exports true typed counts distinctly and does not present them as a reusable filtered table",async()=>{
 const value=envelope(raw,{operation:"counts",http_status:200,phase:"complete",apply_returned:true,error:null,error_type:null,result_kind:"counts",result:{rows:2,columns:2},counts_table_json:{native_bytes:"hex-v1",data:"ff00c328"}}),run:TableAttributesRun={value,sourceFile:new File(["source"],"source.json"),fields:[],raw:"",responseBytes:new Uint8Array()};
 expect(new Uint8Array(await tableAttributesArtifactFile(run,"counts").arrayBuffer())).toEqual(Uint8Array.of(255,0,195,40));expect(()=>tableAttributesArtifactFile(run,"result")).toThrow("unavailable");const html=renderToStaticMarkup(createElement(TableAttributesFilterInspection,{run,onUseTable:()=>{}}));expect(html).toContain("Dictionary entry order is not asserted");expect(html).toContain("Download complete typed attribute counts");expect(html).not.toContain("Use filtered table as source");
});
it("renders the original Polars editor for every method with active engine-specific guidance",()=>{
 for(const operation of tableAttributesOperations("polars")){const html=renderToStaticMarkup(createElement(TableAttributesFilterWorkspace,{engine:"polars",initialForm:{...form(operation),callJson:tableAttributesOriginalExample(operation,"polars")}}));for(const text of ["Complete original polars attributes call JSON","Import original polars attributes call JSON file","Load original polars call example","input_kind","keyword_entries","parameter_entries"])expect(html).toContain(text);for(const text of ["Complete original pandas","pandas_timestamp","Admitted attribute values as JSON","Lower numeric bound","Frequency activity column"])expect(html).not.toContain(text);}
});
it("imports Polars call text exactly and invalidates mode, engine, operation, input-kind and source changes",async()=>{
 expect(await tableAttributesCallFileText(new File([Uint8Array.of(239,187,191),raw],"call.json"))).toBe("\ufeff"+raw);await expect(tableAttributesCallFileText(new File([Uint8Array.of(255)],"bad.json"))).rejects.toThrow("valid UTF-8");
 const source=new File(["source"],"source.json"),identity={source,form:form()},gate=new TableAttributesRequestGate(),ticket=gate.issue(identity);
 for(const changed of [{...identity,source:new File(["source"],"source.json")},{...identity,form:{...identity.form,engine:"pandas"as const}},{...identity,form:{...identity.form,operation:"events"as const}},{...identity,form:{...identity.form,callMode:"friendly"as const}},{...identity,form:{...identity.form,callJson:raw.replace("DataFrame","LazyFrame")}}])expect(gate.accepts(ticket,changed)).toBe(false);
 const controller=new AbortController();controller.abort();expect(gate.accepts(ticket,identity,controller.signal)).toBe(false);
});
