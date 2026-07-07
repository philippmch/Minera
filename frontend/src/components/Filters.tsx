import { useState } from "react";
import {
  type AttributeFilterMode,
  type FilterOptions,
  type MinerOptions,
  type TimeFilterMode,
} from "../api";
import { type CaseAttributeStat, type EventAttributeStat } from "../types";

interface FiltersProps {
  /** Activity names from the original, unfiltered log. */
  activities: string[];
  /** Resource names from the original log; empty when the log has no resources. */
  resources: string[];
  /** Generic event attributes of the original log; empty when it has none. */
  attributes: EventAttributeStat[];
  /** Case attributes of the original log; empty when it has none. */
  caseAttributes: CaseAttributeStat[];
  /** Whether any filter or miner setting is applied (drives the badge). */
  active: boolean;
  mining: boolean;
  onApply: (options: FilterOptions & MinerOptions) => void;
}

// A compact, collapsible bar for the log filters and the miner settings.
// Every apply re-mines the original upload with the full spec, so nothing
// compounds and every setting can always be widened again.
export function Filters({
  activities,
  resources,
  attributes,
  caseAttributes,
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
  const [reworkActivity, setReworkActivity] = useState("");
  const [reworkMin, setReworkMin] = useState("");
  const [betweenStart, setBetweenStart] = useState("");
  const [betweenEnd, setBetweenEnd] = useState("");
  // Activities and resources start all-selected; deselected ones are removed.
  const [removed, setRemoved] = useState<Set<string>>(new Set());
  const [removedResources, setRemovedResources] = useState<Set<string>>(
    new Set(),
  );
  // Attribute filter: one key, its dropped values, and whether dropping acts
  // on events or whole cases. Values start all-selected, like the chips above.
  const [attributeKey, setAttributeKey] = useState("");
  const [removedValues, setRemovedValues] = useState<Set<string>>(new Set());
  const [attributeMode, setAttributeMode] =
    useState<AttributeFilterMode>("events");
  // Case-attribute filter: one key and its dropped values. Dropping a value
  // always removes the whole case — the attribute lives on the case itself.
  const [caseAttributeKey, setCaseAttributeKey] = useState("");
  const [removedCaseValues, setRemovedCaseValues] = useState<Set<string>>(
    new Set(),
  );
  // Miner settings; an empty field leaves the backend default in charge.
  const [noiseThreshold, setNoiseThreshold] = useState("");
  const [inductiveVariant, setInductiveVariant] = useState<"log" | "dfg">("log");
  const [alphaVariant, setAlphaVariant] = useState<"plus" | "classic">("plus");
  const [dependencyThreshold, setDependencyThreshold] = useState("");
  const [andThreshold, setAndThreshold] = useState("");
  const [minEdgeFrequency, setMinEdgeFrequency] = useState("");

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
  const toggleValue = toggle(setRemovedValues);
  const toggleCaseValue = toggle(setRemovedCaseValues);

  const pickAttributeKey = (key: string) => {
    setAttributeKey(key);
    setRemovedValues(new Set());
  };
  const selectedAttribute = attributes.find((a) => a.key === attributeKey);

  const pickCaseAttributeKey = (key: string) => {
    setCaseAttributeKey(key);
    setRemovedCaseValues(new Set());
  };
  const selectedCaseAttribute = caseAttributes.find(
    (a) => a.key === caseAttributeKey,
  );

  const toInt = (value: string) => {
    const n = Number.parseInt(value, 10);
    return Number.isFinite(n) && n > 0 ? n : undefined;
  };
  const toFloat = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) ? n : undefined;
  };

  const apply = () => {
    const removeList = activities.filter((a) => removed.has(a));
    const removeResourceList = resources.filter((r) => removedResources.has(r));
    // Only an actually dropped value activates the attribute filter; a picked
    // key alone is just browsing its values and must not change the mine.
    const removeValueList =
      selectedAttribute?.values
        .map((v) => v.value)
        .filter((v) => removedValues.has(v)) ?? [];
    const removeCaseValueList =
      selectedCaseAttribute?.values
        .map((v) => v.value)
        .filter((v) => removedCaseValues.has(v)) ?? [];
    onApply({
      topVariants: toInt(topVariants),
      minVariantFrequency: toInt(minFrequency),
      removeActivities: removeList.length > 0 ? removeList : undefined,
      removeResources:
        removeResourceList.length > 0 ? removeResourceList : undefined,
      timeFrom: timeFrom || undefined,
      timeTo: timeTo || undefined,
      timeMode: timeFrom || timeTo ? timeMode : undefined,
      reworkActivity: reworkActivity || undefined,
      reworkMin: reworkActivity ? toInt(reworkMin) : undefined,
      betweenStart: betweenStart && betweenEnd ? betweenStart : undefined,
      betweenEnd: betweenStart && betweenEnd ? betweenEnd : undefined,
      attributeKey: removeValueList.length > 0 ? attributeKey : undefined,
      removeAttributeValues:
        removeValueList.length > 0 ? removeValueList : undefined,
      attributeMode:
        removeValueList.length > 0 && attributeMode !== "events"
          ? attributeMode
          : undefined,
      caseAttributeKey:
        removeCaseValueList.length > 0 ? caseAttributeKey : undefined,
      removeCaseAttributeValues:
        removeCaseValueList.length > 0 ? removeCaseValueList : undefined,
      noiseThreshold: toFloat(noiseThreshold),
      inductiveVariant: inductiveVariant !== "log" ? inductiveVariant : undefined,
      alphaVariant: alphaVariant !== "plus" ? alphaVariant : undefined,
      dependencyThreshold: toFloat(dependencyThreshold),
      andThreshold: toFloat(andThreshold),
      minEdgeFrequency: toInt(minEdgeFrequency),
    });
  };

  const reset = () => {
    setTopVariants("");
    setMinFrequency("");
    setTimeFrom("");
    setTimeTo("");
    setTimeMode("events");
    setReworkActivity("");
    setReworkMin("");
    setBetweenStart("");
    setBetweenEnd("");
    setRemoved(new Set());
    setRemovedResources(new Set());
    setAttributeKey("");
    setRemovedValues(new Set());
    setAttributeMode("events");
    setCaseAttributeKey("");
    setRemovedCaseValues(new Set());
    setNoiseThreshold("");
    setInductiveVariant("log");
    setAlphaVariant("plus");
    setDependencyThreshold("");
    setAndThreshold("");
    setMinEdgeFrequency("");
    onApply({});
  };

  const dirty =
    topVariants !== "" ||
    minFrequency !== "" ||
    timeFrom !== "" ||
    timeTo !== "" ||
    reworkActivity !== "" ||
    betweenStart !== "" ||
    betweenEnd !== "" ||
    removed.size > 0 ||
    removedResources.size > 0 ||
    attributeKey !== "" ||
    caseAttributeKey !== "" ||
    noiseThreshold !== "" ||
    inductiveVariant !== "log" ||
    alphaVariant !== "plus" ||
    dependencyThreshold !== "" ||
    andThreshold !== "" ||
    minEdgeFrequency !== "";

  return (
    <section className="filters reveal">
      <div className="filters-bar">
        <button
          className={open ? "filters-toggle open" : "filters-toggle"}
          onClick={() => setOpen((v) => !v)}
        >
          <span className="filters-caret">{open ? "▾" : "▸"}</span> Filters &
          miner settings
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

          <div className="filters-row">
            <label className="filters-field">
              <span>Rework: activity repeats</span>
              <select
                value={reworkActivity}
                onChange={(e) => setReworkActivity(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>at least</span>
              <input
                type="number"
                min={2}
                placeholder="2"
                value={reworkMin}
                disabled={reworkActivity === ""}
                onChange={(e) => setReworkMin(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Cut cases from</span>
              <select
                value={betweenStart}
                onChange={(e) => setBetweenStart(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>to</span>
              <select
                value={betweenEnd}
                onChange={(e) => setBetweenEnd(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
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

          {attributes.length > 0 && (
            <>
              <div className="filters-row">
                <label className="filters-field">
                  <span>Attribute</span>
                  <select
                    value={attributeKey}
                    onChange={(e) => pickAttributeKey(e.target.value)}
                  >
                    <option value="">off</option>
                    {attributes.map((attribute) => (
                      <option key={attribute.key} value={attribute.key}>
                        {attribute.key}
                      </option>
                    ))}
                  </select>
                </label>
                {selectedAttribute && (
                  <label className="filters-field">
                    <span>Dropping a value removes</span>
                    <select
                      value={attributeMode}
                      onChange={(e) =>
                        setAttributeMode(e.target.value as AttributeFilterMode)
                      }
                    >
                      <option value="events">the matching events</option>
                      <option value="traces">whole cases with a match</option>
                    </select>
                  </label>
                )}
              </div>
              {selectedAttribute && (
                <div className="filters-activities">
                  <span className="filters-field-label">
                    {selectedAttribute.key} values{" "}
                    <small>
                      (click to drop
                      {selectedAttribute.values.length <
                        selectedAttribute.distinct_values &&
                        `; top ${selectedAttribute.values.length} of ${selectedAttribute.distinct_values} shown`}
                      )
                    </small>
                  </span>
                  <div className="filters-chips">
                    {selectedAttribute.values.map(({ value, count }) => {
                      const dropped = removedValues.has(value);
                      return (
                        <button
                          key={value}
                          className={
                            dropped ? "filters-chip dropped" : "filters-chip"
                          }
                          onClick={() => toggleValue(value)}
                          title={
                            dropped
                              ? `Add ${value} back`
                              : `Drop ${value} (${count} events)`
                          }
                        >
                          {value}
                        </button>
                      );
                    })}
                  </div>
                </div>
              )}
            </>
          )}

          {caseAttributes.length > 0 && (
            <>
              <div className="filters-row">
                <label className="filters-field">
                  <span>Case attribute</span>
                  <select
                    value={caseAttributeKey}
                    onChange={(e) => pickCaseAttributeKey(e.target.value)}
                  >
                    <option value="">off</option>
                    {caseAttributes.map((attribute) => (
                      <option key={attribute.key} value={attribute.key}>
                        {attribute.key}
                      </option>
                    ))}
                  </select>
                </label>
              </div>
              {selectedCaseAttribute && (
                <div className="filters-activities">
                  <span className="filters-field-label">
                    {selectedCaseAttribute.key} values{" "}
                    <small>
                      (click to drop the cases
                      {selectedCaseAttribute.values.length <
                        selectedCaseAttribute.distinct_values &&
                        `; top ${selectedCaseAttribute.values.length} of ${selectedCaseAttribute.distinct_values} shown`}
                      )
                    </small>
                  </span>
                  <div className="filters-chips">
                    {selectedCaseAttribute.values.map(({ value, count }) => {
                      const dropped = removedCaseValues.has(value);
                      return (
                        <button
                          key={value}
                          className={
                            dropped ? "filters-chip dropped" : "filters-chip"
                          }
                          onClick={() => toggleCaseValue(value)}
                          title={
                            dropped
                              ? `Add ${value} back`
                              : `Drop ${value} (${count} cases)`
                          }
                        >
                          {value}
                        </button>
                      );
                    })}
                  </div>
                </div>
              )}
            </>
          )}

          <div className="filters-activities">
            <span className="filters-field-label">
              Miner settings <small>(empty = default)</small>
            </span>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Inductive: noise threshold</span>
              <input
                type="number"
                min={0}
                max={1}
                step={0.05}
                placeholder="0 = exact"
                value={noiseThreshold}
                onChange={(e) => setNoiseThreshold(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Inductive: recursion</span>
              <select
                value={inductiveVariant}
                onChange={(e) =>
                  setInductiveVariant(e.target.value as "log" | "dfg")
                }
              >
                <option value="log">split the log (default)</option>
                <option value="dfg">graph only (IMd, huge logs)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Alpha variant</span>
              <select
                value={alphaVariant}
                onChange={(e) =>
                  setAlphaVariant(e.target.value as "plus" | "classic")
                }
              >
                <option value="plus">Alpha+ (short loops, default)</option>
                <option value="classic">classic (textbook)</option>
              </select>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Heuristic: dependency threshold</span>
              <input
                type="number"
                min={-1}
                max={1}
                step={0.05}
                placeholder="0.9"
                value={dependencyThreshold}
                onChange={(e) => setDependencyThreshold(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: AND threshold</span>
              <input
                type="number"
                min={0}
                step={0.05}
                placeholder="0.65"
                value={andThreshold}
                onChange={(e) => setAndThreshold(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: min edge frequency</span>
              <input
                type="number"
                min={1}
                placeholder="1"
                value={minEdgeFrequency}
                onChange={(e) => setMinEdgeFrequency(e.target.value)}
              />
            </label>
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
