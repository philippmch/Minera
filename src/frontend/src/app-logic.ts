import {hasExactFilters} from "./exact-filter-controls";
import { type FilterOptions, type MinerOptions } from "./api";
import { type MineraExport } from "./types";
import { traceFacts } from "./mine-traces";
import {
  DECLARE_TEMPLATES,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
} from "./types";

export type Tab = "log" | "dotted" | "timing" | "behavior" | "dfg" | "model" | "conformance" | "resources" | "embeddings" | "llm" | "query" | "anonymization" | "clustering" | "transformation";

/**
 * Opening tab for a freshly loaded analysis: inspect the raw event log first
 * when it is present, otherwise fall back to the directly-follows graph. Exports
 * from older tool versions carry no traces, so they must not land on a log tab
 * that is never rendered.
 */
export function initialTab(data: MineraExport): Tab {
  return traceFacts(data).hasCases ? "log" : "dfg";
}

/**
 * The view to render for the selected tab. A re-mine keeps the selected tab,
 * but the new analysis may no longer offer it (no traces, no timestamps, no
 * conformance); the first available view is shown instead, falling back to
 * the directly-follows graph.
 */
export function displayedTab(tab: Tab, available: readonly Tab[]): Tab {
  return available.includes(tab) ? tab : available[0] ?? "dfg";
}

/**
 * Whether any miner setting deviates from the backend defaults; drives the
 * same "active" badge as the filters. Zero is a meaningful threshold for the
 * heuristic settings, so for those only absence means default.
 */
