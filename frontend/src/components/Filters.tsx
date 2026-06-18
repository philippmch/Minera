import { useState } from "react";
import { type FilterOptions } from "../api";

interface FiltersProps {
  /** Activity names from the original, unfiltered log. */
  activities: string[];
  /** Whether any filter is currently applied (drives the "active" badge). */
  active: boolean;
  mining: boolean;
  onApply: (filters: FilterOptions) => void;
}

// A compact, collapsible filter bar. Every apply re-mines the original upload
// with the full filter spec, so filters never compound and can always be
// widened again.
export function Filters({ activities, active, mining, onApply }: FiltersProps) {
  const [open, setOpen] = useState(false);
  const [topVariants, setTopVariants] = useState("");
  const [minFrequency, setMinFrequency] = useState("");
  // Activities start all-selected; deselected ones are removed from the log.
  const [removed, setRemoved] = useState<Set<string>>(new Set());

  const toggleActivity = (name: string) => {
    setRemoved((prev) => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };

  const toInt = (value: string) => {
    const n = Number.parseInt(value, 10);
    return Number.isFinite(n) && n > 0 ? n : undefined;
  };

  const apply = () => {
    const removeList = activities.filter((a) => removed.has(a));
    onApply({
      topVariants: toInt(topVariants),
      minVariantFrequency: toInt(minFrequency),
      removeActivities: removeList.length > 0 ? removeList : undefined,
    });
  };

  const reset = () => {
    setTopVariants("");
    setMinFrequency("");
    setRemoved(new Set());
    onApply({});
  };

  const dirty =
    topVariants !== "" || minFrequency !== "" || removed.size > 0;

  return (
    <section className="filters reveal">
      <div className="filters-bar">
        <button
          className={open ? "filters-toggle open" : "filters-toggle"}
          onClick={() => setOpen((v) => !v)}
        >
          <span className="filters-caret">{open ? "▾" : "▸"}</span> Filter log
          {active && <span className="filters-dot" aria-label="filters applied" />}
        </button>
        {mining && <span className="filters-status">re-mining…</span>}
      </div>

      {open && (
        <div className="filters-body">
          <div className="filters-row">
            <label className="filters-field">
              <span>Top variants</span>
              <input
                type="number"
                min={1}
                placeholder="all"
                value={topVariants}
                onChange={(e) => setTopVariants(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Min variant frequency</span>
              <input
                type="number"
                min={2}
                placeholder="none"
                value={minFrequency}
                onChange={(e) => setMinFrequency(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-activities">
            <span className="filters-field-label">
              Activities <small>(click to drop)</small>
            </span>
            <div className="filters-chips">
              {activities.map((name) => {
                const dropped = removed.has(name);
                return (
                  <button
                    key={name}
                    className={dropped ? "filters-chip dropped" : "filters-chip"}
                    onClick={() => toggleActivity(name)}
                    title={dropped ? `Add ${name} back` : `Drop ${name}`}
                  >
                    {name}
                  </button>
                );
              })}
            </div>
          </div>

          <div className="filters-actions">
            <button className="filters-apply" onClick={apply} disabled={mining}>
              Apply
            </button>
            <button
              className="filters-reset"
              onClick={reset}
              disabled={mining || !dirty}
            >
              Reset
            </button>
          </div>
        </div>
      )}
    </section>
  );
}
