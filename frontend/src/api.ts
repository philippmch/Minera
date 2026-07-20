import {
  type ActivityStat,
  type Alignments,
  DECLARE_TEMPLATES,
  FOOTPRINTS_CONFORMANCE_VARIANTS,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
  FOOTPRINTS_DISCOVERY_VARIANTS,
  type Decisions,
  type MineraExport,
  type ModelConformance,
  isMineraExport,
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

/** Log-preprocessing filters applied by the backend before discovery. */
export interface FilterOptions {
  topVariants?: number;
  minVariantFrequency?: number;
  /** Cumulative variant coverage in (0, 1]: keep the most frequent variants
   * until together they cover this share of cases. */
  variantCoverage?: number;
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
  /** Keep only cases where this activity occurs at least reworkMin times. */
  reworkActivity?: string;
  reworkMin?: number;
  /** Succession filters, each entry an "a -> b" pair: keep (or drop) cases
   * where the first activity is directly / eventually followed by the second. */
  keepDirectlyFollows?: string[];
  removeDirectlyFollows?: string[];
  keepEventuallyFollows?: string[];
  removeEventuallyFollows?: string[];
  /** Reduce each case to its head up to the first prefixActivity / its tail
   * from the first suffixActivity (anchor included); cases without it drop. */
  prefixActivity?: string;
  suffixActivity?: string;
  /** Cut each case into the subtraces from betweenStart to betweenEnd. */
  betweenStart?: string;
  betweenEnd?: string;
  minLength?: number;
  maxLength?: number;
  /** ISO-8601 bounds, both inclusive; either side may be open. */
  timeFrom?: string;
  timeTo?: string;
  timeMode?: TimeFilterMode;
  /** Generic event-attribute filter: the key, its value constraints, and
   * whether it filters events or whole cases. */
  attributeKey?: string;
  keepAttributeValues?: string[];
  removeAttributeValues?: string[];
  attributeMode?: AttributeFilterMode;
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
  /** "log" (default) splits sublogs; "dfg" recurses on the graph (IMd). */
  inductiveVariant?: "log" | "dfg";
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
  /** PM4Py view-based transition-system options. */
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
  /** PM4Py-compatible token-replay variation used for model conformance. */
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
  /** Aggregate PM4Py footprints discovery/conformance settings. */
  footprintsDiscoveryVariants?: string[];
  footprintsModel?: "process_tree" | "powl" | "inductive" | "alpha" | "heuristic" | "dfg";
  footprintsConformanceVariants?: string[];
  footprintsStrict?: boolean;
  footprintsEnableActAlwaysExecuted?: boolean;
  /** Optional PM4Py statistical and causal DFG analyses. */
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
  /** Optional PM4Py correlation-mining analyses. */
  correlationVariants?: string[];
  correlationStartTimestampAttribute?: string;
  correlationExactTimeMatching?: boolean;
  correlationSampleSize?: number;
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

  if (options.caseColumn) add("case_column", options.caseColumn);
  if (options.activityColumn) add("activity_column", options.activityColumn);
  if (options.timestampColumn) add("timestamp_column", options.timestampColumn);
  if (options.delimiter) add("delimiter", options.delimiter);

  if (options.topVariants) add("top_variants", String(options.topVariants));
  if (options.minVariantFrequency)
    add("min_variant_frequency", String(options.minVariantFrequency));
  if (options.variantCoverage)
    add("variant_coverage", String(options.variantCoverage));
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
  if (options.startActivities?.length)
    add("start_activities", options.startActivities.join(","));
  if (options.endActivities?.length)
    add("end_activities", options.endActivities.join(","));
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
  if (options.prefixActivity) add("prefix_activity", options.prefixActivity);
  if (options.suffixActivity) add("suffix_activity", options.suffixActivity);
  if (options.betweenStart && options.betweenEnd) {
    add("between_start", options.betweenStart);
    add("between_end", options.betweenEnd);
  }
  if (options.minLength) add("min_length", String(options.minLength));
  if (options.maxLength) add("max_length", String(options.maxLength));
  if (options.timeFrom) add("time_from", options.timeFrom);
  if (options.timeTo) add("time_to", options.timeTo);
  if (options.timeMode && (options.timeFrom || options.timeTo))
    add("time_mode", options.timeMode);
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
export async function alignModel(miner: string): Promise<Alignments> {
  let response: Response;
  try {
    response = await fetch(`/api/align?miner=${encodeURIComponent(miner)}`, {
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

/**
 * Asks the backend to mine the decision points of one miner's model: which
 * attribute values steer a case down which branch. Like alignments this is
 * deferred to an on-demand request, so a mine never pays for decisions the
 * user may not open.
 */
export async function decideModel(miner: string): Promise<Decisions> {
  let response: Response;
  try {
    response = await fetch(`/api/decide?miner=${encodeURIComponent(miner)}`, {
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
 * Fetches the most recently mined inductive process tree as PTML — the only
 * miner producing a block-structured tree, so there is no miner parameter.
 */
export async function fetchPtml(): Promise<string> {
  let response: Response;
  try {
    response = await fetch("/api/ptml");
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

/**
 * Simulates one miner's model into a synthetic event log served as standard
 * XES. The backend defaults to 100 traces for Petri models and 1,000 for
 * Declare. Declare classic playout is prefix-safe, so a generated trace may
 * end with pending obligations rather than being a complete fitting execution.
 * Like the other model endpoints this only works for a log mined this session.
 */
export async function fetchPlayout(miner: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(`/api/playout?miner=${encodeURIComponent(miner)}`);
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
