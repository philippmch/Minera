import { Fragment, useEffect, useRef, useState } from "react";
import {
  type Alignments as AlignmentsResult,
  type AlignmentMove,
  type AlignmentDiagnostics,
  type AlignmentOptions,
  type AlignmentVariant,
  type AlignmentCostProfile,
  DEFAULT_ALIGNMENT_OPTIONS,
  type Decisions as DecisionsResult,
  type DecisionPoint,
  type MineraExport,
  type ModelConformance,
  type DfgAlignmentOptions,
  type DfgAlignmentResult,
  type EditDistanceOptions,
  type EditDistanceResult,
  type EvaluationFamily,
  type EvaluationOptions,
  type ModelEvaluation,
  type EmdOptions,
  type EmdResult,
  type ProcessTreeAlignmentOptions,
  type ProcessTreeAlignmentResult,
  type DecomposedAlignmentOptions,
  type DecomposedAlignmentResult,
  type PetriAntiAlignmentOptions,
  type PetriAntiAlignmentResult,
  type PetriMultiAlignmentOptions,
  type PetriMultiAlignmentResult,
  type ProcessTreeReductionOptions,
  type ProcessTreeReductionResult,
  type PetriAnalysisOptions,
  type PetriAnalysisResult,
  type ProcessTreePlayoutOptions,
  type BasicPlayoutOptions,
  type BasicPlayoutVisitedResult,
  type ExtensivePlayoutVisitedResult,
  type StochasticPlayoutOptions,
  type MonteCarloOptions,
  type DfgPlayoutOptions,
  type PetriUsageComparison,
  type ConceptDriftResult,
  numberFormat,
} from "../types";
import { fetchDfgPlayout, fetchPlayout, fetchBasicPlayoutVisited, fetchExtensivePlayoutVisited, fetchMonteCarloPlayout, fetchMonteCarloResult, parseDfgCostMapInput, parseMarkingInput, comparePetriUsage, type ImportedModelCheck, type ConceptDriftOptions, type DecisionMiningOptions } from "../api";
import {
  CONFORMANCE_METHODS,
  deviatingActivities,
  flattenDecisionRules,
} from "../conformance";
import { MINER_GROUPS } from "../miners";
import { evaluationModelIds } from "../app-logic";
import { PetriAnalysis } from "./PetriAnalysis";

type Step = "miner" | "method" | "result";

/** The conformance tab's extra, non-miner model: a user-uploaded PNML or BPMN model. */
const IMPORTED = "imported";

const percent = (value: number) => `${(value * 100).toFixed(1)}%`;

// Maps a [0, 1] quality score to a traffic-light class so fitness and
// precision read at a glance.
const grade = (value: number) =>
  value >= 0.95 ? "good" : value >= 0.8 ? "fair" : "poor";

// A small wizard mirroring ProcessModel: conformance checks a log against a
// discovered model, so it takes two choices — which miner produced the model,
// then which conformance method to replay with — before showing results.
// Both come from shared registries (MINER_GROUPS, CONFORMANCE_METHODS) so new
// miners and methods appear here automatically.
export function Conformance({
  data,
  onRequestAlignment,
  onRequestDecisions,
  onCheckModel,
  onRequestDfg,
  onRequestEditDistance,
  onRequestProcessTree,
  onRequestDecomposed,
  onRequestPetriAnti,
  onRequestPetriMulti,
  onRequestReduction,
  onRequestEvaluation,
  onRequestPetriAnalysis,
  onRequestEmd,
  onRequestConceptDrift,
}: {
  data: MineraExport;
  onRequestAlignment: (minerId: string, options?: AlignmentOptions) => Promise<AlignmentsResult>;
  onRequestDecisions?: (minerId: string, options?: DecisionMiningOptions) => Promise<DecisionsResult>;
  onCheckModel?: (file: File) => Promise<ImportedModelCheck>;
  onRequestDfg?: (options?: DfgAlignmentOptions) => Promise<DfgAlignmentResult>;
  onRequestEditDistance?: (reference: File, options?: EditDistanceOptions) => Promise<EditDistanceResult>;
  onRequestProcessTree?: (options?: ProcessTreeAlignmentOptions) => Promise<ProcessTreeAlignmentResult>;
  onRequestDecomposed?: (miner: string, options?: DecomposedAlignmentOptions) => Promise<DecomposedAlignmentResult>;
  onRequestPetriAnti?: (miner: string, options?: PetriAntiAlignmentOptions) => Promise<PetriAntiAlignmentResult>;
  onRequestPetriMulti?: (miner: string, options?: PetriMultiAlignmentOptions) => Promise<PetriMultiAlignmentResult>;
  onRequestReduction?: (traceIndex: number, options?: ProcessTreeReductionOptions) => Promise<ProcessTreeReductionResult>;
  onRequestEvaluation?: (miner: string, options?: EvaluationOptions) => Promise<ModelEvaluation>;
  onRequestPetriAnalysis?: (options?: PetriAnalysisOptions) => Promise<PetriAnalysisResult>;
  onRequestEmd?: (reference: File, options?: EmdOptions) => Promise<EmdResult>;
  onRequestConceptDrift?: (options?: ConceptDriftOptions) => Promise<ConceptDriftResult>;
}) {
  const analysisIdentity = useRef({ data, id: 0 });
  if (analysisIdentity.current.data !== data) {
    analysisIdentity.current = { data, id: analysisIdentity.current.id + 1 };
  }
  const analysisId = analysisIdentity.current.id;
  const [step, setStep] = useState<Step>("miner");
  const [minerId, setMinerId] = useState<string | null>(null);
  const [methodId, setMethodId] = useState<string | null>(null);

  // A user-uploaded PNML or BPMN model, checked server-side against the mined log.
  const [imported, setImported] = useState<ImportedModelCheck | null>(null);
  const [importedAnalysisId, setImportedAnalysisId] = useState<number | null>(null);
  const [importedGeneration, setImportedGeneration] = useState(0);
  const [importing, setImporting] = useState(false);
  const [importError, setImportError] = useState<string | null>(null);
  const uploadSequence = useRef(0);

  // A re-mine invalidates the uploaded model's results (and the server drops
  // its cached net), so fall back to the start of the wizard.
  useEffect(() => {
    uploadSequence.current += 1;
    setImported(null);
    setImportedAnalysisId(null);
    setImportedGeneration(0);
    setImporting(false);
    setImportError(null);
    setMinerId(null);
    setStep("miner");
  }, [data]);

  const isImported = minerId === IMPORTED;
  const currentImported = importedAnalysisId === analysisId ? imported : null;
  const miner = MINER_GROUPS.find((g) => g.id === minerId) ?? null;
  // Used in prose ("The … model isn't sound"), so lowercase for the upload.
  const minerName = isImported ? "imported" : miner?.name ?? "";
  const method = CONFORMANCE_METHODS.find((m) => m.id === methodId) ?? null;

  const chooseMiner = (id: string) => {
    setMinerId(id);
    setMethodId(null);
    setStep("method");
  };
  const chooseMethod = (id: string) => {
    setMethodId(id);
    setStep("result");
  };

  const uploadModel = async (file: File) => {
    if (!onCheckModel) return;
    const sequence = ++uploadSequence.current;
    const requestedAnalysis = analysisId;
    setImporting(true);
    setImportError(null);
    try {
      const result = await onCheckModel(file);
      if (sequence !== uploadSequence.current || requestedAnalysis !== analysisIdentity.current.id) return;
      setImported(result);
      setImportedAnalysisId(requestedAnalysis);
      setImportedGeneration((current) => current + 1);
      chooseMiner(IMPORTED);
    } catch (cause) {
      if (sequence === uploadSequence.current && requestedAnalysis === analysisIdentity.current.id) {
        setImportError(cause instanceof Error ? cause.message : String(cause));
      }
    } finally {
      if (sequence === uploadSequence.current && requestedAnalysis === analysisIdentity.current.id) setImporting(false);
    }
  };

  // The backend checks every discovered model both ways, keyed by miner id, so
  // any miner × any method has results — as long as this export carries them.
  // The imported model's results come from the upload (or are embedded when a
  // CLI export was produced with --check-pnml / --check-bpmn).
  const models = isImported
    ? currentImported?.model ?? data.conformance?.[IMPORTED]
    : miner
      ? data.conformance?.[miner.id]
      : undefined;
  const supported = Boolean(models && method);

  // An uploaded model can name activities the log never shows; its response
  // carries the extended registry, so the result panels resolve every id.
  const viewData =
    isImported && currentImported ? { ...data, activities: currentImported.activities } : data;
  const petriMiners = [
    data.inductive_petri_net ? { id: "inductive", label: "Inductive" } : null,
    data.petri_net ? { id: "alpha", label: "Alpha" } : null,
    data.heuristic_petri_net ? { id: "heuristic", label: "Heuristic" } : null,
    data.powl_petri_net ? { id: "powl", label: "POWL" } : null,
  ].filter((candidate): candidate is { id: string; label: string } => candidate !== null);

  return (
    <div className="miner-flow reveal">
      <nav className="miner-trail">
        <button
          className={step === "miner" ? "trail-item active" : "trail-item"}
          onClick={() => setStep("miner")}
        >
          Miner
        </button>
        {minerId && step !== "miner" && (
          <>
            <span className="trail-sep">›</span>
            <button
              className={step === "method" ? "trail-item active" : "trail-item"}
              onClick={() => setStep("method")}
            >
              {isImported ? "Imported model" : miner?.name}
            </button>
          </>
        )}
        {minerId && method && step === "result" && (
          <>
            <span className="trail-sep">›</span>
            <span className="trail-item current">{method.name}</span>
          </>
        )}
      </nav>

      {step === "miner" && (
        <div className="miner-step">
          <p className="miner-step-label">Step 1 — Model to check</p>
          <h2 className="miner-step-title">Choose a miner</h2>
          <div className="miner-cards">
            {MINER_GROUPS.filter((g) => g.id !== "log_skeleton" && g.id !== "declare" && g.id !== "footprints").map((g) => (
              <button
                key={g.id}
                className="miner-card"
                onClick={() => chooseMiner(g.id)}
              >
                <span className="miner-card-name">{g.name}</span>
                <span className="miner-card-count">
                  {g.variants.length} variant{g.variants.length > 1 ? "s" : ""}
                </span>
                <p className="miner-card-desc">{g.tagline}</p>
                <span className="miner-card-go">Select →</span>
              </button>
            ))}
            {currentImported || data.conformance?.[IMPORTED] ? (
              <button className="miner-card" onClick={() => chooseMiner(IMPORTED)}>
                <span className="miner-card-name">Imported model</span>
                <span className="miner-card-count">PNML / BPMN / PTML</span>
                <p className="miner-card-desc">
                  A model you supplied, checked against this log.
                </p>
                <span className="miner-card-go">Select →</span>
              </button>
            ) : (
              <label className="miner-card miner-card-upload">
                <span className="miner-card-name">Your model</span>
                <span className="miner-card-count">PNML / BPMN / PTML</span>
                <p className="miner-card-desc">
                  Check the log against a model from another tool — upload a
                  PNML workflow net, a BPMN 2.0 process, or a PTML process
                  tree.
                </p>
                <span className="miner-card-go">
                  {importing ? "Checking…" : "Upload →"}
                </span>
                {importError && (
                  <p className="miner-card-error">{importError}</p>
                )}
                <input
                  type="file"
                  accept=".pnml,.bpmn,.ptml,.xml"
                  hidden
                  disabled={importing}
                  onChange={(e) => {
                    const file = e.target.files?.[0];
                    e.target.value = "";
                    if (file) void uploadModel(file);
                  }}
                />
              </label>
            )}
          </div>
        </div>
      )}

      {step === "method" && minerId && (
        <div className="miner-step">
          <p className="miner-step-label">Step 2 — Conformance method</p>
          <h2 className="miner-step-title">Choose a method</h2>
          <div className="miner-cards">
            {CONFORMANCE_METHODS.map((m) => (
              <button
                key={m.id}
                className="miner-card"
                onClick={() => chooseMethod(m.id)}
              >
                <span className="miner-card-name">{m.name}</span>
                <p className="miner-card-desc">{m.description}</p>
                <span className="miner-card-go">Check →</span>
              </button>
            ))}
          </div>
        </div>
      )}

      {step === "result" &&
        minerId &&
        method &&
        (supported && models ? (
          method.artifact === "alignments" ? (
            <Alignments
              data={viewData}
              minerId={minerId}
              minerName={minerName}
              model={models}
              onRequestAlignment={onRequestAlignment}
            />
          ) : method.artifact === "footprints" ? (
            <Footprints
              data={viewData}
              footprints={models.footprints}
              minerName={minerName}
            />
          ) : method.artifact === "decisions" ? (
            <DecisionsView
              data={viewData}
              minerId={minerId}
              minerName={minerName}
              onRequestDecisions={onRequestDecisions}
            />
          ) : (
            <TokenReplay
              data={viewData}
              conformance={models.token_replay}
              minerName={minerName}
              sound={models.sound}
            />
          )
        ) : (
          <section className="panel map-panel">
            <div className="model-empty">
              No conformance results for {minerName} via {method.name}. This
              export predates per-model conformance — re-open the event log to
              check it against every discovered model.
            </div>
          </section>
        ))}
      <ModelEvaluationPanel key={`evaluation-${analysisId}-${importedGeneration}`} data={data} importedAvailable={Boolean(currentImported || data.conformance?.imported)} onRequestEvaluation={onRequestEvaluation} />
      <PetriAnalysis key={`petri-analysis-${analysisId}-${importedGeneration}`} data={data} importedAvailable={Boolean(currentImported || data.conformance?.imported)} onRequest={onRequestPetriAnalysis} />
      <OtherAlignments key={`other-alignments-${analysisId}-${importedGeneration}`} data={data} importedAvailable={Boolean(currentImported || data.conformance?.imported)} onRequestDfg={onRequestDfg} onRequestEditDistance={onRequestEditDistance} onRequestProcessTree={onRequestProcessTree} onRequestDecomposed={onRequestDecomposed} onRequestPetriAnti={onRequestPetriAnti} onRequestPetriMulti={onRequestPetriMulti} onRequestReduction={onRequestReduction} onRequestEmd={onRequestEmd} />
      <ProcessTreePlayoutPanel dataset={data.dataset} available={Boolean(data.process_tree)} />
      <PetriPlayoutPanel key={`petri-playout-${analysisId}`} dataset={data.dataset} miners={petriMiners} />
      <StochasticPetriPlayoutPanel key={`stochastic-playout-${analysisId}`} dataset={data.dataset} miners={petriMiners} />
      <MonteCarloPlayoutPanel key={`monte-carlo-playout-${analysisId}`} dataset={data.dataset} miners={petriMiners} />
      <DfgPlayoutPanel dataset={data.dataset} available={data.edges.length > 0} />
      <PetriUsageComparisonPanel />
      <ConceptDriftPanel key={`concept-drift-${analysisId}`} onRequest={onRequestConceptDrift} />
    </div>
  );
}

function ConceptDriftPanel({ onRequest }: { onRequest?: (options?: ConceptDriftOptions) => Promise<ConceptDriftResult> }) {
  const [result, setResult] = useState<ConceptDriftResult | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [subLogSize, setSubLogSize] = useState(50);
  const [windowSize, setWindowSize] = useState(8);
  const [numPermutations, setNumPermutations] = useState(100);
  const [threshold, setThreshold] = useState(0.5);
  const [maxChangePoints, setMaxChangePoints] = useState(5);
  const [seed, setSeed] = useState(0);
  const [includeTimestamps, setIncludeTimestamps] = useState(false);
  const [activityAttribute, setActivityAttribute] = useState("concept:name");
  const [timestampAttribute, setTimestampAttribute] = useState("time:timestamp");
  const [caseIdAttribute, setCaseIdAttribute] = useState("case:concept:name");
  const requestId = useRef(0);
  const invalidSettings = !Number.isSafeInteger(subLogSize) || subLogSize < 1 ||
    !Number.isSafeInteger(windowSize) || windowSize < 1 ||
    !Number.isSafeInteger(numPermutations) || numPermutations < 1 || numPermutations > 1_000_000 ||
    !Number.isFinite(threshold) || threshold < 0 || threshold > 1 ||
    !Number.isSafeInteger(maxChangePoints) || maxChangePoints < 0 || maxChangePoints > 1_000_000 ||
    !Number.isSafeInteger(seed) || seed < 0;
  const run = async () => {
    if (!onRequest) return;
    const id = ++requestId.current;
    setBusy(true); setError(null); setResult(null);
    try {
      const next = await onRequest({
        drift_variant: "bose",
        drift_sub_log_size: subLogSize,
        drift_window_size: windowSize,
        drift_num_permutations: numPermutations,
        drift_threshold: threshold,
        drift_max_no_change_points: maxChangePoints,
        drift_seed: seed,
        drift_include_timestamps: includeTimestamps,
        drift_activity_attribute: activityAttribute.trim() || "concept:name",
        drift_timestamp_attribute: timestampAttribute.trim() || "time:timestamp",
        drift_case_id_attribute: caseIdAttribute.trim() || "case:concept:name",
      });
      if (id === requestId.current) setResult(next);
    } catch (cause) {
      if (id === requestId.current) setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (id === requestId.current) setBusy(false);
    }
  };
  return <section className="panel other-align-card">
    <h3>Concept drift</h3>
    <p>Detect distribution changes across consecutive sub-logs.</p>
    <div className="align-settings-grid">
      <label>Sub-log size<input type="number" min={1} step={1} value={subLogSize} onChange={event => setSubLogSize(event.target.valueAsNumber)} /></label>
      <label>Window size<input type="number" min={1} step={1} value={windowSize} onChange={event => setWindowSize(event.target.valueAsNumber)} /></label>
      <label>Permutations<input type="number" min={1} max={1_000_000} step={1} value={numPermutations} onChange={event => setNumPermutations(event.target.valueAsNumber)} /></label>
      <label>Threshold<input type="number" min={0} max={1} step="any" value={threshold} onChange={event => setThreshold(event.target.valueAsNumber)} /></label>
      <label>Maximum change points<input type="number" min={0} max={1_000_000} step={1} value={maxChangePoints} onChange={event => setMaxChangePoints(event.target.valueAsNumber)} /></label>
      <label>Random seed<input type="number" min={0} step={1} value={seed} onChange={event => setSeed(event.target.valueAsNumber)} /></label>
      <label>Activity attribute<input value={activityAttribute} onChange={event => setActivityAttribute(event.target.value)} /></label>
      <label>Timestamp attribute<input value={timestampAttribute} onChange={event => setTimestampAttribute(event.target.value)} /></label>
      <label>Case-id attribute<input value={caseIdAttribute} onChange={event => setCaseIdAttribute(event.target.value)} /></label>
      <label><input type="checkbox" checked={includeTimestamps} onChange={event => setIncludeTimestamps(event.target.checked)} /> Include drift timestamps</label>
    </div>
    <button className="button" disabled={!onRequest || busy || invalidSettings || !activityAttribute.trim() || !timestampAttribute.trim() || !caseIdAttribute.trim()} onClick={() => void run()}>{busy ? "Detecting…" : "Detect concept drift"}</button>
    {error && <div className="error-banner">{error}</div>}
    {result && <small>{result.change_points.length} change point(s), {result.returned_sublogs.length} segment(s) across {result.sublogs.length} sub-logs.</small>}
  </section>;
}

