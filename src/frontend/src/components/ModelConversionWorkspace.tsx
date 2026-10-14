import {useEffect, useMemo, useRef, useState} from "react";
import {
  MODEL_CONVERSION_ARTIFACTS, MODEL_CONVERSION_OPERATIONS, MODEL_CONVERSION_PNML_CONTROLS, MODEL_CONVERSION_POWL_EXAMPLE,
  MODEL_CONVERSION_REDUCTION_CONTROLS, MODEL_CONVERSION_TREE_EXAMPLE, ModelConversionRequestGate, defaultModelConversionForm,
  modelConversionAllTablesCsv, modelConversionArtifactFile, modelConversionFormats, modelConversionGraphs, modelConversionReport,
  modelConversionSummary, modelConversionTableFileName, runModelConversionWorkbench,
  type ModelConversionForm, type ModelConversionOperation, type ModelConversionRun,
} from "../model-conversion-workbench";
import {workflowInputNativeBytes} from "../workflow-input-workbench";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeCell, nativeJson} from "../native-json";
import {nativeResponseRequest} from "../native-response";
import {downloadQueryBlob} from "../log-query";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";
import {ModelGraph} from "./ModelGraph";

const labels: Record<ModelConversionOperation, string> = {tree_to_powl: "Process tree → POWL", net_to_powl: "Petri net → POWL", powl_to_tree: "POWL → process tree", net_to_tree: "Petri net → process tree", powl_to_net: "POWL → Petri net", reduce_petri: "Reduce a Petri net"};
const formatLabels = {auto: "Detect original model format", ptml: "PTML process tree", tree_json: "Full native process-tree JSON", pnml: "PNML Petri net", powl_text: "Native POWL text"};
interface OperationState {form: ModelConversionForm; model: File | null; returned: ModelConversionRun | null; failed: ModelConversionRun | null;}
const initialState = () => Object.fromEntries(MODEL_CONVERSION_OPERATIONS.map(operation => [operation, {form: defaultModelConversionForm(operation), model: null, returned: null, failed: null}])) as Record<ModelConversionOperation, OperationState>;
export function ModelConversionWorkspace() {
  const [operation, setOperation] = useState<ModelConversionOperation>("tree_to_powl"), [states, setStates] = useState(initialState);
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), controller = useRef<AbortController | null>(null), gate = useRef(new ModelConversionRequestGate()), activeOperation = useRef(operation);
  const identities = useRef<Record<ModelConversionOperation, File | null>>(Object.fromEntries(MODEL_CONVERSION_OPERATIONS.map(value => [value, null])) as Record<ModelConversionOperation, File | null>);
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null;}, []);
  const update = (change: (state: OperationState) => OperationState) => setStates(current => ({...current, [operation]: change(current[operation])}));
  const selected = states[operation], form = selected.form;
  const editForm = (change: (value: ModelConversionForm) => ModelConversionForm) => update(current => ({...current, form: change(current.form)}));
  const chooseModel = (model: File | null) => {cancel(); identities.current[operation] = model; update(current => ({...current, model, returned: null, failed: null})); setError(null);};
  const chooseOperation = (value: ModelConversionOperation) => {cancel(); activeOperation.current = value; setOperation(value); setError(null);};
  const execute = async () => {
    if (!selected.model) return; cancel(); const abort = new AbortController(), ticket = gate.current.issue(), model = selected.model, snapshot = structuredClone(form); controller.current = abort; setPending(true); setError(null);
    const accepts = () => activeOperation.current === snapshot.operation && identities.current[snapshot.operation] === model && controller.current === abort && gate.current.accepts(ticket, abort.signal);
    try {const result = await runModelConversionWorkbench(model, snapshot, abort.signal); if (accepts()) setStates(current => ({...current, [snapshot.operation]: {...current[snapshot.operation], ...(result.value.error === null ? {returned: result, failed: null} : {failed: result})}}));}
    catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  const pnml = operation.startsWith("net_") || operation === "reduce_petri", powl = operation.startsWith("powl_"), example = powl ? MODEL_CONVERSION_POWL_EXAMPLE : MODEL_CONVERSION_TREE_EXAMPLE;
  const download = (text: string, name: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), name);
  return <>
    <section className="panel" aria-label="Independent original model conversion and reduction">
      <div className="panel-head"><h2>Convert or reduce an original model</h2><span className="panel-meta">Process trees · POWL · Petri nets</span></div>
      <p>Upload an independent original model. Each operation keeps the original model, the complete native result and every available export from the same run.</p>
      <label className="filters-field"><span>Model operation</span><select value={operation} onChange={event => chooseOperation(event.target.value as ModelConversionOperation)}>{MODEL_CONVERSION_OPERATIONS.map(value => <option key={value} value={value}>{labels[value]}</option>)}</select></label>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Original {pnml ? "PNML" : powl ? "POWL text" : "process-tree"} model</span><input type="file" accept={pnml ? ".pnml,.xml,application/xml" : powl ? ".powl,.txt,text/plain" : ".ptml,.xml,.json,application/xml,application/json"} onChange={event => chooseModel(event.target.files?.[0] ?? null)} /><span>{selected.model?.name ?? "Choose an original model file"}</span></label>
          <label className="filters-field"><span>Original model format</span><select value={form.modelFormat} onChange={event => editForm(current => ({...current, modelFormat: event.target.value as ModelConversionForm["modelFormat"]}))}>{modelConversionFormats(operation).map(value => <option key={value} value={value}>{formatLabels[value]}</option>)}</select></label>
        </div>
        {!pnml && <details><summary>{powl ? "Native POWL text input" : "Full native process-tree input"}</summary><p>{powl ? "Upload the native POWL text grammar with literal Unicode labels. The complete POWL snapshot retains ordered children, stored order pairs, frequencies and the distinction between an empty visible label and a silent node." : "Upload PTML or the full process-tree constructor JSON. The activity registry, child order, operator indexes and stored activity indexes are retained."}</p><pre>{example}</pre><button className="btn-ghost" onClick={() => download(example, `minera-model-conversion-example.${powl ? "powl" : "json"}`, powl ? "text/plain;charset=utf-8" : "application/json")}>Download original model example</button></details>}
        {pnml && <details><summary>PNML import options</summary><div className="query-options">{MODEL_CONVERSION_PNML_CONTROLS.map(([key, label, enabled]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.pnmlFields[key] ?? ""} onChange={event => editForm(current => ({...current, pnmlFields: {...current.pnmlFields, [key]: event.target.value}}))}><option value="">Native default ({String(enabled)})</option><option value="true">true</option><option value="false">false</option></select></label>)}</div></details>}
        {operation === "reduce_petri" && <details open><summary>Petri-net reduction options</summary><p>Omitted markings use the original PNML markings after signed integer range checks. An explicit [] leaves that marking unspecified for reduction. Signed values and complete mapping indexes remain inspectable.</p><div className="query-options">{MODEL_CONVERSION_REDUCTION_CONTROLS.map(control => <label className="filters-field" key={control.key}><span>{control.label}</span>{control.kind === "boolean" ? <select value={form.fields[control.key] ?? ""} onChange={event => editForm(current => ({...current, fields: {...current.fields, [control.key]: event.target.value}}))}><option value="">Native default (false)</option><option value="true">true</option><option value="false">false</option></select> : <textarea rows={3} value={form.fields[control.key] ?? ""} placeholder={control.kind === "marking" ? "Omit, or [] / [1,0,-1]" : "Omit, or 1,0,-1"} onChange={event => editForm(current => ({...current, fields: {...current.fields, [control.key]: event.target.value}}))} />}</label>)}</div></details>}
        {operation === "powl_to_tree" && <p>POWL-to-tree conversion uses a deterministic block-structured approximation for partial orders. Keep the original POWL model when its exact order relationships matter.</p>}
        {operation === "net_to_tree" && <p>Petri-net-to-tree conversion requires an ordinary workflow net that decomposes exactly into supported blocks. The native converter can reject residual partial orders.</p>}
        {operation === "reduce_petri" && <p>The returned changed flag describes reduction. The resulting net is inspected separately because rebuilding can alter native metadata even when changed is false.</p>}
        <div className="query-actions"><button className="btn-ghost" disabled={!selected.model} onClick={() => void execute()}>{operation === "reduce_petri" ? "Reduce original Petri net" : "Convert original model"}</button></div>
      </fieldset>
      {pending && <p role="status">Processing the original model… <button className="btn-ghost" onClick={cancel}>Cancel model conversion request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {selected.failed && <ModelConversionInspection key={selected.failed.raw + ":failed"} run={selected.failed} failed />}
    {selected.returned && <ModelConversionInspection key={selected.returned.raw + ":returned"} run={selected.returned} />}
  </>;
}
function ModelConversionInspection({run, failed = false}: {run: ModelConversionRun; failed?: boolean}) {
  const [tableId, setTableId] = useState(failed ? "/model" : run.value.result?.reduction ? "complete_old_to_new_place" : "/result"), table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const graphs = useMemo(() => modelConversionGraphs(run.value), [run.value]), request = nativeResponseRequest(run.value) ?? {path: "/api/model-conversion-workbench", fields: run.fields}, prefix = `minera-${run.value.operation}${failed ? "-failed" : ""}`;
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), `${prefix}-${suffix}`);
  return <>
    {graphs.map(({title, graph}) => <ModelGraph key={title} graph={graph} title={title} />)}
    <section className="panel" aria-label={failed ? "Failed model conversion inspection" : "Returned model conversion inspection"}>
      <div className="panel-head"><h2>{failed ? "Failed model operation" : "Returned model operation"}</h2><span className="panel-meta">{labels[run.value.operation]}</span></div>
      <p>{run.input.model.name} · {run.value.input_context.detected_format} · {String(nativeCell(run.value.input_context, "original_model_bytes"))} original bytes. Original model unchanged: {String(run.value.input_context.model_unchanged)}.</p>
      <p role="status">{modelConversionSummary(run.value)}</p>
      <details><summary>Exact model request and original file hash</summary><pre>{nativeJson({request, model_sha256: run.input.model_sha256}, 2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete model native JSON</button>
        <button className="btn-ghost" onClick={() => download(modelConversionReport(run), "report.json")}>Download model operation report</button>
        <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download exact model request</button>
        <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${modelConversionTableFileName(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete model table CSV</button>
        <button className="btn-ghost" onClick={() => download(modelConversionAllTablesCsv(run.tables), "all-tables.csv", "text/csv;charset=utf-8")}>Download all model tables CSV</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.input.model, run.input.model.name)}>Download original model bytes</button>
        {MODEL_CONVERSION_ARTIFACTS.map(artifact => <button className="btn-ghost" key={artifact} disabled={run.value.exports[artifact] === null} onClick={() => {const file = modelConversionArtifactFile(run, artifact); downloadQueryBlob(file, file.name);}}>Download {artifact.replace("_", " ").toUpperCase()}</button>)}
      </div>
      {MODEL_CONVERSION_ARTIFACTS.map(artifact => run.value.exports[`${artifact}_error`] && <p key={artifact}>{artifact.replace(/_/g, " ")} export: {run.value.exports[`${artifact}_error`]}</p>)}
      <label className="filters-field"><span>Complete model inspection table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={table.id} table={table} noun="model operation" />
      <details><summary>Complete model operation native JSON</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
