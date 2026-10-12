import { useEffect,useMemo,useRef,useState } from "react";
import { GENERATOR_CONTROLS,GENERATION_PLAYOUT_CONTROLS,defaultGenerationForm,generateTrees,generationReport,generationTables,type TreeGenerationResult } from "../tree-generation";
import { type MineraExport } from "../types";
import { computeTreeLayout } from "../tree-layout";
import { ProcessTree } from "./ProcessTree";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeResponseJson,nativeResponseRequest } from "../native-response";
import { nativeJson } from "../native-json";
import { downloadQueryBlob } from "../log-query";

export function TreeGenerationWorkspace({onOpenLog,onUseModel,sourceAvailable=false}:{
  onOpenLog:(file:File)=>void;onUseModel?:(file:File)=>Promise<void>;sourceAvailable?:boolean;
}) {
  const [form,setForm]=useState(defaultGenerationForm);const [result,setResult]=useState<TreeGenerationResult|null>(null);
  const [index,setIndex]=useState(0);const [tableId,setTableId]=useState("tree_nodes");const [pending,setPending]=useState(false);
  const [error,setError]=useState<string|null>(null);const controller=useRef<AbortController|null>(null);
  useEffect(()=>()=>{controller.current?.abort();controller.current=null;},[]);
  const execute=async()=>{const current=new AbortController();controller.current=current;setPending(true);setError(null);
    try{const next=await generateTrees(form,current.signal);if(controller.current===current&&!current.signal.aborted){setResult(next);setIndex(0);setTableId("tree_nodes");}}
    catch(cause){if(controller.current===current&&!current.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(controller.current===current){controller.current=null;setPending(false);}}
  };
  const selected=result?.models[index];const tables=useMemo(()=>selected?generationTables(selected):[],[selected]);
  const table=tables.find(item=>item.id===tableId)??tables[0];
  const layout=useMemo(()=>selected?computeTreeLayout(selected.tree.root,{activities:selected.tree.activities.map(name=>({name}))} as MineraExport):null,[selected]);
  const download=(raw:string,name:string,type="application/json")=>downloadQueryBlob(new Blob([raw],{type}),name);
  const artifact=(name:"ptml"|"pnml"|"bpmn"|"playout_xes")=>new File([selected?.[name]??""],`generated-model-${index}.${name==="playout_xes"?"xes":name}`,{type:"application/xml"});
  const checkModel=async()=>{if(!onUseModel||!selected?.pnml)return;setError(null);try{await onUseModel(artifact("pnml"));}catch(cause){setError(cause instanceof Error?cause.message:String(cause));}};
  return <>
    <section className="panel" aria-label="Generate process trees">
      <div className="panel-head"><h2>Generate process trees and logs</h2><span className="panel-meta">Independent native models</span></div>
      <p>Create a BASIC recursive tree or a population from the PT-and-log generator. Choose operators, activity counts and seeds, then inspect each model and its generated events.</p>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Tree generator</span><select value={form.variant} onChange={event=>setForm({...form,variant:event.target.value as typeof form.variant})}><option value="ptandloggenerator">PT-and-log generator</option><option value="basic">BASIC recursive generator</option></select></label>
          <label className="filters-field"><span>Generated tree conversion</span><select value={form.conversion} onChange={event=>setForm({...form,conversion:event.target.value as typeof form.conversion})}><option value="standard">Standard</option><option value="transition_bordered">Transition bordered</option></select></label>
          {GENERATOR_CONTROLS.filter(([,,,variant])=>variant==="both"||variant===form.variant).map(([key,label,value])=><label className="filters-field" key={key}><span>{label}</span><input value={form.generator[key]} placeholder={value||"native defaults"} onChange={event=>setForm({...form,generator:{...form.generator,[key]:event.target.value}})} /></label>)}
        </div>
        <p>Blank fields use native defaults. Seeds are sent as entered, including full 64-bit integers. BASIC returns one tree; PT-and-log supports an empty or multiple-model population.</p>
        <label><input type="checkbox" checked={form.withLog} onChange={event=>setForm({...form,withLog:event.target.checked})}/> Generate a log from every model</label>
        {form.withLog&&<><div className="query-options">
          <label className="filters-field"><span>Generated log playout</span><select value={form.playoutVariant} onChange={event=>setForm({...form,playoutVariant:event.target.value as typeof form.playoutVariant})}><option value="TOPBOTTOM">Top-bottom sampling</option><option value="BASIC_PLAYOUT">Basic sampling</option><option value="EXTENSIVE">Extensive enumeration</option></select></label>
          {GENERATION_PLAYOUT_CONTROLS.filter(([,,,variant])=>variant==="both"||(variant==="extensive")===(form.playoutVariant==="EXTENSIVE")).map(([key,label,value])=><label className="filters-field" key={key}><span>{label}</span><input value={form.playout[key]} placeholder={value} onChange={event=>setForm({...form,playout:{...form.playout,[key]:event.target.value}})} /></label>)}
        </div>{form.playoutVariant==="EXTENSIVE"&&<label><input type="checkbox" checked={form.returnStrings} onChange={event=>setForm({...form,returnStrings:event.target.checked})}/> Return enumerated activity words</label>}
        <p>Generated logs are bounded to one million events across all models. Enumeration retains its native truncation flag and configured length, case and loop limits.</p></>}
        <button className="btn-open" onClick={()=>void execute()}>Generate trees and logs</button>
      </fieldset>
      {pending&&<p role="status">Generating… <button className="btn-ghost" onClick={()=>{controller.current?.abort();controller.current=null;setPending(false);}}>Cancel tree generation</button></p>}
      {error&&<p className="error-banner" role="alert">{error}</p>}
      {result&&<div className="query-results"><p>{result.generator.variant} · {result.models.length} generated models · {result.tree_variant} conversion. Results below retain the settings of this completed run.</p>
        <details><summary>Submitted tree-generation request</summary><pre>{nativeJson(nativeResponseRequest(result),2)}</pre></details>
        <div className="query-actions"><button className="btn-ghost" onClick={()=>download(nativeResponseJson(result),"minera-generated-trees-native.json")}>Download complete generation JSON</button><button className="btn-ghost" onClick={()=>download(generationReport(result),"minera-generated-trees-report.json")}>Download tree generation report</button></div>
        {!result.models.length?<p>No models were requested. The complete empty population remains downloadable.</p>:<label className="filters-field"><span>Generated model to inspect</span><select value={index} onChange={event=>{setIndex(Number(event.target.value));setTableId("tree_nodes");}}>{result.models.map(model=><option key={model.model_index} value={model.model_index}>Model #{model.model_index} · {model.tree.activities.length} activities{model.playout?` · ${model.playout.case_count} cases`:""}</option>)}</select></label>}
      </div>}
    </section>
    {selected&&layout&&<ProcessTree key={result?.generator.variant+":"+index} layout={layout} title={`Generated model #${index}`} />}
    {selected&&table&&<section className="panel" aria-label="Generated model inspection"><div className="panel-head"><h2>Generated model #{index}</h2><span className="panel-meta">Complete model and log data</span></div>
      <div className="query-actions">
        {(["ptml","pnml","bpmn"] as const).map(name=><button key={name} className="btn-ghost" disabled={!selected[name]} onClick={()=>downloadQueryBlob(artifact(name),artifact(name).name)}>Download generated {name.toUpperCase()}</button>)}
        <button className="btn-ghost" onClick={()=>download(nativeJson(selected,2),`generated-model-${index}.json`)}>Download selected generated model JSON</button>
        <button className="btn-ghost" onClick={()=>download(diagnosticCsv(table),`generated-model-${index}-${encodeURIComponent(table.id)}.csv`,"text/csv;charset=utf-8")}>Download complete generation table CSV</button>
        {selected.playout_xes&&<><button className="btn-ghost" onClick={()=>downloadQueryBlob(artifact("playout_xes"),artifact("playout_xes").name)}>Download generated log XES</button><button className="btn-open" onClick={()=>onOpenLog(artifact("playout_xes"))}>Open generated log for mining</button></>}
        {onUseModel&&<button className="btn-ghost" disabled={!sourceAvailable||!selected.pnml||pending} onClick={()=>void checkModel()}>Check generated model against current log</button>}
      </div>
      {(["ptml_error","pnml_error","bpmn_error","playout_error"] as const).map(key=>selected[key]&&<p key={key} className="error-inline">{key}: {selected[key]}</p>)}
      {selected.playout&&<p>{selected.playout.variant} · {selected.playout.case_count} cases · {selected.playout.event_count} events · {selected.playout.truncated?"enumeration truncated":"native run not truncated"}</p>}
      <label className="filters-field"><span>Generation result table</span><select value={table.id} onChange={event=>setTableId(event.target.value)}>{tables.map(item=><option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={index+":"+table.id} table={table} noun="generation" />
      <details><summary>Complete selected model and generated log JSON</summary><pre>{nativeJson(selected,2)}</pre></details>
    </section>}
  </>;
}
