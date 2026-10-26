import { useEffect, useMemo, useRef, useState } from "react";
import { defaultSimulationInputs, runSimulationWorkspace, type SimulationInputs, type SimulationRun } from "../simulation-workspace";
import type { MonteCarloOptions } from "../types";
import { requestLogQuerySchema, downloadQueryBlob, type LogQuerySchema } from "../log-query";
import { ocelPreparedCsv } from "../ocel-preparation";
import { nativeJson } from "../native-json";
import { cellText } from "../transformation-workspace";
import { computePetriLayout } from "../petri-layout";
import { PetriNet } from "./PetriNet";

export function SimulationWorkspace({ models, live, dataset, onOpenLog, onOpenUnderlying }: {
  models: string[]; live: boolean; dataset: string; onOpenLog?: (file: File) => void;
  onOpenUnderlying?: (file: File, sourceId: string) => void;
}) {
  const [miner, setMiner] = useState(models.includes("inductive") ? "inductive" : models[0] ?? "");
  const [inputs, setInputs] = useState(defaultSimulationInputs);
  const [schema, setSchema] = useState<LogQuerySchema | null>(null); const [run, setRun] = useState<SimulationRun | null>(null);
  const [pending, setPending] = useState(false); const [error, setError] = useState<string | null>(null);
  const request = useRef<AbortController | null>(null); const source = useRef<AbortController | null>(null);
  const resultSequence = useRef(0);
  const refresh = async () => {
    request.current?.abort(); source.current?.abort(); setPending(false); setSchema(null); setRun(null); setError(null);
    if (!live) return;
    const controller = new AbortController(); source.current = controller;
    try { const value = await requestLogQuerySchema(controller.signal); if (source.current === controller && !controller.signal.aborted) setSchema(value); }
    catch (cause) { if (source.current === controller && !controller.signal.aborted) setError(cause instanceof Error ? cause.message : String(cause)); }
  };
  useEffect(() => { void refresh(); return () => { request.current?.abort(); source.current?.abort(); }; }, [live]);
  const start = async () => {
    if (!schema || !miner || pending) return;
    const controller = new AbortController(); request.current = controller; setPending(true); setError(null);
    try { const next = await runSimulationWorkspace(miner, inputs, schema, controller.signal); if (request.current === controller && !controller.signal.aborted) { resultSequence.current += 1; setRun(next); } }
    catch (cause) { if (request.current === controller && !controller.signal.aborted) setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { if (request.current === controller) { request.current = null; setPending(false); } }
  };
  const option = <K extends keyof MonteCarloOptions>(key: K, value: MonteCarloOptions[K]) => setInputs(old => ({ ...old, options: { ...old.options, [key]: value } }));
  const number = (key: keyof MonteCarloOptions, value: string) => option(key, value.trim() ? Number(value) : undefined);
  const text = (key: Exclude<keyof SimulationInputs, "options">, value: string) => setInputs(old => ({ ...old, [key]: value }));
  const disabled = !live || !schema || !miner || pending;
  return <section className="panel conf-panel simulation-workspace" aria-label="Monte Carlo simulation workspace">
    <div className="panel-head"><h2>Monte Carlo Petri-net simulation</h2><span className="panel-meta">One run · complete diagnostics and generated log</span><span className="panel-meta">{dataset}</span></div>
    <p>Simulate the active workflow, then inspect visible case durations, scheduler times, resource intervals and every generated event. XES and diagnostics come from the same native run.</p>
    {!live && <p>Open a CSV or XES source to run a simulation.</p>}
    <button className="btn-ghost" disabled={!live || pending} onClick={() => void refresh()}>Refresh simulation source</button>
    {schema && <p>Source {schema.source_id} · {schema.case_count} cases · {schema.event_count} events</p>}
    <fieldset disabled={disabled}><div className="align-settings-grid">
      <label>Simulation workflow model<select value={miner} onChange={event => { setMiner(event.target.value); option("process_tree_variant", undefined); }}>{models.map(model => <option key={model}>{model}</option>)}</select></label>
      {miner === "inductive" && <label>Simulation process-tree conversion<select value={inputs.options.process_tree_variant ?? "native"} onChange={event => option("process_tree_variant", event.target.value === "native" ? undefined : event.target.value as MonteCarloOptions["process_tree_variant"])}><option value="native">Native default</option><option value="standard">Standard</option><option value="transition_bordered">Transition bordered</option></select></label>}
      {([['simulations','Simulated cases',1],['max_steps','Maximum firings per case',0],['seed','Simulation seed',0],['case_arrival_seconds','Case arrival interval (s)',0],['start','First arrival epoch (s)',undefined],['default_duration_seconds','Fallback duration (s)',0],['default_resources_per_place','Default resources per place',0]] as const).map(([key,label,min]) => <label key={key}>{label}<input type="number" min={min} step="any" value={inputs.options[key] ?? ""} onChange={event => number(key,event.target.value)} /></label>)}
      <label>Simulation activity field<input value={inputs.options.activity_key ?? ""} placeholder="concept:name" onChange={event => option("activity_key", event.target.value || undefined)} /></label>
      <label>Simulation timestamp field<input value={inputs.options.timestamp_key ?? ""} placeholder="time:timestamp" onChange={event => option("timestamp_key", event.target.value || undefined)} /></label>
      <label>Simulation initial marking<input value={inputs.initial} placeholder="native model marking" onChange={event => text("initial",event.target.value)} /></label>
      <label>Simulation final marking<input value={inputs.final} placeholder="native model marking" onChange={event => text("final",event.target.value)} /></label>
      <label>Simulation replay strategy<select value={inputs.options.token_replay_variant ?? "forward"} onChange={event => option("token_replay_variant",event.target.value as "forward"|"backwards")}><option value="forward">Forward</option><option value="backwards">Backwards</option></select></label>
      <label>Fit duration family<select value={inputs.options.force_distribution ?? "native"} onChange={event => option("force_distribution",event.target.value === "native" ? undefined : event.target.value as MonteCarloOptions["force_distribution"])}><option value="native">Native automatic fit</option><option value="normal">Normal</option><option value="uniform">Uniform</option><option value="exponential">Exponential</option></select></label>
      <label><input type="checkbox" checked={inputs.options.stop_at_final !== false} onChange={event => option("stop_at_final",event.target.checked)} /> Stop at final marking</label>
      <label><input type="checkbox" checked={inputs.options.enable_diagnostics !== false} onChange={event => option("enable_diagnostics",event.target.checked)} /> Collect scheduler elapsed times</label>
    </div>
    <details><summary>Indexed resource, duration and stochastic overrides</summary>
      <p>Indices follow the selected native workflow, including silent transitions. The completed run includes its transition inventory and underlying PNML. Leave an override blank for native fitting; explicit weights and durations take precedence over stochastic-map records.</p>
      <label>Simulation transition weights (comma separated)<input value={inputs.weights} placeholder="1,1,1,1" onChange={event => text("weights",event.target.value)} /></label>
      <label>Simulation transition durations (JSON array)<textarea rows={4} value={inputs.durations} placeholder={'[{"distribution":"deterministic","value_seconds":2}]'} onChange={event => text("durations",event.target.value)} /></label>
      <p>Supply one duration per transition. Families: deterministic, exponential, constant0, immediate, normal, uniform, lognormal and gamma. Normal uses mean and standard deviation; uniform uses lower bound and width; lognormal/gamma use shape, location and scale.</p>
      <label>Simulation place resources (JSON object)<textarea rows={2} value={inputs.resources} placeholder={'{"0":2,"1":4}'} onChange={event => text("resources",event.target.value)} /></label>
      <label>Simulation stochastic map (JSON array)<textarea rows={4} value={inputs.stochasticMap} placeholder={'[{"transition":0,"timing":"timed","distribution":"deterministic","parameter":2,"secondary_parameter":0,"tertiary_parameter":0,"activation_weight":1,"priority":0}]'} onChange={event => text("stochasticMap",event.target.value)} /></label>
    </details>
    <details><summary>Native compatibility settings</summary><p>The current virtual scheduler records and validates these settings; they do not alter its timing or enforce a wall-clock deadline.</p><div className="align-settings-grid">
      {([['diagnostic_interval','Simulation diagnostic interval'],['small_scale_factor','Simulation scale factor'],['max_thread_execution_time','Simulation execution setting (s)']] as const).map(([key,label]) => <label key={key}>{label}<input type="number" min={0} step="any" value={inputs.options[key] ?? ""} onChange={event => number(key,event.target.value)} /></label>)}
    </div></details>
    <button className="btn-open" onClick={() => void start()}>Run native simulation</button></fieldset>
    {pending && <p role="status">Simulating… <button className="btn-ghost" onClick={() => { request.current?.abort(); request.current = null; setPending(false); }}>Cancel simulation request</button></p>}
    {error && <div className="error-banner" role="alert">{error}</div>}
    {run && <SimulationInspection key={resultSequence.current} run={run} onOpenLog={onOpenLog} onOpenUnderlying={onOpenUnderlying} />}
  </section>;
}

function SimulationInspection({ run, onOpenLog, onOpenUnderlying }: { run: SimulationRun; onOpenLog?: (file: File) => void; onOpenUnderlying?: (file: File, sourceId: string) => void }) {
  const [selected, setSelected] = useState("diagnostics"); const [page, setPage] = useState(0); const [columnPage, setColumnPage] = useState(0);
  const [kind, setKind] = useState<"places"|"transitions">("places"); const [treeIndex, setTreeIndex] = useState(0); const [intervalPage, setIntervalPage] = useState(0);
  const tables = run.tables, current = tables[selected] ?? tables.diagnostics, model = run.value.model;
  const layout = useMemo(() => computePetriLayout(model,{activities:model.activities.map(name=>({name}))}),[model]);
  const trees = kind === "places" ? run.value.result.output_places_interval_trees : run.value.result.output_transitions_interval_trees;
  const intervals = trees[treeIndex]?.intervals ?? []; const shown = intervals.slice(intervalPage*100,(intervalPage+1)*100);
  const low = shown.length ? Math.min(...shown.map(interval=>interval.start)) : 0; const high = shown.length ? Math.max(...shown.map(interval=>interval.end)) : 1; const scale = (value: number) => 80+(value-low)/Math.max(1,high-low)*820;
  const xes = () => new File([run.value.xes],`minera-${run.value.miner}-simulation.xes`,{type:"application/xml"});
  const pnml = () => new File([run.value.underlying_pnml],`minera-${run.value.miner}-simulation-model.pnml`,{type:"application/xml"});
  const download = (raw:string,name:string,type="application/json")=>downloadQueryBlob(new Blob([raw],{type}),name);
  const visibleColumns=current.columns.slice(columnPage*8,(columnPage+1)*8);
  return <section className="simulation-inspection" aria-label="Complete simulation inspection">
    <h3>Complete simulation results</h3><p>{run.value.result.log.trace_count} generated cases · {run.value.result.log.event_count} events · {run.value.miner} · source {run.value.source_id}</p>
    <p>Visible case durations measure first to last visible event. Scheduler elapsed times also include silent work. Place intervals describe native resource accounting and can extend beyond event times.</p>
    <div className="query-actions">
      <button className="btn-ghost" onClick={()=>download(run.raw,"minera-simulation-workspace.json")}>Download complete simulation JSON</button>
      <button className="btn-ghost" onClick={()=>download(run.report,"minera-simulation-report.json")}>Download simulation report</button>
      <button className="btn-ghost" onClick={()=>downloadQueryBlob(xes(),xes().name)}>Download matching simulation XES</button>
      {onOpenLog&&<button className="btn-ghost" onClick={()=>onOpenLog(xes())}>Mine this simulated log</button>}
      <button className="btn-ghost" onClick={()=>downloadQueryBlob(pnml(),pnml().name)}>Download simulation workflow PNML</button>
      {onOpenUnderlying&&<button className="btn-ghost" onClick={()=>onOpenUnderlying(pnml(),run.value.source_id)}>Check simulation workflow</button>}
    </div>
    <label>Simulation result table<select value={selected} onChange={event=>{setSelected(event.target.value);setPage(0);setColumnPage(0);}}>{Object.entries(tables).map(([id,table])=><option value={id} key={id}>{id.replaceAll("_"," ")} · {table.count} rows</option>)}</select></label>
    <p>{current.count} rows · {current.columns.length} columns. Downloads retain all rows, columns and nested native attributes.</p>
    <button className="btn-ghost" onClick={()=>downloadQueryBlob(ocelPreparedCsv(current),`minera-simulation-${selected}.csv`)}>Download complete simulation table CSV</button>
    <div className="query-table-wrap"><table className="activity-table"><thead><tr>{visibleColumns.map(column=><th scope="col" key={column}>{column}</th>)}</tr></thead><tbody>{Array.from({length:Math.min(25,Math.max(0,current.count-page*25))},(_,i)=>{const row=current.rowAt(page*25+i);return <tr key={page*25+i}>{visibleColumns.map((column,j)=><td key={column}>{cellText(row[columnPage*8+j])}</td>)}</tr>;})}</tbody></table>{!current.count&&<p>No native rows in this table.</p>}</div>
    <div className="query-actions"><button className="btn-ghost" disabled={!page} onClick={()=>setPage(value=>value-1)}>Previous simulation rows</button><span>Page {page+1} of {Math.max(1,Math.ceil(current.count/25))}</span><button className="btn-ghost" disabled={(page+1)*25>=current.count} onClick={()=>setPage(value=>value+1)}>Next simulation rows</button>{current.columns.length>8&&<><button className="btn-ghost" disabled={!columnPage} onClick={()=>setColumnPage(value=>value-1)}>Previous simulation columns</button><span>Columns {columnPage*8+1}–{Math.min((columnPage+1)*8,current.columns.length)} of {current.columns.length}</span><button className="btn-ghost" disabled={(columnPage+1)*8>=current.columns.length} onClick={()=>setColumnPage(value=>value+1)}>Next simulation columns</button></>}</div>
    <h3>Occupancy intervals</h3><div className="align-settings-grid"><label>Interval family<select value={kind} onChange={event=>{setKind(event.target.value as "places"|"transitions");setTreeIndex(0);setIntervalPage(0);}}><option value="places">Places</option><option value="transitions">Transitions</option></select></label><label>Interval element<select value={treeIndex} onChange={event=>{setTreeIndex(Number(event.target.value));setIntervalPage(0);}}>{trees.map((_,i)=><option value={i} key={i}>{kind==="places"?`Place #${i}`:`Transition #${i} · ${model.transitions[i].activity===null?"silent":model.activities[model.transitions[i].activity!]}`}</option>)}</select></label></div>
    <p>{intervals.length} native intervals. The diagram displays at most 100 per page; complete interval CSV tables retain every record.</p>
    {shown.length?<svg role="img" aria-label="Simulation occupancy timeline" viewBox={`0 0 940 ${Math.max(90,shown.length*16+50)}`}><text x="80" y="16">{low} epoch seconds</text><text x="900" y="16" textAnchor="end">{high} epoch seconds</text>{shown.map((interval,i)=><g key={i}><text x="8" y={40+i*16}>#{intervalPage*100+i}</text><line x1={scale(interval.start)} x2={scale(interval.end)} y1={36+i*16} y2={36+i*16} stroke="currentColor" strokeWidth="4"/><circle cx={scale(interval.start)} cy={36+i*16} r="2"/><title>{nativeJson(interval)}</title></g>)}</svg>:<p>This native element has no intervals.</p>}
    {intervals.length>100&&<div className="query-actions"><button className="btn-ghost" disabled={!intervalPage} onClick={()=>setIntervalPage(value=>value-1)}>Previous interval page</button><span>{intervalPage+1} / {Math.ceil(intervals.length/100)}</span><button className="btn-ghost" disabled={(intervalPage+1)*100>=intervals.length} onClick={()=>setIntervalPage(value=>value+1)}>Next interval page</button></div>}
    <PetriNet layout={layout} heading="Simulation workflow indices" title={run.value.miner} disconnectedDescription="transitions with no incident arcs" />
    <details><summary>Complete native simulation response</summary><pre>{run.raw}</pre></details>
  </section>;
}
