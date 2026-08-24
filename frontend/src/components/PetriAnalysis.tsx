import { useMemo, useState } from "react";
import type { MineraExport, PetriAnalysisFamily, PetriAnalysisOptions, PetriAnalysisResult } from "../types";
import { evaluationModelIds } from "../app-logic";

interface Props {
  data: MineraExport;
  importedAvailable?: boolean;
  onRequest?: (options: PetriAnalysisOptions) => Promise<PetriAnalysisResult>;
}

const list = (value: string, integer: boolean): number[] | undefined => {
  if (!value.trim()) return undefined;
  return value.split(",").map((part) => {
    const number = Number(part.trim());
    if (!Number.isFinite(number) || (integer && !Number.isInteger(number)) || number < 0) {
      throw new Error("lists must contain non-negative finite numbers");
    }
    return number;
  });
};

export function PetriAnalysis({ data, importedAvailable = false, onRequest }: Props) {
  const models = useMemo(() => evaluationModelIds(data, importedAvailable), [data, importedAvailable]);
  const [family, setFamily] = useState<PetriAnalysisFamily>("soundness");
  const [miner, setMiner] = useState(models[0] ?? "");
  const [traceIndex, setTraceIndex] = useState("0");
  const [initial, setInitial] = useState("");
  const [final, setFinal] = useState("");
  const [transitionCosts, setTransitionCosts] = useState("");
  const [modelCosts, setModelCosts] = useState("");
  const [syncCosts, setSyncCosts] = useState("");
  const [logCosts, setLogCosts] = useState("");
  const [splits, setSplits] = useState("");
  const [maxK, setMaxK] = useState("5");
  const [maxStates, setMaxStates] = useState("200000");
  const [fullBootstrap, setFullBootstrap] = useState(true);
  const [diagnostics, setDiagnostics] = useState(true);
  const [returnAsapWhenUnsound, setReturnAsapWhenUnsound] = useState(false);
  const [result, setResult] = useState<PetriAnalysisResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const structuralPairs = (result?.family === "soundness" || result?.family === "woflan")
    ? (result.not_well_handled_pairs ?? result.handled_pairs)
    : undefined;
  const lockingScenarios = (result?.family === "soundness" || result?.family === "woflan") ? result.locking_scenarios : undefined;

  const run = async () => {
    if (!onRequest || !miner) return;
    setLoading(true);
    setError(null);
    try {
      const options: PetriAnalysisOptions = {
        family,
        miner,
        trace_index: Number(traceIndex || 0),
        initial_marking: list(initial, true),
        final_marking: list(final, true),
        transition_costs: list(transitionCosts, false),
        model_costs: list(modelCosts, false),
        sync_costs: list(syncCosts, false),
        log_costs: list(logCosts, false),
        split_indices: list(splits, true),
        max_k: Number(maxK || 0),
        max_states: Number(maxStates || 1),
        full_bootstrap: fullBootstrap,
        diagnostics,
        return_asap_when_unsound: returnAsapWhenUnsound,
      };
      setResult(await onRequest(options));
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      setLoading(false);
    }
  };

  return <section className="panel petri-analysis-panel">
    <div className="panel-head"><h2>Petri analysis</h2><span className="panel-meta">State equation · workflow structure · soundness</span></div>
    <div className="align-settings-grid">
      <label>Model<select value={miner} onChange={(event) => setMiner(event.target.value)} disabled={models.length === 0}>{models.map((id) => <option value={id} key={id}>{id}</option>)}</select></label>
      <label>Family<select value={family} onChange={(event) => { setFamily(event.target.value as PetriAnalysisFamily); setResult(null); }}>{<><option value="soundness">Soundness</option><option value="woflan">Woflan</option><option value="workflow_net">Workflow net</option><option value="marking_equation">Marking equation</option><option value="extended_marking_equation">Extended marking equation</option></>}</select></label>
      {family === "extended_marking_equation" && <label>Trace index<input type="number" min={0} value={traceIndex} onChange={(event) => setTraceIndex(event.target.value)} /></label>}
      {(family === "marking_equation" || family === "extended_marking_equation" || family === "soundness" || family === "woflan") && <>
        <label>Initial marking<input placeholder="default" value={initial} onChange={(event) => setInitial(event.target.value)} /></label>
        <label>Final marking<input placeholder="default" value={final} onChange={(event) => setFinal(event.target.value)} /></label>
      </>}
      {family === "marking_equation" && <label>Transition costs<input placeholder="default" value={transitionCosts} onChange={(event) => setTransitionCosts(event.target.value)} /></label>}
      {family === "extended_marking_equation" && <>
        <label>Model costs<input placeholder="default" value={modelCosts} onChange={(event) => setModelCosts(event.target.value)} /></label>
        <label>Sync costs<input placeholder="0,0,…" value={syncCosts} onChange={(event) => setSyncCosts(event.target.value)} /></label>
        <label>Log costs<input placeholder="default" value={logCosts} onChange={(event) => setLogCosts(event.target.value)} /></label>
        <label>Split indices<input placeholder="auto" value={splits} onChange={(event) => setSplits(event.target.value)} /></label>
        <label>Max k<input type="number" min={0} value={maxK} onChange={(event) => setMaxK(event.target.value)} /></label>
        <label>Full bootstrap<select value={String(fullBootstrap)} onChange={(event) => setFullBootstrap(event.target.value === "true")}><option value="true">On</option><option value="false">Off</option></select></label>
      </>}
      {(family === "extended_marking_equation" || family === "soundness" || family === "woflan" || family === "marking_equation") && <label>State limit<input type="number" min={1} value={maxStates} onChange={(event) => setMaxStates(event.target.value)} /></label>}
      {(family === "soundness" || family === "woflan") && <label>Diagnostics<select value={String(diagnostics)} onChange={(event) => setDiagnostics(event.target.value === "true")}><option value="true">On</option><option value="false">Off</option></select></label>}
      {(family === "soundness" || family === "woflan") && <label>Return early<select value={String(returnAsapWhenUnsound)} onChange={(event) => setReturnAsapWhenUnsound(event.target.value === "true")}><option value="false">Off</option><option value="true">On</option></select></label>}
    </div>
    {models.length === 0 && <p className="conf-inline-note">No Petri model is available in this analysis.</p>}
    <button type="button" className="button" disabled={!onRequest || !miner || loading} onClick={() => void run()}>{loading ? "Analyzing…" : "Run Petri analysis"}</button>
    {error && <p className="error-inline">{error}</p>}
    {structuralPairs && <div className="conf-inline-note">
      <strong>Structural pair diagnostics: {structuralPairs.length}</strong>
      {structuralPairs.length > 0 && <ul>{structuralPairs.map((pair, index) =>
        <li key={`${pair[0]}-${pair[1]}-${index}`}>
          {result?.structural_node_labels?.[pair[0]] ?? `node ${pair[0]}`} → {result?.structural_node_labels?.[pair[1]] ?? `node ${pair[1]}`}
        </li>)}</ul>}
    </div>}
    {result?.family === "woflan" && result.short_circuit_valid !== undefined && <div className="conf-inline-note">
      <strong>Short-circuited workflow</strong>
      <div>Valid: {result.short_circuit_valid ? "yes" : "no"}</div>
      {result.short_circuit_valid && <div>Transition: {result.short_circuit_transition ?? "none"}</div>}
      {result.short_circuit_strongly_connected !== undefined &&
        <div>Strongly connected: {result.short_circuit_strongly_connected ? "yes" : "no"}</div>}
      {result.short_circuit_reachability_exhausted &&
        <div>Short-circuit reachability reached the state limit.</div>}
    </div>}
    {(result?.family === "soundness" || result?.family === "woflan") && (result.dead_tasks || lockingScenarios) && <div className="conf-inline-note">
      <strong>Soundness witnesses</strong>
      {result.dead_tasks && <div>Dead tasks: {result.dead_tasks.length}</div>}
      {result.deadlock_witnesses && <div>Deadlock witnesses: {result.deadlock_witnesses.length}</div>}
      {result.improper_witnesses && <div>Improper-marking witnesses: {result.improper_witnesses.length}</div>}
      {result.completion_witness && <div>Completion path length: {result.completion_witness.length}</div>}
      {lockingScenarios && <div>Locking scenarios: {lockingScenarios.length}</div>}
    </div>}
    {result && <pre className="analysis-json">{JSON.stringify(result, null, 2)}</pre>}
  </section>;
}
