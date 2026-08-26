import { type ReactNode } from "react";
import {
  type LogSkeletonConformanceTrace,
  type LogSkeletonModel as LogSkeletonData,
  type MineraExport,
} from "../types";

const FAMILIES: Array<[keyof Omit<LogSkeletonData, "activ_freq">, string]> = [
  ["equivalence", "Equivalence"],
  ["always_after", "Always-after"],
  ["always_before", "Always-before"],
  ["never_together", "Never-together"],
  ["directly_follows", "Directly-follows"],
];

const activityName = (data: MineraExport, id: number) =>
  data.activities[id]?.name ?? `#${id}`;

const relationLabel = (data: MineraExport, source: number, target: number) =>
  `${activityName(data, source)} → ${activityName(data, target)}`;

interface LogSkeletonModelProps {
  model: LogSkeletonData;
  conformance?: LogSkeletonConformanceTrace[];
  data: MineraExport;
  title?: string;
  actions?: ReactNode;
}

/** A compact, text-first view of log-skeleton constraints and deviations. */
export function LogSkeletonModel({
  model,
  conformance,
  data,
  title = "Log Skeleton",
  actions,
}: LogSkeletonModelProps) {
  const traces = conformance ?? [];
  const fitCount = traces.filter((trace) => trace.is_fit).length;
  const meanFitness =
    traces.length > 0
      ? traces.reduce((sum, trace) => sum + (Number.isFinite(trace.dev_fitness) ? trace.dev_fitness : 0), 0) /
        traces.length
      : null;
  const deviations = traces.filter((trace) => trace.deviations?.length > 0);
  const frequencies = Object.entries(model.activ_freq ?? {}).sort(
    ([left], [right]) => Number(left) - Number(right),
  );

  return (
    <section className="panel map-panel log-skeleton-panel reveal">
      <div className="panel-head">
        <div>
          <p className="eyebrow">Constraint model</p>
          <h2>{title}</h2>
        </div>
        {actions && <div className="panel-actions">{actions}</div>}
      </div>
      <p className="model-caption">
        Relations are shown with activity names. Missing optional sections mean
        this export predates log-skeleton discovery.
      </p>
      <div className="log-skeleton-families">
        {FAMILIES.map(([key, label]) => {
          const relations = Array.isArray(model[key]) ? model[key] : [];
          return (
            <section className="log-skeleton-family" key={key}>
              <h3>{label}</h3>
              {relations.length === 0 ? (
                <p className="model-empty-inline">No relations</p>
              ) : (
                <ul>
                  {relations.map((relation, index) => (
                    <li key={`${relation.source}-${relation.target}-${index}`}>
                      {relationLabel(data, relation.source, relation.target)}
                    </li>
                  ))}
                </ul>
              )}
            </section>
          );
        })}
        <section className="log-skeleton-family">
          <h3>Activity frequency</h3>
          {frequencies.length === 0 ? (
            <p className="model-empty-inline">No frequency constraints</p>
          ) : (
            <ul>
              {frequencies.map(([id, allowed]) => (
                <li key={id}>
                  {activityName(data, Number(id))}: {allowed.length ? allowed.join(", ") : "none"}
                </li>
              ))}
            </ul>
          )}
        </section>
      </div>

      <section className="log-skeleton-conformance">
        <div className="panel-head compact">
          <h3>Conformance</h3>
          {traces.length > 0 && (
            <span className="panel-meta">
              {fitCount}/{traces.length} traces fit
              {meanFitness === null ? "" : ` · mean deviation fitness ${(meanFitness * 100).toFixed(1)}%`}
            </span>
          )}
        </div>
        {traces.length === 0 ? (
          <p className="model-empty-inline">
            No trace conformance is embedded in this export.
          </p>
        ) : deviations.length === 0 ? (
          <p className="model-empty-inline">All traces satisfy the selected constraints.</p>
        ) : (
          <div className="log-skeleton-deviations">
            {deviations.map((trace, index) => (
              <article
                className="log-skeleton-trace"
                key={`${trace.case_id}-${index}`}
              >
                <div className="log-skeleton-trace-head">
                  <strong>{trace.case_id}</strong>
                  <span>{trace.deviations.length} deviation{trace.deviations.length === 1 ? "" : "s"} · {(trace.dev_fitness * 100).toFixed(1)}%</span>
                </div>
                <ul>
                  {trace.deviations.map((deviation, index) => {
                    if ("activity" in deviation) {
                      return (
                        <li key={index}>
                          {deviation.constraint}: {activityName(data, deviation.activity)} observed {deviation.observed_frequency}; allowed {deviation.allowed_frequencies.join(", ") || "none"}
                        </li>
                      );
                    }
                    return (
                      <li key={index}>
                        {deviation.constraint}: {relationLabel(data, deviation.source, deviation.target)}
                      </li>
                    );
                  })}
                </ul>
              </article>
            ))}
          </div>
        )}
      </section>
    </section>
  );
}
