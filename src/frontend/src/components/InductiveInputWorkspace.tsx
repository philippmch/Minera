import {miningSourceWorkbenchFields, type ActiveMiningSourceDescriptor} from "../active-mining-source";
import {useEffect, useMemo, useRef, useState} from "react";
import {
  INDUCTIVE_INPUT_CONTROLS, INDUCTIVE_INPUT_CONVERSIONS, INDUCTIVE_INPUT_FORMATS, INDUCTIVE_INPUT_SHAPES, INDUCTIVE_INPUT_VARIANTS,
  INDUCTIVE_NATIVE_DFG_EXAMPLE, INDUCTIVE_WEIGHTED_VARIANTS_EXAMPLE, InductiveInputRequestGate, defaultInductiveInputForm,
  inductiveInputAllTablesCsv, inductiveInputArtifactFile, inductiveInputReport, inductiveInputSummary,
  inductiveInputTableFileName, inductiveInputTreeGraph, runInductiveInput,
  type InductiveInputControl, type InductiveInputForm, type InductiveInputRun,
} from "../inductive-input-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../token-replay-workbench";
import {WORKFLOW_INPUT_TABLE_EXAMPLE, WORKFLOW_INPUT_XES_VARIANTS, workflowInputNativeBytes} from "../workflow-input-workbench";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeJson} from "../native-json";
import {nativeResponseRequest} from "../native-response";
import {downloadQueryBlob} from "../log-query";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";
import {ModelGraph} from "./ModelGraph";

