import {afterEach,describe,expect,it,vi} from 'vitest';
import evtxRaw from './fixtures/windows-source-evtx.json?raw';
import serviceRaw from './fixtures/windows-source-service.json?raw';
import filteredRaw from './fixtures/windows-source-filtered.json?raw';
import {parseNativeJson,nativeJson} from '../src/native-json';
import {acquireSource,acquiredCapture,acquiredTables,acquisitionQuery,formFromAcquired,validAcquiredSource,type AcquiredSource} from '../src/source-acquisition-workspace';
import {ocelPreparedCsv} from '../src/ocel-preparation';
const parse=(raw=evtxRaw)=>parseNativeJson(raw) as AcquiredSource;
afterEach(()=>vi.unstubAllGlobals());
describe('native Windows event source workflows',()=>{
 it.each([evtxRaw,serviceRaw,filteredRaw])('accepts actual native Windows capture metadata with complete logs',raw=>expect(validAcquiredSource(parse(raw))).toBe(true));
 it('rejects missing, foreign or inconsistent native capture descriptors',()=>{
  let s=parse(serviceRaw);delete s.native_acquisition;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition!.captured_events++;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition!.driver='windows_event_service';expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition!.xml_bytes=0;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_result.traces=[];expect(validAcquiredSource(s)).toBe(false);
 });
 it('restores explicit service channel, native query and file format from snapshots',()=>{
  const service=formFromAcquired(parse(serviceRaw));expect(service.mode).toBe('windows_service');expect(service.windowsChannel).toBe('Application');expect(service.windowsQuery).toBe('*[System[EventID=65535]]');expect(formFromAcquired(parse()).windowsSource).toBe('evtx');
 });
 it('sends live Windows service queries without upload bytes or filesystem paths',async()=>{
  const s=parse(serviceRaw),form=formFromAcquired(s),fetch=vi.fn(async()=>new Response(serviceRaw));vi.stubGlobal('fetch',fetch);await acquireSource(form,s.source_id,null);const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit],q=new URL(url,'http://local').searchParams;expect(q.get('mode')).toBe('windows_service');expect(q.get('windows_events_source')).toBe('service');expect(q.get('windows_events_channel')).toBe('Application');expect(q.has('source_path')).toBe(false);expect(init.body).toBeUndefined();expect(init.headers).toBeUndefined();
 });
 it('keeps EVTX selection, XPath, newest-first and timeout controls explicit',()=>{
  const f=formFromAcquired(parse());f.windowsQuery='*[System[EventID=1]]';f.windowsReverse=true;f.windowsTimeout='250';const q=acquisitionQuery(f,'id');expect(q.get('windows_events_source')).toBe('evtx');expect(q.get('windows_events_query')).toBe(f.windowsQuery);expect(q.get('windows_events_reverse')).toBe('true');expect(q.get('windows_events_timeout_ms')).toBe('250');
 });
 it('rejects missing service channels, cross-connector service modes and bad timeouts',()=>{
  const f=formFromAcquired(parse(serviceRaw));f.windowsChannel='';expect(()=>acquisitionQuery(f,'id')).toThrow(/explicit local Windows/);f.windowsChannel='Application';f.windowsTimeout='30001';expect(()=>acquisitionQuery(f,'id')).toThrow(/timeout/);f.windowsTimeout='';f.connector='sap_o2c';expect(()=>acquisitionQuery(f,'id')).toThrow(/Windows connector/);
 });
 it('exports the complete capture descriptor and all rows beyond visible paging',async()=>{
  const s=parse(),tables=acquiredTables(s);expect(tables.acquisition.count).toBe(1);expect(tables.acquisition.columns).toContain('native_acquisition_json');expect(await ocelPreparedCsv(tables.acquisition).text()).toContain('windows_evtx');expect(tables.events.count).toBe(120);expect(await ocelPreparedCsv(tables.events).text()).toContain(nativeJson(s.native_log.log.traces.flatMap(t=>t.events).at(-1)).replaceAll('"','""'));
 });
 it('downloads actual native XML capture bytes and surfaces native missing-capture errors',async()=>{
  const fetch=vi.fn(async()=>new Response('<Events>actual captured XML</Events>'));vi.stubGlobal('fetch',fetch);expect(await (await acquiredCapture('id')).text()).toBe('<Events>actual captured XML</Events>');expect((fetch.mock.calls[0] as unknown as [string])[0]).toContain('/capture?source_id=id');vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"error":"no native event XML capture"}',{status:404})));await expect(acquiredCapture('id')).rejects.toThrow(/no native/);
 });
});
