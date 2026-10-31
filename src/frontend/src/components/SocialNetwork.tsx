import { useEffect,useMemo,useRef,useState } from "react";
import { requestOrganizationalSna } from "../api";
import { type OrganizationalSnaResult } from "../types";
import { DEFAULT_SOCIAL_NETWORK,SOCIAL_NETWORK_VARIANTS,socialNetworkOptions,type SocialNetworkSettings } from "../social-network-options";
import { socialNetworkGraph,downloadModelJson } from "../model-view-data";
import { ModelGraph } from "./ModelGraph";

export function SocialNetwork({ available,mining }: {available:boolean;mining:boolean}) {
  const [settings,setSettings]=useState({...DEFAULT_SOCIAL_NETWORK});
  const [result,setResult]=useState<OrganizationalSnaResult|null>(null);
  const [error,setError]=useState<string|null>(null);const [pending,setPending]=useState(false);
  const [selected,setSelected]=useState<string|undefined>();const [page,setPage]=useState(0);
  const request=useRef<AbortController|null>(null);
  useEffect(()=>()=>{request.current?.abort();request.current=null;},[]);
  useEffect(()=>{if(!available || mining){request.current?.abort();request.current=null;setResult(null);setPending(false);setError(null);}},[available,mining]);
  const change=(key:keyof SocialNetworkSettings,value:string|boolean)=>{setSettings(old=>({...old,[key]:value}));setResult(null);setError(null);setSelected(undefined);};
  const run=async()=>{
    if(!available || mining || pending)return;
    let options;try{options=socialNetworkOptions(settings);}catch(cause){setError(cause instanceof Error?cause.message:String(cause));return;}
    const controller=new AbortController();request.current?.abort();request.current=controller;setPending(true);setResult(null);setError(null);
    try{const next=await requestOrganizationalSna(options,controller.signal);if(request.current===controller && !controller.signal.aborted){setResult(next);setPage(0);setSelected(undefined);}}
    catch(cause){if(request.current===controller && !controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}
  };
  const graph=useMemo(()=>result?socialNetworkGraph(result):null,[result]);
  const resource=selected&&result?result.resources[Number(selected.slice(1))]:undefined;
  const incident=selected&&result?result.edges.filter(edge=>`r${edge.source}`===selected || `r${edge.target}`===selected):[];
  return <section className="panel social-network-section">
    <div className="panel-head"><h2>Social network discovery</h2><span className="panel-meta">Resource relationships</span></div>
    <p className="model-graph-note">Choose how to compare the people or teams performing the work. Each graph uses the native metric weights.</p>
    {!available&&<p className="model-empty">Open a CSV or XES log to build a social network. Saved JSON exports retain their aggregate resource tables.</p>}
    <form onSubmit={event=>{event.preventDefault();void run();}}><fieldset className="social-network-controls" disabled={!available||mining||pending}>
      <label className="filters-field"><span>Network metric</span><select value={settings.variant} onChange={event=>change("variant",event.target.value)}>{SOCIAL_NETWORK_VARIANTS.map(variant=><option key={variant.id} value={variant.id}>{variant.label}</option>)}</select></label>
      <details><summary>Fields and metric settings</summary><div className="social-network-fields">
        <label className="filters-field"><span>Activity field</span><input value={settings.activity} onChange={event=>change("activity",event.target.value)} /></label>
        <label className="filters-field"><span>Resource field</span><input value={settings.resource} onChange={event=>change("resource",event.target.value)} /></label>
        {["handover_pandas","working_together_pandas","subcontracting_pandas"].includes(settings.variant)&&<label className="filters-field"><span>Case field</span><input value={settings.caseId} onChange={event=>change("caseId",event.target.value)} /></label>}
        {(settings.variant.startsWith("handover")||settings.variant.startsWith("subcontracting"))&&<label className="filters-field"><span>Distance decay</span><input type="number" min="0" max="1" step="0.05" value={settings.beta} onChange={event=>change("beta",event.target.value)} /></label>}
        {settings.variant.startsWith("subcontracting")&&<label className="filters-field"><span>Maximum gap</span><input type="number" min="1" max="10000" value={settings.gap} onChange={event=>change("gap",event.target.value)} /></label>}
        <label className="social-network-check"><input type="checkbox" checked={settings.normalize} onChange={event=>change("normalize",event.target.checked)} />Normalize metric</label>
      </div></details>
      <button className="btn-open" type="submit">Build network</button>
    </fieldset></form>
    {pending&&<div className="model-graph-note" role="status">Building network… <button className="btn-ghost" onClick={()=>{request.current?.abort();request.current=null;setPending(false);}}>Cancel</button></div>}
    {error&&<div className="error-banner" role="alert">{error}</div>}
    {result&&<>
      <p className="model-graph-note">{SOCIAL_NETWORK_VARIANTS.find(item=>item.id===result.variant)?.label} · {result.directed?"directed":"undirected"} · {result.diagnostics.trace_count} cases · {result.diagnostics.event_count} events</p>
      {!result.diagnostics.computed&&<p className="error-banner">{result.diagnostics.message}</p>}
      {result.diagnostics.output_truncated&&<p className="model-graph-note">The native output reached its resource/edge limit: {result.diagnostics.message}</p>}
      {graph&&<ModelGraph graph={graph} title="Resource network" style="network" selected={selected} onSelect={setSelected} actions={<button className="btn-ghost" onClick={()=>downloadModelJson(result,"minera-social-network.json")}>Download JSON</button>} />}
      {resource!==undefined&&<div className="model-resource-inspector"><h3>{resource}</h3><p>{incident.length} incident native entries. All entries appear in the table below.</p></div>}
      {!result.edges.length?<p className="model-empty">No relationships for this metric. Try another metric or check the resource field and event sequences.</p>:<>
        <div className="model-table-wrap"><table className="activity-table"><thead><tr><th>Native entry</th><th>From</th><th>To</th><th>Weight</th></tr></thead><tbody>{result.edges.slice(page*20,page*20+20).map((edge,index)=><tr key={page*20+index}><td>{page*20+index}</td><td><button className="btn-ghost" onClick={()=>setSelected(`r${edge.source}`)}>{result.resources[edge.source]}</button></td><td><button className="btn-ghost" onClick={()=>setSelected(`r${edge.target}`)}>{result.resources[edge.target]}</button></td><td>{edge.weight ?? "NaN"}</td></tr>)}</tbody></table></div>
        {result.edges.length>20&&<div className="model-pagination"><button className="btn-ghost" disabled={page===0} onClick={()=>setPage(page-1)}>Previous</button><span>Page {page+1} of {Math.ceil(result.edges.length/20)}</span><button className="btn-ghost" disabled={(page+1)*20>=result.edges.length} onClick={()=>setPage(page+1)}>Next</button></div>}
      </>}
    </>}
  </section>;
}