export function hasActiveMinerOptions(options: MinerOptions): boolean {
  if (hasExactFilters(options)) return true;
  const allLogSkeletonConstraints = [
    "equivalence",
    "always_after",
    "always_before",
    "never_together",
    "directly_follows",
    "activ_freq",
  ];
  const logSkeletonConstraintsActive =
    options.logSkeletonConstraints !== undefined &&
    (options.logSkeletonConstraints.length === 0 ||
      options.logSkeletonConstraints.length !== allLogSkeletonConstraints.length ||
      allLogSkeletonConstraints.some(
        (name) => !options.logSkeletonConstraints?.includes(name),
      ));
  const declareAllowedTemplatesActive =
    options.declareAllowedTemplates !== undefined &&
    (options.declareAllowedTemplates.length === 0 ||
      options.declareAllowedTemplates.length !== DECLARE_TEMPLATES.length ||
      DECLARE_TEMPLATES.some(
        (name) => !options.declareAllowedTemplates?.includes(name),
      ));
  const footprintsDiscoveryActive =
    options.footprintsDiscoveryVariants !== undefined &&
    (options.footprintsDiscoveryVariants.length !== FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length ||
      FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.some(
        (name) => !options.footprintsDiscoveryVariants?.includes(name),
      ));
  const footprintsConformanceActive =
    options.footprintsConformanceVariants !== undefined &&
    (options.footprintsConformanceVariants.length !== 1 ||
      !options.footprintsConformanceVariants.includes("log_model"));
  const dfgStatisticalActive = options.dfgStatisticalVariants?.some((name) =>
    ["native", "frequency", "frequency_greedy", "frequency_triples", "performance",
      "performance_greedy", "case_attributes", "clean", "clean_polars", "clean_time"].includes(name),
  );
  const dfgCausalActive = options.dfgCausalVariants?.some((name) =>
    ["alpha", "heuristic"].includes(name),
  );
  const correlationActive = Boolean(options.correlationVariants?.some((name) =>
    ["classic", "classic_split", "trace_based"].includes(name),
  ));
  const spectrumActive = (options.spectrumActivities ?? []).filter((name) => name.trim()).length >= 2;
  return Boolean(
    options.noiseThreshold ||
      (options.inductiveVariant && options.inductiveVariant !== "log") ||
      options.inductiveDisableStrictSequenceCut === true ||
      options.inductiveDisableFallthroughs === true ||
      options.inductiveMultiprocessing === true ||
      options.inductiveSortEventsByTimestamp === true ||
      Boolean(options.inductiveActivityKey?.trim() &&
        options.inductiveActivityKey.trim() !== "concept:name") ||
      Boolean(options.inductiveTimestampKey?.trim() &&
        options.inductiveTimestampKey.trim() !== "time:timestamp") ||
      Boolean(options.inductiveCaseIdKey?.trim() &&
        options.inductiveCaseIdKey.trim() !== "case:concept:name") ||
      (options.alphaVariant && options.alphaVariant !== "classic") ||
      Boolean(options.alphaActivityAttribute?.trim() &&
        options.alphaActivityAttribute.trim() !== "concept:name") ||
      options.alphaRemoveUnconnected === true ||
      options.alphaMaxRowsToMaterialize !== undefined ||
      (options.heuristicVariant && options.heuristicVariant !== "classic") ||
      (options.heuristicActivityAttribute &&
        options.heuristicActivityAttribute.trim() !== "concept:name") ||
      (options.heuristicTimestampAttribute &&
        options.heuristicTimestampAttribute.trim() !== "time:timestamp") ||
      options.dependencyThreshold !== undefined ||
      options.andThreshold !== undefined ||
      options.minEdgeFrequency !== undefined ||
      options.heuristicMinActivityCount !== undefined ||
      (options.heuristicDecoration &&
        options.heuristicDecoration !== "frequency") ||
      (options.heuristicPerformanceAggregation &&
        options.heuristicPerformanceAggregation !== "mean") ||
      options.heuristicBusinessHours === true ||
      (options.heuristicDfgPreCleaningNoiseThreshold !== undefined &&
        options.heuristicDfgPreCleaningNoiseThreshold !== 0.05) ||
      (options.heuristicLoopLengthTwoThreshold !== undefined &&
        options.heuristicLoopLengthTwoThreshold !== 0.5) ||
      options.heuristicKeepOncePerCase === true ||
      Boolean(options.heuristicMostCommonPaths?.trim()) ||
      Boolean(options.heuristicStartTimestampAttribute?.trim()) ||
      (options.heuristicLifecycleInstanceAttribute &&
        options.heuristicLifecycleInstanceAttribute !== "concept:instance") ||
      (options.transitionView && options.transitionView !== "sequence") ||
      options.transitionDirection && options.transitionDirection !== "forward" ||
      options.transitionWindow !== undefined && options.transitionWindow !== 2 ||
      options.transitionIncludeData === true ||
      Boolean(options.transitionActivityKey?.trim() &&
        options.transitionActivityKey.trim() !== "concept:name") ||
      Boolean(options.transitionCaseIdKey?.trim() &&
        options.transitionCaseIdKey.trim() !== "case:concept:name") ||
      (options.powlVariant && options.powlVariant !== "maximal") ||
        (options.powlFilteringType && options.powlFilteringType !== "decreasing_factor") ||
      (options.powlFilteringThreshold !== undefined && options.powlFilteringThreshold !== 0) ||
        (options.powlOrderFrequencyRatio !== undefined && options.powlOrderFrequencyRatio !== 1) ||
        (options.powlBruteForceActivityLimit !== undefined && options.powlBruteForceActivityLimit !== 0) ||
        Boolean(options.powlActivityKey?.trim() &&
          options.powlActivityKey.trim() !== "concept:name") ||
        Boolean(options.powlTimestampKey?.trim() &&
          options.powlTimestampKey.trim() !== "time:timestamp") ||
        Boolean(options.powlCaseIdKey?.trim() &&
          options.powlCaseIdKey.trim() !== "case:concept:name") ||
      (options.tokenReplayVariant && options.tokenReplayVariant !== "forward") ||
      options.tokenReplayStopImmediatelyUnfit === true ||
      options.tokenReplayConsiderRemainingInFitness === false ||
      options.tokenReplayTryToReachFinalMarkingThroughHidden === false ||
      options.tokenReplayConsiderActivitiesNotInModelInFitness === true ||
      options.tokenReplayMaxRowsToMaterialize !== undefined ||
      Boolean(options.tokenReplayCaseIdAttribute?.trim() &&
        options.tokenReplayCaseIdAttribute.trim() !== "case:concept:name") ||
      options.tokenReplayCleaningTokenFlood === true ||
      options.tokenReplayExhaustiveInvisibleExploration === true ||
      options.tokenReplayWalkThroughHiddenTransitions === false ||
      options.tokenReplayIncludeTraceDiagnostics === true ||
      options.tokenReplayIncludeDurationDiagnostics === true ||
      options.tokenReplayIncludeRootCauseDiagnostics === true ||
      Boolean(options.tokenReplayRootCauseStringAttributes?.length) ||
      Boolean(options.tokenReplayRootCauseNumericAttributes?.length) ||
      options.tokenReplayRootCauseEnableMultiplier === true ||
      Boolean(options.tokenReplayTimestampAttribute?.trim() &&
        options.tokenReplayTimestampAttribute.trim() !== "time:timestamp") ||
      Boolean(options.tokenReplayActivityAttribute?.trim() &&
        options.tokenReplayActivityAttribute.trim() !== "concept:name") ||
      options.logSkeletonNoiseThreshold !== undefined &&
        options.logSkeletonNoiseThreshold !== 0 ||
      logSkeletonConstraintsActive ||
      options.declareConsideredActivities?.length ||
      options.declareConsideredActivitiesExact !== undefined ||
      options.dfgCaseAttributesExact !== undefined ||
      declareAllowedTemplatesActive ||
      options.declareMinSupportRatio !== undefined ||
      options.declareMinConfidenceRatio !== undefined ||
        (options.declareAutoSelectionMultiplier !== undefined &&
          options.declareAutoSelectionMultiplier !== 0.8) ||
      footprintsDiscoveryActive ||
      options.footprintsModel && options.footprintsModel !== "process_tree" ||
      footprintsConformanceActive ||
      options.footprintsStrict === true ||
      options.footprintsEnableActAlwaysExecuted === false ||
      Boolean(options.footprintsActivityAttribute?.trim() &&
        options.footprintsActivityAttribute.trim() !== "concept:name") ||
      Boolean(options.footprintsCaseIdAttribute?.trim() &&
        options.footprintsCaseIdAttribute.trim() !== "case:concept:name") ||
      Boolean(options.footprintsTimestampAttribute?.trim() &&
        options.footprintsTimestampAttribute.trim() !== "time:timestamp") ||
      Boolean(options.footprintsIndexAttribute?.trim() &&
        options.footprintsIndexAttribute.trim() !== "@@index") ||
      Boolean(options.footprintsStartTimestampAttribute?.trim()) ||
      options.footprintsSortRequired === false ||
      Boolean(options.footprintsInitialMarking?.trim()) ||
      dfgStatisticalActive || dfgCausalActive || correlationActive
      || Boolean(correlationActive && options.correlationActivityAttribute?.trim() &&
        options.correlationActivityAttribute.trim() !== "concept:name")
      || Boolean(correlationActive && options.correlationTimestampAttribute?.trim() &&
        options.correlationTimestampAttribute.trim() !== "time:timestamp")
      || Boolean(correlationActive && options.correlationStartTimestampAttribute?.trim())
      || Boolean(correlationActive && options.correlationCaseIdAttribute?.trim() &&
        options.correlationCaseIdAttribute.trim() !== "case:concept:name")
      || Boolean(options.temporalActivityAttribute?.trim() &&
        options.temporalActivityAttribute.trim() !== "concept:name")
      || Boolean(options.temporalCaseIdAttribute?.trim() &&
        options.temporalCaseIdAttribute.trim() !== "case:concept:name")
      || Boolean(options.temporalSourceTimestampAttribute?.trim())
      || Boolean(options.temporalTargetTimestampAttribute?.trim())
      || options.temporalBusinessHours === true
      || Boolean(options.temporalBusinessHourSlots?.trim())
      || options.temporalStrictAttributes === true
      || spectrumActive
      || (options.batches === true)
      || (options.minimumSelfDistanceVariant && options.minimumSelfDistanceVariant !== "log")
      || Boolean(options.minimumSelfDistanceActivityKey?.trim())
      || Boolean(options.minimumSelfDistanceCaseIdKey?.trim())
      || (options.overlap === true)
  );
}

