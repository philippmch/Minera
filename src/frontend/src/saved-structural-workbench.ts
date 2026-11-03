import {nativeCell,nativeJson} from "./native-json";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {nativeObjectTables,type DiagnosticTable} from "./model-diagnostics";
import {originalEditDistanceWireFilename} from "./original-edit-distance-workbench";
import {isOriginalOcPlayoutNativeType} from "./original-object-centric-playout-workbench";

export const SAVED_STRUCTURAL_OPERATIONS=["trie_to_petri","query_intervals"] as const;
export const SAVED_STRUCTURAL_FORMATS=["direct","legacy_trie","legacy_interval","legacy_transformation","legacy_report","query_result"] as const;
export const SAVED_STRUCTURAL_ARTIFACTS=["constructor_json","options_json","result_json","native_output_bytes","pnml_bytes"] as const;
export type SavedStructuralOperation=typeof SAVED_STRUCTURAL_OPERATIONS[number];
export type SavedStructuralFormat=typeof SAVED_STRUCTURAL_FORMATS[number];
export type SavedStructuralArtifact=typeof SAVED_STRUCTURAL_ARTIFACTS[number];
export type SavedStructuralString=string|{native_bytes:"hex-v1";data:string};
type RecordValue=Record<string,unknown>;
export interface SavedStructuralError {phase:SavedStructuralString;type:SavedStructuralString;message:SavedStructuralString;}
export interface SavedStructuralCarrier {dataset:SavedStructuralString;bytes:number|null;encoding:"utf8"|"hex-v1"|null;data:string|null;error:SavedStructuralError|null;}
export interface SavedStructuralResponse {
 family:"SAVED_STRUCTURAL_INPUTS";operation:SavedStructuralString;input_format:SavedStructuralString;call_form:SavedStructuralString;
 originals:Array<{field:SavedStructuralString;filename:SavedStructuralString|null;file:boolean;original:SavedStructuralCarrier}>;
 supplied_controls:Array<{name:SavedStructuralString;origin:"query"|"multipart"}>;parsed_controls:SavedStructuralString[];
 input:{type:"TrieModel"|"IntervalVector"|null;before:unknown;after:unknown;selection:Array<string|number>|null};
 configured_options:{before:RecordValue;after:RecordValue|null}|null;
 result:{type:"PetriNet"|"IntervalTreeQueryResult";value:unknown}|null;
 native_entered:boolean;native_returned:boolean;input_unchanged:boolean|null;options_unchanged:boolean|null;
 native_calls:Array<{name:SavedStructuralString;input_type:SavedStructuralString;entered:boolean;returned:boolean;error:SavedStructuralError|null}>;
 artifacts:Record<SavedStructuralArtifact,SavedStructuralCarrier|null>;error:SavedStructuralError|null;
}
export interface SavedStructuralForm {operation:SavedStructuralOperation;input_format:SavedStructuralFormat;call_form:"direct"|"default_options"|"with_options";call_form_supplied:boolean;options_supplied:boolean;options_text:string;}
export const defaultSavedStructuralForm=(operation:SavedStructuralOperation="trie_to_petri"):SavedStructuralForm=>({operation,input_format:"direct",call_form:operation==="trie_to_petri"?"direct":"with_options",call_form_supplied:true,options_supplied:operation==="query_intervals",options_text:'{"point":0,"range":null}'});
const object=(v:unknown):v is RecordValue=>v!==null&&typeof v==="object"&&!Array.isArray(v);
// The PetriNet carrier: nine storage fields, plus place_names when the net names its places.
const PETRI_NET_FIELDS=["place_count","source_storage","sink_storage","has_source","has_sink","activities","transitions","discovery_markings","transition_names"] as const;
const keys=(v:unknown,names:readonly string[]):v is RecordValue=>object(v)&&Object.keys(v).length===names.length&&names.every(k=>Object.hasOwn(v,k));
const hexPattern=/^(?:[0-9a-f]{2})*$/i;
const unhex=(raw:string):Uint8Array<ArrayBuffer>=>{if(!hexPattern.test(raw))throw Error("Invalid native byte string.");return Uint8Array.from(raw.match(/../g)??[],x=>parseInt(x,16));};
export function isSavedStructuralString(v:unknown):v is SavedStructuralString{return typeof v==="string"||keys(v,["native_bytes","data"])&&v.native_bytes==="hex-v1"&&typeof v.data==="string"&&hexPattern.test(v.data);}
export function savedStructuralStringBytes(v:SavedStructuralString):Uint8Array<ArrayBuffer>{return typeof v==="string"?nativeStringBytes(v):unhex(v.data);}
export function savedStructuralStringText(v:SavedStructuralString):string{if(typeof v==="string")return v;try{return new TextDecoder("utf8",{fatal:true,ignoreBOM:true}).decode(unhex(v.data));}catch{return "0x"+v.data;}}
const sameString=(left:SavedStructuralString,right:string):boolean=>{const a=savedStructuralStringBytes(left),b=nativeStringBytes(right);return a.length===b.length&&a.every((x,i)=>x===b[i]);};
const size=(holder:object,key:string|number):boolean=>{if(typeof (holder as RecordValue)[key]!=="number")return false;const token=String(nativeCell(holder,key));return /^(?:0|[1-9]\d*)$/.test(token)&&BigInt(token)<=18446744073709551615n;};
const float=(v:unknown):boolean=>typeof v==="number"&&Number.isFinite(v)||v==="NaN"||v==="INF"||v==="-INF";
const nullableFloat=(v:unknown):boolean=>v===null||float(v);
const pair=(v:unknown):boolean=>Array.isArray(v)&&v.length===2&&v.every(float);
const nullablePair=(v:unknown):boolean=>v===null||pair(v);
const nullableBool=(v:unknown):boolean=>v===null||typeof v==="boolean";
const error=(v:unknown):v is SavedStructuralError=>keys(v,["phase","type","message"])&&[v.phase,v.type,v.message].every(isSavedStructuralString);
const nullableError=(v:unknown):boolean=>v===null||error(v);
export function isSavedStructuralTrie(v:unknown):boolean{
 const node=(x:unknown):boolean=>keys(x,["label","children","depth","final"])&&isSavedStructuralString(x.label)&&Array.isArray(x.children)&&x.children.every(node)&&size(x,"depth")&&typeof x.final==="boolean";
 return keys(v,["root","input_traces","distinct_variants","node_count","truncated"])&&node(v.root)&&["input_traces","distinct_variants","node_count"].every(k=>size(v,k))&&typeof v.truncated==="boolean";
}
export function isSavedStructuralIntervals(v:unknown):boolean{return Array.isArray(v)&&v.every(x=>keys(x,["start","end","case_id","source_activity","target_activity","trace_index","source_event_index","target_event_index"])&&float(x.start)&&float(x.end)&&[x.case_id,x.source_activity,x.target_activity].every(isSavedStructuralString)&&["trace_index","source_event_index","target_event_index"].every(k=>size(x,k)));}
export function isSavedStructuralOptions(v:unknown):boolean{return keys(v,["point","range"])&&nullableFloat(v.point)&&nullablePair(v.range);}
export function isSavedStructuralQueryResult(v:unknown):boolean{return keys(v,["point","range","intervals","point_matches","range_matches","interval_count"])&&nullableFloat(v.point)&&nullablePair(v.range)&&isSavedStructuralIntervals(v.intervals)&&["point_matches","range_matches"].every(k=>Array.isArray(v[k])&&(v[k] as unknown[]).every((_,i)=>size(v[k] as object,i)))&&size(v,"interval_count");}
export function savedStructuralCarrierBytes(carrier:SavedStructuralCarrier):Uint8Array<ArrayBuffer>{
 if(carrier.error!==null||carrier.data===null||carrier.bytes===null||carrier.encoding===null)throw Error("This native artifact was not produced.");
 const bytes=carrier.encoding==="hex-v1"?unhex(carrier.data):nativeStringBytes(carrier.data);
 if(!size(carrier,"bytes")||BigInt(String(nativeCell(carrier,"bytes")))!==BigInt(bytes.length))throw Error("Native artifact byte count differs from its bytes.");return bytes;
}
const carrier=(v:unknown):v is SavedStructuralCarrier=>{
 if(!keys(v,["dataset","bytes","encoding","data","error"])||!isSavedStructuralString(v.dataset))return false;
 if(v.error!==null)return error(v.error)&&v.bytes===null&&v.encoding===null&&v.data===null;
 if(!size(v,"bytes")||!['utf8','hex-v1'].includes(String(v.encoding))||typeof v.data!=="string"||v.encoding==="hex-v1"&&!hexPattern.test(v.data))return false;
 try{savedStructuralCarrierBytes(v as unknown as SavedStructuralCarrier);return true;}catch{return false;}
};
const RESPONSE_FIELDS=["family","operation","input_format","call_form","originals","supplied_controls","parsed_controls","input","configured_options","result","native_entered","native_returned","input_unchanged","options_unchanged","native_calls","artifacts","error"] as const;
export function isSavedStructuralResponse(v:unknown):v is SavedStructuralResponse{
 if(!keys(v,RESPONSE_FIELDS)||v.family!=="SAVED_STRUCTURAL_INPUTS"||![v.operation,v.input_format,v.call_form].every(isSavedStructuralString)||typeof v.native_entered!=="boolean"||typeof v.native_returned!=="boolean"||v.native_returned&&!v.native_entered||!nullableBool(v.input_unchanged)||!nullableBool(v.options_unchanged)||!nullableError(v.error))return false;
 if(!Array.isArray(v.originals)||!v.originals.every(x=>keys(x,["field","filename","file","original"])&&isSavedStructuralString(x.field)&&(x.filename===null||isSavedStructuralString(x.filename))&&typeof x.file==="boolean"&&carrier(x.original)&&x.original.error===null))return false;
 if(!Array.isArray(v.supplied_controls)||!v.supplied_controls.every(x=>keys(x,["name","origin"])&&isSavedStructuralString(x.name)&&(x.origin==="query"||x.origin==="multipart"))||!Array.isArray(v.parsed_controls)||!v.parsed_controls.every(isSavedStructuralString))return false;
 const input=v.input;if(!keys(input,["type","before","after","selection"])||!['TrieModel','IntervalVector',null].includes(input.type as string|null))return false;
 const selection=input.selection;if(!(selection===null||Array.isArray(selection)&&selection.every((x,i)=>typeof x==="string"||size(selection,i))))return false;
 const constructor=input.type==="TrieModel"?isSavedStructuralTrie:input.type==="IntervalVector"?isSavedStructuralIntervals:(x:unknown)=>x===null;
 if(![input.before,input.after].every(x=>x===null||constructor(x)))return false;
 if(!(v.configured_options===null||keys(v.configured_options,["before","after"])&&isSavedStructuralOptions(v.configured_options.before)&&(v.configured_options.after===null||isSavedStructuralOptions(v.configured_options.after))))return false;
 if(!(v.result===null||keys(v.result,["type","value"])&&(v.result.type==="IntervalTreeQueryResult"?isSavedStructuralQueryResult(v.result.value):v.result.type==="PetriNet"&&(keys(v.result.value,PETRI_NET_FIELDS)||keys(v.result.value,[...PETRI_NET_FIELDS,"place_names"]))&&v.result.value.discovery_markings===null&&v.result.value.transition_names===null&&isOriginalOcPlayoutNativeType("PetriNet",v.result.value))))return false;
 if(!Array.isArray(v.native_calls)||!v.native_calls.every(x=>keys(x,["name","input_type","entered","returned","error"])&&isSavedStructuralString(x.name)&&isSavedStructuralString(x.input_type)&&typeof x.entered==="boolean"&&typeof x.returned==="boolean"&&(!x.returned||x.entered)&&nullableError(x.error)))return false;
 const artifacts=v.artifacts;return keys(artifacts,SAVED_STRUCTURAL_ARTIFACTS)&&SAVED_STRUCTURAL_ARTIFACTS.every(k=>artifacts[k]===null||carrier(artifacts[k]));
}
export const savedStructuralTables=(value:SavedStructuralResponse):DiagnosticTable[]=>nativeObjectTables(value);
export function savedStructuralCarrierFile(value:SavedStructuralCarrier,artifact:SavedStructuralArtifact):File{
 const dataset=savedStructuralStringText(value.dataset).replace(/[/\\\u0000-\u001f\u007f]/g,"_");
 return new File([savedStructuralCarrierBytes(value)],"minera-saved-structures-"+dataset,{type:artifact==="native_output_bytes"?"application/octet-stream":artifact==="pnml_bytes"?"application/xml":"application/json"});
}
export function savedStructuralEditorFile(text:string,name="saved-structure.json"):File{return new File([nativeStringBytes(text)],name,{type:"application/json"});}
export interface SavedStructuralOriginal {field:string;filename:string|null;file:boolean;bytes:Uint8Array<ArrayBuffer>;local_file:File|null;}
export interface SavedStructuralRun {value:SavedStructuralResponse;raw:string;http_status:number;tables:DiagnosticTable[];request:{path:"/api/structural-inputs";operation:SavedStructuralOperation;input_format:SavedStructuralFormat;originals:SavedStructuralOriginal[]};}
export function savedStructuralRequestFile(run:SavedStructuralRun,index:number):File{const part=run.request.originals[index];if(!part)throw Error("Choose an original request part.");return part.local_file??new File([part.bytes],part.filename??part.field+".bin",{type:"application/octet-stream"});}
export function savedStructuralReport(run:SavedStructuralRun):string{return '{"family":"SAVED_STRUCTURAL_REPORT","http_status":'+run.http_status+',"request":'+nativeJson({path:run.request.path,operation:run.request.operation,input_format:run.request.input_format})+',"response":'+run.raw+'}';}
export async function runSavedStructural(uploads:{input_file?:File;options_json?:File},form:SavedStructuralForm,signal?:AbortSignal):Promise<SavedStructuralRun>{
 signal?.throwIfAborted();if(!SAVED_STRUCTURAL_OPERATIONS.includes(form.operation)||!SAVED_STRUCTURAL_FORMATS.includes(form.input_format)||!(uploads.input_file instanceof File))throw Error("Choose an operation, explicit input format and saved input File.");
 const body=new FormData(),originals:SavedStructuralOriginal[]=[];
 const append=async(field:string,text:string,file?:File)=>{if(file)body.append(field,file,file.name);else appendNativeMultipartField(body,field,text);const wire=body.get(field),physical=wire!==null&&typeof wire!=="string"?wire:null,filename=physical?await originalEditDistanceWireFilename(physical.name):null;originals.push({field,filename,file:filename!==null,bytes:file?new Uint8Array(await file.arrayBuffer()):nativeStringBytes(text),local_file:file??null});signal?.throwIfAborted();};
 await append("operation",form.operation);await append("input_format",form.input_format);if(form.call_form_supplied)await append("call_form",form.call_form);
 if(form.operation==="query_intervals"&&form.options_supplied)await append("options_json",form.options_text,uploads.options_json);
 await append("input_file","",uploads.input_file);
 const path="/api/structural-inputs" as const,response=await fetch(path,{method:"POST",body,signal}),value=await readNativeResponse(response,{path});signal?.throwIfAborted();
 if(!isSavedStructuralResponse(value))throw Error("The server did not return a complete saved-structure response (HTTP "+response.status+").");
 if(value.originals.length!==originals.length)throw Error("Saved-structure response changed original part ownership.");
 for(let i=0;i<originals.length;++i){const sent=originals[i],got=value.originals[i],bytes=savedStructuralCarrierBytes(got.original);if(!sameString(got.field,sent.field)||got.file!==sent.file||(sent.filename===null?got.filename!==null:got.filename===null||!sameString(got.filename,sent.filename))||bytes.length!==sent.bytes.length||!bytes.every((x,j)=>x===sent.bytes[j]))throw Error("Saved-structure response changed original bytes, order or filename presence.");}
 const controls=originals.filter(x=>x.field!=="input_file");if(value.supplied_controls.length!==controls.length||value.supplied_controls.some((x,i)=>x.origin!=="multipart"||!sameString(x.name,controls[i].field)))throw Error("Saved-structure response changed supplied controls.");
 for(const field of ['operation','input_format'] as const)if(value.parsed_controls.some(x=>sameString(x,field))&&!sameString(value[field],form[field]))throw Error("Saved-structure response changed the selected "+field+".");
 if(form.call_form_supplied&&value.parsed_controls.some(x=>sameString(x,"call_form"))&&!sameString(value.call_form,form.call_form))throw Error("Saved-structure response changed the selected call form.");
 return {value,raw:nativeResponseJson(value),http_status:response.status,tables:savedStructuralTables(value),request:{path,operation:form.operation,input_format:form.input_format,originals}};
}
export interface SavedStructuralReuse {label:string;role:"input_file"|"options_json";format:SavedStructuralFormat;operation:SavedStructuralOperation;file:File;}
export function savedStructuralReusableInputs(run:SavedStructuralRun):SavedStructuralReuse[]{
 const result:SavedStructuralReuse[]=[],a=run.value.artifacts,add=(key:SavedStructuralArtifact,label:string,role:SavedStructuralReuse['role'],format:SavedStructuralFormat)=>{const c=a[key];if(c&&c.error===null)result.push({label,role,format,operation:run.request.operation,file:savedStructuralCarrierFile(c,key)});};
 add("constructor_json","Use canonical constructor","input_file","direct");if(run.request.operation==="query_intervals"){add("result_json","Query the same saved interval order again","input_file","query_result");add("options_json","Use complete returned query settings","options_json",run.request.input_format);}return result;
}
export const SAVED_STRUCTURAL_EXAMPLES:Record<SavedStructuralOperation,string>={trie_to_petri:'{"root":{"label":"","children":[{"label":"Review","children":[],"depth":1,"final":true}],"depth":0,"final":true},"input_traces":2,"distinct_variants":2,"node_count":2,"truncated":false}',query_intervals:'[{"start":10,"end":20,"case_id":"later first","source_activity":"Review","target_activity":"Archive","trace_index":7,"source_event_index":4,"target_event_index":5},{"start":0,"end":12,"case_id":"earlier second","source_activity":"Open","target_activity":"Review","trace_index":2,"source_event_index":0,"target_event_index":1}]'};
