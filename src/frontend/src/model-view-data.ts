import { type MineraExport, type OrganizationalSnaResult, type TransitionSystemExport, type TransitionSystemState } from "./types";
import { downloadBlob } from "./download";
import { type ModelGraph } from "./model-graph";

export function transitionStateLabel(state: TransitionSystemState, model: TransitionSystemExport, activities: MineraExport["activities"]): string {
  const name = (id: number | null) => id === null ? "null" : activities[id]?.name ?? `activity ${id}`;
  if (model.view === "multiset") return `{${(state.payload as {activity:number|null;count:number}[]).map(item => `${name(item.activity)} × ${item.count}`).join(", ")}}`;
  const values = (state.payload as (number|null)[]).map(name).join(", ");
  return model.view === "set" ? `{${values}}` : `[${values}]`;
}
export function transitionSystemGraph(model: TransitionSystemExport, activities: MineraExport["activities"]): ModelGraph {
  return {
    nodes: model.states.map(state => ({id:`s${state.id}`,label:transitionStateLabel(state,model,activities),detail:`state ${state.id} · start ${state.start_count} · end ${state.end_count}`,start:state.start_count>0,end:state.end_count>0,outsideRoot:!state.in_root})),
    edges: model.transitions.map((edge,i) => ({id:`t${i}`,source:`s${edge.source}`,target:`s${edge.target}`,label:`${edge.activity===null ? "null" : activities[edge.activity]?.name ?? `activity ${edge.activity}`} · ${edge.count}`,value:edge.count,directed:true})),
  };
}
export function socialNetworkGraph(result: OrganizationalSnaResult): ModelGraph {
  return {nodes:result.resources.map((name,i)=>({id:`r${i}`,label:name,detail:`resource ${i}`})),edges:result.edges.map((edge,i)=>({id:`e${i}`,source:`r${edge.source}`,target:`r${edge.target}`,label:edge.weight===null?"NaN":edge.weight.toFixed(4),value:edge.weight??0,directed:result.directed}))};
}
export function downloadModelJson(value: unknown, filename: string) {
  downloadBlob(new Blob([JSON.stringify(value,null,2)],{type:"application/json"}),filename);
}
