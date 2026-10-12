import { afterEach,describe,expect,it,vi } from "vitest";
import { defaultDfgDiscoveryForm,runDfgDiscovery,validDfgDiscovery,type DfgDiscovery,type DfgNativeSwitch } from "../src/dfg-discovery";
import { parseNativeJson } from "../src/native-json";
import type { LogQuerySchema } from "../src/log-query";
import parameters from "./fixtures/dfg-request-parameters.json";
const fixtures=import.meta.glob<string>('./fixtures/dfg-discovery-*.json',{eager:true,query:'?raw',import:'default'});
const value=(name:string)=>parseNativeJson(fixtures[`./fixtures/dfg-discovery-${name}.json`])as DfgDiscovery;
afterEach(()=>vi.unstubAllGlobals());
describe('real native DFG discovery results',()=>{
  it.each(Object.entries(fixtures))('preserves complete real output and inspection tables %s',async(path,raw)=>{
    const expected=parseNativeJson(raw)as DfgDiscovery,name=path.slice(path.lastIndexOf('/')+1).replace(/^dfg-discovery-/,'').replace(/\.json$/,'');const q=(parameters as Record<string,Record<string,string>>)[name],form=defaultDfgDiscoveryForm();
    Object.assign(form,{statistical:q.dfg_statistical_variants?.split(',')??[],causal:q.dfg_causal_variants?.split(',')??[],window:q.dfg_window??'',keep:(q.dfg_keep_once_per_case??'native')as DfgNativeSwitch,activity:q.dfg_activity_attribute??'',timestamp:q.dfg_timestamp_attribute??'',start:q.dfg_start_timestamp_attribute??'',overrideStart:'dfg_start_timestamp_attribute'in q,aggregation:q.dfg_performance_aggregation??'native',business:(q.dfg_business_hours??'native')as DfgNativeSwitch,overrideSlots:'dfg_business_hour_slots'in q,slots:q.dfg_business_hour_slots??'',overrideAttributes:'dfg_case_attributes'in q,attributes:q.dfg_case_attributes??'',nodes:(q.dfg_return_node_attributes??'native')as DfgNativeSwitch});
    vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));const run=await runDfgDiscovery(form,{source_id:expected.source_id,case_count:120,event_count:720}as LogQuerySchema);expect(run.raw).toBe(raw);expect(validDfgDiscovery(run.value)).toBe(true);expect(JSON.parse(run.report).response).toEqual(JSON.parse(raw));expect(new Set(run.tables.map(t=>t.id)).size).toBe(run.tables.length);
    expect(run.tables.every(t=>t.records?.length===t.rows.length)).toBe(true);
  });
  it('retains named frequency aliases without redefining their native shared representation',()=>{
    const named=value('canonical-alias-selection');expect(named.statistical_variants).toEqual(['native','frequency']);expect(named.causal_variants).toEqual(['alpha','heuristic']);expect(named.result.dfg_variants!.statistical[0].frequency_dfg).toEqual(named.result.dfg_variants!.statistical[1].frequency_dfg);
  });
  it('preserves scalar, vector and complete summary performance values and every sample',()=>{
    const raw=value('performance-raw_values').result.dfg_variants!.statistical[0].performance_dfg!;expect(raw.every(e=>Array.isArray(e.value)&&e.raw_values.length===e.summary.count)).toBe(true);expect(raw.reduce((n,e)=>n+e.raw_values.length,0)).toBe(600);
    expect(value('performance-all').result.dfg_variants!.statistical[0].performance_dfg!.every(e=>typeof e.value==='object'&&!Array.isArray(e.value))).toBe(true);
    expect(value('performance-stdev').result.dfg_variants!.statistical[0].performance_dfg!.every(e=>typeof e.value==='number'&&e.value===e.summary.stdev)).toBe(true);
    expect(value('window-full-width').result.dfg_variants!.statistical[0].frequency_dfg).toEqual([]);
  });
  it('retains signed causal strengths, custom Unicode labels and native empty schedules',()=>{
    expect(value('causal-heuristic').result.dfg_variants!.causal[0].relations.some(r=>r.value<0)).toBe(true);expect(value('custom-all').result.activities.some(a=>a.name.includes('☃🚀'))).toBe(true);expect(value('empty-business-hours').result.dfg_variants!.statistical[0].performance_dfg!.every(e=>e.raw_values.every(v=>v===0))).toBe(true);
  });
  it('rejects a stale source or a mismatched returned selection',async()=>{
    const result=value('stat-frequency'),raw=fixtures['./fixtures/dfg-discovery-stat-frequency.json'];vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));const form=defaultDfgDiscoveryForm();await expect(runDfgDiscovery(form,{source_id:'stale',case_count:120,event_count:720}as LogQuerySchema)).rejects.toThrow(/source/);form.statistical=['clean'];await expect(runDfgDiscovery(form,{source_id:result.source_id,case_count:120,event_count:720}as LogQuerySchema)).rejects.toThrow(/variants/);
  });
});
