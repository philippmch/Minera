import { isRandomTreeResult, isLogConversionResult, type RandomTreeResult, type ProcessTreeNode, type LogConversionResult } from "./types";
import { readNativeResponse, nativeResponseRequest, hasNativeResponse } from "./native-response";
import { nativeCell, nativeJson } from "./native-json";
import { nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";

export const GENERATOR_CONTROLS = [
  ["seed","Generator seed","1","both"],
  ["activities","Activity labels as JSON (empty uses the reference six-character labels)","","basic"],
  ["rec_depth","Initial recursion depth","0","basic"],
  ["min_rec_depth","Minimum recursion depth","1","basic"],
  ["max_rec_depth","Maximum recursion depth","3","basic"],
  ["prob_leaf","Leaf probability","0.25","basic"],
  ["no_models","Number of generated models","1","ptandloggenerator"],
  ["min","Minimum activities","10","ptandloggenerator"],
  ["mode","Typical activities (mode)","20","ptandloggenerator"],
  ["max","Maximum activities","30","ptandloggenerator"],
  ["sequence","Sequence probability","0.25","ptandloggenerator"],
  ["choice","Choice probability","0.25","ptandloggenerator"],
  ["parallel","Parallel probability","0.25","ptandloggenerator"],
  ["loop","Loop probability","0.25","ptandloggenerator"],
  ["or","Inclusive-choice probability","0","ptandloggenerator"],
  ["silent","Silent activity probability","0.25","ptandloggenerator"],
  ["duplicate","Duplicate activity probability","0","ptandloggenerator"],
] as const;
export const GENERATION_PLAYOUT_CONTROLS = [
  ["traces","Generated cases","20","sample"], ["max_steps","Maximum visible steps per case","100","sample"],
  ["seed","Log seed","1","both"], ["min_length","Minimum trace length","0","extensive"],
  ["max_length","Maximum trace length","20","extensive"], ["max_traces","Maximum enumerated cases","100","extensive"],
  ["max_loop_occurrences","Maximum loop redo occurrences","2","extensive"],
  // Empty keeps the reference envelope (TOPBOTTOM and EXTENSIVE: no case id
  // or timestamp; BASIC_PLAYOUT: cases "0".. from epoch 10,000,000, one second
  // per event). Any value opts in to case_N ids with this timing.
  ["start","Log start (epoch seconds; empty: reference envelope)","","both"],
  ["case_interval","Case interval (seconds; empty: reference envelope)","","both"],
  ["event_interval","Event interval (seconds; empty: reference envelope)","","both"],
] as const;
export interface GenerationForm {
  variant: "basic"|"ptandloggenerator"; generator: Record<string,string>;
  conversion: "standard"|"transition_bordered"; withLog:boolean;
  playoutVariant:"BASIC_PLAYOUT"|"EXTENSIVE"|"TOPBOTTOM"; playout:Record<string,string>; returnStrings:boolean;
}
export function defaultGenerationForm(): GenerationForm {
  return {variant:"ptandloggenerator",generator:Object.fromEntries(GENERATOR_CONTROLS.map(([key,,value])=>[key,value])),
    conversion:"standard",withLog:false,playoutVariant:"TOPBOTTOM",playout:Object.fromEntries(GENERATION_PLAYOUT_CONTROLS.map(([key,,value])=>[key,value])),returnStrings:false};
}
export function treeGenerationQuery(form:GenerationForm):string {
  const q=new URLSearchParams({random_tree_variant:form.variant,tree_variant:form.conversion,generate_log:String(form.withLog)});
  for(const [key,,,variant] of GENERATOR_CONTROLS) if(variant==="both"||variant===form.variant) {
    const value=form.generator[key]?.trim(); if(!value)continue;
    if(key==="activities") {
      let labels:unknown;try{labels=JSON.parse(value);}catch{throw Error("Activity labels must be a JSON array of strings.");}
      if(!Array.isArray(labels)||!labels.length||labels.some(label=>typeof label!=="string"||!label.length))throw Error("Activity labels must be a nonempty JSON array of strings.");
      q.set("random_tree_activities",labels.map(label=>(label as string).replaceAll("\\","\\\\").replaceAll(",","\\,")).join(","));
    }else q.set("random_tree_"+key,value);
  }
  if(form.withLog){q.set("playout_variant",form.playoutVariant);
    for(const [key,,,variant]of GENERATION_PLAYOUT_CONTROLS)if(variant==="both"||(variant==="extensive")===(form.playoutVariant==="EXTENSIVE")){
      const value=form.playout[key]?.trim();if(value)q.set("playout_"+key,value);
    }
    if(form.playoutVariant==="EXTENSIVE")q.set("playout_return_set_strings",String(form.returnStrings));
  }
  return q.toString();
}
export interface GeneratedTreeModel {
  model_index:number; tree:{family:"PROCESS_TREE";activities:string[];root:ProcessTreeNode}; net:Record<string,unknown>|null;
  ptml:string|null;ptml_error:string|null;pnml:string|null;pnml_error:string|null;bpmn:string|null;bpmn_error:string|null;
  playout:{family:"GENERATED_TREE_PLAYOUT";variant:string;return_set_strings:boolean;truncated:boolean;case_count:number;event_count:number;
    log:LogConversionResult;language:Record<string,unknown>|null}|null;playout_xes:string|null;playout_error:string|null;
}
export interface TreeGenerationResult {family:"TREE_GENERATION";tree_variant:"standard"|"transition_bordered";generator:RandomTreeResult;models:GeneratedTreeModel[];}
const record=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
function treeNode(value:unknown,names:string[]):value is ProcessTreeNode {
  if(!record(value)||(value.children!==undefined&&!Array.isArray(value.children)))return false;
  const children=Array.isArray(value.children)?value.children:[];
  if(value.operator==="activity")return Number.isSafeInteger(value.activity)&&typeof value.activity==="number"&&value.activity>=0&&value.activity<names.length&&!children.length;
  if(value.activity!==undefined)return false;
  if(value.operator==="tau")return !children.length;
  return ["sequence","exclusive","parallel","inclusive_or","interleaving","loop"].includes(String(value.operator))&&children.length>=2&&children.every(child=>treeNode(child,names));
}
export function isTreeGenerationResult(value:unknown):value is TreeGenerationResult {
  if(!record(value)||value.family!=="TREE_GENERATION"||!["standard","transition_bordered"].includes(String(value.tree_variant))||!isRandomTreeResult(value.generator)||!Array.isArray(value.models))return false;
  const count=value.generator.tree?1:value.generator.trees!.length;
  return value.models.length===count&&value.models.every((model,index)=>{
    if(!record(model)||model.model_index!==index||!record(model.tree)||model.tree.family!=="PROCESS_TREE"||!Array.isArray(model.tree.activities)||!model.tree.activities.every(x=>typeof x==="string")||!treeNode(model.tree.root,model.tree.activities))return false;
    for(const key of ["ptml","ptml_error","pnml","pnml_error","bpmn","bpmn_error","playout_xes","playout_error"])if(model[key]!==null&&typeof model[key]!=="string")return false;
    if(model.net!==null&&!record(model.net))return false;
    const p=model.playout;
    return p===null||(record(p)&&p.family==="GENERATED_TREE_PLAYOUT"&&["BASIC_PLAYOUT","EXTENSIVE","TOPBOTTOM"].includes(String(p.variant))&&typeof p.truncated==="boolean"&&typeof p.return_set_strings==="boolean"&&Number.isSafeInteger(p.case_count)&&Number.isSafeInteger(p.event_count)&&isLogConversionResult(p.log)&&(p.language===null||record(p.language)));
  });
}
export async function generateTrees(form:GenerationForm,signal?:AbortSignal):Promise<TreeGenerationResult> {
  const path="/api/generate-trees?"+treeGenerationQuery(form);
  const response=await fetch(path,{method:"POST",signal});const value=await readNativeResponse(response,{path});
  if(!response.ok)throw Error(record(value)&&typeof value.error==="string"?value.error:`Minera answered with status ${response.status}`);
  if(!isTreeGenerationResult(value))throw Error("Minera returned an invalid tree-generation result.");return value;
}
export function generationReport(result:TreeGenerationResult):string {
  return nativeJson({family:"TREE_GENERATION_REPORT",origin:hasNativeResponse(result)?"native_http_response":"embedded_result",request:nativeResponseRequest(result)??null,native_result:result},2);
}
export function generationTables(model:GeneratedTreeModel):DiagnosticTable[] {
  const tables=nativeObjectTables(model);const rows:unknown[][]=[],records:unknown[]=[];
  const visit=(node:ProcessTreeNode,path:string,parent:string|null)=>{records.push(node);rows.push([path,parent,node.operator,node.activity,node.activity===undefined?undefined:model.tree.activities[node.activity],node.children?.length??0]);node.children?.forEach((child,i)=>visit(child,`${path}.${i}`,path));};
  visit(model.tree.root,"root",null);
  tables.unshift({id:"tree_nodes",title:"All generated tree nodes",columns:["path","parent","operator","activity_index","activity","child_count"],rows,records});
  if(model.playout){
    const traces="log" in model.playout.log?model.playout.log.log.traces:[];
    const events=traces.flatMap((trace,ti)=>trace.events.map((event,ei)=>({event,ti,ei,trace})));
    const keys=[...new Set(events.flatMap(row=>Object.keys(row.event)))];
    tables.push({id:"generated_events",title:"All generated events",columns:["trace_index","event_index","trace_case_id",...keys],
      rows:events.map(row=>[row.ti,row.ei,row.trace.case_id,...keys.map(key=>nativeCell(row.event,key))]),records:events.map(row=>row.event)});
  }
  return tables;
}
