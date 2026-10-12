import { afterEach,describe,expect,it,vi } from "vitest";
import { behaviorQuery,defaultBehaviorForm,runBehaviorAnalysis,validBehaviorAnalysis,type BehaviorAnalysis,type NativeSwitch } from "../src/behavior-analysis";
import { nativeJson,parseNativeJson } from "../src/native-json";
import type { LogQuerySchema } from "../src/log-query";
import parameters from "./fixtures/behavior-request-parameters.json";
const fixtures=import.meta.glob<string>('./fixtures/behavior-analysis-*.json',{eager:true,query:'?raw',import:'default'});
const value=(name:string)=>parseNativeJson(fixtures[`./fixtures/behavior-analysis-${name}.json`]) as BehaviorAnalysis;
afterEach(()=>vi.unstubAllGlobals());
describe('real native behavior results',()=>{
  it.each(Object.entries(fixtures))('preserves complete native output and adapter %s',async(path,raw)=>{
    const expected=parseNativeJson(raw) as BehaviorAnalysis,name=path.slice(path.lastIndexOf('/')+1).replace(/^behavior-analysis-/,'').replace(/\.json$/,'');const q=(parameters as Record<string,Record<string,string>>)[name],form=defaultBehaviorForm(expected.method);
    Object.assign(form,{variant:q.variant,adapter:q.adapter??'dataframe',activity:q.temporal_activity_attribute??q.minimum_self_distance_activity_key??q.correlation_activity_attribute??'',caseId:q.temporal_case_id_attribute??q.minimum_self_distance_case_id_key??q.correlation_case_id_attribute??'',completion:q.temporal_source_timestamp_attribute??q.correlation_timestamp_attribute??'',start:q.temporal_target_timestamp_attribute??q.correlation_start_timestamp_attribute??'',sample:q.correlation_sample_size??'',exact:(q.correlation_exact_time_matching??'native')as NativeSwitch,strict:(q.temporal_strict_attributes??'native')as NativeSwitch,business:(q.temporal_business_hours??'native')as NativeSwitch,overrideSlots:'temporal_business_hour_slots'in q,slots:q.temporal_business_hour_slots??'',overrideStart:'correlation_start_timestamp_attribute'in q,zeta:q.temporal_zeta??'',profile:q.temporal_profile_json??'[]'});
    vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));const run=await runBehaviorAnalysis(expected.method,form,{source_id:expected.source_id,case_count:96,event_count:480}as LogQuerySchema);
    expect(run.raw).toBe(raw);expect(validBehaviorAnalysis(run.value)).toBe(true);expect(JSON.parse(run.report).response).toEqual(JSON.parse(raw));expect(new Set(run.tables.map(t=>t.id)).size).toBe(run.tables.length);expect(run.form).toEqual(form);
  });
  it('preserves native regrouping, empty cases and all 2400 deviations independently of original source cases',()=>{
    const grouped=value('conformance-custom-grouping');expect(grouped.source.case_count).toBe(96);expect(grouped.conformance?.trace_count).toBe(4);expect(grouped.conformance?.deviation_count).toBe(2400);expect(grouped.conformance?.traces.flat()).toHaveLength(2400);
    expect(value('conformance-empty-profile').conformance?.traces).toEqual(Array.from({length:96},()=>[]));expect(value('conformance-positive-infinity').conformance?.deviation_count).toBe(0);
  });
  it('hands off exact native profile labels and moments through unrestricted JSON',()=>{
    const result=value('temporal-custom-log'),form=defaultBehaviorForm('temporal_conformance');form.profile=nativeJson(result.profile,2);const q=behaviorQuery('temporal_conformance',form,result.source_id);expect(parseNativeJson(q.get('temporal_profile_json')!)).toEqual(result.profile);expect(form.profile).toContain('a:b,c->quote');expect(form.profile).toContain('☃🚀');
  });
  it('retains a native empty self-distance result and the selected table adapter',()=>{
    expect(value('self-empty').result.minimum_self_distances).toEqual([]);expect(value('self-polars').result.minimum_self_distance_variant).toBe('polars');expect(value('self-pandas').adapter).toBe('dataframe');
  });
  it('rejects a stale source or mismatched native input adapter',async()=>{
    const result=value('correlation-classic-dataframe'),raw=fixtures['./fixtures/behavior-analysis-correlation-classic-dataframe.json'];vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));const form=defaultBehaviorForm('correlation');await expect(runBehaviorAnalysis('correlation',form,{source_id:'other',case_count:96,event_count:480}as LogQuerySchema)).rejects.toThrow(/source/);
    form.adapter='log';await expect(runBehaviorAnalysis('correlation',form,{source_id:result.source_id,case_count:96,event_count:480}as LogQuerySchema)).rejects.toThrow(/adapter/);
  });
});