function PetriUsageComparisonPanel() {
  const [reference, setReference] = useState<File | null>(null);
  const [miner, setMiner] = useState("inductive");
  const [activityAttribute, setActivityAttribute] = useState("");
  const [initialMarking, setInitialMarking] = useState("");
  const [finalMarking, setFinalMarking] = useState("");
  const [walkThroughHidden, setWalkThroughHidden] = useState(true);
  const [reachFinalThroughHidden, setReachFinalThroughHidden] = useState(true);
  const [stopImmediatelyUnfit, setStopImmediatelyUnfit] = useState(false);
  const [result, setResult] = useState<PetriUsageComparison | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const run = async () => {
    if (!reference) return;
    setBusy(true); setError(null); setResult(null);
    const parseMarking = (value: string): number[] | undefined => {
      const trimmed = value.trim();
      return trimmed === "" ? undefined : trimmed.split(",").map((entry) => Number(entry.trim()));
    };
    try {
      setResult(await comparePetriUsage(reference, miner, {
        activity_attribute: activityAttribute.trim() || undefined,
        initial_marking: parseMarking(initialMarking),
        final_marking: parseMarking(finalMarking),
        walk_through_hidden_trans: walkThroughHidden,
        try_to_reach_final_marking_through_hidden: reachFinalThroughHidden,
        stop_immediately_unfit: stopImmediatelyUnfit,
      }));
    }
    catch (cause) { setResult(null); setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  const rows = result?.transitions
    .map((row) => ({ ...row, delta: row.log2_occurrences - row.log1_occurrences }))
    .sort((a, b) => Math.abs(b.delta) - Math.abs(a.delta)).slice(0, 8) ?? [];
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Petri element usage comparison</h2><span className="panel-meta">Compare replay counts with a reference log</span></div>
    <div className="align-settings-grid">
      <label>Reference log<input type="file" accept=".csv,.xes,.xes.gz,.gz" onChange={e => { setReference(e.target.files?.[0] ?? null); setResult(null); setError(null); }} /></label>
      <label>Miner<select value={miner} onChange={e => { setMiner(e.target.value); setResult(null); setError(null); }}><option value="inductive">Inductive</option><option value="alpha">Alpha</option><option value="heuristic">Heuristic</option><option value="powl">POWL</option></select></label>
      <label>Activity attribute<input value={activityAttribute} placeholder="optional: business_activity" onChange={e => { setActivityAttribute(e.target.value); setResult(null); setError(null); }} /></label>
      <label>Initial marking<input value={initialMarking} placeholder="optional: 1,0,…" onChange={e => { setInitialMarking(e.target.value); setResult(null); setError(null); }} /></label>
      <label>Final marking<input value={finalMarking} placeholder="optional: 0,…,1" onChange={e => { setFinalMarking(e.target.value); setResult(null); setError(null); }} /></label>
      <label className="checkbox-label"><input type="checkbox" checked={walkThroughHidden} onChange={e => { setWalkThroughHidden(e.target.checked); setResult(null); }} /> Route hidden transitions before events</label>
      <label className="checkbox-label"><input type="checkbox" checked={reachFinalThroughHidden} onChange={e => { setReachFinalThroughHidden(e.target.checked); setResult(null); }} /> Route hidden transitions after traces</label>
      <label className="checkbox-label"><input type="checkbox" checked={stopImmediatelyUnfit} onChange={e => { setStopImmediatelyUnfit(e.target.checked); setResult(null); }} /> Stop each trace at first deviation</label>
    </div>
    <button className="button" disabled={!reference || busy} onClick={() => void run()}>{busy ? "Comparing…" : "Compare element usage"}</button>
    {error && <p className="conf-inline-note">{error}</p>}
    {rows.length > 0 && <table className="activity-table deviation-table"><thead><tr><th>Transition</th><th className="num">Log 1</th><th className="num">Log 2</th><th className="num">Δ</th></tr></thead><tbody>{rows.map(row => <tr key={row.id}><td><span style={{ color: row.color ?? "inherit" }}>{row.label ?? row.id}</span></td><td className="num">{numberFormat.format(row.log1_occurrences)}</td><td className="num">{numberFormat.format(row.log2_occurrences)}</td><td className="num">{row.delta > 0 ? "+" : ""}{numberFormat.format(row.delta)}</td></tr>)}</tbody></table>}
  </section>;
}

function ProcessTreePlayoutPanel({ dataset, available }: { dataset: string; available: boolean }) {
  const [options, setOptions] = useState<ProcessTreePlayoutOptions>({ variant: "TOPBOTTOM", traces: 100, max_trace_cap: 1000 });
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const run = async () => {
    setBusy(true); setError(null);
    try {
      const xes = await fetchPlayout("process_tree", options);
      const blob = new Blob([xes], { type: "application/xml" });
      const anchor = document.createElement("a"); anchor.href = URL.createObjectURL(blob);
      anchor.download = `${dataset.replace(/\.[^.]+$/, "")}-process_tree-playout.xes`; anchor.click();
      URL.revokeObjectURL(anchor.href);
    } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  const number = (name: keyof ProcessTreePlayoutOptions, value: string) => setOptions((current) => ({ ...current, [name]: value === "" ? undefined : Number(value) }));
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Process-tree playout</h2><span className="panel-meta">Synthetic XES from the cached process tree</span></div>
    {!available ? <p className="conf-inline-note">No inductive process tree is present in this analysis.</p> : <>
      <div className="align-settings-grid">
        <label>Variant<select value={options.variant} onChange={e => setOptions({ ...options, variant: e.target.value as ProcessTreePlayoutOptions["variant"] })}><option value="BASIC_PLAYOUT">Basic playout</option><option value="EXTENSIVE">Extensive</option><option value="TOPBOTTOM">Top-bottom</option></select></label>
        <label>Traces<input type="number" min={1} value={options.traces ?? ""} onChange={e => number("traces", e.target.value)} /></label>
        <label>Max steps<input type="number" min={0} value={options.max_steps ?? ""} onChange={e => number("max_steps", e.target.value)} /></label>
        <label>Seed<input type="number" min={0} value={options.seed ?? ""} onChange={e => number("seed", e.target.value)} /></label>
        <label>Min length<input type="number" min={0} value={options.min_length ?? ""} onChange={e => number("min_length", e.target.value)} /></label>
        <label>Max length<input type="number" min={0} value={options.max_length ?? ""} onChange={e => number("max_length", e.target.value)} /></label>
        <label>Max loop occurrences<input type="number" min={0} value={options.max_loop_occurrences ?? ""} onChange={e => number("max_loop_occurrences", e.target.value)} /></label>
        <label>Max trace cap<input type="number" min={1} value={options.max_trace_cap ?? ""} onChange={e => number("max_trace_cap", e.target.value)} /></label>
      </div>
      <button className="button" disabled={busy} onClick={() => void run()}>{busy ? "Generating…" : "Download process-tree playout"}</button>
      {error && <p className="conf-inline-note">{error}</p>}
    </>}
  </section>;
}

function PetriPlayoutPanel({ dataset, miners }: { dataset: string; miners: Array<{ id: string; label: string }> }) {
  type VisitedResult = BasicPlayoutVisitedResult | ExtensivePlayoutVisitedResult;
  const [mode, setMode] = useState<"basic" | "extensive">("basic");
  const [miner, setMiner] = useState(miners[0]?.id ?? "inductive");
  const [options, setOptions] = useState<BasicPlayoutOptions>({ traces: 100, max_steps: 1000, max_length: 8 });
  const [initialText, setInitialText] = useState("");
  const [finalText, setFinalText] = useState("");
  const [returnVisited, setReturnVisited] = useState(false);
  const [visited, setVisited] = useState<VisitedResult | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const run = async () => {
    setBusy(true); setError(null); setVisited(null);
    try {
      const selectedMiner = miners.some((candidate) => candidate.id === miner) ? miner : (miners[0]?.id ?? "inductive");
      const common: BasicPlayoutOptions = {
        traces: options.traces,
        initial_marking: parseMarkingInput(initialText),
        final_marking: parseMarkingInput(finalText),
        ...(mode === "basic" ? { mode, max_steps: options.max_steps, seed: options.seed } : {
          mode, max_length: options.max_length, max_marking_occ: options.max_marking_occ,
        }),
      };
      if (returnVisited) {
        const result = mode === "basic"
          ? await fetchBasicPlayoutVisited(selectedMiner, common)
          : await fetchExtensivePlayoutVisited(selectedMiner, common);
        setVisited(result);
        const blob = new Blob([JSON.stringify(result, null, 2)], { type: "application/json" });
        const anchor = document.createElement("a"); anchor.href = URL.createObjectURL(blob);
        anchor.download = `${dataset.replace(/\.[^.]+$/, "")}-${selectedMiner}-${mode}-playout.json`; anchor.click();
        URL.revokeObjectURL(anchor.href);
      } else {
        const xes = await fetchPlayout(selectedMiner, common);
        const blob = new Blob([xes], { type: "application/xml" });
        const anchor = document.createElement("a"); anchor.href = URL.createObjectURL(blob);
        anchor.download = `${dataset.replace(/\.[^.]+$/, "")}-${selectedMiner}-${mode}-playout.xes`; anchor.click();
        URL.revokeObjectURL(anchor.href);
      }
    } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  const number = (name: keyof BasicPlayoutOptions, value: string) =>
    setOptions((current) => ({ ...current, [name]: value === "" ? undefined : Number(value) }));
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Petri-net playout</h2><span className="panel-meta">Basic sampling or extensive enumeration from a cached model</span></div>
    {miners.length === 0 ? <p className="conf-inline-note">No Petri net is present in this analysis.</p> : <>
      <div className="align-settings-grid">
        <label>Miner<select value={miner} onChange={e => setMiner(e.target.value)}>{miners.map(candidate => <option key={candidate.id} value={candidate.id}>{candidate.label}</option>)}</select></label>
        <label>Mode<select value={mode} onChange={e => { setMode(e.target.value as "basic" | "extensive"); setVisited(null); setError(null); }}><option value="basic">Basic sampling</option><option value="extensive">Extensive enumeration</option></select></label>
        <label>Traces<input type="number" min={1} value={options.traces ?? ""} onChange={e => number("traces", e.target.value)} /></label>
        {mode === "basic" ? <>
          <label>Max steps<input type="number" min={0} value={options.max_steps ?? ""} onChange={e => number("max_steps", e.target.value)} /></label>
          <label>Seed<input type="number" min={0} value={options.seed ?? ""} onChange={e => number("seed", e.target.value)} /></label>
        </> : <>
          <label>Max length<input type="number" min={0} value={options.max_length ?? ""} onChange={e => number("max_length", e.target.value)} /></label>
          <label>Max marking occurrences<input type="number" min={1} value={options.max_marking_occ ?? ""} onChange={e => number("max_marking_occ", e.target.value)} /></label>
        </>}
        <label>Initial marking<input value={initialText} placeholder="optional: 1,0" onChange={e => setInitialText(e.target.value)} /></label>
        <label>Final marking<input value={finalText} placeholder="optional: 0,1" onChange={e => setFinalText(e.target.value)} /></label>
        <label><input type="checkbox" checked={returnVisited} onChange={e => setReturnVisited(e.target.checked)} /> Return visited elements</label>
      </div>
      <button className="button" disabled={busy} onClick={() => void run()}>{busy ? "Generating…" : returnVisited ? "Download visited playout" : "Download Petri playout"}</button>
      {error && <p className="conf-inline-note">{error}</p>}
      {visited && <details className="json-preview"><summary>Show visited elements</summary><pre>{JSON.stringify(visited, null, 2)}</pre></details>}
    </>}
  </section>;
}

function StochasticPetriPlayoutPanel({ dataset, miners }: { dataset: string; miners: Array<{ id: string; label: string }> }) {
  const [options, setOptions] = useState<StochasticPlayoutOptions>({ traces: 1000, stop_at_final: true });
  const [miner, setMiner] = useState(miners[0]?.id ?? "inductive");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const run = async () => {
    setBusy(true); setError(null);
    try {
      const selectedMiner = miners.some((candidate) => candidate.id === miner) ? miner : (miners[0]?.id ?? "inductive");
      const xes = await fetchPlayout(selectedMiner, options);
      const blob = new Blob([xes], { type: "application/xml" });
      const anchor = document.createElement("a"); anchor.href = URL.createObjectURL(blob);
      anchor.download = `${dataset.replace(/\.[^.]+$/, "")}-${selectedMiner}-stochastic-playout.xes`; anchor.click();
      URL.revokeObjectURL(anchor.href);
    } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  const number = (name: keyof StochasticPlayoutOptions, value: string) =>
    setOptions((current) => ({ ...current, [name]: value === "" ? undefined : Number(value) }));
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Stochastic Petri-net playout</h2><span className="panel-meta">Weighted random XES from a cached Petri net</span></div>
    {miners.length === 0 ? <p className="conf-inline-note">No Petri net is present in this analysis.</p> : <>
      <div className="align-settings-grid">
        <label>Miner<select value={miner} onChange={e => setMiner(e.target.value)}>{miners.map(candidate => <option key={candidate.id} value={candidate.id}>{candidate.label}</option>)}</select></label>
        <label>Traces<input type="number" min={1} value={options.traces ?? ""} onChange={e => number("traces", e.target.value)} /></label>
        <label>Max steps<input type="number" min={0} value={options.max_steps ?? ""} onChange={e => number("max_steps", e.target.value)} /></label>
        <label>Seed<input type="number" min={0} value={options.seed ?? ""} onChange={e => number("seed", e.target.value)} /></label>
        <label>Transition weights<input type="text" value={options.transition_weights?.join(",") ?? ""} onChange={e => setOptions({ ...options, transition_weights: e.target.value.trim() ? e.target.value.split(",").map(Number) : undefined })} placeholder="1,1,0.5 (optional)" /></label>
        <label><input type="checkbox" checked={Boolean(options.fit_from_log)} onChange={e => setOptions({ ...options, fit_from_log: e.target.checked })} /> Fit weights from cached log</label>
        <label>Case interval (s)<input type="number" min={0} step={0.1} value={options.case_interval ?? ""} onChange={e => number("case_interval", e.target.value)} /></label>
        <label>Event interval (s)<input type="number" min={0} step={0.1} value={options.event_interval ?? ""} onChange={e => number("event_interval", e.target.value)} /></label>
        <label><input type="checkbox" checked={Boolean(options.stop_at_final)} onChange={e => setOptions({ ...options, stop_at_final: e.target.checked })} /> Stop at final marking</label>
      </div>
      <button className="button" disabled={busy} onClick={() => void run()}>{busy ? "Generating…" : "Download stochastic playout"}</button>
      {error && <p className="conf-inline-note">{error}</p>}
    </>}
  </section>;
}

function MonteCarloPlayoutPanel({ dataset, miners }: { dataset: string; miners: Array<{ id: string; label: string }> }) {
  const [options, setOptions] = useState<MonteCarloOptions>({ simulations: 100, max_steps: 1000, stop_at_final: true });
  const [initialMarking, setInitialMarking] = useState("");
  const [finalMarking, setFinalMarking] = useState("");
  const [miner, setMiner] = useState(miners[0]?.id ?? "inductive");
  const [result, setResult] = useState<{ completed_cases: number; deadlocked_cases: number; median_case_execution_seconds: number } | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const parseMarking = (value: string, label: string): number[] | undefined => {
    if (!value.trim()) return undefined;
    const tokens = value.split(",").map((token) => token.trim());
    if (tokens.some((token) => !/^\d+$/.test(token))) {
      throw new Error(`${label} must contain comma-separated token counts`);
    }
    const marking = tokens.map(Number);
    if (marking.some((count) => !Number.isSafeInteger(count) || count > 0xffffffff)) {
      throw new Error(`${label} token counts must fit uint32`);
    }
    return marking;
  };
  const run = async () => {
    setBusy(true); setError(null); setResult(null);
    try {
      const selectedMiner = miners.some((candidate) => candidate.id === miner) ? miner : (miners[0]?.id ?? "inductive");
      const runOptions: MonteCarloOptions = {
        ...options,
        initial_marking: parseMarking(initialMarking, "Initial marking"),
        final_marking: parseMarking(finalMarking, "Final marking"),
      };
      const [summary, xes] = await Promise.all([
        fetchMonteCarloResult(selectedMiner, runOptions),
        fetchMonteCarloPlayout(selectedMiner, runOptions),
      ]);
      setResult({
        completed_cases: summary.diagnostics.completed_cases,
        deadlocked_cases: summary.diagnostics.deadlocked_cases,
        median_case_execution_seconds: summary.diagnostics.median_case_execution_seconds,
      });
      const blob = new Blob([xes], { type: "application/xml" });
      const anchor = document.createElement("a"); anchor.href = URL.createObjectURL(blob);
      anchor.download = `${dataset.replace(/\.[^.]+$/, "")}-${selectedMiner}-monte-carlo.xes`; anchor.click();
      URL.revokeObjectURL(anchor.href);
    } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  const number = (name: keyof MonteCarloOptions, value: string) =>
    setOptions((current) => ({ ...current, [name]: value === "" ? undefined : Number(value) }));
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Monte Carlo Petri-net simulation</h2><span className="panel-meta">Bounded virtual-time XES with resource pools and diagnostics</span></div>
    {miners.length === 0 ? <p className="conf-inline-note">No Petri net is present in this analysis.</p> : <>
      <div className="align-settings-grid">
        <label>Miner<select value={miner} onChange={e => setMiner(e.target.value)}>{miners.map(candidate => <option key={candidate.id} value={candidate.id}>{candidate.label}</option>)}</select></label>
        <label>Simulations<input type="number" min={1} value={options.simulations ?? ""} onChange={e => number("simulations", e.target.value)} /></label>
        <label>Max steps<input type="number" min={0} value={options.max_steps ?? ""} onChange={e => number("max_steps", e.target.value)} /></label>
        <label>Seed<input type="number" min={0} value={options.seed ?? ""} onChange={e => number("seed", e.target.value)} /></label>
        <label>Arrival override (s)<input type="number" min={0} step={0.1} placeholder="infer from log" value={options.case_arrival_ratio ?? ""} onChange={e => number("case_arrival_ratio", e.target.value)} /></label>
        <label>Default duration (s)<input type="number" min={0} step={0.1} value={options.default_duration_seconds ?? ""} onChange={e => number("default_duration_seconds", e.target.value)} /></label>
        <label>Activity key<input value={options.activity_key ?? ""} placeholder="concept:name" onChange={e => setOptions({ ...options, activity_key: e.target.value || undefined })} /></label>
        <label>Timestamp key<input value={options.timestamp_key ?? ""} placeholder="time:timestamp" onChange={e => setOptions({ ...options, timestamp_key: e.target.value || undefined })} /></label>
        <label>Initial marking<input value={initialMarking} placeholder="default source token" onChange={e => setInitialMarking(e.target.value)} /></label>
        <label>Final marking<input value={finalMarking} placeholder="default sink token" onChange={e => setFinalMarking(e.target.value)} /></label>
        <label>Replay strategy<select value={options.token_replay_variant ?? "forward"} onChange={e => setOptions({ ...options, token_replay_variant: e.target.value as "forward" | "backwards" })}><option value="forward">forward</option><option value="backwards">backwards</option></select></label>
        <label>Diagnostic interval<input type="number" min={1} value={options.diagnostic_interval ?? ""} placeholder="32" onChange={e => number("diagnostic_interval", e.target.value)} /></label>
        <label>Scale factor<input type="number" min={0} step={1} value={options.small_scale_factor ?? ""} placeholder="864000" onChange={e => number("small_scale_factor", e.target.value)} /></label>
        <label>Execution limit (s)<input type="number" min={0} step={0.1} value={options.max_thread_execution_time ?? ""} placeholder="60" onChange={e => number("max_thread_execution_time", e.target.value)} /></label>
        <label><input type="checkbox" checked={Boolean(options.stop_at_final)} onChange={e => setOptions({ ...options, stop_at_final: e.target.checked })} /> Stop at final marking</label>
        <label><input type="checkbox" checked={options.enable_diagnostics !== false} onChange={e => setOptions({ ...options, enable_diagnostics: e.target.checked })} /> Collect diagnostics</label>
      </div>
      <button className="button" disabled={busy} onClick={() => void run()}>{busy ? "Simulatingâ€¦" : "Download Monte Carlo playout"}</button>
      {result && <p className="conf-inline-note">Completed {numberFormat.format(result.completed_cases)} Â· deadlocked {numberFormat.format(result.deadlocked_cases)} Â· median execution {result.median_case_execution_seconds.toFixed(2)} s</p>}
      {error && <p className="conf-inline-note">{error}</p>}
    </>}
  </section>;
}

function DfgPlayoutPanel({ dataset, available }: { dataset: string; available: boolean }) {
  const [options, setOptions] = useState<DfgPlayoutOptions>({ variant: "classic" });
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const run = async () => {
    setBusy(true); setError(null);
    try {
      const result = await fetchDfgPlayout(options);
      const structured = typeof result !== "string";
      const body = structured ? JSON.stringify(result, null, 2) : result;
      const blob = new Blob([body], { type: structured ? "application/json" : "application/xml" });
      const anchor = document.createElement("a"); anchor.href = URL.createObjectURL(blob);
      anchor.download = `${dataset.replace(/\.[^.]+$/, "")}-dfg-playout.${structured ? "json" : "xes"}`; anchor.click();
      URL.revokeObjectURL(anchor.href);
    } catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { setBusy(false); }
  };
  const number = (name: keyof DfgPlayoutOptions, value: string) =>
    setOptions((current) => ({ ...current, [name]: value === "" ? undefined : Number(value) }));
  const flag = (name: keyof DfgPlayoutOptions, value: boolean) =>
    setOptions((current) => ({ ...current, [name]: value }));
  const selectVariant = (variant: DfgPlayoutOptions["variant"]) =>
    setOptions((current) => variant === "classic"
      ? { variant, max_variants: current.max_variants, min_weighted_probability: current.min_weighted_probability, max_occurrences_per_activity: current.max_occurrences_per_activity, max_execution_time_ms: current.max_execution_time_ms, interrupt_when_dfg_complete: current.interrupt_when_dfg_complete, add_trace_if_takes_new_edges: current.add_trace_if_takes_new_edges, return_variants: current.return_variants, return_only_if_complete: current.return_only_if_complete, min_variant_occurrences: current.min_variant_occurrences, activity_key: current.activity_key, timestamp_key: current.timestamp_key, case_id_key: current.case_id_key, artificial_start_activity: current.artificial_start_activity, artificial_end_activity: current.artificial_end_activity }
      : { variant, traces: current.traces, case_arrival_rate: current.case_arrival_rate, seed: current.seed, max_events_per_trace: current.max_events_per_trace, start_time: current.start_time, activity_key: current.activity_key, timestamp_key: current.timestamp_key, case_id_key: current.case_id_key, artificial_start_activity: current.artificial_start_activity, artificial_end_activity: current.artificial_end_activity });
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>DFG playout</h2><span className="panel-meta">Synthetic XES from the directly-follows graph</span></div>
    {!available ? <p className="conf-inline-note">No directly-follows graph is present in this analysis.</p> : <>
      <div className="align-settings-grid">
        <label>Variant<select value={options.variant} onChange={e => selectVariant(e.target.value as DfgPlayoutOptions["variant"])}><option value="classic">Classic</option><option value="performance">Performance</option></select></label>
        <label>Activity attribute<input type="text" value={options.activity_key ?? ""} onChange={e => setOptions({ ...options, activity_key: e.target.value || undefined })} placeholder="concept:name" /></label>
        <label>Timestamp attribute<input type="text" value={options.timestamp_key ?? ""} onChange={e => setOptions({ ...options, timestamp_key: e.target.value || undefined })} placeholder="time:timestamp" /></label>
        <label>Case-id attribute<input type="text" value={options.case_id_key ?? ""} onChange={e => setOptions({ ...options, case_id_key: e.target.value || undefined })} placeholder="case:concept:name" /></label>
        <label>Artificial start label<input type="text" value={options.artificial_start_activity ?? ""} onChange={e => setOptions({ ...options, artificial_start_activity: e.target.value || undefined })} placeholder="▶" /></label>
        <label>Artificial end label<input type="text" value={options.artificial_end_activity ?? ""} onChange={e => setOptions({ ...options, artificial_end_activity: e.target.value || undefined })} placeholder="■" /></label>
        {options.variant === "classic" ? <>
          <label>Max variants<input type="number" min={1} value={options.max_variants ?? ""} onChange={e => number("max_variants", e.target.value)} /></label>
          <label>Probability floor<input type="number" min={0} max={1} step={0.01} value={options.min_weighted_probability ?? ""} onChange={e => number("min_weighted_probability", e.target.value)} /></label>
          <label>Max activity occurrences<input type="number" min={1} value={options.max_occurrences_per_activity ?? ""} onChange={e => number("max_occurrences_per_activity", e.target.value)} /></label>
          <label>Max execution (ms)<input type="number" min={0} value={options.max_execution_time_ms ?? ""} onChange={e => number("max_execution_time_ms", e.target.value)} /></label>
        </> : <>
          <label>Traces<input type="number" min={1} value={options.traces ?? ""} onChange={e => number("traces", e.target.value)} /></label>
          <label>Case arrival rate<input type="number" min={0} step={0.01} value={options.case_arrival_rate ?? ""} onChange={e => number("case_arrival_rate", e.target.value)} /></label>
          <label>Max events / trace<input type="number" min={1} value={options.max_events_per_trace ?? ""} onChange={e => number("max_events_per_trace", e.target.value)} /></label>
          <label>Seed<input type="number" min={0} value={options.seed ?? ""} onChange={e => number("seed", e.target.value)} /></label>
          <label>Start time<input type="text" value={options.start_time ?? ""} onChange={e => setOptions({ ...options, start_time: e.target.value || undefined })} placeholder="ISO-8601 (optional)" /></label>
        </>}
      </div>
      {options.variant === "classic" && <div className="align-settings-grid">
        <label><input type="checkbox" checked={Boolean(options.interrupt_when_dfg_complete)} onChange={e => flag("interrupt_when_dfg_complete", e.target.checked)} /> Stop when graph is complete</label>
        <label><input type="checkbox" checked={Boolean(options.add_trace_if_takes_new_edges)} onChange={e => flag("add_trace_if_takes_new_edges", e.target.checked)} /> Keep traces adding new edges</label>
        <label><input type="checkbox" checked={Boolean(options.return_variants)} onChange={e => flag("return_variants", e.target.checked)} /> Return discovered variants</label>
        <label><input type="checkbox" checked={Boolean(options.return_only_if_complete)} onChange={e => flag("return_only_if_complete", e.target.checked)} /> Complete variants only</label>
      </div>}
      <button className="button" disabled={busy} onClick={() => void run()}>{busy ? "Generating…" : "Download DFG playout"}</button>
      {error && <p className="conf-inline-note">{error}</p>}
    </>}
  </section>;
}

function embeddedEvaluation(data: MineraExport, miner: string, family: EvaluationFamily): ModelEvaluation | null {
  const value = data.evaluation?.[miner];
  if (family === "replay_fitness" && value?.replay_fitness) return { miner, replay_fitness: value.replay_fitness };
  if (family === "precision" && value?.precision) return { miner, precision: value.precision };
  if (family === "generalization" && value?.generalization) return { miner, generalization: value.generalization };
  if (family === "simplicity" && value?.simplicity) return { miner, simplicity: value.simplicity };
  if (family === "composite" && value?.composite) return { miner, composite: value.composite };
  return null;
}

const EVALUATION_ALIGNMENT_VARIANTS: Array<{
  value: NonNullable<EvaluationOptions["evaluation_alignment_variant"]>;
  label: string;
}> = [
  { value: "state_equation_a_star", label: "State-equation A*" },
  { value: "dijkstra_no_heuristics", label: "Dijkstra (no heuristics)" },
  { value: "dijkstra_less_memory", label: "Dijkstra (less memory)" },
  { value: "discounted_a_star", label: "Discounted A*" },
  { value: "generator_dijkstra_no_heuristics", label: "Generator Dijkstra (all optima)" },
  { value: "generator_dijkstra_less_memory", label: "Generator Dijkstra (less memory)" },
  { value: "recompos_maximal", label: "Decomposed recomposition" },
];

function ModelEvaluationPanel({ data, importedAvailable, onRequestEvaluation }: {
  data: MineraExport;
  importedAvailable: boolean;
  onRequestEvaluation?: (miner: string, options?: EvaluationOptions) => Promise<ModelEvaluation>;
}) {
  const modelIds = evaluationModelIds(data, importedAvailable);
  const modelKey = modelIds.join("|");
  const [miner, setMiner] = useState(modelIds[0] ?? "");
  const [options, setOptions] = useState<EvaluationOptions>({
    evaluation_family: "composite",
    fitness_variant: "auto",
    precision_variant: "auto",
    precision_use_task_ids: false,
    generalization_variant: "generalization_token",
    simplicity_variant: "arc_degree",
    fitness_weight: 0.25,
    precision_weight: 0.25,
    simplicity_weight: 0.25,
    generalization_weight: 0.25,
  });
  const [evaluationMarkings, setEvaluationMarkings] = useState({ initial: "", final: "" });
  const [simplicityMarkings, setSimplicityMarkings] = useState({ initial: "", final: "" });
  const [result, setResult] = useState<ModelEvaluation | null>(null);
  const [resultSelectionKey, setResultSelectionKey] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<{ key: string; message: string } | null>(null);
  const dataIdentity = useRef({ data, id: 0 });
  if (dataIdentity.current.data !== data) dataIdentity.current = { data, id: dataIdentity.current.id + 1 };
  const selectionKeyFor = (selectedMiner: string, selectedOptions: EvaluationOptions) =>
    JSON.stringify([dataIdentity.current.id, selectedMiner, selectedOptions,
      evaluationMarkings, simplicityMarkings]);
  const selectionKey = selectionKeyFor(miner, options);
  const selectionKeyRef = useRef(selectionKey);
  const requestSequence = useRef(0);
  selectionKeyRef.current = selectionKey;

  useEffect(() => {
    requestSequence.current += 1;
    setLoading(false);
  }, [selectionKey]);

  useEffect(() => {
    requestSequence.current += 1;
    setLoading(false);
    const next = modelIds.includes(miner) ? miner : (modelIds[0] ?? "");
    setMiner(next);
    const selected = options.evaluation_family ?? "composite";
    const embedded = next ? embeddedEvaluation(data, next, selected) : null;
    setResult(embedded);
    setResultSelectionKey(embedded ? selectionKeyFor(next, options) : null);
    setError(null);
  }, [data, modelKey]);

  const family = options.evaluation_family ?? "composite";
  const updateFamily = (next: EvaluationFamily) => {
    const nextOptions = { ...options, evaluation_family: next };
    if (next === "precision" &&
        nextOptions.evaluation_alignment_variant === "recompos_maximal") {
      nextOptions.evaluation_alignment_variant = "dijkstra_less_memory";
      nextOptions.evaluation_alignment_max_total_states = undefined;
      nextOptions.evaluation_alignment_threshold_border_agreement = undefined;
      nextOptions.evaluation_alignment_use_cache = undefined;
      nextOptions.evaluation_alignment_clear_cache = undefined;
    }
    setOptions(nextOptions);
    const embedded = embeddedEvaluation(data, miner, next);
    setResult(embedded);
    setResultSelectionKey(embedded ? selectionKeyFor(miner, nextOptions) : null);
    setError(null);
  };
  const run = async () => {
    if (!miner || !onRequestEvaluation) return;
    const sequence = ++requestSequence.current;
    const requestedSelection = selectionKeyRef.current;
    setLoading(true);
    setError(null);
    try {
      const request: EvaluationOptions = { ...options };
      if (family === "simplicity") {
        request.simplicity_initial_marking = parseMarkingInput(simplicityMarkings.initial);
        request.simplicity_final_marking = parseMarkingInput(simplicityMarkings.final);
      } else {
        request.evaluation_initial_marking = parseMarkingInput(evaluationMarkings.initial);
        request.evaluation_final_marking = parseMarkingInput(evaluationMarkings.final);
      }
      const next = await onRequestEvaluation(miner, request);
      if (sequence === requestSequence.current && requestedSelection === selectionKeyRef.current) {
        setResult(next);
        setResultSelectionKey(requestedSelection);
      }
    } catch (cause) {
      if (sequence === requestSequence.current && requestedSelection === selectionKeyRef.current) {
        setError({ key: requestedSelection, message: cause instanceof Error ? cause.message : String(cause) });
      }
    } finally {
      if (sequence === requestSequence.current) setLoading(false);
    }
  };

  const numberOption = (name:
    | "precision_max_states"
    | "simplicity_arc_degree_k"
    | "simplicity_max_states"
    | "fitness_weight"
    | "precision_weight"
    | "simplicity_weight"
    | "generalization_weight"
    | "evaluation_alignment_max_states"
    | "evaluation_alignment_max_total_states"
    | "evaluation_alignment_max_time"
    | "evaluation_alignment_max_time_trace"
    | "evaluation_alignment_max_alignments"
    | "evaluation_alignment_exponent"
    | "evaluation_alignment_trace_cost"
    | "evaluation_alignment_model_cost"
    | "evaluation_alignment_silent_model_cost"
    | "evaluation_alignment_sync_cost"
    | "evaluation_alignment_threshold_border_agreement"
    | "evaluation_max_rows_to_materialize", value: string) =>
    setOptions((current) => ({ ...current, [name]: value === "" ? undefined : Number(value) }));
  const fitnessVariant = options.fitness_variant ?? "auto";
  const precisionVariant = options.precision_variant ?? "auto";
  const usesTokenReplay =
    (family === "replay_fitness" && fitnessVariant !== "alignment_based") ||
    (family === "precision" && precisionVariant !== "align_etconformance" &&
      precisionVariant !== "automaton_after_align");
  const usesAlignment = family === "replay_fitness" &&
    fitnessVariant !== "token_based";
  const alignmentVariant = options.evaluation_alignment_variant ??
    "dijkstra_less_memory";
  const generatorAlignment = alignmentVariant.startsWith("generator_");
  const discountedAlignment = alignmentVariant === "discounted_a_star";
  const decomposedAlignment = alignmentVariant === "recompos_maximal";
  const updateAlignmentVariant = (
    next: NonNullable<EvaluationOptions["evaluation_alignment_variant"]>,
  ) => setOptions((current) => ({
    ...current,
    evaluation_alignment_variant: next,
    evaluation_alignment_max_alignments: next.startsWith("generator_")
      ? current.evaluation_alignment_max_alignments : undefined,
    evaluation_alignment_exponent: next === "discounted_a_star"
      ? current.evaluation_alignment_exponent : undefined,
    evaluation_alignment_synchronous_dijkstra: next === "discounted_a_star"
      ? current.evaluation_alignment_synchronous_dijkstra : undefined,
    evaluation_alignment_max_total_states: next === "recompos_maximal"
      ? current.evaluation_alignment_max_total_states : undefined,
    evaluation_alignment_threshold_border_agreement: next === "recompos_maximal"
      ? current.evaluation_alignment_threshold_border_agreement : undefined,
    evaluation_alignment_use_cache: next === "recompos_maximal"
      ? current.evaluation_alignment_use_cache : undefined,
    evaluation_alignment_clear_cache: next === "recompos_maximal"
      ? current.evaluation_alignment_clear_cache : undefined,
    evaluation_alignment_cost_profile: next === "recompos_maximal"
      ? "standard" : current.evaluation_alignment_cost_profile,
  }));
  const updateTokenReplayVariant = (
    next: NonNullable<EvaluationOptions["evaluation_token_replay_variant"]>,
  ) => setOptions((current) => ({
    ...current,
    evaluation_token_replay_variant: next,
    fitness_consider_unknown_activities: next === "backwards"
      ? undefined : current.fitness_consider_unknown_activities,
  }));

  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Model quality evaluation</h2><span className="panel-meta">Fitness · precision · generalization · simplicity · composite</span></div>
    <div className="align-settings-grid">
      <label>Model<select value={miner} onChange={(event) => { const next = event.target.value; setMiner(next); const embedded = embeddedEvaluation(data, next, options.evaluation_family ?? "composite"); setResult(embedded); setResultSelectionKey(embedded ? selectionKeyFor(next, options) : null); setError(null); }} disabled={modelIds.length === 0}>{modelIds.map((id) => <option value={id} key={id}>{id}</option>)}</select></label>
      <label>Family<select value={family} onChange={(event) => updateFamily(event.target.value as EvaluationFamily)}><option value="replay_fitness">Replay fitness</option><option value="precision">Precision</option><option value="generalization">Generalization</option><option value="simplicity">Simplicity</option><option value="composite">Composite</option></select></label>
      {family === "replay_fitness" && <label>Variant<select value={options.fitness_variant ?? "auto"} onChange={(event) => setOptions({ ...options, fitness_variant: event.target.value as EvaluationOptions["fitness_variant"] })}><option value="auto">Automatic</option><option value="token_based">Token based</option><option value="alignment_based">Alignment based</option></select></label>}
      {family === "precision" && <>
        <label>Variant<select value={options.precision_variant ?? "auto"} onChange={(event) => { const next = event.target.value as NonNullable<EvaluationOptions["precision_variant"]>; setOptions({ ...options, precision_variant: next, precision_use_task_ids: next === "automaton_after_align" ? options.precision_use_task_ids : false }); }}><option value="auto">Automatic</option><option value="etconformance_token">Token escaping edges</option><option value="align_etconformance">Alignment escaping edges</option><option value="automaton_after_align">Automaton after alignment</option></select></label>
        <label>State limit<input type="number" min={1} step={1} value={options.precision_max_states ?? ""} placeholder="100000" onChange={(event) => numberOption("precision_max_states", event.target.value)} /></label>
        <label className={`checkbox-label ${precisionVariant === "automaton_after_align" ? "" : "disabled"}`}><input type="checkbox" disabled={precisionVariant !== "automaton_after_align"} checked={options.precision_use_task_ids ?? false} onChange={(event) => setOptions({ ...options, precision_use_task_ids: event.target.checked })} /> Use task identities (automaton)</label>
      </>}
      {family === "generalization" && <label>Variant<select value="generalization_token" disabled><option value="generalization_token">Transition-frequency token</option></select></label>}
      {family !== "simplicity" && <label>Activity field<input value={options.evaluation_activity_attribute ?? ""} placeholder="concept:name" onChange={(event) => setOptions({ ...options, evaluation_activity_attribute: event.target.value || undefined })} /></label>}
      {family !== "simplicity" && <label>Case-id field<input value={options.evaluation_case_id_attribute ?? ""} placeholder="case:concept:name" onChange={(event) => setOptions({ ...options, evaluation_case_id_attribute: event.target.value || undefined })} /></label>}
      {family !== "simplicity" && <label>Input row cap<input type="number" min={0} step={1} value={options.evaluation_max_rows_to_materialize ?? ""} placeholder="none" onChange={(event) => numberOption("evaluation_max_rows_to_materialize", event.target.value)} /></label>}
      {family !== "simplicity" && <>
        <label>Initial marking<input value={evaluationMarkings.initial} placeholder="default source token" onChange={(event) => setEvaluationMarkings({ ...evaluationMarkings, initial: event.target.value })} /></label>
        <label>Final marking<input value={evaluationMarkings.final} placeholder="default sink token" onChange={(event) => setEvaluationMarkings({ ...evaluationMarkings, final: event.target.value })} /></label>
      </>}
      {usesTokenReplay && <>
        <label>Replay direction<select value={options.evaluation_token_replay_variant ?? "forward"} onChange={(event) => updateTokenReplayVariant(event.target.value as NonNullable<EvaluationOptions["evaluation_token_replay_variant"]>)}><option value="forward">Forward</option><option value="backwards">Backwards</option></select></label>
        {family === "replay_fitness" && <label className={`checkbox-label ${options.evaluation_token_replay_variant === "backwards" ? "disabled" : ""}`}><input type="checkbox" disabled={options.evaluation_token_replay_variant === "backwards"} checked={options.fitness_consider_unknown_activities ?? false} onChange={(event) => setOptions({ ...options, fitness_consider_unknown_activities: event.target.checked })} /> Penalize unknown activities (forward)</label>}
      </>}
      {usesAlignment && <>
        <label>Alignment variant<select value={alignmentVariant} onChange={(event) => updateAlignmentVariant(event.target.value as NonNullable<EvaluationOptions["evaluation_alignment_variant"]>)}>{EVALUATION_ALIGNMENT_VARIANTS.map((item) => <option key={item.value} value={item.value}>{item.label}</option>)}</select></label>
        <label>Cost profile<select disabled={decomposedAlignment} value={decomposedAlignment ? "standard" : (options.evaluation_alignment_cost_profile ?? "standard")} onChange={(event) => setOptions({ ...options, evaluation_alignment_cost_profile: event.target.value as AlignmentCostProfile })}><option value="standard">Standard costs</option><option value="legacy">Legacy unit costs</option></select></label>
        <label>State limit<input type="number" min={1} step={1} value={options.evaluation_alignment_max_states ?? ""} placeholder="default" onChange={(event) => numberOption("evaluation_alignment_max_states", event.target.value)} /></label>
        <label>Global timeout (s)<input type="number" min={0} step={0.1} value={options.evaluation_alignment_max_time ?? ""} placeholder="none" onChange={(event) => numberOption("evaluation_alignment_max_time", event.target.value)} /></label>
        <label>Per-trace timeout (s)<input type="number" min={0} step={0.1} value={options.evaluation_alignment_max_time_trace ?? ""} placeholder="none" onChange={(event) => numberOption("evaluation_alignment_max_time_trace", event.target.value)} /></label>
        {generatorAlignment && <label>Maximum alternatives<input type="number" min={1} step={1} value={options.evaluation_alignment_max_alignments ?? ""} placeholder="default" onChange={(event) => numberOption("evaluation_alignment_max_alignments", event.target.value)} /></label>}
        {discountedAlignment && <>
          <label>Discount exponent<input type="number" min={1.01} step={0.1} value={options.evaluation_alignment_exponent ?? ""} placeholder="2" onChange={(event) => numberOption("evaluation_alignment_exponent", event.target.value)} /></label>
          <label className="checkbox-label"><input type="checkbox" checked={options.evaluation_alignment_synchronous_dijkstra ?? true} onChange={(event) => setOptions({ ...options, evaluation_alignment_synchronous_dijkstra: event.target.checked })} /> Synchronous Dijkstra</label>
        </>}
        {decomposedAlignment && <>
          <label>Total state limit<input type="number" min={1} step={1} value={options.evaluation_alignment_max_total_states ?? ""} placeholder="default" onChange={(event) => numberOption("evaluation_alignment_max_total_states", event.target.value)} /></label>
          <label>Border agreement<input type="number" min={0} step={0.1} value={options.evaluation_alignment_threshold_border_agreement ?? ""} placeholder="default" onChange={(event) => numberOption("evaluation_alignment_threshold_border_agreement", event.target.value)} /></label>
          <label className="checkbox-label"><input type="checkbox" checked={options.evaluation_alignment_use_cache ?? true} onChange={(event) => setOptions({ ...options, evaluation_alignment_use_cache: event.target.checked })} /> Reuse component cache</label>
          <label className="checkbox-label"><input type="checkbox" checked={options.evaluation_alignment_clear_cache ?? false} onChange={(event) => setOptions({ ...options, evaluation_alignment_clear_cache: event.target.checked })} /> Clear cache first</label>
        </>}
      </>}
      {family === "simplicity" && <>
        <label>Variant<select value={options.simplicity_variant ?? "arc_degree"} onChange={(event) => setOptions({ ...options, simplicity_variant: event.target.value as EvaluationOptions["simplicity_variant"] })}><option value="arc_degree">Arc degree</option><option value="extended_cardoso">Extended Cardoso</option><option value="extended_cyclomatic">Extended cyclomatic</option></select></label>
        {options.simplicity_variant === "arc_degree" && <label>Arc-degree k<input type="number" step={0.1} value={options.simplicity_arc_degree_k ?? ""} placeholder="2" onChange={(event) => numberOption("simplicity_arc_degree_k", event.target.value)} /></label>}
        <label>State limit<input type="number" min={1} step={1} value={options.simplicity_max_states ?? ""} placeholder="100000" onChange={(event) => numberOption("simplicity_max_states", event.target.value)} /></label>
        <label>Initial marking<input value={simplicityMarkings.initial} placeholder="0,1" onChange={(event) => setSimplicityMarkings({ ...simplicityMarkings, initial: event.target.value })} /></label>
        <label>Final marking<input value={simplicityMarkings.final} placeholder="2,3" onChange={(event) => setSimplicityMarkings({ ...simplicityMarkings, final: event.target.value })} /></label>
      </>}
      {family === "composite" && <>
        <label>Fitness weight<input type="number" min={0} step={0.05} value={options.fitness_weight ?? ""} onChange={(event) => numberOption("fitness_weight", event.target.value)} /></label>
        <label>Precision weight<input type="number" min={0} step={0.05} value={options.precision_weight ?? ""} onChange={(event) => numberOption("precision_weight", event.target.value)} /></label>
        <label>Generalization weight<input type="number" min={0} step={0.05} value={options.generalization_weight ?? ""} onChange={(event) => numberOption("generalization_weight", event.target.value)} /></label>
        <label>Simplicity weight<input type="number" min={0} step={0.05} value={options.simplicity_weight ?? ""} onChange={(event) => numberOption("simplicity_weight", event.target.value)} /></label>
      </>}
    </div>
    {usesAlignment && <div className="align-costs">
      <span className="align-settings-caption">Alignment move costs (optional overrides)</span>
      {([ ["evaluation_alignment_trace_cost", "Trace/log"], ["evaluation_alignment_model_cost", "Visible model"], ["evaluation_alignment_silent_model_cost", "Silent model"], ["evaluation_alignment_sync_cost", "Synchronous"] ] as const).map(([key, label]) => (
        <label key={key}>{label}<input type="number" min={0} step={1} value={options[key] ?? ""} onChange={(event) => numberOption(key, event.target.value)} placeholder="default" /></label>
      ))}
    </div>}
    {modelIds.length === 0 && <p className="conf-inline-note">No evaluable Petri model is available in this analysis.</p>}
    <div className="align-settings-actions"><button type="button" className="button" disabled={!onRequestEvaluation || !miner || loading} onClick={() => void run()}>{loading ? "Evaluating…" : "Evaluate model"}</button></div>
    {error?.key === selectionKey && <div className="conf-notice"><span className="conf-notice-mark">⚠</span><p>{error.message}</p></div>}
    {result && resultSelectionKey === selectionKey && <ModelEvaluationResultView result={result} />}
  </section>;
}

function ModelEvaluationResultView({ result }: { result: ModelEvaluation }) {
  const family = result.replay_fitness ?? result.precision ?? result.generalization ?? result.simplicity ?? result.composite;
  const error = family?.error;
  if (!family) return null;
  const familyLabel = result.composite
    ? "composite"
    : "variant" in family
      ? family.variant.replaceAll("_", " ")
      : "unknown";
  const replayAverage = result.replay_fitness
    ? (result.replay_fitness.average_trace_fitness !== undefined
      ? result.replay_fitness.average_trace_fitness
      : (result.replay_fitness.averageFitness ?? null))
    : null;
  const replayPercentage = result.replay_fitness
    ? (result.replay_fitness.percentage_of_fitting_traces !== undefined
      ? result.replay_fitness.percentage_of_fitting_traces
      : (result.replay_fitness.perc_fit_traces ?? result.replay_fitness.percFitTraces ?? null))
    : null;
  const compositeAverage = result.composite
    ? (result.composite.metrics_average_weight !== undefined
      ? result.composite.metrics_average_weight
      : (result.composite.metricsAverageWeight ?? null))
    : null;
  const compositeFitnessScore = result.composite
    ? (result.composite.fitness !== null &&
      typeof result.composite.fitness === "object"
      ? (result.composite.fitness_score ?? result.composite.fitness.log_fitness)
      : (result.composite.fitness_score ?? result.composite.fitness))
    : null;
  const metrics: Array<[string, number | null]> = result.replay_fitness ? [
    ["Fitness", result.replay_fitness.fitness],
    ["Average trace fitness", replayAverage],
    ["Fitting traces", replayPercentage === null ? null : replayPercentage / 100],
  ] : result.precision ? [["Precision", result.precision.precision]]
    : result.generalization ? [["Generalization", result.generalization.score]]
      : result.simplicity ? [["Simplicity", result.simplicity.simplicity]]
        : [["Composite average", compositeAverage], ["F-score", result.composite?.fscore ?? null]];
  return <div className="other-result">
    <div className="conf-metrics">{metrics.map(([label, value]) => <div className={`conf-gauge ${value === null ? "poor" : grade(value)}`} key={label}><div className="conf-gauge-value">{value === null ? "—" : percent(value)}</div><div className="conf-gauge-label">{label}</div></div>)}</div>
    <p><strong>{result.miner}</strong> · {familyLabel} · {family.computed ? "computed" : "not computed"}{"exhausted" in family && family.exhausted ? " · search exhausted" : ""}</p>
    {result.replay_fitness && <small>{numberFormat.format(result.replay_fitness.fitting_traces)} / {numberFormat.format(result.replay_fitness.trace_count)} fitting traces · total cost {result.replay_fitness.total_cost ?? "—"}</small>}
    {result.generalization && <small>{numberFormat.format(result.generalization.executed_transitions)} / {numberFormat.format(result.generalization.transition_count)} transitions executed · frequencies {result.generalization.transition_frequencies.join(", ") || "none"}</small>}
    {result.simplicity && <small>Complexity {result.simplicity.complexity ?? "—"} · raw value {result.simplicity.value ?? "—"} · {numberFormat.format(result.simplicity.vertices)} vertices · {numberFormat.format(result.simplicity.edges)} edges · {numberFormat.format(result.simplicity.strongly_connected_components)} strongly connected components</small>}
    {result.composite && <small>Fitness {compositeFitnessScore === null ? "—" : percent(compositeFitnessScore)} · precision {result.composite.precision === null ? "—" : percent(result.composite.precision)} · generalization {result.composite.generalization === null ? "—" : percent(result.composite.generalization)} · simplicity {result.composite.simplicity === null ? "—" : percent(result.composite.simplicity)}</small>}
    {error && <p className="conf-inline-note">{error}</p>}
    {(result.replay_fitness?.token_replay || result.replay_fitness?.alignments || result.precision?.token_replay || result.precision?.alignments) && <details><summary>Replay details</summary><pre className="json-preview">{JSON.stringify(result.replay_fitness?.token_replay ?? result.replay_fitness?.alignments ?? result.precision?.token_replay ?? result.precision?.alignments, null, 2)}</pre></details>}
  </div>;
}

function OtherAlignments({ data, importedAvailable, onRequestDfg, onRequestEditDistance, onRequestProcessTree, onRequestDecomposed, onRequestPetriAnti, onRequestPetriMulti, onRequestReduction, onRequestEmd }: {
  data: MineraExport;
  importedAvailable?: boolean;
  onRequestDfg?: (options?: DfgAlignmentOptions) => Promise<DfgAlignmentResult>;
  onRequestEditDistance?: (reference: File, options?: EditDistanceOptions) => Promise<EditDistanceResult>;
  onRequestProcessTree?: (options?: ProcessTreeAlignmentOptions) => Promise<ProcessTreeAlignmentResult>;
  onRequestDecomposed?: (miner: string, options?: DecomposedAlignmentOptions) => Promise<DecomposedAlignmentResult>;
  onRequestPetriAnti?: (miner: string, options?: PetriAntiAlignmentOptions) => Promise<PetriAntiAlignmentResult>;
  onRequestPetriMulti?: (miner: string, options?: PetriMultiAlignmentOptions) => Promise<PetriMultiAlignmentResult>;
  onRequestReduction?: (traceIndex: number, options?: ProcessTreeReductionOptions) => Promise<ProcessTreeReductionResult>;
  onRequestEmd?: (reference: File, options?: EmdOptions) => Promise<EmdResult>;
}) {
  const [dfg, setDfg] = useState<DfgAlignmentResult | null>(null);
  const [edit, setEdit] = useState<EditDistanceResult | null>(null);
  const [processTree, setProcessTree] = useState<ProcessTreeAlignmentResult | null>(null);
  const [reference, setReference] = useState<File | null>(null);
  const [emdReference, setEmdReference] = useState<File | null>(null);
  const [emd, setEmd] = useState<EmdResult | null>(data.earth_movers_distance ?? null);
  const [emdResultSelectionKey, setEmdResultSelectionKey] = useState<string | null>(data.earth_movers_distance ? "embedded" : null);
  const [emdOptions, setEmdOptions] = useState<EmdOptions>({ emd_variant: "pyemd", normalize_masses: true, strict_equal_mass: false, use_fast_emd: true });
  const [emdError, setEmdError] = useState<string | null>(null);
  const [dfgOptions, setDfgOptions] = useState<DfgAlignmentOptions>({ variant: "CLASSIC", projection: "none" });
  const [editOptions, setEditOptions] = useState<EditDistanceOptions>({ variant: "EDIT_DISTANCE", projection: "none" });
  const [processTreeOptions, setProcessTreeOptions] = useState<ProcessTreeAlignmentOptions>({ variant: "SEARCH_GRAPH_PT" });
  const petriMiners = [
    data.inductive_petri_net ? "inductive" : null,
    data.petri_net ? "alpha" : null,
    data.heuristic_petri_net ? "heuristic" : null,
    data.powl_petri_net ? "powl" : null,
    importedAvailable ? "imported" : null,
  ].filter((miner): miner is string => miner !== null);
  const petriMinerKey = petriMiners.join("|");
  const [petriMiner, setPetriMiner] = useState(petriMiners[0] ?? "inductive");
  const [decomposed, setDecomposed] = useState<DecomposedAlignmentResult | null>(null);
  const [anti, setAnti] = useState<PetriAntiAlignmentResult | null>(null);
  const [multi, setMulti] = useState<PetriMultiAlignmentResult | null>(null);
  const [reduced, setReduced] = useState<ProcessTreeReductionResult | null>(null);
  const [advancedOptions, setAdvancedOptions] = useState<PetriAntiAlignmentOptions>({ variant: "version_discounted_a_star" });
  const [multiOptions, setMultiOptions] = useState<PetriMultiAlignmentOptions>({ variant: "version_discounted_a_star" });
  const [decomposedOptions, setDecomposedOptions] = useState<DecomposedAlignmentOptions>({ variant: "recompos_maximal" });
  const [decomposedText, setDecomposedText] = useState({ initial_marking: "", final_marking: "", activity_log_costs: "", activity_model_costs: "", activity_sync_costs: "" });
  const [petriText, setPetriText] = useState({ antiInitial: "", antiFinal: "", multiInitial: "", multiFinal: "" });
  const [traceIndex, setTraceIndex] = useState(0);
  const [reductionOptions, setReductionOptions] = useState<ProcessTreeReductionOptions>({ variant: "tree_tr_based" });
  const [busy, setBusy] = useState<"dfg" | "edit" | "emd" | "process-tree" | "decomposed" | "anti" | "multi" | "reduction" | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [costText, setCostText] = useState<Record<string, string>>({ sync_cost: "", model_move_cost: "", log_move_cost: "", internal_log_move_cost: "" });
  const emdDataIdentity = useRef({ data, id: 0 });
  if (emdDataIdentity.current.data !== data) emdDataIdentity.current = { data, id: emdDataIdentity.current.id + 1 };
  const emdSelectionKey = JSON.stringify([emdDataIdentity.current.id, emdReference?.name, emdReference?.size, emdReference?.lastModified, emdOptions]);
  const emdSelectionKeyRef = useRef(emdSelectionKey);
  const emdRequestSequence = useRef(0);
  emdSelectionKeyRef.current = emdSelectionKey;
  const invalidateEmd = () => {
    emdRequestSequence.current += 1;
    setBusy((current) => current === "emd" ? null : current);
    setEmdError(null);
  };
  useEffect(() => { emdRequestSequence.current += 1; setDfg(null); setEdit(null); setEmd(data.earth_movers_distance ?? null); setEmdResultSelectionKey(data.earth_movers_distance ? "embedded" : null); setEmdError(null); setProcessTree(null); setDecomposed(null); setAnti(null); setMulti(null); setReduced(null); setReference(null); setEmdReference(null); setBusy(null); setError(null); setTraceIndex(0); }, [data]);
  useEffect(() => { setPetriMiner((current) => petriMiners.includes(current) ? current : (petriMiners[0] ?? "")); }, [petriMinerKey]);
  const runDfg = async () => { if (!onRequestDfg) return; setBusy("dfg"); setError(null); try { const maps = Object.fromEntries(Object.entries(costText).map(([key, text]) => [key, parseDfgCostMapInput(text)])); setDfg(await onRequestDfg({ ...dfgOptions, ...maps })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runEdit = async () => { if (!onRequestEditDistance || !reference) return; setBusy("edit"); setError(null); try { setEdit(await onRequestEditDistance(reference, editOptions)); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runEmd = async () => {
    if (!onRequestEmd || !emdReference) return;
    const sequence = ++emdRequestSequence.current;
    const requestedSelection = emdSelectionKeyRef.current;
    setBusy("emd");
    setEmdError(null);
    try {
      const next = await onRequestEmd(emdReference, emdOptions);
      if (sequence === emdRequestSequence.current && requestedSelection === emdSelectionKeyRef.current) {
        setEmd(next);
        setEmdResultSelectionKey(requestedSelection);
      }
    } catch (cause) {
      if (sequence === emdRequestSequence.current && requestedSelection === emdSelectionKeyRef.current) setEmdError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (sequence === emdRequestSequence.current && requestedSelection === emdSelectionKeyRef.current) setBusy(null);
    }
  };
  const runProcessTree = async () => { if (!onRequestProcessTree || !data.process_tree) return; setBusy("process-tree"); setError(null); try { setProcessTree(await onRequestProcessTree(processTreeOptions)); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const parseCostMap = (value: string) => value.trim() ? parseDfgCostMapInput(value) : undefined;
  const runDecomposed = async () => { if (!onRequestDecomposed) return; setBusy("decomposed"); setError(null); try { setDecomposed(await onRequestDecomposed(petriMiner, { ...decomposedOptions, initial_marking: parseMarkingInput(decomposedText.initial_marking), final_marking: parseMarkingInput(decomposedText.final_marking), activity_log_costs: parseCostMap(decomposedText.activity_log_costs), activity_model_costs: parseCostMap(decomposedText.activity_model_costs), activity_sync_costs: parseCostMap(decomposedText.activity_sync_costs) })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runAnti = async () => { if (!onRequestPetriAnti) return; setBusy("anti"); setError(null); try { setAnti(await onRequestPetriAnti(petriMiner, { ...advancedOptions, initial_marking: parseMarkingInput(petriText.antiInitial), final_marking: parseMarkingInput(petriText.antiFinal) })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runMulti = async () => { if (!onRequestPetriMulti) return; setBusy("multi"); setError(null); try { setMulti(await onRequestPetriMulti(petriMiner, { ...multiOptions, initial_marking: parseMarkingInput(petriText.multiInitial), final_marking: parseMarkingInput(petriText.multiFinal) })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runReduction = async () => { if (!onRequestReduction || !data.process_tree || !data.traces?.length) return; setBusy("reduction"); setError(null); try { setReduced(await onRequestReduction(traceIndex, reductionOptions)); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Other alignments</h2><span className="panel-meta">Native decomposed · Petri anti/multi · process-tree reduction</span></div>
     <div className="other-align-grid">
      <div className="other-align-card">
        <h3>Activity and timestamp selectors</h3>
        <p>Choose the event fields used by each alignment. Blank values are rejected.</p>
        <div className="align-settings-grid">
          <label>DFG activity attribute<input placeholder="concept:name" value={dfgOptions.activity_attribute ?? ""} onChange={e => setDfgOptions({ ...dfgOptions, activity_attribute: e.target.value })} /></label>
          <label>DFG timestamp attribute<input placeholder="time:timestamp" value={dfgOptions.timestamp_attribute ?? ""} onChange={e => setDfgOptions({ ...dfgOptions, timestamp_attribute: e.target.value })} /></label>
          <label>Edit activity attribute<input placeholder="concept:name" value={editOptions.activity_attribute ?? ""} onChange={e => setEditOptions({ ...editOptions, activity_attribute: e.target.value })} /></label>
          <label>Edit timestamp attribute<input placeholder="time:timestamp" value={editOptions.timestamp_attribute ?? ""} onChange={e => setEditOptions({ ...editOptions, timestamp_attribute: e.target.value })} /></label>
          <label>Process-tree activity attribute<input placeholder="concept:name" value={processTreeOptions.activity_attribute ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, activity_attribute: e.target.value })} /></label>
          <label>Decomposed activity attribute<input placeholder="concept:name" value={decomposedOptions.activity_attribute ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, activity_attribute: e.target.value })} /></label>
          <label>Petri anti activity attribute<input placeholder="concept:name" value={advancedOptions.activity_attribute ?? ""} onChange={e => setAdvancedOptions({ ...advancedOptions, activity_attribute: e.target.value })} /></label>
          <label>Petri multi activity attribute<input placeholder="concept:name" value={multiOptions.activity_attribute ?? ""} onChange={e => setMultiOptions({ ...multiOptions, activity_attribute: e.target.value })} /></label>
        </div>
      </div>
      <div className="other-align-card"><h3>Decomposed recomposition</h3><p>RECOMPOS_MAXIMAL alignment over a selected Petri miner.</p><label>Petri miner<select value={petriMiner} onChange={e => setPetriMiner(e.target.value)}>{petriMiners.map(m => <option key={m} value={m}>{m}</option>)}</select></label><div className="align-settings-grid"><label>Border threshold<input type="number" min={0} value={decomposedOptions.threshold_border_agreement ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, threshold_border_agreement: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Trace time (s)<input type="number" min={0} step={0.1} value={decomposedOptions.max_time_trace ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, max_time_trace: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Total time (s)<input type="number" min={0} step={0.1} value={decomposedOptions.max_time_total ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, max_time_total: e.target.value ? Number(e.target.value) : undefined })} /></label><label>State limit<input type="number" min={1} value={decomposedOptions.max_states ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, max_states: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Total states<input type="number" min={1} value={decomposedOptions.max_total_states ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, max_total_states: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Use cache<select value={String(decomposedOptions.use_cache ?? true)} onChange={e => setDecomposedOptions({ ...decomposedOptions, use_cache: e.target.value === "true" })}><option value="true">On</option><option value="false">Off</option></select></label><label>Clear cache<select value={String(decomposedOptions.clear_cache ?? false)} onChange={e => setDecomposedOptions({ ...decomposedOptions, clear_cache: e.target.value === "true" })}><option value="false">Keep</option><option value="true">Before run</option></select></label><label>Log cost<input type="number" min={0} value={decomposedOptions.log_move_cost ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, log_move_cost: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Model cost<input type="number" min={0} value={decomposedOptions.model_move_cost ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, model_move_cost: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Tau cost<input type="number" min={0} value={decomposedOptions.tau_model_cost ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, tau_model_cost: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Sync cost<input type="number" min={0} value={decomposedOptions.sync_cost ?? ""} onChange={e => setDecomposedOptions({ ...decomposedOptions, sync_cost: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Initial marking<input placeholder="0,1" value={decomposedText.initial_marking} onChange={e => setDecomposedText({ ...decomposedText, initial_marking: e.target.value })} /></label><label>Final marking<input placeholder="2,3" value={decomposedText.final_marking} onChange={e => setDecomposedText({ ...decomposedText, final_marking: e.target.value })} /></label><label>Activity log costs<input placeholder="A=10000" value={decomposedText.activity_log_costs} onChange={e => setDecomposedText({ ...decomposedText, activity_log_costs: e.target.value })} /></label><label>Activity model costs<input placeholder="A=10000" value={decomposedText.activity_model_costs} onChange={e => setDecomposedText({ ...decomposedText, activity_model_costs: e.target.value })} /></label><label>Activity sync costs<input placeholder="A=0" value={decomposedText.activity_sync_costs} onChange={e => setDecomposedText({ ...decomposedText, activity_sync_costs: e.target.value })} /></label></div>{petriMiners.length === 0 && <small>No Petri model is available in this analysis.</small>}<button className="button" disabled={!onRequestDecomposed || petriMiners.length === 0 || busy !== null} onClick={() => void runDecomposed()}>{busy === "decomposed" ? "Aligning…" : "Run decomposed alignment"}</button>{decomposed && <DecomposedResultView result={decomposed} />}</div>
      <div className="other-align-card"><h3>Petri anti / multi</h3><p>VERSION_DISCOUNTED_A_STAR objectives over the selected miner.</p><label>Petri miner<select value={petriMiner} onChange={e => setPetriMiner(e.target.value)}>{petriMiners.map(m => <option key={m} value={m}>{m}</option>)}</select></label><div className="align-settings-grid"><label>Anti exponent<input type="number" min={1.01} step={0.1} value={advancedOptions.exponent ?? 2} onChange={e => setAdvancedOptions({ ...advancedOptions, exponent: Number(e.target.value) })} /></label><label>Anti limit<input type="number" min={1} value={advancedOptions.marking_limit ?? ""} onChange={e => setAdvancedOptions({ ...advancedOptions, marking_limit: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Anti max states<input type="number" min={0} value={advancedOptions.max_states ?? ""} onChange={e => setAdvancedOptions({ ...advancedOptions, max_states: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Anti epsilon<input type="number" min={0} step={0.001} value={advancedOptions.epsilon ?? 0.005} onChange={e => setAdvancedOptions({ ...advancedOptions, epsilon: Number(e.target.value) })} /></label><label>Anti initial<input placeholder="0,1" value={petriText.antiInitial} onChange={e => setPetriText({ ...petriText, antiInitial: e.target.value })} /></label><label>Anti final<input placeholder="2,3" value={petriText.antiFinal} onChange={e => setPetriText({ ...petriText, antiFinal: e.target.value })} /></label><label>Multi exponent<input type="number" min={1.01} step={0.1} value={multiOptions.exponent ?? 2} onChange={e => setMultiOptions({ ...multiOptions, exponent: Number(e.target.value) })} /></label><label>Multi limit<input type="number" min={1} value={multiOptions.marking_limit ?? ""} onChange={e => setMultiOptions({ ...multiOptions, marking_limit: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Multi max states<input type="number" min={0} value={multiOptions.max_states ?? ""} onChange={e => setMultiOptions({ ...multiOptions, max_states: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Multi initial<input placeholder="0,1" value={petriText.multiInitial} onChange={e => setPetriText({ ...petriText, multiInitial: e.target.value })} /></label><label>Multi final<input placeholder="2,3" value={petriText.multiFinal} onChange={e => setPetriText({ ...petriText, multiFinal: e.target.value })} /></label></div><button className="button" disabled={!onRequestPetriAnti || petriMiners.length === 0 || busy !== null} onClick={() => void runAnti()}>{busy === "anti" ? "Aligning…" : "Run anti-alignment"}</button><button className="button" disabled={!onRequestPetriMulti || petriMiners.length === 0 || busy !== null} onClick={() => void runMulti()}>{busy === "multi" ? "Aligning…" : "Run multi-alignment"}</button>{anti && <PetriResultView kind="anti" result={anti} />}{multi && <PetriResultView kind="multi" result={multi} />}</div>
      <div className="other-align-card"><h3>Process-tree reduction</h3><p>Reduce the inductive tree for one trace using TREE_TR_BASED.</p><label>Trace<select value={traceIndex} onChange={e => setTraceIndex(Number(e.target.value))}>{(data.traces ?? []).map((trace, i) => <option key={i} value={i}>#{i + 1} · {trace.case_id}</option>)}</select></label><label>Activity key<input placeholder="concept:name" value={reductionOptions.activity_key ?? ""} onChange={e => setReductionOptions({ ...reductionOptions, activity_key: e.target.value || undefined })} /></label><button className="button" disabled={!onRequestReduction || !data.process_tree || !data.traces?.length || busy !== null} onClick={() => void runReduction()}>{busy === "reduction" ? "Reducing…" : "Reduce process tree"}</button>{reduced && <><small>Reduced tree returned for trace #{reduced.trace_index + 1}.</small><details><summary>Show reduced tree</summary><pre className="json-preview">{JSON.stringify(reduced.process_tree, null, 2)}</pre></details></>}</div>
      <div className="other-align-card">
        <h3>Process tree</h3>
        <p>Align the latest mined inductive process tree against every trace.</p>
        <div className="align-settings-grid">
          <label>Variant<select value={processTreeOptions.variant ?? "SEARCH_GRAPH_PT"} onChange={e => setProcessTreeOptions({ ...processTreeOptions, variant: e.target.value as ProcessTreeAlignmentOptions["variant"] })}><option value="SEARCH_GRAPH_PT">Search graph</option><option value="DYNAMIC_PROGRAMMING">Dynamic programming</option><option value="MILP">MILP</option><option value="APPROXIMATED_ORIGINAL">Approximated original</option></select></label>
          <label>State limit<input type="number" min={0} value={processTreeOptions.max_states ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_states: e.target.value ? Number(e.target.value) : undefined })} /></label>
          <label>Per-trace time (ms)<input type="number" min={0} value={processTreeOptions.max_time_ms ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_time_ms: e.target.value ? Number(e.target.value) : undefined })} /></label>
          <label>Total time (ms)<input type="number" min={0} value={processTreeOptions.max_total_time_ms ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_total_time_ms: e.target.value ? Number(e.target.value) : undefined })} /></label>
          {processTreeOptions.variant === "DYNAMIC_PROGRAMMING" && <>
            <label>Global cache<select value={String(processTreeOptions.use_global_cache ?? false)} onChange={e => setProcessTreeOptions({ ...processTreeOptions, use_global_cache: e.target.value === "true" })}><option value="false">Off</option><option value="true">On</option></select></label>
            <label>Batch size<input type="number" min={1} value={processTreeOptions.batch_size ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, batch_size: e.target.value ? Number(e.target.value) : undefined })} /></label>
            <label>Clear cache<select value={String(processTreeOptions.clear_cache ?? true)} onChange={e => setProcessTreeOptions({ ...processTreeOptions, clear_cache: e.target.value === "true" })}><option value="true">Before run</option><option value="false">Keep</option></select></label>
          </>}
          {processTreeOptions.variant === "APPROXIMATED_ORIGINAL" && <>
            <label>Max trace length<input type="number" min={0} value={processTreeOptions.max_trace_length ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_trace_length: e.target.value ? Number(e.target.value) : undefined })} /></label>
            <label>Max tree height<input type="number" min={0} value={processTreeOptions.max_process_tree_height ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_process_tree_height: e.target.value ? Number(e.target.value) : undefined })} /></label>
            <label>Trace timeout (ms)<input type="number" min={0} value={processTreeOptions.max_trace_time_ms ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_trace_time_ms: e.target.value ? Number(e.target.value) : undefined })} /></label>
            <label>Per-trace timeout (ms)<input type="number" min={0} value={processTreeOptions.max_per_trace_time_ms ?? ""} onChange={e => setProcessTreeOptions({ ...processTreeOptions, max_per_trace_time_ms: e.target.value ? Number(e.target.value) : undefined })} /></label>
          </>}
        </div>
        <button className="button" disabled={!onRequestProcessTree || busy !== null || !data.process_tree} onClick={() => void runProcessTree()}>{busy === "process-tree" ? "Aligning…" : "Run process-tree alignment"}</button>
        {!data.process_tree && <small>No inductive process tree is present in this analysis.</small>}
        {processTree && <ProcessTreeResultView result={processTree} />}
      </div>
      <div className="other-align-card"><h3>DFG classic</h3><p>Align the latest mined log against its directly-follows graph.</p>
        <div className="align-settings-grid"><label>State limit<input type="number" min={1} value={dfgOptions.max_states ?? ""} onChange={e => setDfgOptions({ ...dfgOptions, max_states: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Time limit (ms)<input type="number" min={1} value={dfgOptions.max_time_ms ?? ""} onChange={e => setDfgOptions({ ...dfgOptions, max_time_ms: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Projection<select value={dfgOptions.projection ?? "none"} onChange={e => setDfgOptions({ ...dfgOptions, projection: e.target.value as DfgAlignmentOptions["projection"] })}><option value="none">None</option><option value="log">Projected log</option><option value="metrics">Metrics</option></select></label><label>Sync costs<input placeholder="A=1,B=2" value={costText.sync_cost} onChange={e => setCostText({ ...costText, sync_cost: e.target.value })} /></label><label>Model-move costs<input placeholder="A=1,B=2" value={costText.model_move_cost} onChange={e => setCostText({ ...costText, model_move_cost: e.target.value })} /></label><label>Log-move costs<input placeholder="A=1,B=2" value={costText.log_move_cost} onChange={e => setCostText({ ...costText, log_move_cost: e.target.value })} /></label><label>Internal-log costs<input placeholder="A=1,B=2" value={costText.internal_log_move_cost} onChange={e => setCostText({ ...costText, internal_log_move_cost: e.target.value })} /></label></div>
        <small>Escape delimiters with a backslash; use \s, \t, \n, or \r for whitespace in activity names.</small>
        <button className="button" disabled={!onRequestDfg || busy !== null} onClick={() => void runDfg()}>{busy === "dfg" ? "Aligning…" : "Run DFG alignment"}</button>{dfg && <DfgResult result={dfg} />}
      </div>
      <div className="other-align-card"><h3>Edit distance</h3><p>Compare mined traces with a reference CSV/XES log.</p><label className="file-picker">Reference log<input type="file" accept=".csv,.xes,.xes.gz,.gz,.xml" onChange={e => setReference(e.target.files?.[0] ?? null)} /></label>
        <div className="align-settings-grid"><label>Mode<select value={editOptions.anti ? "anti" : "closest"} onChange={e => setEditOptions({ ...editOptions, anti: e.target.value === "anti" })}><option value="closest">Closest</option><option value="anti">Anti-alignment</option></select></label><label>Deviation cost<input type="number" min={1} value={editOptions.deviation_cost ?? ""} onChange={e => setEditOptions({ ...editOptions, deviation_cost: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Projection<select value={editOptions.projection ?? "none"} onChange={e => setEditOptions({ ...editOptions, projection: e.target.value as EditDistanceOptions["projection"] })}><option value="none">None</option><option value="log">Projected log</option></select></label></div>
        <button className="button" disabled={!onRequestEditDistance || !reference || busy !== null} onClick={() => void runEdit()}>{busy === "edit" ? "Aligning…" : "Run edit-distance alignment"}</button>{edit && <EditDistanceResultView result={edit} />}
      </div>
      <div className="other-align-card"><h3>Stochastic-language distance</h3><p>Compare trace distributions with a reference CSV/XES log using optimal transport.</p><label className="file-picker">Reference log<input type="file" accept=".csv,.xes,.xes.gz,.gz,.xml" onChange={e => { invalidateEmd(); setEmdReference(e.target.files?.[0] ?? null); }} /></label>
        <div className="align-settings-grid"><label>Distance variant<select value={emdOptions.emd_variant ?? "pyemd"} onChange={e => { invalidateEmd(); setEmdOptions({ ...emdOptions, emd_variant: e.target.value as EmdOptions["emd_variant"] }); }}><option value="pyemd">PYEMD-compatible</option><option value="exact">Exact native (explicit)</option></select></label><label>Normalize masses<select value={String(emdOptions.normalize_masses ?? true)} onChange={e => { invalidateEmd(); setEmdOptions({ ...emdOptions, normalize_masses: e.target.value === "true" }); }}><option value="true">On</option><option value="false">Off</option></select></label><label>Equal mass required<select value={String(emdOptions.strict_equal_mass ?? false)} onChange={e => { invalidateEmd(); setEmdOptions({ ...emdOptions, strict_equal_mass: e.target.value === "true" }); }}><option value="false">No</option><option value="true">Yes</option></select></label><label>Fast EMD backend<select value={String(emdOptions.use_fast_emd ?? true)} onChange={e => { invalidateEmd(); setEmdOptions({ ...emdOptions, use_fast_emd: e.target.value === "true" }); }}><option value="true">On</option><option value="false">Off</option></select></label></div>
        <button className="button" disabled={!onRequestEmd || !emdReference || busy !== null} onClick={() => void runEmd()}>{busy === "emd" ? "Comparing…" : "Compare stochastic languages"}</button>{emdError && <p className="conf-inline-note">{emdError}</p>}{emd && (emdResultSelectionKey === emdSelectionKey || (emdResultSelectionKey === "embedded" && !emdReference)) && <EmdResultView result={emd} />}
      </div>
    </div>{error && <div className="conf-notice"><span className="conf-notice-mark">⚠</span><p>{error}</p></div>}
  </section>;
}

function DfgResult({ result }: { result: DfgAlignmentResult }) { return <div className="other-result"><p><strong>{numberFormat.format(result.alignments.length)}</strong> traces · {numberFormat.format(result.unique_variants)} unique variants searched</p><ol className="variant-list">{result.alignments.slice(0, 30).map((a, i) => <li className="variant" key={i}><div className="variant-top"><span className={`conf-pill ${grade(a.fitness)}`}>{percent(a.fitness)}</span><span className="variant-count">cost {numberFormat.format(a.cost)} · internal {numberFormat.format(a.internal_cost)} · states {numberFormat.format(a.visited_states)} · closed {numberFormat.format(a.closed_states)}</span></div><div className="align-moves">{a.moves.map((m, j) => <span className={`align-move ${m.log === ">>" ? "model" : m.model === ">>" ? "log" : "sync"}`} key={j}>{m.log} / {m.model}</span>)}</div></li>)}</ol>{result.projected_dfg && <small>{result.projected_dfg.edges.length} projected edges · {result.projected_dfg.activities.length} activities</small>}</div>; }
function DecomposedResultView({ result }: { result: DecomposedAlignmentResult }) { const first = result.traces[0]; const moves = first?.moves ?? first?.alignment ?? []; return <div className="other-result"><p><strong>{numberFormat.format(result.traces.length)}</strong> traces · {percent(result.fitness)} fitness · cost {numberFormat.format(result.total_cost)}</p><small>Visited {numberFormat.format(result.diagnostics.visited_states)} states · queued {numberFormat.format(result.diagnostics.queued_states)} · merges {numberFormat.format(result.diagnostics.merges)} · {result.complete ? "complete" : "incomplete"}{result.timed_out ? " · timed out" : ""}{result.exhausted ? " · exhausted" : ""}</small>{first && <div className="align-moves">{moves.map((move, i) => <span className={`align-move ${move.log === ">>" ? "model" : move.model === ">>" ? "log" : "sync"}`} key={i}>{move.log ?? "τ"} / {move.model ?? "τ"}</span>)}</div>}</div>; }
function PetriResultView({ kind, result }: { kind: "anti" | "multi"; result: PetriAntiAlignmentResult | PetriMultiAlignmentResult }) { return <div className="other-result"><p><strong>{kind === "anti" ? "Anti-alignment" : "Multi-alignment"}</strong> · objective {numberFormat.format(result.objective)}</p><div className="align-moves">{result.activities.map((activity, i) => <span className="align-move model" key={i}>{activity}</span>)}</div><small>Visible sequence</small><div className="align-moves">{result.transitions.map((transition, i) => <span className="align-move model" key={i}>t{transition}</span>)}</div><small>Transition sequence (includes silent moves) · visited {numberFormat.format(result.diagnostics.visited_states)} states · traversed {numberFormat.format(result.diagnostics.traversed_arcs)} · {result.diagnostics.exhausted ? "search exhausted" : "complete"}{kind === "anti" && "precision" in result ? ` · precision ${percent(result.precision)}` : kind === "multi" && "max_distance_to_log" in result ? ` · max distance ${numberFormat.format(result.max_distance_to_log)}` : ""}</small></div>; }
function EditDistanceResultView({ result }: { result: EditDistanceResult }) { return <div className="other-result"><p><strong>{numberFormat.format(result.source_count)}</strong> source traces · {result.anti ? "anti-alignment" : "closest alignment"}</p><ol className="variant-list">{result.alignments.slice(0, 30).map((a, i) => <li className="variant" key={i}><div className="variant-top"><span className={`conf-pill ${grade(a.fitness)}`}>{percent(a.fitness)}</span><span className="variant-count">cost {numberFormat.format(a.cost)} · best/worst {numberFormat.format(a.best_worst_cost)} · denominator {numberFormat.format(a.denominator)} · target #{a.target_index}</span></div><div className="align-moves">{a.moves.map((m, j) => <span className={`align-move ${m.type}`} key={j}>{m.log} / {m.model}</span>)}</div></li>)}</ol></div>; }
function EmdResultView({ result }: { result: EmdResult }) { return <div className="other-result"><p><strong>{result.distance === null ? "—" : result.distance.toFixed(6)}</strong> distance · {result.variant ?? "exact"} · {result.normalized ? "normalized masses" : "original masses"} · {result.computed ? "computed" : "not computed"}</p>{result.error && <small>{result.error}</small>}</div>; }

function ProcessTreeResultView({ result }: { result: ProcessTreeAlignmentResult }) {
  return <div className="other-result">
    <p><strong>{numberFormat.format(result.traces.length)}</strong> traces · {percent(result.fitness)} fitness · total cost {numberFormat.format(result.total_cost)}</p>
    <p className="variant-count">Variant {result.variant.replaceAll("_", " ")} · {result.computed ? "computed" : "not computed"} · {result.completed ? "completed" : "incomplete"}{result.timed_out ? " · timed out" : ""}{result.exhausted ? " · search exhausted" : ""}</p>
    <small>Visited {numberFormat.format(result.diagnostics.visited_states)} states · queued {numberFormat.format(result.diagnostics.queued_states)} · aligned {numberFormat.format(result.diagnostics.traces_aligned)} traces</small>
    {result.diagnostics.message && <p className="variant-count">{result.diagnostics.message}</p>}
    <ol className="variant-list">{result.traces.slice(0, 30).map((trace, index) => <li className="variant" key={index}>
      <div className="variant-top"><span className={`conf-pill ${grade(trace.fitness)}`}>{percent(trace.fitness)}</span><span className="variant-count">cost {numberFormat.format(trace.cost)} · {trace.completed ? "complete" : "incomplete"}{trace.timed_out ? " · timed out" : ""}</span></div>
      <div className="align-moves">{trace.moves.map((move, moveIndex) => <span className={`align-move ${move.type}`} title={move.activity} key={moveIndex}>{move.log || "∅"} / {move.model || "∅"}</span>)}</div>
    </li>)}</ol>
  </div>;
}

// Token-replay results: quality gauges, the token tally, and per-variant
// deviation. Rendered when the "token-replay" method is selected.
function TokenReplay({
  data,
  conformance,
  minerName,
  sound,
}: {
  data: MineraExport;
  conformance: NonNullable<MineraExport["conformance"]>[string]["token_replay"];
  minerName: string;
  sound: boolean;
}) {
  const fittingShare =
    conformance.trace_count > 0
      ? conformance.fitting_traces / conformance.trace_count
      : 1;
  const deviating = conformance.variants.filter((v) => v.fitness < 0.9999);

  const metrics: { label: string; value: number; hint: string }[] = [
    {
      label: "Fitness",
      value: conformance.fitness,
      hint: "Token-replay: how much of the log the model reproduces.",
    },
    {
      label: "Precision",
      value: conformance.precision,
      hint: "Escaping edges: how tightly the model matches the log (lower = the model allows more unseen behavior).",
    },
    ...(conformance.generalization !== undefined
      ? [
          {
            label: "Generalization",
            value: conformance.generalization,
            hint: "How evenly the log exercises the model (lower = parts of the model hinge on a few observations).",
          },
        ]
      : []),
    ...(conformance.simplicity !== undefined
      ? [
          {
            label: "Simplicity",
            value: conformance.simplicity,
            hint: "How lean the net itself is, by mean arc degree (lower = denser routing).",
          },
        ]
      : []),
    {
      label: "Fitting traces",
      value: fittingShare,
      hint: `${numberFormat.format(conformance.fitting_traces)} of ${numberFormat.format(
        conformance.trace_count,
      )} cases replay with no deviation.`,
    },
  ];

  return (
    <div className="conformance reveal">
      {!sound && (
        <p className="conf-inline-note">
          The {minerName} model isn't a sound workflow net, so token replay has to
          force missing tokens — read these numbers as approximate. Alignments
          can't be computed for it at all.
        </p>
      )}
      {conformance.activity_attribute &&
        conformance.activity_attribute !== "concept:name" && (
          <p className="conf-inline-note">
            Replay activity field: <code>{conformance.activity_attribute}</code>
          </p>
        )}
      <section className="conf-metrics">
        {metrics.map((metric) => (
          <div className={`conf-gauge ${grade(metric.value)}`} key={metric.label}>
            <div className="conf-gauge-value">{percent(metric.value)}</div>
            <div className="conf-gauge-label">{metric.label}</div>
            <p className="conf-gauge-hint">{metric.hint}</p>
          </div>
        ))}
      </section>

      <section className="panel conf-panel">
        <div className="panel-head">
          <h2>Token replay</h2>
          <span className="panel-meta">
            {minerName} model as a workflow net
          </span>
        </div>
        <div className="conf-tokens">
          {([
            ["produced", conformance.produced],
            ["consumed", conformance.consumed],
            ["missing", conformance.missing],
            ["remaining", conformance.remaining],
          ] as [string, number][]).map(([label, value]) => (
            <div className="conf-token" key={label}>
              <div className="conf-token-value">{numberFormat.format(value)}</div>
              <div className="conf-token-label">{label}</div>
            </div>
          ))}
        </div>
      </section>

      <section className="panel conf-panel">
        <div className="panel-head">
          <h2>Variant conformance</h2>
          <span className="panel-meta">
            {deviating.length > 0
              ? `${numberFormat.format(deviating.length)} deviating`
              : "all variants replay perfectly"}
          </span>
        </div>
        {deviating.length === 0 ? (
          <p className="conf-allfit">
            Every distinct path replays against the model with no missing or
            remaining tokens.
          </p>
        ) : (
          <ol className="variant-list">
            {deviating.map((variant, index) => {
              const share =
                conformance.trace_count > 0
                  ? variant.count / conformance.trace_count
                  : 0;
              return (
                <li className="variant" key={index}>
                  <div className="variant-top">
                    <span className={`conf-pill ${grade(variant.fitness)}`}>
                      {percent(variant.fitness)}
                    </span>
                    <span className="variant-count">
                      {numberFormat.format(variant.count)}{" "}
                      <small>{(share * 100).toFixed(1)}%</small>
                    </span>
                  </div>
                  <div className="variant-steps">
                    {variant.activities.map((activityId, i) => {
                      const name = variant.activity_names?.[i] ??
                        (activityId === null ? "?" : data.activities[activityId]?.name ?? "?");
                      return (
                        <Fragment key={i}>
                          {i > 0 && <span className="variant-arrow">→</span>}
                          <span className="variant-step" title={name}>
                            {name}
                          </span>
                        </Fragment>
                      );
                    })}
                  </div>
                </li>
              );
            })}
          </ol>
        )}
      </section>
    </div>
  );
}

// Footprint conformance: the two headline ratios, the relation tally, and the
// deviating successions the log and model disagree on. Computed at mine time and
// robust on unsound nets, so it always has a result to show.
function Footprints({
  data,
  footprints,
  minerName,
}: {
  data: MineraExport;
  footprints: ModelConformance["footprints"];
  minerName: string;
}) {
  if (!footprints) {
    return (
      <section className="panel map-panel">
        <div className="model-empty">
          No footprint comparison for the {minerName} model. This export predates
          footprint conformance — re-open the event log to compute it.
        </div>
      </section>
    );
  }

  const name = (id: number) => data.activities[id]?.name ?? "?";
  const pair = (relation: { source: number; target: number }, i: number) => (
    <li className="footprint-pair" key={i}>
      <span className="variant-step">{name(relation.source)}</span>
      <span className="variant-arrow">→</span>
      <span className="variant-step">{name(relation.target)}</span>
    </li>
  );

  const metrics = [
    {
      label: "Fitness",
      value: footprints.fitness,
      hint: "Share of the log's directly-follows successions the model also allows.",
    },
    {
      label: "Precision",
      value: footprints.precision,
      hint: "Share of the model's directly-follows successions the log also shows.",
    },
  ];

  return (
    <div className="conformance reveal">
      {footprints.exhausted && (
        <p className="conf-inline-note">
          The {minerName} model is large enough that its footprint exploration
          hit its bound — read the model-side numbers as a lower bound.
        </p>
      )}
      <section className="conf-metrics">
        {metrics.map((metric) => (
          <div className={`conf-gauge ${grade(metric.value)}`} key={metric.label}>
            <div className="conf-gauge-value">{percent(metric.value)}</div>
            <div className="conf-gauge-label">{metric.label}</div>
            <p className="conf-gauge-hint">{metric.hint}</p>
          </div>
        ))}
      </section>

      <section className="panel conf-panel">
        <div className="panel-head">
          <h2>Directly-follows relations</h2>
          <span className="panel-meta">
            {numberFormat.format(footprints.shared_relations)} shared ·{" "}
            {numberFormat.format(footprints.log_relations)} in log ·{" "}
            {numberFormat.format(footprints.model_relations)} in model
          </span>
        </div>
        <div className="footprint-columns">
          <div>
            <span className="filters-field-label">
              In log, not in model <small>({footprints.log_only.length})</small>
            </span>
            {footprints.log_only.length === 0 ? (
              <p className="conf-allfit">
                The model reproduces every succession the log shows.
              </p>
            ) : (
              <ul className="footprint-list">
                {footprints.log_only.map(pair)}
              </ul>
            )}
          </div>
          <div>
            <span className="filters-field-label">
              In model, not in log{" "}
              <small>({footprints.model_only.length})</small>
            </span>
            {footprints.model_only.length === 0 ? (
              <p className="conf-allfit">
                The model allows no succession the log never shows.
              </p>
            ) : (
              <ul className="footprint-list">
                {footprints.model_only.map(pair)}
              </ul>
            )}
          </div>
        </div>
      </section>
    </div>
  );
}

// Decision mining is deferred like alignments: this wrapper uses the embedded
// result when a CLI export carries one for the selected miner, and otherwise
// fetches it on demand from the backend.
function DecisionsView({
  data,
  minerId,
  minerName,
  onRequestDecisions,
}: {
  data: MineraExport;
  minerId: string;
  minerName: string;
  onRequestDecisions?: (minerId: string, options?: DecisionMiningOptions) => Promise<DecisionsResult>;
}) {
  const embedded =
    data.decisions && data.decisions.miner === minerId
      ? data.decisions
      : undefined;
  const [fetched, setFetched] = useState<DecisionsResult | null>(
    embedded ?? null,
  );
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [options, setOptions] = useState<DecisionMiningOptions>({});
  const [requestedOptions, setRequestedOptions] = useState<DecisionMiningOptions>({});
  const requestRef = useRef(onRequestDecisions);
  requestRef.current = onRequestDecisions;

  useEffect(() => {
    if (embedded) {
      setFetched(embedded);
      return;
    }
    if (!requestRef.current) return;
    let cancelled = false;
    setFetched(null);
    setError(null);
    setLoading(true);
    requestRef
      .current(minerId, requestedOptions)
      .then((result) => !cancelled && setFetched(result))
      .catch(
        (cause) =>
          !cancelled &&
          setError(cause instanceof Error ? cause.message : String(cause)),
      )
      .finally(() => !cancelled && setLoading(false));
    return () => {
      cancelled = true;
    };
  }, [minerId, embedded, requestedOptions]);

  if (!embedded && !onRequestDecisions) {
    return (
      <section className="panel map-panel">
        <div className="model-empty">
          No decision mining for the {minerName} model in this export. Produce
          it with <code>minera_export --decisions {minerId}</code>, or open the
          raw event log so it can be mined on demand.
        </div>
      </section>
    );
  }
  if (error) {
    return (
      <section className="panel conf-panel">
        <div className="conf-notice">
          <span className="conf-notice-mark">⚠</span>
          <div>
            <h3>Couldn't mine decisions</h3>
            <p>{error}</p>
          </div>
        </div>
      </section>
    );
  }
  if (loading || !fetched) {
    return (
      <section className="panel conf-panel">
        <p className="conf-loading">Mining decision points</p>
      </section>
    );
  }
  const setNumber = (name: keyof DecisionMiningOptions, raw: string) =>
    setOptions((current) => ({ ...current, [name]: raw === "" ? undefined : Number(raw) }));
  const applyOptions = () => {
    const attributes = (options.attributes ?? []).map((value) => value.trim()).filter(Boolean);
    const activityAttribute = options.activity_attribute?.trim();
    setRequestedOptions({
      ...options,
      attributes: attributes.length > 0 ? attributes : undefined,
      activity_attribute: activityAttribute || undefined,
    });
  };
  return <>
    {!embedded && onRequestDecisions && <section className="panel conf-panel">
      <div className="panel-head"><h3>Decision-tree controls</h3><span className="panel-meta">Bounded context and split search</span></div>
      <div className="align-settings-grid">
        <label>Max depth<input type="number" min={0} max={64} value={options.max_depth ?? ""} onChange={(event) => setNumber("max_depth", event.target.value)} /></label>
        <label>Min split<input type="number" min={2} value={options.min_split ?? ""} onChange={(event) => setNumber("min_split", event.target.value)} /></label>
        <label>Min leaf<input type="number" min={1} value={options.min_leaf ?? ""} onChange={(event) => setNumber("min_leaf", event.target.value)} /></label>
        <label>Min gain<input type="number" min={0} step={0.000001} value={options.min_gain ?? ""} onChange={(event) => setNumber("min_gain", event.target.value)} /></label>
        <label>Activity field<input placeholder="concept:name" value={options.activity_attribute ?? ""} onChange={(event) => setOptions((current) => ({ ...current, activity_attribute: event.target.value }))} /></label>
        <label>Context attributes<input placeholder="amount,region" value={(options.attributes ?? []).join(",")} onChange={(event) => setOptions((current) => ({ ...current, attributes: event.target.value.split(",") }))} /></label>
        <label className="checkbox-label"><input type="checkbox" checked={options.include_trace_attributes ?? true} onChange={(event) => setOptions((current) => ({ ...current, include_trace_attributes: event.target.checked }))} /> Include case attributes</label>
      </div>
      <button className="button" disabled={loading} onClick={applyOptions}>Remine decisions</button>
    </section>}
    <DecisionsResultView decisions={fetched} minerName={minerName} />
  </>;
}

// One readable rule per leaf of the mined tree: the conjunction of conditions
// on the path down, the branch the tree predicts there, and its support.
function DecisionRules({
  point,
}: {
  point: DecisionPoint;
}) {
  return (
    <div className="decision-tree">
      {flattenDecisionRules(point.tree).map((rule, index) => (
        <div className="decision-rule" key={index}>
          <span className="decision-rule-prefix">
            {rule.conditions.length > 0
              ? `if ${rule.conditions.join(" and ")}`
              : "always"}{" "}
            →
          </span>
          <span className="decision-rule-branch">
            {point.branches[rule.branch]?.label ?? "?"}
          </span>
          <span className="decision-rule-support">
            {numberFormat.format(rule.hit)} of {numberFormat.format(rule.total)}
          </span>
        </div>
      ))}
    </div>
  );
}

// The mined decision points, most-consulted first: each shows its branch
// frequencies and the attribute rules that explain the choice, with the
// tree's accuracy against the always-guess-the-majority baseline.
function DecisionsResultView({
  decisions,
  minerName,
}: {
  decisions: DecisionsResult;
  minerName: string;
}) {
  if (decisions.decision_points.length === 0) {
    return (
      <section className="panel map-panel">
        <div className="model-empty">
          The {minerName} model has no observed decision points —
          {decisions.observed_traces === 0
            ? " no trace replays cleanly against it, so no choice can be trusted."
            : " it offers no choices along the paths the log takes."}
        </div>
      </section>
    );
  }

  return (
    <div className="conformance reveal">
      {decisions.activity_attribute &&
        decisions.activity_attribute !== "concept:name" && (
          <p className="conf-inline-note">
            Replay activity field: <code>{decisions.activity_attribute}</code>
          </p>
        )}
      {decisions.observed_traces < decisions.trace_count && (
        <p className="conf-inline-note">
          Mined from the {numberFormat.format(decisions.observed_traces)} of{" "}
          {numberFormat.format(decisions.trace_count)} cases that replay
          cleanly against the {minerName} model — a deviating case's choices
          can't be trusted, so it contributes none.
        </p>
      )}
      {decisions.decision_points.map((point, index) => {
        const explains = point.accuracy > point.majority_share + 1e-9;
        return (
          <section className="panel conf-panel" key={point.place}>
            <div className="panel-head">
              <h2>Decision point {index + 1}</h2>
              <span className="panel-meta">
                {numberFormat.format(point.observations)} observations ·{" "}
                {point.branches.length} branches
              </span>
            </div>
            <div className="decision-branches">
              {point.branches.map((branch) => (
                <span className="decision-branch" key={branch.label}>
                  <span className="decision-branch-label">{branch.label}</span>
                  <span className="decision-branch-count">
                    {numberFormat.format(branch.count)}
                    <small>
                      {" "}
                      ·{" "}
                      {point.observations > 0
                        ? `${((branch.count / point.observations) * 100).toFixed(1)}%`
                        : "—"}
                    </small>
                  </span>
                </span>
              ))}
            </div>
            {explains ? (
              <>
                <DecisionRules point={point} />
                <p className="decision-accuracy">
                  These rules predict {percent(point.accuracy)} of the
                  observations; always guessing the most frequent branch gets{" "}
                  {percent(point.majority_share)}.
                </p>
              </>
            ) : (
              <p className="decision-accuracy">
                No attribute in scope separates these branches better than
                always guessing the most frequent one (
                {percent(point.majority_share)}).
              </p>
            )}
          </section>
        );
      })}
    </div>
  );
}

// Alignments are the costly check, so the server defers them: this wrapper uses
// the embedded result (CLI exports) when present, skips the work entirely for an
// unsound model, and otherwise fetches the result on demand for the model the
// user actually opened.
function Alignments({
  data,
  minerId,
  minerName,
  model,
  onRequestAlignment,
}: {
  data: MineraExport;
  minerId: string;
  minerName: string;
  model: ModelConformance;
  onRequestAlignment: (minerId: string, options?: AlignmentOptions) => Promise<AlignmentsResult>;
}) {
  const embedded = model.alignments;
  const [fetched, setFetched] = useState<AlignmentsResult | null>(embedded ?? null);
  const embeddedActivityAttribute = embedded?.activity_attribute &&
    embedded.activity_attribute !== "concept:name"
      ? embedded.activity_attribute
      : undefined;
  const initialAlignmentOptions: AlignmentOptions = {
    ...DEFAULT_ALIGNMENT_OPTIONS,
    ...(embeddedActivityAttribute
      ? { activity_attribute: embeddedActivityAttribute }
      : {}),
  };
  const [draftOptions, setDraftOptions] = useState<AlignmentOptions>(initialAlignmentOptions);
  const [appliedOptions, setAppliedOptions] = useState<AlignmentOptions>(initialAlignmentOptions);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const requestRef = useRef(onRequestAlignment);
  requestRef.current = onRequestAlignment;

  useEffect(() => {
    const isDefault = isDefaultAlignmentOptions(appliedOptions);
    if (embedded && isDefault && !embeddedActivityAttribute) {
      setFetched(embedded);
      return;
    }
    if (!model.sound) return;  // unsound: nothing to fetch, the notice says why
    let cancelled = false;
    setFetched(null);
    setError(null);
    setLoading(true);
    requestRef
      .current(minerId, appliedOptions)
      .then((result) => !cancelled && setFetched(result))
      .catch((cause) =>
        !cancelled && setError(cause instanceof Error ? cause.message : String(cause)),
      )
      .finally(() => !cancelled && setLoading(false));
    return () => {
      cancelled = true;
    };
  }, [minerId, model.sound, embedded, appliedOptions]);

  const settings = (
    <AlignmentSettings
      draft={draftOptions}
      onChange={setDraftOptions}
      applied={appliedOptions}
      onApply={(next) => {
        setError(null);
        setAppliedOptions(next);
      }}
    />
  );

  // Known up front from the mine: an unsound model can't be aligned at all.
  if (!model.sound) {
    return <AlignmentNotice minerName={minerName} exhausted={false} />;
  }
  if (error) {
    return (
      <>
        {settings}
        <section className="panel conf-panel">
        <div className="conf-notice">
          <span className="conf-notice-mark">⚠</span>
          <div>
            <h3>Couldn't compute alignments</h3>
            <p>{error}</p>
          </div>
        </div>
        </section>
      </>
    );
  }
  if (loading || !fetched) {
    return (
      <>
        {settings}
        <section className="panel conf-panel">
          <p className="conf-loading">Computing optimal alignments</p>
        </section>
      </>
    );
  }
  // Sound but the search could not finish (reachable yet unbounded/too large).
  if (!fetched.computed) {
    return <>{settings}<AlignmentNotice minerName={minerName} exhausted={fetched.exhausted} /></>;
  }
  return <>{settings}<AlignmentResultView data={data} alignments={fetched} /></>;
}

const ALIGNMENT_VARIANTS: { value: AlignmentVariant; label: string }[] = [
  { value: "state_equation_a_star", label: "State-equation A*" },
  { value: "dijkstra_no_heuristics", label: "Dijkstra (no heuristics)" },
  { value: "dijkstra_less_memory", label: "Dijkstra (less memory)" },
  { value: "discounted_a_star", label: "Discounted A*" },
  { value: "generator_dijkstra_no_heuristics", label: "Generator Dijkstra (all optima)" },
  { value: "generator_dijkstra_less_memory", label: "Generator Dijkstra (less memory)" },
];

function isDefaultAlignmentOptions(options: AlignmentOptions): boolean {
  return options.variant === DEFAULT_ALIGNMENT_OPTIONS.variant &&
    options.cost_profile === DEFAULT_ALIGNMENT_OPTIONS.cost_profile &&
    (options.activity_attribute === undefined ||
      options.activity_attribute.trim() === "concept:name") &&
    Object.keys(options).every((key) =>
      key === "variant" || key === "cost_profile" || key === "activity_attribute");
}

function AlignmentSettings({
  draft,
  applied,
  onChange,
  onApply,
}: {
  draft: AlignmentOptions;
  applied: AlignmentOptions;
  onChange: (options: AlignmentOptions) => void;
  onApply: (options: AlignmentOptions) => void;
}) {
  const variant = draft.variant ?? DEFAULT_ALIGNMENT_OPTIONS.variant;
  const generator = variant.startsWith("generator_");
  const discounted = variant === "discounted_a_star";
  const numeric = (key: keyof AlignmentOptions, value: string) => {
    const next = value.trim() === "" ? undefined : Number(value);
    onChange({ ...draft, [key]: next });
  };
  const invalid = isInvalidAlignmentOptions(draft);
  const appliedKey = JSON.stringify(applied);
  const draftKey = JSON.stringify(draft);
  const value = (key: keyof AlignmentOptions) => {
    const v = draft[key];
    return v === undefined ? "" : String(v);
  };
  return (
    <section className="panel conf-panel align-settings">
      <div className="panel-head">
        <h2>Alignment settings</h2>
        <span className="panel-meta">Search controls</span>
      </div>
      <div className="align-settings-grid">
        <label>Variant<select value={variant} onChange={(e) => onChange({ ...draft, variant: e.target.value as AlignmentVariant })}>
          {ALIGNMENT_VARIANTS.map((item) => <option key={item.value} value={item.value}>{item.label}</option>)}
        </select></label>
        <label>Cost profile<select value={draft.cost_profile ?? "legacy"} onChange={(e) => onChange({ ...draft, cost_profile: e.target.value as AlignmentCostProfile })}>
          <option value="legacy">Legacy (unit deviations)</option><option value="standard">Standard costs</option>
        </select></label>
        <label>Activity field<input value={value("activity_attribute")} onChange={(e) => onChange({ ...draft, activity_attribute: e.target.value })} placeholder="concept:name" /></label>
        <label>State limit<input type="number" min={1} step={1} value={value("max_states")} onChange={(e) => numeric("max_states", e.target.value)} placeholder="default" /></label>
        <label>Global timeout (s)<input type="number" min={0} step={0.1} value={value("max_align_time")} onChange={(e) => numeric("max_align_time", e.target.value)} placeholder="none" /></label>
        <label>Per-trace timeout (s)<input type="number" min={0} step={0.1} value={value("max_align_time_trace")} onChange={(e) => numeric("max_align_time_trace", e.target.value)} placeholder="none" /></label>
        <label className={!generator ? "disabled" : ""}>Maximum alternatives<input disabled={!generator} type="number" min={1} step={1} value={value("max_alignments")} onChange={(e) => numeric("max_alignments", e.target.value)} placeholder="default" /></label>
        <label className={!discounted ? "disabled" : ""}>Discount exponent<input disabled={!discounted} type="number" min={1.01} step={0.1} value={value("exponent")} onChange={(e) => numeric("exponent", e.target.value)} placeholder="2" /></label>
        <label className={!discounted ? "disabled checkbox-label" : "checkbox-label"}><input disabled={!discounted} type="checkbox" checked={draft.synchronous_dijkstra ?? true} onChange={(e) => onChange({ ...draft, synchronous_dijkstra: e.target.checked })} /> Synchronous Dijkstra</label>
      </div>
      <div className="align-costs">
        <span className="align-settings-caption">Move costs (profile defaults can be overridden)</span>
        {([ ["trace_cost", "Trace/log"], ["model_cost", "Visible model"], ["silent_model_cost", "Silent model"], ["sync_cost", "Synchronous"] ] as const).map(([key, label]) => (
          <label key={key}>{label}<input type="number" min={0} step={1} value={value(key)} onChange={(e) => numeric(key, e.target.value)} placeholder="default" /></label>
        ))}
      </div>
      <div className="align-settings-actions">
        {invalid && <span className="conf-inline-note">Enter valid non-negative numbers; limits and alternatives must be whole numbers, and the discount exponent must be greater than 1.</span>}
        <button type="button" className="button" disabled={invalid || draftKey === appliedKey} onClick={() => onApply(cleanAlignmentOptions(draft))}>Apply</button>
        <button type="button" className="button secondary" disabled={draftKey === JSON.stringify(DEFAULT_ALIGNMENT_OPTIONS)} onClick={() => { const reset = { ...DEFAULT_ALIGNMENT_OPTIONS }; onChange(reset); onApply(reset); }}>Reset</button>
      </div>
    </section>
  );
}

/** Whether alignment settings contain an invalid value that applies to the selected variant. */
export function isInvalidAlignmentOptions(draft: AlignmentOptions): boolean {
  const variant = draft.variant ?? DEFAULT_ALIGNMENT_OPTIONS.variant;
  const generator = variant.startsWith("generator_");
  const discounted = variant === "discounted_a_star";
  return (
    (draft.activity_attribute !== undefined && draft.activity_attribute.trim() === "") ||
    (draft.max_states !== undefined && (!Number.isInteger(draft.max_states) || draft.max_states < 1)) ||
    (draft.max_align_time !== undefined && (!Number.isFinite(draft.max_align_time) || draft.max_align_time < 0)) ||
    (draft.max_align_time_trace !== undefined && (!Number.isFinite(draft.max_align_time_trace) || draft.max_align_time_trace < 0)) ||
    (generator && draft.max_alignments !== undefined && (!Number.isInteger(draft.max_alignments) || draft.max_alignments < 1)) ||
    (discounted && draft.exponent !== undefined && (!Number.isFinite(draft.exponent) || draft.exponent <= 1)) ||
    [draft.trace_cost, draft.model_cost, draft.silent_model_cost, draft.sync_cost].some(
      (value) => value !== undefined && (!Number.isFinite(value) || value < 0),
    )
  );
}

export function cleanAlignmentOptions(options: AlignmentOptions): AlignmentOptions {
  const clean: AlignmentOptions = {
    variant: options.variant ?? DEFAULT_ALIGNMENT_OPTIONS.variant,
    cost_profile: options.cost_profile ?? DEFAULT_ALIGNMENT_OPTIONS.cost_profile,
  };
  const activityAttribute = options.activity_attribute?.trim();
  if (activityAttribute && activityAttribute !== "concept:name")
    clean.activity_attribute = activityAttribute;
  for (const key of ["max_states", "max_align_time", "max_align_time_trace", "trace_cost", "model_cost", "silent_model_cost", "sync_cost"] as const) {
    if (options[key] !== undefined) clean[key] = options[key];
  }
  if (options.variant?.startsWith("generator_") && options.max_alignments !== undefined) clean.max_alignments = options.max_alignments;
  if (options.variant === "discounted_a_star") {
    if (options.exponent !== undefined) clean.exponent = options.exponent;
    if (options.synchronous_dijkstra !== undefined) clean.synchronous_dijkstra = options.synchronous_dijkstra;
  }
  return clean;
}

// Explains why a model could not be aligned: unsound (unreachable end state) or
// exhausted (reachable but the search hit its bound).
function AlignmentNotice({
  minerName,
  exhausted,
}: {
  minerName: string;
  exhausted: boolean;
}) {
  return (
    <section className="panel conf-panel">
      <div className="conf-notice">
        <span className="conf-notice-mark">⚠</span>
        <div>
          <h3>Alignments unavailable for the {minerName} model</h3>
          {exhausted ? (
            <p>
              The {minerName} model is reachable, but the alignment search hit a
              configured state or time limit before finding optimal alignments.
              This can happen because the model is complex or admits unbounded
              behavior, so partial numbers are not shown.
            </p>
          ) : (
            <p>
              The {minerName} model isn't a <strong>sound workflow net</strong>:
              its end state can't be reached without leftover or duplicated
              tokens. (The Alpha Miner produces this when concurrent activities
              are the process's start or end points.) Optimal alignments require a
              sound net.
            </p>
          )}
          <p>
            <strong>Token Replay</strong> still works on this model, and the{" "}
            <strong>Inductive Miner</strong>'s model is always sound.
          </p>
        </div>
      </div>
    </section>
  );
}

/** Safe display label for ordinary and silent alignment moves. */
export function alignmentMoveLabel(
  move: AlignmentMove,
  activities: MineraExport["activities"],
): string {
  if (move.silent || (move.activity === null && !move.activity_name)) return "τ";
  if (move.activity_name) return move.activity_name;
  return move.activity === null ? "?" : activities[move.activity]?.name ?? "?";
}

const alignmentMoveIsSilent = (move: AlignmentMove) =>
  move.silent === true || (move.activity === null && !move.activity_name);

function alignmentDiagnosticItems(diagnostics?: AlignmentDiagnostics) {
  if (!diagnostics) return [];
  return [
    ["Visited states", diagnostics.visited_states],
    ["Queued states", diagnostics.queued_states],
    ["Traversed arcs", diagnostics.traversed_arcs],
    ["LP solves", diagnostics.lp_solved],
    ["Closed set", diagnostics.closed_set_length],
    ["Visited markings", diagnostics.num_visited_markings],
  ].filter((item): item is [string, number] => item[1] !== undefined);
}

// Renders a computed alignment result: cost-based gauges and, per deviating
// variant, the optimal alignment move by move so a log-only insertion or a
// model-only skip is visible at a glance.
function AlignmentResultView({
  data,
  alignments,
}: {
  data: MineraExport;
  alignments: AlignmentsResult;
}) {
  const fittingShare =
    alignments.trace_count > 0
      ? alignments.fitting_traces / alignments.trace_count
      : 1;
  const deviating = alignments.variants.filter((v) => v.cost > 0);
  const deviationRows = deviatingActivities(alignments.activity_deviations);

  const metrics: { label: string; value: number; hint: string }[] = [
    {
      label: "Fitness",
      value: alignments.fitness,
      hint: "Cost-based: how cheaply the log aligns to the model (1 = no deviation).",
    },
    // Alignment-based precision is absent in exports from older versions.
    ...(alignments.precision !== undefined
      ? [
          {
            label: "Precision",
            value: alignments.precision,
            hint: "Escaping edges at the states the optimal alignments visit: how much extra behavior the model allows.",
          },
        ]
      : []),
    {
      label: "Fitting traces",
      value: fittingShare,
      hint: `${numberFormat.format(alignments.fitting_traces)} of ${numberFormat.format(
        alignments.trace_count,
      )} cases align with zero cost.`,
    },
  ];

  return (
    <div className="conformance reveal">
      {(alignments.variant || alignments.alignment_variant || alignments.diagnostics) && (
        <section className="panel conf-panel align-diagnostics">
          <div className="panel-head">
            <h2>Alignment run</h2>
            <span className="panel-meta">{(alignments.variant ?? alignments.alignment_variant)?.replaceAll("_", " ") ?? "legacy export"}</span>
          </div>
          {alignments.diagnostics && (
            <div className="align-diagnostics-grid">
              {alignmentDiagnosticItems(alignments.diagnostics).map(([label, value]) => <span key={label}><strong>{label}</strong>{numberFormat.format(value)}</span>)}
            </div>
          )}
        </section>
      )}
      {alignments.activity_attribute &&
        alignments.activity_attribute !== "concept:name" && (
          <p className="conf-inline-note">
            Alignment activity field: <code>{alignments.activity_attribute}</code>
          </p>
        )}
      <section className="conf-metrics">
        {metrics.map((metric) => (
          <div className={`conf-gauge ${grade(metric.value)}`} key={metric.label}>
            <div className="conf-gauge-value">{percent(metric.value)}</div>
            <div className="conf-gauge-label">{metric.label}</div>
            <p className="conf-gauge-hint">{metric.hint}</p>
          </div>
        ))}
      </section>

      {deviationRows.length > 0 && (
        <section className="panel conf-panel">
          <div className="panel-head">
            <h2>Deviating activities</h2>
            <span className="panel-meta">
              across all {numberFormat.format(alignments.trace_count)} cases,
              worst first
            </span>
          </div>
          <table className="activity-table deviation-table">
            <thead>
              <tr>
                <th>Activity</th>
                <th
                  className="num"
                  title="Log and model agree on this activity"
                >
                  Synchronous
                </th>
                <th
                  className="num"
                  title="Log move: the log shows the activity where the model has no step for it"
                >
                  Inserted
                </th>
                <th
                  className="num"
                  title="Model move: the model requires the activity where the log lacks it"
                >
                  Skipped
                </th>
              </tr>
            </thead>
            <tbody>
              {deviationRows.map((row) => (
                <tr key={row.activity_name ?? row.activity ?? "unknown"}>
                  <td>{row.activity_name ?? (row.activity === null ? "?" : data.activities[row.activity]?.name ?? "?")}</td>
                  <td className="num">
                    {row.synchronous > 0
                      ? numberFormat.format(row.synchronous)
                      : "·"}
                  </td>
                  <td className="num">
                    {row.log_moves > 0
                      ? numberFormat.format(row.log_moves)
                      : "·"}
                  </td>
                  <td className="num">
                    {row.model_moves > 0
                      ? numberFormat.format(row.model_moves)
                      : "·"}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>
      )}

      <section className="panel conf-panel">
        <div className="panel-head">
          <h2>Optimal alignments</h2>
          <span className="panel-meta">
            {deviating.length > 0
              ? `${numberFormat.format(deviating.length)} deviating · total cost ${numberFormat.format(
                  alignments.total_cost,
                )}`
              : "every variant aligns perfectly"}
          </span>
        </div>

        <div className="align-legend">
          <span className="align-move sync">A</span> sync — log &amp; model agree
          <span className="align-move log">A</span> log only — inserted
          <span className="align-move model">A</span> model only — skipped
        </div>

        {deviating.length === 0 ? (
          <p className="conf-allfit">
            Every distinct path walks the model with only synchronous moves — no
            log-only or model-only deviations.
          </p>
        ) : (
          <ol className="variant-list">
            {deviating.map((variant, index) => {
              const share =
                alignments.trace_count > 0
                  ? variant.count / alignments.trace_count
                  : 0;
              return (
                <li className="variant" key={index}>
                  <div className="variant-top">
                    <span className={`conf-pill ${grade(variant.fitness)}`}>
                      {percent(variant.fitness)}
                    </span>
                    <span className="variant-count">
                      {numberFormat.format(variant.count)}{" "}
                      <small>
                        {(share * 100).toFixed(1)}% · cost{" "}
                        {numberFormat.format(variant.cost)}
                      </small>
                    </span>
                  </div>
                  <div className="align-moves">
                    {variant.moves.map((move, i) => {
                      const name = alignmentMoveLabel(move, data.activities);
                      return (
                        <span
                          key={i}
                          className={`align-move ${move.type}${alignmentMoveIsSilent(move) ? " silent" : ""}`}
                          title={alignmentMoveIsSilent(move) ? "model (silent): τ" : `${move.type}: ${name}`}
                        >
                          {name}
                        </span>
                      );
                    })}
                  </div>
                  {alignmentDiagnosticItems(variant.diagnostics).length > 0 && (
                    <div className="align-variant-diagnostics">
                      {alignmentDiagnosticItems(variant.diagnostics).map(([label, value]) => (
                        <span key={label} title={label}>{label}: {numberFormat.format(value)}</span>
                      ))}
                    </div>
                  )}
                  {(() => {
                    const alternatives = variant.alternatives ?? variant.optimal_alignments;
                    return alternatives && alternatives.length > 0 ? (
                      <details className="align-alternatives">
                        <summary>{numberFormat.format(alternatives.length)} equal-optimal alignments (including primary)</summary>
                        {alternatives.map((moves, altIndex) => (
                          <div className="align-moves" key={altIndex}>
                            {moves.map((move, i) => {
                              const name = alignmentMoveLabel(move, data.activities);
                              const silent = alignmentMoveIsSilent(move);
                              return <span key={i} className={`align-move ${move.type}${silent ? " silent" : ""}`} title={silent ? "model (silent): τ" : `${move.type}: ${name}`}>{name}</span>;
                            })}
                          </div>
                        ))}
                      </details>
                    ) : null;
                  })()}
                </li>
              );
            })}
          </ol>
        )}
      </section>
    </div>
  );
}
