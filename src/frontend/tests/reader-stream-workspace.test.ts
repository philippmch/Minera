import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import csvRaw from "./fixtures/reader-stream-csv.json?raw";
import tracesRaw from "./fixtures/reader-stream-traces.json?raw";
import resetRaw from "./fixtures/reader-stream-reset.json?raw";
import largeRaw from "./fixtures/reader-stream-large.json?raw";
import { parseNativeJson, nativeJson } from "../src/native-json";
import { actReader, createReader, defaultReaderForm, getReader, listReaders, readReader, readerCreateQuery, readerExport, readerExportImportOptions, readerFormFrom, readerOriginal, readerTables, validReaderSnapshot, type ReaderRun, type ReaderSnapshot } from "../src/reader-stream-workspace";
import { ocelPreparedCsv } from "../src/ocel-preparation";
import { ReaderStreamWorkspace } from "../src/components/ReaderStreamWorkspace";
const parse=(raw=csvRaw)=>parseNativeJson(raw) as ReaderSnapshot;
const run=(raw=csvRaw):ReaderRun=>({result:parse(raw),raw,report_json:"{}",tables:readerTables(parse(raw))});
afterEach(()=>vi.unstubAllGlobals());
describe("native event and trace reader workflows",()=>{
  it("accepts actual native CSV, complete trace, reset and large reader outputs",()=>{for(const raw of [csvRaw,tracesRaw,resetRaw,largeRaw])expect(validReaderSnapshot(parse(raw))).toBe(true);});
  it("rejects foreign sessions, impossible queues and corrupt generation receipts",()=>{
    let s=parse();s.configuration.session_id="foreign";expect(validReaderSnapshot(s)).toBe(false);
    s=parse();s.pending=999;expect(validReaderSnapshot(s)).toBe(false);
    s=parse();s.items[0].reader_position=999;expect(validReaderSnapshot(s)).toBe(false);
    s=parse();s.batches[0].first_position=1;expect(validReaderSnapshot(s)).toBe(false);
    s=parse();s.items[0].generation=999;expect(validReaderSnapshot(s)).toBe(false);
    s=parse();s.deliveries[0].position=1;expect(validReaderSnapshot(s)).toBe(false);
  });
  it("preserves empty whole traces and native nested attributes without event flattening",()=>{
    const s=parse(tracesRaw),tables=readerTables(s);expect(s.items).toHaveLength(3);expect(tables.read_items.rowAt(1).slice(4,6)).toEqual(["same",0]);expect(tables.read_events.count).toBe(3);
    expect(nativeJson(s.items[0].native_item)).toContain("9223372036854775807");expect(nativeJson(s.items[0].native_item)).toContain('"container":"list"');expect(s.items[0].native_item.log.traces[0].events[0].activity).toBe("A,\n東京");
  });
  it("keeps reader results and observer deliveries distinct through detach and rewind",()=>{
    const s=parse(resetRaw),tables=readerTables(s);expect(s.enqueued).toBe(4);expect(s.delivered).toBe(3);expect(s.reader_generation).toBe(1);expect(tables.read_items.rowAt(3).slice(0,3)).toEqual([3,1,0]);expect(tables.read_events.count).toBe(4);expect(tables.delivered_events.count).toBe(3);
  });
  it("exports all 120 native items and events beyond display pages",async()=>{
    const tables=readerTables(parse(largeRaw));expect(tables.read_items.count).toBe(120);expect(tables.delivered_events.count).toBe(120);expect((await ocelPreparedCsv(tables.read_items).text()).split("\r\n")).toHaveLength(122);
  });
  it("preserves literal selectors, transformed-label filters and exact empty IDs",()=>{
    const form=defaultReaderForm();form.activityColumn=" Act ,東京";form.prefix="prefix,\n";form.acceptCase=true;form.acceptActivity=true;form.activity="prefix,\nA";
    const q=readerCreateQuery(form,"session");expect(q.get("activity_column")).toBe(" Act ,東京");expect(q.get("accept_case")).toBe("");expect(q.get("accept_activity")).toBe("prefix,\nA");expect(q.get("activity_prefix")).toBe("prefix,\n");expect(q.has("xes_variant")).toBe(false);
  });
  it("validates native read bounds and reconnects with retained configuration",async()=>{
    const form=defaultReaderForm();form.kind="xes_traces";form.minimum="2";form.maximum="1";expect(()=>readerCreateQuery(form,"session")).toThrow(/Minimum/);form.minimum="0";form.maximum="0";form.xesVariant="rustxes";form.maxTraces="0";
    expect(readerCreateQuery(form,"session").get("xes_max_traces")).toBe("0");const resumed=readerFormFrom(parse(tracesRaw));expect(resumed.kind).toBe("xes_traces");expect(resumed.xesCase).toBe("concept:name");expect(()=>readReader(run(),"0","batch")).toThrow(/Items per read/);expect(()=>readReader(run(),"10001","batch")).toThrow(/Items per read/);
  });
  it("sends the original source bytes and binds its SHA-256 metadata",async()=>{
    const s=parse(),file=new File(['case_id,activity\n"c,東京",A\n'],"original-東京.csv"),form=defaultReaderForm();const fetch=vi.fn(async()=>new Response(csvRaw));vi.stubGlobal("fetch",fetch);
    const next=await createReader(form,s.session_id,file);expect(next.result.session_id).toBe(s.session_id);const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];const q=new URL(url,"http://local").searchParams;expect(q.get("source_name")).toBe(file.name);expect(q.get("source_sha256")).toMatch(/^[a-f0-9]{64}$/);expect(await ((init.body as FormData).get("log") as File).text()).toBe(await file.text());
  });
  it("sends revision-checked reads and actions and retains the complete raw report",async()=>{
    const s=parse();const fetch=vi.fn(async()=>new Response(csvRaw));vi.stubGlobal("fetch",fetch);const next=await readReader(run(),"2","unique-batch");expect(JSON.parse(next.report_json).response.items).toHaveLength(3);
    let [url]=fetch.mock.calls[0] as unknown as [string];let q=new URL(url,"http://local").searchParams;expect(q.get("maximum_items")).toBe("2");expect(q.get("expected_revision")).toBe(String(s.revision));expect(q.get("batch_id")).toBe("unique-batch");await actReader(run(),"reset_reader");[url]=fetch.mock.calls[1] as unknown as [string];q=new URL(url,"http://local").searchParams;expect(q.get("action")).toBe("reset_reader");
  });
  it("exposes native errors and rejects mismatched session responses",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"reader revision changed"}',{status:409})));await expect(getReader("session")).rejects.toThrow(/revision/);
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(csvRaw)));await expect(getReader("foreign")).rejects.toThrow(/different reader session/);
  });
  it("uses actual original/export bodies and validates reconnectable lists",async()=>{
    const s=parse();let fetch=vi.fn(async()=>new Response('{"sessions":['+csvRaw+']}'));vi.stubGlobal("fetch",fetch);expect(await listReaders()).toHaveLength(1);
    fetch=vi.fn(async()=>new Response('original bytes'));vi.stubGlobal("fetch",fetch);expect(await (await readerOriginal(s.session_id)).text()).toBe('original bytes');expect(await (await readerExport(s.session_id,"read","xes","etree",true)).text()).toBe('original bytes');const calls=fetch.mock.calls as unknown as [string][];expect(calls[1][0]).toContain('scope=read');expect(calls[1][0]).toContain('gzip=true');
  });
  it("renders all reader selectors and clearly distinguishes source import from cursor reads",()=>{
    const html=renderToStaticMarkup(createElement(ReaderStreamWorkspace,{}));expect(html).toContain("XES whole-trace reader");expect(html).toContain("CSV event reader");expect(html).toContain("import the upload in memory");expect(html).toContain("Native reader cursors");expect(html).toContain("Activity prefix transform");
  });
  it("carries all five literal export selectors into analysis rather than guessing canonical keys",()=>{
    const form=defaultReaderForm();form.caseColumn="Case";form.activityColumn=" Act ";form.timestampColumn=" Time ";form.resourceColumn="Who";form.lifecycleColumn="Life";expect(readerExportImportOptions(form)).toEqual({xesCaseAttribute:"Case",xesActivityAttribute:" Act ",xesTimestampAttribute:" Time ",xesResourceAttribute:"Who",xesLifecycleAttribute:"Life"});
    form.kind="xes_traces";form.xesCase="case,東京";expect(readerExportImportOptions(form).xesCaseAttribute).toBe("case,東京");
  });
});
