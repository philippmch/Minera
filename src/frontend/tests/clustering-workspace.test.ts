import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { ClusteringWorkspace } from "../src/components/ClusteringWorkspace";
import { DEFAULT_HIERARCHY,DEFAULT_PROFILE,hierarchyOptions,profileOptions,isClusterAnalysis,requestClusterAnalysis,requestClusterExport,hierarchyEntries,selectedClusterCases,dendrogramLayout,type ClusterAnalysis } from "../src/clustering-workspace";
import { profileClusteringQuery } from "../src/api";
import { type ClusteringNode,type MineraExport } from "../src/types";

const leaf=(name:string,size=1):ClusteringNode=>({name,size,distance:0,leaf:true,children:[]});
const tree:ClusteringNode={name:"root",size:4,distance:4,leaf:false,children:[leaf("root",2),{name:"root-a-b",size:2,distance:1,leaf:false,children:[leaf("a-b"),leaf("")]}]};
const cases=[{index:0,case_id:"same",event_count:2},{index:1,case_id:"same",event_count:3},{index:2,case_id:"case 2",event_count:1},{index:3,case_id:"empty",event_count:0}];
const hierarchy={family:"CLUSTER_ANALYSIS",analysis_id:"9",source_id:"7",cases,groups:[{name:"root",trace_indices:[0,1]},{name:"a-b",trace_indices:[2]},{name:"",trace_indices:[3]}],result:{family:"TRACE_ATTRIBUTE_CLUSTERING",tree,leaf_names:["root","a-b",""],diagnostics:{computed:true,output_truncated:false,trace_count:4,group_count:3,node_count:5,message:"ok"}}} as const;
const profiles={family:"CLUSTER_ANALYSIS",analysis_id:"10",source_id:"7",cases,groups:[],result:{family:"PROFILE_CLUSTERING",variant:"sklearn_profiles",labels:[0,1,0,1],clusters:[[0,2],[1,3]],feature_names:["activity:A","event:cost"],centroids:[[1,2.5],[0,4]],inertia:2,diagnostics:{computed:true,converged:true,trace_count:4,feature_count:2,cluster_count:2,iterations:3,message:"ok"},seed:17}} as const;
const analysis=(value:unknown)=>({...value as ClusterAnalysis,report_json:""});
const respond=(value:unknown)=>vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>JSON.stringify(value)}));
afterEach(()=>vi.unstubAllGlobals());
describe("clustering workflow",()=>{
  it("supports all five native hierarchy distances and preserves exact selectors",()=>{
    for(const variant of ["DMM_LEVEN","AVG_LEVEN","DMM_VEC","AVG_VEC","DFG"] as const)expect(hierarchyOptions({...DEFAULT_HIERARCHY,variant,attribute:" segment,東京 ",activity:"phase"})).toMatchObject({clustering_variant:variant,clustering_trace_attribute:" segment,東京 ",clustering_activity_attribute:"phase"});
    for(const alpha of ["","NaN","-0.1","1.1"])expect(()=>hierarchyOptions({...DEFAULT_HIERARCHY,alpha})).toThrow(/Alpha/);
    expect(()=>hierarchyOptions({...DEFAULT_HIERARCHY,percent:"0"})).toThrow(/coverage/);expect(()=>hierarchyOptions({...DEFAULT_HIERARCHY,nodes:"200001"})).toThrow(/nodes/);expect(()=>hierarchyOptions({...DEFAULT_HIERARCHY,attribute:""})).toThrow(/attributes/);
  });
  it("preserves exact profile lists and native automatic selection when lists are blank",()=>{
    const options=profileOptions({...DEFAULT_PROFILE,traceStrings:" segment,東京 \npath\\name",eventNumbers:"cost\namount",includeActivity:false,includeSuccession:false});
    expect(options.profile_trace_string_attributes).toEqual([" segment,東京 ","path\\name"]);expect(options.profile_feature_names).toBeUndefined();
    const query=new URLSearchParams(profileClusteringQuery(options));expect(query.get("profile_trace_string_attributes")).toBe(" segment\\,東京 ,path\\\\name");expect(query.has("profile_feature_names")).toBe(false);expect(query.get("profile_include_activity")).toBe("false");
    expect(()=>profileClusteringQuery(profileOptions({...DEFAULT_PROFILE,eventStrings:"resource\nresource"}))).toThrow(/duplicates/);
  });
  it("enforces profile bounds and the browser exact integer seed range",()=>{
    for(const [key,value] of [["clusters","0"],["iterations","10001"],["traces","100001"],["features","10001"],["seed","9007199254740992"],["tolerance","Infinity"]])expect(()=>profileOptions({...DEFAULT_PROFILE,[key]:value})).toThrow();
    expect(profileOptions({...DEFAULT_PROFILE,seed:"9007199254740991",tolerance:"0"})).toMatchObject({profile_seed:9007199254740991,profile_tolerance:0});
  });
  it("validates source positions and complete group memberships despite repeated case IDs",()=>{
    expect(isClusterAnalysis(hierarchy)).toBe(true);expect(isClusterAnalysis({...hierarchy,cases:[cases[1],cases[0],...cases.slice(2)]})).toBe(false);
    expect(isClusterAnalysis({...hierarchy,groups:[{name:"root",trace_indices:[0,0]},...hierarchy.groups.slice(1)]})).toBe(false);
    expect(isClusterAnalysis({...hierarchy,groups:[{name:"root",trace_indices:[0,4]},...hierarchy.groups.slice(1)]})).toBe(false);
    expect(selectedClusterCases(analysis(hierarchy),"r")).toEqual([0,1,2,3]);expect(selectedClusterCases(analysis(hierarchy),"r/0")).toEqual([0,1]);expect(selectedClusterCases(analysis(hierarchy),"r/1")).toEqual([2,3]);expect(selectedClusterCases(analysis(hierarchy),"r/1/1")).toEqual([3]);
  });
  it("requires profile memberships to agree with labels and finite centroid dimensions",()=>{
    expect(isClusterAnalysis(profiles)).toBe(true);expect(isClusterAnalysis({...profiles,result:{...profiles.result,labels:[1,1,0,1]}})).toBe(false);
    expect(isClusterAnalysis({...profiles,result:{...profiles.result,clusters:[[0,2],[1,4]]}})).toBe(false);expect(isClusterAnalysis({...profiles,result:{...profiles.result,centroids:[[1],[0,4]]}})).toBe(false);expect(isClusterAnalysis({...profiles,result:{...profiles.result,centroids:[[NaN,2],[0,4]]}})).toBe(false);
    expect(selectedClusterCases(analysis(profiles),"c/1")).toEqual([1,3]);
  });
  it("uses path identities when group names collide with root and internal node labels",()=>{
    expect(hierarchyEntries(tree).map(e=>e.id)).toEqual(["r","r/0","r/1","r/1/0","r/1/1"]);expect(new Set(hierarchyEntries(tree).map(e=>e.id)).size).toBe(5);
    const layout=dendrogramLayout(tree);expect(layout.nodes.find(e=>e.id==="r")?.x).toBe(80);expect(layout.nodes.find(e=>e.id==="r/1")?.x).toBe(545);expect(layout.nodes.filter(e=>e.node.leaf).every(e=>e.x===700)).toBe(true);expect(layout.edges).toHaveLength(4);
    const focus=dendrogramLayout(tree,"r/1");expect(focus.nodes.map(e=>e.id)).toEqual(["r/1","r/1/0","r/1/1"]);expect(focus.maximum).toBe(1);
  });
  it("retains full node memberships beyond the chart cap and handles tied and empty trees",()=>{
    const wide={name:"root",size:130,distance:0,leaf:false,children:Array.from({length:130},(_,i)=>leaf(String(i)))};const layout=dendrogramLayout(wide);expect(layout.visibleLeaves).toBe(120);expect(layout.totalLeaves).toBe(130);expect(hierarchyEntries(wide)).toHaveLength(131);expect(layout.nodes.every(e=>Number.isFinite(e.x)&&Number.isFinite(e.y))).toBe(true);
    const empty={...leaf("root",0)};expect(dendrogramLayout(empty).nodes).toEqual([]);const missing={...hierarchy,groups:[],result:{...hierarchy.result,tree:empty,leaf_names:[],diagnostics:{...hierarchy.result.diagnostics,group_count:0,node_count:1}}};expect(isClusterAnalysis(missing)).toBe(true);expect(selectedClusterCases(analysis(missing),"r")).toEqual([]);
  });
  it("binds both native requests to the source and retains the original report JSON",async()=>{
    const fetch=respond(hierarchy);vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;const options=hierarchyOptions(DEFAULT_HIERARCHY);const result=await requestClusterAnalysis("hierarchical","7",options,signal);const [url,init]=fetch.mock.calls[0];expect(url).toContain("/api/clustering?");expect(url).toContain("source_id=7&include_cases=true");expect(init).toEqual({method:"POST",signal});expect(JSON.parse(result.report_json)).toMatchObject({request:{method:"hierarchical",source_id:"7",options},response:hierarchy});
    const raw=JSON.stringify(profiles).replace('"seed":17','"seed":18446744073709551615');const profileFetch=vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>raw}));vi.stubGlobal("fetch",profileFetch);const p=await requestClusterAnalysis("profiles","7",profileOptions(DEFAULT_PROFILE));expect(p.report_json).toContain('"seed":18446744073709551615');expect(profileFetch.mock.calls[0][0]).toContain("/api/profile-clustering?");
  });
  it("rejects stale sources, mismatched methods, backend errors and invalid response text",async()=>{
    vi.stubGlobal("fetch",respond({...hierarchy,source_id:"8"}));await expect(requestClusterAnalysis("hierarchical","7",{})).rejects.toThrow(/source log changed/);
    vi.stubGlobal("fetch",respond(profiles));await expect(requestClusterAnalysis("hierarchical","7",{})).rejects.toThrow(/different clustering method/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,text:async()=>'{"error":"group bound exceeded"}'})));await expect(requestClusterAnalysis("hierarchical","7",{})).rejects.toThrow(/group bound/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,text:async()=>"<html>"})));await expect(requestClusterAnalysis("profiles","7",{})).rejects.toThrow(/invalid/);
  });
  it("exports the exact selected analysis without recomputing and forwards cancellation",async()=>{
    const blob=new Blob(["owned XES"]);const fetch=vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,blob:async()=>blob}));vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;expect(await requestClusterExport(analysis(hierarchy),"r/1/0","xes",signal)).toBe(blob);expect(fetch.mock.calls).toEqual([["/api/clustering/export?analysis_id=9&source_id=7&selection=r%2F1%2F0&format=xes",{method:"POST",signal}]]);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,json:async()=>({error:"analysis was replaced"})})));await expect(requestClusterExport(analysis(hierarchy),"r","json")).rejects.toThrow(/replaced/);
  });
  it("exposes all hierarchy methods and disables an unavailable saved-analysis source",()=>{
    const data={dataset:"saved.json",trace_count:4,event_count:6} as MineraExport;const html=renderToStaticMarkup(createElement(ClusteringWorkspace,{data,available:false,mining:false,onOpenCluster:()=>{}}));expect(html).toContain("Saved analysis JSON does not provide an active clustering source");expect(html).toContain('<fieldset class="query-fields" disabled');for(const variant of ["DMM_LEVEN","AVG_LEVEN","DMM_VEC","AVG_VEC","DFG"])expect(html).toContain(`value="${variant}"`);expect(html).toContain("Case profiles");
  });
});
