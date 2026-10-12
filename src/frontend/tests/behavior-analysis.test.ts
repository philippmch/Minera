import { describe,expect,it } from "vitest";
import { BEHAVIOR_ADAPTERS,behaviorQuery,defaultBehaviorForm,validBehaviorAnalysis,validProfile,type BehaviorFamily } from "../src/behavior-analysis";
import { parseNativeJson } from "../src/native-json";

describe('native behavior analysis controls',()=>{
  it.each(BEHAVIOR_ADAPTERS)('preserves temporal input adapter %s',variant=>{
    const form=defaultBehaviorForm('temporal_profile');form.variant=variant;
    expect(behaviorQuery('temporal_profile',form,'source').get('variant')).toBe(variant);
    expect(behaviorQuery('temporal_profile',form,'source').has('temporal_strict_attributes')).toBe(false);
  });
  it('preserves explicit empty business slots and independent start/completion selectors',()=>{
    const form=defaultBehaviorForm('temporal_profile');Object.assign(form,{overrideSlots:true,business:'true',completion:'done',start:'begun'});
    const q=behaviorQuery('temporal_profile',form,'source');expect(q.get('temporal_business_hour_slots')).toBe('');expect(q.get('temporal_source_timestamp_attribute')).toBe('done');expect(q.get('temporal_target_timestamp_attribute')).toBe('begun');
  });
  it('preserves arbitrary profile labels, exact moment tokens and empty profiles',()=>{
    const form=defaultBehaviorForm('temporal_conformance');form.profile='[["a:b,c->quote\\\"☃🚀","end",0.12345678901234567,1.0000000000000002]]';
    expect(validProfile(parseNativeJson(form.profile))).toBe(true);expect(behaviorQuery('temporal_conformance',form,'source').get('temporal_profile_json')).toBe(form.profile);
    form.profile='[]';expect(behaviorQuery('temporal_conformance',form,'source').get('temporal_profile_json')).toBe('[]');
  });
  it.each(['-2','inf','-Infinity'])('retains the native zeta convention %s',zeta=>{
    const form=defaultBehaviorForm('temporal_conformance');form.zeta=zeta;expect(behaviorQuery('temporal_conformance',form,'source').get('temporal_zeta')).toBe(zeta);
  });
  it('rejects invalid moments and NaN thresholds before a request',()=>{
    const form=defaultBehaviorForm('temporal_conformance');form.profile='[["A","B",-1,2]]';expect(()=>behaviorQuery('temporal_conformance',form,'source')).toThrow(/non-negative/);
    form.profile='[]';form.zeta='NaN';expect(()=>behaviorQuery('temporal_conformance',form,'source')).toThrow(/NaN/);
  });
  it('retains a complete unsigned correlation sample and explicit start fallback',()=>{
    const form=defaultBehaviorForm('correlation');Object.assign(form,{adapter:'stream',sample:'18446744073709551615',overrideStart:true});
    const q=behaviorQuery('correlation',form,'source');expect(q.get('correlation_sample_size')).toBe(form.sample);expect(q.get('correlation_start_timestamp_attribute')).toBe('');expect(q.get('adapter')).toBe('stream');
    form.sample='0';expect(()=>behaviorQuery('correlation',form,'source')).toThrow(/positive/);form.sample='18446744073709551616';expect(()=>behaviorQuery('correlation',form,'source')).toThrow(/64-bit/);
  });
  it('preserves witness discovery selectors and rejects an unsupported variant',()=>{
    const form=defaultBehaviorForm('minimum_self_distance');Object.assign(form,{variant:'polars',activity:'phase',caseId:'bucket'});const q=behaviorQuery('minimum_self_distance',form,'source');expect(q.get('minimum_self_distance_activity_key')).toBe('phase');expect(q.get('minimum_self_distance_case_id_key')).toBe('bucket');
    form.variant='unsupported';expect(()=>behaviorQuery('minimum_self_distance',form,'source')).toThrow(/variant/);
  });
  it('rejects unsupported response families without following prototype properties',()=>{
    expect(validBehaviorAnalysis({family:'BEHAVIOR_ANALYSIS',method:'toString'})).toBe(false);
    for(const family of ['temporal_profile','temporal_conformance','minimum_self_distance','correlation']as BehaviorFamily[])expect(()=>behaviorQuery(family,defaultBehaviorForm(family),'')).toThrow(/source/);
  });
});
