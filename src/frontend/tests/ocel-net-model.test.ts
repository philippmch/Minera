import { describe, expect, it, vi, afterEach } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { ocelNetModels, tokenWeightLabel } from "../src/ocel-net-model";
import { OCEL_MODEL_VARIANTS, ocelExplorerOptions } from "../src/ocel-explorer-options";
import { computePetriLayout } from "../src/petri-layout";
import { OcelNetModel } from "../src/components/OcelNetModel";
import { OcelExplorer } from "../src/components/OcelExplorer";
import { ocelDiscoveryQuery, requestOcelDiscovery } from "../src/api";
import { type OcelDiscoveryResult } from "../src/types";

function net(label: string) { return {place_count:2,source:0,sink:1,activities:[label],transitions:[{activity:0,inputs:[0],outputs:[1]}]}; }
function ocpn(): OcelDiscoveryResult {
  return {family:"OCEL_DISCOVERY",variant:"ocpn",result:{petri_nets:{Order:net("Create"),Item:net("Pack")},
    double_arcs_on_activity:{Item:{Pack:true}},tbr_results:{Item:{enabled:true,trace_count:2,fitting_traces:1,places:[{m:1,r:0,c:2,p:2},{m:0,r:1,c:0,p:2}],transition_activations:[2]}},
    activity_object_types:{Create:["Order"],Pack:["Item","Order"]},activity_variable_object_types:{Pack:["Item"]},
    annotations:{activities_indep:{Pack:{events:["e0","e1"],unique_objects:["i0","i1","o0"]}}}}};
}
function saw(): OcelDiscoveryResult {
  const n = {...net("Pack"),transitions:[{activity:0,inputs:[0],outputs:[1],weight_distribution:{"3":2,"1":5}}]};
  return {family:"OCEL_DISCOVERY",variant:"saw",result:{ot_saw_nets:{Item:n},multi_saw_net:n,multi_place_object_types:{"0":"Item","1":"Item"}}};
}
const settings = {activityAttribute:" selected_activity ",objectTypeAttribute:" selected_type ",annotated:false,replayDiagnostics:true};
afterEach(() => vi.unstubAllGlobals());

