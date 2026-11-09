import { type ReactNode, useState } from "react";
import { useStepFocus } from "../step-focus";
import { downloadBlob } from "../download";
import { type ModelLayouts, type ModelNet } from "../model-layouts";
import { type MineraExport } from "../types";
import { MINER_GROUPS, type MinerVariant } from "../miners";
import { fetchBpmn, fetchPlayout, fetchPnml, fetchPtml, type CsvOptions } from "../api";
import { ConstraintDiscovery } from "./ConstraintDiscovery";
import { GraphDiscovery } from "./GraphDiscovery";
import { ProcessTree } from "./ProcessTree";
import { PetriNet } from "./PetriNet";
import { DependencyGraph } from "./DependencyGraph";
import { PowlModel } from "./PowlModel";
import { NativeDiscovery } from "./NativeDiscovery";
import { WorkflowDiscovery } from "./WorkflowDiscovery";

type Step = "miner" | "variant" | "result";

interface ProcessModelProps {
  /** The analysis, used to mine the heuristic miner's dependency graph live. */
  data: MineraExport;
  /** The analysis's process tree and workflow nets, each laid out only when the shown model needs it. */
  layouts: ModelLayouts;
  /** Whether the backend can export PNML (a log was mined this session). */
  canExportPnml: boolean;
  /** Mined source the shown models came from; binds playout to it. */
  playoutSourceId?: string;
  mining?: boolean;
  onUseModel?: (file: File, sourceId: string, activityKey?: string) => void;
  onOpenPlayout?: (file: File,activityKey?:string,options?:CsvOptions) => void;
}

