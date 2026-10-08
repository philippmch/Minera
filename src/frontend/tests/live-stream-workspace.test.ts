import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import dfgRaw from "./fixtures/live-stream-dfg.json?raw";
import footprintsRaw from "./fixtures/live-stream-footprints.json?raw";
import temporalRaw from "./fixtures/live-stream-temporal.json?raw";
import tbrRaw from "./fixtures/live-stream-tbr.json?raw";
import largeRaw from "./fixtures/live-stream-large.json?raw";
import { parseNativeJson } from "../src/native-json";
import { defaultStreaming } from "../src/streaming-workspace";
import { defaultLiveImport, liveCreateQuery, validLiveSnapshot, liveTables, createLiveSession, getLiveSession, appendLiveSession, actLiveSession, listLiveSessions, liveOriginal, formFromLive, importsFromLive, type LiveSnapshot, type LiveRun } from "../src/live-stream-workspace";
import { ocelPreparedCsv } from "../src/ocel-preparation";
import { LiveStreamingInspection } from "../src/components/LiveStreamingInspection";
const parse = (raw:string) => parseNativeJson(raw) as LiveSnapshot;
const run = (raw=dfgRaw):LiveRun => ({result:parse(raw),raw,report_json:"{}",tables:liveTables(parse(raw))});
const file = new File(['case_id,activity\n"case,東京","A"\n'],"batch-東京.csv");
afterEach(()=>vi.unstubAllGlobals());
describe("persistent native stream sessions",()=>{
  it("accepts actual native snapshots for all four monitor families",()=>{
    for(const raw of [dfgRaw,footprintsRaw,temporalRaw,tbrRaw])expect(validLiveSnapshot(parse(raw))).toBe(true);
  });
  it("rejects wrong session bindings, impossible queues and malformed delivery receipts",()=>{
    let s=parse(dfgRaw);s.configuration.session_id="foreign";expect(validLiveSnapshot(s)).toBe(false);
    s=parse(dfgRaw);s.deliveries[1].position=s.deliveries[0].position;expect(validLiveSnapshot(s)).toBe(false);
    s=parse(dfgRaw);s.batches[1].first_position=0;expect(validLiveSnapshot(s)).toBe(false);
    s=parse(dfgRaw);s.deliveries[0].batch_id="missing";expect(validLiveSnapshot(s)).toBe(false);
    s=parse(dfgRaw);s.pending=999;expect(validLiveSnapshot(s)).toBe(false);
    s=parse(dfgRaw);s.registered=false;expect(validLiveSnapshot(s)).toBe(false);
    s=parse(dfgRaw);s.sources.pop();expect(validLiveSnapshot(s)).toBe(false);
  });
  it("retains queue history and native monitor counters as separate table fields",()=>{
    const s=parse(dfgRaw),tables=liveTables(s);
    expect(tables.session.rowAt(0)).toEqual([s.session_id,"dfg","ACTIVE",4,0,6,6,true,1,false]);
    expect(tables.batches.rowAt(1)).toEqual(["owned-dfg-second",2,2]);
    expect(tables.deliveries.rowAt(2).slice(0,6)).toEqual([2,0,"owned-dfg-second","c1","B",true]);
    expect(tables.edges.count).toBe(2);expect(tables.input.rowAt(0)).toEqual(["input",6]);
  });
  it("exports all delivery columns, original source positions and nullable errors",async()=>{
    const tables=liveTables(parse(dfgRaw)),csv=await ocelPreparedCsv(tables.deliveries).text();
    expect(csv.split("\r\n")).toHaveLength(8);expect(csv).toContain('"position","source_position","batch_id","case_id","activity","accepted","event_index","error"');
    expect(csv).toContain('"2","0","owned-dfg-second","c1","B","true","3","null"');
  });
  it("exports all 80 native deliveries and 40 cases beyond display pages",async()=>{
    const s=parse(largeRaw);expect(validLiveSnapshot(s)).toBe(true);const tables=liveTables(s);
    expect(tables.deliveries.count).toBe(80);expect(tables.open_cases.count).toBe(40);
    expect((await ocelPreparedCsv(tables.deliveries).text()).split("\r\n")).toHaveLength(82);
    expect((await ocelPreparedCsv(tables.open_cases).text()).split("\r\n")).toHaveLength(42);
    expect(tables.deliveries.rowAt(79).slice(0,6)).toEqual([79,39,"large-second","case-39","B",true]);
  });
  it("retains native finalization history and exact marking arrays after stream finish",async()=>{
    const s=parse(tbrRaw),tables=liveTables(s);
    expect(s.state).toBe("FINISHED");expect(s.result.family==="STREAMING_TBR"&&s.result.variant).toBe("BACKWARDS");expect(tables.terminations.count).toBe(2);
    expect(tables.terminations.rowAt(0)).toEqual(["c1",6,true,0,0,"[0,0,0,1]"]);
    expect(await ocelPreparedCsv(tables.terminations).text()).toContain('"[0,0,0,1]"');
    const f=liveTables(parse(footprintsRaw));expect(f.terminations.rowAt(0)).toEqual(["c1",6,true,null,null,null]);
  });
  it("scopes native configuration and exposes independent CSV/XES settings",()=>{
    const form=defaultStreaming(),imports=defaultLiveImport();form.method="tbr";form.finalize=true;form.variant="backwards";form.maxInvisible="18446744073709551615";imports.delimiter="\t";imports.xesVariant="rustxes";imports.xesActivity="activity 東京";imports.trim=false;
    const params=liveCreateQuery(form,imports,"owned");expect(params.get("streaming_tbr_terminate_all")).toBe("false");expect(params.get("streaming_tbr_max_invisible")).toBe("18446744073709551615");expect(params.get("csv_delimiter")).toBe("\t");expect(params.get("csv_trim_fields")).toBe("false");expect(params.get("xes_activity_attribute")).toBe("activity 東京");
    imports.delimiter="||";expect(()=>liveCreateQuery(form,imports,"owned")).toThrow(/one character/);
  });
  it("restores the actual immutable monitor configuration when reconnecting",()=>{
    const d=formFromLive(parse(dfgRaw));expect(d.activities).toBe('["A","B","C"]');expect(d.sequence).toBe('[["A","B"],["B","C"]]');
    const f=formFromLive(parse(footprintsRaw));expect(f.method).toBe("footprints");expect(f.activities).toBe('["A","B","C"]');expect(f.starts).toBe('["A"]');expect(f.sequence).toBe('[["A","B"],["B","C"]]');
    const t=formFromLive(parse(temporalRaw));expect(t.profile).toBe('[["A","B",5,1],["B","C",5,1]]');expect(t.zeta).toBe("1");expect(formFromLive(parse(tbrRaw)).variant).toBe("backwards");
    expect(importsFromLive(parse(dfgRaw))).toEqual(defaultLiveImport());
  });
  it("uploads unchanged PNML bytes and binds a caller-selected recoverable creation ID",async()=>{
    const fetcher=vi.fn(async()=>new Response(tbrRaw));vi.stubGlobal("fetch",fetcher);
    const model=new File(["<pnml> original bytes </pnml>"],"model-東京.pnml"),form=defaultStreaming();form.method="tbr";form.variant="backwards";
    const created=await createLiveSession(form,defaultLiveImport(),"owned-tbr-backwards",model);const [url,init]=fetcher.mock.calls[0] as unknown as [string,RequestInit];
    const params=new URLSearchParams(url.split("?")[1]);expect(params.get("session_id")).toBe("owned-tbr-backwards");expect(params.get("source_sha256")).toMatch(/^[a-f0-9]{64}$/);expect(await ((init.body as FormData).get("model") as File).text()).toBe(await model.text());expect(created.raw).toBe(tbrRaw);
  });
  it("creates non-replay monitors without an empty multipart transport",async()=>{
    const fetcher=vi.fn(async()=>new Response(dfgRaw));vi.stubGlobal("fetch",fetcher);
    await createLiveSession(defaultStreaming(),defaultLiveImport(),"owned-dfg");
    const [,init]=fetcher.mock.calls[0] as unknown as [string,RequestInit];expect(init.method).toBe("POST");expect(init.body).toBeUndefined();
  });
  it("appends original batch bytes with exact revision and a unique receipt ID",async()=>{
    const fetcher=vi.fn(async()=>new Response(dfgRaw));vi.stubGlobal("fetch",fetcher);const controller=new AbortController();await appendLiveSession(run(),file,"csv","new-batch",true,controller.signal);
    const [url,init]=fetcher.mock.calls[0] as unknown as [string,RequestInit];const params=new URLSearchParams(url.split("?")[1]);expect(params.get("expected_revision")).toBe("4");expect(params.get("batch_id")).toBe("new-batch");expect(params.get("format")).toBe("csv");expect(init.signal).toBe(controller.signal);expect(await ((init.body as FormData).get("log") as File).text()).toBe(await file.text());
  });
  it("preserves case IDs in revision-checked lifecycle and finalization requests",async()=>{
    const fetcher=vi.fn(async()=>new Response(footprintsRaw));vi.stubGlobal("fetch",fetcher);await actLiveSession(run(footprintsRaw),"terminate_case","case,東京");
    const [url]=fetcher.mock.calls[0] as unknown as [string];const params=new URLSearchParams(url.split("?")[1]);expect(params.get("case_id")).toBe("case,東京");expect(params.get("action")).toBe("terminate_case");expect(params.get("expected_revision")).toBe("6");
  });
  it("surfaces stale mutations and rejects foreign reconnection results",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"live session revision changed"}',{status:409})));
    await expect(actLiveSession(run(),"reset_all","")).rejects.toThrow(/revision changed/);
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(dfgRaw)));await expect(getLiveSession("foreign")).rejects.toThrow(/different live session/);
  });
  it("supports listing while rejecting duplicate session identities",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"sessions":['+dfgRaw+','+temporalRaw+']}')));expect(await listLiveSessions()).toHaveLength(2);
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"sessions":['+dfgRaw+','+dfgRaw+']}')));await expect(listLiveSessions()).rejects.toThrow(/valid server/);
  });
  it("checks original server-held upload bytes against the recorded size and hash",async()=>{
    const hash=Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256",await file.arrayBuffer())),v=>v.toString(16).padStart(2,"0")).join("");const source={source_id:"source",name:file.name,format:"csv" as const,size:file.size,provided_sha256:hash};
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(file)));expect(await (await liveOriginal(run(),source)).text()).toBe(await file.text());
    source.provided_sha256="0".repeat(64);await expect(liveOriginal(run(),source)).rejects.toThrow(/binding differs/);
  });
  it("cancels before mutation and does not publish a response after abort",async()=>{
    const controller=new AbortController();controller.abort();const fetcher=vi.fn(async()=>new Response(dfgRaw));vi.stubGlobal("fetch",fetcher);await expect(getLiveSession("owned-dfg",controller.signal)).rejects.toThrow();expect(fetcher).not.toHaveBeenCalled();
    const later=new AbortController();vi.stubGlobal("fetch",vi.fn(async()=>{later.abort();return new Response(dfgRaw);}));await expect(getLiveSession("owned-dfg",later.signal)).rejects.toThrow();
  });
  it("renders persistent lifecycle controls, complete exports and reconnect semantics",()=>{
    const html=renderToStaticMarkup(createElement(LiveStreamingInspection,{run:run(),pending:false,onAction:()=>{},onDisconnect:()=>{},onError:()=>{}}));
    for(const text of ["Persistent native stream","Drain queued events","Finish stream","Detach monitor","Reset monitor","Download live session report","Download complete live CSV","server restart"])expect(html).toContain(text);
    const final=renderToStaticMarkup(createElement(LiveStreamingInspection,{run:run(tbrRaw),pending:false,onAction:()=>{},onDisconnect:()=>{},onError:()=>{}}));expect(final).toContain("Finalize all open cases");
  });
});
