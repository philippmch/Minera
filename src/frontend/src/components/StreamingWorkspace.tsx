import { useEffect, useRef, useState } from "react";
import { downloadQueryBlob } from "../log-query";
import { ocelPreparedCsv } from "../ocel-preparation";
import { nativeJson } from "../native-json";
import { cellText } from "../transformation-workspace";
import {
  STREAMING_METHODS, defaultStreaming, requestStreamingWorkflow, streamingGraph,
  type StreamingForm, type StreamingRun,
} from "../streaming-workspace";
import { ModelGraph } from "./ModelGraph";
import { type MineraExport } from "../types";
import { DeclareStreamModelFields } from "./DeclareStreamModelFields";
import { DeclareStreamChart } from "./DeclareStreamChart";
import { LiveStreamingInspection } from "./LiveStreamingInspection";
import { ReaderStreamWorkspace } from "./ReaderStreamWorkspace";
import { type CsvOptions } from "../api";
import { LIVE_XES_VARIANTS, defaultLiveImport, createLiveSession, getLiveSession, appendLiveSession, actLiveSession, listLiveSessions, formFromLive, importsFromLive, type LiveRun, type LiveSnapshot, type LiveAction, type LiveImportForm } from "../live-stream-workspace";

export function StreamingWorkspace({ file = null, busy = false, data, onOpen }: { file?: File | null; busy?: boolean; data?:MineraExport;onOpen?:(file:File,activityAttribute:string,importOptions?:CsvOptions)=>void }) {
  const [form, setForm] = useState(defaultStreaming);
  const [mode,setMode]=useState<"fresh"|"live"|"reader">("fresh");
  const [imports,setImports]=useState(defaultLiveImport);
  const [liveRun,setLiveRun]=useState<LiveRun|null>(null);
  const [sessions,setSessions]=useState<LiveSnapshot[]>([]);
  const [connectionId,setConnectionId]=useState(()=>typeof localStorage==="undefined"?"":localStorage.getItem("minera.liveStreamSession")??"");
  const [batchFormat,setBatchFormat]=useState<"csv"|"xes">("csv");
  const [waitForBatch,setWaitForBatch]=useState(true);
  const [log, setLog] = useState<File | null>(null);
  const [model, setModel] = useState<File | null>(null);
  const [run, setRun] = useState<StreamingRun | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [pending, setPending] = useState(false);
  const [selection, setSelection] = useState("");
  const [page, setPage] = useState(0);
  const [selectedNode, setSelectedNode] = useState<string>();
  const request = useRef<AbortController | null>(null);
  const cancel = () => { request.current?.abort(); request.current = null; setPending(false); };
  useEffect(() => () => request.current?.abort(), []);
  const change = <K extends keyof StreamingForm>(key: K, value: StreamingForm[K]) => {
    setForm(previous => ({ ...previous, [key]: value })); setRun(null); setError(null);
  };
  const upload = (role: "log" | "model", value: File | null) => {
    cancel(); setRun(null); setError(null);
    if (role === "log") {setLog(value);setBatchFormat(/\.xes(?:\.gz)?$/i.test(value?.name??"")?"xes":"csv");} else setModel(value);
  };
  const remember=(id:string)=>{setConnectionId(id);if(typeof localStorage!=="undefined")localStorage.setItem("minera.liveStreamSession",id);};
  const liveOperation=async(operation:(signal:AbortSignal)=>Promise<LiveRun>)=>{
    cancel();const controller=new AbortController();request.current=controller;setPending(true);setError(null);
    try{const next=await operation(controller.signal);if(request.current===controller&&!controller.signal.aborted){setLiveRun(next);remember(next.result.session_id);setForm(formFromLive(next.result));setImports(importsFromLive(next.result));}}
    catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}
  };
  const create=()=>{const id=crypto.randomUUID();remember(id);void liveOperation(signal=>createLiveSession(form,imports,id,model,signal));};
  const connect=(id:string)=>{if(id)void liveOperation(signal=>getLiveSession(id,signal));};
  const list=async()=>{cancel();const controller=new AbortController();request.current=controller;setPending(true);setError(null);try{const next=await listLiveSessions(controller.signal);if(request.current===controller&&!controller.signal.aborted)setSessions(next);}catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}finally{if(request.current===controller){request.current=null;setPending(false);}}};
  const append=()=>{if(liveRun&&log){const batchId=crypto.randomUUID();void liveOperation(signal=>appendLiveSession(liveRun,log,batchFormat,batchId,waitForBatch,signal));}};
  const liveAction=(action:LiveAction,caseId:string)=>{if(liveRun)void liveOperation(signal=>actLiveSession(liveRun,action,caseId,signal));};
  const importField=<K extends keyof LiveImportForm>(key:K,value:LiveImportForm[K])=>setImports(old=>({...old,[key]:value}));
  const execute = async () => {
    if (!log) return;
    cancel(); const controller = new AbortController(); request.current = controller;
    setPending(true); setError(null);
    try {
      const next = await requestStreamingWorkflow(form, log, model, controller.signal);
      if (request.current === controller && !controller.signal.aborted) {
        setRun(next); setSelection(Object.keys(next.tables)[0]); setPage(0); setSelectedNode(undefined);
      }
    } catch (cause) {
      if (request.current === controller && !controller.signal.aborted)
        setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (request.current === controller) { request.current = null; setPending(false); }
    }
  };
  const selected = run?.tables[selection];
  const graph = run && streamingGraph(run.result);
  const declareModel=run?.result.family==="STREAMING_DECLARE"?run.result.model:null;
  const scalar = (key: "caseColumn" | "activityColumn" | "timestampColumn" | "startColumn" | "zeta" | "maxInvisible", label: string) =>
    <label className="filters-field" key={key}><span>{label} (empty uses native default)</span>
      <input value={form[key]} onChange={event => change(key, event.target.value)} /></label>;
  const jsonField = (key: "activities" | "starts" | "ends" | "sequence" | "parallel" | "profile", label: string, help: string) =>
    <label className="filters-field" key={key}><span>{label}</span>
      <textarea rows={3} value={form[key]} onChange={event => change(key, event.target.value)} /><small>{help}</small></label>;
  if(mode==="reader")return <><section className="panel"><label className="filters-field"><span>Streaming execution</span><select disabled={busy} value={mode} onChange={event=>setMode(event.target.value as typeof mode)}><option value="fresh">Fresh file replay</option><option value="live">Persistent server session</option><option value="reader">Native event and trace readers</option></select></label></section><ReaderStreamWorkspace file={file} busy={busy} onOpen={onOpen}/></>;
  return <>
    <section className="panel streaming-workspace">
      <div className="panel-head"><h2>Replay event streams</h2><span className="panel-meta">{mode==="live"?"Server monitor sessions":"Independent event uploads"}</span></div>
      <p>{mode==="fresh"?"Replay an uploaded event sequence through a fresh native streaming monitor. Inspect open-case prefixes, or finalize footprint and token-replay cases at the end of the file.":"Keep a native monitor in the server, queue uploads, then start and append more batches. Inspect state, detach or reattach the observer, reset cases and finalize conformance."}</p>
      <label className="filters-field streaming-mode"><span>Streaming execution</span><select disabled={busy||pending} value={mode} onChange={event=>{cancel();setMode(event.target.value as typeof mode);setError(null);if(event.target.value==="live"&&liveRun){setForm(formFromLive(liveRun.result));setImports(importsFromLive(liveRun.result));}}}><option value="fresh">Fresh file replay</option><option value="live">Persistent server session</option><option value="reader">Native event and trace readers</option></select></label>
      {mode==="live"&&<div className="query-options live-connections"><label className="filters-field"><span>Server stream session ID</span><input disabled={busy||pending} value={connectionId} onChange={event=>setConnectionId(event.target.value)}/></label><button className="btn-ghost" disabled={busy||pending||!connectionId} onClick={()=>connect(connectionId)}>Connect or refresh session</button><button className="btn-ghost" disabled={busy||pending} onClick={()=>void list()}>List server stream sessions</button>{sessions.map(s=><button className="btn-ghost" disabled={busy||pending} key={s.session_id} onClick={()=>connect(s.session_id)}>Connect {s.method} · {s.session_id} · {s.state}</button>)}</div>}
      <fieldset className="query-fields" disabled={busy || pending || mode==="live"&&!!liveRun&&!liveRun.result.released}>
        <div className="query-options">
          <label className="filters-field"><span>Streaming method</span><select value={form.method} onChange={event => change("method", event.target.value as StreamingForm["method"])}>
            {STREAMING_METHODS.map(([id, label]) => <option key={id} value={id}>{label}</option>)}
          </select></label>
          <label className="filters-field"><span>Event replay order</span><select value={form.order} onChange={event => change("order", event.target.value as StreamingForm["order"])}>
            <option value="input">CSV input order</option><option value="imported">Case-grouped importer order</option>
          </select></label>
          {mode==="fresh"&&<label className="filters-field"><span>{form.method==="declare"?"Declare event upload":"Streaming event CSV"}</span><input type="file" accept={form.method==="declare"?".csv,.xes,.xes.gz,text/csv,application/xml,application/gzip":".csv,text/csv"} aria-label={form.method==="declare"?"Declare event upload":"Streaming event CSV"} onChange={event => upload("log", event.target.files?.[0] ?? null)} />
            <small>{log?.name ?? "Columns: case_id, activity; timestamp optional except temporal checks."}</small></label>
          }
        </div>
        {mode==="fresh"&&file && /\.csv$/i.test(file.name) && <button className="btn-ghost" onClick={() => upload("log", file)}>Use current original CSV for streaming</button>}
        <p>{mode==="fresh"?"CSV input order preserves parsed event arrival order across cases. Importer order groups cases and sorts their events by timestamps when available. Each run starts empty; it does not append to the preceding result. Original uploads are independent of current workspace filters.":"Arrival order applies within each batch, and batches continue the same native monitor. Importer order groups/sorts each upload separately. Configuration is fixed when a session is created; start a separate session to change it."}</p>
        <div className="query-options">{scalar("caseColumn", "Case column")}{scalar("activityColumn", "Activity column")}</div>
        {form.method==="declare"&&<><DeclareStreamModelFields file={model} data={data} onFile={value=>upload("model",value)}/><div className="query-options">{scalar("timestampColumn","Declare timestamp column")}</div></>}
        {form.method === "footprints" && <>
          <div className="query-options">
            {jsonField("activities", "Footprint activities JSON", 'Nonempty list, for example ["A","B","C"].')}
            {jsonField("starts", "Footprint start activities JSON", "Empty text omits the start constraint; otherwise supply a nonempty list.")}
            {jsonField("ends", "Footprint end activities JSON", "Empty text omits the end constraint; checked when cases are finalized.")}
            {jsonField("sequence", "Footprint sequence pairs JSON", 'Pairs such as [["A","B"]]. Empty text or [] omits pairs.')}
            {jsonField("parallel", "Footprint parallel pairs JSON", "Pairs of activities permitted in parallel. Empty text or [] omits pairs.")}
          </div>
          <p>The native query syntax excludes commas and arrows inside footprint activity names.</p>
        </>}
        {form.method === "temporal" && <>
          {jsonField("profile", "Temporal profile JSON", 'Tuples [source, target, mean seconds, standard deviation seconds], for example [["A","B",5,1]].')}
          <div className="query-options">{scalar("timestampColumn", "Completion timestamp column")}{scalar("startColumn", "Start timestamp column")}{scalar("zeta", "Zeta")}</div>
          <p>The native monitor checks eligible earlier/later pairs on arrival. Invalid or missing timestamps can be skipped; compare parsed and processed counts. Its profile query syntax excludes commas, arrows and colons inside activity names.</p>
        </>}
        {form.method === "tbr" && <>
          <div className="query-options">
            <label className="filters-field"><span>Streaming PNML model</span><input type="file" accept=".pnml,.xml,application/xml" aria-label="Streaming PNML model" onChange={event => upload("model", event.target.files?.[0] ?? null)} /><small>{mode==="live"&&liveRun?.result.method==="tbr" ? liveRun.result.sources.find(source=>source.source_id==="model")?.name??"Native session model" : model?.name ?? "Choose a PNML workflow net with single source/sink markings."}</small></label>
            <label className="filters-field"><span>Token replay variant</span><select value={form.variant} onChange={event => change("variant", event.target.value as StreamingForm["variant"])}><option value="forward">Forward</option><option value="backwards">Backwards</option></select></label>
            {scalar("maxInvisible", "Invisible search limit")}
          </div>
          <label className="query-check"><input type="checkbox" checked={form.stopImmediately} onChange={event => change("stopImmediately", event.target.checked)} />Stop accepting events for an unfit case</label>
          <p>Silent-transition search is bounded. Marking vectors use the native parsed place order; the original PNML is recorded with the result.</p>
        </>}
        {mode==="fresh"&&(form.method === "footprints" || form.method === "tbr") &&
          <label className="query-check"><input type="checkbox" checked={form.finalize} onChange={event => change("finalize", event.target.checked)} />Finalize all cases after replay</label>}
        {mode==="fresh"?<button className="btn-open" disabled={!log || ((form.method === "tbr"||form.method==="declare") && !model)} onClick={() => void execute()}>Run streaming workflow</button>:<>
          <details><summary>Live CSV and XES import settings</summary><div className="query-options">
            <label className="filters-field"><span>Live CSV delimiter</span><select value={imports.delimiter} onChange={event=>importField("delimiter",event.target.value)}><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>
            <label className="query-check"><input type="checkbox" checked={imports.trim} onChange={event=>importField("trim",event.target.checked)}/>Trim CSV fields</label><label className="query-check"><input type="checkbox" checked={imports.header} onChange={event=>importField("header",event.target.checked)}/>CSV has header</label><label className="query-check"><input type="checkbox" checked={imports.caseAliases} onChange={event=>importField("caseAliases",event.target.checked)}/>Allow CSV case-ID aliases</label>
            {([['resource','Live CSV resource column'],['lifecycle','Live CSV lifecycle column']] as const).map(([key,label])=><label className="filters-field" key={key}><span>{label} (empty uses native default)</span><input value={imports[key]} onChange={event=>importField(key,event.target.value)}/></label>)}
            <label className="filters-field"><span>Live XES parser</span><select value={imports.xesVariant} onChange={event=>importField("xesVariant",event.target.value as LiveImportForm["xesVariant"])}>{LIVE_XES_VARIANTS.map(variant=><option value={variant} key={variant}>{variant}</option>)}</select></label>
            {([['xesCase','Live XES case attribute'],['xesActivity','Live XES activity attribute'],['xesTimestamp','Live XES timestamp attribute'],['xesResource','Live XES resource attribute'],['xesLifecycle','Live XES lifecycle attribute']] as const).map(([key,label])=><label className="filters-field" key={key}><span>{label}</span><input value={imports[key]} onChange={event=>importField(key,event.target.value)}/></label>)}
          </div></details><button className="btn-open" disabled={(form.method==="tbr"||form.method==="declare")&&!model} onClick={create}>Create server stream session</button>
        </>}
      </fieldset>
      {mode==="live"&&liveRun&&<fieldset className="query-fields" disabled={busy||pending||!!liveRun.result.released||liveRun.result.state==="FINISHED"}><div className="query-options"><label className="filters-field"><span>Live event batch</span><input aria-label="Live event batch" type="file" accept=".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" onChange={event=>upload("log",event.target.files?.[0]??null)}/><small>{log?.name??"Upload a complete CSV or XES batch, with its header or XML log wrapper."}</small></label><label className="filters-field"><span>Live batch format</span><select value={batchFormat} onChange={event=>setBatchFormat(event.target.value as typeof batchFormat)}><option value="csv">CSV</option><option value="xes">XES (including gzip)</option></select></label><label className="query-check"><input type="checkbox" checked={waitForBatch} onChange={event=>setWaitForBatch(event.target.checked)}/>Wait for the batch to drain when active</label></div>{file&&<button className="btn-ghost" onClick={()=>upload("log",file)}>Use current original file as live batch</button>}<button className="btn-open" disabled={!log} onClick={append}>{liveRun.result.state==="INACTIVE"?"Queue event batch":"Append event batch"}</button><p>Each click appends a new batch, including repeated files. A stale revision or repeated batch receipt is rejected. After an interrupted request, refresh the session to see whether it committed before sending it again.</p></fieldset>}
      {pending && <p role="status">{mode==="fresh"?"Replaying…":"Updating live session…"} <button className="btn-ghost" onClick={cancel}>{mode==="fresh"?"Cancel streaming workflow":"Cancel live request"}</button></p>}
      {error && <div className="error-banner" role="alert">{error}</div>}
    </section>
    {mode==="live"&&liveRun&&<LiveStreamingInspection run={liveRun} pending={pending||busy} onAction={liveAction} onDisconnect={()=>{cancel();setLiveRun(null);setError(null);}} onError={setError}/>}
    {mode==="fresh"&&run && selected && <section className="panel streaming-inspection">
      <div className="panel-head"><h2>Native streaming data</h2><span className="panel-meta">{run.method} · {selection} · {selected.count} rows</span></div>
      <p>Parsed events: {run.result.input.parsed_events} · Processed events: {run.result.family === "STREAMING_DFG" ? run.result.events : run.result.total_events_processed} · Order: {run.result.input.event_order}</p>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => downloadQueryBlob(new Blob([run.raw], { type: "application/json" }), "minera-streaming-data.json")}>Download native streaming data</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(new Blob([run.report_json], { type: "application/json" }), "minera-streaming-report.json")}>Download streaming report</button>
        {declareModel&&<button className="btn-ghost" onClick={()=>downloadQueryBlob(new Blob([nativeJson(declareModel)],{type:"application/json"}),"minera-declare-model.json")}>Download native Declare model</button>}
        <button className="btn-ghost" onClick={() => downloadQueryBlob(ocelPreparedCsv(selected), "minera-streaming-" + selection + ".csv")}>Download complete streaming CSV</button>
      </div>
      {run.result.prefix && <p>Prefix diagnostics precede final checks. Finalized cases are removed from open_cases; their separate termination results remain available.</p>}
      <div className="query-actions">{Object.entries(run.tables).map(([name, table]) =>
        <button key={name} className={name === selection ? "btn-open" : "btn-ghost"} onClick={() => { setSelection(name); setPage(0); }}>{name} · {table.count} rows</button>)}</div>
      <div className="query-table-wrap"><table className="activity-table"><thead><tr>{selected.columns.map((column, i) => <th key={i}>{column}</th>)}</tr></thead>
        <tbody>{Array.from({ length: Math.max(0, Math.min(25, selected.count - page * 25)) }, (_, i) => <tr key={i}>{selected.rowAt(page * 25 + i).map((value, k) => <td key={k}>{cellText(value)}</td>)}</tr>)}</tbody></table></div>
      {!selected.count && <p>No native rows in this category. The complete CSV retains its column schema.</p>}
      {selected.count > 25 && <div className="query-actions"><button className="btn-ghost" disabled={!page} onClick={() => setPage(value => value - 1)}>Previous streaming rows</button><span>Rows {page * 25 + 1}–{Math.min(selected.count, page * 25 + 25)} / {selected.count}</span><button className="btn-ghost" disabled={(page + 1) * 25 >= selected.count} onClick={() => setPage(value => value + 1)}>Next streaming rows</button></div>}
      <details><summary>Streaming request and source bindings</summary><pre>{JSON.stringify({ query: run.query, sources: run.sources }, null, 2)}</pre></details>
      <details><summary>Complete native streaming response</summary><pre>{nativeJson(run.result, 2)}</pre></details>
    </section>}
    {mode==="fresh"&&graph && <ModelGraph graph={graph} title="Streaming directly-follows graph" selected={selectedNode} onSelect={setSelectedNode} />}
    {mode==="fresh"&&run?.result.family==="STREAMING_DECLARE"&&<DeclareStreamChart key={run.sources.map(s=>s.sha256).join()} result={run.result}/>}
  </>;
}
