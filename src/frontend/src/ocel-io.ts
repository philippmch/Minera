import { type OcelExchangeResult,type OcelSqliteResult,isOcelExchangeResult,isOcelSqliteResult } from './types';
import { backendFetch } from "./backend-fetch";
import { parseNativeJson,nativeJson } from './native-json';
import { preparedOcelTables,type OcelPreparedTable } from './ocel-preparation';
export const OCEL_IMPORT_FORMATS=[['json','Classic JSON'],['json_standard','Standard OCEL 2.0 JSON'],['xml','Classic XML'],['xml_standard','Standard OCEL 2.0 XML'],['csv','Event CSV with optional object table'],['sqlite','OCEL 2.0 SQLite']] as const;
export const OCEL_EXPORT_FORMATS=[...OCEL_IMPORT_FORMATS.filter(([id])=>id!=='csv'),['csv','Event CSV'],['csv_objects','Object CSV'],['csv_pair','Event and object CSV pair']] as const;
export type OcelIoFormat=typeof OCEL_IMPORT_FORMATS[number][0]|'csv_objects'|'csv_pair';
export const OCEL_CSV_FIELDS=[['ocel_csv_event_id_column','Event ID column'],['ocel_csv_activity_column','Activity column'],['ocel_csv_timestamp_column','Timestamp column'],['ocel_csv_object_type_prefix','Object type column prefix'],['ocel_csv_object_id_column','Object ID column'],['ocel_csv_object_type_column','Object type column']] as const;
export const OCEL_IO_SWITCHES=[['ocel_csv_trim_fields','Trim CSV fields','csv','import'],['ocel_csv_sort_events_by_timestamp','Sort imported events by timestamp','csv','import'],['ocel_csv_reject_unsupported','Reject unsupported CSV fields','csv','import'],['ocel_csv_allow_lossy','Allow CSV data loss','csv','export'],['reject_unsupported','Reject unsupported XML constructs','xml','import'],['include_declaration','Include XML declaration','xml','export'],['allow_lossy','XML writer loss allowance','xml','export'],['validate_references','Validate SQLite relation references','sqlite','both'],['readable_type_table_names','Readable SQLite type table names','sqlite','export']] as const;
export interface OcelIoForm {mode:'import'|'export';format:OcelIoFormat;columns:Record<typeof OCEL_CSV_FIELDS[number][0],string>;delimiter:string;switches:Record<typeof OCEL_IO_SWITCHES[number][0],'native'|'true'|'false'>;maxBytes:string;}
export function defaultOcelIo(mode:'import'|'export'='import'):OcelIoForm {return {mode,format:'json_standard',columns:Object.fromEntries(OCEL_CSV_FIELDS.map(([key])=>[key,''])) as OcelIoForm['columns'],delimiter:'',switches:Object.fromEntries(OCEL_IO_SWITCHES.map(([key])=>[key,'native'])) as OcelIoForm['switches'],maxBytes:''};}
const group=(form:OcelIoForm)=>form.format.startsWith('csv')?'csv':form.format.startsWith('xml')?'xml':form.format;
export function ocelIoSwitches(form:OcelIoForm) {return OCEL_IO_SWITCHES.filter(([, ,format,mode])=>format===group(form)&&(mode==='both'||mode===form.mode));}
export function ocelIoQuery(form:OcelIoForm):string {
 if(form.mode==='import'&&(form.format==='csv_objects'||form.format==='csv_pair'))throw new Error('Choose an event CSV for import.');
 const q=new URLSearchParams(form.format==='sqlite'?{mode:form.mode}:{format:form.format==='csv_pair'?'csv':form.format,mode:form.mode});
 if(group(form)==='csv'){for(const [key] of OCEL_CSV_FIELDS)if(form.columns[key]!=='')q.set(key,form.columns[key]);if(form.delimiter!==''){const delimiter=form.delimiter==='\\t'?'\t':form.delimiter;if(new TextEncoder().encode(delimiter).length!==1||['"','\r','\n','\0'].includes(delimiter))throw new Error('CSV delimiter must be one non-quote byte; use \\t for a tab.');q.set('ocel_csv_delimiter',delimiter);}if(form.format==='csv_pair')q.set('include_objects','true');}
 for(const [key] of ocelIoSwitches(form))if(form.switches[key]!=='native')q.set(key,form.switches[key]);
 if(group(form)==='xml'&&form.mode==='import'&&form.maxBytes!==''){if(!/^\d+$/.test(form.maxBytes)||BigInt(form.maxBytes)>18446744073709551615n)throw new Error('XML byte limit must be a native unsigned integer.');q.set('max_bytes',form.maxBytes);}
 if(form.mode==='import')q.set('include_open_source','true');return q.toString();
}
interface BoundFile {name:string;size:number;sha256:string;}
async function bind(blob:Blob,name:string):Promise<BoundFile> {const digest=await crypto.subtle.digest('SHA-256',await blob.arrayBuffer());return {name,size:blob.size,sha256:Array.from(new Uint8Array(digest)).map(v=>v.toString(16).padStart(2,'0')).join('')};}
export interface OcelIoArtifact {name:string;blob:Blob;binding:BoundFile;preview?:string;}
export interface OcelIoRun {mode:'import'|'export';format:OcelIoFormat;query:string;sources:Record<string,BoundFile>;report_json:string;raw?:string;result?:OcelExchangeResult|OcelSqliteResult;tables?:Record<string,OcelPreparedTable>;artifacts:OcelIoArtifact[];}
const record=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
async function artifact(name:string,blob:Blob):Promise<OcelIoArtifact> {return {name,blob,binding:await bind(blob,name),...(name.endsWith('.sqlite')?{}:{preview:(await blob.text()).slice(0,6000)})};}
export async function requestOcelIo(form:OcelIoForm,file:File,objects?:File,signal?:AbortSignal):Promise<OcelIoRun> {
 const query=ocelIoQuery(form);const sources:Record<string,BoundFile>={original:await bind(file,file.name)};
 if(form.mode==='import'&&form.format==='csv'&&objects)sources.objects=await bind(objects,objects.name);
 if(signal?.aborted)throw new DOMException('File exchange cancelled','AbortError');
 const upload=new FormData();upload.append('log',file,file.name);if(sources.objects)upload.append('objects',objects!,objects!.name);
 const response=await backendFetch('/api/ocel/'+(form.format==='sqlite'?'sqlite':'io')+'?'+query,{method:'POST',body:upload,...(signal?{signal}:{})});
 if(!response.ok){const raw=await response.text();let value:unknown;try{value=JSON.parse(raw);}catch{}throw new Error(record(value)&&value.error?String(value.error):`OCEL file exchange returned ${response.status}.`);}
 const artifacts:OcelIoArtifact[]=[];let raw:string|undefined;let result:OcelIoRun['result'];let tables:OcelIoRun['tables'];
 if(form.mode==='import'){
  raw=await response.text();const value=parseNativeJson(raw);if(!(form.format==='sqlite'?isOcelSqliteResult(value):isOcelExchangeResult(value)))throw new Error('The backend returned an invalid imported OCEL.');result=value as NonNullable<OcelIoRun['result']>;
  const expected=form.format==='sqlite'?'IMPORT':'IMPORT_'+form.format.toUpperCase();if(result.variant!==expected)throw new Error('The backend returned a different import format.');
  if(!record(result.open_log)||!Array.isArray(result.open_log.events)||!Array.isArray(result.open_log.objects))throw new Error('The backend returned an invalid standard opening source.');tables=preparedOcelTables(result.log);
  artifacts.push(await artifact('imported-'+file.name.replace(/\.[^.]+$/,'')+'.json',new Blob([nativeJson(result.open_log)],{type:'application/json'})));
 }else if(form.format==='csv_pair'){
  raw=await response.text();const pair=parseNativeJson(raw);if(!record(pair)||pair.family!=='OCEL_IO_EXPORT'||pair.variant!=='CSV_PAIR'||typeof pair.events_csv!=='string'||typeof pair.objects_csv!=='string')throw new Error('The backend returned an invalid paired CSV export.');
  artifacts.push(await artifact('ocel-events.csv',new Blob([pair.events_csv],{type:'text/csv'})),await artifact('ocel-objects.csv',new Blob([pair.objects_csv],{type:'text/csv'})));
 }else{
  const blob=await response.blob();const format=form.format;const extension=format==='sqlite'?'sqlite':format.startsWith('json')?'json':format.startsWith('xml')?'xml':'csv';
  if(format==='sqlite'){if(new TextDecoder().decode(await blob.slice(0,16).arrayBuffer())!=='SQLite format 3\0')throw new Error('The backend returned an invalid SQLite file.');}
  else {const text=await blob.text();if(format.startsWith('json')){const value=parseNativeJson(text);if(!record(value)||!(record(value['ocel:events'])&&record(value['ocel:objects'])||Array.isArray(value.events)&&Array.isArray(value.objects)))throw new Error('The backend returned invalid OCEL JSON.');}else if(format.startsWith('xml')&&!text.trimStart().startsWith('<')||format.startsWith('csv')&&!text.trim())throw new Error('The backend returned invalid OCEL text.');}
  artifacts.push(await artifact(format==='csv_objects'?'ocel-objects.csv':'ocel-'+format+'.'+extension,blob));
 }
 const request={mode:form.mode,format:form.format,sources,query,parameters:Object.fromEntries(new URLSearchParams(query))};const outputs=artifacts.map(a=>a.binding);const report_json=`{"family":"OCEL_IO_REPORT","request":${JSON.stringify(request)},"outputs":${JSON.stringify(outputs)}${raw!==undefined?',"response":'+raw:''}}`;
 return {mode:form.mode,format:form.format,query,sources,report_json,raw,result,tables,artifacts};
}
export function importedOcelFile(run:OcelIoRun):File {if(run.mode!=='import'||!run.artifacts[0])throw new Error('No imported OCEL is available.');const a=run.artifacts[0];return new File([a.blob],a.name,{type:'application/json'});}
