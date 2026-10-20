import {SavedDfgProjectionWorkspace} from "./SavedDfgProjectionWorkspace";
import {miningSourceWorkbenchFields, type ActiveMiningSourceDescriptor} from "../active-mining-source";
import {useEffect, useRef, useState} from "react";
import {
  GRAPH_ALIGNMENT_CONTROLS, GRAPH_ALIGNMENT_DFG_EXAMPLE, GRAPH_ALIGNMENT_INPUT_FORMATS, GRAPH_ALIGNMENT_INPUT_SHAPES,
  GRAPH_ALIGNMENT_MODES, GRAPH_ALIGNMENT_PNML_CONTROLS, GRAPH_ALIGNMENT_PROJECTIONS, GRAPH_ALIGNMENT_TRACE_EXAMPLE,
  GraphAlignmentRequestGate, defaultGraphAlignmentForm, graphAlignmentAllTablesCsv, graphAlignmentArtifactFile,
  graphAlignmentReport, graphAlignmentSucceeded, graphAlignmentSummary, runGraphAlignmentWorkbench,
  type GraphAlignmentControl, type GraphAlignmentForm, type GraphAlignmentMode, type GraphAlignmentRun,
} from "../graph-alignment-workbench";
import {TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS} from "../token-replay-workbench";
import {WORKFLOW_INPUT_TABLE_EXAMPLE, WORKFLOW_INPUT_XES_VARIANTS, workflowInputNativeBytes} from "../workflow-input-workbench";
import {diagnosticCsv} from "../model-diagnostics";
import {nativeCell, nativeJson} from "../native-json";
import {nativeResponseRequest} from "../native-response";
import {downloadQueryBlob} from "../log-query";
import {DiagnosticTableView} from "./ModelDiagnosticsInspection";

