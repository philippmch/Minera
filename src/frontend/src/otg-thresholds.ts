import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {parseNativeJson} from "./native-json";

export const OTG_THRESHOLD_FIELD="otg_theta_by_relation_json";
export const OTG_THRESHOLD_ALIASES=["otg_theta_interaction","otg_theta_descendants","otg_theta_inheritance","otg_theta_cobirth","otg_theta_codeath"] as const;
export type OtgNativeString=string|{native_bytes:"hex-v1";data:string};
export type OtgThreshold=number|"NaN"|"INF"|"-INF";
export type OtgThresholdPair=[OtgNativeString,OtgThreshold];
const record=(v:unknown):v is Record<string,unknown>=>!!v&&typeof v==="object"&&!Array.isArray(v);
export function otgNativeStringBytes(value:unknown):Uint8Array<ArrayBuffer> {
 if(typeof value==="string")return nativeStringBytes(value);
 if(!record(value)||Object.keys(value).length!==2||value.native_bytes!=="hex-v1"||typeof value.data!=="string"||!/^(?:[0-9a-fA-F]{2})*$/.test(value.data))throw new Error("Relation names must be strings or exact native_bytes hex-v1 values.");
 const bytes=new Uint8Array(value.data.length/2);for(let i=0;i<bytes.length;i++)bytes[i]=parseInt(value.data.slice(i*2,i*2+2),16);return bytes;
}
/** Validate without serializing: the original numeric tokens, signed zero and whitespace remain intact. */
export function parseOtgThresholds(raw:string):OtgThresholdPair[] {
 if(typeof raw!=="string")throw new Error("OTG relation thresholds must be JSON text.");
 let value:unknown;try{value=parseNativeJson(raw);}catch{throw new Error("OTG relation thresholds must be a JSON array of [relation, threshold] pairs.");}
 if(!Array.isArray(value))throw new Error("OTG relation thresholds must be an array of pairs.");
 const keys=new Set<string>();for(const pair of value){if(!Array.isArray(pair)||pair.length!==2)throw new Error("Each OTG relation threshold must contain exactly a relation and a threshold.");const bytes=otgNativeStringBytes(pair[0]);const key=Array.from(bytes,b=>b.toString(16).padStart(2,"0")).join("");if(keys.has(key))throw new Error("Duplicate OTG relation names after native byte decoding.");keys.add(key);if(!(typeof pair[1]==="number"&&Number.isFinite(pair[1]))&&!["NaN","INF","-INF"].includes(pair[1]))throw new Error('OTG thresholds require finite JSON numbers or the tags "NaN", "INF", "-INF".');}
 return value as OtgThresholdPair[];
}
/** URLSearchParams accepts USVString: escape UTF-16 units before passing exact JSON to it. */
export function otgThresholdQueryText(raw:string):string {parseOtgThresholds(raw);return raw.replace(/[\ud800-\udfff]/g,c=>"\\u"+c.charCodeAt(0).toString(16).padStart(4,"0"));}
export function validateOtgThresholdOptions(options:object&{variant?:string;otg_theta_by_relation_json?:string}):string|undefined {
 if(!Object.prototype.hasOwnProperty.call(options,OTG_THRESHOLD_FIELD))return undefined;
 if(options.variant!=="otg")throw new Error("Exact relation thresholds apply only to the OTG variant.");
 for(const alias of OTG_THRESHOLD_ALIASES)if(Object.prototype.hasOwnProperty.call(options,alias))throw new Error("Exact relation thresholds cannot be combined with named threshold aliases.");
 const raw=options.otg_theta_by_relation_json;if(typeof raw!=="string")throw new Error("OTG relation thresholds must be JSON text.");parseOtgThresholds(raw);return raw;
}
export function appendOtgThresholds(body:FormData,raw:string):void {parseOtgThresholds(raw);appendNativeMultipartField(body,OTG_THRESHOLD_FIELD,raw);}
export interface OtgConformanceMetadata {raw:string;report_json:string;query:string;threshold_json:string;original_files:{real:Blob;normative:Blob};sources:Record<string,{name:string;size:number;sha256:string}>;}
const metadata=new WeakMap<object,OtgConformanceMetadata>();
export function otgConformanceMetadata(value:object):OtgConformanceMetadata|undefined{return metadata.get(value);}
export async function bindOtgConformanceMetadata(value:object,raw:string,query:string,threshold_json:string,real:Blob,normative:Blob):Promise<OtgConformanceMetadata>{
 const bind=async(file:Blob,fallback:string)=>({name:typeof File!=="undefined"&&file instanceof File?file.name:fallback,size:file.size,sha256:Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256",await file.arrayBuffer())),b=>b.toString(16).padStart(2,"0")).join("")});
 const [realSource,normativeSource]=await Promise.all([bind(real,"ocel.json"),bind(normative,"normative.json")]);const sources={real:realSource,normative:normativeSource};const report_json=`{"family":"OCEL_EXECUTION_REPORT","request":${JSON.stringify({method:"conformance",sources,query,parameters:Object.fromEntries(new URLSearchParams(query)),otg_theta_by_relation_json:threshold_json})},"response":${raw}}`;const info={raw,query,threshold_json,original_files:{real,normative},sources,report_json};metadata.set(value,info);return info;
}

export function requireExactOtgEnvelope(value:unknown):void {
 if(!record(value)||value.family!=="OCEL_CONFORMANCE"||value.variant!=="otg"||!record(value.result)||!record(value.exact_otg)||value.exact_otg.native_schema!=="otg-exact-v1")throw new Error("The backend returned an invalid exact OTG conformance envelope.");
}

export interface ExactOtgLegacyArtifact {name:string;label:string;bytes:Uint8Array<ArrayBuffer>;mime:"application/octet-stream";}
/** Download the original serializers' byte artifacts even when they are not valid UTF-8 JSON. */
export function exactOtgLegacyArtifacts(value:unknown):ExactOtgLegacyArtifact[] {
 if(!record(value)||value.family!=="OCEL_CONFORMANCE"||value.variant!=="otg"||!record(value.exact_otg)||value.exact_otg.native_schema!=="otg-exact-v1")return [];
 const exact=value.exact_otg,artifacts:ExactOtgLegacyArtifact[]=[];
 const append=(kind:string,label:string,bytes:unknown,error:unknown)=>{if(bytes===null||bytes===undefined||error!==null&&error!==undefined)return;try{artifacts.push({name:"minera-otg-legacy-"+kind+".json",label,bytes:otgNativeStringBytes(bytes),mime:"application/octet-stream"});}catch{/* Malformed optional artifacts cannot discard the safe complete envelope. */}};
 append("comparison","comparison",exact.legacy_result_bytes,exact.legacy_result_error);
 for(const [field,kind,label] of [["legacy_real_graph","real-graph","real graph"],["legacy_normative_graph","normative-graph","normative graph"]]){const graph=exact[field];if(record(graph))append(kind,label,graph.bytes,graph.error);}
 return artifacts;
}
