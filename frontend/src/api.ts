import {
  type ActivityStat,
  type Alignments,
  type AlignmentOptions,
  DECLARE_TEMPLATES,
  FOOTPRINTS_CONFORMANCE_VARIANTS,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
  FOOTPRINTS_DISCOVERY_VARIANTS,
  type Decisions,
  type DecisionTablesResult,
  type DataPetriNetResult,
  type MineraExport,
  type ModelConformance,
  type DfgAlignmentOptions,
  type DfgAlignmentResult,
  type DfgPrecisionResult,
  type DfgPetriConversionOptions,
  type DfgPetriConversionResult,
  type EditDistanceOptions,
  type EditDistanceResult,
  type EvaluationOptions,
  type ModelEvaluation,
  type PetriAnalysisOptions,
  type PetriAnalysisResult,
  type EmdOptions,
  type ConceptDriftResult,
  type LogToTargetResult,
  type LogToFeaturesResult,
  type LogUtilityResult,
  type EmbeddingVariant,
  type EmbeddingApiResult,
  type LabelSplittingResult,
  type TrieResult,
  type IntervalTreeResult,
  type CaseRelationsResult,
  type LinkAnalysisResult,
  type TimestampInterleavingsResult,
  type RandomTreeResult,
  type OcelDiscoveryVariant,
  type OcelConformanceVariant,
  type OcelDiscoveryResult,
  type OcelConformanceResult,
  type OcelFilterResult,
  type OcelSqliteResult,
  type OcelExchangeFormat,
  type OcelExchangeResult,
  type OcelSplitResult,
  type OcelPlayoutResult,
  type OcelFeatureResult,
  type OcelStatisticsResult,
  type OcelDescriptionResult,
  type ProcessCubeTable,
  type ProcessCubeResult,
  type StreamingDfgResult,
  type StreamingFootprintsResult,
  type StreamingTemporalResult,
  type StreamingTbrResult,
  type TriePetriConversionResult,
  type ILPMinerOptions,
  type ILPMinerResult,
  type GeneticMinerOptions,
  type GeneticMinerResult,
  type TraceVariantQueryOptions,
  type TraceVariantQueryResult,
  type PripelOptions,
  type PripelResult,
  type OrganizationalSnaOptions,
  type OrganizationalSnaResult,
  isLogToTargetResult,
  isConceptDriftResult,
  isLogToFeaturesResult,
  isLogUtilityResult,
  isEmbeddingApiResult,
  isLabelSplittingResult,
  isTrieResult,
  isIntervalTreeResult,
  isCaseRelationsResult,
  isLinkAnalysisResult,
  isTimestampInterleavingsResult,
  isRandomTreeResult,
  isOcelDiscoveryResult,
  isOcelConformanceResult,
  isOcelFilterResult,
  isOcelSqliteResult,
  isOcelExchangeResult,
  isOcelSplitResult,
  isOcelPlayoutResult,
  isOcelFeatureResult,
  isOcelStatisticsResult,
  isOcelDescriptionResult,
  isProcessCubeResult,
  isStreamingDfgResult,
  isStreamingFootprintsResult,
  isStreamingTemporalResult,
  isStreamingTbrResult,
  isDataPetriNetResult,
  isDecisionTablesResult,
  isProcessCubeTable,
  isTriePetriConversionResult,
  isDfgPrecisionResult,
  isILPMinerResult,
  isGeneticMinerResult,
  isTraceVariantQueryResult,
  isPripelResult,
  isOrganizationalSnaResult,
  type NetworkAnalysisOptions,
  type NetworkAnalysisResult,
  isNetworkAnalysisResult,
  type RoleDiscoveryOptions,
  type RoleDiscoveryResult,
  isRoleDiscoveryResult,
  type ResourceProfilesOptions,
  type ResourceProfilesResult,
  isResourceProfilesResult,
  type LocalDiagnosticsOptions,
  type LocalDiagnosticsResult,
  isLocalDiagnosticsResult,
  type ClusteringOptions,
  type ClusteringResult,
  isClusteringResult,
  type ProfileClusteringOptions,
  type ProfileClusteringResult,
  isProfileClusteringResult,
  type EmdResult,
  type ProcessTreeAlignmentOptions,
  type ProcessTreeAlignmentResult,
  type DecomposedAlignmentOptions,
  type DecomposedAlignmentResult,
  type PetriAntiAlignmentOptions,
  type PetriAntiAlignmentResult,
  type PetriMultiAlignmentOptions,
  type PetriMultiAlignmentResult,
  type ProcessTreeReductionOptions,
  type ProcessTreeReductionResult,
  type ProcessTreePlayoutOptions,
  type StochasticPlayoutOptions,
  type MonteCarloOptions,
  type MonteCarloResult,
  type ChaoticActivitiesResult,
  type ChaoticActivitiesOptions,
  type PassedTimeResult,
  type PassedTimeOptions,
  type ServiceTimeResult,
  type ServiceTimeOptions,
  type ReworkResult,
  type ReworkOptions,
  type CycleTimeResult,
  type CycleTimeOptions,
  type CaseArrivalResult,
  type CaseArrivalOptions,
  type CaseStatisticsResult,
  type CaseStatisticsOptions,
  type AttributeStatisticsResult,
  type AttributeStatisticsOptions,
  type AttributeDistributionResult,
  type AttributeDistributionOptions,
  type AttributeKdeResult,
  type AttributeKdeOptions,
  type BoundaryActivitiesResult,
  type BoundaryActivitiesOptions,
  type VariantStatisticsResult,
  type VariantStatisticsOptions,
  type DfgPlayoutOptions,
  type PetriUsageComparison,
  isDfgAlignmentResult,
  isDfgPetriConversionResult,
  isEditDistanceResult,
  isModelEvaluation,
  isPetriAnalysisResult,
  isEmdResult,
  isProcessTreeAlignmentResult,
  isDecomposedAlignmentResult,
  isPetriAntiAlignmentResult,
  isPetriMultiAlignmentResult,
  isProcessTreeReductionResult,
  isMineraExport,
  isPetriUsageComparison,
  isMonteCarloResult,
  isChaoticActivitiesResult,
  isPassedTimeResult,
  isServiceTimeResult,
  isReworkResult,
  isCycleTimeResult,
  isCaseArrivalResult,
  isCaseStatisticsResult,
  isAttributeStatisticsResult,
  isAttributeDistributionResult,
  isAttributeKdeResult,
  isBoundaryActivitiesResult,
  isVariantStatisticsResult,
} from "./types";

export interface CsvOptions {
  caseColumn?: string;
  activityColumn?: string;
  timestampColumn?: string;
  delimiter?: string;
}

/** How the backend's time-range filter reads a trace. */
export type TimeFilterMode =
  | "events"
  | "traces_contained"
  | "traces_intersecting";

/** How the backend's attribute filter applies: to events or to whole cases. */
export type AttributeFilterMode = "events" | "traces";

/** Optional bounded temporal predicate applied after ordinary log filters. */
export type LtlVariant =
  | "eventually_follows"
  | "a_next_b_next_c"
  | "four_eyes_principle"
  | "attr_value_different_persons";

export interface LtlFilterOptions {
  ltlVariant?: LtlVariant;
  ltlAttributeKey?: string;
  ltlAttributeValues?: string[];
  ltlPositive?: boolean;
  ltlEnableTimestamp?: boolean;
  ltlTimestampDiffBoundaries?: Array<[number, number]>;
  ltlResourceKey?: string;
}

/** Log-preprocessing filters applied by the backend before discovery. */
export interface FilterOptions extends LtlFilterOptions {
  topVariants?: number;
  minVariantFrequency?: number;
  /** Cumulative variant coverage in (0, 1]: keep the most frequent variants
   * until together they cover this share of cases. */
  variantCoverage?: number;
  /** Inclusive per-variant coverage bounds and cumulative top-variant target. */
  variantMinCoverage?: number;
  variantMaxCoverage?: number;
  variantPercentage?: number;
  keepActivities?: string[];
  removeActivities?: string[];
  keepResources?: string[];
  removeResources?: string[];
  /** Lifecycle projection: e.g. keepLifecycles: ["complete"] reduces a
   * start/complete log to atomic events for discovery. */
  keepLifecycles?: string[];
  removeLifecycles?: string[];
  startActivities?: string[];
  endActivities?: string[];
  startActivitiesPositive?: boolean;
  endActivitiesPositive?: boolean;
  /** Keep only cases where this activity occurs at least reworkMin times. */
  reworkActivity?: string;
  reworkMin?: number;
  /** Succession filters, each entry an "a -> b" pair: keep (or drop) cases
   * where the first activity is directly / eventually followed by the second. */
  keepDirectlyFollows?: string[];
  removeDirectlyFollows?: string[];
  keepEventuallyFollows?: string[];
  removeEventuallyFollows?: string[];
  /** Exact/prefix/suffix/infix activity patterns; `...` matches any span. */
  tracePatterns?: string[];
  tracePatternPositive?: boolean;
  /** Reduce each case to its head up to the first prefixActivity / its tail
   * from the first suffixActivity (anchor included); cases without it drop. */
  prefixActivity?: string;
  suffixActivity?: string;
  prefixStrict?: boolean;
  prefixOccurrence?: "first" | "last";
  suffixStrict?: boolean;
  suffixOccurrence?: "first" | "last";
  /** Cut each case into the subtraces from betweenStart to betweenEnd. */
  betweenStart?: string;
  betweenEnd?: string;
  /** Split each case at one or more anchor activities. */
  activitySplit?: string[];
  activitySplitSeparator?: string;
  activitySplitCutMode?: "this" | "next";
  /** Keep whole cases whose target attribute/value count is in a range. */
  repetitionValue?: string;
  repetitionAttributeKey?: string;
  repetitionMin?: number;
  repetitionMax?: number;
  /** Collapse consecutive equal activities, retaining first or last event. */
  consecutiveGrouping?: "first" | "last";
  consecutiveGroupingActivityKey?: string;
  /** Collapse events sharing a timestamp, optionally concatenating activities. */
  timestampGrouping?: "first" | "last" | "concat";
  timestampGroupingActivityKey?: string;
  timestampGroupingTimestampKey?: string;
  timestampGroupingSeparator?: string;
  /** Optional native segment operations, applied after ordinary filters. */
  segmentPrefixActivity?: string;
  segmentPrefixStrict?: boolean;
  segmentPrefixOccurrence?: "first" | "last";
  segmentSuffixActivity?: string;
  segmentSuffixStrict?: boolean;
  segmentSuffixOccurrence?: "first" | "last";
  segmentBetweenStarts?: string[];
  segmentBetweenEnds?: string[];
  segmentBetweenSeparator?: string;
  /** Optional adjacent predecessor/successor path filter. */
  pathPredecessor?: string;
  pathSuccessor?: string;
  pathMinPerformance?: number;
  pathMaxPerformance?: number;
  pathPositive?: boolean;
  /** Keep only the first maxCases surviving cases; zero keeps none. */
  maxCases?: number;
  /** Inclusive event-count bounds for each surviving case; zero max is open. */
  minLength?: number;
  maxLength?: number;
  /** ISO-8601 bounds, both inclusive; either side may be open. */
  timeFrom?: string;
  timeTo?: string;
  timeMode?: TimeFilterMode;
  /** Inclusive complete-case duration bounds, in seconds. */
  caseDurationMin?: number;
  caseDurationMax?: number;
  /** Generic event-attribute filter: the key, its value constraints, and
   * whether it filters events or whole cases. */
  attributeKey?: string;
  keepAttributeValues?: string[];
  removeAttributeValues?: string[];
  attributeMode?: AttributeFilterMode;
  /** Inclusive numeric range over one generic event attribute. */
  numericAttributeKey?: string;
  numericAttributeMin?: number;
  numericAttributeMax?: number;
  numericAttributeMode?: AttributeFilterMode;
  numericAttributePositive?: boolean;
  /** Keep events carrying the most frequent activity/attribute values. */
  maxActivities?: number;
  topActivitiesAttributeKey?: string;
  /** Keep values occurring in at least a fraction of cases or events. */
  relativeAttributeKey?: string;
  relativeAttributeStake?: number;
  relativeAttributeKeepOncePerCase?: boolean;
  /** Keep events whose value occurs at least this absolute number of times. */
  attributeOccurrenceKey?: string;
  attributeOccurrenceMin?: number;
  /** Case-attribute filter: the key and its value constraints; always
   * keeps or drops whole cases. */
  caseAttributeKey?: string;
  keepCaseAttributeValues?: string[];
  removeCaseAttributeValues?: string[];
}

/** Discovery options for the miners themselves, applied at mine time. */
export interface MinerOptions {
  /** Inductive Miner – infrequent noise threshold in [0, 1]; 0 is classic. */
  noiseThreshold?: number;
  /** "log" splits sublogs; "imf" applies infrequent filtering; "dfg" recurses on the graph (IMd). */
  inductiveVariant?: "log" | "imf" | "dfg";
  /** "plus" (default) handles short loops; "classic" is the textbook Alpha. */
  alphaVariant?: "plus" | "classic";
  /** Heuristic Miner algorithm variant (classic by default). */
  heuristicVariant?: "classic" | "plusplus";
  /** Heuristic Miner: minimum dependency measure for an edge. */
  dependencyThreshold?: number;
  /** Heuristic Miner: minimum AND measure for a parallel split (default 0.65). */
  andThreshold?: number;
  /** Heuristic Miner: direct successions an edge needs (default 1). */
  minEdgeFrequency?: number;
  /** Heuristics Miner++: minimum retained activity occurrences (default 1). */
  heuristicMinActivityCount?: number;
  /** Heuristics-net edge decoration (frequency by default). */
  heuristicDecoration?: "frequency" | "performance";
  /** Performance edge aggregation (mean by default). */
  heuristicPerformanceAggregation?:
    | "mean"
    | "median"
    | "min"
    | "max"
    | "stdev"
    | "sum"
    | "raw_values"
    | "all";
  /** Classic DFG pre-cleaning noise threshold (default 0.05). */
  heuristicDfgPreCleaningNoiseThreshold?: number;
  /** Classic two-step-loop threshold (default 0.5). */
  heuristicLoopLengthTwoThreshold?: number;
  /** Deduplicate each classic DFG relation within a case. */
  heuristicKeepOncePerCase?: boolean;
  /** Comma-separated classic path exemptions in `A -> B` syntax. */
  heuristicMostCommonPaths?: string;
  /** Miner++ generic event attribute holding interval start timestamps. */
  heuristicStartTimestampAttribute?: string;
  /** Miner++ generic event attribute distinguishing lifecycle instances. */
  heuristicLifecycleInstanceAttribute?: string;
  /** View-based transition-system options. */
  transitionView?: "sequence" | "set" | "multiset";
  transitionDirection?: "forward" | "backward";
  transitionWindow?: number;
  transitionIncludeData?: boolean;
  /** POWL discovery algorithm variant (maximal is the default). */
  powlVariant?: "tree" | "brute_force" | "maximal" | "dynamic_clustering" | "dynamic_clustering_plain";
  powlFilteringType?: "decreasing_factor" | "dynamic";
  powlFilteringThreshold?: number;
  powlOrderFrequencyRatio?: number;
  powlBruteForceActivityLimit?: number;
  /** Reference-compatible token-replay variation used for model conformance. */
  tokenReplayVariant?: "forward" | "backwards";
  tokenReplayStopImmediatelyUnfit?: boolean;
  /** Classic log-skeleton noise threshold (default 0). */
  logSkeletonNoiseThreshold?: number;
  /** Selected log-skeleton constraint families; all six is the default. */
  logSkeletonConstraints?: string[];
  /** Classic Declare discovery controls. */
  declareConsideredActivities?: string[];
  declareAllowedTemplates?: string[];
  declareMinSupportRatio?: number;
  declareMinConfidenceRatio?: number;
  declareAutoSelectionMultiplier?: number;
  /** Aggregate footprints discovery/conformance settings. */
  footprintsDiscoveryVariants?: string[];
  footprintsModel?: "process_tree" | "powl" | "inductive" | "alpha" | "heuristic" | "dfg";
  footprintsConformanceVariants?: string[];
  footprintsStrict?: boolean;
  footprintsEnableActAlwaysExecuted?: boolean;
  /** Optional statistical and causal DFG analyses. */
  dfgStatisticalVariants?: string[];
  dfgCausalVariants?: string[];
  dfgWindow?: number;
  dfgKeepOncePerCase?: boolean;
  dfgPerformanceAggregation?:
    | "mean" | "median" | "min" | "max" | "stdev" | "sum" | "raw_values" | "all";
  dfgStartTimestampAttribute?: string;
  dfgBusinessHours?: boolean;
  dfgBusinessHourSlots?: string;
  dfgCaseAttributes?: string[];
  dfgReturnNodeAttributes?: boolean;
  /** Optional correlation-mining analyses. */
  correlationVariants?: string[];
  correlationStartTimestampAttribute?: string;
  correlationExactTimeMatching?: boolean;
  correlationSampleSize?: number;
  /** Performance spectrum (opt-in when activities are supplied). */
  spectrumActivities?: string[];
  spectrumVariant?:
    | "connected" | "disconnected" | "dataframe" | "log"
    | "dataframe_disconnected" | "log_disconnected";
  spectrumSampleSize?: number;
  spectrumSortLog?: boolean;
  spectrumActivityKey?: string;
  spectrumTimestampKey?: string;
  spectrumCaseIdKey?: string;
  /** Batch discovery (opt-in). */
  batches?: boolean;
  batchVariant?: "log" | "pandas" | "polars";
  batchMergeDistanceSeconds?: number;
  batchMinSize?: number;
  batchStartTimestampAttribute?: string;
  /** Minimum self-distance adapter selector included in the analysis export. */
  minimumSelfDistanceVariant?: "log" | "pandas" | "polars";
  /** Optional overlap statistics (interval events, cases, concurrent activities). */
  overlap?: boolean;
  overlapActivityAttribute?: string;
  overlapStartAttribute?: string;
  overlapCompletionAttribute?: string;
  overlapEpsilon?: number;
  overlapStrict?: boolean;
}

export type MineOptions = CsvOptions & FilterOptions & MinerOptions;

/**
 * The form fields a mine request sends for the given options, as name/value
 * pairs. Pure so the option-to-field mapping is unit-testable; absent or
 * default-signalling options produce no field, leaving the backend defaults
 * in charge. Activity lists are comma-joined to match the backend's parser.
 */
