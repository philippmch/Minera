import { type ReactNode, useState } from "react";
import { type TreeLayout } from "../tree-layout";
import { type PetriLayout } from "../petri-layout";
import { type MineraExport } from "../types";
import { MINER_GROUPS, type MinerVariant } from "../miners";
import { fetchBpmn, fetchPnml } from "../api";
import { ProcessTree } from "./ProcessTree";
import { PetriNet } from "./PetriNet";
import { DependencyGraph } from "./DependencyGraph";

type Step = "miner" | "variant" | "result";

interface ProcessModelProps {
  /** The analysis, used to mine the heuristic miner's dependency graph live. */
  data: MineraExport;
  /** Layout for the inductive miner's process tree, if the export carries one. */
  treeLayout: TreeLayout | null;
  /** Layout for the alpha miner's Petri net, if the export carries one. */
  petriLayout: PetriLayout | null;
  /** Whether the backend can export PNML (a log was mined this session). */
  canExportPnml: boolean;
}

// A small wizard: pick a miner family, then a variant, then the mined model is
// shown. The breadcrumb steps back to any earlier choice.
export function ProcessModel({
  data,
  treeLayout,
  petriLayout,
  canExportPnml,
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
          variant={variant}
          minerId={group.id}
          groupName={group.name}
          data={data}
          treeLayout={treeLayout}
          petriLayout={petriLayout}
          canExportPnml={canExportPnml}
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
  canExportPnml: boolean;
}

// Renders the mined model for the chosen variant by its artifact type. A null
// layout means the export carries no model of that kind (an older export, or a
// log too small to mine one), so an honest placeholder is shown instead.
function ModelResult({
  variant,
  minerId,
  groupName,
  data,
  treeLayout,
  petriLayout,
  canExportPnml,
}: ModelResultProps) {
  // Every miner exports both ways: PNML from its workflow net, BPMN from the
  // inductive tree or from the net's token semantics (converted server-side).
  const actions = canExportPnml ? (
    <>
      <PnmlButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
      <BpmnButton minerId={minerId} groupName={groupName} dataset={data.dataset} />
    </>
  ) : null;

  if (variant.artifact === "process-tree") {
    return treeLayout ? (
      <ProcessTree layout={treeLayout} title={groupName} actions={actions} />
    ) : (
      <Placeholder>
        No model to display. This export predates model discovery — re-open the
        CSV event log to mine a process tree.
      </Placeholder>
    );
  }

  if (variant.artifact === "dependency-graph") {
    // The dependency graph is derived from the directly-follows counts, which
    // every analysis carries, so it is always available.
    return <DependencyGraph data={data} title={groupName} actions={actions} />;
  }

  return petriLayout ? (
    <PetriNet layout={petriLayout} title={groupName} actions={actions} />
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

function Placeholder({ children }: { children: ReactNode }) {
  return (
    <section className="panel map-panel">
      <div className="model-empty">{children}</div>
    </section>
  );
}
