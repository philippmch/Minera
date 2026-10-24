import {useEffect,useRef,useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile,referenceLogFilterStringText} from "../reference-log-filters";
import {LOG_TIMESTAMP_OPERATIONS,LOG_TIMESTAMP_LABELS,LOG_TIMESTAMP_PREDICATES,defaultLogTimestampForm,logTimestampExample,logTimestampSourceExample,logTimestampCallText,logTimestampCallFile,requestLogTimestamp,logTimestampArtifactFile,logTimestampResponseFile,logTimestampCountText,logTimestampBoolean,logTimestampCanReuse,LogTimestampRequestGate,type LogTimestampForm,type LogTimestampRun,type LogTimestampArtifact} from "../log-timestamp-original-call";

export function LogTimestampOriginalCallWorkspace({initialForm}:{initialForm?:LogTimestampForm}){
  const [source,setSource]=useState<File|null>(null),[form,setForm]=useState(()=>initialForm??defaultLogTimestampForm());
  const [ownedExample,setOwnedExample]=useState(initialForm===undefined),[pending,setPending]=useState(false),[importing,setImporting]=useState(false),[error,setError]=useState<string|null>(null),[run,setRun]=useState<LogTimestampRun|null>(null);
  const generation=useRef(0),gate=useRef(new LogTimestampRequestGate()),request=useRef<AbortController|null>(null),current=useRef({source,form});current.current={source,form};
  const cancel=()=>{gate.current.invalidate();request.current?.abort();request.current=null;setPending(false);};
  const edit=(action:()=>void)=>{++generation.current;setImporting(false);cancel();setRun(null);setError(null);action();};
  useEffect(()=>()=>{++generation.current;gate.current.invalidate();request.current?.abort();},[]);
  const importCall=async(file:File|null)=>{edit(()=>{});if(!file)return;const ticket=generation.current;setImporting(true);
    try{const raw=await logTimestampCallText(file);if(ticket===generation.current)edit(()=>{setOwnedExample(false);setForm(previous=>({...previous,callJson:raw}));});}
    catch(cause){if(ticket===generation.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(ticket===generation.current)setImporting(false);}};
  const execute=async()=>{const identity=current.current;if(!identity.source)return;cancel();setRun(null);setError(null);const controller=new AbortController(),ticket=gate.current.issue(identity);request.current=controller;setPending(true);
    try{const result=await requestLogTimestamp(identity.source,identity.form,controller.signal);if(gate.current.accepts(ticket,current.current,controller.signal))setRun(result);}
    catch(cause){if(gate.current.accepts(ticket,current.current,controller.signal))setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <div className="reference-log-timestamp-workspace"><section className="panel query-workspace" aria-labelledby="log-timestamp-heading"><div className="panel-head"><h2 id="log-timestamp-heading">Original timestamp calls</h2></div>
    <p className="query-intro">Filter complete logs by time, or test a selected trace and inspect its Boolean result.</p>
    <fieldset className="query-fields"><legend>Source log</legend>
      <label className="filters-field"><span>Timestamp native event log file</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setSource(file));}}/><small>{source?`${source.name} (${source.size} bytes)`:"Choose complete native EventLog JSON with stored timestamps and context."}</small></label>
      <button className="btn-ghost" onClick={()=>{const file=logTimestampSourceExample();downloadQueryBlob(file,file.name);}}>Download timestamp example native log</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Original typed call</legend>
      <label className="filters-field"><span>Timestamp operation</span><select value={form.operation} onChange={event=>{const operation=event.target.value as LogTimestampForm["operation"];edit(()=>setForm(previous=>ownedExample?logTimestampExample(operation):{...previous,operation}));}}>{LOG_TIMESTAMP_OPERATIONS.map(operation=><option key={operation} value={operation}>{LOG_TIMESTAMP_LABELS[operation]} ({operation})</option>)}</select></label>
      <label className="query-text"><span>Complete original timestamp call JSON</span><textarea rows={17} spellCheck={false} value={form.callJson} onChange={event=>{const callJson=event.target.value;edit(()=>{setOwnedExample(false);setForm(previous=>({...previous,callJson}));});}}/><small>Args contain only non-source operands. Ordered keyword_entries and exact integer tokens are sent unchanged.</small></label>
      <label className="filters-field"><span>Import original timestamp call JSON file</span><input type="file" accept=".json,.txt" onChange={event=>void importCall(referenceLogFilterPickedFile(event.target))}/></label>
      {importing&&<p role="status">Reading timestamp call…</p>}
      <div className="query-actions"><button className="btn-ghost" onClick={()=>edit(()=>{setOwnedExample(true);setForm(logTimestampExample(form.operation));})}>Load timestamp call example</button><button className="btn-ghost" onClick={()=>{const file=logTimestampCallFile(form);downloadQueryBlob(file,file.name);}}>Download current timestamp call</button></div>
      <p>Imported or edited text stays unchanged when you select another operation. Loading an example replaces it.</p>
      {LOG_TIMESTAMP_PREDICATES.includes(form.operation)?<p>This predicate uses source_trace_index to select the actual trace in the uploaded log. It returns true or false. Its datetime operands are typed Packed values.</p>:<p>Time filters receive a caller-owned options object. The source-only helpers preserve their native diagnostics. Unused keyword data remains available in the call download.</p>}
      <div className="query-actions"><button className="btn-open" disabled={pending||importing||source===null} onClick={()=>void execute()}>Run original timestamp call</button>{pending&&<><span role="status">Processing timestamp call…</span><button className="btn-ghost" onClick={cancel}>Cancel timestamp processing</button></>}</div>
      {!source&&<p>Choose a native event log file to run this call.</p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run&&<LogTimestampInspection run={run} onUseLog={file=>edit(()=>setSource(file))}/>}</div>;
}
const artifacts:Array<[LogTimestampArtifact,string]>=[["call_json","exact timestamp call"],["call_projection_json","native call projection"],["source_log_json","complete source before processing"],["source_after_log_json","complete source after processing"],["controls_before_json","actual controls before processing"],["controls_after_json","actual controls after processing"],["filtered_log_json","returned native log JSON"],["value_json","typed Boolean result"],["filtered_xes","returned XES"]];
export function LogTimestampInspection({run,onUseLog}:{run:LogTimestampRun;onUseLog?:(file:File)=>void}){
  const v=run.value,success=v.http_status===200&&v.error===null&&v.inspection_error===null,bool=logTimestampBoolean(v.value_json);
  return <section className="panel" aria-labelledby="log-timestamp-result-heading"><div className="panel-head"><h2 id="log-timestamp-result-heading">{success?v.result_kind==="bool"?"Trace predicate result":"Filtered timestamp log":"Timestamp call could not be completed"}</h2></div><div className="query-results">
    {v.error!==null&&<div className="error-banner" role="alert">{v.error_type===null?"":`${referenceLogFilterStringText(v.error_type)}: `}{referenceLogFilterStringText(v.error)}</div>}
    {v.inspection_error!==null&&<div className="error-banner" role="alert">Inspection failed: {referenceLogFilterStringText(v.inspection_error)}</div>}
    <p>{run.sourceFile.name}; operation: {v.operation??"unavailable"}; phase: {v.phase}; HTTP {v.http_status}.</p>
    <p>Native call returned: {v.native_returned?"yes":"no"}; result kind: {v.result_kind??"unavailable"}.</p>
    {bool!==null&&<p role="status">Boolean result: <strong>{String(bool)}</strong></p>}
    {v.source&&<table><caption>Source and returned log</caption><thead><tr><th scope="col">Log</th><th scope="col">Cases</th><th scope="col">Events</th></tr></thead><tbody>{([["Source",v.source],...(v.result?[["Returned",v.result]as const]:[])]as const).map(([label,count])=><tr key={label}><th scope="row">{label}</th><td>{logTimestampCountText(count,"traces")}</td><td>{logTimestampCountText(count,"events")}</td></tr>)}</tbody></table>}
    {success&&v.result?.traces===0&&<p>No cases matched this condition.</p>}
    {([["Complete source",v.source_unchanged],["Actual caller controls",v.controls_unchanged]]as const).map(([label,unchanged])=><p key={label} role={unchanged===false?"status":undefined}>{label}: {unchanged===null?"comparison unavailable":unchanged?"preserved":"changed; inspect the before and after snapshots"}.</p>)}
    {v.call_projection_json!==null&&<details><summary>Native call projection</summary><pre aria-label="Native call projection">{referenceLogFilterStringText(v.call_projection_json)}</pre></details>}
    {v.value_json!==null&&<details><summary>Complete typed Boolean artifact</summary><pre>{referenceLogFilterStringText(v.value_json)}</pre></details>}
    <div className="query-actions">{artifacts.map(([key,label])=>v[key]!==null&&<button key={key} className="btn-ghost" onClick={()=>{const file=logTimestampArtifactFile(run,key);downloadQueryBlob(file,file.name);}}>Download {label}</button>)}{onUseLog&&logTimestampCanReuse(v)&&<button className="btn-open" onClick={()=>onUseLog(logTimestampArtifactFile(run,"filtered_log_json"))}>Use returned native log as source</button>}<button className="btn-ghost" onClick={()=>{const file=logTimestampResponseFile(run);downloadQueryBlob(file,file.name);}}>Download complete timestamp response</button></div>
    {v.xes_error!==null&&<p role="status">XES export is unavailable: {referenceLogFilterStringText(v.xes_error)}. Native JSON retains the result.</p>}
    <p>Native snapshots retain stored values and context. A predicate trace wrapper is an inspection artifact; the source download contains the complete backing log.</p>
  </div></section>;
}
