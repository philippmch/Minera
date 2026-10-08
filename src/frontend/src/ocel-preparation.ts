import { ocelEnrichmentQuery,ocelFlattenQuery,type OcelFilterSelector,type OcelEnrichmentOptions } from "./api";
import { isOcelFilterResult,isOcelSplitResult,isOcelEnrichmentResult,type OcelFilterResult,type OcelSplitResult,type OcelEnrichmentResult } from "./types";
import { parseNativeJson,nativeJson,nativeCell } from "./native-json";
import { cellText } from "./transformation-workspace";
import { isOcelJsonDocument } from "./app-logic";

export const OCEL_PREPARATION_METHODS=[['filter','Filter OCEL'],['split','Split OCEL'],['enrich','Enrich relationships'],['flatten','Flatten to a classic log']] as const;
export type OcelPreparationMethod=typeof OCEL_PREPARATION_METHODS[number][0];
export const OCEL_FILTERS=[['event_attribute','Event attribute'],['object_attribute','Object attribute'],['object_types','Object types'],['objects','Object IDs'],['events','Event IDs'],['timestamp','Event timestamp window'],['activity_type_matching','Activity/type matching'],['object_count','Object counts per event'],['start_events','Lifecycle starts by type'],['end_events','Lifecycle ends by type'],['activities_object_type','Activities connected to a type'],['cc_object','Component containing an object'],['cc_length','Component size'],['cc_object_type','Components with an object type'],['cc_activity','Components with an activity']] as const;
export const PREPARATION_GRAPHS=[['object_interaction_graph','Interaction'],['object_descendants_graph','Descendants'],['object_inheritance_graph','Inheritance'],['object_cobirth_graph','Co-birth'],['object_codeath_graph','Co-death']] as const;
export interface OcelFilterForm {variant:OcelFilterSelector;positive:boolean;key:string;values:string;names:string;level:string;from:string;to:string;type:string;id:string;activity:string;minimum:string;maximum:string;counts:string;allowed:string;}
export const DEFAULT_OCEL_FILTER:OcelFilterForm={variant:'object_types',positive:true,key:'',values:'[]',names:'[]',level:'1',from:'',to:'',type:'',id:'',activity:'',minimum:'0',maximum:'100',counts:'{}',allowed:'{}'};
export interface OcelSplitForm {variant:'connected_components'|'ancestors_descendants';type:string;maximum:string;degree:string;}
export const DEFAULT_OCEL_SPLIT:OcelSplitForm={variant:'connected_components',type:'',maximum:'',degree:''};
export interface OcelEnrichmentForm {mode:'native'|'selected';graphs:Record<typeof PREPARATION_GRAPHS[number][0],boolean>;lifecycle:'native'|'true'|'false';}
export function defaultOcelEnrichment():OcelEnrichmentForm {return {mode:'native',graphs:Object.fromEntries(PREPARATION_GRAPHS.map(([id])=>[id,true])) as OcelEnrichmentForm['graphs'],lifecycle:'native'};}
export interface OcelFlattenForm {type:string;activity:string;timestamp:string;}
export const DEFAULT_OCEL_FLATTEN:OcelFlattenForm={type:'',activity:'',timestamp:''};
export const polarityFilters=new Set<OcelFilterSelector>(['event_attribute','object_attribute','object_types','objects','events','cc_object_type']);
export const typeFilters=new Set<OcelFilterSelector>(['start_events','end_events','activities_object_type','cc_object_type']);
export const attributeFilters=new Set<OcelFilterSelector>(['event_attribute','object_attribute']);
export const listFilters=new Set<OcelFilterSelector>(['object_types','objects','events']);
const record=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
const escape=(value:string)=>value.replaceAll('\\','\\\\').replaceAll(',','\\,').replaceAll(':','\\:').replaceAll(';','\\;').replaceAll('|','\\|');
function parse(text:string,label:string):unknown {try{return parseNativeJson(text);}catch{throw new Error(`${label} must be valid JSON.`);}}
function names(value:unknown,label:string):string[] {if(!Array.isArray(value)||value.some(v=>typeof v!=='string'||!v))throw new Error(`${label} must be a JSON list of nonempty strings.`);return value;}
function required(value:string,label:string):string {if(!value.length)throw new Error(`${label} must not be empty.`);return value;}
function size(value:unknown,label:string,positive=false):string {const text=String(value);if(!/^\d+$/.test(text)||BigInt(text)>18446744073709551615n||positive&&BigInt(text)===0n)throw new Error(`${label} must be ${positive?'a positive':'an unsigned'} 64-bit integer.`);return text;}
export function typedOcelFilterValues(text:string):string {
 const values=parse(text,'Attribute values');if(!Array.isArray(values))throw new Error('Attribute values must be a JSON list of typed scalar entries.');
 return values.map(entry=>{if(!record(entry)||typeof entry.type!=='string')throw new Error('Each attribute value needs a type.');const value=entry.value;
  if(entry.type==='null'&&(value===undefined||value===null))return 'n:';
  if(entry.type==='string'&&typeof value==='string')return 's:'+escape(value);
  if(entry.type==='boolean'&&typeof value==='boolean')return 'b:'+value;
  if(entry.type==='integer'){const literal=nativeCell(entry,'value');const str=String(literal);if(/^-?\d+$/.test(str)&&BigInt(str)>=-9223372036854775808n&&BigInt(str)<=9223372036854775807n)return 'i:'+str;}
  if(entry.type==='decimal'){const literal=nativeCell(entry,'value');const str=String(literal);if(/^-?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$/.test(str)&&Number.isFinite(Number(str)))return 'd:'+str;}
  throw new Error('Use string, integer, decimal, boolean or null values. Integers must fit int64; decimals must be finite.');
 }).join(',');
}
export function ocelPreparationFilterQuery(form:OcelFilterForm):string {
 const q=new URLSearchParams({variant:form.variant});const variant=form.variant;if(polarityFilters.has(variant))q.set('positive',String(form.positive));
 if(attributeFilters.has(variant)){q.set('attribute_key',form.key);const values=typedOcelFilterValues(form.values);if(values)q.set('attribute_values',values);}
 if(listFilters.has(variant)){const list=names(parse(form.names,'Names'),'Names');if(list.length)q.set(variant==='object_types'?'types':variant==='objects'?'ids':'event_ids',list.map(escape).join(','));}
 if(variant==='object_types'||variant==='objects'){const level=size(form.level,'Neighborhood level');if(BigInt(level)>32n)throw new Error('Neighborhood level must be from 0 through 32.');q.set('level',level);}
 if(variant==='timestamp'){if(form.from)q.set('from',form.from);if(form.to)q.set('to',form.to);}
 if(typeFilters.has(variant))q.set('object_type',required(form.type,'Object type'));
 if(variant==='cc_object')q.set('object_id',required(form.id,'Object ID'));
 if(variant==='cc_activity')q.set('activity',required(form.activity,'Activity'));
 if(variant==='cc_length'){q.set('min_size',size(form.minimum,'Minimum component size'));q.set('max_size',size(form.maximum,'Maximum component size'));}
 if(variant==='object_count'){const counts=parse(form.counts,'Minimum counts');if(!record(counts))throw new Error('Minimum counts must be a JSON object.');const entries=Object.keys(counts).sort().map(key=>{required(key,'Count object type');return escape(key)+':'+size(nativeCell(counts,key),'Minimum count');});if(entries.length)q.set('minimum_counts',entries.join(','));}
 if(variant==='activity_type_matching'){const allowed=parse(form.allowed,'Allowed activities');if(!record(allowed))throw new Error('Allowed activities must be a JSON object.');const entries=Object.keys(allowed).sort().map(key=>{required(key,'Allowed object type');const list=names(allowed[key],'Allowed activities');if(!list.length)throw new Error('Allowed activity lists must not be empty.');return escape(key)+':'+list.map(escape).join('|');});if(entries.length)q.set('allowed',entries.join(';'));}
 return q.toString();
}
export function ocelPreparationSplitQuery(form:OcelSplitForm):string {const q=new URLSearchParams({variant:form.variant});if(form.variant==='ancestors_descendants'){q.set('object_type',required(form.type,'Central object type'));if(form.maximum)q.set('max_objects',size(form.maximum,'Maximum central objects',true));}else if(form.degree){const degree=Number(form.degree);if(!form.degree.trim()||!Number.isFinite(degree)||degree<0||degree>1)throw new Error('Maximum degree centrality must be from 0 through 1.');q.set('max_degree_centrality',form.degree);}return q.toString();}
export function ocelPreparationEnrichmentQuery(form:OcelEnrichmentForm):string {const options:OcelEnrichmentOptions={};if(form.mode==='selected')options.graphs=PREPARATION_GRAPHS.filter(([id])=>form.graphs[id]).map(([id])=>id);if(form.lifecycle!=='native')options.lifecycle_qualifiers=form.lifecycle==='true';return ocelEnrichmentQuery(options);}
export function ocelPreparationFlattenQuery(form:OcelFlattenForm):string {return ocelFlattenQuery({object_type:form.type,...(form.activity?{event_activity_attribute:form.activity}:{}),...(form.timestamp?{event_timestamp_attribute:form.timestamp}:{})});}

