import { afterEach, describe, expect, it, vi } from "vitest";
import normal from "./fixtures/simulation-workspace-default.json?raw";
import resources from "./fixtures/simulation-workspace-resources.json?raw";
import noDiagnostics from "./fixtures/simulation-workspace-no-diagnostics.json?raw";
import imported from "./fixtures/simulation-workspace-model-imported.json?raw";
import converted from "./fixtures/simulation-workspace-transition-bordered.json?raw";
import stochasticMap from "./fixtures/simulation-workspace-stochastic-map.json?raw";
import { defaultSimulationInputs, simulationOptions, simulationTables, runSimulationWorkspace, validSimulationWorkspace, type SimulationWorkspace } from "../src/simulation-workspace";
import { monteCarloQuery } from "../src/api";
import { nativeJson, parseNativeJson } from "../src/native-json";
import { ocelPreparedCsv } from "../src/ocel-preparation";
import type { LogQuerySchema } from "../src/log-query";
const parse = (raw=normal) => parseNativeJson(raw) as SimulationWorkspace;
const schema = ():LogQuerySchema => ({family:"LOG_QUERY_SCHEMA",source_id:parse().source_id,case_count:60,event_count:180,columns:[],table_name:"dataframe",event_index_column:"index",sqlite_available:true,duckdb_configured:false});
afterEach(()=>vi.unstubAllGlobals());
describe("single-run native Monte Carlo workspace",()=>{
 it.each([normal,resources,noDiagnostics,imported,converted,stochasticMap])("accepts the actual native result and matching complete log/model",raw=>expect(validSimulationWorkspace(parse(raw))).toBe(true));
 it("rejects malformed source, result/log counts and topology",()=>{
  let value=parse();value.source_id="";expect(validSimulationWorkspace(value)).toBe(false);
  value=parse();value.result.log.event_count++;expect(validSimulationWorkspace(value)).toBe(false);
  value=parse();value.model.transitions[0].inputs=[999];expect(validSimulationWorkspace(value)).toBe(false);
  value=parse();value.result.output_places_interval_trees.pop();expect(validSimulationWorkspace(value)).toBe(false);
 });
 it("requests one source-bound native envelope instead of two simulations",async()=>{
  const fetch=vi.fn(async()=>new Response(normal));vi.stubGlobal("fetch",fetch);
  const inputs=defaultSimulationInputs();inputs.options.simulations=60;
  const run=await runSimulationWorkspace("alpha",inputs,schema());
  expect(fetch).toHaveBeenCalledTimes(1);expect(run.raw).toBe(normal);expect(run.value.xes).toBe(parse().xes);
  const report=JSON.parse(run.report);expect(report.response).toEqual(JSON.parse(normal));
  expect(report.request.parameters).toMatchObject({miner:"alpha",source_id:schema().source_id,include_workspace:"true",monte_carlo_simulations:"60"});
 });
 it("exposes all cases, events, native durations, model indices and complete interval trees",()=>{
  const value=parse(),tables=simulationTables(value);expect(tables.traces.count).toBe(60);expect(tables.events.count).toBe(180);
  expect(tables.traces.rowAt(59)[0]).toBe(59);expect(tables.events.rowAt(179).slice(0,2)).toEqual([59,2]);
  expect(tables.visible_durations.rowAt(0)[2]).toBe(4);expect(tables.scheduler_durations.rowAt(0)[1]).toBe(6);
  expect(tables.places_trees.count).toBe(value.model.place_count);expect(tables.transitions_trees.count).toBe(value.model.transitions.length);
  expect(tables.places_intervals.count).toBe(value.result.output_places_interval_trees.reduce((sum,tree)=>sum+tree.intervals.length,0));
  expect(tables.transitions_intervals.count).toBe(value.result.output_transitions_interval_trees.reduce((sum,tree)=>sum+tree.intervals.length,0));
 });
 it("preserves visible durations and intervals with legacy elapsed diagnostics disabled",()=>{
  const tables=simulationTables(parse(noDiagnostics));expect(tables.scheduler_durations.count).toBe(0);expect(tables.visible_durations.count).toBe(60);expect(tables.events.count).toBe(180);expect(tables.places_trees.count).toBe(4);
 });
 it("downloads every generated event and its complete typed attributes beyond paging",async()=>{
  const tables=simulationTables(parse());const csv=await ocelPreparedCsv(tables.events).text();expect(csv.split("\r\n")).toHaveLength(182);
  expect(tables.events.columns).toContain("attributes");expect(csv).toContain('"59","2"');expect(csv).toContain("1970-01-12T13:46:46.000000Z");
 });
 it("retains fractional native interval spelling in CSV and full reports",async()=>{
  const raw=normal.replace('"start":1000000','"start":1000000.000000001');
  const value=parse(raw),tables=simulationTables(value);expect(await ocelPreparedCsv(tables.places_intervals).text()).toContain("1000000.000000001");expect(nativeJson(value)).toContain("1000000.000000001");
 });
 it("serializes every existing typed override and preserves its native defaults",()=>{
  const inputs=defaultSimulationInputs();Object.assign(inputs.options,{process_tree_variant:"transition_bordered",start:-2,force_distribution:"uniform",token_replay_variant:"backwards",diagnostic_interval:17,small_scale_factor:8,max_thread_execution_time:3,enable_diagnostics:false,default_resources_per_place:0});
  inputs.initial="1,0";inputs.final="0,1";inputs.weights="-1,-2";inputs.resources='{"0":0,"1":8}';inputs.durations='[{"distribution":"uniform","value_seconds":2,"secondary_seconds":3},{"distribution":"gamma","value_seconds":2,"secondary_seconds":-1,"tertiary_seconds":4}]';inputs.stochasticMap="[]";
  const q=new URLSearchParams(monteCarloQuery(simulationOptions(inputs)));expect(q.get("monte_carlo_transition_weights")).toBe("-1,-2");expect(q.get("monte_carlo_transition_durations")).toBe("uniform:2;3,gamma:2;-1;4");expect(q.get("monte_carlo_resources_per_place")).toBe("0=0,1=8");expect(q.get("monte_carlo_provided_stochastic_map")).toBe("empty");expect(q.get("monte_carlo_enable_diagnostics")).toBe("false");expect(q.get("process_tree_variant")).toBe("transition_bordered");
  const native=new URLSearchParams(monteCarloQuery(simulationOptions(defaultSimulationInputs())));expect(native.has("monte_carlo_seed")).toBe(false);expect(native.has("monte_carlo_transition_durations")).toBe(false);
 });
 it("rejects malformed marking, JSON, weight and duration controls before submission",()=>{
  const inputs=defaultSimulationInputs();inputs.initial="4294967296";expect(()=>simulationOptions(inputs)).toThrow(/uint32/);inputs.initial="";inputs.weights="1,,2";expect(()=>simulationOptions(inputs)).toThrow(/finite/);inputs.weights="";inputs.resources="{";expect(()=>simulationOptions(inputs)).toThrow(/JSON/);inputs.resources="";inputs.durations='[{"distribution":"normal","value_seconds":1}]';expect(()=>simulationOptions(inputs)).toThrow(/distribution/);
 });
 it("rejects a replaced source and preserves native backend errors",async()=>{
  vi.stubGlobal("fetch",vi.fn(async()=>new Response(normal)));await expect(runSimulationWorkspace("alpha",defaultSimulationInputs(),{...schema(),source_id:"old"})).rejects.toThrow(/does not match/);
  vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"Monte Carlo resource place index is invalid"}',{status:400})));await expect(runSimulationWorkspace("alpha",defaultSimulationInputs(),schema())).rejects.toThrow(/resource place/);
 });
});
