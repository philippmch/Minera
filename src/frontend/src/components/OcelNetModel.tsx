import { useMemo, useState } from "react";
import { type OcelDiscoveryResult } from "../types";
import { ocelNetModels, tokenWeightLabel, type OcelNetView } from "../ocel-net-model";
import { computePetriLayout } from "../petri-layout";
import { PetriNet } from "./PetriNet";

const COLORS = ["#427a81", "#b35324", "#718054", "#8067a4", "#a17c35"];

export function OcelNetModel({ result }: { result: OcelDiscoveryResult }) {
  const decoded = useMemo(() => {
    try { return { model: ocelNetModels(result), error: null }; }
    catch (cause) { return { model: null, error: cause instanceof Error ? cause.message : "Could not display this model." }; }
  }, [result]);
  const [selected, setSelected] = useState<string | null>(null);
  const [showWeights, setShowWeights] = useState(true);
  if (decoded.error || !decoded.model) return <section className="panel" role="alert">{decoded.error}</section>;
  const { views, bindings } = decoded.model;
  const view = views.find(v => v.key === selected) ?? views[0];
  if (!view) return <section className="panel"><h2>No object-type nets</h2><p>Upload a log containing objects and their event relationships to discover a net.</p></section>;
  const types = [...new Set(views.flatMap(v => [...v.placeTypes.values()]))].sort();
  const colors = new Map(types.map((t,i) => [t,COLORS[i % COLORS.length]]));
  return <>
    <div className="ocel-view-toolbar">
      <label className="threshold"><span className="threshold-label">object type</span>
        <select aria-label="Object type net" value={view.key} onChange={e => setSelected(e.target.value)}>
          {views.map(v => <option key={v.key} value={v.key}>{v.label}</option>)}
        </select>
      </label>
      {types.map(t => <span key={t} className="ocel-type-key"><i style={{background:colors.get(t)}} />{t || "Unnamed object type"}</span>)}
      {result.variant === "saw" && <label className="ocel-check"><input type="checkbox" checked={showWeights} onChange={e => setShowWeights(e.target.checked)} />Token weights</label>}
      {view.variableTransitions.size > 0 && <span className="ocel-variable-key">Dashed arcs: variable object count</span>}
    </div>
    <NetView view={view} colors={colors} showWeights={showWeights} variant={result.variant} />
    {view.replay && <section className="panel ocel-net-details">
      <div className="panel-head"><h2>Replay diagnostics</h2><span className="panel-meta">{view.replay.fitting_traces} of {view.replay.trace_count} object traces fit</span></div>
      <div className="ocel-table-scroll"><table className="ocel-net-table"><thead><tr><th>Place</th><th>Missing</th><th>Remaining</th><th>Consumed</th><th>Produced</th></tr></thead>
        <tbody>{view.replay.places.map((p,i) => <tr key={i}><th scope="row">p{i}</th><td>{p.m}</td><td>{p.r}</td><td>{p.c}</td><td>{p.p}</td></tr>)}</tbody>
      </table></div>
    </section>}
    {result.variant === "saw" && <section className="panel ocel-net-details">
      <div className="panel-head"><h2>Token-weight distributions</h2><span className="panel-meta">{view.label}</span></div>
      {view.weights.size === 0 ? <p>No token-weight observations for this net.</p> : <div className="ocel-table-scroll"><table className="ocel-net-table">
        <thead><tr><th>Transition</th><th>Tokens per firing</th><th>Observed frequency</th></tr></thead>
        <tbody>{[...view.weights].flatMap(([id,weights]) => weights.map(w => <tr key={`${id}:${w.tokens}`}><th scope="row">{view.net.transitions[id].activity === null ? `Silent step t${id}` : view.net.activities[view.net.transitions[id].activity!]} <small>t{id}</small></th><td>{w.tokens}</td><td>{w.frequency}</td></tr>))}</tbody>
      </table></div>}
    </section>}
    {bindings.length > 0 && <section className="panel ocel-net-details">
      <div className="panel-head"><h2>Activity bindings</h2><span className="panel-meta">{result.result.annotations === null ? "Unannotated model" : "Annotated model"}</span></div>
      <div className="ocel-table-scroll"><table className="ocel-net-table"><thead><tr><th>Activity</th><th>Object types</th><th>Variable object count</th><th>Events</th><th>Objects</th></tr></thead>
        <tbody>{bindings.map(b => <tr key={b.activity}><th scope="row">{b.activity || "Unnamed activity"}</th><td>{b.objectTypes.map(t => t || "Unnamed object type").join(", ") || "—"}</td><td>{b.variableTypes.map(t => t || "Unnamed object type").join(", ") || "—"}</td><td>{b.eventCount ?? "—"}</td><td>{b.objectCount ?? "—"}</td></tr>)}</tbody>
      </table></div>
    </section>}
  </>;
}

function NetView({ view,colors,showWeights,variant }: { view:OcelNetView; colors:Map<string,string>; showWeights:boolean; variant:string }) {
  const layout = useMemo(() => computePetriLayout(view.net,{activities:view.net.activities.map(name => ({name}))}),[view.net]);
  const annotations = useMemo(() => {
    const transitionLabels = new Map<number,string>();
    const transitionTooltips = new Map<number,string>();
    const placeTooltips = new Map<number,string>();
    const placeColors = new Map<number,string>();
    for (const id of view.variableTransitions) transitionLabels.set(id,"Variable count");
    for (const [id,weights] of view.weights) {
      if (showWeights) transitionLabels.set(id,tokenWeightLabel(weights));
      transitionTooltips.set(id,weights.map(w => `${w.tokens} token${w.tokens===1?"":"s"}: ${w.frequency} observations`).join("; "));
    }
    if (view.replay) view.replay.transition_activations.forEach((n,id) => {
      transitionLabels.set(id,`${n} firings`);
      transitionTooltips.set(id,`${n} replay activations${view.variableTransitions.has(id)?"; variable object count":""}`);
    });
    for (const [id,type] of view.placeTypes) {
      const p = view.replay?.places[id];
      placeTooltips.set(id,`${type || "Unnamed object type"}; p${id}${p?`; missing ${p.m}, remaining ${p.r}, consumed ${p.c}, produced ${p.p}`:""}`);
      placeColors.set(id,colors.get(type)!);
    }
    return { variableTransitions:view.variableTransitions,transitionLabels,transitionTooltips,placeTooltips,placeColors };
  },[view,colors,showWeights]);
  return <PetriNet layout={layout} heading={variant === "saw" ? "Stochastic arc-weight net" : "Object-centric Petri net"} title={view.label} annotations={annotations} disconnectedDescription="Disconnected transitions" />;
}
