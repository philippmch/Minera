import { type ReactNode, useState } from "react";
import { type TreeLayout } from "../tree-layout";
import { type PetriLayout } from "../petri-layout";
import { type MineraExport } from "../types";
import { MINER_GROUPS, type MinerVariant } from "../miners";
import { fetchBpmn, fetchPlayout, fetchPnml, fetchPtml } from "../api";
import { ProcessTree } from "./ProcessTree";
import { PetriNet } from "./PetriNet";
import { DependencyGraph } from "./DependencyGraph";
import { PowlModel } from "./PowlModel";
import { LogSkeletonModel } from "./LogSkeletonModel";
import { DeclareModel } from "./DeclareModel";
import { FootprintsModel } from "./FootprintsModel";
import { TransitionSystemModel } from "./TransitionSystemModel";
import { NativeDiscovery } from "./NativeDiscovery";
import { WorkflowDiscovery } from "./WorkflowDiscovery";

type Step = "miner" | "variant" | "result";

interface ProcessModelProps {
  /** The analysis, used to mine the heuristic miner's dependency graph live. */
  data: MineraExport;
  /** Layout for the inductive miner's process tree, if the export carries one. */
  treeLayout: TreeLayout | null;
  /** Layout for the alpha miner's Petri net, if the export carries one. */
  petriLayout: PetriLayout | null;
  /** Layout for the inductive model's workflow net, if the export carries one. */
  inductivePetriLayout: PetriLayout | null;
  /** Layout for the heuristic model's workflow net, if the export carries one. */
  heuristicPetriLayout: PetriLayout | null;
  powlPetriLayout: PetriLayout | null;
  /** Whether the backend can export PNML (a log was mined this session). */
  canExportPnml: boolean;
  mining?: boolean;
  onUseModel?: (file: File, sourceId: string, activityKey?: string) => void;
  onOpenPlayout?: (file: File) => void;
}

