import { nativeJson, parseNativeJson } from "./native-json";
export interface NativeRequest {
  path: string; fields?: Array<[string,string]>;
  reference?: {name:string;size:number;last_modified:number};
}
const captures=new WeakMap<object,{raw:string;request:NativeRequest}>();
export async function readNativeResponse(response:Response,request:NativeRequest):Promise<unknown> {
  const raw=await response.text().catch(()=>"");
  try{const value=parseNativeJson(raw);if(value&&typeof value==='object')captures.set(value,{raw,request:structuredClone(request)});return value;}catch{return null;}
}
export function nativeResponseJson(value:object):string {return captures.get(value)?.raw??nativeJson(value,2);}
export function nativeResponseRequest(value:object):NativeRequest|undefined {return captures.get(value)?.request;}
export function hasNativeResponse(value:object):boolean {return captures.has(value);}
