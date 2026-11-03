import { type LinkAnalysisResult,isCaseRelationsResult,isLinkAnalysisResult,isTimestampInterleavingsResult } from './types';
import { backendFetch } from "./backend-fetch";
import { parseNativeJson,nativeJson,nativeCell } from './native-json';
import { nativeRecordTables } from './ocel-execution';
import { type OcelPreparedTable } from './ocel-preparation';
import { type LogQuerySchema } from './log-query';
import { type ModelGraph } from './model-graph';
import {nativeStringBytes} from './native-multipart';
import {isOriginalEmbeddingFilterNativeString,isOriginalEmbeddingFilterTable,isOriginalEmbeddingFilterRawLog,originalEmbeddingFilterStringBytes,type OriginalEmbeddingFilterNativeString} from './original-embedding-filter-workbench';
export const RELATED_METHODS=[['links','Link events in the active log'],['interleavings','Timestamp interleavings between logs'],['merge','Merge by case relations'],['format','Format a native event table']] as const;
export type RelatedMethod=typeof RELATED_METHODS[number][0];
export const RELATED_FIELDS={links:[['link_analysis_out_column','Outgoing value column'],['link_analysis_in_column','Incoming value column'],['link_analysis_sorting_column','Event sorting column'],['link_analysis_index_column','Generated index field'],['link_analysis_max_links','Maximum links']],interleavings:[['interleavings_timestamp_key','Interleaving timestamp field'],['interleavings_left_column','Left relation column'],['interleavings_right_column','Right relation column'],['interleavings_max_results','Maximum interleavings']],merge:[['merge_timestamp_key','Merge timestamp field'],['merge_case_id_key','Merge case ID field'],['merge_left_column','Left relation column'],['merge_right_column','Right relation column'],['merge_left_suffix','Left default relation suffix'],['merge_right_suffix','Right default relation suffix']]} as const;
export const LINK_SWITCHES=[['link_analysis_look_forward','Look forward'],['link_analysis_keep_first_occurrence','Keep first occurrence'],['link_analysis_propagate','Propagate links']] as const;
export const FORMAT_FIELDS=[['case_id','Formatting case ID column'],['activity_key','Formatting activity column'],['timestamp_key','Formatting timestamp column'],['timest_format','Timestamp format string']] as const;
export interface RelatedForm {method:RelatedMethod;fields:Record<string,string>;switches:Record<string,'native'|'true'|'false'>;emptySuffixes:boolean;format:Record<typeof FORMAT_FIELDS[number][0],string>;startMode:'native'|'disabled'|'column';startColumn:string;uploadedOptions:boolean;mergeInputFormat:'auto'|'event_table_json';mergeExactValues:string;}
export function defaultRelated():RelatedForm {return {method:'links',fields:{},switches:{},emptySuffixes:false,format:{case_id:'',activity_key:'',timestamp_key:'',timest_format:''},startMode:'native',startColumn:'',uploadedOptions:false,mergeInputFormat:'auto',mergeExactValues:'{}'};}
export function relatedQuery(form:RelatedForm):string {
 const q=new URLSearchParams();if(form.method==='format')return '';const exact=relatedMergeExactValues(form);
 for(const [key,label] of RELATED_FIELDS[form.method]){const value=form.fields[key]??'';if(RELATED_EXACT_MERGE_FIELDS.some(([native,query])=>query===key&&Object.hasOwn(exact,native)))continue;if(value==='')continue;if(key.endsWith('max_links')||key.endsWith('max_results')){if(!/^\d+$/.test(value)||BigInt(value)<1n||BigInt(value)>18446744073709551615n)throw new Error(label+' must be a positive native unsigned integer.');}q.set(key,value);}
 if(form.method==='links')for(const [key] of LINK_SWITCHES){const value=form.switches[key]??'native';if(value!=='native')q.set(key,value);}
 if(form.method==='merge'){q.set('merge_variant','pandas');if(form.mergeInputFormat==='event_table_json'){q.set('merge_input_format','event_table_json');for(const [native,key] of RELATED_EXACT_MERGE_FIELDS)if(Object.hasOwn(exact,native))q.set(key+'_json',nativeJson(exact[native]));}if(form.emptySuffixes&&form.mergeInputFormat!=='event_table_json'){let changed=false;for(const side of ['left','right'])if(form.fields['merge_'+side+'_column']){q.set('merge_'+side+'_suffix','');changed=true;}if(!changed)throw new Error('Empty unused suffixes require an explicit relation column.');}q.set('include_open_log','true');q.set('include_format_table','true');}
 return q.toString();
}
const record=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
export interface NativeRelationTable extends Record<string,unknown> {columns:string[];rows:(Record<string,unknown>|null)[][];}
export function isNativeRelationTable(value:unknown):value is NativeRelationTable {return record(value)&&Array.isArray(value.columns)&&value.columns.every(c=>typeof c==='string')&&Array.isArray(value.rows)&&value.rows.every(row=>Array.isArray(row)&&row.length===(value.columns as unknown[]).length&&row.every(c=>c===null||record(c)));}
const table=(columns:string[],rows:unknown[][]):OcelPreparedTable=>({columns,count:rows.length,rowAt:i=>rows[i]});
export function nativeTableViews(input:NativeRelationTable|RelatedRawTable,prefix=''):Record<string,OcelPreparedTable> {
 if(isRelatedRawTable(input))return rawTableViews(input,prefix);
 const cells:unknown[][]=[];const rows=input.rows.map((row,i)=>{row.forEach((cell,k)=>cells.push([i,k,input.columns[k],cell===null?'absent':cell.null===true?'null':'value',cell?.kind??null,cell?.container??null,cell?.value??null,nativeJson(cell)]));return [i,...row.map(cell=>cell===null?'(absent)':cell.null===true?'(null)':cell.value??nativeJson(cell))];});
 const schema=input.columns.map((name,i)=>[i,name,Array.isArray(input.dtypes)?nativeJson(input.dtypes[i]):'null']);const metadata=Object.keys(input).filter(key=>!['columns','rows','dtypes'].includes(key)).map(key=>[key,nativeJson(input[key])]);
 return {[prefix+'rows']:table(['row_position',...input.columns],rows),[prefix+'cells']:table(['row_position','column_position','column_name','state','kind','container','native_lexical_value','native_cell_json'],cells),[prefix+'schema']:table(['column_position','column_name','native_dtype_json'],schema),[prefix+'metadata']:table(['native_field','native_value_json'],metadata)};
}
interface BoundFile {name:string;size:number;sha256:string;}
async function bind(file:Blob,name:string,signal?:AbortSignal):Promise<BoundFile> {signal?.throwIfAborted();const bytes=await file.arrayBuffer();signal?.throwIfAborted();const hash=await crypto.subtle.digest('SHA-256',bytes);signal?.throwIfAborted();return {name,size:file.size,sha256:Array.from(new Uint8Array(hash)).map(v=>v.toString(16).padStart(2,'0')).join('')};}
export interface RelatedSources {left?:File;right?:File;relations?:File;format?:File;original?:File|null;schema?:LogQuerySchema|null;dataset?:string;}
export interface RelatedRun {method:RelatedMethod;query:string;sources:Record<string,BoundFile>;schema?:LogQuerySchema;raw:string;report_json:string;result:Record<string,unknown>;tables:Record<string,OcelPreparedTable>;ok:boolean;error?:string;}
export async function requestRelated(form:RelatedForm,files:RelatedSources,signal?:AbortSignal):Promise<RelatedRun> {
 signal?.throwIfAborted();const sources:Record<string,BoundFile>={};let query=relatedQuery(form),route='',body:BodyInit|undefined,headers:Record<string,string>|undefined,payloadBinding:BoundFile|undefined;
 if(form.method==='links'){if(!files.schema?.source_id)throw new Error('Refresh the active mined source first.');route='link-analysis';const q=new URLSearchParams(query);q.set('source_id',files.schema.source_id);q.set('include_source','true');query=q.toString();if(files.original)sources.original=await bind(files.original,files.original.name,signal);}
 else if(form.method==='format'){
  if(!files.format)throw new Error('Choose a native table or formatting request JSON.');sources.format=await bind(files.format,files.format.name,signal);const value=parseNativeJson(await files.format.text());signal?.throwIfAborted();if(!record(value))throw new Error('The formatting source must be a JSON object.');
  if(form.uploadedOptions){if(!isNativeRelationTable(value.table))throw new Error('Uploaded options require a complete native request with a table.');body=files.format;}
  else {const input=isNativeRelationTable(value)?value:isNativeRelationTable(value.format_table)?value.format_table:isNativeRelationTable(value.table)?value.table:null;if(!input)throw new Error('The file does not contain a native event table.');const options:Record<string,unknown>={};for(const [key] of FORMAT_FIELDS)if(form.format[key]!=='')options[key]=form.format[key];if(form.startMode==='disabled')options.start_timestamp_key=null;else if(form.startMode==='column'){if(!form.startColumn)throw new Error('Enter the start timestamp column.');options.start_timestamp_key=form.startColumn;}body=nativeJson({table:input,options});}
  route='format-dataframe';headers={'Content-Type':'application/json'};payloadBinding=await bind(typeof body==='string'?new Blob([body]):files.format,'native-format-request.json',signal);
 }else {
  if(!files.left||!files.right||!files.relations)throw new Error('Choose the independent base, incoming and relation files.');for(const [key,file] of [['left',files.left],['right',files.right],['relations',files.relations]] as const)sources[key]=await bind(file,file.name,signal);
  const upload=new FormData();const fields=form.method==='merge'?['base_log','incoming_log','case_relations']:['left','right','relations'];[files.left,files.right,files.relations].forEach((file,i)=>upload.append(fields[i],file,file.name));body=upload;route=form.method==='merge'?'merge-case-relations':'timestamp-interleavings';
 }
 if(signal?.aborted)throw new DOMException('Relation workflow cancelled','AbortError');const response=await backendFetch('/api/'+route+(query?'?'+query:''),{method:'POST',...(body!==undefined?{body}:{}),...(headers?{headers}:{}),...(signal?{signal}:{})});const raw=await response.text();signal?.throwIfAborted();let value:unknown;try{value=parseNativeJson(raw);}catch{throw new Error('The backend returned invalid relation-workflow JSON.');}
 const partial=form.method==='format'&&record(value)&&value.family==='FORMAT_DATAFRAME'&&typeof value.error==='string'&&isNativeRelationTable(value.input_after);
 if(!response.ok&&!partial)throw new Error(record(value)&&isOriginalEmbeddingFilterNativeString(value.error)?relatedNativeText(value.error):`Relation workflow returned ${response.status}.`);
 const tables:Record<string,OcelPreparedTable>={};
 if(form.method==='links'){if(!isLinkAnalysisResult(value)||value.source_id!==files.schema!.source_id)throw new Error('The active link-analysis source changed or returned invalid data.');Object.assign(tables,nativeRecordTables(value as unknown as Record<string,unknown>));}
 else if(form.method==='interleavings'){if(!isTimestampInterleavingsResult(value))throw new Error('The backend returned invalid timestamp interleavings.');Object.assign(tables,nativeRecordTables(value as unknown as Record<string,unknown>));}
 else if(form.method==='merge'){
  if(form.mergeInputFormat==='event_table_json'){if(!isRelatedTypedMerge(value))throw Error('The backend returned incomplete typed merge storage.');Object.assign(tables,nativeTableViews(value.table,'merged/'));for(const state of ['before','after'] as const)for(const role of ['base','incoming','relations'] as const)Object.assign(tables,nativeTableViews((state==='before'?value.input_before:value.input_after)[role],role+'_'+state+'/'));if(value.format_table)Object.assign(tables,nativeTableViews(value.format_table,'formatting/'));Object.assign(tables,nativeRecordTables({options:value.options,projected_log:value.projected_log,log_projection_error:value.log_projection_error,format_table_error:value.format_table_error,open_log_error:value.open_log_error},'native/'));}
  else {if(!isCaseRelationsResult(value)||!isNativeRelationTable(value.table)||!isNativeRelationTable(value.format_table))throw new Error('The backend returned invalid merged tables.');
  if(value.open_log!==null&&(!value.open_log||typeof value.open_log.xes!=='string'||typeof value.open_log.activity_attribute!=='string'||typeof value.open_log.complete_activity_field!=='boolean'||![value.open_log.case_count,value.open_log.event_count].every(n=>Number.isSafeInteger(n)&&n>=0)))throw new Error('The backend returned an invalid merged opening source.');
  Object.assign(tables,nativeTableViews(value.table,'merged/'),nativeTableViews(value.format_table,'formatting/'));if(value.log)Object.assign(tables,nativeRecordTables(value.log,'projection/'));}
 }else {if(!record(value)||value.family!=='FORMAT_DATAFRAME'||!isNativeRelationTable(value.input_after)||!partial&&!isNativeRelationTable(value.table))throw new Error('The backend returned invalid formatted event data.');if(isNativeRelationTable(value.table))Object.assign(tables,nativeTableViews(value.table,'formatted/'));Object.assign(tables,nativeTableViews(value.input_after,'input_after/'));}
 const result=value as unknown as Record<string,unknown>;const ok=!partial;const request={method:form.method,...(form.method==='merge'?{input_format:form.mergeInputFormat,...(form.mergeInputFormat==='event_table_json'?{exact_values:relatedMergeExactValues(form)}:{})}:{}),query,parameters:Object.fromEntries(new URLSearchParams(query)),sources,...(form.method==='links'?{active_source:files.schema,dataset:files.dataset}:{}),...(payloadBinding?{payload:payloadBinding,uploaded_options:form.uploadedOptions}: {})};
 return {method:form.method,query,sources,...(form.method==='links'?{schema:files.schema!}:{}),raw,report_json:`{"family":"RELATED_LOGS_REPORT","request":${JSON.stringify(request)},"response":${raw}}`,result,tables,ok,...(!ok?{error:String(result.error)}:{})};
}
export function relatedGraph(run:RelatedRun):ModelGraph|null {
 if(run.method!=='links')return null;const result=run.result as unknown as LinkAnalysisResult;const nodes=new Map<string,ModelGraph['nodes'][number]>();const edges=result.links.map((link,i)=>{for(const e of [link.source,link.target]){const id='event/'+String(nativeCell(e,'index'));if(!nodes.has(id))nodes.set(id,{id,label:e.activity,detail:`${e.case_id} · trace ${e.trace_index} / event ${e.event_index} · ${e.timestamp}`});}return {id:'link/'+i,source:'event/'+String(nativeCell(link.source,'index')),target:'event/'+String(nativeCell(link.target,'index')),label:'link '+i,value:1,directed:true};});return {nodes:[...nodes.values()],edges};
}
export function mergedOpening(run:RelatedRun):RelatedOpening|null|undefined {return run.method==='merge'?run.result.open_log as RelatedOpening|null|undefined:null;}
export function mergedXesFile(run:RelatedRun):File {const open=mergedOpening(run);if(!open)throw new Error('The merged table has no native event-log projection.');return new File([originalEmbeddingFilterStringBytes(open.xes)],'merged-case-relations.xes',{type:'application/xml'});}
export function formattingTableFile(run:RelatedRun):File {const value=run.method==='merge'?run.result.format_table:run.result.table;if(!isNativeRelationTable(value))throw new Error('No typed formatting table is available.');return new File([nativeJson(value)],'native-event-table.json',{type:'application/json'});}