// A small wizard: pick a miner family, then a variant, then the mined model is
// shown. The breadcrumb steps back to any earlier choice.
export function ProcessModel({
  data,
  layouts,
  canExportPnml,
  playoutSourceId,
  mining = false,
  onUseModel,
  onOpenPlayout,
}: ProcessModelProps) {
  const [step, setStep] = useState<Step>("miner");
  const flowRef = useStepFocus<HTMLDivElement>(step);
  const [groupId, setGroupId] = useState<string | null>(null);
  const [variantId, setVariantId] = useState<string | null>(null);

  const group = MINER_GROUPS.find((g) => g.id === groupId) ?? null;
  const variant = group?.variants.find((v) => v.id === variantId) ?? null;

  const chooseMiner = (id: string) => {
    setGroupId(id);
    setVariantId(null);
    setStep("variant");
  };
  const chooseVariant = (id: string) => {
    setVariantId(id);
    setStep("result");
  };

  return (
    <div className="miner-flow reveal" ref={flowRef}>
      <nav className="miner-trail">
        <button
          className={step === "miner" ? "trail-item active" : "trail-item"}
          onClick={() => setStep("miner")}
        >
          Miner
        </button>
        {group && step !== "miner" && (
          <>
            <span className="trail-sep">›</span>
            <button
              className={step === "variant" ? "trail-item active" : "trail-item"}
              onClick={() => setStep("variant")}
            >
              {group.name}
            </button>
          </>
        )}
        {variant && step === "result" && (
          <>
            <span className="trail-sep">›</span>
            <span className="trail-item current">{variant.name}</span>
          </>
        )}
      </nav>

      {step === "miner" && (
        <div className="miner-step">
          <p className="miner-step-label">Step 1 — Discovery miner</p>
          <h2 className="miner-step-title">Choose a miner</h2>
          <div className="miner-cards">
            {MINER_GROUPS.map((g) => (
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
          </div>
        </div>
      )}

      {step === "variant" && group && (
        <div className="miner-step">
          <p className="miner-step-label">Step 2 — {group.name} variant</p>
          <h2 className="miner-step-title">Choose a variant</h2>
          <div className="miner-cards">
            {group.variants.map((v) => (
              <button
                key={v.id}
                className="miner-card"
                onClick={() => chooseVariant(v.id)}
              >
                <span className="miner-card-name">{v.name}</span>
                <p className="miner-card-desc">{v.description}</p>
                <span className="miner-card-go">Mine →</span>
              </button>
            ))}
          </div>
        </div>
      )}

      {step === "result" && group && variant && (
        <ModelResult
          key={variant.id}
          variant={variant}
          minerId={group.id}
          groupName={group.name}
          data={data}
          layouts={layouts}
          canExportPnml={canExportPnml}
          playoutSourceId={playoutSourceId}
          mining={mining}
          onUseModel={onUseModel}
          onOpenPlayout={onOpenPlayout}
        />
      )}
    </div>
  );
}

interface ModelResultProps {
  variant: MinerVariant;
  minerId: string;
  groupName: string;
  data: MineraExport;
  layouts: ModelLayouts;
  canExportPnml: boolean;
  playoutSourceId?: string;
  mining: boolean;
  onUseModel?: (file: File, sourceId: string, activityKey?: string) => void;
  onOpenPlayout?: (file: File,activityKey?:string,options?:CsvOptions) => void;
}

// Renders the mined model for the chosen variant by its artifact type. A null
// layout means the export carries no model of that kind (an older export, or a
// log too small to mine one), so an honest placeholder is shown instead.
// The tree and dependency-graph artifacts can flip to the model's workflow
// net — the exact net conformance checks and exports use — where the replay
// annotations (firing counts, place waits) live.
function ModelResult({
  variant,
  minerId,
  groupName,
  data,
  layouts,
  canExportPnml,
  playoutSourceId,
  mining,
  onUseModel,
  onOpenPlayout,
}: ModelResultProps) {

  // Every miner exports both ways: PNML from its workflow net, BPMN from the
  // inductive tree or from the net's token semantics (converted server-side),
  // and PTML when the selected workflow net is exactly block structured.
  const actions = canExportPnml ? (
    <>
      <PnmlButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <BpmnButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <PtmlButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <PlayoutButton minerId={minerId} dataset={data.dataset} sourceId={playoutSourceId} />
    </>
  ) : null;
  // The workflow-discovery panels export the model they discover; the model mined with this analysis (the one
  // conformance checks) stays exportable beside them.
  // Only whether the analysis carries the net: the workflow-discovery panels lay out what they show themselves.
  const savedNet = (["alpha", "heuristic", "inductive", "powl"] as const).includes(minerId as ModelNet) && layouts.hasNet(minerId as ModelNet);
  const saved = actions && savedNet ? (
    <section className="panel saved-model-exports" aria-label="Analysis model exports">
      <div className="panel-head"><h2>Model mined with this analysis</h2><span className="panel-meta">{groupName} · analysis settings</span></div>
      <div className="query-actions">{actions}</div>
    </section>
  ) : null;
  const savedView = savedNet || minerId === "heuristic" || minerId === "inductive" && layouts.tree() !== null || minerId === "powl" && !!data.powl
    ? <AnalysisModelView minerId={minerId as ModelNet} title={`${groupName} · mined with this analysis (analysis settings)`} data={data} layouts={layouts} />
    : null;
  if (minerId === "ilp" || minerId === "genetic")
    return <NativeDiscovery family={minerId} data={data} available={canExportPnml} mining={mining} onUseModel={onUseModel} />;
  if (minerId === "alpha" || minerId === "heuristic")
    return <>{saved}<WorkflowDiscovery family={minerId} variant={variant.id as "classic"|"plus"|"plusplus"} savedView={savedView} data={data} available={canExportPnml} mining={mining} onUseModel={onUseModel} onOpenPlayout={onOpenPlayout} /></>;
  if (minerId === "inductive" || minerId === "powl")
    return <>{saved}<WorkflowDiscovery family={minerId} variant={(variant.id==='basic'?'log':variant.id==='configured'?'maximal':variant.id) as "log"|"imf"|"dfg"|"tree"|"brute_force"|"dynamic_clustering"|"dynamic_clustering_plain"} savedView={savedView} data={data} available={canExportPnml} mining={mining} onUseModel={onUseModel} onOpenPlayout={onOpenPlayout} /></>;
  if (minerId === "declare" || minerId === "log_skeleton")
    return <ConstraintDiscovery family={minerId} data={data} available={canExportPnml} mining={mining} onOpenLog={onOpenPlayout} />;
  if (minerId === "footprints" || minerId === "transition_system")
    return <GraphDiscovery family={minerId} data={data} available={canExportPnml} mining={mining} />;

  return null;
}

/**
 * The model the analysis mined (with the Filters & miner settings), shown while no variant run is on screen. A tree,
 * dependency graph or POWL model flips to the same workflow net conformance checks, and every net carries the
 * analysis's replay decoration (Plain / Frequency / Performance) when the export has one.
 */
export function AnalysisModelView({ minerId, title, data, layouts }: { minerId: ModelNet; title: string; data: MineraExport; layouts: ModelLayouts }) {
  const [asNet, setAsNet] = useState(false);
  const toggle = (primaryLabel: string) =>
    layouts.hasNet(minerId) ? (
      <div className="map-mode" role="group" aria-label="Model view">
        <button className={asNet ? "mode-btn" : "mode-btn active"} aria-pressed={!asNet} onClick={() => setAsNet(false)}>{primaryLabel}</button>
        <button className={asNet ? "mode-btn active" : "mode-btn"} aria-pressed={asNet} onClick={() => setAsNet(true)}>Workflow net</button>
      </div>
    ) : null;
  const decoration = minerId === "alpha" ? data.petri_net_decoration : minerId === "inductive" ? data.inductive_petri_net_decoration : minerId === "heuristic" ? data.heuristic_petri_net_decoration : data.powl_petri_net_decoration;
  const primary = minerId === "inductive" ? "Tree" : minerId === "heuristic" ? "Dependency graph" : minerId === "powl" ? "POWL" : null;
  const net = minerId === "alpha" || asNet ? layouts.net(minerId) : null;
  if (net) return <PetriNet layout={net} heading="Workflow net" title={title} actions={primary ? toggle(primary) : undefined} decoration={decoration ?? null} disconnectedDescription={minerId === "alpha" ? undefined : "transitions with no incident arcs"} />;
  if (minerId === "inductive") {
    const tree = layouts.tree();
    return tree ? <ProcessTree layout={tree} title={title} actions={toggle("Tree")} /> : <Placeholder>No process tree is available in this export — re-open the CSV event log to mine one.</Placeholder>;
  }
  // The dependency graph comes from the directly-follows counts every analysis carries.
  if (minerId === "heuristic") return <DependencyGraph data={data} title={title} actions={toggle("Dependency graph")} />;
  if (minerId === "powl") return data.powl ? <PowlModel model={data.powl} data={data} title={title} actions={toggle("POWL")} /> : <Placeholder>No POWL model is available in this export — re-open the CSV event log to discover one.</Placeholder>;
  return <Placeholder>No workflow net is available in this export — re-open the CSV event log to mine one.</Placeholder>;
}

// Downloads the shown model as standard PNML. Every miner is translated to a
// workflow net server-side — the same net used for conformance — so the export
// is faithful to what Minera actually checks.
function PnmlButton({
  minerId,
  groupName,
  dataset,
}: {
  minerId: string;
  groupName: string;
  dataset: string;
}) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchPnml(minerId)
      .then((xml) => {
        const blob = new Blob([xml], { type: "application/xml" });
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        downloadBlob(blob, `${base}-${minerId}.pnml`);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : `Download ${groupName} as PNML`}
      </button>
      {error && <span className="pnml-error" role="alert">{error}</span>}
    </span>
  );
}

// Downloads the shown model as BPMN 2.0. The conversion runs server-side —
// from the process tree for the inductive miner, from the workflow net's
// token semantics for the others — so the export matches the model Minera
// actually discovered.
function BpmnButton({
  minerId,
  groupName,
  dataset,
}: {
  minerId: string;
  groupName: string;
  dataset: string;
}) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchBpmn(minerId)
      .then((xml) => {
        const blob = new Blob([xml], { type: "application/xml" });
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        downloadBlob(blob, `${base}-${minerId}.bpmn`);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : `Download ${groupName} as BPMN`}
      </button>
      {error && <span className="pnml-error" role="alert">{error}</span>}
    </span>
  );
}

// Downloads the inductive miner's process tree as PTML, the block-structured
// tree interchange format other process-mining tools read.
function PtmlButton({
  minerId,
  groupName,
  dataset,
}: {
  minerId: string;
  groupName: string;
  dataset: string;
}) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchPtml(minerId)
      .then((xml) => {
        const blob = new Blob([xml], { type: "application/xml" });
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        downloadBlob(blob, `${base}-${minerId}.ptml`);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : `Download ${groupName} as PTML`}
      </button>
      {error && <span className="pnml-error" role="alert">{error}</span>}
    </span>
  );
}

