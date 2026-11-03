import {miningSourceHash} from "./active-mining-source";
import { backendFetch } from "./backend-fetch";
import {nativeObjectTables,type DiagnosticTable} from "./model-diagnostics";
import {nativeCell,nativeJson,parseNativeJson} from "./native-json";
import {appendNativeMultipartField,nativeStringBytes} from "./native-multipart";
import {nativeResponseJson,readNativeResponse} from "./native-response";
import {isOriginalEmbeddingFilterNativeString,isOriginalEmbeddingFilterRawLog,originalEmbeddingFilterOriginalBytes,originalEmbeddingFilterStringBytes,originalEmbeddingFilterStringText,type OriginalEmbeddingFilterBytes,type OriginalEmbeddingFilterNativeString} from "./original-embedding-filter-workbench";
import {defaultOriginalEditDistanceControl,originalEditDistanceFileName,originalEditDistanceWireFilename,type OriginalEditDistanceControl,type OriginalEditDistanceRequestOriginal} from "./original-edit-distance-workbench";
import {originalRootCauseEditMember,originalRootCauseRawMember} from "./original-root-cause-workbench";
export {OriginalEditDistanceRequestGate as OriginalAnonymizationRequestGate} from "./original-edit-distance-workbench";
export type OriginalAnonymizationBytes=OriginalEmbeddingFilterBytes;
export type OriginalAnonymizationNativeString=OriginalEmbeddingFilterNativeString;
export type OriginalAnonymizationControl=OriginalEditDistanceControl;
export const ORIGINAL_ANONYMIZATION_ARTIFACTS=["source_log_json", "query_log_json", "diagnostics_input_json", "source_after_json", "query_after_json", "diagnostics_after_json", "configured_options_json", "full_result_json", "released_log_json", "result_diagnostics_json", "native_text", "result_xes"] as const;
export type OriginalAnonymizationArtifact=typeof ORIGINAL_ANONYMIZATION_ARTIFACTS[number];
export const ORIGINAL_ANONYMIZATION_SPECS={
 "anonymize_trace_variant_query": {
  "label": "anonymize trace variant query",
  "call": "anonymize_trace_variant_query(source, options)",
  "input": "EventLog",
  "roles": [
   "source"
  ],
  "options": "TraceVariantQueryOptions",
  "result": "TraceVariantQueryResult"
 },
 "query_trace_variants": {
  "label": "query trace variants",
  "call": "query_trace_variants(source, options)",
  "input": "EventLog",
  "roles": [
   "source"
  ],
  "options": "TraceVariantQueryOptions",
  "result": "TraceVariantQueryResult"
 },
 "discover_trace_variant_query": {
  "label": "discover trace variant query",
  "call": "discover_trace_variant_query(source, options)",
  "input": "EventLog",
  "roles": [
   "source"
  ],
  "options": "TraceVariantQueryOptions",
  "result": "TraceVariantQueryResult"
 },
 "anonymize_trace_variants": {
  "label": "anonymize trace variants",
  "call": "anonymize_trace_variants(source, options)",
  "input": "EventLog",
  "roles": [
   "source"
  ],
  "options": "TraceVariantQueryOptions",
  "result": "TraceVariantQueryResult"
 },
 "anonymize_pripel": {
  "label": "anonymize pripel",
  "call": "anonymize_pripel(source, released_query, options)",
  "input": "PripelInputPair",
  "roles": [
   "source",
   "query"
  ],
  "options": "PripelOptions",
  "result": "PripelResult"
 },
 "pripel_anonymize": {
  "label": "pripel anonymize",
  "call": "pripel_anonymize(source, released_query, options)",
  "input": "PripelInputPair",
  "roles": [
   "source",
   "query"
  ],
  "options": "PripelOptions",
  "result": "PripelResult"
 },
 "anonymize_pripel_log": {
  "label": "anonymize pripel log",
  "call": "anonymize_pripel_log(source, released_query, options)",
  "input": "PripelInputPair",
  "roles": [
   "source",
   "query"
  ],
  "options": "PripelOptions",
  "result": "PripelResult"
 },
 "pripel_diagnostics_to_json": {
  "label": "pripel diagnostics to json",
  "call": "diagnostics.to_json()",
  "input": "PripelDiagnostics",
  "roles": [
   "diagnostics"
  ],
  "options": null,
  "result": "std::string"
 }
} as const;
export type OriginalAnonymizationOperation=keyof typeof ORIGINAL_ANONYMIZATION_SPECS;
export const ORIGINAL_ANONYMIZATION_OPERATIONS=Object.keys(ORIGINAL_ANONYMIZATION_SPECS) as OriginalAnonymizationOperation[];
export const ORIGINAL_ANONYMIZATION_RECORD_SCHEMAS={
 "TraceVariantQueryOptions": {
  "variant": "TraceVariantQueryVariant",
  "epsilon": "double",
  "max_prefix_length": "std::size_t",
  "prune_threshold": "std::uint64_t",
  "seed": "std::uint64_t",
  "secure_random": "bool",
  "max_output_traces": "std::size_t",
  "max_output_variants": "std::size_t"
 },
 "TraceVariantQueryDiagnostics": {
  "variant": "TraceVariantQueryVariant",
  "epsilon": "double",
  "max_prefix_length": "std::size_t",
  "prune_threshold": "std::uint64_t",
  "seed": "std::uint64_t",
  "secure_random": "bool",
  "candidate_count": "std::size_t",
  "selected_violation_count": "std::size_t",
  "retained_variant_count": "std::size_t",
  "released_trace_count": "std::uint64_t",
  "output_truncated": "bool",
  "message": "std::string"
 },
 "TraceVariantQueryResult": {
  "log": "EventLog",
  "frequencies": "TraceVariantQueryFrequencies",
  "diagnostics": "TraceVariantQueryDiagnostics"
 },
 "PripelOptions": {
  "epsilon": "double",
  "blocklist": "std::vector<std::string>",
  "seed": "std::uint64_t",
  "secure_random": "bool",
  "max_output_traces": "std::size_t",
  "max_distance": "std::size_t"
 },
 "PripelDiagnostics": {
  "epsilon": "double",
  "seed": "std::uint64_t",
  "secure_random": "bool",
  "query_trace_count": "std::size_t",
  "released_trace_count": "std::size_t",
  "exact_matches": "std::size_t",
  "nearest_matches": "std::size_t",
  "synthesized_traces": "std::size_t",
  "synthesized_events": "std::size_t",
  "output_truncated": "bool",
  "message": "std::string"
 },
 "PripelResult": {
  "log": "EventLog",
  "diagnostics": "PripelDiagnostics"
 }
} as const;
export const ORIGINAL_ANONYMIZATION_OPTION_MEMBERS={
 "TraceVariantQueryOptions": [
  "variant",
  "epsilon",
  "max_prefix_length",
  "prune_threshold",
  "seed",
  "secure_random",
  "max_output_traces",
  "max_output_variants"
 ],
 "PripelOptions": [
  "epsilon",
  "blocklist",
  "seed",
  "secure_random",
  "max_output_traces",
  "max_distance"
 ]
} as const;

