import {afterEach,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {TableAttributesFilterWorkspace,TableAttributesFilterInspection} from "../src/components/TableAttributesFilterWorkspace";
import {defaultTableAttributesForm,tableAttributesOriginalExample,tableAttributesCallFileText,tableAttributesFields,requestTableAttributes,tableAttributesArtifactFile,tableAttributesOperations,isTableAttributesEnvelope,TableAttributesRequestGate,type TableAttributesEnvelope,type TableAttributesRun} from "../src/table-attributes-filters";
import {nativeStringBytes} from "../src/native-multipart";

afterEach(()=>vi.unstubAllGlobals());
const call=' {\r\n"schema":"minera-pandas-attributes-original-call-v1","args":[],"keyword_entries":[{"key":"max_no_activities","value":{"type":"integer","value":"9007199254740993"}}]\r\n} ';
function envelope(raw:string):TableAttributesEnvelope{return {family:"TABLE_ATTRIBUTES_FILTER",engine:"pandas",operation:"top_activities",http_status:400,phase:"apply",apply_returned:false,source:{rows:1,columns:2},result:null,source_unchanged:true,case_id_key:"",attribute_key:"",source_table_json:"complete source",source_after_table_json:"complete source",filtered_table_json:null,counts_table_json:null,result_kind:null,error_type:"TypeError",error:"full native diagnostic",call_mode:"original",call_json:raw,controls_before_json:"complete controls",controls_after_json:"complete controls",controls_unchanged:true};}
const form=()=>({...defaultTableAttributesForm("pandas"),operation:"top_activities"as const,callMode:"original"as const,callJson:call});
it("uploads exact original bytes and actual table without injecting friendly defaults",async()=>{
 const source=new File(["actual native table"],"source.json"),f={...form(),positive:false,countsMode:"supplied"as const,caseKeyMode:"custom"as const,caseKey:"inactive"};
 expect(tableAttributesFields(f)).toEqual([["engine","pandas"],["operation","top_activities"],["call_json",call]]);
 vi.stubGlobal("fetch",vi.fn(async(_url:string,init:RequestInit)=>{const body=init.body as FormData;expect([...body.keys()].sort()).toEqual(["call_json","engine","operation","source_file"]);expect(await(body.get("source_file")as File).text()).toBe("actual native table");const raw=body.get("call_json")as File;expect(raw.name).toBe("call_json.txt");expect(new Uint8Array(await raw.arrayBuffer())).toEqual(nativeStringBytes(call));return new Response(JSON.stringify(envelope(call)),{status:400});}));
 const run=await requestTableAttributes(source,f);expect(run.sourceFile).toBe(source);expect(await tableAttributesArtifactFile(run,"call").text()).toBe(call);
});
it("rejects unrelated call bytes, modes, projected selectors and incomplete controls envelopes",async()=>{
 for(const patch of [{call_json:call+" "},{call_mode:"friendly",call_json:null},{attribute_key:"concept:name"},{case_id_key:"case:concept:name"}]){
  vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify({...envelope(call),...patch}),{status:400})));
  await expect(requestTableAttributes(new File(["source"],"source.json"),form())).rejects.toThrow(/another|friendly selector/);
 }
 for(const patch of [{controls_before_json:12},{controls_after_json:{}},{controls_unchanged:"true"},{call_mode:"other"},{call_json:null},{extra:true}])expect(isTableAttributesEnvelope({...envelope(call),...patch})).toBe(false);
 const {controls_unchanged:_ignored,...missing}=envelope(call);expect(isTableAttributesEnvelope(missing)).toBe(false);
 expect(()=>tableAttributesFields({...form(),engine:"polars"})).toThrow("require pandas");
});
it("imports UTF8 without changing BOM, CRLF, whitespace or large integer tokens",async()=>{
 expect(await tableAttributesCallFileText(new File([Uint8Array.of(239,187,191),call],"call.json"))).toBe("\ufeff"+call);
 await expect(tableAttributesCallFileText(new File([Uint8Array.of(255)],"bad.json"))).rejects.toThrow("valid UTF-8");
});
it("offers an original editor for all eight pandas methods while friendly controls remain hidden",()=>{
 for(const operation of tableAttributesOperations("pandas")){
  const html=renderToStaticMarkup(createElement(TableAttributesFilterWorkspace,{engine:"pandas",initialForm:{...form(),operation,callJson:tableAttributesOriginalExample(operation)}}));
  for(const text of ["Complete original pandas attributes call JSON","Import original pandas attributes call JSON file","parameter_entries","keyword_entries"])expect(html).toContain(text);
  for(const text of ["Admitted attribute values as JSON","Lower numeric bound","Include missing values in matching","Frequency activity column"])expect(html).not.toContain(text);
 }
 expect(renderToStaticMarkup(createElement(TableAttributesFilterWorkspace,{engine:"polars"}))).not.toContain("Table attributes call mode");
});
it("keeps complete controls and exact call downloads available after a native error",async()=>{
 const value=envelope(call),run:TableAttributesRun={value,sourceFile:new File(["source"],"source.json"),fields:[],raw:"",responseBytes:new Uint8Array()};
 for(const [kind,wanted]of [["call",call],["controls_before","complete controls"],["controls_after","complete controls"]]as const)expect(await tableAttributesArtifactFile(run,kind).text()).toBe(wanted);
 const html=renderToStaticMarkup(createElement(TableAttributesFilterInspection,{run}));
 for(const text of ["Download exact original pandas attributes call","Download complete call controls before filtering","Download complete call controls after filtering","full native diagnostic"])expect(html).toContain(text);
 expect(html).not.toContain("Use filtered table as source");expect(html).not.toContain("attribute:");
});
it("reuses a complete filtered table as a File and invalidates original call/mode edits",async()=>{
 const value={...envelope(call),http_status:200 as const,error:null,error_type:null,apply_returned:true,phase:"complete",result:{rows:1,columns:2},result_kind:"table"as const,filtered_table_json:{native_bytes:"hex-v1"as const,data:"ff00c328"}},run:TableAttributesRun={value,sourceFile:new File(["x"],"x"),fields:[],raw:"",responseBytes:new Uint8Array()};
 expect(new Uint8Array(await tableAttributesArtifactFile(run,"result").arrayBuffer())).toEqual(Uint8Array.of(255,0,195,40));
 expect(renderToStaticMarkup(createElement(TableAttributesFilterInspection,{run,onUseTable:()=>{}}))).toContain("Use filtered table as source");
 const gate=new TableAttributesRequestGate(),identity={source:run.sourceFile,form:form()},ticket=gate.issue(identity);
 expect(gate.accepts(ticket,{...identity,form:{...identity.form,callJson:call+" "}})).toBe(false);
 expect(gate.accepts(ticket,{...identity,form:{...identity.form,callMode:"friendly"}})).toBe(false);
});