// Typed merge transport keeps canonical native storage separate from display values.
export const RELATED_EXACT_MERGE_FIELDS=[['timestamp_key','merge_timestamp_key'],['case_id_key','merge_case_id_key'],['left_suffix','merge_left_suffix'],['right_suffix','merge_right_suffix'],['relation_left_column','merge_left_column'],['relation_right_column','merge_right_column']] as const;
export type RelatedNativeString=OriginalEmbeddingFilterNativeString;
export interface RelatedRawTable extends Record<string,unknown>{native_byte_strings:'hex-v1';native_schema:'event-table-raw-v1';value:{columns:string[];rows:(Record<string,unknown>|null)[][];attributes:unknown[];metadata:Record<string,unknown>;dtypes:string[];index:unknown;categorical_dtypes:unknown[]};}
export interface RelatedTypedMerge extends Record<string,unknown>{family:'MERGE_CASE_RELATIONS';variant:'pandas';input_format:'event_table_json';options:Record<string,unknown>;input_before:Record<'base'|'incoming'|'relations',RelatedRawTable>;input_after:Record<'base'|'incoming'|'relations',RelatedRawTable>;table:RelatedRawTable;projected_log:unknown;log_projection_error?:RelatedNativeString;format_table?:NativeRelationTable|null;format_table_error?:RelatedNativeString;open_log?:RelatedOpening|null;open_log_error?:RelatedNativeString;}
export interface RelatedOpening {xes:RelatedNativeString;activity_attribute:RelatedNativeString;case_count:number;event_count:number;complete_activity_field:boolean;}
const exactKeys=(v:unknown,names:readonly string[]):v is Record<string,unknown>=>record(v)&&Object.keys(v).length===names.length&&names.every(k=>Object.hasOwn(v,k));
const nativeSize=(v:object,key:string):boolean=>{const token=String(nativeCell(v,key));return typeof (v as Record<string,unknown>)[key]==='number'&&/^(?:0|[1-9]\d*)$/.test(token)&&BigInt(token)<=18446744073709551615n;};
export function relatedMergeExactValues(form:RelatedForm):Record<string,RelatedNativeString>{
 if(form.method!=='merge'||form.mergeInputFormat!=='event_table_json')return {};
 let value:unknown;try{value=parseNativeJson(form.mergeExactValues);}catch{throw Error('Exact merge values must be a JSON object.');}
 if(!record(value))throw Error('Exact merge values must be a JSON object.');
 for(const key of Object.keys(value))if(!RELATED_EXACT_MERGE_FIELDS.some(([name])=>name===key)||!isOriginalEmbeddingFilterNativeString(value[key]))throw Error('Exact merge values accept only the six native selector/suffix names with JSON strings or native byte strings.');
 return value as Record<string,RelatedNativeString>;
}
export function isRelatedRawTable(value:unknown):value is RelatedRawTable{
 if(!exactKeys(value,['native_byte_strings','native_schema','value'])||value.native_byte_strings!=='hex-v1'||value.native_schema!=='event-table-raw-v1'||!exactKeys(value.value,['columns','rows','attributes','metadata','dtypes','index','categorical_dtypes']))return false;
 const t=value.value;if(!exactKeys(t.metadata,['extensions','classifiers','trace_globals','event_globals','properties','globals','compatibility_snapshot_valid','compatibility_globals_snapshot','compatibility_trace_globals_snapshot','compatibility_event_globals_snapshot']))return false;
 return isOriginalEmbeddingFilterTable(value)&&Array.isArray(t.rows)&&Array.isArray(t.columns)&&t.rows.every(row=>Array.isArray(row)&&row.length===(t.columns as unknown[]).length);
}
export function isRelatedTypedMerge(value:unknown):value is RelatedTypedMerge{
 if(!record(value)||value.family!=='MERGE_CASE_RELATIONS'||value.variant!=='pandas'||value.input_format!=='event_table_json'||!exactKeys(value.options,['variant','timestamp_key','case_id_key','left_suffix','right_suffix','relation_left_column','relation_right_column'])||value.options.variant!=='pandas'||!RELATED_EXACT_MERGE_FIELDS.every(([key])=>isOriginalEmbeddingFilterNativeString((value.options as Record<string,unknown>)[key]))||!isRelatedRawTable(value.table))return false;
 for(const key of ['input_before','input_after'])if(!exactKeys(value[key],['base','incoming','relations'])||!['base','incoming','relations'].every(role=>isRelatedRawTable((value[key] as Record<string,unknown>)[role])))return false;
 if(value.projected_log!==null&&!isOriginalEmbeddingFilterRawLog(value.projected_log))return false;
 for(const key of ['log_projection_error','format_table_error','open_log_error'])if(value[key]!==undefined&&!isOriginalEmbeddingFilterNativeString(value[key]))return false;
 if(value.projected_log===null&&value.log_projection_error===undefined)return false;
 if(value.format_table!==undefined&&value.format_table!==null&&!isNativeRelationTable(value.format_table))return false;
 if(value.format_table===null&&value.format_table_error===undefined)return false;
 if(value.open_log!==undefined&&value.open_log!==null){const open=value.open_log;if(!exactKeys(open,['xes','case_count','event_count','activity_attribute','complete_activity_field'])||!isOriginalEmbeddingFilterNativeString(open.xes)||!isOriginalEmbeddingFilterNativeString(open.activity_attribute)||!nativeSize(open,'case_count')||!nativeSize(open,'event_count')||typeof open.complete_activity_field!=='boolean')return false;}
 return !(value.open_log===null&&value.open_log_error===undefined);
}
export function relatedNativeText(value:RelatedNativeString):string{const bytes=originalEmbeddingFilterStringBytes(value);try{return new TextDecoder('utf-8',{fatal:true,ignoreBOM:true}).decode(bytes);}catch{return 'hex:'+Array.from(bytes,x=>x.toString(16).padStart(2,'0')).join('');}}
function rawText(value:string):string{return relatedNativeText({native_bytes:'hex-v1',data:value});}
export function relatedTableColumns(value:NativeRelationTable|RelatedRawTable):string[]{return isRelatedRawTable(value)?value.value.columns.map(rawText):value.columns;}
export function relatedOpeningActivity(run:RelatedRun):string|null{const open=mergedOpening(run);if(!open)return null;try{return new TextDecoder('utf-8',{fatal:true,ignoreBOM:true}).decode(originalEmbeddingFilterStringBytes(open.activity_attribute));}catch{return null;}}
function rawTableViews(input:RelatedRawTable,prefix:string):Record<string,OcelPreparedTable>{
 const value=input.value,columns=value.columns.map(rawText),cells:unknown[][]=[];
 const rows=value.rows.map((row,i)=>{row.forEach((cell,k)=>cells.push([i,k,columns[k],cell===null?'absent':cell.null===true?'null':'value',cell?.kind_index??null,cell?.container_index??null,cell&&typeof cell.value==='string'?rawText(cell.value):null,cell?nativeJson(cell):'null']));return [i,...row.map(cell=>cell===null?'(absent)':cell.null===true?'(null)':typeof cell.value==='string'?rawText(cell.value):nativeJson(cell))];});
 const schema=columns.map((name,i)=>[i,name,value.dtypes[i]===undefined?null:rawText(value.dtypes[i])]);
 // Full raw JSON remains the source for nested attributes, categories, index and all metadata.
 const metadata=Object.keys(value).filter(k=>!['columns','rows','dtypes'].includes(k)).map(k=>[k,nativeJson(value[k as keyof typeof value])]);
 return {[prefix+'rows']:table(['row_position',...columns],rows),[prefix+'cells']:table(['row_position','column_position','column_name','state','kind_index','container_index','native_lexical_value','complete_raw_cell_json'],cells),[prefix+'schema']:table(['column_position','column_name','native_dtype'],schema),[prefix+'metadata']:table(['native_field','complete_raw_json'],metadata)};
}
export type RelatedTableSlot='merged'|'base_before'|'incoming_before'|'relations_before'|'base_after'|'incoming_after'|'relations_after';
export const RELATED_TYPED_TABLE_SLOTS:readonly RelatedTableSlot[]=['merged','base_before','incoming_before','relations_before','base_after','incoming_after','relations_after'];
export function relatedTypedTableFile(run:RelatedRun,slot:RelatedTableSlot='merged'):File{
 if(!isRelatedTypedMerge(run.result))throw Error('This run has no complete native table merge.');let value:RelatedRawTable;
 if(slot==='merged')value=run.result.table;else{const [role,state]=slot.split('_') as ['base'|'incoming'|'relations','before'|'after'];value=(state==='before'?run.result.input_before:run.result.input_after)[role];}
 if(!isRelatedRawTable(value))throw Error('Choose a retained native table snapshot.');return new File([nativeStringBytes(nativeJson(value))],'native-relations-'+slot+'.json',{type:'application/json'});
}

