import {afterEach,describe,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import freshRaw from "./fixtures/declare-stream-fresh.json?raw";
import liveRaw from "./fixtures/declare-stream-live.json?raw";
import largeRaw from "./fixtures/declare-stream-large.json?raw";
import specialRaw from "./fixtures/declare-stream-special.json?raw";
import emptyRaw from "./fixtures/declare-stream-empty.json?raw";
import {parseNativeJson,nativeJson} from "../src/native-json";
import {DECLARE_TEMPLATES,declareBinary,isDeclareStreamResult,validDeclareStreamModel,declareStreamTables,declareRulesFile,displayedDeclareFile,type DeclareStreamResult} from "../src/declare-stream-workspace";
import {validLiveSnapshot,liveTables,type LiveSnapshot} from "../src/live-stream-workspace";
import {defaultStreaming,streamingQuery,requestStreamingWorkflow} from "../src/streaming-workspace";
import {ocelPreparedCsv} from "../src/ocel-preparation";
import {type MineraExport} from "../src/types";
import {DeclareStreamModelFields} from "../src/components/DeclareStreamModelFields";
import {DeclareStreamChart} from "../src/components/DeclareStreamChart";
const parse=(raw:string)=>parseNativeJson(raw) as DeclareStreamResult;
afterEach(()=>vi.unstubAllGlobals());
describe("native Declare prefix workflows",()=>{
  it("accepts actual fresh, empty, large, special-label and persistent outputs",()=>{
    for(const raw of [freshRaw,largeRaw,specialRaw,emptyRaw])expect(isDeclareStreamResult(parse(raw))).toBe(true);
    expect(validLiveSnapshot(parseNativeJson(liveRaw))).toBe(true);
  });
  it("exposes all eighteen known template arities and retains unknown inert templates",()=>{
    expect(DECLARE_TEMPLATES).toHaveLength(18);expect(DECLARE_TEMPLATES.filter(t=>!declareBinary(t))).toEqual(["existence","exactly_one","init","absence"]);
    const model=parse(freshRaw).model;expect(model.rules.some(r=>r.template==="custom,inert")).toBe(true);expect(model.rules.filter(r=>r.template==="response")).toHaveLength(2);expect(validDeclareStreamModel(model)).toBe(true);
  });
  it("rejects wrong native rule references, duplicate violation identities and inconsistent totals",()=>{
    let r=parse(freshRaw);r.cases.c1.constraint_states[0].first="foreign";expect(isDeclareStreamResult(r)).toBe(false);
    r=parse(freshRaw);r.model.rules[0].constraint_index=99;expect(isDeclareStreamResult(r)).toBe(false);
    r=parse(freshRaw);r.total_deviations++;expect(isDeclareStreamResult(r)).toBe(false);
    r=parse(freshRaw);const event=r.event_results!.find(e=>e.violated_constraints.length>0)!;event.violated_constraints.push(event.violated_constraints[0]);event.deviations++;expect(isDeclareStreamResult(r)).toBe(false);
  });
  it("retains native per-event violations, automaton state and separate repeated timestamps",()=>{
    const r=parse(freshRaw),tables=declareStreamTables(r);expect(tables.declare_rules.count).toBe(20);expect(tables.constraint_states.count).toBe(40);expect(tables.time_deviations.count).toBe(5);
    expect(tables.time_deviations.rowAt(0)[1]).toBe("1");expect(r.deviations_per_time.filter(t=>t.timestamp==="repeat")).toHaveLength(4);
    expect(tables.event_violations.count).toBe(r.total_deviations);expect(tables.constraint_states.rowAt(0)[0]).toBe("c1");
  });
  it("exports all 800 states and 120 arrival/event rows beyond display pages",async()=>{
    const r=parse(largeRaw),tables=declareStreamTables(r);expect(tables.constraint_states.count).toBe(800);
    expect((await ocelPreparedCsv(tables.constraint_states).text()).split("\r\n")).toHaveLength(802);expect((await ocelPreparedCsv(tables.time_deviations).text()).split("\r\n")).toHaveLength(122);expect(tables.declare_events.count).toBe(120);
  });
  it("preserves control characters, delimiters, Unicode and signed int32 metadata",async()=>{
    const r=parse(specialRaw),rule=r.model.rules[0];expect(rule.first).toBe("A,->:\n東京");expect(rule.second).toBe("B\\β");expect(rule.support).toBe(-2);expect(rule.confidence).toBe(2147483647);
    const csv=await ocelPreparedCsv(declareStreamTables(r).declare_rules).text();expect(csv).toContain('"A,->:\n東京"');expect(csv).toContain('"-2","2147483647"');expect(nativeJson(r.model)).toContain('A,->:\\n東京');
  });
  it("preserves original authored JSON bytes and uses exact displayed-model activity IDs",async()=>{
    const raw=' {"rules":[]}\n';expect(await declareRulesFile(raw).text()).toBe(raw);
    const data={dataset:"source",activities:[{name:"A,東京"},{name:"B"}],declare:{response:[{source:0,target:1,support:4,confidence:2}]}} as MineraExport;
    const file=displayedDeclareFile(data),value=JSON.parse(await file.text());expect(value.activities).toEqual(["A,東京","B"]);expect(value.declare.response[0]).toEqual({source:0,target:1,support:4,confidence:2});expect(value.provenance.dataset).toBe("source");
  });
  it("sends original Declare model and gzip event bytes without PNML or query-label conversion",async()=>{
    const form=defaultStreaming();form.method="declare";form.timestampColumn="when,東京";const model=new File(['{"rules":[]}'],"rules.json"),events=new File([new Uint8Array([31,139,0])],"events.xes.gz");const fetcher=vi.fn(async()=>new Response(freshRaw));vi.stubGlobal("fetch",fetcher);
    const run=await requestStreamingWorkflow(form,events,model);const [url,init]=fetcher.mock.calls[0] as unknown as [string,RequestInit];const params=new URLSearchParams(url.split("?")[1]);expect(params.get("format")).toBe("xes");expect(params.get("streaming_declare_timestamp_column")).toBe("when,東京");expect(params.has("streaming_tbr_variant")).toBe(false);expect(await ((init.body as FormData).get("model") as File).text()).toBe(await model.text());expect(new Uint8Array(await ((init.body as FormData).get("log") as File).arrayBuffer())).toEqual(new Uint8Array([31,139,0]));expect(JSON.parse(run.report_json).request.parameters.format).toBe("xes");
  });
  it("includes persistent Declare delivery identities while retaining other live-family contracts",()=>{
    const s=parseNativeJson(liveRaw) as LiveSnapshot,tables=liveTables(s);expect(tables.declare_events.count).toBe(5);expect(tables.event_violations.count).toBe(s.result.family==="STREAMING_DECLARE"&&s.result.total_deviations);
    expect(tables.declare_events.rowAt(0).slice(0,6)).toEqual([0,0,"live-queue","c1","A","1"]);
    s.deliveries[0].native_event!.activity="foreign";expect(validLiveSnapshot(s)).toBe(false);
  });
  it("shows prefix semantics, all template choices and native arrival observations",()=>{
    const fields=renderToStaticMarkup(createElement(DeclareStreamModelFields,{file:null,onFile:()=>{}}));for(const t of DECLARE_TEMPLATES)expect(fields).toContain('value="'+t+'"');expect(fields).toContain("no end-of-case finalization");expect(fields).toContain('aria-label="Declare model JSON"');
    const chart=renderToStaticMarkup(createElement(DeclareStreamChart,{result:parse(freshRaw)}));expect(chart).toContain("Repeated timestamps remain separate");expect(chart).toContain("Declare deviations by native arrival entry");
  });
  it("rejects invalid JSON authoring and native model errors before publication",async()=>{
    expect(()=>declareRulesFile("[]")).toThrow(/rules array/);expect(()=>declareRulesFile("bad")).toThrow(/valid JSON/);
    const form=defaultStreaming();form.method="declare";const model=new File(["bad"],"model.json"),events=new File(["case_id,activity\nc,A\n"],"events.csv");vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"invalid arity"}',{status:400})));await expect(requestStreamingWorkflow(form,events,model)).rejects.toThrow(/arity/);expect(new URLSearchParams(streamingQuery(form)).has("streaming_tbr_variant")).toBe(false);
  });
});
