import type { ReactNode } from "react";
import {
  DECLARE_TEMPLATES,
  type DeclareConformanceTrace,
  type DeclareModel as DeclareData,
  type MineraExport,
} from "../types";

interface DeclareModelProps {
  model: DeclareData;
  conformance?: DeclareConformanceTrace[];
  data: MineraExport;
  title?: string;
  actions?: ReactNode;
}

const labels: Record<string, string> = {
  existence: "Existence", exactly_one: "Exactly one", init: "Init",
  responded_existence: "Responded existence", response: "Response", precedence: "Precedence",
  succession: "Succession", altresponse: "Alternate response", altprecedence: "Alternate precedence",
  altsuccession: "Alternate succession", chainresponse: "Chain response", chainprecedence: "Chain precedence",
  chainsuccession: "Chain succession", absence: "Absence", coexistence: "Coexistence",
  noncoexistence: "Non-coexistence", nonsuccession: "Non-succession", nonchainsuccession: "Non-chain succession",
};

function activityName(data: MineraExport, id: unknown): string {
  if (typeof id !== "number" || !Number.isInteger(id) || id < 0 || id >= data.activities.length)
    return `unknown (${String(id)})`;
  return data.activities[id]?.name ?? `unknown (${id})`;
}

export function DeclareModel({ model, conformance, data, title = "Declare", actions }: DeclareModelProps) {
  const families = DECLARE_TEMPLATES.filter((template) => Array.isArray(model[template]));
  return (
    <section className="model-card declare-model reveal">
      <div className="model-card-header">
        <div><p className="eyebrow">Declarative discovery</p><h2>{title}</h2></div>
        {actions && <div className="model-actions">{actions}</div>}
      </div>
      <p className="model-caption">{families.reduce((total, family) => total + (model[family]?.length ?? 0), 0)} rules across {families.length} template families.</p>
      <div className="declare-families">
        {families.map((family) => {
          const rules = model[family] ?? [];
          return (
            <section className="declare-family" key={family}>
              <h3>{labels[family] ?? family} <span className="declare-count">{rules.length}</span></h3>
              {rules.length === 0 ? <p className="muted">No rules discovered.</p> : (
                <ul className="declare-rules">
                  {rules.map((rule, index) => {
                    const binary = "source" in rule && "target" in rule;
                    return <li key={`${family}-${index}`}>
                      <span className="declare-rule-activities">
                        {binary ? `${activityName(data, rule.source)} → ${activityName(data, rule.target)}` : activityName(data, rule.activity)}
                      </span>
                      <span className="declare-rule-stats">support {rule.support} · confidence {rule.confidence}</span>
                    </li>;
                  })}
                </ul>
              )}
            </section>
          );
        })}
      </div>
      {conformance && conformance.length > 0 && (
        <section className="declare-conformance">
          <h3>Per-trace conformance</h3>
          <div className="declare-diagnostics">
            {conformance.map((trace, index) => (
              <article className={trace.is_fit ? "declare-trace fit" : "declare-trace unfit"} key={trace.trace_index ?? index}>
                <header><strong>{trace.case_id || `trace ${trace.trace_index ?? index}`}</strong><span>{trace.is_fit ? "fit" : "unfit"} · fitness {trace.dev_fitness.toFixed(3)}</span></header>
                {trace.deviations.length === 0 ? <p className="muted">No deviations.</p> : <ul>
                  {trace.deviations.map((deviation, deviationIndex) => {
                    const binary = "source" in deviation && "target" in deviation;
                    return <li key={`${trace.trace_index ?? index}-${deviationIndex}`}>
                      {deviation.template}: {binary ? `${activityName(data, deviation.source)} → ${activityName(data, deviation.target)}` : activityName(data, deviation.activity)}
                    </li>;
                  })}
                </ul>}
                <small>{trace.no_dev_total} deviations / {trace.no_constr_total} constraints</small>
              </article>
            ))}
          </div>
        </section>
      )}
    </section>
  );
}
