import { useEffect, useRef, useState } from "react";
import { FOOTPRINTS_DISCOVERY_VARIANTS, FOOTPRINTS_CONFORMANCE_VARIANTS, type FootprintsModelVariant, type MineraExport } from "../types";
import { defaultFootprintsForm, defaultTransitionForm, runGraphDiscovery, type FootprintsForm, type TransitionForm, type GraphDiscoveryFamily, type GraphDiscoveryRun } from "../graph-discovery";
import { requestLogQuerySchema, downloadQueryBlob, type LogQuerySchema } from "../log-query";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeJson } from "../native-json";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";
import { FootprintsModel } from "./FootprintsModel";
import { TransitionSystemModel } from "./TransitionSystemModel";

export function GraphDiscovery({family,data,available,mining}:{family:GraphDiscoveryFamily;data:MineraExport;available:boolean;mining:boolean}) {
  const [footprints,setFootprints]=useState(defaultFootprintsForm),[transition,setTransition]=useState(defaultTransitionForm);
  const [source,setSource]=useState<LogQuerySchema|null>(null),[run,setRun]=useState<GraphDiscoveryRun|null>(null),[pending,setPending]=useState(false),[error,setError]=useState<string|null>(null);
  const request=useRef<AbortController|null>(null),schemaRequest=useRef<AbortController|null>(null),sequence=useRef(0);
  const name=family==='footprints'?'Footprints':'Transition system';
  const cancel=()=>{request.current?.abort();request.current=null;setPending(false);};
  const refresh=async()=>{cancel();schemaRequest.current?.abort();setSource(null);setRun(null);setError(null);if(!available||mining)return;const controller=new AbortController();schemaRequest.current=controller;try{const next=await requestLogQuerySchema(controller.signal);if(schemaRequest.current===controller&&!controller.signal.aborted)setSource(next);}catch(cause){if(schemaRequest.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}};
  useEffect(()=>{void refresh();return()=>{request.current?.abort();schemaRequest.current?.abort();};},[available,mining,family]);
  const fp=<K extends keyof FootprintsForm>(key:K,value:FootprintsForm[K])=>setFootprints(previous=>({...previous,[key]:value}));
  const ts=<K extends keyof TransitionForm>(key:K,value:TransitionForm[K])=>setTransition(previous=>({...previous,[key]:value}));
  const discover=async()=>{if(!source||pending||mining)return;const controller=new AbortController();request.current=controller;setPending(true);setError(null);try{const next=await runGraphDiscovery(family,family==='footprints'?footprints:transition,source,controller.signal);if(request.current===controller&&!controller.signal.aborted){sequence.current++;setRun(next);}}catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <>
    <section className="panel" aria-label="Configured graph discovery"><div className="panel-head"><h2>{name} discovery</h2><span className="panel-meta">{data.dataset}</span></div>
      <p>Run the selected native discovery on the current source. Footprint comparisons use the actual cached workflow models; results retain their submitted settings.</p>
      {!available&&<p>Open a CSV or XES source to discover another configuration. The saved result below retains its original settings.</p>}
      <button className="btn-ghost" disabled={!available||mining||pending} onClick={()=>void refresh()}>Reload source</button>{source&&<p>Source {source.source_id} · {source.case_count} cases · {source.event_count} events</p>}
      <fieldset disabled={!source||mining||pending}>{family==='footprints'?<>
        <div className="align-settings-grid"><label>Footprint comparison model<select value={footprints.model} onChange={event=>fp('model',event.target.value as FootprintsModelVariant)}>{['process_tree','powl','inductive','alpha','heuristic','dfg'].map(model=><option key={model}>{model}</option>)}</select></label>
          {([['activity','Footprint activity field','concept:name'],['caseId','Footprint case-id field','case:concept:name'],['timestamp','Footprint timestamp field','time:timestamp'],['start','Footprint start-time field','none (no separate start time)'],['index','Footprint index field','@@index'],['marking','Footprint initial marking','native model marking']]as const).map(([key,label,placeholder])=><label key={key}>{label}<input disabled={key==='index'&&!footprints.overrideIndex} value={footprints[key]} placeholder={placeholder} onChange={event=>fp(key,event.target.value)}/></label>)}
          {([['strict','Strict footprint comparison'],['always','Check activities that always execute'],['sort','Sort footprint table events'],['overrideIndex','Override footprint index field']]as const).map(([key,label])=><label key={key}><input type="checkbox" checked={footprints[key]} onChange={event=>fp(key,event.target.checked)}/>{label}</label>)}
        </div>
        <p>Blank fields use native defaults. A start-time field of none disables a separate start column. Enabling an index override preserves an explicitly blank index field. Markings accept comma-separated unsigned token counts.</p>
        <div className="align-settings-grid" role="group" aria-label="Footprint discovery variants">{FOOTPRINTS_DISCOVERY_VARIANTS.map(variant=><label key={variant}><input type="checkbox" checked={footprints.discovery.includes(variant)} onChange={event=>fp('discovery',event.target.checked?[...footprints.discovery,variant]:footprints.discovery.filter(value=>value!==variant))}/>{variant}</label>)}</div>
        <div className="align-settings-grid" role="group" aria-label="Footprint conformance variants">{FOOTPRINTS_CONFORMANCE_VARIANTS.map(variant=><label key={variant}><input type="checkbox" checked={footprints.conformance.includes(variant)} onChange={event=>fp('conformance',event.target.checked?[...footprints.conformance,variant]:footprints.conformance.filter(value=>value!==variant))}/>{variant}</label>)}</div>
      </>:<div className="align-settings-grid">
        <label>Transition system view<select value={transition.view} onChange={event=>ts('view',event.target.value as TransitionForm['view'])}>{['sequence','set','multiset'].map(value=><option key={value}>{value}</option>)}</select></label>
        <label>Transition system direction<select value={transition.direction} onChange={event=>ts('direction',event.target.value as TransitionForm['direction'])}><option value="forward">Forward</option><option value="backward">Backward</option></select></label>
        <label>Transition system window<input value={transition.window} placeholder="2 (signed integer)" onChange={event=>ts('window',event.target.value)}/></label>
        <label>Transition system activity field<input value={transition.activity} placeholder="concept:name" onChange={event=>ts('activity',event.target.value)}/></label>
        <label>Transition system case-id field<input value={transition.caseId} placeholder="case:concept:name" onChange={event=>ts('caseId',event.target.value)}/></label>
        <label><input type="checkbox" checked={transition.includeData} onChange={event=>ts('includeData',event.target.checked)}/> Include transition traversal references</label>
      </div>}
      <button className="btn-open" onClick={()=>void discover()}>Discover graph configuration</button></fieldset>
      {pending&&<p role="status">Discovering graph… <button className="btn-ghost" onClick={cancel}>Cancel graph discovery</button></p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </section>
    {run?<GraphInspection key={sequence.current} run={run}/>:family==='footprints'?data.footprints&&<FootprintsModel data={data} footprints={data.footprints} title="Saved footprints"/>:data.transition_system&&<TransitionSystemModel model={data.transition_system} data={data} title="Saved transition system" configurationNote="These settings belong to the saved analysis."/>}
  </>;
}
function GraphInspection({run}:{run:GraphDiscoveryRun}) {
  const value=run.value;const [selected,setSelected]=useState(value.method==='footprints'?'/result/footprints/metrics':'/result/transition_system/states');const table=run.tables.find(item=>item.id===selected)??run.tables[0];
  const download=(raw:string,name:string,type='application/json')=>downloadQueryBlob(new Blob([raw],{type}),name);
  return <>
    {value.method==='footprints'?<><FootprintsModel data={value.result} footprints={value.result.footprints!} title="Discovered footprints"/><p>Model comparisons use {value.model_origin==='selected_source_dfg'?'the selected source DFG':'the actual cached analysis model'}. Model variant provenance remains as recorded by the cached analysis. Cards preview relations/traces; complete tables retain every row.</p><details><summary>Exact compared model footprint</summary><pre>{nativeJson(value.model_footprint,2)}</pre></details></>:<TransitionSystemModel model={value.result.transition_system!} data={value.result} title="Discovered transition system" configurationNote="These settings belong to this completed discovery run."/>}
    <section className="panel" aria-label="Complete graph discovery inspection"><div className="panel-head"><h2>Complete graph discovery results</h2><span className="panel-meta">{value.method} · {value.variant} · source {value.source_id}</span></div>
      <p>Original source: {value.source.case_count} cases · {value.source.event_count} events. Analysis source: {value.analysis_source.case_count} cases · {value.analysis_source.event_count} events ({value.analysis_source.origin}). The complete native model, relation order, trace results, state identities and optional traversal references remain available.</p>
      <div className="query-actions"><button className="btn-ghost" onClick={()=>download(run.raw,'minera-graph-discovery.json')}>Download complete graph discovery JSON</button><button className="btn-ghost" onClick={()=>download(run.report,'minera-graph-discovery-report.json')}>Download graph discovery report</button><button className="btn-ghost" onClick={()=>download(nativeJson(value.result),'minera-discovered-graph-analysis.json')}>Download graph analysis JSON</button>{value.model_footprint&&<button className="btn-ghost" onClick={()=>download(nativeJson(value.model_footprint),'minera-compared-model-footprint.json')}>Download compared model footprint</button>}</div>
      <label>Graph discovery result table<select value={table.id} onChange={event=>setSelected(event.target.value)}>{run.tables.map(item=><option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label><button className="btn-ghost" onClick={()=>download(diagnosticCsv(table),'minera-graph-discovery-'+encodeURIComponent(table.id)+'.csv','text/csv;charset=utf-8')}>Download complete graph discovery table CSV</button>
      <DiagnosticTableView key={table.id} table={table} noun="graph discovery"/><details><summary>Complete native graph discovery response</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
