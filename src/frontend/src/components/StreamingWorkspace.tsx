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

export function StreamingWorkspace({ file = null, busy = false }: { file?: File | null; busy?: boolean }) {
  const [form, setForm] = useState(defaultStreaming);
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
    if (role === "log") setLog(value); else setModel(value);
  };
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
  const scalar = (key: "caseColumn" | "activityColumn" | "timestampColumn" | "startColumn" | "zeta" | "maxInvisible", label: string) =>
    <label className="filters-field" key={key}><span>{label} (empty uses native default)</span>
      <input value={form[key]} onChange={event => change(key, event.target.value)} /></label>;
  const jsonField = (key: "activities" | "starts" | "ends" | "sequence" | "parallel" | "profile", label: string, help: string) =>
    <label className="filters-field" key={key}><span>{label}</span>
      <textarea rows={3} value={form[key]} onChange={event => change(key, event.target.value)} /><small>{help}</small></label>;
  return <>
    <section className="panel streaming-workspace">
      <div className="panel-head"><h2>Replay event streams</h2><span className="panel-meta">Independent CSV uploads</span></div>
      <p>Replay an uploaded event sequence through a fresh native streaming monitor. Inspect open-case prefixes, or finalize footprint and token-replay cases at the end of the file.</p>
      <fieldset className="query-fields" disabled={busy || pending}>
        <div className="query-options">
          <label className="filters-field"><span>Streaming method</span><select value={form.method} onChange={event => change("method", event.target.value as StreamingForm["method"])}>
            {STREAMING_METHODS.map(([id, label]) => <option key={id} value={id}>{label}</option>)}
          </select></label>
          <label className="filters-field"><span>Event replay order</span><select value={form.order} onChange={event => change("order", event.target.value as StreamingForm["order"])}>
            <option value="input">CSV input order</option><option value="imported">Case-grouped importer order</option>
          </select></label>
          <label className="filters-field"><span>Streaming event CSV</span><input type="file" accept=".csv,text/csv" aria-label="Streaming event CSV" onChange={event => upload("log", event.target.files?.[0] ?? null)} />
            <small>{log?.name ?? "Columns: case_id, activity; timestamp optional except temporal checks."}</small></label>
        </div>
        {file && /\.csv$/i.test(file.name) && <button className="btn-ghost" onClick={() => upload("log", file)}>Use current original CSV for streaming</button>}
        <p>CSV input order preserves parsed event arrival order across cases. Importer order groups cases and sorts their events by timestamps when available. Each run starts empty; it does not append to the preceding result. Original uploads are independent of current workspace filters.</p>
        <div className="query-options">{scalar("caseColumn", "Case column")}{scalar("activityColumn", "Activity column")}</div>
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
            <label className="filters-field"><span>Streaming PNML model</span><input type="file" accept=".pnml,.xml,application/xml" aria-label="Streaming PNML model" onChange={event => upload("model", event.target.files?.[0] ?? null)} /><small>{model?.name ?? "Choose a PNML workflow net with single source/sink markings."}</small></label>
            <label className="filters-field"><span>Token replay variant</span><select value={form.variant} onChange={event => change("variant", event.target.value as StreamingForm["variant"])}><option value="forward">Forward</option><option value="backwards">Backwards</option></select></label>
            {scalar("maxInvisible", "Invisible search limit")}
          </div>
          <label className="query-check"><input type="checkbox" checked={form.stopImmediately} onChange={event => change("stopImmediately", event.target.checked)} />Stop accepting events for an unfit case</label>
          <p>Silent-transition search is bounded. Marking vectors use the native parsed place order; the original PNML is recorded with the result.</p>
        </>}
        {(form.method === "footprints" || form.method === "tbr") &&
          <label className="query-check"><input type="checkbox" checked={form.finalize} onChange={event => change("finalize", event.target.checked)} />Finalize all cases after replay</label>}
        <button className="btn-open" disabled={!log || (form.method === "tbr" && !model)} onClick={() => void execute()}>Run streaming workflow</button>
      </fieldset>
      {pending && <p role="status">Replaying… <button className="btn-ghost" onClick={cancel}>Cancel streaming workflow</button></p>}
      {error && <div className="error-banner" role="alert">{error}</div>}
    </section>
    {run && selected && <section className="panel streaming-inspection">
      <div className="panel-head"><h2>Native streaming data</h2><span className="panel-meta">{run.method} · {selection} · {selected.count} rows</span></div>
      <p>Parsed events: {run.result.input.parsed_events} · Processed events: {run.result.family === "STREAMING_DFG" ? run.result.events : run.result.total_events_processed} · Order: {run.result.input.event_order}</p>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => downloadQueryBlob(new Blob([run.raw], { type: "application/json" }), "minera-streaming-data.json")}>Download native streaming data</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(new Blob([run.report_json], { type: "application/json" }), "minera-streaming-report.json")}>Download streaming report</button>
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
    {graph && <ModelGraph graph={graph} title="Streaming directly-follows graph" selected={selectedNode} onSelect={setSelectedNode} />}
  </>;
}
