import type {LlmProvider,LlmRuntimeStatus,LlmCompletionRequest} from "./llm";

/** These values belong only to the actual completion request, never its download snapshot. */
export interface LlmCallOverrides {api_key?:string|null;api_url?:string|null;openai_token_field?:"max_completion_tokens"|"max_tokens"|null;}
export interface ProviderCallForm {keyMode:"inherit"|"override";key:string;urlMode:"inherit"|"override";url:string;tokenField:"inherit"|"max_completion_tokens"|"max_tokens";}
export type ProviderCallForms=Record<LlmProvider,ProviderCallForm>;
export function defaultProviderCallForm():ProviderCallForm{return {keyMode:"inherit",key:"",urlMode:"inherit",url:"",tokenField:"inherit"};}
export function defaultProviderCallForms():ProviderCallForms{return {openai:defaultProviderCallForm(),anthropic:defaultProviderCallForm(),google:defaultProviderCallForm()};}
export function providerCallOverrides(provider:LlmProvider,form:ProviderCallForm):LlmCallOverrides{return {...(form.keyMode==="override"?{api_key:form.key}:{}),...(form.urlMode==="override"?{api_url:form.url}:{}),...(provider==="openai"&&form.tokenField!=="inherit"?{openai_token_field:form.tokenField}:{})};}
export function providerCallConfigured(status:LlmRuntimeStatus|null,provider:LlmProvider,overrides:LlmCallOverrides={}):boolean {
 if(!status?.ready)return false;
 // Status describes startup defaults, not a call-local URL/key. The runtime owns
 // complete validation and knows inherited credentials that the browser cannot inspect.
 return status.providers.some(item=>item.id===provider&&item.configured)||typeof overrides.api_key==="string"&&overrides.api_key.length>0||typeof overrides.api_url==="string";
}
const CALL_FIELDS=new Set(["api_key","api_url","openai_token_field"]);
export function llmDownloadRequest(input:LlmCompletionRequest):LlmCompletionRequest{return Object.fromEntries(Object.entries(input).filter(([name])=>!CALL_FIELDS.has(name))) as unknown as LlmCompletionRequest;}
export interface ProviderCallTicket {generation:number;provider:LlmProvider;form:ProviderCallForm;}
export class ProviderCallRequestGate {
 private generation=0;
 issue(provider:LlmProvider,form:ProviderCallForm):ProviderCallTicket{return {generation:++this.generation,provider,form:{...form}};}
 invalidate():void{++this.generation;}
 accepts(ticket:ProviderCallTicket,provider:LlmProvider,form:ProviderCallForm,signal?:AbortSignal):boolean{return !signal?.aborted&&ticket.generation===this.generation&&ticket.provider===provider&&(Object.keys(ticket.form) as Array<keyof ProviderCallForm>).every(key=>ticket.form[key]===form[key]);}
}