export interface OcelPreparedTable {columns:string[];count:number;rowAt:(index:number)=>unknown[];}
const table=(columns:string[],rows:unknown[][]):OcelPreparedTable=>({columns,count:rows.length,rowAt:index=>rows[index]});
const obj=(value:unknown):Record<string,unknown>=>{if(!record(value))throw new Error('Native OCEL table record is invalid.');return value;};
const array=(value:unknown):unknown[]=>{if(!Array.isArray(value))throw new Error('Native OCEL relation array is invalid.');return value;};
export function preparedOcelTables(log:Record<string,unknown>):Record<string,OcelPreparedTable> {
 const events=Object.entries(obj(log['ocel:events'])),objects=Object.entries(obj(log['ocel:objects']));const links:unknown[][]=[],o2o:unknown[][]=[];
 const eventRows=events.map(([id,value],i)=>{const e=obj(value),ids=array(e['ocel:omap']),typed=array(e['ocel:typedOmap']);if(ids.length!==typed.length)throw new Error('Native OCEL relation positions are inconsistent.');typed.forEach((entry,k)=>{const relation=obj(entry);if(relation['ocel:oid']!==ids[k])throw new Error('Native OCEL relation identities are inconsistent.');links.push([links.length,i,id,k,ids[k],relation['ocel:qualifier']??null,nativeJson(relation)]);});return [i,id,e['ocel:activity'],e['ocel:timestamp'],nativeJson(ids),nativeJson(e['ocel:vmap']),nativeJson(e)];});
 const objectRows=objects.map(([id,value],i)=>{const o=obj(value);array(o['ocel:o2o']).forEach((entry,k)=>{const relation=obj(entry);o2o.push([o2o.length,i,id,k,relation['ocel:oid'],relation['ocel:qualifier']??null,nativeJson(relation)]);});return [i,id,o['ocel:type'],nativeJson(o['ocel:ovmap']),nativeJson(o)];});
 const changes=array(log['ocel:objectChanges']).map((value,i)=>{const change=obj(value),field=change['ocel:field'];if(typeof field!=='string')throw new Error('Native OCEL change field is invalid.');return [i,change['ocel:oid'],change['ocel:type']??null,change['ocel:timestamp'],field,nativeCell(change,field)??null,nativeJson(change)];});
 const metadata=Object.entries(log).filter(([key])=>!['ocel:events','ocel:objects','ocel:objectChanges','ocel:e2e'].includes(key)).map(([key,value])=>[key,nativeJson(value)]);
 return {events:table(['event_position','event_id','activity','timestamp','object_ids_json','attributes_json','native_event_json'],eventRows),objects:table(['object_position','object_id','object_type','attributes_json','native_object_json'],objectRows),relations:table(['relation_position','event_position','event_id','event_relation_position','object_id','qualifier','native_relation_json'],links),object_links:table(['link_position','object_position','source_object','object_link_position','target_object','qualifier','native_link_json'],o2o),changes:table(['change_position','object_id','object_type','timestamp','field','value','native_change_json'],changes),event_links:table(['link_position','native_link_json'],(log['ocel:e2e']===undefined?[]:array(log['ocel:e2e'])).map((value,i)=>[i,nativeJson(value)])),metadata:table(['native_key','native_value_json'],metadata)};
}
const xmlAttrs=(element:Element)=>Object.fromEntries(Array.from(element.attributes).map(a=>[a.name,a.value]));
interface XesField {kind:string;key:string|null;value:string|null;attributes:Record<string,string>;children?:XesField[];}
function xesFields(element:Element):XesField[] {return Array.from(element.children).filter(child=>!['event','trace'].includes(child.localName)).map(child=>({kind:child.localName,key:child.getAttribute('key'),value:child.getAttribute('value'),attributes:xmlAttrs(child),...(child.children.length?{children:xesFields(child)}:{})}));}
export function flattenedXesTables(raw:string):Record<string,OcelPreparedTable> {
 const document=new DOMParser().parseFromString(raw,'application/xml');if(document.getElementsByTagName('parsererror').length||document.documentElement.localName!=='log')throw new Error('The backend returned invalid flattened XES.');
 const traces=Array.from(document.documentElement.children).filter(child=>child.localName==='trace');const rows:unknown[][]=[];const cases=traces.map((trace,i)=>{const attrs=xesFields(trace),id=attrs.find(a=>a.key==='concept:name')?.value??'';const events=Array.from(trace.children).filter(child=>child.localName==='event');events.forEach((event,k)=>{const fields=xesFields(event);rows.push([i,id,k,fields.find(a=>a.key==='concept:name')?.value??null,fields.find(a=>a.key==='time:timestamp')?.value??null,JSON.stringify(fields)]);});return [i,id,events.length,JSON.stringify(attrs)];});
 return {events:table(['trace_position','case_id','event_position','activity','timestamp','typed_attributes_json'],rows),cases:table(['trace_position','case_id','event_count','typed_attributes_json'],cases),metadata:table(['root_attributes_json','native_log_fields_json'],[[JSON.stringify(xmlAttrs(document.documentElement)),JSON.stringify(xesFields(document.documentElement))]])};
}
export function ocelPreparedCsv(selected:OcelPreparedTable):Blob {const quote=(value:unknown)=>'"'+cellText(value).replaceAll('"','""')+'"';const lines=[selected.columns.map(quote).join(',')];for(let i=0;i<selected.count;i++)lines.push(selected.rowAt(i).map(quote).join(','));return new Blob([lines.join('\r\n')+'\r\n'],{type:'text/csv;charset=utf-8'});}
export interface OcelPreparedOutput {position:number;central_object?:string;log:Record<string,unknown>;open_log:Record<string,unknown>;tables:Record<string,OcelPreparedTable>;}
export interface OcelPreparationRun {method:OcelPreparationMethod;variant:string;source:{name:string;size:number;sha256:string};raw:string;report_json:string;result?:OcelFilterResult|OcelSplitResult|OcelEnrichmentResult;outputs:OcelPreparedOutput[];flattened?:Record<string,OcelPreparedTable>;}
export async function requestOcelPreparation(method:OcelPreparationMethod,file:File,query:string,signal?:AbortSignal):Promise<OcelPreparationRun> {
 if(method!=='flatten'){const parameters=new URLSearchParams(query);parameters.set('include_open_source','true');query=parameters.toString();}
 const hash=await crypto.subtle.digest('SHA-256',await file.arrayBuffer());if(signal?.aborted)throw new DOMException('Preparation cancelled','AbortError');const source={name:file.name,size:file.size,sha256:Array.from(new Uint8Array(hash)).map(v=>v.toString(16).padStart(2,'0')).join('')};const form=new FormData();form.append('log',file,file.name);const response=await fetch('/api/ocel/'+method+(query?'?'+query:''),{method:'POST',body:form,...(signal?{signal}:{})});const raw=await response.text();
 if(!response.ok){try{const error=JSON.parse(raw) as {error?:unknown};if(error.error)throw new Error(String(error.error));}catch(error){if(error instanceof Error&&!(error instanceof SyntaxError))throw error;}throw new Error(`OCEL preparation returned ${response.status}.`);}
 let result:OcelPreparationRun['result'];const outputs:OcelPreparedOutput[]=[];let flattened:OcelPreparationRun['flattened'];let variant='XES';
 if(method==='flatten')flattened=flattenedXesTables(raw);else {let value:unknown;try{value=parseNativeJson(raw);}catch{throw new Error('The backend returned invalid OCEL preparation JSON.');}const valid=method==='filter'?isOcelFilterResult(value):method==='split'?isOcelSplitResult(value):isOcelEnrichmentResult(value);if(!valid)throw new Error('The backend returned an invalid OCEL preparation result.');result=value as NonNullable<OcelPreparationRun['result']>;variant=result.variant;const requested=new URLSearchParams(query).get('variant');if(requested&&requested.toLowerCase()!==variant.toLowerCase())throw new Error('The backend returned a different preparation variant.');
  const openSource=(value:unknown)=>{if(!record(value)||!isOcelJsonDocument(value)||!Array.isArray(value.events)||!Array.isArray(value.objects))throw new Error('The backend returned an invalid prepared OCEL 2.0 source.');return value;};
  if(result.family==='OCEL_SPLIT'){if(!Array.isArray(result.open_logs)||result.open_logs.length!==result.logs.length)throw new Error('Prepared OCEL opening sources do not match the returned projections.');const splitResult=result;splitResult.logs.forEach((entry,position)=>{const log=splitResult.variant==='ANCESTORS_DESCENDANTS'?obj(entry.log):entry;outputs.push({position,...(splitResult.variant==='ANCESTORS_DESCENDANTS'?{central_object:String(entry.central_object)}:{}),log,open_log:openSource(splitResult.open_logs![position]),tables:preparedOcelTables(log)});});}else outputs.push({position:0,log:result.log,open_log:openSource(result.open_log),tables:preparedOcelTables(result.log)});
 }
 const report_json=`{"family":"OCEL_PREPARATION_REPORT","request":${JSON.stringify({method,source,parameters:Object.fromEntries(new URLSearchParams(query)),query})},"response":${method==='flatten'?JSON.stringify({format:'xes',data:raw}):raw}}`;
 return {method,variant,source,raw,report_json,result,outputs,flattened};
}
export function preparedOcelFile(run:OcelPreparationRun,position=0):File {if(run.method==='flatten')return new File([run.raw],`flattened-${run.source.name.replace(/\.[^.]+$/,'')}.xes`,{type:'application/xml'});const output=run.outputs[position];if(!output)throw new Error('Select a returned OCEL projection.');return new File([nativeJson(output.open_log)],`prepared-${run.method}-${position}-${run.source.name.replace(/\.[^.]+$/,'')}.json`,{type:'application/json'});}