export function mineFormEntries(options: MineOptions): Array<[string, string]> {
  const entries: Array<[string, string]> = [];
  const add = (name: string, value: string) => entries.push([name, value]);
  const validateNonnegativeInteger = (name: string, value: number) => {
    if (!Number.isInteger(value) || value < 0)
      throw new Error(`${name} must be a non-negative integer`);
  };

  if (options.caseColumn) add("case_column", options.caseColumn);
  if (options.activityColumn) add("activity_column", options.activityColumn);
  if (options.timestampColumn) add("timestamp_column", options.timestampColumn);
  if (options.delimiter) add("delimiter", options.delimiter);

  if (options.topVariants) add("top_variants", String(options.topVariants));
  if (options.minVariantFrequency)
    add("min_variant_frequency", String(options.minVariantFrequency));
  if (options.variantCoverage)
    add("variant_coverage", String(options.variantCoverage));
  const variantCoverageValues = [
    ["variantMinCoverage", options.variantMinCoverage],
    ["variantMaxCoverage", options.variantMaxCoverage],
    ["variantPercentage", options.variantPercentage],
  ] as const;
  for (const [name, value] of variantCoverageValues) {
    if (value === undefined) continue;
    if (!Number.isFinite(value) || value < 0 || value > 1)
      throw new Error(`${name} must be finite and in [0, 1]`);
  }
  if (options.variantMinCoverage !== undefined &&
      options.variantMaxCoverage !== undefined &&
      options.variantMinCoverage > options.variantMaxCoverage)
    throw new Error("variantMinCoverage must not exceed variantMaxCoverage");
  if (options.variantMinCoverage !== undefined)
    add("variant_min_coverage", String(options.variantMinCoverage));
  if (options.variantMaxCoverage !== undefined)
    add("variant_max_coverage", String(options.variantMaxCoverage));
  if (options.variantPercentage !== undefined)
    add("variant_percentage", String(options.variantPercentage));
  if (options.keepActivities?.length)
    add("keep_activities", options.keepActivities.join(","));
  if (options.removeActivities?.length)
    add("remove_activities", options.removeActivities.join(","));
  if (options.keepResources?.length)
    add("keep_resources", options.keepResources.join(","));
  if (options.removeResources?.length)
    add("remove_resources", options.removeResources.join(","));
  if (options.keepLifecycles?.length)
    add("keep_lifecycles", options.keepLifecycles.join(","));
  if (options.removeLifecycles?.length)
    add("remove_lifecycles", options.removeLifecycles.join(","));
  if (options.startActivities?.length) {
    add("start_activities", options.startActivities.join(","));
    if (options.startActivitiesPositive !== undefined)
      add("start_activities_positive", String(options.startActivitiesPositive));
  }
  if (options.endActivities?.length) {
    add("end_activities", options.endActivities.join(","));
    if (options.endActivitiesPositive !== undefined)
      add("end_activities_positive", String(options.endActivitiesPositive));
  }
  if (options.reworkActivity) {
    add("rework_activity", options.reworkActivity);
    if (options.reworkMin)
      add("rework_min_occurrences", String(options.reworkMin));
  }
  if (options.keepDirectlyFollows?.length)
    add("keep_directly_follows", options.keepDirectlyFollows.join(","));
  if (options.removeDirectlyFollows?.length)
    add("remove_directly_follows", options.removeDirectlyFollows.join(","));
  if (options.keepEventuallyFollows?.length)
    add("keep_eventually_follows", options.keepEventuallyFollows.join(","));
  if (options.removeEventuallyFollows?.length)
    add("remove_eventually_follows", options.removeEventuallyFollows.join(","));
  if (options.tracePatterns?.length) {
    add("trace_patterns", options.tracePatterns.join(";"));
    if (options.tracePatternPositive === false)
      add("trace_pattern_positive", "false");
  }
  if (options.prefixActivity) {
    add("prefix_activity", options.prefixActivity);
    if (options.prefixStrict !== undefined)
      add("prefix_strict", String(options.prefixStrict));
    if (options.prefixOccurrence !== undefined)
      add("prefix_occurrence", options.prefixOccurrence);
  }
  if (options.suffixActivity) {
    add("suffix_activity", options.suffixActivity);
    if (options.suffixStrict !== undefined)
      add("suffix_strict", String(options.suffixStrict));
    if (options.suffixOccurrence !== undefined)
      add("suffix_occurrence", options.suffixOccurrence);
  }
  if (options.betweenStart && options.betweenEnd) {
    add("between_start", options.betweenStart);
    add("between_end", options.betweenEnd);
  }
  if (options.activitySplit?.length) {
    add("activity_split", options.activitySplit.join(","));
    if (options.activitySplitSeparator)
      add("activity_split_separator", options.activitySplitSeparator);
    if (options.activitySplitCutMode)
      add("activity_split_cut_mode", options.activitySplitCutMode);
  }
  if (options.repetitionValue !== undefined) {
    add("repetition_value", options.repetitionValue);
    if (options.repetitionAttributeKey)
      add("repetition_attribute_key", options.repetitionAttributeKey);
    if (options.repetitionMin !== undefined)
      add("repetition_min", String(options.repetitionMin));
    if (options.repetitionMax !== undefined)
      add("repetition_max", String(options.repetitionMax));
  }
  if (options.consecutiveGrouping) {
    add("consecutive_case_grouping", options.consecutiveGrouping);
    if (options.consecutiveGroupingActivityKey)
      add("consecutive_case_grouping_activity_key", options.consecutiveGroupingActivityKey);
  }
  if (options.timestampGrouping) {
    add("timestamp_case_grouping", options.timestampGrouping);
    if (options.timestampGroupingActivityKey)
      add("timestamp_case_grouping_activity_key", options.timestampGroupingActivityKey);
    if (options.timestampGroupingTimestampKey)
      add("timestamp_case_grouping_timestamp_key", options.timestampGroupingTimestampKey);
    if (options.timestampGroupingSeparator)
      add("timestamp_case_grouping_separator", options.timestampGroupingSeparator);
  }
  if (options.maxCases !== undefined) {
    validateNonnegativeInteger("maxCases", options.maxCases);
    add("max_cases", String(options.maxCases));
  }
  if (options.minLength !== undefined) {
    validateNonnegativeInteger("minLength", options.minLength);
    add("min_length", String(options.minLength));
  }
  if (options.maxLength !== undefined) {
    validateNonnegativeInteger("maxLength", options.maxLength);
    add("max_length", String(options.maxLength));
  }
  if (options.maxLength !== undefined && options.maxLength !== 0 &&
      options.minLength !== undefined && options.minLength > options.maxLength)
    throw new Error("minLength must not exceed maxLength");
  if (options.maxActivities !== undefined ||
      options.topActivitiesAttributeKey !== undefined) {
    if (options.maxActivities === undefined)
      throw new Error("maxActivities is required for top-activity filtering");
    validateNonnegativeInteger("maxActivities", options.maxActivities);
    add("max_activities", String(options.maxActivities));
    if (options.topActivitiesAttributeKey !== undefined)
      add("top_activities_attribute_key", options.topActivitiesAttributeKey);
  }
  const relativeAttributeConfigured =
    options.relativeAttributeKey !== undefined ||
    options.relativeAttributeStake !== undefined ||
    options.relativeAttributeKeepOncePerCase !== undefined;
  if (relativeAttributeConfigured) {
    if (options.relativeAttributeKey !== undefined)
      add("relative_attribute_key", options.relativeAttributeKey);
    if (options.relativeAttributeStake !== undefined) {
      if (!Number.isFinite(options.relativeAttributeStake) ||
          options.relativeAttributeStake < 0 ||
          options.relativeAttributeStake > 1)
        throw new Error("relativeAttributeStake must be finite and in [0, 1]");
      add("relative_attribute_stake", String(options.relativeAttributeStake));
    }
    if (options.relativeAttributeKeepOncePerCase !== undefined)
      add("relative_attribute_keep_once_per_case",
        String(options.relativeAttributeKeepOncePerCase));
  }
  const attributeOccurrenceConfigured =
    options.attributeOccurrenceKey !== undefined ||
    options.attributeOccurrenceMin !== undefined;
  if (attributeOccurrenceConfigured) {
    if (options.attributeOccurrenceMin === undefined)
      throw new Error("attributeOccurrenceMin is required for threshold filtering");
    validateNonnegativeInteger("attributeOccurrenceMin", options.attributeOccurrenceMin);
    if (options.attributeOccurrenceKey !== undefined)
      add("attribute_occurrence_key", options.attributeOccurrenceKey);
    add("attribute_occurrence_min", String(options.attributeOccurrenceMin));
  }
  if (options.timeFrom) add("time_from", options.timeFrom);
  if (options.timeTo) add("time_to", options.timeTo);
  if (options.timeMode && (options.timeFrom || options.timeTo))
    add("time_mode", options.timeMode);
  if (options.caseDurationMin !== undefined)
    add("case_duration_min", String(options.caseDurationMin));
  if (options.caseDurationMax !== undefined)
    add("case_duration_max", String(options.caseDurationMax));
  if (options.attributeKey) {
    add("attribute_key", options.attributeKey);
    if (options.keepAttributeValues?.length)
      add("keep_attribute_values", options.keepAttributeValues.join(","));
    if (options.removeAttributeValues?.length)
      add("remove_attribute_values", options.removeAttributeValues.join(","));
    if (options.attributeMode)
      add("attribute_mode", options.attributeMode);
  }
  if (options.caseAttributeKey) {
    add("case_attribute_key", options.caseAttributeKey);
    if (options.keepCaseAttributeValues?.length)
      add("keep_case_attribute_values", options.keepCaseAttributeValues.join(","));
    if (options.removeCaseAttributeValues?.length)
      add(
        "remove_case_attribute_values",
        options.removeCaseAttributeValues.join(","),
      );
  }
  const numericAttributeConfigured =
    options.numericAttributeKey !== undefined ||
    options.numericAttributeMin !== undefined ||
    options.numericAttributeMax !== undefined ||
    options.numericAttributeMode !== undefined ||
    options.numericAttributePositive !== undefined;
  if (numericAttributeConfigured) {
    if (options.numericAttributeKey !== undefined)
      add("numeric_attribute_key", options.numericAttributeKey);
    if (options.numericAttributeMin !== undefined) {
      if (!Number.isFinite(options.numericAttributeMin))
        throw new Error("numericAttributeMin must be finite");
      add("numeric_attribute_min", String(options.numericAttributeMin));
    }
    if (options.numericAttributeMax !== undefined) {
      if (!Number.isFinite(options.numericAttributeMax))
        throw new Error("numericAttributeMax must be finite");
      add("numeric_attribute_max", String(options.numericAttributeMax));
    }
    if (options.numericAttributeMin !== undefined &&
        options.numericAttributeMax !== undefined &&
        options.numericAttributeMin > options.numericAttributeMax)
      throw new Error("numericAttributeMin must not exceed numericAttributeMax");
    if (options.numericAttributeMode !== undefined)
      add("numeric_attribute_mode", options.numericAttributeMode);
    if (options.numericAttributePositive !== undefined)
      add("numeric_attribute_positive", String(options.numericAttributePositive));
  }

  // Miner settings. Zero is a meaningful heuristic threshold (unlike the
  // filters above, where zero means "off"), so these send whenever set.
  if (options.noiseThreshold !== undefined)
    add("noise_threshold", String(options.noiseThreshold));
  if (options.inductiveVariant) add("inductive_variant", options.inductiveVariant);
  if (options.alphaVariant) add("alpha_variant", options.alphaVariant);
  if (options.heuristicVariant && options.heuristicVariant !== "classic")
    add("heuristic_variant", options.heuristicVariant);
  if (options.dependencyThreshold !== undefined)
    add("dependency_threshold", String(options.dependencyThreshold));
  if (options.andThreshold !== undefined)
    add("and_threshold", String(options.andThreshold));
  if (options.minEdgeFrequency !== undefined)
    add("min_edge_frequency", String(options.minEdgeFrequency));
  if (options.heuristicMinActivityCount !== undefined)
    add("heuristic_min_activity_count", String(options.heuristicMinActivityCount));
  if (options.heuristicDecoration && options.heuristicDecoration !== "frequency")
    add("heuristic_decoration", options.heuristicDecoration);
  if (
    options.heuristicPerformanceAggregation &&
    options.heuristicPerformanceAggregation !== "mean"
  )
    add(
      "heuristic_performance_aggregation",
      options.heuristicPerformanceAggregation,
    );
  if (
    options.heuristicDfgPreCleaningNoiseThreshold !== undefined &&
    options.heuristicDfgPreCleaningNoiseThreshold !== 0.05
  )
    add(
      "heuristic_dfg_pre_cleaning_noise_threshold",
      String(options.heuristicDfgPreCleaningNoiseThreshold),
    );
  if (
    options.heuristicLoopLengthTwoThreshold !== undefined &&
    options.heuristicLoopLengthTwoThreshold !== 0.5
  )
    add(
      "heuristic_loop_length_two_threshold",
      String(options.heuristicLoopLengthTwoThreshold),
    );
  if (options.heuristicKeepOncePerCase)
    add("heuristic_keep_once_per_case", "true");
  if (options.heuristicMostCommonPaths?.trim())
    add("heuristic_most_common_paths", options.heuristicMostCommonPaths.trim());
  if (
    options.heuristicStartTimestampAttribute &&
    options.heuristicStartTimestampAttribute !== "start_timestamp"
  )
    add(
      "heuristic_start_timestamp_attribute",
      options.heuristicStartTimestampAttribute,
    );
  if (
    options.heuristicLifecycleInstanceAttribute &&
    options.heuristicLifecycleInstanceAttribute !== "concept:instance"
  )
    add(
      "heuristic_lifecycle_instance_attribute",
      options.heuristicLifecycleInstanceAttribute,
    );
  if (options.transitionView && options.transitionView !== "sequence")
    add("transition_view", options.transitionView);
  if (options.transitionDirection && options.transitionDirection !== "forward")
    add("transition_direction", options.transitionDirection);
  if (options.transitionWindow !== undefined && options.transitionWindow !== 2)
    add("transition_window", String(options.transitionWindow));
  if (options.transitionIncludeData) add("transition_include_data", "true");
  if (options.powlVariant && options.powlVariant !== "maximal")
    add("powl_variant", options.powlVariant);
  if (options.powlFilteringType && options.powlFilteringType !== "decreasing_factor")
    add("powl_filtering_type", options.powlFilteringType);
  if (options.powlFilteringThreshold !== undefined && options.powlFilteringThreshold >= 0 && options.powlFilteringThreshold < 1 && options.powlFilteringThreshold !== 0)
    add("powl_filtering_threshold", String(options.powlFilteringThreshold));
  if (options.powlOrderFrequencyRatio !== undefined && options.powlOrderFrequencyRatio > 0.5 && options.powlOrderFrequencyRatio <= 1 && options.powlOrderFrequencyRatio !== 1)
    add("powl_order_frequency_ratio", String(options.powlOrderFrequencyRatio));
  if (options.powlBruteForceActivityLimit !== undefined && Number.isInteger(options.powlBruteForceActivityLimit) && options.powlBruteForceActivityLimit >= 0 && options.powlBruteForceActivityLimit !== 0)
    add("powl_brute_force_activity_limit", String(options.powlBruteForceActivityLimit));
  if (options.tokenReplayVariant && options.tokenReplayVariant !== "forward")
    add("token_replay_variant", options.tokenReplayVariant);
  if (options.tokenReplayStopImmediatelyUnfit)
    add("token_replay_stop_immediately_unfit", "true");
  if (
    options.logSkeletonNoiseThreshold !== undefined &&
    Number.isFinite(options.logSkeletonNoiseThreshold) &&
    options.logSkeletonNoiseThreshold >= 0 &&
    options.logSkeletonNoiseThreshold <= 1 &&
    options.logSkeletonNoiseThreshold !== 0
  ) {
    add("log_skeleton_noise_threshold", String(options.logSkeletonNoiseThreshold));
  }
  if (options.segmentPrefixActivity) {
    add("segment_prefix_activity", options.segmentPrefixActivity);
    if (options.segmentPrefixStrict !== undefined)
      add("segment_prefix_strict", String(options.segmentPrefixStrict));
    if (options.segmentPrefixOccurrence)
      add("segment_prefix_occurrence", options.segmentPrefixOccurrence);
  }
  if (options.segmentSuffixActivity) {
    add("segment_suffix_activity", options.segmentSuffixActivity);
    if (options.segmentSuffixStrict !== undefined)
      add("segment_suffix_strict", String(options.segmentSuffixStrict));
    if (options.segmentSuffixOccurrence)
      add("segment_suffix_occurrence", options.segmentSuffixOccurrence);
  }
  if (options.segmentBetweenStarts?.length && options.segmentBetweenEnds?.length) {
    add("segment_between_starts", options.segmentBetweenStarts.join(","));
    add("segment_between_ends", options.segmentBetweenEnds.join(","));
    if (options.segmentBetweenSeparator)
      add("segment_between_separator", options.segmentBetweenSeparator);
  }
  if (options.pathPredecessor || options.pathSuccessor ||
      options.pathMinPerformance !== undefined ||
      options.pathMaxPerformance !== undefined ||
      options.pathPositive !== undefined) {
    if (options.pathPredecessor) add("path_predecessor", options.pathPredecessor);
    if (options.pathSuccessor) add("path_successor", options.pathSuccessor);
    if (options.pathPredecessor && options.pathSuccessor) {
      if (options.pathMinPerformance !== undefined)
        add("path_min_performance", String(options.pathMinPerformance));
      if (options.pathMaxPerformance !== undefined)
        add("path_max_performance", String(options.pathMaxPerformance));
      if (options.pathPositive !== undefined)
        add("path_positive", String(options.pathPositive));
    }
  }
  if (options.ltlVariant && options.ltlAttributeValues?.length) {
    add("ltl_variant", options.ltlVariant);
    add("ltl_attribute_values", options.ltlAttributeValues.join(","));
    if (options.ltlAttributeKey) add("ltl_attribute_key", options.ltlAttributeKey);
    if (options.ltlPositive !== undefined) add("ltl_positive", String(options.ltlPositive));
    if (options.ltlEnableTimestamp !== undefined)
      add("ltl_enable_timestamp", String(options.ltlEnableTimestamp));
    if (options.ltlTimestampDiffBoundaries?.length)
      add("ltl_timestamp_diff_boundaries", options.ltlTimestampDiffBoundaries.map(([lo, hi]) => `${lo}:${hi}`).join(","));
    if (options.ltlResourceKey) add("ltl_resource_key", options.ltlResourceKey);
  }
  const allLogSkeletonConstraints = [
    "equivalence",
    "always_after",
    "always_before",
    "never_together",
    "directly_follows",
    "activ_freq",
  ];
  const selectedLogSkeletonConstraints = options.logSkeletonConstraints?.filter(
    (name) => allLogSkeletonConstraints.includes(name),
  );
  if (
    selectedLogSkeletonConstraints &&
    selectedLogSkeletonConstraints.length > 0 &&
    (selectedLogSkeletonConstraints.length !== allLogSkeletonConstraints.length ||
      allLogSkeletonConstraints.some((name) => !selectedLogSkeletonConstraints.includes(name)))
  ) {
    add("log_skeleton_constraints", selectedLogSkeletonConstraints.join(","));
  }

  const selectedDeclareTemplates = options.declareAllowedTemplates?.filter(
    (name) => (DECLARE_TEMPLATES as readonly string[]).includes(name),
  );
  if (options.declareConsideredActivities?.length)
    add("declare_considered_activities", options.declareConsideredActivities.join(","));
  if (
    selectedDeclareTemplates && selectedDeclareTemplates.length > 0 &&
    (selectedDeclareTemplates.length !== DECLARE_TEMPLATES.length ||
      DECLARE_TEMPLATES.some((name) => !selectedDeclareTemplates.includes(name)))
  ) add("declare_allowed_templates", selectedDeclareTemplates.join(","));
  const declareRatio = (value: number | undefined) =>
    value !== undefined && Number.isFinite(value) && value >= 0 && value <= 1
      ? String(value)
      : undefined;
  const support = declareRatio(options.declareMinSupportRatio);
  const confidence = declareRatio(options.declareMinConfidenceRatio);
  const multiplier = declareRatio(options.declareAutoSelectionMultiplier);
  if (support !== undefined) add("declare_min_support_ratio", support);
  if (confidence !== undefined) add("declare_min_confidence_ratio", confidence);
  if (
    multiplier !== undefined && Number(multiplier) !== 0.8 &&
    options.declareMinSupportRatio === undefined &&
    options.declareMinConfidenceRatio === undefined
  )
    add("declare_auto_selection_multiplier", multiplier);

  const selectedFootprintsDiscovery = options.footprintsDiscoveryVariants
    ? FOOTPRINTS_DISCOVERY_VARIANTS.filter(
        (name) =>
          options.footprintsDiscoveryVariants?.includes(name) &&
          !(options.footprintsModel === "dfg" && name === "petri_reach_graph"),
      )
    : undefined;
  if (
    selectedFootprintsDiscovery && selectedFootprintsDiscovery.length > 0 &&
    (selectedFootprintsDiscovery.length !== FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length ||
      FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.some((name) => !selectedFootprintsDiscovery.includes(name)))
  ) add("footprints_discovery_variants", selectedFootprintsDiscovery.join(","));
  if (options.footprintsModel && options.footprintsModel !== "process_tree")
    add("footprints_model", options.footprintsModel);
  const selectedFootprintsConformance = options.footprintsConformanceVariants
    ? FOOTPRINTS_CONFORMANCE_VARIANTS.filter((name) => options.footprintsConformanceVariants?.includes(name))
    : undefined;
  if (
    selectedFootprintsConformance && selectedFootprintsConformance.length > 0 &&
    (selectedFootprintsConformance.length !== 1 || selectedFootprintsConformance[0] !== "log_model")
  ) add("footprints_conformance_variants", selectedFootprintsConformance.join(","));
  if (options.footprintsStrict) add("footprints_strict", "true");
  if (options.footprintsEnableActAlwaysExecuted === false)
    add("footprints_enable_act_always_executed", "false");

  const dfgStatisticalNames = [
    "native", "frequency", "frequency_greedy", "frequency_triples",
    "performance", "performance_greedy", "case_attributes", "clean",
    "clean_polars", "clean_time",
  ];
  const dfgCausalNames = ["alpha", "heuristic"];
  const selectedDfgStatistical = options.dfgStatisticalVariants
    ? dfgStatisticalNames.filter((name) => options.dfgStatisticalVariants?.includes(name))
    : [];
  const selectedDfgCausal = options.dfgCausalVariants
    ? dfgCausalNames.filter((name) => options.dfgCausalVariants?.includes(name))
    : [];
  const hasDfgFrequency =
    selectedDfgCausal.length > 0 ||
    selectedDfgStatistical.some((name) =>
      ["native", "frequency", "frequency_greedy"].includes(name));
  const hasDfgPerformance = selectedDfgStatistical.some((name) =>
    ["performance", "performance_greedy"].includes(name));
  const hasDfgCaseAttributes =
    selectedDfgStatistical.includes("case_attributes");
  if (selectedDfgStatistical.length)
    add("dfg_statistical_variants", selectedDfgStatistical.join(","));
  if (selectedDfgCausal.length)
    add("dfg_causal_variants", selectedDfgCausal.join(","));
  if (hasDfgFrequency) {
    if (options.dfgWindow !== undefined && Number.isInteger(options.dfgWindow) && options.dfgWindow >= 0 && options.dfgWindow !== 1)
      add("dfg_window", String(options.dfgWindow));
    if (options.dfgKeepOncePerCase) add("dfg_keep_once_per_case", "true");
  }
  if (hasDfgPerformance) {
    const aggregations = ["mean", "median", "min", "max", "stdev", "sum", "raw_values", "all"];
    if (options.dfgPerformanceAggregation && aggregations.includes(options.dfgPerformanceAggregation) && options.dfgPerformanceAggregation !== "mean")
      add("dfg_performance_aggregation", options.dfgPerformanceAggregation);
    if (options.dfgStartTimestampAttribute?.trim() && options.dfgStartTimestampAttribute.trim() !== "start_timestamp")
      add("dfg_start_timestamp_attribute", options.dfgStartTimestampAttribute.trim());
    if (options.dfgBusinessHours) add("dfg_business_hours", "true");
    const businessHourSlots = options.dfgBusinessHourSlots?.trim();
    if (options.dfgBusinessHours && businessHourSlots) {
      const validSlots = businessHourSlots.split(",").every((slot) => {
        const match = slot.trim().match(
          /^(\d+(?:\.\d+)?)-(\d+(?:\.\d+)?)$/,
        );
        if (!match) return false;
        const start = Number(match[1]);
        const end = Number(match[2]);
        return Number.isFinite(start) && Number.isFinite(end) &&
          start >= 0 && start < end && end <= 604800;
      });
      if (validSlots) add("dfg_business_hour_slots", businessHourSlots);
    }
  }
  if (hasDfgCaseAttributes) {
    const caseAttributes = options.dfgCaseAttributes?.map((name) => name.trim()).filter(Boolean);
    if (caseAttributes?.length && !(caseAttributes.length === 1 && caseAttributes[0] === "concept:name"))
      add("dfg_case_attributes", caseAttributes.join(","));
    if (options.dfgReturnNodeAttributes) add("dfg_return_node_attributes", "true");
  }

  const correlationNames = ["classic", "classic_split", "trace_based"];
  const selectedCorrelation = options.correlationVariants
    ? correlationNames.filter((name) =>
        options.correlationVariants?.includes(name))
    : [];
  if (selectedCorrelation.length) {
    add("correlation_variants", selectedCorrelation.join(","));
    const startAttribute = options.correlationStartTimestampAttribute?.trim();
    if (startAttribute) {
      add("correlation_start_timestamp_attribute", startAttribute);
    }
    const exactCapable =
      selectedCorrelation.includes("classic") ||
      selectedCorrelation.includes("classic_split");
    if (exactCapable && options.correlationExactTimeMatching) {
      add("correlation_exact_time_matching", "true");
    }
    if (
      selectedCorrelation.includes("classic_split") &&
      options.correlationSampleSize !== undefined &&
      Number.isInteger(options.correlationSampleSize) &&
      options.correlationSampleSize > 0 &&
      options.correlationSampleSize !== 100000
    ) {
      add("correlation_sample_size", String(options.correlationSampleSize));
    }
  }

  // Temporal/behavioural analyses are opt-in. Dependent settings are sent
  // only while their analysis is active, and defaults stay on the backend.
  const spectrumActivities = options.spectrumActivities?.map((name) => name.trim()).filter(Boolean);
  if (spectrumActivities && spectrumActivities.length >= 2) {
    add("spectrum_activities", spectrumActivities.join(","));
    if (options.spectrumVariant && options.spectrumVariant !== "connected")
      add("spectrum_variant", options.spectrumVariant);
    if (options.spectrumSampleSize !== undefined && Number.isInteger(options.spectrumSampleSize) && options.spectrumSampleSize > 0 && options.spectrumSampleSize !== 10000)
      add("spectrum_sample_size", String(options.spectrumSampleSize));
    if (options.spectrumSortLog === false) add("spectrum_sort_log", "false");
    if (options.spectrumActivityKey?.trim())
      add("spectrum_activity_key", options.spectrumActivityKey.trim());
    if (options.spectrumTimestampKey?.trim())
      add("spectrum_timestamp_key", options.spectrumTimestampKey.trim());
    if (options.spectrumCaseIdKey?.trim())
      add("spectrum_case_id_key", options.spectrumCaseIdKey.trim());
  }
  if (options.batches) {
    add("batches", "true");
    if (options.batchVariant && options.batchVariant !== "log")
      add("batch_variant", options.batchVariant);
    if (options.batchMergeDistanceSeconds !== undefined && Number.isFinite(options.batchMergeDistanceSeconds) && options.batchMergeDistanceSeconds >= 0 && options.batchMergeDistanceSeconds !== 900)
      add("batch_merge_distance_seconds", String(options.batchMergeDistanceSeconds));
    if (options.batchMinSize !== undefined && Number.isInteger(options.batchMinSize) && options.batchMinSize >= 2 && options.batchMinSize !== 2)
      add("batch_min_size", String(options.batchMinSize));
    if (options.batchStartTimestampAttribute?.trim())
      add("batch_start_timestamp_attribute", options.batchStartTimestampAttribute.trim());
  }
  if (options.minimumSelfDistanceVariant &&
      options.minimumSelfDistanceVariant !== "log") {
    add("minimum_self_distance_variant", options.minimumSelfDistanceVariant);
  }
  if (options.overlap) {
    add("overlap", "true");
    if (options.overlapActivityAttribute?.trim())
      add("overlap_activity_attribute", options.overlapActivityAttribute.trim());
    if (options.overlapStartAttribute?.trim())
      add("overlap_start_attribute", options.overlapStartAttribute.trim());
    if (options.overlapCompletionAttribute?.trim())
      add("overlap_completion_attribute", options.overlapCompletionAttribute.trim());
    if (options.overlapEpsilon !== undefined && Number.isFinite(options.overlapEpsilon) && options.overlapEpsilon >= 0)
      add("overlap_epsilon", String(options.overlapEpsilon));
    if (options.overlapStrict) add("overlap_strict", "true");
  }

  return entries;
}

