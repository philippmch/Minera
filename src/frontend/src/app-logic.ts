import { type FilterOptions, type MinerOptions } from "./api";
import { type MineraExport } from "./types";
import {
  DECLARE_TEMPLATES,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
} from "./types";

export type Tab = "log" | "dotted" | "dfg" | "model" | "conformance" | "resources";

/**
 * Opening tab for a freshly loaded analysis: inspect the raw event log first
 * when it is present, otherwise fall back to the directly-follows graph. Exports
 * from older tool versions carry no traces, so they must not land on a log tab
 * that is never rendered.
 */
export function initialTab(data: MineraExport): Tab {
  return data.traces && data.traces.length > 0 ? "log" : "dfg";
}

/**
 * Whether any miner setting deviates from the backend defaults; drives the
 * same "active" badge as the filters. Zero is a meaningful threshold for the
 * heuristic settings, so for those only absence means default.
 */
export function hasActiveMinerOptions(options: MinerOptions): boolean {
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
      (options.heuristicDfgPreCleaningNoiseThreshold !== undefined &&
        options.heuristicDfgPreCleaningNoiseThreshold !== 0.05) ||
      (options.heuristicLoopLengthTwoThreshold !== undefined &&
        options.heuristicLoopLengthTwoThreshold !== 0.5) ||
      options.heuristicKeepOncePerCase === true ||
      Boolean(options.heuristicMostCommonPaths?.trim()) ||
      (options.heuristicStartTimestampAttribute &&
        options.heuristicStartTimestampAttribute !== "start_timestamp") ||
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
  const seen: string[] = [];
  for (const trace of data.traces ?? []) {
    for (const event of trace.events) {
      if (event.lifecycle && !seen.includes(event.lifecycle)) {
        seen.push(event.lifecycle);
      }
    }
  }
  return seen;
}

/**
 * Largest edge frequency in the log, floored at 1 so edge-weight scaling never
 * divides by zero on a log with no edges (e.g. a single-activity log).
 */
export function maxEdgeCount(data: MineraExport): number {
  return Math.max(1, ...data.edges.map((edge) => edge.count));
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
