import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { ContextBuilder } from "../src/components/ContextBuilder";
import { PromptChat } from "../src/components/AskModel";
import { contextDefaults,LOG_CONTEXTS,OCEL_CONTEXTS,isLlmContext,requestLlmContext } from "../src/llm-context";
import { type LogQuerySchema } from "../src/log-query";
import { requestKnowledgePreview } from "../src/llm";
const schema:LogQuerySchema={family:"LOG_QUERY_SCHEMA",source_id:"source α",event_count:3,case_count:2,table_name:"dataframe",event_index_column:"__minera_event_index",columns:["activity"],sqlite_available:true,duckdb_configured:false};
const result={family:"LLM_CONTEXT",variant:"dfg",source_id:schema.source_id,text:"Open -> Review α ( frequency = 1 )",characters:33,truncated:false};
afterEach(()=>vi.unstubAllGlobals());
describe("native log and model context delivery",()=>{
 it("retains all thirteen public context families with independent source choices",()=>{expect(LOG_CONTEXTS).toHaveLength(10);expect(OCEL_CONTEXTS).toHaveLength(3);expect(new Set([...LOG_CONTEXTS,...OCEL_CONTEXTS].map(item=>item.id)).size).toBe(13);expect(contextDefaults(true).variant).toBe("ocel");});
 it("guards context variant source IDs and explicit truncation",()=>{expect(isLlmContext(result)).toBe(true);expect(isLlmContext({...result,variant:"unknown"})).toBe(false);expect(isLlmContext({...result,characters:-1})).toBe(false);expect(isLlmContext({...result,truncated:"yes"})).toBe(false);});
 it("serializes every visible control and binds a cached source revision",async()=>{
  const fetch=vi.fn(async(_url:string,_options:RequestInit)=>({ok:true,json:async()=>result}));vi.stubGlobal("fetch",fetch);const options={...contextDefaults(),relative_frequency:true,primary_aggregation:"median",secondary_aggregation:"max"};const signal=new AbortController().signal;
  expect(await requestLlmContext(options,schema,null,signal)).toEqual(result);const [url,request]=fetch.mock.calls[0];expect(url).toBe("/api/llm-context?source_id=source+%CE%B1");expect(request.signal).toBe(signal);const form=request.body as FormData;expect(form.get("relative_frequency")).toBe("true");expect(form.get("primary_aggregation")).toBe("median");expect(form.get("secondary_aggregation")).toBe("max");expect(form.get("include_header")).toBe("true");
 });
 it("posts the actual immutable OCEL file and exact object type",async()=>{
  const file=new File(['{"events":[],"objects":[]}'],"objects-α.json",{lastModified:7,type:"application/json"});const source=`ocel:${file.name}:${file.size}:7`;const fetch=vi.fn(async(_url:string,_options:RequestInit)=>({ok:true,json:async()=>({...result,variant:"ocel_features",source_id:source})}));vi.stubGlobal("fetch",fetch);
  await requestLlmContext({...contextDefaults(true),variant:"ocel_features",object_type:"Order",include_lifecycle_paths:false},null,file);const form=fetch.mock.calls[0][1].body as FormData;expect((form.get("ocel") as File).name).toBe(file.name);expect(form.get("object_type")).toBe("Order");expect(form.get("include_lifecycle_paths")).toBe("false");
 });
 it("rejects absent sources and invalid controls before sending",async()=>{const fetch=vi.fn();vi.stubGlobal("fetch",fetch);await expect(requestLlmContext(contextDefaults(),null,null)).rejects.toThrow(/CSV/);await expect(requestLlmContext(contextDefaults(true),schema,null)).rejects.toThrow(/OCEL/);await expect(requestLlmContext({...contextDefaults(),max_len:0},schema,null)).rejects.toThrow(/budget/);expect(fetch).not.toHaveBeenCalled();});
 it("rejects stale results and surfaces native errors",async()=>{vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,json:async()=>({...result,source_id:"changed"})})));await expect(requestLlmContext(contextDefaults(),schema,null)).rejects.toThrow(/stale/);vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,status:400,json:async()=>({error:"case index is outside the log"})})));await expect(requestLlmContext({...contextDefaults(),variant:"case"},schema,null)).rejects.toThrow(/outside/);});
 it("normalizes multipart line endings to the text displayed and sent by the editor",async()=>{vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,json:async()=>({family:"KNOWLEDGE_INJECTION",variant:"process_mining",computed:true,prompt:"Section\nLine one\r\nLine two\rEnd",input_bytes:30,error:""})})));const preview=await requestKnowledgePreview({knowledge_variant:"process_mining",knowledge_description:"Line one\nLine two",knowledge_query:"Question"});expect(preview.prompt).toBe("Section\nLine one\nLine two\nEnd");});
 it("provides native log/model controls and a saved-export source guard",()=>{const markup=renderToStaticMarkup(createElement(ContextBuilder,{available:false,mining:false,onUse:vi.fn()}));expect(markup).toContain("Petri net");expect(markup).toContain("Temporal profile");expect(markup).toContain("native context");expect(markup).toContain("disabled");});
 it("makes the same editable prompt workflow available to OCEL",()=>{const file=new File(["{}"],"sample-ocel.json");const markup=renderToStaticMarkup(createElement(PromptChat,{mining:false,available:true,ocelFile:file}));expect(markup).toContain("Events and objects");expect(markup).toContain("Object features");expect(markup).toContain("Object-centric directly-follows graph");expect(markup).toContain("Prompt to send");expect(markup).toContain("sample-ocel.json");});
});
