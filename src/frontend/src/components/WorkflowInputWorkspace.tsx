import {miningSourceWorkbenchFields, type ActiveMiningSourceDescriptor} from "../active-mining-source";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  WORKFLOW_INPUT_CONTROLS, WORKFLOW_INPUT_FORMATS, WORKFLOW_INPUT_SHAPES, WORKFLOW_INPUT_TABLE_EXAMPLE, WORKFLOW_INPUT_XES_VARIANTS,
  WorkflowInputRequestGate, defaultWorkflowInputForm, runWorkflowInput, workflowInputAllTablesCsv, workflowInputControlActive,
  workflowInputDfgExample, workflowInputGraph, workflowInputModelFile, workflowInputNativeBytes, workflowInputReport, workflowInputSummary,
  type WorkflowInputControl, type WorkflowInputFamily, type WorkflowInputForm, type WorkflowInputFormat, type WorkflowInputRun, type WorkflowInputShape,
} from "../workflow-input-workbench";
import { TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS } from "../token-replay-workbench";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeJson } from "../native-json";
import { nativeResponseRequest } from "../native-response";
import { downloadQueryBlob } from "../log-query";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";
import { ModelGraph } from "./ModelGraph";

const SHAPE_LABELS: Record<WorkflowInputShape, string> = {log: "Grouped log", stream: "Event stream", dataframe: "Dataframe", lazyframe: "Lazyframe", dfg: "Direct structured DFG", pandas_mutable: "Mutable Heuristics++ pandas table"};
const FORMAT_LABELS: Record<WorkflowInputFormat, string> = {auto: "CSV / TSV / XES / gzip XES", table: "Typed EventTable JSON", dfg_json: "Structured DFG JSON"};
export function WorkflowInputWorkspace({file: original = null, source = null}: {file?: File | null; source?: ActiveMiningSourceDescriptor | null}) {
  const [input, setInput] = useState<File | null>(original), [form, setForm] = useState(defaultWorkflowInputForm);
  const [completed, setCompleted] = useState<WorkflowInputRun | null>(null), [failed, setFailed] = useState<WorkflowInputRun | null>(null);
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null), gate = useRef(new WorkflowInputRequestGate()), inputIdentity = useRef<File | null>(original);
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const chooseInput = (value: File | null) => {cancel(); inputIdentity.current = value; setInput(value); setCompleted(null); setFailed(null); setError(null);};
  const adoptSource = () => {chooseInput(original); if (source) {const mapped = miningSourceWorkbenchFields(source, {case_id_key: ["alpha_case_id_attribute", "heuristic_case_id_attribute"], activity_key: ["alpha_activity_attribute", "heuristic_activity_attribute"], timestamp_key: ["alpha_timestamp_attribute", "heuristic_timestamp_attribute"]}); setForm(current => ({...current, format: source.format, shape: source.shape, fields: {...current.fields, ...mapped.fields}, explicitEmptySelectors: {...current.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields}));}};
  useEffect(() => {adoptSource();}, [original, source]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null;}, []);
  const field = (key: string, value: string) => setForm(current => ({...current, fields: {...current.fields, [key]: value}}));
  const importField = (key: string, value: string) => setForm(current => ({...current, importFields: {...current.importFields, [key]: value}}));
  const family = (value: WorkflowInputFamily) => setForm(current => ({...current, family: value, ...(current.shape === "pandas_mutable" && value === "alpha" ? {shape: "dataframe" as const} : {})}));
  const shape = (value: WorkflowInputShape) => setForm(current => ({...current, shape: value,
    ...(value === "dfg" ? {format: "dfg_json" as const, alphaVariant: "classic" as const, heuristicVariant: "classic" as const} : current.format === "dfg_json" ? {format: "table" as const} : {}),
    ...(value === "pandas_mutable" ? {family: "heuristic" as const, heuristicVariant: "plusplus" as const} : {}),
  }));
  const format = (value: WorkflowInputFormat) => setForm(current => ({...current, format: value,
    ...(value === "dfg_json" ? {shape: "dfg" as const, alphaVariant: "classic" as const, heuristicVariant: "classic" as const} : current.shape === "dfg" ? {shape: "dataframe" as const} : {}),
  }));
  const execute = async () => {
    if (!input) return;
    cancel(); const active = new AbortController(), ticket = gate.current.issue(), selected = input, snapshot = structuredClone(form);
    controller.current = active; setPending(true); setError(null);
    const accepts = () => inputIdentity.current === selected && controller.current === active && gate.current.accepts(ticket, active.signal);
    try {
      const result = await runWorkflowInput(selected, snapshot, active.signal);
      if (accepts()) {
        if (result.value.computed) {setCompleted(result); setFailed(null);} else {setFailed(result); setError(`${result.value.error!.type}: ${result.value.error!.message}`);}
      }
    } catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === active) {controller.current = null; setPending(false);}}
  };
  const isXes = /\.(?:xes|xes\.gz|gz)$/i.test(input?.name ?? "");
  const controls = WORKFLOW_INPUT_CONTROLS[form.family].filter(control => workflowInputControlActive(control, form));
  const download = (text: string, name: string) => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type: "application/json"}), name);
  return <>
    <section className="panel" aria-label="Independent workflow input discovery">
      <div className="panel-head"><h2>Discover a workflow from original inputs</h2><span className="panel-meta">Independent Alpha and Heuristic discovery</span></div>
      <p>Upload an original log, a typed event table or a structured DFG. Choose the native input path, then inspect its complete model, table state and exports.</p>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Independent discovery input file</span><input type="file" accept={form.format === "auto" ? ".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" : ".json,application/json"} onChange={event => chooseInput(event.target.files?.[0] ?? null)} /><span>{input?.name ?? "Choose an original input file"}</span></label>
          <label className="filters-field"><span>Independent discovery family</span><select value={form.family} onChange={event => family(event.target.value as WorkflowInputFamily)}><option value="alpha">Alpha</option><option value="heuristic">Heuristic</option></select></label>
          <label className="filters-field"><span>Independent discovery variant</span><select value={form.family === "alpha" ? form.alphaVariant : form.heuristicVariant} disabled={form.shape === "dfg" || form.shape === "pandas_mutable"} onChange={event => setForm(current => current.family === "alpha" ? {...current, alphaVariant: event.target.value as WorkflowInputForm["alphaVariant"]} : {...current, heuristicVariant: event.target.value as WorkflowInputForm["heuristicVariant"]})}>
            <option value="classic">Classic</option><option value={form.family === "alpha" ? "plus" : "plusplus"}>{form.family === "alpha" ? "Alpha+" : "Heuristics++"}</option>
          </select></label>
          <label className="filters-field"><span>Independent discovery input shape</span><select value={form.shape} onChange={event => shape(event.target.value as WorkflowInputShape)}>{WORKFLOW_INPUT_SHAPES.map(value => <option key={value} value={value}>{SHAPE_LABELS[value]}</option>)}</select></label>
          <label className="filters-field"><span>Independent discovery input format</span><select value={form.format} onChange={event => format(event.target.value as WorkflowInputFormat)}>{WORKFLOW_INPUT_FORMATS.map(value => <option key={value} value={value}>{FORMAT_LABELS[value]}</option>)}</select></label>
        </div>
        {original && <button className="btn-ghost" onClick={adoptSource}>Use current original log</button>}
        <InputGuide form={form} />
        {form.format === "table" && <details><summary>Typed EventTable JSON example and fields</summary>
          <p>The table retains ordered columns and rows, declared dtypes, index labels, categorical dtypes, attributes and metadata. A null row cell is missing; a typed cell with null:true is a present null. Integer cells can declare integer_storage as int64 or uint64. Typed float values retain native NaN/INF/-INF strings.</p>
          <pre>{WORKFLOW_INPUT_TABLE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(WORKFLOW_INPUT_TABLE_EXAMPLE, "minera-discovery-event-table-example.json")}>Download typed discovery table example</button>
        </details>}
        {form.format === "dfg_json" && <details open><summary>Structured {form.family === "alpha" ? "Alpha" : "Heuristic"} DFG JSON input</summary>
          {form.family === "alpha" ? <p>Choose kind sparse, clean or graph. Every DFG row has source, target and an unsigned count. Sparse input infers boundaries when both maps are omitted; explicitly empty start_activities and end_activities suppress inference. Clean requires both maps. Graph requires an ordered unique activities registry and both maps.</p> : <p>DFG rows have source, target and an unsigned count. Optional fields include activities, activity_counts, start_counts, end_counts, window_two_dfg, frequency_triples, performance_dfg, start_activities, end_activities, dfg_window_2_supplied, freq_triples_supplied, dfg_order and performance_values. Omitted fields and explicit empty containers retain their distinct native behavior.</p>}
          <pre>{workflowInputDfgExample(form.family)}</pre><button className="btn-ghost" onClick={() => download(workflowInputDfgExample(form.family), `minera-discovery-${form.family}-dfg-example.json`)}>Download {form.family} DFG example</button>
          {form.family === "heuristic" && <p>frequency_triples uses source, via, target and count. performance_dfg uses source, target and samples. performance_values uses source, target and value with type scalar, samples, summary or structured. Summary fields are mean, median, minimum, maximum, stdev and sum. Structured values use recursive type/value envelopes: null, boolean, int64, uint64, float, string, array or object; float accepts NaN/INF/-INF strings.</p>}
        </details>}
        {form.format === "auto" && <details><summary>Independent discovery import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key]) => isXes ? key.startsWith("xes_") : !key.startsWith("xes_")).map(([key, label, placeholder]) => <label className="filters-field" key={key}><span>{label}</span><input value={form.importFields[key] ?? ""} placeholder={placeholder} onChange={event => importField(key, event.target.value)} /></label>)}
          {isXes ? <>
            <label className="filters-field"><span>Independent discovery XES importer</span><select value={form.importFields.xes_variant ?? ""} onChange={event => importField("xes_variant", event.target.value)}><option value="">Native default</option>{WORKFLOW_INPUT_XES_VARIANTS.map(value => <option key={value}>{value}</option>)}</select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key] ?? ""} onChange={event => importField(key, event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}
          </> : <label className="filters-field"><span>Independent discovery CSV delimiter</span><select value={form.importFields.delimiter ?? ""} onChange={event => importField("delimiter", event.target.value)}><option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>}
        </div></details>}
        <div className="query-options">{controls.map(control => <WorkflowInputField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={!!form.explicitEmptySelectors[control.key]} onChange={value => field(control.key, value)} onEmptyChange={value => setForm(current => ({...current, explicitEmptySelectors: {...current.explicitEmptySelectors, [control.key]: value}, ...(value ? {fields: {...current.fields, [control.key]: ""}} : {})}))} />)}</div>
        <p>Blank controls use native defaults. Numeric and JSON text is submitted as entered. For a literal empty selector, choose its explicit-empty option. Completed results retain their original input file and options.</p>
        <button className="btn-open" disabled={!input} onClick={() => void execute()}>Discover independent workflow</button>
      </fieldset>
      {pending && <p role="status">Discovering independent workflow… <button className="btn-ghost" onClick={cancel}>Cancel independent discovery request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {failed && <WorkflowInputInspection key={failed.raw + ":" + failed.input_sha256} run={failed} failed />}
    {completed && <WorkflowInputInspection key={completed.raw + ":" + completed.input_sha256} run={completed} />}
  </>;
}
function WorkflowInputField({control, value, explicitEmpty, onChange, onEmptyChange}: {control: WorkflowInputControl; value: string; explicitEmpty: boolean; onChange: (value: string) => void; onEmptyChange: (value: boolean) => void}) {
  return <div className="filters-field"><label><span>{control.label}</span>
    {control.kind === "boolean" || control.kind === "enum" ? <select value={value} onChange={event => onChange(event.target.value)}><option value="">Native default</option>{(control.kind === "boolean" ? ["true", "false"] : control.choices ?? []).map(choice => <option key={choice} value={choice}>{control.kind === "boolean" ? choice === "true" ? "On" : "Off" : choice}</option>)}</select> : control.kind === "json" ? <textarea rows={3} value={value} placeholder="Native omission, or any JSON value" onChange={event => onChange(event.target.value)} /> : <input disabled={control.kind === "selector" && explicitEmpty} value={value} placeholder="Native default" onChange={event => onChange(event.target.value)} />}
  </label>{control.kind === "selector" && <label className="query-check"><input type="checkbox" checked={explicitEmpty} onChange={event => onEmptyChange(event.target.checked)} />Submit explicit empty {control.label.toLowerCase()}</label>}</div>;
}
function InputGuide({form}: {form: WorkflowInputForm}) {
  if (form.shape === "dfg") return <p>Direct DFG input is a Classic-only native path. Its boundaries, optional count maps, ordering and typed performance values come from the uploaded JSON.</p>;
  if (form.shape === "pandas_mutable") return <p>This dedicated Heuristics++ pandas path mutates the native table. Declared timestamp dtypes are authoritative. The response retains the complete table before and after evaluation, including a newly created start column or @@diff column when a later native operation fails.</p>;
  if (form.family === "alpha") return <p>Alpha Classic table discovery can sort its footprint by case/start/completion while retaining original per-case endpoints. Alpha+, event streams and grouped logs retain native case order. Live and detached Alpha+ markings are inspected separately.</p>;
  return <p>Grouped logs retain trace boundaries; flat inputs use the selected case attribute. The const-table Heuristics++ path remains distinct from mutable pandas evaluation. Every returned frequency, performance value, matrix, bundle and metadata entry remains available in the complete inspection.</p>;
}
function WorkflowInputInspection({run, failed = false}: {run: WorkflowInputRun; failed?: boolean}) {
  const [tableId, setTableId] = useState(failed ? "input_after_cells" : run.value.method === "alpha" && run.value.alpha_discovery ? "/alpha_discovery/places" : run.value.result ? "/result" : run.value.native_net ? "/native_net/transitions" : "/native_heuristics");
  const table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const graph = useMemo(() => workflowInputGraph(run.value), [run.value]);
  const request = nativeResponseRequest(run.value) ?? {path: "/api/discover-workflow-input", fields: run.fields};
  const prefix = `minera-input-${run.value.method}-${run.value.variant}${failed ? "-failed" : ""}`;
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), `${prefix}-${suffix}`);
  return <>
    {graph && <ModelGraph graph={graph} title={`Independent ${run.value.method} · ${run.value.variant} workflow`} />}
    <section className="panel" aria-label={failed ? "Complete failed workflow input inspection" : "Complete independent workflow input inspection"}>
      <div className="panel-head"><h2>{failed ? "Failed discovery attempt and mutable input state" : "Complete independent workflow result"}</h2><span className="panel-meta">{run.input.name}</span></div>
      <p>{workflowInputSummary(run)}</p>
      {run.value.result_error && <p>Native discovery completed; the analysis projection is unavailable: {run.value.result_error}. The complete structural net and native results remain inspectable.</p>}
      {failed && <p>The native failure retains this attempt’s original file, request and complete input tables.</p>}
      <details><summary>Submitted independent discovery request and original file hash</summary><pre>{nativeJson({request, input: {name: run.input.name, bytes: run.input.size, last_modified: run.input.lastModified, sha256: run.input_sha256}}, 2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete independent discovery native JSON</button>
        <button className="btn-ghost" onClick={() => download(workflowInputReport(run), "report.json")}>Download independent discovery report</button>
        <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download independent discovery request</button>
        <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${encodeURIComponent(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete independent discovery table CSV</button>
        <button className="btn-ghost" onClick={() => download(workflowInputAllTablesCsv(run), "all-tables.csv", "text/csv;charset=utf-8")}>Download all independent discovery tables CSV</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.input, run.input.name)}>Download original discovery input</button>
        {(["pnml", "bpmn", "ptml"] as const).map(format => <button key={format} className="btn-ghost" disabled={run.value[format] === null} onClick={() => {const file = workflowInputModelFile(run, format); downloadQueryBlob(file, file.name);}}>Download this discovered model {format.toUpperCase()}</button>)}
      </div>
      {(["pnml", "bpmn", "ptml"] as const).map(format => run.value[`${format}_error`] && <p key={format}>{format.toUpperCase()}: {run.value[`${format}_error`]}</p>)}
      <label className="filters-field"><span>Independent discovery result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={table.id} table={table} noun="discovery" />
      <details><summary>Complete independent discovery native result JSON</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
