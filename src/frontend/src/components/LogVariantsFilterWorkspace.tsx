import {useEffect,useRef,useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile,referenceLogFilterStringText} from "../reference-log-filters";
import {LOG_VARIANTS_OPERATIONS,LOG_VARIANTS_LABELS,defaultLogVariantsForm,logVariantsOriginalExample,logVariantsSourceExample,logVariantsCallFileText,requestLogVariants,logVariantsArtifactFile,logVariantsResponseFile,logVariantsCountText,LogVariantsRequestGate,type LogVariantsForm,type LogVariantsRun,type LogVariantsArtifact} from "../log-variants-filters";

export function LogVariantsFilterWorkspace({initialForm}:{initialForm?:LogVariantsForm}){
  const [source,setSource]=useState<File|null>(null),[form,setForm]=useState(()=>initialForm??defaultLogVariantsForm());
  const [pending,setPending]=useState(false),[importing,setImporting]=useState(false),[error,setError]=useState<string|null>(null),[run,setRun]=useState<LogVariantsRun|null>(null);
  const generation=useRef(0),gate=useRef(new LogVariantsRequestGate()),request=useRef<AbortController|null>(null),current=useRef({source,form});current.current={source,form};
  const cancel=()=>{gate.current.invalidate();request.current?.abort();request.current=null;setPending(false);};
  const edit=(action:()=>void)=>{++generation.current;setImporting(false);cancel();setError(null);setRun(null);action();};
  const change=<K extends keyof LogVariantsForm>(key:K,value:LogVariantsForm[K])=>edit(()=>setForm(previous=>({...previous,[key]:value})));
  useEffect(()=>()=>{++generation.current;gate.current.invalidate();request.current?.abort();},[]);
  const importCall=async(file:File|null)=>{edit(()=>{});if(!file)return;const ticket=generation.current;setImporting(true);
    try{const raw=await logVariantsCallFileText(file);if(ticket===generation.current)edit(()=>setForm(previous=>({...previous,callJson:raw})));}
    catch(cause){if(ticket===generation.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(ticket===generation.current)setImporting(false);}};
  const execute=async()=>{const identity=current.current;if(!identity.source)return;cancel();setRun(null);setError(null);
    const controller=new AbortController(),ticket=gate.current.issue(identity);request.current=controller;setPending(true);
    try{const result=await requestLogVariants(identity.source,identity.form,controller.signal);if(gate.current.accepts(ticket,current.current,controller.signal))setRun(result);}
    catch(cause){if(gate.current.accepts(ticket,current.current,controller.signal))setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <div className="reference-log-variants-workspace"><section className="panel query-workspace" aria-labelledby="log-variants-heading">
    <div className="panel-head"><h2 id="log-variants-heading">Activity-variant calls</h2></div>
    <p className="query-intro">Match complete case variants, compare coverage, or find a threshold using exact typed operands and supplied traces.</p>
    <fieldset className="query-fields"><legend>Source log</legend>
      <label className="filters-field"><span>Variants native event log file</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setSource(file));}}/><small>{source?`${source.name} (${source.size} bytes)`:"Choose complete native EventLog JSON. Every operation, including the threshold helper, requires this source."}</small></label>
      <button className="btn-ghost" onClick={()=>{const file=logVariantsSourceExample();downloadQueryBlob(file,file.name);}}>Download variants example native log</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Original typed call</legend>
      <label className="filters-field"><span>Variants operation</span><select value={form.operation} onChange={event=>{const operation=event.target.value as LogVariantsForm["operation"];edit(()=>setForm({operation,callJson:logVariantsOriginalExample(operation)}));}}>{LOG_VARIANTS_OPERATIONS.map(operation=><option key={operation} value={operation}>{LOG_VARIANTS_LABELS[operation]}</option>)}</select></label>
      <label className="query-text"><span>Complete original log variants call JSON</span><textarea rows={17} spellCheck={false} value={form.callJson} onChange={event=>change("callJson",event.target.value)}/><small>Args exclude the actual source log. Ordered keyword_entries and original Packed type tags are sent unchanged, including exact integer tokens.</small></label>
      <label className="filters-field"><span>Import original log variants call JSON file</span><input type="file" accept=".json,.txt" onChange={event=>void importCall(referenceLogFilterPickedFile(event.target))}/></label>
      {importing&&<p role="status">Reading original variants call…</p>}
      <button className="btn-ghost" onClick={()=>change("callJson",logVariantsOriginalExample(form.operation))}>Load original variants call example</button>
      <p>Collections use values; dictionaries use ordered items with typed key/value pairs. A tuple sequence matches a complete variant; a list remains a list. None, omitted arguments and empty dictionaries remain distinct.</p>
      <p>Supplied trace lists use <code>{'{"type":"trace_ref","source_trace_index":0}'}</code> to copy a complete trace from this source. Keep each dictionary entry and its trace list separate. Source indices are checked against the actual uploaded log.</p>
      {form.operation==="find_auto_threshold"&&<p>This helper returns a typed numeric artifact, not a filtered log. It still uses the complete source and supplied variant traces.</p>}
      <div className="query-actions"><button className="btn-open" disabled={pending||importing||source===null} onClick={()=>void execute()}>Run original variants call</button>{pending&&<><span role="status">Processing variants…</span><button className="btn-ghost" onClick={cancel}>Cancel variants processing</button></>}</div>
      {!source&&<p>Choose a native event log file to run this operation.</p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run&&<LogVariantsFilterInspection run={run} onUseLog={file=>edit(()=>setSource(file))}/>}</div>;
}
const artifacts:Array<[LogVariantsArtifact,string]>=[["call_json","exact original variants call"],["controls_before_json","complete controls before processing"],["controls_after_json","complete controls after processing"],["source_log_json","complete parsed source log"],["source_after_log_json","source log after processing"]];
export function LogVariantsFilterInspection({run,onUseLog}:{run:LogVariantsRun;onUseLog?:(file:File)=>void}){
  const value=run.value,success=value.http_status===200&&value.error===null,scalar=value.operation==="find_auto_threshold";
  const download=(key:LogVariantsArtifact)=>{const file=logVariantsArtifactFile(run,key);downloadQueryBlob(file,file.name);};
  return <section className="panel" aria-labelledby="log-variants-result-heading"><div className="panel-head"><h2 id="log-variants-result-heading">{success?scalar?"Automatic variant threshold":"Filtered activity variants":"Variants operation could not be completed"}</h2></div><div className="query-results">
    {value.error!==null&&<div className="error-banner" role="alert">{value.error_type!==null?`${value.error_type}: `:""}{referenceLogFilterStringText(value.error)}</div>}
    <p>Operation: {value.operation??"unavailable"}; phase: {value.phase}; HTTP {value.http_status}.</p><p>{run.sourceFile.name}</p>
    {value.source&&<table><caption>{scalar?"Source log for threshold calculation":"Source and filtered log"}</caption><thead><tr><th scope="col">Log</th><th scope="col">Cases</th><th scope="col">Events</th></tr></thead><tbody>{([["Source",value.source],...(value.result?[["Filtered",value.result]as const]:[])]as const).map(([label,count])=><tr key={label}><th scope="row">{label}</th><td>{logVariantsCountText(count,"traces")}</td><td>{logVariantsCountText(count,"events")}</td></tr>)}</tbody></table>}
    {success&&scalar&&<p>The numeric result retains its complete native floating-point storage in the downloadable typed artifact.</p>}
    {success&&scalar&&value.value_json!==null&&<details><summary>Complete typed threshold artifact</summary><pre aria-label="Complete typed threshold artifact">{referenceLogFilterStringText(value.value_json)}</pre></details>}
    {success&&!scalar&&value.result?.traces===0&&<p>No cases matched this condition.</p>}
    {([["Complete source log",value.source_unchanged],["Complete parsed call controls",value.controls_unchanged]]as const).map(([label,unchanged])=>unchanged===null?null:<p key={label} role={unchanged?undefined:"status"}>{label} {unchanged?"was preserved.":"changed. Inspect the complete before and after snapshots."}</p>)}
    <div className="query-actions">{artifacts.map(([key,label])=>value[key]!==null&&<button key={key} className="btn-ghost" onClick={()=>download(key)}>Download {label}</button>)}</div>
    <div className="query-actions">{success&&!scalar&&value.filtered_log_json!==null&&<><button className="btn-ghost" onClick={()=>download("filtered_log_json")}>Download filtered native log JSON</button>{onUseLog&&<button className="btn-open" onClick={()=>onUseLog(logVariantsArtifactFile(run,"filtered_log_json"))}>Use filtered native log as source</button>}</>}{success&&!scalar&&value.filtered_xes!==null&&value.xes_error===null&&<button className="btn-ghost" onClick={()=>download("filtered_xes")}>Download filtered XES</button>}{success&&scalar&&value.value_json!==null&&<button className="btn-ghost" onClick={()=>download("value_json")}>Download complete typed threshold</button>}<button className="btn-ghost" onClick={()=>{const file=logVariantsResponseFile(run);downloadQueryBlob(file,file.name);}}>Download complete variants response</button></div>
    {value.xes_error!==null&&<p role="status">XES export is unavailable: {referenceLogFilterStringText(value.xes_error)}. Native JSON retains the result.</p>}
    {!scalar&&<p>Native JSON preserves stored types and full context. XES can project bytes and containers to text.</p>}
  </div></section>;
}
