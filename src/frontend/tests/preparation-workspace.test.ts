import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { PreparationWorkspace } from "../src/components/PreparationWorkspace";
import { TransformationHub } from "../src/components/TransformationHub";
import { CONVERSION_METHODS,UTILITY_METHODS,defaultConversionForm,DEFAULT_UTILITY,conversionOptions,utilityOptions,isPreparationRun,requestPreparation,outputLog,preparationTable,preparationCsv,preparedXes,attributedGraphLayout,type PreparationRun } from "../src/preparation-workspace";
import { logConversionQuery,logUtilityQuery } from "../src/api";
import { type MineraExport,type LogConversionGraph,type LogConversionAttributeKind,type LogConversionAttribute } from "../src/types";

const attribute=(key:string,value:string,kind:LogConversionAttributeKind="string",extra:Partial<LogConversionAttribute>={}):LogConversionAttribute=>({key,value,kind,container:"scalar",null:false,...extra});
const event=(activity:string,input_order=0)=>({case_id:"same",activity,timestamp:"2026-10-01T09:00:00.123456Z",resource:"R 東京",lifecycle:"complete",input_order,attributes:[attribute("cost","1.25","float"),attribute("approved","true","boolean"),attribute("time:timestamp","2026-10-01T09:00:00.123456Z","date"),attribute("native_id","18446744073709551615","int"),attribute("optional","", "string",{null:true})]});
const log={attributes:[attribute("concept:name","Owned log")],traces:[{case_id:"same",attributes:[attribute("segment","gold")],events:[event('A,"東京'),event("B",1)]},{case_id:"empty",attributes:[],events:[]},{case_id:"same",attributes:[attribute("segment","silver")],events:[event("C")]}]};
const output=(id="log",selected=log)=>({id,name:"Same label",case_count:selected.traces.length,event_count:selected.traces.reduce((sum,trace)=>sum+trace.events.length,0),activity_attribute:"phase",complete_activity_field:true,xes:'<?xml version="1.0"?><log><trace/></log>'});
const envelope=(result:unknown,outputs:unknown[]=[])=>({family:"LOG_PREPARATION",source_id:"7",result,outputs});
const run=(result:unknown,outputs:unknown[]=[])=>({...envelope(result,outputs),data_json:"",report_json:""}) as PreparationRun;
const fixed={family:"LOG_UTIL",variant:"FIXED_PREFIXES",log};
const respond=(value:unknown)=>vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>JSON.stringify(value)}));
afterEach(()=>vi.unstubAllGlobals());

