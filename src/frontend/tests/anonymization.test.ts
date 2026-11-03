import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { AnonymizationWorkspace } from "../src/components/AnonymizationWorkspace";
import { attributeOptions, variantOptions, isAnonymizationRelease, requestVariantRelease, requestAttributeRelease, releasedFile, releaseEventCount, type VariantForm, type PripelForm } from "../src/anonymization";
import { type MineraExport } from "../src/types";

const variantForm:VariantForm={variant:"laplace",epsilon:"0.5",length:"3",threshold:"2",noise:"seeded",seed:"17",traces:"100",variants:"200"};
const pripelForm:PripelForm={epsilon:"1",noise:"secure",seed:"ignored",traces:"100",distance:"3",blocklist:"cost\ncase,id\npath\\name\ncost"};
const result={family:"TRACE_VARIANT_QUERY",variant:"laplace",epsilon:0.5,k:3,p:2,seed:17,frequencies:[{activities:["Review 東京"],count:1}],diagnostics:{candidate_count:2,retained_variant_count:1,released_trace_count:1,output_truncated:false,message:"Released"},log:{traces:[{case_id:"released-1",events:[{activity:"Review 東京",timestamp:"",resource:"",lifecycle:"",input_order:0,attributes:{}}]}]}};
const envelope={family:"ANONYMIZATION_RELEASE",source_id:"7",result,released_xes:'<?xml version="1.0"?><log><trace><string key="concept:name" value="released-1"/><event><string key="concept:name" value="Review 東京"/></event></trace></log>'};
const respond=(value:unknown)=>vi.fn(async(_url:string,_init:RequestInit)=>({ok:true,text:async()=>JSON.stringify(value)}));
afterEach(()=>vi.unstubAllGlobals());
describe("anonymization delivery",()=>{
  it("validates finite epsilon, integer bounds and reproducible preview seeds",()=>{
    expect(variantOptions(variantForm)).toMatchObject({tvq_variant:"laplace",tvq_epsilon:0.5,tvq_k:3,tvq_p:2,tvq_seed:17,tvq_secure_random:false,tvq_max_output_traces:100,tvq_max_output_variants:200});
    expect(variantOptions({...variantForm,noise:"secure",seed:"ignored",length:"0"})).toMatchObject({tvq_seed:0,tvq_secure_random:true,tvq_k:0});
    for(const epsilon of ["","0","-1","Infinity","NaN"])expect(()=>variantOptions({...variantForm,epsilon})).toThrow(/Epsilon/);
    expect(()=>variantOptions({...variantForm,seed:"9007199254740992"})).toThrow(/Seed/);
    expect(()=>variantOptions({...variantForm,traces:"100001"})).toThrow(/trace limit/);
    expect(()=>variantOptions({...variantForm,length:"1.5"})).toThrow(/prefix length/);
  });
  it("preserves punctuation in blocklisted names and removes duplicates",()=>{
    expect(attributeOptions(pripelForm)).toMatchObject({pripel_secure_random:true,pripel_seed:0,pripel_blocklist:["cost","case,id","path\\name"]});
    expect(()=>attributeOptions({...pripelForm,distance:"-1"})).toThrow(/matching distance/);
  });
  it("rejects malformed releases and inconsistent released counts",()=>{
    expect(isAnonymizationRelease(envelope)).toBe(true);
    expect(isAnonymizationRelease({...envelope,released_xes:""})).toBe(false);
    expect(isAnonymizationRelease({...envelope,result:{...result,diagnostics:{...result.diagnostics,released_trace_count:2}}})).toBe(false);
  });
  it("keeps a release whose text XES cannot carry and reports the writer's refusal",()=>{
    const refused={...envelope,released_xes:null,released_xes_error:"XES cannot represent this text: \"Pr\\ufffdfung\""};
    expect(isAnonymizationRelease(refused)).toBe(true);
    expect(isAnonymizationRelease({...envelope,released_xes:null})).toBe(false);
    expect(isAnonymizationRelease({...envelope,released_xes_error:"refused"})).toBe(false);
    expect(()=>releasedFile({...refused,data_json:"",report_json:""} as never)).toThrow(/XES export is unavailable: XES cannot represent/);
  });
  it("binds a single run to its source and exports the exact returned XES and JSON",async()=>{
    const fetch=respond(envelope);vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;
    const release=await requestVariantRelease("7",variantOptions(variantForm),signal);
    const [url,init]=fetch.mock.calls[0];expect(url).toContain("source_id=7&include_xes=true");expect(url).toContain("tvq_seed=17");expect(init).toEqual({method:"POST",signal});
    expect(releaseEventCount(release)).toBe(1);expect(releasedFile(release).name).toBe("minera-laplace-released.xes");expect(await releasedFile(release).text()).toBe(envelope.released_xes);expect(release.report_json).toBe(JSON.stringify(envelope));expect(fetch).toHaveBeenCalledTimes(1);
  });
  it("forwards the released query bytes to PRIPEL with source and cancellation guards",async()=>{
    const diagnostics={epsilon:1,seed:0,query_trace_count:1,released_trace_count:1,exact_matches:1,nearest_matches:0,synthesized_traces:0,synthesized_events:0,output_truncated:false,message:"Released"};
    const fetch=respond({...envelope,result:{family:"PRIPEL",diagnostics,log:result.log}});vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;
    const query=new File([envelope.released_xes],"released.xes");const release=await requestAttributeRelease("7",query,attributeOptions(pripelForm),signal);
    const [url,init]=fetch.mock.calls[0];expect(url).toContain("source_id=7&include_xes=true");expect(url).toContain("pripel_blocklist=cost%2Ccase%5C%2Cid%2Cpath%5C%5Cname");expect(init.signal).toBe(signal);
    expect(await ((init.body as FormData).get("trace_variant_query") as File).text()).toBe(envelope.released_xes);
    expect(releasedFile(release).name).toBe("minera-pripel-released.xes");
  });
  it("retains raw reports containing integers beyond the JavaScript exact range",async()=>{
    const raw=JSON.stringify(envelope).replace('"seed":17','"seed":18446744073709551615');vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,text:async()=>raw})));
    expect((await requestVariantRelease("7",{})).report_json).toContain('"seed":18446744073709551615');
  });
  it("rejects stale sources, wrong methods, malformed responses and native errors",async()=>{
    vi.stubGlobal("fetch",respond({...envelope,source_id:"8"}));await expect(requestVariantRelease("7",{})).rejects.toThrow(/source log changed/);
    vi.stubGlobal("fetch",respond(envelope));await expect(requestAttributeRelease("7",new File(["x"],"x.csv"),{})).rejects.toThrow(/wrong anonymization method/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,text:async()=>'{"error":"unknown trace-variant query parameter"}'})));await expect(requestVariantRelease("7",{})).rejects.toThrow(/unknown/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,text:async()=>"<html>"})));await expect(requestVariantRelease("7",{})).rejects.toThrow(/invalid/);
  });
  it("exposes both variant methods, PRIPEL, entropy/preview choices and saved-source guidance",()=>{
    const data={dataset:"fixture.csv",trace_count:2,event_count:3,activities:[],edges:[],variants:[]} as MineraExport;
    const html=renderToStaticMarkup(createElement(AnonymizationWorkspace,{data,available:false,mining:false,onOpenReleased:vi.fn()}));
    for(const label of ["LAPLACE","SACOFA","Run PRIPEL","Seeded preview","OS entropy","Attributes to remove","Reload source","A saved analysis does not provide an active source log"])expect(html).toContain(label);
    expect(html).toContain("<fieldset class=\"query-fields\" disabled");
  });
});
