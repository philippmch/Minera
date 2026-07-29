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
  numberFormat,
} from "../types";
import { parseDfgCostMapInput, parseMarkingInput, type ImportedModelCheck } from "../api";
import {
  CONFORMANCE_METHODS,
  deviatingActivities,
  flattenDecisionRules,
} from "../conformance";
import { MINER_GROUPS } from "../miners";

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
}: {
  data: MineraExport;
  onRequestAlignment: (minerId: string, options?: AlignmentOptions) => Promise<AlignmentsResult>;
  onRequestDecisions?: (minerId: string) => Promise<DecisionsResult>;
  onCheckModel?: (file: File) => Promise<ImportedModelCheck>;
  onRequestDfg?: (options?: DfgAlignmentOptions) => Promise<DfgAlignmentResult>;
  onRequestEditDistance?: (reference: File, options?: EditDistanceOptions) => Promise<EditDistanceResult>;
  onRequestProcessTree?: (options?: ProcessTreeAlignmentOptions) => Promise<ProcessTreeAlignmentResult>;
  onRequestDecomposed?: (miner: string, options?: DecomposedAlignmentOptions) => Promise<DecomposedAlignmentResult>;
  onRequestPetriAnti?: (miner: string, options?: PetriAntiAlignmentOptions) => Promise<PetriAntiAlignmentResult>;
  onRequestPetriMulti?: (miner: string, options?: PetriMultiAlignmentOptions) => Promise<PetriMultiAlignmentResult>;
  onRequestReduction?: (traceIndex: number, options?: ProcessTreeReductionOptions) => Promise<ProcessTreeReductionResult>;
}) {
  const [step, setStep] = useState<Step>("miner");
  const [minerId, setMinerId] = useState<string | null>(null);
  const [methodId, setMethodId] = useState<string | null>(null);

  // A user-uploaded PNML or BPMN model, checked server-side against the mined log.
  const [imported, setImported] = useState<ImportedModelCheck | null>(null);
  const [importing, setImporting] = useState(false);
  const [importError, setImportError] = useState<string | null>(null);

  // A re-mine invalidates the uploaded model's results (and the server drops
  // its cached net), so fall back to the start of the wizard.
  useEffect(() => {
    setImported(null);
    setImportError(null);
    setMinerId(null);
    setStep("miner");
  }, [data]);

  const isImported = minerId === IMPORTED;
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
    setImporting(true);
    setImportError(null);
    try {
      const result = await onCheckModel(file);
      setImported(result);
      chooseMiner(IMPORTED);
    } catch (cause) {
      setImportError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      setImporting(false);
    }
  };

  // The backend checks every discovered model both ways, keyed by miner id, so
  // any miner × any method has results — as long as this export carries them.
  // The imported model's results come from the upload (or are embedded when a
  // CLI export was produced with --check-pnml / --check-bpmn).
  const models = isImported
    ? imported?.model ?? data.conformance?.[IMPORTED]
    : miner
      ? data.conformance?.[miner.id]
      : undefined;
  const supported = Boolean(models && method);

  // An uploaded model can name activities the log never shows; its response
  // carries the extended registry, so the result panels resolve every id.
  const viewData =
    isImported && imported ? { ...data, activities: imported.activities } : data;

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
            {imported || data.conformance?.[IMPORTED] ? (
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
      <OtherAlignments data={data} importedAvailable={Boolean(imported || data.conformance?.imported)} onRequestDfg={onRequestDfg} onRequestEditDistance={onRequestEditDistance} onRequestProcessTree={onRequestProcessTree} onRequestDecomposed={onRequestDecomposed} onRequestPetriAnti={onRequestPetriAnti} onRequestPetriMulti={onRequestPetriMulti} onRequestReduction={onRequestReduction} />
    </div>
  );
}

