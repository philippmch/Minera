import { afterEach,describe,expect,it,vi } from "vitest";
import { defaultFootprintsForm,defaultTransitionForm,graphDiscoveryQuery,graphDiscoveryTables,runGraphDiscovery,validGraphDiscovery,type GraphDiscovery,type FootprintsForm,type TransitionForm } from "../src/graph-discovery";
import { nativeCell,nativeJson,parseNativeJson } from "../src/native-json";
import { diagnosticCsv } from "../src/model-diagnostics";
import type { LogQuerySchema } from "../src/log-query";
import parameters from "./fixtures/graph-request-parameters.json";
const fixtures=import.meta.glob<string>("./fixtures/graph-discovery-*.json",{eager:true,query:"?raw",import:"default"});
const value=(name:string)=>parseNativeJson(fixtures[`./fixtures/graph-discovery-${name}.json`]) as GraphDiscovery;
function recordedForm(name:string):FootprintsForm|TransitionForm {
  const q=(parameters as Record<string,Record<string,string>>)[name];
  if(q.family==='transition_system')return {...defaultTransitionForm(),view:(q.transition_view??'sequence') as TransitionForm['view'],direction:(q.transition_direction??'forward') as TransitionForm['direction'],window:q.transition_window??'',includeData:q.transition_include_data==='true',activity:q.transition_activity_attribute??'',caseId:q.transition_case_id_attribute??''};
  const f=defaultFootprintsForm();for(const[key,wire]of [['activity','activity'],['caseId','case_id'],['timestamp','timestamp'],['start','start_timestamp']]as const)f[key]=q['footprints_'+wire+'_attribute']??'';
  return {...f,discovery:q.footprints_discovery_variants?.split(',')??f.discovery,conformance:q.footprints_conformance_variants?.split(',')??f.conformance,model:(q.footprints_model??f.model)as FootprintsForm['model'],strict:q.footprints_strict==='true',always:q.footprints_enable_act_always_executed!=='false',sort:q.footprints_sort_required!=='false',overrideIndex:'footprints_index_attribute'in q,index:q.footprints_index_attribute??'',marking:q.footprints_initial_marking??''};
}
afterEach(()=>vi.unstubAllGlobals());
describe('independent native footprints and transition systems',()=>{
  it.each(Object.entries(fixtures))('retains the exact real fixture and submitted configuration %s',async(path,raw)=>{
    const expected=parseNativeJson(raw) as GraphDiscovery;
    const id=path.slice(path.lastIndexOf('/')+1).replace(/^graph-discovery-/,'').replace(/\.json$/,'');
    vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));
    const run=await runGraphDiscovery(expected.method,recordedForm(id),{source_id:expected.source_id,case_count:120,event_count:360}as LogQuerySchema);
    expect(run.raw).toBe(raw);expect(validGraphDiscovery(run.value)).toBe(true);expect(JSON.parse(run.report).response).toEqual(JSON.parse(raw));
    expect(new Set(run.tables.map(table=>table.id)).size).toBe(run.tables.length);
    if(expected.method==='transition_system')expect(run.tables.find(table=>table.id==='/result/transition_system/states')?.rows.length).toBe(expected.result.transition_system!.states.length);
  });
  it('retains all eight discovery variants, three conformance methods and every trace beyond preview limits',()=>{
    const result=value('footprints-all'),tables=graphDiscoveryTables(result,new URLSearchParams());
    expect(Object.keys(result.result.footprints!.discovery!)).toHaveLength(8);expect(Object.keys(result.result.footprints!.conformance!)).toHaveLength(4);
    const traces=tables.find(table=>table.id==='/result/footprints/discovery/trace_by_trace')!;expect(traces.rows).toHaveLength(120);expect(diagnosticCsv(traces)).toContain('資料-119');
    expect(tables.find(table=>table.id==='/result/footprints/conformance/trace_extensive')?.rows).toHaveLength(120);
  });
  it('retains native absent fields and the cached Alpha input alphabet',()=>{
    const dfg=value('footprints-model-dfg');expect(dfg.model_origin).toBe('selected_source_dfg');expect(dfg.model_footprint).not.toHaveProperty('dfg');expect(dfg.model_footprint).not.toHaveProperty('min_trace_length');
    const alpha=value('footprints-model-alpha');expect(alpha.analysis_source.origin).toBe('cached_alpha_projection');expect(alpha.source.case_count).toBe(120);
    expect(alpha.result.traces!.every(trace=>trace.events.every(event=>alpha.result.activities[event.activity].name.startsWith('活動-')))).toBe(true);
  });
  it('exports every native traversal occurrence with state and transition identity',()=>{
    const result=value('transition-backward-multiset'),model=result.result.transition_system!;
    const count=model.states.reduce((n,s)=>n+(s.incoming_occurrences?.length??0)+(s.outgoing_occurrences?.length??0),0)+model.transitions.reduce((n,t)=>n+(t.occurrences?.length??0),0);
    expect(graphDiscoveryTables(result,new URLSearchParams()).find(table=>table.id==='traversals')?.rows).toHaveLength(count);expect(count).toBeGreaterThan(360);
    expect(model.states.some(state=>state.payload.some(item=>item!==null&&typeof item==='object'&&'count'in item))).toBe(true);
  });
  it('retains the signed 64-bit window token in requests, native metadata and all derived reports',()=>{
    const form=defaultTransitionForm();form.window='9223372036854775807';expect(graphDiscoveryQuery('transition_system',form,'owned').get('transition_window')).toBe(form.window);
    const result=value('transition-wide-window');expect(String(nativeCell(result.result.transition_system!,'window'))).toBe(form.window);expect(nativeJson(result)).toContain(form.window);
    form.window='-9223372036854775808';expect(graphDiscoveryQuery('transition_system',form,'owned').get('transition_window')).toBe(form.window);
    form.window='9223372036854775808';expect(()=>graphDiscoveryQuery('transition_system',form,'owned')).toThrow(/64-bit/);
  });
  it('preserves explicit empty index overrides and original selectors/marking text',()=>{
    const f=defaultFootprintsForm();Object.assign(f,{overrideIndex:true,index:'',activity:'phase',caseId:'bucket',timestamp:'reverse_time',start:'none',sort:false,marking:'18446744073709551615,0'});
    const q=graphDiscoveryQuery('footprints',f,'owned');expect(q.has('footprints_index_attribute')).toBe(true);expect(q.get('footprints_index_attribute')).toBe('');expect(q.get('footprints_initial_marking')).toBe(f.marking);expect(q.get('footprints_sort_required')).toBe('false');expect(q.get('footprints_start_timestamp_attribute')).toBe('none');
    expect(value('footprints-selectors').result.footprints?.case_id_attribute).toBe('bucket');
  });
  it('rejects empty/incompatible discovery selections before issuing a request',()=>{
    const f=defaultFootprintsForm();f.discovery=[];expect(()=>graphDiscoveryQuery('footprints',f,'owned')).toThrow(/at least one/);f.discovery=['petri_reach_graph'];f.model='dfg';expect(()=>graphDiscoveryQuery('footprints',f,'owned')).toThrow(/Petri-capable/);
  });
  it('rejects stale source responses and a returned configuration that differs from the request',async()=>{
    const result=value('transition-forward-sequence');vi.stubGlobal('fetch',vi.fn(async()=>new Response(nativeJson(result))));
    await expect(runGraphDiscovery('transition_system',defaultTransitionForm(),{source_id:'other',case_count:120,event_count:360}as LogQuerySchema)).rejects.toThrow(/active source/);
    await expect(runGraphDiscovery('transition_system',{...defaultTransitionForm(),view:'set',includeData:true},{source_id:result.source_id,case_count:120,event_count:360}as LogQuerySchema)).rejects.toThrow(/configuration/);
  });
});
