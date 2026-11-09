import {DiscoveryVectorInput} from "./DiscoveryVectorInput";
import { useEffect, useRef, useState } from "react";
import { DECLARE_TEMPLATES, type MineraExport } from "../types";
import { declareAutoSelectionActive, defaultConstraintForm, runConstraintDiscovery, SKELETON_FAMILIES, type ConstraintFamily, type ConstraintForm, type ConstraintRun } from "../constraint-discovery";
import { downloadQueryBlob, requestLogQuerySchema, type LogQuerySchema } from "../log-query";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeJson } from "../native-json";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";
import { DeclareModel } from "./DeclareModel";
import { LogSkeletonModel } from "./LogSkeletonModel";

export function ConstraintDiscovery({family,data,available,mining,onOpenLog}:{family:ConstraintFamily;data:MineraExport;available:boolean;mining:boolean;onOpenLog?:(file:File)=>void}) {
  const [form,setForm]=useState(()=>defaultConstraintForm(family)), [source,setSource]=useState<LogQuerySchema|null>(null), [run,setRun]=useState<ConstraintRun|null>(null);
  const [pending,setPending]=useState(false), [error,setError]=useState<string|null>(null);
  const request=useRef<AbortController|null>(null), schemaRequest=useRef<AbortController|null>(null), sequence=useRef(0);
  const name=family === "declare" ? "Declare" : "Log Skeleton";
  const cancel=()=>{request.current?.abort();request.current=null;setPending(false);};
  const refresh=async()=>{cancel();schemaRequest.current?.abort();setSource(null);setRun(null);setError(null);if(!available||mining)return;const controller=new AbortController();schemaRequest.current=controller;try{const next=await requestLogQuerySchema(controller.signal);if(schemaRequest.current===controller&&!controller.signal.aborted)setSource(next);}catch(cause){if(schemaRequest.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}};
  useEffect(()=>{void refresh();return()=>{request.current?.abort();schemaRequest.current?.abort();};},[available,mining,family]);
  const change=<K extends keyof ConstraintForm>(key:K,value:ConstraintForm[K])=>setForm(previous=>({...previous,[key]:value}));
  const discover=async()=>{if(!source||pending||mining)return;const controller=new AbortController();request.current=controller;setPending(true);setError(null);try{const next=await runConstraintDiscovery(family,form,source,controller.signal);if(request.current===controller&&!controller.signal.aborted){sequence.current++;setRun(next);}}catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}finally{if(request.current===controller){request.current=null;setPending(false);}}};
  const families=family === "declare" ? DECLARE_TEMPLATES : SKELETON_FAMILIES;
  return <>
    <section className="panel" aria-label="Configured constraint discovery"><div className="panel-head"><h2>{name} discovery and conformance</h2><span className="panel-meta">{data.dataset}</span></div>
      <p>Discover constraints from the current source, then check every case against that exact model. Completed results retain their submitted controls.</p>
      {!available&&<p>Open a CSV or XES source to discover constraints. The model below belongs to the saved analysis.</p>}
      <button className="btn-ghost" disabled={!available||mining||pending} onClick={()=>void refresh()}>Reload source</button>
      {source&&<p>Source {source.source_id} · {source.case_count} cases · {source.event_count} events</p>}
      <fieldset disabled={!source||mining||pending}><div className="align-settings-grid">
        <label>Constraint discovery activity field<input value={form.activity} placeholder="concept:name" onChange={event=>change('activity',event.target.value)}/></label>
        <label>Constraint conformance activity field<input value={form.conformanceActivity} placeholder="same as discovery" onChange={event=>change('conformanceActivity',event.target.value)}/></label>
        <label>Constraint diagnostics case-id field<input value={form.diagnosticsCase} placeholder="case:concept:name" onChange={event=>change('diagnosticsCase',event.target.value)}/></label>
        <label>Constraint family selection<select value={form.selection} onChange={event=>change('selection',event.target.value as ConstraintForm['selection'])}><option value="native">Native default</option><option value="selected">Selected families (including none)</option></select></label>
        {family === "declare" ? <>
          <label>Declare activity selection<select value={form.activitySelection} onChange={event=>change('activitySelection',event.target.value as ConstraintForm['activitySelection'])}><option value="native">Infer from source</option><option value="selected">Explicit activities (including none)</option></select></label>
          {form.activitySelection === "selected"&&<DiscoveryVectorInput label="Declare considered activities" mode={form.activitiesMode} legacyValue={form.activities} literalValue={form.activitiesJson} onMode={value=>change('activitiesMode',value)} onLegacy={value=>change('activities',value)} onLiteral={value=>change('activitiesJson',value)}/>}
          {([['support','Declare minimum support ratio'],['confidence','Declare minimum confidence ratio']] as const).map(([key,label])=><label key={key}>{label}<input value={form[key]} inputMode="decimal" placeholder="native automatic selection" onChange={event=>change(key,event.target.value)}/></label>)}
          <div><label>Declare automatic selection multiplier<input value={form.multiplier} inputMode="decimal" placeholder="0.8 (automatic selection)" disabled={!declareAutoSelectionActive(form)} aria-describedby="declare-multiplier-note" onChange={event=>change('multiplier',event.target.value)}/></label><small id="declare-multiplier-note">{declareAutoSelectionActive(form) ? "Scales the automatically chosen support and confidence thresholds." : "Not used while a minimum support or confidence ratio is set; the blank one counts as 0."}</small></div>
        </> : <label>Log Skeleton noise threshold<input value={form.noise} placeholder="0" onChange={event=>change('noise',event.target.value)}/></label>}
      </div>
      {form.selection === "selected"&&<><p>An empty selection is sent explicitly. Declare discovers no templates; Log Skeleton retains its discovered model and checks no selected families.</p><div className="align-settings-grid" role="group" aria-label="Selected constraint families">{families.map(template=><label key={template}><input type="checkbox" checked={form.templates.includes(template)} onChange={event=>change('templates',event.target.checked ? [...form.templates,template] : form.templates.filter(value=>value!==template))}/>{template}</label>)}</div></>}
      {family === "declare"&&<details><summary>Generate a log from this Declare model</summary><label><input type="checkbox" checked={form.generateLog} onChange={event=>change('generateLog',event.target.checked)}/> Generate Declare playout</label><p>Native prefix-safe sampling can end with pending obligations. The generated log includes a completed conformance check so those deviations remain visible.</p><div className="align-settings-grid">{([['traces','Declare playout cases','1000'],['minimum','Declare playout minimum length','3'],['maximum','Declare playout maximum length','15'],['seed','Declare playout seed','native random seed']] as const).map(([key,label,placeholder])=><label key={key}>{label}<input disabled={!form.generateLog} value={form[key]} placeholder={placeholder} onChange={event=>change(key,event.target.value)}/></label>)}</div></details>}
      <button className="btn-open" onClick={()=>void discover()}>Discover and check constraints</button></fieldset>
      {pending&&<p role="status">Discovering constraints… <button className="btn-ghost" onClick={cancel}>Cancel constraint discovery</button></p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </section>
    {run?<ConstraintInspection key={sequence.current} run={run} onOpenLog={onOpenLog}/>:<SavedConstraints family={family} data={data}/>}
  </>;
}
function SavedConstraints({family,data}:{family:ConstraintFamily;data:MineraExport}) {
  return family === "declare" ? data.declare&&<DeclareModel model={data.declare} data={data} title="Saved Declare model"/> : data.log_skeleton&&<LogSkeletonModel model={data.log_skeleton} conformance={data.log_skeleton_conformance} data={data} title="Saved Log Skeleton model"/>;
}
function ConstraintInspection({run,onOpenLog}:{run:ConstraintRun;onOpenLog?:(file:File)=>void}) {
  const [selected,setSelected]=useState('/conformance');const table=run.tables.find(item=>item.id===selected)??run.tables[0], value=run.value;
  const conformance=value.method === "declare" ? value.result.declare_conformance! : value.result.log_skeleton_conformance!;
  const log=()=>new File([value.playout_xes!],'minera-discovered-declare-playout.xes',{type:'application/xml'});
  const download=(raw:string,name:string,type='application/json')=>downloadQueryBlob(new Blob([raw],{type}),name);
  return <>
    {value.method === "declare" ? <DeclareModel model={value.result.declare!} data={value.result} title="Discovered Declare model"/> : <LogSkeletonModel model={value.result.log_skeleton!} conformance={value.result.log_skeleton_conformance} data={value.result} title="Discovered Log Skeleton model"/>}
    <section className="panel" aria-label="Complete constraint discovery inspection"><div className="panel-head"><h2>Complete constraint results</h2><span className="panel-meta">{value.method} · classic · source {value.source_id}</span></div>
      <p>{conformance.filter(trace=>trace.is_fit).length} of {conformance.length} source cases satisfy this model. Tables retain every rule, activity, case and deviation. Native counts, fitness values and errors remain unchanged.</p>
      <div className="query-actions">
        <button className="btn-ghost" onClick={()=>download(run.raw,'minera-constraint-discovery.json')}>Download complete constraint JSON</button>
        <button className="btn-ghost" onClick={()=>download(run.report,'minera-constraint-discovery-report.json')}>Download constraint discovery report</button>
        <button className="btn-ghost" onClick={()=>download(nativeJson(value.result),'minera-discovered-constraints-analysis.json')}>Download constraint analysis JSON</button>
        <button className="btn-ghost" onClick={()=>download(nativeJson(value.diagnostics),'minera-constraint-diagnostics.json')}>Download native constraint diagnostics</button>
        <button className="btn-ghost" disabled={!value.playout_xes} onClick={()=>downloadQueryBlob(log(),log().name)}>Download discovered Declare playout</button>
        {onOpenLog&&<button className="btn-ghost" disabled={!value.playout_xes} onClick={()=>onOpenLog(log())}>Mine discovered Declare playout</button>}
        {value.playout_result&&<button className="btn-ghost" onClick={()=>download(nativeJson(value.playout_result),'minera-declare-playout-analysis.json')}>Download Declare playout analysis</button>}
      </div>
      {value.playout_result&&<p>Generated log: {value.playout_result.trace_count} cases · {value.playout_result.event_count} events. Completed conformance: {value.playout_result.declare_conformance!.filter(trace=>trace.is_fit).length} of {value.playout_result.trace_count} fit. Prefix-safe playout does not promise that every obligation is complete.</p>}
      {value.diagnostics_error&&<p role="status">Diagnostics error: {value.diagnostics_error}</p>}{value.playout_error&&<p role="status">Playout error: {value.playout_error}</p>}
      <label>Constraint result table<select value={table.id} onChange={event=>setSelected(event.target.value)}>{run.tables.map(item=><option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <button className="btn-ghost" onClick={()=>download(diagnosticCsv(table),'minera-constraint-'+encodeURIComponent(table.id)+'.csv','text/csv;charset=utf-8')}>Download complete constraint table CSV</button>
      <DiagnosticTableView key={table.id} table={table} noun="constraint"/>
      <details><summary>Complete native constraint response</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
