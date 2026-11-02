import { nativeJson, parseNativeJson } from "./native-json";
export interface NativeRequest {
  path: string; fields?: Array<[string,string]>;
  reference?: {name:string;size:number;last_modified:number};
}
const captures=new WeakMap<object,{raw:string;request:NativeRequest}>();
const failures=new WeakMap<Response,string>();
const reason=(cause:unknown)=>cause instanceof Error?cause.message:String(cause);
export async function readNativeResponse(response:Response,request:NativeRequest):Promise<unknown> {
  let raw="";
  try{raw=await response.text();}catch(cause){failures.set(response,`its body could not be read (${reason(cause)})`);return null;}
  try{const value=parseNativeJson(raw);if(value&&typeof value==='object')captures.set(value,{raw,request:structuredClone(request)});return value;}
  catch(cause){failures.set(response,raw.length?`its ${nativeResponseSize(raw.length)} body is not readable JSON (${reason(cause)})`:"its body was empty");return null;}
}
/** Why readNativeResponse returned null for this response: an unreadable, empty or unparsable body. */
export function nativeResponseFailure(response:Response):string|undefined {return failures.get(response);}
export function nativeResponseSize(characters:number):string {return characters>=1e6?`${(characters/1e6).toFixed(1)} MB`:characters>=1e3?`${(characters/1e3).toFixed(1)} kB`:`${characters}-character`;}
export function nativeResponseJson(value:object):string {return captures.get(value)?.raw??nativeJson(value,2);}
export function nativeResponseRequest(value:object):NativeRequest|undefined {return captures.get(value)?.request;}
export function hasNativeResponse(value:object):boolean {return captures.has(value);}
