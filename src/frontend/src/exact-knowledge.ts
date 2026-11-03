import {parseNativeJson,nativeCell,nativeJson} from "./native-json";
import { backendFetch } from "./backend-fetch";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {otgNativeStringBytes,type OtgNativeString} from "./otg-thresholds";
import {type KnowledgeInjectionOptions,type KnowledgeInjectionVariant} from "./types";
export type KnowledgeNativeString=OtgNativeString;
export interface ExactKnowledgeFields {
 knowledge_variant?:KnowledgeInjectionVariant|"db"|"process-mining";knowledge_description?:string;knowledge_query?:string;knowledge_database_schema?:string;
 knowledge_max_input_bytes?:number|string;knowledge_max_prompt_bytes?:number|string;
 knowledge_assumptions_json?:string;knowledge_assumptions_file?:File;knowledge_context_json?:string;knowledge_context_file?:File;knowledge_options_json?:string;knowledge_options_file?:File;
}
type ExactMarker={knowledge_assumptions_json:string}|{knowledge_assumptions_file:File}|{knowledge_context_json:string}|{knowledge_context_file:File}|{knowledge_options_json:string}|{knowledge_options_file:File};
export type ExactKnowledgeRequest=ExactKnowledgeFields&ExactMarker;
export type KnowledgeRequest=KnowledgeInjectionOptions|ExactKnowledgeFields;
export interface ExactKnowledgeResult {family:"KNOWLEDGE_INJECTION";variant:KnowledgeInjectionVariant;computed:boolean;prompt:KnowledgeNativeString;input_bytes:number;error:KnowledgeNativeString;}
export interface ExactKnowledgeEnvelope {family:"KNOWLEDGE_INJECTION";operation:"exact";result:ExactKnowledgeResult|null;error:{type:string;message:KnowledgeNativeString}|null;exact_knowledge:{native_schema:"knowledge-exact-v1";options:Record<string,unknown>|null;original_inputs:unknown[];supplied_fields:string[];prompt_bytes:number|null;legacy_result_bytes:KnowledgeNativeString|null;legacy_result_error:KnowledgeNativeString|null;legacy_result_transport_error:KnowledgeNativeString|null;};}
export const KNOWLEDGE_EXACT_FIELDS=["knowledge_assumptions_json","knowledge_assumptions_file","knowledge_context_json","knowledge_context_file","knowledge_options_json","knowledge_options_file"] as const;
const SCALARS=["knowledge_variant","knowledge_description","knowledge_query","knowledge_database_schema","knowledge_max_input_bytes","knowledge_max_prompt_bytes"] as const;
const OPTIONS=["variant","description","query","database_schema","assumptions","context","max_input_bytes","max_prompt_bytes"];
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==="object"&&!Array.isArray(v);
const own=(v:object,key:string)=>Object.prototype.hasOwnProperty.call(v,key);
export const hasExactKnowledge=(v:object)=>KNOWLEDGE_EXACT_FIELDS.some(key=>own(v,key));
export function knowledgeBytes(v:unknown):Uint8Array<ArrayBuffer>{try{return otgNativeStringBytes(v);}catch{throw new Error("Native knowledge strings require literal text or an exact native_bytes hex-v1 carrier.");}}
const hex=(bytes:Uint8Array)=>Array.from(bytes,b=>b.toString(16).padStart(2,"0")).join("");
/** Keep decimal tokens and reject object-key ambiguity before projecting to host values. */
function exactJson(raw:string):unknown {
 if(typeof raw!=="string")throw new Error("Exact knowledge controls require JSON text.");
 let value:unknown;try{value=parseNativeJson(raw);}catch{throw new Error("Exact knowledge controls must contain valid JSON.");}
 const tokens=raw.match(/"(?:[^"\\]|\\.)*"|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?|true|false|null|[{}\[\],:]/g)??[];let i=0;
 const walk=()=>{const token=tokens[i++];if(token==="{"){const keys=new Set<string>();while(tokens[i]!=="}"){const name=JSON.parse(tokens[i++]) as string;if(keys.has(name))throw new Error("Duplicate exact JSON object field: "+name);keys.add(name);i++;walk();if(tokens[i]!==",")break;i++;}i++;}else if(token==="["){while(tokens[i]!=="]"){walk();if(tokens[i]!==",")break;i++;}i++;}};walk();return value;
}
export function knowledgeSize(value:unknown,label="Native byte bound",allowZero=true):string {
 const text=typeof value==="number"&&Number.isSafeInteger(value)?String(value):typeof value==="string"?value:"";
 if(!/^[0-9]+$/.test(text)||BigInt(text)>18446744073709551615n||!allowZero&&BigInt(text)===0n)throw new Error(label+" requires an exact "+(allowZero?"unsigned":"positive")+" native size_t decimal through 18446744073709551615.");return text;
}
export function knowledgeAssumptions(raw:string):unknown[] {const v=exactJson(raw);if(!Array.isArray(v))throw new Error("Assumptions require an array of NativeString values.");v.forEach(knowledgeBytes);return v;}
export function knowledgeContext(raw:string):unknown[] {const v=exactJson(raw);if(!Array.isArray(v))throw new Error("Context requires an array of [NativeString, NativeString] pairs.");const keys=new Set<string>();for(const pair of v){if(!Array.isArray(pair)||pair.length!==2)throw new Error("Context entries require exactly a key and a value.");const key=hex(knowledgeBytes(pair[0]));knowledgeBytes(pair[1]);if(keys.has(key))throw new Error("Duplicate context keys after native byte decoding.");keys.add(key);}return v;}
export function knowledgeOptions(raw:string):Record<string,unknown> {const v=exactJson(raw);if(!record(v)||Object.keys(v).length!==8||OPTIONS.some(k=>!own(v,k)))throw new Error("Complete native options require exactly all eight native fields.");if(typeof v.variant!=="string"||!["description","process_mining","process-mining","database","db"].includes(v.variant))throw new Error("Unknown native knowledge variant.");for(const k of ["description","query","database_schema"])knowledgeBytes(v[k]);knowledgeAssumptions(nativeJson(v.assumptions));knowledgeContext(nativeJson(v.context));for(const k of ["max_input_bytes","max_prompt_bytes"]){const token=nativeCell(v,k);knowledgeSize(typeof token==="number"?String(token):token,k);}return v;}
export function validateExactKnowledge(options:KnowledgeRequest):void {
 if(!hasExactKnowledge(options))return;
 const full=own(options,"knowledge_options_json")||own(options,"knowledge_options_file");
 if(full){for(const k of [...SCALARS,...KNOWLEDGE_EXACT_FIELDS.filter(k=>!k.startsWith("knowledge_options_"))])if(own(options,k))throw new Error("Complete native options conflict with individual knowledge controls.");}
 const o=options as ExactKnowledgeFields;
 for(const role of ["assumptions","context","options"] as const){const json="knowledge_"+role+"_json",file="knowledge_"+role+"_file";if(own(options,json)&&own(options,file))throw new Error("Supply "+role+" as JSON or File, not both.");if(own(options,json)){const raw=(o as unknown as Record<string,unknown>)[json];if(typeof raw!=="string")throw new Error(json+" must be JSON text.");({assumptions:knowledgeAssumptions,context:knowledgeContext,options:knowledgeOptions}[role])(raw);}if(own(options,file)&&!(typeof File!=="undefined"&&(o as unknown as Record<string,unknown>)[file] instanceof File))throw new Error(file+" requires an original File.");}
 for(const k of ["knowledge_description","knowledge_query","knowledge_database_schema"] as const)if(o[k]!==undefined&&typeof o[k]!=="string")throw new Error(k+" requires literal text.");
 if(o.knowledge_variant!==undefined&&!["description","process_mining","process-mining","database","db"].includes(o.knowledge_variant))throw new Error("Unknown native knowledge variant.");
 for(const k of ["knowledge_max_input_bytes","knowledge_max_prompt_bytes"] as const)if(o[k]!==undefined)knowledgeSize(o[k],k);
}
const nativeQueryBytes=(raw:string)=>Array.from(nativeStringBytes(raw),byte=>byte>=65&&byte<=90||byte>=97&&byte<=122||byte>=48&&byte<=57||[45,46,95,126].includes(byte)?String.fromCharCode(byte):"%"+byte.toString(16).padStart(2,"0").toUpperCase()).join("");
export function exactKnowledgeQuery(options:KnowledgeRequest):string {validateExactKnowledge(options);const fields:string[]=[];for(const [key,value] of Object.entries(options)){if(value===undefined)continue;if(key.endsWith("_file"))throw new Error("Original knowledge Files require a multipart request helper.");fields.push(encodeURIComponent(key)+"="+nativeQueryBytes(String(value)));}return fields.join("&");}
export function isExactKnowledgeEnvelope(v:unknown):v is ExactKnowledgeEnvelope {if(!record(v)||v.family!=="KNOWLEDGE_INJECTION"||v.operation!=="exact"||!record(v.exact_knowledge)||v.exact_knowledge.native_schema!=="knowledge-exact-v1"||!Array.isArray(v.exact_knowledge.original_inputs)||!Array.isArray(v.exact_knowledge.supplied_fields))return false;if(v.error!==null&&(!record(v.error)||typeof v.error.type!=="string"))return false;if(v.result===null)return v.error!==null;if(!record(v.result)||v.result.family!=="KNOWLEDGE_INJECTION"||!["description","process_mining","database"].includes(String(v.result.variant))||typeof v.result.computed!=="boolean")return false;try{knowledgeBytes(v.result.prompt);knowledgeBytes(v.result.error);knowledgeSize(nativeCell(v.result,"input_bytes"));return true;}catch{return false;}}
export interface ExactKnowledgeMetadata {raw:string;report_json:string;http_status:number;fields:Record<string,string>;original_files:Record<string,File>;sources:Record<string,{name:string;size:number;sha256:string}>;}
const metadata=new WeakMap<object,ExactKnowledgeMetadata>();
export function exactKnowledgeMetadata(value:object):ExactKnowledgeMetadata|undefined{return metadata.get(value);}
export async function requestExactKnowledge(options:KnowledgeRequest,signal?:AbortSignal):Promise<ExactKnowledgeEnvelope> {
 validateExactKnowledge(options);if(!hasExactKnowledge(options))throw new Error("Select at least one exact knowledge input.");signal?.throwIfAborted();const form=new FormData(),fields:Record<string,string>={},original_files:Record<string,File>={},sources:ExactKnowledgeMetadata["sources"]={};
 for(const [key,value] of Object.entries(options)){if(value===undefined)continue;if(key.endsWith("_file")){const file=value as File;form.append(key,file,file.name);original_files[key]=file;sources[key]={name:file.name,size:file.size,sha256:hex(new Uint8Array(await crypto.subtle.digest("SHA-256",await file.arrayBuffer())))};}else{const raw=String(value);fields[key]=raw;appendNativeMultipartField(form,key,raw);}}
 signal?.throwIfAborted();const response=await backendFetch("/api/knowledge-injection",{method:"POST",body:form,...(signal?{signal}:{})});const raw=await response.text();let value:unknown;try{value=parseNativeJson(raw);}catch{throw new Error("The backend returned invalid exact knowledge JSON.");}if(!isExactKnowledgeEnvelope(value))throw new Error("The backend returned an invalid exact knowledge envelope (HTTP "+response.status+").");const report_json=`{"family":"EXACT_KNOWLEDGE_REPORT","request":${JSON.stringify({fields,sources})},"http_status":${response.status},"response":${raw}}`;metadata.set(value,{raw,report_json,http_status:response.status,fields,original_files,sources});return value;
}
export interface KnowledgeArtifact {name:string;label:string;bytes:Uint8Array<ArrayBuffer>;mime:"application/octet-stream"|"application/json";}
export function exactKnowledgeArtifacts(value:ExactKnowledgeEnvelope):KnowledgeArtifact[] {const out:KnowledgeArtifact[]=[];if(value.result)out.push({name:"minera-native-knowledge-prompt.bin",label:"native prompt bytes",bytes:knowledgeBytes(value.result.prompt),mime:"application/octet-stream"});if(value.exact_knowledge.legacy_result_bytes!==null&&value.exact_knowledge.legacy_result_error===null)out.push({name:"minera-legacy-knowledge-result.json",label:"original serializer bytes",bytes:knowledgeBytes(value.exact_knowledge.legacy_result_bytes),mime:"application/octet-stream"});return out;}
/** Strict Unicode only. Invalid UTF-8/WTF8 stays in native inspection/downloads. No replacement decoding. */
export function exactKnowledgeProviderText(value:ExactKnowledgeEnvelope):string|null {if(!value.result?.computed)return null;try{return new TextDecoder("utf-8",{fatal:true,ignoreBOM:true}).decode(knowledgeBytes(value.result.prompt)).replace(/\r\n?/g,"\n");}catch{return null;}}
export interface KnowledgeEditor {mode:"legacy"|"independent"|"full";assumptionsMode:"omitted"|"json"|"file";assumptionsJson:string;assumptionsFile:File|null;contextMode:"omitted"|"json"|"file";contextJson:string;contextFile:File|null;optionsMode:"json"|"file";optionsJson:string;optionsFile:File|null;maxInput:string;maxPrompt:string;}
export function defaultKnowledgeEditor():KnowledgeEditor{return {mode:"legacy",assumptionsMode:"json",assumptionsJson:"[]",assumptionsFile:null,contextMode:"omitted",contextJson:"[]",contextFile:null,optionsMode:"json",optionsJson:'{"variant":"process_mining","description":"","query":"","database_schema":"","assumptions":[],"context":[],"max_input_bytes":1048576,"max_prompt_bytes":2097152}',optionsFile:null,maxInput:"",maxPrompt:""};}
export function knowledgeEditorRequest(form:KnowledgeEditor,variant:KnowledgeInjectionVariant,description:string,query:string,database_schema:string):ExactKnowledgeFields {
 if(form.mode==="full"){const result=form.optionsMode==="file"?{knowledge_options_file:form.optionsFile!}:{knowledge_options_json:form.optionsJson};validateExactKnowledge(result);return result;}
 if(form.mode!=="independent")throw new Error("Use the standard prompt builder for legacy preview.");
 const result:ExactKnowledgeFields={knowledge_variant:variant,knowledge_description:description,knowledge_query:query,knowledge_database_schema:database_schema};
 if(form.maxInput!=="")result.knowledge_max_input_bytes=knowledgeSize(form.maxInput,"Input byte bound");if(form.maxPrompt!=="")result.knowledge_max_prompt_bytes=knowledgeSize(form.maxPrompt,"Prompt byte bound");
 for(const role of ["assumptions","context"] as const){const mode=form[role+"Mode" as "assumptionsMode"|"contextMode"];if(mode==="json")Object.assign(result,{["knowledge_"+role+"_json"]:form[role+"Json" as "assumptionsJson"|"contextJson"]});if(mode==="file")Object.assign(result,{["knowledge_"+role+"_file"]:form[role+"File" as "assumptionsFile"|"contextFile"]});}
 if(!hasExactKnowledge(result))throw new Error("Choose an assumptions or context input, or use complete options for an explicit empty native configuration.");validateExactKnowledge(result);return result;
}
