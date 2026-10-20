import {MINING_SOURCE_SELECTORS, activeMiningSourceAllTablesCsv, activeMiningSourceBytes, activeMiningSourceTableFileName, miningSourceWorkbenchFields, type ActiveMiningSourceDescriptor} from "../active-mining-source";
import { useEffect,useRef,useState } from "react";
import { defaultTokenReplayWorkbenchForm,TOKEN_REPLAY_CONTROLS,TOKEN_REPLAY_IMPORT_CONTROLS,TOKEN_REPLAY_XES_BOOLEAN_CONTROLS,
  runTokenReplayWorkbench,tokenReplayWorkbenchReport,type TokenReplayWorkbenchRun } from "../token-replay-workbench";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeResponseRequest } from "../native-response";
import { nativeJson } from "../native-json";
import { downloadQueryBlob } from "../log-query";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";

import { ReplayDurationWorkbench } from "./ReplayDurationWorkbench";

export function TokenReplayWorkspace({file,source=null}:{file:File|null;source?:ActiveMiningSourceDescriptor|null}){
  const [log,setLog]=useState<File|null>(file);const [model,setModel]=useState<File|null>(null);
  const [form,setForm]=useState(defaultTokenReplayWorkbenchForm);const [run,setRun]=useState<TokenReplayWorkbenchRun|null>(null);
  const [tableId,setTableId]=useState("/diagnostics/traces");const [pending,setPending]=useState(false);const [error,setError]=useState<string|null>(null);
  const controller=useRef<AbortController|null>(null);
  const cancel = () => {controller.current?.abort(); controller.current = null; setPending(false);};
  const chooseLog = (value: File | null) => {cancel(); setLog(value); setRun(null); setError(null);};
  const chooseModel = (value: File | null) => {cancel(); setModel(value); setRun(null); setError(null);};
  const adoptSource = () => {chooseLog(file); if (source) {const mapped = miningSourceWorkbenchFields(source, {}); setForm(current => ({...current, inputFormat: source.format, adapter: source.shape, miningSelectors: {...source.selectors}, importFields: mapped.importFields}));}};
  useEffect(() => {adoptSource();}, [file, source]);
  useEffect(()=>()=>{controller.current?.abort();controller.current=null;},[]);
  const execute=async()=>{if(!log||!model)return;const current=new AbortController();controller.current=current;setPending(true);setError(null);
    try{const result=await runTokenReplayWorkbench(log,model,form,current.signal);if(controller.current===current&&!current.signal.aborted){setRun(result);setTableId("/diagnostics/traces");}}
    catch(cause){if(controller.current===current&&!current.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(controller.current===current){controller.current=null;setPending(false);}}
  };
  const table=run?.tables.find(item=>item.id===tableId)??run?.tables[0];const input=run?.value.input_context;
  const download=(raw:string,name:string,type="application/json")=>downloadQueryBlob(new Blob([activeMiningSourceBytes(raw)],{type}),name);
  const isXes=/\.(?:xes|xes\.gz|gz)$/i.test(log?.name??"");
  return <>
    <ReplayDurationWorkbench file={file} source={source} replayRun={run}/>
    <section className="panel" aria-label="Independent token replay">
      <div className="panel-head"><h2>Replay a log against a PNML model</h2><span className="panel-meta">Independent inputs</span></div>
      <p>Upload a log and its model, choose the replay policy, then inspect every trace, marking and diagnostic. The model keeps its supplied initial and final tokens.</p>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Replay log file</span><input type="file" accept=".csv,.tsv,.xes,.xes.gz,.gz,.json,text/csv,application/xml,application/gzip,application/json" onChange={event=>chooseLog(event.target.files?.[0]??null)}/><span>{log?.name??"Choose CSV, TSV or XES"}</span></label>
          <label className="filters-field"><span>Replay PNML model</span><input type="file" accept=".pnml,.xml,application/xml" onChange={event=>chooseModel(event.target.files?.[0]??null)}/><span>{model?.name??"Choose PNML"}</span></label>
          <label className="filters-field"><span>Replay direction</span><select value={form.variant} onChange={event=>setForm({...form,variant:event.target.value as typeof form.variant})}><option value="forward">Forward</option><option value="backwards">Backwards</option></select></label>
          <label className="filters-field"><span>Replay input shape</span><select value={form.adapter} onChange={event=>setForm({...form,adapter:event.target.value as typeof form.adapter})}><option value="log">Grouped log</option><option value="stream">Event stream</option><option value="dataframe">Dataframe</option><option value="lazyframe">Lazyframe</option></select></label>
        </div>
        {file && <button className="btn-ghost" onClick={adoptSource}>Use current original source settings</button>}
        <label className="filters-field"><span>Replay original source format</span><select value={form.inputFormat ?? "auto"} onChange={event => setForm(current => ({...current, inputFormat: event.target.value as "auto" | "table"}))}><option value="auto">CSV / TSV / XES / gzip XES</option><option value="table">Full native EventTable JSON</option></select></label>
        <details><summary>Literal source selectors</summary><p>Source selectors configure the uploaded source. Specific replay options below can override their activity and case defaults.</p><div className="query-options">{MINING_SOURCE_SELECTORS.map(([key, label, canonical]) => <div className="filters-field" key={key}><label><span>{label}</span><input disabled={form.miningSelectors?.[key] === undefined} value={form.miningSelectors?.[key] ?? ""} placeholder={canonical} onChange={event => setForm(current => ({...current, miningSelectors: {...current.miningSelectors, [key]: event.target.value}}))} /></label><label className="query-check"><input type="checkbox" checked={form.miningSelectors?.[key] !== undefined} onChange={event => setForm(current => {const selectors = {...current.miningSelectors}; if (event.target.checked) selectors[key] = ""; else delete selectors[key]; return {...current, miningSelectors: selectors};})} />Submit exact selector, including empty</label></div>)}</div></details>
        {form.inputFormat !== "table" && <details><summary>Replay import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key])=>isXes?key.startsWith("xes_"):!key.startsWith("xes_")).map(([key,label,placeholder])=><label className="filters-field" key={key}><span>{label}</span><input value={form.importFields[key]??""} placeholder={placeholder} onChange={event=>setForm({...form,importFields:{...form.importFields,[key]:event.target.value}})}/></label>)}
          {isXes?<><label className="filters-field"><span>XES importer</span><select value={form.importFields.xes_variant??""} onChange={event=>setForm({...form,importFields:{...form.importFields,xes_variant:event.target.value}})}><option value="">Native default</option>{["iterparse","line_by_line","iterparse_20","iterparse_mem_compressed","rustxes","chunk_regex"].map(value=><option key={value} value={value}>{value}</option>)}</select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key,label])=><label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key]??""} onChange={event=>setForm({...form,importFields:{...form.importFields,[key]:event.target.value}})}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}</>
            :<label className="filters-field"><span>CSV delimiter</span><select value={form.importFields.delimiter??""} onChange={event=>setForm({...form,importFields:{...form.importFields,delimiter:event.target.value}})}><option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>}
        </div></details>}
        <div className="query-options">
          {TOKEN_REPLAY_CONTROLS.filter(([,,,scope])=>scope==="both"||form.variant==="forward").map(([key,label,kind])=><label className="filters-field" key={key}><span>{label}</span>
            {kind==="boolean"?<select value={form.replayFields[key]??""} onChange={event=>setForm({...form,replayFields:{...form.replayFields,[key]:event.target.value}})}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select>
              :kind==="json"?<textarea rows={3} value={form.replayFields[key]??""} placeholder='Automatic, or {"p0":{"p1":["tau"]}}' onChange={event=>setForm({...form,replayFields:{...form.replayFields,[key]:event.target.value}})}/>
                :<input value={form.replayFields[key]??""} placeholder="Native default" onChange={event=>setForm({...form,replayFields:{...form.replayFields,[key]:event.target.value}})}/>}
          </label>)}
        </div>
        <p>Flat CSV inputs retain original row order and columns. Case selection can regroup those events. Grouped logs keep their trace boundaries. Blank settings use native defaults; an event cap rejects oversized inputs.</p>
        {form.variant==="backwards"&&<p>Backwards replay returns native trace diagnostics. Place/transition fitness maps, named public results and variant grouping use forward replay.</p>}
        <button className="btn-open" disabled={!log||!model} onClick={()=>void execute()}>Run independent token replay</button>
      </fieldset>
      {pending&&<p role="status">Replaying… <button className="btn-ghost" onClick={()=>{controller.current?.abort();controller.current=null;setPending(false);}}>Cancel replay request</button></p>}
      {error&&<p className="error-banner" role="alert">{error}</p>}
    </section>
    {run&&input&&table&&<section className="panel" aria-label="Complete independent replay inspection">
      <div className="panel-head"><h2>Complete token replay result</h2><span className="panel-meta">{run.value.variant} · {input.adapter}</span></div>
      <p>{input.dataset} against {input.model}: {input.evaluated_case_count} evaluated cases · {input.evaluated_event_count} events. Original import: {input.source_case_count} cases · {input.source_event_count} events. Results retain the inputs and settings of this completed run.</p>
      <details><summary>Submitted replay request and original file hashes</summary><pre>{nativeJson({request:nativeResponseRequest(run.value),log_sha256:run.inputs.log_sha256,model_sha256:run.inputs.model_sha256},2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={()=>download(run.raw,"minera-token-replay-native.json")}>Download complete token replay native JSON</button>
        <button className="btn-ghost" onClick={()=>download(tokenReplayWorkbenchReport(run),"minera-token-replay-report.json")}>Download token replay report</button>
        <button className="btn-ghost" onClick={()=>download(diagnosticCsv(table),`minera-token-replay-${activeMiningSourceTableFileName(table.id)}.csv`,"text/csv;charset=utf-8")}>Download complete token replay table CSV</button>
        <button className="btn-ghost" onClick={() => download(activeMiningSourceAllTablesCsv(run.tables), "minera-token-replay-all-tables.csv", "text/csv;charset=utf-8")}>Download all complete native tables CSV</button>
        <button className="btn-ghost" onClick={()=>downloadQueryBlob(run.inputs.log,run.inputs.log.name)}>Download original replay log</button>
        <button className="btn-ghost" onClick={()=>downloadQueryBlob(run.inputs.model,run.inputs.model.name)}>Download original replay PNML</button>
        {run.value.full_result&&<button className="btn-ghost" onClick={()=>download(nativeJson(run.value.full_result,2),"minera-token-replay-public.json")}>Download typed public replay result</button>}
      </div>
      <label className="filters-field"><span>Token replay result table</span><select value={table.id} onChange={event=>setTableId(event.target.value)}>{run.tables.map(item=><option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={run.raw+":"+table.id} table={table} noun="replay"/>
      <details><summary>Complete token replay JSON</summary><pre>{run.raw}</pre></details>
    </section>}
  </>;
}
