import { afterEach,describe,expect,it,vi } from "vitest";
import { defaultWorkflowForm,runWorkflowDiscovery,validWorkflowDiscovery,workflowModelFile,workflowNet,workflowQuery,workflowTables,type WorkflowDiscovery,type WorkflowFamily } from "../src/workflow-discovery";
import { nativeJson,parseNativeJson } from "../src/native-json";
import type { LogQuerySchema } from "../src/log-query";
const fixtures=import.meta.glob<string>("./fixtures/structured-discovery-*.json",{eager:true,query:"?raw",import:"default"});
const value=(name:string)=>parseNativeJson(fixtures[`./fixtures/structured-discovery-${name}.json`]) as WorkflowDiscovery;
afterEach(()=>vi.unstubAllGlobals());
describe("separate native Inductive and POWL discovery",()=>{
  it.each(Object.entries(fixtures))("retains complete real native output %s",async(_,raw)=>{
    const expected=parseNativeJson(raw) as WorkflowDiscovery;
    const schema={source_id:expected.source_id,case_count:expected.source.case_count,event_count:expected.source.event_count} as LogQuerySchema;
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw)));
    const run=await runWorkflowDiscovery(expected.method,defaultWorkflowForm(expected.variant),schema);
    expect(run.raw).toBe(raw);expect(validWorkflowDiscovery(run.value)).toBe(true);expect(JSON.parse(run.report).response).toEqual(JSON.parse(raw));
    expect(workflowNet(run.value).transitions.length).toBeGreaterThan(0);
    expect(run.tables.structure_nodes.count).toBeGreaterThan(0);
  });
  it.each(['inductive','powl'] as WorkflowFamily[])("keeps original and adapted %s source counts and exact import selectors",family=>{
    const result=value(family+'-adapted');expect(result.source.case_count).toBe(60);expect(result.discovery_source?.case_count).toBe(2);expect(result.result.trace_count).toBe(2);
    expect(result.source_xes_options).toEqual({xesActivityAttribute:family==='inductive'?'phase':'activity',xesCaseAttribute:'bucket',xesTimestampAttribute:'reverse_time',xesTimestampSort:false});
    expect(result.result.activities.some(a=>a.name==='活動-A')).toBe(true);
    expect(workflowTables(result).structure_nodes.count).toBeGreaterThan(0);
  });
  it("retains every ordered POWL subtree and all native frequency/relation records",()=>{
    const result=value('powl-dynamic_clustering');const table=workflowTables(result).structure_nodes;
    expect(table.rowAt(0)[0]).toBe('root');expect(table.rowAt(0)[1]).toBe(nativeJson(result.result.powl));
    expect(Array.from({length:table.count},(_,i)=>String(table.rowAt(i)[0])).every(path=>path.startsWith('root'))).toBe(true);
    expect(nativeJson(result.result.powl)).toContain('frequency');
  });
  it("serializes the complete native Inductive control family and full-width playout seed",()=>{
    const form=defaultWorkflowForm('imf');Object.assign(form,{noise:'0.2',activity:'phase',timestamp:'reverse_time',caseId:'bucket',disableStrict:true,disableFallthroughs:true,multiprocessing:true,sortTime:true,conversion:'transition_bordered',playoutSeed:'18446744073709551615'});
    const q=workflowQuery('inductive',form,'owned');expect(q.get('inductive_variant')).toBe('imf');expect(q.get('noise_threshold')).toBe('0.2');
    for(const key of ['inductive_disable_strict_sequence_cut','inductive_disable_fallthroughs','inductive_multiprocessing','inductive_sort_events_by_timestamp'])expect(q.get(key)).toBe('true');
    expect(q.get('inductive_case_id_key')).toBe('bucket');expect(q.get('discovery_tree_variant')).toBe('transition_bordered');expect(q.get('discovery_playout_seed')).toBe('18446744073709551615');
    form.noise='2';expect(()=>workflowQuery('inductive',form,'owned')).toThrow(/\[0,1\]/);
    form.noise='-0.1';expect(()=>workflowQuery('inductive',form,'owned')).toThrow(/\[0,1\]/);
  });
  it("sends all five POWL variants and native filtering/order/search controls",()=>{
    for(const variant of ['tree','brute_force','maximal','dynamic_clustering','dynamic_clustering_plain'] as const){const form=defaultWorkflowForm(variant);Object.assign(form,{filtering:'dynamic',filteringThreshold:'0.2',orderRatio:'0.8',bruteLimit:'4'});const q=workflowQuery('powl',form,'owned');expect(q.get('powl_variant')).toBe(variant);expect(q.get('powl_filtering_type')).toBe('dynamic');expect(q.get('powl_filtering_threshold')).toBe('0.2');expect(q.get('powl_order_frequency_ratio')).toBe('0.8');expect(q.get('powl_brute_force_activity_limit')).toBe('4');}
  });
  it("preserves same-model XML and refuses missing/mismatched adapted-source contracts",async()=>{
    const result=value('inductive-dfg');const run={value:result,raw:nativeJson(result),report:'',tables:workflowTables(result)};
    for(const format of ['pnml','ptml','bpmn']as const)if(result[format])expect(await workflowModelFile(run,format).text()).toBe(result[format]);
    expect(validWorkflowDiscovery({...result,source_xes_options:undefined})).toBe(false);
    expect(validWorkflowDiscovery({...result,discovery_source:{case_count:999,event_count:180}})).toBe(false);
    expect(()=>workflowQuery('powl',defaultWorkflowForm('imf'),'owned')).toThrow(/variant/);
  });
  it("rejects a stale source response without substituting cached discovery",async()=>{
    const result=value('inductive-log');vi.stubGlobal('fetch',vi.fn(async()=>new Response(nativeJson(result))));
    await expect(runWorkflowDiscovery('inductive',defaultWorkflowForm('log'),{source_id:'another',case_count:60,event_count:180}as LogQuerySchema)).rejects.toThrow(/does not match/);
  });
});
