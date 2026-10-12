import { afterEach,describe,expect,it,vi } from "vitest";
import { defaultTimingForm,runTimingDiscovery,timingQuery,validTimingDiscovery,type TimingDiscovery } from "../src/timing-discovery";
import { parseNativeJson } from "../src/native-json";
import { diagnosticCsv } from "../src/model-diagnostics";
import type { LogQuerySchema } from "../src/log-query";
import parameters from "./fixtures/timing-request-parameters.json";
const fixtures=import.meta.glob<string>('./fixtures/timing-discovery-*.json',{eager:true,query:'?raw',import:'default'});
const value=(name:string)=>parseNativeJson(fixtures[`./fixtures/timing-discovery-${name}.json`]) as TimingDiscovery;
afterEach(()=>vi.unstubAllGlobals());
describe('native timing discovery',()=>{
  it.each(Object.entries(fixtures))('preserves complete real output and active variant %s',async(path,raw)=>{
    const expected=parseNativeJson(raw) as TimingDiscovery,name=path.slice(path.lastIndexOf('/')+1).replace(/^timing-discovery-/,'').replace(/\.json$/,'');
    const q=(parameters as Record<string,Record<string,string>>)[name],form=defaultTimingForm(expected.method);
    Object.assign(form,{variant:q.spectrum_variant??q.batch_variant,pattern:q.spectrum_activities??'',sample:q.spectrum_sample_size??'',sort:q.spectrum_sort_log!=='false',activity:q.spectrum_activity_key??q.batch_activity_attribute??'',timestamp:q.spectrum_timestamp_key??q.batch_timestamp_attribute??'',caseId:q.spectrum_case_id_key??q.batch_case_id_attribute??'',resource:q.batch_resource_attribute??'',overrideStart:'batch_start_timestamp_attribute'in q,start:q.batch_start_timestamp_attribute??'',eventId:q.batch_event_id_attribute??'',merge:q.batch_merge_distance_seconds??'',minimum:q.batch_min_size??''});
    vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));
    const run=await runTimingDiscovery(expected.method,form,{source_id:expected.source_id,case_count:80,event_count:320}as LogQuerySchema);
    expect(run.raw).toBe(raw);expect(validTimingDiscovery(run.value)).toBe(true);expect(JSON.parse(run.report).response).toEqual(JSON.parse(raw));expect(new Set(run.tables.map(t=>t.id)).size).toBe(run.tables.length);
    expect(run.tables.some(t=>diagnosticCsv(t).includes(expected.method==='spectrum'?'observations':'groups'))).toBe(true);
  });
  it('retains every disconnected observation and its native pattern offset/labels',()=>{
    const connected=value('spectrum-log'),disconnected=value('spectrum-log_disconnected');
    expect(connected.result.performance_spectrum?.observations).toHaveLength(80);expect(disconnected.result.performance_spectrum?.observations).toHaveLength(160);
    expect(disconnected.result.performance_spectrum?.observations.some(o=>o.pattern_offset>0)).toBe(true);
  });
  it('preserves sample-zero behavior and full unsigned count text',()=>{
    const form=defaultTimingForm('spectrum');form.pattern='A,D';form.sample='18446744073709551615';expect(timingQuery('spectrum',form,'owned').get('spectrum_sample_size')).toBe(form.sample);
    const observations=value('spectrum-zero-sample').result.performance_spectrum?.observations;
    expect(observations).toHaveLength(2);
    expect(observations?.map(o=>o.case_id)).toEqual(['資料-0','資料-79']);
    form.sample='18446744073709551616';expect(()=>timingQuery('spectrum',form,'owned')).toThrow(/64-bit/);
  });
  it('preserves explicit start fallback and rejects an event-id selection on the log adapter',()=>{
    const form=defaultTimingForm('batches');form.overrideStart=true;expect(timingQuery('batches',form,'owned').get('batch_start_timestamp_attribute')).toBe('');
    form.eventId='event_id';expect(()=>timingQuery('batches',form,'owned')).toThrow(/table|pandas/);
    form.variant='pandas';expect(timingQuery('batches',form,'owned').get('batch_event_id_attribute')).toBe('event_id');
    expect(value('batches-empty').result.batch_discovery?.groups).toHaveLength(0);
  });
  it('retains fractional epoch seconds and nonzero durations in spectrum and batch output',()=>{
    const observation=value('spectrum-log').result.performance_spectrum!.observations[0];
    expect(observation.instants).toEqual([1704103200.125,1704103260.375,1704103380.875]);
    expect(observation.instants[2]-observation.instants[0]).toBe(180.75);
    const batch=value('batches-log').result.batch_discovery!.groups[0].batches[0];
    expect(batch.complete-batch.start).toBe(300);
    expect(batch.events.every(event=>event.complete-event.start===300)).toBe(true);
  });
  it('rejects a stale source and a mismatched returned timing variant',async()=>{
    const result=value('spectrum-log'),raw=fixtures['./fixtures/timing-discovery-spectrum-log.json'];vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));
    const form=defaultTimingForm('spectrum');form.pattern='A,B,D';await expect(runTimingDiscovery('spectrum',form,{source_id:'other',case_count:80,event_count:320}as LogQuerySchema)).rejects.toThrow(/source/);
    form.variant='dataframe';await expect(runTimingDiscovery('spectrum',form,{source_id:result.source_id,case_count:80,event_count:320}as LogQuerySchema)).rejects.toThrow(/variant/);
  });
});