describe("log preparation workflows",()=>{
  it("exposes all six native conversion variants and bounded conversion options",()=>{
    for(const [variant] of CONVERSION_METHODS)expect(conversionOptions({...defaultConversionForm(),variant}).log_conversion_variant).toBe(variant);
    const form={...defaultConversionForm(),variant:"to_nx" as const,caseNodes:{mode:"list" as const,text:'["segment","A,α"]'},eventNodes:{mode:"list" as const,text:'["cost","path\\\\name"]'},prefix:"",maximumVariants:"0",maximumRows:"0",includeCases:false,lazy:true,postprocess:true};
    const options=conversionOptions(form);expect(options).toMatchObject({log_conversion_include_case_attributes:false,log_conversion_lazy_output:true,log_conversion_stream_postprocessing:true,log_conversion_case_attribute_prefix:"",log_conversion_max_variants_to_return:0,log_conversion_case_attributes_as_nodes:["segment","A,α"]});
    const query=new URLSearchParams(logConversionQuery(options));expect(query.get("log_conversion_case_attributes_as_nodes")).toBe("segment,A\\,α");expect(query.get("log_conversion_event_attributes_as_nodes")).toBe("cost,path\\\\name");
    expect(()=>conversionOptions({...form,maximumNodes:"200001"})).toThrow(/Maximum graph nodes/);expect(()=>conversionOptions({...form,eventNodes:{mode:"list",text:'[""]'}})).toThrow(/non-empty names/);
  });
  it("keeps a prepared output whose text XES cannot carry and reports the writer's refusal",()=>{
    const refused={...output(),xes:null,xes_error:"XES cannot represent this text: \"A\\u0001x\""};
    expect(isPreparationRun(envelope(fixed,[refused]))).toBe(true);
    expect(isPreparationRun(envelope(fixed,[{...output(),xes:null}]))).toBe(false);
    expect(isPreparationRun(envelope(fixed,[{...output(),xes_error:"refused"}]))).toBe(false);
    expect(()=>preparedXes(refused)).toThrow(/XES export is unavailable: XES cannot represent/);
  });
  it("serializes all six case utilities with exact activity lists and native defaults",()=>{
    expect(UTILITY_METHODS).toHaveLength(6);
    const forms=[DEFAULT_UTILITY,{...DEFAULT_UTILITY,variant:"fixed_prefixes" as const,length:"0"},{...DEFAULT_UTILITY,variant:"all_prefixes" as const},{...DEFAULT_UTILITY,variant:"until_activity" as const,activity:" A,α "},{...DEFAULT_UTILITY,variant:"to_activities" as const,activities:'["A,α","B 東京"]'},{...DEFAULT_UTILITY,variant:"feature_selection" as const,minimum:"0",mandatory:{mode:"list" as const,text:'["native_id","A,α"]'}}];
    for(const form of forms)expect(utilityOptions(form).variant).toBe(form.variant);
    expect(utilityOptions(forms[1])).toMatchObject({prefix_length:0});expect(new URLSearchParams(logUtilityQuery(utilityOptions(forms[4]))).get("activities")).toBe("A\\,α,B 東京");expect(utilityOptions(forms[2])).toEqual({variant:"all_prefixes"});
    expect(()=>utilityOptions({...DEFAULT_UTILITY,percentage:"1.01"})).toThrow(/Training percentage/);expect(()=>utilityOptions({...DEFAULT_UTILITY,seed:"9007199254740992"})).toThrow(/Seed/);expect(()=>utilityOptions({...forms[5],minimum:"51"})).toThrow(/minimum exceeds maximum/);
  });
  it("accepts native numeric, date and boolean utility attributes, including uint64 lexical values",()=>{
    expect(isPreparationRun(envelope(fixed,[output()]))).toBe(true);
    const r=run(fixed,[output()]);const table=preparationTable(r,"log");expect(table.rowAt(0)[table.columns.indexOf("native_id")]).toBe("18446744073709551615");expect(table.rowAt(0)[table.columns.indexOf("optional")]).toBeNull();
    const invalid=structuredClone(log);invalid.traces[0].events[0].attributes[0].kind="unknown" as LogConversionAttributeKind;expect(isPreparationRun(envelope({...fixed,log:invalid},[output()]))).toBe(false);
  });
  it("validates stable split and target output positions, independent of repeated names and IDs",()=>{
    const split={family:"LOG_UTIL",variant:"SPLIT_TRAIN_TEST",training:log,testing:log};const outputs=[output("training"),output("testing")];expect(isPreparationRun(envelope(split,outputs))).toBe(true);expect(isPreparationRun(envelope(split,outputs.slice().reverse()))).toBe(false);expect(isPreparationRun(envelope(split,[outputs[0],outputs[0]]))).toBe(false);
    const targets={family:"LOG_UTIL",variant:"TO_ACTIVITIES",activities:["A","A"],logs:[log,log]};const r=run(targets,[output("target/0"),output("target/1")]);expect(isPreparationRun(r)).toBe(true);expect(outputLog(r,"target/1")).toBe(log);expect(outputLog(r,"target/2")).toBeUndefined();
    expect(isPreparationRun(envelope(fixed,[{...output(),event_count:4}]))).toBe(false);expect(isPreparationRun(envelope(fixed,[]))).toBe(false);
  });
  it("keeps empty traces and output positions distinct when flattening event rows",()=>{
    const r=run(fixed,[output()]);const table=preparationTable(r,"log");expect(table.count).toBe(3);expect(table.rowAt(2).slice(0,4)).toEqual([2,"same",0,"C"]);expect(outputLog(r,"log")?.traces[1].events).toEqual([]);
    expect(preparationTable(r,"unselected").count).toBe(0);const prefixes={family:"LOG_UTIL",variant:"ALL_PREFIXES",log,change_indexes:[[1,1],[],[2]]};expect(isPreparationRun(envelope(prefixes,[output()]))).toBe(true);
  });
  it("retains table nulls, duplicate names and exact native scalar values in complete CSV",async()=>{
    const result={family:"LOG_CONVERSION",variant:"TO_DATA_FRAME",table:{columns:["","dup","dup"],rows:[[attribute("","18446744073709551615","int"),attribute("dup",'A,"東京'),null],[null,attribute("dup","false","boolean"),attribute("dup","NaN","float")]],lazy:true}};
    const r=run(result);expect(isPreparationRun(r)).toBe(true);expect(preparationTable(r).rowAt(0)).toEqual(["18446744073709551615",'A,"東京',null]);const csv=await preparationCsv(r).text();expect(csv).toBe('"","dup","dup"\r\n"18446744073709551615","A,""東京","null"\r\n"null","false","NaN"\r\n');
    expect(await preparationCsv(run(fixed,[output()]),"log").text()).toContain('"2","same","0","C"');
  });
  it("retains stream field order and latest duplicate-key values without inventing missing attributes",()=>{
    const r=run({family:"LOG_CONVERSION",variant:"TO_EVENT_STREAM",stream:{attributes:[],events:[{fields:[attribute("x","1","int"),attribute("x","2","int")]},{fields:[attribute("y","true","boolean")]}]}});expect(isPreparationRun(r)).toBe(true);const table=preparationTable(r);expect(table.columns).toEqual(["output_event_position","x","y"]);expect(table.rowAt(0)).toEqual([0,"2",null]);expect(table.rowAt(1)).toEqual([1,null,"true"]);
  });
  it("shows complete attributed graph data and retains native edge indices across a drawing cap",()=>{
    const graph:LogConversionGraph={nodes:[{id:"same",type:"CASE",attributes:[]},{id:"same",type:"EVENT",attributes:[]},{id:"same",type:"ATTRIBUTE_NODE",attributes:[attribute("cost","1.25","float")]}],edges:[{source:2,target:1,type:"ATTRIBUTE_EDGE",name:"cost"},{source:1,target:0,type:"BELONGS_TO"}]};const r=run({family:"LOG_CONVERSION",variant:"TO_NX",graph});expect(isPreparationRun(r)).toBe(true);expect(preparationTable(r).rowAt(2)).toEqual([2,"same","ATTRIBUTE_NODE",JSON.stringify(graph.nodes[2].attributes)]);
    const layout=attributedGraphLayout(graph,2);expect(layout.nodes.map(node=>node.index)).toEqual([0,1]);expect(layout.edges.map(edge=>[edge.index,edge.from.index,edge.to.index])).toEqual([[1,1,0]]);expect(layout.totalNodes).toBe(3);expect(layout.totalEdges).toBe(2);expect(isPreparationRun(run({family:"LOG_CONVERSION",variant:"TO_NX",graph:{...graph,edges:[{source:9,target:0,type:"DF"}]}}))).toBe(false);
  });
  it("exposes attribute-selection scopes and exact same-run XES text",async()=>{
    const r=run({family:"LOG_UTIL",variant:"FEATURE_SELECTION",event_attributes:["cost","cost"],case_attributes:["native_id"]});expect(isPreparationRun(r)).toBe(true);const table=preparationTable(r);expect(table.count).toBe(3);expect(table.rowAt(2)).toEqual(["Case","native_id"]);expect(await preparedXes(output()).text()).toBe(output().xes);
    for(const variant of ["TO_EVENT_LOG","DF_TO_EVENT_LOG_1V","DF_TO_EVENT_LOG_NV"])expect(isPreparationRun(envelope({family:"LOG_CONVERSION",variant,log},[output()]))).toBe(true);
  });
  it("binds both routes to the current source and retains original JSON bytes in reports",async()=>{
    for(const method of ["conversion","utilities"] as const){const result=method==="conversion"?{family:"LOG_CONVERSION",variant:"TO_EVENT_LOG",log}:fixed;const raw=JSON.stringify(envelope(result,[output()])).replace('"input_order":0','"input_order":0,"opaque_uint64":18446744073709551615');const fetch=vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>raw}));vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;const r=await requestPreparation(method,"7",method==="conversion"?{log_conversion_variant:"to_event_log"}:{variant:"fixed_prefixes",prefix_length:2},signal);expect(fetch.mock.calls[0][0]).toContain(`/api/${method==="conversion"?"log-convert":"log-util"}?`);const params=new URLSearchParams(fetch.mock.calls[0][0].split("?")[1]);expect(params.get("source_id")).toBe("7");expect(params.get("include_source")).toBe("true");expect(fetch.mock.calls[0][1]).toEqual({method:"POST",signal});expect(r.data_json).toBe(raw);expect(r.report_json).toContain('"opaque_uint64":18446744073709551615');}
  });
  it("rejects stale, mismatched and invalid results and surfaces native errors",async()=>{
    const value=envelope(fixed,[output()]);vi.stubGlobal("fetch",respond({...value,source_id:"8"}));await expect(requestPreparation("utilities","7",{variant:"fixed_prefixes",prefix_length:2})).rejects.toThrow(/source log changed/);
    vi.stubGlobal("fetch",respond(value));await expect(requestPreparation("conversion","7",{})).rejects.toThrow(/different preparation method/);await expect(requestPreparation("utilities","7",{variant:"all_prefixes"})).rejects.toThrow(/different preparation variant/);
    vi.stubGlobal("fetch",respond(envelope(fixed,[])));await expect(requestPreparation("utilities","7",{})).rejects.toThrow(/invalid preparation result/);vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,text:async()=>'{"error":"graph node limit exceeded"}'})));await expect(requestPreparation("conversion","7",{})).rejects.toThrow(/node limit/);
  });
  it("keeps saved analysis sources disabled and exposes preparation beside existing transformations",()=>{
    const data={dataset:"saved.json",trace_count:3,event_count:3} as MineraExport;const html=renderToStaticMarkup(createElement(PreparationWorkspace,{data,available:false,mining:false,onOpenPrepared:()=>{}}));expect(html).toContain("Saved analysis JSON does not provide an active preparation source");expect(html).toContain('<fieldset class="query-fields" disabled');for(const [variant] of CONVERSION_METHODS)expect(html).toContain(`value="${variant}"`);
    const hub=renderToStaticMarkup(createElement(TransformationHub,{data,available:false,mining:false,onOpenTransformed:()=>{}}));expect(hub).toContain("Features, targets &amp; labels");expect(hub).toContain("Prepare logs &amp; event data");
  });
});
