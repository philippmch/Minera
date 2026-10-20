import {useEffect, useRef, useState} from "react";
import {type GraphAlignmentRun, GRAPH_ALIGNMENT_CONTROLS, GRAPH_ALIGNMENT_PROJECTIONS, GraphAlignmentRequestGate, graphAlignmentAllTablesCsv} from "../graph-alignment-workbench";
import {defaultSavedDfgProjectionForm, SAVED_DFG_INPUT_SHAPES, runSavedDfgProjectionWorkbench, savedDfgProjectionReport, savedDfgProjectionReuse, type SavedDfgProjectionRun, type SavedDfgProjectionForm} from "../saved-dfg-projection-workbench";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeJson, parseNativeJson} from "../native-json";
import {nativeStringBytes} from "../native-multipart";
import {originalEmbeddingFilterStringBytes} from "../original-embedding-filter-workbench";
import {downloadQueryBlob} from "../log-query";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";

export function SavedDfgProjectionWorkspace({active, completed, original = null}: {active: boolean; completed: GraphAlignmentRun | null; original?: File | null}) {
  const [input, setInput] = useState<File | null>(original), [model, setModel] = useState<File | null>(null), [saved, setSaved] = useState<File | null>(null);
  const [form, setForm] = useState(defaultSavedDfgProjectionForm), [imports, setImports] = useState("{}"), [inline, setInline] = useState(false), [inlineText, setInlineText] = useState("");
  const [run, setRun] = useState<SavedDfgProjectionRun | null>(null), [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const gate = useRef(new GraphAlignmentRequestGate()), controller = useRef<AbortController | null>(null);
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const invalidate = () => {cancel(); setError(null);};
  useEffect(() => {if (!active) cancel();}, [active]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort();}, []);
  const update = (change: (current: SavedDfgProjectionForm) => SavedDfgProjectionForm) => {invalidate(); setForm(change);};
  const editSavedInline = async () => {
    if (!saved) return;
    invalidate(); const ticket = gate.current.issue(), selected = saved;
    try {const text = new TextDecoder("utf-8", {fatal: true}).decode(await selected.arrayBuffer()); parseNativeJson(text); if (gate.current.accepts(ticket)) {setInlineText(text); setInline(true); setSaved(null);}}
    catch (cause) {if (gate.current.accepts(ticket)) setError(cause instanceof Error ? cause.message : String(cause));}
  };
  const reuse = (shape: "workbench" | "report" = "workbench") => {
    if (!completed) return;
    invalidate(); try {const value = savedDfgProjectionReuse(completed, shape); setInput(value.input); setModel(value.model); setSaved(value.saved); setForm(value.form); setImports(nativeJson(value.form.source.importFields, 2)); setInline(false); setInlineText("");}
    catch (cause) {setError(cause instanceof Error ? cause.message : String(cause));}
  };
  const execute = async () => {
    if (!input || !model) return;
    invalidate(); const ticket = gate.current.issue(), abort = new AbortController(); controller.current = abort; setPending(true);
    try {
      const imported = parseNativeJson(imports);
      if (!imported || typeof imported !== "object" || Array.isArray(imported) || Object.values(imported).some(value => typeof value !== "string")) throw new Error("Source import settings must be a JSON object of exact string values.");
      const snapshot = {...form, source: {...form.source, importFields: imported as Record<string, string>}, ...(inline ? {savedJson: inlineText} : {})};
      const value = await runSavedDfgProjectionWorkbench(input, model, snapshot, abort.signal, inline ? null : saved);
      if (gate.current.accepts(ticket, abort.signal)) setRun(value);
    } catch (cause) {if (gate.current.accepts(ticket, abort.signal)) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  return <div hidden={!active}>
    <section className="panel" aria-label="Saved DFG projection inputs">
      <div className="panel-head"><h2>Project supplied DFG alignment routes</h2><span className="panel-meta">Saved result</span></div>
      <p>Choose the original source, the complete DFG and a saved result. Native projection uses the supplied per-trace routes without running alignment search or recalculating their scores. Accepted routes do not establish optimality or source/model provenance.</p>
      {completed && <div className="query-actions"><button className="btn-ghost" onClick={() => reuse()}>Reuse completed DFG original Files and full result</button><button className="btn-ghost" onClick={() => reuse("report")}>Reuse completed DFG report and original Files</button></div>}
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Saved projection original source</span><input type="file" onChange={event => {invalidate(); setInput(event.target.files?.[0] ?? null);}} /><span>{input?.name ?? "Choose original source"}</span></label>
          <label className="filters-field"><span>Saved projection complete DFG model</span><input type="file" accept=".json,.dfg,application/json,text/plain" onChange={event => {invalidate(); setModel(event.target.files?.[0] ?? null);}} /><span>{model?.name ?? "Choose complete DFG and boundaries"}</span></label>
          <label className="filters-field"><span>Saved projection source format</span><select value={form.inputFormat} onChange={event => update(current => ({...current, inputFormat: event.target.value as SavedDfgProjectionForm["inputFormat"]}))}><option value="auto">CSV / TSV / XES / gzip XES</option><option value="table">Complete typed EventTable JSON</option><option value="raw_log">Complete native raw EventLog JSON</option></select></label>
          <label className="filters-field"><span>Saved projection model format</span><select value={form.source.modelFormat} onChange={event => update(current => ({...current, source: {...current.source, modelFormat: event.target.value as SavedDfgProjectionForm["source"]["modelFormat"]}}))}><option value="auto">Detect DFG model</option><option value="dfg_json">Structured DFG JSON</option><option value="dfg">Classic DFG text</option></select></label>
          <label className="filters-field"><span>Saved result envelope shape</span><select value={form.savedInputShape} onChange={event => update(current => ({...current, savedInputShape: event.target.value as SavedDfgProjectionForm["savedInputShape"]}))}>{SAVED_DFG_INPUT_SHAPES.map(value => <option key={value}>{value}</option>)}</select></label>
          <label className="filters-field"><span>Saved result projection</span><select value={form.source.projection} onChange={event => update(current => ({...current, source: {...current.source, projection: event.target.value as SavedDfgProjectionForm["source"]["projection"]}}))}>{GRAPH_ALIGNMENT_PROJECTIONS.map(value => <option key={value}>{value}</option>)}</select></label>
        </div>
        <label className="query-check"><input type="checkbox" checked={inline} onChange={event => {invalidate(); setInline(event.target.checked);}} />Enter or edit saved JSON inline</label>
        {inline ? <label className="filters-field"><span>Complete saved alignment JSON</span><textarea rows={10} value={inlineText} onChange={event => {invalidate(); setInlineText(event.target.value);}} /></label> : <label className="filters-field"><span>Original saved alignment File</span><input type="file" accept=".json,application/json" onChange={event => {invalidate(); setSaved(event.target.files?.[0] ?? null);}} /><span>{saved?.name ?? "Choose the original full saved result"}</span></label>}
        {saved && !inline && <button className="btn-ghost" onClick={() => void editSavedInline()}>Edit uploaded saved result inline</button>}
        <details><summary>Exact selectors, import settings and native cost options</summary>
          <label className="filters-field"><span>Literal alignment selectors JSON</span><textarea rows={4} value={form.selectorsJson} placeholder={'Native defaults, or {"dfg_activity_attribute":"concept:name","dfg_timestamp_attribute":{"native_bytes":"hex-v1","data":"ff"}}'} onChange={event => update(current => ({...current, selectorsJson: event.target.value}))} /></label>
          <p>Blank uses native selectors. Explicit empty strings, whitespace and hex-v1 NativeString carriers remain exact. A supplied selector object overrides all selector entries.</p>
          <label className="filters-field"><span>Source import settings JSON</span><textarea rows={4} value={imports} onChange={event => {invalidate(); setImports(event.target.value);}} /></label>
          <p>Use the original import’s exact string controls, including CSV mappings, delimiter, XES variant and boolean values. Typed and raw-log formats retain their complete native structures.</p>
          {GRAPH_ALIGNMENT_CONTROLS.DFG_CLASSIC.filter(control => control.kind === "map").map(control => <label className="filters-field" key={control.key}><span>{control.label}</span><textarea rows={3} value={form.source.fields[control.key] ?? ""} placeholder="Native omission, or {}" onChange={event => update(current => ({...current, source: {...current.source, fields: {...current.source.fields, [control.key]: event.target.value}}}))} /></label>)}
        </details>
        <p>Supply full alignments, scores, computed flags, unique-variant count and search counters. The original saved bytes, canonical result and separate projection/export failures remain inspectable.</p>
        <button className="btn-open" disabled={!input || !model || (!inline && !saved)} onClick={() => void execute()}>Project saved DFG result</button>
      </fieldset>
      {pending && <p role="status">Projecting supplied DFG routes… <button className="btn-ghost" onClick={cancel}>Cancel saved DFG projection</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {run && <SavedDfgProjectionInspection key={run.raw} run={run} />}
  </div>;
}
function SavedDfgProjectionInspection({run}: {run: SavedDfgProjectionRun}) {
  const [tableId, setTableId] = useState("summary"), table = run.tables.find(value => value.id === tableId) ?? run.tables[0];
  const download = (text: string, name: string, type = "application/json") => downloadQueryBlob(new Blob([nativeStringBytes(text)], {type}), `minera-saved-dfg-${name}`);
  return <section className="panel" aria-label="Complete saved DFG projection inspection">
    <div className="panel-head"><h2>Supplied routes and projected results</h2><span className="panel-meta">No alignment search</span></div>
    <p>{run.inputs.input.name} · {run.inputs.model.name} · {run.inputs.saved?.name ?? "Inline saved result"}. HTTP {run.http_status}. Scores and counters belong to the supplied result.</p>
    <div className="query-actions">
      <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete saved projection native JSON</button>
      <button className="btn-ghost" onClick={() => download(savedDfgProjectionReport(run), "report.json")}>Download saved projection report</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(run.fields, 2), "request.json")}>Download exact projection request</button>
      <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.input, run.inputs.input.name)}>Download original projection source</button>
      <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.model, run.inputs.model.name)}>Download original projection model</button>
      <button className="btn-ghost" onClick={() => run.inputs.saved ? downloadQueryBlob(run.inputs.saved, run.inputs.saved.name) : download(run.savedJson!, "submitted-inline.json")}>Download original supplied result</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(run.value.result, 2), "canonical-supplied-result.json")}>Download canonical supplied alignment result</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(run.value.model_context.dfg, 2), "canonical-model.json")}>Download complete canonical DFG and boundaries</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(run.value.input_raw_log, 2), "original-raw-log.json")}>Download complete source raw log snapshot</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(run.value.projection, 2), "metrics.json")}>Download projected DFG metrics</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(run.value.projected_raw_log, 2), "projected-raw-log.json")}>Download complete projected raw log</button>
      <button className="btn-ghost" disabled={run.value.projected_xes === null} onClick={() => downloadQueryBlob(new Blob([originalEmbeddingFilterStringBytes(run.value.projected_xes!)], {type: "application/xml"}), "minera-saved-dfg-projected.xes")}>Download saved projection XES</button>
      <button className="btn-ghost" disabled={run.value.model_export === null} onClick={() => downloadQueryBlob(new Blob([originalEmbeddingFilterStringBytes(run.value.model_export!)], {type: "text/plain"}), "minera-saved-dfg-model.dfg")}>Download canonical DFG export</button>
      <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${encodeURIComponent(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete saved projection table CSV</button>
      <button className="btn-ghost" onClick={() => download(graphAlignmentAllTablesCsv(run.tables), "all-tables.csv", "text/csv;charset=utf-8")}>Download all saved projection tables CSV</button>
    </div>
    {["error", "projection_error", "metrics_error", "log_projection_error", "projected_log_error", "projected_xes_error", "model_export_error"].map(key => run.value[key] !== null && <p key={key}>{key}: {nativeJson(run.value[key])}</p>)}
    <label className="filters-field"><span>Saved DFG projection result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(value => <option key={value.id} value={value.id}>{value.title} · {value.rows.length} rows</option>)}</select></label>
    <DiagnosticTableView key={table.id} table={table} noun="saved projection" />
    <details><summary>Complete supplied routes, input/model snapshots and projection JSON</summary><pre>{run.raw}</pre></details>
  </section>;
}
