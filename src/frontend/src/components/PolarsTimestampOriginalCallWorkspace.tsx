import {useEffect,useRef,useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile,referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText} from "../table-attribute-repetition";
import {tableTimestampSourceExample} from "../table-timestamp-filters";
import {POLARS_TIMESTAMP_ORIGINAL_OPERATIONS,POLARS_TIMESTAMP_ORIGINAL_LABELS,defaultPolarsTimestampOriginalForm,polarsTimestampOriginalExample,polarsTimestampOriginalCallText,polarsTimestampOriginalCallFile,requestPolarsTimestampOriginal,polarsTimestampOriginalArtifactFile,polarsTimestampOriginalResponseFile,polarsTimestampOriginalCanReuse,PolarsTimestampOriginalRequestGate,type PolarsTimestampOriginalForm,type PolarsTimestampOriginalExample,type PolarsTimestampOriginalArtifact,type PolarsTimestampOriginalRun} from "../polars-timestamp-original-call";

export function PolarsTimestampOriginalCallWorkspace({initialForm}:{initialForm?:PolarsTimestampOriginalForm}){
  const [source,setSource]=useState<File|null>(null),[form,setForm]=useState(()=>initialForm??defaultPolarsTimestampOriginalForm());
  const [ownedExample,setOwnedExample]=useState(initialForm===undefined),[example,setExample]=useState<PolarsTimestampOriginalExample>("operation");
  const [pending,setPending]=useState(false),[importing,setImporting]=useState(false),[error,setError]=useState<string|null>(null),[run,setRun]=useState<PolarsTimestampOriginalRun|null>(null);
  const generation=useRef(0),gate=useRef(new PolarsTimestampOriginalRequestGate()),request=useRef<AbortController|null>(null),current=useRef({source,form});current.current={source,form};
  const cancel=()=>{gate.current.invalidate();request.current?.abort();request.current=null;setPending(false);};
  const edit=(action:()=>void)=>{++generation.current;setImporting(false);cancel();setRun(null);setError(null);action();};
  useEffect(()=>()=>{++generation.current;gate.current.invalidate();request.current?.abort();},[]);
  const importCall=async(file:File|null)=>{edit(()=>{});if(!file)return;const ticket=generation.current;setImporting(true);
    try{const raw=await polarsTimestampOriginalCallText(file);if(ticket===generation.current)edit(()=>{setOwnedExample(false);setForm(previous=>({...previous,callJson:raw}));});}
    catch(cause){if(ticket===generation.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(ticket===generation.current)setImporting(false);}};
  const execute=async()=>{const identity=current.current;if(!identity.source)return;cancel();setRun(null);setError(null);const controller=new AbortController(),ticket=gate.current.issue(identity);request.current=controller;setPending(true);
    try{const result=await requestPolarsTimestampOriginal(identity.source,identity.form,controller.signal);if(gate.current.accepts(ticket,current.current,controller.signal))setRun(result);}
    catch(cause){if(gate.current.accepts(ticket,current.current,controller.signal))setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <div className="reference-polars-timestamp-original-workspace"><section className="panel query-workspace" aria-labelledby="polars-timestamp-original-heading"><div className="panel-head"><h2 id="polars-timestamp-original-heading">Original Polars timestamp calls</h2></div>
    <p className="query-intro">Run a time-window filter with complete typed bounds and ordered parameters, then inspect every native artifact.</p>
    <fieldset className="query-fields"><legend>Complete native table</legend>
      <label className="filters-field"><span>Original Polars timestamp source table</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setSource(file));}}/><small>{source?`${source.name} (${source.size} bytes)`:"Choose a genuine native EventTable JSON File with full rows, stored types, index and metadata."}</small></label>
      <button className="btn-ghost" onClick={()=>{const file=tableTimestampSourceExample("polars");downloadQueryBlob(file,file.name);}}>Download original Polars timestamp example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Original typed call</legend>
      <label className="filters-field"><span>Original Polars timestamp operation</span><select value={form.operation} onChange={event=>{const operation=event.target.value as PolarsTimestampOriginalForm["operation"];edit(()=>setForm(previous=>ownedExample?polarsTimestampOriginalExample(operation,example):{...previous,operation}));}}>{POLARS_TIMESTAMP_ORIGINAL_OPERATIONS.map(operation=><option key={operation} value={operation}>{POLARS_TIMESTAMP_ORIGINAL_LABELS[operation]} ({operation})</option>)}</select></label>
      <label className="query-text"><span>Complete original Polars timestamp call JSON</span><textarea rows={17} spellCheck={false} value={form.callJson} onChange={event=>{const callJson=event.target.value;edit(()=>{setOwnedExample(false);setForm(previous=>({...previous,callJson}));});}}/><small>source_kind selects an eager DataFrame or a LazyFrame. Ordered keyword_entries carry dt1 and dt2, plus attribute and attribute_value for the attribute filter; the helpers take only parameters. The text is sent unchanged, including integer tokens.</small></label>
      <label className="filters-field"><span>Import original Polars timestamp call JSON file</span><input type="file" accept=".json,.txt" onChange={event=>void importCall(referenceLogFilterPickedFile(event.target))}/></label>
      {importing&&<p role="status">Reading original Polars timestamp call…</p>}
      <label className="filters-field"><span>Original Polars timestamp example</span><select value={example} onChange={event=>{const chosen=event.target.value as PolarsTimestampOriginalExample;edit(()=>setExample(chosen));}}><option value="operation">Lazy frame with canonical string bounds</option><option value="eager">Eager DataFrame source</option><option value="typed">Typed UTC datetime bounds</option><option value="mapping">String parameter mapping</option><option value="enum">Ordered enum and string keys</option><option value="raw">Raw parameter diagnostic</option></select></label>
      <div className="query-actions"><button className="btn-ghost" onClick={()=>edit(()=>{setOwnedExample(true);setForm(polarsTimestampOriginalExample(form.operation,example));})}>Load original Polars timestamp example</button><button className="btn-ghost" onClick={()=>{const file=polarsTimestampOriginalCallFile(form);downloadQueryBlob(file,file.name);}}>Download current Polars timestamp call</button></div>
      <p>String bounds use the strict <code>YYYY-MM-DD HH:mm:ss</code> form; other spellings keep their native diagnostics. Typed datetime bounds keep their wall clock and are compared as UTC. A LazyFrame reports schema errors in the collect phase. Any parameter dictionary keeps its ordered enum/string bindings. Imported or edited text stays unchanged when you select another operation.</p>
      <div className="query-actions"><button className="btn-open" disabled={pending||importing||source===null} onClick={()=>void execute()}>Run original Polars timestamp call</button>{pending&&<><span role="status">Processing original Polars timestamp call…</span><button className="btn-ghost" onClick={cancel}>Cancel original Polars timestamp processing</button></>}</div>
      {!source&&<p>Choose a native source table to run this call.</p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run&&<PolarsTimestampOriginalInspection run={run} onUseTable={file=>edit(()=>setSource(file))}/>}</div>;
}
const artifacts:Array<[PolarsTimestampOriginalArtifact,string]>=[["call_json","exact original Polars timestamp call"],["call_projection_json","native timestamp call projection"],["source_table_json","complete original timestamp source table"],["source_after_table_json","original timestamp source after processing"],["controls_before_json","complete original timestamp controls before processing"],["controls_after_json","complete original timestamp controls after processing"]];
export function PolarsTimestampOriginalInspection({run,onUseTable}:{run:PolarsTimestampOriginalRun;onUseTable?:(file:File)=>void}){
  const v=run.value,success=polarsTimestampOriginalCanReuse(v);
  const download=(key:PolarsTimestampOriginalArtifact)=>{const file=polarsTimestampOriginalArtifactFile(run,key);downloadQueryBlob(file,file.name);};
  return <section className="panel" aria-labelledby="polars-timestamp-original-result-heading"><div className="panel-head"><h2 id="polars-timestamp-original-result-heading">{success?"Filtered original Polars timestamps":"Original Polars timestamp call could not be completed"}</h2></div><div className="query-results">
    {v.error!==null&&<div role="alert" className="error-banner">{v.error_type!==null?`${referenceLogFilterStringText(v.error_type)}: `:""}{referenceLogFilterStringText(v.error)}</div>}
    <p>{run.sourceFile.name} · operation: {v.operation??"unavailable"}; phase: {v.phase}; lazy plan or eager call returned: {v.apply_returned?"yes":"no"}.</p>
    {v.source&&<table><caption>Original source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([["Source",v.source],...(v.result?[["Filtered",v.result]as const]:[])]as const).map(([label,count])=><tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count,"rows")}</td><td>{tableRepetitionCountText(count,"columns")}</td></tr>)}</tbody></table>}
    {success&&v.result?.rows===0&&<p>No rows matched this time window.</p>}
    {([["Complete source table",v.source_unchanged],["Complete caller controls",v.controls_unchanged]]as const).map(([label,unchanged])=>unchanged===null?null:<p key={label} role={unchanged?undefined:"status"}>{label} {unchanged?"was preserved.":"changed. Inspect the complete native snapshots."}</p>)}
    {v.call_projection_json!==null&&<details><summary>Native timestamp call projection</summary><pre aria-label="Native timestamp call projection">{referenceLogFilterStringText(v.call_projection_json)}</pre></details>}
    <div className="query-actions">{artifacts.map(([key,label])=>v[key]!==null&&<button key={key} className="btn-ghost" onClick={()=>download(key)}>Download {label}</button>)}</div>
    <div className="query-actions">{success&&<><button className="btn-ghost" onClick={()=>download("filtered_table_json")}>Download original filtered native timestamp table</button>{onUseTable&&<button className="btn-open" onClick={()=>onUseTable(polarsTimestampOriginalArtifactFile(run,"filtered_table_json"))}>Use filtered timestamp table as source</button>}</>}<button className="btn-ghost" onClick={()=>{const file=polarsTimestampOriginalResponseFile(run);downloadQueryBlob(file,file.name);}}>Download complete original Polars timestamp response</button></div>
    <p>Native JSON preserves stored table values, index and metadata. Original mode has no inferred friendly column selectors.</p>
  </div></section>;
}
