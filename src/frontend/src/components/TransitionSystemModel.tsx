import { useEffect, useMemo, useState } from "react";
import { type MineraExport, type TransitionSystemExport } from "../types";
import { transitionStateLabel, transitionSystemGraph, downloadModelJson } from "../model-view-data";
import { ModelGraph } from "./ModelGraph";

export function TransitionSystemModel({ model, data }: { model: TransitionSystemExport; data: MineraExport }) {
  const initial = model.states.find(state => state.start_count>0)?.id ?? model.states[0]?.id;
  const [selected,setSelected] = useState(initial);
  const [query,setQuery] = useState("");const [page,setPage] = useState(0);
  useEffect(()=>{setSelected(initial);setPage(0);},[model,initial]);
  const graph = useMemo(()=>transitionSystemGraph(model,data.activities),[model,data.activities]);
  const state = model.states.find(item=>item.id===selected);
  const filtered = model.states.filter(item=>`${item.id} ${transitionStateLabel(item,model,data.activities)}`.toLowerCase().includes(query.toLowerCase()));
  return <>
    <p className="model-graph-note">{model.direction} · {model.view} · window {model.window}. Change discovery settings in Filters &amp; miner settings.</p>
    <div className="model-inspection-layout">
      <ModelGraph graph={graph} title="Transition system" minimumLabel="Minimum transition count" selected={selected===undefined ? undefined : `s${selected}`} onSelect={id=>setSelected(Number(id.slice(1)))} actions={<button className="btn-ghost" onClick={()=>downloadModelJson({activities:data.activities,transition_system:model},"minera-transition-system.json")}>Download JSON</button>} />
      <aside className="panel model-state-inspector"><div className="panel-head"><h2>State details</h2></div>{state ? <>
        <h3>State {state.id}</h3><p>{transitionStateLabel(state,model,data.activities)}</p>
        <dl><dt>Root member</dt><dd>{state.in_root ? "Yes" : "No"}</dd><dt>Starts</dt><dd>{state.start_count}</dd><dt>Ends</dt><dd>{state.end_count}</dd><dt>Incoming transition IDs</dt><dd>{state.incoming.join(", ") || "None"}</dd><dt>Outgoing transition IDs</dt><dd>{state.outgoing.join(", ") || "None"}</dd></dl>
        <details><summary>Native state data</summary><pre>{JSON.stringify(state,null,2)}</pre></details>
        {model.include_data ? <p>{state.incoming_occurrences?.length ?? 0} incoming and {state.outgoing_occurrences?.length ?? 0} outgoing event references. Native state data retains their trace/event indices.</p> : <p>Enable traversal data in discovery settings to retain event references.</p>}
      </> : <p className="model-empty">Select a state to inspect its native data.</p>}</aside>
    </div>
    <section className="panel model-state-table"><div className="panel-head"><h2>All states</h2><span className="panel-meta">{filtered.length} / {model.states.length}</span></div>
      <label className="model-graph-search">Find a state <input value={query} onChange={event=>{setQuery(event.target.value);setPage(0);}} placeholder="State ID or activity" /></label>
      <div className="model-table-wrap"><table className="activity-table"><thead><tr><th>State</th><th>Payload</th><th>Root member</th><th>Starts</th><th>Ends</th></tr></thead><tbody>{filtered.slice(page*20,page*20+20).map(item=><tr key={item.id}><td><button className="btn-ghost" onClick={()=>setSelected(item.id)}>State {item.id}</button></td><td>{transitionStateLabel(item,model,data.activities)}</td><td>{item.in_root ? "Yes" : "No"}</td><td>{item.start_count}</td><td>{item.end_count}</td></tr>)}</tbody></table></div>
      {filtered.length>20 && <div className="model-pagination"><button className="btn-ghost" disabled={page===0} onClick={()=>setPage(page-1)}>Previous</button><span>Page {page+1} of {Math.ceil(filtered.length/20)}</span><button className="btn-ghost" disabled={(page+1)*20>=filtered.length} onClick={()=>setPage(page+1)}>Next</button></div>}
      <details className="model-native-data"><summary>Native transitions · {model.transitions.length} entries</summary><pre>{JSON.stringify(model.transitions,null,2)}</pre></details>
    </section>
  </>;
}
