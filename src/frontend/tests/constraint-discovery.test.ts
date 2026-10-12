import { afterEach, describe, expect, it, vi } from "vitest";
import { constraintQuery, constraintTables, defaultConstraintForm, runConstraintDiscovery, validConstraintDiscovery, type ConstraintDiscovery } from "../src/constraint-discovery";
import { diagnosticCsv } from "../src/model-diagnostics";
import { nativeJson, parseNativeJson } from "../src/native-json";
import type { LogQuerySchema } from "../src/log-query";
const fixtures=import.meta.glob<string>("./fixtures/constraint-discovery-*.json",{eager:true,query:"?raw",import:"default"});
const value=(name:string)=>parseNativeJson(fixtures[`./fixtures/constraint-discovery-${name}.json`]) as ConstraintDiscovery;
afterEach(()=>vi.unstubAllGlobals());
describe("source-bound native constraint discovery",()=>{
  it.each(Object.entries(fixtures))("retains the complete actual native fixture %s",async(_,raw)=>{
    const expected=parseNativeJson(raw) as ConstraintDiscovery;
    const schema={source_id:expected.source_id,case_count:60,event_count:180} as LogQuerySchema;
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw)));
    const run=await runConstraintDiscovery(expected.method,defaultConstraintForm(expected.method),schema);
    expect(run.raw).toBe(raw);expect(validConstraintDiscovery(run.value)).toBe(true);
    expect(JSON.parse(run.report).response).toEqual(JSON.parse(raw));
    const cases=run.tables.find(table=>table.id==='/conformance')!;
    expect(cases.rows.length).toBe(60);expect(cases.records?.[59]).toEqual((expected.method==='declare'?expected.result.declare_conformance!:expected.result.log_skeleton_conformance!)[59]);
    expect(diagnosticCsv(cases)).toContain('資料-59');
  });
  it("distinguishes omitted and explicitly empty template/activity filters",()=>{
    const form=defaultConstraintForm('declare');expect(constraintQuery('declare',form,'owned').has('declare_allowed_templates')).toBe(false);
    form.selection='selected';form.templates=[];form.activitySelection='selected';
    const q=constraintQuery('declare',form,'owned');expect(q.has('declare_allowed_templates')).toBe(true);expect(q.get('declare_allowed_templates')).toBe('');expect(q.has('declare_considered_activities')).toBe(true);expect(q.get('declare_considered_activities')).toBe('');
    for(const name of ['declare-empty-templates','declare-empty-activities'])expect(Object.values(value(name).result.declare!).every(rules=>rules!.length===0)).toBe(true);
  });
  it("preserves the discovered skeleton when an explicit empty selection checks no families",()=>{
    const empty=value('skeleton-empty'),all=value('skeleton-default');expect(empty.result.log_skeleton).toEqual(all.result.log_skeleton);
    expect(empty.result.log_skeleton_conformance!.every(trace=>trace.no_constr_total===0&&trace.deviations.length===0)).toBe(true);
    const form=defaultConstraintForm('log_skeleton');form.selection='selected';form.templates=[];expect(constraintQuery('log_skeleton',form,'owned').get('log_skeleton_constraints')).toBe('');
  });
  it("retains a full-width Declare seed and explicit zero thresholds",()=>{
    const form=defaultConstraintForm('declare');Object.assign(form,{generateLog:true,seed:'18446744073709551615',support:'0',confidence:'0',multiplier:'0.2'});
    const q=constraintQuery('declare',form,'owned');expect(q.get('declare_playout_seed')).toBe('18446744073709551615');expect(q.get('declare_min_support_ratio')).toBe('0');expect(q.get('declare_min_confidence_ratio')).toBe('0');expect(q.get('declare_auto_selection_multiplier')).toBe('0.2');
    form.seed='18446744073709551616';expect(()=>constraintQuery('declare',form,'owned')).toThrow(/64-bit/);
    form.seed='42';form.support='NaN';expect(()=>constraintQuery('declare',form,'owned')).toThrow(/finite/);
  });
  it("keeps prefix-safe playout and its actual incomplete-obligation diagnostics together",()=>{
    const result=value('declare-playout');expect(result.playout_result?.trace_count).toBe(7);expect(result.playout_result?.event_count).toBe(25);
    expect(result.playout_result?.declare_conformance!.filter(trace=>trace.is_fit).length).toBe(2);
    expect(result.playout_result?.declare_conformance!.some(trace=>trace.deviations.length>0)).toBe(true);
    expect(constraintTables(result,new URLSearchParams()).find(table=>table.id==='/playout/declare_conformance')?.rows.length).toBe(7);
    expect(result.playout_xes).toContain('<log');
  });
  it("preserves typed diagnostics, selectors and partial-output errors",()=>{
    const custom=value('skeleton-custom');expect(custom.activity_key).toBe('phase');expect(custom.conformance_activity_key).toBe('phase');expect(custom.result.activities.some(a=>a.name==='活動-A')).toBe(true);
    const diagnostics=value('declare-default').diagnostics!;expect(diagnostics.columns).toEqual(['case_id','no_dev_total','no_constr_total','dev_fitness']);
    expect(nativeJson(diagnostics)).toContain('typed_value');expect(nativeJson(diagnostics)).toContain('first_class_source_key');
    const error=value('skeleton-diagnostics-error');expect(error.diagnostics).toBeNull();expect(error.diagnostics_error).toContain('missing or null');expect(error.result.log_skeleton).toBeDefined();
    expect(value('declare-playout-error').playout_error).toContain('event budget');
    expect(value('declare-mismatch').result.declare_conformance!.some(trace=>!trace.is_fit)).toBe(true);
  });
  it("exposes the source and generated-log summaries through distinct export selectors",()=>{
    const result=value('declare-playout');const tables=constraintTables(result,new URLSearchParams());
    expect(new Set(tables.map(table=>table.id)).size).toBe(tables.length);
    expect(tables.find(table=>table.id==='summary')?.columns).toEqual(['field','type','value']);
    expect(tables.find(table=>table.id==='playout_summary')?.rows[0][0]).toBe('playout');
    expect(tables.find(table=>table.id==='playout_summary')?.rows[0][2]).toEqual(result.playout_result);
    expect(validConstraintDiscovery({...result,playout_result:{...result.playout_result,declare_conformance:undefined}})).toBe(false);
  });
  it("rejects a stale returned source and a mismatched optional playout contract",async()=>{
    const result=value('declare-default');vi.stubGlobal('fetch',vi.fn(async()=>new Response(nativeJson(result))));
    await expect(runConstraintDiscovery('declare',defaultConstraintForm('declare'),{source_id:'stale',case_count:60,event_count:180} as LogQuerySchema)).rejects.toThrow(/does not match/);
    expect(validConstraintDiscovery({...result,playout_xes:'<log/>'})).toBe(false);
    expect(validConstraintDiscovery({...result,source:{case_count:5,event_count:180}})).toBe(false);
  });
  it("rejects unknown families and invalid thresholds before issuing a request",()=>{
    const form=defaultConstraintForm('log_skeleton');form.selection='selected';form.templates=['unknown'];expect(()=>constraintQuery('log_skeleton',form,'owned')).toThrow(/supported/);
    form.templates=[];form.noise='-0.1';expect(()=>constraintQuery('log_skeleton',form,'owned')).toThrow(/\[0,1\]/);
    form.noise='0';expect(()=>constraintQuery('log_skeleton',form,'')).toThrow(/Refresh/);
  });
});
