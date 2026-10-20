import { generatedSql,requestLogQuery,type LogQuerySchema,type LogQueryResult,type QueryEngine } from "./log-query";
import {requestExactLogQuery,type ExactQueryControls,type ExactLogQueryRun,exactQueryStringText} from "./exact-log-query";
import { llmLogContext } from "./llm";
import { type MineraExport } from "./types";

export type StudyTask="clusters"|"hypotheses";
export interface ClusterDefinition {name:string;regex:string;}
export interface ClusterSummary extends ClusterDefinition {family:"LOG_CLUSTER";source_id:string;case_count:number;event_count:number;selected_event_indices:number[];}
export interface HypothesisDefinition {description:string;sql:string;}
export interface ClusterStudyItem extends ClusterDefinition {result:ClusterSummary|null;error:string|null;}
export interface HypothesisStudyItem extends HypothesisDefinition {result:LogQueryResult|null;error:string|null;exact_run?:ExactLogQueryRun;}

export function clusterDefinitions(response:string):ClusterDefinition[]{
 const match=response.match(/```json\s*([\s\S]*?)```/);if(!match)throw new Error("The cluster response must contain a ```json block.");
 const items:unknown=JSON.parse(match[1]);if(!Array.isArray(items)||items.length>100||!items.every(item=>item&&typeof item==="object"&&typeof item.name==="string"&&typeof item.regex==="string"))throw new Error("Provide a JSON list of at most 100 clusters with string name and regex fields.");
 return items.map(item=>({name:item.name,regex:item.regex}));
}
export function hypothesisDefinitions(response:string):HypothesisDefinition[]{
 const descriptions=[...response.matchAll(/<description>([\s\S]*?)<\/description>/g)].map(match=>match[1].trim());const queries=[...response.matchAll(/```sql\s*([\s\S]*?)```/g)].map(match=>generatedSql(match[0]));
 if(!queries.length||queries.length>100||descriptions.length!==queries.length)throw new Error("Provide matching <description> tags and ```sql blocks for 1 through 100 hypotheses.");
 return queries.map((sql,index)=>({description:descriptions[index],sql}));
}
export function studyPrompt(task:StudyTask,data:MineraExport,schema:LogQuerySchema,engine:QueryEngine,maxLen:number,tableName=schema.table_name):string{
 if(!Number.isSafeInteger(maxLen)||maxLen<1||maxLen>2*1024*1024)throw new Error("Context character limit must be from 1 through 2097152.");
 if(task==="clusters"){
  const variants=data.variants.map(item=>" "+item.activities.map(id=>data.activities[id]?.name??"").join(" -> ")+" ( frequency = "+item.count+" )").join("\n");
  return ("If I have a process with the following process variants:\n\n"+variants).slice(0,maxLen)+"\n\nIdentify clusters of traces. Return a JSON list inside ```json tags, with string 'name' and 'regex' fields for every cluster. Regex searches complete activity sequences joined with ' -> '. Cases may match more than one cluster.";
 }
 const alias='"'+tableName.replaceAll('"','""')+'"';const context=llmLogContext(data)+"\n\nTable "+alias+" has one row per event. Columns (quote literal names):\n"+schema.columns.filter(name=>name!==schema.event_index_column).map(name=>JSON.stringify(name)).join("\n");
 return "Formulate hypotheses over the event data.\n\n"+context.slice(0,maxLen)+`\n\nThe database engine is ${engine==="sqlite"?"SQLite":"DuckDB"}. Return each hypothesis in <description> and </description> tags, with its read-only SELECT query between \`\`\`sql and \`\`\` tags. Use only ${alias}. Do not modify data or access files.`;
}
export function isClusterSummary(value:unknown):value is ClusterSummary{
 if(!value||typeof value!=="object")return false;const item=value as Partial<ClusterSummary>;
 return item.family==="LOG_CLUSTER"&&typeof item.source_id==="string"&&typeof item.name==="string"&&typeof item.regex==="string"&&Number.isSafeInteger(item.case_count)&&item.case_count!>=0&&Number.isSafeInteger(item.event_count)&&item.event_count!>=0&&Array.isArray(item.selected_event_indices)&&item.selected_event_indices.every(index=>Number.isSafeInteger(index)&&index>=0);
}
function clusterUrl(schema:LogQuerySchema,definition:ClusterDefinition,format:string){return "/api/log-cluster?"+new URLSearchParams({source_id:schema.source_id,name:definition.name,format});}
async function clusterResponse(schema:LogQuerySchema,definition:ClusterDefinition,format:string,signal?:AbortSignal){
 const response=await fetch(clusterUrl(schema,definition,format),{method:"POST",headers:{"Content-Type":"text/plain; charset=utf-8"},body:definition.regex,...(signal?{signal}:{})});
 if(!response.ok){const value=await response.json().catch(()=>null);throw new Error(value?.error||`Cluster backend returned ${response.status}.`);}return response;
}
export async function requestCluster(schema:LogQuerySchema,definition:ClusterDefinition,signal?:AbortSignal):Promise<ClusterSummary>{
 const value:unknown=await (await clusterResponse(schema,definition,"summary",signal)).json();if(!isClusterSummary(value)||value.source_id!==schema.source_id)throw new Error("Invalid or stale cluster result; refresh the study source.");return value;
}
export async function requestClusterExport(schema:LogQuerySchema,definition:ClusterDefinition,format:"xes"|"json",signal?:AbortSignal){return (await clusterResponse(schema,definition,format,signal)).blob();}
export async function executeHypotheses(schema:LogQuerySchema,engine:QueryEngine,definitions:HypothesisDefinition[],limit:number,signal?:AbortSignal):Promise<HypothesisStudyItem[]>{
 const results:HypothesisStudyItem[]=[];for(const definition of definitions){if(signal?.aborted)throw new DOMException("Study cancelled","AbortError");try{const result=await requestLogQuery(schema,engine,definition.sql,limit,signal);results.push({...definition,result,error:null});}catch(cause){if(signal?.aborted)throw cause;results.push({...definition,result:null,error:cause instanceof Error?cause.message:String(cause)});}}
 return results;
}
/** Each owned failure remains beside later hypotheses; no option carrier is parsed or rewritten. */
export async function executeExactHypotheses(schema:LogQuerySchema,engine:QueryEngine,definitions:HypothesisDefinition[],controls:ExactQueryControls,signal?:AbortSignal):Promise<HypothesisStudyItem[]>{const results:HypothesisStudyItem[]=[];for(const definition of definitions){signal?.throwIfAborted();try{const exact_run=await requestExactLogQuery(schema,engine,definition.sql,controls,signal);results.push({...definition,result:null,error:exact_run.value.error?exactQueryStringText(exact_run.value.error.message):null,exact_run});}catch(cause){if(signal?.aborted)throw cause;results.push({...definition,result:null,error:cause instanceof Error?cause.message:String(cause)});}}return results;}