/** Cases the "Simulate log" button asks a Petri miner for: pm4py's play_out default. */
export const SIMULATED_LOG_TRACES = 1000;

// Petri miners get an explicit trace count, so the download keeps pm4py's 1000
// cases rather than a server default that may shrink to fit its event budget.
// The other playout families keep their own model-specific defaults.
export function simulatedLogOptions(minerId: string): { traces?: number } {
  return ["declare", "dfg", "process_tree", "stochastic", "monte_carlo"].includes(minerId)
    ? {}
    : { traces: SIMULATED_LOG_TRACES };
}

// Simulates the shown model into an XES log using the backend's model-specific
// defaults. Declare traces are prefix-safe and may retain pending obligations.
function PlayoutButton({
  minerId,
  dataset,
  sourceId,
}: {
  minerId: string;
  dataset: string;
  sourceId?: string;
}) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchPlayout(minerId, simulatedLogOptions(minerId), sourceId)
      .then((xml) => {
        const blob = new Blob([xml], { type: "application/xml" });
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        downloadBlob(blob, `${base}-${minerId}-playout.xes`);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Simulating…" : "Simulate log (XES)"}
      </button>
      {error && <span className="pnml-error" role="alert">{error}</span>}
    </span>
  );
}

function Placeholder({ children }: { children: ReactNode }) {
  return (
    <section className="panel map-panel">
      <div className="model-empty">{children}</div>
    </section>
  );
}
