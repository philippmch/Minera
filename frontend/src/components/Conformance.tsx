import { Fragment, useState } from "react";
import { type MineraExport, numberFormat } from "../types";
import { CONFORMANCE_METHODS } from "../conformance";
import { MINER_GROUPS } from "../miners";

type Step = "miner" | "method" | "result";

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
export function Conformance({ data }: { data: MineraExport }) {
  const [step, setStep] = useState<Step>("miner");
  const [minerId, setMinerId] = useState<string | null>(null);
  const [methodId, setMethodId] = useState<string | null>(null);

  const miner = MINER_GROUPS.find((g) => g.id === minerId) ?? null;
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

  // The backend mines the inductive-miner model at import and replays it with
  // token replay; that single combination is what data.conformance carries.
  // Any other miner/method pair is offered but has no results to render yet.
  const supported =
    miner?.id === "inductive" && method?.artifact === "token-replay";

  return (
    <div className="miner-flow reveal">
      <nav className="miner-trail">
        <button
          className={step === "miner" ? "trail-item active" : "trail-item"}
          onClick={() => setStep("miner")}
        >
          Miner
        </button>
        {miner && step !== "miner" && (
          <>
            <span className="trail-sep">›</span>
            <button
              className={step === "method" ? "trail-item active" : "trail-item"}
              onClick={() => setStep("method")}
            >
              {miner.name}
            </button>
          </>
        )}
        {miner && method && step === "result" && (
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

      {step === "method" && miner && (
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
        miner &&
        method &&
        (supported ? (
          <TokenReplay data={data} />
        ) : (
          <section className="panel map-panel">
            <div className="model-empty">
              Conformance for {miner.name} via {method.name} isn't computed yet.
              The backend currently replays the inductive-miner model with token
              replay — pick that pair to see results.
            </div>
          </section>
        ))}
    </div>
  );
}

// Token-replay results: quality gauges, the token tally, and per-variant
// deviation. Rendered when the "token-replay" method is selected.
function TokenReplay({ data }: { data: MineraExport }) {
  const conformance = data.conformance;
  if (!conformance) {
    return (
      <section className="panel map-panel">
        <div className="model-empty">
          No conformance results. This export predates conformance checking —
          re-open the CSV event log to replay it against the process model.
        </div>
      </section>
    );
  }

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
            inductive-miner process tree as a workflow net
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
