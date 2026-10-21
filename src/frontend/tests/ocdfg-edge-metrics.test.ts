import {afterEach,describe,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {ocelDiscoveryQuery,requestOcelDiscovery} from "../src/api";
import {isOcelDiscoveryResult,type OcelDiscoveryResult,type OcdfgEdgeMetricsEdge,type OcdfgEdgeMetricsReport} from "../src/types";
import {defaultOcdfgEdgeMetricsSettings,DEFAULT_OCDFG_PERFORMANCE,ocelExplorerOptions,type OcelExplorerSettings} from "../src/ocel-explorer-options";
import {OCDFG_METRIC_PROJECTIONS,downloadOcdfgEdgeMetrics,ocdfgMetricEdges,ocdfgMetricPage,ocdfgMetricRows} from "../src/ocdfg-edge-metrics";
import {OcdfgEdgeMetrics,OcdfgEdgeMetricRows} from "../src/components/OcdfgEdgeMetrics";
import {OcelExplorer,OcdfgEdgeMetricsControls} from "../src/components/OcelExplorer";

function edge(index=0,rows=30):OcdfgEdgeMetricsEdge {
  return {source_activity:`Source ${index}`,target_activity:`Target ${index}`,
    associations:Array.from({length:rows},(_,i)=>[`from ${i}`,`to ${i}`,`object ${i}`]),
    event_couples:[["from","to"]],total_objects:[["from","to","object"]],unique_objects:["object"]};
}
function report(edges=1,rows=30):OcdfgEdgeMetricsReport {
  return {object_types:[{object_type:"",edges:Array.from({length:edges},(_,i)=>edge(i,rows))}]};
}
function envelope(metrics?:OcdfgEdgeMetricsReport):OcelDiscoveryResult {
  return {family:"OCEL_DISCOVERY",variant:"ocdfg",result:{activities:[],object_types:[],activities_indep:{},activities_ot:{},edges:{},performance_computed:true},
    ...(metrics ? {edge_metrics:metrics} : {})};
}
const base:OcelExplorerSettings={activityAttribute:"",objectTypeAttribute:"",annotated:true,replayDiagnostics:false};
afterEach(()=>{vi.restoreAllMocks();vi.unstubAllGlobals();});

describe("OCDFG edge metrics delivery",()=>{
  it("keeps metrics optional and guards every nested string tuple without rejecting empty identities",()=>{
    expect(isOcelDiscoveryResult(envelope())).toBe(true);
    const literal=report();literal.object_types[0].edges[0]={source_activity:"",target_activity:" A\nΩ\0 ",
      associations:[["","",""],["","",""]],event_couples:[["",""]],total_objects:[["","",""]],unique_objects:[""]};
    expect(isOcelDiscoveryResult(envelope(literal))).toBe(true);
    for(const invalid of [null,[],{}, {object_types:null},{object_types:[null]},
      {object_types:[{object_type:3,edges:[]}]},{object_types:[{object_type:"T",edges:[{}]}]}])
      expect(isOcelDiscoveryResult({...envelope(),edge_metrics:invalid})).toBe(false);
    for(const patch of [{source_activity:null},{target_activity:1},{associations:[["a","b"]]},
      {associations:[["a","b","c","extra"]]},{associations:[["a",null,"o"]]},
      {event_couples:[["a","b","o"]]},{total_objects:[["a",1,"o"]]},
      {unique_objects:[false]},{unique_objects:null}]) {
      const value=report();Object.assign(value.object_types[0].edges[0],patch);
      expect(isOcelDiscoveryResult(envelope(value))).toBe(false);
    }
    expect(isOcelDiscoveryResult({...envelope(report()),variant:"otg"})).toBe(false);
  });

  it("preserves previous defaults and model settings while scoping association options to OCDFG",()=>{
    const disabled={...base,ocdfgEdgeMetrics:defaultOcdfgEdgeMetricsSettings()};
    expect(ocelExplorerOptions("ocdfg",disabled)).toEqual({variant:"ocdfg"});
    const metrics=defaultOcdfgEdgeMetricsSettings();metrics.enabled=true;
    metrics.roles.eventId={custom:true,key:""};metrics.roles.activity={custom:true,key:" A\r\nΩ\0 "};
    const settings={...base,activityAttribute:" selected ",ocdfgEdgeMetrics:metrics,
      ocdfgPerformance:{...DEFAULT_OCDFG_PERFORMANCE,enabled:true,businessHours:false}};
    expect(ocelExplorerOptions("ocdfg",settings)).toEqual({variant:"ocdfg",include_edge_metrics:true,
      ocdfg_edge_metrics_event_id_key:"",ocdfg_edge_metrics_activity_key:" A\r\nΩ\0 ",
      ocdfg_activity_attribute:"selected",compute_edges_performance:true,ocdfg_business_hours:false});
    for(const variant of ["etot","otg","ocpn","saw"] as const)
      expect(ocelExplorerOptions(variant,settings)).toEqual(ocelExplorerOptions(variant,{...settings,ocdfgEdgeMetrics:undefined}));
    metrics.enabled=false;
    expect(ocelExplorerOptions("ocdfg",settings).include_edge_metrics).toBeUndefined();
    expect(metrics.roles.eventId.key).toBe("");
  });

  it("transports all five literal selector names losslessly and omits inactive overrides",()=>{
    const options={variant:"ocdfg" as const,include_edge_metrics:true,
      ocdfg_edge_metrics_event_id_key:"",ocdfg_edge_metrics_object_id_key:" object ",
      ocdfg_edge_metrics_object_type_key:"type\0",ocdfg_edge_metrics_activity_key:"activity\nline",
      ocdfg_edge_metrics_timestamp_key:" time\r\n ",ocdfg_business_hours:false};
    const query=new URLSearchParams(ocelDiscoveryQuery(options));
    for(const [key,value]of Object.entries(options))expect(query.get(key)).toBe(String(value));
    expect(ocelDiscoveryQuery({variant:"etot",include_edge_metrics:true,ocdfg_edge_metrics_event_id_key:""})).toBe("variant=etot");
    expect(new URLSearchParams(ocelDiscoveryQuery({...options,include_edge_metrics:false})).has("ocdfg_edge_metrics_event_id_key")).toBe(false);
    expect(()=>ocelDiscoveryQuery({variant:"ocdfg",include_edge_metrics:"yes" as unknown as boolean})).toThrow(/boolean/);
    expect(()=>ocelDiscoveryQuery({variant:"ocdfg",include_edge_metrics:true,ocdfg_edge_metrics_activity_key:3 as unknown as string})).toThrow(/string/);
  });

  it("fetches the complete optional report with immutable upload and abort signal and rejects malformed reports",async()=>{
    const body=envelope(report(27,31));
    const fetch=vi.fn(async()=>({ok:true,json:async()=>body}));vi.stubGlobal("fetch",fetch);
    const file=new File(['{"literal":"source"}'],"source.json"),signal=new AbortController().signal;
    const returned=await requestOcelDiscovery(file,{variant:"ocdfg",include_edge_metrics:true,ocdfg_edge_metrics_event_id_key:""},signal);
    expect(returned).toBe(body);expect(returned.edge_metrics?.object_types[0].edges[26].associations).toHaveLength(31);
    const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];
    const query=new URL(url,"http://localhost").searchParams;
    expect(query.get("include_edge_metrics")).toBe("true");expect(query.get("ocdfg_edge_metrics_event_id_key")).toBe("");
    expect(init.signal).toBe(signal);expect(await ((init.body as FormData).get("log") as File).text()).toBe(await file.text());
    (body.edge_metrics!.object_types[0].edges[0] as unknown as {event_couples:unknown}).event_couples=[[0,1]];
    await expect(requestOcelDiscovery(file,{variant:"ocdfg",include_edge_metrics:true})).rejects.toThrow(/invalid OCEL discovery/);
  });

  it("paginates independently without mutating ordered duplicate associations or delimiter-containing identities",()=>{
    const source=report(51,52);const item=source.object_types[0].edges[0];
    item.associations.splice(1,0,[...item.associations[0]]);
    source.object_types[0].edges[0].source_activity="A->B";source.object_types[0].edges[0].target_activity="C";
    source.object_types[0].edges[1].source_activity="A";source.object_types[0].edges[1].target_activity="B->C";
    const before=JSON.stringify(source),edges=ocdfgMetricEdges(source);
    expect(edges).toHaveLength(51);expect(edges[0].edge.source_activity).toBe("A->B");expect(edges[1].edge.source_activity).toBe("A");
    expect(ocdfgMetricPage(edges,0).rows).toHaveLength(25);expect(ocdfgMetricPage(edges,1).rows).toHaveLength(25);
    expect(ocdfgMetricPage(edges,99)).toMatchObject({page:2,total:51,offset:50});
    expect(ocdfgMetricPage([],99)).toMatchObject({page:0,pageCount:1,total:0,rows:[]});
    expect(ocdfgMetricPage(edges,NaN).page).toBe(0);
    expect(ocdfgMetricPage(ocdfgMetricRows(item,"associations"),0).rows.slice(0,2)).toEqual([item.associations[0],item.associations[0]]);
    expect(ocdfgMetricPage(ocdfgMetricRows(item,"associations"),2).rows).toHaveLength(3);
    expect(ocdfgMetricRows(item,"event_couples")).toEqual(item.event_couples);
    expect(ocdfgMetricRows(item,"total_objects")).toEqual(item.total_objects);
    expect(ocdfgMetricRows(item,"unique_objects")).toEqual([["object"]]);
    expect(JSON.stringify(source)).toBe(before);
  });

  it("renders the opt-in and literal textarea controls only in the existing OCDFG form",()=>{
    const settings=defaultOcdfgEdgeMetricsSettings();settings.enabled=true;
    settings.roles.activity={custom:true,key:" A\nΩ "};settings.roles.eventId={custom:true,key:""};
    const controls=renderToStaticMarkup(createElement(OcdfgEdgeMetricsControls,{settings,disabled:true,onChange:vi.fn()}));
    expect(controls).toContain('fieldset class="query-fields" disabled');expect(controls).toContain("Include edge association metrics");
    expect(controls).toContain('<textarea rows="2"');expect(controls).toContain('aria-label="Association activity key"');
    expect(controls).toContain(" A\nΩ ");expect(controls).toContain("empty names and line breaks");
    const html=renderToStaticMarkup(createElement(OcelExplorer,{result:envelope(report()),busy:false,onDiscover:vi.fn()}));
    expect(html).toContain("Include edge association metrics");expect(html).toContain("Configure edge performance timing");
    expect(html).toContain("Edge association metrics");
    const other=renderToStaticMarkup(createElement(OcelExplorer,{result:{...envelope(),variant:"etot"},busy:false,onDiscover:vi.fn()}));
    expect(other).not.toContain("Include edge association metrics");
  });

  it("renders explicit type/source/target/counts, caps edges and rows at25, and offers all projections and empty reports",()=>{
    const source=report(26,30);source.object_types[0].edges[0].associations[0]=["","line\nend"," Ω "];
    const html=renderToStaticMarkup(createElement(OcdfgEdgeMetrics,{report:source}));
    for(const text of ["1 object types","26 edges","780 raw association rows","Object type","Source activity","Target activity","Previous edges","Next edges","Next metric rows","Download complete edge metrics JSON"])
      expect(html).toContain(text);
    expect(html).toContain("Inspect edge 25");expect(html).not.toContain("Inspect edge 26");
    expect(html).toContain("from 24");expect(html).not.toContain("from 25");expect(html).toContain("line\\nend");
    for(const option of OCDFG_METRIC_PROJECTIONS)expect(html).toContain(`value="${option.id}"`);
    for(const projection of OCDFG_METRIC_PROJECTIONS) {
      const rows=renderToStaticMarkup(createElement(OcdfgEdgeMetricRows,{edge:source.object_types[0].edges[0],projection:projection.id,page:0,onPage:vi.fn()}));
      expect(rows).toContain(projection.id==="unique_objects"?"Object ID":"Source event ID");
    }
    expect(renderToStaticMarkup(createElement(OcdfgEdgeMetrics,{report:{object_types:[]}}))).toContain("No edge associations were returned");
  });

  it("downloads every edge and row through the existing JSON download path regardless of page limits",async()=>{
    const source=report(31,61);source.object_types[0].edges[30].associations[60]=["", " tail\nΩ\0 ", " object "];
    let blob:Blob|undefined;
    vi.spyOn(URL,"createObjectURL").mockImplementation(value=>{blob=value as Blob;return "blob:metrics";});
    const revoke=vi.spyOn(URL,"revokeObjectURL").mockImplementation(()=>{});
    const anchor={href:"",download:"",click:vi.fn()};vi.stubGlobal("document",{createElement:()=>anchor});
    downloadOcdfgEdgeMetrics(source);
    expect(anchor.download).toBe("minera-ocdfg-edge-metrics.json");expect(anchor.click).toHaveBeenCalledOnce();
    expect(revoke).toHaveBeenCalledWith("blob:metrics");expect(blob?.type).toBe("application/json");
    const restored=JSON.parse(await blob!.text()) as OcdfgEdgeMetricsReport;
    expect(restored).toEqual(source);expect(restored.object_types[0].edges).toHaveLength(31);
    expect(restored.object_types[0].edges[30].associations).toHaveLength(61);
    expect(restored.object_types[0].edges[30].associations[60]).toEqual(["", " tail\nΩ\0 ", " object "]);
  });
});