/** Uploads a CSV event log to the local Minera backend for mining. */
export async function mineCsv(
  file: File,
  options: MineOptions = {},
): Promise<MineraExport> {
  const form = new FormData();
  form.append("log", file, file.name);
  for (const [name, value] of mineFormEntries(options)) form.append(name, value);

  let response: Response;
  try {
    response = await fetch("/api/mine", { method: "POST", body: form });
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isMineraExport(parsed)) {
    throw new Error("the Minera backend returned an invalid analysis");
  }
  return parsed;
}

/**
 * Asks the backend to align the most recently mined log against one miner's
 * model. The server defers this costlier computation to here so a mine never
 * pays for alignments the user may not open.
 */
/** Stable query serialization used by the alignment cache and request API. */
export function alignmentQuery(options: AlignmentOptions = {}): string {
  const query = new URLSearchParams();
  const add = (name: string, value: unknown) => {
    if (value !== undefined && value !== null && value !== "") query.set(name, String(value));
  };
  add("alignment_variant", options.variant);
  add("max_states", options.max_states);
  add("max_align_time", options.max_align_time);
  add("max_align_time_trace", options.max_align_time_trace);
  add("max_alignments", options.max_alignments);
  add("exponent", options.exponent);
  if (options.synchronous_dijkstra !== undefined) add("synchronous_dijkstra", options.synchronous_dijkstra);
  add("cost_profile", options.cost_profile);
  add("trace_cost", options.trace_cost);
  add("model_cost", options.model_cost);
  add("silent_model_cost", options.silent_model_cost);
  add("sync_cost", options.sync_cost);
  return query.toString();
}

export async function alignModel(miner: string, options: AlignmentOptions = {}): Promise<Alignments> {
  let response: Response;
  try {
    const query = alignmentQuery(options);
    const params = [`miner=${encodeURIComponent(miner)}`, query].filter(Boolean).join("&");
    response = await fetch(`/api/align?${params}`, {
      method: "POST",
    });
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (parsed === null || typeof parsed !== "object" || !("computed" in parsed)) {
    throw new Error("the Minera backend returned an invalid alignment");
  }
  return parsed as Alignments;
}

function escapeCostActivity(name: string): string {
  let escaped = "";
  for (const char of name) {
    if (char === " ") escaped += "\\s";
    else if (char === "\t") escaped += "\\t";
    else if (char === "\n") escaped += "\\n";
    else if (char === "\r") escaped += "\\r";
    else if ("\\,=:".includes(char)) escaped += `\\${char}`;
    else escaped += char;
  }
  return escaped;
}

/** Parse the escaped activity=cost syntax used by DFG cost controls. */
export function parseDfgCostMapInput(value: string): Record<string, number> | undefined {
  if (!value.trim()) return undefined;
  const items: string[] = [];
  let begin = 0;
  let escaped = false;
  for (let i = 0; i <= value.length; i += 1) {
    if (i < value.length && escaped) { escaped = false; continue; }
    if (i < value.length && value[i] === "\\") { escaped = true; continue; }
    if (i === value.length || value[i] === ",") {
      const item = value.slice(begin, i).trim();
      if (!item) throw new Error("cost maps must contain activity=value pairs");
      items.push(item);
      begin = i + 1;
    }
  }
  if (escaped) throw new Error("cost-map activity has a dangling escape");

  const result: Record<string, number> = {};
  for (const item of items) {
    let separator = -1;
    escaped = false;
    for (let i = 0; i < item.length; i += 1) {
      if (escaped) { escaped = false; continue; }
      if (item[i] === "\\") { escaped = true; continue; }
      if (item[i] === "=" || item[i] === ":") { separator = i; break; }
    }
    if (separator <= 0 || separator + 1 >= item.length)
      throw new Error(`invalid cost entry "${item}"; expected activity=value`);
    let activity = "";
    for (let i = 0; i < separator; i += 1) {
      if (item[i] !== "\\") { activity += item[i]; continue; }
      i += 1;
      if (i >= separator || !"\\,=:stnr".includes(item[i]))
        throw new Error(`invalid escape in cost activity "${item.slice(0, separator)}"`);
      activity += item[i] === "s" ? " " : item[i] === "t" ? "\t" : item[i] === "n" ? "\n" : item[i] === "r" ? "\r" : item[i];
    }
    const rawCost = item.slice(separator + 1).trim();
    const cost = Number(rawCost);
    if (!activity || activity === ">>" || !rawCost || !Number.isFinite(cost) || cost < 0)
      throw new Error(`invalid cost entry "${item}"; costs must be finite and non-negative`);
    result[activity] = cost;
  }
  return result;
}

/** Parse comma-separated non-negative marking place ids. */
export function parseMarkingInput(value: string): number[] | undefined {
  if (!value.trim()) return undefined;
  const entries = value.split(",").map((part) => part.trim());
  if (entries.some((part) => !/^\d+$/.test(part))) throw new Error("markings must be comma-separated non-negative integers");
  return entries.map((part) => Number(part));
}

function encodeCostMap(map?: Record<string, number>): string | undefined {
  if (!map) return undefined;
  const entries = Object.entries(map).sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0);
  for (const [name, value] of entries) {
    if (!name || name === ">>" || !Number.isFinite(value) || value < 0)
      throw new Error("DFG cost maps require non-gap activities and finite non-negative costs");
  }
  return entries.length ? entries.map(([name, value]) => `${escapeCostActivity(name)}=${value}`).join(",") : undefined;
}

/** Deterministic query serialization for classic DFG alignment requests. */
export function dfgAlignmentQuery(options: DfgAlignmentOptions = {}): string {
  const q = new URLSearchParams();
  const add = (name: string, value: unknown) => { if (value !== undefined && value !== null && value !== "") q.set(name, String(value)); };
  add("dfg_variant", options.variant);
  const maxStates = options.max_states;
  const maxTimeMs = options.max_time_ms;
  if (typeof maxStates === "number" && Number.isInteger(maxStates) && maxStates > 0) add("dfg_max_states", maxStates);
  if (typeof maxTimeMs === "number" && Number.isInteger(maxTimeMs) && maxTimeMs > 0) add("dfg_max_time_ms", maxTimeMs);
  add("dfg_sync_cost", encodeCostMap(options.sync_cost));
  add("dfg_model_move_cost", encodeCostMap(options.model_move_cost));
  add("dfg_log_move_cost", encodeCostMap(options.log_move_cost));
  add("dfg_internal_log_move_cost", encodeCostMap(options.internal_log_move_cost));
  add("projection", options.projection);
  return q.toString();
}

export async function alignDfg(options: DfgAlignmentOptions = {}): Promise<DfgAlignmentResult> {
  let response: Response;
  try { response = await fetch(`/api/align-dfg${dfgAlignmentQuery(options) ? `?${dfgAlignmentQuery(options)}` : ""}`, { method: "POST" }); }
  catch { throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isDfgAlignmentResult(parsed)) throw new Error("the Minera backend returned an invalid DFG alignment");
  return parsed;
}

/** Stable form serialization for edit-distance alignment controls. */
export function editDistanceFormEntries(options: EditDistanceOptions = {}): Array<[string, string]> {
  const entries: Array<[string, string]> = [];
  if (options.variant) entries.push(["edit_variant", options.variant]);
  if (options.anti !== undefined) entries.push(["anti", String(options.anti)]);
  const deviationCost = options.deviation_cost;
  if (typeof deviationCost === "number" && Number.isInteger(deviationCost) && deviationCost > 0) entries.push(["deviation_cost", String(deviationCost)]);
  if (options.projection) entries.push(["projection", options.projection]);
  return entries;
}

export async function alignEditDistance(reference: File, options: EditDistanceOptions = {}): Promise<EditDistanceResult> {
  const form = new FormData();
  form.append("reference_log", reference, reference.name);
  for (const [name, value] of editDistanceFormEntries(options)) form.append(name, value);
  let response: Response;
  try { response = await fetch("/api/align-edit-distance", { method: "POST", body: form }); }
  catch { throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isEditDistanceResult(parsed)) throw new Error("the Minera backend returned an invalid edit-distance alignment");
  return parsed;
}

/** Stable, validated query serialization for one model-quality evaluation. */
export function evaluationQuery(options: EvaluationOptions = {}): string {
  const family = options.evaluation_family ?? "replay_fitness";
  if (!["replay_fitness", "precision", "generalization", "simplicity"].includes(family)) {
    throw new Error("evaluation_family is invalid");
  }
  if (options.fitness_variant !== undefined && !["auto", "token_based", "alignment_based"].includes(options.fitness_variant)) {
    throw new Error("fitness_variant is invalid");
  }
  if (options.precision_variant !== undefined && !["auto", "etconformance_token", "align_etconformance", "automaton_after_align"].includes(options.precision_variant)) {
    throw new Error("precision_variant is invalid");
  }
  if (options.generalization_variant !== undefined && options.generalization_variant !== "generalization_token") {
    throw new Error("generalization_variant is invalid");
  }
  if (options.simplicity_variant !== undefined && !["arc_degree", "extended_cardoso", "extended_cyclomatic"].includes(options.simplicity_variant)) {
    throw new Error("simplicity_variant is invalid");
  }
  if (options.precision_use_task_ids !== undefined && typeof options.precision_use_task_ids !== "boolean") {
    throw new Error("precision_use_task_ids must be boolean");
  }
  for (const [name, value] of [
    ["precision_max_states", options.precision_max_states],
    ["simplicity_max_states", options.simplicity_max_states],
  ] as const) {
    if (value !== undefined && (!Number.isInteger(value) || value < 1)) throw new Error(`${name} must be a positive integer`);
  }
  if (options.simplicity_arc_degree_k !== undefined && (!Number.isFinite(options.simplicity_arc_degree_k) || options.simplicity_arc_degree_k < 0)) {
    throw new Error("simplicity_arc_degree_k must be a finite non-negative number");
  }
  for (const [name, value] of [
    ["simplicity_initial_marking", options.simplicity_initial_marking],
    ["simplicity_final_marking", options.simplicity_final_marking],
  ] as const) {
    if (value !== undefined && (value.length === 0 || value.some((item) => !Number.isInteger(item) || item < 0))) {
      throw new Error(`${name} must contain non-negative integers`);
    }
  }

  const query = new URLSearchParams();
  query.set("evaluation_family", family);
  const add = (name: string, value: unknown) => { if (value !== undefined) query.set(name, Array.isArray(value) ? value.join(",") : String(value)); };
  add("fitness_variant", options.fitness_variant);
  add("precision_variant", options.precision_variant);
  add("precision_use_task_ids", options.precision_use_task_ids);
  add("precision_max_states", options.precision_max_states);
  add("generalization_variant", options.generalization_variant);
  add("simplicity_variant", options.simplicity_variant);
  add("simplicity_arc_degree_k", options.simplicity_arc_degree_k);
  add("simplicity_max_states", options.simplicity_max_states);
  add("simplicity_initial_marking", options.simplicity_initial_marking);
  add("simplicity_final_marking", options.simplicity_final_marking);
  return query.toString();
}

/** Runs one selected quality evaluator against a cached model. */
export async function evaluateModel(miner: string, options: EvaluationOptions = {}): Promise<ModelEvaluation> {
  const query = new URLSearchParams(evaluationQuery(options));
  query.set("miner", miner);
  return postChecked(`/api/evaluate?${query.toString()}`, isModelEvaluation, "model evaluation");
}

/** Stable query serialization for the native Petri-analysis families. */
export function petriAnalysisQuery(options: PetriAnalysisOptions = {}): string {
  const family = options.family ?? "marking_equation";
  if (!["marking_equation", "extended_marking_equation", "workflow_net", "soundness", "woflan"].includes(family)) {
    throw new Error("petri analysis family is invalid");
  }
  if (options.miner !== undefined && options.miner.trim() === "") throw new Error("petri analysis miner must not be empty");
  for (const [name, value] of [
    ["trace_index", options.trace_index], ["max_k", options.max_k], ["max_states", options.max_states],
  ] as const) {
    if (value !== undefined && (!Number.isInteger(value) || value < (name === "max_states" ? 1 : 0))) {
      throw new Error(`${name} must be a valid non-negative integer`);
    }
  }
  for (const [name, value] of [
    ["initial_marking", options.initial_marking], ["final_marking", options.final_marking],
    ["split_indices", options.split_indices],
  ] as const) {
    if (value !== undefined && (value.length === 0 || value.some((item) => !Number.isFinite(item) || !Number.isInteger(item) || item < 0))) {
      throw new Error(`${name} must contain non-negative finite integers`);
    }
  }
  for (const [name, value] of [["transition_costs", options.transition_costs], ["model_costs", options.model_costs], ["sync_costs", options.sync_costs], ["log_costs", options.log_costs]] as const) {
    if (value !== undefined && value.some((item) => !Number.isFinite(item) || item < 0)) throw new Error(`${name} must contain non-negative finite costs`);
  }
  if (options.full_bootstrap !== undefined && typeof options.full_bootstrap !== "boolean") throw new Error("full_bootstrap must be boolean");
  if (options.diagnostics !== undefined && typeof options.diagnostics !== "boolean") throw new Error("diagnostics must be boolean");
  if (options.return_asap_when_unsound !== undefined && typeof options.return_asap_when_unsound !== "boolean") throw new Error("return_asap_when_unsound must be boolean");
  const query = new URLSearchParams();
  const add = (name: string, value: unknown) => { if (value !== undefined) query.set(`petri_analysis_${name}`, Array.isArray(value) ? value.join(",") : String(value)); };
  add("family", family);
  add("miner", options.miner);
  add("trace_index", options.trace_index);
  add("initial_marking", options.initial_marking);
  add("final_marking", options.final_marking);
  add("transition_costs", options.transition_costs);
  add("model_costs", options.model_costs);
  add("sync_costs", options.sync_costs);
  add("log_costs", options.log_costs);
  add("split_indices", options.split_indices);
  add("max_k", options.max_k);
  add("max_states", options.max_states);
  add("full_bootstrap", options.full_bootstrap);
  add("diagnostics", options.diagnostics);
  add("return_asap_when_unsound", options.return_asap_when_unsound);
  return query.toString();
}

/** Runs one selected Petri-analysis family against a cached model. */
export async function analyzePetriModel(options: PetriAnalysisOptions = {}): Promise<PetriAnalysisResult> {
  const query = petriAnalysisQuery(options);
  return postChecked(`/api/petri-analysis?${query}`, isPetriAnalysisResult, "Petri analysis");
}

/** Stable query serialization for stochastic-language distance controls. */
export function emdQuery(options: EmdOptions = {}): string {
  for (const [name, value] of Object.entries(options)) {
    if (typeof value !== "boolean") throw new Error(`${name} must be boolean`);
  }
  const query = new URLSearchParams();
  if (options.normalize_masses !== undefined) query.set("normalize_masses", String(options.normalize_masses));
  if (options.strict_equal_mass !== undefined) query.set("strict_equal_mass", String(options.strict_equal_mass));
  return query.toString();
}

