import { useEffect, useRef, useState } from "react";
import { type MineraExport } from "../types";
import { requestLogQuerySchema, downloadQueryBlob, type LogQuerySchema } from "../log-query";
import { attributeOptions, variantOptions, requestVariantRelease, requestAttributeRelease, releasedFile, releaseEventCount, type AnonymizationRelease, type VariantForm, type PripelForm, type NoiseMode } from "../anonymization";

function NoiseFields({noise,seed,onChange}:{noise:NoiseMode;seed:string;onChange:(noise:NoiseMode,seed:string)=>void}) {
  return <><label className="filters-field"><span>Noise source</span><select value={noise} onChange={e=>onChange(e.target.value as NoiseMode,seed)}><option value="secure">OS entropy</option><option value="seeded">Seeded preview</option></select></label>{noise==="seeded"&&<label className="filters-field"><span>Seed</span><input type="number" min="0" step="1" value={seed} onChange={e=>onChange(noise,e.target.value)} /></label>}</>;
}
function ReleaseView({release,onOpen}:{release:AnonymizationRelease;onOpen:(file:File)=>void}) {
  const [page,setPage]=useState(0);const [selected,setSelected]=useState<number|null>(null);const [frequencyPage,setFrequencyPage]=useState(0);const [eventPage,setEventPage]=useState(0);
  useEffect(()=>{setPage(0);setSelected(null);setFrequencyPage(0);setEventPage(0);},[release]);
  const result=release.result;const traces=result.log.traces;const events=releaseEventCount(release);const name=result.family==="PRIPEL"?"PRIPEL":result.variant.toUpperCase();
  const chosen=selected===null?null:traces[selected];
  return <section className="query-results anonymization-results">
    <div className="panel-head"><h2>{name} release</h2><span className="panel-meta">{traces.length} cases · {events} events</span></div>
    <p>{result.diagnostics.message}</p>
    {result.diagnostics.output_truncated&&<p role="status">The output reached its limit. This preview and every export contain the same capped release.</p>}
    {!traces.length&&<p role="status">No traces were released. Adjust the query settings to try again; the empty release can still be downloaded.</p>}
    <div className="query-actions"><button className="btn-ghost" onClick={()=>downloadQueryBlob(new Blob([release.report_json],{type:"application/json"}),`minera-${name.toLowerCase()}-release.json`)}>Download release report</button><button className="btn-ghost" onClick={()=>downloadQueryBlob(releasedFile(release),releasedFile(release).name)}>Download released XES</button><button className="btn-open" disabled={events===0} onClick={()=>onOpen(releasedFile(release))}>Open released log</button></div>
    <details><summary>Release diagnostics</summary><pre>{JSON.stringify(result.diagnostics,null,2)}</pre></details>
    {result.family==="TRACE_VARIANT_QUERY"&&<details open><summary>Released activity variants · {result.frequencies.length}</summary><div className="query-table-wrap"><table className="activity-table"><thead><tr><th>Activity sequence</th><th>Frequency</th></tr></thead><tbody>{result.frequencies.slice(frequencyPage*25,frequencyPage*25+25).map((entry,index)=><tr key={index}><td>{entry.activities.length?entry.activities.join(" → "):"(empty trace)"}</td><td>{entry.count}</td></tr>)}</tbody></table></div><div className="query-actions"><button className="btn-ghost" disabled={frequencyPage===0} onClick={()=>setFrequencyPage(p=>p-1)}>Previous variants</button><span>{frequencyPage+1} / {Math.max(1,Math.ceil(result.frequencies.length/25))}</span><button className="btn-ghost" disabled={(frequencyPage+1)*25>=result.frequencies.length} onClick={()=>setFrequencyPage(p=>p+1)}>Next variants</button></div></details>}
    <h3>Released cases</h3><div className="query-table-wrap"><table className="activity-table"><thead><tr><th>Case</th><th>Events</th><th>Activity sequence</th><th>Inspect</th></tr></thead><tbody>{traces.slice(page*25,page*25+25).map((trace,index)=><tr key={page*25+index}><td>{trace.case_id}</td><td>{trace.events.length}</td><td>{trace.events.map(event=>event.activity).join(" → ")||"(empty trace)"}</td><td><button className="btn-ghost" onClick={()=>{setSelected(page*25+index);setEventPage(0);}}>Inspect case {page*25+index+1}</button></td></tr>)}</tbody></table></div>
    <div className="query-actions"><button className="btn-ghost" disabled={page===0} onClick={()=>setPage(p=>p-1)}>Previous cases</button><span>{page+1} / {Math.max(1,Math.ceil(traces.length/25))}</span><button className="btn-ghost" disabled={(page+1)*25>=traces.length} onClick={()=>setPage(p=>p+1)}>Next cases</button></div>
    {chosen&&<section className="anonymization-case"><h3>Case {chosen.case_id}</h3><p>Case attributes: {JSON.stringify(chosen.attributes??{})}</p><div className="query-table-wrap"><table className="activity-table"><thead><tr><th>Activity</th><th>Timestamp</th><th>Resource</th><th>Lifecycle</th><th>Attributes</th></tr></thead><tbody>{chosen.events.slice(eventPage*100,eventPage*100+100).map((event,index)=><tr key={index}><td>{event.activity}</td><td>{event.timestamp}</td><td>{event.resource}</td><td>{event.lifecycle}</td><td>{JSON.stringify(event.attributes??{})}</td></tr>)}</tbody></table></div>{chosen.events.length>100&&<div className="query-actions"><button className="btn-ghost" disabled={eventPage===0} onClick={()=>setEventPage(p=>p-1)}>Previous events</button><span>{eventPage+1} / {Math.ceil(chosen.events.length/100)}</span><button className="btn-ghost" disabled={(eventPage+1)*100>=chosen.events.length} onClick={()=>setEventPage(p=>p+1)}>Next events</button></div>}</section>}
  </section>;
}
export function AnonymizationWorkspace({data,available,mining,onOpenReleased}:{data:MineraExport;available:boolean;mining:boolean;onOpenReleased:(file:File)=>void}) {
  const maxLength=data.traces?.reduce((value,trace)=>Math.max(value,trace.events.length),0)??3;
  const [variants,setVariants]=useState<VariantForm>({variant:"sacofa",epsilon:"1",length:String(Math.min(20,Math.max(1,maxLength))),threshold:"1",noise:"secure",seed:"0",traces:"1000",variants:"10000"});
  const [attributes,setAttributes]=useState<PripelForm>({epsilon:"1",noise:"secure",seed:"0",traces:"1000",distance:"3",blocklist:""});
  const [schema,setSchema]=useState<LogQuerySchema|null>(null);const [variantRelease,setVariantRelease]=useState<AnonymizationRelease|null>(null);const [attributeRelease,setAttributeRelease]=useState<AnonymizationRelease|null>(null);const [queryFile,setQueryFile]=useState<File|null>(null);
  const queryInput=useRef<HTMLInputElement|null>(null);
  const [error,setError]=useState<string|null>(null);const [pending,setPending]=useState<string|null>(null);const request=useRef<AbortController|null>(null);const sourceRequest=useRef<AbortController|null>(null);
  const reset=()=>{request.current?.abort();request.current=null;setPending(null);setVariantRelease(null);setAttributeRelease(null);};
  const refresh=async()=>{reset();sourceRequest.current?.abort();setSchema(null);setError(null);if(!available||mining)return;const controller=new AbortController();sourceRequest.current=controller;try{const next=await requestLogQuerySchema(controller.signal);if(sourceRequest.current===controller&&!controller.signal.aborted)setSchema(next);}catch(cause){if(sourceRequest.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}};
  useEffect(()=>{void refresh();return()=>{request.current?.abort();sourceRequest.current?.abort();};},[available]);
  useEffect(()=>{if(mining){reset();sourceRequest.current?.abort();setSchema(null);}},[mining]);
  const disabled=!available||mining||!!pending||!schema;
  const v=(key:keyof VariantForm,value:string)=>setVariants(form=>({...form,[key]:value}));const a=(key:keyof PripelForm,value:string)=>setAttributes(form=>({...form,[key]:value}));
  const run=async(stage:"variants"|"attributes")=>{
    if(disabled||!schema)return;const controller=new AbortController();request.current=controller;setPending(stage);setError(null);
    try{
      let next:AnonymizationRelease;
      if(stage==="variants"){const options=variantOptions(variants);next=await requestVariantRelease(schema.source_id,options,controller.signal);}
      else {const options=attributeOptions(attributes);const query=queryFile??(variantRelease?releasedFile(variantRelease):null);if(!query)throw new Error("Release activity variants or choose a CSV/XES query log first.");next=await requestAttributeRelease(schema.source_id,query,options,controller.signal);}
      if(request.current!==controller||controller.signal.aborted)return;
      if(stage==="variants"){setVariantRelease(next);setAttributeRelease(null);}else setAttributeRelease(next);
    }catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(null);}}
  };
  return <section className="panel anonymization-workspace"><div className="panel-head"><h2>Anonymize the log</h2><span className="panel-meta">{data.dataset}</span></div>
    <p className="query-intro">Release activity sequences with LAPLACE or SACOFA, then optionally add anonymized event attributes with PRIPEL using a released query log.</p>
    {!available&&<p className="model-empty">Open a CSV or XES log to anonymize it. A saved analysis does not provide an active source log.</p>}
    <div className="query-setup"><button className="btn-ghost" disabled={!available||mining||!!pending} onClick={()=>void refresh()}>Refresh anonymization source</button>{schema&&<p>Source: {schema.case_count} cases · {schema.event_count} events</p>}</div>
    <fieldset className="query-fields" disabled={disabled}><legend>1. Release activity variants</legend><div className="query-options">
      <label className="filters-field"><span>Variant method</span><select value={variants.variant} onChange={e=>v("variant",e.target.value)}><option value="sacofa">SACOFA</option><option value="laplace">LAPLACE</option></select></label>
      <label className="filters-field"><span>Variant epsilon</span><input type="number" min="0" step="any" value={variants.epsilon} onChange={e=>v("epsilon",e.target.value)} /></label>
      <label className="filters-field"><span>Maximum prefix length</span><input type="number" min="0" max="100" step="1" value={variants.length} onChange={e=>v("length",e.target.value)} /></label>
      <label className="filters-field"><span>Prune threshold</span><input type="number" min="1" step="1" value={variants.threshold} onChange={e=>v("threshold",e.target.value)} /></label>
      <NoiseFields noise={variants.noise} seed={variants.seed} onChange={(noise,seed)=>setVariants(form=>({...form,noise,seed}))} />
      <label className="filters-field"><span>Variant output trace limit</span><input type="number" min="1" max="100000" value={variants.traces} onChange={e=>v("traces",e.target.value)} /></label><label className="filters-field"><span>Candidate variant limit</span><input type="number" min="1" max="100000" value={variants.variants} onChange={e=>v("variants",e.target.value)} /></label>
    </div><p>Length 0 releases an empty log. Seeded preview makes repeated runs reproducible.</p><button className="btn-open" onClick={()=>void run("variants")}>Release activity variants</button></fieldset>
    {variantRelease&&<ReleaseView release={variantRelease} onOpen={onOpenReleased} />}
    <fieldset className="query-fields" disabled={disabled}><legend>2. Anonymize attributes with PRIPEL</legend><p>{queryFile?`Query log: ${queryFile.name}`:variantRelease?`Query log: the ${variantRelease.result.family==="TRACE_VARIANT_QUERY"?variantRelease.result.variant.toUpperCase():""} release above (${variantRelease.result.log.traces.length} cases)`:"First release activity variants, or choose a query log."}</p>
      <label className="filters-field"><span>Released query log (CSV or XES)</span><input ref={queryInput} type="file" accept=".csv,.xes" onChange={e=>{setQueryFile(e.target.files?.[0]??null);setAttributeRelease(null);}} /></label>{queryFile&&<button className="btn-ghost" onClick={()=>{setQueryFile(null);setAttributeRelease(null);if(queryInput.current)queryInput.current.value="";}}>Use generated query release</button>}
      <div className="query-options"><label className="filters-field"><span>PRIPEL epsilon</span><input type="number" min="0" step="any" value={attributes.epsilon} onChange={e=>a("epsilon",e.target.value)} /></label><NoiseFields noise={attributes.noise} seed={attributes.seed} onChange={(noise,seed)=>setAttributes(form=>({...form,noise,seed}))} /><label className="filters-field"><span>PRIPEL output trace limit</span><input type="number" min="1" max="100000" value={attributes.traces} onChange={e=>a("traces",e.target.value)} /></label><label className="filters-field"><span>Maximum matching distance</span><input type="number" min="0" max="1000000" value={attributes.distance} onChange={e=>a("distance",e.target.value)} /></label></div>
      <label className="query-text"><span>Attributes to remove (one name per line)</span><textarea rows={3} value={attributes.blocklist} onChange={e=>a("blocklist",e.target.value)} /></label><button className="btn-open" disabled={!queryFile&&!variantRelease} onClick={()=>void run("attributes")}>Run PRIPEL</button>
    </fieldset>
    {pending&&<p className="query-intro" role="status">Running {pending==="variants"?"trace-variant query":"PRIPEL"}… <button className="btn-ghost" onClick={reset}>Cancel anonymization</button></p>}
    {error&&<div className="error-banner" role="alert">{error}</div>}
    {attributeRelease&&<ReleaseView release={attributeRelease} onOpen={onOpenReleased} />}
  </section>;
}