const variantLabels = {log: "IM · log recursion", imf: "IMf · infrequent", dfg: "IMd · graph recursion"};
const shapeLabels = {log: "Grouped log", stream: "Event stream", dataframe: "Dataframe", lazyframe: "Lazyframe", dfg: "Direct native DFG", variants: "Compact weighted variants"};
const formatLabels = {auto: "CSV / TSV / XES / gzip XES", table: "Typed EventTable JSON", dfg_json: "Native DFG JSON", variants_json: "Weighted variants JSON"};
export function InductiveInputWorkspace({file: original = null, source = null}: {file?: File | null; source?: ActiveMiningSourceDescriptor | null}) {
  const [input, setInput] = useState<File | null>(original), [form, setForm] = useState(defaultInductiveInputForm);
  const [completed, setCompleted] = useState<InductiveInputRun | null>(null), [failed, setFailed] = useState<InductiveInputRun | null>(null);
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null), gate = useRef(new InductiveInputRequestGate()), identity = useRef<File | null>(original);
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const chooseInput = (file: File | null) => {cancel(); identity.current = file; setInput(file); setCompleted(null); setFailed(null); setError(null);};
  const adoptSource = () => {chooseInput(original); if (source) {const mapped = miningSourceWorkbenchFields(source, {case_id_key: "inductive_case_id_key", activity_key: "inductive_activity_key", timestamp_key: "inductive_timestamp_key"}); setForm(current => ({...current, format: source.format, shape: source.shape, fields: {...current.fields, ...mapped.fields}, explicitEmptySelectors: {...current.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields}));}};
  useEffect(() => {adoptSource();}, [original, source]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null;}, []);
  const field = (key: string, value: string) => setForm(current => ({...current, fields: {...current.fields, [key]: value}}));
  const importField = (key: string, value: string) => setForm(current => ({...current, importFields: {...current.importFields, [key]: value}}));
  const shape = (value: InductiveInputForm["shape"]) => setForm(current => ({...current, shape: value, format: value === "dfg" ? "dfg_json" : value === "variants" ? "variants_json" : ["dfg_json", "variants_json"].includes(current.format) ? "table" : current.format}));
  const format = (value: InductiveInputForm["format"]) => setForm(current => ({...current, format: value, shape: value === "dfg_json" ? "dfg" : value === "variants_json" ? "variants" : ["dfg", "variants"].includes(current.shape) ? "dataframe" : current.shape}));
  const execute = async () => {
    if (!input) return; cancel(); const abort = new AbortController(), ticket = gate.current.issue(), file = input, snapshot = structuredClone(form);
    controller.current = abort; setPending(true); setError(null);
    const accepts = () => identity.current === file && controller.current === abort && gate.current.accepts(ticket, abort.signal);
    try {
      const result = await runInductiveInput(file, snapshot, abort.signal);
      if (accepts()) {if (result.value.computed) {setCompleted(result); setFailed(null);} else {setFailed(result); setError(`${result.value.error!.type}: ${result.value.error!.message}`);}}
    } catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  const isXes = /\.(?:xes|xes\.gz|gz)$/i.test(input?.name ?? "");
  const download = (text: string, name: string) => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type: "application/json"}), name);
  return <>
    <section className="panel" aria-label="Independent Inductive input discovery">
      <div className="panel-head"><h2>Discover an Inductive workflow</h2><span className="panel-meta">Independent IM, IMf and IMd discovery</span></div>
      <p>Upload an original log, a typed event table, compact weighted variants or a native DFG. Choose the input adapter and discovery options, then inspect the complete native tree and every model export.</p>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Independent Inductive input file</span><input type="file" accept={form.format === "auto" ? ".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" : ".json,application/json"} onChange={event => chooseInput(event.target.files?.[0] ?? null)} /><span>{input?.name ?? "Choose an original input file"}</span></label>
          <label className="filters-field"><span>Independent Inductive variant</span><select value={form.variant} onChange={event => setForm(current => ({...current, variant: event.target.value as InductiveInputForm["variant"]}))}>{INDUCTIVE_INPUT_VARIANTS.map(value => <option key={value} value={value}>{variantLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Independent Inductive input shape</span><select value={form.shape} onChange={event => shape(event.target.value as InductiveInputForm["shape"])}>{INDUCTIVE_INPUT_SHAPES.map(value => <option key={value} value={value}>{shapeLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Independent Inductive input format</span><select value={form.format} onChange={event => format(event.target.value as InductiveInputForm["format"])}>{INDUCTIVE_INPUT_FORMATS.map(value => <option key={value} value={value}>{formatLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Discovered-tree Petri conversion</span><select value={form.conversion} onChange={event => setForm(current => ({...current, conversion: event.target.value as InductiveInputForm["conversion"]}))}>{INDUCTIVE_INPUT_CONVERSIONS.map(value => <option key={value} value={value}>{value === "standard" ? "Standard" : "Transition bordered"}</option>)}</select></label>
        </div>
        {original && <button className="btn-ghost" onClick={adoptSource}>Use current original log for Inductive discovery</button>}
        {form.shape === "dfg" ? <p>Direct graph input uses native IMd graph recursion. The requested variant and configured options remain in the result alongside the effective discovery behavior.</p> : form.shape === "variants" ? <p>Weighted variants go directly to the compact native overload. Counts retain exact unsigned 64-bit values; rows, repeated words and zero counts remain in the source carrier.</p> : form.shape === "log" ? <p>Grouped-log discovery retains imported trace boundaries and event order by default. Custom case and timestamp selectors follow the grouped-log preparation rules.</p> : <p>Event streams, dataframes and lazyframes group by the exact case selector and use stable timestamp ordering. Selected activity, case and timestamp fields must satisfy native flat-input validation.</p>}
        {form.format === "table" && <details><summary>Typed Inductive event-table JSON</summary><p>The original table preserves every cell, dtype, index, category and metadata field. Missing cells, present nulls and empty values remain distinct.</p><pre>{WORKFLOW_INPUT_TABLE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(WORKFLOW_INPUT_TABLE_EXAMPLE, "minera-inductive-input-table.json")}>Download Inductive table example</button></details>}
        {form.format === "variants_json" && <details open><summary>Compact weighted-variant JSON</summary><p>Upload a bare array of records with activities as an ordered string array and count as an unsigned integer. Repeated records, empty words, literal empty activities and zero counts retain their native input meaning.</p><pre>{INDUCTIVE_WEIGHTED_VARIANTS_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(INDUCTIVE_WEIGHTED_VARIANTS_EXAMPLE, "minera-inductive-weighted-variants.json")}>Download weighted Inductive variant example</button></details>}
        {form.format === "dfg_json" && <details open><summary>Complete native DFG constructor JSON</summary><p>Upload an ordered unique activities registry, ordered edges with source and target indices and unsigned counts, plus aligned start_counts and end_counts arrays. Zero-valued edges and unused activity identities remain in the supplied graph.</p><pre>{INDUCTIVE_NATIVE_DFG_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(INDUCTIVE_NATIVE_DFG_EXAMPLE, "minera-inductive-native-dfg.json")}>Download native Inductive DFG example</button></details>}
        {form.format === "auto" && <details><summary>Independent Inductive import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key]) => isXes ? key.startsWith("xes_") : !key.startsWith("xes_")).map(([key, label, placeholder]) => <label className="filters-field" key={key}><span>{label}</span><input value={form.importFields[key] ?? ""} placeholder={placeholder} onChange={event => importField(key, event.target.value)} /></label>)}
          {isXes ? <><label className="filters-field"><span>Independent Inductive XES importer</span><select value={form.importFields.xes_variant ?? ""} onChange={event => importField("xes_variant", event.target.value)}><option value="">Native default</option>{WORKFLOW_INPUT_XES_VARIANTS.map(value => <option key={value}>{value}</option>)}</select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key] ?? ""} onChange={event => importField(key, event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}</>
            : <label className="filters-field"><span>Independent Inductive CSV delimiter</span><select value={form.importFields.delimiter ?? ""} onChange={event => importField("delimiter", event.target.value)}><option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>}
        </div></details>}
        <div className="query-options">{INDUCTIVE_INPUT_CONTROLS.map(control => <InductiveInputField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={!!form.explicitEmptySelectors[control.key]} onChange={value => field(control.key, value)} onEmptyChange={value => setForm(current => ({...current, explicitEmptySelectors: {...current.explicitEmptySelectors, [control.key]: value}, ...(value ? {fields: {...current.fields, [control.key]: ""}} : {})}))} />)}</div>
        <p>Blank settings use native defaults. Literal selector spellings and numeric text are submitted as entered. Noise filtering affects IMf. Each completed result retains its original file, request and options.</p>
        <button className="btn-open" disabled={!input} onClick={() => void execute()}>Discover independent Inductive workflow</button>
      </fieldset>
      {pending && <p role="status">Discovering independent Inductive workflow… <button className="btn-ghost" onClick={cancel}>Cancel Inductive discovery request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {failed && <InductiveInputInspection key={failed.raw + ":failed"} run={failed} failed />}
    {completed && <InductiveInputInspection key={completed.raw + ":complete"} run={completed} />}
  </>;
}
function InductiveInputField({control, value, explicitEmpty, onChange, onEmptyChange}: {control: InductiveInputControl; value: string; explicitEmpty: boolean; onChange: (value: string) => void; onEmptyChange: (value: boolean) => void}) {
  return <div className="filters-field"><label><span>{control.label}</span>{control.kind === "boolean" ? <select value={value} onChange={event => onChange(event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select> : <input disabled={control.kind === "selector" && explicitEmpty} value={value} placeholder="Native default" onChange={event => onChange(event.target.value)} />}</label>
    {control.kind === "selector" && <label className="query-check"><input type="checkbox" checked={explicitEmpty} onChange={event => onEmptyChange(event.target.checked)} />Submit explicit empty {control.label.toLowerCase()}</label>}
  </div>;
}
function InductiveInputInspection({run, failed = false}: {run: InductiveInputRun; failed?: boolean}) {
  const [tableId, setTableId] = useState(failed ? "/input_after/rows" : "complete_tree_nodes"), table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const graph = useMemo(() => run.value.computed && run.value.native_tree ? inductiveInputTreeGraph(run.value.native_tree) : null, [run.value]);
  const request = nativeResponseRequest(run.value) ?? {path: "/api/discover-inductive-input", fields: run.fields}, prefix = `minera-inductive-${run.value.variant.toLowerCase()}${failed ? "-failed" : ""}`;
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), `${prefix}-${suffix}`);
  return <>
    {graph && <ModelGraph graph={graph} title={`Independent ${run.value.discovery_context.effective_variant} process tree`} />}
    <section className="panel" aria-label={failed ? "Complete failed Inductive input inspection" : "Complete independent Inductive input inspection"}>
      <div className="panel-head"><h2>{failed ? "Failed Inductive discovery attempt" : "Complete native Inductive result"}</h2><span className="panel-meta">{run.input.name}</span></div>
      <p>{inductiveInputSummary(run.value)}</p>
      {run.value.result_error && <p>Supplemental event-log analysis: {run.value.result_error}. The complete native model remains available.</p>}
      <details><summary>Exact Inductive request and original input hash</summary><pre>{nativeJson({request, input: {name: run.input.name, bytes: run.input.size, last_modified: run.input.lastModified, sha256: run.input_sha256}}, 2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete Inductive native JSON</button>
        <button className="btn-ghost" onClick={() => download(inductiveInputReport(run), "report.json")}>Download Inductive input report</button>
        <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download Inductive input request</button>
        <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${inductiveInputTableFileName(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete Inductive table CSV</button>
        <button className="btn-ghost" onClick={() => download(inductiveInputAllTablesCsv(run.tables), "all-tables.csv", "text/csv;charset=utf-8")}>Download all Inductive tables CSV</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.input, run.input.name)}>Download original Inductive input</button>
        {(["pnml", "bpmn", "ptml", "source_xes"] as const).map(format => <button key={format} className="btn-ghost" disabled={run.value[format] === null} onClick={() => {const file = inductiveInputArtifactFile(run, format); downloadQueryBlob(file, file.name);}}>Download this run’s {format === "source_xes" ? "prepared source XES" : format.toUpperCase() + " model"}</button>)}
      </div>
      {(["input_log_error", "prepared_log_error", "source_xes_error", "pnml_error", "bpmn_error", "ptml_error"] as const).map(key => run.value[key] && <p key={key}>{key.replace(/_/g, " ")}: {run.value[key]}</p>)}
      {run.value.conversion_context.error && <p>Petri conversion: {run.value.conversion_context.error}</p>}
      <label className="filters-field"><span>Independent Inductive result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={table.id} table={table} noun="discovery" />
      <details><summary>Complete independent Inductive native JSON</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
