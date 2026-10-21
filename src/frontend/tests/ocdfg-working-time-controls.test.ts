import {afterEach,describe,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {ocelDiscoveryQuery,requestOcelDiscovery} from "../src/api";
import {DEFAULT_OCDFG_PERFORMANCE,ocelExplorerOptions,type OcelExplorerSettings} from "../src/ocel-explorer-options";
import {OcdfgPerformanceControls} from "../src/components/OcelExplorer";

const base:OcelExplorerSettings={activityAttribute:"",objectTypeAttribute:"",annotated:true,replayDiagnostics:false};
afterEach(()=>vi.unstubAllGlobals());
describe("OCDFG working-time discovery controls",()=>{
  it("preserves omitted timing options and scopes configured timing to OCDFG",()=>{
    expect(ocelExplorerOptions("ocdfg",base)).toEqual({variant:"ocdfg"});
    const disabled={...base,ocdfgPerformance:{...DEFAULT_OCDFG_PERFORMANCE}};
    expect(ocelExplorerOptions("ocdfg",disabled)).toEqual({variant:"ocdfg"});
    const enabled={...base,ocdfgPerformance:{...DEFAULT_OCDFG_PERFORMANCE,enabled:true}};
    expect(ocelExplorerOptions("ocdfg",enabled)).toEqual({variant:"ocdfg",compute_edges_performance:true,ocdfg_business_hours:true});
    expect(ocelExplorerOptions("saw",enabled)).toEqual({variant:"saw"});
    expect(ocelExplorerOptions("etot",enabled)).toEqual({variant:"etot"});
  });
  it("distinguishes default slots, empty slots and explicit elapsed timing",()=>{
    const settings={...base,ocdfgPerformance:{...DEFAULT_OCDFG_PERFORMANCE,enabled:true,slotsMode:"custom" as "default"|"custom",slotsJson:"[]"}};
    expect(ocelExplorerOptions("ocdfg",settings).ocdfg_business_hour_slots_json).toBe("[]");
    settings.ocdfgPerformance.slotsJson="null";
    expect(ocelExplorerOptions("ocdfg",settings).ocdfg_business_hour_slots_json).toBe("null");
    settings.ocdfgPerformance.businessHours=false;
    expect(ocelExplorerOptions("ocdfg",settings).ocdfg_business_hours).toBe(false);
    expect(ocelExplorerOptions("ocdfg",settings).ocdfg_business_hour_slots_json).toBeUndefined();
    settings.ocdfgPerformance.slotsMode="default";
    expect(ocelExplorerOptions("ocdfg",settings).ocdfg_business_hour_slots_json).toBeUndefined();
  });
  it("transports literal fractional/reversed/outside-week slots and selectors unchanged",()=>{
    const raw=' \n[[25200.5,61200.125],[500,100],[-1,700000]]\r\n';
    const query=new URLSearchParams(ocelDiscoveryQuery({variant:"ocdfg",ocdfg_business_hours:false,ocdfg_business_hour_slots_json:raw,ocdfg_event_id_attribute:"",ocdfg_timestamp_attribute:" time ",ocdfg_workcalendar_json:'{"holidays":[]}'}));
    expect(query.get("ocdfg_business_hour_slots_json")).toBe(raw);expect(query.get("ocdfg_business_hours")).toBe("false");
    expect(query.get("ocdfg_event_id_attribute")).toBe("");expect(query.get("ocdfg_timestamp_attribute")).toBe(" time ");
    expect(query.get("ocdfg_workcalendar_json")).toBe('{"holidays":[]}');
    expect(()=>ocelDiscoveryQuery({ocdfg_business_hours:"yes" as unknown as boolean})).toThrow(/boolean/);
  });
  it("shows working-time options and custom JSON within the existing discovery form",()=>{
    const display=(settings:typeof DEFAULT_OCDFG_PERFORMANCE)=>renderToStaticMarkup(createElement(OcdfgPerformanceControls,{settings,disabled:false,onChange:vi.fn()}));
    const hidden=display(DEFAULT_OCDFG_PERFORMANCE);expect(hidden).toContain("Configure edge performance timing");expect(hidden).not.toContain("Weekly slots JSON");
    const html=display({...DEFAULT_OCDFG_PERFORMANCE,enabled:true,slotsMode:"custom",slotsJson:"[]"});
    expect(html).toContain("Count only working time");expect(html).toContain("Weekly slots JSON");expect(html).toContain("Monday 00:00");
    expect(html).toContain("local wall time");expect(html).toContain("empty list counts zero");expect(html).toContain("Overlapping slots are merged");
  });
  it("sends timing settings with the immutable source upload and abort signal",async()=>{
    const fetch=vi.fn(async()=>({ok:true,json:async()=>({family:"OCEL_DISCOVERY",variant:"ocdfg",result:{activities:[],object_types:[],activities_indep:{},activities_ot:{},edges:{},performance_computed:true}})}));
    vi.stubGlobal("fetch",fetch);const file=new File(['{"ocel:events":{},"ocel:objects":{}}'],"source.json"),signal=new AbortController().signal;
    await requestOcelDiscovery(file,{variant:"ocdfg",ocdfg_business_hours:true,ocdfg_business_hour_slots_json:"[]"},signal);
    const [url,init]=fetch.mock.calls[0] as unknown as [string,RequestInit];
    expect(new URL(url,"http://localhost").searchParams.get("ocdfg_business_hour_slots_json")).toBe("[]");
    expect(init.signal).toBe(signal);expect(await ((init.body as FormData).get("log") as File).text()).toBe(await file.text());
  });
});
