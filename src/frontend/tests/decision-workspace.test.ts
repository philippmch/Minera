import {afterEach,describe,expect,it,vi} from 'vitest';
import guarded from './fixtures/decision-workspace-guarded.json?raw';
import tables from './fixtures/decision-workspace-tables.json?raw';
import lookback from './fixtures/decision-workspace-lookback.json?raw';
import depth from './fixtures/decision-workspace-depth.json?raw';
import noDiagnostics from './fixtures/decision-workspace-no-diagnostics.json?raw';
import {defaultDecisionForm,decisionQuery,decisionTables,decisionPetriModel,embeddedDecisionRun,runDecisionWorkspace,validDecisionWorkspace,type DecisionWorkspace} from '../src/decision-workspace';
import {parseNativeJson,nativeJson} from '../src/native-json';
import {type LogQuerySchema} from '../src/log-query';
import {ocelPreparedCsv} from '../src/ocel-preparation';
const parse=(raw=guarded)=>parseNativeJson(raw) as DecisionWorkspace;
const schema=():LogQuerySchema=>({family:'LOG_QUERY_SCHEMA',source_id:parse().source_id,case_count:60,event_count:180,columns:[],table_name:'dataframe',event_index_column:'index',sqlite_available:true,duckdb_configured:false});
afterEach(()=>vi.unstubAllGlobals());
describe('native encoded decisions and guarded model workspace',()=>{
 it.each([guarded,tables,lookback,depth,noDiagnostics])('accepts actual complete native envelopes',raw=>expect(validDecisionWorkspace(parse(raw))).toBe(true));
 it('rejects source/marking omissions and invalid native rows',()=>{
  let s=parse();s.underlying_pnml='';expect(validDecisionWorkspace(s)).toBe(false);s=parse();s.source_id='';expect(validDecisionWorkspace(s)).toBe(false);s=parse();s.result.family='DECISION_TABLES';expect(validDecisionWorkspace(s)).toBe(false);const t=parse(tables);if(t.result.family==='DECISION_TABLES')t.result.tables[0].rows[0].target=999;expect(validDecisionWorkspace(t)).toBe(false);
 });
 it('keeps native defaults omitted and binds source/model/envelope explicitly',()=>{
  const f=defaultDecisionForm('alpha'),q=decisionQuery(f,schema());expect(Object.fromEntries(q)).toEqual({miner:'alpha',source_id:schema().source_id,include_workspace:'true'});
 });
 it('serializes every native tree, context and diagnostics option for guarded output',()=>{
  const f=defaultDecisionForm('alpha');Object.assign(f,{activity:'phase',attributes:'amount,region',caseAttributes:'false',lookback:'2',depth:'3',split:'4',leaf:'2',categories:'5',thresholds:'6',gain:'0.1',diagnostics:'false'});const q=decisionQuery(f,schema());expect(q.get('decision_activity_attribute')).toBe('phase');expect(q.get('decision_attributes')).toBe('amount,region');expect(q.get('decision_include_trace_attributes')).toBe('false');expect(q.get('decision_lookback')).toBe('2');expect(q.get('decision_max_categories')).toBe('5');expect(q.get('decision_max_thresholds')).toBe('6');expect(q.get('include_decision_diagnostics')).toBe('false');
 });
 it('selects the real inductive conversion option and omits irrelevant tree controls for tables',()=>{
  const f=defaultDecisionForm('inductive');f.conversion='transition_bordered';expect(decisionQuery(f,schema()).get('process_tree_variant')).toBe('transition_bordered');f.mode='tables';f.depth='0';f.diagnostics='false';const q=decisionQuery(f,schema());expect(q.has('decision_max_depth')).toBe(false);expect(q.has('include_decision_diagnostics')).toBe(false);
 });
 it('rejects invalid native depth, lookback and feature-scope requests',()=>{
  const f=defaultDecisionForm('alpha');f.depth='65';expect(()=>decisionQuery(f,schema())).toThrow(/depth/);f.depth='';f.lookback='0';expect(()=>decisionQuery(f,schema())).toThrow(/lookback/);f.lookback='';f.attributes='amount,';expect(()=>decisionQuery(f,schema())).toThrow(/attributes/);f.attributes='';f.gain='NaN';expect(()=>decisionQuery(f,schema())).toThrow(/gain/);
 });
 it('retains all 60 rows, targets, classes and native numeric literals in complete CSV',async()=>{
  const s=parse(tables),out=decisionTables(s),training=out.training_0;expect(training.count).toBe(60);const csv=await ocelPreparedCsv(training).text();expect(csv.split('\r\n').length).toBe(62);expect(csv).toContain('"59"');expect(out.classes.count).toBe(2);expect(out.diagnostics.count).toBe(1);
 });
 it('retains every native guard, variable and recursive decision rule',()=>{
  const s=parse(),out=decisionTables(s);expect(out.transitions.count).toBeGreaterThan(0);expect(out.variables.count).toBeGreaterThan(0);expect(out.rules.count).toBeGreaterThan(0);if(s.result.family==='DATA_PETRI_NET'){const model=decisionPetriModel(s.result);expect(model.net.transitions.length).toBe(s.result.transitions.length);expect(model.net.transitions.every(t=>t.activity===null||t.activity>=0)).toBe(true);expect(model.labels.activities.map(a=>a.name)).toEqual(s.result.activities);}
 });
 it('uses the original guarded workspace wire response in its complete saved report',async()=>{
  vi.stubGlobal('fetch',vi.fn(async()=>new Response(guarded)));const run=await runDecisionWorkspace(defaultDecisionForm('alpha'),schema());expect(run.raw).toBe(guarded);expect(nativeJson(JSON.parse(run.report).response.result)).toBe(nativeJson(JSON.parse(guarded).result));expect(run.tables.configuration.columns).toEqual(['parameter','value']);expect(run.tables.training_0.count).toBe(60);
 });
 it('rejects changed sources and preserves native backend errors',async()=>{
  const s=parse();s.source_id='changed';vi.stubGlobal('fetch',vi.fn(async()=>new Response(JSON.stringify(s))));await expect(runDecisionWorkspace(defaultDecisionForm('alpha'),schema())).rejects.toThrow(/selected source/);vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"error":"the source log changed"}',{status:400})));await expect(runDecisionWorkspace(defaultDecisionForm('alpha'),schema())).rejects.toThrow(/source log changed/);
 });
 it('opens encoded CLI tables for offline inspection without inventing a native source ID or PNML',()=>{
  const s=parse(tables);if(s.result.family!=='DECISION_TABLES')throw new Error('wrong fixture');const run=embeddedDecisionRun(s.result,'loaded.json',60,180);expect(run.value.source_id).toBe('');expect(run.value.underlying_pnml).toBe('');expect(run.tables.training_0.count).toBe(60);expect(JSON.parse(run.report).origin).toBe('loaded_export');expect(nativeJson(JSON.parse(run.raw))).toBe(nativeJson(s.result));
 });
});
describe('decision cell preview',()=>{
 it('keeps short cells whole and never splits a surrogate pair at the cut',async()=>{
  const {decisionCellPreview,DECISION_CELL_PREVIEW_CHARACTERS:n}=await import('../src/decision-workspace');
  expect(decisionCellPreview('x'.repeat(n))).toBe('x'.repeat(n));
  const pair='x'.repeat(n-1)+'\u{1F600}'+'y',cut=decisionCellPreview(pair);
  expect(cut.startsWith('x'.repeat(n-1)+'…')).toBe(true);
  expect(cut).toContain(`(${pair.length} characters`);
  const after='x'.repeat(n-2)+'\u{1F600}'+'yy';
  expect(decisionCellPreview(after).startsWith('x'.repeat(n-2)+'\u{1F600}…')).toBe(true);
  // A lone high surrogate at the cut is the text's own unit and is kept.
  const lone='x'.repeat(n-1)+'\ud800'+'y';
  expect(decisionCellPreview(lone).startsWith('x'.repeat(n-1)+'\ud800…')).toBe(true);
 });
});