interface ModeState {form: GraphAlignmentForm; input: File | null; model: File | null; completed: GraphAlignmentRun | null; failed: GraphAlignmentRun | null;}
const modeLabel = (mode: GraphAlignmentMode) => mode === "DFG_CLASSIC" ? "Classic DFG alignment" : "Maximal decomposed Petri alignment";
const inputLabels = {auto: "CSV / TSV / XES / gzip XES", table: "Typed EventTable JSON", trace_words: "Literal trace words JSON"};
const shapeLabels = {log: "Every grouped trace", trace: "One selected original trace", words: "Literal trace words"};
const initialState = (input: File | null): Record<GraphAlignmentMode, ModeState> => ({
  DFG_CLASSIC: {form: defaultGraphAlignmentForm("DFG_CLASSIC"), input, model: null, completed: null, failed: null},
  RECOMPOS_MAXIMAL: {form: defaultGraphAlignmentForm("RECOMPOS_MAXIMAL"), input, model: null, completed: null, failed: null},
});
export function GraphAlignmentWorkspace({file: original = null, source = null}: {file?: File | null; source?: ActiveMiningSourceDescriptor | null}) {
  const [operation, setOperation] = useState<"new_alignment" | "project_saved_dfg">("new_alignment");
  const [mode, setMode] = useState<GraphAlignmentMode>("DFG_CLASSIC"), [states, setStates] = useState(() => initialState(original));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null), gate = useRef(new GraphAlignmentRequestGate());
  const identities = useRef<Record<GraphAlignmentMode, {input: File | null; model: File | null}>>({DFG_CLASSIC: {input: original, model: null}, RECOMPOS_MAXIMAL: {input: original, model: null}});
  const activeMode = useRef(mode);
  const cancel = () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null; setPending(false);};
  const update = (change: (state: ModeState) => ModeState) => setStates(current => ({...current, [mode]: change(current[mode])}));
  const chooseInput = (value: File | null) => {cancel(); identities.current[mode].input = value; update(current => ({...current, input: value, completed: null, failed: null})); setError(null);};
  const chooseModel = (value: File | null) => {cancel(); identities.current[mode].model = value; update(current => ({...current, model: value, completed: null, failed: null})); setError(null);};
  useEffect(() => {
    cancel(); for (const value of GRAPH_ALIGNMENT_MODES) identities.current[value].input = original;
    setStates(current => Object.fromEntries(GRAPH_ALIGNMENT_MODES.map(value => {const mapped = source ? miningSourceWorkbenchFields(source, value === "DFG_CLASSIC" ? {case_id_key: "alignment_case_id_attribute", activity_key: "dfg_activity_attribute", timestamp_key: "dfg_timestamp_attribute"} : {case_id_key: "alignment_case_id_attribute", activity_key: "activity_attribute"}) : null; return [value, {...current[value], input: original, completed: null, failed: null, form: mapped ? {...current[value].form, inputFormat: source!.format, inputShape: current[value].form.inputShape === "words" ? "log" : current[value].form.inputShape, fields: {...current[value].form.fields, ...mapped.fields}, explicitEmptySelectors: {...current[value].form.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields} : current[value].form}];})) as Record<GraphAlignmentMode, ModeState>); setError(null);
  }, [original, source]);
  useEffect(() => () => {gate.current.invalidate(); controller.current?.abort(); controller.current = null;}, []);
  const selected = states[mode], form = selected.form;
  const editForm = (change: (form: GraphAlignmentForm) => GraphAlignmentForm) => update(current => ({...current, form: change(current.form)}));
  const changeMode = (value: GraphAlignmentMode) => {cancel(); activeMode.current = value; setMode(value); setError(null);};
  const inputFormat = (value: GraphAlignmentForm["inputFormat"]) => editForm(current => ({...current, inputFormat: value, inputShape: value === "trace_words" ? "words" : current.inputShape === "words" ? "log" : current.inputShape, projection: value === "trace_words" ? "none" : current.projection}));
  const inputShape = (value: GraphAlignmentForm["inputShape"]) => editForm(current => ({...current, inputShape: value, inputFormat: value === "words" ? "trace_words" : current.inputFormat === "trace_words" ? "auto" : current.inputFormat, projection: value !== "log" ? "none" : current.projection}));
  const field = (key: string, value: string) => editForm(current => ({...current, fields: {...current.fields, [key]: value}}));
  const importField = (key: string, value: string) => editForm(current => ({...current, importFields: {...current.importFields, [key]: value}}));
  const execute = async () => {
    if (!selected.input || !selected.model) return;
    cancel(); const abort = new AbortController(), ticket = gate.current.issue(), input = selected.input, model = selected.model, snapshot = structuredClone(form);
    controller.current = abort; setPending(true); setError(null);
    const accepts = () => activeMode.current === snapshot.mode && identities.current[snapshot.mode].input === input && identities.current[snapshot.mode].model === model && controller.current === abort && gate.current.accepts(ticket, abort.signal);
    try {
      const result = await runGraphAlignmentWorkbench(input, model, snapshot, abort.signal);
      if (accepts()) setStates(current => ({...current, [snapshot.mode]: {...current[snapshot.mode], ...(graphAlignmentSucceeded(result.value) ? {completed: result, failed: null} : {failed: result})}}));
    } catch (cause) {if (accepts()) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (controller.current === abort) {controller.current = null; setPending(false);}}
  };
  const isXes = /\.(?:xes|xes\.gz|gz)$/i.test(selected.input?.name ?? "");
  const download = (text: string, name: string) => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type: "application/json"}), name);
  return <>
    <section className="panel" aria-label="Graph alignment operation">
      <label className="filters-field"><span>Graph alignment operation</span><select value={operation} onChange={event => {cancel(); setOperation(event.target.value as "new_alignment" | "project_saved_dfg");}}><option value="new_alignment">Compute new alignment</option><option value="project_saved_dfg">Project saved DFG alignment result</option></select></label>
    </section>
    <SavedDfgProjectionWorkspace active={operation === "project_saved_dfg"} completed={states.DFG_CLASSIC.completed} original={original} />
    <div hidden={operation !== "new_alignment"}>
    <section className="panel" aria-label="Independent graph alignment">
      <div className="panel-head"><h2>Align original traces against a DFG or PNML model</h2><span className="panel-meta">Independent inputs</span></div>
      <p>Choose Classic DFG alignment or maximal decomposed Petri alignment. Upload the original input and its normative model, then inspect every returned move, search diagnostic and model snapshot.</p>
      <label className="filters-field"><span>Graph alignment mode</span><select value={mode} onChange={event => changeMode(event.target.value as GraphAlignmentMode)}>{GRAPH_ALIGNMENT_MODES.map(value => <option key={value} value={value}>{modeLabel(value)}</option>)}</select></label>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Graph alignment original input</span><input type="file" accept={form.inputFormat === "auto" ? ".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" : ".json,application/json"} onChange={event => chooseInput(event.target.files?.[0] ?? null)} /><span>{selected.input?.name ?? "Choose an original input"}</span></label>
          <label className="filters-field"><span>Graph alignment normative model</span><input type="file" accept={mode === "DFG_CLASSIC" ? ".json,.dfg,application/json,text/plain" : ".pnml,.xml,application/xml"} onChange={event => chooseModel(event.target.files?.[0] ?? null)} /><span>{selected.model?.name ?? (mode === "DFG_CLASSIC" ? "Choose a DFG model" : "Choose PNML")}</span></label>
          <label className="filters-field"><span>Graph alignment input format</span><select value={form.inputFormat} onChange={event => inputFormat(event.target.value as GraphAlignmentForm["inputFormat"])}>{GRAPH_ALIGNMENT_INPUT_FORMATS.map(value => <option key={value} value={value}>{inputLabels[value]}</option>)}</select></label>
          <label className="filters-field"><span>Graph alignment trace selection</span><select value={form.inputShape} onChange={event => inputShape(event.target.value as GraphAlignmentForm["inputShape"])}>{GRAPH_ALIGNMENT_INPUT_SHAPES.map(value => <option key={value} value={value}>{shapeLabels[value]}</option>)}</select></label>
          {form.inputShape === "trace" && <label className="filters-field"><span>Original trace index (zero-based)</span><input value={form.traceIndex} placeholder="Native default: first trace" onChange={event => editForm(current => ({...current, traceIndex: event.target.value}))} /></label>}
          <label className="filters-field"><span>Normative model format</span><select value={form.modelFormat} onChange={event => editForm(current => ({...current, modelFormat: event.target.value as GraphAlignmentForm["modelFormat"]}))}>
            <option value="auto">Detect from model file</option>{mode === "DFG_CLASSIC" ? <><option value="dfg_json">Structured DFG JSON</option><option value="dfg">Classic DFG text</option></> : <option value="pnml">PNML</option>}
          </select></label>
          {mode === "DFG_CLASSIC" && form.inputShape === "log" && <label className="filters-field"><span>Same-run DFG projection</span><select value={form.projection} onChange={event => editForm(current => ({...current, projection: event.target.value as GraphAlignmentForm["projection"]}))}>{GRAPH_ALIGNMENT_PROJECTIONS.map(value => <option key={value} value={value}>{value === "none" ? "No projection" : value === "log" ? "Projected log" : value === "metrics" ? "Projected metrics" : "Projected log and metrics"}</option>)}</select></label>}
        </div>
        {original && <button className="btn-ghost" onClick={() => {chooseInput(original); if (source) {const mapped = miningSourceWorkbenchFields(source, mode === "DFG_CLASSIC" ? {case_id_key: "alignment_case_id_attribute", activity_key: "dfg_activity_attribute", timestamp_key: "dfg_timestamp_attribute"} : {case_id_key: "alignment_case_id_attribute", activity_key: "activity_attribute"}); editForm(current => ({...current, inputFormat: source.format, inputShape: current.inputShape === "words" ? "log" : current.inputShape, fields: {...current.fields, ...mapped.fields}, explicitEmptySelectors: {...current.explicitEmptySelectors, ...mapped.explicitEmptySelectors}, importFields: mapped.importFields}));} else inputFormat("auto");}}>Use current original log for graph alignment</button>}
        {form.inputFormat === "trace_words" && <details open><summary>Literal trace words JSON</summary><p>Upload one JSON array of strings. Repeats, empty labels, whitespace and Unicode remain literal activities in the selected native trace alignment.</p><pre>{GRAPH_ALIGNMENT_TRACE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(GRAPH_ALIGNMENT_TRACE_EXAMPLE, "minera-graph-alignment-trace-words.json")}>Download literal trace example</button></details>}
        {form.inputFormat === "table" && <details><summary>Typed event table JSON</summary><p>The original table retains cells, dtypes, indices, categories and metadata. Conversion groups it by the selected case attribute. Missing cells, present nulls and empty labels retain their native meaning.</p><pre>{WORKFLOW_INPUT_TABLE_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(WORKFLOW_INPUT_TABLE_EXAMPLE, "minera-graph-alignment-table.json")}>Download typed alignment table example</button></details>}
        {mode === "DFG_CLASSIC" && <details><summary>Normative structured DFG input</summary><p>Upload structured Alpha DFG JSON with kind sparse, clean or graph, or classic .dfg text. The supplied graph and boundaries define the alignment model.</p><pre>{GRAPH_ALIGNMENT_DFG_EXAMPLE}</pre><button className="btn-ghost" onClick={() => download(GRAPH_ALIGNMENT_DFG_EXAMPLE, "minera-graph-alignment-model.json")}>Download normative DFG example</button></details>}
        {form.inputFormat === "auto" && <details><summary>Graph alignment import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key]) => isXes ? key.startsWith("xes_") : !key.startsWith("xes_")).map(([key, label, placeholder]) => <label className="filters-field" key={key}><span>{label}</span><input value={form.importFields[key] ?? ""} placeholder={placeholder} onChange={event => importField(key, event.target.value)} /></label>)}
          {isXes ? <><label className="filters-field"><span>Graph alignment XES importer</span><select value={form.importFields.xes_variant ?? ""} onChange={event => importField("xes_variant", event.target.value)}><option value="">Native default</option>{WORKFLOW_INPUT_XES_VARIANTS.map(value => <option key={value}>{value}</option>)}</select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key] ?? ""} onChange={event => importField(key, event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}</>
            : <label className="filters-field"><span>Graph alignment CSV delimiter</span><select value={form.importFields.delimiter ?? ""} onChange={event => importField("delimiter", event.target.value)}><option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option></select></label>}
        </div></details>}
        {mode === "RECOMPOS_MAXIMAL" && <details><summary>PNML import settings</summary><div className="query-options">{GRAPH_ALIGNMENT_PNML_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.pnmlFields[key] ?? ""} onChange={event => editForm(current => ({...current, pnmlFields: {...current.pnmlFields, [key]: event.target.value}}))}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select></label>)}</div></details>}
        <div className="query-options">{GRAPH_ALIGNMENT_CONTROLS[mode].filter(control => !["map", "marking"].includes(control.kind)).map(control => <GraphAlignmentField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={!!form.explicitEmptySelectors[control.key]} onChange={value => field(control.key, value)} onEmptyChange={value => editForm(current => ({...current, explicitEmptySelectors: {...current.explicitEmptySelectors, [control.key]: value}, ...(value ? {fields: {...current.fields, [control.key]: ""}} : {})}))} />)}</div>
        <details><summary>Exact activity costs {mode === "RECOMPOS_MAXIMAL" ? "and marking overrides" : ""}</summary><div className="query-options">{GRAPH_ALIGNMENT_CONTROLS[mode].filter(control => ["map", "marking"].includes(control.kind)).map(control => <GraphAlignmentField key={control.key} control={control} value={form.fields[control.key] ?? ""} explicitEmpty={false} onChange={value => field(control.key, value)} onEmptyChange={() => {}} />)}</div></details>
        <p>Blank controls use native defaults. Explicit empty cost objects and marking arrays are submitted as entered. Literal selectors retain their exact text; native validation reports unsupported names. Completed results retain the files and options used for their run.</p>
        {mode === "DFG_CLASSIC" && <p>Grouped-log alignment computes trace scores. Raw trace alignment leaves fitness and best/worst cost undefined. Projections use the alignments returned by this run.</p>}
        {mode === "RECOMPOS_MAXIMAL" && <p>Marking positions follow the imported place order. An explicit empty marking array selects the native source/sink default. A zero search-state limit can return an incomplete native result.</p>}
        <button className="btn-open" disabled={!selected.input || !selected.model} onClick={() => void execute()}>Run independent graph alignment</button>
      </fieldset>
      {pending && <p role="status">Aligning original input… <button className="btn-ghost" onClick={cancel}>Cancel graph alignment request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {selected.failed && <GraphAlignmentInspection key={selected.failed.raw + ":failed"} run={selected.failed} incomplete />}
    {selected.completed && <GraphAlignmentInspection key={selected.completed.raw + ":completed"} run={selected.completed} />}
    </div>
  </>;
}
function GraphAlignmentField({control, value, explicitEmpty, onChange, onEmptyChange}: {control: GraphAlignmentControl; value: string; explicitEmpty: boolean; onChange: (value: string) => void; onEmptyChange: (value: boolean) => void}) {
  return <div className="filters-field"><label><span>{control.label}</span>
    {control.kind === "boolean" ? <select value={value} onChange={event => onChange(event.target.value)}><option value="">Native default</option><option value="true">On</option><option value="false">Off</option></select> : ["map", "marking"].includes(control.kind) ? <textarea rows={3} value={value} placeholder={control.kind === "map" ? "Native omission, or {}" : "Native omission, or []"} onChange={event => onChange(event.target.value)} /> : <input disabled={control.kind === "selector" && explicitEmpty} value={value} placeholder="Native default" onChange={event => onChange(event.target.value)} />}
  </label>{control.kind === "selector" && <label className="query-check"><input type="checkbox" checked={explicitEmpty} onChange={event => onEmptyChange(event.target.checked)} />Submit explicit empty {control.label.toLowerCase()}</label>}</div>;
}
function GraphAlignmentInspection({run, incomplete = false}: {run: GraphAlignmentRun; incomplete?: boolean}) {
  const [tableId, setTableId] = useState("complete_alignment_moves"), table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const request = nativeResponseRequest(run.value) ?? {path: "/api/align-graph-workbench", fields: run.fields};
  const prefix = `minera-${run.value.mode.toLowerCase()}${incomplete ? "-incomplete" : ""}`;
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([workflowInputNativeBytes(text)], {type}), `${prefix}-${suffix}`);
  return <section className="panel" aria-label={incomplete ? "Incomplete graph alignment inspection" : "Complete graph alignment inspection"}>
    <div className="panel-head"><h2>{incomplete ? "Incomplete or failed graph alignment attempt" : "Complete graph alignment result"}</h2><span className="panel-meta">{modeLabel(run.value.mode)}</span></div>
    <p>{run.inputs.input.name} against {run.inputs.model.name} · {run.value.input_shape}. Checked cases: {run.value.input_context.checked_case_count === null ? "unknown" : String(nativeCell(run.value.input_context, "checked_case_count"))}; checked events: {run.value.input_context.checked_event_count === null ? "unknown" : String(nativeCell(run.value.input_context, "checked_event_count"))}.</p>
    <p role="status">{graphAlignmentSummary(run.value)}</p>
    <details><summary>Exact graph alignment request and original file hashes</summary><pre>{nativeJson({request, input_sha256: run.inputs.input_sha256, model_sha256: run.inputs.model_sha256}, 2)}</pre></details>
    <div className="query-actions">
      <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete graph alignment native JSON</button>
      <button className="btn-ghost" onClick={() => download(graphAlignmentReport(run), "report.json")}>Download graph alignment report</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download graph alignment request</button>
      <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${encodeURIComponent(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete graph alignment table CSV</button>
      <button className="btn-ghost" onClick={() => download(graphAlignmentAllTablesCsv(run.tables), "all-tables.csv", "text/csv;charset=utf-8")}>Download all graph alignment tables CSV</button>
      <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.input, run.inputs.input.name)}>Download original alignment input</button>
      <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.model, run.inputs.model.name)}>Download original normative model</button>
      <button className="btn-ghost" disabled={run.value.model_export === null} onClick={() => {const file = graphAlignmentArtifactFile(run, "model"); downloadQueryBlob(file, file.name);}}>Download this run’s normative model {run.value.model_export_format.toUpperCase()}</button>
      <button className="btn-ghost" disabled={run.value.projected_xes === null} onClick={() => {const file = graphAlignmentArtifactFile(run, "projection"); downloadQueryBlob(file, file.name);}}>Download this run’s projected XES</button>
    </div>
    {run.value.projection_error && <p>Projection: {run.value.projection_error}</p>}{run.value.model_export_error && <p>Model export: {run.value.model_export_error}</p>}
    <label className="filters-field"><span>Graph alignment result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}</select></label>
    <DiagnosticTableView key={table.id} table={table} noun="alignment" />
    <details><summary>Complete graph alignment native JSON</summary><pre>{run.raw}</pre></details>
  </section>;
}
