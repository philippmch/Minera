import { useEffect, useRef, useState } from "react";
import { requestLogQuerySchema, downloadQueryBlob } from "../log-query";
import { ocelPreparedCsv } from "../ocel-preparation";
import { cellText } from "../transformation-workspace";
import { nativeJson } from "../native-json";
import {
  MODEL_FORMATS, PNML_IMPORT_FLAGS, defaultModelExchange, requestModelExchange,
  modelExchangeGraph, modelExchangeTreeGraph, exchangedPnml,
  type ModelExchangeForm, type ModelExchangeRun,
} from "../model-exchange";
import { ModelGraph } from "./ModelGraph";

export function ModelExchangeWorkspace({ available = false, busy = false, onUseModel }: {
  available?: boolean; busy?: boolean;
  onUseModel?: (file: File, sourceId: string, activityKey?: string, inferLegacyBoundaries?: boolean) => void;
}) {
  const [form, setForm] = useState(defaultModelExchange);
  const [file, setFile] = useState<File | null>(null);
  const [run, setRun] = useState<ModelExchangeRun | null>(null);
  const [pending, setPending] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [selection, setSelection] = useState("");
  const [page, setPage] = useState(0);
  const [columnPage, setColumnPage] = useState(0);
  const [activity, setActivity] = useState("");
  const [selectedNode, setSelectedNode] = useState<string>();
  const request = useRef<AbortController | null>(null);
  const cancel = () => { request.current?.abort(); request.current = null; setPending(false); };
  useEffect(() => () => request.current?.abort(), []);
  const change = <K extends keyof ModelExchangeForm>(key: K, value: ModelExchangeForm[K]) => {
    setForm(old => ({ ...old, [key]: value, ...(key === "format" && value !== "dfg" && old.output === "dfg" ? { output: "inspect" } : {}) }));
    setRun(null); setError(null);
  };
  const upload = (value: File | null) => {
    cancel(); setFile(value); setRun(null); setError(null);
    const suffix = value?.name.split(".").at(-1)?.toLowerCase();
    if (MODEL_FORMATS.includes(suffix as ModelExchangeForm["format"]))
      setForm(old => ({ ...old, format: suffix as ModelExchangeForm["format"], output: "inspect" }));
  };
  const execute = async () => {
    if (!file) return;
    cancel(); const controller = new AbortController(); request.current = controller;
    setPending(true); setError(null);
    try {
      const next = await requestModelExchange(file, form, controller.signal);
      if (request.current === controller && !controller.signal.aborted) {
        setRun(next); setSelection(Object.keys(next.tables)[0]); setPage(0); setColumnPage(0); setSelectedNode(undefined);
      }
    } catch (cause) {
      if (request.current === controller && !controller.signal.aborted)
        setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (request.current === controller) { request.current = null; setPending(false); }
    }
  };
  const useModel = async () => {
    if (!run || !onUseModel) return;
    cancel(); const controller = new AbortController(); request.current = controller;
    setPending(true); setError(null);
    try {
      const schema = await requestLogQuerySchema(controller.signal);
      if (request.current === controller && !controller.signal.aborted)
        onUseModel(exchangedPnml(run), schema.source_id, activity || undefined, false);
    } catch (cause) {
      if (request.current === controller && !controller.signal.aborted)
        setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (request.current === controller) { request.current = null; setPending(false); }
    }
  };
  const selected = run?.tables[selection], firstColumn = columnPage * 8;
  const graph = run && modelExchangeGraph(run.result);
  const downloadText = (text: string, suffix: string) =>
    downloadQueryBlob(new Blob([text], { type: suffix === "dfg" ? "text/plain;charset=utf-8" : "application/xml" }),
      "minera-exchanged-model." + suffix);
  return <>
    <section className="panel model-exchange-workspace">
      <div className="panel-head"><h2>Inspect and convert model files</h2><span className="panel-meta">Independent model uploads</span></div>
      <p>Read a PNML net, BPMN process, PTML tree or classic DFG file. Inspect the native model, select a conversion, or hand its same-run PNML to log conformance.</p>
      <fieldset className="query-fields" disabled={busy || pending}>
        <div className="query-options">
          <label className="filters-field"><span>Model document</span><input type="file" accept=".pnml,.bpmn,.ptml,.dfg,.xml,.txt" aria-label="Model document" onChange={event => upload(event.target.files?.[0] ?? null)} /><small>{file?.name ?? "Choose a model file; no event log is required for inspection."}</small></label>
          <label className="filters-field"><span>Model input format</span><select value={form.format} onChange={event => change("format", event.target.value as ModelExchangeForm["format"])}>{MODEL_FORMATS.map(format => <option key={format} value={format}>{format.toUpperCase()}</option>)}</select></label>
          <label className="filters-field"><span>Model output</span><select value={form.output} onChange={event => change("output", event.target.value as ModelExchangeForm["output"])}><option value="inspect">Inspect source model</option>{MODEL_FORMATS.filter(format => format !== "dfg" || form.format === "dfg").map(format => <option key={format} value={format}>Export {format.toUpperCase()}</option>)}</select></label>
        </div>
        {form.format === "pnml" && <>
          <div className="query-options">{PNML_IMPORT_FLAGS.map(([key, label]) =>
            <label className="query-check" key={key}><input type="checkbox" checked={form.flags[key]} onChange={event => change("flags", { ...form.flags, [key]: event.target.checked })} />{label}</label>)}</div>
          <button className="btn-ghost" onClick={() => change("flags", { pnml_infer_initial_marking: false, pnml_infer_final_marking: false, pnml_import_stochastic_map: true, pnml_require_workflow_boundaries: false, pnml_public_marking_semantics: false })}>Preserve general PNML markings</button>
          <p>General PNML settings preserve omitted markings as zero vectors. Public marking semantics instead retains positive entries across final markings and can infer structural sinks; it overrides the separate inference and boundary choices.</p>
        </>}
        {form.format === "ptml" && <label className="filters-field"><span>Process-tree net construction</span><select value={form.treeVariant} onChange={event => change("treeVariant", event.target.value as ModelExchangeForm["treeVariant"])}><option value="standard">Standard</option><option value="transition_bordered">Transition bordered</option></select></label>}
        {form.format === "dfg" && <><div className="query-options">
          <label className="filters-field"><span>DFG net construction</span><select value={form.dfgVariant} onChange={event => change("dfgVariant", event.target.value as ModelExchangeForm["dfgVariant"])}><option value="activity_defines_place">Activity defines place</option><option value="invisibles_no_duplicates">Invisibles without duplicate labels</option></select></label>
          {form.dfgVariant === "invisibles_no_duplicates" && <><label className="filters-field"><span>Artificial start activity (empty uses native default)</span><input value={form.startActivity} onChange={event => change("startActivity", event.target.value)} /></label>
          <label className="filters-field"><span>Artificial end activity (empty uses native default)</span><input value={form.endActivity} onChange={event => change("endActivity", event.target.value)} /></label></>}
        </div>{form.dfgVariant === "invisibles_no_duplicates" && <p>Artificial identifiers must be distinct and avoid DFG activity names. Their native boundary transitions are silent; they do not become visible activity labels in PNML.</p>}</>}
        <button className="btn-open" disabled={!file} onClick={() => void execute()}>Run model exchange</button>
      </fieldset>
      {pending && <p role="status">Reading model… <button className="btn-ghost" onClick={cancel}>Cancel model exchange</button></p>}
      {error && <div className="error-banner" role="alert">{error}</div>}
    </section>
    {run && <section className="panel model-exchange-inspection">
      <div className="panel-head"><h2>Model data</h2><span className="panel-meta">{run.result.format.toUpperCase()} · {run.source.name}</span></div>
      <p>{run.result.net ? run.result.net.place_count + " places · " + run.result.net.transition_count + " transitions · " + run.result.arcs.length + " arc occurrences" : "DFG source data"} · Requested output: {run.result.output_format}</p>
      {run.result.conversion_error && <div className="error-banner" role="alert">Native conversion: {run.result.conversion_error}. Imported data remains available.</div>}
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => downloadQueryBlob(new Blob([run.raw], { type: "application/json" }), "minera-model-data.json")}>Download native model data</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(new Blob([run.report_json], { type: "application/json" }), "minera-model-report.json")}>Download model exchange report</button>
        <button className="btn-ghost" onClick={() => downloadText(run.result.canonical, run.result.format)}>{run.result.format === "bpmn" ? "Download validated BPMN source" : "Download normalized source model"}</button>
        {run.result.pnml && <button className="btn-ghost" onClick={() => downloadText(run.result.pnml!, "pnml")}>Download native PNML</button>}
        {run.result.exported && <button className="btn-open" onClick={() => downloadText(run.result.exported!, run.result.output_format)}>Download converted model</button>}
        {file && <button className="btn-ghost" onClick={() => downloadQueryBlob(file, file.name)}>Download original model upload</button>}
      </div>
      {onUseModel && <div className="query-options">
        <label className="filters-field"><span>Log activity field for model check (empty uses current default)</span><input disabled={busy || pending} value={activity} onChange={event => setActivity(event.target.value)} /></label>
        <button className="btn-open" disabled={!available || busy || pending || !run.result.pnml} onClick={() => void useModel()}>Check exchanged model against current log</button>
        <p>{available ? "Handoff uses the current log revision and preserves this run's PNML markings." : "Open a CSV/XES log to check this model. Independent inspection and exports remain available."}</p>
      </div>}
      <div className="query-actions">{Object.entries(run.tables).map(([name, table]) => <button key={name} className={name === selection ? "btn-open" : "btn-ghost"} onClick={() => { setSelection(name); setPage(0); setColumnPage(0); }}>{name} · {table.count} rows</button>)}</div>
      {selected && <><p>Native indices, XML IDs and activity labels are separate fields. Complete exports retain every row and column. Arc rows distinguish occurrences, direction, type and exact weight.</p>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(ocelPreparedCsv(selected), "minera-model-" + selection + ".csv")}>Download complete model CSV</button>
        <div className="query-table-wrap"><table className="activity-table"><thead><tr>{selected.columns.slice(firstColumn, firstColumn + 8).map((name, i) => <th key={i}>{name}</th>)}</tr></thead><tbody>
          {Array.from({ length: Math.max(0, Math.min(25, selected.count - page * 25)) }, (_, i) => <tr key={i}>{selected.rowAt(page * 25 + i).slice(firstColumn, firstColumn + 8).map((value, k) => <td key={k}>{cellText(value)}</td>)}</tr>)}
        </tbody></table></div>
        {!selected.count && <p>No native rows in this category; its CSV schema remains available.</p>}
        {selected.count > 25 && <div className="query-actions"><button className="btn-ghost" disabled={!page} onClick={() => setPage(p => p - 1)}>Previous model rows</button><span>Rows {page * 25 + 1}–{Math.min(selected.count, page * 25 + 25)} / {selected.count}</span><button className="btn-ghost" disabled={(page + 1) * 25 >= selected.count} onClick={() => setPage(p => p + 1)}>Next model rows</button></div>}
        {selected.columns.length > 8 && <div className="query-actions"><button className="btn-ghost" disabled={!columnPage} onClick={() => setColumnPage(p => p - 1)}>Previous model columns</button><span>Columns {firstColumn + 1}–{Math.min(selected.columns.length, firstColumn + 8)} / {selected.columns.length}</span><button className="btn-ghost" disabled={firstColumn + 8 >= selected.columns.length} onClick={() => setColumnPage(p => p + 1)}>Next model columns</button></div>}
      </>}
      <details><summary>Model exchange request and source binding</summary><pre>{JSON.stringify({ source: run.source, query: run.query }, null, 2)}</pre></details>
      <details><summary>Complete native model response</summary><pre>{nativeJson(run.result, 2)}</pre></details>
    </section>}
    {graph && <ModelGraph graph={graph} title={run!.result.net ? "Native exchanged Petri net" : "Native imported DFG"} selected={selectedNode} onSelect={setSelectedNode} minimumLabel="Minimum displayed occurrence weight" />}
    {run?.result.tree && <><p className="model-graph-note">Tree edges show child positions and containment; they are distinct from Petri arcs and event order.</p><ModelGraph graph={modelExchangeTreeGraph(run.result.tree)} title="Native exchanged process tree" /></>}
  </>;
}
