import {afterEach,describe,expect,it,vi} from 'vitest';
import camunda from './fixtures/database-source-camunda_workflow.json?raw';
import accounting from './fixtures/database-source-sap_accounting.json?raw';
import o2c from './fixtures/database-source-sap_o2c.json?raw';
import odbc from './fixtures/database-source-odbc.json?raw';
import camundaTable from './fixtures/database-table-camunda_workflow.json?raw';
import camundaFormatted from './fixtures/database-formatted-camunda_workflow.json?raw';
import odbcTable from './fixtures/database-table-odbc.json?raw';
import odbcFormatted from './fixtures/database-formatted-odbc.json?raw';
import {parseNativeJson} from '../src/native-json';
import {acquireSource,acquiredDatabaseTable,acquisitionQuery,formFromAcquired,getAcquiredSource,isDatabaseAcquisition,releaseAcquiredSource,validAcquiredSource,validDatabaseTable,type AcquiredSource,type DatabaseAcquisitionMetadata} from '../src/source-acquisition-workspace';
import {ocelPreparedCsv} from '../src/ocel-preparation';
const parse=(raw=camunda)=>parseNativeJson(raw) as AcquiredSource&{native_acquisition:DatabaseAcquisitionMetadata};
afterEach(()=>vi.unstubAllGlobals());
describe('native database history acquisition',()=>{
 it.each([camunda,accounting,o2c,odbc])('accepts an actual native database connector result',raw=>{const s=parse(raw);expect(validAcquiredSource(s)).toBe(true);expect(isDatabaseAcquisition(s.native_acquisition)).toBe(true);});
 it('rejects incorrect row counts, missing table descriptors and cross-driver source modes',()=>{
  let s=parse();s.native_acquisition.captured_rows++;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition.table_bytes=0;expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.native_acquisition.connector='sap_o2c';expect(validAcquiredSource(s)).toBe(false);
  s=parse();s.source.mode='database';s.configuration.mode='database';expect(validAcquiredSource(s)).toBe(false);
 });
 it('validates complete raw and formatted table schemas, NULLs and indexes',()=>{
  expect(validDatabaseTable(parseNativeJson(camundaTable))).toBe(true);expect(validDatabaseTable(parseNativeJson(camundaFormatted),true)).toBe(true);expect(validDatabaseTable(parseNativeJson(camundaFormatted))).toBe(false);const v=JSON.parse(camundaTable);v.rows[0].pop();expect(validDatabaseTable(v)).toBe(false);v.rows=[];expect(validDatabaseTable(v)).toBe(false);
 });
 it('restores driver/dialect/prefix settings without storing a connection credential',()=>{
  expect(formFromAcquired(parse()).databaseDriver).toBe('sqlite');const form=formFromAcquired(parse(odbc));expect(form.databaseDriver).toBe('odbc');expect(form.databaseDialect).toBe('access');expect(JSON.stringify(form)).not.toContain('DRIVER=');
 });
 it('serializes schema qualification and bounds for native history queries',()=>{
  const f=formFromAcquired(parse());f.databasePrefix='main.';f.databaseRows='12';f.databaseTimeout='5';let q=acquisitionQuery(f,'id');expect(q.get('database_driver')).toBe('sqlite');expect(q.get('database_prefix')).toBe('main.');expect(q.get('database_max_rows')).toBe('12');expect(q.get('database_timeout_seconds')).toBe('5');f.databasePrefix='X;DROP.';expect(()=>acquisitionQuery(f,'id')).toThrow(/prefix/);f.databasePrefix='';f.databaseRows='0';expect(()=>acquisitionQuery(f,'id')).toThrow(/Maximum database rows/);f.databaseRows='';f.databaseTimeout='301';expect(()=>acquisitionQuery(f,'id')).toThrow(/timeout/);
 });
 it('uses database connection mode and an explicit installed-driver dialect',()=>{
  const f=formFromAcquired(parse(odbc)),q=acquisitionQuery(f,'id');expect(q.get('mode')).toBe('database');expect(q.get('database_dialect')).toBe('access');expect(q.has('source_path')).toBe(false);
 });
 it('uploads original SQLite bytes and fetches both complete captured tables',async()=>{
  const s=parse(),form=formFromAcquired(s),file=new File(['SQLite-original-fixture'],'original.sqlite');const fetch=vi.fn(async(url:RequestInfo|URL)=>new Response(String(url).includes('/table?')?String(url).includes('formatted=true')?camundaFormatted:camundaTable:camunda));vi.stubGlobal('fetch',fetch);const run=await acquireSource(form,s.source_id,file);expect(fetch).toHaveBeenCalledTimes(3);const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];expect(new URL(url,'http://local').searchParams.get('source_sha256')).toMatch(/^[a-f0-9]{64}$/);expect(await ((init.body as FormData).get('source') as File).text()).toBe(await file.text());expect(run.tables.query_rows.count).toBe(3);expect(run.tables.formatted_rows.count).toBe(3);expect(JSON.parse(run.report_json).database_tables.formatted.rows).toEqual(JSON.parse(camundaFormatted).rows);expect(await ocelPreparedCsv(run.tables.formatted_rows).text()).toContain('2024-01-03T10:00:00.000000000Z');
 });
 it('sends ODBC credentials only as a request header and excludes them from reports',async()=>{
  const s=parse(odbc),fetch=vi.fn(async(url:RequestInfo|URL)=>new Response(String(url).includes('/table?')?String(url).includes('formatted=true')?odbcFormatted:odbcTable:odbc));vi.stubGlobal('fetch',fetch);const run=await acquireSource(formFromAcquired(s),s.source_id,null,'',undefined,'synthetic-connection-secret');const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];expect(init.headers).toEqual({'X-Minera-Database-Connection':'synthetic-connection-secret'});expect(init.body).toBeUndefined();expect(url).not.toContain('synthetic-connection-secret');expect(run.report_json).not.toContain('synthetic-connection-secret');expect(JSON.parse(run.report_json).request.authentication_provided).toBe(true);
 });
 it('reconnects tables without rerunning a database acquisition or sending credentials',async()=>{
  const s=parse(),fetch=vi.fn(async(url:RequestInfo|URL)=>new Response(String(url).includes('/table?')?String(url).includes('formatted=true')?camundaFormatted:camundaTable:camunda));vi.stubGlobal('fetch',fetch);const run=await getAcquiredSource(s.source_id);expect(run.databaseTables!.formatted.row_count).toBe(3);expect(fetch.mock.calls.every(([url])=>!String(url).includes('/acquire?'))).toBe(true);
 });
 it('keeps complete inline database tables after the server source is released',async()=>{
  const source=JSON.parse(camunda);source.released=true;source.database_tables={raw:JSON.parse(camundaTable),formatted:JSON.parse(camundaFormatted)};const fetch=vi.fn(async()=>new Response(JSON.stringify(source)));vi.stubGlobal('fetch',fetch);const run=await releaseAcquiredSource(source.source_id);expect(fetch).toHaveBeenCalledTimes(1);expect(run.result.released).toBe(true);expect(run.tables.query_rows.count).toBe(3);expect(run.tables.formatted_rows.count).toBe(3);expect(JSON.parse(run.report_json).response.database_tables.raw.rows).toEqual(source.database_tables.raw.rows);
 });
 it('rejects table substitutions that break the immutable source descriptor',async()=>{
  const s=parse();const bad=JSON.parse(camundaTable);bad.rows.pop();bad.row_count--;vi.stubGlobal('fetch',vi.fn(async(url:RequestInfo|URL)=>new Response(String(url).includes('/table?')?String(url).includes('formatted=true')?camundaFormatted:JSON.stringify(bad):camunda)));await expect(getAcquiredSource(s.source_id)).rejects.toThrow(/do not match/);
 });
 it('exposes database table errors clearly',async()=>{
  vi.stubGlobal('fetch',vi.fn(async()=>new Response('{"error":"database source unavailable"}',{status:404})));await expect(acquiredDatabaseTable('id')).rejects.toThrow(/unavailable/);
 });
});
