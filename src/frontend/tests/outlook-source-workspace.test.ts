import {afterEach,describe,expect,it,vi} from 'vitest';
import mail from './fixtures/outlook-live-mail.json?raw';
import calendar from './fixtures/outlook-live-calendar.json?raw';
import shared from './fixtures/outlook-live-shared.json?raw';
import bounded from './fixtures/outlook-live-bounded.json?raw';
import noReminders from './fixtures/outlook-live-no-reminders.json?raw';
import {parseNativeJson,nativeJson} from '../src/native-json';
import {acquireSource,acquiredOutlookTable,acquiredTables,acquisitionQuery,formFromAcquired,getAcquiredSource,isOutlookAcquisition,releaseAcquiredSource,validAcquiredSource,validOutlookItemTable,validOutlookFormattedTable,type AcquiredSource,type OutlookAcquisitionMetadata} from '../src/source-acquisition-workspace';
import {ocelPreparedCsv} from '../src/ocel-preparation';
const parse=(raw=mail)=>parseNativeJson(raw) as AcquiredSource&{native_acquisition:OutlookAcquisitionMetadata;outlook_tables:NonNullable<AcquiredSource['outlook_tables']>};
afterEach(()=>vi.unstubAllGlobals());
describe('native Outlook acquisition workspace',()=>{
 it.each([mail,calendar,shared,bounded,noReminders])('accepts actual production-handler/native-COM captures',raw=>{const s=parse(raw);expect(validAcquiredSource(s)).toBe(true);expect(isOutlookAcquisition(s.native_acquisition)).toBe(true);});
 it('rejects missing item tables, conflicting counts, source types and fabricated originals',()=>{
  let s=parse();delete(s as AcquiredSource).outlook_tables;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition.captured_items++;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition.skipped_items++;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition.examined_items=s.native_acquisition.total_items+1;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.source.mode='upload';s.configuration.mode='upload';expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.source.has_original=true;s.source.size=1;expect(validAcquiredSource(s)).toBe(false);
 });
 it('rejects substituted formatted fields even when the row count and schema remain correct',()=>{
  const s=parse();s.outlook_tables.formatted.rows[0][6]='substituted subject';expect(validAcquiredSource(s)).toBe(false);const c=parse(calendar);c.outlook_tables.formatted.rows[0][5]=9;expect(validAcquiredSource(c)).toBe(false);
 });
 it('validates raw item/skipped schemas and integer formatted indexes',()=>{
  const s=parse();expect(validOutlookItemTable(s.outlook_tables.items)).toBe(true);expect(validOutlookFormattedTable(s.outlook_tables.formatted)).toBe(true);s.outlook_tables.items.rows[0].pop();expect(validOutlookItemTable(s.outlook_tables.items)).toBe(false);const c=parse();c.outlook_tables.formatted.rows[0][7]='0';expect(validOutlookFormattedTable(c.outlook_tables.formatted)).toBe(false);
 });
 it('restores desktop mode and shared calendar settings from native configuration',()=>{
  const f=formFromAcquired(parse(shared));expect(f.mode).toBe('outlook_com');expect(f.outlookEmail).toBe('owner@example.invalid');expect(f.outlookCalendarId).toBe('9');expect(formFromAcquired(parse(noReminders)).reminders).toBe(false);
 });
 it('sends Outlook desktop acquisition without an upload, path or credential headers',async()=>{
  const s=parse(),fetch=vi.fn(async()=>new Response(mail));vi.stubGlobal('fetch',fetch);const run=await acquireSource(formFromAcquired(s),s.source_id,null,'unused-token',undefined,'unused-connection');expect(fetch).toHaveBeenCalledTimes(1);const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit],q=new URL(url,'http://local').searchParams;expect(q.get('mode')).toBe('outlook_com');expect(q.get('outlook_source')).toBe('com');expect(q.has('source_path')).toBe(false);expect(init.body).toBeUndefined();expect(init.headers).toBeUndefined();expect(JSON.parse(run.report_json).response.outlook_tables.items.row_count).toBe(120);
 });
 it('serializes shared folder/reminder/item limits only for the appropriate connector',()=>{
  const f=formFromAcquired(parse(shared));f.maximumItems='4';f.reminders=false;const q=acquisitionQuery(f,'id');expect(q.get('outlook_email_user')).toBe(f.outlookEmail);expect(q.get('outlook_calendar_id')).toBe('9');expect(q.get('communication_max_items')).toBe('4');expect(q.get('communication_include_reminders')).toBe('false');f.connector='outlook_mail_extractor';expect(acquisitionQuery(f,'id').has('outlook_email_user')).toBe(false);expect(acquisitionQuery(f,'id').has('outlook_calendar_id')).toBe(false);
 });
 it('rejects invalid folder/item limits and incompatible desktop connector modes',()=>{
  const f=formFromAcquired(parse(shared));f.outlookCalendarId='0';expect(()=>acquisitionQuery(f,'id')).toThrow(/folder ID/);f.outlookCalendarId='1025';expect(()=>acquisitionQuery(f,'id')).toThrow(/folder ID/);f.outlookCalendarId='9';f.maximumItems='1000001';expect(()=>acquisitionQuery(f,'id')).toThrow(/items/);f.maximumItems='';f.connector='windows_events';expect(()=>acquisitionQuery(f,'id')).toThrow(/Outlook connector/);
 });
 it('exports all native item, formatted, skipped and metadata rows beyond visible pages',async()=>{
  const s=parse(),tables=acquiredTables(s);expect(tables.outlook_items.count).toBe(120);expect(tables.formatted_rows.count).toBe(120);expect(tables.skipped_items.count).toBe(2);expect(tables.acquisition.columns).toContain('total_items');const csv=await ocelPreparedCsv(tables.outlook_items).text();expect(csv).toContain(s.outlook_tables.items.rows.at(-1)![7]);expect(await ocelPreparedCsv(tables.skipped_items).text()).toContain('com_property_error');expect(await ocelPreparedCsv(tables.attributes).text()).toContain('@@index');
 });
 it('reconnects and releases complete inline capture tables without additional backend reads',async()=>{
  const s=parse(),fetch=vi.fn(async()=>new Response(mail));vi.stubGlobal('fetch',fetch);const connected=await getAcquiredSource(s.source_id);expect(fetch).toHaveBeenCalledTimes(1);expect(connected.tables.outlook_items.count).toBe(120);const releasedRaw=mail.replace(/}$/,',"released":true}');vi.stubGlobal('fetch',vi.fn(async()=>new Response(releasedRaw)));const released=await releaseAcquiredSource(s.source_id);expect(released.result.released).toBe(true);expect(released.tables.outlook_items.count).toBe(120);expect(nativeJson(released.result.outlook_tables)).toBe(nativeJson(s.outlook_tables));
 });
 it('downloads the original native item/formatted table wire response and rejects wrong table types',async()=>{
  const s=parse(),raw=nativeJson(s.outlook_tables.items);vi.stubGlobal('fetch',vi.fn(async()=>new Response(raw)));expect((await acquiredOutlookTable(s.source_id)).raw).toBe(raw);await expect(acquiredOutlookTable(s.source_id,true)).rejects.toThrow(/invalid Outlook/);vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"error":"source was released"}',{status:404})));await expect(acquiredOutlookTable(s.source_id)).rejects.toThrow(/released/);
 });
});
