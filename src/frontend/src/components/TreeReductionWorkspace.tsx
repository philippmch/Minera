import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeJson} from "../native-json";
import {nativeStringBytes} from "../native-multipart";
import {originalEmbeddingFilterStringText as stringText} from "../original-embedding-filter-workbench";
import {TREE_REDUCTION_MODEL_EXAMPLE, TREE_REDUCTION_OPTIONS_EXAMPLE, TREE_REDUCTION_TRACE_EXAMPLE, TreeReductionRequestGate, defaultTreeReductionForm, runTreeReductionWorkbench, treeReductionAllTablesCsv, treeReductionArtifacts, treeReductionGraph, treeReductionReport, type TreeReductionForm, type TreeReductionRun, type TreeReductionUploads} from "../tree-reduction-workbench";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";
import {ModelGraph} from "./ModelGraph";

const downloadText = (text: string, name: string, mime = "application/json") => downloadQueryBlob(new Blob([nativeStringBytes(text)], {type: mime}), name);
export function TreeReductionWorkspace({active}: {active: boolean}) {
  const [formats, setFormats] = useState(defaultTreeReductionForm), [model, setModel] = useState<File | null>(null), [trace, setTrace] = useState<File | null>(null);
  const [mode, setMode] = useState<"scalar" | "json" | "file">("scalar"), [keyMode, setKeyMode] = useState<"omit" | "text" | "file">("omit");
  const [variant, setVariant] = useState(""), [keyText, setKeyText] = useState(""), [rawOptions, setRawOptions] = useState(TREE_REDUCTION_OPTIONS_EXAMPLE), [optionsFile, setOptionsFile] = useState<File | undefined>(), [keyFile, setKeyFile] = useState<File | undefined>();
  const [returned, setReturned] = useState<TreeReductionRun | null>(null), [failed, setFailed] = useState<TreeReductionRun | null>(null), [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null), gate = useRef(new TreeReductionRequestGate());
  const fields: Record<string, string> = mode === "json" ? {tree_reduction_options_json: rawOptions} : mode === "scalar" ? {...(variant ? {tree_reduction_variant: variant} : {}), ...(keyMode === "text" ? {tree_reduction_activity_key: keyText} : {})} : {};
  const form: TreeReductionForm = {...formats, fields}, uploads: TreeReductionUploads = {...(mode === "file" && optionsFile ? {options: optionsFile} : {}), ...(mode === "scalar" && keyMode === "file" && keyFile ? {activityKey: keyFile} : {})};
  const current = useRef({active, model, trace, form, uploads}); current.current = {active, model, trace, form, uploads};
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const edit = (change: () => void) => {cancel(); change(); setError(null);};
  useEffect(() => {if (!active) {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);}}, [active]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort();}, []);
  const execute = async () => {
    if (!model || !trace || !active) return;
    cancel(); const abort = new AbortController(), snapshot = structuredClone(form), files = {...uploads}, ticket = gate.current.issue(model, trace, snapshot, files); controller.current = abort; setPending(true); setError(null);
    const accepts = () => {const now = current.current; return now.active && controller.current === abort && gate.current.accepts(ticket, now.model, now.trace, now.form, now.uploads, abort.signal);};
    try {const run = await runTreeReductionWorkbench(model, trace, snapshot, files, abort.signal); if (accepts()) {if (run.value.error) setFailed(run); else {setReturned(run); setFailed(null);}}}
    catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  return <div hidden={!active}>
    <section className="panel" aria-label="Independent original process-tree reduction">
      <div className="panel-head"><h2>Reduce an original process tree</h2><span className="panel-meta">TREE_TR_BASED · original tree and trace</span></div>
      <p>Supply the original process tree and a separate trace. The complete activity registry, ordered children, inactive stored indexes, Trace attributes and first-class states remain inspectable alongside the reduced tree.</p>
      <fieldset className="query-fields">
        <div className="query-options">
          <label className="filters-field"><span>Original process-tree File</span><input type="file" accept=".ptml,.xml,.json,application/xml,application/json" onChange={event => {const file = event.target.files?.[0] ?? null; event.target.value = ""; edit(() => {setModel(file); setReturned(null); setFailed(null);});}} /><span>{model?.name ?? "Choose the original tree"}</span></label>
          <label className="filters-field"><span>Original tree format</span><select value={form.modelFormat} onChange={event => edit(() => setFormats(value => ({...value, modelFormat: event.target.value as TreeReductionForm["modelFormat"]})))}><option value="auto">Detect PTML or native tree JSON</option><option value="ptml">PTML</option><option value="tree_json">Full native tree JSON</option></select></label>
          <label className="filters-field"><span>Original trace File</span><input type="file" accept=".json,application/json" onChange={event => {const file = event.target.files?.[0] ?? null; event.target.value = ""; edit(() => {setTrace(file); setReturned(null); setFailed(null);});}} /><span>{trace?.name ?? "Choose the original trace"}</span></label>
          <label className="filters-field"><span>Original trace format</span><select value={form.traceFormat} onChange={event => edit(() => setFormats(value => ({...value, traceFormat: event.target.value as TreeReductionForm["traceFormat"]})))}><option value="native_trace">Full native Trace constructor</option><option value="trace_words">Ordered NativeString word array</option></select></label>
        </div>
        <details><summary>Complete original tree and trace examples</summary><p>The tree contains an optional A branch, B, and an unused registry entry. The full Trace example retains two B events with their original order and every stored first-class field. Word input is an ordered JSON array, for example ["B","B"]. String leaves also accept {`{"native_bytes":"hex-v1","data":"ff00eda080"}`} byte carriers.</p><pre>{TREE_REDUCTION_MODEL_EXAMPLE}</pre><button className="btn-ghost" onClick={() => downloadText(TREE_REDUCTION_MODEL_EXAMPLE, "minera-reduction-original-tree.json")}>Download full tree example</button><pre>{TREE_REDUCTION_TRACE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => downloadText(TREE_REDUCTION_TRACE_EXAMPLE, "minera-reduction-original-trace.json")}>Download full native Trace example</button></details>
        <label className="filters-field"><span>Tree reduction options carrier</span><select value={mode} onChange={event => edit(() => setMode(event.target.value as typeof mode))}><option value="scalar">Native defaults or scalar controls</option><option value="json">Complete options JSON</option><option value="file">Complete original options File</option></select></label>
        {mode === "json" && <label className="filters-field"><span>Complete tree reduction options JSON</span><textarea rows={5} value={rawOptions} onChange={event => edit(() => setRawOptions(event.target.value))} /></label>}
        {mode === "file" && <label className="filters-field"><span>Original tree reduction options File</span><input type="file" accept=".json,application/json" onChange={event => {const file = event.target.files?.[0]; event.target.value = ""; edit(() => setOptionsFile(file));}} /><span>{optionsFile?.name ?? "Choose complete options JSON bytes"}</span></label>}
        {mode !== "scalar" && <p>Complete options contain exactly variant_index (signed int32) and activity_key (NativeString). Unknown variant indexes remain native errors with the parsed options retained.</p>}
        {mode === "scalar" && <div className="query-options">
          <label className="filters-field"><span>Tree reduction variant</span><select value={variant} onChange={event => edit(() => setVariant(event.target.value))}><option value="">Omit · native TREE_TR_BASED default</option><option value="TREE_TR_BASED">TREE_TR_BASED</option><option value="tree_tr_based">tree_tr_based alias</option></select></label>
          <label className="filters-field"><span>Activity-key carrier</span><select value={keyMode} onChange={event => edit(() => setKeyMode(event.target.value as typeof keyMode))}><option value="omit">Omit · native activity selector</option><option value="text">Literal key, including explicit empty</option><option value="file">Original raw activity-key File</option></select></label>
          {keyMode === "text" && <label className="filters-field"><span>Literal tree reduction activity key</span><textarea rows={3} value={keyText} onChange={event => edit(() => setKeyText(event.target.value))} /></label>}
          {keyMode === "file" && <label className="filters-field"><span>Original raw activity-key File</span><input type="file" onChange={event => {const file = event.target.files?.[0]; event.target.value = ""; edit(() => setKeyFile(file));}} /><span>{keyFile?.name ?? "Choose literal key bytes"}</span></label>}
        </div>}
        <div className="query-actions"><button className="btn-ghost" disabled={pending || !model || !trace || mode === "file" && !optionsFile || mode === "scalar" && keyMode === "file" && !keyFile} onClick={() => void execute()}>Reduce original process tree</button></div>
      </fieldset>
      {pending && <p role="status">Reducing the original tree with the original trace… <button className="btn-ghost" onClick={cancel}>Cancel tree reduction request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {failed && <TreeReductionInspection run={failed} failed />}
    {returned && <TreeReductionInspection run={returned} />}
  </div>;
}
export function TreeReductionInspection({run, failed = false}: {run: TreeReductionRun; failed?: boolean}) {
  const [tableId, setTableId] = useState("source_tree_nodes"), table = run.tables.find(item => item.id === tableId) ?? run.tables[0], artifacts = treeReductionArtifacts(run);
  const prefix = failed ? "minera-reduce_tree-failed" : "minera-reduce_tree";
  return <>
    {run.value.model.process_tree && <ModelGraph graph={treeReductionGraph(run.value.model.process_tree)} title="Original process tree before reduction" />}
    {run.value.result?.process_tree && <ModelGraph graph={treeReductionGraph(run.value.result.process_tree)} title="Reduced process tree" />}
    <section className="panel" aria-label={failed ? "Failed original tree reduction inspection" : "Returned original tree reduction inspection"}>
      <div className="panel-head"><h2>{failed ? "Failed original tree reduction" : "Returned original tree reduction"}</h2><span className="panel-meta">HTTP {run.http_status} · entered {String(run.value.native_entered)} · returned {String(run.value.native_returned)}</span></div>
      <p>{run.input.model.name} + {run.input.trace.name}. Original model unchanged: {String(run.value.input_context.model_unchanged)}. Original trace unchanged: {String(run.value.trace_context.trace_unchanged)}.</p>
      {run.value.error && <p className="error-banner" role="alert">{stringText(run.value.error.type)}: {stringText(run.value.error.message)}</p>}
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => downloadText(run.raw, `${prefix}-native.json`)}>Download complete native tree reduction JSON</button>
        <button className="btn-ghost" onClick={() => downloadText(treeReductionReport(run), `${prefix}-report.json`)}>Download tree reduction report</button>
        <button className="btn-ghost" onClick={() => downloadText(diagnosticCsv(table), `${prefix}-table.csv`, "text/csv;charset=utf-8")}>Download complete selected table CSV</button>
        <button className="btn-ghost" onClick={() => downloadText(treeReductionAllTablesCsv(run.tables), `${prefix}-all-tables.csv`, "text/csv;charset=utf-8")}>Download every tree reduction table CSV</button>
        {(["model", "trace", "options", "activityKey"] as const).map(key => {const file = run.input[key]; return file && <button className="btn-ghost" key={key} onClick={() => downloadQueryBlob(file, file.name)}>Download original {key} File</button>;})}
        {artifacts.map(artifact => <button className="btn-ghost" key={artifact.name} onClick={() => downloadQueryBlob(new Blob([artifact.bytes], {type: artifact.mime}), artifact.name)}>Download {artifact.name.replace("minera-reduce_tree-", "")}</button>)}
      </div>
      {[...Object.entries(run.value.artifacts), ...Object.entries(run.value.exports)].filter(([key]) => key.endsWith("_error")).map(([key, value]) => value !== null && <p key={key}>{key}: {stringText(value)}</p>)}
      <details><summary>Both original File hashes, exact request and complete native report</summary><pre>{treeReductionReport(run)}</pre></details>
      <label className="filters-field"><span>Complete tree reduction inspection table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={table.id} table={table} noun="tree reduction" />
      <details><summary>Complete original and result native snapshot JSON</summary><pre>{run.raw}</pre></details>
      <details><summary>Exact submitted scalar and JSON fields</summary><pre>{nativeJson(run.fields, 2)}</pre></details>
    </section>
  </>;
}
