import {miningSourceWorkbenchFields, type ActiveMiningSourceDescriptor} from "../active-mining-source";
import {useEffect, useMemo, useRef, useState} from "react";
import {
  PETRI_ANTI_MULTI_ALGORITHMS, PETRI_ANTI_MULTI_CONTROLS, PETRI_ANTI_MULTI_INPUT_FORMATS, PETRI_ANTI_MULTI_INPUT_SHAPES,
  PETRI_ANTI_MULTI_MODEL_FORMATS, PETRI_ANTI_MULTI_PNML_CONTROLS, PETRI_ANTI_MULTI_TRACE_EXAMPLE, PETRI_ANTI_MULTI_VARIANT,
  PetriAntiMultiRequestGate, defaultPetriAntiMultiForm, petriAntiMultiAllTablesCsv, petriAntiMultiArtifactFile,
  petriAntiMultiGraph, petriAntiMultiReport, petriAntiMultiSummary, petriAntiMultiTableFileName, runPetriAntiMultiWorkbench,
  type PetriAntiMultiAlgorithm, type PetriAntiMultiControl, type PetriAntiMultiForm, type PetriAntiMultiRun,
} from "../petri-anti-multi-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../token-replay-workbench";
import {WORKFLOW_INPUT_TABLE_EXAMPLE, WORKFLOW_INPUT_XES_VARIANTS, workflowInputNativeBytes} from "../workflow-input-workbench";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeCell, nativeJson} from "../native-json";
import {nativeResponseRequest} from "../native-response";
import {downloadQueryBlob} from "../log-query";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";
import {ModelGraph} from "./ModelGraph";

