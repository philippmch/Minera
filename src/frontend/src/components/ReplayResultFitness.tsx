import { useEffect, useRef, useState } from "react";
import { defaultReplayResultFitnessForm, replayResultExample, runReplayResultFitness, type ReplayInputKind, type ReplayResultVariant, type ReplayResultFitnessRun } from "../replay-result-fitness";
import { downloadQueryBlob } from "../log-query";
import { nativeJson } from "../native-json";
import { diagnosticCsv } from "../model-diagnostics";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";

export function ReplayResultFitness() {
  const [form, setForm] = useState(defaultReplayResultFitnessForm);
  const [run, setRun] = useState<ReplayResultFitnessRun | null>(null);
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const request = useRef<AbortController | null>(null), sequence = useRef(0);
  useEffect(() => () => request.current?.abort(), []);
  const evaluate = async () => {
    if (pending) return;
    const controller = new AbortController(); request.current = controller; setPending(true); setError(null);
    try { const next = await runReplayResultFitness(form, controller.signal); if (request.current === controller && !controller.signal.aborted) { sequence.current++; setRun(next); } }
    catch (cause) { if (request.current === controller && !controller.signal.aborted) setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { if (request.current === controller) { request.current = null; setPending(false); } }
  };
  const loadFile = async (file?: File) => {
    if (!file) return;
    try { const json = await file.text(); setForm(current => ({ ...current, json })); setError(null); }
    catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
  };
  return <section className="panel conf-panel query-workspace" aria-label="Replay-result fitness">
    <div className="panel-head"><h2>Replay-result fitness</h2><span className="panel-meta">Evaluate already-computed replay records</span></div>
    <p>Paste or open replay-result JSON to calculate native fitness without running replay again. Every token record contributes once. Alignment evaluation skips null and uncomputed records.</p>
    <div className="align-settings-grid">
      <label>Replay input kind<select aria-label="Replay input kind" value={form.kind} onChange={event => setForm(current => ({ ...current, kind: event.target.value as ReplayInputKind }))}><option value="token">Token replay records</option><option value="alignment">Alignment replay records</option></select></label>
      <label>Replay-result fitness variant<select aria-label="Replay-result fitness variant" value={form.variant} onChange={event => setForm(current => ({ ...current, variant: event.target.value as ReplayResultVariant }))}><option value="native">Native default</option><option value="token_based">Token based</option><option value="alignment_based">Alignment based</option><option value="auto">Automatic</option></select></label>
      <label>Open replay-result JSON<input aria-label="Open replay-result JSON" type="file" accept=".json,application/json" onChange={event => { void loadFile(event.target.files?.[0]); event.currentTarget.value = ""; }} /></label>
    </div>
    <label className="query-text"><span>Replay-result JSON</span><textarea aria-label="Replay-result JSON" rows={9} value={form.json} onChange={event => setForm(current => ({ ...current, json: event.target.value }))} /></label>
    <p>Token counts accept signed decimal integers or decimal strings. The *_tokens fields take precedence over short count aliases, including explicit zero; null uses the short alias. Token trace_fitness may use "nan", "inf" or "-inf". Additional row metadata stays in the original input.</p>
    <button className="btn-ghost" disabled={pending} onClick={() => setForm(current => ({ ...current, json: replayResultExample(current.kind) }))}>Load replay-result example</button>{" "}
    <button className="btn-open" disabled={pending} onClick={() => void evaluate()}>Evaluate native replay-result fitness</button>
    {pending && <p role="status">Evaluating replay-result fitness…</p>}{error && <p role="alert">{error}</p>}
    {run && <ReplayFitnessInspection key={sequence.current} run={run} />}
  </section>;
}
function ReplayFitnessInspection({ run }: { run: ReplayResultFitnessRun }) {
  const [tableId, setTableId] = useState(run.tables[0]?.id ?? "summary");
  const table = run.tables.find(value => value.id === tableId) ?? run.tables[0];
  const fitness = run.value.result.replay_fitness;
  const save = (suffix: string, text: string, type = "application/json") => downloadQueryBlob(new Blob([text], { type }), "replay-result-fitness-" + suffix);
  return <section aria-label="Complete replay-result fitness inspection">
    <div className="panel-head"><h3>Complete replay-result fitness results</h3><span className="panel-meta">{run.value.input_kind} · {fitness.variant} · {run.value.submitted_rows} submitted rows</span></div>
    <p>{fitness.trace_count} evaluated records · {fitness.fitting_traces} fitting records · fitness {fitness.fitness === null ? "nonfinite" : fitness.fitness} · average trace fitness {fitness.average_trace_fitness === null ? "nonfinite" : fitness.average_trace_fitness}</p>
    {!fitness.computed && <p>Native result is uncomputed: {fitness.error}</p>}
    <div className="query-actions"><button className="btn-ghost" onClick={() => save("native.json", run.raw)}>Download complete replay-result fitness JSON</button>{" "}
    <button className="btn-ghost" onClick={() => save("report.json", nativeJson({ request: run.request, response: run.value }, 2))}>Download replay-result fitness report</button>{" "}
    <button className="btn-ghost" onClick={() => save("input.json", run.request.input_json)}>Download original replay-result JSON</button>{" "}
    <button className="btn-ghost" onClick={() => save("evaluation.json", nativeJson(run.value.result, 2))}>Download replay-result evaluation JSON</button></div>
    <p>Original input, exact token totals, native typed public metrics and every submitted record remain available beyond previews and paging. Completed results retain their original input and settings when controls are edited.</p>
    <div className="align-settings-grid"><label>Replay-result fitness table<select aria-label="Replay-result fitness table" value={table.id} onChange={event => setTableId(event.target.value)}>{run.tables.map(value => <option key={value.id} value={value.id}>{value.title} · {value.rows.length} rows</option>)}</select></label></div>
    <button className="btn-ghost" onClick={() => save(encodeURIComponent(table.id) + ".csv", diagnosticCsv(table), "text/csv;charset=utf-8")}>Download complete replay-result fitness table CSV</button>
    <DiagnosticTableView key={table.id} table={table} noun="replay-result fitness" />
    <details><summary>Complete native replay-result fitness response</summary><pre>{run.raw}</pre></details>
  </section>;
}
