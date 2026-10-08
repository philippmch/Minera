import { knowledgeInjectionQuery } from "./api";
import { isKnowledgeInjectionResult, type KnowledgeInjectionOptions, type KnowledgeInjectionResult, type KnowledgeInjectionVariant, type MineraExport } from "./types";

export const LLM_PROVIDERS = [{id:"openai",label:"OpenAI"},{id:"anthropic",label:"Anthropic"},{id:"google",label:"Google"}] as const;
export type LlmProvider = typeof LLM_PROVIDERS[number]["id"];
export interface LlmRuntimeStatus { family:"LLM_RUNTIME";ready:boolean;providers:{id:LlmProvider;configured:boolean;base_url:string}[];error?:string;max_input_bytes?:number; }
export interface LlmImage {name:string;media_type:string;data:string;detail?:"auto"|"low"|"high"|"original";}
export interface LlmRequestExtras {images?:LlmImage[];extra_payload?:Record<string,unknown>;openai_api?:"responses"|"chat_completions";}
export interface LlmCompletionRequest extends LlmRequestExtras { provider:LlmProvider;model:string;prompt:string;max_tokens:number;temperature?:number; }
export interface LlmCompletion { family:"LLM_COMPLETION";provider:LlmProvider;model:string;text:string;finish_reason:string;usage:{input_tokens:number|null;output_tokens:number|null};response:Record<string,unknown>; }
const provider = (value:unknown):value is LlmProvider => LLM_PROVIDERS.some(item=>item.id===value);
const tokens = (value:unknown) => value===null || Number.isSafeInteger(value) && (value as number)>=0;
export function isLlmRuntimeStatus(value:unknown):value is LlmRuntimeStatus {
  if(!value || typeof value!=="object")return false;
  const result=value as Partial<LlmRuntimeStatus>;
  return result.family==="LLM_RUNTIME" && typeof result.ready==="boolean" && Array.isArray(result.providers) && result.providers.every(item=>provider(item.id)&&typeof item.configured==="boolean"&&typeof item.base_url==="string") && new Set(result.providers.map(item=>item.id)).size===result.providers.length && (result.error===undefined||typeof result.error==="string");
}
export function isLlmCompletion(value:unknown):value is LlmCompletion {
  if(!value || typeof value!=="object")return false;
  const result=value as Partial<LlmCompletion>;
  return result.family==="LLM_COMPLETION" && provider(result.provider) && typeof result.model==="string" && typeof result.text==="string" && typeof result.finish_reason==="string" && !!result.usage && tokens(result.usage.input_tokens)&&tokens(result.usage.output_tokens) && !!result.response && typeof result.response==="object" && !Array.isArray(result.response);
}
export function llmLogContext(data:MineraExport):string {
  const names=(ids:number[])=>ids.map(id=>data.activities[id]?.name??`activity ${id}`).join(" → ");
  return [`Dataset: ${data.dataset}`,`Cases: ${data.trace_count}; events: ${data.event_count}; activities: ${data.activities.length}`,
    "Activities (up to 50):",...data.activities.slice(0,50).map(item=>`${item.name}: ${item.count} events`),
    "Directly-follows edges (up to 50):",...data.edges.slice(0,50).map(edge=>`${names([edge.source,edge.target])}: ${edge.count}`),
    "Trace variants (up to 20):",...data.variants.slice(0,20).map(item=>`${names(item.activities)}: ${item.count} cases`)].join("\n");
}
export function llmKnowledgeOptions(variant:KnowledgeInjectionVariant,context:string,question:string,schema:string):KnowledgeInjectionOptions {
  if(!question.trim())throw new Error("Enter a question or instruction.");
  if(variant==="database"&&!schema.trim())throw new Error("Enter a database schema.");
  return {knowledge_variant:variant,knowledge_description:variant==="description"?`${context}\n\n${question}`:context,knowledge_query:question,knowledge_database_schema:variant==="database"?schema:undefined};
}
export function llmExtraPayload(text:string):Record<string,unknown> {
  let value:unknown;try{value=JSON.parse(text.trim()||"{}");}catch{throw new Error("Extra provider payload must be valid JSON.");}
  if(!value||typeof value!=="object"||Array.isArray(value))throw new Error("Extra provider payload must be a JSON object.");
  if((value as Record<string,unknown>).stream||(value as Record<string,unknown>).background)throw new Error("Streaming and background responses need a separate consumer.");
  return value as Record<string,unknown>;
}
export function llmCompletionRequest(providerId:LlmProvider,model:string,prompt:string,maxTokens:string,temperature:string,extras:LlmRequestExtras={}):LlmCompletionRequest {
  if(!provider(providerId))throw new Error("Select a supported provider.");
  if(!model.trim())throw new Error("Enter a model name.");
  if(!prompt.trim())throw new Error("Preview or enter a prompt before sending.");
  const limit=maxTokens.trim()?Number(maxTokens):NaN;
  if(!Number.isSafeInteger(limit)||limit<1||limit>1000000)throw new Error("Output token limit must be a whole number from 1 to 1000000.");
  const value=temperature.trim()?Number(temperature):undefined;
  const maximum=providerId==="anthropic"?1:2;
  if(value!==undefined&&(!Number.isFinite(value)||value<0||value>maximum))throw new Error("Temperature must be from 0 to "+maximum+", or left blank for the model default.");
  if(extras.images&&extras.images.length>8)throw new Error("Attach at most eight images.");
  return {provider:providerId,model:model.trim(),prompt,max_tokens:limit,...(value===undefined?{}:{temperature:value}),...extras};
}
async function checkedJson<T>(response:Response,guard:(value:unknown)=>value is T,label:string):Promise<T> {
  const value:unknown=await response.json().catch(()=>null);
  if(value&&typeof value==="object"&&"error" in value&&!("family" in value))throw new Error(String((value as {error:unknown}).error));
  if(!response.ok)throw new Error(`The Minera backend answered with status ${response.status}.`);
  if(!guard(value))throw new Error(`The backend returned an invalid ${label}.`);
  return value;
}
export async function requestLlmStatus(signal?:AbortSignal):Promise<LlmRuntimeStatus> {
  return checkedJson(await fetch("/api/llm/status",signal?{signal}:{}),isLlmRuntimeStatus,"language-model runtime status");
}
export async function requestKnowledgePreview(options:KnowledgeInjectionOptions,signal?:AbortSignal):Promise<KnowledgeInjectionResult> {
  const form=new FormData();
  for(const [key,value] of new URLSearchParams(knowledgeInjectionQuery(options)))form.append(key,value);
  const result=await checkedJson(await fetch("/api/knowledge-injection",{method:"POST",body:form,...(signal?{signal}:{})}),isKnowledgeInjectionResult,"knowledge prompt");
  return {...result,prompt:result.prompt.replace(/\r\n?/g,"\n")};
}
export async function requestLlmCompletion(input:LlmCompletionRequest,signal?:AbortSignal):Promise<LlmCompletion> {
  const body=JSON.stringify(input);
  if(new TextEncoder().encode(body).length>2*1024*1024)throw new Error("The request exceeds the runtime input bound.");
  return checkedJson(await fetch("/api/llm/complete",{method:"POST",headers:{"Content-Type":"application/json"},body,...(signal?{signal}:{})}),isLlmCompletion,"language-model completion");
}
