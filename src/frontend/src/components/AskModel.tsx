import { useEffect,useRef,useState } from "react";
import { LLM_PROVIDERS,llmLogContext,llmKnowledgeOptions,llmCompletionRequest,llmExtraPayload,requestLlmStatus,requestKnowledgePreview,requestLlmCompletion,type LlmProvider,type LlmRuntimeStatus,type LlmCompletion,type LlmImage,type LlmCompletionRequest } from "../llm";
import { LLM_IMAGE_ACCEPT,readLlmImage,llmImageUrl } from "../llm-images";
import { type KnowledgeInjectionVariant,type MineraExport } from "../types";
import { downloadModelJson } from "../model-view-data";
import { LlmStudies } from "./LlmStudies";
import { ContextBuilder } from "./ContextBuilder";
import { type VisualizationDraft } from "../visualization-explanation";

export function AskModel({data,mining,available=false,onOpenFiltered,visualization}:{data:MineraExport;mining:boolean;available?:boolean;onOpenFiltered?:((file:File)=>void);visualization?:VisualizationDraft|null}){
 const [task,setTask]=useState<"prompt"|"clusters"|"hypotheses">("prompt");
 return <><label className="filters-field model-task"><span>Model task</span><select disabled={mining} value={task} onChange={event=>setTask(event.target.value as typeof task)}><option value="prompt">Ask a question</option><option value="clusters">Case clustering</option><option value="hypotheses">Hypotheses</option></select></label>{task==="prompt"?<PromptChat data={data} mining={mining} available={available} visualization={visualization}/>:<LlmStudies key={task} task={task} data={data} available={available} mining={mining} onOpenFiltered={onOpenFiltered}/>}</>;
}
export function PromptChat({data,mining,available=false,ocelFile=null,dataset,visualization}:{data?:MineraExport;mining:boolean;available?:boolean;ocelFile?:File|null;dataset?:string;visualization?:VisualizationDraft|null}) {
  const [variant,setVariant]=useState<KnowledgeInjectionVariant>("process_mining");
  const [context,setContext]=useState(()=>data?llmLogContext(data):"");const [question,setQuestion]=useState("");const [schema,setSchema]=useState("");
  const [prompt,setPrompt]=useState(visualization?.prompt??"");const [provider,setProvider]=useState<LlmProvider>("openai");const [model,setModel]=useState("");
  const [maxTokens,setMaxTokens]=useState("4096");const [temperature,setTemperature]=useState("");
  const [images,setImages]=useState<LlmImage[]>(visualization?[visualization.image]:[]);const [openaiApi,setOpenaiApi]=useState<"responses"|"chat_completions">("responses");const [extraPayload,setExtraPayload]=useState("{}");
  const [resultRequest,setResultRequest]=useState<LlmCompletionRequest|null>(null);const attachmentRead=useRef(0);
  const [status,setStatus]=useState<LlmRuntimeStatus|null>(null);const [result,setResult]=useState<LlmCompletion|null>(null);
  const [error,setError]=useState<string|null>(null);const [pending,setPending]=useState<"preview"|"completion"|"attachment"|null>(null);
  const request=useRef<AbortController|null>(null);const readiness=useRef<AbortController|null>(null);
  const refresh=async()=>{readiness.current?.abort();const controller=new AbortController();readiness.current=controller;try{const next=await requestLlmStatus(controller.signal);if(readiness.current===controller&&!controller.signal.aborted)setStatus(next);}catch(cause){if(!controller.signal.aborted)setStatus({family:"LLM_RUNTIME",ready:false,providers:[],error:cause instanceof Error?cause.message:String(cause)});}};
  useEffect(()=>{void refresh();return()=>{request.current?.abort();readiness.current?.abort();attachmentRead.current++;};},[]);
  useEffect(()=>{if(mining){request.current?.abort();request.current=null;attachmentRead.current++;setPending(null);setResult(null);}},[mining]);
  const attach=async(files:File[])=>{
    const generation=++attachmentRead.current;setPending("attachment");setError(null);setResult(null);
    try{if(images.length+files.length>8)throw new Error("Attach at most eight images.");const added=await Promise.all(files.map(readLlmImage));if(generation===attachmentRead.current)setImages(value=>[...value,...added]);}
    catch(cause){if(generation===attachmentRead.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(generation===attachmentRead.current)setPending(null);}
  };
  const invalidate=()=>{setPrompt("");setResult(null);setError(null);};
  const run=async(operation:"preview"|"completion")=>{
    if(mining||pending)return;
    let input;try{input=operation==="preview"?llmKnowledgeOptions(variant,context,question,schema):llmCompletionRequest(provider,model,prompt,maxTokens,temperature,{...(images.length?{images}:{}),...(extraPayload.trim()&&extraPayload.trim()!=="{}"?{extra_payload:llmExtraPayload(extraPayload)}:{}),...(provider==="openai"?{openai_api:openaiApi}:{})});}catch(cause){setError(cause instanceof Error?cause.message:String(cause));return;}
    const controller=new AbortController();request.current=controller;setPending(operation);setError(null);setResult(null);
    try{if(operation==="preview"){const next=await requestKnowledgePreview(input as ReturnType<typeof llmKnowledgeOptions>,controller.signal);if(!next.computed)throw new Error(next.error||"Could not build the prompt.");if(request.current===controller&&!controller.signal.aborted)setPrompt(next.prompt);}else{const submitted=input as LlmCompletionRequest;const next=await requestLlmCompletion(submitted,controller.signal);if(request.current===controller&&!controller.signal.aborted){setResult(next);setResultRequest(submitted);}}}
    catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(null);}}
  };
  const configured=status?.ready&&status.providers.some(item=>item.id===provider&&item.configured);
  const endpoint=status?.providers.find(item=>item.id===provider)?.base_url;
  const label=LLM_PROVIDERS.find(item=>item.id===provider)!.label;
  return <section className="panel ask-model-panel">
    <div className="panel-head"><h2>Ask a model</h2><span className="panel-meta">{data?.dataset??dataset??ocelFile?.name}</span></div>
    <p className="ask-model-intro">Build a prompt from the log or a database schema. Review and edit the displayed prompt before sending it.</p>
    {visualization&&<p className="ask-model-intro">Rendered {visualization.title} from {visualization.dataset} · {visualization.width} × {visualization.height} pixels. The attachment captures the selected view and its displayed detail.</p>}
    <ContextBuilder available={available} mining={mining} ocelFile={ocelFile} onUse={text=>{request.current?.abort();request.current=null;attachmentRead.current++;setPending(null);if(variant==="database")setVariant("process_mining");setContext(text);setPrompt("");setResult(null);setError(null);}} />
    <fieldset className="ask-model-fields" disabled={mining||pending!==null}>
      <label className="filters-field"><span>Prompt context</span><select value={variant} onChange={event=>{setVariant(event.target.value as KnowledgeInjectionVariant);invalidate();}}><option value="process_mining">Process mining</option><option value="description">General description</option><option value="database">Database schema</option></select></label>
      {variant==="database"?<label className="ask-model-text"><span>Database schema</span><textarea rows={6} value={schema} onChange={event=>{setSchema(event.target.value);invalidate();}} placeholder="Tables, columns and relationships" /></label>:<label className="ask-model-text"><span>Log context</span><textarea rows={8} value={context} onChange={event=>{setContext(event.target.value);invalidate();}} /><small>{data?"The initial context includes up to 50 activities, 50 directly-follows edges and 20 variants. You can edit it.":"Build native context above or enter your own process context."}</small></label>}
      <label className="ask-model-text"><span>Question or instruction</span><textarea rows={3} value={question} onChange={event=>{setQuestion(event.target.value);invalidate();}} placeholder="What should the model help you understand?" /></label>
      <button className="btn-ghost" onClick={()=>void run("preview")}>Preview prompt</button>
      <label className="ask-model-text"><span>Prompt to send</span><textarea rows={8} value={prompt} onChange={event=>{setPrompt(event.target.value);setResult(null);}} placeholder="Preview a prompt or enter your own" /></label>
      <label className="filters-field"><span>Image attachments</span><input type="file" accept={LLM_IMAGE_ACCEPT} multiple onChange={event=>{const files=Array.from(event.target.files??[]);event.target.value="";if(files.length)void attach(files);}} /></label>
      <p className="ask-model-intro">Attach process diagrams or other images, then ask about what they show. Review the selected images before sending. The complete request has a 2 MiB limit.</p>
      {!!images.length&&<div className="llm-attachments">{images.map((image,index)=><figure key={index}><img src={llmImageUrl(image)} alt={image.name}/><figcaption>{image.name} · {image.media_type}</figcaption><button className="btn-ghost" onClick={()=>{setImages(value=>value.filter((_,i)=>i!==index));setResult(null);}}>Remove image {index+1}</button></figure>)}</div>}
      <div className="ask-model-options">
        <label className="filters-field"><span>Provider</span><select value={provider} onChange={event=>{setProvider(event.target.value as LlmProvider);setResult(null);}}>{LLM_PROVIDERS.map(item=><option key={item.id} value={item.id}>{item.label}</option>)}</select></label>
        <label className="filters-field ask-model-name"><span>Model name</span><input value={model} onChange={event=>{setModel(event.target.value);setResult(null);}} placeholder="A model available to your provider" /></label>
        <label className="filters-field"><span>Output token limit</span><input type="number" min="1" max="1000000" value={maxTokens} onChange={event=>{setMaxTokens(event.target.value);setResult(null);}} /></label>
        <label className="filters-field"><span>Temperature</span><input type="number" min="0" max={provider==="anthropic"?"1":"2"} step="0.1" value={temperature} onChange={event=>{setTemperature(event.target.value);setResult(null);}} placeholder="Model default" /></label>
        {provider==="openai"&&<label className="filters-field"><span>OpenAI API</span><select value={openaiApi} onChange={event=>{setOpenaiApi(event.target.value as typeof openaiApi);setResult(null);}}><option value="responses">Responses</option><option value="chat_completions">Chat Completions</option></select></label>}
      </div>
      <details><summary>Extra provider payload</summary><label className="ask-model-text"><span>Extra payload JSON</span><textarea rows={5} value={extraPayload} onChange={event=>{setExtraPayload(event.target.value);setResult(null);}} /></label><p className="ask-model-intro">These fields override generated provider fields. Use the provider's JSON names; Google generation controls belong inside generationConfig. Streaming and background responses need a separate consumer.</p></details>
      <p className="ask-model-intro">{configured?`${label} is configured at ${endpoint}. The displayed prompt, selected images and extra payload are sent.`:status?.error||`${label} is not configured. Set its credential or local endpoint in the runtime environment.`}</p>
      <div className="ask-model-actions"><button className="btn-open" disabled={!configured||!prompt.trim()} onClick={()=>void run("completion")}>Send displayed prompt to {label}</button><button className="btn-ghost" onClick={()=>void refresh()}>Refresh runtime</button><button className="btn-ghost" disabled={!prompt} onClick={()=>downloadModelJson({prompt,images,provider,model,openai_api:provider==="openai"?openaiApi:undefined,extra_payload:extraPayload},"minera-prompt.json")}>Download prompt</button></div>
    </fieldset>
    {pending&&<p className="ask-model-status" role="status">{pending==="preview"?"Building prompt…":pending==="attachment"?"Reading images…":"Waiting for the model…"} <button className="btn-ghost" onClick={()=>{request.current?.abort();request.current=null;attachmentRead.current++;setPending(null);}}>Cancel</button></p>}
    {error&&<div className="error-banner" role="alert">{error}</div>}
    {!status?.ready&&<details className="ask-model-setup"><summary>Connect a language-model runtime</summary><p>Set OPENAI_API_KEY, ANTHROPIC_API_KEY or GEMINI_API_KEY in the runtime environment. For a local compatible service, configure its provider base URL.</p><pre>python scripts/llm_runtime.py --port 8189{"\n"}minera_server --llm-runtime-url http://127.0.0.1:8189</pre></details>}
    {result&&<article className="ask-model-answer"><div className="panel-head"><h2>Model response</h2><button className="btn-ghost" onClick={()=>downloadModelJson({prompt:resultRequest?.prompt??prompt,request:resultRequest,...result},"minera-model-response.json")}>Download response</button></div><p className="panel-meta">{result.provider} · {result.model} · input tokens {result.usage.input_tokens??"unknown"} · output tokens {result.usage.output_tokens??"unknown"}</p>{["length","max_tokens","max_output_tokens","MAX_TOKENS"].includes(result.finish_reason)&&<p>The response reached the output token limit.</p>}{result.finish_reason==="refusal"&&<p>The provider declined this request.</p>}<pre>{result.text||"The provider returned an empty text response."}</pre></article>}
  </section>;
}
