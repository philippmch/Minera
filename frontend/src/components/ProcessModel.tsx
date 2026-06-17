import { useState } from "react";
import { type TreeLayout } from "../tree-layout";
import { MINER_GROUPS } from "../miners";
import { ProcessTree } from "./ProcessTree";

type Step = "algorithm" | "variant" | "result";

interface ProcessModelProps {
  /** Layout for the inductive miner's process tree, if the export carries one. */
  treeLayout: TreeLayout | null;
}

// A small wizard: pick an algorithm family, then a variant, then the mined
// model is shown. The breadcrumb steps back to any earlier choice.
export function ProcessModel({ treeLayout }: ProcessModelProps) {
  const [step, setStep] = useState<Step>("algorithm");
  const [groupId, setGroupId] = useState<string | null>(null);
  const [variantId, setVariantId] = useState<string | null>(null);

  const group = MINER_GROUPS.find((g) => g.id === groupId) ?? null;
  const variant = group?.variants.find((v) => v.id === variantId) ?? null;

  const chooseAlgorithm = (id: string) => {
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
          className={step === "algorithm" ? "trail-item active" : "trail-item"}
          onClick={() => setStep("algorithm")}
        >
          Algorithm
        </button>
        {group && step !== "algorithm" && (
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

      {step === "algorithm" && (
        <div className="miner-step">
          <p className="miner-step-label">Step 1 — Discovery algorithm</p>
          <h2 className="miner-step-title">Choose an algorithm</h2>
          <div className="miner-cards">
            {MINER_GROUPS.map((g) => (
              <button
                key={g.id}
                className="miner-card"
                onClick={() => chooseAlgorithm(g.id)}
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

      {step === "result" &&
        variant &&
        (variant.artifact === "process-tree" && treeLayout ? (
          <ProcessTree layout={treeLayout} />
        ) : (
          <section className="panel map-panel">
            <div className="model-empty">
              No model to display. This export predates model discovery — re-open
              the CSV event log to mine a process tree.
            </div>
          </section>
        ))}
    </div>
  );
}
