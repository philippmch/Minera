import { Fragment, useEffect, useRef, useState } from "react";
import {
  type Alignments as AlignmentsResult,
  type Decisions as DecisionsResult,
  type DecisionPoint,
  type MineraExport,
  type ModelConformance,
  numberFormat,
} from "../types";
import { type ImportedModelCheck } from "../api";
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
}: {
  data: MineraExport;
  onRequestAlignment: (minerId: string) => Promise<AlignmentsResult>;
  onRequestDecisions?: (minerId: string) => Promise<DecisionsResult>;
  onCheckModel?: (file: File) => Promise<ImportedModelCheck>;
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
            {imported || data.conformance?.[IMPORTED] ? (
              <button className="miner-card" onClick={() => chooseMiner(IMPORTED)}>
                <span className="miner-card-name">Imported model</span>
                <span className="miner-card-count">PNML / BPMN</span>
                <p className="miner-card-desc">
                  A model you supplied, checked against this log.
                </p>
                <span className="miner-card-go">Select →</span>
              </button>
            ) : (
              <label className="miner-card miner-card-upload">
                <span className="miner-card-name">Your model</span>
                <span className="miner-card-count">PNML / BPMN</span>
                <p className="miner-card-desc">
                  Check the log against a model from another tool — upload a
                  PNML workflow net or a BPMN 2.0 process.
                </p>
                <span className="miner-card-go">
                  {importing ? "Checking…" : "Upload →"}
                </span>
                {importError && (
                  <p className="miner-card-error">{importError}</p>
                )}
                <input
                  type="file"
                  accept=".pnml,.bpmn,.xml"
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
    </div>
  );
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
  onRequestAlignment: (minerId: string) => Promise<AlignmentsResult>;
}) {
  const embedded = model.alignments;
  const [fetched, setFetched] = useState<AlignmentsResult | null>(embedded ?? null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const requestRef = useRef(onRequestAlignment);
  requestRef.current = onRequestAlignment;

  useEffect(() => {
    if (embedded) {
      setFetched(embedded);
      return;
    }
    if (!model.sound) return;  // unsound: nothing to fetch, the notice says why
    let cancelled = false;
    setFetched(null);
    setError(null);
    setLoading(true);
    requestRef
      .current(minerId)
      .then((result) => !cancelled && setFetched(result))
      .catch((cause) =>
        !cancelled && setError(cause instanceof Error ? cause.message : String(cause)),
      )
      .finally(() => !cancelled && setLoading(false));
    return () => {
      cancelled = true;
    };
  }, [minerId, model.sound, embedded]);

  // Known up front from the mine: an unsound model can't be aligned at all.
  if (!model.sound) {
    return <AlignmentNotice minerName={minerName} exhausted={false} />;
  }
  if (error) {
    return (
      <section className="panel conf-panel">
        <div className="conf-notice">
          <span className="conf-notice-mark">⚠</span>
          <div>
            <h3>Couldn't compute alignments</h3>
            <p>{error}</p>
          </div>
        </div>
      </section>
    );
  }
  if (loading || !fetched) {
    return (
      <section className="panel conf-panel">
        <p className="conf-loading">Computing optimal alignments</p>
      </section>
    );
  }
  // Sound but the search could not finish (reachable yet unbounded/too large).
  if (!fetched.computed) {
    return <AlignmentNotice minerName={minerName} exhausted={fetched.exhausted} />;
  }
  return <AlignmentResultView data={data} alignments={fetched} />;
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
              The {minerName} model is reachable but admits{" "}
              <strong>unbounded behavior</strong>, so the alignment search hit its
              limit before finding optimal alignments — the numbers wouldn't be
              trustworthy.
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
                      const name = data.activities[move.activity]?.name ?? "?";
                      return (
                        <span
                          key={i}
                          className={`align-move ${move.type}`}
                          title={`${move.type}: ${name}`}
                        >
                          {name}
                        </span>
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
