import {useEffect,useRef,useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile,referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText} from "../table-attribute-repetition";
import {tableTimestampSourceExample} from "../table-timestamp-filters";
import {PANDAS_TIMESTAMP_ORIGINAL_OPERATIONS,PANDAS_TIMESTAMP_ORIGINAL_LABELS,defaultPandasTimestampOriginalForm,pandasTimestampOriginalExample,pandasTimestampOriginalCallText,pandasTimestampOriginalCallFile,requestPandasTimestampOriginal,pandasTimestampOriginalArtifactFile,pandasTimestampOriginalResponseFile,pandasTimestampOriginalCanReuse,PandasTimestampOriginalRequestGate,type PandasTimestampOriginalForm,type PandasTimestampOriginalExample,type PandasTimestampOriginalArtifact,type PandasTimestampOriginalRun} from "../pandas-timestamp-original-call";

export function PandasTimestampOriginalCallWorkspace({initialForm}:{initialForm?:PandasTimestampOriginalForm}){
  const [source,setSource]=useState<File|null>(null),[form,setForm]=useState(()=>initialForm??defaultPandasTimestampOriginalForm());
  const [ownedExample,setOwnedExample]=useState(initialForm===undefined),[example,setExample]=useState<PandasTimestampOriginalExample>("operation");
  const [pending,setPending]=useState(false),[importing,setImporting]=useState(false),[error,setError]=useState<string|null>(null),[run,setRun]=useState<PandasTimestampOriginalRun|null>(null);
  const generation=useRef(0),gate=useRef(new PandasTimestampOriginalRequestGate()),request=useRef<AbortController|null>(null),current=useRef({source,form});current.current={source,form};
  const cancel=()=>{gate.current.invalidate();request.current?.abort();request.current=null;setPending(false);};
  const edit=(action:()=>void)=>{++generation.current;setImporting(false);cancel();setRun(null);setError(null);action();};
  useEffect(()=>()=>{++generation.current;gate.current.invalidate();request.current?.abort();},[]);
  const importCall=async(file:File|null)=>{edit(()=>{});if(!file)return;const ticket=generation.current;setImporting(true);
    try{const raw=await pandasTimestampOriginalCallText(file);if(ticket===generation.current)edit(()=>{setOwnedExample(false);setForm(previous=>({...previous,callJson:raw}));});}
    catch(cause){if(ticket===generation.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(ticket===generation.current)setImporting(false);}};
  const execute=async()=>{const identity=current.current;if(!identity.source)return;cancel();setRun(null);setError(null);const controller=new AbortController(),ticket=gate.current.issue(identity);request.current=controller;setPending(true);
    try{const result=await requestPandasTimestampOriginal(identity.source,identity.form,controller.signal);if(gate.current.accepts(ticket,current.current,controller.signal))setRun(result);}
    catch(cause){if(gate.current.accepts(ticket,current.current,controller.signal))setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <div className="reference-pandas-timestamp-original-workspace"><section className="panel query-workspace" aria-labelledby="pandas-timestamp-original-heading"><div className="panel-head"><h2 id="pandas-timestamp-original-heading">Original pandas timestamp calls</h2></div>
    <p className="query-intro">Run a time-window filter with complete typed bounds and ordered parameters, then inspect every native artifact.</p>
    <fieldset className="query-fields"><legend>Complete native table</legend>
      <label className="filters-field"><span>Original pandas timestamp source table</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setSource(file));}}/><small>{source?`${source.name} (${source.size} bytes)`:"Choose a genuine native EventTable JSON File with full rows, stored types, index and metadata."}</small></label>
      <button className="btn-ghost" onClick={()=>{const file=tableTimestampSourceExample("pandas");downloadQueryBlob(file,file.name);}}>Download original pandas timestamp example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Original typed call</legend>
      <label className="filters-field"><span>Original pandas timestamp operation</span><select value={form.operation} onChange={event=>{const operation=event.target.value as PandasTimestampOriginalForm["operation"];edit(()=>setForm(previous=>ownedExample?pandasTimestampOriginalExample(operation,example):{...previous,operation}));}}>{PANDAS_TIMESTAMP_ORIGINAL_OPERATIONS.map(operation=><option key={operation} value={operation}>{PANDAS_TIMESTAMP_ORIGINAL_LABELS[operation]} ({operation})</option>)}</select></label>
      <label className="query-text"><span>Complete original pandas timestamp call JSON</span><textarea rows={17} spellCheck={false} value={form.callJson} onChange={event=>{const callJson=event.target.value;edit(()=>{setOwnedExample(false);setForm(previous=>({...previous,callJson}));});}}/><small>Args contain the operands after the DataFrame: two bounds, or attribute, value and two bounds. The helpers take none. The text is sent unchanged, including integer tokens.</small></label>
      <label className="filters-field"><span>Import original pandas timestamp call JSON file</span><input type="file" accept=".json,.txt" onChange={event=>void importCall(referenceLogFilterPickedFile(event.target))}/></label>
      {importing&&<p role="status">Reading original pandas timestamp call…</p>}
      <label className="filters-field"><span>Original pandas timestamp example</span><select value={example} onChange={event=>{const chosen=event.target.value as PandasTimestampOriginalExample;edit(()=>setExample(chosen));}}><option value="operation">Canonical string bounds</option><option value="typed">Typed UTC Timestamp bounds</option><option value="mapping">String parameter mapping</option><option value="enum">Ordered enum and string keys</option><option value="raw">Raw parameter diagnostic</option></select></label>
      <div className="query-actions"><button className="btn-ghost" onClick={()=>edit(()=>{setOwnedExample(true);setForm(pandasTimestampOriginalExample(form.operation,example));})}>Load original pandas timestamp example</button><button className="btn-ghost" onClick={()=>{const file=pandasTimestampOriginalCallFile(form);downloadQueryBlob(file,file.name);}}>Download current pandas timestamp call</button></div>
      <p>String bounds use the strict <code>YYYY-MM-DD HH:mm:ss</code> form; other spellings keep their native diagnostics. A dictionary with an enum key keeps its ordered enum/string identities. A dictionary of string keys, None and raw values are passed as one value. Imported or edited text stays unchanged when you select another operation.</p>
      <div className="query-actions"><button className="btn-open" disabled={pending||importing||source===null} onClick={()=>void execute()}>Run original pandas timestamp call</button>{pending&&<><span role="status">Processing original pandas timestamp call…</span><button className="btn-ghost" onClick={cancel}>Cancel original pandas timestamp processing</button></>}</div>
      {!source&&<p>Choose a native source table to run this call.</p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run&&<PandasTimestampOriginalInspection run={run} onUseTable={file=>edit(()=>setSource(file))}/>}</div>;
}
const artifacts:Array<[PandasTimestampOriginalArtifact,string]>=[["call_json","exact original pandas timestamp call"],["call_projection_json","native timestamp call projection"],["source_table_json","complete original timestamp source table"],["source_after_table_json","original timestamp source after processing"],["controls_before_json","complete original timestamp controls before processing"],["controls_after_json","complete original timestamp controls after processing"]];
export function PandasTimestampOriginalInspection({run,onUseTable}:{run:PandasTimestampOriginalRun;onUseTable?:(file:File)=>void}){
  const v=run.value,success=pandasTimestampOriginalCanReuse(v);
  const download=(key:PandasTimestampOriginalArtifact)=>{const file=pandasTimestampOriginalArtifactFile(run,key);downloadQueryBlob(file,file.name);};
  return <section className="panel" aria-labelledby="pandas-timestamp-original-result-heading"><div className="panel-head"><h2 id="pandas-timestamp-original-result-heading">{success?"Filtered original pandas timestamps":"Original pandas timestamp call could not be completed"}</h2></div><div className="query-results">
    {v.error!==null&&<div role="alert" className="error-banner">{v.error_type!==null?`${referenceLogFilterStringText(v.error_type)}: `:""}{referenceLogFilterStringText(v.error)}</div>}
    <p>{run.sourceFile.name} · operation: {v.operation??"unavailable"}; phase: {v.phase}; apply returned: {v.apply_returned?"yes":"no"}.</p>
    {v.source&&<table><caption>Original source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([["Source",v.source],...(v.result?[["Filtered",v.result]as const]:[])]as const).map(([label,count])=><tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count,"rows")}</td><td>{tableRepetitionCountText(count,"columns")}</td></tr>)}</tbody></table>}
    {success&&v.result?.rows===0&&<p>No rows matched this time window.</p>}
    {([["Complete source table",v.source_unchanged],["Complete caller controls",v.controls_unchanged]]as const).map(([label,unchanged])=>unchanged===null?null:<p key={label} role={unchanged?undefined:"status"}>{label} {unchanged?"was preserved.":"changed. Inspect the complete native snapshots."}</p>)}
    {v.call_projection_json!==null&&<details><summary>Native timestamp call projection</summary><pre aria-label="Native timestamp call projection">{referenceLogFilterStringText(v.call_projection_json)}</pre></details>}
    <div className="query-actions">{artifacts.map(([key,label])=>v[key]!==null&&<button key={key} className="btn-ghost" onClick={()=>download(key)}>Download {label}</button>)}</div>
    <div className="query-actions">{success&&<><button className="btn-ghost" onClick={()=>download("filtered_table_json")}>Download original filtered native timestamp table</button>{onUseTable&&<button className="btn-open" onClick={()=>onUseTable(pandasTimestampOriginalArtifactFile(run,"filtered_table_json"))}>Use filtered timestamp table as source</button>}</>}<button className="btn-ghost" onClick={()=>{const file=pandasTimestampOriginalResponseFile(run);downloadQueryBlob(file,file.name);}}>Download complete original pandas timestamp response</button></div>
    <p>Native JSON preserves stored table values, index and metadata. Original mode has no inferred friendly column selectors.</p>
  </div></section>;
}
