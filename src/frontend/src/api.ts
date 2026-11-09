import {appendNativeMultipartField} from "./native-multipart";
import {hasExactFilters,exactFilterFields,appendExactFilterFiles,readExactFilterResponse,OwnedExactFilterResult,type ExactFilterControls} from "./exact-filter-controls";
import {hasExactKnowledge,exactKnowledgeQuery,requestExactKnowledge,type ExactKnowledgeRequest,type ExactKnowledgeEnvelope,type KnowledgeRequest} from "./exact-knowledge";
import {requireExactOtgEnvelope,OTG_THRESHOLD_FIELD,appendOtgThresholds,otgThresholdQueryText,validateOtgThresholdOptions,bindOtgConformanceMetadata} from "./otg-thresholds";
import {parseNativeJson} from "./native-json";
import { appendOcpnDiscoveryFlags, type OcpnDiscoveryFlags } from "./ocpn-discovery-options";
import {discoveryStringVectorJson, type DiscoveryNativeString} from "./discovery-string-vectors";
import {
  type ActivityStat,
  type Alignments,
  type AlignmentOptions,
  DECLARE_TEMPLATES,
  FOOTPRINTS_CONFORMANCE_VARIANTS,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
  FOOTPRINTS_DISCOVERY_VARIANTS,
  type FootprintsConformanceVariant,
  type FootprintsDiscoveryVariant,
  type FootprintsModelVariant,
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
  type DfgDataframeOptions,
  type DfgDataframeResult,
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
  type LogConversionResult,
  type LogUtilityResult,
  type EmbeddingVariant,
  type EmbeddingProvider,
  type EmbeddingApiResult,
  type BrowserHistoryVariant,
  type BrowserHistoryResult,
  type WindowsEventsResult,
  type RepositoryHistoryResult,
  isRepositoryHistoryResult,
  type WorkflowHistoryResult,
  isWorkflowHistoryResult,
  type EnterpriseHistoryResult,
  isEnterpriseHistoryResult,
  type CommunicationHistoryResult,
  isCommunicationHistoryResult,
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
  type OcelEnrichmentGraph,
  type OcelEnrichmentResult,
  type OcelObjectGraphResult,
  type OcelHeterogeneousGraphResult,
  type OcelPlayoutResult,
  type OcelFeatureResult,
  type OcelInteractionGraphSource,
  type OcelObjectNeighborGraph,
  type OcelStatisticsResult,
  type OcelDescriptionResult,
  type ProcessCubeTable,
  type ProcessCubeResult,
  type StreamingDfgResult,
  type StreamingFootprintsResult,
  type StreamingTemporalResult,
  type TemporalConformanceResult,
  type StreamingTbrResult,
  type TriePetriConversionResult,
  type ProcessTreePetriNetVariant,
  type ProcessTreePetriNetConversionResult,
  type PetriNetReductionResult,
  type ILPMinerOptions,
  type ILPMinerResult,
  type GeneticMinerOptions,
  type GeneticMinerResult,
  type GeneticMatrixConversionResult,
  type TraceVariantQueryOptions,
  type TraceVariantQueryResult,
  type PripelOptions,
  type PripelResult,
  type OrganizationalSnaOptions,
  type OrganizationalSnaResult,
  isLogToTargetResult,
  isConceptDriftResult,
  isLogToFeaturesResult,
  isLogConversionResult,
  isLogUtilityResult,
  isEmbeddingApiResult,
  isBrowserHistoryResult,
  isWindowsEventsResult,
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
  isOcelEnrichmentResult,
  isOcelObjectGraphResult,
  isOcelHeterogeneousGraphResult,
  isOcelPlayoutResult,
  isOcelFeatureResult,
  isOcelStatisticsResult,
  isOcelDescriptionResult,
  isProcessCubeResult,
  isStreamingDfgResult,
  isStreamingFootprintsResult,
  isStreamingTemporalResult,
  isTemporalConformanceResult,
  isStreamingTbrResult,
  isDataPetriNetResult,
  isDecisionTablesResult,
  isProcessCubeTable,
  isTriePetriConversionResult,
  isProcessTreePetriNetConversionResult,
  isPetriNetReductionResult,
  isDfgPrecisionResult,
  isILPMinerResult,
  isGeneticMinerResult,
  isGeneticMatrixConversionResult,
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
  type DeclarePlayoutOptions,
  type ProcessTreePlayoutSetResult,
  type BasicPlayoutOptions,
  type BasicPlayoutVisitedResult,
  type StochasticPlayoutOptions,
  type StochasticPlayoutVisitedResult,
  type ExtensivePlayoutVisitedResult,
  type StochasticPetriOptions,
  type StochasticPetriResult,
  type StochasticPetriDistributionResult,
  type StochasticMapOptions,
  type StochasticMapResult,
  type StochasticMapTransition,
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
  type DfgPlayoutVariantsResult,
  type PetriUsageComparison,
  type PetriUsageComparisonOptions,
  isDfgAlignmentResult,
  isDfgPetriConversionResult,
  isDfgDataframeResult,
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
  isDfgPlayoutVariantsResult,
  isBasicPlayoutVisitedResult,
  expandPlayoutVisitedResult,
  isStochasticPlayoutVisitedResult,
  isExtensivePlayoutVisitedResult,
  isStochasticPetriResult,
  isStochasticPetriDistributionResult,
  isStochasticMapResult,
  isProcessTreePlayoutSetResult,
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
import { nativeJson } from "./native-json";
import { readAlignmentResponse } from "./alignment-inspection";
import { nativeResponseFailure, readNativeResponse } from "./native-response";
import { importedModelFetch, importedModelIdOf, setImportedModelId } from "./imported-model-binding";
import {isActiveSourceMining, miningSourceFields, type ActiveDatasetResult, type MiningInputOptions} from "./active-mining-source";
import {markLiveTraces} from "./mine-traces";
import {waitForCanceledMine} from "./mine-progress";

export interface CsvOptions {
  caseColumn?: string;
  activityColumn?: string;
  timestampColumn?: string;
  delimiter?: string;
  xesVariant?: "iterparse" | "line_by_line" | "iterparse_mem_compressed" |
    "iterparse_20" | "chunk_regex" | "rustxes";
  xesCaseAttribute?: string;
  xesActivityAttribute?: string;
  xesTimestampAttribute?: string;
  xesResourceAttribute?: string;
  xesLifecycleAttribute?: string;
  xesRequireTimestamp?: boolean;
  xesTimestampSort?: boolean;
  xesReverseSort?: boolean;
  xesMaxTraces?: number;
  xesAttributesToRead?: string[];
  xesMaxDecompressedBytes?: number;
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

export interface LtlFilterOptions extends ExactFilterControls {
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
  /** Exact admitted variants, encoded as comma-separated activities and
   * semicolon-separated variants; `[]` denotes the empty variant. */
  admittedVariants?: string[];
  admittedVariantsPositive?: boolean;
  /** Reduce each case to its head up to prefixActivity / its tail from
   * suffixActivity. Strict cuts leave the anchor out, non-strict keep it;
   * cases without the anchor, or whose cut keeps no events, drop. */
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
  pathActivityAttribute?: string;
  pathTimestampAttribute?: string;
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
export interface MinerOptions extends ExactFilterControls {
  /** Inductive Miner – infrequent noise threshold in [0, 1]; 0 is classic. */
  noiseThreshold?: number;
  /** "log" splits sublogs; "imf" applies infrequent filtering; "dfg" recurses on the graph (IMd). */
  inductiveVariant?: "log" | "imf" | "dfg";
  /** Disable the strict sequence cut and use the ordinary sequence cut. */
  inductiveDisableStrictSequenceCut?: boolean;
  /** Suppress specialized Inductive fall-throughs and use the generic fallback. */
  inductiveDisableFallthroughs?: boolean;
  /** Evaluate large concurrent-activity fall-through searches in parallel. */
  inductiveMultiprocessing?: boolean;
  /** Sort each case chronologically by the selected timestamp field. */
  inductiveSortEventsByTimestamp?: boolean;
  /** Event field used as the Inductive activity label. */
  inductiveActivityKey?: string;
  /** Event field used to order Inductive events. */
  inductiveTimestampKey?: string;
  /** Event field used to group rows into Inductive cases. */
  inductiveCaseIdKey?: string;
  /** "classic" (default) is the textbook Alpha; "plus" handles short loops. */
  alphaVariant?: "plus" | "classic";
  /** Event field used as the Alpha activity label. */
  alphaActivityAttribute?: string;
  /** Remove visible Alpha transitions that cannot be connected to a place. */
  alphaRemoveUnconnected?: boolean;
  /** Hard event cap for Alpha discovery; omitted means unlimited. */
  alphaMaxRowsToMaterialize?: number;
  /** Heuristic Miner algorithm variant (classic by default). */
  heuristicVariant?: "classic" | "plusplus";
  /** Event field used as the heuristic activity label. */
  heuristicActivityAttribute?: string;
  /** Event field used as the heuristic completion timestamp. */
  heuristicTimestampAttribute?: string;
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
  /** Use business-hour calendars for heuristic performance and sojourn times. */
  heuristicBusinessHours?: boolean;
  /** Optional weekly business-hour slots; blank uses Mon–Fri 07:00–17:00, `none` is explicitly empty. */
  heuristicBusinessHourSlots?: string;
  /** Classic DFG pre-cleaning noise threshold (default 0.05). */
  heuristicDfgPreCleaningNoiseThreshold?: number;
  /** Classic two-step-loop threshold (default 0.5). */
  heuristicLoopLengthTwoThreshold?: number;
  /** Deduplicate each classic DFG relation within a case. */
  heuristicKeepOncePerCase?: boolean;
  /** Comma-separated classic path exemptions in `A -> B` syntax. */
  heuristicMostCommonPaths?: string;
  /** Optional event attribute holding interval start timestamps; empty means automatic. */
  heuristicStartTimestampAttribute?: string;
  /** Miner++ generic event attribute distinguishing lifecycle instances. */
  heuristicLifecycleInstanceAttribute?: string;
  /** View-based transition-system options. */
  transitionView?: "sequence" | "set" | "multiset";
  transitionDirection?: "forward" | "backward";
  transitionWindow?: number;
  transitionIncludeData?: boolean;
  /** Event/table activity attribute used by view-based transition systems. */
  transitionActivityKey?: string;
  /** Event/table case-id attribute used by view-based transition systems. */
  transitionCaseIdKey?: string;
  /** POWL discovery algorithm variant (maximal is the default). */
  powlVariant?: "tree" | "brute_force" | "maximal" | "dynamic_clustering" | "dynamic_clustering_plain";
  powlFilteringType?: "decreasing_factor" | "dynamic";
  powlFilteringThreshold?: number;
  powlOrderFrequencyRatio?: number;
  powlBruteForceActivityLimit?: number;
  /** Event field used as the POWL activity label. */
  powlActivityKey?: string;
  /** Timestamp field retained for POWL tabular parity. */
  powlTimestampKey?: string;
  /** Case-id field retained for POWL tabular parity. */
  powlCaseIdKey?: string;
  /** Reference-compatible token-replay variation used for model conformance. */
  tokenReplayVariant?: "forward" | "backwards";
  tokenReplayStopImmediatelyUnfit?: boolean;
  tokenReplayConsiderRemainingInFitness?: boolean;
  tokenReplayTryToReachFinalMarkingThroughHidden?: boolean;
  tokenReplayConsiderActivitiesNotInModelInFitness?: boolean;
  tokenReplayCleaningTokenFlood?: boolean;
  tokenReplayExhaustiveInvisibleExploration?: boolean;
  tokenReplayWalkThroughHiddenTransitions?: boolean;
  tokenReplayIncludeTraceDiagnostics?: boolean;
  tokenReplayIncludeDurationDiagnostics?: boolean;
  tokenReplayIncludeRootCauseDiagnostics?: boolean;
  tokenReplayRootCauseStringAttributes?: string[];
  tokenReplayRootCauseNumericAttributes?: string[];
  tokenReplayRootCauseEnableMultiplier?: boolean;
  tokenReplayTimestampAttribute?: string;
  tokenReplayCaseIdAttribute?: string;
  tokenReplayMaxRowsToMaterialize?: number;
  /** Event field used to form token-replay variants. */
  tokenReplayActivityAttribute?: string;
  /** Classic log-skeleton noise threshold (default 0). */
  logSkeletonNoiseThreshold?: number;
  /** Selected log-skeleton constraint families; all six is the default. */
  logSkeletonConstraints?: string[];
  /** Event field used as the log-skeleton activity label. */
  logSkeletonActivityAttribute?: string;
  /** Event field used as the Declare activity label for discovery/conformance. */
  declareActivityAttribute?: string;
  /** Classic Declare discovery controls. */
  declareConsideredActivities?: string[];
  /** Literal native vector. Omission infers; [] explicitly selects none. Mutually exclusive with the comma-separated option. */
  declareConsideredActivitiesExact?: DiscoveryNativeString[];
  declareAllowedTemplates?: string[];
  declareMinSupportRatio?: number;
  declareMinConfidenceRatio?: number;
  declareAutoSelectionMultiplier?: number;
  /** Aggregate footprints discovery/conformance settings. */
  footprintsDiscoveryVariants?: FootprintsDiscoveryVariant[];
  footprintsModel?: FootprintsModelVariant;
  footprintsConformanceVariants?: FootprintsConformanceVariant[];
  footprintsStrict?: boolean;
  footprintsEnableActAlwaysExecuted?: boolean;
  /** Event/table activity selector (concept:name by default). */
  footprintsActivityAttribute?: string;
  /** Event/table case-id selector (case:concept:name by default). */
  footprintsCaseIdAttribute?: string;
  /** Optional event/table start-time selector; empty means none. */
  footprintsStartTimestampAttribute?: string;
  /** Event/table completion-time selector (time:timestamp by default). */
  footprintsTimestampAttribute?: string;
  /** Optional event/table row-index selector (@@index by default). */
  footprintsIndexAttribute?: string;
  /** Sort table rows by case and completion time (true by default). */
  footprintsSortRequired?: boolean;
  /** Comma-separated non-negative uint64 Petri initial marking. */
  footprintsInitialMarking?: string;
  /** Optional statistical and causal DFG analyses. */
  dfgStatisticalVariants?: string[];
  dfgCausalVariants?: string[];
  dfgWindow?: number;
  dfgKeepOncePerCase?: boolean;
  dfgActivityAttribute?: string;
  dfgTimestampAttribute?: string;
  dfgPerformanceAggregation?:
    | "mean" | "median" | "min" | "max" | "stdev" | "sum" | "raw_values" | "all";
  dfgStartTimestampAttribute?: string;
  dfgBusinessHours?: boolean;
  dfgBusinessHourSlots?: string;
  dfgCaseAttributes?: string[];
  /** Literal native vector, including [] and arbitrary byte-string keys. */
  dfgCaseAttributesExact?: DiscoveryNativeString[];
  dfgReturnNodeAttributes?: boolean;
  /** Optional correlation-mining analyses. */
  correlationVariants?: string[];
  /** Event field used as the correlation activity label. */
  correlationActivityAttribute?: string;
  /** Event field used as the correlation completion timestamp. */
  correlationTimestampAttribute?: string;
  /** Optional event field used as the correlation start timestamp. */
  correlationStartTimestampAttribute?: string;
  /** Event field used as the correlation case identifier. */
  correlationCaseIdAttribute?: string;
  correlationExactTimeMatching?: boolean;
  correlationSampleSize?: number;
  /** Temporal-profile discovery settings (profile discovery is always included). */
  temporalActivityAttribute?: string;
  temporalCaseIdAttribute?: string;
  temporalSourceTimestampAttribute?: string;
  temporalTargetTimestampAttribute?: string;
  temporalBusinessHours?: boolean;
  /** `none` selects an explicitly empty schedule. */
  temporalBusinessHourSlots?: string;
  temporalStrictAttributes?: boolean;
  /** Performance spectrum (opt-in when activities are supplied). */
  spectrumActivities?: string[];
  spectrumVariant?:
    | "connected" | "disconnected" | "dataframe" | "log"
    | "dataframe_disconnected" | "log_disconnected"
    | "lazyframe" | "lazyframe_disconnected";
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
  batchActivityAttribute?: string;
  batchResourceAttribute?: string;
  batchTimestampAttribute?: string;
  batchCaseIdAttribute?: string;
  batchStartTimestampAttribute?: string;
  /** Optional event-id attribute for table batch discovery (pandas/polars). */
  batchEventIdAttribute?: string;
  /** Minimum self-distance adapter selector included in the analysis export. */
  minimumSelfDistanceVariant?: "log" | "pandas" | "polars";
  minimumSelfDistanceActivityKey?: string;
  minimumSelfDistanceCaseIdKey?: string;
  /** Optional overlap statistics (interval events, cases, concurrent activities). */
  overlap?: boolean;
  overlapActivityAttribute?: string;
  overlapStartAttribute?: string;
  overlapCompletionAttribute?: string;
  overlapEpsilon?: number;
  overlapStrict?: boolean;
}

export type MineOptions = CsvOptions & FilterOptions & MinerOptions & MiningInputOptions;

/**
 * The form fields a mine request sends for the given options, as name/value
 * pairs. Pure so the option-to-field mapping is unit-testable; absent or
 * default-signalling options produce no field, leaving the backend defaults
 * in charge. Label lists travel as JSON arrays (`<field>_json`) so labels
 * containing commas survive; the backend's comma form is for the CLI.
 */
export function mineFormEntries(options: MineOptions): Array<[string, string]> {
  if (options.miningInputFormat === "table") {
    options = {...options};
    for (const key of ["caseColumn", "activityColumn", "timestampColumn", "delimiter", "xesVariant", "xesCaseAttribute", "xesActivityAttribute", "xesTimestampAttribute", "xesResourceAttribute", "xesLifecycleAttribute", "xesRequireTimestamp", "xesTimestampSort", "xesReverseSort", "xesMaxTraces", "xesAttributesToRead", "xesMaxDecompressedBytes"] as const) delete options[key];
  }
  if (options.declareConsideredActivities !== undefined && options.declareConsideredActivitiesExact !== undefined)
    throw new Error("Choose either comma-separated or literal Declare considered activities.");
  if (options.dfgCaseAttributes !== undefined && options.dfgCaseAttributesExact !== undefined)
    throw new Error("Choose either comma-separated or literal DFG case attributes.");
  const exactFields = exactFilterFields(options, [
    ...([['ltlVariant','ltl_variant'],['ltlAttributeKey','ltl_attribute_key'],['ltlAttributeValues','ltl_attribute_values'],['ltlPositive','ltl_positive'],['ltlEnableTimestamp','ltl_enable_timestamp'],['ltlTimestampDiffBoundaries','ltl_timestamp_diff_boundaries'],['ltlResourceKey','ltl_resource_key'],['heuristicVariant','heuristic_variant'],['heuristicActivityAttribute','heuristic_activity_attribute'],['heuristicTimestampAttribute','heuristic_timestamp_attribute'],['dependencyThreshold','dependency_threshold'],['andThreshold','and_threshold'],['minEdgeFrequency','min_edge_frequency'],['heuristicMinActivityCount','heuristic_min_activity_count'],['heuristicDecoration','heuristic_decoration'],['heuristicPerformanceAggregation','heuristic_performance_aggregation'],['heuristicBusinessHours','heuristic_business_hours'],['heuristicBusinessHourSlots','heuristic_business_hour_slots'],['heuristicDfgPreCleaningNoiseThreshold','heuristic_dfg_pre_cleaning_noise_threshold'],['heuristicLoopLengthTwoThreshold','heuristic_loop_length_two_threshold'],['heuristicKeepOncePerCase','heuristic_keep_once_per_case'],['heuristicStartTimestampAttribute','heuristic_start_timestamp_attribute'],['heuristicLifecycleInstanceAttribute','heuristic_lifecycle_instance_attribute'],['heuristicMostCommonPaths','heuristic_most_common_paths']] as const).filter(([key])=>options[key]!==undefined).map(([,wire])=>[wire,''] as [string,string])
  ]);
  const exactValues = options.ltl_attribute_values_json !== undefined || options.ltl_attribute_values_file !== undefined;
  const entries: Array<[string, string]> = [...exactFields];
  const add = (name: string, value: string) => entries.push([name, value]);
  const validateNonnegativeInteger = (name: string, value: number) => {
    if (!Number.isInteger(value) || value < 0)
      throw new Error(`${name} must be a non-negative integer`);
  };

  if (options.caseColumn) add("case_column", options.caseColumn);
  if (options.activityColumn) add("activity_column", options.activityColumn);
  if (options.timestampColumn) add("timestamp_column", options.timestampColumn);
  if (options.delimiter) add("delimiter", options.delimiter);
  if (options.xesVariant) {
    const variants = ["iterparse", "line_by_line", "iterparse_mem_compressed",
      "iterparse_20", "chunk_regex", "rustxes"];
    if (!variants.includes(options.xesVariant))
      throw new Error("xesVariant is not supported");
    add("xes_variant", options.xesVariant);
  }
  const addXesSelector = (name: string, value: string | undefined) => {
    if (value === undefined) return;
    if (!value) throw new Error(`${name} must not be empty`);
    add(name, value);
  };
  addXesSelector("xes_case_attribute", options.xesCaseAttribute);
  addXesSelector("xes_activity_attribute", options.xesActivityAttribute);
  addXesSelector("xes_timestamp_attribute", options.xesTimestampAttribute);
  addXesSelector("xes_resource_attribute", options.xesResourceAttribute);
  addXesSelector("xes_lifecycle_attribute", options.xesLifecycleAttribute);
  if (options.xesRequireTimestamp !== undefined)
    add("xes_require_timestamp", String(options.xesRequireTimestamp));
  if (options.xesTimestampSort !== undefined)
    add("xes_timestamp_sort", String(options.xesTimestampSort));
  if (options.xesReverseSort !== undefined)
    add("xes_reverse_sort", String(options.xesReverseSort));
  if (options.xesMaxTraces !== undefined) {
    validateNonnegativeInteger("xesMaxTraces", options.xesMaxTraces);
    add("xes_max_traces", String(options.xesMaxTraces));
  }
  if (options.xesAttributesToRead !== undefined) {
    const attributes = options.xesAttributesToRead.map((key) => key.trim());
    if (attributes.some((key) => !key))
      throw new Error("xesAttributesToRead must contain non-empty keys");
    add("xes_attributes_to_read", attributes.join(","));
  }
  if (options.xesMaxDecompressedBytes !== undefined) {
    if (!Number.isSafeInteger(options.xesMaxDecompressedBytes) ||
        options.xesMaxDecompressedBytes <= 0)
      throw new Error("xesMaxDecompressedBytes must be a positive safe integer");
    add("xes_max_decompressed_bytes", String(options.xesMaxDecompressedBytes));
  }

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
    add("keep_activities_json", JSON.stringify(options.keepActivities));
  if (options.removeActivities?.length)
    add("remove_activities_json", JSON.stringify(options.removeActivities));
  if (options.keepResources?.length)
    add("keep_resources_json", JSON.stringify(options.keepResources));
  if (options.removeResources?.length)
    add("remove_resources_json", JSON.stringify(options.removeResources));
  if (options.keepLifecycles?.length)
    add("keep_lifecycles_json", JSON.stringify(options.keepLifecycles));
  if (options.removeLifecycles?.length)
    add("remove_lifecycles_json", JSON.stringify(options.removeLifecycles));
  if (options.startActivities?.length) {
    add("start_activities_json", JSON.stringify(options.startActivities));
    if (options.startActivitiesPositive !== undefined)
      add("start_activities_positive", String(options.startActivitiesPositive));
  }
  if (options.endActivities?.length) {
    add("end_activities_json", JSON.stringify(options.endActivities));
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
  if (options.admittedVariants?.length) {
    add("admitted_variants", options.admittedVariants.join(";"));
    if (options.admittedVariantsPositive === false)
      add("admitted_variants_positive", "false");
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
      add("keep_attribute_values_json", JSON.stringify(options.keepAttributeValues));
    if (options.removeAttributeValues?.length)
      add("remove_attribute_values_json", JSON.stringify(options.removeAttributeValues));
    if (options.attributeMode)
      add("attribute_mode", options.attributeMode);
  }
  if (options.caseAttributeKey) {
    add("case_attribute_key", options.caseAttributeKey);
    if (options.keepCaseAttributeValues?.length)
      add("keep_case_attribute_values_json", JSON.stringify(options.keepCaseAttributeValues));
    if (options.removeCaseAttributeValues?.length)
      add(
        "remove_case_attribute_values_json",
        JSON.stringify(options.removeCaseAttributeValues),
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
  if (options.inductiveDisableStrictSequenceCut === true)
    add("inductive_disable_strict_sequence_cut", "true");
  if (options.inductiveDisableFallthroughs === true)
    add("inductive_disable_fallthroughs", "true");
  if (options.inductiveMultiprocessing === true)
    add("inductive_multiprocessing", "true");
  if (options.inductiveSortEventsByTimestamp === true)
    add("inductive_sort_events_by_timestamp", "true");
  const inductiveSelectors = [
    ["inductiveActivityKey", "inductive_activity_key", "concept:name"],
    ["inductiveTimestampKey", "inductive_timestamp_key", "time:timestamp"],
    ["inductiveCaseIdKey", "inductive_case_id_key", "case:concept:name"],
  ] as const;
  for (const [option, wireName, defaultValue] of inductiveSelectors) {
    const value = options[option];
    if (value === undefined) continue;
    const normalized = value.trim();
    if (!normalized) throw new Error(`${wireName} must not be empty`);
    if (normalized !== defaultValue) add(wireName, normalized);
  }
  if (options.alphaVariant) add("alpha_variant", options.alphaVariant);
  if (options.alphaActivityAttribute !== undefined) {
    const value = options.alphaActivityAttribute.trim();
    if (!value) throw new Error("alphaActivityAttribute must not be empty");
    if (value !== "concept:name") add("alpha_activity_attribute", value);
  }
  if (options.alphaRemoveUnconnected === true)
    add("alpha_remove_unconnected", "true");
  if (options.alphaMaxRowsToMaterialize !== undefined) {
    const value = options.alphaMaxRowsToMaterialize;
    if (!Number.isSafeInteger(value) || value < 0)
      throw new Error("alphaMaxRowsToMaterialize must be a non-negative safe integer");
    add("alpha_max_rows_to_materialize", String(value));
  }
  if (options.heuristicVariant && options.heuristicVariant !== "classic")
    add("heuristic_variant", options.heuristicVariant);
  if (options.heuristicActivityAttribute !== undefined) {
    const value = options.heuristicActivityAttribute.trim();
    if (!value) throw new Error("heuristicActivityAttribute must not be empty");
    if (value !== "concept:name") add("heuristic_activity_attribute", value);
  }
  if (options.heuristicTimestampAttribute !== undefined) {
    const value = options.heuristicTimestampAttribute.trim();
    if (!value) throw new Error("heuristicTimestampAttribute must not be empty");
    if (value !== "time:timestamp") add("heuristic_timestamp_attribute", value);
  }
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
  if (options.heuristicBusinessHours)
    add("heuristic_business_hours", "true");
  if (options.heuristicBusinessHours &&
      options.heuristicBusinessHourSlots !== undefined) {
    const slots = options.heuristicBusinessHourSlots.trim();
    if (slots) {
      if (slots.toLowerCase() === "none") {
        add("heuristic_business_hour_slots", "none");
      } else {
        const validSlots = slots.split(",").every((slot) => {
          const match = slot.trim().match(
            /^(\d+(?:\.\d+)?)-(\d+(?:\.\d+)?)$/,
          );
          if (!match) return false;
          const start = Number(match[1]);
          const end = Number(match[2]);
          return Number.isFinite(start) && Number.isFinite(end) &&
            start >= 0 && start < end && end <= 604800;
        });
        if (!validSlots)
          throw new Error(
            "heuristicBusinessHourSlots must be `none` or start-end pairs within one week",
          );
        add("heuristic_business_hour_slots", slots);
      }
    }
  }
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
  if (options.heuristicStartTimestampAttribute !== undefined) {
    const value = options.heuristicStartTimestampAttribute.trim();
    if (value)
      add("heuristic_start_timestamp_attribute", value);
  }
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
  const transitionViews = ["sequence", "set", "multiset"];
  if (options.transitionView !== undefined &&
      !transitionViews.includes(options.transitionView))
    throw new Error("transitionView is not a supported transition-system view");
  const transitionDirections = ["forward", "backward"];
  if (options.transitionDirection !== undefined &&
      !transitionDirections.includes(options.transitionDirection))
    throw new Error("transitionDirection is not a supported transition-system direction");
  if (options.transitionWindow !== undefined &&
      (!Number.isSafeInteger(options.transitionWindow) || options.transitionWindow < 0))
    throw new Error("transitionWindow must be a non-negative safe integer");
  const addTransitionSelector = (
    name: string,
    value: string | undefined,
    defaultValue: string,
  ) => {
    if (value === undefined) return;
    const normalized = value.trim();
    if (!normalized) throw new Error(`${name} must not be empty`);
    if (normalized !== defaultValue) add(name, normalized);
  };
  addTransitionSelector(
    "transition_activity_attribute", options.transitionActivityKey, "concept:name",
  );
  addTransitionSelector(
    "transition_case_id_attribute", options.transitionCaseIdKey, "case:concept:name",
  );

  // Validate every explicitly supplied POWL setting before serializing it.
  // Omitting an invalid value would silently select the backend default and
  // make a user-visible setting appear to have been accepted.
  if (options.powlVariant !== undefined &&
      !(<[string, ...string[]]>[
        "tree", "brute_force", "maximal", "dynamic_clustering",
        "dynamic_clustering_plain",
      ]).includes(options.powlVariant)) {
    throw new Error("powlVariant is not a supported POWL variant");
  }
  if (options.powlFilteringType !== undefined &&
      !(<[string, ...string[]]>["decreasing_factor", "dynamic"])
        .includes(options.powlFilteringType)) {
    throw new Error("powlFilteringType is not a supported POWL filtering type");
  }
  if (options.powlFilteringThreshold !== undefined &&
      (!Number.isFinite(options.powlFilteringThreshold) ||
       options.powlFilteringThreshold < 0 ||
       options.powlFilteringThreshold >= 1)) {
    throw new Error("powlFilteringThreshold must be finite and satisfy 0 <= value < 1");
  }
  if (options.powlOrderFrequencyRatio !== undefined &&
      (!Number.isFinite(options.powlOrderFrequencyRatio) ||
       options.powlOrderFrequencyRatio <= 0.5 ||
       options.powlOrderFrequencyRatio > 1)) {
    throw new Error("powlOrderFrequencyRatio must be finite and satisfy 0.5 < value <= 1");
  }
  if (options.powlBruteForceActivityLimit !== undefined &&
      (!Number.isInteger(options.powlBruteForceActivityLimit) ||
       options.powlBruteForceActivityLimit < 0)) {
    throw new Error("powlBruteForceActivityLimit must be a non-negative integer");
  }
  const addPowlSelector = (
    name: string,
    value: string | undefined,
    defaultValue: string,
  ) => {
    if (value === undefined) return;
    const normalized = value.trim();
    if (!normalized) throw new Error(`${name} must not be empty`);
    if (normalized !== defaultValue) add(name, normalized);
  };
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
  addPowlSelector("powl_activity_key", options.powlActivityKey, "concept:name");
  addPowlSelector("powl_timestamp_key", options.powlTimestampKey, "time:timestamp");
  addPowlSelector("powl_case_id_key", options.powlCaseIdKey, "case:concept:name");
  if (options.tokenReplayVariant && options.tokenReplayVariant !== "forward")
    add("token_replay_variant", options.tokenReplayVariant);
  if (options.tokenReplayStopImmediatelyUnfit)
    add("token_replay_stop_immediately_unfit", "true");
  for (const [key, parameter] of [
    ["tokenReplayConsiderRemainingInFitness", "token_replay_consider_remaining_in_fitness"],
    ["tokenReplayTryToReachFinalMarkingThroughHidden", "token_replay_try_to_reach_final_marking_through_hidden"],
    ["tokenReplayConsiderActivitiesNotInModelInFitness", "token_replay_consider_activities_not_in_model_in_fitness"],
    ["tokenReplayCleaningTokenFlood", "token_replay_cleaning_token_flood"],
    ["tokenReplayExhaustiveInvisibleExploration", "token_replay_exhaustive_invisible_exploration"],
    ["tokenReplayWalkThroughHiddenTransitions", "token_replay_walk_through_hidden_transitions"],
    ["tokenReplayIncludeTraceDiagnostics", "token_replay_include_trace_diagnostics"],
    ["tokenReplayIncludeDurationDiagnostics", "token_replay_include_duration_diagnostics"],
    ["tokenReplayIncludeRootCauseDiagnostics", "token_replay_include_root_cause_diagnostics"],
    ["tokenReplayRootCauseEnableMultiplier", "token_replay_root_cause_enable_multiplier"],
  ] as const) {
    const value = options[key];
    if (value !== undefined) {
      if (typeof value !== "boolean") throw new Error(`${key} must be a boolean`);
      add(parameter, String(value));
    }
  }
  for (const [key, parameter] of [
    ["tokenReplayRootCauseStringAttributes", "token_replay_root_cause_string_attributes"],
    ["tokenReplayRootCauseNumericAttributes", "token_replay_root_cause_numeric_attributes"],
  ] as const) {
    const values = options[key];
    if (values !== undefined) {
      if (!Array.isArray(values) || values.some((value) =>
        typeof value !== "string" || !value.trim() || /[,\r\n]/.test(value)))
        throw new Error(`${key} must contain non-empty comma-free attribute names`);
      const names = values.map((value) => value.trim());
      add(parameter, names.join(","));
    }
  }
  if (options.tokenReplayActivityAttribute !== undefined) {
    const value = options.tokenReplayActivityAttribute.trim();
    if (!value) throw new Error("tokenReplayActivityAttribute must not be empty");
    add("token_replay_activity_attribute", value);
  }
  if (options.tokenReplayTimestampAttribute !== undefined) {
    if (typeof options.tokenReplayTimestampAttribute !== "string")
      throw new Error("tokenReplayTimestampAttribute must be a string");
    const value = options.tokenReplayTimestampAttribute.trim();
    if (!value) throw new Error("tokenReplayTimestampAttribute must not be empty");
    add("token_replay_timestamp_attribute", value);
  }
  if (options.tokenReplayCaseIdAttribute !== undefined) {
    const value = options.tokenReplayCaseIdAttribute.trim();
    if (!value) throw new Error("tokenReplayCaseIdAttribute must not be empty");
    add("token_replay_case_id_attribute", value);
  }
  if (options.tokenReplayMaxRowsToMaterialize !== undefined) {
    const value = options.tokenReplayMaxRowsToMaterialize;
    if (!Number.isSafeInteger(value) || value < 0)
      throw new Error("tokenReplayMaxRowsToMaterialize must be a non-negative safe integer");
    add("token_replay_max_rows_to_materialize", String(value));
  }
  const addMineSelector = (
    optionName: string,
    fieldName: string,
    value: string | undefined,
    defaultValue: string,
  ) => {
    if (value === undefined) return;
    const normalized = value.trim();
    if (!normalized) throw new Error(`${optionName} must not be empty`);
    if (normalized !== defaultValue) add(fieldName, normalized);
  };
  addMineSelector(
    "logSkeletonActivityAttribute", "log_skeleton_activity_attribute",
    options.logSkeletonActivityAttribute, "concept:name",
  );
  addMineSelector(
    "declareActivityAttribute", "declare_activity_attribute",
    options.declareActivityAttribute, "concept:name",
  );
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
      options.pathActivityAttribute || options.pathTimestampAttribute ||
      options.pathMinPerformance !== undefined ||
      options.pathMaxPerformance !== undefined ||
      options.pathPositive !== undefined) {
    if (options.pathPredecessor) add("path_predecessor", options.pathPredecessor);
    if (options.pathSuccessor) add("path_successor", options.pathSuccessor);
    if (options.pathActivityAttribute)
      add("path_activity_attribute", options.pathActivityAttribute);
    if (options.pathTimestampAttribute)
      add("path_timestamp_attribute", options.pathTimestampAttribute);
    if (options.pathPredecessor && options.pathSuccessor) {
      if (options.pathMinPerformance !== undefined)
        add("path_min_performance", String(options.pathMinPerformance));
      if (options.pathMaxPerformance !== undefined)
        add("path_max_performance", String(options.pathMaxPerformance));
      if (options.pathPositive !== undefined)
        add("path_positive", String(options.pathPositive));
    }
  }
  if (exactValues || options.ltlVariant && options.ltlAttributeValues?.length) {
    if (options.ltlVariant !== undefined) add("ltl_variant", options.ltlVariant);
    if (options.ltlAttributeValues !== undefined) add("ltl_attribute_values", options.ltlAttributeValues.join(","));
    if (exactValues ? options.ltlAttributeKey !== undefined : options.ltlAttributeKey) add("ltl_attribute_key", options.ltlAttributeKey!);
    if (options.ltlPositive !== undefined) add("ltl_positive", String(options.ltlPositive));
    if (options.ltlEnableTimestamp !== undefined) add("ltl_enable_timestamp", String(options.ltlEnableTimestamp));
    if (exactValues ? options.ltlTimestampDiffBoundaries !== undefined : options.ltlTimestampDiffBoundaries?.length) add("ltl_timestamp_diff_boundaries", options.ltlTimestampDiffBoundaries!.map(([lo,hi])=>`${lo}:${hi}`).join(","));
    if (exactValues ? options.ltlResourceKey !== undefined : options.ltlResourceKey) add("ltl_resource_key", options.ltlResourceKey!);
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
  if (selectedLogSkeletonConstraints !== undefined) {
    if (options.logSkeletonConstraints!.length === 0) {
      // An explicit empty list is meaningful to conformance; retain its
      // presence rather than silently selecting all six native families.
      add("log_skeleton_constraints", "");
    } else if (selectedLogSkeletonConstraints.length > 0 && (
      selectedLogSkeletonConstraints.length !== allLogSkeletonConstraints.length ||
      allLogSkeletonConstraints.some((name) => !selectedLogSkeletonConstraints.includes(name))
    )) {
      add("log_skeleton_constraints", selectedLogSkeletonConstraints.join(","));
    }
  }

  const selectedDeclareTemplates = options.declareAllowedTemplates?.filter(
    (name) => (DECLARE_TEMPLATES as readonly string[]).includes(name),
  );
  if (options.declareConsideredActivitiesExact !== undefined)
    add("declare_considered_activities_json", discoveryStringVectorJson(options.declareConsideredActivitiesExact, "Declare considered activities"));
  else if (options.declareConsideredActivities?.length)
    add("declare_considered_activities", options.declareConsideredActivities.join(","));
  if (selectedDeclareTemplates !== undefined) {
    if (options.declareAllowedTemplates!.length === 0) {
      // An explicit empty list disables Declare template checks. Preserve the
      // present-but-empty wire field so it is not confused with the default.
      add("declare_allowed_templates", "");
    } else if (selectedDeclareTemplates.length > 0 && (
      selectedDeclareTemplates.length !== DECLARE_TEMPLATES.length ||
      DECLARE_TEMPLATES.some((name) => !selectedDeclareTemplates.includes(name))
    )) {
      add("declare_allowed_templates", selectedDeclareTemplates.join(","));
    }
  }
  // Any finite ratio, as the reference takes it.
  const declareRatio = (value: number | undefined) =>
    value !== undefined && Number.isFinite(value) ? String(value) : undefined;
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

  const requestedFootprintsDiscovery = options.footprintsDiscoveryVariants;
  if (requestedFootprintsDiscovery !== undefined) {
    if (requestedFootprintsDiscovery.length === 0)
      throw new Error("footprintsDiscoveryVariants must not be empty");
    const unknown = requestedFootprintsDiscovery.find(
      (name) => !(FOOTPRINTS_DISCOVERY_VARIANTS as readonly string[]).includes(name),
    );
    if (unknown !== undefined)
      throw new Error(`Unknown footprints discovery variant: ${unknown}`);
    if (options.footprintsModel === "dfg" &&
        requestedFootprintsDiscovery.includes("petri_reach_graph")) {
      throw new Error(
        "petri_reach_graph is incompatible with footprintsModel=dfg",
      );
    }
  }
  const selectedFootprintsDiscovery = requestedFootprintsDiscovery
    ? FOOTPRINTS_DISCOVERY_VARIANTS.filter((name) =>
        requestedFootprintsDiscovery.includes(name),
      )
    : undefined;
  if (
    selectedFootprintsDiscovery && selectedFootprintsDiscovery.length > 0 &&
    (selectedFootprintsDiscovery.length !== FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length ||
      FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.some((name) => !selectedFootprintsDiscovery.includes(name)))
  ) add("footprints_discovery_variants", selectedFootprintsDiscovery.join(","));
  if (options.footprintsModel !== undefined) {
    const footprintModels: readonly FootprintsModelVariant[] = [
      "process_tree", "powl", "inductive", "alpha", "heuristic", "dfg",
    ];
    if (!footprintModels.includes(options.footprintsModel)) {
      throw new Error(`Unknown footprints model: ${options.footprintsModel}`);
    }
    if (options.footprintsModel !== "process_tree")
      add("footprints_model", options.footprintsModel);
  }
  const requestedFootprintsConformance = options.footprintsConformanceVariants;
  if (requestedFootprintsConformance !== undefined) {
    if (requestedFootprintsConformance.length === 0)
      throw new Error("footprintsConformanceVariants must not be empty");
    const unknown = requestedFootprintsConformance.find(
      (name) => !(FOOTPRINTS_CONFORMANCE_VARIANTS as readonly string[]).includes(name),
    );
    if (unknown !== undefined)
      throw new Error(`Unknown footprints conformance variant: ${unknown}`);
  }
  const selectedFootprintsConformance = requestedFootprintsConformance
    ? FOOTPRINTS_CONFORMANCE_VARIANTS.filter((name) =>
        requestedFootprintsConformance.includes(name),
      )
    : undefined;
  if (
    selectedFootprintsConformance && selectedFootprintsConformance.length > 0 &&
    (selectedFootprintsConformance.length !== 1 || selectedFootprintsConformance[0] !== "log_model")
  ) add("footprints_conformance_variants", selectedFootprintsConformance.join(","));
  if (options.footprintsStrict) add("footprints_strict", "true");
  if (options.footprintsEnableActAlwaysExecuted === false)
    add("footprints_enable_act_always_executed", "false");
  const addFootprintsSelector = (
    optionName: string,
    fieldName: string,
    value: string | undefined,
    defaultValue: string,
  ) => {
    if (value === undefined) return;
    const normalized = value.trim();
    if (!normalized) throw new Error(`${optionName} must not be empty`);
    if (normalized !== defaultValue) add(fieldName, normalized);
  };
  addFootprintsSelector(
    "footprintsActivityAttribute", "footprints_activity_attribute",
    options.footprintsActivityAttribute, "concept:name",
  );
  addFootprintsSelector(
    "footprintsCaseIdAttribute", "footprints_case_id_attribute",
    options.footprintsCaseIdAttribute, "case:concept:name",
  );
  addFootprintsSelector(
    "footprintsTimestampAttribute", "footprints_timestamp_attribute",
    options.footprintsTimestampAttribute, "time:timestamp",
  );
  if (options.footprintsIndexAttribute !== undefined) {
    const normalized = options.footprintsIndexAttribute.trim();
    if (normalized !== "@@index") add("footprints_index_attribute", normalized);
  }
  if (options.footprintsStartTimestampAttribute !== undefined) {
    const normalized = options.footprintsStartTimestampAttribute.trim();
    if (normalized) add("footprints_start_timestamp_attribute", normalized);
  }
  if (options.footprintsSortRequired !== undefined &&
      options.footprintsSortRequired !== true) {
    add("footprints_sort_required", "false");
  }
  if (options.footprintsInitialMarking !== undefined) {
    const rawTokens = options.footprintsInitialMarking.split(",");
    if (rawTokens.length === 0)
      throw new Error("footprintsInitialMarking must not be empty");
    const maxUint64 = 18446744073709551615n;
    const tokens = rawTokens.map((raw) => {
      const token = raw.trim();
      if (!/^\d+$/.test(token)) {
        throw new Error(
          "footprintsInitialMarking must contain non-negative uint64 tokens",
        );
      }
      let parsed: bigint;
      try {
        parsed = BigInt(token);
      } catch {
        throw new Error(
          "footprintsInitialMarking must contain non-negative uint64 tokens",
        );
      }
      if (parsed > maxUint64) {
        throw new Error("footprintsInitialMarking token exceeds uint64 range");
      }
      return parsed.toString();
    });
    add("footprints_initial_marking", tokens.join(","));
  }

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
  if (selectedDfgStatistical.length || selectedDfgCausal.length) {
    const activityAttribute = options.dfgActivityAttribute?.trim();
    if (activityAttribute && activityAttribute !== "concept:name")
      add("dfg_activity_attribute", activityAttribute);
  }
  if (hasDfgFrequency) {
    if (options.dfgWindow !== undefined && Number.isInteger(options.dfgWindow) && options.dfgWindow >= 0 && options.dfgWindow !== 1)
      add("dfg_window", String(options.dfgWindow));
    if (options.dfgKeepOncePerCase) add("dfg_keep_once_per_case", "true");
  }
  if (hasDfgPerformance) {
    const aggregations = ["mean", "median", "min", "max", "stdev", "sum", "raw_values", "all"];
    if (options.dfgPerformanceAggregation && aggregations.includes(options.dfgPerformanceAggregation) && options.dfgPerformanceAggregation !== "mean")
      add("dfg_performance_aggregation", options.dfgPerformanceAggregation);
    if (options.dfgStartTimestampAttribute?.trim())
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
  const hasDfgTimestampSelector = selectedDfgStatistical.some((name) =>
    ["performance", "performance_greedy", "clean", "clean_polars", "clean_time"].includes(name));
  if (hasDfgTimestampSelector) {
    const timestampAttribute = options.dfgTimestampAttribute?.trim();
    if (timestampAttribute && timestampAttribute !== "time:timestamp")
      add("dfg_timestamp_attribute", timestampAttribute);
  }
  if (options.dfgCaseAttributesExact !== undefined)
    add("dfg_case_attributes_json", discoveryStringVectorJson(options.dfgCaseAttributesExact, "DFG case attributes"));
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
  const correlationSelectors: Array<[string, string | undefined, string, boolean]> = [
    ["correlation_activity_attribute", options.correlationActivityAttribute, "concept:name", false],
    ["correlation_timestamp_attribute", options.correlationTimestampAttribute, "time:timestamp", false],
    ["correlation_start_timestamp_attribute", options.correlationStartTimestampAttribute, "", true],
    ["correlation_case_id_attribute", options.correlationCaseIdAttribute, "case:concept:name", false],
  ];
  if (selectedCorrelation.length) {
    const traceEnabled = selectedCorrelation.includes("trace_based");
    for (const [name, raw, , allowEmpty] of correlationSelectors) {
      if (raw === undefined) continue;
      const value = raw.trim();
      if (!allowEmpty && !value &&
          (name !== "correlation_case_id_attribute" || traceEnabled))
        throw new Error(`${name} must not be empty`);
    }
    add("correlation_variants", selectedCorrelation.join(","));
    for (const [name, raw, defaultValue] of correlationSelectors) {
      const value = raw?.trim();
      if (value && value !== defaultValue) add(name, value);
    }
    const exactCapable =
      selectedCorrelation.includes("classic") ||
      selectedCorrelation.includes("classic_split");
    if (exactCapable && options.correlationExactTimeMatching) {
      add("correlation_exact_time_matching", "true");
    }
    if (selectedCorrelation.includes("classic_split") &&
        options.correlationSampleSize !== undefined) {
      const sampleSize = options.correlationSampleSize;
      if (!Number.isSafeInteger(sampleSize) || sampleSize <= 0) {
        throw new Error(
          "correlationSampleSize must be a positive safe integer",
        );
      }
      if (sampleSize !== 100000)
        add("correlation_sample_size", String(sampleSize));
    }
  }

  const temporalTextOptions: Array<[string, string | undefined, string]> = [
    ["temporal_activity_attribute", options.temporalActivityAttribute, "concept:name"],
    ["temporal_case_id_attribute", options.temporalCaseIdAttribute, "case:concept:name"],
    ["temporal_source_timestamp_attribute", options.temporalSourceTimestampAttribute, ""],
    ["temporal_target_timestamp_attribute", options.temporalTargetTimestampAttribute, ""],
  ];
  for (const [name, raw, defaultValue] of temporalTextOptions) {
    if (raw === undefined) continue;
    const value = raw.trim();
    if (!value) throw new Error(`${name} must not be empty`);
    if (value !== defaultValue) add(name, value);
  }
  if (options.temporalBusinessHours) add("temporal_business_hours", "true");
  if (options.temporalStrictAttributes === true)
    add("temporal_strict_attributes", "true");
  if (options.temporalBusinessHourSlots !== undefined) {
    const value = options.temporalBusinessHourSlots.trim();
    if (value === "" || value.toLowerCase() === "none") {
      add("temporal_business_hour_slots", "none");
    } else {
      const valid = value.split(",").every((slot) => {
        const match = slot.trim().match(/^(\d+(?:\.\d+)?)-(\d+(?:\.\d+)?)$/);
        if (!match) return false;
        const start = Number(match[1]);
        const end = Number(match[2]);
        return Number.isFinite(start) && Number.isFinite(end) &&
          start >= 0 && start < end && end <= 604800;
      });
      if (!valid) {
        throw new Error(
          "temporalBusinessHourSlots must be `none` or start-end pairs within one week",
        );
      }
      add("temporal_business_hour_slots", value);
    }
  }

  // Optional temporal/behavioural analyses keep dependent settings scoped to
  // the selected analysis while defaults stay on the backend.
  if (options.spectrumActivities !== undefined) {
    const spectrumActivities = options.spectrumActivities
      .map((name) => name.trim());
    if (spectrumActivities.some((name) => !name))
      throw new Error("spectrumActivities must not contain empty activities");
    if (spectrumActivities.length < 2)
      throw new Error(
        "spectrumActivities must contain at least two non-empty activities",
      );
    add("spectrum_activities", spectrumActivities.join(","));
    const spectrumVariants = [
      "connected", "disconnected", "dataframe", "log",
      "dataframe_disconnected", "log_disconnected", "lazyframe",
      "lazyframe_disconnected",
    ];
    if (options.spectrumVariant !== undefined &&
        !spectrumVariants.includes(options.spectrumVariant))
      throw new Error("spectrumVariant is not a supported performance-spectrum variant");
    if (options.spectrumVariant && options.spectrumVariant !== "connected")
      add("spectrum_variant", options.spectrumVariant);
    if (options.spectrumSampleSize !== undefined) {
      if (!Number.isSafeInteger(options.spectrumSampleSize) ||
          options.spectrumSampleSize < 0)
        throw new Error("spectrumSampleSize must be a non-negative safe integer");
      if (options.spectrumSampleSize !== 10000)
        add("spectrum_sample_size", String(options.spectrumSampleSize));
    }
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
    if (options.batchActivityAttribute?.trim())
      add("batch_activity_attribute", options.batchActivityAttribute.trim());
    if (options.batchResourceAttribute?.trim())
      add("batch_resource_attribute", options.batchResourceAttribute.trim());
    if (options.batchTimestampAttribute?.trim())
      add("batch_timestamp_attribute", options.batchTimestampAttribute.trim());
    if (options.batchCaseIdAttribute?.trim())
      add("batch_case_id_attribute", options.batchCaseIdAttribute.trim());
    if (options.batchStartTimestampAttribute?.trim())
      add("batch_start_timestamp_attribute", options.batchStartTimestampAttribute.trim());
    // Event ids are only projected by the table adapters. A caller that
    // supplies one for the default LOG adapter likely has stale UI state, but
    // the typed API must surface that contract violation instead of silently
    // dropping the value.
    if (options.batchEventIdAttribute !== undefined) {
      if (options.batchVariant === undefined || options.batchVariant === "log")
        throw new Error("batchEventIdAttribute is only supported for table batches");
      const eventId = options.batchEventIdAttribute.trim();
      if (!eventId) throw new Error("batchEventIdAttribute must not be empty");
      add("batch_event_id_attribute", eventId);
    }
  }
  if (options.minimumSelfDistanceVariant &&
      options.minimumSelfDistanceVariant !== "log") {
    add("minimum_self_distance_variant", options.minimumSelfDistanceVariant);
  }
  if (options.minimumSelfDistanceActivityKey?.trim())
    add("minimum_self_distance_activity_key", options.minimumSelfDistanceActivityKey.trim());
  if (options.minimumSelfDistanceCaseIdKey?.trim())
    add("minimum_self_distance_case_id_key", options.minimumSelfDistanceCaseIdKey.trim());
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

  const sourceFields = miningSourceFields(options), overridden = new Set(sourceFields.map(([key]) => key));
  return [...entries.filter(([key]) => !overridden.has(key)), ...sourceFields];
}

/** The backend's request limit (minera_server kMaxRequestPayloadBytes). */
export const MAX_UPLOAD_BYTES = 128 * 1024 * 1024;

/** The message for an upload over MAX_UPLOAD_BYTES, naming the limit. */
export function uploadTooLargeMessage(size?: number): string {
  const limit = `${MAX_UPLOAD_BYTES / (1024 * 1024)} MiB`;
  const actual = size === undefined ? "" : ` (${(size / (1024 * 1024)).toFixed(1)} MiB)`;
  return `the upload${actual} is larger than the ${limit} limit of the Minera backend; compress an XES log as .xes.gz, which Minera reads directly`;
}

/**
 * The backend mines one log at a time (503 MINE_BUSY for any other). This tab
 * cancels its own earlier mine before it starts another and then waits for it
 * (the backend reports it as "canceling"), so a mine still running without a
 * cancel was started elsewhere: in another tab or window, or by another client
 * of the same server. Its Cancel button is there, not here, so the message
 * offers no cancel. (The backend's own MINE_BUSY text, which says "wait or
 * cancel it", is written for whichever client started that mine.)
 */
export const MINE_BUSY_MESSAGE = "Another log is being mined on this Minera server, from another tab, window or client. Try again when it finishes, or cancel it where it was started.";
/** A canceled mine that still held the backend's slot when this mine stopped waiting for it. */
export const MINE_BUSY_CANCELING_MESSAGE = "The Minera server is still stopping a canceled mine. Try again in a moment.";
/** How long a mine waits for a canceled mine that still holds the backend's single slot. */
export const MINE_BUSY_WAIT_MS = 120_000;

/** A mine the backend refused before parsing it: 413 MINE_LIMIT, with its estimate. */
export class MineLimitError extends Error {
  constructor(message: string, readonly estimatedEvents?: number, readonly estimatedBytes?: number,
              readonly limit?: string) {
    super(message);
    this.name = "MineLimitError";
  }
}

type MineAdmission = {family?: unknown; error?: unknown; canceling?: unknown; retry_after_seconds?: unknown;
  estimated_events?: unknown; estimated_bytes?: unknown; limit?: unknown};

async function mineAdmission(response: Response): Promise<MineAdmission | undefined> {
  if (response.status !== 503 && response.status !== 413) return undefined;
  try {
    const value: unknown = await response.clone().json();
    if (value === null || typeof value !== "object") return undefined;
    const family = (value as MineAdmission).family;
    return family === "MINE_BUSY" || family === "MINE_LIMIT" ? value as MineAdmission : undefined;
  } catch {return undefined;}
}

function waitFor(ms: number, signal?: AbortSignal): Promise<void> {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {reject(signal.reason); return;}
    const timer = setTimeout(() => {signal?.removeEventListener("abort", stop); resolve();}, ms);
    const stop = () => {clearTimeout(timer); reject(signal?.reason);};
    signal?.addEventListener("abort", stop, {once: true});
  });
}

const finiteNumber = (value: unknown) => typeof value === "number" && Number.isFinite(value) ? value : undefined;

/** How much of the analysis a mine response carries. */
export interface MineResponseOptions {
  /**
   * true keeps the "traces" array inline in the response. By default the app
   * mines without it and loads the cases on demand (GET /api/traces).
   */
  includeTraces?: boolean;
  /**
   * true first waits (polling GET /api/mine-progress) for a canceled mine
   * that still holds the backend's single slot, as one this tab just
   * canceled does while it stops, instead of posting into 503 MINE_BUSY.
   */
  waitForCanceledMine?: boolean;
}

/** Upload the unchanged original file and retain typed source-only outcomes. */
export async function mineActiveDataset(
  file: File,
  options: MineOptions & MineResponseOptions = {},
  signal?: AbortSignal,
): Promise<ActiveDatasetResult> {
  if (file.size > MAX_UPLOAD_BYTES) throw new Error(uploadTooLargeMessage(file.size));
  const form = new FormData();
  if (hasExactFilters(options)) form.append("log", file); else form.append("log", file, file.name);
  const fields = mineFormEntries({...options, miningRequestId: options.miningRequestId ?? crypto.randomUUID()});
  // The cases are left out of the response (a trace summary stands in) and
  // loaded on demand by the views that read them (mine-traces.ts). An exact
  // native-control mine sends only the controls the user supplied, as its
  // provenance records them, so it keeps the traces inline.
  if (options.includeTraces !== true && !hasExactFilters(options)) fields.push(["include_traces", "false"]);
  for (const [name, value] of fields) {if (hasExactFilters(options)) appendNativeMultipartField(form, name, value); else form.append(name, value);}
  appendExactFilterFiles(form, options);

  let response: Response;
  // A mine the app just canceled (or replaced) keeps the backend's single
  // mining slot until its next stage; the backend says so ("canceling"), and
  // this request retries after Retry-After instead of failing.
  const started = Date.now();
  if (options.waitForCanceledMine) await waitForCanceledMine(signal, MINE_BUSY_WAIT_MS);
  for (;;) {
    try {
      response = await importedModelFetch("/api/mine", { method: "POST", body: form, signal });
    } catch (cause) {
      if (signal?.aborted) throw cause;
      throw new Error(
        "could not reach the Minera backend — is minera_server running?",
      );
    }
    const admission = await mineAdmission(response);
    if (!admission) break;
    if (admission.family === "MINE_LIMIT") {
      throw new MineLimitError(typeof admission.error === "string" ? admission.error : "the log is too large for this Minera backend",
        finiteNumber(admission.estimated_events), finiteNumber(admission.estimated_bytes),
        typeof admission.limit === "string" ? admission.limit : undefined);
    }
    const retryHeader = Number(response.headers.get("Retry-After"));
    const retrySeconds = Number.isFinite(retryHeader) && retryHeader > 0 ? retryHeader : finiteNumber(admission.retry_after_seconds) ?? 1;
    if (admission.canceling !== true) throw new Error(MINE_BUSY_MESSAGE);
    if (Date.now() - started + retrySeconds * 1000 > MINE_BUSY_WAIT_MS) throw new Error(MINE_BUSY_CANCELING_MESSAGE);
    await waitFor(retrySeconds * 1000, signal);
  }

  const exactRun = hasExactFilters(options) ? await readExactFilterResponse(response, "/api/mine", form, fields, signal) : null;
  const parsed: unknown = exactRun?.value ?? await readNativeResponse(response, {path: "/api/mine", fields});
  if (exactRun && (!response.ok || exactRun.value.family === "EXACT_FILTER_DISCOVERY")) throw new OwnedExactFilterResult(exactRun);
  signal?.throwIfAborted();
  // This response's summary (if any) may fetch its cases from the backend.
  if (response.ok) markLiveTraces(parsed);
  if (isActiveSourceMining(parsed)) return parsed;
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    if (exactRun) throw new OwnedExactFilterResult(exactRun);
    throw new Error(String((parsed as { error: unknown }).error));
  }
  const unreadable = parsed === null && !exactRun ? nativeResponseFailure(response) : undefined;
  if (response.status === 413) throw new Error(uploadTooLargeMessage());
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}${unreadable ? `, and ${unreadable}` : ""}`);
  }
  if (unreadable) throw new Error(`the Minera backend finished mining, but the browser could not use its response: ${unreadable}`);
  if (!isMineraExport(parsed)) {
    if (exactRun) throw new OwnedExactFilterResult(exactRun);
    throw new Error("the Minera backend returned an invalid analysis");
  }
  return parsed;
}
/** Historical callers require an analysis; the active-source API retains independent native failures. */
export async function mineCsv(file: File, options: MineOptions & MineResponseOptions = {}, signal?: AbortSignal): Promise<MineraExport> {const value = await mineActiveDataset(file, options, signal); if (isActiveSourceMining(value)) {if (value.analysis) return value.analysis; throw new Error(value.error?.message ?? "The source is active, but no classic analysis is available. Inspect its native family results.");} return value;}

/**
 * Asks the backend to align the most recently mined log against one miner's
 * model. The server defers this costlier computation to here so a mine never
 * pays for alignments the user may not open.
 */
/** Stable query serialization used by the alignment cache and request API. */
export function alignmentQuery(options: AlignmentOptions = {}): string {
  const query = new URLSearchParams();
  setProcessTreePetriNetVariant(query, options.process_tree_variant);
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
  if (options.activity_attribute !== undefined) {
    const value = options.activity_attribute.trim();
    if (!value) throw new Error("activity_attribute must not be empty");
    add("alignment_activity_attribute", value);
  }
  return query.toString();
}

export async function alignModel(miner: string, options: AlignmentOptions = {}, signal?: AbortSignal): Promise<Alignments> {
  // Build the query before the network try, so a parameter error is reported as itself.
  const query = alignmentQuery(options);
  const params = [`miner=${encodeURIComponent(miner)}`, query].filter(Boolean).join("&");
  let response: Response;
  try {
    // Aborting closes the connection, and the backend stops the search.
    response = await importedModelFetch(`/api/align?${params}`, signal ? { method: "POST", signal } : { method: "POST" });
  } catch (cause) {
    if (signal?.aborted) throw cause;
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  // An uncomputed result (search exhausted, or no reachable final marking) is a
  // valid answer: it carries its reason in `error`, and the caller renders the
  // notice and caches it like any other alignment.
  if (response.ok && isUncomputedAlignmentPayload(parsed)) return parsed as unknown as Alignments;
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

/**
 * True for the uncomputed alignment shape the server answers with status 200:
 * `computed: false` with a boolean `exhausted` and/or a string `error`.
 */
export function isUncomputedAlignmentPayload(value: unknown): value is { computed: false; exhausted?: boolean; error?: string } {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  if (record.computed !== false) return false;
  const exhausted = record.exhausted, error = record.error;
  if (exhausted !== undefined && typeof exhausted !== "boolean") return false;
  if (error !== undefined && typeof error !== "string") return false;
  return typeof exhausted === "boolean" || typeof error === "string";
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
  if (value.trim() === "{}") return {};
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
    Object.defineProperty(result, activity, { value: cost, enumerable: true, writable: true, configurable: true });
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

function encodeCostMap(map?: Record<string, number>, preserveEmpty = false): string | undefined {
  if (!map) return undefined;
  const entries = Object.entries(map).sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0);
  for (const [name, value] of entries) {
    if (!name || name === ">>" || !Number.isFinite(value) || value < 0)
      throw new Error("DFG cost maps require non-gap activities and finite non-negative costs");
  }
  return entries.length ? entries.map(([name, value]) => `${escapeCostActivity(name)}=${value}`).join(",") : preserveEmpty ? "{}" : undefined;
}

/** Deterministic query serialization for classic DFG alignment requests. */
export function dfgAlignmentQuery(options: DfgAlignmentOptions = {}): string {
  const q = new URLSearchParams();
  const add = (name: string, value: unknown) => { if (value !== undefined && value !== null && value !== "") q.set(name, String(value)); };
  const addSelector = (name: string, value: string | undefined) => {
    if (value === undefined) return;
    const selector = value.trim();
    if (!selector) throw new Error(`${name} must not be empty`);
    q.set(name, selector);
  };
  add("dfg_variant", options.variant);
  addSelector("dfg_activity_attribute", options.activity_attribute);
  addSelector("dfg_timestamp_attribute", options.timestamp_attribute);
  const maxStates = options.max_states;
  const maxTimeMs = options.max_time_ms;
  if (typeof maxStates === "number" && Number.isInteger(maxStates) && maxStates > 0) add("dfg_max_states", maxStates);
  if (typeof maxTimeMs === "number" && Number.isInteger(maxTimeMs) && maxTimeMs > 0) add("dfg_max_time_ms", maxTimeMs);
  add("dfg_sync_cost", encodeCostMap(options.sync_cost, true));
  add("dfg_model_move_cost", encodeCostMap(options.model_move_cost, true));
  add("dfg_log_move_cost", encodeCostMap(options.log_move_cost, true));
  add("dfg_internal_log_move_cost", encodeCostMap(options.internal_log_move_cost, true));
  add("projection", options.projection);
  return q.toString();
}

export async function alignDfg(options: DfgAlignmentOptions = {}): Promise<DfgAlignmentResult> {
  const query = dfgAlignmentQuery(options);
  const path = `/api/align-dfg${query ? `?${query}` : ""}`;
  let response: Response;
  try { response = await importedModelFetch(path, { method: "POST" }); }
  catch { throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed = await readAlignmentResponse(response, { path });
  if (response.ok && isUncomputedAlignmentPayload(parsed) && isDfgAlignmentResult(parsed)) return parsed;
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isDfgAlignmentResult(parsed)) throw new Error("the Minera backend returned an invalid DFG alignment");
  return parsed;
}

/** Stable form serialization for edit-distance alignment controls. */
export function editDistanceFormEntries(options: EditDistanceOptions = {}): Array<[string, string]> {
  const entries: Array<[string, string]> = [];
  const addSelector = (name: string, value: string | undefined) => {
    if (value === undefined) return;
    const selector = value.trim();
    if (!selector) throw new Error(`${name} must not be empty`);
    entries.push([name, selector]);
  };
  if (options.variant) entries.push(["edit_variant", options.variant]);
  addSelector("edit_activity_attribute", options.activity_attribute);
  addSelector("edit_timestamp_attribute", options.timestamp_attribute);
  if (options.anti !== undefined) entries.push(["anti", String(options.anti)]);
  const deviationCost = options.deviation_cost;
  if (typeof deviationCost === "number" && Number.isInteger(deviationCost) && deviationCost > 0) entries.push(["deviation_cost", String(deviationCost)]);
  if (options.projection) entries.push(["projection", options.projection]);
  return entries;
}

export async function alignEditDistance(reference: File, options: EditDistanceOptions = {}): Promise<EditDistanceResult> {
  const form = new FormData();
  form.append("reference_log", reference, reference.name);
  const fields = editDistanceFormEntries(options);
  for (const [name, value] of fields) form.append(name, value);
  let response: Response;
  try { response = await importedModelFetch("/api/align-edit-distance", { method: "POST", body: form }); }
  catch { throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed = await readAlignmentResponse(response, { path: "/api/align-edit-distance", fields,
    reference: { name: reference.name, size: reference.size, last_modified: reference.lastModified } });
  if (response.ok && isUncomputedAlignmentPayload(parsed) && isEditDistanceResult(parsed)) return parsed;
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isEditDistanceResult(parsed)) throw new Error("the Minera backend returned an invalid edit-distance alignment");
  return parsed;
}

/** Stable, validated query serialization for one model-quality evaluation. */
export function evaluationQuery(options: EvaluationOptions = {}): string {
  const family = options.evaluation_family ?? "composite";
  if (!["replay_fitness", "precision", "generalization", "simplicity", "composite"].includes(family)) {
    throw new Error("evaluation_family is invalid");
  }
  if (options.evaluation_input_adapter !== undefined &&
      !["log", "stream", "dataframe", "lazyframe"].includes(options.evaluation_input_adapter)) {
    throw new Error("evaluation_input_adapter is invalid");
  }
  if (options.source_id !== undefined && !options.source_id.trim()) throw new Error("source_id must not be empty");
  if (options.fitness_variant !== undefined && !["auto", "token_based", "alignment_based"].includes(options.fitness_variant)) {
    throw new Error("fitness_variant is invalid");
  }
  if (options.evaluation_activity_attribute !== undefined &&
      !options.evaluation_activity_attribute.trim()) {
    throw new Error("evaluation_activity_attribute must not be empty");
  }
  if (options.evaluation_case_id_attribute !== undefined &&
      !options.evaluation_case_id_attribute.trim()) {
    throw new Error("evaluation_case_id_attribute must not be empty");
  }
  if (options.evaluation_max_rows_to_materialize !== undefined &&
      (!Number.isInteger(options.evaluation_max_rows_to_materialize) ||
       options.evaluation_max_rows_to_materialize < 0)) {
    throw new Error("evaluation_max_rows_to_materialize must be a non-negative integer");
  }
  if (options.evaluation_token_replay_variant !== undefined &&
      !["forward", "backwards"].includes(options.evaluation_token_replay_variant)) {
    throw new Error("evaluation_token_replay_variant is invalid");
  }
  const alignmentVariant = options.evaluation_alignment_variant;
  const alignmentVariants = [
    "state_equation_a_star", "dijkstra_no_heuristics",
    "dijkstra_less_memory", "discounted_a_star",
    "generator_dijkstra_no_heuristics",
    "generator_dijkstra_less_memory", "recompos_maximal",
  ];
  if (alignmentVariant !== undefined &&
      !alignmentVariants.includes(alignmentVariant)) {
    throw new Error("evaluation_alignment_variant is invalid");
  }
  if (family === "precision" && alignmentVariant === "recompos_maximal") {
    throw new Error("recompos_maximal is only available for replay fitness");
  }
  for (const [name, value] of [
    ["fitness_consider_unknown_activities", options.fitness_consider_unknown_activities],
    ["fitness_cleaning_token_flood", options.fitness_cleaning_token_flood],
    ["evaluation_alignment_synchronous_dijkstra", options.evaluation_alignment_synchronous_dijkstra],
    ["evaluation_alignment_use_cache", options.evaluation_alignment_use_cache],
    ["evaluation_alignment_clear_cache", options.evaluation_alignment_clear_cache],
  ] as const) {
    if (value !== undefined && typeof value !== "boolean") {
      throw new Error(`${name} must be boolean`);
    }
  }
  if (options.evaluation_alignment_cost_profile !== undefined &&
      !["standard", "legacy"].includes(options.evaluation_alignment_cost_profile)) {
    throw new Error("evaluation_alignment_cost_profile is invalid");
  }
  for (const [name, value] of [
    ["evaluation_alignment_max_states", options.evaluation_alignment_max_states],
    ["evaluation_alignment_max_total_states", options.evaluation_alignment_max_total_states],
    ["evaluation_alignment_max_alignments", options.evaluation_alignment_max_alignments],
  ] as const) {
    if (value !== undefined && (!Number.isInteger(value) || value < 1)) {
      throw new Error(`${name} must be a positive integer`);
    }
  }
  for (const [name, value] of [
    ["evaluation_alignment_max_time", options.evaluation_alignment_max_time],
    ["evaluation_alignment_max_time_trace", options.evaluation_alignment_max_time_trace],
    ["evaluation_alignment_trace_cost", options.evaluation_alignment_trace_cost],
    ["evaluation_alignment_model_cost", options.evaluation_alignment_model_cost],
    ["evaluation_alignment_silent_model_cost", options.evaluation_alignment_silent_model_cost],
    ["evaluation_alignment_sync_cost", options.evaluation_alignment_sync_cost],
    ["evaluation_alignment_threshold_border_agreement", options.evaluation_alignment_threshold_border_agreement],
  ] as const) {
    if (value !== undefined && (!Number.isFinite(value) || value < 0)) {
      throw new Error(`${name} must be a finite non-negative number`);
    }
  }
  if (options.evaluation_alignment_exponent !== undefined &&
      (!Number.isFinite(options.evaluation_alignment_exponent) ||
       options.evaluation_alignment_exponent <= 1)) {
    throw new Error("evaluation_alignment_exponent must be greater than 1");
  }
  const generatorAlignment = alignmentVariant === "generator_dijkstra_no_heuristics" ||
    alignmentVariant === "generator_dijkstra_less_memory";
  if (options.evaluation_alignment_max_alignments !== undefined && !generatorAlignment) {
    throw new Error("evaluation_alignment_max_alignments requires a generator variant");
  }
  const discountedAlignment = alignmentVariant === "discounted_a_star";
  if ((options.evaluation_alignment_exponent !== undefined ||
       options.evaluation_alignment_synchronous_dijkstra !== undefined) &&
      !discountedAlignment) {
    throw new Error("discounted alignment controls require discounted_a_star");
  }
  const decomposedAlignment = alignmentVariant === "recompos_maximal";
  if ((options.evaluation_alignment_max_total_states !== undefined ||
       options.evaluation_alignment_threshold_border_agreement !== undefined ||
       options.evaluation_alignment_use_cache !== undefined ||
       options.evaluation_alignment_clear_cache !== undefined) &&
      !decomposedAlignment) {
    throw new Error("decomposed alignment controls require recompos_maximal");
  }
  if (decomposedAlignment &&
      options.evaluation_alignment_cost_profile === "legacy") {
    throw new Error("recompos_maximal requires standard costs");
  }
  if (options.precision_variant !== undefined && !["auto", "etconformance_token", "align_etconformance", "automaton_after_align"].includes(options.precision_variant)) {
    throw new Error("precision_variant is invalid");
  }
  if (options.evaluation_token_replay_variant === "backwards" &&
      options.fitness_consider_unknown_activities === true) {
    throw new Error("fitness_consider_unknown_activities requires forward replay");
  }
  if (options.precision_use_task_ids === true &&
      options.precision_variant !== "automaton_after_align") {
    throw new Error("precision_use_task_ids requires automaton_after_align");
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
  if (options.simplicity_arc_degree_k !== undefined && !Number.isFinite(options.simplicity_arc_degree_k)) {
    throw new Error("simplicity_arc_degree_k must be finite");
  }
  const weights = [
    ["fitness_weight", options.fitness_weight],
    ["precision_weight", options.precision_weight],
    ["simplicity_weight", options.simplicity_weight],
    ["generalization_weight", options.generalization_weight],
  ] as const;
  for (const [name, value] of weights) {
    if (value !== undefined && typeof value !== "number") throw new Error(`${name} must be a number`);
  }
  for (const [name, value] of [
    ["evaluation_initial_marking", options.evaluation_initial_marking],
    ["evaluation_final_marking", options.evaluation_final_marking],
    ["simplicity_initial_marking", options.simplicity_initial_marking],
    ["simplicity_final_marking", options.simplicity_final_marking],
  ] as const) {
    if (value !== undefined && (value.length === 0 || value.some((item) => !Number.isInteger(item) || item < 0))) {
      throw new Error(`${name} must contain non-negative integers`);
    }
  }

  const query = new URLSearchParams();
  setProcessTreePetriNetVariant(query, options.process_tree_variant);
  query.set("evaluation_family", family);
  const add = (name: string, value: unknown) => { if (value !== undefined) query.set(name, Array.isArray(value) ? value.join(",") : name.endsWith("_weight") && Object.is(value,-0) ? "-0" : String(value)); };
  add("fitness_variant", options.fitness_variant);
  add("evaluation_input_adapter", options.evaluation_input_adapter);
  add("source_id", options.source_id);
  if (options.evaluation_activity_attribute !== undefined) {
    add("evaluation_activity_attribute",
        options.evaluation_activity_attribute.trim());
  }
  if (options.evaluation_case_id_attribute !== undefined) {
    add("evaluation_case_id_attribute",
        options.evaluation_case_id_attribute.trim());
  }
  add("evaluation_max_rows_to_materialize",
      options.evaluation_max_rows_to_materialize);
  add("evaluation_initial_marking", options.evaluation_initial_marking);
  add("evaluation_final_marking", options.evaluation_final_marking);
  add("evaluation_token_replay_variant", options.evaluation_token_replay_variant);
  add("fitness_consider_unknown_activities", options.fitness_consider_unknown_activities);
  add("fitness_cleaning_token_flood", options.fitness_cleaning_token_flood);
  add("evaluation_alignment_variant", options.evaluation_alignment_variant);
  add("evaluation_alignment_max_states", options.evaluation_alignment_max_states);
  add("evaluation_alignment_max_total_states", options.evaluation_alignment_max_total_states);
  add("evaluation_alignment_max_time", options.evaluation_alignment_max_time);
  add("evaluation_alignment_max_time_trace", options.evaluation_alignment_max_time_trace);
  add("evaluation_alignment_max_alignments", options.evaluation_alignment_max_alignments);
  add("evaluation_alignment_exponent", options.evaluation_alignment_exponent);
  add("evaluation_alignment_synchronous_dijkstra", options.evaluation_alignment_synchronous_dijkstra);
  add("evaluation_alignment_cost_profile", options.evaluation_alignment_cost_profile);
  add("evaluation_alignment_trace_cost", options.evaluation_alignment_trace_cost);
  add("evaluation_alignment_model_cost", options.evaluation_alignment_model_cost);
  add("evaluation_alignment_silent_model_cost", options.evaluation_alignment_silent_model_cost);
  add("evaluation_alignment_sync_cost", options.evaluation_alignment_sync_cost);
  add("evaluation_alignment_threshold_border_agreement", options.evaluation_alignment_threshold_border_agreement);
  add("evaluation_alignment_use_cache", options.evaluation_alignment_use_cache);
  add("evaluation_alignment_clear_cache", options.evaluation_alignment_clear_cache);
  add("precision_variant", options.precision_variant);
  add("precision_use_task_ids", options.precision_use_task_ids);
  add("precision_max_states", options.precision_max_states);
  add("generalization_variant", options.generalization_variant);
  add("simplicity_variant", options.simplicity_variant);
  add("simplicity_arc_degree_k", options.simplicity_arc_degree_k);
  add("simplicity_max_states", options.simplicity_max_states);
  add("simplicity_initial_marking", options.simplicity_initial_marking);
  add("simplicity_final_marking", options.simplicity_final_marking);
  add("fitness_weight", options.fitness_weight);
  add("precision_weight", options.precision_weight);
  add("simplicity_weight", options.simplicity_weight);
  add("generalization_weight", options.generalization_weight);
  return query.toString();
}

/** Runs one selected quality evaluator against a cached model. */
export async function evaluateModel(miner: string, options: EvaluationOptions = {}): Promise<ModelEvaluation> {
  const query = new URLSearchParams(evaluationQuery(options));
  query.set("miner", miner);
  const result=await postNativeChecked(`/api/evaluate?${query.toString()}`, isModelEvaluation, "model evaluation");
  const expected=(options.evaluation_family??"composite")==="simplicity"?"model_only":options.evaluation_input_adapter??"log";
  if (result.miner!==miner || (result.input_context && (result.input_context.adapter!==expected ||
      (options.source_id!==undefined && result.input_context.source_id!==options.source_id)))) {
    throw new Error("The native evaluation does not match the requested model, input shape or source.");
  }
  return result;
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
    ["invariant_search_limit", options.invariant_search_limit],
  ] as const) {
    if (value !== undefined && (!Number.isInteger(value) || value < (name === "max_states" || name === "invariant_search_limit" ? 1 : 0))) {
      throw new Error(`${name} must be a valid integer`);
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
  setProcessTreePetriNetVariant(query, options.process_tree_variant);
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
  add("invariant_search_limit", options.invariant_search_limit);
  add("full_bootstrap", options.full_bootstrap);
  add("diagnostics", options.diagnostics);
  add("return_asap_when_unsound", options.return_asap_when_unsound);
  return query.toString();
}

/** Runs one selected Petri-analysis family against a cached model. */
export async function analyzePetriModel(options: PetriAnalysisOptions = {}): Promise<PetriAnalysisResult> {
  const query = petriAnalysisQuery(options);
  return postNativeChecked(`/api/petri-analysis?${query}`, isPetriAnalysisResult, "Petri analysis");
}

/** Stable query serialization for stochastic-language distance controls. */
export function emdQuery(options: EmdOptions = {}): string {
  const variant = options.emd_variant ?? "pyemd";
  if (variant !== "exact" && variant !== "pyemd") {
    throw new Error("emd_variant is invalid");
  }
  for (const [name, value] of Object.entries(options)) {
    if (name === "emd_variant") continue;
    if (typeof value !== "boolean") throw new Error(`${name} must be boolean`);
  }
  const query = new URLSearchParams();
  query.set("emd_variant", variant);
  if (options.normalize_masses !== undefined) query.set("normalize_masses", String(options.normalize_masses));
  if (options.strict_equal_mass !== undefined) query.set("strict_equal_mass", String(options.strict_equal_mass));
  if (options.use_fast_emd !== undefined) query.set("use_fast_emd", String(options.use_fast_emd));
  return query.toString();
}

/** Compares the cached log's stochastic language with an uploaded reference log. */
export async function evaluateEmd(reference: File, options: EmdOptions = {}): Promise<EmdResult> {
  const form = new FormData();
  form.append("reference_log", reference, reference.name);
  const query = emdQuery(options);
  const path = `/api/evaluate-emd${query ? `?${query}` : ""}`;
  let response: Response;
  try {
    response = await importedModelFetch(path, { method: "POST", body: form });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed = await readAlignmentResponse(response, { path,
    reference: { name: reference.name, size: reference.size, last_modified: reference.lastModified } });
  if (!response.ok) {
    if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isEmdResult(parsed)) throw new Error("the Minera backend returned an invalid stochastic-language distance");
  return parsed;
}

export const DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS: Required<ProcessTreeAlignmentOptions> = {
  variant: "SEARCH_GRAPH_PT",
  activity_attribute: "concept:name",
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
  const activityAttribute = options.activity_attribute;
  if (activityAttribute !== undefined && !activityAttribute.trim())
    throw new Error("pt_alignment_activity_attribute must not be empty");

  const query = new URLSearchParams();
  const add = (name: string, value: unknown) => { if (value !== undefined && value !== null && value !== "") query.set(name, String(value)); };
  if (variant !== DEFAULT_PROCESS_TREE_ALIGNMENT_OPTIONS.variant) add("pt_alignment_variant", variant);
  if (activityAttribute !== undefined) add("pt_alignment_activity_attribute", activityAttribute.trim());
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
  const query = processTreeAlignmentQuery(options);
  const path = `/api/align-process-tree${query ? `?${query}` : ""}`;
  let response: Response;
  try {
    response = await importedModelFetch(path, { method: "POST" });
  } catch (cause) {
    if (cause instanceof Error && cause.message.startsWith("pt_alignment_")) throw cause;
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const parsed = await readAlignmentResponse(response, { path });
  if (response.ok && isUncomputedAlignmentPayload(parsed) && isProcessTreeAlignmentResult(parsed)) return parsed;
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
  if (options.activity_attribute !== undefined && !options.activity_attribute.trim())
    throw new Error("activity_attribute must not be empty");
  if (options.activity_attribute !== undefined) o.activity_attribute = options.activity_attribute.trim();
  if (typeof o.max_states === "number" && o.max_states < 1) throw new Error("max_states must be positive");
  if (typeof o.max_total_states === "number" && o.max_total_states < 1) throw new Error("max_total_states must be positive");
  return advancedQuery(o);
}
export function petriAntiAlignmentQuery(options: PetriAntiAlignmentOptions = {}): string {
  const o: Record<string, unknown> = { ...options, variant: options.variant ?? "version_discounted_a_star" };
  checkedOptions(o, ["marking_limit", "max_states"]);
  if (options.activity_attribute !== undefined && !options.activity_attribute.trim())
    throw new Error("activity_attribute must not be empty");
  if (options.activity_attribute !== undefined) o.activity_attribute = options.activity_attribute.trim();
  if (typeof o.marking_limit === "number" && o.marking_limit < 1) throw new Error("marking_limit must be positive");
  if (typeof o.exponent === "number" && o.exponent <= 1) throw new Error("exponent must be greater than 1");
  return advancedQuery(o);
}
export function petriMultiAlignmentQuery(options: PetriMultiAlignmentOptions = {}): string {
  const o: Record<string, unknown> = { ...options, variant: options.variant ?? "version_discounted_a_star" };
  checkedOptions(o, ["marking_limit", "max_states"]);
  if (options.activity_attribute !== undefined && !options.activity_attribute.trim())
    throw new Error("activity_attribute must not be empty");
  if (options.activity_attribute !== undefined) o.activity_attribute = options.activity_attribute.trim();
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
  const addText = (name: string, value: string | undefined) => {
    if (value === undefined) return;
    if (value.length === 0) throw new Error(`${name} must not be empty`);
    query.set(name, value);
  };
  if (options.activity_key !== undefined) query.set("ilp_activity_key", options.activity_key);
  addText("ilp_artificial_start_activity", options.artificial_start_activity);
  addText("ilp_artificial_end_activity", options.artificial_end_activity);
  if (options.alpha !== undefined) {
    if (!Number.isFinite(options.alpha) || options.alpha < 0 || options.alpha > 1)
      throw new Error("alpha must be a finite number in [0,1]");
    query.set("ilp_alpha", String(options.alpha));
  }
  addText("ilp_causal_relation", options.causal_relation);
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
  for (const [name, value] of [["genetic_timestamp_key", options.timestamp_key],
                               ["genetic_case_id_key", options.case_id_key]] as const) {
    if (value !== undefined && value.length === 0) throw new Error(`${name} must not be empty`);
  }
  const entries: Array<[string, string | undefined]> = [
    ["genetic_activity_key", options.activity_key],
    ["genetic_timestamp_key", options.timestamp_key],
    ["genetic_case_id_key", options.case_id_key],
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

export function geneticMatrixConversionQuery(
  options: GeneticMinerOptions = {},
  variant: "to_petri_net" = "to_petri_net",
): string {
  if (variant !== "to_petri_net")
    throw new Error("genetic matrix conversion variant is not supported");
  const query = new URLSearchParams(geneticMinerQuery(options));
  query.set("genetic_conversion_variant", variant);
  return query.toString();
}

export function requestGeneticMatrixConversion(
  options: GeneticMinerOptions = {},
  variant: "to_petri_net" = "to_petri_net",
): Promise<GeneticMatrixConversionResult> {
  const query = geneticMatrixConversionQuery(options, variant);
  return postChecked(
    `/api/genetic-matrix-convert?${query}`,
    isGeneticMatrixConversionResult,
    "genetic matrix conversion",
  );
}
/** Query string for the cached-session organizational SNA endpoint. */
export function organizationalSnaQuery(options: OrganizationalSnaOptions = {}): string {
  const caseId = options.sna_case_id_attribute;
  const defaultCase = caseId === undefined || caseId === "case_id" ||
    caseId === "case:id" || caseId === "case:concept:name";
  const caseIdVariant = options.sna_variant === "handover_pandas" ||
    options.sna_variant === "working_together_pandas" ||
    options.sna_variant === "subcontracting_pandas";
  if (!defaultCase && !caseIdVariant)
    throw new Error(
      "sna_case_id_attribute requires a handover, working_together, or subcontracting pandas variant");
  const query = new URLSearchParams();
  const entries: Array<[string, string | undefined]> = [
    ["sna_variant", options.sna_variant],
    ["sna_activity_attribute", options.sna_activity_attribute],
    ["sna_resource_attribute", options.sna_resource_attribute],
    ["sna_case_id_attribute", options.sna_case_id_attribute],
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
export function requestOrganizationalSna(options: OrganizationalSnaOptions = {}, signal?: AbortSignal): Promise<OrganizationalSnaResult> {
  const query = organizationalSnaQuery(options);
  return postChecked(`/api/organizational-sna${query ? `?${query}` : ""}`, isOrganizationalSnaResult, "organizational SNA", undefined, signal);
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
  // An empty key is the field named "", not the omitted default.
  if (options.attribute_distribution_timestamp_key !== undefined) {
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
  if (options.clustering_activity_attribute !== undefined && !options.clustering_activity_attribute) {
    throw new Error("clustering_activity_attribute must be non-empty");
  }
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
  if (options.profile_variant !== undefined && options.profile_variant !== "sklearn_profiles") {
    throw new Error("profile_variant must be sklearn_profiles");
  }
  if (options.profile_activity_attribute !== undefined && !options.profile_activity_attribute) {
    throw new Error("profile_activity_attribute must be non-empty");
  }
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
    profile_activity_attribute: options.profile_activity_attribute,
    profile_variant: options.profile_variant,
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
/**
 * Rejects a non-2xx response with the server's own `{"error": ...}` message
 * when the body carries one, falling back to the HTTP status otherwise.
 * Returns without touching the body when the response is ok.
 */
export async function throwIfBackendError(response: Response): Promise<void> {
  if (response.ok) return;
  // On failure the caller never reads the body, so test doubles without clone() are fine.
  const source = typeof response.clone === "function" ? response.clone() : response;
  const payload: unknown = typeof source.json === "function" ? await source.json().catch(() => null) : null;
  if (payload !== null && typeof payload === "object" && "error" in payload) {
    const error = (payload as { error: unknown }).error;
    if (typeof error === "string" && error.length > 0) throw new Error(error);
    if (error !== undefined && error !== null) throw new Error(String(typeof error === "object" ? JSON.stringify(error) : error));
  }
  throw new Error(`the Minera backend answered with status ${response.status}`);
}
async function postChecked<T>(path: string, guard: (value: unknown) => value is T, label: string, body?: BodyInit, signal?: AbortSignal): Promise<T> {
  let response: Response;
  try { response = await importedModelFetch(path, { method: "POST", ...(body === undefined ? {} : { body }), ...(signal ? {signal} : {}) }); }
  catch(cause) { if(signal?.aborted)throw cause;throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed: unknown = await response.json().catch(() => null);
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!guard(parsed)) throw new Error(`the Minera backend returned an invalid ${label}`);
  return parsed;
}
async function postNativeChecked<T>(path:string,guard:(value:unknown)=>value is T,label:string):Promise<T> {
  let response:Response;
  try{response=await importedModelFetch(path,{method:"POST"});}catch{throw new Error("could not reach the Minera backend — is minera_server running?");}
  const parsed=await readNativeResponse(response,{path});
  // Diagnostic results carry their own error field, including an empty string.
  // Preserve valid uncomputed/exhausted results for inspection and export.
  if(response.ok&&guard(parsed))return parsed;
  if(parsed&&typeof parsed==="object"&&"error"in parsed)throw new Error(String((parsed as {error:unknown}).error));
  if(!response.ok)throw new Error(`the Minera backend answered with status ${response.status}`);
  if(!guard(parsed))throw new Error(`the Minera backend returned an invalid ${label}`);
  return parsed;
}
export function alignDecomposed(miner: string, options: DecomposedAlignmentOptions = {}): Promise<DecomposedAlignmentResult> {
  const query = new URLSearchParams(decomposedAlignmentQuery(options)); query.set("miner", miner);
  return postAlignmentChecked(`/api/align-decomposed?${query.toString()}`, isDecomposedAlignmentResult, "decomposed alignment");
}
export function alignPetriAnti(miner: string, options: PetriAntiAlignmentOptions = {}): Promise<PetriAntiAlignmentResult> {
  const query = new URLSearchParams(petriAntiAlignmentQuery(options)); query.set("miner", miner);
  return postAlignmentChecked(`/api/align-petri-anti?${query.toString()}`, isPetriAntiAlignmentResult, "Petri anti-alignment");
}
export function alignPetriMulti(miner: string, options: PetriMultiAlignmentOptions = {}): Promise<PetriMultiAlignmentResult> {
  const query = new URLSearchParams(petriMultiAlignmentQuery(options)); query.set("miner", miner);
  return postAlignmentChecked(`/api/align-petri-multi?${query.toString()}`, isPetriMultiAlignmentResult, "Petri multi-alignment");
}
export function reduceProcessTree(traceIndex: number, options: ProcessTreeReductionOptions = {}): Promise<ProcessTreeReductionResult> {
  if (!Number.isInteger(traceIndex) || traceIndex < 0) return Promise.reject(new Error("trace_index must be a non-negative integer"));
  const query = new URLSearchParams(processTreeReductionQuery(options)); query.set("trace_index", String(traceIndex));
  return postAlignmentChecked(`/api/reduce-process-tree?${query.toString()}`, isProcessTreeReductionResult, "process-tree reduction");
}

async function postAlignmentChecked<T>(path: string, guard: (value: unknown) => value is T, label: string): Promise<T> {
  let response: Response;
  try { response = await importedModelFetch(path, { method: "POST" }); }
  catch { throw new Error("could not reach the Minera backend — is minera_server running?"); }
  const parsed = await readAlignmentResponse(response, { path });
  // A valid uncomputed/exhausted result carries its reason in `error`; keep it.
  if (response.ok && isUncomputedAlignmentPayload(parsed) && guard(parsed)) return parsed;
  if (parsed && typeof parsed === "object" && "error" in parsed) throw new Error(String((parsed as { error: unknown }).error));
  if (!response.ok) throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!guard(parsed)) throw new Error(`the Minera backend returned an invalid ${label}`);
  return parsed;
}

export interface DecisionMiningOptions {
  process_tree_variant?: ProcessTreePetriNetVariant;
  max_depth?: number;
  min_split?: number;
  min_leaf?: number;
  max_categories?: number;
  max_thresholds?: number;
  min_gain?: number;
  include_trace_attributes?: boolean;
  attributes?: string[];
  /** Event field used to form replay variants; defaults to concept:name. */
  activity_attribute?: string;
}

export interface DecisionTablesRequestOptions extends DecisionMiningOptions {
  miner?: string;
  /** Number of preceding event-context snapshots encoded per choice. */
  lookback?: number;
}

export function decisionMiningQuery(options: DecisionMiningOptions = {}): string {
  const query = new URLSearchParams();
  setProcessTreePetriNetVariant(query, options.process_tree_variant);
  if (options.activity_attribute !== undefined) {
    if (typeof options.activity_attribute !== "string" ||
        !options.activity_attribute.trim())
      throw new Error("decision_activity_attribute must not be empty");
    query.set("decision_activity_attribute", options.activity_attribute.trim());
  }
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

/** Query string for encoded tables and guarded data-Petri projections. */
export function decisionTablesQuery(options: DecisionTablesRequestOptions = {}): string {
  const query = new URLSearchParams(decisionMiningQuery(options));
  if (options.lookback !== undefined) {
    if (!Number.isSafeInteger(options.lookback) ||
        options.lookback < 1 || options.lookback > 64)
      throw new Error("decision_lookback must be a safe integer from 1 through 64");
    query.set("decision_lookback", String(options.lookback));
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
    response = await importedModelFetch(`/api/decide?${params.toString()}`, {
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
  /** Server id of this import; later runs on "imported" are bound to it. */
  imported_model_id?: number;
}

/**
 * Checks the most recently mined log against a user-supplied model — a PNML
 * workflow net or a BPMN 2.0 process, told apart server-side. The server
 * keeps the net under the miner id "imported", so alignments can be
 * requested for it afterwards like for any discovered model.
 */
export async function checkModel(file: File, options: { pnmlPublicMarkings?: boolean; pnmlInferLegacyBoundaries?: boolean; sourceId?: string; activityKey?: string } = {}): Promise<ImportedModelCheck> {
  const form = new FormData();
  form.append("model", file, file.name);
  let response: Response;
  try {
    const parameters = new URLSearchParams();
    if (options.pnmlPublicMarkings !== undefined) parameters.set("pnml_public_markings", String(options.pnmlPublicMarkings));
    if (options.pnmlInferLegacyBoundaries !== undefined) parameters.set("pnml_infer_legacy_boundaries", String(options.pnmlInferLegacyBoundaries));
    if (options.sourceId !== undefined) parameters.set("source_id", options.sourceId);
    if (options.activityKey !== undefined) parameters.set("activity_attribute", options.activityKey);
    const query = parameters.size ? `?${parameters}` : "";
    response = await importedModelFetch(`/api/check-model${query}`, { method: "POST", body: form });
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
  // Later runs on "imported" are bound to this model; an answer without an id
  // (an older server) leaves them unbound rather than bound to the old model.
  setImportedModelId(importedModelIdOf(parsed));
  return parsed as ImportedModelCheck;
}

/**
 * Fetches one miner's model from the most recently mined log as standard PNML.
 * The net is translated server-side (the single source of truth used for
 * conformance), so this only works for a log mined this session.
 */
export interface ProcessTreePetriNetOptions {
  variant?: ProcessTreePetriNetVariant;
}

export function processTreePetriNetQuery(
  options: ProcessTreePetriNetOptions = {},
): string {
  if (options.variant !== undefined &&
      options.variant !== "standard" &&
      options.variant !== "transition_bordered") {
    throw new Error("variant expects standard or transition_bordered");
  }
  const params = new URLSearchParams();
  if (options.variant !== undefined) {
    params.set("process_tree_variant", options.variant);
  }
  return params.toString();
}

export async function fetchProcessTreePetriNet(
  options: ProcessTreePetriNetOptions = {},
): Promise<ProcessTreePetriNetConversionResult> {
  const query = processTreePetriNetQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(`/api/process-tree-petri${query ? `?${query}` : ""}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  const parsed: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isProcessTreePetriNetConversionResult(parsed)) {
    throw new Error("the Minera backend returned an invalid process-tree Petri net");
  }
  return parsed;
}

export async function fetchPnml(
  miner: string,
  options: ProcessTreePetriNetOptions = {},
): Promise<string> {
  const query = processTreePetriNetQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(
      `/api/pnml?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`,
    );
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
    response = await importedModelFetch(`/api/ptml${query}`);
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
export interface XesExportOptions {
  preserveNulls?: boolean;
  variant?: "line_by_line" | "etree";
  compress?: boolean;
  inferMetadata?: boolean;
}

export interface PetriNetReductionOptions {
  initial_marking?: number[];
  final_marking?: number[];
  parallel_place_fusion?: boolean;
  abstraction?: boolean;
}

export function petriNetReductionQuery(
  options: PetriNetReductionOptions = {},
): string {
  const params = new URLSearchParams();
  const marking = (name: string, values: number[] | undefined) => {
    if (values === undefined) return;
    if (values.length === 0 ||
        !values.every((value) => Number.isSafeInteger(value) && value >= 0))
      throw new Error(`${name} must be a non-empty list of non-negative integers`);
    params.set(name, values.join(","));
  };
  marking("petri_reduction_initial_marking", options.initial_marking);
  marking("petri_reduction_final_marking", options.final_marking);
  if (options.parallel_place_fusion !== undefined)
    params.set("petri_reduction_parallel_place_fusion",
               String(options.parallel_place_fusion));
  if (options.abstraction !== undefined)
    params.set("petri_reduction_abstraction", String(options.abstraction));
  return params.toString();
}

export async function reducePetriNet(
  model: Blob,
  options: PetriNetReductionOptions = {},
): Promise<PetriNetReductionResult> {
  const query = petriNetReductionQuery(options);
  const form = new FormData();
  form.append("model", model, "model.pnml");
  let response: Response;
  try {
    response = await importedModelFetch(
      `/api/reduce-petri-net${query ? `?${query}` : ""}`,
      { method: "POST", body: form },
    );
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isPetriNetReductionResult(payload))
    throw new Error("the Minera backend returned an invalid Petri-net reduction");
  return payload;
}

export function xesExportQuery(options: XesExportOptions = {}): string {
  if (options.variant !== undefined &&
      !["line_by_line", "etree"].includes(options.variant))
    throw new Error("XES export variant is not supported");
  const query = new URLSearchParams();
  if (options.variant !== undefined)
    query.set("xes_export_variant", options.variant);
  if (options.compress !== undefined)
    query.set("xes_compress", String(options.compress));
  if (options.inferMetadata !== undefined)
    query.set("xes_infer_metadata", String(options.inferMetadata));
  if (options.preserveNulls !== undefined) {
    if (typeof options.preserveNulls !== "boolean")
      throw new Error("XES preserveNulls must be boolean");
    query.set("xes_preserve_nulls", String(options.preserveNulls));
  }
  return query.toString();
}

export async function fetchXes(options: XesExportOptions = {}): Promise<Blob> {
  const query = xesExportQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(`/api/xes${query ? `?${query}` : ""}`);
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
  return response.blob();
}

/** Requests the encoded decision tables for one cached model. */
export function requestDecisionTables(
  miner?: string,
  options?: DecisionTablesRequestOptions,
): Promise<DecisionTablesResult>;
export function requestDecisionTables(
  options?: DecisionTablesRequestOptions,
): Promise<DecisionTablesResult>;
export function requestDecisionTables(
  minerOrOptions: string | DecisionTablesRequestOptions = "inductive",
  legacyOptions: DecisionTablesRequestOptions = {},
): Promise<DecisionTablesResult> {
  const options = typeof minerOrOptions === "string"
    ? { ...legacyOptions, miner: minerOrOptions }
    : (minerOrOptions ?? {});
  const miner = options.miner ?? "inductive";
  if (!miner.trim()) return Promise.reject(new Error("miner must not be empty"));
  const query = new URLSearchParams({ miner });
  try {
    for (const [key, value] of new URLSearchParams(decisionTablesQuery(options)))
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
  options?: DecisionTablesRequestOptions,
): Promise<DataPetriNetResult>;
export function requestDataPetriNet(
  options?: DecisionTablesRequestOptions,
): Promise<DataPetriNetResult>;
export function requestDataPetriNet(
  minerOrOptions: string | DecisionTablesRequestOptions = "inductive",
  legacyOptions: DecisionTablesRequestOptions = {},
): Promise<DataPetriNetResult> {
  const options = typeof minerOrOptions === "string"
    ? { ...legacyOptions, miner: minerOrOptions }
    : (minerOrOptions ?? {});
  const miner = options.miner ?? "inductive";
  if (!miner.trim()) return Promise.reject(new Error("miner must not be empty"));
  const query = new URLSearchParams({ miner });
  try {
    for (const [key, value] of new URLSearchParams(decisionTablesQuery(options)))
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
  activities_percentage?: number;
  paths_percentage?: number;
  /** Dependency threshold used by the connected-edge filter. */
  keep_connected?: number;
  keep_all_activities?: boolean;
  to_activity?: string;
  from_activity?: string;
  contain_activity?: string;
}

export function dfgExportQuery(options: DfgExportOptions = {}): string {
  const query = new URLSearchParams();
  if (options.noise_threshold !== undefined) {
    if (!Number.isFinite(options.noise_threshold))
      throw new Error("noise_threshold must be finite");
    query.set("noise_threshold", String(options.noise_threshold));
  }
  if (options.most_common_paths !== undefined) {
    // Labels are literal: the text goes as typed (review Y1).
    const paths = options.most_common_paths;
    if (!paths.trim()) throw new Error("most_common_paths must not be empty");
    if (options.noise_threshold === undefined)
      throw new Error("most_common_paths requires noise_threshold");
    query.set("most_common_paths", paths);
  }
  const fractionOptions = [
    ["activities_percentage", options.activities_percentage],
    ["paths_percentage", options.paths_percentage],
  ] as const;
  for (const [name, value] of fractionOptions) {
    if (value === undefined) continue;
    if (!Number.isFinite(value) || value < 0 || value > 1)
      throw new Error(`${name} must be finite and in [0, 1]`);
    query.set(name, String(value));
  }
  if (options.keep_connected !== undefined) {
    if (!Number.isFinite(options.keep_connected))
      throw new Error("keep_connected must be finite");
    query.set("keep_connected", String(options.keep_connected));
  }
  if (options.keep_all_activities !== undefined) {
    if (options.keep_all_activities &&
        options.paths_percentage === undefined &&
        options.keep_connected === undefined) {
      throw new Error(
        "keep_all_activities requires paths_percentage or keep_connected",
      );
    }
    query.set("keep_all_activities", options.keep_all_activities ? "true" : "false");
  }
  for (const name of ["to_activity", "from_activity", "contain_activity"] as const) {
    const value = options[name];
    if (value === undefined) continue;
    // Activity labels are literal: surrounding white space is part of the
    // name, and "" names the empty label (review Y1).
    query.set(name, value);
  }
  return query.toString();
}

export async function fetchDfg(options: DfgExportOptions = {}): Promise<string> {
  const query = dfgExportQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(`/api/dfg${query ? `?${query}` : ""}`);
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

const dfgDataframeOperationNames = [
  "graph", "partial_order", "concurrent_events",
] as const;
const dfgDataframeMeasureNames = [
  "frequency", "performance", "both", "cost",
] as const;
const dfgDataframeBackendNames = ["pandas", "polars"] as const;
const dfgDataframeAggregationNames = [
  "mean", "median", "min", "max", "stdev", "sum", "raw_values", "all",
] as const;

function dfgDataframeOption(options: DfgDataframeOptions, key: string): unknown {
  const values = options as unknown as Record<string, unknown>;
  const direct = values[key];
  const prefixed = values[`dfg_${key}`];
  if (direct !== undefined && prefixed !== undefined &&
      JSON.stringify(direct) !== JSON.stringify(prefixed)) {
    throw new Error(`${key} and dfg_${key} must not disagree`);
  }
  return direct !== undefined ? direct : prefixed;
}

function dfgDataframeEnum<T extends string>(
  value: unknown,
  name: string,
  allowed: readonly T[],
  fallback: T,
): T {
  if (value === undefined) return fallback;
  if (typeof value !== "string" || !allowed.includes(value as T))
    throw new Error(`${name} must be one of ${allowed.join(", ")}`);
  return value as T;
}

function dfgDataframeString(
  options: DfgDataframeOptions,
  key: string,
  allowEmpty = false,
): string | undefined {
  const value = dfgDataframeOption(options, key);
  if (value === undefined) return undefined;
  if (typeof value !== "string") throw new Error(`${key} must be a string`);
  const normalized = value.trim();
  if (!allowEmpty && !normalized) throw new Error(`${key} must not be empty`);
  return normalized || undefined;
}

function dfgDataframeBoolean(
  options: DfgDataframeOptions,
  key: string,
): boolean | undefined {
  const value = dfgDataframeOption(options, key);
  if (value === undefined) return undefined;
  if (typeof value !== "boolean") throw new Error(`${key} must be boolean`);
  return value;
}

function dfgDataframeNonnegativeInteger(
  options: DfgDataframeOptions,
  key: string,
): number | undefined {
  const value = dfgDataframeOption(options, key);
  if (value === undefined) return undefined;
  if (!Number.isSafeInteger(value) || (value as number) < 0)
    throw new Error(`${key} must be a non-negative integer`);
  return value as number;
}

function dfgDataframeSlotString(
  options: DfgDataframeOptions,
  businessHours: boolean | undefined,
): string | undefined {
  const raw = dfgDataframeOption(options, "business_hour_slots");
  if (raw === undefined) return undefined;
  if (typeof raw !== "string")
    throw new Error("business_hour_slots must be `none` or start-end pairs");
  const slots = raw.trim();
  if (!slots) throw new Error("business_hour_slots must be `none` or start-end pairs");
  if (businessHours !== true)
    throw new Error("business_hour_slots requires business_hours=true");
  if (slots === "none") return slots;
  const valid = slots.split(",").every((part) => {
    const separator = part.includes(":") ? part.indexOf(":") : part.indexOf("-");
    if (separator <= 0 || separator === part.length - 1) return false;
    const start = Number(part.slice(0, separator).trim());
    const end = Number(part.slice(separator + 1).trim());
    return Number.isFinite(start) && Number.isFinite(end) &&
      start >= 0 && start < end && end <= 604800;
  });
  if (!valid)
    throw new Error("business_hour_slots must satisfy 0 <= start < end <= 604800");
  return slots;
}

/** Stable query serialization and validation for table-backed DFG requests. */
export function dfgDataframeQuery(options: DfgDataframeOptions = {}): string {
  const operation = dfgDataframeEnum(
    dfgDataframeOption(options, "operation"), "operation",
    dfgDataframeOperationNames, "graph",
  );
  const measureRaw = dfgDataframeOption(options, "measure");
  const measure = dfgDataframeEnum(
    measureRaw, "measure", dfgDataframeMeasureNames, "frequency",
  );
  const backend = dfgDataframeEnum(
    dfgDataframeOption(options, "backend"), "backend",
    dfgDataframeBackendNames, "pandas",
  );
  const activity = dfgDataframeString(options, "activity_attribute");
  const targetActivity = dfgDataframeString(options, "target_activity_attribute", true);
  const timestamp = dfgDataframeString(options, "timestamp_attribute");
  const startTimestamp = dfgDataframeString(options, "start_timestamp_attribute");
  // A cost selector is meaningful only for the cost measure. Ignore stale
  // text from the form when another graph measure is selected.
  const cost = measure === "cost"
    ? dfgDataframeString(options, "cost_attribute")
    : undefined;
  const caseId = dfgDataframeString(options, "case_id_attribute");
  const window = dfgDataframeNonnegativeInteger(options, "window");
  const keepOnce = dfgDataframeBoolean(options, "keep_once_per_case");
  const aggregationRaw = dfgDataframeOption(options, "performance_aggregation");
  const aggregation = dfgDataframeEnum(
    aggregationRaw, "performance_aggregation", dfgDataframeAggregationNames, "mean",
  );
  const businessHours = dfgDataframeBoolean(options, "business_hours");
  const businessSlots = dfgDataframeSlotString(options, businessHours);
  const sortCaseId = dfgDataframeBoolean(options, "sort_caseid_required");
  const sortTimestamp = dfgDataframeBoolean(options, "sort_timestamp_along_case_id");
  const reduceColumns = dfgDataframeBoolean(options, "reduce_columns");
  const workcalendar = dfgDataframeString(options, "workcalendar");
  const eventIndex = dfgDataframeString(options, "event_index_attribute");
  const maxStart = dfgDataframeString(options, "max_start_column");
  const minComplete = dfgDataframeString(options, "min_complete_column");
  const overlap = dfgDataframeString(options, "overlap_column");
  const strict = dfgDataframeBoolean(options, "strict");
  const keepFirst = dfgDataframeBoolean(options, "keep_first_following");

  const supplied = (key: string) => dfgDataframeOption(options, key) !== undefined;
  if (operation !== "graph") {
    for (const key of [
      "measure", "target_activity_attribute", "cost_attribute",
      "performance_aggregation", "window", "keep_once_per_case",
    ]) {
      if (supplied(key)) throw new Error(`${key} applies only to graph operation`);
    }
  }
  if (operation !== "concurrent_events") {
    for (const key of ["max_start_column", "min_complete_column", "overlap_column", "strict"]) {
      if (supplied(key)) throw new Error(`${key} applies only to concurrent_events operation`);
    }
  }
  if (operation === "graph" && supplied("event_index_attribute"))
    throw new Error("event_index_attribute applies only to partial_order");
  if (operation === "graph" && supplied("keep_first_following"))
    throw new Error("keep_first_following applies only to partial_order");
  if (operation === "concurrent_events" && supplied("event_index_attribute"))
    throw new Error("event_index_attribute applies only to partial_order");
  if (operation === "concurrent_events" && supplied("keep_first_following"))
    throw new Error("keep_first_following applies only to partial_order");
  if (measure === "cost" && cost === undefined)
    throw new Error("cost measure requires cost_attribute");

  const query = new URLSearchParams();
  const add = (name: string, value: unknown) => {
    if (value !== undefined) query.set(name, String(value));
  };
  add("dfg_operation", dfgDataframeOption(options, "operation") === undefined ? undefined : operation);
  add("dfg_measure", measureRaw === undefined ? undefined : measure);
  add("dfg_backend", dfgDataframeOption(options, "backend") === undefined ? undefined : backend);
  add("dfg_activity_attribute", activity);
  add("dfg_target_activity_attribute", targetActivity);
  add("dfg_timestamp_attribute", timestamp);
  add("dfg_start_timestamp_attribute", startTimestamp);
  add("dfg_cost_attribute", cost);
  add("dfg_case_id_attribute", caseId);
  add("dfg_window", window);
  add("dfg_keep_once_per_case", keepOnce);
  add("dfg_performance_aggregation", aggregationRaw === undefined ? undefined : aggregation);
  add("dfg_business_hours", businessHours);
  add("dfg_business_hour_slots", businessSlots);
  add("dfg_sort_caseid_required", sortCaseId);
  add("dfg_sort_timestamp_along_case_id", sortTimestamp);
  add("dfg_reduce_columns", reduceColumns);
  add("dfg_workcalendar", workcalendar);
  add("dfg_event_index_attribute", eventIndex);
  add("dfg_max_start_column", maxStart);
  add("dfg_min_complete_column", minComplete);
  add("dfg_overlap_column", overlap);
  add("dfg_strict", strict);
  add("dfg_keep_first_following", keepFirst);
  return query.toString();
}

/** Fetches one table-backed DFG result for the active mined session. */
export async function requestDfgDataframe(
  options: DfgDataframeOptions = {},
): Promise<DfgDataframeResult> {
  const query = dfgDataframeQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(`/api/dfg-dataframe${query ? `?${query}` : ""}`, {
      method: "GET",
    });
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (payload && typeof payload === "object" && "error" in payload)
    throw new Error(String((payload as { error: unknown }).error));
  if (!response.ok)
    throw new Error(`the Minera backend answered with status ${response.status}`);
  if (!isDfgDataframeResult(payload))
    throw new Error("the Minera backend returned an invalid dataframe DFG result");
  return payload;
}
export const dataframeDfgQuery = dfgDataframeQuery;
export const requestDataframeDfg = requestDfgDataframe;

/** Validates and normalizes a classic DFG text document through the backend. */
export async function importDfg(document: Blob): Promise<string> {
  let response: Response;
  try {
    response = await importedModelFetch("/api/dfg", {
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
    response = await importedModelFetch(`/api/dfg-precision${query ? `?${query}` : ""}`, {
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
    response = await importedModelFetch(`/api/dfg-to-petri${query ? `?${query}` : ""}`, {
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
    response = await importedModelFetch(`/api/process-cube${query ? `?${query}` : ""}`, {
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
    response = await importedModelFetch(`/api/streaming-dfg${query ? `?${query}` : ""}`, {
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
    response = await importedModelFetch(`/api/streaming-footprints?${query}`, {
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

function serializeTemporalProfile(
  values: Array<[string, string, number, number]>, name: string,
): string {
  if (!Array.isArray(values) || values.length === 0)
    throw new Error(`${name} must be a non-empty list`);
  return values.map((entry) => {
    if (!Array.isArray(entry) || entry.length !== 4 ||
        !Number.isFinite(entry[2]) || entry[2] < 0 ||
        !Number.isFinite(entry[3]) || entry[3] < 0) {
      throw new Error(`${name} entries must be source/target/mean/stddev`);
    }
    const source = validateStreamingFootprintActivity(entry[0]);
    const target = validateStreamingFootprintActivity(entry[1]);
    if (source.includes(":") || target.includes(":"))
      throw new Error(`${name} entries must be source/target/mean/stddev`);
    return `${source}->${target}:${entry[2]}:${entry[3]}`;
  }).join(",");
}

function serializeStreamingTemporalProfile(
  values: Array<[string, string, number, number]>,
): string {
  return serializeTemporalProfile(values, "streaming_temporal_profile");
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
    // Any finite zeta is accepted, as in the reference: zero or a negative
    // value reports every observed pair whose distance exceeds it.
    if (!Number.isFinite(options.streaming_temporal_zeta))
      throw new Error("streaming_temporal_zeta must be finite");
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
    response = await importedModelFetch(`/api/streaming-temporal?${query}`, {
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

export interface TemporalConformanceOptions {
  temporal_profile: Array<[string, string, number, number]>;
  temporal_variant?: "log" | "dataframe";
  temporal_activity_attribute?: string;
  temporal_case_id_attribute?: string;
  temporal_source_timestamp_attribute?: string;
  temporal_target_timestamp_attribute?: string;
  temporal_business_hours?: boolean;
  temporal_business_hour_slots?: string;
  temporal_strict_attributes?: boolean;
  temporal_zeta?: number;
}

/** Query string for conformance against a profile using the cached session log. */
export function temporalConformanceQuery(
  options: TemporalConformanceOptions,
): string {
  const query = new URLSearchParams();
  query.set("temporal_profile", serializeTemporalProfile(
    options.temporal_profile, "temporal_profile",
  ));
  if (options.temporal_variant !== undefined) {
    if (options.temporal_variant !== "log" && options.temporal_variant !== "dataframe")
      throw new Error("temporal_variant must be log or dataframe");
    if (options.temporal_variant !== "log")
      query.set("temporal_variant", options.temporal_variant);
  }
  for (const name of [
    "temporal_activity_attribute",
    "temporal_case_id_attribute",
    "temporal_source_timestamp_attribute",
    "temporal_target_timestamp_attribute",
    "temporal_business_hour_slots",
  ] as const) {
    const value = options[name];
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${name} must not be empty`);
      query.set(name, value.trim());
    }
  }
  if (options.temporal_business_hours !== undefined)
    query.set("temporal_business_hours", options.temporal_business_hours ? "true" : "false");
  if (options.temporal_strict_attributes !== undefined)
    query.set("temporal_strict_attributes", options.temporal_strict_attributes ? "true" : "false");
  if (options.temporal_zeta !== undefined) {
    if (Number.isNaN(options.temporal_zeta))
      throw new Error("temporal_zeta must not be NaN");
    query.set("temporal_zeta", String(options.temporal_zeta));
  }
  return query.toString();
}

/** Checks a supplied temporal profile against the latest cached event log. */
export function requestTemporalConformance(
  options: TemporalConformanceOptions,
): Promise<TemporalConformanceResult> {
  const query = temporalConformanceQuery(options);
  return postChecked(
    `/api/temporal-conformance?${query}`,
    isTemporalConformanceResult,
    "temporal profile conformance",
  );
}
export const conformTemporalProfile = requestTemporalConformance;

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
    response = await importedModelFetch(`/api/streaming-tbr${query ? `?${query}` : ""}`, {
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

function serializeComparisonMarking(name: string, value: number[]): string {
  if (value.length === 0) throw new Error(`${name} must contain at least one place`);
  if (value.some((entry) => !Number.isSafeInteger(entry) || entry < 0 || entry > 0xffffffff)) {
    throw new Error(`${name} must contain non-negative uint32 integers`);
  }
  return value.join(",");
}

function setProcessTreePetriNetVariant(
  params: URLSearchParams,
  value: ProcessTreePetriNetVariant | undefined,
): void {
  if (value === undefined) return;
  if (value !== "standard" && value !== "transition_bordered") {
    throw new Error("process_tree_variant expects standard or transition_bordered");
  }
  params.set("process_tree_variant", value);
}

export async function comparePetriUsage(
  reference: File,
  miner = "inductive",
  options: PetriUsageComparisonOptions = {},
): Promise<PetriUsageComparison> {
  const form = new FormData();
  form.append("reference_log", reference, reference.name);
  form.append("miner", miner);
  if (options.activity_attribute !== undefined) {
    const key = options.activity_attribute.trim();
    if (!key) throw new Error("activity_attribute must not be empty");
    form.append("activity_attribute", key);
  }
  if (options.initial_marking !== undefined) {
    form.append("initial_marking", serializeComparisonMarking(
      "initial_marking", options.initial_marking));
  }
  if (options.final_marking !== undefined) {
    form.append("final_marking", serializeComparisonMarking(
      "final_marking", options.final_marking));
  }
  for (const [name, value] of [
    ["walk_through_hidden_trans", options.walk_through_hidden_trans],
    ["try_to_reach_final_marking_through_hidden", options.try_to_reach_final_marking_through_hidden],
    ["stop_immediately_unfit", options.stop_immediately_unfit],
    ["cleaning_token_flood", options.cleaning_token_flood],
    ["exhaustive_invisible_exploration", options.exhaustive_invisible_exploration],
  ] as const) {
    if (value !== undefined) form.append(name, value ? "true" : "false");
  }
  let response: Response;
  try {
    response = await importedModelFetch("/api/compare-petri", { method: "POST", body: form });
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

export function basicPlayoutQuery(options: BasicPlayoutOptions = {}): string {
  const params = new URLSearchParams();
  setProcessTreePetriNetVariant(params, options.process_tree_variant);
  if (options.mode !== undefined) {
    if (options.mode !== "basic" && options.mode !== "extensive")
      throw new Error("mode must be basic or extensive");
    params.set("mode", options.mode);
  }
  if (options.traces !== undefined) {
    validatePlayoutInteger("traces", options.traces, true);
    params.set("traces", String(options.traces));
  }
  if (options.max_steps !== undefined) {
    if (options.mode === "extensive")
      throw new Error("max_steps requires basic mode");
    validatePlayoutInteger("max_steps", options.max_steps);
    params.set("max_steps", String(options.max_steps));
  }
  if (options.max_length !== undefined) {
    if (options.mode !== "extensive")
      throw new Error("max_length requires extensive mode");
    validatePlayoutInteger("max_length", options.max_length);
    params.set("max_length", String(options.max_length));
  }
  if (options.seed !== undefined) {
    validatePlayoutInteger("seed", options.seed);
    params.set("seed", String(options.seed));
  }
  for (const name of ["initial_marking", "final_marking"] as const) {
    const value = options[name];
    if (value === undefined) continue;
    if (!Array.isArray(value) || value.length === 0 ||
        value.some((entry) => !Number.isInteger(entry) || entry < 0 ||
          entry > 0xffffffff)) {
      throw new Error(`${name} must be a non-empty uint32 marking`);
    }
    params.set(name, value.join(","));
  }
  for (const name of ["start", "case_interval", "event_interval"] as const) {
    const value = options[name];
    if (value === undefined) continue;
    if (!Number.isFinite(value) || (name !== "start" && value < 0)) {
      throw new Error(`${name} must be a finite ${name === "start" ? "number" : "non-negative number"}`);
    }
    params.set(name, String(value));
  }
  for (const name of [
    "add_only_if_fm_is_reached", "fm_leq_accepted", "return_visited_elements",
  ] as const) {
    const value = options[name];
    if (value === undefined) continue;
    if (typeof value !== "boolean") throw new Error(`${name} must be boolean`);
    params.set(name, value ? "true" : "false");
  }
  if (options.return_elements !== undefined) {
    if (typeof options.return_elements !== "boolean")
      throw new Error("return_elements must be boolean");
    if (options.mode !== "extensive")
      throw new Error("return_elements requires extensive mode");
    params.set("return_elements", options.return_elements ? "true" : "false");
  }
  if (options.max_marking_occ !== undefined) {
    validatePlayoutInteger("max_marking_occ", options.max_marking_occ, true);
    if (options.mode !== "extensive")
      throw new Error("max_marking_occ requires extensive mode");
    params.set("max_marking_occ", String(options.max_marking_occ));
  }
  return params.toString();
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
  if (options.return_set_strings !== undefined) {
    if (typeof options.return_set_strings !== "boolean")
      throw new Error("return_set_strings must be boolean");
    if (options.return_set_strings && options.variant !== "EXTENSIVE")
      throw new Error("return_set_strings requires EXTENSIVE variant");
    params.set("return_set_strings", options.return_set_strings ? "true" : "false");
  }
  if (options.min_length !== undefined && options.max_length !== undefined && options.min_length > options.max_length) {
    throw new Error("min_length must not exceed max_length");
  }
  return params.toString();
}

export function declarePlayoutQuery(options: DeclarePlayoutOptions = {}): string {
  const params = new URLSearchParams();
  const bounded = (name: keyof DeclarePlayoutOptions, value: number | undefined,
                   maximum: number) => {
    if (value === undefined) return;
    if (!Number.isSafeInteger(value) || value < 0 || value > maximum)
      throw new Error(`${name} must be an integer between 0 and ${maximum}`);
    params.set(name, String(value));
  };
  bounded("traces", options.traces, 100000);
  bounded("min_length", options.min_length, 10000);
  bounded("max_length", options.max_length, 10000);
  bounded("seed", options.seed, Number.MAX_SAFE_INTEGER);
  if (options.min_length !== undefined && options.max_length !== undefined &&
      options.min_length > options.max_length)
    throw new Error("min_length must not exceed max_length");
  return params.toString();
}

export function stochasticPlayoutQuery(options: StochasticPlayoutOptions = {}): string {
  const params = new URLSearchParams();
  setProcessTreePetriNetVariant(params, options.process_tree_variant);
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
  if (options.fit_from_log !== undefined) {
    if (typeof options.fit_from_log !== "boolean") throw new Error("fit_from_log must be boolean");
    params.set("stochastic_fit_from_log", options.fit_from_log ? "true" : "false");
  }
  if (options.stop_at_final !== undefined) {
    if (typeof options.stop_at_final !== "boolean") throw new Error("stop_at_final must be boolean");
    params.set("stochastic_stop_at_final", options.stop_at_final ? "true" : "false");
  }
  if (options.return_visited_elements !== undefined) {
    if (typeof options.return_visited_elements !== "boolean")
      throw new Error("return_visited_elements must be boolean");
    params.set("stochastic_return_visited_elements",
      options.return_visited_elements ? "true" : "false");
  }
  for (const name of ["initial_marking", "final_marking"] as const) {
    const value = options[name];
    if (value === undefined) continue;
    if (!Array.isArray(value) || value.length === 0 ||
        value.some((entry) => !Number.isInteger(entry) || entry < 0 ||
          entry > 0xffffffff)) {
      throw new Error(`${name} must be a non-empty uint32 marking`);
    }
    params.set(`stochastic_${name}`, value.join(","));
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

/** Fetches the structured visitation sequence emitted by stochastic Petri playout. */
/**
 * Binds a playout request to the mined source the page is showing. The server
 * plays out its current session only while that session is still this source,
 * so a re-mine elsewhere (another tab) cannot answer for the wrong dataset.
 */
export function playoutUrl(url: string, sourceId?: string): string {
  if (sourceId === undefined) return url;
  return `${url}${url.includes("?") ? "&" : "?"}source_id=${encodeURIComponent(sourceId)}`;
}

export async function fetchStochasticPlayoutVisited(
  options: StochasticPlayoutOptions = {}, sourceId?: string,
): Promise<StochasticPlayoutVisitedResult> {
  const query = stochasticPlayoutQuery({
    ...options,
    return_visited_elements: true,
  });
  const response = await importedModelFetch(playoutUrl(`/api/playout?miner=stochastic${query ? `&${query}` : ""}`, sourceId));
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = expandPlayoutVisitedResult(await response.json());
  if (!isStochasticPlayoutVisitedResult(payload))
    throw new Error("the Minera backend returned an invalid stochastic Petri visited playout");
  return payload;
}

function monteCarloMarking(name: string, marking: number[] | undefined): string | undefined {
  if (marking === undefined) return undefined;
  if (!Array.isArray(marking) || marking.length === 0 ||
      marking.some((count) => !Number.isSafeInteger(count) || count < 0 || count > 0xffffffff)) {
    throw new Error(`${name} must be a non-empty list of uint32 token counts`);
  }
  return marking.join(",");
}

function validateMonteCarloMapTransition(
  transition: StochasticMapTransition,
  seen: Set<number>,
): void {
  if (!transition || !Number.isSafeInteger(transition.transition) ||
      transition.transition < 0 || seen.has(transition.transition)) {
    throw new Error("provided_stochastic_map must use unique non-negative transition indexes");
  }
  seen.add(transition.transition);
  if (transition.timing !== "timed" && transition.timing !== "immediate") {
    throw new Error("provided_stochastic_map contains an invalid timing");
  }
  if (!["deterministic", "exponential", "constant0", "immediate", "normal",
        "uniform", "lognormal", "gamma"].includes(transition.distribution)) {
    throw new Error("provided_stochastic_map contains an invalid distribution");
  }
  const { parameter, secondary_parameter: secondary,
    tertiary_parameter: tertiary, activation_weight: weight, priority } = transition;
  if (![parameter, secondary, tertiary, weight].every(Number.isFinite) ||
      (transition.distribution !== "uniform" && secondary < 0) || tertiary < 0 ||
      !Number.isSafeInteger(priority) || priority < -2147483648 ||
      priority > 2147483647) {
    throw new Error("provided_stochastic_map contains invalid numeric fields");
  }
  switch (transition.distribution) {
    case "exponential":
      if (parameter < 0 || (transition.timing === "timed" && parameter <= 0))
        throw new Error("provided_stochastic_map exponential rates must be positive when timed");
      break;
    case "uniform":
      if (secondary < parameter)
        throw new Error("provided_stochastic_map uniform bounds must be ordered");
      break;
    case "normal":
      break;
    case "deterministic":
      if (parameter < 0)
        throw new Error("provided_stochastic_map duration means must be non-negative");
      break;
    case "gamma":
      if (parameter < 0 || secondary <= 0)
        throw new Error("provided_stochastic_map gamma scale must be non-negative and shape positive");
      break;
    case "constant0":
    case "immediate":
      if (parameter !== 0 || secondary !== 0 || tertiary !== 0)
        throw new Error("provided_stochastic_map immediate distributions must have zero parameters");
      break;
    case "lognormal":
      break;
  }
}

function monteCarloStochasticMap(map: StochasticMapTransition[] | undefined): string | undefined {
  if (map === undefined) return undefined;
  if (!Array.isArray(map)) throw new Error("provided_stochastic_map must be an array");
  if (map.length === 0) return "empty";
  const seen = new Set<number>();
  for (const transition of map) validateMonteCarloMapTransition(transition, seen);
  return [...map]
    .sort((left, right) => left.transition - right.transition)
    .map((transition) => [
      transition.transition, transition.timing, transition.distribution,
      transition.parameter, transition.secondary_parameter,
      transition.tertiary_parameter, transition.activation_weight,
      transition.priority,
    ].join(":"))
    .join(",");
}

export function monteCarloQuery(options: MonteCarloOptions = {}): string {
  const params = new URLSearchParams();
  setProcessTreePetriNetVariant(params, options.process_tree_variant);
  const positiveIntegers: Array<keyof MonteCarloOptions> = ["simulations", "diagnostic_interval"];
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
      validatePlayoutNumber(name, value as number);
      params.set(`monte_carlo_${name}`, String(value));
    }
  }
  if (options.case_arrival_ratio !== undefined && options.case_arrival_seconds !== undefined) {
    throw new Error("case_arrival_ratio and case_arrival_seconds are aliases; supply only one");
  }
  for (const name of ["case_arrival_ratio", "case_arrival_seconds", "start",
    "default_duration_seconds", "small_scale_factor", "max_thread_execution_time"] as const) {
    const value = options[name];
    if (value !== undefined) {
      const positive = name === "small_scale_factor" || name === "max_thread_execution_time";
      if (!Number.isFinite(value) || (name !== "start" && value < 0) || (positive && value === 0)) {
        throw new Error(`${name} must be a finite ${name === "start" ? "number" : positive ? "positive number" : "non-negative number"}`);
      }
      params.set(`monte_carlo_${name}`, String(value));
    }
  }
  for (const [name, value] of [
    ["activity_key", options.activity_key],
    ["timestamp_key", options.timestamp_key],
  ] as const) {
    if (value !== undefined) {
      const trimmed = value.trim();
      if (!trimmed) throw new Error(`${name} must not be empty`);
      params.set(`monte_carlo_${name}`, trimmed);
    }
  }
  for (const [name, marking] of [
    ["initial_marking", options.initial_marking],
    ["final_marking", options.final_marking],
  ] as const) {
    const encoded = monteCarloMarking(name, marking);
    if (encoded !== undefined) params.set(`monte_carlo_${name}`, encoded);
  }
  if (options.token_replay_variant !== undefined) {
    if (options.token_replay_variant !== "forward" && options.token_replay_variant !== "backwards") {
      throw new Error("token_replay_variant must be forward or backwards");
    }
    params.set("monte_carlo_token_replay_variant", options.token_replay_variant);
  }
  const stochasticMap = monteCarloStochasticMap(options.provided_stochastic_map);
  if (stochasticMap !== undefined) {
    params.set("monte_carlo_provided_stochastic_map", stochasticMap);
  }
  if (options.force_distribution !== undefined) {
    if (!["normal", "uniform", "exponential"].includes(options.force_distribution)) {
      throw new Error("force_distribution must be normal, uniform, or exponential");
    }
    params.set("monte_carlo_force_distribution", options.force_distribution);
  }
  if (options.transition_weights !== undefined) {
    if (!Array.isArray(options.transition_weights) || options.transition_weights.length === 0 ||
        options.transition_weights.some((value) => !Number.isFinite(value))) {
      throw new Error("transition_weights must be a non-empty list of finite numbers");
    }
    params.set("monte_carlo_transition_weights", options.transition_weights.join(","));
  }
  if (options.transition_durations !== undefined) {
    if (!Array.isArray(options.transition_durations) || options.transition_durations.length === 0 ||
        options.transition_durations.some((duration) => {
          if (!duration || !Number.isFinite(duration.value_seconds)) return true;
          const distribution = duration.distribution;
          if (!["deterministic", "exponential", "constant0", "immediate", "normal", "uniform", "lognormal", "gamma"]
            .includes(distribution)) return true;
          const secondary = duration.secondary_seconds;
          const tertiary = duration.tertiary_seconds;
          const shapeFamily = distribution === "lognormal" || distribution === "gamma";
          const exponential = distribution === "exponential";
          if (secondary !== undefined && (!Number.isFinite(secondary) || (!shapeFamily && !exponential && secondary < 0))) return true;
          if (exponential && duration.value_seconds === 0 && (secondary ?? 0) < 0) return true;
          if (tertiary !== undefined && (!Number.isFinite(tertiary) || tertiary < 0 || (!shapeFamily && tertiary === 0))) return true;
          if ((distribution === "normal" || distribution === "uniform") && secondary === undefined) return true;
          if (duration.value_seconds < 0 && distribution !== "normal" && distribution !== "uniform") return true;
          if (distribution === "normal" && duration.value_seconds < 0 && secondary === 0) return true;
          if (distribution === "uniform") {
            const upper = duration.value_seconds + (secondary ?? 0);
            if (!Number.isFinite(upper) || (duration.value_seconds < 0 && upper <= 0)) return true;
          }
          if (shapeFamily && (duration.value_seconds <= 0 ||
              (tertiary === 0 && (secondary ?? 0) < 0))) return true;
          return false;
        })) {
      throw new Error("transition_durations must contain valid distribution parameters");
    }
    params.set("monte_carlo_transition_durations", options.transition_durations
      .map((duration) => {
        const distribution = duration.distribution;
        if (distribution === "constant0" || distribution === "immediate") return `${distribution}:0`;
        if (distribution === "exponential" && duration.secondary_seconds !== undefined) {
          return `${distribution}:${duration.value_seconds};${duration.secondary_seconds}`;
        }
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
          !/^\d+$/.test(place) || !Number.isSafeInteger(count) || count < 0)) {
      throw new Error("resources_per_place must map non-negative places to non-negative integers");
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
  for (const [name, value] of [
    ["activity_key", options.activity_key],
    ["timestamp_key", options.timestamp_key],
    ["case_id_key", options.case_id_key],
    ["artificial_start_activity", options.artificial_start_activity],
    ["artificial_end_activity", options.artificial_end_activity],
  ] as const) {
    if (value !== undefined) {
      if (!value.trim()) throw new Error(`${name} must not be empty`);
      params.set(`dfg_playout_${name}`, value);
    }
  }
  return params.toString();
}

async function fetchPlayoutText(path: string, init?: RequestInit): Promise<string> {
  return (await fetchPlayoutResponse(path, init)).text();
}

async function fetchPlayoutResponse(path: string, init?: RequestInit): Promise<Response> {
  let response: Response;
  try {
    response = init === undefined ? await importedModelFetch(path) : await importedModelFetch(path, init);
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
  return response;
}

/** Process-tree playout XES and whether the server cut it short: the
 * X-Minera-Truncated header is true when the EXTENSIVE word budget or the
 * max_trace_cap output bound left words out (the XES itself has no flag). */
export async function fetchProcessTreePlayoutDownload(
  options: ProcessTreePlayoutOptions = {}, sourceId?: string,
): Promise<{ xes: string; truncated: boolean }> {
  const query = processTreePlayoutQuery(options);
  const response = await fetchPlayoutResponse(
    playoutUrl(`/api/playout?miner=process_tree${query ? `&${query}` : ""}`, sourceId),
  );
  return { xes: await response.text(), truncated: response.headers.get("X-Minera-Truncated") === "true" };
}

/** Fetches Monte Carlo XES from the playout endpoint with an explicit mode. */
export async function fetchMonteCarloPlayout(
  miner = "inductive", options: MonteCarloOptions = {}, sourceId?: string,
): Promise<string> {
  const params = new URLSearchParams(monteCarloQuery(options));
  params.set("miner", miner);
  if (sourceId !== undefined) params.set("source_id", sourceId);
  params.set("mode", "monte_carlo");
  return fetchPlayoutText("/api/playout", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: params.toString(),
  });
}

/**
 * Compatibility overload for callers that used the mode name as the miner.
 * Monte Carlo controls are intentionally only exposed for this literal
 * discriminator; callers with an ordinary miner must use the dedicated
 * function so ambiguous controls cannot be silently reinterpreted.
 */
export function fetchPlayout(
  miner: "monte_carlo", options?: MonteCarloOptions, sourceId?: string,
): Promise<string>;
export function fetchPlayout(
  miner: string,
  options?: BasicPlayoutOptions | ProcessTreePlayoutOptions |
    DeclarePlayoutOptions | StochasticPlayoutOptions | DfgPlayoutOptions,
  sourceId?: string,
): Promise<string>;
export async function fetchPlayout(
  miner: string,
  options: BasicPlayoutOptions | ProcessTreePlayoutOptions |
    DeclarePlayoutOptions | StochasticPlayoutOptions | DfgPlayoutOptions | MonteCarloOptions = {},
  sourceId?: string,
): Promise<string> {
  if (miner === "monte_carlo") {
    return fetchMonteCarloPlayout("inductive", options as MonteCarloOptions, sourceId);
  }
  const monteCarloOnly = [
    "simulations", "case_arrival_ratio", "case_arrival_seconds",
    "default_duration_seconds", "force_distribution", "transition_durations",
    "resources_per_place", "default_resources_per_place", "provided_stochastic_map",
    "token_replay_variant", "diagnostic_interval", "small_scale_factor",
    "max_thread_execution_time", "enable_diagnostics",
  ] as const;
  const unsupported = monteCarloOnly.find((name) => name in options);
  if (unsupported !== undefined) {
    throw new Error(`${unsupported} is Monte Carlo-only; use fetchMonteCarloPlayout`);
  }
  const stochastic = miner === "stochastic" || "transition_weights" in options ||
    "fit_from_log" in options || "stop_at_final" in options;
  let query: string;
  if (stochastic) {
    const controls = stochasticPlayoutQuery(options as StochasticPlayoutOptions);
    query = `mode=stochastic${controls ? `&${controls}` : ""}`;
  } else {
    query = miner === "dfg"
      ? dfgPlayoutQuery(options as DfgPlayoutOptions)
      : miner === "process_tree"
        ? processTreePlayoutQuery(options as ProcessTreePlayoutOptions)
        : miner === "declare"
          ? declarePlayoutQuery(options as DeclarePlayoutOptions)
        : basicPlayoutQuery(options as BasicPlayoutOptions);
  }
  return fetchPlayoutText(
    playoutUrl(`/api/playout?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`, sourceId),
  );
}

/** Fetches the structured visitation sequence emitted by basic Petri playout. */
export async function fetchBasicPlayoutVisited(
  miner = "inductive",
  options: BasicPlayoutOptions = {},
  sourceId?: string,
): Promise<BasicPlayoutVisitedResult> {
  const query = basicPlayoutQuery({ ...options, return_visited_elements: true });
  let response: Response;
  try {
    response = await importedModelFetch(playoutUrl(`/api/playout?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`, sourceId));
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  const payload: unknown = expandPlayoutVisitedResult(await response.json().catch(() => null));
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload) {
      throw new Error(String((payload as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isBasicPlayoutVisitedResult(payload)) {
    throw new Error("the Minera backend returned an invalid Petri visited playout");
  }
  return payload;
}

/** Fetches alternating markings and transitions from extensive Petri playout. */
export async function fetchExtensivePlayoutVisited(
  miner = "inductive",
  options: BasicPlayoutOptions = {},
  sourceId?: string,
): Promise<ExtensivePlayoutVisitedResult> {
  const query = basicPlayoutQuery({
    ...options,
    mode: "extensive",
    return_elements: true,
  });
  let response: Response;
  try {
    response = await importedModelFetch(playoutUrl(`/api/playout?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`, sourceId));
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = expandPlayoutVisitedResult(await response.json().catch(() => null));
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isExtensivePlayoutVisitedResult(payload))
    throw new Error("the Minera backend returned an invalid extensive visited playout");
  return payload;
}

export function stochasticPetriQuery(
  options: StochasticPetriOptions = {},
): string {
  const params = new URLSearchParams();
  if (options.transition_rates !== undefined) {
    if (!Array.isArray(options.transition_rates) ||
        options.transition_rates.length === 0 ||
        options.transition_rates.some((rate) => !Number.isFinite(rate) || rate < 0))
      throw new Error("transition_rates must be a non-empty finite non-negative list");
    params.set("stochastic_petri_rates", options.transition_rates.join(","));
  }
  for (const name of ["initial_marking", "immediate_transitions"] as const) {
    const value = options[name];
    if (value === undefined) continue;
    if (!Array.isArray(value) || value.length === 0 ||
        value.some((entry) => !Number.isSafeInteger(entry) || entry < 0))
      throw new Error(`${name} must be a non-empty list of non-negative integers`);
    params.set(`stochastic_petri_${name}`, value.join(","));
  }
  if (options.max_states !== undefined) {
    validatePlayoutInteger("max_states", options.max_states, true);
    if (options.max_states > 2000)
      throw new Error("max_states must not exceed 2000 for the dense generator");
    params.set("stochastic_petri_max_states", String(options.max_states));
  }
  if (options.analysis !== undefined) {
    if (!["reachability", "transient", "steady_state"].includes(options.analysis))
      throw new Error("analysis must be reachability, transient, or steady_state");
    params.set("stochastic_petri_analysis", options.analysis);
  }
  if (options.time_seconds !== undefined) {
    if (!Number.isFinite(options.time_seconds) || options.time_seconds < 0)
      throw new Error("time_seconds must be finite and non-negative");
    if (options.time_seconds > 1_000_000)
      throw new Error("time_seconds exceeds the transient safety limit");
    if ((options.analysis ?? "reachability") !== "transient")
      throw new Error("time_seconds requires transient analysis");
    params.set("stochastic_petri_time", String(options.time_seconds));
  }
  if (options.initial_state !== undefined) {
    if (!Number.isSafeInteger(options.initial_state) || options.initial_state < 0)
      throw new Error("initial_state must be a non-negative integer");
    if ((options.analysis ?? "reachability") !== "transient")
      throw new Error("initial_state requires transient analysis");
    if (options.initial_probabilities !== undefined)
      throw new Error("initial_state and initial_probabilities are mutually exclusive");
    params.set("stochastic_petri_initial_state", String(options.initial_state));
  }
  if (options.initial_probabilities !== undefined) {
    if (!Array.isArray(options.initial_probabilities) ||
        options.initial_probabilities.length === 0 ||
        options.initial_probabilities.some((probability) =>
          !Number.isFinite(probability) || probability < 0))
      throw new Error("initial_probabilities must be a non-empty finite non-negative list");
    const mass = options.initial_probabilities.reduce((sum, probability) => sum + probability, 0);
    if (!Number.isFinite(mass) || Math.abs(mass - 1) > 1e-10)
      throw new Error("initial_probabilities must have unit mass");
    if ((options.analysis ?? "reachability") !== "transient")
      throw new Error("initial_probabilities requires transient analysis");
    params.set("stochastic_petri_initial_probabilities",
      options.initial_probabilities.join(","));
  }
  return params.toString();
}

export async function fetchStochasticPetri(
  miner = "inductive",
  options: StochasticPetriOptions = {},
): Promise<StochasticPetriResult> {
  const query = stochasticPetriQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(`/api/stochastic-petri?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`);
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isStochasticPetriResult(payload))
    throw new Error("the Minera backend returned an invalid stochastic Petri result");
  return payload;
}

export async function fetchStochasticPetriDistribution(
  miner = "inductive",
  options: StochasticPetriOptions = {},
): Promise<StochasticPetriDistributionResult> {
  if (options.analysis !== "transient" && options.analysis !== "steady_state")
    throw new Error("analysis must be transient or steady_state for distribution analysis");
  const query = stochasticPetriQuery(options);
  const response = await importedModelFetch(`/api/stochastic-petri?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`);
  const payload: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isStochasticPetriDistributionResult(payload))
    throw new Error("the Minera backend returned an invalid stochastic Petri distribution");
  return payload;
}

export function stochasticMapQuery(options: StochasticMapOptions = {}): string {
  const params = new URLSearchParams();
  if (options.source !== undefined) {
    if (options.source !== "log" && options.source !== "performance_dfg")
      throw new Error("source must be log or performance_dfg");
    params.set("stochastic_map_source", options.source);
  }
  if (options.distribution !== undefined) {
    const distributions = ["automatic", "deterministic", "exponential", "constant0",
      "immediate", "normal", "uniform", "lognormal", "gamma"];
    if (!distributions.includes(options.distribution))
      throw new Error("distribution is not supported");
    if (options.distribution === "automatic" && options.source === "performance_dfg")
      throw new Error("automatic distribution requires source log");
    params.set("stochastic_map_distribution", options.distribution);
  }
  if (options.default_parameter !== undefined) {
    if (!Number.isFinite(options.default_parameter) || options.default_parameter < 0)
      throw new Error("default_parameter must be finite and non-negative");
    if (options.distribution === "exponential" && options.default_parameter <= 0)
      throw new Error("default_parameter must be positive for exponential maps");
    if (options.distribution === "gamma" && options.default_parameter <= 0)
      throw new Error("default_parameter must be positive for gamma maps");
    params.set("stochastic_map_default_parameter", String(options.default_parameter));
  }
  const booleans = ["silent_transitions_immediate", "zero_duration_immediate",
    "fit_activation_weights", "fit_timing", "frequency_activation_weights"] as const;
  for (const name of booleans) {
    const value = options[name];
    if (value !== undefined) {
      if (typeof value !== "boolean") throw new Error(`${name} must be boolean`);
      params.set(`stochastic_map_${name}`, value ? "true" : "false");
    }
  }
  if (options.default_activation_weight !== undefined) {
    if (!Number.isFinite(options.default_activation_weight) ||
        options.default_activation_weight < 0)
      throw new Error("default_activation_weight must be finite and non-negative");
    params.set("stochastic_map_default_activation_weight",
      String(options.default_activation_weight));
  }
  if (options.conversion_variant !== undefined) {
    if (options.conversion_variant !== "activity_defines_place" &&
        options.conversion_variant !== "invisibles_no_duplicates")
      throw new Error("conversion_variant is not supported");
    params.set("stochastic_map_conversion_variant", options.conversion_variant);
  }
  if (options.initial_marking !== undefined) {
    if (!Array.isArray(options.initial_marking) || options.initial_marking.length === 0 ||
        options.initial_marking.some((entry) =>
          !Number.isSafeInteger(entry) || entry < 0))
      throw new Error("initial_marking must be a non-empty list of non-negative integers");
    params.set("stochastic_map_initial_marking", options.initial_marking.join(","));
  }
  if (options.max_states !== undefined) {
    validatePlayoutInteger("max_states", options.max_states, true);
    if (options.max_states > 2000)
      throw new Error("max_states must not exceed 2000 for the dense generator");
    params.set("stochastic_map_max_states", String(options.max_states));
  }
  return params.toString();
}

export async function fetchStochasticMap(
  miner = "inductive", options: StochasticMapOptions = {},
): Promise<StochasticMapResult> {
  const query = stochasticMapQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(`/api/stochastic-map?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`);
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isStochasticMapResult(payload))
    throw new Error("the Minera backend returned an invalid stochastic map result");
  return payload;
}

/** Fetches the deterministic activity-word set from extensive process-tree playout. */
export async function fetchProcessTreePlayoutSet(
  options: ProcessTreePlayoutOptions = {}, sourceId?: string,
): Promise<ProcessTreePlayoutSetResult> {
  const query = processTreePlayoutQuery({
    ...options,
    variant: "EXTENSIVE",
    return_set_strings: true,
  });
  let response: Response;
  try {
    response = await importedModelFetch(playoutUrl(`/api/playout?miner=process_tree${query ? `&${query}` : ""}`, sourceId));
  } catch {
    throw new Error("could not reach the Minera backend — is minera_server running?");
  }
  const payload: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    if (payload !== null && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isProcessTreePlayoutSetResult(payload))
    throw new Error("the Minera backend returned an invalid process-tree set playout");
  return payload;
}

/** Fetches DFG playout, decoding the structured classic variant response. */
export async function fetchDfgPlayout(
  options: DfgPlayoutOptions = {}, sourceId?: string,
): Promise<string | DfgPlayoutVariantsResult> {
  const query = dfgPlayoutQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(playoutUrl(`/api/playout?miner=dfg${query ? `&${query}` : ""}`, sourceId));
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
  if (options.return_variants && (options.variant ?? "classic") === "classic") {
    const parsed: unknown = await response.json().catch(() => null);
    if (!isDfgPlayoutVariantsResult(parsed)) {
      throw new Error("the Minera backend returned invalid DFG variants");
    }
    return parsed;
  }
  return response.text();
}

export async function fetchMonteCarloResult(
  miner = "inductive", options: MonteCarloOptions = {},
): Promise<MonteCarloResult> {
  const params = new URLSearchParams(monteCarloQuery(options));
  params.set("miner", miner);
  let response: Response;
  try {
    response = await importedModelFetch("/api/monte-carlo", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: params.toString(),
    });
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
export async function fetchBpmn(
  miner: string,
  options: ProcessTreePetriNetOptions = {},
): Promise<string> {
  const query = processTreePetriNetQuery(options);
  let response: Response;
  try {
    response = await importedModelFetch(
      `/api/bpmn?miner=${encodeURIComponent(miner)}${query ? `&${query}` : ""}`,
    );
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
  drift_variant?: "bose";
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
  if (options.drift_variant !== undefined && options.drift_variant !== "bose") {
    throw new Error("drift_variant must be bose");
  }
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value !== undefined) query.set(key, String(value));
  }
  return query.toString();
}

export async function requestConceptDrift(options: ConceptDriftOptions = {}): Promise<ConceptDriftResult> {
  const query = conceptDriftQuery(options);
  const response = await importedModelFetch(`/api/concept-drift${query ? `?${query}` : ""}`, { method: "POST" });
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/transform-target${query ? `?${query}` : ""}`, { method: "POST" });
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/log-util${query ? `?${query}` : ""}`, { method: "POST" });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isLogUtilityResult(payload)) throw new Error("the Minera backend returned an invalid log utility result");
  return payload;
}

export interface LogToFeaturesOptions {
  features_variant?: "event_based" | "trace_based" | "temporal";
  features_activity_key?: string;
  features_case_id_key?: string;
  features_timestamp_key?: string;
  features_start_timestamp_key?: string;
  features_resource_key?: string;
  features_arrival_rate?: string;
  features_finish_rate?: string;
  features_service_time?: string;
  features_waiting_time?: string;
  features_sojourn_time?: string;
  features_diff_start_end?: string;
  features_grouper_freq?: string;
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
  const calendarTemporal = options.features_variant === "temporal" &&
    (options.features_grouper_freq !== undefined ? options.features_grouper_freq !== "" : options.features_window_seconds === undefined);
  if (options.features_epsilon !== undefined &&
      (!Number.isFinite(options.features_epsilon) || options.features_epsilon < 0))
    throw new Error("features_epsilon must be finite and non-negative");
  if (options.features_min_num_different_string_values !== undefined &&
      (!Number.isSafeInteger(options.features_min_num_different_string_values) || options.features_min_num_different_string_values < 0))
    throw new Error("features_min_num_different_string_values must be a non-negative integer");
  if (options.features_max_num_different_string_values !== undefined &&
      (!Number.isSafeInteger(options.features_max_num_different_string_values) || options.features_max_num_different_string_values < 0))
    throw new Error("features_max_num_different_string_values must be a non-negative integer");
  if (options.features_variant !== undefined && options.features_variant !== "trace_based" && !calendarTemporal &&
      options.features_min_num_different_string_values !== undefined &&
      options.features_max_num_different_string_values !== undefined &&
      options.features_min_num_different_string_values > options.features_max_num_different_string_values)
    throw new Error("features string cardinality bounds are inverted");
  const query = new URLSearchParams();
  const encode = (s: string) => s.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value === undefined) continue;
    query.set(key, Array.isArray(value) ? value.map((entry) =>
      key === "features_feature_names" && options.features_variant !== "temporal" && entry === ""
        ? "\\e" : encode(entry)).join(",") : String(value));
  }
  return query.toString();
}
export async function requestLogToFeatures(options: LogToFeaturesOptions = {}): Promise<LogToFeaturesResult> {
  const query = logToFeaturesQuery(options);
  const response = await importedModelFetch(`/api/log-to-features${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isLogToFeaturesResult(payload)) throw new Error("the Minera backend returned an invalid feature extraction");
  return payload;
}

export type LogConversionSelector =
  | "to_event_log"
  | "to_event_stream"
  | "to_data_frame"
  | "to_nx"
  | "df_to_event_log_1v"
  | "df_to_event_log_nv";
export interface LogConversionOptions {
  log_conversion_variant?: LogConversionSelector;
  log_conversion_include_case_attributes?: boolean;
  log_conversion_case_attribute_prefix?: string;
  log_conversion_case_id_key?: string;
  log_conversion_stream_postprocessing?: boolean;
  log_conversion_max_rows_to_materialize?: number;
  log_conversion_deep_copy?: boolean;
  log_conversion_compress?: boolean;
  log_conversion_lazy_output?: boolean;
  log_conversion_graph_case_id_attribute?: string;
  log_conversion_include_df?: boolean;
  log_conversion_case_attributes_as_nodes?: string[];
  log_conversion_event_attributes_as_nodes?: string[];
  log_conversion_return_variants?: boolean;
  log_conversion_max_variants_to_return?: number;
  log_conversion_max_graph_nodes?: number;
  log_conversion_max_graph_edges?: number;
}
export function logConversionQuery(options: LogConversionOptions = {}): string {
  const variants: readonly LogConversionSelector[] = [
    "to_event_log", "to_event_stream", "to_data_frame", "to_nx",
    "df_to_event_log_1v", "df_to_event_log_nv",
  ];
  if (options.log_conversion_variant !== undefined && !variants.includes(options.log_conversion_variant))
    throw new Error("log_conversion_variant is not supported");
  for (const [key, value] of [
    ["log_conversion_case_id_key", options.log_conversion_case_id_key],
    ["log_conversion_graph_case_id_attribute", options.log_conversion_graph_case_id_attribute],
  ] as const) if (value !== undefined && value.trim().length === 0) throw new Error(`${key} must not be empty`);
  for (const [key, values] of [
    ["log_conversion_case_attributes_as_nodes", options.log_conversion_case_attributes_as_nodes],
    ["log_conversion_event_attributes_as_nodes", options.log_conversion_event_attributes_as_nodes],
  ] as const) {
    if (values !== undefined && (values.length === 0 || values.some((value) => value.trim().length === 0)))
      throw new Error(`${key} must contain non-empty attribute names`);
  }
  for (const [key, value, maximum] of [
    ["log_conversion_max_graph_nodes", options.log_conversion_max_graph_nodes, 200_000],
    ["log_conversion_max_graph_edges", options.log_conversion_max_graph_edges, 400_000],
  ] as const) if (value !== undefined && (!Number.isSafeInteger(value) || value <= 0 || value > maximum))
    throw new Error(`${key} must be a positive integer within the safety limit`);
  if (options.log_conversion_max_variants_to_return !== undefined
      && (!Number.isSafeInteger(options.log_conversion_max_variants_to_return)
        || options.log_conversion_max_variants_to_return < 0
        || options.log_conversion_max_variants_to_return > 100_000))
    throw new Error("log_conversion_max_variants_to_return must be a non-negative integer within the safety limit");
  if (options.log_conversion_max_rows_to_materialize !== undefined
      && (!Number.isSafeInteger(options.log_conversion_max_rows_to_materialize)
        || options.log_conversion_max_rows_to_materialize < 0
        || options.log_conversion_max_rows_to_materialize > 1_000_000))
    throw new Error("log_conversion_max_rows_to_materialize must be a non-negative integer within the safety limit");
  const encode = (value: string) => value.replaceAll("\\", "\\\\").replaceAll(",", "\\,");
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([left], [right]) => left.localeCompare(right))) {
    if (value === undefined) continue;
    query.set(key, Array.isArray(value) ? value.map(encode).join(",") : String(value));
  }
  return query.toString();
}
export async function requestLogConversion(options: LogConversionOptions = {}): Promise<LogConversionResult> {
  const query = logConversionQuery(options);
  const response = await importedModelFetch(`/api/log-convert${query ? `?${query}` : ""}`, { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isLogConversionResult(payload)) throw new Error("the Minera backend returned an invalid log conversion");
  return payload;
}

export interface EmbeddingOptions {
  embedding_variant?: EmbeddingVariant;
  embedding_provider?: EmbeddingProvider;
  embedding_model?: string;
  embedding_attribute_key?: string;
  embedding_event_id_key?: string;
  embedding_dimensions?: number;
  embedding_max_items?: number;
  embedding_keep_cases?: boolean;
  embedding_target?: string;
  embedding_top_k?: number;
}

export interface BrowserHistoryOptions {
  browser_history_variant: BrowserHistoryVariant;
  browser_history_path: string;
  browser_history_max_events?: number;
}
export function browserHistoryQuery(options: BrowserHistoryOptions): string {
  if (options.browser_history_path.trim().length === 0)
    throw new Error("browser_history_path must not be empty");
  if (options.browser_history_variant !== "chrome_history" &&
      options.browser_history_variant !== "firefox_history")
    throw new Error("browser_history_variant is not supported");
  if (options.browser_history_max_events !== undefined &&
      (!Number.isSafeInteger(options.browser_history_max_events) ||
       options.browser_history_max_events <= 0 ||
       options.browser_history_max_events > 10_000_000))
    throw new Error("browser_history_max_events must be in [1,10000000]");
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b)))
    if (value !== undefined) query.set(key, String(value));
  return query.toString();
}
export async function requestBrowserHistory(
  options: BrowserHistoryOptions,
): Promise<BrowserHistoryResult> {
  const response = await importedModelFetch(`/api/browser-history?${browserHistoryQuery(options)}`,
                               { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isBrowserHistoryResult(payload))
    throw new Error("the Minera backend returned invalid browser history");
  return payload;
}

export interface WindowsEventsOptions {
  windows_events_path: string;
  windows_events_max_events?: number;
}
export function windowsEventsQuery(options: WindowsEventsOptions): string {
  if (options.windows_events_path.trim().length === 0)
    throw new Error("windows_events_path must not be empty");
  if (options.windows_events_max_events !== undefined &&
      (!Number.isSafeInteger(options.windows_events_max_events) ||
       options.windows_events_max_events <= 0 ||
       options.windows_events_max_events > 10_000_000))
    throw new Error("windows_events_max_events must be in [1,10000000]");
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b)))
    if (value !== undefined) query.set(key, String(value));
  return query.toString();
}
export async function requestWindowsEvents(
  options: WindowsEventsOptions,
): Promise<WindowsEventsResult> {
  const response = await importedModelFetch(`/api/windows-events?${windowsEventsQuery(options)}`,
                               { method: "POST" });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isWindowsEventsResult(payload))
    throw new Error("the Minera backend returned invalid Windows events");
  return payload;
}
export interface RepositoryHistoryOptions {
  repository_owner: string;
  repository_name: string;
  repository_api_base_url?: string;
  repository_max_pages?: number;
  repository_max_issues?: number;
  repository_max_events?: number;
}
export function repositoryHistoryQuery(options: RepositoryHistoryOptions): string {
  if (!options.repository_owner.trim() || !options.repository_name.trim())
    throw new Error("repository owner and name must not be empty");
  const bounds: Array<[keyof RepositoryHistoryOptions, number]> = [
    ["repository_max_pages", 10_000], ["repository_max_issues", 1_000_000],
    ["repository_max_events", 10_000_000],
  ];
  for (const [key, maximum] of bounds) {
    const value = options[key];
    if (value !== undefined && (typeof value !== "number" || !Number.isSafeInteger(value) || value <= 0 || value > maximum))
      throw new Error(`${key} must be an integer in [1,${maximum}]`);
  }
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b)))
    if (value !== undefined) query.set(key, String(value));
  return query.toString();
}
export async function requestRepositoryHistory(
  options: RepositoryHistoryOptions, bearerToken?: string,
): Promise<RepositoryHistoryResult> {
  const headers: Record<string, string> = {};
  if (bearerToken) headers.Authorization = `Bearer ${bearerToken}`;
  const response = await importedModelFetch(`/api/repository-history?${repositoryHistoryQuery(options)}`,
                               { method: "POST", headers });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isRepositoryHistoryResult(payload))
    throw new Error("the Minera backend returned invalid repository history");
  return payload;
}
export async function requestWorkflowHistory(
  csv: string, maxEvents?: number,
): Promise<WorkflowHistoryResult> {
  if (maxEvents !== undefined && (!Number.isSafeInteger(maxEvents) || maxEvents <= 0 || maxEvents > 10_000_000))
    throw new Error("workflow_max_events must be in [1,10000000]");
  const query = maxEvents === undefined ? "" : `?workflow_max_events=${maxEvents}`;
  const response = await importedModelFetch(`/api/workflow-history${query}`, {
    method: "POST", headers: { "Content-Type": "text/csv" }, body: csv,
  });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload)
      throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isWorkflowHistoryResult(payload))
    throw new Error("the Minera backend returned invalid workflow history");
  return payload;
}
export async function requestEnterpriseHistory(
  csv: string, variant: "order_to_cash" | "accounting", maxEvents?: number,
): Promise<EnterpriseHistoryResult> {
  if (maxEvents !== undefined && (!Number.isSafeInteger(maxEvents) || maxEvents <= 0 || maxEvents > 10_000_000))
    throw new Error("enterprise_max_events must be in [1,10000000]");
  const query = new URLSearchParams({ enterprise_variant: variant });
  if (maxEvents !== undefined) query.set("enterprise_max_events", String(maxEvents));
  const response = await importedModelFetch(`/api/enterprise-history?${query}`, {
    method: "POST", headers: { "Content-Type": "text/csv" }, body: csv,
  });
  if (!response.ok) {
    const payload: unknown = await response.json().catch(() => null);
    if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error));
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  const payload: unknown = await response.json();
  if (!isEnterpriseHistoryResult(payload)) throw new Error("the Minera backend returned invalid enterprise history");
  return payload;
}
export interface CommunicationHistoryOptions { communication_variant: "mailbox" | "calendar"; communication_include_reminders?: boolean; communication_current_time?: string; communication_max_items?: number; communication_max_events?: number; }
export async function requestCommunicationHistory(csv: string, options: CommunicationHistoryOptions): Promise<CommunicationHistoryResult> {
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) if (value !== undefined) query.set(key, String(value));
  const response = await importedModelFetch(`/api/communication-history?${query}`, { method: "POST", headers: { "Content-Type": "text/csv" }, body: csv });
  if (!response.ok) { const payload: unknown = await response.json().catch(() => null); if (payload && typeof payload === "object" && "error" in payload) throw new Error(String((payload as { error: unknown }).error)); throw new Error(`the Minera backend answered with status ${response.status}`); }
  const payload: unknown = await response.json(); if (!isCommunicationHistoryResult(payload)) throw new Error("the Minera backend returned invalid communication history"); return payload;
}
export function embeddingsQuery(options: EmbeddingOptions = {}): string {
  if (options.embedding_provider !== undefined &&
      options.embedding_provider !== "hashing" &&
      options.embedding_provider !== "transformer")
    throw new Error("embedding_provider must be hashing or transformer");
  if (options.embedding_dimensions !== undefined) {
    if (!Number.isInteger(options.embedding_dimensions) ||
        options.embedding_dimensions < 0 || options.embedding_dimensions > 4096)
      throw new Error("embedding_dimensions must be an integer in [0,4096]");
    if (options.embedding_dimensions === 0 && options.embedding_provider !== "transformer")
      throw new Error("embedding_dimensions=0 requires transformer provider");
  }
  const query = new URLSearchParams();
  for (const [key, value] of Object.entries(options).sort(([a], [b]) => a.localeCompare(b))) {
    if (value !== undefined) query.set(key, String(value));
  }
  return query.toString();
}
export async function requestEmbeddings(options: EmbeddingOptions = {}, signal?: AbortSignal): Promise<EmbeddingApiResult> {
  const query = embeddingsQuery(options);
  const response = await importedModelFetch(`/api/embeddings${query ? `?${query}` : ""}`, { method: "POST", signal });
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
  const response = await importedModelFetch(`/api/label-splitting${query ? `?${query}` : ""}`, { method: "POST" });
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
  const response = await importedModelFetch(`/api/trie${query ? `?${query}` : ""}`);
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isTrieResult(payload)) throw new Error("the Minera backend returned an invalid trie");
  return payload;
}

export async function requestTriePetriNet(options: TrieOptions = {}): Promise<TriePetriConversionResult> {
  const query = trieQuery(options);
  const response = await importedModelFetch(`/api/trie-to-petri${query ? `?${query}` : ""}`);
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/interval-tree${query ? `?${query}` : ""}`);
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isIntervalTreeResult(payload)) throw new Error("the Minera backend returned an invalid interval tree");
  return payload;
}

export interface CaseRelationsOptions {
  merge_variant?: "pandas";
  merge_timestamp_key?: string;
  merge_case_id_key?: string;
  merge_left_suffix?: string;
  merge_right_suffix?: string;
  merge_left_column?: string;
  merge_right_column?: string;
}

export function caseRelationsQuery(options: CaseRelationsOptions = {}): string {
  const query = new URLSearchParams();
  if (options.merge_variant !== undefined) query.set("merge_variant", options.merge_variant);
  const strings: Array<[keyof CaseRelationsOptions, string]> = [
    ["merge_timestamp_key", "merge_timestamp_key"],
    ["merge_case_id_key", "merge_case_id_key"],
    ["merge_left_suffix", "merge_left_suffix"],
    ["merge_right_suffix", "merge_right_suffix"],
    ["merge_left_column", "merge_left_column"],
    ["merge_right_column", "merge_right_column"],
  ];
  for (const [property, key] of strings) {
    const value = options[property];
    if (value === undefined) continue;
    if (typeof value !== "string" || value.length === 0)
      throw new Error(`${key} must be a non-empty string`);
    query.set(key, value);
  }
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
  const response = await importedModelFetch(`/api/merge-case-relations${query ? `?${query}` : ""}`, { method: "POST", body: form });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isCaseRelationsResult(payload)) throw new Error("the Minera backend returned an invalid case-relations merge");
  return payload;
}

export interface LinkAnalysisOptions {
  link_analysis_out_column?: string;
  link_analysis_in_column?: string;
  link_analysis_sorting_column?: string;
  link_analysis_index_column?: string;
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
  add("link_analysis_index_column", options.link_analysis_index_column);
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
  const response = await importedModelFetch(`/api/link-analysis${query ? `?${query}` : ""}`, {
    method: "POST",
  });
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/timestamp-interleavings${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/random-tree${query ? `?${query}` : ""}`);
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isRandomTreeResult(payload)) throw new Error("the Minera backend returned an invalid random tree");
  return payload;
}

export interface OcelDiscoveryOptions extends OcpnDiscoveryFlags {
  variant?: OcelDiscoveryVariant;
  include_edge_metrics?: boolean;
  ocdfg_edge_metrics_event_id_key?: string;
  ocdfg_edge_metrics_object_id_key?: string;
  ocdfg_edge_metrics_object_type_key?: string;
  ocdfg_edge_metrics_activity_key?: string;
  ocdfg_edge_metrics_timestamp_key?: string;
  compute_edges_performance?: boolean;
  ocdfg_business_hours?: boolean;
  ocdfg_business_hour_slots_json?: string;
  ocdfg_event_id_attribute?: string;
  ocdfg_timestamp_attribute?: string;
  ocdfg_workcalendar_json?: string;
  ocdfg_activity_attribute?: string;
  ocdfg_object_type_attribute?: string;
  saw_activity_attribute?: string;
  saw_object_type_attribute?: string;
  ocpn_annotation?: "annotated" | "none";
  ocpn_activity_attribute?: string;
  ocpn_object_type_attribute?: string;
  diagnostics_with_token_based_replay?: boolean;
}

export function ocelDiscoveryQuery(options: OcelDiscoveryOptions = {}): string {
  const query = new URLSearchParams();
  appendOcpnDiscoveryFlags(query, options);
  if (options.variant !== undefined) query.set("variant", options.variant);
  if ((options.variant ?? "ocdfg") === "ocdfg" && options.include_edge_metrics !== undefined) {
    if (typeof options.include_edge_metrics !== "boolean") throw new Error("include_edge_metrics must be boolean");
    query.set("include_edge_metrics", String(options.include_edge_metrics));
    if (options.include_edge_metrics) {
      for (const key of ["ocdfg_edge_metrics_event_id_key", "ocdfg_edge_metrics_object_id_key",
        "ocdfg_edge_metrics_object_type_key", "ocdfg_edge_metrics_activity_key", "ocdfg_edge_metrics_timestamp_key"] as const) {
        if (options[key] !== undefined) {
          if (typeof options[key] !== "string") throw new Error(`${key} must be a string`);
          query.set(key, options[key]);
        }
      }
    }
  }
  if (options.compute_edges_performance !== undefined) {
    query.set("compute_edges_performance", options.compute_edges_performance ? "true" : "false");
  }
  if (options.ocdfg_business_hours !== undefined) {
    if (typeof options.ocdfg_business_hours !== "boolean") throw new Error("ocdfg_business_hours must be boolean");
    query.set("ocdfg_business_hours", String(options.ocdfg_business_hours));
  }
  for (const key of ["ocdfg_business_hour_slots_json", "ocdfg_event_id_attribute", "ocdfg_timestamp_attribute", "ocdfg_workcalendar_json"] as const)
    if (options[key] !== undefined) query.set(key, options[key]);
  if (options.ocpn_annotation !== undefined) query.set("ocpn_annotation", options.ocpn_annotation);
  const addSelector = (key: string, value: string | undefined) => {
    if (value === undefined) return;
    const selector = value.trim();
    if (!selector) throw new Error(`${key} must not be empty`);
    query.set(key, selector);
  };
  addSelector("ocpn_activity_attribute", options.ocpn_activity_attribute);
  addSelector("ocpn_object_type_attribute", options.ocpn_object_type_attribute);
  addSelector("ocdfg_activity_attribute", options.ocdfg_activity_attribute);
  addSelector("ocdfg_object_type_attribute", options.ocdfg_object_type_attribute);
  addSelector("saw_activity_attribute", options.saw_activity_attribute);
  addSelector("saw_object_type_attribute", options.saw_object_type_attribute);
  if (options.diagnostics_with_token_based_replay !== undefined) {
    if (options.diagnostics_with_token_based_replay !== true &&
        options.diagnostics_with_token_based_replay !== false) {
      throw new Error("diagnostics_with_token_based_replay must be boolean");
    }
    query.set("diagnostics_with_token_based_replay",
      options.diagnostics_with_token_based_replay ? "true" : "false");
  }
  return query.toString();
}

export async function requestOcelDiscovery(
  log: Blob,
  options: OcelDiscoveryOptions = {},
  signal?: AbortSignal,
): Promise<OcelDiscoveryResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelDiscoveryQuery(options);
  const response = await importedModelFetch(`/api/ocel/discover${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
    signal,
  });
  if (!response.ok) {
    const detail: unknown = await response.json().catch(() => null);
    const message = typeof detail === "object" && detail !== null && "error" in detail && typeof detail.error === "string"
      ? detail.error : `the Minera backend answered with status ${response.status}`;
    throw new Error(message);
  }
  const payload: unknown = await response.json();
  if (!isOcelDiscoveryResult(payload)) throw new Error("the Minera backend returned an invalid OCEL discovery");
  return payload;
}

/** Imports an OCEL 2 relational SQLite upload through the local backend. */
export async function requestOcelSqlite(log: Blob): Promise<OcelSqliteResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.sqlite";
  form.append("log", log, logName);
  const response = await importedModelFetch("/api/ocel/sqlite", { method: "POST", body: form });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelSqliteResult(payload)) throw new Error("the Minera backend returned an invalid OCEL SQLite import");
  return payload;
}

/** Exports an OCEL JSON document as a standard SQLite download. */
export async function requestOcelSqliteExport(
  log: Record<string, unknown> | Blob,
): Promise<Blob> {
  const response = await importedModelFetch("/api/ocel/sqlite?mode=export", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: log instanceof Blob ? log : nativeJson(log),
  });
  await throwIfBackendError(response);
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
  options: OcelCsvOptions = {},
): string {
  const query = new URLSearchParams();
  query.set("format", format);
  query.set("mode", mode);
  if (format === "csv" || format === "csv_objects") {
    appendOcelCsvOptions(query, options);
  }
  return query.toString();
}

export interface OcelCsvOptions {
  ocel_csv_event_id_column?: string;
  ocel_csv_activity_column?: string;
  ocel_csv_timestamp_column?: string;
  ocel_csv_object_type_prefix?: string;
  ocel_csv_object_id_column?: string;
  ocel_csv_object_type_column?: string;
  ocel_csv_delimiter?: string;
  ocel_csv_trim_fields?: boolean;
  ocel_csv_sort_events_by_timestamp?: boolean;
  ocel_csv_reject_unsupported?: boolean;
  ocel_csv_allow_lossy?: boolean;
}

function appendOcelCsvOptions(
  query: URLSearchParams,
  options: OcelCsvOptions,
): void {
  const strings: Array<[keyof OcelCsvOptions, string]> = [
    ["ocel_csv_event_id_column", "ocel_csv_event_id_column"],
    ["ocel_csv_activity_column", "ocel_csv_activity_column"],
    ["ocel_csv_timestamp_column", "ocel_csv_timestamp_column"],
    ["ocel_csv_object_type_prefix", "ocel_csv_object_type_prefix"],
    ["ocel_csv_object_id_column", "ocel_csv_object_id_column"],
    ["ocel_csv_object_type_column", "ocel_csv_object_type_column"],
  ];
  for (const [property, key] of strings) {
    const value = options[property];
    if (value === undefined) continue;
    if (typeof value !== "string" || value.length === 0) {
      throw new Error(`${key} must be a non-empty string`);
    }
    query.set(key, value);
  }
  if (options.ocel_csv_delimiter !== undefined) {
    if (typeof options.ocel_csv_delimiter !== "string" ||
        options.ocel_csv_delimiter.length !== 1) {
      throw new Error("ocel_csv_delimiter must contain exactly one character");
    }
    query.set("ocel_csv_delimiter", options.ocel_csv_delimiter);
  }
  const booleans: Array<[keyof OcelCsvOptions, string]> = [
    ["ocel_csv_trim_fields", "ocel_csv_trim_fields"],
    ["ocel_csv_sort_events_by_timestamp", "ocel_csv_sort_events_by_timestamp"],
    ["ocel_csv_reject_unsupported", "ocel_csv_reject_unsupported"],
    ["ocel_csv_allow_lossy", "ocel_csv_allow_lossy"],
  ];
  for (const [property, key] of booleans) {
    const value = options[property];
    if (value === undefined) continue;
    if (typeof value !== "boolean") throw new Error(`${key} must be boolean`);
    query.set(key, value ? "true" : "false");
  }
}

/** Imports classic or standard OCEL JSON/XML through the local backend. */
export async function requestOcelExchangeImport(
  log: Blob,
  format: OcelExchangeFormat = "json",
  objects?: Blob,
  options: OcelCsvOptions = {},
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
  const response = await importedModelFetch(`/api/ocel/io?${ocelExchangeQuery(format, "import", options)}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelExchangeResult(payload))
    throw new Error("the Minera backend returned an invalid OCEL exchange import");
  return payload;
}

/** Exports an OCEL JSON document as classic or standard JSON/XML. */
export async function requestOcelExchangeExport(
  log: Record<string, unknown> | Blob,
  format: OcelExchangeFormat = "json",
  options: OcelCsvOptions = {},
): Promise<Blob> {
  const response = await importedModelFetch(`/api/ocel/io?${ocelExchangeQuery(format, "export", options)}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: log instanceof Blob ? log : nativeJson(log),
  });
  await throwIfBackendError(response);
  const payload = await response.blob();
  const text = await payload.text();
  if (format.startsWith("json")) {
    let parsed: unknown;
    try { parsed = JSON.parse(text); } catch { throw new Error("the Minera backend returned invalid OCEL JSON"); }
    const isRecord = (value: unknown): value is Record<string, unknown> =>
      !!value && typeof value === "object" && !Array.isArray(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed) ||
        !((isRecord((parsed as Record<string, unknown>)["ocel:events"]) &&
           isRecord((parsed as Record<string, unknown>)["ocel:objects"])) ||
          (Array.isArray((parsed as Record<string, unknown>).events) &&
           Array.isArray((parsed as Record<string, unknown>).objects)))) {
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

export interface OcelFlattenOptions {
  object_type: string;
  event_activity_attribute?: string;
  event_timestamp_attribute?: string;
}

/** Converts an OCEL upload into a classic event log for one object type. */
export function ocelFlattenQuery(options?: OcelFlattenOptions): string {
  const objectType = options?.object_type;
  if (typeof objectType !== "string" || !objectType.trim()) {
    throw new Error("OCEL flatten object_type must be a non-empty string");
  }
  const query = new URLSearchParams();
  query.set("object_type", objectType);
  const addSelector = (key: string, value: string | undefined) => {
    if (value === undefined) return;
    if (typeof value !== "string" || !value.trim()) {
      throw new Error(`OCEL flatten ${key} must be a non-empty string`);
    }
    query.set(key, value);
  };
  addSelector("event_activity_attribute", options?.event_activity_attribute);
  addSelector("event_timestamp_attribute", options?.event_timestamp_attribute);
  return query.toString();
}

/** Converts an OCEL upload into XES for the selected object type. */
export async function requestOcelFlatten(
  log: Blob,
  options: OcelFlattenOptions,
): Promise<Blob> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const response = await importedModelFetch(`/api/ocel/flatten?${ocelFlattenQuery(options)}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  return response.blob();
}

export type OcelFilterSelector =
  | "event_attribute" | "object_attribute" | "object_types" | "objects"
  | "events" | "timestamp" | "activity_type_matching" | "activity_type_matching_exact" | "object_count"
  | "event_attribute_exact" | "object_attribute_exact" | "timestamp_exact" | "object_count_exact"
  | "start_events" | "end_events" | "activities_object_type" | "cc_object"
  | "cc_length" | "cc_object_type" | "cc_activity";
export interface OcelFilterOptions {
  variant?: OcelFilterSelector;
  attribute_key?: string;
  attribute_values?: Array<string | number | boolean | null>;
  attribute_values_json?: string;
  allowed_json?: string;
  activity_key?: string;
  object_type_key?: string;
  temporary_column?: string;
  temporary_separator?: string;
  column_kind?: "auto" | "scalar" | "timestamp";
  timestamp_key?: string;
  from_json?: string;
  to_json?: string;
  minimum_counts_json?: string;
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
  for (const key of ["attribute_values_json", "timestamp_key", "from_json", "to_json", "minimum_counts_json", "allowed_json", "activity_key", "object_type_key", "temporary_column", "temporary_separator"] as const)
    if (options[key] !== undefined) query.set(key, options[key]);
  if (options.column_kind !== undefined) {
    if (!["auto", "scalar", "timestamp"].includes(options.column_kind)) throw new Error("OCEL attribute column kind must be auto, scalar or timestamp");
    query.set("column_kind", options.column_kind);
  }
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
  const response = await importedModelFetch(`/api/ocel/filter${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const raw = typeof response.text === "function" ? await response.text() : undefined;
  const payload: unknown = raw !== undefined ? parseNativeJson(raw) : await response.json();
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
  const response = await importedModelFetch(`/api/ocel/split${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelSplitResult(payload)) throw new Error("the Minera backend returned an invalid OCEL split");
  return payload;
}

export interface OcelEnrichmentOptions {
  graphs?: OcelEnrichmentGraph[];
  lifecycle_qualifiers?: boolean;
}

export function ocelEnrichmentQuery(options: OcelEnrichmentOptions = {}): string {
  const query = new URLSearchParams();
  if (options.graphs !== undefined) {
    const graphs = [...options.graphs].sort();
    if (graphs.some((graph) => ![
      "object_interaction_graph", "object_descendants_graph",
      "object_inheritance_graph", "object_cobirth_graph",
      "object_codeath_graph",
    ].includes(graph))) throw new Error("OCEL enrichment graph is invalid");
    query.set("graphs", graphs.length ? graphs.join(",") : "none");
  }
  if (options.lifecycle_qualifiers !== undefined)
    query.set("lifecycle_qualifiers", options.lifecycle_qualifiers ? "true" : "false");
  return query.toString();
}

export async function requestOcelEnrichment(
  log: Blob,
  options: OcelEnrichmentOptions = {},
): Promise<OcelEnrichmentResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelEnrichmentQuery(options);
  const response = await importedModelFetch(`/api/ocel/enrich${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelEnrichmentResult(payload))
    throw new Error("the Minera backend returned an invalid OCEL enrichment");
  return payload;
}

export interface OcelObjectGraphOptions {
  variant?: OcelEnrichmentGraph;
}

export function ocelObjectGraphQuery(options: OcelObjectGraphOptions = {}): string {
  const query = new URLSearchParams();
  if (options.variant !== undefined) {
    if (!["object_interaction_graph", "object_descendants_graph",
      "object_inheritance_graph", "object_cobirth_graph",
      "object_codeath_graph"].includes(options.variant)) {
      throw new Error("OCEL object graph variant is invalid");
    }
    query.set("variant", options.variant);
  }
  return query.toString();
}

export async function requestOcelObjectGraph(
  log: Blob,
  options: OcelObjectGraphOptions = {},
): Promise<OcelObjectGraphResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelObjectGraphQuery(options);
  const response = await importedModelFetch(`/api/ocel/graph${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelObjectGraphResult(payload))
    throw new Error("the Minera backend returned an invalid OCEL object graph");
  return payload;
}

export interface OcelHeterogeneousGraphOptions {
  include_df?: boolean;
  include_object_changes?: boolean;
}
export type OcelEventObjectGraphOptions = OcelHeterogeneousGraphOptions;

export function ocelEventObjectGraphQuery(
  options: OcelEventObjectGraphOptions = {},
): string {
  if (options.include_df !== undefined && typeof options.include_df !== "boolean")
    throw new Error("OCEL event/object graph include_df must be boolean");
  if (options.include_object_changes !== undefined &&
      typeof options.include_object_changes !== "boolean")
    throw new Error("OCEL event/object graph include_object_changes must be boolean");
  const query = new URLSearchParams();
  query.set("variant", "heterogeneous");
  query.set("include_df", String(options.include_df ?? true));
  query.set("include_object_changes", String(options.include_object_changes ?? true));
  return query.toString();
}

export const ocelHeterogeneousGraphQuery = ocelEventObjectGraphQuery;

export async function requestOcelEventObjectGraph(
  log: Blob,
  options: OcelEventObjectGraphOptions = {},
): Promise<OcelHeterogeneousGraphResult> {
  const form = new FormData();
  const logName = typeof File !== "undefined" && log instanceof File ? log.name : "ocel.json";
  form.append("log", log, logName);
  const query = ocelEventObjectGraphQuery(options);
  const response = await importedModelFetch(`/api/ocel/event-object-graph?${query}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelHeterogeneousGraphResult(payload))
    throw new Error("the Minera backend returned an invalid OCEL event/object graph");
  return payload;
}

export const requestOcelHeterogeneousGraph = requestOcelEventObjectGraph;

export interface OcelPlayoutOptions extends OcpnDiscoveryFlags {
  model?: "ocpn" | "causal_net";
  variant?: "extensive";
  max_bindings_per_activity?: number;
  branching_factor_transitions?: number;
  branching_factor_bindings?: number;
  branching_factor_activities?: number;
  seed?: number;
  max_traces?: number;
  max_states?: number;
  objects?: Record<string, string[]>;
  initial_marking?: Record<string, number[]>;
  final_marking?: Record<string, number[]>;
  objects_unique_per_trace?: boolean;
  event_binding?: "independent" | "shared_activity" | "synchronized" | "reference";
  exists_trace?: boolean;
  return_sequences?: boolean;
  return_traces?: boolean;
  start?: number;
  case_interval?: number;
  event_interval?: number;
  ocpn_variant?: "log" | "dfg";
  ocpn_annotation?: "annotated" | "none";
  ocpn_activity_attribute?: string;
  ocpn_object_type_attribute?: string;
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
  appendOcpnDiscoveryFlags(query, options);
  if (options.model !== undefined) query.set("model", options.model);
  if (options.variant !== undefined) query.set("variant", options.variant);
  const addInteger = (name: string, value: number | undefined) => {
    if (value !== undefined && Number.isInteger(value) && value >= 0)
      query.set(name, String(value));
  };
  const addBranching = (name: string, value: number | undefined) => {
    if (value === undefined) return;
    if (!Number.isFinite(value) || value < 0)
      throw new Error(`${name} must be a finite non-negative number`);
    query.set(name, String(value));
  };
  addInteger("max_bindings_per_activity", options.max_bindings_per_activity);
  addBranching("branching_factor_transitions", options.branching_factor_transitions);
  addBranching("branching_factor_bindings", options.branching_factor_bindings);
  addBranching("branching_factor_activities", options.branching_factor_activities);
  if (options.branching_factor_activities !== undefined &&
      options.model !== "causal_net") {
    throw new Error("branching_factor_activities requires model=causal_net");
  }
  addInteger("seed", options.seed);
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
  const addMarkings = (name: string, markings: Record<string, number[]> | undefined) => {
    if (markings === undefined) return;
    const entries = Object.entries(markings)
      .sort(([left], [right]) => left < right ? -1 : left > right ? 1 : 0)
      .map(([type, values]) => {
        if (!type || !Array.isArray(values) || values.length === 0 ||
            values.some((value) => !Number.isInteger(value) || value < 0 ||
              value > 0xffffffff)) {
          throw new Error("OCEL playout markings require non-empty uint32 vectors");
        }
        return `${escapeOcelPlayoutPart(type)}:${values.join(",")}`;
      });
    if (entries.length) query.set(name, entries.join(";"));
  };
  addMarkings("initial_marking", options.initial_marking);
  addMarkings("final_marking", options.final_marking);
  if (options.objects_unique_per_trace !== undefined)
    query.set("objects_unique_per_trace", options.objects_unique_per_trace ? "true" : "false");
  if (options.event_binding !== undefined &&
      options.event_binding !== "independent" &&
      options.event_binding !== "shared_activity" &&
      options.event_binding !== "synchronized" &&
      options.event_binding !== "reference") {
    throw new Error("OCEL playout event_binding is invalid");
  }
  if (options.event_binding !== undefined)
    query.set("event_binding", options.event_binding);
  if (options.exists_trace !== undefined)
    query.set("exists_trace", options.exists_trace ? "true" : "false");
  if (options.return_sequences !== undefined)
    query.set("return_sequences", options.return_sequences ? "true" : "false");
  if (options.return_traces !== undefined)
    query.set("return_traces", options.return_traces ? "true" : "false");
  const addNumber = (name: string, value: number | undefined) => {
    if (value !== undefined && Number.isFinite(value)) query.set(name, String(value));
  };
  addNumber("start", options.start);
  addNumber("case_interval", options.case_interval);
  addNumber("event_interval", options.event_interval);
  if (options.ocpn_variant !== undefined) query.set("ocpn_variant", options.ocpn_variant);
  if (options.ocpn_annotation !== undefined) query.set("ocpn_annotation", options.ocpn_annotation);
  const addSelector = (key: string, value: string | undefined) => {
    if (value === undefined) return;
    const selector = value.trim();
    if (!selector) throw new Error(`${key} must not be empty`);
    query.set(key, selector);
  };
  addSelector("ocpn_activity_attribute", options.ocpn_activity_attribute);
  addSelector("ocpn_object_type_attribute", options.ocpn_object_type_attribute);
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
  const response = await importedModelFetch(`/api/ocel/playout${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelPlayoutResult(payload)) throw new Error("the Minera backend returned an invalid OCEL playout");
  return payload;
}

export interface OcelFeatureOptions {
  variant?: "events" | "objects" | "events_objects";
  feature_name_style?: "native" | "compatibility";
  interaction_graph_source?: OcelInteractionGraphSource;
  object_neighbor_graph?: OcelObjectNeighborGraph;
  max_features?: number;
  include_activity_features?: boolean;
  include_timestamp_features?: boolean;
  include_relation_features?: boolean;
  include_event_num_rel_objs_features?: boolean;
  include_event_num_rel_objs_type_features?: boolean;
  include_endpoint_features?: boolean;
  include_event_start_ot_features?: boolean;
  include_event_end_ot_features?: boolean;
  include_attribute_counts?: boolean;
  include_interaction_features?: boolean;
  include_new_interactions_features?: boolean;
  include_degree_centrality_features?: boolean;
  include_all_object_features?: boolean;
  include_interaction_by_type_features?: boolean;
  include_general_interaction_features?: boolean;
  include_general_descendants_features?: boolean;
  include_general_inheritance_features?: boolean;
  include_cobirth_features?: boolean;
  include_codeath_features?: boolean;
  include_related_events_features?: boolean;
  include_related_activities_features?: boolean;
  include_related_objects_features?: boolean;
  include_obj_con_in_graph_features?: boolean;
  include_prefix_features?: boolean;
  include_prefix_length_features?: boolean;
  include_prefix_timediff_features?: boolean;
  include_prefix_one_hot_features?: boolean;
  include_event_pointwise_features?: boolean;
  include_object_pointwise_features?: boolean;
  include_lifecycle_features?: boolean;
  include_lifecycle_length_features?: boolean;
  include_lifecycle_unique_activity_features?: boolean;
  include_lifecycle_duration_features?: boolean;
  include_lifecycle_activity_features?: boolean;
  include_lifecycle_path_features?: boolean;
  include_lifecycle_wip_features?: boolean;
  numeric_event_attributes?: string[];
  string_event_attributes?: string[];
  numeric_object_attributes?: string[];
  string_object_attributes?: string[];
  filter_per_type?: string;
}

export function ocelFeaturesQuery(options: OcelFeatureOptions = {}): string {
  const relatedObjectEventContext = options.include_related_objects_features === true &&
    options.variant === "events";
  const objectGraphControl = options.include_interaction_by_type_features !== undefined ||
    options.include_general_interaction_features !== undefined ||
    options.include_general_descendants_features !== undefined ||
    options.include_general_inheritance_features !== undefined ||
    options.include_cobirth_features !== undefined ||
    options.include_codeath_features !== undefined;
  if (options.include_degree_centrality_features !== undefined &&
      options.variant !== "objects" && !relatedObjectEventContext)
    throw new Error("include_degree_centrality_features applies only to objects");
  if (options.include_all_object_features !== undefined && options.variant !== "objects")
    throw new Error("include_all_object_features applies only to objects");
  if (options.feature_name_style !== undefined &&
      options.feature_name_style !== "native" &&
      options.feature_name_style !== "compatibility")
    throw new Error("feature_name_style must be native or compatibility");
  if (options.feature_name_style === "compatibility" &&
      options.variant === "events_objects")
    throw new Error("compatibility feature names apply only to events or objects");
  if (options.interaction_graph_source !== undefined &&
      options.interaction_graph_source !== "event_cooccurrence" &&
      options.interaction_graph_source !== "event_and_o2o")
    throw new Error("interaction_graph_source must be event_cooccurrence or event_and_o2o");
  if (options.interaction_graph_source !== undefined &&
      options.variant !== "objects" && !relatedObjectEventContext)
    throw new Error("interaction_graph_source applies only to objects");
  if (options.object_neighbor_graph !== undefined &&
      options.object_neighbor_graph !== "interaction" &&
      options.object_neighbor_graph !== "cobirth" &&
      options.object_neighbor_graph !== "codeath")
    throw new Error("object_neighbor_graph must be interaction, cobirth, or codeath");
  if (options.object_neighbor_graph !== undefined &&
      options.include_obj_con_in_graph_features !== true)
    throw new Error("object_neighbor_graph requires include_obj_con_in_graph_features=true");
  if (options.object_neighbor_graph !== undefined &&
      options.variant !== "objects" && !relatedObjectEventContext)
    throw new Error("object_neighbor_graph applies only to objects");
  if (objectGraphControl && options.variant !== "objects" &&
      !relatedObjectEventContext)
    throw new Error("object graph features apply only to objects");
  const objectOnlyFeatureControl = options.include_related_events_features !== undefined ||
    options.include_related_activities_features !== undefined ||
    options.include_obj_con_in_graph_features !== undefined ||
    options.include_lifecycle_features !== undefined ||
    options.include_lifecycle_length_features !== undefined ||
    options.include_lifecycle_unique_activity_features !== undefined ||
    options.include_lifecycle_duration_features !== undefined ||
    options.include_lifecycle_activity_features !== undefined ||
    options.include_lifecycle_wip_features !== undefined ||
    options.include_lifecycle_path_features !== undefined;
  if (options.include_related_objects_features !== undefined && options.variant !== "events")
    throw new Error("include_related_objects_features applies only to events");
  const eventControl = options.include_event_num_rel_objs_features !== undefined ||
    options.include_event_num_rel_objs_type_features !== undefined ||
    options.include_event_start_ot_features !== undefined ||
    options.include_event_end_ot_features !== undefined ||
    options.include_new_interactions_features !== undefined;
  if (eventControl && options.variant !== "events")
    throw new Error("independent event feature controls apply only to events");
  const prefixControl = options.include_prefix_features !== undefined ||
    options.include_prefix_length_features !== undefined ||
    options.include_prefix_timediff_features !== undefined ||
    options.include_prefix_one_hot_features !== undefined;
  if (prefixControl && options.variant !== "events_objects")
    throw new Error("prefix features apply only to events_objects");
  const pointwiseControl = options.include_event_pointwise_features !== undefined ||
    options.include_object_pointwise_features !== undefined;
  if (pointwiseControl && options.variant !== "events_objects")
    throw new Error("pointwise features apply only to events_objects");
  if (objectOnlyFeatureControl && options.variant !== "objects" &&
      !relatedObjectEventContext)
    throw new Error("related-event, neighbor, and lifecycle-path features apply only to objects");
  const query = new URLSearchParams();
  if (options.variant !== undefined) query.set("variant", options.variant);
  if (options.feature_name_style !== undefined)
    query.set("feature_name_style", options.feature_name_style);
  if (options.interaction_graph_source !== undefined)
    query.set("interaction_graph_source", options.interaction_graph_source);
  if (options.object_neighbor_graph !== undefined)
    query.set("object_neighbor_graph", options.object_neighbor_graph);
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
  addBoolean("include_event_num_rel_objs_features", options.include_event_num_rel_objs_features);
  addBoolean("include_event_num_rel_objs_type_features", options.include_event_num_rel_objs_type_features);
  addBoolean("include_endpoint_features", options.include_endpoint_features);
  addBoolean("include_event_start_ot_features", options.include_event_start_ot_features);
  addBoolean("include_event_end_ot_features", options.include_event_end_ot_features);
  addBoolean("include_attribute_counts", options.include_attribute_counts);
  addBoolean("include_interaction_features", options.include_interaction_features);
  addBoolean("include_new_interactions_features", options.include_new_interactions_features);
  addBoolean("include_degree_centrality_features", options.include_degree_centrality_features);
  addBoolean("include_all_object_features", options.include_all_object_features);
  addBoolean("include_interaction_by_type_features", options.include_interaction_by_type_features);
  addBoolean("include_general_interaction_features", options.include_general_interaction_features);
  addBoolean("include_general_descendants_features", options.include_general_descendants_features);
  addBoolean("include_general_inheritance_features", options.include_general_inheritance_features);
  addBoolean("include_cobirth_features", options.include_cobirth_features);
  addBoolean("include_codeath_features", options.include_codeath_features);
  addBoolean("include_related_events_features", options.include_related_events_features);
  addBoolean("include_related_activities_features", options.include_related_activities_features);
  addBoolean("include_related_objects_features", options.include_related_objects_features);
  addBoolean("include_obj_con_in_graph_features", options.include_obj_con_in_graph_features);
  addBoolean("include_prefix_features", options.include_prefix_features);
  addBoolean("include_prefix_length_features", options.include_prefix_length_features);
  addBoolean("include_prefix_timediff_features", options.include_prefix_timediff_features);
  addBoolean("include_prefix_one_hot_features", options.include_prefix_one_hot_features);
  addBoolean("include_event_pointwise_features", options.include_event_pointwise_features);
  addBoolean("include_object_pointwise_features", options.include_object_pointwise_features);
  addBoolean("include_lifecycle_features", options.include_lifecycle_features);
  addBoolean("include_lifecycle_length_features", options.include_lifecycle_length_features);
  addBoolean("include_lifecycle_unique_activity_features", options.include_lifecycle_unique_activity_features);
  addBoolean("include_lifecycle_duration_features", options.include_lifecycle_duration_features);
  addBoolean("include_lifecycle_activity_features", options.include_lifecycle_activity_features);
  addBoolean("include_lifecycle_path_features", options.include_lifecycle_path_features);
  addBoolean("include_lifecycle_wip_features", options.include_lifecycle_wip_features);
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
  if (options.filter_per_type !== undefined)
    query.set("ocel_features_filter_per_type", options.filter_per_type);
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
  const response = await importedModelFetch(`/api/ocel/features${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/ocel/statistics${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
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
  const response = await importedModelFetch(`/api/ocel/description${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  if (!isOcelDescriptionResult(payload)) throw new Error("the Minera backend returned an invalid OCEL description result");
  return payload;
}

export interface OcelConformanceOptions {
  variant?: OcelConformanceVariant;
  /** Kind of the real-side input; omitted to preserve the OCEL upload default. */
  real?: "ocel" | "graph";
  /** Graph representation used by the real-side input when `real` is `graph`. */
  real_graph_kind?: OcelConformanceVariant;
  alpha?: number;
  beta?: number;
  gamma?: number;
  delta?: number;
  etot_theta?: number;
  otg_theta?: number;
  /** Exact arbitrary relation map; omission and explicit [] are distinct. OTG only. */
  otg_theta_by_relation_json?: string;
  otg_theta_interaction?: number;
  otg_theta_descendants?: number;
  otg_theta_inheritance?: number;
  otg_theta_cobirth?: number;
  otg_theta_codeath?: number;
  ocdfg_theta_activity?: number;
  ocdfg_theta_flow?: number;
  ocdfg_activity_attribute?: string;
  ocdfg_object_type_attribute?: string;
}

export function ocelConformanceQuery(options: OcelConformanceOptions = {}): string {
  const exactThresholds = validateOtgThresholdOptions(options);
  const query = new URLSearchParams();
  if (options.real !== undefined && options.real !== "ocel" && options.real !== "graph") {
    throw new Error("real must be ocel or graph");
  }
  if (options.real_graph_kind !== undefined &&
      !["etot", "ocdfg", "otg"].includes(options.real_graph_kind)) {
    throw new Error("real_graph_kind must be etot, ocdfg, or otg");
  }
  if (options.real !== "graph" && options.real_graph_kind !== undefined) {
    throw new Error("real_graph_kind applies only when real is graph");
  }
  if (options.real === "graph" && options.real_graph_kind !== undefined &&
      options.variant !== undefined &&
      options.real_graph_kind !== options.variant) {
    throw new Error("real_graph_kind must match variant");
  }
  const add = (name: string, value: number | string | undefined) => {
    if (value !== undefined) query.set(name, String(value));
  };
  const addSelector = (name: string, value: string | undefined) => {
    if (value === undefined) return;
    const selector = value.trim();
    if (!selector) throw new Error(`${name} must not be empty`);
    query.set(name, selector);
  };
  add("variant", options.variant);
  add("real", options.real);
  add("real_graph_kind", options.real_graph_kind);
  add("alpha", options.alpha);
  add("beta", options.beta);
  add("gamma", options.gamma);
  add("delta", options.delta);
  add("etot_theta", options.etot_theta);
  add("otg_theta", options.otg_theta);
  if (exactThresholds !== undefined) query.set(OTG_THRESHOLD_FIELD, otgThresholdQueryText(exactThresholds));
  const addRelationThreshold = (name: string, value: number | undefined) => {
    if (value === undefined) return;
    if (!Number.isFinite(value) || value < 0)
      throw new Error(`${name} must be a finite non-negative number`);
    query.set(name, String(value));
  };
  addRelationThreshold("otg_theta_interaction", options.otg_theta_interaction);
  addRelationThreshold("otg_theta_descendants", options.otg_theta_descendants);
  addRelationThreshold("otg_theta_inheritance", options.otg_theta_inheritance);
  addRelationThreshold("otg_theta_cobirth", options.otg_theta_cobirth);
  addRelationThreshold("otg_theta_codeath", options.otg_theta_codeath);
  add("ocdfg_theta_activity", options.ocdfg_theta_activity);
  add("ocdfg_theta_flow", options.ocdfg_theta_flow);
  addSelector("ocdfg_activity_attribute", options.ocdfg_activity_attribute);
  addSelector("ocdfg_object_type_attribute", options.ocdfg_object_type_attribute);
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
  const realField = options.real === "graph" ? "real_graph" : "log";
  form.append(realField, log, logName);
  form.append("normative", normative, normativeName);
  const parameters = new URLSearchParams(ocelConformanceQuery(options));
  const exactThresholds = validateOtgThresholdOptions(options);
  if (exactThresholds !== undefined) {parameters.delete(OTG_THRESHOLD_FIELD);appendOtgThresholds(form, exactThresholds);}
  const query = parameters.toString();
  const response = await importedModelFetch(`/api/ocel/conformance${query ? `?${query}` : ""}`, {
    method: "POST",
    body: form,
  });
  await throwIfBackendError(response);
  const raw = exactThresholds !== undefined ? await response.text() : undefined;
  const payload: unknown = raw !== undefined ? parseNativeJson(raw) : await response.json();
  if (!isOcelConformanceResult(payload)) throw new Error("the Minera backend returned an invalid OCEL conformance result");
  if (raw !== undefined && exactThresholds !== undefined) {requireExactOtgEnvelope(payload);await bindOtgConformanceMetadata(payload, raw, query, exactThresholds, log, normative);}
  return payload;
}

export function knowledgeInjectionQuery(options: KnowledgeRequest = {}): string {
  if (hasExactKnowledge(options)) return exactKnowledgeQuery(options);
  const query = new URLSearchParams();
  const variant = options.knowledge_variant;
  if (variant !== undefined && !["description", "process_mining", "database"].includes(variant))
    throw new Error("knowledge_variant is not supported");
  const positive = (name: string, value: number | undefined) => {
    if (value === undefined) return;
    if (!Number.isSafeInteger(value) || value <= 0) throw new Error(`${name} must be a positive integer`);
    query.set(name, String(value));
  };
  if (variant !== undefined) query.set("knowledge_variant", variant);
  if (options.knowledge_description !== undefined) query.set("knowledge_description", options.knowledge_description);
  if (options.knowledge_query !== undefined) query.set("knowledge_query", options.knowledge_query);
  if (options.knowledge_database_schema !== undefined) query.set("knowledge_database_schema", options.knowledge_database_schema);
  positive("knowledge_max_input_bytes", options.knowledge_max_input_bytes as number | undefined);
  positive("knowledge_max_prompt_bytes", options.knowledge_max_prompt_bytes as number | undefined);
  return query.toString();
}

export function requestKnowledgeInjection(options: ExactKnowledgeRequest): Promise<ExactKnowledgeEnvelope>;
export function requestKnowledgeInjection(options?: import("./types").KnowledgeInjectionOptions): Promise<import("./types").KnowledgeInjectionResult>;
export async function requestKnowledgeInjection(
  options: KnowledgeRequest = {},
): Promise<import("./types").KnowledgeInjectionResult | ExactKnowledgeEnvelope> {
  if (hasExactKnowledge(options)) return requestExactKnowledge(options);
  const query = knowledgeInjectionQuery(options);
  const response = await importedModelFetch(`/api/knowledge-injection${query ? `?${query}` : ""}`, { method: "POST" });
  await throwIfBackendError(response);
  const payload: unknown = await response.json();
  const { isKnowledgeInjectionResult } = await import("./types");
  if (!isKnowledgeInjectionResult(payload)) throw new Error("the Minera backend returned an invalid knowledge injection result");
  return payload;
}
