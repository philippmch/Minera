import { Fragment } from "react";
import { type MineraExport, formatDuration, numberFormat } from "../types";
import { cumulativeCoverage } from "../variant-insights";

interface VariantListProps {
  data: MineraExport;
  selected: number | null;
  onSelect: (index: number | null) => void;
}

export function VariantList({ data, selected, onSelect }: VariantListProps) {
  const maxCount = Math.max(...data.variants.map((v) => v.count));
  const coverage = cumulativeCoverage(data.variants, data.trace_count);

  return (
    <aside className="panel variants-panel reveal">
      <div className="panel-head">
        <h2>Variants</h2>
        <span className="panel-meta">
          {numberFormat.format(data.variants.length)} distinct paths
        </span>
      </div>
      <ol className="variant-list">
        {data.variants.map((variant, index) => {
          const share = variant.count / data.trace_count;
          return (
            <li
              className={selected === index ? "variant active" : "variant"}
              key={index}
              onClick={() => onSelect(selected === index ? null : index)}
            >
              <div className="variant-top">
                <span className="variant-rank">
                  {String(index + 1).padStart(2, "0")}
                </span>
                <span className="variant-count">
                  {numberFormat.format(variant.count)}{" "}
                  <small>{(share * 100).toFixed(1)}%</small>
                </span>
                {variant.duration && variant.duration.count > 0 && (
                  <span className="variant-duration" title="median throughput time">
                    {formatDuration(variant.duration.median)}
                  </span>
                )}
                <span className="variant-cumulative" title="cumulative share of cases">
                  Σ {(coverage[index] * 100).toFixed(0)}%
                </span>
                <span className="variant-share">
                  <i style={{ width: `${((variant.count / maxCount) * 100).toFixed(1)}%` }} />
                </span>
              </div>
              <div className="variant-steps">
                {variant.activities.map((activityId, i) => {
                  const name = data.activities[activityId].name;
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
    </aside>
  );
}