interface AlgorithmState {form: PetriAntiMultiForm; input: File | null; model: File | null; returned: PetriAntiMultiRun | null; failed: PetriAntiMultiRun | null;}
const algorithmLabel = (algorithm: PetriAntiMultiAlgorithm) => algorithm === "anti" ? "Petri anti-alignment" : "Petri multi-alignment";
const inputLabels = {auto: "CSV / TSV / XES / gzip XES", table: "Typed EventTable JSON", trace_words: "Literal trace words JSON"};
const shapeLabels = {log: "Every grouped trace", trace: "One selected original trace", words: "Literal trace words"};
const initialState = (input: File | null): Record<PetriAntiMultiAlgorithm, AlgorithmState> => ({anti: {form: defaultPetriAntiMultiForm("anti"), input, model: null, returned: null, failed: null}, multi: {form: defaultPetriAntiMultiForm("multi"), input, model: null, returned: null, failed: null}});
export function PetriAntiMultiWorkspace({file: original = null, source = null}: {file?: File | null; source?: ActiveMiningSourceDescriptor | null}) {
  const [algorithm, setAlgorithm] = useState<PetriAntiMultiAlgorithm>("anti"), [states, setStates] = useState(() => initialState(original));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null), gate = useRef(new PetriAntiMultiRequestGate()), activeAlgorithm = useRef(algorithm);
  const identities = useRef<Record<PetriAntiMultiAlgorithm, {input: File | null; model: File | null}>>({anti: {input: original, model: null}, multi: {input: original, model: null}});
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const update = (change: (state: AlgorithmState) => AlgorithmState) => setStates(current => ({...current, [algorithm]: change(current[algorithm])}));
  const chooseInput = (value: File | null) => {cancel(); identities.current[algorithm].input = value; update(current => ({...current, input: value, returned: null, failed: null})); setError(null);};
  const chooseModel = (value: File | null) => {cancel(); identities.current[algorithm].model = value; update(current => ({...current, model: value, returned: null, failed: null})); setError(null);};
  useEffect(() => {
    cancel(); for (const value of PETRI_ANTI_MULTI_ALGORITHMS) identities.current[value].input = original;
    setStates(current => Object.fromEntries(PETRI_ANTI_MULTI_ALGORITHMS.map(value => {const mapped = source ? miningSourceWorkbenchFields(source, {case_id_key: "alignment_case_id_attribute", activity_key: "activity_attribute"}) : null; return [value, {...current[value], input: original, returned: null, failed: null, form: mapped ? {...current[value].form, inputFormat: source!.format, inputShape: current[value].form.inputShape === "words" ? "log" : current[value].form.inputShape, fields: {...current[value].form.fields, ...mapped.fields}, explicitEmptySelectors: {...current[value].form.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields} : current[value].form}];})) as Record<PetriAntiMultiAlgorithm, AlgorithmState>); setError(null);
  }, [original, source]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null;}, []);
  const selected = states[algorithm], form = selected.form;
  const editForm = (change: (form: PetriAntiMultiForm) => PetriAntiMultiForm) => update(current => ({...current, form: change(current.form)}));
  const changeAlgorithm = (value: PetriAntiMultiAlgorithm) => {cancel(); activeAlgorithm.current = value; setAlgorithm(value); setError(null);};
  const inputFormat = (value: PetriAntiMultiForm["inputFormat"]) => editForm(current => ({...current, inputFormat: value, inputShape: value === "trace_words" ? "words" : current.inputShape === "words" ? "log" : current.inputShape}));
  const inputShape = (value: PetriAntiMultiForm["inputShape"]) => editForm(current => ({...current, inputShape: value, inputFormat: value === "words" ? "trace_words" : current.inputFormat === "trace_words" ? "auto" : current.inputFormat}));
  const field = (key: string, value: string) => editForm(current => ({...current, fields: {...current.fields, [key]: value}}));
  const importField = (key: string, value: string) => editForm(current => ({...current, importFields: {...current.importFields, [key]: value}}));
  const execute = async () => {
    if (!selected.input || !selected.model) return; cancel(); const abort = new AbortController(), ticket = gate.current.issue(), input = selected.input, model = selected.model, snapshot = structuredClone(form);
    controller.current = abort; setPending(true); setError(null);
    const accepts = () => activeAlgorithm.current === snapshot.algorithm && identities.current[snapshot.algorithm].input === input && identities.current[snapshot.algorithm].model === model && controller.current === abort && gate.current.accepts(ticket, abort.signal);
    try {const result = await runPetriAntiMultiWorkbench(input, model, snapshot, abort.signal); if (accepts()) setStates(current => ({...current, [snapshot.algorithm]: {...current[snapshot.algorithm], ...(result.value.error === null ? {returned: result, failed: null} : {failed: result})}}));}
    catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  const isXes = /\.(?:xes|xes\.gz|gz)$/i.test(selected.input?.name ?? "");
  const download = (text: string, name: string) => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type: "application/json"}), name);
  return <>
    <section className="panel" aria-label="Independent Petri anti and multi alignment">
      <div className="panel-head"><h2>Compare original traces with an independent PNML model</h2><span className="panel-meta">Anti-alignment and multi-alignment</span></div>
      <p>Upload an original input and normative PNML model. Each algorithm returns one aggregate visible model sequence and its complete transition path, including silent transitions.</p>
      <label className="filters-field"><span>Independent Petri objective</span><select value={algorithm} onChange={event => changeAlgorithm(event.target.value as PetriAntiMultiAlgorithm)}>{PETRI_ANTI_MULTI_ALGORITHMS.map(value => <option key={value} value={value}>{algorithmLabel(value)}</option>)}</select></label>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Anti/multi original input</span><input type="file" accept={form.inputFormat === "auto" ? ".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" : ".json,application/json"} onChange={event => chooseInput(event.target.files?.[0] ?? null)} /><span>{selected.input?.name ?? "Choose an original input"}</span></label>
          <label className="filters-field"><span>Anti/multi normative PNML</span><input type="file" accept=".pnml,.xml,application/xml" onChange={event => chooseModel(event.target.files?.[0] ?? null)} /><span>{selected.model?.name ?? "Choose the original PNML model"}</span></label>
          <label className="filters-field"><span>Anti/multi input format</span><select value={form.inputFormat} onChange={event => inputFormat(event.target.value as PetriAntiMultiForm["inputFormat"])}>{PETRI_ANTI_MULTI_INPUT_FORMATS.map(value => <option key={value} value={value}>{inputLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Anti/multi trace selection</span><select value={form.inputShape} onChange={event => inputShape(event.target.value as PetriAntiMultiForm["inputShape"])}>{PETRI_ANTI_MULTI_INPUT_SHAPES.map(value => <option key={value} value={value}>{shapeLabels[value]}</option>)}</select></label>
          {form.inputShape === "trace" && <label className="filters-field"><span>Original trace index (zero-based)</span><input value={form.traceIndex} placeholder="Native default: first trace" onChange={event => editForm(current => ({...current, traceIndex: event.target.value}))} /></label>}
          <label className="filters-field"><span>Normative PNML format</span><select value={form.modelFormat} onChange={event => editForm(current => ({...current, modelFormat: event.target.value as PetriAntiMultiForm["modelFormat"]}))}>{PETRI_ANTI_MULTI_MODEL_FORMATS.map(value => <option key={value} value={value}>{value === "auto" ? "Detect PNML model" : "PNML"}</option>)}</select></label>
          <label className="filters-field"><span>Native anti/multi variant</span><select value={PETRI_ANTI_MULTI_VARIANT} disabled><option>{PETRI_ANTI_MULTI_VARIANT}</option></select></label>
        </div>
        {original && <button className="btn-ghost" onClick={() => {chooseInput(original); if (source) {const mapped = miningSourceWorkbenchFields(source, {case_id_key: "alignment_case_id_attribute", activity_key: "activity_attribute"}); editForm(current => ({...current, inputFormat: source.format, inputShape: current.inputShape === "words" ? "log" : current.inputShape, fields: {...current.fields, ...mapped.fields}, explicitEmptySelectors: {...current.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields}));} else inputFormat("auto");}}>Use current original log for anti/multi alignment</button>}
        {form.inputFormat === "trace_words" && <details open><summary>Literal trace words JSON</summary><p>Upload one ordered JSON string array. It becomes one grouped trace with present activity values, preserving repeats, empty labels, whitespace and Unicode. An empty array represents one empty trace.</p><pre>{PETRI_ANTI_MULTI_TRACE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(PETRI_ANTI_MULTI_TRACE_EXAMPLE, "minera-petri-anti-multi-words.json")}>Download anti/multi trace example</button></details>}
        {form.inputFormat === "table" && <details><summary>Typed anti/multi input table</summary><p>Generic log conversion uses the literal case selector. The original table preserves missing cells, present nulls, lexical values, dtypes, indices, categories and metadata.</p><pre>{WORKFLOW_INPUT_TABLE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(WORKFLOW_INPUT_TABLE_EXAMPLE, "minera-petri-anti-multi-table.json")}>Download anti/multi table example</button></details>}
        {form.inputFormat === "auto" && <details><summary>Anti/multi import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key]) => isXes ? key.startsWith("xes_") : !key.startsWith("xes_")).map(([key, label, placeholder]) => <label className="filters-field" key={key}><span>{label}</span><input value={form.importFields[key] ?? ""} placeholder={placeholder} onChange={event => importField(key, event.target.value)} /></label>)}
          {isXes ? <><label className="filters-field"><span>Anti/multi XES importer</span><select value={form.importFields.xes_variant ?? ""} onChange={event => importField("xes_variant", event.target.value)}><option value="">Native default</option>{WORKFLOW_INPUT_XES_VARIANTS.map(value => <option key={value}>{value}</option>)}</select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key] ?? ""} onChange={event => importField(key, event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}</>
            : <label className="filters-field"><span>Anti/multi CSV delimiter</span><select value={form.importFields.delimiter ?? ""} onChange={event => importField("delimiter", event.target.value)}><option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>}
        </div></details>}
        <details><summary>Anti/multi PNML import settings</summary><div className="query-options">{PETRI_ANTI_MULTI_PNML_CONTROLS.map(([key, label, enabled]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.pnmlFields[key] ?? ""} onChange={event => editForm(current => ({...current, pnmlFields: {...current.pnmlFields, [key]: event.target.value}}))}><option value="">Native default ({enabled ? "on" : "off"})</option><option value="true">On</option><option value="false">Off</option></select></label>)}</div></details>
        <div className="query-options">{PETRI_ANTI_MULTI_CONTROLS[algorithm].filter(control => !control.kind.endsWith("marking")).map(control => <PetriObjectiveField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={!!form.explicitEmptySelectors[control.key]} onChange={value => field(control.key, value)} onEmptyChange={value => editForm(current => ({...current, explicitEmptySelectors: {...current.explicitEmptySelectors, [control.key]: value}, ...(value ? {fields: {...current.fields, [control.key]: ""}} : {})}))} />)}</div>
        <details><summary>Exact marking overrides</summary><p>Positions follow the parsed PNML place order. Omitted overrides use PNML markings. Explicit JSON [] selects native source/sink defaults; an all-zero vector remains an explicit zero marking. Choose one carrier for each marking.</p><div className="query-options">{PETRI_ANTI_MULTI_CONTROLS[algorithm].filter(control => control.kind.endsWith("marking")).map(control => <PetriObjectiveField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={false} onChange={value => field(control.key, value)} onEmptyChange={() => {}} />)}</div></details>
        <p>Blank controls use native defaults. Exponent must be greater than one and anti-alignment epsilon nonnegative; native errors retain valid input/model snapshots. A zero max_states uses marking_limit, which bounds repeated closed-marking expansions. These algorithms compare distinct trace words and ignore their frequencies.</p>
        <p>Original PNML exports preserve the parsed document. Checked PNML exports use the resolved markings from this run. Search diagnostics report pruning; the native result has no per-trace completion or time-limit fields.</p>
        <button className="btn-open" disabled={!selected.input || !selected.model} onClick={() => void execute()}>Run independent {algorithm === "anti" ? "anti-alignment" : "multi-alignment"}</button>
      </fieldset>
      {pending && <p role="status">Searching the independent PNML model… <button className="btn-ghost" onClick={cancel}>Cancel anti/multi alignment request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {selected.failed && <PetriObjectiveInspection key={selected.failed.raw + ":failed"} run={selected.failed} failed />}
    {selected.returned && <PetriObjectiveInspection key={selected.returned.raw + ":returned"} run={selected.returned} />}
  </>;
}
function PetriObjectiveField({control, value, explicitEmpty, onChange, onEmptyChange}: {control: PetriAntiMultiControl; value: string; explicitEmpty: boolean; onChange: (value: string) => void; onEmptyChange: (value: boolean) => void}) {
  return <div className="filters-field"><label><span>{control.label}</span>{control.kind.endsWith("marking") ? <textarea rows={3} value={value} placeholder={control.kind === "marking" ? "Native omission, or []" : "Native omission, or 1,0,0"} onChange={event => onChange(event.target.value)} /> : <input disabled={control.kind === "selector" && explicitEmpty} value={value} placeholder="Native default" onChange={event => onChange(event.target.value)} />}</label>
    {control.kind === "selector" && <label className="query-check"><input type="checkbox" checked={explicitEmpty} onChange={event => onEmptyChange(event.target.checked)} />Submit explicit empty {control.label.toLowerCase()}</label>}
  </div>;
}
function PetriObjectiveInspection({run, failed = false}: {run: PetriAntiMultiRun; failed?: boolean}) {
  const [tableId, setTableId] = useState(failed ? "/options_context" : "complete_transitions"), table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const graph = useMemo(() => petriAntiMultiGraph(run.value), [run.value]), request = nativeResponseRequest(run.value) ?? {path: "/api/align-petri-anti-multi-workbench", fields: run.fields}, prefix = `minera-petri-${run.value.algorithm}${failed ? "-failed" : ""}`;
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), `${prefix}-${suffix}`);
  return <>
    <ModelGraph graph={graph} title="Original normative PNML net" />
    <section className="panel" aria-label={failed ? "Failed Petri objective inspection" : "Returned Petri objective inspection"}>
      <div className="panel-head"><h2>{failed ? "Failed anti/multi alignment attempt" : "Returned aggregate Petri objective"}</h2><span className="panel-meta">{algorithmLabel(run.value.algorithm)}</span></div>
      <p>{run.inputs.input.name} against {run.inputs.model.name} · {run.value.input_shape}. Checked cases: {run.value.input_context.checked_case_count === null ? "unknown" : String(nativeCell(run.value.input_context, "checked_case_count"))}; checked events: {run.value.input_context.checked_event_count === null ? "unknown" : String(nativeCell(run.value.input_context, "checked_event_count"))}.</p>
      <p role="status">{petriAntiMultiSummary(run.value)}</p>
      <details><summary>Exact anti/multi request and original file hashes</summary><pre>{nativeJson({request, input_sha256: run.inputs.input_sha256, model_sha256: run.inputs.model_sha256}, 2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete anti/multi native JSON</button>
        <button className="btn-ghost" onClick={() => download(petriAntiMultiReport(run), "report.json")}>Download anti/multi report</button>
        <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download anti/multi request</button>
        <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${petriAntiMultiTableFileName(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete anti/multi table CSV</button>
        <button className="btn-ghost" onClick={() => download(petriAntiMultiAllTablesCsv(run.tables), "all-tables.csv", "text/csv;charset=utf-8")}>Download all anti/multi tables CSV</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.input, run.inputs.input.name)}>Download original anti/multi input</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.model, run.inputs.model.name)}>Download original normative PNML</button>
        {(["pnml", "checked_pnml", "source_xes"] as const).map(format => <button className="btn-ghost" key={format} disabled={run.value[format] === null} onClick={() => {const file = petriAntiMultiArtifactFile(run, format); downloadQueryBlob(file, file.name);}}>Download this run’s {format === "pnml" ? "parsed original PNML" : format === "checked_pnml" ? "PNML with checked markings" : "checked source XES"}</button>)}
      </div>
      {(["input_log_error", "checked_log_error", "source_xes_error", "pnml_error", "checked_pnml_error"] as const).map(key => run.value[key] && <p key={key}>{key.replace(/_/g, " ")}: {run.value[key]}</p>)}
      <label className="filters-field"><span>Anti/multi result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
      <DiagnosticTableView key={table.id} table={table} noun="Petri objective" />
      <details><summary>Complete anti/multi native JSON</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