/** Compares the cached log's stochastic language with an uploaded reference log. */
export async function evaluateEmd(reference: File, options: EmdOptions = {}): Promise<EmdResult> {
  const form = new FormData();
  form.append("reference_log", reference, reference.name);
  const query = emdQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/evaluate-emd${query ? `?${query}` : ""}`, { method: "POST", body: form });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isEmdResult(parsed)) throw new Error("the Minera backend returned an invalid stochastic-language distance");
  return parsed;
}

export const DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS: Required<ProcessTreeAlignmentOptions> = {
  variant: "SEARCH_GRAPH_PT",
  max_states: 0,
  max_time_ms: 0,
  max_total_time_ms: 0,
  use_global_cache: false,
  batch_size: 1000,
  clear_cache: true,
  max_trace_length: 1,
  max_process_tree_height: 1,
  max_trace_time_ms: 0,
  max_per_trace_time_ms: 0,
};

function nonNegativeInteger(value: number | undefined, name: string): boolean {
  if (value === undefined) return true;
  if (!Number.isInteger(value) || value < 0) throw new Error(`${name} must be a non-negative integer`);
  return true;
}

function nonNegativeTime(value: number | undefined, name: string): boolean {
  if (value === undefined) return true;
  if (!Number.isFinite(value) || value < 0) throw new Error(`${name} must be a non-negative finite number`);
  return true;
}

/** Stable query serialization for process-tree alignment requests. */
export function processTreeAlignmentQuery(options: ProcessTreeAlignmentOptions = {}): string {
  const variant = options.variant ?? DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.variant;
  if (!["SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP", "APPROXIMATED_ORIGINAL"].includes(variant)) {
    throw new Error("pt_alignment_variant is invalid");
  }
  nonNegativeInteger(options.max_states, "pt_alignment_max_states");
  nonNegativeTime(options.max_time_ms, "pt_alignment_max_time_ms");
  nonNegativeTime(options.max_total_time_ms, "pt_alignment_max_total_time_ms");
  nonNegativeInteger(options.max_trace_length, "pt_alignment_max_trace_length");
  nonNegativeInteger(options.max_process_tree_height, "pt_alignment_max_process_tree_height");
  nonNegativeTime(options.max_trace_time_ms, "pt_alignment_max_trace_time_ms");
  nonNegativeTime(options.max_per_trace_time_ms, "pt_alignment_max_per_trace_time_ms");
  if (options.batch_size !== undefined && (!Number.isInteger(options.batch_size) || options.batch_size < 1)) {
    throw new Error("pt_alignment_batch_size must be a positive integer");
  }
  if (options.use_global_cache !== undefined && typeof options.use_global_cache !== "boolean") throw new Error("pt_alignment_use_global_cache must be boolean");
  if (options.clear_cache !== undefined && typeof options.clear_cache !== "boolean") throw new Error("pt_alignment_clear_cache must be boolean");

  const query = new URLSearchParams();
  const add = (name: string, value: unknown) => { if (value !== undefined && value !== null && value !== "") query.set(name, String(value)); };
  if (variant !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.variant) add("pt_alignment_variant", variant);
  if (options.max_states !== undefined && options.max_states !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_states) add("pt_alignment_max_states", options.max_states);
  if (options.max_time_ms !== undefined && options.max_time_ms !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_time_ms) add("pt_alignment_max_time_ms", options.max_time_ms);
  if (options.max_total_time_ms !== undefined && options.max_total_time_ms !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_total_time_ms) add("pt_alignment_max_total_time_ms", options.max_total_time_ms);
  // DP-only controls are ignored by the native parser for other variants.
  if (variant === "DYNAMIC_PROGRAMMING") {
    if (options.use_global_cache !== undefined && options.use_global_cache !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.use_global_cache) add("pt_alignment_use_global_cache", options.use_global_cache);
    if (options.batch_size !== undefined && options.batch_size !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.batch_size) add("pt_alignment_batch_size", options.batch_size);
    if (options.clear_cache !== undefined && options.clear_cache !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.clear_cache) add("pt_alignment_clear_cache", options.clear_cache);
  }
  if (variant === "APPROXIMATED_ORIGINAL") {
    if (options.max_trace_length !== undefined && options.max_trace_length !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_trace_length) add("pt_alignment_max_trace_length", options.max_trace_length);
    if (options.max_process_tree_height !== undefined && options.max_process_tree_height !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_process_tree_height) add("pt_alignment_max_process_tree_height", options.max_process_tree_height);
    if (options.max_trace_time_ms !== undefined && options.max_trace_time_ms !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_trace_time_ms) add("pt_alignment_max_trace_time_ms", options.max_trace_time_ms);
    if (options.max_per_trace_time_ms !== undefined && options.max_per_trace_time_ms !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.max_per_trace_time_ms) add("pt_alignment_max_per_trace_time_ms", options.max_per_trace_time_ms);
  }
  return query.toString();
}

export async function alignProcessTree(options: ProcessTreeAlignmentOptions = {}): Promise<ProcessTreeAlignmentResult> {
  let response: Response;
  try {
    const query = processTreeAlignmentQuery(options);
    response = await fetch(`/api/align-process-tree${query ? `?${query}` : ""}`, { method: "POST" });
  } catch (cause) {
    if (cause instanceof Error && cause.message.startsWith("pt_alignment_")) throw cause;
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isProcessTreeAlignmentResult(parsed)) throw new Error("the Minera backend returned an invalid process-tree alignment");
  return parsed;
}

function advancedQuery(options: Record<string, unknown>): string {
  const query = new URLSearchParams();
  const add = (name: string, value: unknown) => {
    if (value === undefined || value === null || value === "") return;
    if (Array.isArray(value)) { if (value.length) query.set(name, value.join(",")); return; }
    if (typeof value === "object") {
      if (["activity_log_costs", "activity_model_costs", "activity_sync_costs"].includes(name)) {
        const encoded = encodeCostMap(value as Record<string, number>);
        if (encoded) query.set(name, encoded);
        return;
      }
      const entries = Object.entries(value as Record<string, unknown>).sort(([a], [b]) => a.localeCompare(b));
      if (entries.some(([, item]) => typeof item !== "number" || !Number.isFinite(item) || item < 0)) throw new Error(`${name} must contain finite non-negative costs`);
      if (entries.length) query.set(name, entries.map(([key, item]) => `${key}=${item}`).join(","));
      return;
    }
    query.set(name, String(value));
  };
  Object.keys(options).sort().forEach((name) => add(name, options[name]));
  return query.toString();
}
function checkedNumber(value: number | undefined, name: string, integer = false) {
  if (value === undefined) return;
  if (!Number.isFinite(value) || value < 0 || (integer && !Number.isInteger(value))) throw new Error(`${name} must be a non-negative ${integer ? "integer" : "number"}`);
}
function checkedOptions(options: Record<string, unknown>, integerKeys: string[] = []) {
  for (const [name, value] of Object.entries(options)) {
    if (typeof value === "number") checkedNumber(value, name, integerKeys.includes(name));
    if (typeof value === "boolean" || typeof value === "string" || Array.isArray(value)) continue;
    if (value !== undefined && value !== null && typeof value !== "number" && !["activity_log_costs", "activity_model_costs", "activity_sync_costs"].includes(name)) throw new Error(`${name} is invalid`);
  }
  for (const key of ["initial_marking", "final_marking"]) {
    const value = options[key];
    if (Array.isArray(value) && value.some((item) => !Number.isInteger(item) || (item as number) < 0)) throw new Error(`${key} must contain non-negative integers`);
  }
}
export function decomposedAlignmentQuery(options: DecomposedAlignmentOptions = {}): string {
  const o: Record<string, unknown> = { ...options, variant: options.variant ?? "recompos_maximal" };
  checkedOptions(o, ["max_states", "max_total_states"]);
  if (typeof o.max_states === "number" && o.max_states < 1) throw new Error("max_states must be positive");
  if (typeof o.max_total_states === "number" && o.max_total_states < 1) throw new Error("max_total_states must be positive");
  return advancedQuery(o);
}
export function petriAntiAlignmentQuery(options: PetriAntiAlignmentOptions = {}): string {
  const o: Record<string, unknown> = { ...options, variant: options.variant ?? "version_discounted_a_star" };
  checkedOptions(o, ["marking_limit", "max_states"]);
  if (typeof o.marking_limit === "number" && o.marking_limit < 1) throw new Error("marking_limit must be positive");
  if (typeof o.exponent === "number" && o.exponent <= 1) throw new Error("exponent must be greater than 1");
  return advancedQuery(o);
}
export function petriMultiAlignmentQuery(options: PetriMultiAlignmentOptions = {}): string {
  const o: Record<string, unknown> = { ...options, variant: options.variant ?? "version_discounted_a_star" };
  checkedOptions(o, ["marking_limit", "max_states"]);
  if (typeof o.marking_limit === "number" && o.marking_limit < 1) throw new Error("marking_limit must be positive");
  if (typeof o.exponent === "number" && o.exponent <= 1) throw new Error("exponent must be greater than 1");
  return advancedQuery(o);
}
export function processTreeReductionQuery(options: ProcessTreeReductionOptions = {}): string {
  return advancedQuery(options.variant ? { ...options } : { activity_key: options.activity_key });
}
/** Query string for the hidden cached-session ILP miner endpoint. */
export function ilpMinerQuery(options: ILPMinerOptions = {}): string {
  const query = new URLSearchParams();
  if (options.activity_key !== undefined) query.set("ilp_activity_key", options.activity_key);
  if (options.noise_threshold !== undefined) query.set("ilp_noise_threshold", String(options.noise_threshold));
  if (options.max_nodes !== undefined) query.set("ilp_max_nodes", String(options.max_nodes));
  return query.toString();
}
export const ilpMineQuery = ilpMinerQuery;
/** Mines the most recently cached event log with the bounded classic ILP miner. */
export function requestIlpMine(options: ILPMinerOptions = {}): Promise<ILPMinerResult> {
  const query = ilpMinerQuery(options);
  return postChecked(`/api/ilp-mine${query ? `?${query}` : ""}`, isILPMinerResult, "ILP miner");
}
export const mineIlp = requestIlpMine;
export const requestILPMine = requestIlpMine;
/** Query string for the hidden cached-session classic genetic miner. */
export function geneticMinerQuery(options: GeneticMinerOptions = {}): string {
  const query = new URLSearchParams();
  const entries: Array<[string, string | undefined]> = [
    ["genetic_activity_key", options.activity_key],
    ["genetic_seed", options.seed === undefined ? undefined : String(options.seed)],
    ["genetic_population_size", options.population_size === undefined ? undefined : String(options.population_size)],
    ["genetic_generations", options.generations === undefined ? undefined : String(options.generations)],
    ["genetic_elitism_rate", options.elitism_rate === undefined ? undefined : String(options.elitism_rate)],
    ["genetic_crossover_rate", options.crossover_rate === undefined ? undefined : String(options.crossover_rate)],
    ["genetic_mutation_rate", options.mutation_rate === undefined ? undefined : String(options.mutation_rate)],
    ["genetic_elitism_min_sample", options.elitism_min_sample === undefined ? undefined : String(options.elitism_min_sample)],
    ["genetic_max_activities", options.max_activities === undefined ? undefined : String(options.max_activities)],
    ["genetic_max_states", options.max_states === undefined ? undefined : String(options.max_states)],
  ];
  for (const [name, value] of entries) if (value !== undefined) query.set(name, value);
  return query.toString();
}
export const geneticMineQuery = geneticMinerQuery;
/** Mines the latest cached log with bounded classic genetic discovery. */
export function requestGeneticMine(options: GeneticMinerOptions = {}): Promise<GeneticMinerResult> {
  const query = geneticMinerQuery(options);
  return postChecked(`/api/genetic-mine${query ? `?${query}` : ""}`, isGeneticMinerResult, "genetic miner");
}
export const mineGenetic = requestGeneticMine;
export const requestGeneticMiner = requestGeneticMine;
/** Query string for the cached-session organizational SNA endpoint. */
export function organizationalSnaQuery(options: OrganizationalSnaOptions = {}): string {
  const query = new URLSearchParams();
  const entries: Array<[string, string | undefined]> = [
    ["sna_variant", options.sna_variant],
    ["sna_activity_attribute", options.sna_activity_attribute],
    ["sna_resource_attribute", options.sna_resource_attribute],
    ["sna_beta", options.sna_beta === undefined ? undefined : String(options.sna_beta)],
    ["sna_n", options.sna_n === undefined ? undefined : String(options.sna_n)],
    ["sna_metric_normalization", options.sna_metric_normalization === undefined ? undefined : String(options.sna_metric_normalization)],
    ["sna_max_resources", options.sna_max_resources === undefined ? undefined : String(options.sna_max_resources)],
    ["sna_max_edges", options.sna_max_edges === undefined ? undefined : String(options.sna_max_edges)],
  ];
  for (const [key, value] of entries) if (value !== undefined) query.set(key, value);
  return query.toString();
}
export const snaQuery = organizationalSnaQuery;
export function requestOrganizationalSna(options: OrganizationalSnaOptions = {}): Promise<OrganizationalSnaResult> {
  const query = organizationalSnaQuery(options);
  return postChecked(`/api/organizational-sna${query ? `?${query}` : ""}`, isOrganizationalSnaResult, "organizational SNA");
}
export const requestSna = requestOrganizationalSna;
export const discoverOrganizationalSna = requestOrganizationalSna;
export function networkAnalysisQuery(options: NetworkAnalysisOptions = {}): string {
  const query = new URLSearchParams();
  const entries: Array<[string, unknown]> = [
    ["network_analysis_variant", options.network_analysis_variant],
    ["network_analysis_sorting_attribute", options.network_analysis_sorting_attribute],
    ["network_analysis_timestamp_attribute", options.network_analysis_timestamp_attribute],
    ["network_analysis_source_node_attribute", options.network_analysis_source_node_attribute],
    ["network_analysis_target_node_attribute", options.network_analysis_target_node_attribute],
    ["network_analysis_edge_attribute", options.network_analysis_edge_attribute],
    ["network_analysis_edge_reference", options.network_analysis_edge_reference],
    ["network_analysis_out_relation_attribute", options.network_analysis_out_relation_attribute],
    ["network_analysis_in_relation_attribute", options.network_analysis_in_relation_attribute],
    ["network_analysis_include_performance", options.network_analysis_include_performance],
    ["network_analysis_business_hours", options.network_analysis_business_hours],
    ["network_analysis_business_hour_slots", options.network_analysis_business_hour_slots],
    ["network_analysis_max_rows", options.network_analysis_max_rows],
    ["network_analysis_max_values", options.network_analysis_max_values],
  ];
  for (const [key, value] of entries) if (value !== undefined) query.set(key, String(value));
  return query.toString();
}
export function requestNetworkAnalysis(options: NetworkAnalysisOptions = {}): Promise<NetworkAnalysisResult> {
  const query = networkAnalysisQuery(options);
  return postChecked(`/api/network-analysis${query ? `?${query}` : ""}`, isNetworkAnalysisResult, "network analysis");
}
export const discoverNetworkAnalysis = requestNetworkAnalysis;
export function roleDiscoveryQuery(options: RoleDiscoveryOptions = {}): string {
  const query = new URLSearchParams();
  if (options.roles_variant !== undefined) query.set("roles_variant", options.roles_variant);
  if (options.roles_activity_attribute !== undefined) query.set("roles_activity_attribute", options.roles_activity_attribute);
  if (options.roles_resource_attribute !== undefined) query.set("roles_resource_attribute", options.roles_resource_attribute);
  if (options.roles_threshold !== undefined) query.set("roles_threshold", String(options.roles_threshold));
  if (options.roles_max_resources !== undefined) query.set("roles_max_resources", String(options.roles_max_resources));
  if (options.roles_max_activities !== undefined) query.set("roles_max_activities", String(options.roles_max_activities));
  return query.toString();
}
export function requestRoleDiscovery(options: RoleDiscoveryOptions = {}): Promise<RoleDiscoveryResult> {
  const query = roleDiscoveryQuery(options);
  return postChecked(`/api/roles${query ? `?${query}` : ""}`, isRoleDiscoveryResult, "role discovery");
}
export const discoverRoles = requestRoleDiscovery;
export function resourceProfilesQuery(options: ResourceProfilesOptions = {}): string {
  const query = new URLSearchParams();
  for (const [k,v] of Object.entries(options)) if (v !== undefined) query.set(k, String(v));
  return query.toString();
}
export function requestResourceProfiles(options: ResourceProfilesOptions = {}): Promise<ResourceProfilesResult> {
  const query = resourceProfilesQuery(options);
  return postChecked(`/api/resource-profiles${query ? `?${query}` : ""}`, isResourceProfilesResult, "resource profiles");
}

export function localDiagnosticsQuery(options: LocalDiagnosticsOptions = {}): string {
  const query = new URLSearchParams();
  for (const [k, v] of Object.entries(options)) if (v !== undefined) query.set(k, String(v));
  return query.toString();
}
export function requestLocalDiagnostics(options: LocalDiagnosticsOptions = {}): Promise<LocalDiagnosticsResult> {
  const query = localDiagnosticsQuery(options);
  return postChecked(`/api/local-diagnostics${query ? `?${query}` : ""}`, isLocalDiagnosticsResult, "local diagnostics");
}
export const discoverLocalDiagnostics = requestLocalDiagnostics;

/** Query string for the cached-session chaotic-activity statistics endpoint. */
export function chaoticActivitiesQuery(
  options: ChaoticActivitiesOptions = {},
): string {
  const query = new URLSearchParams();
  if (options.chaotic_activity_key !== undefined) {
    const key = options.chaotic_activity_key.trim();
    if (!key) throw new Error("chaotic_activity_key must not be empty");
    query.set("chaotic_activity_key", key);
  }
  if (options.chaotic_alpha !== undefined) {
    if (!Number.isFinite(options.chaotic_alpha) || options.chaotic_alpha < 0)
      throw new Error("chaotic_alpha must be finite and non-negative");
    query.set("chaotic_alpha", String(options.chaotic_alpha));
  }
  for (const [name, value] of [
    ["chaotic_max_activities", options.chaotic_max_activities],
    ["chaotic_max_events", options.chaotic_max_events],
  ] as const) {
    if (value !== undefined) {
      if (!Number.isInteger(value) || value <= 0)
        throw new Error(`${name} must be a positive integer`);
      query.set(name, String(value));
    }
  }
  return query.toString();
}

export function requestChaoticActivities(
  options: ChaoticActivitiesOptions = {},
): Promise<ChaoticActivitiesResult> {
  const query = chaoticActivitiesQuery(options);
  return postChecked(
    `/api/chaotic-activities${query ? `?${query}` : ""}`,
    isChaoticActivitiesResult,
    "chaotic activities",
  );
}
export const discoverChaoticActivities = requestChaoticActivities;

export function passedTimeQuery(options: PassedTimeOptions = {}): string {
  const query = new URLSearchParams();
  if (options.passed_time_activity !== undefined) {
    if (!options.passed_time_activity) throw new Error("passed_time_activity must not be empty");
    query.set("passed_time_activity", options.passed_time_activity);
  }
  const strings: Array<[string, string | undefined]> = [
    ["passed_time_variant", options.passed_time_variant],
    ["passed_time_activity_key", options.passed_time_activity_key],
    ["passed_time_timestamp_key", options.passed_time_timestamp_key],
    ["passed_time_start_timestamp_key", options.passed_time_start_timestamp_key],
    ["passed_time_aggregation", options.passed_time_aggregation],
    ["passed_time_business_hour_slots", options.passed_time_business_hour_slots],
  ];
  if (options.passed_time_variant !== undefined &&
      !["pre", "post", "prepost"].includes(options.passed_time_variant))
    throw new Error("passed_time_variant is not supported");
  if (options.passed_time_aggregation !== undefined &&
      !["mean", "median", "min", "max", "stdev", "sum"]
        .includes(options.passed_time_aggregation))
    throw new Error("passed_time_aggregation is not supported");
  for (const [key, value] of strings) {
    if (value !== undefined) {
      if (!value) throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  if (options.passed_time_business_hours !== undefined &&
      typeof options.passed_time_business_hours !== "boolean")
    throw new Error("passed_time_business_hours must be boolean");
  if (options.passed_time_business_hours !== undefined)
    query.set("passed_time_business_hours", String(options.passed_time_business_hours));
  return query.toString();
}

export function requestPassedTime(options: PassedTimeOptions = {}): Promise<PassedTimeResult> {
  const query = passedTimeQuery(options);
  return postChecked(`/api/passed-time${query ? `?${query}` : ""}`, isPassedTimeResult, "passed time");
}
export const discoverPassedTime = requestPassedTime;

export function serviceTimeQuery(options: ServiceTimeOptions = {}): string {
  const query = new URLSearchParams();
  const strings: Array<[string, string | undefined]> = [
    ["service_time_activity_key", options.service_time_activity_key],
    ["service_time_start_timestamp_key", options.service_time_start_timestamp_key],
    ["service_time_timestamp_key", options.service_time_timestamp_key],
    ["service_time_aggregation", options.service_time_aggregation],
    ["service_time_business_hour_slots", options.service_time_business_hour_slots],
  ];
  for (const [key, value] of strings) {
    if (value !== undefined) {
      if (!value) throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  if (options.service_time_business_hours !== undefined &&
      typeof options.service_time_business_hours !== "boolean")
    throw new Error("service_time_business_hours must be boolean");
  if (options.service_time_business_hours !== undefined)
    query.set("service_time_business_hours", String(options.service_time_business_hours));
  return query.toString();
}

export function requestServiceTime(options: ServiceTimeOptions = {}): Promise<ServiceTimeResult> {
  const query = serviceTimeQuery(options);
  return postChecked(`/api/service-time${query ? `?${query}` : ""}`, isServiceTimeResult, "service time");
}
export const discoverServiceTime = requestServiceTime;

export function reworkQuery(options: ReworkOptions = {}): string {
  const query = new URLSearchParams();
  if (options.rework_variant !== undefined &&
      !["activities", "cases"].includes(options.rework_variant))
    throw new Error("rework_variant is not supported");
  if (options.rework_variant !== undefined) query.set("rework_variant", options.rework_variant);
  for (const [key, value] of [
    ["rework_activity_key", options.rework_activity_key],
    ["rework_case_id_key", options.rework_case_id_key],
  ] as const) {
    if (value !== undefined) {
      if (!value) throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  return query.toString();
}

export function requestRework(options: ReworkOptions = {}): Promise<ReworkResult> {
  const query = reworkQuery(options);
  return postChecked(`/api/rework${query ? `?${query}` : ""}`, isReworkResult, "rework");
}
export const discoverRework = requestRework;

export function cycleTimeQuery(options: CycleTimeOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of [
    ["cycle_time_timestamp_key", options.cycle_time_timestamp_key],
    ["cycle_time_start_timestamp_key", options.cycle_time_start_timestamp_key],
  ] as const) {
    if (value !== undefined) {
      if (!value) throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  return query.toString();
}

export function requestCycleTime(options: CycleTimeOptions = {}): Promise<CycleTimeResult> {
  const query = cycleTimeQuery(options);
  return postChecked(`/api/cycle-time${query ? `?${query}` : ""}`, isCycleTimeResult, "cycle time");
}
export const discoverCycleTime = requestCycleTime;

export function caseArrivalQuery(options: CaseArrivalOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of [
    ["case_arrival_timestamp_key", options.case_arrival_timestamp_key],
    ["case_arrival_business_hour_slots", options.case_arrival_business_hour_slots],
  ] as const) {
    if (value !== undefined) {
      if (!value) throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  if (options.case_arrival_business_hours !== undefined) {
    if (typeof options.case_arrival_business_hours !== "boolean")
      throw new Error("case_arrival_business_hours must be boolean");
    query.set("case_arrival_business_hours", String(options.case_arrival_business_hours));
  }
  return query.toString();
}

export function requestCaseArrival(options: CaseArrivalOptions = {}): Promise<CaseArrivalResult> {
  const query = caseArrivalQuery(options);
  return postChecked(`/api/case-arrival${query ? `?${query}` : ""}`, isCaseArrivalResult, "case arrival");
}
export const discoverCaseArrival = requestCaseArrival;

export function caseStatisticsQuery(options: CaseStatisticsOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of [
    ["case_statistics_case_id_key", options.case_statistics_case_id_key],
    ["case_statistics_timestamp_key", options.case_statistics_timestamp_key],
    ["case_statistics_sort_by", options.case_statistics_sort_by],
    ["case_statistics_business_hour_slots", options.case_statistics_business_hour_slots],
  ] as const) {
    if (value !== undefined) {
      if (!value) throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  for (const [key, value] of [
    ["case_statistics_enable_sort", options.case_statistics_enable_sort],
    ["case_statistics_sort_ascending", options.case_statistics_sort_ascending],
    ["case_statistics_business_hours", options.case_statistics_business_hours],
  ] as const) {
    if (value !== undefined) {
      if (typeof value !== "boolean") throw new Error(`${key} must be boolean`);
      query.set(key, String(value));
    }
  }
  if (options.case_statistics_max_cases !== undefined) {
    if (!Number.isInteger(options.case_statistics_max_cases) || options.case_statistics_max_cases < 0)
      throw new Error("case_statistics_max_cases must be a non-negative integer");
    query.set("case_statistics_max_cases", String(options.case_statistics_max_cases));
  }
  if (options.case_statistics_sort_by !== undefined &&
      !["case_id", "start_time", "end_time", "duration"].includes(options.case_statistics_sort_by))
    throw new Error("case_statistics_sort_by is not supported");
  return query.toString();
}

export function requestCaseStatistics(options: CaseStatisticsOptions = {}): Promise<CaseStatisticsResult> {
  const query = caseStatisticsQuery(options);
  return postChecked(`/api/case-statistics${query ? `?${query}` : ""}`, isCaseStatisticsResult, "case statistics");
}
export const discoverCaseStatistics = requestCaseStatistics;

export function attributeStatisticsQuery(options: AttributeStatisticsOptions = {}): string {
  const query = new URLSearchParams();
  if (options.attribute_statistics_key !== undefined) {
    if (!options.attribute_statistics_key)
      throw new Error("attribute_statistics_key must not be empty");
    query.set("attribute_statistics_key", options.attribute_statistics_key);
  }
  if (options.attribute_statistics_scope !== undefined) {
    if (!["events", "traces"].includes(options.attribute_statistics_scope))
      throw new Error("attribute_statistics_scope is not supported");
    query.set("attribute_statistics_scope", options.attribute_statistics_scope);
  }
  if (options.attribute_statistics_keep_once_per_case !== undefined) {
    if (typeof options.attribute_statistics_keep_once_per_case !== "boolean")
      throw new Error("attribute_statistics_keep_once_per_case must be boolean");
    query.set("attribute_statistics_keep_once_per_case", String(options.attribute_statistics_keep_once_per_case));
  }
  return query.toString();
}

export function requestAttributeStatistics(options: AttributeStatisticsOptions = {}): Promise<AttributeStatisticsResult> {
  const query = attributeStatisticsQuery(options);
  return postChecked(`/api/attribute-statistics${query ? `?${query}` : ""}`, isAttributeStatisticsResult, "attribute statistics");
}
export const discoverAttributeStatistics = requestAttributeStatistics;

export function attributeDistributionQuery(options: AttributeDistributionOptions = {}): string {
  const query = new URLSearchParams();
  if (options.attribute_distribution_type !== undefined) {
    if (!["days_month", "months", "years", "hours", "days_week", "weeks"].includes(options.attribute_distribution_type))
      throw new Error("attribute_distribution_type is not supported");
    query.set("attribute_distribution_type", options.attribute_distribution_type);
  }
  if (options.attribute_distribution_timestamp_key !== undefined) {
    if (!options.attribute_distribution_timestamp_key)
      throw new Error("attribute_distribution_timestamp_key must not be empty");
    query.set("attribute_distribution_timestamp_key", options.attribute_distribution_timestamp_key);
  }
  return query.toString();
}

export function requestAttributeDistribution(options: AttributeDistributionOptions = {}): Promise<AttributeDistributionResult> {
  const query = attributeDistributionQuery(options);
  return postChecked(`/api/attribute-distribution${query ? `?${query}` : ""}`, isAttributeDistributionResult, "attribute distribution");
}
export const discoverAttributeDistribution = requestAttributeDistribution;

export function attributeKdeQuery(options: AttributeKdeOptions = {}): string {
  const query = new URLSearchParams();
  if (options.attribute_kde_key !== undefined) {
    if (!options.attribute_kde_key)
      throw new Error("attribute_kde_key must not be empty");
    query.set("attribute_kde_key", options.attribute_kde_key);
  }
  if (options.attribute_kde_variant !== undefined) {
    if (!["numeric", "date"].includes(options.attribute_kde_variant))
      throw new Error("attribute_kde_variant is not supported");
    query.set("attribute_kde_variant", options.attribute_kde_variant);
  }
  const positiveInteger = (value: number, name: string, maximum: number) => {
    if (!Number.isInteger(value) || value < 1 || value > maximum)
      throw new Error(`${name} must be a positive integer no greater than ${maximum}`);
    query.set(name, String(value));
  };
  if (options.attribute_kde_graph_points !== undefined)
    positiveInteger(options.attribute_kde_graph_points, "attribute_kde_graph_points", 10000);
  if (options.attribute_kde_points_to_sample !== undefined)
    positiveInteger(options.attribute_kde_points_to_sample, "attribute_kde_points_to_sample", 100000);
  return query.toString();
}

export function requestAttributeKde(options: AttributeKdeOptions = {}): Promise<AttributeKdeResult> {
  const query = attributeKdeQuery(options);
  return postChecked(`/api/attribute-kde${query ? `?${query}` : ""}`, isAttributeKdeResult, "attribute KDE");
}
export const discoverAttributeKde = requestAttributeKde;

export function boundaryActivitiesQuery(options: BoundaryActivitiesOptions = {}): string {
  const query = new URLSearchParams();
  if (options.boundary_activities_variant !== undefined) {
    if (!["start", "end", "both"].includes(options.boundary_activities_variant))
      throw new Error("boundary_activities_variant is not supported");
    query.set("boundary_activities_variant", options.boundary_activities_variant);
  }
  if (options.boundary_activities_activity_key !== undefined) {
    if (!options.boundary_activities_activity_key)
      throw new Error("boundary_activities_activity_key must not be empty");
    query.set("boundary_activities_activity_key", options.boundary_activities_activity_key);
  }
  return query.toString();
}

export function requestBoundaryActivities(options: BoundaryActivitiesOptions = {}): Promise<BoundaryActivitiesResult> {
  const query = boundaryActivitiesQuery(options);
  return postChecked(`/api/boundary-activities${query ? `?${query}` : ""}`, isBoundaryActivitiesResult, "boundary activities");
}
export const discoverBoundaryActivities = requestBoundaryActivities;

export function variantStatisticsQuery(options: VariantStatisticsOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant_statistics_activity_key !== undefined) {
    if (!options.variant_statistics_activity_key)
      throw new Error("variant_statistics_activity_key must not be empty");
    query.set("variant_statistics_activity_key", options.variant_statistics_activity_key);
  }
  if (options.variant_statistics_max_variants !== undefined) {
    if (!Number.isInteger(options.variant_statistics_max_variants) || options.variant_statistics_max_variants < 0)
      throw new Error("variant_statistics_max_variants must be a non-negative integer");
    query.set("variant_statistics_max_variants", String(options.variant_statistics_max_variants));
  }
  return query.toString();
}

export function requestVariantStatistics(options: VariantStatisticsOptions = {}): Promise<VariantStatisticsResult> {
  const query = variantStatisticsQuery(options);
  return postChecked(`/api/variant-statistics${query ? `?${query}` : ""}`, isVariantStatisticsResult, "variant statistics");
}
export const discoverVariantStatistics = requestVariantStatistics;
export function clusteringQuery(options: ClusteringOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options)) if (value !== undefined) query.set(key, String(value));
  return query.toString();
}
export function requestClustering(options: ClusteringOptions = {}): Promise<ClusteringResult> {
  const query = clusteringQuery(options);
  return postChecked(`/api/clustering${query ? `?${query}` : ""}`, isClusteringResult, "trace clustering");
}
export const discoverClustering = requestClustering;
function profileAttributeList(values: string[] | undefined, name: string): string | undefined {
  if (values === undefined) return undefined;
  if (values.length === 0 || values.some((value) => !value))
    throw new Error(`${name} must contain non-empty values`);
  if (new Set(values).size !== values.length)
    throw new Error(`${name} must not contain duplicates`);
  return values.map((value) => value.replaceAll("\\", "\\\\").replaceAll(",", "\\,")).join(",");
}
export function profileClusteringQuery(options: ProfileClusteringOptions = {}): string {
  const positive = [
    ["profile_cluster_count", options.profile_cluster_count],
    ["profile_max_iterations", options.profile_max_iterations],
    ["profile_max_traces", options.profile_max_traces],
    ["profile_max_features", options.profile_max_features],
  ] as const;
  for (const [name, value] of positive) {
    if (value !== undefined && (!Number.isSafeInteger(value) || value <= 0 || value > 100000))
      throw new Error(`${name} must be a positive integer within the safety limit`);
  }
  if (options.profile_seed !== undefined && (!Number.isSafeInteger(options.profile_seed) || options.profile_seed < 0))
    throw new Error("profile_seed must be a non-negative integer");
  if (options.profile_tolerance !== undefined && (!Number.isFinite(options.profile_tolerance) || options.profile_tolerance < 0))
    throw new Error("profile_tolerance must be finite and non-negative");
  const values: Record<string, string | undefined> = {
    profile_cluster_count: options.profile_cluster_count === undefined ? undefined : String(options.profile_cluster_count),
    profile_event_numeric_attributes: profileAttributeList(options.profile_event_numeric_attributes, "profile_event_numeric_attributes"),
    profile_event_string_attributes: profileAttributeList(options.profile_event_string_attributes, "profile_event_string_attributes"),
    profile_feature_names: profileAttributeList(options.profile_feature_names, "profile_feature_names"),
    profile_include_activity: options.profile_include_activity === undefined ? undefined : String(options.profile_include_activity),
    profile_include_succession: options.profile_include_succession === undefined ? undefined : String(options.profile_include_succession),
    profile_max_features: options.profile_max_features === undefined ? undefined : String(options.profile_max_features),
    profile_max_iterations: options.profile_max_iterations === undefined ? undefined : String(options.profile_max_iterations),
    profile_max_traces: options.profile_max_traces === undefined ? undefined : String(options.profile_max_traces),
    profile_seed: options.profile_seed === undefined ? undefined : String(options.profile_seed),
    profile_tolerance: options.profile_tolerance === undefined ? undefined : String(options.profile_tolerance),
    profile_trace_numeric_attributes: profileAttributeList(options.profile_trace_numeric_attributes, "profile_trace_numeric_attributes"),
    profile_trace_string_attributes: profileAttributeList(options.profile_trace_string_attributes, "profile_trace_string_attributes"),
  };
  const query = new URLSearchParams();
  for (const key of Object.keys(values).sort()) if (values[key] !== undefined) query.set(key, values[key] as string);
  return query.toString();
}
export function requestProfileClustering(options: ProfileClusteringOptions = {}): Promise<ProfileClusteringResult> {
  const query = profileClusteringQuery(options);
  return postChecked(`/api/profile-clustering${query ? `?${query}` : ""}`, isProfileClusteringResult, "profile clustering");
}
export const discoverProfileClustering = requestProfileClustering;
/** Query string for the cached-session trace-variant anonymizer. */
export function traceVariantQuery(options: TraceVariantQueryOptions = {}): string {
  const query = new URLSearchParams();
  const entries: Array<[string, string | undefined]> = [
    ["tvq_variant", options.tvq_variant],
    ["tvq_epsilon", options.tvq_epsilon === undefined ? undefined : String(options.tvq_epsilon)],
    ["tvq_k", options.tvq_k === undefined ? undefined : String(options.tvq_k)],
    ["tvq_p", options.tvq_p === undefined ? undefined : String(options.tvq_p)],
    ["tvq_seed", options.tvq_seed === undefined ? undefined : String(options.tvq_seed)],
    ["tvq_secure_random", options.tvq_secure_random === undefined ? undefined : String(options.tvq_secure_random)],
    ["tvq_max_output_traces", options.tvq_max_output_traces === undefined ? undefined : String(options.tvq_max_output_traces)],
    ["tvq_max_output_variants", options.tvq_max_output_variants === undefined ? undefined : String(options.tvq_max_output_variants)],
  ];
  for (const [key, value] of entries) if (value !== undefined) query.set(key, value);
  return query.toString();
}
export const traceVariantQueryOptions = traceVariantQuery;
/** Requests a trace-variant query from the latest cached session. */
export function requestTraceVariantQuery(options: TraceVariantQueryOptions = {}): Promise<TraceVariantQueryResult> {
  const query = traceVariantQuery(options);
  return postChecked(`/api/trace-variant-query${query ? `?${query}` : ""}`, isTraceVariantQueryResult, "trace-variant query");
}
export const anonymizeTraceVariants = requestTraceVariantQuery;
export function pripelQuery(options: PripelOptions = {}): string {
  const query = new URLSearchParams();
  const escape = (value: string) => value.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
  const entries: Array<[string, string | undefined]> = [
    ["pripel_epsilon", options.pripel_epsilon === undefined ? undefined : String(options.pripel_epsilon)],
    ["pripel_blocklist", options.pripel_blocklist === undefined ? undefined : options.pripel_blocklist.map(escape).join(",")],
    ["pripel_seed", options.pripel_seed === undefined ? undefined : String(options.pripel_seed)],
    ["pripel_secure_random", options.pripel_secure_random === undefined ? undefined : String(options.pripel_secure_random)],
    ["pripel_max_output_traces", options.pripel_max_output_traces === undefined ? undefined : String(options.pripel_max_output_traces)],
    ["pripel_max_distance", options.pripel_max_distance === undefined ? undefined : String(options.pripel_max_distance)],
  ];
  for (const [key, value] of entries) if (value !== undefined) query.set(key, value);
  return query.toString();
}
export async function requestPripel(queryLog: File, options: PripelOptions = {}): Promise<PripelResult> {
  const query = pripelQuery(options);
  const form = new FormData();
  form.append("trace_variant_query", queryLog, queryLog.name);
  return postChecked(`/api/pripel${query ? `?${query}` : ""}`, isPripelResult, "PRIPEL anonymization", form);
}
export const anonymizePripel = requestPripel;
async function postChecked<T>(path: string, guard: (value: unknown) => value is T, label: string, body?: BodyInit): Promise<T> {
  let response: Response;
  try { response = await fetch(path, { method: "POST", ...(body === undefined ? {} : { body }) }); }
  catch { throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!guard(parsed)) throw new Error(`the Minera backend returned an invalid ${label}`);
  return parsed;
}
export function alignDecomposed(miner: string, options: DecomposedAlignmentOptions = {}): Promise<DecomposedAlignmentResult> {
  const query = new URLSearchParams(decomposedAlignmentQuery(options)); query.set("miner", miner);
  return postChecked(`/api/align-decomposed?${query.toString()}`, isDecomposedAlignmentResult, "decomposed alignment");
}
export function alignPetriAnti(miner: string, options: PetriAntiAlignmentOptions = {}): Promise<PetriAntiAlignmentResult> {
  const query = new URLSearchParams(petriAntiAlignmentQuery(options)); query.set("miner", miner);
  return postChecked(`/api/align-petri-anti?${query.toString()}`, isPetriAntiAlignmentResult, "Petri anti-alignment");
}
export function alignPetriMulti(miner: string, options: PetriMultiAlignmentOptions = {}): Promise<PetriMultiAlignmentResult> {
  const query = new URLSearchParams(petriMultiAlignmentQuery(options)); query.set("miner", miner);
  return postChecked(`/api/align-petri-multi?${query.toString()}`, isPetriMultiAlignmentResult, "Petri multi-alignment");
}
export function reduceProcessTree(traceIndex: number, options: ProcessTreeReductionOptions = {}): Promise<ProcessTreeReductionResult> {
  if (!Number.isInteger(traceIndex) || traceIndex < 0) return Promise.reject(new Error("trace_index must be a non-negative integer"));
  const query = new URLSearchParams(processTreeReductionQuery(options)); query.set("trace_index", String(traceIndex));
  return postChecked(`/api/reduce-process-tree?${query.toString()}`, isProcessTreeReductionResult, "process-tree reduction");
}

export interface DecisionMiningOptions {
  max_depth?: number;
  min_split?: number;
  min_leaf?: number;
  max_categories?: number;
  max_thresholds?: number;
  min_gain?: number;
  include_trace_attributes?: boolean;
  attributes?: string[];
}

export interface DecisionTablesRequestOptions extends DecisionMiningOptions {
  miner?: string;
}

export function decisionMiningQuery(options: DecisionMiningOptions = {}): string {
  const query = new URLSearchParams();
  const integer = (name: string, value: number | undefined, minimum: number) => {
    if (value === undefined) return;
    if (!Number.isSafeInteger(value) || value < minimum)
      throw new Error(`${name} must be a safe integer >= ${minimum}`);
    query.set(name, String(value));
  };
  integer("decision_max_depth", options.max_depth, 0);
  if (options.max_depth !== undefined && options.max_depth > 64)
    throw new Error("decision_max_depth must be at most 64");
  integer("decision_min_split", options.min_split, 2);
  integer("decision_min_leaf", options.min_leaf, 1);
  integer("decision_max_categories", options.max_categories, 1);
  integer("decision_max_thresholds", options.max_thresholds, 1);
  if (options.min_gain !== undefined) {
    if (!Number.isFinite(options.min_gain) || options.min_gain < 0)
      throw new Error("decision_min_gain must be finite and non-negative");
    query.set("decision_min_gain", String(options.min_gain));
  }
  if (options.include_trace_attributes !== undefined)
    query.set("decision_include_trace_attributes", String(options.include_trace_attributes));
  if (options.attributes !== undefined) {
    if (!Array.isArray(options.attributes) || options.attributes.length === 0 ||
        options.attributes.some((attribute) => typeof attribute !== "string" ||
          !attribute.trim() || attribute.includes(",")))
      throw new Error("decision_attributes must contain non-empty comma-free names");
    query.set("decision_attributes", options.attributes.map((attribute) => attribute.trim()).join(","));
  }
  return query.toString();
}

/**
 * Asks the backend to mine the decision points of one miner's model: which
 * attribute values steer a case down which branch. Like alignments this is
 * deferred to an on-demand request, so a mine never pays for decisions the
 * user may not open.
 */
export async function decideModel(
  miner: string,
  options: DecisionMiningOptions = {},
): Promise<Decisions> {
  if (!miner.trim()) throw new Error("miner must not be empty");
  const params = new URLSearchParams(decisionMiningQuery(options));
  params.set("miner", miner);
  let response: Response;
  try {
    response = await fetch(`/api/decide?${params.toString()}`, {
      method: "POST",
    });
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (
    parsed === null ||
    typeof parsed !== "object" ||
    !("decision_points" in parsed)
  ) {
    throw new Error("the Minera backend returned an invalid decision mining");
  }
  return parsed as Decisions;
}

/** The backend's answer to checking an uploaded model against the log. */
export interface ImportedModelCheck {
  /**
   * The activity registry, re-sent because the imported model may name
   * activities the log never shows; ids in `model` index into this list.
   */
  activities: ActivityStat[];
  model: ModelConformance;
}

/**
 * Checks the most recently mined log against a user-supplied model — a PNML
 * workflow net or a BPMN 2.0 process, told apart server-side. The server
 * keeps the net under the miner id "imported", so alignments can be
 * requested for it afterwards like for any discovered model.
 */
export async function checkModel(file: File): Promise<ImportedModelCheck> {
  const form = new FormData();
  form.append("model", file, file.name);
  let response: Response;
  try {
    response = await fetch("/api/check-model", { method: "POST", body: form });
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (
    parsed === null ||
    typeof parsed !== "object" ||
    !("model" in parsed) ||
    !("activities" in parsed)
  ) {
    throw new Error("the Minera backend returned an invalid model check");
  }
  return parsed as ImportedModelCheck;
}

/**
 * Fetches one miner's model from the most recently mined log as standard PNML.
 * The net is translated server-side (the single source of truth used for
 * conformance), so this only works for a log mined this session.
 */
export async function fetchPnml(miner: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(`/api/pnml?miner=${encodeURIComponent(miner)}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

/**
 * Fetches a block-structured process tree as PTML from the most recent mine.
 * Inductive is the default; other workflow-net miners are reduced server-side
 * and reject when their structure is not exactly block structured.
 */
export async function fetchPtml(miner?: string): Promise<string> {
  let response: Response;
  try {
    const query = miner ? `?miner=${encodeURIComponent(miner)}` : "";
    response = await fetch(`/api/ptml${query}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

/**
 * Fetches the most recently mined event log — after any filtering applied at
 * mine time — as standard XES. Like PNML, this is served from the backend's
 * cached session, so it only works for a log mined this session.
 */
export async function fetchXes(): Promise<string> {
  let response: Response;
  try {
    response = await fetch("/api/xes");
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

/** Requests the encoded decision tables for one cached model. */
export function requestDecisionTables(
  miner?: string,
  options?: DecisionMiningOptions,
): Promise<DecisionTablesResult>;
export function requestDecisionTables(
  options?: DecisionTablesRequestOptions,
): Promise<DecisionTablesResult>;
export function requestDecisionTables(
  minerOrOptions: string | DecisionTablesRequestOptions = "inductive",
  legacyOptions: DecisionMiningOptions = {},
): Promise<DecisionTablesResult> {
  const options = typeof minerOrOptions === "string"
    ? { ...legacyOptions, miner: minerOrOptions }
    : (minerOrOptions ?? {});
  const miner = options.miner ?? "inductive";
  if (!miner.trim()) return Promise.reject(new Error("miner must not be empty"));
  const query = new URLSearchParams({ miner });
  try {
    for (const [key, value] of new URLSearchParams(decisionMiningQuery(options)))
      query.append(key, value);
  } catch (error) {
    return Promise.reject(error);
  }
  return postChecked(
    `/api/decision-tables?${query.toString()}`,
    isDecisionTablesResult,
    "decision tables",
  );
}

/** Requests a guarded data-Petri-net projection of the latest cached model. */
export function requestDataPetriNet(
  miner?: string,
  options?: DecisionMiningOptions,
): Promise<DataPetriNetResult>;
export function requestDataPetriNet(
  options?: DecisionTablesRequestOptions,
): Promise<DataPetriNetResult>;
export function requestDataPetriNet(
  minerOrOptions: string | DecisionTablesRequestOptions = "inductive",
  legacyOptions: DecisionMiningOptions = {},
): Promise<DataPetriNetResult> {
  const options = typeof minerOrOptions === "string"
    ? { ...legacyOptions, miner: minerOrOptions }
    : (minerOrOptions ?? {});
  const miner = options.miner ?? "inductive";
  if (!miner.trim()) return Promise.reject(new Error("miner must not be empty"));
  const query = new URLSearchParams({ miner });
  try {
    for (const [key, value] of new URLSearchParams(decisionMiningQuery(options)))
      query.append(key, value);
  } catch (error) {
    return Promise.reject(error);
  }
  return postChecked(`/api/data-petri-net?${query.toString()}`,
                     isDataPetriNetResult, "data Petri net");
}

/**
 * Fetches the most recently mined directly-follows graph in the compact
 * classic text interchange format. The graph is generated from the cached
 * post-filter session on the backend.
 */
export interface DfgExportOptions {
  noise_threshold?: number;
  most_common_paths?: string;
}

export function dfgExportQuery(options: DfgExportOptions = {}): string {
  const query = new URLSearchParams();
  if (options.noise_threshold !== undefined) {
    if (!Number.isFinite(options.noise_threshold))
      throw new Error("noise_threshold must be finite");
    query.set("noise_threshold", String(options.noise_threshold));
  }
  if (options.most_common_paths !== undefined) {
    const paths = options.most_common_paths.trim();
    if (!paths) throw new Error("most_common_paths must not be empty");
    query.set("most_common_paths", paths);
  }
  return query.toString();
}

export async function fetchDfg(options: DfgExportOptions = {}): Promise<string> {
  let response: Response;
  try {
    const query = dfgExportQuery(options);
    response = await fetch(`/api/dfg${query ? `?${query}` : ""}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

/** Validates and normalizes a classic DFG text document through the backend. */
export async function importDfg(document: Blob): Promise<string> {
  let response: Response;
  try {
    response = await fetch("/api/dfg", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: document,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

export interface DfgPrecisionOptions {
  activity_attribute?: string;
}

export function dfgPrecisionQuery(options: DfgPrecisionOptions = {}): string {
  const query = new URLSearchParams();
  if (options.activity_attribute !== undefined) {
    const attribute = options.activity_attribute.trim();
    if (!attribute) throw new Error("activity_attribute must not be empty");
    if (attribute !== "activity") query.set("activity_attribute", attribute);
  }
  return query.toString();
}

export async function requestDfgPrecision(
  document: Blob, options: DfgPrecisionOptions = {},
): Promise<DfgPrecisionResult> {
  const query = dfgPrecisionQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/dfg-precision${query ? `?${query}` : ""}`, {
      method: "POST", headers: { "Content-Type": "text/plain" }, body: document,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload &&
      typeof (payload as { error: unknown }).error === "string" &&
      (payload as { error: string }).error.length > 0)
    throw new Error((payload as { error: string }).error);
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isDfgPrecisionResult(payload))
    throw new Error("the Minera backend returned an invalid DFG precision result");
  return payload;
}

export function dfgPetriConversionQuery(options: DfgPetriConversionOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  for (const [key, value] of [
    ["artificial_start_activity", options.artificial_start_activity],
    ["artificial_end_activity", options.artificial_end_activity],
  ] as const) {
    if (value !== undefined) {
      if (value.trim() === "") throw new Error(`${key} must not be empty`);
      query.set(key, value);
    }
  }
  return query.toString();
}

export async function convertDfgToPetriNet(
  document: Blob,
  options: DfgPetriConversionOptions = {},
): Promise<DfgPetriConversionResult> {
  const query = dfgPetriConversionQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/dfg-to-petri${query ? `?${query}` : ""}`, {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: document,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed)
    throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isDfgPetriConversionResult(parsed))
    throw new Error("the Minera backend returned an invalid DFG conversion");
  return parsed;
}

export interface ProcessCubeOptions {
  x_col?: string;
  x_cols?: string[];
  y_col?: string;
  y_cols?: string[];
  agg_col?: string;
  aggregation?: "mean" | "sum" | "min" | "max" | "count" | "median" | "first" | "last";
  max_divisions_x?: number;
  max_divisions_y?: number;
  x_bins?: number[];
  y_bins?: number[];
}

export function processCubeQuery(options: ProcessCubeOptions = {}): string {
  const query = new URLSearchParams();
  const addName = (name: string, value: string | undefined) => {
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${name} must not be empty`);
      query.set(name, value);
    }
  };
  const addNames = (name: string, values: string[] | undefined) => {
    if (values !== undefined) {
      if (values.length === 0 || values.some((value) => !value.trim()))
        throw new Error(`${name} must contain non-empty names`);
      const encoded = values.map((value) => value.replaceAll("\\", "\\\\").replaceAll(",", "\\,"));
      query.set(name, encoded.join(","));
    }
  };
  if (options.x_col !== undefined && options.x_cols !== undefined)
    throw new Error("x_col and x_cols are mutually exclusive");
  if (options.y_col !== undefined && options.y_cols !== undefined)
    throw new Error("y_col and y_cols are mutually exclusive");
  addName("x_col", options.x_col);
  addNames("x_cols", options.x_cols);
  addName("y_col", options.y_col);
  addNames("y_cols", options.y_cols);
  addName("agg_col", options.agg_col);
  if (options.aggregation !== undefined) {
    if (!["mean", "sum", "min", "max", "count", "median", "first", "last"]
      .includes(options.aggregation)) throw new Error("aggregation is not supported");
    query.set("aggregation", options.aggregation);
  }
  const addDivision = (name: string, value: number | undefined) => {
    if (value !== undefined) {
      if (!Number.isSafeInteger(value) || value < 1 || value > 10000)
        throw new Error(`${name} must be an integer from 1 through 10000`);
      query.set(name, String(value));
    }
  };
  addDivision("max_divisions_x", options.max_divisions_x);
  addDivision("max_divisions_y", options.max_divisions_y);
  const addBins = (name: string, values: number[] | undefined) => {
    if (values !== undefined) {
      if (values.length < 2 || values.some((value) => !Number.isFinite(value)))
        throw new Error(`${name} must contain at least two finite edges`);
      query.set(name, values.join(","));
    }
  };
  addBins("x_bins", options.x_bins);
  addBins("y_bins", options.y_bins);
  return query.toString();
}

export async function requestProcessCube(
  table: ProcessCubeTable,
  options: ProcessCubeOptions = {},
): Promise<ProcessCubeResult> {
  if (!isProcessCubeTable(table))
    throw new Error("process cube tables require rectangular finite numeric or categorical columns");
  const query = processCubeQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/process-cube${query ? `?${query}` : ""}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(table),
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload)
    throw new Error(String((payload as { error: unknown }).error));
  if (!response.ok)
    throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isProcessCubeResult(payload))
    throw new Error("the Minera backend returned an invalid process cube");
  return payload;
}

export interface StreamingDfgOptions {
  streaming_case_id_column?: string;
  streaming_activity_column?: string;
}

export function streamingDfgQuery(options: StreamingDfgOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options)) {
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${key} must not be empty`);
      query.set(key, value.trim());
    }
  }
  return query.toString();
}

export async function requestStreamingDfg(
  log: Blob,
  options: StreamingDfgOptions = {},
): Promise<StreamingDfgResult> {
  const form = new FormData();
  const name = typeof File !== "undefined" && log instanceof File ? log.name : "log.csv";
  form.append("log", log, name);
  const query = streamingDfgQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/streaming-dfg${query ? `?${query}` : ""}`, {
      method: "POST", body: form,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload)
    throw new Error(String((payload as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isStreamingDfgResult(payload))
    throw new Error("the Minera backend returned an invalid streaming DFG");
  return payload;
}

export interface StreamingFootprintsOptions {
  streaming_footprints_case_id_column?: string;
  streaming_footprints_activity_column?: string;
  streaming_footprints_activities: string[];
  streaming_footprints_start_activities?: string[];
  streaming_footprints_end_activities?: string[];
  streaming_footprints_sequence?: Array<[string, string]>;
  streaming_footprints_parallel?: Array<[string, string]>;
  streaming_footprints_terminate_all?: boolean;
}

function validateStreamingFootprintActivity(value: string): string {
  const normalized = value.trim();
  if (!normalized || normalized.includes(",") || normalized.includes("->"))
    throw new Error("streaming footprints activity names cannot be empty or contain ',' or '->'");
  return normalized;
}

function serializeStreamingFootprintList(values: string[], name: string): string {
  if (!Array.isArray(values) || values.length === 0)
    throw new Error(`${name} must be a non-empty list`);
  return values.map(validateStreamingFootprintActivity).join(",");
}

function serializeStreamingFootprintPairs(
  values: Array<[string, string]>, name: string,
): string {
  if (!Array.isArray(values) || values.length === 0)
    throw new Error(`${name} must be a non-empty list`);
  return values.map((pair) => {
    if (!Array.isArray(pair) || pair.length !== 2)
      throw new Error(`${name} must contain source/target pairs`);
    return `${validateStreamingFootprintActivity(pair[0])}->${validateStreamingFootprintActivity(pair[1])}`;
  }).join(",");
}

export function streamingFootprintsQuery(options: StreamingFootprintsOptions): string {
  const query = new URLSearchParams();
  for (const name of [
    "streaming_footprints_case_id_column",
    "streaming_footprints_activity_column",
  ] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${name} must not be empty`);
      query.set(name, value.trim());
    }
  }
  query.set("streaming_footprints_activities",
    serializeStreamingFootprintList(options.streaming_footprints_activities,
      "streaming_footprints_activities"));
  for (const name of [
    "streaming_footprints_start_activities",
    "streaming_footprints_end_activities",
  ] as const) {
    if (options[name] !== undefined)
      query.set(name, serializeStreamingFootprintList(options[name]!, name));
  }
  for (const name of ["streaming_footprints_sequence", "streaming_footprints_parallel"] as const) {
    if (options[name] !== undefined)
      query.set(name, serializeStreamingFootprintPairs(options[name]!, name));
  }
  if (options.streaming_footprints_terminate_all !== undefined)
    query.set("streaming_footprints_terminate_all",
      options.streaming_footprints_terminate_all ? "true" : "false");
  return query.toString();
}

export async function requestStreamingFootprints(
  log: Blob, options: StreamingFootprintsOptions,
): Promise<StreamingFootprintsResult> {
  const form = new FormData();
  const name = typeof File !== "undefined" && log instanceof File ? log.name : "log.csv";
  form.append("log", log, name);
  const query = streamingFootprintsQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/streaming-footprints?${query}`, {
      method: "POST", body: form,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload)
    throw new Error(String((payload as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isStreamingFootprintsResult(payload))
    throw new Error("the Minera backend returned invalid streaming footprints");
  return payload;
}

export interface StreamingTemporalOptions {
  streaming_temporal_profile: Array<[string, string, number, number]>;
  streaming_temporal_case_id_column?: string;
  streaming_temporal_activity_column?: string;
  streaming_temporal_start_timestamp_column?: string;
  streaming_temporal_timestamp_column?: string;
  streaming_temporal_zeta?: number;
}

function serializeStreamingTemporalProfile(
  values: Array<[string, string, number, number]>,
): string {
  if (!Array.isArray(values) || values.length === 0)
    throw new Error("streaming_temporal_profile must be a non-empty list");
  return values.map((entry) => {
    const source = validateStreamingFootprintActivity(entry[0]);
    const target = validateStreamingFootprintActivity(entry[1]);
    if (!Array.isArray(entry) || entry.length !== 4 ||
        !Number.isFinite(entry[2]) || entry[2] < 0 ||
        !Number.isFinite(entry[3]) || entry[3] < 0 ||
        source.includes(":") || target.includes(":")) {
      throw new Error("streaming temporal profile entries must be source/target/mean/stddev");
    }
    return `${source}->${target}:${entry[2]}:${entry[3]}`;
  }).join(",");
}

export function streamingTemporalQuery(options: StreamingTemporalOptions): string {
  const query = new URLSearchParams();
  query.set("streaming_temporal_profile",
    serializeStreamingTemporalProfile(options.streaming_temporal_profile));
  for (const name of [
    "streaming_temporal_case_id_column",
    "streaming_temporal_activity_column",
    "streaming_temporal_start_timestamp_column",
    "streaming_temporal_timestamp_column",
  ] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${name} must not be empty`);
      query.set(name, value.trim());
    }
  }
  if (options.streaming_temporal_zeta !== undefined) {
    if (!Number.isFinite(options.streaming_temporal_zeta) || options.streaming_temporal_zeta <= 0)
      throw new Error("streaming_temporal_zeta must be finite and greater than zero");
    query.set("streaming_temporal_zeta", String(options.streaming_temporal_zeta));
  }
  return query.toString();
}

