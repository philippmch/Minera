import {afterEach,describe,expect,it,vi} from 'vitest';
import classic from './fixtures/workflow-discovery-alpha-classic.json?raw';
import plus from './fixtures/workflow-discovery-alpha-plus.json?raw';
import phase from './fixtures/workflow-discovery-alpha-phase.json?raw';
import heuristic from './fixtures/workflow-discovery-heuristic-classic.json?raw';
import plusplus from './fixtures/workflow-discovery-heuristic-plusplus.json?raw';
import performance from './fixtures/workflow-discovery-heuristic-performance.json?raw';
import connected from './fixtures/workflow-discovery-heuristic-connected.json?raw';
import structured from './fixtures/workflow-discovery-structured-alpha.json?raw';
import {defaultWorkflowForm,workflowQuery,validWorkflowDiscovery,workflowTables,workflowNet,runWorkflowDiscovery,workflowModelFile,type WorkflowDiscovery,type WorkflowVariant} from '../src/workflow-discovery';
import {parseNativeJson,nativeJson} from '../src/native-json';
import {ocelPreparedCsv} from '../src/ocel-preparation';
import {MINER_GROUPS} from '../src/miners';
import type {LogQuerySchema} from '../src/log-query';
const parse=(raw=classic)=>parseNativeJson(raw)as WorkflowDiscovery;
const schema=(value=parse()):LogQuerySchema=>({family:'LOG_QUERY_SCHEMA',source_id:value.source_id,case_count:value.source.case_count,event_count:value.source.event_count,columns:[],table_name:'dataframe',event_index_column:'index',sqlite_available:true,duckdb_configured:false});
afterEach(()=>vi.unstubAllGlobals());
describe('actual native workflow discovery variants',()=>{
 it.each([classic,plus,phase,heuristic,plusplus,performance,connected,structured])('accepts complete native variant/model/format output',raw=>expect(validWorkflowDiscovery(parse(raw))).toBe(true));
 it('exposes both genuine Alpha and Heuristic variant choices',()=>{
  expect(MINER_GROUPS.find(group=>group.id==='alpha')!.variants.map(v=>v.id)).toEqual(['classic','plus']);
  expect(MINER_GROUPS.find(group=>group.id==='heuristic')!.variants.map(v=>v.id)).toEqual(['plusplus','classic']);
 });
 it('keeps Classic and Alpha+ native topology and explicit marking identities distinct',()=>{
  expect(workflowNet(parse())).not.toEqual(workflowNet(parse(plus)));
  expect(parse().alpha_discovery).toBeNull();expect(parse(plus).alpha_discovery?.family).toBe('ALPHA_PLUS');
  const tables=workflowTables(parse(plus));expect(tables.alpha_places.count).toBe((parse(plus).alpha_discovery!.places as unknown[]).length);expect(tables.alpha_detached_initial_marking.count).toBe(0);
 });
 it('keeps Unicode selector labels and model IDs without remapping the response',()=>{
  const value=parse(phase),net=workflowNet(value);expect(net.transitions.filter(t=>t.activity!==null).every(t=>value.result.activities[t.activity!].name.startsWith('Phase/'))).toBe(true);
  expect(nativeJson(value)).toContain('東京');
 });
 it('exposes every native heuristic statistic, bundle, metadata object and matrix',async()=>{
  const value=parse(heuristic),tables=workflowTables(value),h=value.result.heuristics_net!;
  for(const key of Object.keys(h)){if(Array.isArray(h[key as keyof typeof h])||typeof h[key as keyof typeof h]==='object')expect(tables['heuristic_'+key]).toBeDefined();}
  expect(tables.heuristic_dependency_measures.count).toBe(h.dependency_measures!.length);
  const csv=await ocelPreparedCsv(tables.heuristic_relation_statistics).text();expect(csv).toContain('eventually_follows_frequency');expect(csv).toContain('raw_frequency');
 });
 it('retains actual performance aggregation and explicit empty business-hour output',()=>{
  const value=parse(performance);expect(value.result.heuristics_net).toMatchObject({variant:'plusplus',decoration:'performance',performance_aggregation:'all'});
  expect(workflowTables(value).heuristic_summary.count).toBe(Object.keys(value.result.heuristics_net!).length);
 });
 it('serializes all heuristic controls with their native names and selected variant',()=>{
  const f=defaultWorkflowForm('plusplus');Object.assign(f,{activity:'phase',timestamp:'completed',start:'started',lifecycle:'instance',dependency:'-0.1',and:'0.8',frequency:'3',minimumActivity:'2',decoration:'performance',aggregation:'all',businessHours:true,slots:'none',noise:'0.2',loop:'0.4',once:true,paths:'A -> B',connect:'true',decorationSelector:'performance',decorationValue:'null',playoutTraces:'5',playoutSteps:'30',playoutSeed:'7'});
  const q=workflowQuery('heuristic',f,'source');expect(q.get('heuristic_variant')).toBe('plusplus');expect(q.get('heuristic_activity_attribute')).toBe('phase');expect(q.get('dependency_threshold')).toBe('-0.1');expect(q.get('heuristic_business_hour_slots')).toBe('none');expect(q.get('heuristic_plusplus_decoration_value')).toBe('null');expect(q.get('heuristic_connect_all')).toBe('true');expect(q.get('discovery_playout_traces')).toBe('5');
  expect(Array.from(q.keys()).every(key=>key==='family'||key==='source_id'||key==='dependency_threshold'||key==='and_threshold'||key==='min_edge_frequency'||key.startsWith('heuristic_')||key.startsWith('discovery_playout_'))).toBe(true);
 });
 it('serializes Alpha choices without unrelated miner parameters',()=>{
  const f=defaultWorkflowForm('plus');f.activity='phase';f.remove=true;f.maximumRows='0';const q=workflowQuery('alpha',f,'source');expect(Object.fromEntries(q)).toEqual({alpha_variant:'plus',alpha_activity_attribute:'phase',alpha_remove_unconnected:'true',alpha_max_rows_to_materialize:'0',family:'alpha',source_id:'source'});
 });
 it('rejects invalid integer/JSON controls and crossed family selectors',()=>{
  const f=defaultWorkflowForm('plus');expect(()=>workflowQuery('heuristic',f,'source')).toThrow(/variant/);f.variant='classic';f.frequency='1.5';expect(()=>workflowQuery('heuristic',f,'source')).toThrow(/integer/);f.frequency='';f.decorationValue='{';expect(()=>workflowQuery('heuristic',f,'source')).toThrow(/JSON/);expect(()=>workflowQuery('alpha',f,'')).toThrow(/source/);
 });
 it('rejects a response with another variant or source while retaining the original wire report',async()=>{
  vi.stubGlobal('fetch',vi.fn(async()=>new Response(plus)));const f=defaultWorkflowForm('plus');const run=await runWorkflowDiscovery('alpha',f,schema(parse(plus)));expect(run.raw).toBe(plus);expect(JSON.parse(run.report).response).toEqual(JSON.parse(plus));
  await expect(runWorkflowDiscovery('alpha',defaultWorkflowForm(),schema())).rejects.toThrow(/variant/);await expect(runWorkflowDiscovery('alpha',f,{...schema(),source_id:'old'})).rejects.toThrow(/source/);
 });
 it('retains original numeric tokens in marking CSV and nested JSON',async()=>{
  const raw=plus.replace('"initial":0','"initial":9007199254740993'),value=parse(raw);expect(raw).not.toBe(plus);expect(await ocelPreparedCsv(workflowTables(value).alpha_places).text()).toContain('9007199254740993');expect(nativeJson(value)).toContain('9007199254740993');
 });
 it('preserves representable PNML/BPMN/PTML artifacts and native conversion failures',async()=>{
  vi.stubGlobal('fetch',vi.fn(async()=>new Response(structured)));const value=parse(structured),run=await runWorkflowDiscovery('alpha',defaultWorkflowForm(),schema(value));
  for(const format of ['pnml','bpmn','ptml']as const)expect(await workflowModelFile(run,format).text()).toBe(value[format]);
  expect(parse(plus).ptml).toBeNull();expect(parse(plus).ptml_error).toMatch(/source-to-sink/);expect(parse(plus).playout_xes).toContain('<log');
 });
 it('retains empty native tables and backend failures instead of inventing a model',async()=>{
  const value=parse(plus);value.pnml=null;value.pnml_error='detached marking';expect(validWorkflowDiscovery(value)).toBe(true);expect(workflowTables(value).alpha_detached_final_marking.count).toBe(0);
  vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"error":"unknown workflow discovery parameter"}',{status:400})));await expect(runWorkflowDiscovery('alpha',defaultWorkflowForm('classic' as WorkflowVariant),schema())).rejects.toThrow(/unknown/);
 });
});