function OtherAlignments({ data, importedAvailable, onRequestDfg, onRequestEditDistance, onRequestProcessTree, onRequestDecomposed, onRequestPetriAnti, onRequestPetriMulti, onRequestReduction }: {
  data: MineraExport;
  importedAvailable?: boolean;
  onRequestDfg?: (options?: DfgAlignmentOptions) => Promise<DfgAlignmentResult>;
  onRequestEditDistance?: (reference: File, options?: EditDistanceOptions) => Promise<EditDistanceResult>;
  onRequestProcessTree?: (options?: ProcessTreeAlignmentOptions) => Promise<ProcessTreeAlignmentResult>;
  onRequestDecomposed?: (miner: string, options?: DecomposedAlignmentOptions) => Promise<DecomposedAlignmentResult>;
  onRequestPetriAnti?: (miner: string, options?: PetriAntiAlignmentOptions) => Promise<PetriAntiAlignmentResult>;
  onRequestPetriMulti?: (miner: string, options?: PetriMultiAlignmentOptions) => Promise<PetriMultiAlignmentResult>;
  onRequestReduction?: (traceIndex: number, options?: ProcessTreeReductionOptions) => Promise<ProcessTreeReductionResult>;
}) {
  const [dfg, setDfg] = useState<DfgAlignmentResult | null>(null);
  const [edit, setEdit] = useState<EditDistanceResult | null>(null);
  const [processTree, setProcessTree] = useState<ProcessTreeAlignmentResult | null>(null);
  const [reference, setReference] = useState<File | null>(null);
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
  const [busy, setBusy] = useState<"dfg" | "edit" | "process-tree" | "decomposed" | "anti" | "multi" | "reduction" | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [costText, setCostText] = useState<Record<string, string>>({ sync_cost: "", model_move_cost: "", log_move_cost: "", internal_log_move_cost: "" });
  useEffect(() => { setDfg(null); setEdit(null); setProcessTree(null); setDecomposed(null); setAnti(null); setMulti(null); setReduced(null); setReference(null); setError(null); setTraceIndex(0); }, [data]);
  useEffect(() => { setPetriMiner((current) => petriMiners.includes(current) ? current : (petriMiners[0] ?? "")); }, [petriMinerKey]);
  const runDfg = async () => { if (!onRequestDfg) return; setBusy("dfg"); setError(null); try { const maps = Object.fromEntries(Object.entries(costText).map(([key, text]) => [key, parseDfgCostMapInput(text)])); setDfg(await onRequestDfg({ ...dfgOptions, ...maps })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runEdit = async () => { if (!onRequestEditDistance || !reference) return; setBusy("edit"); setError(null); try { setEdit(await onRequestEditDistance(reference, editOptions)); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runProcessTree = async () => { if (!onRequestProcessTree || !data.process_tree) return; setBusy("process-tree"); setError(null); try { setProcessTree(await onRequestProcessTree(processTreeOptions)); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const parseCostMap = (value: string) => value.trim() ? parseDfgCostMapInput(value) : undefined;
  const runDecomposed = async () => { if (!onRequestDecomposed) return; setBusy("decomposed"); setError(null); try { setDecomposed(await onRequestDecomposed(petriMiner, { ...decomposedOptions, initial_marking: parseMarkingInput(decomposedText.initial_marking), final_marking: parseMarkingInput(decomposedText.final_marking), activity_log_costs: parseCostMap(decomposedText.activity_log_costs), activity_model_costs: parseCostMap(decomposedText.activity_model_costs), activity_sync_costs: parseCostMap(decomposedText.activity_sync_costs) })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runAnti = async () => { if (!onRequestPetriAnti) return; setBusy("anti"); setError(null); try { setAnti(await onRequestPetriAnti(petriMiner, { ...advancedOptions, initial_marking: parseMarkingInput(petriText.antiInitial), final_marking: parseMarkingInput(petriText.antiFinal) })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runMulti = async () => { if (!onRequestPetriMulti) return; setBusy("multi"); setError(null); try { setMulti(await onRequestPetriMulti(petriMiner, { ...multiOptions, initial_marking: parseMarkingInput(petriText.multiInitial), final_marking: parseMarkingInput(petriText.multiFinal) })); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  const runReduction = async () => { if (!onRequestReduction || !data.process_tree || !data.traces?.length) return; setBusy("reduction"); setError(null); try { setReduced(await onRequestReduction(traceIndex, reductionOptions)); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setBusy(null); } };
  return <section className="panel conf-panel other-alignments">
    <div className="panel-head"><h2>Other alignments</h2><span className="panel-meta">Native decomposed · Petri anti/multi · process-tree reduction</span></div>
    <div className="other-align-grid">
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
      <div className="other-align-card"><h3>Edit distance</h3><p>Compare mined traces with a reference CSV/XES log.</p><label className="file-picker">Reference log<input type="file" accept=".csv,.xes,.xml" onChange={e => setReference(e.target.files?.[0] ?? null)} /></label>
        <div className="align-settings-grid"><label>Mode<select value={editOptions.anti ? "anti" : "closest"} onChange={e => setEditOptions({ ...editOptions, anti: e.target.value === "anti" })}><option value="closest">Closest</option><option value="anti">Anti-alignment</option></select></label><label>Deviation cost<input type="number" min={1} value={editOptions.deviation_cost ?? ""} onChange={e => setEditOptions({ ...editOptions, deviation_cost: e.target.value ? Number(e.target.value) : undefined })} /></label><label>Projection<select value={editOptions.projection ?? "none"} onChange={e => setEditOptions({ ...editOptions, projection: e.target.value as EditDistanceOptions["projection"] })}><option value="none">None</option><option value="log">Projected log</option></select></label></div>
        <button className="button" disabled={!onRequestEditDistance || !reference || busy !== null} onClick={() => void runEdit()}>{busy === "edit" ? "Aligning…" : "Run edit-distance alignment"}</button>{edit && <EditDistanceResultView result={edit} />}
      </div>
    </div>{error && <div className="conf-notice"><span className="conf-notice-mark">⚠</span><p>{error}</p></div>}
  </section>;
}

function DfgResult({ result }: { result: DfgAlignmentResult }) { return <div className="other-result"><p><strong>{numberFormat.format(result.alignments.length)}</strong> traces · {numberFormat.format(result.unique_variants)} unique variants searched</p><ol className="variant-list">{result.alignments.slice(0, 30).map((a, i) => <li className="variant" key={i}><div className="variant-top"><span className={`conf-pill ${grade(a.fitness)}`}>{percent(a.fitness)}</span><span className="variant-count">cost {numberFormat.format(a.cost)} · internal {numberFormat.format(a.internal_cost)} · states {numberFormat.format(a.visited_states)} · closed {numberFormat.format(a.closed_states)}</span></div><div className="align-moves">{a.moves.map((m, j) => <span className={`align-move ${m.log === ">>" ? "model" : m.model === ">>" ? "log" : "sync"}`} key={j}>{m.log} / {m.model}</span>)}</div></li>)}</ol>{result.projected_dfg && <small>{result.projected_dfg.edges.length} projected edges · {result.projected_dfg.activities.length} activities</small>}</div>; }
function DecomposedResultView({ result }: { result: DecomposedAlignmentResult }) { const first = result.traces[0]; const moves = first?.moves ?? first?.alignment ?? []; return <div className="other-result"><p><strong>{numberFormat.format(result.traces.length)}</strong> traces · {percent(result.fitness)} fitness · cost {numberFormat.format(result.total_cost)}</p><small>Visited {numberFormat.format(result.diagnostics.visited_states)} states · queued {numberFormat.format(result.diagnostics.queued_states)} · merges {numberFormat.format(result.diagnostics.merges)} · {result.complete ? "complete" : "incomplete"}{result.timed_out ? " · timed out" : ""}{result.exhausted ? " · exhausted" : ""}</small>{first && <div className="align-moves">{moves.map((move, i) => <span className={`align-move ${move.log === ">>" ? "model" : move.model === ">>" ? "log" : "sync"}`} key={i}>{move.log ?? "τ"} / {move.model ?? "τ"}</span>)}</div>}</div>; }
function PetriResultView({ kind, result }: { kind: "anti" | "multi"; result: PetriAntiAlignmentResult | PetriMultiAlignmentResult }) { return <div className="other-result"><p><strong>{kind === "anti" ? "Anti-alignment" : "Multi-alignment"}</strong> · objective {numberFormat.format(result.objective)}</p><div className="align-moves">{result.activities.map((activity, i) => <span className="align-move model" key={i}>{activity}</span>)}</div><small>Visible sequence</small><div className="align-moves">{result.transitions.map((transition, i) => <span className="align-move model" key={i}>t{transition}</span>)}</div><small>Transition sequence (includes silent moves) · visited {numberFormat.format(result.diagnostics.visited_states)} states · traversed {numberFormat.format(result.diagnostics.traversed_arcs)} · {result.diagnostics.exhausted ? "search exhausted" : "complete"}{kind === "anti" && "precision" in result ? ` · precision ${percent(result.precision)}` : kind === "multi" && "max_distance_to_log" in result ? ` · max distance ${numberFormat.format(result.max_distance_to_log)}` : ""}</small></div>; }
function EditDistanceResultView({ result }: { result: EditDistanceResult }) { return <div className="other-result"><p><strong>{numberFormat.format(result.source_count)}</strong> source traces · {result.anti ? "anti-alignment" : "closest alignment"}</p><ol className="variant-list">{result.alignments.slice(0, 30).map((a, i) => <li className="variant" key={i}><div className="variant-top"><span className={`conf-pill ${grade(a.fitness)}`}>{percent(a.fitness)}</span><span className="variant-count">cost {numberFormat.format(a.cost)} · best/worst {numberFormat.format(a.best_worst_cost)} · denominator {numberFormat.format(a.denominator)} · target #{a.target_index}</span></div><div className="align-moves">{a.moves.map((m, j) => <span className={`align-move ${m.type}`} key={j}>{m.log} / {m.model}</span>)}</div></li>)}</ol></div>; }

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
                      const name = data.activities[activityId]?.name ?? "?";
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
  onRequestDecisions?: (minerId: string) => Promise<DecisionsResult>;
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
      .current(minerId)
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
  }, [minerId, embedded]);

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
  return <DecisionsResultView decisions={fetched} minerName={minerName} />;
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
  const [draftOptions, setDraftOptions] = useState<AlignmentOptions>({ ...DEFAULT_ALIGNMENT_OPTIONS });
  const [appliedOptions, setAppliedOptions] = useState<AlignmentOptions>({ ...DEFAULT_ALIGNMENT_OPTIONS });
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const requestRef = useRef(onRequestAlignment);
  requestRef.current = onRequestAlignment;

  useEffect(() => {
    const isDefault = isDefaultAlignmentOptions(appliedOptions);
    if (embedded && isDefault) {
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
    Object.keys(options).every((key) => key === "variant" || key === "cost_profile");
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
  if (move.silent || move.activity === null) return "τ";
  return activities[move.activity]?.name ?? "?";
}

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
                <tr key={row.activity}>
                  <td>{data.activities[row.activity]?.name ?? "?"}</td>
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
                          className={`align-move ${move.type}${move.silent || move.activity === null ? " silent" : ""}`}
                          title={move.silent || move.activity === null ? "model (silent): τ" : `${move.type}: ${name}`}
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
                              const silent = move.silent || move.activity === null;
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