export async function requestStreamingTemporal(
  log: Blob, options: StreamingTemporalOptions,
): Promise<StreamingTemporalResult> {
  const form = new FormData();
  const name = typeof File !== "undefined" && log instanceof File ? log.name : "log.csv";
  form.append("log", log, name);
  const query = streamingTemporalQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/streaming-temporal?${query}`, {
      method: "POST", body: form,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload)
    throw new Error(String((payload as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isStreamingTemporalResult(payload))
    throw new Error("the Minera backend returned invalid streaming temporal conformance");
  return payload;
}

export interface StreamingTbrOptions {
  streaming_tbr_variant?: "forward" | "backwards";
  streaming_tbr_case_id_column?: string;
  streaming_tbr_activity_column?: string;
  streaming_tbr_max_invisible?: number;
  streaming_tbr_stop_immediately?: boolean;
}

export function streamingTbrQuery(options: StreamingTbrOptions = {}): string {
  const query = new URLSearchParams();
  if (options.streaming_tbr_variant !== undefined) {
    if (options.streaming_tbr_variant !== "forward" && options.streaming_tbr_variant !== "backwards")
      throw new Error("streaming_tbr_variant must be forward or backwards");
    query.set("streaming_tbr_variant", options.streaming_tbr_variant);
  }
  for (const name of ["streaming_tbr_case_id_column", "streaming_tbr_activity_column"] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${name} must not be empty`);
      query.set(name, value.trim());
    }
  }
  if (options.streaming_tbr_max_invisible !== undefined) {
    if (!Number.isSafeInteger(options.streaming_tbr_max_invisible) || options.streaming_tbr_max_invisible < 0)
      throw new Error("streaming_tbr_max_invisible must be a non-negative integer");
    query.set("streaming_tbr_max_invisible", String(options.streaming_tbr_max_invisible));
  }
  if (options.streaming_tbr_stop_immediately !== undefined)
    query.set("streaming_tbr_stop_immediately", options.streaming_tbr_stop_immediately ? "true" : "false");
  return query.toString();
}

