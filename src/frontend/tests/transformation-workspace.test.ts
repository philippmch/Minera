import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { TransformationWorkspace } from "../src/components/TransformationWorkspace";
import { FEATURE_LISTS,EXTRA_FEATURES,defaultFeatureForm,DEFAULT_TARGET,DEFAULT_LABEL,exactList,featureOptions,targetOptions,labelOptions,isTransformationRun,requestTransformation,transformationMatrix,transformationCsv,transformedXes,type TransformationRun } from "../src/transformation-workspace";
import { logToFeaturesQuery,labelSplittingQuery,mineFormEntries } from "../src/api";
import { type MineraExport } from "../src/types";

const cases=[{index:0,case_id:"same",event_count:2},{index:1,case_id:"empty",event_count:0},{index:2,case_id:"same",event_count:1}];
const eventResult={family:"LOG_TO_FEATURES",variant:"event_based",feature_names:["activity:A","cost"],event_rows:[[[1,2.5],[0,"NaN"]],[[0,0],[0,0]],[[1,3],[0,0]]]} as const;
const traceResult={family:"LOG_TO_FEATURES",variant:"trace_based",feature_names:["@@case_id_column","","cost","cost"],case_ids:["same","empty","same"],trace_rows:[[{key:"id",value:"18446744073709551615",kind:"int",container:"scalar",null:false},0,2.5,2.5],["empty",0,0,0],[true,"Infinity",3,3]]} as const;
const targetResult={family:"LOG_TO_TARGET",variant:"next_activity",trace_count:3,target_width:2,padded:true,classes:["B 東京","A,α"],rows:[[[1,0],[0,0],[0,0]],[[0,0],[0,0],[0,0]],[[0,0],[0,0],[0,0]]]} as const;
const temporalResult={family:"LOG_TO_FEATURES",variant:"temporal",feature_names:["total_number_of_reworked_activities"],temporal_rows:[{timestamp:1790899200,timestamp_ns:"1790899200000000000",values:[""]}]} as const;
const envelope=(result:unknown)=>({family:"LOG_TRANSFORMATION",source_id:"7",cases,result,transformed_xes:null});
const run=(result:unknown)=>({...envelope(result),data_json:"",report_json:""}) as TransformationRun;
const labels={...envelope({family:"LABEL_SPLITTING",variant:"contextual",log:{traces:cases.map(c=>({case_id:c.case_id,events:Array.from({length:c.event_count},(_,i)=>({activity:i?"B":"A_0",timestamp:"",resource:"",lifecycle:"",input_order:i}))}))}}),transformed_xes:'<?xml version="1.0"?><log/>',activity_attribute:"phase",complete_activity_field:true,label_rows:[{case_index:0,event_index:0,input:"A",previous:"A",output:"A_0"},{case_index:0,event_index:1,input:"B",previous:"B",output:"B"},{case_index:2,event_index:0,input:"A",previous:"A",output:"A_1"}]};
const respond=(value:unknown)=>vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>JSON.stringify(value)}));
afterEach(()=>vi.unstubAllGlobals());
describe("native transformation workflows",()=>{
  it("retains literal XES selector names through the transformed-log handoff",()=>{
    const entries=Object.fromEntries(mineFormEntries({xesActivityAttribute:" output field 東京 ",xesCaseAttribute:" case id "}));expect(entries.xes_activity_attribute).toBe(" output field 東京 ");expect(entries.xes_case_attribute).toBe(" case id ");expect(()=>mineFormEntries({xesActivityAttribute:""})).toThrow(/must not be empty/);
  });
  it("preserves omitted lists, explicit empty lists and literal JSON names",()=>{
    expect(exactList({mode:"native",text:"invalid"},"Names")).toBeUndefined();expect(exactList({mode:"empty",text:"invalid"},"Names")).toEqual([]);
    expect(exactList({mode:"list",text:'["A,α","path\\\\name",""," A ","A,α"]'},"Names")).toEqual(["A,α","path\\name",""," A ","A,α"]);expect(()=>exactList({mode:"list",text:"[1]"},"Names")).toThrow(/JSON array/);
    const form=defaultFeatureForm();form.lists.event_string_attributes={mode:"empty",text:""};form.lists.feature_names={mode:"list",text:'["", "dup", "dup", "A,α"]'};const options=featureOptions(form);const query=new URLSearchParams(logToFeaturesQuery(options));expect(query.get("features_event_string_attributes")).toBe("");expect(query.has("features_case_string_attributes")).toBe(false);expect(query.get("features_feature_names")).toBe('\\e,dup,dup,A\\,α');
  });
  it("supports all projections, calendar/fixed windows and explicit extra-feature overrides",()=>{
    for(const variant of ["event_based","trace_based","temporal"] as const)expect(featureOptions({...defaultFeatureForm(),variant}).features_variant).toBe(variant);
    const form=defaultFeatureForm();form.allExtra=true;form.extra.enable_case_duration="false";form.extra.enable_resource_workload="true";expect(featureOptions(form)).toMatchObject({features_enable_all_extra_features:true,features_enable_case_duration:false,features_enable_resource_workload:true});expect(featureOptions(form).features_enable_work_in_progress).toBeUndefined();
    const fixed=featureOptions({...form,variant:"temporal",grouping:"fixed",window:"3600"});expect(fixed.features_window_seconds).toBe(3600);expect(fixed.features_grouper_freq).toBeUndefined();const calendar=featureOptions({...form,variant:"temporal",frequency:"MS",destinations:{...form.destinations,service_time:""}});expect(calendar).toMatchObject({features_grouper_freq:"MS",features_service_time:""});
    expect(FEATURE_LISTS).toHaveLength(8);expect(EXTRA_FEATURES).toHaveLength(10);expect(()=>featureOptions({...form,epsilon:"-1"})).toThrow(/Epsilon/);expect(()=>featureOptions({...form,maximum:"9007199254740992"})).toThrow(/Maximum/);
  });
  it("retains activity class order, padding bounds and native label context parameters",()=>{
    const options=targetOptions({...DEFAULT_TARGET,padding:true,size:"3",activities:{mode:"list",text:'["B 東京","A,α"]'}});expect(options).toMatchObject({target_activities:["B 東京","A,α"],target_pad_size:3,target_enable_padding:true});expect(targetOptions({...DEFAULT_TARGET,variant:"next_time",activities:{mode:"list",text:"invalid"}}).target_activities).toBeUndefined();expect(()=>targetOptions({...DEFAULT_TARGET,padding:true,size:"100001"})).toThrow(/Padding/);
    const split=labelOptions({...DEFAULT_LABEL,prefix:"0",suffix:"4096",weight:"-1",separator:"→",activities:{mode:"list",text:'["A,α"]'}});expect(split).toMatchObject({label_splitting_prefix_length:0,label_splitting_suffix_length:4096,label_splitting_min_edge_weight:-1});expect(new URLSearchParams(labelSplittingQuery(split)).get("label_splitting_target_activities")).toBe("A\\,α");expect(()=>labelOptions({...DEFAULT_LABEL,prefix:"4097"})).toThrow(/Prefix/);expect(()=>labelOptions({...DEFAULT_LABEL,activities:{mode:"list",text:'["A","A"]'}})).toThrow(/unique/);
  });
  it("validates event rows, repeated source IDs, rich identifiers and temporal schema",()=>{
    for(const result of [eventResult,traceResult,temporalResult])expect(isTransformationRun(envelope(result))).toBe(true);
    expect(isTransformationRun(envelope({...eventResult,event_rows:[[[1,2.5]],[],[[1,3]]]}))).toBe(false);expect(isTransformationRun(envelope({...eventResult,event_rows:[[[1],[0]],[],[[1]]]}))).toBe(false);expect(isTransformationRun({...envelope(traceResult),cases:[cases[2],cases[1],cases[0]]})).toBe(false);expect(isTransformationRun(envelope({...traceResult,case_ids:["only"]}))).toBe(false);
    const matrix=transformationMatrix(run(traceResult));expect(matrix.columns).toEqual(traceResult.feature_names);expect(matrix.rowAt(0).metadata).toEqual([0,"same","same"]);expect(matrix.rowAt(2).metadata).toEqual([2,"same","same"]);
  });
  it("validates padding dimensions and exposes actual event versus padding rows",()=>{
    expect(isTransformationRun(envelope(targetResult))).toBe(true);expect(isTransformationRun(envelope({...targetResult,target_width:1}))).toBe(false);expect(isTransformationRun(envelope({...targetResult,rows:[targetResult.rows[0],[],targetResult.rows[2]]}))).toBe(false);
    const matrix=transformationMatrix(run(targetResult));expect(matrix.count).toBe(9);expect(matrix.rowAt(1).metadata).toEqual([0,"same",1,"event"]);expect(matrix.rowAt(2).metadata).toEqual([0,"same",2,"padding"]);expect(matrix.rowAt(3).metadata).toEqual([1,"empty",0,"padding"]);expect(matrix.rowAt(6).metadata).toEqual([2,"same",0,"event"]);
  });
  it("retains padded empty traces without inventing source events or changing source indices",()=>{
    const matrix=transformationMatrix(run(eventResult));expect(matrix.count).toBe(6);expect(matrix.rowAt(4)).toEqual({metadata:[2,"same",0,"event"],values:[1,3]});expect(matrix.rowAt(2)).toEqual({metadata:[1,"empty",0,"padding"],values:[0,0]});const view=transformationMatrix(run(temporalResult));expect(view.rowAt(0).metadata[2]).toBe("1790899200000000000");expect(view.rowAt(0).values).toEqual([""]);
  });
  it("validates ordered label rows and output field completeness, and exports the same XES",async()=>{
    expect(isTransformationRun(labels)).toBe(true);expect(isTransformationRun({...labels,label_rows:[labels.label_rows[2],...labels.label_rows.slice(0,2)]})).toBe(false);expect(isTransformationRun({...labels,complete_activity_field:false})).toBe(false);
    const partial={...labels,complete_activity_field:false,label_rows:labels.label_rows.map((row,i)=>i?row:{...row,output:""})};expect(isTransformationRun(partial)).toBe(true);
    const r={...labels,data_json:"",report_json:""} as TransformationRun;expect(transformationMatrix(r).rowAt(2)).toEqual({metadata:[2,"same",0],values:["A","A","A_1"]});expect(await transformedXes(r).text()).toBe(labels.transformed_xes);expect(()=>transformedXes(run(eventResult))).toThrow(/does not produce/);
  });
  it("downloads all rows and duplicate/empty dimensions with quoted CSV data",async()=>{
    const csv=await transformationCsv(run(traceResult)).text();expect(csv.split("\r\n")).toHaveLength(5);expect(csv).toContain('"@@case_id_column","","cost","cost"');expect(csv).toContain('"18446744073709551615","0","2.5","2.5"');expect(csv).toContain('"true","Infinity","3","3"');const target=await transformationCsv(run(targetResult)).text();expect(target).toContain('"B 東京","A,α"');expect(target.match(/"padding"/g)).toHaveLength(6);
  });
  it("binds all routes to the source and preserves the complete original native JSON",async()=>{
    for(const [method,result,route] of [["features",eventResult,"log-to-features"],["targets",targetResult,"transform-target"],["labels",labels.result,"label-splitting"]] as const){const raw=JSON.stringify(method==="labels"?labels:envelope(result));const fetch=vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>raw}));vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;const r=await requestTransformation(method,"7",{},signal);expect(fetch.mock.calls[0][0]).toContain(`/api/${route}?source_id=7&include_source=true`);expect(fetch.mock.calls[0][1]).toEqual({method:"POST",signal});expect(r.data_json).toBe(raw);expect(JSON.parse(r.report_json).response).toEqual(JSON.parse(raw));}
  });
  it("retains exact native timestamp strings and rejects stale/mismatched responses",async()=>{
    vi.stubGlobal("fetch",respond(envelope(temporalResult)));const r=await requestTransformation("features","7",{features_variant:"temporal"});expect(r.report_json).toContain('"timestamp_ns":"1790899200000000000"');vi.stubGlobal("fetch",respond({...envelope(eventResult),source_id:"8"}));await expect(requestTransformation("features","7",{})).rejects.toThrow(/source log changed/);vi.stubGlobal("fetch",respond(envelope(targetResult)));await expect(requestTransformation("features","7",{})).rejects.toThrow(/different transformation method/);vi.stubGlobal("fetch",respond(envelope(eventResult)));await expect(requestTransformation("features","7",{features_variant:"trace_based"})).rejects.toThrow(/different transformation variant/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,text:async()=>'{"error":"padding must cover the longest trace"}'})));await expect(requestTransformation("targets","7",{})).rejects.toThrow(/longest trace/);
  });
  it("disables saved-analysis sources and exposes all feature projections in the app",()=>{
    const data={dataset:"saved.json",trace_count:3,event_count:3} as MineraExport;const html=renderToStaticMarkup(createElement(TransformationWorkspace,{data,available:false,mining:false,onOpenTransformed:()=>{}}));expect(html).toContain("Saved analysis JSON does not provide an active transformation source");expect(html).toContain('<fieldset class="query-fields" disabled');for(const value of ["features","targets","labels","event_based","trace_based","temporal"])expect(html).toContain(`value="${value}"`);for(const [,label] of FEATURE_LISTS)expect(html).toContain(label);for(const [,label] of EXTRA_FEATURES)expect(html).toContain(label);
  });
});
