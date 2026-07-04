import { useState } from "react";
import { type FilterOptions, type TimeFilterMode } from "../api";

interface FiltersProps {
  /** Activity names from the original, unfiltered log. */
  activities: string[];
  /** Resource names from the original log; empty when the log has no resources. */
  resources: string[];
  /** Whether any filter is currently applied (drives the "active" badge). */
  active: boolean;
  mining: boolean;
  onApply: (filters: FilterOptions) => void;
}

// A compact, collapsible filter bar. Every apply re-mines the original upload
// with the full filter spec, so filters never compound and can always be
// widened again.
export function Filters({
  activities,
  resources,
  active,
  mining,
  onApply,
}: FiltersProps) {
  const [open, setOpen] = useState(false);
  const [topVariants, setTopVariants] = useState("");
  const [minFrequency, setMinFrequency] = useState("");
  const [timeFrom, setTimeFrom] = useState("");
  const [timeTo, setTimeTo] = useState("");
  const [timeMode, setTimeMode] = useState<TimeFilterMode>("events");
  // Activities and resources start all-selected; deselected ones are removed.
  const [removed, setRemoved] = useState<Set<string>>(new Set());
  const [removedResources, setRemovedResources] = useState<Set<string>>(
    new Set(),
  );

  const toggle = (setter: typeof setRemoved) => (name: string) => {
    setter((prev) => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };
  const toggleActivity = toggle(setRemoved);
  const toggleResource = toggle(setRemovedResources);

  const toInt = (value: string) => {
    const n = Number.parseInt(value, 10);
    return Number.isFinite(n) && n > 0 ? n : undefined;
  };

  const apply = () => {
    const removeList = activities.filter((a) => removed.has(a));
    const removeResourceList = resources.filter((r) => removedResources.has(r));
    onApply({
      topVariants: toInt(topVariants),
      minVariantFrequency: toInt(minFrequency),
      removeActivities: removeList.length > 0 ? removeList : undefined,
      removeResources:
        removeResourceList.length > 0 ? removeResourceList : undefined,
      timeFrom: timeFrom || undefined,
      timeTo: timeTo || undefined,
      timeMode: timeFrom || timeTo ? timeMode : undefined,
    });
  };

  const reset = () => {
    setTopVariants("");
    setMinFrequency("");
    setTimeFrom("");
    setTimeTo("");
    setTimeMode("events");
    setRemoved(new Set());
    setRemovedResources(new Set());
    onApply({});
  };

  const dirty =
    topVariants !== "" ||
    minFrequency !== "" ||
    timeFrom !== "" ||
    timeTo !== "" ||
    removed.size > 0 ||
    removedResources.size > 0;

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

          <div className="filters-row">
            <label className="filters-field">
              <span>From</span>
              <input
                type="datetime-local"
                value={timeFrom}
                onChange={(e) => setTimeFrom(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>To</span>
              <input
                type="datetime-local"
                value={timeTo}
                onChange={(e) => setTimeTo(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Time window keeps</span>
              <select
                value={timeMode}
                onChange={(e) => setTimeMode(e.target.value as TimeFilterMode)}
              >
                <option value="events">events in the window</option>
                <option value="traces_contained">cases fully inside</option>
                <option value="traces_intersecting">cases touching it</option>
              </select>
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

          {resources.length > 0 && (
            <div className="filters-activities">
              <span className="filters-field-label">
                Resources <small>(click to drop)</small>
              </span>
              <div className="filters-chips">
                {resources.map((name) => {
                  const dropped = removedResources.has(name);
                  return (
                    <button
                      key={name}
                      className={dropped ? "filters-chip dropped" : "filters-chip"}
                      onClick={() => toggleResource(name)}
                      title={dropped ? `Add ${name} back` : `Drop ${name}`}
                    >
                      {name}
                    </button>
                  );
                })}
              </div>
            </div>
          )}

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
