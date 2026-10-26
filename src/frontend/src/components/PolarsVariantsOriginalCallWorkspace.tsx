import {useEffect,useRef,useState} from "react";
import {nativeCell} from "../native-json";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile,referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText} from "../table-attribute-repetition";
import {tableVariantsSourceExample} from "../table-variants-filters";
import {POLARS_VARIANTS_ORIGINAL_OPERATIONS,POLARS_VARIANTS_ORIGINAL_LABELS,defaultPolarsVariantsOriginalForm,polarsVariantsOriginalExample,polarsVariantsOriginalCallText,requestPolarsVariantsOriginal,polarsVariantsOriginalArtifactFile,polarsVariantsOriginalHeaderContextFile,polarsVariantsOriginalResponseFile,polarsVariantsReturnedMetadata,PolarsVariantsOriginalRequestGate,type PolarsVariantsOriginalForm,type PolarsVariantsOriginalExample,type PolarsVariantsOriginalArtifact,type PolarsVariantsOriginalRun} from "../polars-variants-original-call";

export function PolarsVariantsOriginalCallWorkspace({initialForm}:{initialForm?:PolarsVariantsOriginalForm}){
  const [source,setSource]=useState<File|null>(null),[form,setForm]=useState(()=>initialForm??defaultPolarsVariantsOriginalForm());
  const [example,setExample]=useState<PolarsVariantsOriginalExample>("operation"),[pending,setPending]=useState(false),[importing,setImporting]=useState(false),[error,setError]=useState<string|null>(null),[run,setRun]=useState<PolarsVariantsOriginalRun|null>(null);
  const generation=useRef(0),gate=useRef(new PolarsVariantsOriginalRequestGate()),request=useRef<AbortController|null>(null),current=useRef({source,form});current.current={source,form};
  const cancel=()=>{gate.current.invalidate();request.current?.abort();request.current=null;setPending(false);};
  const edit=(action:()=>void)=>{++generation.current;setImporting(false);cancel();setRun(null);setError(null);action();};
  useEffect(()=>()=>{++generation.current;gate.current.invalidate();request.current?.abort();},[]);
  const importCall=async(file:File|null)=>{edit(()=>{});if(!file)return;const ticket=generation.current;setImporting(true);
    try{const raw=await polarsVariantsOriginalCallText(file);if(ticket===generation.current)edit(()=>setForm(previous=>({...previous,callJson:raw})));}
    catch(cause){if(ticket===generation.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(ticket===generation.current)setImporting(false);}};
  const execute=async()=>{const identity=current.current;if(!identity.source)return;cancel();setRun(null);setError(null);const controller=new AbortController(),ticket=gate.current.issue(identity);request.current=controller;setPending(true);
    try{const result=await requestPolarsVariantsOriginal(identity.source,identity.form,controller.signal);if(gate.current.accepts(ticket,current.current,controller.signal))setRun(result);}
    catch(cause){if(gate.current.accepts(ticket,current.current,controller.signal))setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <div className="reference-polars-variants-original-workspace"><section className="panel query-workspace" aria-labelledby="polars-variants-original-heading"><div className="panel-head"><h2 id="polars-variants-original-heading">Polars activity-variant calls</h2></div>
    <p className="query-intro">Run complete typed keyword calls with an eager or lazy source selected by the original descriptor.</p>
    <fieldset className="query-fields"><legend>Complete native source table</legend>
      <label className="filters-field"><span>Original Polars variants source table</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setSource(file));}}/><small>{source?`${source.name} (${source.size} bytes)`:"Choose a genuine native EventTable JSON File with full rows, stored types, categories and metadata."}</small></label>
      <button className="btn-ghost" onClick={()=>{const file=tableVariantsSourceExample("polars");downloadQueryBlob(file,file.name);}}>Download original Polars variants example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Original typed call</legend>
      <label className="filters-field"><span>Original Polars variants operation</span><select value={form.operation} onChange={event=>{const operation=event.target.value as PolarsVariantsOriginalForm["operation"];edit(()=>setForm(polarsVariantsOriginalExample(operation)));}}>{POLARS_VARIANTS_ORIGINAL_OPERATIONS.map(operation=><option key={operation} value={operation}>{POLARS_VARIANTS_ORIGINAL_LABELS[operation]}</option>)}</select></label>
      <label className="query-text"><span>Complete original Polars variants call JSON</span><textarea rows={17} spellCheck={false} value={form.callJson} onChange={event=>{const callJson=event.target.value;edit(()=>setForm(previous=>({...previous,callJson})));}}/><small>The descriptor input_kind selects DataFrame or LazyFrame. Keyword entries contain all supplied controls; the source is a separate File. Imported text is sent unchanged.</small></label>
      <label className="filters-field"><span>Import original Polars variants call JSON file</span><input type="file" accept=".json,.txt" onChange={event=>void importCall(referenceLogFilterPickedFile(event.target))}/></label>
      {importing&&<p role="status">Reading original Polars variants call…</p>}
      <label className="filters-field"><span>Original Polars variants example</span><select value={example} onChange={event=>{const chosen=event.target.value as PolarsVariantsOriginalExample;edit(()=>setExample(chosen));}}><option value="operation">Selected operation with LazyFrame</option><option value="eager">Selected operation with DataFrame</option><option value="missing">Missing required control with unused parameters</option><option value="none">Explicit None criterion</option><option value="enum">Ordered enum and string keys</option><option value="raw">Raw parameter diagnostic</option></select></label>
      <button className="btn-ghost" onClick={()=>edit(()=>setForm(polarsVariantsOriginalExample(form.operation,example)))}>Load original Polars variants example</button>
      <p>Loading an example intentionally replaces the editor. Source kind remains inside the descriptor; importing a call never rewrites it. Missing required operands, supplied None and omitted parameters remain distinct.</p>
      <p>Serialized-list and automatic helpers remain callable and retain their native AttributeError diagnostics. Ordered parameters keep literal and enum key identities. No friendly column selectors override this call.</p>
      <div className="query-actions"><button className="btn-open" disabled={pending||importing||source===null} onClick={()=>void execute()}>Run original Polars variants call</button>{pending&&<><span role="status">Processing original Polars variants…</span><button className="btn-ghost" onClick={cancel}>Cancel original Polars variants processing</button></>}</div>
      {!source&&<p>Choose a native source table to run this call.</p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run&&<PolarsVariantsOriginalInspection run={run} onUseTable={file=>edit(()=>setSource(file))}/>}</div>;
}
const artifacts:Array<[PolarsVariantsOriginalArtifact,string]>=[["call_json","exact original Polars variants call"],["source_table_json","complete original Polars variants source table"],["source_after_table_json","original Polars variants source after processing"],["controls_before_json","complete original Polars variants controls before processing"],["controls_after_json","complete original Polars variants controls after processing"],["returned_table_metadata_json","returned Polars variants metadata"]];
export function PolarsVariantsOriginalInspection({run,onUseTable}:{run:PolarsVariantsOriginalRun;onUseTable?:(file:File)=>void}){
  const v=run.value,success=v.http_status===200&&v.error===null,metadata=polarsVariantsReturnedMetadata(v.returned_table_metadata_json);
  const download=(key:PolarsVariantsOriginalArtifact)=>{const file=polarsVariantsOriginalArtifactFile(run,key);downloadQueryBlob(file,file.name);};
  return <section className="panel" aria-labelledby="polars-variants-original-result-heading"><div className="panel-head"><h2 id="polars-variants-original-result-heading">{success?"Filtered original Polars variants":"Original Polars variants could not be completed"}</h2></div><div className="query-results">
    {v.error!==null&&<div role="alert" className="error-banner">{v.error_type!==null?`${referenceLogFilterStringText(v.error_type)}: `:""}{referenceLogFilterStringText(v.error)}</div>}
    <p>{run.sourceFile.name} · operation: {v.operation??"unavailable"}; phase: {v.phase}; apply returned: {v.apply_returned?"yes":"no"}.</p>
    {v.source&&<table><caption>Original source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([["Source",v.source],...(v.result?[["Filtered",v.result]as const]:[])]as const).map(([label,count])=><tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count,"rows")}</td><td>{tableRepetitionCountText(count,"columns")}</td></tr>)}</tbody></table>}
    {success&&v.result?.rows===0&&<p>No rows matched this variant condition.</p>}
    {([["Complete source table",v.source_unchanged],["Complete caller controls",v.controls_unchanged]]as const).map(([label,unchanged])=>unchanged===null?null:<p key={label} role={unchanged?undefined:"status"}>{label} {unchanged?"was preserved.":"changed. Inspect the complete native snapshots."}</p>)}
    {metadata&&<><p>Returned native object: {metadata.kind}; row count: {metadata.row_count_known?String(nativeCell(metadata,"row_count")):"unknown"}.</p><p>The returned header context contains empty rows to describe actual native context. It is a diagnostic artifact and cannot be reused as source data.</p></>}
    <div className="query-actions">{artifacts.map(([key,label])=>v[key]!==null&&<button key={key} className="btn-ghost" onClick={()=>download(key)}>Download {label}</button>)}{metadata&&<button className="btn-ghost" onClick={()=>{const file=polarsVariantsOriginalHeaderContextFile(run);downloadQueryBlob(file,file.name);}}>Download native returned header context</button>}</div>
    <div className="query-actions">{success&&v.filtered_table_json!==null&&<><button className="btn-ghost" onClick={()=>download("filtered_table_json")}>Download original filtered native Polars variants table</button>{onUseTable&&<button className="btn-open" onClick={()=>onUseTable(polarsVariantsOriginalArtifactFile(run,"filtered_table_json"))}>Use filtered Polars variants table as source</button>}</>}<button className="btn-ghost" onClick={()=>{const file=polarsVariantsOriginalResponseFile(run);downloadQueryBlob(file,file.name);}}>Download complete original Polars variants response</button></div>
    <p>Native stages and returned-object headers are distinct from Python explain plans, object identity and warning observations.</p>
  </div></section>;
}
