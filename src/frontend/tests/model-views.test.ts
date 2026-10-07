import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { modelGraphLayout,type ModelGraph } from "../src/model-graph";
import { socialNetworkGraph,transitionStateLabel,transitionSystemGraph } from "../src/model-view-data";
import { DEFAULT_SOCIAL_NETWORK,SOCIAL_NETWORK_VARIANTS,socialNetworkOptions } from "../src/social-network-options";
import { ModelGraph as GraphView } from "../src/components/ModelGraph";
import { TransitionSystemModel } from "../src/components/TransitionSystemModel";
import { SocialNetwork } from "../src/components/SocialNetwork";
import { requestOrganizationalSna } from "../src/api";
import { MINER_GROUPS } from "../src/miners";
import { type MineraExport,type OrganizationalSnaResult,type TransitionSystemExport } from "../src/types";

const activities=[{name:"A"},{name:"B"}] as MineraExport["activities"];
function system():TransitionSystemExport{return {name:"contexts",view:"sequence",direction:"forward",window:2,include_data:true,states:[{id:91,payload:[],in_root:true,name_order:[],incoming:[],outgoing:[0],start_count:2,end_count:0,outgoing_occurrences:[{trace_index:0,event_index:0}]},{id:17,payload:[],in_root:false,name_order:[],incoming:[0],outgoing:[],start_count:0,end_count:2}],transitions:[{source:91,target:17,activity:null,count:2,occurrences:[{trace_index:0,event_index:0}]}]};}
function network():OrganizationalSnaResult{return {family:"ORGANIZATIONAL_SNA",variant:"jointactivities",directed:false,resources:["Ana","Ben"],edges:[{source:0,target:1,weight:-0.75},{source:1,target:0,weight:-0.75}],diagnostics:{computed:true,output_truncated:false,trace_count:2,event_count:4,resource_count:2,edge_count:2,message:"ok"}};}
afterEach(()=>vi.unstubAllGlobals());
describe("remaining model view delivery",()=>{
  it("retains distinct equal-payload state identities, outside-root endpoints and null activity",()=>{
    const graph=transitionSystemGraph(system(),activities);
    expect(graph.nodes.map(node=>node.id)).toEqual(["s91","s17"]);
    expect(graph.nodes[1].outsideRoot).toBe(true);
    expect(graph.edges[0]).toMatchObject({source:"s91",target:"s17",label:"null · 2",value:2});
    const layout=modelGraphLayout(graph,"flow");expect(layout.edges).toHaveLength(1);expect(layout.nodes[1].x).toBeGreaterThan(layout.nodes[0].x);
  });
  it("renders sequence, set and multiset payloads without dropping nulls or multiplicities",()=>{
    const model=system();const state={...model.states[0],payload:[1,null,0]};
    expect(transitionStateLabel(state,model,activities)).toBe("[B, null, A]");
    expect(transitionStateLabel(state,{...model,view:"set"},activities)).toBe("{B, null, A}");
    expect(transitionStateLabel({...state,payload:[{activity:null,count:2},{activity:0,count:3}]},{...model,view:"multiset"},activities)).toBe("{null × 2, A × 3}");
  });
  it("retains signed undirected entries and separates reciprocal/parallel routes",()=>{
    const graph=socialNetworkGraph(network());const layout=modelGraphLayout(graph,"network");
    expect(layout.edges.map(edge=>edge.value)).toEqual([-0.75,-0.75]);expect(layout.edges.every(edge=>!edge.directed)).toBe(true);
    expect(new Set(layout.edges.map(edge=>edge.path)).size).toBe(2);
    expect(modelGraphLayout(graph,"network",0.8).edges).toHaveLength(0);
    const markup=renderToStaticMarkup(createElement(GraphView,{graph,title:"Resource network",style:"network"}));
    expect(markup).toContain("negative");expect(markup).not.toContain('marker-end=');expect(markup).toContain("Dashed ties");
  });
  it("keeps cycles, multiple self loops and empty graphs finite",()=>{
    const graph:ModelGraph={nodes:[{id:"n",label:"state"}],edges:[{id:"a",source:"n",target:"n",label:"A",value:1,directed:true},{id:"b",source:"n",target:"n",label:"B",value:3,directed:true}]};
    for(const style of ["flow","network"] as const){const layout=modelGraphLayout(graph,style);expect(new Set(layout.edges.map(edge=>edge.path)).size).toBe(2);expect(JSON.stringify(layout)).not.toMatch(/NaN|Infinity/);expect(layout.fit.w).toBeGreaterThan(0);}
    expect(modelGraphLayout({nodes:[],edges:[]},"flow").fit.w).toBeGreaterThan(0);
    const chain:ModelGraph={nodes:Array.from({length:10},(_,i)=>({id:`s${i}`,label:`state ${i}`,start:i===0})),edges:Array.from({length:9},(_,i)=>({id:`e${i}`,source:`s${i}`,target:`s${i+1}`,label:"next",value:1,directed:true}))};
    const wrapped=modelGraphLayout(chain,"flow");expect(wrapped.fit.w).toBeLessThan(1400);expect(new Set(wrapped.nodes.map(node=>node.y)).size).toBeGreaterThan(1);expect(wrapped.edges).toHaveLength(9);
  });
  it("bounds only the diagram and brings an off-page selected node into view",()=>{
    const graph:ModelGraph={nodes:Array.from({length:130},(_,i)=>({id:`n${i}`,label:`node ${i}`})),edges:[{id:"e",source:"n129",target:"n128",label:"1",value:1,directed:true}]};
    const snapshot=JSON.stringify(graph),layout=modelGraphLayout(graph,"flow",0,"n129");
    expect(layout.totalNodes).toBe(130);expect(layout.nodes).toHaveLength(120);expect(layout.nodes.some(node=>node.id==="n129")).toBe(true);expect(layout.edges).toHaveLength(1);expect(JSON.stringify(graph)).toBe(snapshot);
    const dense={...graph,edges:graph.nodes.slice(0,129).map(node=>({id:node.id,source:node.id,target:"n129",label:"1",value:1,directed:true}))};
    expect(modelGraphLayout(dense,"network",0,"n129").nodes.some(node=>node.id==="n129")).toBe(true);
    const capped={nodes:graph.nodes.slice(0,3),edges:[...Array.from({length:400},(_,i)=>({id:`other${i}`,source:"n0",target:"n1",label:"1",value:1,directed:true})),{id:"selected",source:"n2",target:"n1",label:"2",value:2,directed:true}]};
    expect(modelGraphLayout(capped,"flow",0,"n2").edges.some(edge=>edge.id==="selected")).toBe(true);
  });
  it("exposes transition discovery and native state data without inventing Petri operations",()=>{
    expect(MINER_GROUPS.find(group=>group.id==="transition_system")?.variants[0].artifact).toBe("transition-system");
    const data={activities} as MineraExport;
    const markup=renderToStaticMarkup(createElement(TransitionSystemModel,{model:system(),data}));
    expect(markup).toContain("State details");expect(markup).toContain("Native state data");expect(markup).toContain("State 17");expect(markup).toContain("Download JSON");expect(markup).not.toContain("PNML");
  });
  it("exposes all eight SNA variants and scopes case/gap/decay settings to native support",()=>{
    expect(SOCIAL_NETWORK_VARIANTS).toHaveLength(8);
    const working=socialNetworkOptions({...DEFAULT_SOCIAL_NETWORK,variant:"working_together",gap:"",beta:"",caseId:"wrong"});
    expect(working.sna_case_id_attribute).toBeUndefined();expect(working.sna_n).toBeUndefined();expect(working.sna_beta).toBeUndefined();
    const table=socialNetworkOptions({...DEFAULT_SOCIAL_NETWORK,variant:"subcontracting_pandas",caseId:"case:team",beta:"0.5",gap:"3"});
    expect(table).toMatchObject({sna_case_id_attribute:"case:team",sna_beta:0.5,sna_n:3});
    expect(()=>socialNetworkOptions({...DEFAULT_SOCIAL_NETWORK,beta:"1.2"})).toThrow(/decay/);
    expect(()=>socialNetworkOptions({...DEFAULT_SOCIAL_NETWORK,variant:"subcontracting",gap:"1.5"})).toThrow(/gap/);
    expect(socialNetworkOptions({...DEFAULT_SOCIAL_NETWORK,variant:"jointactivities_pandas",caseId:"not-supported"}).sna_case_id_attribute).toBeUndefined();
  });
  it("disables native SNA requests from static exports and propagates cancellation",async()=>{
    const markup=renderToStaticMarkup(createElement(SocialNetwork,{available:false,mining:false}));expect(markup).toContain('disabled=""');expect(markup).toContain("Open a CSV or XES log");
    const fetch=vi.fn(async()=>({ok:true,json:async()=>network()}));vi.stubGlobal("fetch",fetch);
    const signal=new AbortController().signal;expect(await requestOrganizationalSna({sna_variant:"jointactivities"},signal)).toEqual(network());
    expect(fetch.mock.calls[0]).toEqual(["/api/organizational-sna?sna_variant=jointactivities",{method:"POST",signal}]);
  });
});