describe("object-centric model delivery", () => {
  it("exposes all five discovery choices with model-specific controls", () => {
    expect(OCEL_MODEL_VARIANTS.map(v => v.id)).toEqual(["ocdfg","etot","otg","ocpn","saw"]);
    expect(new URLSearchParams(ocelDiscoveryQuery(ocelExplorerOptions("ocpn",settings))).get("ocpn_annotation")).toBe("none");
    expect(ocelExplorerOptions("saw",settings)).toEqual({variant:"saw",saw_activity_attribute:"selected_activity",saw_object_type_attribute:"selected_type"});
    expect(ocelExplorerOptions("etot",settings)).toEqual({variant:"etot"});
    const markup = renderToStaticMarkup(createElement(OcelExplorer,{result:ocpn(),busy:true,onDiscover:() => {}}));
    expect(markup).toContain('value="saw"');expect(markup).toContain('value="ocpn"');expect(markup).toContain("Replay diagnostics");expect(markup).toContain("disabled");
  });
  it("keeps each object's local activity table aligned with transitions and arcs", () => {
    const views = ocelNetModels(ocpn()).views;
    const names = views.map(v => computePetriLayout(v.net,{activities:v.net.activities.map(name => ({name}))}).transitions[0].label);
    expect(names).toEqual(["Create","Pack"]);
    expect(views[0].variableTransitions.size).toBe(0);expect([...views[1].variableTransitions]).toEqual([0]);
    const layout = computePetriLayout(views[1].net,{activities:[{name:"Pack"}]});
    expect(layout.arcs.map(a => [a.transition,a.place,a.input])).toEqual([[0,0,true],[0,1,false]]);
    expect(layout.arcs.every(a => !/NaN|Infinity/.test(a.path))).toBe(true);
  });
  it("retains shared-activity bindings, association counts and optional replay counters", () => {
    const model = ocelNetModels(ocpn());
    expect(model.bindings[1]).toEqual({activity:"Pack",objectTypes:["Item","Order"],variableTypes:["Item"],eventCount:2,objectCount:3});
    expect(model.views[0].replay).toBeNull();expect(model.views[1].replay?.places[0].m).toBe(1);
    const item = {...ocpn(),result:{...ocpn().result,petri_nets:{Item:net("Pack")}}};
    const markup = renderToStaticMarkup(createElement(OcelNetModel,{result:item}));
    expect(markup).toContain("Replay diagnostics");expect(markup).toContain("1 of 2 object traces fit");expect(markup).toContain('petri-arc variable');expect(markup).toContain("2 firings");
  });
  it("includes activities with only variable object bindings", () => {
    const result = ocpn();result.result.activity_variable_object_types = {Pack:["Item"],"Variable-only":["Item"]};
    const model = ocelNetModels(result);
    expect(model.bindings.find(b => b.activity === "Variable-only")?.objectTypes).toEqual(["Item"]);
    expect(model.bindings.find(b => b.activity === "Variable-only")?.variableTypes).toEqual(["Item"]);
  });
  it("shows no stochastic net for an OCEL without object types", () => {
    const empty: OcelDiscoveryResult = {family:"OCEL_DISCOVERY",variant:"saw",result:{ot_saw_nets:{},multi_saw_net:{place_count:0,source:0,sink:0,activities:[],transitions:[]},multi_place_object_types:{}}};
    expect(ocelNetModels(empty).views).toEqual([]);
  });
  it("supports combined and per-type stochastic nets without losing weight frequencies", () => {
    const views = ocelNetModels(saw()).views;
    expect(views.map(v => v.key)).toEqual(["combined","type:Item"]);
    expect(views[0].weights.get(0)).toEqual([{tokens:1,frequency:5},{tokens:3,frequency:2}]);
    expect(views[0].placeTypes.get(0)).toBe("Item");expect(tokenWeightLabel(views[0].weights.get(0)!)).toBe("1–3 tokens");
    const markup = renderToStaticMarkup(createElement(OcelNetModel,{result:saw()}));
    expect(markup).toContain("Stochastic arc-weight net");expect(markup).toContain("Token-weight distributions");expect(markup).toContain("Observed frequency");
  });
  it("rejects invalid ids, misaligned diagnostics and unsafe weight counts", () => {
    const bad = ocpn();(bad.result.petri_nets as Record<string,ReturnType<typeof net>>).Order.transitions[0].outputs = [99];
    expect(() => ocelNetModels(bad)).toThrow(/invalid transition/);
    const wrong = ocpn();(wrong.result.tbr_results as Record<string,{places:unknown[]}>).Item.places = [];
    expect(() => ocelNetModels(wrong)).toThrow(/do not match/);
    const unsafe = saw();(unsafe.result.multi_saw_net as {transitions:{weight_distribution:Record<string,number>}[]}).transitions[0].weight_distribution = {"9007199254740992":1};
    expect(() => ocelNetModels(unsafe)).toThrow(/positive counts/);
  });
  it("handles empty models and object types whose names shadow object properties", () => {
    const empty: OcelDiscoveryResult = {family:"OCEL_DISCOVERY",variant:"ocpn",result:{petri_nets:{},tbr_results:{}}};
    expect(renderToStaticMarkup(createElement(OcelNetModel,{result:empty}))).toContain("No object-type nets");
    const result: OcelDiscoveryResult = {family:"OCEL_DISCOVERY",variant:"ocpn",result:{petri_nets:{constructor:net("toString")},tbr_results:{},double_arcs_on_activity:{}}};
    expect(ocelNetModels(result).views[0].replay).toBeNull();
  });
  it("sends the chosen controls and cancellation signal in the real API request", async () => {
    const controller = new AbortController();const calls: {url:string;init:RequestInit}[]=[];
    vi.stubGlobal("fetch",async (url:string,init:RequestInit) => {calls.push({url,init});return {ok:true,json:async() => ocpn()};});
    await requestOcelDiscovery(new Blob(["{}"]),ocelExplorerOptions("ocpn",settings),controller.signal);
    expect(calls[0].url).toContain("diagnostics_with_token_based_replay=true");expect(calls[0].url).toContain("ocpn_activity_attribute=selected_activity");
    expect(calls[0].init.signal).toBe(controller.signal);expect((calls[0].init.body as FormData).get("log")).not.toBeNull();
  });
  it("reports the backend's selector error so users can correct their settings", async () => {
    vi.stubGlobal("fetch",async() => ({ok:false,status:400,json:async() => ({error:"Missing selected activity attribute"})}));
    await expect(requestOcelDiscovery(new Blob(["{}"]),{variant:"ocpn"})).rejects.toThrow("Missing selected activity attribute");
  });
});
