import { useState } from "react";
import {
  type AttributeFilterMode,
  type FilterOptions,
  type MinerOptions,
  type TimeFilterMode,
} from "../api";
import {
  DECLARE_TEMPLATES,
  FOOTPRINTS_CONFORMANCE_VARIANTS,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
  FOOTPRINTS_DISCOVERY_VARIANTS,
} from "../types";
import { type CaseAttributeStat, type EventAttributeStat } from "../types";

interface FiltersProps {
  /** Activity names from the original, unfiltered log. */
  activities: string[];
  /** Resource names from the original log; empty when the log has no resources. */
  resources: string[];
  /** Lifecycle transitions of the original log; empty for atomic events. */
  lifecycles: string[];
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
  lifecycles,
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
  const [variantCoverage, setVariantCoverage] = useState(""); // percent, 1–100
  const [prefixActivity, setPrefixActivity] = useState("");
  const [suffixActivity, setSuffixActivity] = useState("");
  // One succession constraint: keep or drop cases where `from` is directly /
  // eventually followed by `to` (the CLI accepts several pairs; the UI one).
  const [successionAction, setSuccessionAction] = useState<
    "off" | "keep" | "remove"
  >("off");
  const [successionRelation, setSuccessionRelation] = useState<
    "directly" | "eventually"
  >("directly");
  const [successionFrom, setSuccessionFrom] = useState("");
  const [successionTo, setSuccessionTo] = useState("");
  // Activities and resources start all-selected; deselected ones are removed.
  const [removed, setRemoved] = useState<Set<string>>(new Set());
  const [removedResources, setRemovedResources] = useState<Set<string>>(
    new Set(),
  );
  const [removedLifecycles, setRemovedLifecycles] = useState<Set<string>>(
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
  const [heuristicVariant, setHeuristicVariant] =
    useState<"classic" | "plusplus">("classic");
  const [dependencyThreshold, setDependencyThreshold] = useState("");
  const [andThreshold, setAndThreshold] = useState("");
  const [minEdgeFrequency, setMinEdgeFrequency] = useState("");
  const [heuristicMinActivityCount, setHeuristicMinActivityCount] = useState("");
  const [heuristicDecoration, setHeuristicDecoration] =
    useState<"frequency" | "performance">("frequency");
  const [
    heuristicPerformanceAggregation,
    setHeuristicPerformanceAggregation,
  ] = useState<
    "mean" | "median" | "min" | "max" | "stdev" | "sum" | "raw_values" | "all"
  >("mean");
  const [
    heuristicDfgPreCleaningNoiseThreshold,
    setHeuristicDfgPreCleaningNoiseThreshold,
  ] = useState("0.05");
  const [
    heuristicLoopLengthTwoThreshold,
    setHeuristicLoopLengthTwoThreshold,
  ] = useState("0.5");
  const [heuristicKeepOncePerCase, setHeuristicKeepOncePerCase] =
    useState(false);
  const [heuristicMostCommonPaths, setHeuristicMostCommonPaths] = useState("");
  const [
    heuristicStartTimestampAttribute,
    setHeuristicStartTimestampAttribute,
  ] = useState("start_timestamp");
  const [
    heuristicLifecycleInstanceAttribute,
    setHeuristicLifecycleInstanceAttribute,
  ] = useState("concept:instance");
  const [powlVariant, setPowlVariant] = useState<"tree" | "brute_force" | "maximal" | "dynamic_clustering" | "dynamic_clustering_plain">("maximal");
  const [powlFilteringType, setPowlFilteringType] = useState<"decreasing_factor" | "dynamic">("decreasing_factor");
  const [powlFilteringThreshold, setPowlFilteringThreshold] = useState("");
  const [powlOrderFrequencyRatio, setPowlOrderFrequencyRatio] = useState("1");
  const [powlBruteForceActivityLimit, setPowlBruteForceActivityLimit] = useState("");
  const [tokenReplayVariant, setTokenReplayVariant] =
    useState<"forward" | "backwards">("forward");
  const [
    tokenReplayStopImmediatelyUnfit,
    setTokenReplayStopImmediatelyUnfit,
  ] = useState(false);
  const logSkeletonFamilies = [
    ["equivalence", "Equivalence"],
    ["always_after", "Always-after"],
    ["always_before", "Always-before"],
    ["never_together", "Never-together"],
    ["directly_follows", "Directly-follows"],
    ["activ_freq", "Activity frequency"],
  ] as const;
  const [logSkeletonNoiseThreshold, setLogSkeletonNoiseThreshold] = useState("");
  const [logSkeletonConstraints, setLogSkeletonConstraints] = useState<Set<string>>(
    () => new Set(logSkeletonFamilies.map(([name]) => name)),
  );
  const declareFamilies = [
    ["existence", "Existence"], ["exactly_one", "Exactly-one"], ["init", "Init"],
    ["responded_existence", "Responded existence"], ["response", "Response"],
    ["precedence", "Precedence"], ["succession", "Succession"], ["altresponse", "Alternate response"],
    ["altprecedence", "Alternate precedence"], ["altsuccession", "Alternate succession"],
    ["chainresponse", "Chain response"], ["chainprecedence", "Chain precedence"],
    ["chainsuccession", "Chain succession"], ["absence", "Absence"], ["coexistence", "Coexistence"],
    ["noncoexistence", "Non-coexistence"], ["nonsuccession", "Non-succession"],
    ["nonchainsuccession", "Non-chain succession"],
  ] as const;
  const [declareConsideredActivities, setDeclareConsideredActivities] = useState("");
  const [declareAllowedTemplates, setDeclareAllowedTemplates] = useState<Set<string>>(
    () => new Set(DECLARE_TEMPLATES),
  );
  const [declareMinSupportRatio, setDeclareMinSupportRatio] = useState("");
  const [declareMinConfidenceRatio, setDeclareMinConfidenceRatio] = useState("");
  const [declareAutoSelectionMultiplier, setDeclareAutoSelectionMultiplier] = useState("0.8");
  const [footprintsDiscoveryVariants, setFootprintsDiscoveryVariants] = useState<Set<string>>(
    () => new Set(FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS),
  );
  const [footprintsModel, setFootprintsModel] = useState<MinerOptions["footprintsModel"]>("process_tree");
  const [footprintsConformanceVariants, setFootprintsConformanceVariants] = useState<Set<string>>(
    () => new Set(["log_model"]),
  );
  const [footprintsStrict, setFootprintsStrict] = useState(false);
  const [footprintsEnableActAlwaysExecuted, setFootprintsEnableActAlwaysExecuted] = useState(true);

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
  const toggleLifecycle = toggle(setRemovedLifecycles);
  const toggleValue = toggle(setRemovedValues);
  const toggleCaseValue = toggle(setRemovedCaseValues);
  const toggleLogSkeletonConstraint = (name: string) => {
    setLogSkeletonConstraints((previous) => {
      // Empty means backend default-all, so keep at least one selected.
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };
  const toggleDeclareTemplate = (name: string) => {
    setDeclareAllowedTemplates((previous) => {
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };
  const toggleFootprintsDiscovery = (name: string) => {
    setFootprintsDiscoveryVariants((previous) => {
      if (name === "petri_reach_graph" && footprintsModel === "dfg") return previous;
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name); else next.add(name);
      if (name === "petri_reach_graph" && next.has(name)) setFootprintsModel("process_tree");
      return next;
    });
  };
  const selectFootprintsModel = (model: MinerOptions["footprintsModel"]) => {
    setFootprintsModel(model);
    if (model === "dfg") {
      setFootprintsDiscoveryVariants((previous) => {
        const next = new Set(previous);
        next.delete("petri_reach_graph");
        if (next.size === 0) next.add("entire_event_log");
        return next;
      });
    }
  };
  const toggleFootprintsConformance = (name: string) => {
    setFootprintsConformanceVariants((previous) => {
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name); else next.add(name);
      return next;
    });
  };

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
  const toPowlThreshold = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) && n >= 0 && n < 1 ? n : undefined;
  };
  const toPowlRatio = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) && n > 0.5 && n <= 1 ? n : undefined;
  };
  const toPowlActivityLimit = (value: string) => {
    if (value.trim() === "") return undefined;
    const n = Number(value);
    return Number.isInteger(n) && n >= 0 ? n : undefined;
  };
  const toLogSkeletonNoise = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) && n >= 0 && n <= 1 ? n : undefined;
  };
  const toDeclareRatio = (value: string) => {
    if (value.trim() === "") return undefined;
    const n = Number(value);
    return Number.isFinite(n) && n >= 0 && n <= 1 ? n : undefined;
  };
  const toDeclareActivities = (value: string) => {
    const names = value.split(",").map((name) => name.trim()).filter(Boolean);
    return names.length ? [...new Set(names)] : undefined;
  };

  const apply = () => {
    const removeList = activities.filter((a) => removed.has(a));
    const removeResourceList = resources.filter((r) => removedResources.has(r));
    const removeLifecycleList = lifecycles.filter((l) =>
      removedLifecycles.has(l),
    );
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
    // The UI takes coverage as a percentage; the backend expects (0, 1].
    const coveragePercent = toFloat(variantCoverage);
    const coverage =
      coveragePercent && coveragePercent > 0 && coveragePercent <= 100
        ? coveragePercent / 100
        : undefined;
    const successionPair =
      successionAction !== "off" && successionFrom && successionTo
        ? [`${successionFrom} -> ${successionTo}`]
        : undefined;
    const succession = (action: "keep" | "remove", relation: string) =>
      successionAction === action && successionRelation === relation
        ? successionPair
        : undefined;
    const declareAutoSelectionActive =
      declareMinSupportRatio.trim() === "" && declareMinConfidenceRatio.trim() === "";
    onApply({
      topVariants: toInt(topVariants),
      minVariantFrequency: toInt(minFrequency),
      variantCoverage: coverage,
      keepDirectlyFollows: succession("keep", "directly"),
      removeDirectlyFollows: succession("remove", "directly"),
      keepEventuallyFollows: succession("keep", "eventually"),
      removeEventuallyFollows: succession("remove", "eventually"),
      prefixActivity: prefixActivity || undefined,
      suffixActivity: suffixActivity || undefined,
      removeActivities: removeList.length > 0 ? removeList : undefined,
      removeResources:
        removeResourceList.length > 0 ? removeResourceList : undefined,
      removeLifecycles:
        removeLifecycleList.length > 0 ? removeLifecycleList : undefined,
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
      heuristicVariant:
        heuristicVariant !== "classic" ? heuristicVariant : undefined,
      dependencyThreshold: toFloat(dependencyThreshold),
      andThreshold: toFloat(andThreshold),
      minEdgeFrequency: toInt(minEdgeFrequency),
      heuristicMinActivityCount: toInt(heuristicMinActivityCount),
      heuristicDecoration:
        heuristicDecoration !== "frequency" ? heuristicDecoration : undefined,
      heuristicPerformanceAggregation:
        heuristicPerformanceAggregation !== "mean"
          ? heuristicPerformanceAggregation
          : undefined,
      heuristicDfgPreCleaningNoiseThreshold: toFloat(
        heuristicDfgPreCleaningNoiseThreshold,
      ),
      heuristicLoopLengthTwoThreshold: toFloat(
        heuristicLoopLengthTwoThreshold,
      ),
      heuristicKeepOncePerCase: heuristicKeepOncePerCase || undefined,
      heuristicMostCommonPaths:
        heuristicMostCommonPaths.trim() || undefined,
      heuristicStartTimestampAttribute:
        heuristicStartTimestampAttribute !== "start_timestamp"
          ? heuristicStartTimestampAttribute
          : undefined,
      heuristicLifecycleInstanceAttribute:
        heuristicLifecycleInstanceAttribute !== "concept:instance"
          ? heuristicLifecycleInstanceAttribute
          : undefined,
      powlVariant: powlVariant !== "maximal" ? powlVariant : undefined,
      powlFilteringType:
        powlFilteringType !== "decreasing_factor" ? powlFilteringType : undefined,
      powlFilteringThreshold: toPowlThreshold(powlFilteringThreshold),
      powlOrderFrequencyRatio: toPowlRatio(powlOrderFrequencyRatio),
      powlBruteForceActivityLimit: toPowlActivityLimit(powlBruteForceActivityLimit),
      tokenReplayVariant:
        tokenReplayVariant !== "forward" ? tokenReplayVariant : undefined,
      tokenReplayStopImmediatelyUnfit:
        tokenReplayStopImmediatelyUnfit || undefined,
      logSkeletonNoiseThreshold: toLogSkeletonNoise(logSkeletonNoiseThreshold),
      logSkeletonConstraints:
        logSkeletonConstraints.size === logSkeletonFamilies.length
          ? undefined
          : [...logSkeletonConstraints],
      declareConsideredActivities: toDeclareActivities(declareConsideredActivities),
      declareAllowedTemplates:
        declareAllowedTemplates.size === declareFamilies.length
          ? undefined
          : [...declareAllowedTemplates],
      declareMinSupportRatio: toDeclareRatio(declareMinSupportRatio),
      declareMinConfidenceRatio: toDeclareRatio(declareMinConfidenceRatio),
      declareAutoSelectionMultiplier: declareAutoSelectionActive
        ? toDeclareRatio(declareAutoSelectionMultiplier)
        : undefined,
      footprintsDiscoveryVariants:
        footprintsDiscoveryVariants.size === FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length &&
        FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.every((name) => footprintsDiscoveryVariants.has(name))
          ? undefined : [...footprintsDiscoveryVariants],
      footprintsModel: footprintsModel !== "process_tree" ? footprintsModel : undefined,
      footprintsConformanceVariants:
        footprintsConformanceVariants.size === 1 && footprintsConformanceVariants.has("log_model")
          ? undefined : [...footprintsConformanceVariants],
      footprintsStrict: footprintsStrict || undefined,
      footprintsEnableActAlwaysExecuted: footprintsEnableActAlwaysExecuted ? undefined : false,
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
    setVariantCoverage("");
    setPrefixActivity("");
    setSuffixActivity("");
    setSuccessionAction("off");
    setSuccessionRelation("directly");
    setSuccessionFrom("");
    setSuccessionTo("");
    setRemoved(new Set());
    setRemovedResources(new Set());
    setRemovedLifecycles(new Set());
    setAttributeKey("");
    setRemovedValues(new Set());
    setAttributeMode("events");
    setCaseAttributeKey("");
    setRemovedCaseValues(new Set());
    setNoiseThreshold("");
    setInductiveVariant("log");
    setAlphaVariant("plus");
    setHeuristicVariant("classic");
    setDependencyThreshold("");
    setAndThreshold("");
    setMinEdgeFrequency("");
    setHeuristicMinActivityCount("");
    setHeuristicDecoration("frequency");
    setHeuristicPerformanceAggregation("mean");
    setHeuristicDfgPreCleaningNoiseThreshold("0.05");
    setHeuristicLoopLengthTwoThreshold("0.5");
    setHeuristicKeepOncePerCase(false);
    setHeuristicMostCommonPaths("");
    setHeuristicStartTimestampAttribute("start_timestamp");
    setHeuristicLifecycleInstanceAttribute("concept:instance");
    setPowlVariant("maximal");
    setPowlFilteringType("decreasing_factor");
    setPowlFilteringThreshold("");
    setPowlOrderFrequencyRatio("1");
    setPowlBruteForceActivityLimit("");
    setTokenReplayVariant("forward");
    setTokenReplayStopImmediatelyUnfit(false);
    setLogSkeletonNoiseThreshold("");
    setLogSkeletonConstraints(new Set(logSkeletonFamilies.map(([name]) => name)));
    setDeclareConsideredActivities("");
    setDeclareAllowedTemplates(new Set(DECLARE_TEMPLATES));
    setDeclareMinSupportRatio("");
    setDeclareMinConfidenceRatio("");
    setDeclareAutoSelectionMultiplier("0.8");
    setFootprintsDiscoveryVariants(new Set(FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS));
    setFootprintsModel("process_tree");
    setFootprintsConformanceVariants(new Set(["log_model"]));
    setFootprintsStrict(false);
    setFootprintsEnableActAlwaysExecuted(true);
    onApply({});
  };

  const dirty =
    topVariants !== "" ||
    minFrequency !== "" ||
    variantCoverage !== "" ||
    timeFrom !== "" ||
    timeTo !== "" ||
    reworkActivity !== "" ||
    betweenStart !== "" ||
    betweenEnd !== "" ||
    prefixActivity !== "" ||
    suffixActivity !== "" ||
    successionAction !== "off" ||
    removed.size > 0 ||
    removedResources.size > 0 ||
    removedLifecycles.size > 0 ||
    attributeKey !== "" ||
    caseAttributeKey !== "" ||
    noiseThreshold !== "" ||
    inductiveVariant !== "log" ||
    alphaVariant !== "plus" ||
    heuristicVariant !== "classic" ||
    dependencyThreshold !== "" ||
    andThreshold !== "" ||
    minEdgeFrequency !== "" ||
    heuristicMinActivityCount !== "" ||
    heuristicDecoration !== "frequency" ||
    heuristicPerformanceAggregation !== "mean" ||
    heuristicDfgPreCleaningNoiseThreshold !== "0.05" ||
    heuristicLoopLengthTwoThreshold !== "0.5" ||
    heuristicKeepOncePerCase ||
    heuristicMostCommonPaths.trim() !== "" ||
    heuristicStartTimestampAttribute !== "start_timestamp" ||
    heuristicLifecycleInstanceAttribute !== "concept:instance";

  const powlDirty =
    powlVariant !== "maximal" ||
    powlFilteringType !== "decreasing_factor" ||
    powlFilteringThreshold !== "" && powlFilteringThreshold !== "0" ||
    powlOrderFrequencyRatio !== "" && powlOrderFrequencyRatio !== "1" ||
    powlBruteForceActivityLimit !== "" && powlBruteForceActivityLimit !== "0";
  const tokenReplayDirty =
    tokenReplayVariant !== "forward" || tokenReplayStopImmediatelyUnfit;
  const logSkeletonDirty =
    logSkeletonNoiseThreshold !== "" ||
    logSkeletonConstraints.size !== logSkeletonFamilies.length;
  const declareDirty =
    declareConsideredActivities.trim() !== "" ||
    declareAllowedTemplates.size !== declareFamilies.length ||
    declareMinSupportRatio !== "" || declareMinConfidenceRatio !== "" ||
    declareAutoSelectionMultiplier !== "" && declareAutoSelectionMultiplier !== "0.8";
  const footprintsDirty =
    footprintsModel !== "process_tree" || footprintsStrict || !footprintsEnableActAlwaysExecuted ||
    footprintsDiscoveryVariants.size !== FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length ||
    FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.some((name) => !footprintsDiscoveryVariants.has(name)) ||
    footprintsConformanceVariants.size !== 1 || !footprintsConformanceVariants.has("log_model");
  const formDirty = dirty || powlDirty || tokenReplayDirty || logSkeletonDirty || declareDirty || footprintsDirty;

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
            <label className="filters-field">
              <span>Variant coverage %</span>
              <input
                type="number"
                min={1}
                max={100}
                placeholder="all"
                title="Keep the most frequent variants that together cover this share of cases"
                value={variantCoverage}
                onChange={(e) => setVariantCoverage(e.target.value)}
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
              <span>POWL discovery algorithm</span>
              <select value={powlVariant} onChange={(e) => setPowlVariant(e.target.value as typeof powlVariant)}>
                <option value="maximal">maximal (default)</option>
                <option value="tree">tree</option>
                <option value="brute_force">brute force</option>
                <option value="dynamic_clustering">dynamic clustering</option>
                <option value="dynamic_clustering_plain">dynamic clustering (plain)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>POWL filtering</span>
              <select value={powlFilteringType} onChange={(e) => setPowlFilteringType(e.target.value as typeof powlFilteringType)}>
                <option value="decreasing_factor">decreasing factor (default)</option>
                <option value="dynamic">dynamic</option>
              </select>
            </label>
            <label className="filters-field">
              <span>POWL filter threshold</span>
              <input type="number" min={0} max={0.99} step={0.05} placeholder="0" value={powlFilteringThreshold} onChange={(e) => setPowlFilteringThreshold(e.target.value)} />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>POWL order-frequency ratio</span>
              <input type="number" min={0.51} max={1} step={0.01} placeholder="1" value={powlOrderFrequencyRatio} onChange={(e) => setPowlOrderFrequencyRatio(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>POWL brute-force activity limit</span>
              <input type="number" min={0} step={1} placeholder="0 = unlimited" value={powlBruteForceActivityLimit} onChange={(e) => setPowlBruteForceActivityLimit(e.target.value)} />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Token replay</span>
              <select
                value={tokenReplayVariant}
                onChange={(e) =>
                  setTokenReplayVariant(
                    e.target.value as typeof tokenReplayVariant,
                  )
                }
              >
                <option value="forward">forward (default)</option>
                <option value="backwards">backwards</option>
              </select>
            </label>
            <label className="filters-field">
              <span>On an unfit event</span>
              <select
                value={tokenReplayStopImmediatelyUnfit ? "stop" : "continue"}
                onChange={(e) =>
                  setTokenReplayStopImmediatelyUnfit(e.target.value === "stop")
                }
              >
                <option value="continue">continue replay (default)</option>
                <option value="stop">stop the trace immediately</option>
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Log skeleton noise threshold</span>
              <input
                type="number"
                min={0}
                max={1}
                step={0.05}
                placeholder="0 (default)"
                title="Ignore infrequent log-skeleton constraints; 0 keeps every observed constraint"
                value={logSkeletonNoiseThreshold}
                onChange={(event) => setLogSkeletonNoiseThreshold(event.target.value)}
              />
            </label>
            <div className="filters-field filters-field-wide">
              <span title="Constraint families to mine and check">Log skeleton constraints</span>
              <div className="filter-chips" role="group" aria-label="Log skeleton constraints">
                {logSkeletonFamilies.map(([name, label]) => {
                  const selected = logSkeletonConstraints.has(name);
                  return (
                    <button
                      type="button"
                      key={name}
                      className={selected ? "filter-chip active" : "filter-chip"}
                      aria-pressed={selected}
                      title={selected ? `Disable ${label}` : `Enable ${label}`}
                      onClick={() => toggleLogSkeletonConstraint(name)}
                    >
                      {label}
                    </button>
                  );
                })}
              </div>
              <small>At least one family stays selected; all six is the default.</small>
            </div>
          </div>

          <div className="filters-row">
            <label className="filters-field filters-field-wide">
              <span>Declare considered activities <small>(comma-separated; blank = infer)</small></span>
              <input
                type="text"
                placeholder="A, B, C"
                value={declareConsideredActivities}
                onChange={(event) => setDeclareConsideredActivities(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Declare min support ratio</span>
              <input
                type="number" min={0} max={1} step={0.05} placeholder="auto"
                value={declareMinSupportRatio}
                onChange={(event) => setDeclareMinSupportRatio(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Declare min confidence ratio</span>
              <input
                type="number" min={0} max={1} step={0.05} placeholder="auto"
                value={declareMinConfidenceRatio}
                onChange={(event) => setDeclareMinConfidenceRatio(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span
                title="Used only when both minimum support and confidence are left blank"
              >
                Declare auto-selection multiplier
              </span>
              <input
                type="number" min={0} max={1} step={0.05} placeholder="0.8"
                value={declareAutoSelectionMultiplier}
                disabled={declareMinSupportRatio.trim() !== "" || declareMinConfidenceRatio.trim() !== ""}
                title="Active only when both explicit threshold fields are blank"
                onChange={(event) => setDeclareAutoSelectionMultiplier(event.target.value)}
              />
              {(declareMinSupportRatio.trim() !== "" || declareMinConfidenceRatio.trim() !== "") && (
                <small>Disabled while an explicit support or confidence threshold is set.</small>
              )}
            </label>
          </div>
          <div className="filters-activities">
            <span className="filters-field-label">
              Declare templates <small>(at least one; all eighteen is the default)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Declare templates">
              {declareFamilies.map(([name, label]) => {
                const selected = declareAllowedTemplates.has(name);
                return (
                  <button
                    type="button" key={name}
                    className={selected ? "filter-chip active" : "filter-chip"}
                    aria-pressed={selected}
                    title={selected ? `Disable ${label}` : `Enable ${label}`}
                    onClick={() => toggleDeclareTemplate(name)}
                  >
                    {label}
                  </button>
                );
              })}
            </div>
          </div>

          <div className="filters-activities">
            <span className="filters-field-label">
              Footprints discovery variants <small>(at least one; four common variants are the default)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Footprints discovery variants">
              {FOOTPRINTS_DISCOVERY_VARIANTS.map((name) => {
                const selected = footprintsDiscoveryVariants.has(name);
                const disabled = name === "petri_reach_graph" && footprintsModel === "dfg";
                return (
                  <button type="button" key={name}
                    className={selected ? "filter-chip active" : "filter-chip"}
                    aria-pressed={selected} disabled={disabled}
                    title={disabled ? "Petri reachability requires a Petri-net-backed model" : undefined}
                    onClick={() => toggleFootprintsDiscovery(name)}>
                    {name.replaceAll("_", " ")}
                  </button>
                );
              })}
            </div>
            {footprintsModel === "dfg" && <small>Petri reachability is unavailable with the DFG model.</small>}
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Footprints model</span>
              <select value={footprintsModel} onChange={(event) => selectFootprintsModel(event.target.value as MinerOptions["footprintsModel"])}>
                <option value="process_tree">process tree (default)</option>
                <option value="powl">POWL</option>
                <option value="inductive">inductive</option>
                <option value="alpha">Alpha</option>
                <option value="heuristic">heuristic</option>
                <option value="dfg" disabled={footprintsDiscoveryVariants.has("petri_reach_graph")}>DFG</option>
              </select>
              {footprintsDiscoveryVariants.has("petri_reach_graph") && <small>DFG is disabled while Petri reachability is selected.</small>}
            </label>
            <label className="filters-field">
              <span>Footprints flags</span>
              <label className="checkbox-field">
                <input type="checkbox" checked={footprintsStrict} onChange={(event) => setFootprintsStrict(event.target.checked)} /> strict log-model checks
              </label>
              <label className="checkbox-field">
                <input type="checkbox" checked={footprintsEnableActAlwaysExecuted} onChange={(event) => setFootprintsEnableActAlwaysExecuted(event.target.checked)} /> activity-always-executed checks
              </label>
            </label>
          </div>
          <div className="filters-activities">
            <span className="filters-field-label">
              Footprints conformance variants <small>(at least one; log-model is the default)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Footprints conformance variants">
              {FOOTPRINTS_CONFORMANCE_VARIANTS.map((name) => {
                const selected = footprintsConformanceVariants.has(name);
                return (
                  <button type="button" key={name}
                    className={selected ? "filter-chip active" : "filter-chip"}
                    aria-pressed={selected} onClick={() => toggleFootprintsConformance(name)}>
                    {name.replaceAll("_", " ")}
                  </button>
                );
              })}
            </div>
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

          <div className="filters-row">
            <label className="filters-field">
              <span>Head: keep up to first</span>
              <select
                value={prefixActivity}
                onChange={(e) => setPrefixActivity(e.target.value)}
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
              <span>Tail: keep from first</span>
              <select
                value={suffixActivity}
                onChange={(e) => setSuffixActivity(e.target.value)}
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

          <div className="filters-row">
            <label className="filters-field">
              <span>Succession</span>
              <select
                value={successionAction}
                onChange={(e) =>
                  setSuccessionAction(e.target.value as typeof successionAction)
                }
              >
                <option value="off">off</option>
                <option value="keep">keep cases where</option>
                <option value="remove">drop cases where</option>
              </select>
            </label>
            <label className="filters-field">
              <span>activity</span>
              <select
                value={successionFrom}
                disabled={successionAction === "off"}
                onChange={(e) => setSuccessionFrom(e.target.value)}
              >
                <option value="">—</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>is</span>
              <select
                value={successionRelation}
                disabled={successionAction === "off"}
                onChange={(e) =>
                  setSuccessionRelation(
                    e.target.value as typeof successionRelation,
                  )
                }
              >
                <option value="directly">directly followed by</option>
                <option value="eventually">eventually followed by</option>
              </select>
            </label>
            <label className="filters-field">
              <span>activity</span>
              <select
                value={successionTo}
                disabled={successionAction === "off"}
                onChange={(e) => setSuccessionTo(e.target.value)}
              >
                <option value="">—</option>
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

          {lifecycles.length > 0 && (
            <div className="filters-activities">
              <span className="filters-field-label">
                Lifecycle transitions{" "}
                <small>(click to drop; keep only “complete” to make a
                start/complete log atomic)</small>
              </span>
              <div className="filters-chips">
                {lifecycles.map((name) => {
                  const dropped = removedLifecycles.has(name);
                  return (
                    <button
                      key={name}
                      className={dropped ? "filters-chip dropped" : "filters-chip"}
                      onClick={() => toggleLifecycle(name)}
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
              <span>Heuristic variant</span>
              <select
                value={heuristicVariant}
                onChange={(e) =>
                  setHeuristicVariant(
                    e.target.value as "classic" | "plusplus",
                  )
                }
              >
                <option value="classic">classic (default)</option>
                <option value="plusplus">Miner++ (interval concurrency)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Heuristic: dependency threshold</span>
              <input
                type="number"
                min={-1}
                max={1}
                step={0.05}
                placeholder="0.5"
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
              <span>Heuristic: min path frequency</span>
              <input
                type="number"
                min={1}
                placeholder="1"
                value={minEdgeFrequency}
                onChange={(e) => setMinEdgeFrequency(e.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Heuristic: min activity count</span>
              <input
                type="number"
                min={1}
                placeholder="1"
                value={heuristicMinActivityCount}
                onChange={(e) => setHeuristicMinActivityCount(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic decoration</span>
              <select
                value={heuristicDecoration}
                onChange={(e) =>
                  setHeuristicDecoration(
                    e.target.value as "frequency" | "performance",
                  )
                }
              >
                <option value="frequency">frequency (default)</option>
                <option value="performance">performance</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Miner++: start-time attribute</span>
              <input
                type="text"
                value={heuristicStartTimestampAttribute}
                disabled={heuristicVariant !== "plusplus"}
                onChange={(e) =>
                  setHeuristicStartTimestampAttribute(e.target.value)
                }
              />
            </label>
            <label className="filters-field">
              <span>Miner++: lifecycle instance</span>
              <input
                type="text"
                value={heuristicLifecycleInstanceAttribute}
                disabled={heuristicVariant !== "plusplus"}
                onChange={(e) =>
                  setHeuristicLifecycleInstanceAttribute(e.target.value)
                }
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Classic: DFG pre-clean noise</span>
              <input
                type="number"
                min={0}
                step={0.01}
                value={heuristicDfgPreCleaningNoiseThreshold}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) =>
                  setHeuristicDfgPreCleaningNoiseThreshold(e.target.value)
                }
              />
            </label>
            <label className="filters-field">
              <span>Classic: length-two loop threshold</span>
              <input
                type="number"
                min={0}
                max={1}
                step={0.05}
                value={heuristicLoopLengthTwoThreshold}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) =>
                  setHeuristicLoopLengthTwoThreshold(e.target.value)
                }
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={heuristicKeepOncePerCase}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) => setHeuristicKeepOncePerCase(e.target.checked)}
              />
              <span>Classic: one path count per case</span>
            </label>
            <label className="filters-field">
              <span>Classic: pre-clean path exemptions</span>
              <input
                type="text"
                placeholder="A -> B, C -> D"
                value={heuristicMostCommonPaths}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) => setHeuristicMostCommonPaths(e.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Performance aggregation</span>
              <select
                value={heuristicPerformanceAggregation}
                disabled={heuristicDecoration !== "performance"}
                onChange={(e) =>
                  setHeuristicPerformanceAggregation(
                    e.target.value as
                      | "mean"
                      | "median"
                      | "min"
                      | "max"
                      | "stdev"
                      | "sum"
                      | "raw_values"
                      | "all",
                  )
                }
              >
                <option value="mean">mean (default)</option>
                <option value="median">median</option>
                <option value="min">minimum</option>
                <option value="max">maximum</option>
                <option value="stdev">standard deviation</option>
                <option value="sum">sum</option>
                <option value="raw_values">raw values</option>
                <option value="all">all summaries</option>
              </select>
            </label>
          </div>

          <div className="filters-actions">
            <button className="filters-apply" onClick={apply} disabled={mining}>
              Apply
            </button>
            <button
              className="filters-reset"
              onClick={reset}
              disabled={mining || !formDirty}
            >
              Reset
            </button>
          </div>
        </div>
      )}
    </section>
  );
}
