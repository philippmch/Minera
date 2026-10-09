import {afterEach,describe,expect,it,vi} from 'vitest';
import {createElement} from 'react';
import {renderToStaticMarkup} from 'react-dom/server';
import chrome from './fixtures/acquisition-chrome_history.json?raw';
import firefox from './fixtures/acquisition-firefox_history.json?raw';
import windows from './fixtures/acquisition-windows_events.json?raw';
import repository from './fixtures/acquisition-github_repo.json?raw';
import workflow from './fixtures/acquisition-camunda_workflow.json?raw';
import o2c from './fixtures/acquisition-sap_o2c.json?raw';
import accounting from './fixtures/acquisition-sap_accounting.json?raw';
import mailbox from './fixtures/acquisition-outlook_mail_extractor.json?raw';
import calendar from './fixtures/acquisition-outlook_calendar.json?raw';
import large from './fixtures/acquisition-large.json?raw';
import {acquireSource,acquiredExport,acquiredOriginal,acquiredTables,acquisitionQuery,defaultAcquisitionForm,formFromAcquired,getAcquiredSource,listAcquiredSources,releaseAcquiredSource,SOURCE_CONNECTORS,validAcquiredSource,type AcquiredSource} from '../src/source-acquisition-workspace';
import {nativeJson,parseNativeJson} from '../src/native-json';
import {ocelPreparedCsv} from '../src/ocel-preparation';
import {SourceAcquisitionWorkspace} from '../src/components/SourceAcquisitionWorkspace';
const parse=(raw=workflow)=>parseNativeJson(raw) as AcquiredSource;
afterEach(()=>vi.unstubAllGlobals());
describe('native source acquisition workflows',()=>{
 it.each([chrome,firefox,windows,repository,workflow,o2c,accounting,mailbox,calendar,large])('accepts a complete captured native connector result',raw=>{expect(validAcquiredSource(parse(raw))).toBe(true);});
 it('rejects foreign IDs, connector variants, inconsistent counts and flattened-only logs',()=>{
  const variants=[(s:AcquiredSource)=>{s.configuration.source_id='foreign';},(s:AcquiredSource)=>{s.event_count++;},(s:AcquiredSource)=>{s.native_result.variant='wrong';},(s:AcquiredSource)=>{s.native_log.log.traces[0].events[0].activity='corrupt';},(s:AcquiredSource)=>{s.source.size=null;}];
  for(const change of variants){const s=parse();change(s);expect(validAcquiredSource(s)).toBe(false);}
  expect(validAcquiredSource({...parse(),native_log:undefined})).toBe(false);
 });
 it('exports all large rows, all attribute scopes and full native JSON',async()=>{
  const s=parse(large),tables=acquiredTables(s);expect(tables.cases.count).toBe(60);expect(tables.events.count).toBe(120);expect(tables.events.rowAt(119)[2]).toBe('case-59');const csv=await ocelPreparedCsv(tables.events).text();expect(csv).toContain('case-59');expect(csv).toContain('native_event_json');expect(tables.attributes.count).toBe(60);expect(nativeJson(s.native_log.log)).toContain('case:process_id');
 });
 it('preserves exact native number tokens in inspection and complete CSV',async()=>{
  const raw=workflow.replace('"input_order":0','"input_order":9007199254740993'),s=parse(raw),t=acquiredTables(s).events;expect(nativeJson(s)).toContain('9007199254740993');expect(String(t.rowAt(0)[7])).toBe('9007199254740993');expect(await ocelPreparedCsv(t).text()).toContain('9007199254740993');
 });
 it('retains explicit Unicode paths and bounded native options without guessing a profile',()=>{
  const f=defaultAcquisitionForm();f.mode='path';f.path='C:/owned/東京/History';f.maximumEvents='12';const q=acquisitionQuery(f,'id');expect(q.get('source_path')).toBe(f.path);expect(q.get('browser_history_max_events')).toBe('12');f.path='';expect(()=>acquisitionQuery(f,'id')).toThrow(/explicit server/);f.maximumEvents='0';f.mode='upload';expect(()=>acquisitionQuery(f,'id')).toThrow(/Maximum native events/);
 });
 it('preserves calendar controls and reconstructs saved acquisition settings',()=>{
  const s=parse(calendar),f=formFromAcquired(s);expect(f.currentTime).toBe('2024-02-01T00:00:00Z');f.reminders=false;f.maximumItems='2';const q=acquisitionQuery(f,'id');expect(q.get('communication_include_reminders')).toBe('false');expect(q.get('communication_max_items')).toBe('2');expect(q.get('communication_current_time')).toBe(f.currentTime);
 });
 it('rejects embedded repository credentials, malformed bounds and missing repository names',()=>{
  const f=defaultAcquisitionForm();f.connector='github_repo';expect(()=>acquisitionQuery(f,'id')).toThrow(/owner and name/);f.owner='a';f.repository='b';for(const api of ['file:///etc/passwd','https://token@host','https://host?q=x','https://host#x']){f.apiBase=api;expect(()=>acquisitionQuery(f,'id')).toThrow(/HTTP/);}f.apiBase='';f.maximumPages='10001';expect(()=>acquisitionQuery(f,'id')).toThrow(/pages/);
 });
 it('uploads original bytes with independently computed SHA metadata',async()=>{
  const s=parse(),file=new File(['process_id,case_id,activity\np,c,東京\n'],'original-東京.csv'),f=defaultAcquisitionForm();f.connector='camunda_workflow';const fetch=vi.fn(async()=>new Response(workflow));vi.stubGlobal('fetch',fetch);await acquireSource(f,s.source_id,file);const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit],q=new URL(url,'http://local').searchParams;expect(q.get('source_name')).toBe(file.name);expect(q.get('source_sha256')).toMatch(/^[a-f0-9]{64}$/);expect(await ((init.body as FormData).get('source') as File).text()).toBe(await file.text());expect(init.headers).toBeUndefined();
 });
 it('sends repository credentials only in the acquisition header and excludes them from reports',async()=>{
  const s=parse(repository),f=formFromAcquired(s),fetch=vi.fn(async()=>new Response(repository));vi.stubGlobal('fetch',fetch);const run=await acquireSource(f,s.source_id,null,'synthetic-secret');const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];expect(init.headers).toEqual({Authorization:'Bearer synthetic-secret'});expect(url).not.toContain('synthetic-secret');expect(init.body).toBeUndefined();expect(run.report_json).not.toContain('synthetic-secret');expect(JSON.parse(run.report_json).request.authentication_provided).toBe(true);expect(JSON.parse(run.report_json).response.native_log).toEqual(JSON.parse(repository).native_log);
 });
 it('reads server paths without an upload body and sends no unrelated credential',async()=>{
  const s=parse(),f=formFromAcquired(s);f.mode='path';f.path='C:/owned/source.csv';const fetch=vi.fn(async()=>new Response(workflow));vi.stubGlobal('fetch',fetch);await acquireSource(f,s.source_id,null,'unrelated');const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];expect(url).toContain('mode=path');expect(init.body).toBeUndefined();expect(init.headers).toBeUndefined();
 });
 it('reconnects, releases and validates list snapshots without repeating source acquisition',async()=>{
  const s=parse(),fetch=vi.fn(async(url:RequestInfo|URL)=>new Response(String(url).includes('/list')?'{"sources":['+workflow+']}':workflow));vi.stubGlobal('fetch',fetch);expect((await getAcquiredSource(s.source_id)).result.source_id).toBe(s.source_id);await releaseAcquiredSource(s.source_id);expect(await listAcquiredSources()).toHaveLength(1);expect(fetch.mock.calls.every(([url])=>!String(url).includes('/acquire?'))).toBe(true);
 });
 it('returns actual original/export response bytes and native errors',async()=>{
  const fetch=vi.fn(async()=>new Response('actual original bytes'));vi.stubGlobal('fetch',fetch);expect(await (await acquiredOriginal('id')).text()).toBe('actual original bytes');await acquiredExport('id','xes','etree',true);const [url]=fetch.mock.calls[1] as unknown as [string];expect(url).toContain('variant=etree');expect(url).toContain('gzip=true');vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"error":"source unavailable"}',{status:404})));await expect(getAcquiredSource('id')).rejects.toThrow(/unavailable/);
 });
 it('rejects invalid responses and cross-source substitutions',async()=>{
  vi.stubGlobal('fetch',vi.fn(async()=>new Response(workflow)));await expect(getAcquiredSource('foreign')).rejects.toThrow(/different acquired source/);vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"sources":[{}]}')));await expect(listAcquiredSources()).rejects.toThrow(/invalid/);
 });
 it('renders all nine connector choices with explicit source and driver boundaries',()=>{
  const html=renderToStaticMarkup(createElement(SourceAcquisitionWorkspace,{}));for(const [,label] of SOURCE_CONNECTORS)expect(html).toContain(label);expect(html).toContain('Upload original source');expect(html).toContain('Explicit server file or directory');expect(html).toContain('Acquire native source log');
 });
});