export type OriginalAnonymizationRole="source"|"query"|"diagnostics";
export interface OriginalAnonymizationForm {operation:OriginalAnonymizationOperation;options:OriginalAnonymizationControl;}
export const defaultOriginalAnonymizationForm=(operation:OriginalAnonymizationOperation="anonymize_trace_variant_query"):OriginalAnonymizationForm=>({operation,options:defaultOriginalEditDistanceControl("{}",false)});
export interface OriginalAnonymizationSubmittedControl {name:string;text:string;file:File|null;}
export function originalAnonymizationControls(form:OriginalAnonymizationForm):OriginalAnonymizationSubmittedControl[]{
 if(!Object.hasOwn(ORIGINAL_ANONYMIZATION_SPECS,form.operation))throw new Error("Choose an original anonymization function.");
 const controls:OriginalAnonymizationSubmittedControl[]=[{name:"operation",text:form.operation,file:null}];
 if(form.options.supplied){if(ORIGINAL_ANONYMIZATION_SPECS[form.operation].options===null)throw new Error("options_json is inapplicable to diagnostics.to_json().");controls.push({name:"options_json",text:form.options.text,file:form.options.file});}return controls;
}
const object=(value:unknown):value is Record<string,unknown>=>value!==null&&typeof value==="object"&&!Array.isArray(value);
const exact=(value:Record<string,unknown>,keys:readonly string[]):boolean=>Object.keys(value).length===keys.length&&keys.every(key=>Object.hasOwn(value,key));
const list=(value:unknown,predicate:(item:unknown)=>boolean):value is unknown[]=>Array.isArray(value)&&value.every(predicate);
const nullable=(value:unknown,predicate:(item:unknown)=>boolean):boolean=>value===null||predicate(value);
const strings=(value:unknown):value is string[]=>list(value,item=>typeof item==="string");
const nativeString=isOriginalEmbeddingFilterNativeString;
const equalBytes=(a:Uint8Array,b:Uint8Array):boolean=>a.length===b.length&&a.every((byte,index)=>byte===b[index]);
const sameString=(a:OriginalAnonymizationNativeString,b:OriginalAnonymizationNativeString):boolean=>equalBytes(originalEmbeddingFilterStringBytes(a),originalEmbeddingFilterStringBytes(b));
function integer(value:unknown,holder:object,key:string|number,signed=false,bits=64):boolean{if(typeof value!=="number")return false;const token=String(nativeCell(holder,key));if(!(signed?/^-?(?:0|[1-9]\d*)$/:/^(?:0|[1-9]\d*)$/).test(token))return false;const n=BigInt(token),limit=1n<<BigInt(bits-(signed?1:0));return n>=(signed?-limit:0n)&&n<limit;}
const double=(value:unknown):boolean=>typeof value==="number"&&Number.isFinite(value)||typeof value==="string"&&["NaN","INF","-INF"].includes(value);
export function isOriginalAnonymizationVariant(value:unknown,intBits=32):boolean{if(!object(value)||!exact(value,["name","index"])||!integer(value.index,value,"index",true,intBits))return false;const n=BigInt(String(nativeCell(value,"index")));return value.name===(n===0n?"laplace":n===1n?"sacofa":null);}
export function isOriginalAnonymizationNativeType(type:string,value:unknown,holder?:object,key?:string|number,sizeBits=64,intBits=32):boolean{
 if(type==="EventLog")return isOriginalEmbeddingFilterRawLog(value);
 if(type==="std::string")return nativeString(value);if(type==="double")return double(value);if(type==="bool")return typeof value==="boolean";
 if(type==="std::size_t"||type==="std::uint64_t")return !!holder&&key!==undefined&&integer(value,holder,key,false,type==="std::uint64_t"?64:sizeBits);
 if(type==="TraceVariantQueryVariant")return isOriginalAnonymizationVariant(value,intBits);
 if(type==="std::vector<std::string>")return list(value,nativeString);
 if(type==="TraceVariantQueryFrequencies")return list(value,item=>object(item)&&exact(item,["activities","count"])&&list(item.activities,nativeString)&&integer(item.count,item,"count"));
 const schema=Object.hasOwn(ORIGINAL_ANONYMIZATION_RECORD_SCHEMAS,type)?(ORIGINAL_ANONYMIZATION_RECORD_SCHEMAS as Record<string,Record<string,string>>)[type]:null;
 return !!schema&&object(value)&&exact(value,Object.keys(schema))&&Object.entries(schema).every(([name,member])=>isOriginalAnonymizationNativeType(member,value[name],value,name,sizeBits,intBits));
}
export const isOriginalAnonymizationOptions=(type:"TraceVariantQueryOptions"|"PripelOptions",value:unknown,sizeBits=64,intBits=32):value is Record<string,unknown>=>isOriginalAnonymizationNativeType(type,value,undefined,undefined,sizeBits,intBits);
export const isOriginalAnonymizationDiagnostics=(type:"TraceVariantQueryDiagnostics"|"PripelDiagnostics",value:unknown,sizeBits=64,intBits=32):value is Record<string,unknown>=>isOriginalAnonymizationNativeType(type,value,undefined,undefined,sizeBits,intBits);
export interface OriginalAnonymizationResult {type:string;value:unknown;}
export function isOriginalAnonymizationResult(value:unknown,sizeBits=64,intBits=32):value is OriginalAnonymizationResult{return object(value)&&exact(value,["type","value"])&&typeof value.type==="string"&&["TraceVariantQueryResult","PripelResult","std::string"].includes(value.type)&&isOriginalAnonymizationNativeType(value.type,value.value,undefined,undefined,sizeBits,intBits);}
export function isOriginalAnonymizationBytes(value:unknown):value is OriginalAnonymizationBytes{return object(value)&&exact(value,["dataset","encoding","data","bytes","sha256"])&&nativeString(value.dataset)&&typeof value.data==="string"&&integer(value.bytes,value,"bytes")&&typeof value.sha256==="string"&&/^[a-f0-9]{64}$/.test(value.sha256)&&(value.encoding==="utf8"||value.encoding==="hex"&&/^(?:[a-fA-F0-9]{2})*$/.test(value.data));}
export interface OriginalAnonymizationError {phase:string;exception_type:OriginalAnonymizationNativeString;message:OriginalAnonymizationNativeString;}
const error=(value:unknown):value is OriginalAnonymizationError=>object(value)&&exact(value,["phase","exception_type","message"])&&typeof value.phase==="string"&&nativeString(value.exception_type)&&nativeString(value.message);
export interface OriginalAnonymizationCall {name:OriginalAnonymizationNativeString;stage:"primary"|"export";input_kind:OriginalAnonymizationNativeString;returned:boolean;error:OriginalAnonymizationError|null;}
const call=(value:unknown):value is OriginalAnonymizationCall=>object(value)&&exact(value,["name","stage","input_kind","returned","error"])&&nativeString(value.name)&&["primary","export"].includes(String(value.stage))&&nativeString(value.input_kind)&&typeof value.returned==="boolean"&&nullable(value.error,error);
type Snapshot={source:Record<string,unknown>|null;query:Record<string,unknown>|null;diagnostics:Record<string,unknown>|null};
export interface OriginalAnonymizationWorkbench extends Record<string,unknown>{family:"ORIGINAL_ANONYMIZATION_WORKBENCH";version:"original-anonymization-workbench-v1";operation:OriginalAnonymizationNativeString;executed:boolean;native_entered:boolean;native_returned:boolean;native_calls:OriginalAnonymizationCall[];error:OriginalAnonymizationError|null;originals:Array<{field:OriginalAnonymizationNativeString;filename:OriginalAnonymizationNativeString|null;file:boolean;original:OriginalAnonymizationBytes}>;parsed_controls:string[];supplied_controls:Array<{name:OriginalAnonymizationNativeString;origin:"query"|"multipart"}>;input_context:{size_t_bytes:number;int_bytes:number;input_type:OriginalAnonymizationNativeString|null;source_prepared:boolean;query_prepared:boolean;diagnostics_prepared:boolean;input_unchanged:boolean|null;options_unchanged:boolean|null};input:{before:Snapshot;after:Snapshot};submitted_options:OriginalAnonymizationBytes|null;configured_options:Record<string,unknown>|null;supplied_options:string[];result:OriginalAnonymizationResult|null;artifact_bytes:Record<OriginalAnonymizationArtifact,OriginalAnonymizationBytes|null>;artifact_errors:Array<{name:"result_xes";error:OriginalAnonymizationError}>;}
const dataset=(name:OriginalAnonymizationArtifact):string=>name==="native_text"?"native-diagnostics.txt":name==="result_xes"?"result.xes":name+".json";
export function isOriginalAnonymizationWorkbench(value:unknown):value is OriginalAnonymizationWorkbench{
 if(!object(value)||!exact(value,["family","version","operation","executed","native_entered","native_returned","native_calls","error","originals","parsed_controls","supplied_controls","input_context","input","submitted_options","configured_options","supplied_options","result","artifact_bytes","artifact_errors"])||value.family!=="ORIGINAL_ANONYMIZATION_WORKBENCH"||value.version!=="original-anonymization-workbench-v1"||!nativeString(value.operation)||!["executed","native_entered","native_returned"].every(key=>typeof value[key]==="boolean")||value.executed!==value.native_returned||value.native_returned&&!value.native_entered||!list(value.native_calls,call)||!nullable(value.error,error)||!list(value.originals,item=>object(item)&&exact(item,["field","filename","file","original"])&&nativeString(item.field)&&nullable(item.filename,nativeString)&&typeof item.file==="boolean"&&item.file===(item.filename!==null)&&isOriginalAnonymizationBytes(item.original))||!strings(value.parsed_controls)||!strings(value.supplied_options)||!list(value.supplied_controls,item=>object(item)&&exact(item,["name","origin"])&&nativeString(item.name)&&["query","multipart"].includes(String(item.origin)))||!object(value.input_context)||!object(value.input)||!nullable(value.submitted_options,isOriginalAnonymizationBytes)||!nullable(value.configured_options,object)||!object(value.artifact_bytes)||!exact(value.artifact_bytes,ORIGINAL_ANONYMIZATION_ARTIFACTS)||!ORIGINAL_ANONYMIZATION_ARTIFACTS.every(name=>nullable((value.artifact_bytes as Record<string,unknown>)[name],item=>isOriginalAnonymizationBytes(item)&&sameString(item.dataset,dataset(name))))||!list(value.artifact_errors,item=>object(item)&&exact(item,["name","error"])&&item.name==="result_xes"&&error(item.error)&&item.error.phase==="export_error"))return false;
 const c=value.input_context,sizeBits=c.size_t_bytes===4?32:c.size_t_bytes===8?64:0,intBits=c.int_bytes===4?32:c.int_bytes===8?64:0;
 const snapshot=(item:unknown):item is Snapshot=>object(item)&&exact(item,["source","query","diagnostics"])&&nullable(item.source,isOriginalEmbeddingFilterRawLog)&&nullable(item.query,isOriginalEmbeddingFilterRawLog)&&nullable(item.diagnostics,d=>isOriginalAnonymizationDiagnostics("PripelDiagnostics",d,sizeBits,intBits));
 if(!sizeBits||!intBits||!exact(c,["size_t_bytes","int_bytes","input_type","source_prepared","query_prepared","diagnostics_prepared","input_unchanged","options_unchanged"])||!nullable(c.input_type,nativeString)||!["source_prepared","query_prepared","diagnostics_prepared"].every(key=>typeof c[key]==="boolean")||!["input_unchanged","options_unchanged"].every(key=>c[key]===null||typeof c[key]==="boolean")||!exact(value.input,["before","after"])||!snapshot(value.input.before)||!snapshot(value.input.after)||!nullable(value.result,item=>isOriginalAnonymizationResult(item,sizeBits,intBits)))return false;
 if(value.error!==null)return true;
 const op=typeof value.operation==="string"&&Object.hasOwn(ORIGINAL_ANONYMIZATION_SPECS,value.operation)?value.operation as OriginalAnonymizationOperation:null;if(!op)return false;
 const spec=ORIGINAL_ANONYMIZATION_SPECS[op],before=value.input.before,after=value.input.after;
 if(!value.native_entered||!value.native_returned||!object(value.result)||value.result.type!==spec.result||c.input_type===null||!sameString(c.input_type as OriginalAnonymizationNativeString,spec.input)||c.input_unchanged!==true)return false;
 if(!spec.roles.every(role=>before[role]!==null&&after[role]!==null&&c[role+"_prepared"]===true&&(value.originals as OriginalAnonymizationWorkbench["originals"]).some(item=>sameString(item.field,role+"_file"))))return false;
 if((['source','query','diagnostics'] as const).some(role=>!spec.roles.some(allowed=>allowed===role)&&(before[role]!==null||after[role]!==null||c[role+"_prepared"]!==false)))return false;
 const artifacts=value.artifact_bytes as OriginalAnonymizationWorkbench['artifact_bytes'];
 if(artifacts.full_result_json===null)return false;
 const exporters=(value.native_calls as OriginalAnonymizationCall[]).filter(item=>item.stage==="export");
 if(spec.result==="std::string"){if(artifacts.native_text===null||exporters.length!==0||artifacts.released_log_json!==null||artifacts.result_diagnostics_json!==null||artifacts.result_xes!==null||value.artifact_errors.length!==0)return false;}
 else if(artifacts.released_log_json===null||artifacts.result_diagnostics_json===null||artifacts.native_text!==null||exporters.length!==1||!sameString(exporters[0].name,"serialize_xes(result.log)")||!sameString(exporters[0].input_kind,"EventLog")||(artifacts.result_xes===null?exporters[0].returned||value.artifact_errors.length!==1:!exporters[0].returned||exporters[0].error!==null||value.artifact_errors.length!==0))return false;
 const primary=(value.native_calls as OriginalAnonymizationCall[]).filter(item=>item.stage==="primary");
 return primary.length===1&&sameString(primary[0].name,spec.call)&&sameString(primary[0].input_kind,spec.input)&&primary[0].returned&&primary[0].error===null&&(spec.options?isOriginalAnonymizationOptions(spec.options,value.configured_options,sizeBits,intBits)&&c.options_unchanged===true:value.configured_options===null&&value.submitted_options===null&&value.supplied_options.length===0&&c.options_unchanged===null);
}
const hex=(bytes:Uint8Array):string=>Array.from(bytes,byte=>byte.toString(16).padStart(2,"0")).join("");
const fromHex=(raw:string):Uint8Array<ArrayBuffer>=>Uint8Array.from(raw.match(/../g)??[],pair=>parseInt(pair,16));
export interface OriginalAnonymizationRequestOriginal extends OriginalEditDistanceRequestOriginal {content_type:string|null;}
export interface OriginalAnonymizationRun {value:OriginalAnonymizationWorkbench;raw:string;http_status:number;tables:DiagnosticTable[];request:{path:"/api/anonymization-workbench";operation:OriginalAnonymizationOperation;originals:OriginalAnonymizationRequestOriginal[]};uploads:Partial<Record<OriginalAnonymizationRole,File>>;controls:OriginalAnonymizationSubmittedControl[];}
export async function runOriginalAnonymizationWorkbench(uploads:Partial<Record<OriginalAnonymizationRole,File>>,form:OriginalAnonymizationForm,signal?:AbortSignal):Promise<OriginalAnonymizationRun>{
 signal?.throwIfAborted();const operation=form.operation,controls=originalAnonymizationControls(form),spec=ORIGINAL_ANONYMIZATION_SPECS[operation],files={...uploads},roles:readonly OriginalAnonymizationRole[]=spec.roles;
 for(const role of ['source','query','diagnostics'] as const){if(roles.includes(role)&&!(files[role] instanceof File))throw new Error("Choose an independent "+role+" File.");if(!roles.includes(role)&&files[role])throw new Error(role+"_file is inapplicable to this function.");}
 const body=new FormData(),originals:OriginalAnonymizationRequestOriginal[]=[];
 const capture=async(field:string,bytes:Uint8Array<ArrayBuffer>,localFile:File|null)=>{const transported=body.get(field),wireFile=typeof transported!=="string"&&transported!==null?transported:null;originals.push({field,filename:wireFile?await originalEditDistanceWireFilename(wireFile.name):null,local_filename:localFile?.name??null,local_last_modified:localFile?.lastModified??null,content_type:wireFile?wireFile.type||"application/octet-stream":null,bytes:bytes.length,sha256:await miningSourceHash(new File([bytes],field)),encoding:"hex",data:hex(bytes)});signal?.throwIfAborted();};
 for(const control of controls){const bytes=control.file?new Uint8Array(await control.file.arrayBuffer()):nativeStringBytes(control.text);if(control.file)body.append(control.name,control.file,control.file.name);else appendNativeMultipartField(body,control.name,control.text);await capture(control.name,bytes,control.file);}
 for(const role of roles){const file=files[role]!;body.append(role+"_file",file,file.name);await capture(role+"_file",new Uint8Array(await file.arrayBuffer()),file);}
 const request:OriginalAnonymizationRun['request']={path:"/api/anonymization-workbench",operation,originals},response=await backendFetch(request.path,{method:"POST",body,signal}),value=await readNativeResponse(response,{path:request.path,fields:controls.filter(item=>!item.file).map(item=>[item.name,item.text])});signal?.throwIfAborted();
 if(!isOriginalAnonymizationWorkbench(value))throw new Error(object(value)&&typeof value.error==="string"?value.error:"The server did not return a complete owned anonymization response (HTTP "+response.status+").");
 const transportFailure=!response.ok&&value.error?.phase==="configuration_error"&&value.operation==="anonymize_trace_variant_query"&&!value.native_entered&&!value.native_returned&&value.originals.length===0&&value.native_calls.length===0&&value.parsed_controls.length===0&&value.supplied_controls.length===0&&value.configured_options===null&&value.submitted_options===null&&value.supplied_options.length===0&&value.result===null&&Object.values(value.input.before).every(item=>item===null)&&Object.values(value.input.after).every(item=>item===null)&&Object.values(value.artifact_bytes).every(item=>item===null)&&value.artifact_errors.length===0&&value.input_context.input_type===null&&value.input_context.input_unchanged===null&&value.input_context.options_unchanged===null;
 if(!transportFailure&&(value.operation!==operation||value.originals.length!==originals.length||value.supplied_controls.length!==controls.length||value.supplied_controls.some((item,index)=>item.origin!=="multipart"||!sameString(item.name,controls[index].name))))throw new Error("The anonymization response changed operation or control ownership.");
 if(!transportFailure)for(let index=0;index<originals.length;++index){const submitted=originals[index],actual=value.originals[index];if(!sameString(actual.field,submitted.field)||actual.original.sha256!==submitted.sha256||BigInt(String(nativeCell(actual.original,"bytes")))!==BigInt(submitted.bytes)||!equalBytes(originalEmbeddingFilterOriginalBytes(actual.original),fromHex(submitted.data))||actual.file!==(submitted.filename!==null)||(submitted.filename===null?actual.filename!==null:actual.filename===null||!sameString(actual.filename,submitted.filename))||!sameString(actual.original.dataset,submitted.filename??submitted.field))throw new Error("The anonymization response changed original bytes, hash, order, dataset or filename presence.");}
 if(!transportFailure){const submitted=originals.find(item=>item.field==="options_json"),actual=value.submitted_options;if(submitted?actual===null||actual.sha256!==submitted.sha256||!sameString(actual.dataset,submitted.filename??submitted.field)||!equalBytes(originalEmbeddingFilterOriginalBytes(actual),fromHex(submitted.data)):actual!==null)throw new Error("The anonymization response changed submitted options control bytes or presence.");}
 for(const carrier of [...Object.values(value.artifact_bytes),value.submitted_options].filter((item):item is OriginalAnonymizationBytes=>item!==null)){const bytes=originalEmbeddingFilterOriginalBytes(carrier);if(BigInt(String(nativeCell(carrier,"bytes")))!==BigInt(bytes.length)||await miningSourceHash(new File([bytes],"carrier"))!==carrier.sha256)throw new Error("The anonymization response changed authoritative carrier bytes or hash.");signal?.throwIfAborted();}
 const raw=nativeResponseJson(value),paths:Partial<Record<OriginalAnonymizationArtifact,string[]>>={source_log_json:["input","before","source"],query_log_json:["input","before","query"],diagnostics_input_json:["input","before","diagnostics"],source_after_json:["input","after","source"],query_after_json:["input","after","query"],diagnostics_after_json:["input","after","diagnostics"],configured_options_json:["configured_options"],full_result_json:["result"]};
 if(value.result&&value.result.type!=="std::string"){paths.released_log_json=["result","value","log"];paths.result_diagnostics_json=["result","value","diagnostics"];}
 for(const [name,path] of Object.entries(paths)){if(!path)continue;const fragment=originalRootCauseRawMember(raw,path),carrier=value.artifact_bytes[name as OriginalAnonymizationArtifact];if(fragment==="null"?carrier!==null:carrier===null||!equalBytes(originalEmbeddingFilterOriginalBytes(carrier),nativeStringBytes(fragment)))throw new Error("The canonical "+name+" artifact differs from its actual response fragment.");}
 if(value.result?.type==="std::string"){const carrier=value.artifact_bytes.native_text;if(!carrier||!equalBytes(originalEmbeddingFilterOriginalBytes(carrier),originalEmbeddingFilterStringBytes(value.result.value as OriginalAnonymizationNativeString)))throw new Error("The native diagnostics text bytes differ from the actual returned string.");}
 if(!response.ok&&value.error===null)throw new Error("The server returned a native result with failure status "+response.status+".");
 return {value,raw,http_status:response.status,request,uploads:files,controls,tables:originalAnonymizationTables(value,originals)};
}
export function originalAnonymizationTables(value:OriginalAnonymizationWorkbench,originals?:OriginalAnonymizationRequestOriginal[]):DiagnosticTable[]{const tables=nativeObjectTables(value);for(const table of tables)if(table.records?.some(item=>item===null||typeof item!=="object"))table.records=table.records.map((item,index)=>object(item)||Array.isArray(item)?item:parseNativeJson('{"value":'+(typeof item==="number"?nativeCell(table.records!,index):nativeJson(item))+'}'));if(value.result)tables.push({id:"complete_native_result",title:"Complete "+value.result.type+" storage",columns:["type","value"],rows:[[value.result.type,value.result.value]],records:[value.result]});if(originals)tables.push({id:"exact_request_originals",title:"Every original control and role byte",columns:["field","wire_filename","local_filename","content_type","bytes","sha256","hex"],rows:originals.map(item=>[item.field,item.filename,item.local_filename,item.content_type,item.bytes,item.sha256,item.data]),records:originals});return tables;}
export const originalAnonymizationReport=(run:OriginalAnonymizationRun):string=>nativeJson({family:"ORIGINAL_ANONYMIZATION_WORKBENCH_REPORT",http_status:run.http_status,request:run.request,raw_native_response:run.raw,native_response:run.value},2);
export function originalAnonymizationCarrierFile(carrier:OriginalAnonymizationBytes):File{const name=originalEditDistanceFileName(originalEmbeddingFilterStringText(carrier.dataset));return new File([originalEmbeddingFilterOriginalBytes(carrier)],"minera-anonymization-"+name,{type:name.endsWith(".json")?"application/json":name.endsWith(".xes")?"application/xml":"application/octet-stream"});}
export function originalAnonymizationRequestFile(run:OriginalAnonymizationRun,index:number):File{const item=run.request.originals[index];if(!item)throw new Error("Choose an owned request part.");return new File([fromHex(item.data)],item.local_filename??item.filename??item.field+".txt",{type:item.content_type??"application/octet-stream",...(item.local_last_modified===null?{}:{lastModified:item.local_last_modified})});}
export function originalAnonymizationResultFile(run:OriginalAnonymizationRun):File{const carrier=run.value.artifact_bytes.full_result_json;if(!carrier)throw new Error("No complete canonical result was returned.");return originalAnonymizationCarrierFile(carrier);}
export interface OriginalAnonymizationReusable {label:string;operation:OriginalAnonymizationOperation;role:OriginalAnonymizationRole|"options";file:File;}
export function originalAnonymizationReusableInputs(run:OriginalAnonymizationRun):OriginalAnonymizationReusable[]{const entries:OriginalAnonymizationReusable[]=[],a=run.value.artifact_bytes;for(const name of ["source_log_json","query_log_json","source_after_json","query_after_json","released_log_json"] as const){const carrier=a[name];if(!carrier)continue;for(const operation of ORIGINAL_ANONYMIZATION_OPERATIONS)for(const role of ORIGINAL_ANONYMIZATION_SPECS[operation].roles)if(role!=="diagnostics")entries.push({label:"Use actual "+name+" as "+role+" for "+operation,operation,role,file:originalAnonymizationCarrierFile(carrier)});}
 if(a.configured_options_json&&ORIGINAL_ANONYMIZATION_SPECS[run.request.operation].options)entries.push({label:"Reuse complete configured options as File",operation:run.request.operation,role:"options",file:originalAnonymizationCarrierFile(a.configured_options_json)});
 for(const name of ["diagnostics_input_json","diagnostics_after_json"] as const)if(a[name])entries.push({label:"Reuse actual "+name+" for diagnostics.to_json()",operation:"pripel_diagnostics_to_json",role:"diagnostics",file:originalAnonymizationCarrierFile(a[name])});
 if(run.value.result?.type==="PripelResult"&&a.result_diagnostics_json)entries.push({label:"Use actual eleven-field PRIPEL diagnostics",operation:"pripel_diagnostics_to_json",role:"diagnostics",file:originalAnonymizationCarrierFile(a.result_diagnostics_json)});return entries;
}
export const originalAnonymizationEditMember=originalRootCauseEditMember;
export const ORIGINAL_ANONYMIZATION_DEFAULT_LOG='{"traces":[{"case_id":"case-1","events":[{"activity":"A"},{"activity":"B"}]}]}';
export const ORIGINAL_ANONYMIZATION_DEFAULT_DIAGNOSTICS='{"epsilon":1,"seed":0,"secure_random":false,"query_trace_count":1,"released_trace_count":1,"exact_matches":1,"nearest_matches":0,"synthesized_traces":0,"synthesized_events":0,"output_truncated":false,"message":""}';
export const originalAnonymizationDefaultInput=(role:OriginalAnonymizationRole):string=>role==="diagnostics"?ORIGINAL_ANONYMIZATION_DEFAULT_DIAGNOSTICS:ORIGINAL_ANONYMIZATION_DEFAULT_LOG;
