import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { StructuralWorkspace } from "../src/components/StructuralWorkspace";
import { TransformationHub } from "../src/components/TransformationHub";
import { DEFAULT_TRIE,DEFAULT_INTERVAL,trieOptions,intervalOptions,trieRows,trieGraph,isStructuralRun,requestStructural,structuralCsv,structuralPnml,structuralReplayKey,structuralPetriLayout,intervalIndices,intervalTimeline,epochText,type StructuralRun } from "../src/structural-workspace";
import { intervalTreeQuery } from "../src/api";
import { type MineraExport,type TrieResult,type IntervalTreeResult } from "../src/types";

const cases=[{index:0,case_id:"same",event_count:2},{index:1,case_id:"same",event_count:2},{index:2,case_id:"empty",event_count:0}];
const trie:TrieResult={family:"LOG_TO_TRIE",input_traces:3,distinct_variants:2,node_count:4,truncated:false,root:{label:"",depth:0,final:false,children:[{label:"A,α",depth:1,final:true,children:[{label:"Review",depth:2,final:true,children:[]}]},{label:"Review",depth:1,final:false,children:[]}]}};
const net={family:"TRIE_CONVERSION",variant:"TO_PETRI_NET",source:0,sink:1,activities:["A,α","Review"],place_count:3,transitions:[{activity:0,inputs:[0],outputs:[2]},{activity:1,inputs:[2],outputs:[1]},{activity:null,inputs:[2],outputs:[1]}]};
const intervals:IntervalTreeResult={family:"LOG_TO_INTERVAL_TREE",interval_count:2,intervals:[{start:1790845200.123446,end:1790848800.123466,case_id:"same",source_activity:"A,α",target_activity:"Review",trace_index:0,source_event_index:0,target_event_index:1},{start:1790845200.123446,end:1790848800.123466,case_id:"same",source_activity:"B 東京",target_activity:"Review",trace_index:1,source_event_index:0,target_event_index:1}],point_query:1790847000.123456,point_matches:[1],range_query:[1790848800.123466,1790848800.123466],range_matches:[0,1]};
const envelope=(result:unknown)=>({family:"STRUCTURAL_TRANSFORMATION",source_id:"7",activity_key:"activity",cases,result,trie:result===net?trie:null,pnml:result===net?'<?xml version="1.0"?><pnml/>':null});
const run=(result:unknown)=>({...envelope(result),data_json:"",report_json:""}) as StructuralRun;
const respond=(value:unknown)=>vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>JSON.stringify(value)}));
afterEach(()=>vi.unstubAllGlobals());
describe("structural transformation workflows",()=>{
  it("preserves full paths, literal activity selectors and explicit positive truncation limits",()=>{
    expect(trieOptions(DEFAULT_TRIE)).toEqual({trie_activity_key:"activity"});expect(trieOptions({...DEFAULT_TRIE,activity:" phase 東京 ",limit:"2"})).toEqual({trie_activity_key:" phase 東京 ",trie_max_path_length:2});for(const limit of ["0","-1","1.5","NaN","9007199254740992"])expect(()=>trieOptions({...DEFAULT_TRIE,limit})).toThrow();
  });
  it("serializes native epoch queries and escaped activity pairs without rounding or trimming labels",()=>{
    const options=intervalOptions({...DEFAULT_INTERVAL,filter:true,source:" A,α ",target:"Approve, 東京",point:true,pointValue:"1790845200.123446",range:true,lower:"1790845200.123446",upper:"1790845200.123446"});const query=new URLSearchParams(intervalTreeQuery(options));expect(query.get("interval_filter_activity_couple")).toBe(" A\\,α ,Approve\\, 東京");expect(query.get("interval_query_point")).toBe("1790845200.123446");expect(options.interval_query_start).toBe(options.interval_query_end);expect(intervalOptions(DEFAULT_INTERVAL).interval_query_point).toBeUndefined();expect(()=>intervalOptions({...DEFAULT_INTERVAL,epsilon:"-1"})).toThrow(/non-negative/);expect(()=>intervalOptions({...DEFAULT_INTERVAL,range:true,lower:"2",upper:"1"})).toThrow(/must not exceed/);expect(()=>intervalOptions({...DEFAULT_INTERVAL,point:true,pointValue:"Infinity"})).toThrow(/finite/);
  });
  it("uses stable child-position identities for repeated labels, terminals and exact paths",()=>{
    const rows=trieRows(trie);expect(rows.map(row=>row.id)).toEqual(["root","root/0","root/0/0","root/1"]);expect(rows[2].path).toEqual(["A,α","Review"]);expect(rows[1].node.final).toBe(true);const graph=trieGraph(trie);expect(graph.nodes.filter(node=>node.label==="Review").map(node=>node.id)).toEqual(["root/0/0","root/1"]);expect(graph.nodes[1].end).toBe(true);expect(graph.edges[1]).toMatchObject({source:"root/0",target:"root/0/0"});
  });
  it("validates trie counts/depth and source positions rather than deduplicating case IDs",()=>{
    expect(isStructuralRun(envelope(trie))).toBe(true);expect(isStructuralRun(envelope({...trie,node_count:3}))).toBe(false);expect(isStructuralRun(envelope({...trie,input_traces:2}))).toBe(false);expect(isStructuralRun({...envelope(trie),cases:[cases[1],cases[0],cases[2]]})).toBe(false);const wrong=structuredClone(trie);wrong.root.children[0].depth=2;expect(isStructuralRun(envelope(wrong))).toBe(false);
  });
  it("validates actual interval event indices against the source including repeated IDs",()=>{
    expect(isStructuralRun(envelope(intervals))).toBe(true);expect(isStructuralRun(envelope({...intervals,intervals:[{...intervals.intervals[0],trace_index:2},intervals.intervals[1]]}))).toBe(false);expect(isStructuralRun(envelope({...intervals,intervals:[{...intervals.intervals[0],case_id:"other"},intervals.intervals[1]]}))).toBe(false);expect(isStructuralRun(envelope({...intervals,point_matches:[9]}))).toBe(false);
  });
  it("retains native match indices without inferring them from serialized boundaries",()=>{
    expect(intervalIndices(intervals,"all")).toEqual([0,1]);expect(intervalIndices(intervals,"point")).toEqual([1]);expect(intervalIndices(intervals,"range")).toEqual([0,1]);expect(intervalIndices(intervals,"either")).toEqual([0,1]);const layout=intervalTimeline(intervals,[1]);expect(layout.rows[0]).toMatchObject({index:1,point:true,range:true});expect(layout.rows[0].interval.trace_index).toBe(1);expect(layout.queryPoint).not.toBeNull();expect(epochText(intervals.intervals[0].start)).toContain("2026-10-01T09:00:00.123Z");expect(intervalTimeline({...intervals,point_query:0},[0]).queryPoint).toBeNull();
  });
  it("exports every interval and exact scalar boundaries independent of a filtered view",async()=>{
    const csv=await structuralCsv(run(intervals)).text();expect(csv.split("\r\n")).toHaveLength(4);expect(csv).toContain('"1790845200.123446","1790848800.123466","false","true"');expect(csv).toContain('"1","1","same","0","1","B 東京"');const tree=await structuralCsv(run(trie)).text();expect(tree).toContain('"root/0/0","root/0","Review","2","true"');expect(tree.split("\r\n")).toHaveLength(6);
  });
  it("retains silent transitions, marking identities and exact same-run PNML",async()=>{
    const r=run(net);expect(isStructuralRun(r)).toBe(true);expect(await structuralPnml(r).text()).toBe(r.pnml);expect(structuralPetriLayout(r).places.length).toBeGreaterThan(0);expect(structuralReplayKey(r)).toBeUndefined();expect(structuralReplayKey({...r,activity_key:"concept:name"})).toBeUndefined();expect(structuralReplayKey({...r,activity_key:" phase 東京 "})).toBe(" phase 東京 ");const csv=await structuralCsv(r).text();expect(csv).toContain('"2","null","null","true","[2]","[1]"');expect(isStructuralRun({...r,pnml:null})).toBe(false);expect(()=>structuralPnml(run(trie))).toThrow(/no PNML/);
  });
  it("binds all three GET routes and preserves original JSON in complete reports",async()=>{
    for(const [method,result,route] of [["trie",trie,"trie"],["trie_petri",net,"trie-to-petri"],["intervals",intervals,"interval-tree"]] as const){const raw=JSON.stringify(envelope(result)).replace('"index":0','"index":0,"opaque_uint64":18446744073709551615');const fetch=vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>raw}));vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;const r=await requestStructural(method,"7",{},signal);expect(fetch.mock.calls[0][0]).toBe(`/api/${route}?source_id=7&include_source=true`);expect(fetch.mock.calls[0][1]).toEqual({signal});expect(r.data_json).toBe(raw);expect(r.report_json).toContain('"opaque_uint64":18446744073709551615');}
  });
  it("rejects stale/wrong method/selector responses and exposes native errors",async()=>{
    vi.stubGlobal("fetch",respond({...envelope(trie),source_id:"8"}));await expect(requestStructural("trie","7",{})).rejects.toThrow(/source log changed/);vi.stubGlobal("fetch",respond(envelope(trie)));await expect(requestStructural("intervals","7",{})).rejects.toThrow(/different structural method/);await expect(requestStructural("trie","7",{trie_activity_key:"phase"})).rejects.toThrow(/different activity selector/);vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,text:async()=>'{"error":"interval tree: invalid timestamp"}'})));await expect(requestStructural("intervals","7",{})).rejects.toThrow(/invalid timestamp/);
  });
  it("disables saved-analysis sources and groups all structural entry points under Transform",()=>{
    const data={dataset:"saved.json",trace_count:3,event_count:4} as MineraExport;const html=renderToStaticMarkup(createElement(StructuralWorkspace,{data,available:false,mining:false}));expect(html).toContain("Saved analysis JSON does not provide an active structural source");expect(html).toContain('<fieldset class="query-fields" disabled');for(const value of ["trie","trie_petri","intervals"])expect(html).toContain(`value="${value}"`);const hub=renderToStaticMarkup(createElement(TransformationHub,{data,available:false,mining:false,onOpenTransformed:()=>{}}));expect(hub).toContain("Tries, nets &amp; intervals");
  });
});