/** Whether any filter constraint is set; drives the "filtered" badge. */
export function hasActiveFilters(filters: FilterOptions): boolean {
  if (hasExactFilters(filters)) return true;
  return Boolean(
    filters.topVariants ||
      filters.minVariantFrequency ||
      filters.variantCoverage ||
      filters.variantMinCoverage !== undefined ||
      filters.variantMaxCoverage !== undefined ||
      filters.variantPercentage !== undefined ||
      filters.keepDirectlyFollows?.length ||
      filters.removeDirectlyFollows?.length ||
      filters.keepEventuallyFollows?.length ||
      filters.removeEventuallyFollows?.length ||
      filters.tracePatterns?.length ||
      filters.admittedVariants?.length ||
      filters.prefixActivity ||
      filters.suffixActivity ||
      (filters.prefixActivity &&
        (filters.prefixStrict === false || filters.prefixOccurrence === "last")) ||
      (filters.suffixActivity &&
        (filters.suffixStrict === false || filters.suffixOccurrence === "last")) ||
      filters.segmentPrefixActivity ||
      filters.segmentSuffixActivity ||
      (filters.segmentPrefixActivity &&
        (filters.segmentPrefixStrict === false ||
          filters.segmentPrefixOccurrence === "last")) ||
      (filters.segmentSuffixActivity &&
        (filters.segmentSuffixStrict === false ||
          filters.segmentSuffixOccurrence === "last")) ||
      filters.segmentBetweenStarts?.length && filters.segmentBetweenEnds?.length ||
      (filters.pathPredecessor && filters.pathSuccessor) ||
      filters.maxCases !== undefined ||
      filters.removeActivities?.length ||
      filters.keepActivities?.length ||
      filters.removeResources?.length ||
      filters.keepResources?.length ||
      filters.removeLifecycles?.length ||
      filters.keepLifecycles?.length ||
      filters.startActivities?.length ||
      filters.endActivities?.length ||
      (filters.startActivities?.length && filters.startActivitiesPositive === false) ||
      (filters.endActivities?.length && filters.endActivitiesPositive === false) ||
      filters.minLength ||
      filters.maxLength ||
      filters.timeFrom ||
      filters.timeTo ||
      filters.caseDurationMin !== undefined ||
      filters.caseDurationMax !== undefined ||
      filters.maxActivities !== undefined ||
      filters.relativeAttributeKey !== undefined ||
      filters.relativeAttributeStake !== undefined ||
      filters.relativeAttributeKeepOncePerCase === false ||
      filters.attributeOccurrenceMin !== undefined ||
      (filters.numericAttributeKey &&
        (filters.numericAttributeMin !== undefined ||
          filters.numericAttributeMax !== undefined)) ||
      filters.reworkActivity ||
      filters.attributeKey ||
      filters.caseAttributeKey ||
      filters.ltlVariant && filters.ltlAttributeValues?.length ||
      (filters.betweenStart && filters.betweenEnd) ||
      filters.activitySplit?.length ||
      filters.repetitionValue !== undefined ||
      filters.consecutiveGrouping ||
      filters.timestampGrouping,
  );
}

