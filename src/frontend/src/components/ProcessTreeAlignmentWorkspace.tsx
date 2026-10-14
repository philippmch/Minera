import {miningSourceWorkbenchFields, type ActiveMiningSourceDescriptor} from "../active-mining-source";
import {useEffect, useMemo, useRef, useState} from "react";
import {
  TREE_ALIGNMENT_CONTROLS, TREE_ALIGNMENT_CONVERSIONS, TREE_ALIGNMENT_INPUT_FORMATS, TREE_ALIGNMENT_INPUT_SHAPES,
  TREE_ALIGNMENT_MODEL_EXAMPLE, TREE_ALIGNMENT_MODEL_FORMATS, TREE_ALIGNMENT_TRACE_EXAMPLE, TREE_ALIGNMENT_VARIANTS,
  ProcessTreeAlignmentRequestGate, defaultProcessTreeAlignmentForm, processTreeAlignmentAllTablesCsv,
  processTreeAlignmentArtifactFile, processTreeAlignmentReport, processTreeAlignmentSucceeded, processTreeAlignmentSummary,
  processTreeAlignmentTableFileName, processTreeAlignmentTreeGraph, runProcessTreeAlignmentWorkbench,
  type ProcessTreeAlignmentForm, type ProcessTreeAlignmentRun, type TreeAlignmentControl,
} from "../process-tree-alignment-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../token-replay-workbench";
import {WORKFLOW_INPUT_TABLE_EXAMPLE, WORKFLOW_INPUT_XES_VARIANTS, workflowInputNativeBytes} from "../workflow-input-workbench";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeCell, nativeJson} from "../native-json";
import {nativeResponseRequest} from "../native-response";
import {downloadQueryBlob} from "../log-query";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";
import {ModelGraph} from "./ModelGraph";

