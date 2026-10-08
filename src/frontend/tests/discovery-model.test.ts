import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { NativeDiscovery } from "../src/components/NativeDiscovery";
import { MINER_GROUPS } from "../src/miners";
import { DEFAULT_ILP_FORM,DEFAULT_GENETIC_FORM,ilpOptions,geneticOptions,isDiscoveryModel,requestDiscoveryModel,discoveryLayout,discoveryPnml,matrixGroups,type DiscoveryModel } from "../src/discovery-model";
import { checkModel } from "../src/api";
import { type MineraExport } from "../src/types";

const net={place_count:3,source:0,sink:2,activities:["Approve 東京","Review β"],transitions:[{activity:1,inputs:[0],outputs:[1]},{activity:0,inputs:[1],outputs:[2]}]};
const ilp={family:"ILP",net,markings:{initial:[1,0,0],final:[0,0,1]},diagnostics:{computed:true,exhausted:false,visited_nodes:3,lp_solved:2,branch_and_bound_nodes:5,message:"ok"}};
const genetic={family:"GENETIC",net,markings:ilp.markings,matrix:{activities:net.activities,input_map:[[[1]],[]],output_map:[[],[[0]]]},diagnostics:{computed:true,exhausted:false,generations_completed:2,candidates_evaluated:8,population_size:4,activity_count:2,best_fitness:1,seed:17,message:"ok"}};
const model={family:"DISCOVERY_MODEL",source_id:"7",result:ilp,pnml:'<?xml version="1.0"?><pnml><net id="owned"/></pnml>'};
const respond=(value:unknown)=>vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>JSON.stringify(value)}));
afterEach(()=>vi.unstubAllGlobals());
describe("ILP and genetic discovery delivery",()=>{
  it("registers both actual classic discovery families in the existing picker",()=>{
    for(const family of ["ilp","genetic"])expect(MINER_GROUPS.find(group=>group.id===family)?.variants[0]).toMatchObject({id:"classic",artifact:"petri-net"});
  });
  it("preserves textual selectors and validates ILP rates, boundaries and bounds",()=>{
    expect(ilpOptions({...DEFAULT_ILP_FORM,activity:"stage:label",start:"start α",end:"end β",causal:"Review=>Approve"})).toMatchObject({activity_key:"stage:label",artificial_start_activity:"start α",artificial_end_activity:"end β",causal_relation:"Review=>Approve"});
    expect(ilpOptions(DEFAULT_ILP_FORM).activity_key).toBeUndefined();
    for(const alpha of ["","NaN","-1","1.1"])expect(()=>ilpOptions({...DEFAULT_ILP_FORM,alpha})).toThrow(/Alpha/);
    expect(()=>ilpOptions({...DEFAULT_ILP_FORM,start:""})).toThrow(/boundary labels/);expect(()=>ilpOptions({...DEFAULT_ILP_FORM,nodes:"1000001"})).toThrow(/Maximum ILP nodes/);
  });
  it("exposes zero generations, all evolutionary parameters and strict tournament bounds",()=>{
    expect(geneticOptions({...DEFAULT_GENETIC_FORM,activity:"stage",timestamp:"when",caseId:"case_key",seed:"17",population:"4",generations:"0",tournament:"1"})).toMatchObject({activity_key:"stage",timestamp_key:"when",case_id_key:"case_key",seed:17,population_size:4,generations:0,elitism_min_sample:1,elitism_rate:0.1,crossover_rate:0.8,mutation_rate:0.1,max_activities:12,max_states:10000});
    expect(()=>geneticOptions({...DEFAULT_GENETIC_FORM,tournament:"32"})).toThrow(/smaller/);expect(()=>geneticOptions({...DEFAULT_GENETIC_FORM,mutation:"Infinity"})).toThrow(/Mutation/);expect(()=>geneticOptions({...DEFAULT_GENETIC_FORM,seed:"9007199254740992"})).toThrow(/Seed/);
  });
  it("uses net-local activity IDs and preserves executable arcs and silent transitions",()=>{
    const layout=discoveryLayout({...model,report_json:""} as DiscoveryModel);expect(layout.transitions.map(t=>t.label)).toEqual(["Review β","Approve 東京"]);expect(layout.arcs).toHaveLength(4);
    const silent={...model,result:{...ilp,net:{...net,transitions:[{activity:null,inputs:[0],outputs:[2]}]}}};expect(discoveryLayout({...silent,report_json:""} as DiscoveryModel).transitions[0]).toMatchObject({label:"τ",silent:true});
  });
  it("requires a PNML for computed models and preserves unavailable diagnostics",()=>{
    expect(isDiscoveryModel(model)).toBe(true);expect(isDiscoveryModel({...model,pnml:null})).toBe(false);
    const unavailable={...model,pnml:null,result:{...ilp,diagnostics:{...ilp.diagnostics,computed:false,exhausted:true}}};expect(isDiscoveryModel(unavailable)).toBe(true);expect(()=>discoveryPnml({...unavailable,report_json:""} as DiscoveryModel)).toThrow(/no model/);
  });
  it("binds requests to the source and exports the original returned PNML with reviewed parameters",async()=>{
    const fetch=respond(model);vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;const options=ilpOptions(DEFAULT_ILP_FORM);const result=await requestDiscoveryModel("ilp","7",options,signal);
    const [url,init]=fetch.mock.calls[0];expect(url).toContain("ilp_artificial_start_activity=%E2%96%B6");expect(url).toContain("source_id=7&include_pnml=true");expect(init).toEqual({method:"POST",signal});expect(await discoveryPnml(result).text()).toBe(model.pnml);
    const report=JSON.parse(result.report_json);expect(report.request.options).toEqual(options);expect(report.response).toEqual(model);expect(fetch).toHaveBeenCalledTimes(1);
    const selected=await requestDiscoveryModel("ilp","7",{activity_key:"phase"});expect(selected.activity_key).toBe("phase");
  });
  it("retains exact raw native integers in genetic reports and labels causal matrix groups",async()=>{
    const raw=JSON.stringify({...model,result:genetic}).replace('"seed":17','"seed":18446744073709551615');vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,text:async()=>raw})));const result=await requestDiscoveryModel("genetic","7",{});expect(result.report_json).toContain('"seed":18446744073709551615');
    expect(matrixGroups([[0,1],[1]],net.activities)).toBe("(Approve 東京 OR Review β) AND (Review β)");expect(matrixGroups([],net.activities)).toBe("None");
  });
  it("rejects stale sources, wrong discovery methods, native errors and malformed responses",async()=>{
    vi.stubGlobal("fetch",respond({...model,source_id:"8"}));await expect(requestDiscoveryModel("ilp","7",{})).rejects.toThrow(/source log changed/);
    vi.stubGlobal("fetch",respond({...model,result:genetic}));await expect(requestDiscoveryModel("ilp","7",{})).rejects.toThrow(/different discovery method/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,text:async()=>'{"error":"genetic safety bound exceeded"}'})));await expect(requestDiscoveryModel("genetic","7",{})).rejects.toThrow(/safety bound/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,text:async()=>"<html>"})));await expect(requestDiscoveryModel("genetic","7",{})).rejects.toThrow(/invalid/);
  });
  it("forwards the source revision with the exact PNML conformance handoff",async()=>{
    const fetch=vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,json:async()=>({model:{miner:"imported"},activities:[]})}));vi.stubGlobal("fetch",fetch);const file=new File([model.pnml],"owned.pnml");await checkModel(file,{sourceId:"source 7",pnmlPublicMarkings:true,activityKey:"phase"});const [url,init]=fetch.mock.calls[0];expect(url).toBe("/api/check-model?pnml_public_markings=true&source_id=source+7&activity_attribute=phase");expect(await ((init.body as FormData).get("model") as File).text()).toBe(model.pnml);
  });
  it("exposes each family's settings and disables mining for saved analysis JSON",()=>{
    const data={dataset:"saved.json",trace_count:2,event_count:4,activities:[],edges:[],variants:[]} as MineraExport;
    for(const family of ["ilp","genetic"] as const){const html=renderToStaticMarkup(createElement(NativeDiscovery,{family,data,available:false,mining:false}));expect(html).toContain("Saved analysis JSON does not provide an active discovery source");expect(html).toContain('<fieldset class="query-fields" disabled');expect(html).toContain(family==="ilp"?"Optional causal relations":"Tournament sample");}
  });
});