/**
 * Distinct lifecycle transitions the log's events carry, in first-seen order.
 * Empty for a log of atomic events; feeds the lifecycle filter chips.
 */
export function logLifecycles(data: MineraExport): string[] {
  // From the traces when the analysis carries them, else from its trace summary.
  return [...traceFacts(data).lifecycles];
}

/**
 * Largest edge frequency in the log, floored at 1 so edge-weight scaling never
 * divides by zero on a log with no edges (e.g. a single-activity log).
 */
export function maxEdgeCount(data: MineraExport): number {
  // A loop, not Math.max(...counts): spreading a very large edge list overflows the call stack.
  let max = 1;
  for (const edge of data.edges) if (edge.count > max) max = edge.count;
  return max;
}

/** Whether an opened file is a pre-mined JSON export rather than a raw CSV log. */
export function isJsonExport(name: string): boolean {
  return /\.json$/i.test(name);
}

/** Whether a JSON document carries either supported object-centric envelope. */
export function isOcelJsonDocument(value: unknown): boolean {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  const hasPair = (events: string, objects: string) =>
    record[events] !== null && typeof record[events] === "object" &&
    record[objects] !== null && typeof record[objects] === "object";
  return hasPair("ocel:events", "ocel:objects") ||
    hasPair("events", "objects");
}

/**
 * Field delimiter implied by a file's name: a tab for ".tsv" uploads, otherwise
 * undefined so the backend applies its default (comma).
 */
export function delimiterForFile(name: string): string | undefined {
  return /\.tsv$/i.test(name) ? "\t" : undefined;
}