const variantLabels = {SEARCH_GRAPH_PT: "Search graph", DYNAMIC_PROGRAMMING: "Dynamic programming", MILP: "MILP", APPROXIMATED_ORIGINAL: "Approximated original"};
const inputLabels = {auto: "CSV / TSV / XES / gzip XES", table: "Typed EventTable JSON", trace_words: "Literal trace words JSON"};
const shapeLabels = {log: "Every grouped trace", trace: "One selected original trace", words: "Literal trace words"};
const modelLabels = {auto: "Detect from model file", ptml: "PTML process tree", tree_json: "Complete native tree JSON"};
export function ProcessTreeAlignmentWorkspace({file: original = null, source = null}: {file?: File | null; source?: ActiveMiningSourceDescriptor | null}) {
  const [input, setInput] = useState<File | null>(original), [model, setModel] = useState<File | null>(null), [form, setForm] = useState(defaultProcessTreeAlignmentForm);
  const [completed, setCompleted] = useState<ProcessTreeAlignmentRun | null>(null), [failed, setFailed] = useState<ProcessTreeAlignmentRun | null>(null);
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null), gate = useRef(new ProcessTreeAlignmentRequestGate()), identities = useRef({input: original, model: null as File | null});
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const clearResults = () => {setCompleted(null); setFailed(null); setError(null);};
  const chooseInput = (value: File | null) => {cancel(); identities.current.input = value; setInput(value); clearResults();};
  const chooseModel = (value: File | null) => {cancel(); identities.current.model = value; setModel(value); clearResults();};
  const adoptSource = () => {chooseInput(original); if (source) {const mapped = miningSourceWorkbenchFields(source, {case_id_key: "alignment_case_id_attribute", activity_key: "pt_alignment_activity_attribute"}); setForm(current => ({...current, inputFormat: source.format, inputShape: current.inputShape === "words" ? "log" : current.inputShape, fields: {...current.fields, ...mapped.fields}, explicitEmptySelectors: {...current.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields}));}};
  useEffect(() => {adoptSource();}, [original, source]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null;}, []);
  const field = (key: string, value: string) => setForm(current => ({...current, fields: {...current.fields, [key]: value}}));
  const importField = (key: string, value: string) => setForm(current => ({...current, importFields: {...current.importFields, [key]: value}}));
  const inputFormat = (value: ProcessTreeAlignmentForm["inputFormat"]) => setForm(current => ({...current, inputFormat: value, inputShape: value === "trace_words" ? "words" : current.inputShape === "words" ? "log" : current.inputShape}));
  const inputShape = (value: ProcessTreeAlignmentForm["inputShape"]) => setForm(current => ({...current, inputShape: value, inputFormat: value === "words" ? "trace_words" : current.inputFormat === "trace_words" ? "auto" : current.inputFormat}));
  const execute = async () => {
    if (!input || !model) return; cancel(); const abort = new AbortController(), ticket = gate.current.issue(), submittedInput = input, submittedModel = model, snapshot = structuredClone(form);
    controller.current = abort; setPending(true); setError(null);
    const accepts = () => identities.current.input === submittedInput && identities.current.model === submittedModel && controller.current === abort && gate.current.accepts(ticket, abort.signal);
    try {
      const result = await runProcessTreeAlignmentWorkbench(submittedInput, submittedModel, snapshot, abort.signal);
      if (accepts()) {if (processTreeAlignmentSucceeded(result.value)) {setCompleted(result); setFailed(null);} else setFailed(result);}
    } catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  const isXes = /\.(?:xes|xes\.gz|gz)$/i.test(input?.name ?? "");
  const download = (text: string, name: string) => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type: "application/json"}), name);
  return <>
    <section className="panel" aria-label="Independent process-tree alignment">
      <div className="panel-head"><h2>Align original traces against a process-tree model</h2><span className="panel-meta">Independent inputs and model</span></div>
      <p>Upload an original input and a normative PTML or complete native process tree. Choose an alignment variant, then inspect every returned move, native diagnostic and original model field.</p>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Tree alignment original input</span><input type="file" accept={form.inputFormat === "auto" ? ".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" : ".json,application/json"} onChange={event => chooseInput(event.target.files?.[0] ?? null)} /><span>{input?.name ?? "Choose an original input"}</span></label>
          <label className="filters-field"><span>Tree alignment normative model</span><input type="file" accept=".ptml,.xml,.json,application/xml,application/json" onChange={event => chooseModel(event.target.files?.[0] ?? null)} /><span>{model?.name ?? "Choose PTML or native tree JSON"}</span></label>
          <label className="filters-field"><span>Tree alignment variant</span><select value={form.variant} onChange={event => setForm(current => ({...current, variant: event.target.value as ProcessTreeAlignmentForm["variant"]}))}>{TREE_ALIGNMENT_VARIANTS.map(value => <option key={value} value={value}>{variantLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Tree alignment input format</span><select value={form.inputFormat} onChange={event => inputFormat(event.target.value as ProcessTreeAlignmentForm["inputFormat"])}>{TREE_ALIGNMENT_INPUT_FORMATS.map(value => <option key={value} value={value}>{inputLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Tree alignment trace selection</span><select value={form.inputShape} onChange={event => inputShape(event.target.value as ProcessTreeAlignmentForm["inputShape"])}>{TREE_ALIGNMENT_INPUT_SHAPES.map(value => <option key={value} value={value}>{shapeLabels[value]}</option>)}</select></label>
          {form.inputShape === "trace" && <label className="filters-field"><span>Original trace index (zero-based)</span><input value={form.traceIndex} placeholder="Native default: first trace" onChange={event => setForm(current => ({...current, traceIndex: event.target.value}))} /></label>}
          <label className="filters-field"><span>Tree alignment model format</span><select value={form.modelFormat} onChange={event => setForm(current => ({...current, modelFormat: event.target.value as ProcessTreeAlignmentForm["modelFormat"]}))}>{TREE_ALIGNMENT_MODEL_FORMATS.map(value => <option key={value} value={value}>{modelLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Original-tree Petri conversion</span><select value={form.conversion} onChange={event => setForm(current => ({...current, conversion: event.target.value as ProcessTreeAlignmentForm["conversion"]}))}>{TREE_ALIGNMENT_CONVERSIONS.map(value => <option key={value} value={value}>{value === "standard" ? "Standard" : "Transition bordered"}</option>)}</select></label>
        </div>
        {original && <button className="btn-ghost" onClick={adoptSource}>Use current original log for tree alignment</button>}
        {form.inputFormat === "trace_words" && <details open><summary>Literal trace words JSON</summary><p>Upload one JSON array of strings. The route builds a grouped log for the native alignment call. Repeats, whitespace, empty labels and Unicode retain their exact input spelling; native validation determines accepted activities.</p><pre>{TREE_ALIGNMENT_TRACE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(TREE_ALIGNMENT_TRACE_EXAMPLE, "minera-tree-alignment-words.json")}>Download literal tree alignment trace example</button></details>}
        {form.inputFormat === "table" && <details><summary>Typed event table JSON</summary><p>The original table preserves every cell, dtype, index, category and metadata field. Generic log conversion groups it by the literal case selector. Missing values, present nulls and empty strings remain distinct.</p><pre>{WORKFLOW_INPUT_TABLE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(WORKFLOW_INPUT_TABLE_EXAMPLE, "minera-tree-alignment-table.json")}>Download typed tree alignment table example</button></details>}
        <details><summary>Complete native process-tree JSON</summary><p>The model declares an ordered activities registry and a root with operator, operator index, stored activity index and ordered children at every node. Duplicate or unused registry labels and latent indices remain in the supplied model. The complete model is inspectable even when native alignment rejects it.</p><pre>{TREE_ALIGNMENT_MODEL_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(TREE_ALIGNMENT_MODEL_EXAMPLE, "minera-tree-alignment-model.json")}>Download native tree model example</button></details>
        {form.inputFormat === "auto" && <details><summary>Tree alignment import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key]) => isXes ? key.startsWith("xes_") : !key.startsWith("xes_")).map(([key, label, placeholder]) => <label className="filters-field" key={key}><span>{label}</span><input value={form.importFields[key] ?? ""} placeholder={placeholder} onChange={event => importField(key, event.target.value)} /></label>)}
          {isXes ? <><label className="filters-field"><span>Tree alignment XES importer</span><select value={form.importFields.xes_variant ?? ""} onChange={event => importField("xes_variant", event.target.value)}><option value="">Native default</option>{WORKFLOW_INPUT_XES_VARIANTS.map(value => <option key={value}>{value}</option>)}</select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key] ?? ""} onChange={event => importField(key, event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}</>
            : <label className="filters-field"><span>Tree alignment CSV delimiter</span><select value={form.importFields.delimiter ?? ""} onChange={event => importField("delimiter", event.target.value)}><option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>}
        </div></details>}
        <div className="query-options">{TREE_ALIGNMENT_CONTROLS.map(control => <TreeAlignmentField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={!!form.explicitEmptySelectors[control.key]} onChange={value => field(control.key, value)} onEmptyChange={value => setForm(current => ({...current, explicitEmptySelectors: {...current.explicitEmptySelectors, [control.key]: value}, ...(value ? {fields: {...current.fields, [control.key]: ""}} : {})}))} />)}</div>
        <p>Blank controls use native defaults. Time controls are entered in milliseconds; the complete native options snapshot records seconds. Zero state/time bounds and zero approximation thresholds mean unlimited. Each completed result retains the files and request used for its run.</p>
        <button className="btn-open" disabled={!input || !model} onClick={() => void execute()}>Run independent tree alignment</button>
      </fieldset>
      {pending && <p role="status">Aligning original process-tree input… <button className="btn-ghost" onClick={cancel}>Cancel tree alignment request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {failed && <TreeAlignmentInspection key={failed.raw + ":failed"} run={failed} failed />}
    {completed && <TreeAlignmentInspection key={completed.raw + ":complete"} run={completed} />}
  </>;
}
function TreeAlignmentField({control, value, explicitEmpty, onChange, onEmptyChange}: {control: TreeAlignmentControl; value: string; explicitEmpty: boolean; onChange: (value: string) => void; onEmptyChange: (value: boolean) => void}) {
  return <div className="filters-field"><label><span>{control.label}</span>{control.kind === "boolean" ? <select value={value} onChange={event => onChange(event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select> : <input disabled={control.kind === "selector" && explicitEmpty} value={value} placeholder="Native default" onChange={event => onChange(event.target.value)} />}</label>
    {control.kind === "selector" && <label className="query-check"><input type="checkbox" checked={explicitEmpty} onChange={event => onEmptyChange(event.target.checked)} />Submit explicit empty {control.label.toLowerCase()}</label>}
  </div>;
}
function TreeAlignmentInspection({run, failed = false}: {run: ProcessTreeAlignmentRun; failed?: boolean}) {
  const [tableId, setTableId] = useState("complete_alignment_moves"), table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const graph = useMemo(() => processTreeAlignmentTreeGraph(run.value.model_context.native_tree), [run.value]);
  const request = nativeResponseRequest(run.value) ?? {path: "/api/align-process-tree-workbench", fields: run.fields}, prefix = `minera-tree-alignment-${run.value.variant.toLowerCase()}${failed ? "-incomplete" : ""}`;
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), `${prefix}-${suffix}`);
  return <>
    <ModelGraph graph={graph} title="Original normative process tree" />
    <section className="panel" aria-label={failed ? "Incomplete tree alignment inspection" : "Complete tree alignment inspection"}>
      <div className="panel-head"><h2>{failed ? "Incomplete or failed tree alignment attempt" : "Complete process-tree alignment result"}</h2><span className="panel-meta">{run.value.variant}</span></div>
      <p>{run.inputs.input.name} against {run.inputs.model.name} · {run.value.input_shape}. Checked cases: {run.value.input_context.checked_case_count === null ? "unknown" : String(nativeCell(run.value.input_context, "checked_case_count"))}; checked events: {run.value.input_context.checked_event_count === null ? "unknown" : String(nativeCell(run.value.input_context, "checked_event_count"))}.</p>
      <p role="status">{processTreeAlignmentSummary(run.value)}</p>
      {run.value.result?.diagnostics.message && <p>{run.value.result.diagnostics.message}</p>}
      <details><summary>Exact tree alignment request and original file hashes</summary><pre>{nativeJson({request, input_sha256: run.inputs.input_sha256, model_sha256: run.inputs.model_sha256}, 2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete tree alignment native JSON</button>
        <button className="btn-ghost" onClick={() => download(processTreeAlignmentReport(run), "report.json")}>Download tree alignment report</button>
        <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download tree alignment request</button>
        <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${processTreeAlignmentTableFileName(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete tree alignment table CSV</button>
        <button className="btn-ghost" onClick={() => download(processTreeAlignmentAllTablesCsv(run.tables), "all-tables.csv", "text/csv;charset=utf-8")}>Download all tree alignment tables CSV</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.input, run.inputs.input.name)}>Download original tree alignment input</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.model, run.inputs.model.name)}>Download original normative tree model</button>
        {(["ptml", "pnml", "bpmn", "source_xes"] as const).map(format => <button className="btn-ghost" key={format} disabled={run.value[format] === null} onClick={() => {const file = processTreeAlignmentArtifactFile(run, format); downloadQueryBlob(file, file.name);}}>Download this run’s {format === "source_xes" ? "checked source XES" : format.toUpperCase() + " model"}</button>)}
      </div>
      {(["input_log_error", "checked_log_error", "source_xes_error", "ptml_error", "pnml_error", "bpmn_error"] as const).map(key => run.value[key] && <p key={key}>{key.replace(/_/g, " ")}: {run.value[key]}</p>)}
      {run.value.conversion_context.error && <p>Petri conversion: {run.value.conversion_context.error}</p>}
      <label className="filters-field"><span>Tree alignment result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={table.id} table={table} noun="tree alignment" />
      <details><summary>Complete tree alignment native JSON</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
