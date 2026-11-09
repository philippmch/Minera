import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import { WORKFLOW_VARIANTS, defaultWorkflowForm, runWorkflowDiscovery, workflowModelFile, workflowNet, workflowReplayKey, type WorkflowFamily, type WorkflowVariant, type WorkflowForm, type WorkflowRun } from "../workflow-discovery";
import type { MineraExport } from "../types";
import { requestLogQuerySchema, downloadQueryBlob, type LogQuerySchema } from "../log-query";
import { ocelPreparedCsv } from "../ocel-preparation";
import { nativeJson } from "../native-json";
import { cellText } from "../transformation-workspace";
import { computePetriLayout } from "../petri-layout";
import { PetriNet } from "./PetriNet";
import { DependencyGraph } from "./DependencyGraph";
import { ProcessTree } from "./ProcessTree";
import { computeTreeLayout } from "../tree-layout";
import { PowlModel } from "./PowlModel";
import type { CsvOptions } from "../api";

/** Whether the selected variant is discovered without a click: once per arming (the variant card, or a re-mined
 * analysis), as soon as the source is known and nothing else is running. */
export function workflowAutoStarts(state:{armed:boolean;schema:boolean;run:boolean;pending:boolean;mining:boolean;available:boolean}):boolean{return state.armed&&state.schema&&!state.run&&!state.pending&&!state.mining&&state.available;}
export function WorkflowDiscovery({ family, variant, data, available, mining, onUseModel, onOpenPlayout, savedView, onVariantChange }: {
 family:WorkflowFamily;variant:WorkflowVariant;data:MineraExport;available:boolean;mining:boolean;
 /** The analysis's own model, shown (labelled as such) only while no run of the selected variant is on screen. */
 savedView?:ReactNode;
 /** The variant dropdown changed (the miner breadcrumb follows it). */
 onVariantChange?:(variant:WorkflowVariant)=>void;
 onUseModel?:(file:File,sourceId:string,activityKey?:string)=>void;onOpenPlayout?:(file:File,activityKey?:string,options?:CsvOptions)=>void;
}) {
 const [form,setForm]=useState(()=>defaultWorkflowForm(variant));const [schema,setSchema]=useState<LogQuerySchema|null>(null);const [run,setRun]=useState<WorkflowRun|null>(null);const [pending,setPending]=useState(false);const [error,setError]=useState<string|null>(null);
 const request=useRef<AbortController|null>(null),source=useRef<AbortController|null>(null),sequence=useRef(0);
 // The variant card says "Mine": the selected variant is discovered as soon as the source is known, and again after the
 // analysis is re-mined (filters). A cancelled or failed run leaves the button for the next attempt.
 const autoStart=useRef(true);
 const refresh=async()=>{request.current?.abort();source.current?.abort();setPending(false);setSchema(null);setRun(null);setError(null);if(!available||mining)return;const controller=new AbortController();source.current=controller;try{const next=await requestLogQuerySchema(controller.signal);if(source.current===controller&&!controller.signal.aborted)setSchema(next);}catch(cause){if(source.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}};
 useEffect(()=>{autoStart.current=true;void refresh();return()=>{request.current?.abort();source.current?.abort();};},[available,mining]);
 useEffect(()=>{if(workflowAutoStarts({armed:autoStart.current,schema:schema!==null,run:run!==null,pending,mining,available})){autoStart.current=false;void start();}},[schema]);
 const change=<K extends keyof WorkflowForm>(key:K,value:WorkflowForm[K])=>setForm(old=>({...old,[key]:value}));
 const start=async()=>{if(!schema||pending||mining)return;const controller=new AbortController();request.current=controller;setPending(true);setError(null);try{const next=await runWorkflowDiscovery(family,form,schema,controller.signal);if(request.current===controller&&!controller.signal.aborted){sequence.current++;setRun(next);}}catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}finally{if(request.current===controller){request.current=null;setPending(false);}}};
 return <>
  <section className="panel workflow-discovery" aria-label="Configured workflow discovery"><div className="panel-head"><h2>{{alpha:'Alpha',heuristic:'Heuristic',inductive:'Inductive',powl:'POWL'}[family]} discovery</h2><span className="panel-meta">{data.dataset}</span></div>
   <p>Select the native variant and mine the current source. The resulting graph and downloads come from that discovery run.</p>
   {!available&&<p>Open a CSV or XES source to discover another variant. The saved model below retains its recorded identity.</p>}
   <button className="btn-ghost" disabled={!available||mining||pending} onClick={()=>void refresh()}>Reload source</button>{schema&&<p>Source {schema.source_id} · {schema.case_count} cases · {schema.event_count} events</p>}
   <fieldset disabled={!schema||mining||pending}><div className="align-settings-grid">
    <label>Workflow discovery variant<select value={form.variant} onChange={event=>{const next=event.target.value as WorkflowVariant;change('variant',next);onVariantChange?.(next);}}>{WORKFLOW_VARIANTS[family].map(([value,label])=><option key={value} value={value}>{label}</option>)}</select></label>
    <label>Discovery activity field<input value={form.activity} placeholder="native selector" onChange={event=>change('activity',event.target.value)}/></label>
    {family==='alpha'&&<><label>Alpha input row cap<input type="number" min={0} value={form.maximumRows} onChange={event=>change('maximumRows',event.target.value)}/></label><label><input type="checkbox" checked={form.remove} onChange={event=>change('remove',event.target.checked)}/> Remove unconnected Alpha+ transitions</label></>}
    {family==='heuristic'&&<>
     <label>Discovery timestamp field<input value={form.timestamp} placeholder="time:timestamp" onChange={event=>change('timestamp',event.target.value)}/></label>
     {([['dependency','Dependency threshold'],['and','AND threshold'],['frequency','Minimum edge frequency'],['minimumActivity','Minimum activity count']]as const).map(([key,label])=><label key={key}>{label}<input type="number" min={0} step="any" value={form[key]} onChange={event=>change(key,event.target.value)}/></label>)}
     <label>Heuristic decoration<select value={form.decoration} onChange={event=>change('decoration',event.target.value as WorkflowForm['decoration'])}><option value="frequency">Frequency</option><option value="performance">Performance</option></select></label>
     <label>Performance aggregation<select value={form.aggregation} onChange={event=>change('aggregation',event.target.value as WorkflowForm['aggregation'])}>{['mean','median','min','max','stdev','sum','raw_values','all'].map(value=><option key={value}>{value}</option>)}</select></label>
     <label>Connect strongest dependencies<select value={form.connect} onChange={event=>change('connect',event.target.value as WorkflowForm['connect'])}><option value="native">Native default</option><option value="true">Connect all</option><option value="false">Threshold only</option></select></label>
    </>}
    {(family==='inductive'||family==='powl')&&<>
      <label>Discovery timestamp field<input value={form.timestamp} placeholder="time:timestamp" onChange={event=>change('timestamp',event.target.value)}/></label>
      <label>Discovery case-id field<input value={form.caseId} placeholder="case:concept:name" onChange={event=>change('caseId',event.target.value)}/></label>
      {family==='inductive'?<><label>Inductive noise threshold<input value={form.noise} placeholder="0 (used by IMf)" onChange={event=>change('noise',event.target.value)}/></label><label>Discovered tree conversion<select value={form.conversion} onChange={event=>change('conversion',event.target.value as WorkflowForm['conversion'])}><option value="standard">Standard</option><option value="transition_bordered">Transition bordered</option></select></label>{([['disableStrict','Disable strict sequence cut'],['disableFallthroughs','Disable specialized fallthroughs'],['multiprocessing','Parallel fallthrough evaluation'],['sortTime','Sort discovery events by timestamp']]as const).map(([key,label])=><label key={key}><input type="checkbox" checked={form[key]} onChange={event=>change(key,event.target.checked)}/>{label}</label>)}</>:<><label>POWL filtering mode<select value={form.filtering} onChange={event=>change('filtering',event.target.value as WorkflowForm['filtering'])}><option value="decreasing_factor">Decreasing factor</option><option value="dynamic">Dynamic</option></select></label>{([['filteringThreshold','POWL filtering threshold'],['orderRatio','POWL order frequency ratio'],['bruteLimit','POWL brute-force activity cap']]as const).map(([key,label])=><label key={key}>{label}<input value={form[key]} placeholder="native default" onChange={event=>change(key,event.target.value)}/></label>)}</>}
    </>}
   </div>
   {family==='heuristic'&&<details><summary>Heuristic timing, loops and native decoration controls</summary><div className="align-settings-grid">
    <label>Discovery start-time field<input value={form.start} placeholder="variant-specific native default" onChange={event=>change('start',event.target.value)}/></label><label>Discovery lifecycle instance field<input value={form.lifecycle} placeholder="concept:instance" onChange={event=>change('lifecycle',event.target.value)}/></label>
    <label>Pre-cleaning noise threshold<input type="number" min={0} step="any" value={form.noise} onChange={event=>change('noise',event.target.value)}/></label><label>Length-two-loop threshold<input type="number" min={0} step="any" value={form.loop} onChange={event=>change('loop',event.target.value)}/></label>
    <label><input type="checkbox" checked={form.once} onChange={event=>change('once',event.target.checked)}/> Count each path once per case</label><label>Most common paths<input value={form.paths} placeholder="A -> B, B -> D" onChange={event=>change('paths',event.target.value)}/></label>
    <label><input type="checkbox" checked={form.businessHours} onChange={event=>change('businessHours',event.target.checked)}/> Use business hours</label><label>Discovery weekly business-hour slots<input value={form.slots} placeholder="none or 25200-61200,..." onChange={event=>change('slots',event.target.value)}/></label>
    <label>Heuristics++ native decoration selector<input value={form.decorationSelector} onChange={event=>change('decorationSelector',event.target.value)}/></label><label>Heuristics++ native decoration value (JSON)<textarea rows={3} value={form.decorationValue} onChange={event=>change('decorationValue',event.target.value)}/></label>
   </div></details>}
   <details><summary>Playout from this discovered model</summary><p>Optional seeded XES uses this model's native markings. A zero or blank trace count skips playout. Partial paths and conversion failures retain their native behavior.</p><div className="align-settings-grid">{([['playoutTraces','Discovered-model playout traces'],['playoutSteps','Discovered-model firing cap'],['playoutSeed','Discovered-model playout seed']]as const).map(([key,label])=><label key={key}>{label}<input type="number" min={0} value={form[key]} onChange={event=>change(key,event.target.value)}/></label>)}</div></details>
   <button className="btn-open" onClick={()=>void start()}>Discover selected workflow</button></fieldset>
   {pending&&<p role="status">Discovering… <button className="btn-ghost" onClick={()=>{request.current?.abort();request.current=null;setPending(false);}}>Cancel discovery request</button></p>}{error&&<div className="error-banner" role="alert">{error}</div>}
  </section>
  {run&&!pending&&run.value.variant!==form.variant&&<StaleRunNotice family={family} shown={run.value.variant} selected={form.variant} canDiscover={!!schema&&!mining} onDiscover={()=>void start()}/>}
  {run&&<WorkflowInspection key={sequence.current} run={run} onUseModel={onUseModel} onOpenPlayout={onOpenPlayout}/>}
  {!run&&!pending&&(savedView!==undefined?savedView&&<SavedNotice family={family} selected={form.variant} canDiscover={available}>{savedView}</SavedNotice>:<SavedWorkflow family={family} data={data}/>)}
 </>;
}
/** A run of another variant than the one now selected: says so, and offers to discover the selected one. */
function StaleRunNotice({family,shown,selected,canDiscover,onDiscover}:{family:WorkflowFamily;shown:WorkflowVariant;selected:WorkflowVariant;canDiscover:boolean;onDiscover:()=>void}){const name=(variant:WorkflowVariant)=>WORKFLOW_VARIANTS[family].find(([value])=>value===variant)?.[1]??variant;return <p className="saved-model-notice stale-run-notice" role="status">The results below come from {name(shown)}, not the selected {name(selected)}. <button className="btn-ghost" disabled={!canDiscover} onClick={onDiscover}>Discover {name(selected)}</button></p>;}
/** The analysis model, introduced as what it is: not a run of the variant selected above. */
function SavedNotice({family,selected,canDiscover,children}:{family:WorkflowFamily;selected:WorkflowVariant;canDiscover:boolean;children:ReactNode}){const label=WORKFLOW_VARIANTS[family].find(([value])=>value===selected)?.[1]??selected;return <><p className="saved-model-notice" role="note">No {label} run is shown yet. Below is the model mined with this analysis (its Filters &amp; miner settings), not a {label} discovery: {canDiscover?<>press “Discover selected workflow” to mine {label}.</>:<>open a CSV or XES source to mine {label}.</>}</p>{children}</>;}
function SavedWorkflow({family,data}:{family:WorkflowFamily;data:MineraExport}) {
 const net=family==='alpha'?data.petri_net:family==='heuristic'?data.heuristic_petri_net:family==='inductive'?data.inductive_petri_net:data.powl_petri_net;const layout=useMemo(()=>net?computePetriLayout(net,data):null,[net,data]);
 const tree=useMemo(()=>family==='inductive'&&data.process_tree?computeTreeLayout(data.process_tree,data):null,[family,data]);
 return <>{tree&&<ProcessTree layout={tree} title="Saved Inductive model · variant not recorded"/>}{family==='powl'&&data.powl&&<PowlModel model={data.powl} data={data} title="Saved POWL model · variant not recorded"/>}{family==='heuristic'&&data.heuristics_net&&<DependencyGraph data={data} title={`Saved heuristic model · ${data.heuristics_net.variant??'variant not recorded'}`}/>}{layout&&<PetriNet layout={layout} heading="Saved workflow model" title={family==='alpha'?'Alpha variant not recorded':family==='heuristic'?data.heuristics_net?.variant??'variant not recorded':'Saved '+family+' · variant not recorded'} disconnectedDescription="transitions with no incident arcs"/>}</>;
}
function WorkflowInspection({run,onUseModel,onOpenPlayout}:{run:WorkflowRun;onUseModel?:(file:File,sourceId:string,activityKey?:string)=>void;onOpenPlayout?:(file:File,activityKey?:string,options?:CsvOptions)=>void}) {
 const [selected,setSelected]=useState(run.value.method==='alpha'&&run.tables.alpha_places?'alpha_places':'transitions'),[page,setPage]=useState(0),[columnPage,setColumnPage]=useState(0);
 const model=workflowNet(run.value),layout=useMemo(()=>computePetriLayout(model,run.value.result),[model,run.value.result]),current=run.tables[selected]??run.tables.transitions;
 const treeLayout=useMemo(()=>run.value.result.process_tree?computeTreeLayout(run.value.result.process_tree,run.value.result):null,[run.value.result]);
 const download=(raw:string,name:string,type='application/json')=>downloadQueryBlob(new Blob([raw],{type}),name),playout=()=>new File([run.value.playout_xes!],`minera-${run.value.method}-${run.value.variant}-playout.xes`,{type:'application/xml'});
 const columns=current.columns.slice(columnPage*8,(columnPage+1)*8);
 return <>
  {run.value.method==='heuristic'&&<DependencyGraph data={run.value.result} title={`Native ${run.value.variant}`}/>}
  {treeLayout&&<ProcessTree layout={treeLayout} title={`Native ${run.value.variant}`}/>}
  {run.value.result.powl&&<PowlModel model={run.value.result.powl} data={run.value.result} title={`Native ${run.value.variant}`}/>}
  <PetriNet layout={layout} heading="Discovered workflow" title={`${run.value.method} · ${run.value.variant}`} disconnectedDescription="transitions with no incident arcs"/>
  <section className="panel workflow-inspection" aria-label="Complete workflow discovery inspection"><div className="panel-head"><h3>Complete discovery results</h3><span className="panel-meta">{run.value.method} · {run.value.variant} · source {run.value.source_id}</span></div>
   <p>The model, marking/heuristic tables and export files come from this exact native discovery. Existing cached analysis models remain unchanged. Alpha+ retains live and detached markings in its full discovery JSON.</p>
   {run.value.discovery_source&&<p>Original source: {run.value.source.case_count} cases · {run.value.source.event_count} events. Adapted discovery source: {run.value.discovery_source.case_count} cases · {run.value.discovery_source.event_count} events. Checking the current source uses its existing case/order projection; open the adapted source to use the discovery grouping and ordering.</p>}
   <div className="query-actions"><button className="btn-ghost" onClick={()=>download(run.raw,'minera-workflow-discovery.json')}>Download complete discovery JSON</button><button className="btn-ghost" onClick={()=>download(run.report,'minera-workflow-discovery-report.json')}>Download workflow discovery report</button><button className="btn-ghost" onClick={()=>download(nativeJson(run.value.result),'minera-discovered-analysis.json')}>Download discovered analysis JSON</button>
    {(['pnml','bpmn','ptml']as const).map(format=><button key={format} className="btn-ghost" disabled={!run.value[format]} onClick={()=>downloadQueryBlob(workflowModelFile(run,format),workflowModelFile(run,format).name)}>Download discovered {format.toUpperCase()}</button>)}
    {onUseModel&&<button className="btn-ghost" disabled={!run.value.pnml} onClick={()=>onUseModel(workflowModelFile(run,'pnml'),run.value.source_id,workflowReplayKey(run))}>Check this discovered workflow</button>}
    {run.value.source_xes&&<><button className="btn-ghost" onClick={()=>download(run.value.source_xes!,'minera-adapted-discovery-source.xes','application/xml')}>Download adapted discovery source XES</button>{onOpenPlayout&&<button className="btn-ghost" onClick={()=>{
      const file=new File([run.value.source_xes!],'minera-adapted-discovery-source.xes',{type:'application/xml'});
      onOpenPlayout(file,run.value.source_xes_options?.xesActivityAttribute,run.value.source_xes_options);
    }}>Open adapted discovery source</button>}</>}
    <button className="btn-ghost" disabled={!run.value.playout_xes} onClick={()=>downloadQueryBlob(playout(),playout().name)}>Download discovered-model playout</button>{onOpenPlayout&&<button className="btn-ghost" disabled={!run.value.playout_xes} onClick={()=>onOpenPlayout(playout())}>Mine discovered-model playout</button>}
   </div>
   {(['pnml_error','bpmn_error','ptml_error','playout_error']as const).map(key=>run.value[key]?<p role="status" key={key}>{key.replaceAll('_',' ')}: {run.value[key]}</p>:null)}
   <label>Discovery result table<select value={selected} onChange={event=>{setSelected(event.target.value);setPage(0);setColumnPage(0);}}>{Object.entries(run.tables).map(([id,table])=><option key={id} value={id}>{id.replaceAll('_',' ')} · {table.count} rows</option>)}</select></label><p>{current.count} rows · {current.columns.length} columns. CSV downloads retain every row and column, including nested native structures.</p>
   <button className="btn-ghost" onClick={()=>downloadQueryBlob(ocelPreparedCsv(current),`minera-discovery-${selected}.csv`)}>Download complete discovery table CSV</button>
   <div className="query-table-wrap"><table className="activity-table"><thead><tr>{columns.map(column=><th scope="col" key={column}>{column}</th>)}</tr></thead><tbody>{Array.from({length:Math.min(25,Math.max(0,current.count-page*25))},(_,i)=>{const row=current.rowAt(page*25+i);return <tr key={page*25+i}>{columns.map((column,j)=><td key={column}>{cellText(row[columnPage*8+j])}</td>)}</tr>;})}</tbody></table>{!current.count&&<p>No native rows in this table.</p>}</div>
   <div className="query-actions"><button className="btn-ghost" disabled={!page} onClick={()=>setPage(value=>value-1)}>Previous discovery rows</button><span>Page {page+1} of {Math.max(1,Math.ceil(current.count/25))}</span><button className="btn-ghost" disabled={(page+1)*25>=current.count} onClick={()=>setPage(value=>value+1)}>Next discovery rows</button>{current.columns.length>8&&<><button className="btn-ghost" disabled={!columnPage} onClick={()=>setColumnPage(value=>value-1)}>Previous discovery columns</button><span>Columns {columnPage*8+1}–{Math.min((columnPage+1)*8,current.columns.length)} of {current.columns.length}</span><button className="btn-ghost" disabled={(columnPage+1)*8>=current.columns.length} onClick={()=>setColumnPage(value=>value+1)}>Next discovery columns</button></>}</div>
   <details><summary>Complete native discovery response</summary><pre>{run.raw}</pre></details>
  </section>
 </>;
}
