import {useEffect,useRef,useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile,referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText} from "../table-attribute-repetition";
import {tableVariantsSourceExample,tableVariantsMappingExample} from "../table-variants-filters";
import {PANDAS_VARIANTS_ORIGINAL_OPERATIONS,PANDAS_VARIANTS_ORIGINAL_LABELS,defaultPandasVariantsOriginalForm,pandasVariantsOriginalExample,pandasVariantsOriginalCallText,requestPandasVariantsOriginal,pandasVariantsOriginalArtifactFile,pandasVariantsOriginalResponseFile,PandasVariantsOriginalRequestGate,type PandasVariantsOriginalForm,type PandasVariantsOriginalExample,type PandasVariantsOriginalArtifact,type PandasVariantsOriginalRun} from "../pandas-variants-original-call";

export function PandasVariantsOriginalCallWorkspace({initialForm}:{initialForm?:PandasVariantsOriginalForm}){
  const [source,setSource]=useState<File|null>(null),[variants,setVariants]=useState<File|null>(null),[form,setForm]=useState(()=>initialForm??defaultPandasVariantsOriginalForm());
  const [example,setExample]=useState<PandasVariantsOriginalExample>("operation"),[pending,setPending]=useState(false),[importing,setImporting]=useState(false),[error,setError]=useState<string|null>(null),[run,setRun]=useState<PandasVariantsOriginalRun|null>(null);
  const generation=useRef(0),gate=useRef(new PandasVariantsOriginalRequestGate()),request=useRef<AbortController|null>(null),current=useRef({source,variants,form});current.current={source,variants,form};
  const cancel=()=>{gate.current.invalidate();request.current?.abort();request.current=null;setPending(false);};
  const edit=(action:()=>void)=>{++generation.current;setImporting(false);cancel();setRun(null);setError(null);action();};
  useEffect(()=>()=>{++generation.current;gate.current.invalidate();request.current?.abort();},[]);
  const importCall=async(file:File|null)=>{edit(()=>{});if(!file)return;const ticket=generation.current;setImporting(true);
    try{const raw=await pandasVariantsOriginalCallText(file);if(ticket===generation.current)edit(()=>setForm(previous=>({...previous,callJson:raw})));}
    catch(cause){if(ticket===generation.current)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(ticket===generation.current)setImporting(false);}};
  const execute=async()=>{const identity=current.current;if(!identity.source)return;cancel();setRun(null);setError(null);const controller=new AbortController(),ticket=gate.current.issue(identity);request.current=controller;setPending(true);
    try{const result=await requestPandasVariantsOriginal(identity.source,identity.form,identity.variants,controller.signal);if(gate.current.accepts(ticket,current.current,controller.signal))setRun(result);}
    catch(cause){if(gate.current.accepts(ticket,current.current,controller.signal))setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}};
  return <div className="reference-pandas-variants-original-workspace"><section className="panel query-workspace" aria-labelledby="pandas-variants-original-heading"><div className="panel-head"><h2 id="pandas-variants-original-heading">Original pandas activity-variant calls</h2></div>
    <p className="query-intro">Use complete typed arguments, ordered parameter entries and an optional native case-to-variant table.</p>
    <fieldset className="query-fields"><legend>Complete native tables</legend>
      <label className="filters-field"><span>Original pandas variants source table</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setSource(file));}}/><small>{source?`${source.name} (${source.size} bytes)`:"Choose a genuine native EventTable JSON File with full rows, stored types, index and metadata."}</small></label>
      <button className="btn-ghost" onClick={()=>{const file=tableVariantsSourceExample("pandas");downloadQueryBlob(file,file.name);}}>Download original pandas variants example table</button>
      <label className="filters-field"><span>Original supplied variants table</span><input type="file" accept=".json" onChange={event=>{const file=referenceLogFilterPickedFile(event.target);edit(()=>setVariants(file));}}/><small>{variants?`${variants.name} (${variants.size} bytes)`:"Required only when the descriptor directly references variants_file through the literal variants_df parameter."}</small></label>
      {variants&&<button className="btn-ghost" onClick={()=>edit(()=>setVariants(null))}>Remove original supplied variants table</button>}
      <button className="btn-ghost" onClick={()=>{const file=tableVariantsMappingExample();downloadQueryBlob(file,file.name);}}>Download original variants mapping example</button>
      <p>The supplied table retains its real index, categories, attributes and rows. An unused upload or a missing referenced File is reported by the native controls.</p>
    </fieldset>
    <fieldset className="query-fields"><legend>Original typed call</legend>
      <label className="filters-field"><span>Original pandas variants operation</span><select value={form.operation} onChange={event=>{const operation=event.target.value as PandasVariantsOriginalForm["operation"];edit(()=>setForm(pandasVariantsOriginalExample(operation)));}}>{PANDAS_VARIANTS_ORIGINAL_OPERATIONS.map(operation=><option key={operation} value={operation}>{PANDAS_VARIANTS_ORIGINAL_LABELS[operation]}</option>)}</select></label>
      <label className="query-text"><span>Complete original pandas variants call JSON</span><textarea rows={17} spellCheck={false} value={form.callJson} onChange={event=>{const callJson=event.target.value;edit(()=>setForm(previous=>({...previous,callJson})));}}/><small>Args contain exactly one criterion; the source is a separate File. The complete original text is sent unchanged, including integer tokens.</small></label>
      <label className="filters-field"><span>Import original pandas variants call JSON file</span><input type="file" accept=".json,.txt" onChange={event=>void importCall(referenceLogFilterPickedFile(event.target))}/></label>
      {importing&&<p role="status">Reading original pandas variants call…</p>}
      <label className="filters-field"><span>Original pandas variants example</span><select value={example} onChange={event=>{const chosen=event.target.value as PandasVariantsOriginalExample;edit(()=>setExample(chosen));}}><option value="operation">Selected operation</option><option value="mapping">Reference a supplied variants table</option><option value="dictionary">Typed dictionary and wide integer</option><option value="enum">Ordered enum and string keys</option><option value="raw">Raw parameter diagnostic</option></select></label>
      <button className="btn-ghost" onClick={()=>edit(()=>setForm(pandasVariantsOriginalExample(form.operation,example)))}>Load original pandas variants example</button>
      <p>Sequences use values; dictionaries use ordered typed items pairs. Tuple and list criteria remain distinct. Omitted parameters, None, raw values and enum/string key order remain inspectable.</p>
      <p>Use <code>{'{"type":"pandas.DataFrame","file":"variants_file"}'}</code> only as the direct value of the literal variants_df parameter. The actual uploaded File supplies the full table.</p>
      <div className="query-actions"><button className="btn-open" disabled={pending||importing||source===null} onClick={()=>void execute()}>Run original pandas variants call</button>{pending&&<><span role="status">Processing original pandas variants…</span><button className="btn-ghost" onClick={cancel}>Cancel original pandas variants processing</button></>}</div>
      {!source&&<p>Choose a native source table to run this call.</p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run&&<PandasVariantsOriginalInspection run={run} onUseTable={file=>edit(()=>setSource(file))}/>}</div>;
}
const artifacts:Array<[PandasVariantsOriginalArtifact,string]>=[["call_json","exact original pandas variants call"],["source_table_json","complete original variants source table"],["source_after_table_json","original variants source after processing"],["controls_before_json","complete original variants controls before processing"],["controls_after_json","complete original variants controls after processing"],["variants_table_json","actual supplied variants table"]];
export function PandasVariantsOriginalInspection({run,onUseTable}:{run:PandasVariantsOriginalRun;onUseTable?:(file:File)=>void}){
  const v=run.value,success=v.http_status===200&&v.error===null;
  const download=(key:PandasVariantsOriginalArtifact)=>{const file=pandasVariantsOriginalArtifactFile(run,key);downloadQueryBlob(file,file.name);};
  return <section className="panel" aria-labelledby="pandas-variants-original-result-heading"><div className="panel-head"><h2 id="pandas-variants-original-result-heading">{success?"Filtered original pandas variants":"Original pandas variants could not be completed"}</h2></div><div className="query-results">
    {v.error!==null&&<div role="alert" className="error-banner">{v.error_type!==null?`${referenceLogFilterStringText(v.error_type)}: `:""}{referenceLogFilterStringText(v.error)}</div>}
    <p>{run.sourceFile.name} · operation: {v.operation??"unavailable"}; phase: {v.phase}; apply returned: {v.apply_returned?"yes":"no"}.</p>
    {v.source&&<table><caption>Original source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([["Source",v.source],...(v.result?[["Filtered",v.result]as const]:[])]as const).map(([label,count])=><tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count,"rows")}</td><td>{tableRepetitionCountText(count,"columns")}</td></tr>)}</tbody></table>}
    {success&&v.result?.rows===0&&<p>No rows matched this variant condition.</p>}
    {([["Complete source table",v.source_unchanged],["Complete caller controls",v.controls_unchanged],["Actual supplied variants table",v.variants_source_unchanged]]as const).map(([label,unchanged])=>unchanged===null?null:<p key={label} role={unchanged?undefined:"status"}>{label} {unchanged?"was preserved.":"changed. Inspect the complete native snapshots."}</p>)}
    <div className="query-actions">{artifacts.map(([key,label])=>v[key]!==null&&<button key={key} className="btn-ghost" onClick={()=>download(key)}>Download {label}</button>)}</div>
    <div className="query-actions">{success&&v.filtered_table_json!==null&&<><button className="btn-ghost" onClick={()=>download("filtered_table_json")}>Download original filtered native variants table</button>{onUseTable&&<button className="btn-open" onClick={()=>onUseTable(pandasVariantsOriginalArtifactFile(run,"filtered_table_json"))}>Use filtered variants table as source</button>}</>}<button className="btn-ghost" onClick={()=>{const file=pandasVariantsOriginalResponseFile(run);downloadQueryBlob(file,file.name);}}>Download complete original pandas variants response</button></div>
    <p>Native JSON preserves stored table values and metadata. Original mode has no inferred friendly column selectors.</p>
  </div></section>;
}
