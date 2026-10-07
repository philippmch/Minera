import { type OcelDiscoveryResult, type PetriNetModel, type OcpnTokenReplayDiagnostics } from "./types";

export interface TokenWeight { tokens: number; frequency: number; }
export interface OcelNetView {
  key: string;
  label: string;
  net: PetriNetModel & { activities: string[] };
  variableTransitions: Set<number>;
  weights: Map<number, TokenWeight[]>;
  replay: OcpnTokenReplayDiagnostics | null;
  placeTypes: Map<number, string>;
}
export interface OcelNetModels {
  views: OcelNetView[];
  bindings: { activity: string; objectTypes: string[]; variableTypes: string[]; eventCount?: number; objectCount?: number }[];
}

const record = (v: unknown): v is Record<string, unknown> =>
  typeof v === "object" && v !== null && !Array.isArray(v);
const strings = (v: unknown): v is string[] => Array.isArray(v) && v.every(x => typeof x === "string");
const count = (v: unknown): v is number => Number.isSafeInteger(v) && (v as number) >= 0;
const name = (type: string) => type || "Unnamed object type";
const own = (value: Record<string,unknown>, key: string): unknown => Object.hasOwn(value,key) ? value[key] : undefined;

function readNet(raw: unknown): OcelNetView["net"] {
  if (!record(raw) || !count(raw.place_count) || raw.place_count === 0 ||
      !count(raw.source) || raw.source >= raw.place_count ||
      !count(raw.sink) || raw.sink >= raw.place_count || !strings(raw.activities) ||
      !Array.isArray(raw.transitions)) throw new Error("The discovered net has invalid places or activities.");
  const validPlaces = (v: unknown): v is number[] => Array.isArray(v) && v.every(p => count(p) && p < (raw.place_count as number));
  const transitions = raw.transitions.map(t => {
    if (!record(t) || !(t.activity === null || count(t.activity) && t.activity < (raw.activities as string[]).length) ||
        !validPlaces(t.inputs) || !validPlaces(t.outputs)) throw new Error("The discovered net has an invalid transition.");
    return { activity: t.activity as number | null, inputs: [...t.inputs], outputs: [...t.outputs] };
  });
  return { place_count: raw.place_count, source: raw.source, sink: raw.sink, activities: [...raw.activities], transitions };
}

function readReplay(raw: unknown, net: PetriNetModel): OcpnTokenReplayDiagnostics | null {
  if (raw === undefined) return null;
  if (!record(raw) || raw.enabled !== true || !count(raw.trace_count) || !count(raw.fitting_traces) ||
      raw.fitting_traces > raw.trace_count || !Array.isArray(raw.places) || raw.places.length !== net.place_count ||
      !Array.isArray(raw.transition_activations) || raw.transition_activations.length !== net.transitions.length ||
      !raw.transition_activations.every(count) || !raw.places.every(p => record(p) && [p.m,p.r,p.c,p.p].every(count))) {
    throw new Error("Replay diagnostics do not match the discovered net.");
  }
  return raw as unknown as OcpnTokenReplayDiagnostics;
}

/** Decode each native net with its own activity ids; never reuse another type's labels. */
export function ocelNetModels(result: OcelDiscoveryResult): OcelNetModels {
  const graph = result.result;
  const views: OcelNetView[] = [];
  const bindings: OcelNetModels["bindings"] = [];
  if (result.variant === "ocpn") {
    if (!record(graph.petri_nets)) throw new Error("The discovered model has no object-type nets.");
    const doubles = record(graph.double_arcs_on_activity) ? graph.double_arcs_on_activity : {};
    const diagnostics = record(graph.tbr_results) ? graph.tbr_results : {};
    for (const [type, raw] of Object.entries(graph.petri_nets)) {
      const net = readNet(raw);
      const rawFlags = own(doubles,type);
      const flags = record(rawFlags) ? rawFlags : {};
      const variableTransitions = new Set<number>();
      net.transitions.forEach((t,i) => { if (t.activity !== null && flags[net.activities[t.activity]] === true) variableTransitions.add(i); });
      views.push({ key: `type:${type}`, label: name(type), net, variableTransitions, weights: new Map(),
        replay: readReplay(own(diagnostics,type),net), placeTypes: new Map<number,string>(Array.from({length:net.place_count},(_,i):[number,string] => [i,type])) });
    }
    const fixedBindings = record(graph.activity_object_types) ? graph.activity_object_types : {};
    const variableBindings = record(graph.activity_variable_object_types) ? graph.activity_variable_object_types : {};
    for (const activity of new Set([...Object.keys(fixedBindings),...Object.keys(variableBindings)])) {
      const types = own(fixedBindings,activity) ?? [];
      const variable = own(variableBindings,activity) ?? [];
      if (!strings(types) || !strings(variable)) throw new Error("Activity bindings contain invalid object types.");
      const independent = record(graph.annotations) && record(graph.annotations.activities_indep) ? own(graph.annotations.activities_indep,activity) : undefined;
      bindings.push({ activity, objectTypes: [...new Set([...types,...variable])], variableTypes: [...variable],
        eventCount: record(independent) && strings(independent.events) ? independent.events.length : undefined,
        objectCount: record(independent) && strings(independent.unique_objects) ? independent.unique_objects.length : undefined });
    }
  } else if (result.variant === "saw") {
    if (!record(graph.ot_saw_nets)) throw new Error("The discovered model has no stochastic nets.");
    const add = (key: string, label: string, raw: unknown, types: Map<number,string>) => {
      const net = readNet(raw);
      const weights = new Map<number,TokenWeight[]>();
      if (!record(raw) || !Array.isArray(raw.transitions)) throw new Error("The stochastic net has invalid transitions.");
      raw.transitions.forEach((t,i) => {
        if (!record(t) || !record(t.weight_distribution)) throw new Error("The stochastic net has an invalid token-weight distribution.");
        const values = Object.entries(t.weight_distribution).map(([tokens,frequency]) => {
          const value = Number(tokens);
          if (!/^\d+$/.test(tokens) || !count(value) || value === 0 || !count(frequency) || frequency === 0) throw new Error("Token weights must be positive counts.");
          return { tokens:value, frequency };
        }).sort((a,b) => a.tokens-b.tokens);
        if (values.length) weights.set(i,values);
      });
      views.push({ key,label,net,weights,variableTransitions:new Set(),replay:null,placeTypes:types });
    };
    if (graph.multi_saw_net !== undefined) {
      const types = new Map<number,string>();
      if (record(graph.multi_place_object_types)) for (const [id,type] of Object.entries(graph.multi_place_object_types)) {
        if (count(Number(id)) && typeof type === "string") types.set(Number(id),type);
      }
      add("combined","All object types",graph.multi_saw_net,types);
    }
    for (const [type,raw] of Object.entries(graph.ot_saw_nets)) {
      const net = readNet(raw);
      add(`type:${type}`,name(type),raw,new Map<number,string>(Array.from({length:net.place_count},(_,i):[number,string] => [i,type])));
    }
  } else throw new Error("Choose an object-centric Petri net or a stochastic arc-weight net.");
  return { views,bindings };
}

export function tokenWeightLabel(values: readonly TokenWeight[]): string {
  if (!values.length) return "No observations";
  const low = values[0].tokens, high = values[values.length-1].tokens;
  return low === high ? `${low} token${low===1?"":"s"}` : `${low}–${high} tokens`;
}