export async function requestStreamingTbr(
  model: Blob, log: Blob, options: StreamingTbrOptions = {},
): Promise<StreamingTbrResult> {
  const form = new FormData();
  const modelName = typeof File !== "undefined" && model instanceof File ? model.name : "model.pnml";
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "log.csv";
  form.append("model", model, modelName);
  form.append("log", log, logName);
  const query = streamingTbrQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/streaming-tbr${query ? `?${query}` : ""}`, {
      method: "POST", body: form,
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload)
    throw new Error(String((payload as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isStreamingTbrResult(payload))
    throw new Error("the Minera backend returned invalid streaming token replay");
  return payload;
}

/**
 * Simulates one miner's model into a synthetic event log served as standard
 * XES. The backend defaults to 100 traces for Petri models and 1,000 for
 * Declare. Declare classic playout is prefix-safe, so a generated trace may
 * end with pending obligations rather than being a complete fitting execution.
 * Like the other model endpoints this only works for a log mined this session.
 */
function validatePlayoutInteger(name: string, value: number, positive = false): void {
  if (!Number.isSafeInteger(value) || value < 0 || (positive && value === 0)) {
    throw new Error(`${name} must be a ${positive ? "positive" : "non-negative"} integer`);
  }
}

export async function comparePetriUsage(
  reference: File,
  miner = "inductive",
): Promise<PetriUsageComparison> {
  const form = new FormData();
  form.append("reference_log", reference, reference.name);
  form.append("miner", miner);
  let response: Response;
  try {
    response = await fetch("/api/compare-petri", { method: "POST", body: form });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isPetriUsageComparison(parsed)) throw new Error("the Minera backend returned an invalid Petri usage comparison");
  return parsed;
}

export function processTreePlayoutQuery(options: ProcessTreePlayoutOptions = {}): string {
  const params = new URLSearchParams();
  if (options.variant !== undefined) {
    if (options.variant !== "BASIC_PLAYOUT" && options.variant !== "EXTENSIVE" && options.variant !== "TOPBOTTOM") {
      throw new Error("variant must be BASIC_PLAYOUT, EXTENSIVE, or TOPBOTTOM");
    }
    params.set("variant", options.variant);
  }
  if (options.traces !== undefined) { validatePlayoutInteger("traces", options.traces, true); params.set("traces", String(options.traces)); }
  if (options.max_steps !== undefined) { validatePlayoutInteger("max_steps", options.max_steps); params.set("max_steps", String(options.max_steps)); }
  if (options.seed !== undefined) { validatePlayoutInteger("seed", options.seed); params.set("seed", String(options.seed)); }
  if (options.min_length !== undefined) { validatePlayoutInteger("min_length", options.min_length); params.set("min_length", String(options.min_length)); }
  if (options.max_length !== undefined) { validatePlayoutInteger("max_length", options.max_length); params.set("max_length", String(options.max_length)); }
  if (options.max_loop_occurrences !== undefined) { validatePlayoutInteger("max_loop_occurrences", options.max_loop_occurrences); params.set("max_loop_occurrences", String(options.max_loop_occurrences)); }
  if (options.max_trace_cap !== undefined) { validatePlayoutInteger("max_trace_cap", options.max_trace_cap, true); params.set("max_trace_cap", String(options.max_trace_cap)); }
  if (options.min_length !== undefined && options.max_length !== undefined && options.min_length > options.max_length) {
    throw new Error("min_length must not exceed max_length");
  }
  return params.toString();
}

export function stochasticPlayoutQuery(options: StochasticPlayoutOptions = {}): string {
  const params = new URLSearchParams();
  const integerFields: Array<[keyof StochasticPlayoutOptions, boolean]> = [
    ["traces", true], ["max_steps", false], ["seed", false],
  ];
  for (const [name, positive] of integerFields) {
    const value = options[name];
    if (value !== undefined) {
      validatePlayoutInteger(name, value as number, positive);
      params.set(`stochastic_${name}`, String(value));
    }
  }
  if (options.transition_weights !== undefined) {
    if (!Array.isArray(options.transition_weights) || options.transition_weights.length === 0 ||
        options.transition_weights.some((value) => !Number.isFinite(value) || value < 0)) {
      throw new Error("transition_weights must be a non-empty list of finite non-negative numbers");
    }
    params.set("stochastic_transition_weights", options.transition_weights.join(","));
  }
  if (options.stop_at_final !== undefined) {
    if (typeof options.stop_at_final !== "boolean") throw new Error("stop_at_final must be boolean");
    params.set("stochastic_stop_at_final", options.stop_at_final ? "true" : "false");
  }
  for (const name of ["start", "case_interval", "event_interval"] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (!Number.isFinite(value) || (name !== "start" && value < 0)) {
        throw new Error(`${name} must be a finite ${name === "start" ? "number" : "non-negative number"}`);
      }
      params.set(`stochastic_${name}`, String(value));
    }
  }
  return params.toString();
}

export function monteCarloQuery(options: MonteCarloOptions = {}): string {
  const params = new URLSearchParams();
  const positiveIntegers: Array<keyof MonteCarloOptions> = ["simulations"];
  for (const name of positiveIntegers) {
    const value = options[name];
    if (value !== undefined) {
      validatePlayoutNumber(name, value as number, true);
      params.set(`monte_carlo_${name}`, String(value));
    }
  }
  for (const name of ["max_steps", "seed", "default_resources_per_place"] as const) {
    const value = options[name];
    if (value !== undefined) {
      validatePlayoutNumber(name, value as number, name === "default_resources_per_place");
      params.set(`monte_carlo_${name}`, String(value));
    }
  }
  for (const name of ["case_arrival_seconds", "start", "default_duration_seconds"] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (!Number.isFinite(value) || (name !== "start" && value < 0)) {
        throw new Error(`${name} must be a finite ${name === "start" ? "number" : "non-negative number"}`);
      }
      params.set(`monte_carlo_${name}`, String(value));
    }
  }
  if (options.transition_weights !== undefined) {
    if (!Array.isArray(options.transition_weights) || options.transition_weights.length === 0 ||
        options.transition_weights.some((value) => !Number.isFinite(value) || value < 0)) {
      throw new Error("transition_weights must be a non-empty list of finite non-negative numbers");
    }
    params.set("monte_carlo_transition_weights", options.transition_weights.join(","));
  }
  if (options.transition_durations !== undefined) {
    if (!Array.isArray(options.transition_durations) || options.transition_durations.length === 0 ||
        options.transition_durations.some((duration) => {
          if (!duration || !Number.isFinite(duration.value_seconds) || duration.value_seconds < 0) return true;
          const distribution = duration.distribution;
          if (!["deterministic", "exponential", "constant0", "immediate", "normal", "uniform", "lognormal", "gamma"]
            .includes(distribution)) return true;
          const secondary = duration.secondary_seconds;
          const tertiary = duration.tertiary_seconds;
          if (secondary !== undefined && (!Number.isFinite(secondary) || secondary < 0)) return true;
          if (tertiary !== undefined && (!Number.isFinite(tertiary) || tertiary <= 0)) return true;
          if ((distribution === "normal" || distribution === "uniform") && secondary === undefined) return true;
          if ((distribution === "lognormal" || distribution === "gamma") &&
              (duration.value_seconds <= 0 || (tertiary !== undefined && tertiary <= 0))) return true;
          return false;
        })) {
      throw new Error("transition_durations must contain valid distribution parameters");
    }
    params.set("monte_carlo_transition_durations", options.transition_durations
      .map((duration) => {
        const distribution = duration.distribution;
        if (distribution === "constant0" || distribution === "immediate") return `${distribution}:0`;
        if (distribution === "normal" || distribution === "uniform") {
          return `${distribution}:${duration.value_seconds};${duration.secondary_seconds}`;
        }
        if (distribution === "lognormal" || distribution === "gamma") {
          return `${distribution}:${duration.value_seconds};${duration.secondary_seconds ?? 0};${duration.tertiary_seconds ?? 1}`;
        }
        return `${distribution}:${duration.value_seconds}`;
      }).join(","));
  }
  if (options.resources_per_place !== undefined) {
    if (!options.resources_per_place || typeof options.resources_per_place !== "object" ||
        Object.entries(options.resources_per_place).some(([place, count]) =>
          !/^\d+$/.test(place) || !Number.isSafeInteger(count) || count <= 0)) {
      throw new Error("resources_per_place must map non-negative places to positive integers");
    }
    const entries = Object.entries(options.resources_per_place)
      .sort(([left], [right]) => Number(left) - Number(right));
    params.set("monte_carlo_resources_per_place", entries.map(([place, count]) => `${place}=${count}`).join(","));
  }
  for (const name of ["enable_diagnostics", "stop_at_final"] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (typeof value !== "boolean") throw new Error(`${name} must be boolean`);
      params.set(`monte_carlo_${name}`, value ? "true" : "false");
    }
  }
  return params.toString();
}

function validatePlayoutNumber(name: string, value: number, positive = false): void {
  if (!Number.isFinite(value) || !Number.isSafeInteger(value) || value < 0 || (positive && value === 0)) {
    throw new Error(`${name} must be a ${positive ? "positive" : "non-negative"} integer`);
  }
}

export function dfgPlayoutQuery(options: DfgPlayoutOptions = {}): string {
  const params = new URLSearchParams();
  if (options.variant !== undefined) {
    if (options.variant !== "classic" && options.variant !== "performance") {
      throw new Error("variant must be classic or performance");
    }
    params.set("dfg_playout_variant", options.variant);
  }
  const positiveIntegers: Array<keyof DfgPlayoutOptions> = [
    "max_variants", "max_occurrences_per_activity",
    "min_variant_occurrences", "traces", "max_events_per_trace",
  ];
  for (const name of positiveIntegers) {
    const value = options[name];
    if (value !== undefined) {
      validatePlayoutNumber(name, value as number, true);
      params.set(`dfg_playout_${name}`, String(value));
    }
  }
  if (options.max_execution_time_ms !== undefined) {
    validatePlayoutNumber("max_execution_time_ms", options.max_execution_time_ms);
    params.set("dfg_playout_max_execution_time_ms", String(options.max_execution_time_ms));
  }
  if (options.max_steps !== undefined) {
    validatePlayoutNumber("max_steps", options.max_steps, true);
    params.set("dfg_playout_max_steps", String(options.max_steps));
  }
  if (options.min_weighted_probability !== undefined) {
    if (!Number.isFinite(options.min_weighted_probability) || options.min_weighted_probability < 0 || options.min_weighted_probability > 1) {
      throw new Error("min_weighted_probability must be between 0 and 1");
    }
    params.set("dfg_playout_min_weighted_probability", String(options.min_weighted_probability));
  }
  if (options.case_arrival_rate !== undefined) {
    if (!Number.isFinite(options.case_arrival_rate) || options.case_arrival_rate < 0) {
      throw new Error("case_arrival_rate must be non-negative");
    }
    params.set("dfg_playout_case_arrival_rate", String(options.case_arrival_rate));
  }
  if (options.seed !== undefined) {
    validatePlayoutNumber("seed", options.seed);
    params.set("dfg_playout_seed", String(options.seed));
  }
  for (const name of ["interrupt_when_dfg_complete", "add_trace_if_takes_new_edges", "return_variants", "return_only_if_complete"] as const) {
    const value = options[name];
    if (value !== undefined) params.set(`dfg_playout_${name}`, value ? "true" : "false");
  }
  if (options.start_time !== undefined) {
    if (!options.start_time.trim()) throw new Error("start_time must not be empty");
    params.set("dfg_playout_start_time", options.start_time);
  }
  return params.toString();
}

export async function fetchPlayout(miner: string, options: ProcessTreePlayoutOptions | StochasticPlayoutOptions | MonteCarloOptions | DfgPlayoutOptions = {}): Promise<string> {
  const monteCarlo = miner === "monte_carlo" || "simulations" in options ||
    "default_duration_seconds" in options || "transition_durations" in options ||
    "resources_per_place" in options;
  const stochastic = miner === "stochastic" || "transition_weights" in options || "stop_at_final" in options || "case_interval" in options || "event_interval" in options || "start" in options;
  let query: string;
  if (monteCarlo) {
    const controls = monteCarloQuery(options as MonteCarloOptions);
    query = `mode=monte_carlo${controls ? `&${controls}` : ""}`;
  } else if (stochastic) {
    const controls = stochasticPlayoutQuery(options as StochasticPlayoutOptions);
    query = `mode=stochastic${controls ? `&${controls}` : ""}`;
  } else {
    query = miner === "dfg" ? dfgPlayoutQuery(options as DfgPlayoutOptions) : processTreePlayoutQuery(options as ProcessTreePlayoutOptions);
  }
  let response: Response;
  try {
    response = await fetch(`/api/playout?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

export async function fetchMonteCarloResult(
  miner = "inductive", options: MonteCarloOptions = {},
): Promise<MonteCarloResult> {
  const controls = monteCarloQuery(options);
  let response: Response;
  try {
    response = await fetch(`/api/monte-carlo?miner=${encodeURIComponent(miner)}${controls ? `&${controls}` : ""}`);
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (parsed && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isMonteCarloResult(parsed)) throw new Error("the Minera backend returned an invalid Monte Carlo result");
  return parsed;
}

/**
 * Fetches one miner's model from the most recently mined log as BPMN 2.0.
 * The inductive model converts from its block-structured process tree; the
 * others convert from their workflow nets server-side. Like PNML this only
 * works for a log mined this session.
 */
export async function fetchBpmn(miner: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(`/api/bpmn?miner=${encodeURIComponent(miner)}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

export interface ConceptDriftOptions {
  drift_sub_log_size?: number;
  drift_window_size?: number;
  drift_num_permutations?: number;
  drift_threshold?: number;
  drift_max_no_change_points?: number;
  drift_seed?: number;
  drift_include_timestamps?: boolean;
  drift_activity_attribute?: string;
  drift_timestamp_attribute?: string;
  drift_case_id_attribute?: string;
}

export function conceptDriftQuery(options: ConceptDriftOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value !== undefined) query.set(key, String(value));
  }
  return query.toString();
}

export async function requestConceptDrift(options: ConceptDriftOptions = {}): Promise<ConceptDriftResult> {
  const query = conceptDriftQuery(options);
  const response = await fetch(`/api/concept-drift${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isConceptDriftResult(payload)) {
    throw new Error("the backend returned an invalid concept-drift response");
  }
  return payload;
}

export interface LogToTargetOptions {
  target_variant?: "next_activity" | "next_time" | "remaining_time";
  target_activity_key?: string;
  target_timestamp_key?: string;
  target_enable_padding?: boolean;
  target_pad_size?: number;
  target_activities?: string[];
}

export function logToTargetQuery(options: LogToTargetOptions = {}): string {
  const query = new URLSearchParams();
  const encodeActivity = (activity: string) => activity.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
  const entries: Array<[string, string | undefined]> = [
    ["target_activity_key", options.target_activity_key],
    ["target_activities", options.target_activities?.map(encodeActivity).join(",")],
    ["target_enable_padding", options.target_enable_padding === undefined ? undefined : String(options.target_enable_padding)],
    ["target_pad_size", options.target_pad_size === undefined ? undefined : String(options.target_pad_size)],
    ["target_timestamp_key", options.target_timestamp_key],
    ["target_variant", options.target_variant],
  ];
  for (const [key, value] of entries) if (value !== undefined) query.set(key, value);
  return query.toString();
}

export async function requestLogToTarget(options: LogToTargetOptions = {}): Promise<LogToTargetResult> {
  const query = logToTargetQuery(options);
  const response = await fetch(`/api/transform-target${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isLogToTargetResult(payload)) {
    throw new Error("the Minera backend returned an invalid target transformation");
  }
  return payload;
}

export interface LogUtilityOptions {
  variant?: "split_train_test" | "fixed_prefixes" | "all_prefixes" | "until_activity" | "to_activities" | "feature_selection";
  train_percentage?: number;
  seed?: number;
  prefix_length?: number;
  activity?: string;
  activity_key?: string;
  timestamp_key?: string;
  activities?: string[];
  feature_min_different_values?: number;
  feature_max_different_values?: number;
  feature_consider_all_attributes?: boolean;
  feature_mandatory_attributes?: string[];
}

export function logUtilityQuery(options: LogUtilityOptions = {}): string {
  const variant = options.variant ?? "split_train_test";
  if (!["split_train_test", "fixed_prefixes", "all_prefixes", "until_activity", "to_activities", "feature_selection"].includes(variant)) throw new Error("log utility variant is not supported");
  if (options.train_percentage !== undefined && (!Number.isFinite(options.train_percentage) || options.train_percentage < 0 || options.train_percentage > 1)) throw new Error("train_percentage must be in [0,1]");
  if (options.seed !== undefined && (!Number.isSafeInteger(options.seed) || options.seed < 0)) throw new Error("seed must be a non-negative integer");
  if (options.prefix_length !== undefined && (!Number.isSafeInteger(options.prefix_length) || options.prefix_length < 0 || options.prefix_length > 100000)) throw new Error("prefix_length exceeds the safety limit");
  if (variant === "fixed_prefixes" && options.prefix_length === undefined) throw new Error("prefix_length is required for fixed_prefixes");
  if (variant === "until_activity" && !options.activity) throw new Error("activity is required for until_activity");
  if (variant === "to_activities" && (!options.activities || options.activities.length === 0 || options.activities.some((activity) => !activity))) throw new Error("activities are required for to_activities");
  for (const [key, value] of [["feature_min_different_values", options.feature_min_different_values], ["feature_max_different_values", options.feature_max_different_values]] as const) if (value !== undefined && (!Number.isSafeInteger(value) || value < 0 || value > 100000)) throw new Error(`${key} must be a non-negative integer`);
  if (options.feature_min_different_values !== undefined && options.feature_max_different_values !== undefined && options.feature_min_different_values > options.feature_max_different_values) throw new Error("feature minimum exceeds maximum");
  const values: Record<string, string | undefined> = {
    activity: options.activity,
    activity_key: options.activity_key,
    activities: options.activities?.map((activity) => activity.replaceAll("\\", "\\\\").replaceAll(",", "\\,")).join(","),
    feature_consider_all_attributes: options.feature_consider_all_attributes === undefined ? undefined : String(options.feature_consider_all_attributes),
    feature_mandatory_attributes: options.feature_mandatory_attributes?.map((attribute) => attribute.replaceAll("\\", "\\\\").replaceAll(",", "\\,")).join(","),
    feature_max_different_values: options.feature_max_different_values === undefined ? undefined : String(options.feature_max_different_values),
    feature_min_different_values: options.feature_min_different_values === undefined ? undefined : String(options.feature_min_different_values),
    prefix_length: options.prefix_length === undefined ? undefined : String(options.prefix_length),
    seed: options.seed === undefined ? undefined : String(options.seed),
    timestamp_key: options.timestamp_key,
    train_percentage: options.train_percentage === undefined ? undefined : String(options.train_percentage),
    variant,
  };
  const query = new URLSearchParams();
  for (const key of Object.keys(values).sort()) if (values[key] !== undefined) query.set(key, values[key] as string);
  return query.toString();
}

export async function requestLogUtility(options: LogUtilityOptions = {}): Promise<LogUtilityResult> {
  const query = logUtilityQuery(options);
  const response = await fetch(`/api/log-util${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isLogUtilityResult(payload)) throw new Error("the Minera backend returned an invalid log utility result");
  return payload;
}

export interface LogToFeaturesOptions {
  features_variant?: "event_based" | "trace_based" | "temporal";
  features_activity_key?: string;
  features_timestamp_key?: string;
  features_start_timestamp_key?: string;
  features_resource_key?: string;
  features_window_seconds?: number;
  features_default_not_present?: number;
  features_event_string_attributes?: string[];
  features_event_numeric_attributes?: string[];
  features_case_string_attributes?: string[];
  features_case_numeric_attributes?: string[];
  features_event_attributes?: string[];
  features_case_attributes?: string[];
  features_event_successor_attributes?: string[];
  features_feature_names?: string[];
  features_enable_activity_def_representation?: boolean;
  features_enable_succ_def_representation?: boolean;
  features_enable_all_extra_features?: boolean;
  features_enable_case_duration?: boolean;
  features_add_case_identifier_column?: boolean;
  features_enable_times_from_first_occurrence?: boolean;
  features_enable_times_from_last_occurrence?: boolean;
  features_enable_direct_paths_times_last_occ?: boolean;
  features_enable_indirect_paths_times_last_occ?: boolean;
  features_enable_work_in_progress?: boolean;
  features_enable_resource_workload?: boolean;
  features_enable_first_last_activity_index?: boolean;
  features_enable_max_concurrent_events?: boolean;
  features_enable_max_concurrent_events_per_activity?: boolean;
  features_epsilon?: number;
  features_min_num_different_string_values?: number;
  features_max_num_different_string_values?: number;
}
export function logToFeaturesQuery(options: LogToFeaturesOptions = {}): string {
  if (options.features_epsilon !== undefined &&
      (!Number.isFinite(options.features_epsilon) || options.features_epsilon < 0))
    throw new Error("features_epsilon must be finite and non-negative");
  if (options.features_min_num_different_string_values !== undefined &&
      (!Number.isSafeInteger(options.features_min_num_different_string_values) || options.features_min_num_different_string_values < 0))
    throw new Error("features_min_num_different_string_values must be a non-negative integer");
  if (options.features_max_num_different_string_values !== undefined &&
      (!Number.isSafeInteger(options.features_max_num_different_string_values) || options.features_max_num_different_string_values < 0))
    throw new Error("features_max_num_different_string_values must be a non-negative integer");
  if (options.features_min_num_different_string_values !== undefined &&
      options.features_max_num_different_string_values !== undefined &&
      options.features_min_num_different_string_values > options.features_max_num_different_string_values)
    throw new Error("features string cardinality bounds are inverted");
  const query = new URLSearchParams();
  const encode = (s: string) => s.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value === undefined) continue;
    query.set(key, Array.isArray(value) ? value.map(encode).join(",") : String(value));
  }
  return query.toString();
}
export async function requestLogToFeatures(options: LogToFeaturesOptions = {}): Promise<LogToFeaturesResult> {
  const query = logToFeaturesQuery(options);
  const response = await fetch(`/api/log-to-features${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isLogToFeaturesResult(payload)) throw new Error("the Minera backend returned an invalid feature extraction");
  return payload;
}

export interface EmbeddingOptions {
  embedding_variant?: EmbeddingVariant;
  embedding_model?: string;
  embedding_attribute_key?: string;
  embedding_event_id_key?: string;
  embedding_dimensions?: number;
  embedding_max_items?: number;
  embedding_keep_cases?: boolean;
  embedding_target?: string;
  embedding_top_k?: number;
}
export function embeddingsQuery(options: EmbeddingOptions = {}): string {
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value !== undefined) query.set(key, String(value));
  }
  return query.toString();
}
export async function requestEmbeddings(options: EmbeddingOptions = {}): Promise<EmbeddingApiResult> {
  const query = embeddingsQuery(options);
  const response = await fetch(`/api/embeddings${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isEmbeddingApiResult(payload)) throw new Error("the Minera backend returned an invalid embedding result");
  return payload;
}

export interface LabelSplittingOptions {
  label_splitting_variant?: "contextual";
  label_splitting_activity_key?: string;
  label_splitting_target_key?: string;
  label_splitting_prefix_length?: number;
  label_splitting_suffix_length?: number;
  label_splitting_min_edge_weight?: number;
  label_splitting_activities_suffix?: string;
  label_splitting_target_activities?: string[];
}
export function labelSplittingQuery(options: LabelSplittingOptions = {}): string {
  const query = new URLSearchParams();
  const encode = (s: string) => s.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value === undefined) continue;
    query.set(key, Array.isArray(value) ? value.map(encode).join(",") : String(value));
  }
  return query.toString();
}
export async function requestLabelSplitting(options: LabelSplittingOptions = {}): Promise<LabelSplittingResult> {
  const query = labelSplittingQuery(options);
  const response = await fetch(`/api/label-splitting${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isLabelSplittingResult(payload)) throw new Error("the Minera backend returned an invalid label splitting result");
  return payload;
}

export interface TrieOptions {
  trie_activity_key?: string;
  trie_max_path_length?: number;
}

export function trieQuery(options: TrieOptions = {}): string {
  const query = new URLSearchParams();
  if (options.trie_activity_key !== undefined) query.set("trie_activity_key", options.trie_activity_key);
  if (options.trie_max_path_length !== undefined) query.set("trie_max_path_length", String(options.trie_max_path_length));
  return query.toString();
}

export async function requestTrie(options: TrieOptions = {}): Promise<TrieResult> {
  const query = trieQuery(options);
  const response = await fetch(`/api/trie${query ? `?${query}` : ""}`);
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isTrieResult(payload)) throw new Error("the Minera backend returned an invalid trie");
  return payload;
}

export async function requestTriePetriNet(options: TrieOptions = {}): Promise<TriePetriConversionResult> {
  const query = trieQuery(options);
  const response = await fetch(`/api/trie-to-petri${query ? `?${query}` : ""}`);
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isTriePetriConversionResult(payload))
    throw new Error("the Minera backend returned an invalid trie conversion");
  return payload;
}

export interface IntervalTreeOptions {
  interval_activity_key?: string;
  interval_start_timestamp_key?: string;
  interval_timestamp_key?: string;
  interval_epsilon?: number;
  interval_filter_activity_couple?: [string, string];
  interval_query_point?: number;
  interval_query_start?: number;
  interval_query_end?: number;
}

export function intervalTreeQuery(options: IntervalTreeOptions = {}): string {
  const query = new URLSearchParams();
  if (options.interval_activity_key !== undefined) query.set("interval_activity_key", options.interval_activity_key);
  if (options.interval_start_timestamp_key !== undefined) query.set("interval_start_timestamp_key", options.interval_start_timestamp_key);
  if (options.interval_timestamp_key !== undefined) query.set("interval_timestamp_key", options.interval_timestamp_key);
  if (options.interval_epsilon !== undefined) query.set("interval_epsilon", String(options.interval_epsilon));
  if (options.interval_filter_activity_couple !== undefined) {
    const escape = (value: string) => value.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
    query.set("interval_filter_activity_couple", options.interval_filter_activity_couple.map(escape).join(","));
  }
  if (options.interval_query_point !== undefined) {
    if (!Number.isFinite(options.interval_query_point))
      throw new Error("interval_query_point must be finite");
    query.set("interval_query_point", String(options.interval_query_point));
  }
  const queryStart = options.interval_query_start;
  const queryEnd = options.interval_query_end;
  if ((queryStart === undefined) !== (queryEnd === undefined))
    throw new Error("interval query range requires both start and end");
  if (queryStart !== undefined && queryEnd !== undefined) {
    if (!Number.isFinite(queryStart) || !Number.isFinite(queryEnd))
      throw new Error("interval query range must be finite");
    if (queryStart > queryEnd)
      throw new Error("interval query start must not exceed end");
    query.set("interval_query_start", String(queryStart));
    query.set("interval_query_end", String(queryEnd));
  }
  return query.toString();
}

export async function requestIntervalTree(options: IntervalTreeOptions = {}): Promise<IntervalTreeResult> {
  const query = intervalTreeQuery(options);
  const response = await fetch(`/api/interval-tree${query ? `?${query}` : ""}`);
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isIntervalTreeResult(payload)) throw new Error("the Minera backend returned an invalid interval tree");
  return payload;
}

export interface CaseRelationsOptions {
  merge_variant?: "pandas";
  merge_timestamp_key?: string;
  merge_left_column?: string;
  merge_right_column?: string;
}

export function caseRelationsQuery(options: CaseRelationsOptions = {}): string {
  const query = new URLSearchParams();
  if (options.merge_variant !== undefined) query.set("merge_variant", options.merge_variant);
  if (options.merge_timestamp_key !== undefined) query.set("merge_timestamp_key", options.merge_timestamp_key);
  if (options.merge_left_column !== undefined) query.set("merge_left_column", options.merge_left_column);
  if (options.merge_right_column !== undefined) query.set("merge_right_column", options.merge_right_column);
  return query.toString();
}

export async function requestCaseRelations(
  base: File,
  incoming: File,
  relations: File,
  options: CaseRelationsOptions = {},
): Promise<CaseRelationsResult> {
  const form = new FormData();
  form.append("base_log", base, base.name);
  form.append("incoming_log", incoming, incoming.name);
  form.append("case_relations", relations, relations.name);
  const query = caseRelationsQuery(options);
  const response = await fetch(`/api/merge-case-relations${query ? `?${query}` : ""}`, { method: "POST", body: form });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isCaseRelationsResult(payload)) throw new Error("the Minera backend returned an invalid case-relations merge");
  return payload;
}

export interface LinkAnalysisOptions {
  link_analysis_out_column?: string;
  link_analysis_in_column?: string;
  link_analysis_sorting_column?: string;
  link_analysis_look_forward?: boolean;
  link_analysis_keep_first_occurrence?: boolean;
  link_analysis_propagate?: boolean;
  link_analysis_max_links?: number;
}

export function linkAnalysisQuery(options: LinkAnalysisOptions = {}): string {
  const query = new URLSearchParams();
  const add = (name: string, value: string | number | boolean | undefined) => {
    if (value !== undefined) query.set(name, String(value));
  };
  add("link_analysis_out_column", options.link_analysis_out_column);
  add("link_analysis_in_column", options.link_analysis_in_column);
  add("link_analysis_sorting_column", options.link_analysis_sorting_column);
  add("link_analysis_look_forward", options.link_analysis_look_forward);
  add("link_analysis_keep_first_occurrence", options.link_analysis_keep_first_occurrence);
  add("link_analysis_propagate", options.link_analysis_propagate);
  add("link_analysis_max_links", options.link_analysis_max_links);
  return query.toString();
}

export async function requestLinkAnalysis(
  options: LinkAnalysisOptions = {},
): Promise<LinkAnalysisResult> {
  const query = linkAnalysisQuery(options);
  const response = await fetch(`/api/link-analysis${query ? `?${query}` : ""}`, {
    method: "POST",
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isLinkAnalysisResult(payload)) throw new Error("the Minera backend returned an invalid link analysis");
  return payload;
}

export interface TimestampInterleavingsOptions {
  interleavings_timestamp_key?: string;
  interleavings_max_results?: number;
  interleavings_left_column?: string;
  interleavings_right_column?: string;
}

export function timestampInterleavingsQuery(options: TimestampInterleavingsOptions = {}): string {
  const query = new URLSearchParams();
  const add = (name: string, value: string | number | undefined) => {
    if (value !== undefined) query.set(name, String(value));
  };
  add("interleavings_timestamp_key", options.interleavings_timestamp_key);
  add("interleavings_max_results", options.interleavings_max_results);
  add("interleavings_left_column", options.interleavings_left_column);
  add("interleavings_right_column", options.interleavings_right_column);
  return query.toString();
}

export async function requestTimestampInterleavings(
  left: File,
  right: File,
  relations: File,
  options: TimestampInterleavingsOptions = {},
): Promise<TimestampInterleavingsResult> {
  const form = new FormData();
  form.append("left", left, left.name);
  form.append("right", right, right.name);
  form.append("relations", relations, relations.name);
  const query = timestampInterleavingsQuery(options);
  const response = await fetch(`/api/timestamp-interleavings${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isTimestampInterleavingsResult(payload)) throw new Error("the Minera backend returned an invalid timestamp interleavings result");
  return payload;
}

export interface RandomTreeOptions {
  random_tree_variant?: "basic" | "ptandloggenerator";
  random_tree_rec_depth?: number;
  random_tree_min_rec_depth?: number;
  random_tree_max_rec_depth?: number;
  random_tree_prob_leaf?: number;
  random_tree_mode?: number;
  random_tree_min?: number;
  random_tree_max?: number;
  random_tree_sequence?: number;
  random_tree_choice?: number;
  random_tree_parallel?: number;
  random_tree_loop?: number;
  random_tree_or?: number;
  random_tree_silent?: number;
  random_tree_duplicate?: number;
  random_tree_no_models?: number;
  random_tree_seed?: number;
  random_tree_activities?: string[];
}

export function randomTreeQuery(options: RandomTreeOptions = {}): string {
  const query = new URLSearchParams();
  if (options.random_tree_variant !== undefined) query.set("random_tree_variant", options.random_tree_variant);
  if (options.random_tree_rec_depth !== undefined) query.set("random_tree_rec_depth", String(options.random_tree_rec_depth));
  if (options.random_tree_min_rec_depth !== undefined) query.set("random_tree_min_rec_depth", String(options.random_tree_min_rec_depth));
  if (options.random_tree_max_rec_depth !== undefined) query.set("random_tree_max_rec_depth", String(options.random_tree_max_rec_depth));
  if (options.random_tree_prob_leaf !== undefined) query.set("random_tree_prob_leaf", String(options.random_tree_prob_leaf));
  if (options.random_tree_mode !== undefined) query.set("random_tree_mode", String(options.random_tree_mode));
  if (options.random_tree_min !== undefined) query.set("random_tree_min", String(options.random_tree_min));
  if (options.random_tree_max !== undefined) query.set("random_tree_max", String(options.random_tree_max));
  if (options.random_tree_sequence !== undefined) query.set("random_tree_sequence", String(options.random_tree_sequence));
  if (options.random_tree_choice !== undefined) query.set("random_tree_choice", String(options.random_tree_choice));
  if (options.random_tree_parallel !== undefined) query.set("random_tree_parallel", String(options.random_tree_parallel));
  if (options.random_tree_loop !== undefined) query.set("random_tree_loop", String(options.random_tree_loop));
  if (options.random_tree_or !== undefined) query.set("random_tree_or", String(options.random_tree_or));
  if (options.random_tree_silent !== undefined) query.set("random_tree_silent", String(options.random_tree_silent));
  if (options.random_tree_duplicate !== undefined) query.set("random_tree_duplicate", String(options.random_tree_duplicate));
  if (options.random_tree_no_models !== undefined) query.set("random_tree_no_models", String(options.random_tree_no_models));
  if (options.random_tree_seed !== undefined) query.set("random_tree_seed", String(options.random_tree_seed));
  if (options.random_tree_activities !== undefined) {
    const escape = (value: string) => value.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
    query.set("random_tree_activities", options.random_tree_activities.map(escape).join(","));
  }
  return query.toString();
}

export async function requestRandomTree(options: RandomTreeOptions = {}): Promise<RandomTreeResult> {
  const query = randomTreeQuery(options);
  const response = await fetch(`/api/random-tree${query ? `?${query}` : ""}`);
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isRandomTreeResult(payload)) throw new Error("the Minera backend returned an invalid random tree");
  return payload;
}

export interface OcelDiscoveryOptions {
  variant?: OcelDiscoveryVariant;
  compute_edges_performance?: boolean;
  ocpn_annotation?: "annotated" | "none";
}

export function ocelDiscoveryQuery(options: OcelDiscoveryOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  if (options.compute_edges_performance !== undefined) {
    query.set("compute_edges_performance", options.compute_edges_performance ? "true" : "false");
  }
  if (options.ocpn_annotation !== undefined) query.set("ocpn_annotation", options.ocpn_annotation);
  return query.toString();
}

export async function requestOcelDiscovery(
  log: Blob,
  options: OcelDiscoveryOptions = {},
): Promise<OcelDiscoveryResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelDiscoveryQuery(options);
  const response = await fetch(`/api/ocel/discover${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelDiscoveryResult(payload)) throw new Error("the Minera backend returned an invalid OCEL discovery");
  return payload;
}

/** Imports an OCEL 2 relational SQLite upload through the local backend. */
export async function requestOcelSqlite(log: Blob): Promise<OcelSqliteResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.sqlite";
  form.append("log", log, logName);
  const response = await fetch("/api/ocel/sqlite", { method: "POST", body: form });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelSqliteResult(payload)) throw new Error("the Minera backend returned an invalid OCEL SQLite import");
  return payload;
}

/** Exports an OCEL JSON document as a standard SQLite download. */
export async function requestOcelSqliteExport(
  log: Record<string, unknown>,
): Promise<Blob> {
  const response = await fetch("/api/ocel/sqlite?mode=export", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(log),
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload = await response.blob();
  const signature = new TextDecoder().decode(
    await payload.slice(0, 16).arrayBuffer(),
  );
  if (signature !== "SQLite format 3\0")
    throw new Error("the Minera backend returned an invalid SQLite export");
  return payload;
}

export function ocelExchangeQuery(
  format: OcelExchangeFormat,
  mode: "import" | "export",
): string {
  const query = new URLSearchParams();
  query.set("format", format);
  query.set("mode", mode);
  return query.toString();
}

/** Imports classic or standard OCEL JSON/XML through the local backend. */
export async function requestOcelExchangeImport(
  log: Blob,
  format: OcelExchangeFormat = "json",
  objects?: Blob,
): Promise<OcelExchangeResult> {
  const form = new FormData();
  if (format === "csv_objects")
    throw new Error("OCEL csv_objects is an export-only format");
  const extension = format.startsWith("xml") ? "xml" : format === "csv" ? "csv" : "json";
  const logName = typeof File !== "undefined" && log instanceof File
    ? log.name
    : `ocel.${extension}`;
  form.append("log", log, logName);
  if (objects !== undefined) {
    if (format !== "csv") throw new Error("OCEL object-table uploads require csv format");
    const objectName = typeof File !== "undefined" && objects instanceof File
      ? objects.name
      : "ocel_objects.csv";
    form.append("objects", objects, objectName);
  }
  const response = await fetch(`/api/ocel/io?${ocelExchangeQuery(format, "import")}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelExchangeResult(payload))
    throw new Error("the Minera backend returned an invalid OCEL exchange import");
  return payload;
}