// A small wizard: pick a miner family, then a variant, then the mined model is
// shown. The breadcrumb steps back to any earlier choice.
export function ProcessModel({
  data,
  treeLayout,
  petriLayout,
  inductivePetriLayout,
  heuristicPetriLayout,
  powlPetriLayout,
  canExportPnml,
  mining = false,
  onUseModel,
  onOpenPlayout,
}: ProcessModelProps) {
  const [step, setStep] = useState<Step>("miner");
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
    <div className="miner-flow reveal">
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
          treeLayout={treeLayout}
          petriLayout={petriLayout}
          inductivePetriLayout={inductivePetriLayout}
          heuristicPetriLayout={heuristicPetriLayout}
          powlPetriLayout={powlPetriLayout}
          canExportPnml={canExportPnml}
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
  treeLayout: TreeLayout | null;
  petriLayout: PetriLayout | null;
  inductivePetriLayout: PetriLayout | null;
  heuristicPetriLayout: PetriLayout | null;
  powlPetriLayout: PetriLayout | null;
  canExportPnml: boolean;
  mining: boolean;
  onUseModel?: (file: File, sourceId: string, activityKey?: string) => void;
  onOpenPlayout?: (file: File) => void;
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
  treeLayout,
  petriLayout,
  inductivePetriLayout,
  heuristicPetriLayout,
  powlPetriLayout,
  canExportPnml,
  mining,
  onUseModel,
  onOpenPlayout,
}: ModelResultProps) {
  const [asNet, setAsNet] = useState(false);

  if (minerId === "ilp" || minerId === "genetic")
    return <NativeDiscovery family={minerId} data={data} available={canExportPnml} mining={mining} onUseModel={onUseModel} />;
  if (minerId === "alpha" || minerId === "heuristic")
    return <WorkflowDiscovery family={minerId} variant={variant.id as "classic"|"plus"|"plusplus"} data={data} available={canExportPnml} mining={mining} onUseModel={onUseModel} onOpenPlayout={onOpenPlayout} />;

  // Every miner exports both ways: PNML from its workflow net, BPMN from the
  // inductive tree or from the net's token semantics (converted server-side),
  // and PTML when the selected workflow net is exactly block structured.
  const actions = canExportPnml ? (
    <>
      <PnmlButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <BpmnButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <PtmlButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <PlayoutButton minerId={minerId} dataset={data.dataset} />
    </>
  ) : null;

  // The primary-view ⇄ workflow-net flip, offered when the export carries the
  // net (older exports lack it and simply show no toggle).
  const netToggle = (primaryLabel: string, netLayout: PetriLayout | null) =>
    netLayout ? (
      <div className="map-mode" role="group" aria-label="Model view">
        <button
          className={asNet ? "mode-btn" : "mode-btn active"}
          onClick={() => setAsNet(false)}
        >
          {primaryLabel}
        </button>
        <button
          className={asNet ? "mode-btn active" : "mode-btn"}
          onClick={() => setAsNet(true)}
        >
          Workflow net
        </button>
      </div>
    ) : null;

  if (variant.artifact === "process-tree") {
    if (asNet && inductivePetriLayout) {
      return (
        <PetriNet
          layout={inductivePetriLayout}
          title={groupName}
          actions={
            <>
              {netToggle("Tree", inductivePetriLayout)}
              {actions}
            </>
          }
          decoration={data.inductive_petri_net_decoration ?? null}
        />
      );
    }
    return treeLayout ? (
      <ProcessTree
        layout={treeLayout}
        title={groupName}
        actions={
          <>
            {netToggle("Tree", inductivePetriLayout)}
            {actions}
          </>
        }
      />
    ) : (
      <Placeholder>
        No model to display. This export predates model discovery — re-open the
        CSV event log to mine a process tree.
      </Placeholder>
    );
  }

  if (variant.artifact === "dependency-graph") {
    if (asNet && heuristicPetriLayout) {
      return (
        <PetriNet
          layout={heuristicPetriLayout}
          title={groupName}
          actions={
            <>
              {netToggle("Dependency graph", heuristicPetriLayout)}
              {actions}
            </>
          }
          decoration={data.heuristic_petri_net_decoration ?? null}
        />
      );
    }
    // The dependency graph is derived from the directly-follows counts, which
    // every analysis carries, so it is always available.
    return (
      <DependencyGraph
        data={data}
        title={groupName}
        actions={
          <>
            {netToggle("Dependency graph", heuristicPetriLayout)}
            {actions}
          </>
        }
      />
    );
  }

  if (variant.artifact === "powl") {
    if (asNet && powlPetriLayout) {
      return (
        <PetriNet
          layout={powlPetriLayout}
          title={groupName}
          actions={
            <>
              {netToggle("POWL", powlPetriLayout)}
              {actions}
            </>
          }
          decoration={data.powl_petri_net_decoration ?? null}
        />
      );
    }
    return data.powl ? (
      <PowlModel
        model={data.powl}
        data={data}
        title={groupName}
        actions={
          <>
            {netToggle("POWL", powlPetriLayout)}
            {actions}
          </>
        }
      />
    ) : (
      <Placeholder>
        No POWL model is available in this export — re-open the CSV event log to discover one.
      </Placeholder>
    );
  }

  if (variant.artifact === "log-skeleton") {
    return data.log_skeleton ? (
      <LogSkeletonModel
        model={data.log_skeleton}
        conformance={data.log_skeleton_conformance}
        data={data}
        title={groupName}
      />
    ) : (
      <Placeholder>
        No log-skeleton model is available in this export — re-open the CSV event
        log to discover one.
      </Placeholder>
    );
  }

  if (variant.artifact === "declare") {
    return data.declare ? (
      <DeclareModel
        model={data.declare}
        conformance={data.declare_conformance}
        data={data}
        title={groupName}
        actions={
          canExportPnml ? (
            <PlayoutButton minerId="declare" dataset={data.dataset} />
          ) : null
        }
      />
    ) : (
      <Placeholder>
        No Declare model is available in this export — re-open the CSV event log to discover one.
      </Placeholder>
    );
  }

  if (variant.artifact === "transition-system") {
    return data.transition_system ? <TransitionSystemModel model={data.transition_system} data={data} /> : <Placeholder>No transition system is included in this export. Open a CSV or XES log to discover one.</Placeholder>;
  }

  if (variant.artifact === "footprints") {
    return data.footprints ? (
      <FootprintsModel data={data} footprints={data.footprints} title={groupName} />
    ) : (
      <Placeholder>
        No aggregate footprints are available in this export — re-open the CSV event log to discover them.
      </Placeholder>
    );
  }

  return petriLayout ? (
    <PetriNet
      layout={petriLayout}
      title={groupName}
      actions={actions}
      decoration={data.petri_net_decoration ?? null}
    />
  ) : (
    <Placeholder>
      No model to display. This export predates {groupName} discovery — re-open
      the CSV event log to mine a Petri net.
    </Placeholder>
  );
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
        const url = URL.createObjectURL(blob);
        const anchor = document.createElement("a");
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        anchor.href = url;
        anchor.download = `${base}-${minerId}.pnml`;
        anchor.click();
        URL.revokeObjectURL(url);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : `Download ${groupName} as PNML`}
      </button>
      {error && <span className="pnml-error">{error}</span>}
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
        const url = URL.createObjectURL(blob);
        const anchor = document.createElement("a");
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        anchor.href = url;
        anchor.download = `${base}-${minerId}.bpmn`;
        anchor.click();
        URL.revokeObjectURL(url);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : `Download ${groupName} as BPMN`}
      </button>
      {error && <span className="pnml-error">{error}</span>}
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
        const url = URL.createObjectURL(blob);
        const anchor = document.createElement("a");
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        anchor.href = url;
        anchor.download = `${base}-${minerId}.ptml`;
        anchor.click();
        URL.revokeObjectURL(url);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : `Download ${groupName} as PTML`}
      </button>
      {error && <span className="pnml-error">{error}</span>}
    </span>
  );
}

// Simulates the shown model into an XES log using the backend's model-specific
// defaults. Declare traces are prefix-safe and may retain pending obligations.
function PlayoutButton({
  minerId,
  dataset,
}: {
  minerId: string;
  dataset: string;
}) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchPlayout(minerId)
      .then((xml) => {
        const blob = new Blob([xml], { type: "application/xml" });
        const url = URL.createObjectURL(blob);
        const anchor = document.createElement("a");
        const base = dataset.replace(/\.[^.]+$/, "") || "model";
        anchor.href = url;
        anchor.download = `${base}-${minerId}-playout.xes`;
        anchor.click();
        URL.revokeObjectURL(url);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Simulating…" : "Simulate log (XES)"}
      </button>
      {error && <span className="pnml-error">{error}</span>}
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
