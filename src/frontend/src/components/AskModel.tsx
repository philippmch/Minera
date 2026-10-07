import { useEffect,useRef,useState } from "react";
import { LLM_PROVIDERS,llmLogContext,llmKnowledgeOptions,llmCompletionRequest,requestLlmStatus,requestKnowledgePreview,requestLlmCompletion,type LlmProvider,type LlmRuntimeStatus,type LlmCompletion } from "../llm";
import { type KnowledgeInjectionVariant,type MineraExport } from "../types";
import { downloadModelJson } from "../model-view-data";
import { LlmStudies } from "./LlmStudies";

export function AskModel({data,mining,available=false,onOpenFiltered}:{data:MineraExport;mining:boolean;available?:boolean;onOpenFiltered?:((file:File)=>void)}){
 const [task,setTask]=useState<"prompt"|"clusters"|"hypotheses">("prompt");
 return <><label className="filters-field model-task"><span>Model task</span><select disabled={mining} value={task} onChange={event=>setTask(event.target.value as typeof task)}><option value="prompt">Ask a question</option><option value="clusters">Case clustering</option><option value="hypotheses">Hypotheses</option></select></label>{task==="prompt"?<PromptChat data={data} mining={mining}/>:<LlmStudies key={task} task={task} data={data} available={available} mining={mining} onOpenFiltered={onOpenFiltered}/>}</>;
}
function PromptChat({data,mining}:{data:MineraExport;mining:boolean}) {
  const [variant,setVariant]=useState<KnowledgeInjectionVariant>("process_mining");
  const [context,setContext]=useState(()=>llmLogContext(data));const [question,setQuestion]=useState("");const [schema,setSchema]=useState("");
  const [prompt,setPrompt]=useState("");const [provider,setProvider]=useState<LlmProvider>("openai");const [model,setModel]=useState("");
  const [maxTokens,setMaxTokens]=useState("4096");const [temperature,setTemperature]=useState("");
  const [status,setStatus]=useState<LlmRuntimeStatus|null>(null);const [result,setResult]=useState<LlmCompletion|null>(null);
  const [error,setError]=useState<string|null>(null);const [pending,setPending]=useState<"preview"|"completion"|null>(null);
  const request=useRef<AbortController|null>(null);const readiness=useRef<AbortController|null>(null);
  const refresh=async()=>{readiness.current?.abort();const controller=new AbortController();readiness.current=controller;try{const next=await requestLlmStatus(controller.signal);if(readiness.current===controller&&!controller.signal.aborted)setStatus(next);}catch(cause){if(!controller.signal.aborted)setStatus({family:"LLM_RUNTIME",ready:false,providers:[],error:cause instanceof Error?cause.message:String(cause)});}};
  useEffect(()=>{void refresh();return()=>{request.current?.abort();readiness.current?.abort();};},[]);
  useEffect(()=>{if(mining){request.current?.abort();request.current=null;setPending(null);setResult(null);}},[mining]);
  const invalidate=()=>{setPrompt("");setResult(null);setError(null);};
  const run=async(operation:"preview"|"completion")=>{
    if(mining||pending)return;
    let input;try{input=operation==="preview"?llmKnowledgeOptions(variant,context,question,schema):llmCompletionRequest(provider,model,prompt,maxTokens,temperature);}catch(cause){setError(cause instanceof Error?cause.message:String(cause));return;}
    const controller=new AbortController();request.current=controller;setPending(operation);setError(null);setResult(null);
    try{if(operation==="preview"){const next=await requestKnowledgePreview(input as ReturnType<typeof llmKnowledgeOptions>,controller.signal);if(!next.computed)throw new Error(next.error||"Could not build the prompt.");if(request.current===controller&&!controller.signal.aborted)setPrompt(next.prompt);}else{const next=await requestLlmCompletion(input as ReturnType<typeof llmCompletionRequest>,controller.signal);if(request.current===controller&&!controller.signal.aborted)setResult(next);}}
    catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(null);}}
  };
  const configured=status?.ready&&status.providers.some(item=>item.id===provider&&item.configured);
  const endpoint=status?.providers.find(item=>item.id===provider)?.base_url;
  const label=LLM_PROVIDERS.find(item=>item.id===provider)!.label;
  return <section className="panel ask-model-panel">
    <div className="panel-head"><h2>Ask a model</h2><span className="panel-meta">{data.dataset}</span></div>
    <p className="ask-model-intro">Build a prompt from the log or a database schema. Review and edit the displayed prompt before sending it.</p>
    <fieldset className="ask-model-fields" disabled={mining||pending!==null}>
      <label className="filters-field"><span>Prompt context</span><select value={variant} onChange={event=>{setVariant(event.target.value as KnowledgeInjectionVariant);invalidate();}}><option value="process_mining">Process mining</option><option value="description">General description</option><option value="database">Database schema</option></select></label>
      {variant==="database"?<label className="ask-model-text"><span>Database schema</span><textarea rows={6} value={schema} onChange={event=>{setSchema(event.target.value);invalidate();}} placeholder="Tables, columns and relationships" /></label>:<label className="ask-model-text"><span>Log context</span><textarea rows={8} value={context} onChange={event=>{setContext(event.target.value);invalidate();}} /><small>The initial context includes up to 50 activities, 50 directly-follows edges and 20 variants. You can edit it.</small></label>}
      <label className="ask-model-text"><span>Question or instruction</span><textarea rows={3} value={question} onChange={event=>{setQuestion(event.target.value);invalidate();}} placeholder="What should the model help you understand?" /></label>
      <button className="btn-ghost" onClick={()=>void run("preview")}>Preview prompt</button>
      <label className="ask-model-text"><span>Prompt to send</span><textarea rows={8} value={prompt} onChange={event=>{setPrompt(event.target.value);setResult(null);}} placeholder="Preview a prompt or enter your own" /></label>
      <div className="ask-model-options">
        <label className="filters-field"><span>Provider</span><select value={provider} onChange={event=>{setProvider(event.target.value as LlmProvider);setResult(null);}}>{LLM_PROVIDERS.map(item=><option key={item.id} value={item.id}>{item.label}</option>)}</select></label>
        <label className="filters-field ask-model-name"><span>Model name</span><input value={model} onChange={event=>{setModel(event.target.value);setResult(null);}} placeholder="A model available to your provider" /></label>
        <label className="filters-field"><span>Output token limit</span><input type="number" min="1" max="1000000" value={maxTokens} onChange={event=>setMaxTokens(event.target.value)} /></label>
        <label className="filters-field"><span>Temperature</span><input type="number" min="0" max="1" step="0.1" value={temperature} onChange={event=>setTemperature(event.target.value)} placeholder="Model default" /></label>
      </div>
      <p className="ask-model-intro">{configured?`${label} is configured at ${endpoint}. Only the displayed prompt is sent.`:status?.error||`${label} is not configured. Set its credential or local endpoint in the runtime environment.`}</p>
      <div className="ask-model-actions"><button className="btn-open" disabled={!configured||!prompt.trim()} onClick={()=>void run("completion")}>Send displayed prompt to {label}</button><button className="btn-ghost" onClick={()=>void refresh()}>Refresh runtime</button><button className="btn-ghost" disabled={!prompt} onClick={()=>downloadModelJson({prompt},"minera-prompt.json")}>Download prompt</button></div>
    </fieldset>
    {pending&&<p className="ask-model-status" role="status">{pending==="preview"?"Building prompt…":"Waiting for the model…"} <button className="btn-ghost" onClick={()=>{request.current?.abort();request.current=null;setPending(null);}}>Cancel</button></p>}
    {error&&<div className="error-banner" role="alert">{error}</div>}
    {!status?.ready&&<details className="ask-model-setup"><summary>Connect a language-model runtime</summary><p>Set OPENAI_API_KEY, ANTHROPIC_API_KEY or GEMINI_API_KEY in the runtime environment. For a local compatible service, configure its provider base URL.</p><pre>python scripts/llm_runtime.py --port 8189{"\n"}minera_server --llm-runtime-url http://127.0.0.1:8189</pre></details>}
    {result&&<article className="ask-model-answer"><div className="panel-head"><h2>Model response</h2><button className="btn-ghost" onClick={()=>downloadModelJson({prompt,...result},"minera-model-response.json")}>Download response</button></div><p className="panel-meta">{result.provider} · {result.model} · input tokens {result.usage.input_tokens??"unknown"} · output tokens {result.usage.output_tokens??"unknown"}</p>{["length","max_tokens","MAX_TOKENS"].includes(result.finish_reason)&&<p>The response reached the output token limit.</p>}{result.finish_reason==="refusal"&&<p>The provider declined this request.</p>}<pre>{result.text||"The provider returned an empty text response."}</pre></article>}
  </section>;
}
