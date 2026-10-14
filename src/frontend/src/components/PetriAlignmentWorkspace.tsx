import { useEffect, useRef, useState } from "react";
import {
  defaultPetriAlignmentWorkbenchForm, PETRI_ALIGNMENT_CONTROLS, PETRI_ALIGNMENT_VARIANTS, PETRI_ALIGNMENT_XES_VARIANTS,
  petriAlignmentControlActive, petriAlignmentResultSummary, petriAlignmentWorkbenchReport, runPetriAlignmentWorkbench,
  type PetriAlignmentWorkbenchRun,
} from "../petri-alignment-workbench";
import { TOKEN_REPLAY_IMPORT_CONTROLS, TOKEN_REPLAY_XES_BOOLEAN_CONTROLS } from "../token-replay-workbench";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeResponseRequest } from "../native-response";
import { nativeJson } from "../native-json";
import { downloadQueryBlob } from "../log-query";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";

export function PetriAlignmentWorkspace({file}: {file: File | null}) {
  const [log, setLog] = useState<File | null>(file), [model, setModel] = useState<File | null>(null);
  const [form, setForm] = useState(defaultPetriAlignmentWorkbenchForm), [run, setRun] = useState<PetriAlignmentWorkbenchRun | null>(null);
  const [tableId, setTableId] = useState("/trace_groups"), [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const controller = useRef<AbortController | null>(null);
  useEffect(() => () => {controller.current?.abort(); controller.current = null;}, []);
  const setImport = (key: string, value: string) => setForm(current => ({...current, importFields: {...current.importFields, [key]: value}}));
  const setAlignment = (key: string, value: string) => setForm(current => ({...current, alignmentFields: {...current.alignmentFields, [key]: value}}));
  const execute = async () => {
    if (!log || !model) return;
    controller.current?.abort();
    const current = new AbortController(); controller.current = current; setPending(true); setError(null);
    try {
      const result = await runPetriAlignmentWorkbench(log, model, form, current.signal);
      if (controller.current === current && !current.signal.aborted) {setRun(result); setTableId("/trace_groups");}
    } catch (cause) {
      if (controller.current === current && !current.signal.aborted) setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (controller.current === current) {controller.current = null; setPending(false);}
    }
  };
  const table = run?.tables.find(item => item.id === tableId) ?? run?.tables[0], input = run?.value.input_context;
  const download = (raw: string, name: string, type = "application/json") => downloadQueryBlob(new Blob([raw], {type}), name);
  const isXes = /\.(?:xes|xes\.gz|gz)$/i.test(log?.name ?? "");
  const activeControls = PETRI_ALIGNMENT_CONTROLS.filter(([,,, scope]) => petriAlignmentControlActive(scope, form.variant));
  const control = ([key, label, kind]: typeof PETRI_ALIGNMENT_CONTROLS[number]) => <label className="filters-field" key={key}>
    <span>{label}</span>
    {kind === "boolean" ? <select value={form.alignmentFields[key] ?? ""} onChange={event => setAlignment(key, event.target.value)}>
      <option value="">Native default</option><option value="true">On</option><option value="false">Off</option>
    </select> : kind === "profile" ? <select value={form.alignmentFields[key] ?? ""} onChange={event => setAlignment(key, event.target.value)}>
      <option value="">Native default</option><option value="legacy">Legacy</option><option value="standard">Standard</option>
    </select> : kind === "map" || kind === "array" ? <textarea rows={3} value={form.alignmentFields[key] ?? ""} placeholder={kind === "map" ? "Native default, or {}" : "Native default, or []"} onChange={event => setAlignment(key, event.target.value)} />
      : <input value={form.alignmentFields[key] ?? ""} placeholder="Native default" onChange={event => setAlignment(key, event.target.value)} />}
  </label>;
  return <>
    <section className="panel" aria-label="Independent Petri alignment">
      <div className="panel-head"><h2>Align a log against a PNML model</h2><span className="panel-meta">Independent inputs</span></div>
      <p>Upload a log and its model, choose the alignment search and costs, then inspect every trace group, move, alternative and visited marking.</p>
      <fieldset className="query-fields" disabled={pending}>
        <div className="query-options">
          <label className="filters-field"><span>Alignment log file</span><input type="file" accept=".csv,.tsv,.xes,.xes.gz,.gz,text/csv,application/xml,application/gzip" onChange={event => setLog(event.target.files?.[0] ?? null)} /><span>{log?.name ?? "Choose CSV, TSV or XES"}</span></label>
          <label className="filters-field"><span>Alignment PNML model</span><input type="file" accept=".pnml,.xml,application/xml" onChange={event => setModel(event.target.files?.[0] ?? null)} /><span>{model?.name ?? "Choose PNML"}</span></label>
          <label className="filters-field"><span>Alignment search variant</span><select value={form.variant} onChange={event => setForm(current => ({...current, variant: event.target.value as typeof form.variant}))}>
            {PETRI_ALIGNMENT_VARIANTS.map(value => <option key={value} value={value}>{value}</option>)}
          </select></label>
          <label className="filters-field"><span>Alignment input shape</span><select value={form.adapter} onChange={event => setForm(current => ({...current, adapter: event.target.value as typeof form.adapter}))}>
            <option value="log">Grouped log</option><option value="stream">Event stream</option><option value="dataframe">Dataframe</option><option value="lazyframe">Lazyframe</option>
          </select></label>
        </div>
        {file && <button className="btn-ghost" onClick={() => setLog(file)}>Use current original log</button>}
        <details><summary>Alignment import settings</summary><div className="query-options">
          {TOKEN_REPLAY_IMPORT_CONTROLS.filter(([key]) => isXes ? key.startsWith("xes_") : !key.startsWith("xes_")).map(([key, label, placeholder]) => <label className="filters-field" key={key}>
            <span>{label}</span><input value={form.importFields[key] ?? ""} placeholder={placeholder} onChange={event => setImport(key, event.target.value)} />
          </label>)}
          {isXes ? <>
            <label className="filters-field"><span>XES importer</span><select value={form.importFields.xes_variant ?? ""} onChange={event => setImport("xes_variant", event.target.value)}>
              <option value="">Native default</option>{PETRI_ALIGNMENT_XES_VARIANTS.map(value => <option key={value} value={value}>{value}</option>)}
            </select></label>
            {TOKEN_REPLAY_XES_BOOLEAN_CONTROLS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><select value={form.importFields[key] ?? ""} onChange={event => setImport(key, event.target.value)}>
              <option value="">Native default</option><option value="true">On</option><option value="false">Off</option>
            </select></label>)}
          </> : <label className="filters-field"><span>CSV delimiter</span><select value={form.importFields.delimiter ?? ""} onChange={event => setImport("delimiter", event.target.value)}>
            <option value="">Native default (tab for TSV)</option><option value=",">Comma</option><option value=";">Semicolon</option><option value={"\t"}>Tab</option><option value="|">Pipe</option>
          </select></label>}
        </div></details>
        <div className="query-options">{activeControls.filter(([,, kind]) => !["map", "array", "marking"].includes(kind)).map(control)}</div>
        <details><summary>Alignment cost maps and marking overrides</summary><div className="query-options">{activeControls.filter(([,, kind]) => ["map", "array", "marking"].includes(kind)).map(control)}</div></details>
        <p>Flat inputs retain original row order and columns; the case attribute can regroup events. Grouped logs retain trace boundaries. Blank settings use native defaults. Explicit empty cost objects and arrays are submitted as entered. Marking positions follow the PNML place order shown in the result.</p>
        {form.variant.startsWith("generator_") && <p>Generator search returns the optimal alternatives permitted by the chosen limit. Every returned alternative remains available in the complete tables and downloads.</p>}
        <button className="btn-open" disabled={!log || !model} onClick={() => void execute()}>Run independent Petri alignment</button>
      </fieldset>
      {pending && <p role="status">Aligning… <button className="btn-ghost" onClick={() => {controller.current?.abort(); controller.current = null; setPending(false);}}>Cancel alignment request</button></p>}
      {error && <p className="error-banner" role="alert">{error}</p>}
    </section>
    {run && input && table && <section className="panel" aria-label="Complete independent alignment inspection">
      <div className="panel-head"><h2>Complete Petri alignment result</h2><span className="panel-meta">{run.value.result.variant} · {input.adapter}</span></div>
      <p>{input.dataset} against {input.model}: {input.evaluated_case_count} evaluated cases · {input.evaluated_event_count} events. Original import: {input.source_case_count} cases · {input.source_event_count} events. Results retain the inputs and settings of this completed run.</p>
      <p role="status">{petriAlignmentResultSummary(run.value.result)}</p>
      <details><summary>Submitted alignment request and original file hashes</summary><pre>{nativeJson({request: nativeResponseRequest(run.value), log_sha256: run.inputs.log_sha256, model_sha256: run.inputs.model_sha256}, 2)}</pre></details>
      <div className="query-actions">
        <button className="btn-ghost" onClick={() => download(run.raw, "minera-petri-alignment-native.json")}>Download complete alignment native JSON</button>
        <button className="btn-ghost" onClick={() => download(petriAlignmentWorkbenchReport(run), "minera-petri-alignment-report.json")}>Download alignment report</button>
        <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `minera-petri-alignment-${encodeURIComponent(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete alignment table CSV</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.log, run.inputs.log.name)}>Download original alignment log</button>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(run.inputs.model, run.inputs.model.name)}>Download original alignment PNML</button>
      </div>
      <label className="filters-field"><span>Alignment result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>
        {run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}
      </select></label>
      <DiagnosticTableView key={run.raw + ":" + table.id} table={table} noun="alignment" />
      <details><summary>Complete alignment JSON</summary><pre>{run.raw}</pre></details>
    </section>}
  </>;
}