/** Exports an OCEL JSON document as classic or standard JSON/XML. */
export async function requestOcelExchangeExport(
  log: Record<string, unknown>,
  format: OcelExchangeFormat = "json",
): Promise<Blob> {
  const response = await fetch(`/api/ocel/io?${ocelExchangeQuery(format, "export")}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(log),
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload = await response.blob();
  const text = await payload.text();
  if (format.startsWith("json")) {
    let parsed: unknown;
    try { parsed = JSON.parse(text); } catch { throw new Error("the Minera backend returned invalid OCEL JSON"); }
    const isRecord = (value: unknown): value is Record<string, unknown> =>
      !!value && typeof value === "object" && !Array.isArray(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed) ||
        !isRecord((parsed as Record<string, unknown>)["ocel:events"]) ||
        !isRecord((parsed as Record<string, unknown>)["ocel:objects"])) {
      throw new Error("the Minera backend returned invalid OCEL JSON");
    }
  } else if (format.startsWith("xml")) {
    if (!text.trimStart().startsWith("<"))
      throw new Error("the Minera backend returned invalid OCEL XML");
  } else if (!text.trim()) {
    throw new Error("the Minera backend returned empty OCEL CSV");
  } else if (format !== "csv" && format !== "csv_objects") {
    throw new Error("the Minera backend returned invalid OCEL XML");
  }
  return payload;
}

export type OcelFilterSelector =
  | "event_attribute" | "object_attribute" | "object_types" | "objects"
  | "events" | "timestamp" | "activity_type_matching" | "object_count"
  | "start_events" | "end_events" | "activities_object_type" | "cc_object"
  | "cc_length" | "cc_object_type" | "cc_activity";
export interface OcelFilterOptions {
  variant?: OcelFilterSelector;
  attribute_key?: string;
  attribute_values?: Array<string | number | boolean | null>;
  positive?: boolean;
  types?: string[];
  ids?: string[];
  event_ids?: string[];
  from?: string;
  to?: string;
  object_type?: string;
  activity?: string;
  minimum_counts?: Record<string, number>;
  level?: number;
  object_id?: string;
  min_size?: number;
  max_size?: number;
  allowed?: Record<string, string[]>;
}

export function ocelFilterQuery(options: OcelFilterOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  if (options.attribute_key !== undefined) query.set("attribute_key", options.attribute_key);
  const escapePart = (value: string) => value.replaceAll("\\", "\\\\")
    .replaceAll(",", "\\,").replaceAll(":", "\\:")
    .replaceAll(";", "\\;").replaceAll("|", "\\|");
  const addList = (name: string, values: string[] | undefined) => {
    if (values?.length) query.set(name, values.map(escapePart).join(","));
  };
  if (options.attribute_values?.length) {
    const values = options.attribute_values.map((value) => {
      if (value === null) return "n:";
      if (typeof value === "boolean") return `b:${value ? "true" : "false"}`;
      if (typeof value === "number") {
        if (!Number.isFinite(value)) throw new Error("OCEL filter attribute values must be finite");
        if (Number.isInteger(value)) return `i:${value}`;
        return `d:${value}`;
      }
      return `s:${escapePart(value)}`;
    });
    query.set("attribute_values", values.join(","));
  }
  addList("types", options.types);
  addList("ids", options.ids);
  addList("event_ids", options.event_ids);
  if (options.positive !== undefined) query.set("positive", options.positive ? "true" : "false");
  if (options.from !== undefined) query.set("from", options.from);
  if (options.to !== undefined) query.set("to", options.to);
  if (options.object_type !== undefined) query.set("object_type", options.object_type);
  if (options.activity !== undefined) query.set("activity", options.activity);
  if (options.level !== undefined) {
    if (!Number.isInteger(options.level) || options.level < 0 || options.level > 32) throw new Error("OCEL filter level must be an integer from 0 through 32");
    query.set("level", String(options.level));
  }
  if (options.minimum_counts !== undefined) {
    const entries = Object.entries(options.minimum_counts).sort(([left], [right]) => left < right ? -1 : left > right ? 1 : 0);
    if (entries.some(([type, count]) => !type || !Number.isInteger(count) || count < 0)) {
      throw new Error("OCEL filter minimum counts must be non-negative integers");
    }
    if (entries.length) query.set("minimum_counts", entries.map(([type, count]) => `${escapePart(type)}:${count}`).join(","));
  }
  if (options.object_id !== undefined) query.set("object_id", options.object_id);
  const addSize = (name: string, value: number | undefined) => {
    if (value !== undefined) {
      if (!Number.isInteger(value) || value < 0) throw new Error(`OCEL filter ${name} must be a non-negative integer`);
      query.set(name, String(value));
    }
  };
  addSize("min_size", options.min_size);
  addSize("max_size", options.max_size);
  if (options.allowed !== undefined) {
    const entries = Object.entries(options.allowed).sort(([left], [right]) => left < right ? -1 : left > right ? 1 : 0);
    if (entries.some(([type, activities]) => !type || !activities.length || activities.some((activity) => !activity))) {
      throw new Error("OCEL filter allowed activities require non-empty types and activities");
    }
    if (entries.length) query.set("allowed", entries.map(([type, activities]) => `${escapePart(type)}:${activities.map(escapePart).join("|")}`).join(";"));
  }
  return query.toString();
}

export async function requestOcelFilter(
  log: Blob,
  options: OcelFilterOptions = {},
): Promise<OcelFilterResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelFilterQuery(options);
  const response = await fetch(`/api/ocel/filter${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelFilterResult(payload)) throw new Error("the Minera backend returned an invalid OCEL filter result");
  return payload;
}

export interface OcelSplitOptions {
  variant?: "connected_components" | "ancestors_descendants";
  object_type?: string;
  max_objects?: number;
  max_degree_centrality?: number;
}

export function ocelSplitQuery(options: OcelSplitOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  if (options.object_type !== undefined) query.set("object_type", options.object_type);
  if (options.max_objects !== undefined) query.set("max_objects", String(options.max_objects));
  if (options.max_degree_centrality !== undefined) {
    query.set("max_degree_centrality", String(options.max_degree_centrality));
  }
  return query.toString();
}

export async function requestOcelSplit(
  log: Blob,
  options: OcelSplitOptions = {},
): Promise<OcelSplitResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelSplitQuery(options);
  const response = await fetch(`/api/ocel/split${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelSplitResult(payload)) throw new Error("the Minera backend returned an invalid OCEL split");
  return payload;
}

export interface OcelPlayoutOptions {
  model?: "ocpn" | "causal_net";
  variant?: "extensive";
  max_bindings_per_activity?: number;
  branching_factor_transitions?: number;
  branching_factor_bindings?: number;
  max_traces?: number;
  max_states?: number;
  objects?: Record<string, string[]>;
  objects_unique_per_trace?: boolean;
  event_binding?: "independent" | "shared_activity" | "synchronized";
  exists_trace?: boolean;
  return_sequences?: boolean;
  start?: number;
  case_interval?: number;
  event_interval?: number;
  ocpn_variant?: "log" | "dfg";
  ocpn_annotation?: "annotated" | "none";
  double_arc_threshold?: number;
  noise_threshold?: number;
}

function escapeOcelPlayoutPart(value: string): string {
  return value.replaceAll("\\", "\\\\")
    .replaceAll(":", "\\:")
    .replaceAll(",", "\\,")
    .replaceAll(";", "\\;");
}

export function ocelPlayoutQuery(options: OcelPlayoutOptions = {}): string {
  const query = new URLSearchParams();
  if (options.model !== undefined) query.set("model", options.model);
  if (options.variant !== undefined) query.set("variant", options.variant);
  const addInteger = (name: string, value: number | undefined) => {
    if (value !== undefined && Number.isInteger(value) && value >= 0)
      query.set(name, String(value));
  };
  addInteger("max_bindings_per_activity", options.max_bindings_per_activity);
  addInteger("branching_factor_transitions", options.branching_factor_transitions);
  addInteger("branching_factor_bindings", options.branching_factor_bindings);
  addInteger("max_traces", options.max_traces);
  addInteger("max_states", options.max_states);
  if (options.objects !== undefined) {
    const entries = Object.entries(options.objects)
      .sort(([left], [right]) => left < right ? -1 : left > right ? 1 : 0)
      .map(([type, ids]) => {
        if (!type || !Array.isArray(ids) || ids.some((id) => !id))
          throw new Error("OCEL playout objects require non-empty types and ids");
        return `${escapeOcelPlayoutPart(type)}:${ids.map(escapeOcelPlayoutPart).join(",")}`;
      });
    if (entries.length) query.set("objects", entries.join(";"));
  }
  if (options.objects_unique_per_trace !== undefined)
    query.set("objects_unique_per_trace", options.objects_unique_per_trace ? "true" : "false");
  if (options.event_binding !== undefined &&
      options.event_binding !== "independent" &&
      options.event_binding !== "shared_activity" &&
      options.event_binding !== "synchronized") {
    throw new Error("OCEL playout event_binding is invalid");
  }
  if (options.event_binding !== undefined)
    query.set("event_binding", options.event_binding);
  if (options.exists_trace !== undefined)
    query.set("exists_trace", options.exists_trace ? "true" : "false");
  if (options.return_sequences !== undefined)
    query.set("return_sequences", options.return_sequences ? "true" : "false");
  const addNumber = (name: string, value: number | undefined) => {
    if (value !== undefined && Number.isFinite(value)) query.set(name, String(value));
  };
  addNumber("start", options.start);
  addNumber("case_interval", options.case_interval);
  addNumber("event_interval", options.event_interval);
  if (options.ocpn_variant !== undefined) query.set("ocpn_variant", options.ocpn_variant);
  if (options.ocpn_annotation !== undefined) query.set("ocpn_annotation", options.ocpn_annotation);
  addNumber("double_arc_threshold", options.double_arc_threshold);
  addNumber("noise_threshold", options.noise_threshold);
  return query.toString();
}

export async function requestOcelPlayout(
  log: Blob,
  options: OcelPlayoutOptions = {},
): Promise<OcelPlayoutResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelPlayoutQuery(options);
  const response = await fetch(`/api/ocel/playout${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelPlayoutResult(payload)) throw new Error("the Minera backend returned an invalid OCEL playout");
  return payload;
}

export interface OcelFeatureOptions {
  variant?: "events" | "objects" | "events_objects";
  max_features?: number;
  include_activity_features?: boolean;
  include_timestamp_features?: boolean;
  include_relation_features?: boolean;
  include_endpoint_features?: boolean;
  include_attribute_counts?: boolean;
  include_interaction_features?: boolean;
  include_lifecycle_features?: boolean;
  numeric_event_attributes?: string[];
  string_event_attributes?: string[];
  numeric_object_attributes?: string[];
  string_object_attributes?: string[];
}

export function ocelFeaturesQuery(options: OcelFeatureOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  const addInteger = (name: string, value: number | undefined) => {
    if (value !== undefined && Number.isInteger(value) && value > 0)
      query.set(name, String(value));
  };
  addInteger("max_features", options.max_features);
  const addBoolean = (name: string, value: boolean | undefined) => {
    if (value !== undefined) query.set(name, value ? "true" : "false");
  };
  addBoolean("include_activity_features", options.include_activity_features);
  addBoolean("include_timestamp_features", options.include_timestamp_features);
  addBoolean("include_relation_features", options.include_relation_features);
  addBoolean("include_endpoint_features", options.include_endpoint_features);
  addBoolean("include_attribute_counts", options.include_attribute_counts);
  addBoolean("include_interaction_features", options.include_interaction_features);
  addBoolean("include_lifecycle_features", options.include_lifecycle_features);
  const addList = (name: string, values: string[] | undefined) => {
    if (values?.length) {
      const escape = (value: string) => value.replaceAll("\\", "\\\\")
        .replaceAll(",", "\\,").replaceAll(":", "\\:")
        .replaceAll(";", "\\;").replaceAll("|", "\\|");
      query.set(name, [...values].sort().map(escape).join(","));
    }
  };
  addList("numeric_event_attributes", options.numeric_event_attributes);
  addList("string_event_attributes", options.string_event_attributes);
  addList("numeric_object_attributes", options.numeric_object_attributes);
  addList("string_object_attributes", options.string_object_attributes);
  return query.toString();
}

export async function requestOcelFeatures(
  log: Blob,
  options: OcelFeatureOptions = {},
): Promise<OcelFeatureResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelFeaturesQuery(options);
  const response = await fetch(`/api/ocel/features${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelFeatureResult(payload)) throw new Error("the Minera backend returned an invalid OCEL feature table");
  return payload;
}

export interface OcelStatisticsOptions {
  variant?: "object_type_activities" | "objects_ot_count" |
    "activity_associations" | "object_type_activity_associations" | "edge_associations";
  prefilter?: "none" | "start" | "end";
  business_hours?: boolean;
  business_hour_slots?: string;
}

export function ocelStatisticsQuery(options: OcelStatisticsOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  if (options.prefilter !== undefined && options.prefilter !== "none")
    query.set("prefilter", options.prefilter);
  if (options.business_hours !== undefined)
    query.set("business_hours", options.business_hours ? "true" : "false");
  const slots = options.business_hour_slots?.trim();
  if (slots) {
    if (options.business_hours === false)
      throw new Error("business-hour slots require business_hours=true");
    const valid = slots.split(",").every((part) => {
      const separator = part.includes(":") ? part.indexOf(":") : part.indexOf("-");
      if (separator <= 0 || separator === part.length - 1) return false;
      const start = Number(part.slice(0, separator));
      const end = Number(part.slice(separator + 1));
      return Number.isFinite(start) && Number.isFinite(end) &&
        start >= 0 && start < end && end <= 604800;
    });
    if (!valid) throw new Error("business-hour slots must satisfy 0 <= start < end <= 604800");
    if (options.business_hours === undefined) query.set("business_hours", "true");
    query.set("business_hour_slots", slots);
  }
  return query.toString();
}

export async function requestOcelStatistics(
  log: Blob,
  options: OcelStatisticsOptions = {},
): Promise<OcelStatisticsResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelStatisticsQuery(options);
  const response = await fetch(`/api/ocel/statistics${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelStatisticsResult(payload)) throw new Error("the Minera backend returned an invalid OCEL statistics result");
  return payload;
}

export interface OcelDescriptionOptions {
  variant?: "variant1" | "variant2";
  include_timestamps?: boolean;
}

export function ocelDescriptionQuery(options: OcelDescriptionOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  if (options.include_timestamps !== undefined)
    query.set("include_timestamps", options.include_timestamps ? "true" : "false");
  return query.toString();
}

export async function requestOcelDescription(
  log: Blob,
  options: OcelDescriptionOptions = {},
): Promise<OcelDescriptionResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelDescriptionQuery(options);
  const response = await fetch(`/api/ocel/description${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelDescriptionResult(payload)) throw new Error("the Minera backend returned an invalid OCEL description result");
  return payload;
}

export interface OcelConformanceOptions {
  variant?: OcelConformanceVariant;
  alpha?: number;
  beta?: number;
  gamma?: number;
  delta?: number;
  etot_theta?: number;
  otg_theta?: number;
  ocdfg_theta_activity?: number;
  ocdfg_theta_flow?: number;
}

export function ocelConformanceQuery(options: OcelConformanceOptions = {}): string {
  const query = new URLSearchParams();
  const add = (name: string, value: number | string | undefined) => {
    if (value !== undefined) query.set(name, String(value));
  };
  add("variant", options.variant);
  add("alpha", options.alpha);
  add("beta", options.beta);
  add("gamma", options.gamma);
  add("delta", options.delta);
  add("etot_theta", options.etot_theta);
  add("otg_theta", options.otg_theta);
  add("ocdfg_theta_activity", options.ocdfg_theta_activity);
  add("ocdfg_theta_flow", options.ocdfg_theta_flow);
  return query.toString();
}

export async function requestOcelConformance(
  log: Blob,
  normative: Blob,
  options: OcelConformanceOptions = {},
): Promise<OcelConformanceResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  const normativeName = typeof File !== "undefined" && normative instanceof File ? normative.name : "normative.json";
  form.append("log", log, logName);
  form.append("normative", normative, normativeName);
  const query = ocelConformanceQuery(options);
  const response = await fetch(`/api/ocel/conformance${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  const payload: unknown = await response.json();
  if (!isOcelConformanceResult(payload)) throw new Error("the Minera backend returned an invalid OCEL conformance result");
  return payload;
}
