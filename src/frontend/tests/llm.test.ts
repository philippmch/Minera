import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { AskModel } from "../src/components/AskModel";
import { isLlmCompletion,isLlmRuntimeStatus,llmCompletionRequest,llmKnowledgeOptions,llmLogContext,requestKnowledgePreview,requestLlmCompletion,type LlmCompletion } from "../src/llm";
import { type MineraExport } from "../src/types";

const data={dataset:"fixture.csv",trace_count:4,event_count:24,activities:[{name:"Review α",count:12,start_count:4,end_count:0},{name:"Close",count:12,start_count:0,end_count:4}],edges:[{source:0,target:1,count:4}],variants:[{activities:[0,1],count:4}]} as MineraExport;
const result:LlmCompletion={family:"LLM_COMPLETION",provider:"openai",model:"fixture",text:"Summary α",finish_reason:"stop",usage:{input_tokens:9,output_tokens:3},response:{choices:[]}};
afterEach(()=>vi.unstubAllGlobals());
describe("language-model provider delivery",()=>{
  it("builds editable bounded log context with real counts and activity names",()=>{
    expect(llmLogContext(data)).toContain("Review α → Close: 4");expect(llmLogContext(data)).toContain("Cases: 4; events: 24");
    const many={...data,activities:Array.from({length:60},(_,i)=>({name:`activity ${i}`,count:1,start_count:0,end_count:0}))};
    expect(llmLogContext(many)).not.toContain("activity 59:");expect(llmLogContext(many)).toContain("up to 50");
  });
  it("retains the question in all three knowledge modes and requires database schema",()=>{
    expect(llmKnowledgeOptions("description","context","Question α","").knowledge_description).toContain("Question α");
    expect(llmKnowledgeOptions("process_mining","context","Question α","")).toMatchObject({knowledge_description:"context",knowledge_query:"Question α"});
    expect(()=>llmKnowledgeOptions("database","context","Question α","")).toThrow(/schema/);
    expect(llmKnowledgeOptions("database","context","Question α","events(activity TEXT)")).toMatchObject({knowledge_database_schema:"events(activity TEXT)"});
    expect(()=>llmKnowledgeOptions("process_mining","context"," ","")).toThrow(/question/);
  });
  it("honors model defaults and validates output controls before a provider call",()=>{
    expect(llmCompletionRequest("openai"," fixture ","Prompt α","256","")).toEqual({provider:"openai",model:"fixture",prompt:"Prompt α",max_tokens:256});
    expect(llmCompletionRequest("google","fixture","Prompt","256","0.5").temperature).toBe(0.5);
    expect(()=>llmCompletionRequest("anthropic","fixture","Prompt","1.5","")).toThrow(/whole number/);
    expect(()=>llmCompletionRequest("openai","fixture","Prompt","256","2.1")).toThrow(/Temperature/);
    expect(()=>llmCompletionRequest("openai","","Prompt","256","")).toThrow(/model name/);
  });
  it("guards nullable usage, refusal/empty text, duplicate providers and malformed results",()=>{
    expect(isLlmCompletion({...result,text:"",finish_reason:"refusal",usage:{input_tokens:null,output_tokens:null}})).toBe(true);
    expect(isLlmCompletion({...result,usage:{input_tokens:true,output_tokens:3}})).toBe(false);
    expect(isLlmCompletion({...result,response:[]})).toBe(false);
    expect(isLlmRuntimeStatus({family:"LLM_RUNTIME",ready:false,providers:[],error:"No runtime"})).toBe(true);
    const provider={id:"openai",configured:true,base_url:"http://127.0.0.1:8189"};
    expect(isLlmRuntimeStatus({family:"LLM_RUNTIME",ready:true,providers:[provider,provider]})).toBe(false);
  });
  it("previews knowledge in multipart bodies and sends exactly the displayed prompt as JSON",async()=>{
    const fetch=vi.fn(async(path:string,_options?:RequestInit)=>({ok:true,json:async()=>path.includes("knowledge")?{family:"KNOWLEDGE_INJECTION",variant:"process_mining",computed:true,prompt:"Prompt α",input_bytes:8,error:""}:result}));vi.stubGlobal("fetch",fetch);
    const signal=new AbortController().signal;
    await requestKnowledgePreview({knowledge_variant:"process_mining",knowledge_description:"Context α",knowledge_query:"Question"},signal);
    const options=fetch.mock.calls[0][1] as RequestInit;
    expect(fetch.mock.calls[0][0]).toBe("/api/knowledge-injection");expect(options.body).toBeInstanceOf(FormData);expect((options.body as FormData).get("knowledge_description")).toBe("Context α");
    expect(await requestLlmCompletion(llmCompletionRequest("openai","fixture","Prompt α","256",""),signal)).toEqual(result);
    expect(fetch.mock.calls[1][1]).toEqual({method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({provider:"openai",model:"fixture",prompt:"Prompt α",max_tokens:256}),signal});
  });
  it("shows a review step and disabled execution before a runtime is configured",()=>{
    const markup=renderToStaticMarkup(createElement(AskModel,{data,mining:false}));
    expect(markup).toContain("Preview prompt");expect(markup).toContain("Prompt to send");expect(markup).toContain("Send displayed prompt to");expect(markup).toContain("disabled");expect(markup).toContain("Connect a language-model runtime");
  });
});