/** Every Petri model that the quality evaluators can address in this view. */
export function evaluationModelIds(data: MineraExport, importedAvailable = false): string[] {
  const ids = new Set<string>([
    ...Object.keys(data.conformance ?? {}),
    ...Object.keys(data.evaluation ?? {}),
  ]);
  if (data.inductive_petri_net) ids.add("inductive");
  if (data.petri_net) ids.add("alpha");
  if (data.heuristic_petri_net) ids.add("heuristic");
  if (data.powl_petri_net) ids.add("powl");
  if (importedAvailable) ids.add("imported");
  return [...ids].sort();
}

/** How a rollback (cancel-mine) settled; undefined when nothing was posted, so the backend never left the previous source. */
export type LiveSourceRollback = {confirmed: boolean; activeSourceId?: string | null} | undefined | void;

/**
 * Hands the previous live backend source back after a load that failed or was
 * cancelled. The backend keeps (or, after cancel-mine, restores) its previous
 * session in that case, so the still-displayed analysis is live again once the
 * rollback request has finished, but only when the backend confirmed the
 * cancel and, if it reported the source it now holds, that source is the
 * previous one: a cancel that failed, or one overtaken by another load, leaves
 * the analysis offline instead of claiming a source the backend no longer
 * holds. A newer load, or going home, supersedes a pending handback.
 */
export class LiveSourceHandback {
  private epoch = 0;
  /** A new load or going home: any pending handback must not apply. */
  supersede(): void { this.epoch += 1; }
  async handback(previous: string | null | undefined, rollback: Promise<LiveSourceRollback>, restore: (sourceId: string) => void): Promise<boolean> {
    if (!previous) return false;
    const epoch = ++this.epoch;
    const outcome: LiveSourceRollback = await rollback.catch(() => ({confirmed: false}));
    if (epoch !== this.epoch) return false;
    if (outcome && (!outcome.confirmed || (outcome.activeSourceId !== undefined && outcome.activeSourceId !== previous))) return false;
    restore(previous);
    return true;
  }
}

/** A workspace run stays valid when another model is imported, unless it ran on the imported model itself. */
export function runSurvivesModelImport(runMiner: string | null | undefined): boolean {
  return runMiner !== "imported";
}

/** Whether a run started on `miner` at imported-model generation `started` is stale now (generation `current`): another model was imported while a run on the imported net was in flight. */
export function importedRunStale(miner: string | null | undefined, started: number, current: number): boolean {
  return started !== current && !runSurvivesModelImport(miner);
}

export const IMPORTED_MODEL_CHANGED = "The imported model changed while this run was pending; run it again.";

/** Case rows with stable React keys: logs may repeat a case id, so rows are keyed by trace position. */
export function caseRowEntries<T extends { case_id: string }>(traces: readonly T[], query: string): Array<{ key: number; trace: T }> {
  const needle = query.trim().toLowerCase(), rows: Array<{ key: number; trace: T }> = [];
  traces.forEach((trace, key) => { if (!needle || trace.case_id.toLowerCase().includes(needle)) rows.push({ key, trace }); });
  return rows;
}

/** A numeric input's value: an emptied field means "not set" rather than 0. */
export function optionalNumber(text: string): number | undefined {
  return text.trim() === "" ? undefined : Number(text);
}

const identityKeys = new WeakMap<object, number>();
let nextIdentityKey = 0;
/** A React key for an object's identity, e.g. a File: two uploads with the same name stay distinct. */
export function identityKey(value: object | null | undefined): string {
  if (!value) return "none";
  let key = identityKeys.get(value);
  if (key === undefined) { key = ++nextIdentityKey; identityKeys.set(value, key); }
  return `object-${key}`;
}

/** "1 case", "2 cases", "1,234 events": a formatted count with the noun in the right number. */
export function countLabel(count: number, singular: string, plural = `${singular}s`): string {
  return `${new Intl.NumberFormat("en-US").format(count)} ${count === 1 ? singular : plural}`;
}

/** Whether an object-centric discovery result was computed from a log with no objects (nothing to show). */
export function ocelResultHasNoObjects(result: Record<string, unknown> | null | undefined): boolean {
  if (!result) return false;
  const types = result.object_types;
  return Array.isArray(types) && types.length === 0;
}
