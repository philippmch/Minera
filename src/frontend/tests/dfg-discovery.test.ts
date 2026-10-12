import { describe,expect,it } from "vitest";
import { DFG_AGGREGATIONS,DFG_STATISTICAL,dfgDiscoveryQuery,defaultDfgDiscoveryForm,validDfgDiscovery } from "../src/dfg-discovery";
describe('native DFG discovery controls',()=>{
  it('preserves named aliases, canonical order and distinct statistical/causal choices',()=>{
    const form=defaultDfgDiscoveryForm();form.statistical=['clean_time','frequency','native','frequency','performance_greedy'];form.causal=['heuristic','alpha','heuristic'];const q=dfgDiscoveryQuery(form,'source');expect(q.get('dfg_statistical_variants')).toBe('native,frequency,performance_greedy,clean_time');expect(q.get('dfg_causal_variants')).toBe('alpha,heuristic');expect(DFG_STATISTICAL).toHaveLength(10);
  });
  it('preserves zero and complete unsigned window text without numeric conversion',()=>{
    const form=defaultDfgDiscoveryForm();for(const window of ['0','18446744073709551615']){form.window=window;expect(dfgDiscoveryQuery(form,'source').get('dfg_window')).toBe(window);}form.window='18446744073709551616';expect(()=>dfgDiscoveryQuery(form,'source')).toThrow(/64-bit/);
  });
  it.each(DFG_AGGREGATIONS)('retains native performance aggregation %s',aggregation=>{
    const form=defaultDfgDiscoveryForm();form.aggregation=aggregation;expect(dfgDiscoveryQuery(form,'source').get('dfg_performance_aggregation')).toBe(aggregation);
  });
  it('preserves explicit blank start, calendar and case-attribute selections',()=>{
    const form=defaultDfgDiscoveryForm();Object.assign(form,{overrideStart:true,overrideSlots:true,overrideAttributes:true});const q=dfgDiscoveryQuery(form,'source');expect(q.get('dfg_start_timestamp_attribute')).toBe('');expect(q.get('dfg_business_hour_slots')).toBe('');expect(q.get('dfg_case_attributes')).toBe('');expect(q.has('dfg_return_node_attributes')).toBe(false);
  });
  it('retains independent timestamp/activity and native boolean settings',()=>{
    const form=defaultDfgDiscoveryForm();Object.assign(form,{activity:'phase',timestamp:'complete',start:'started',keep:'true',business:'false',nodes:'true'});const q=dfgDiscoveryQuery(form,'source');expect(q.get('dfg_activity_attribute')).toBe('phase');expect(q.get('dfg_timestamp_attribute')).toBe('complete');expect(q.get('dfg_start_timestamp_attribute')).toBe('started');expect(q.get('dfg_keep_once_per_case')).toBe('true');expect(q.get('dfg_return_node_attributes')).toBe('true');
  });
  it('rejects empty or unsupported selections and a missing source',()=>{
    const form=defaultDfgDiscoveryForm();form.statistical=[];expect(()=>dfgDiscoveryQuery(form,'source')).toThrow(/at least one/);form.causal=['unsupported'];expect(()=>dfgDiscoveryQuery(form,'source')).toThrow(/supported/);expect(()=>dfgDiscoveryQuery(defaultDfgDiscoveryForm(),'')).toThrow(/source/);expect(validDfgDiscovery({family:'DFG_DISCOVERY'})).toBe(false);
  });
});