export function relatedFormatColumns(run:RelatedRun):string[]{const value=run.method==='merge'?run.result.format_table:run.result.table;return isNativeRelationTable(value)?value.columns:[];}
export function relatedProjectionError(run:RelatedRun):string{const value=run.result.open_log_error??run.result.log_projection_error??'not supplied';return isOriginalEmbeddingFilterNativeString(value)?relatedNativeText(value):String(value);}

export function relatedNativeErrorText(value:unknown):string{return isOriginalEmbeddingFilterNativeString(value)?relatedNativeText(value):String(value);}

export function relatedFormattingSelectors(run:RelatedRun):{case_id:string;timestamp_key:string}|null{
 if(isRelatedTypedMerge(run.result)){
  try{const decode=(value:unknown)=>new TextDecoder('utf-8',{fatal:true,ignoreBOM:true}).decode(originalEmbeddingFilterStringBytes(value as RelatedNativeString));return {case_id:decode(run.result.options.case_id_key),timestamp_key:decode(run.result.options.timestamp_key)};}catch{return null;}
 }
 const params=new URLSearchParams(run.query),columns=relatedFormatColumns(run);return {case_id:params.get('merge_case_id_key')??'case_id',timestamp_key:params.get('merge_timestamp_key')??(columns.includes('timestamp')?'timestamp':'time:timestamp')};
}

export function relatedMergedFormatForm(run:RelatedRun,form:RelatedForm=defaultRelated()):RelatedForm|null{
 const selectors=relatedFormattingSelectors(run);if(!selectors)return null;const columns=relatedFormatColumns(run),activity=relatedOpeningActivity(run);
 return {...form,method:'format',uploadedOptions:false,format:{...form.format,...selectors,activity_key:activity&&columns.includes(activity)?activity:columns.includes('concept:name')?'concept:name':'activity'},startMode:'disabled'};
}
