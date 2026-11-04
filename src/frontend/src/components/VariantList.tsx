import { Fragment, useMemo, useState } from "react";
import { type MineraExport, formatDuration, numberFormat } from "../types";
import { cumulativeCoverage } from "../variant-insights";

interface VariantListProps {
  data: MineraExport;
  selected: number | null;
  onSelect: (index: number | null) => void;
}

/** Variants rendered at first, and added by each "Show more". */
export const VARIANT_PAGE_SIZE = 100;

/** The largest variant count; a loop, since spreading tens of thousands of variants into Math.max overflows the stack. */
export function maxVariantCount(variants: readonly { count: number }[]): number {
  let max = 0;
  for (const variant of variants) if (variant.count > max) max = variant.count;
  return max;
}

export function VariantList({ data, selected, onSelect }: VariantListProps) {
  const maxCount = useMemo(() => maxVariantCount(data.variants), [data]);
  const coverage = useMemo(() => cumulativeCoverage(data.variants, data.trace_count), [data]);
  // A log with many distinct paths would otherwise render every one of them
  // (hundreds of thousands of nodes); the list grows a page at a time, and
  // starts over for another analysis.
  const [shown, setShown] = useState({ data, limit: VARIANT_PAGE_SIZE });
  const limit = shown.data === data ? Math.max(shown.limit, selected === null ? 0 : selected + 1) : VARIANT_PAGE_SIZE;
  const remaining = data.variants.length - limit;

  return (
    <aside className="panel variants-panel reveal">
      <div className="panel-head">
        <h2>Variants</h2>
        <span className="panel-meta">
          {numberFormat.format(data.variants.length)} distinct paths
        </span>
      </div>
      <ol className="variant-list">
        {data.variants.slice(0, limit).map((variant, index) => {
          const share = variant.count / data.trace_count;
          return (
            <li className={selected === index ? "variant active" : "variant"} key={index}>
              {/* A button, so the highlight can be toggled from the keyboard too. */}
              <button
                type="button"
                className="variant-select"
                aria-pressed={selected === index}
                onClick={() => onSelect(selected === index ? null : index)}
              >
                <span className="variant-top">
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
                  <span className="variant-share" aria-hidden="true">
                    <i style={{ width: `${((variant.count / maxCount) * 100).toFixed(1)}%` }} />
                  </span>
                </span>
                <span className="variant-steps">
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
                </span>
              </button>
            </li>
          );
        })}
      </ol>
      {remaining > 0 && (
        <button
          type="button"
          className="btn-ghost case-more"
          onClick={() => setShown({ data, limit: limit + VARIANT_PAGE_SIZE })}
        >
          Show more · {numberFormat.format(remaining)} remaining
        </button>
      )}
    </aside>
  );
}
