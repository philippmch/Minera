import { type ReactNode, useState } from "react";
import { type TreeLayout } from "../tree-layout";
import { type PetriLayout } from "../petri-layout";
import { type MineraExport } from "../types";
import { MINER_GROUPS, type MinerVariant } from "../miners";
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
}

// A small wizard: pick a miner family, then a variant, then the mined model is
// shown. The breadcrumb steps back to any earlier choice.
export function ProcessModel({
  data,
  treeLayout,
  petriLayout,
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
          groupName={group.name}
          data={data}
          treeLayout={treeLayout}
          petriLayout={petriLayout}
        />
      )}
    </div>
  );
}

interface ModelResultProps {
  variant: MinerVariant;
  groupName: string;
  data: MineraExport;
  treeLayout: TreeLayout | null;
  petriLayout: PetriLayout | null;
}

// Renders the mined model for the chosen variant by its artifact type. A null
// layout means the export carries no model of that kind (an older export, or a
// log too small to mine one), so an honest placeholder is shown instead.
function ModelResult({
  variant,
  groupName,
  data,
  treeLayout,
  petriLayout,
}: ModelResultProps) {
  if (variant.artifact === "process-tree") {
    return treeLayout ? (
      <ProcessTree layout={treeLayout} title={groupName} />
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
    return <DependencyGraph data={data} title={groupName} />;
  }

  return petriLayout ? (
    <PetriNet layout={petriLayout} title={groupName} />
  ) : (
    <Placeholder>
      No model to display. This export predates {groupName} discovery — re-open
      the CSV event log to mine a Petri net.
    </Placeholder>
  );
}

function Placeholder({ children }: { children: ReactNode }) {
  return (
    <section className="panel map-panel">
      <div className="model-empty">{children}</div>
    </section>
  );
}
