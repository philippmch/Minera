export interface ActivityStat {
  name: string;
  count: number;
  start_count: number;
  end_count: number;
  /**
   * In how many cases this activity occurs more than once (rework). Absent
   * in exports from older minera versions.
   */
  rework_cases?: number;
}

export interface EdgeStat {
  source: number;
  target: number;
  count: number;
}

export type TransitionSystemView = "sequence" | "set" | "multiset";
export type TransitionSystemDirection = "forward" | "backward";
export interface TransitionOccurrence {
  trace_index: number;
  event_index: number;
}
export interface TransitionSystemState {
  id: number;
  /** Sequence/set payloads are activity ids; multiset uses activity/count pairs. */
  payload: number[] | Array<{ activity: number; count: number }>;
  start_count: number;
  end_count: number;
  incoming_occurrences?: TransitionOccurrence[];
  outgoing_occurrences?: TransitionOccurrence[];
}
export interface TransitionSystemTransition {
  source: number;
  target: number;
  activity: number;
  count: number;
  occurrences?: TransitionOccurrence[];
}
export interface TransitionSystemExport {
  view: TransitionSystemView;
  direction: TransitionSystemDirection;
  window: number;
  include_data: boolean;
  states: TransitionSystemState[];
  transitions: TransitionSystemTransition[];
}

/** Timing statistics for an ordered activity pair in the temporal profile. */
export interface TemporalProfileEntry {
  /** Source activity id; indexes into MineraExport.activities. */
  source: number;
  /** Target activity id; indexes into MineraExport.activities. */
  target: number;
  count: number;
  mean_seconds: number;
  standard_deviation_seconds: number;
}

export interface MinimumSelfDistanceEntry {
  activity: number;
  distance: number;
  witnesses: number[];
}

export interface VariantStat {
  activities: number[];
  count: number;
  /**
   * Case throughput-time statistics for this variant, in seconds. Absent in
   * exports from older minera versions and on logs with no parseable
   * timestamps; `duration.count === 0` likewise means no timing was observed.
   */
  duration?: DurationStats;
}

export type ProcessTreeOperator =
  | "sequence"
  | "exclusive"
  | "parallel"
  | "loop"
  | "activity"
  | "tau";

export interface ProcessTreeNode {
  operator: ProcessTreeOperator;
  /** Present iff operator === "activity"; indexes into activities. */
  activity?: number;
  children?: ProcessTreeNode[];
}

export interface PetriTransition {
  /** Activity id this transition fires; null for a silent (tau) transition. */
  activity: number | null;
  /** Place ids consumed when the transition fires. */
  inputs: number[];
  /** Place ids produced when the transition fires. */
  outputs: number[];
}

export interface PetriNetModel {
  /** Places are ids 0..place_count-1. */
  place_count: number;
  source: number;
  sink: number;
  transitions: PetriTransition[];
}

/** Recursive POWL model emitted by the POWL discovery miner. */
export type PowlNode =
  | { kind: "transition"; activity: number; frequency: { minimum: number; maximum: number | null } }
  | { kind: "silent" }
  | { kind: "xor" | "loop"; children: PowlNode[] }
  | { kind: "strict_partial_order"; children: PowlNode[]; order: Array<{ source: number; target: number }> };

/** One ordered relation in a classic log-skeleton model. */
export interface LogSkeletonRelation {
  source: number;
  target: number;
}

/** The six constraint families emitted by log-skeleton discovery. */
export interface LogSkeletonModel {
  equivalence: LogSkeletonRelation[];
  always_after: LogSkeletonRelation[];
  always_before: LogSkeletonRelation[];
  never_together: LogSkeletonRelation[];
  directly_follows: LogSkeletonRelation[];
  /** Allowed occurrence counts per activity id (as JSON object keys). */
  activ_freq: Record<string, number[]>;
}

export interface LogSkeletonPairDeviation {
  constraint: string;
  source: number;
  target: number;
}

export interface LogSkeletonFrequencyDeviation {
  constraint: string;
  activity: number;
  observed_frequency: number;
  allowed_frequencies: number[];
}

export type LogSkeletonDeviation =
  | LogSkeletonPairDeviation
  | LogSkeletonFrequencyDeviation;

/** Per-trace conformance result for a log-skeleton model. */
export interface LogSkeletonConformanceTrace {
  case_id: string;
  deviations: LogSkeletonDeviation[];
  no_dev_total: number;
  no_constr_total: number;
  dev_fitness: number;
  is_fit: boolean;
}

export const DECLARE_TEMPLATES = [
  "existence", "exactly_one", "init", "responded_existence", "response",
  "precedence", "succession", "altresponse", "altprecedence", "altsuccession",
  "chainresponse", "chainprecedence", "chainsuccession", "absence", "coexistence",
  "noncoexistence", "nonsuccession", "nonchainsuccession",
] as const;
export type DeclareTemplate = (typeof DECLARE_TEMPLATES)[number];
export interface DeclareUnaryRule {
  activity: number;
  support: number;
  confidence: number;
}
export interface DeclareBinaryRule {
  source: number;
  target: number;
  support: number;
  confidence: number;
}
export type DeclareRule = DeclareUnaryRule | DeclareBinaryRule;
export type DeclareModel = Partial<Record<DeclareTemplate, DeclareRule[]>>;
export type DeclareDeviation =
  | { template: DeclareTemplate | string; activity: number }
  | { template: DeclareTemplate | string; source: number; target: number };
export interface DeclareConformanceTrace {
  trace_index?: number;
  case_id: string;
  deviations: DeclareDeviation[];
  no_constr_total: number;
  no_dev_total: number;
  dev_fitness: number;
  is_fit: boolean;
}

/** One place's replay decoration: tokens through it, and how long they rest. */
export interface PetriPlaceDecoration {
  /** Tokens consumed from this place across the cleanly replaying cases. */
  count: number;
  /** Waiting time between a token's production and its consumption. */
  wait: DurationStats;
}

/**
 * Frequency/performance annotations for the Petri net, measured by token
 * replay of the log's cleanly fitting cases; indices match `petri_net`.
 */
export interface PetriDecoration {
  /** Cases whose replay was clean — the ones the annotations describe. */
  decorated_traces: number;
  trace_count: number;
  /** Firing count per transition, aligned with petri_net.transitions. */
  transitions: number[];
  /** Per place id: token count and waiting time. */
  places: PetriPlaceDecoration[];
}

export interface HeuristicEdge {
  source: number;
  target: number;
  /** Dependency measure in (-1, 1]: how reliably source is followed by target. */
  dependency: number;
  /** Direct successions observed (|source > target|). */
  frequency: number;
  /** Frequency count or mean wait, according to the selected decoration. */
  representation?: number;
  /** True when classic discovery rescued this relation as a length-two loop. */
  length_two_loop?: boolean;
}

export interface HeuristicsNet {
  variant?: "classic" | "plusplus";
  decoration?: "frequency" | "performance";
  performance_aggregation?:
    | "mean"
    | "median"
    | "min"
    | "max"
    | "stdev"
    | "sum"
    | "raw_values"
    | "all";
  edges: HeuristicEdge[];
  start_activities: number[];
  end_activities: number[];
  active_activities?: number[];
  activity_statistics?: {
    activity: number;
    occurrences: number;
    start_occurrences: number;
    end_occurrences: number;
    mean_sojourn_seconds: number;
  }[];
  relation_statistics?: {
    source: number;
    target: number;
    eventually_follows_frequency: number;
    direct_adjacency_frequency: number;
    mean_wait_seconds: number;
    raw_frequency?: number;
    cleaned_frequency?: number;
    representation?: number;
    length_two_loop?: boolean;
    wait_samples?: number[];
    median_wait_seconds?: number;
    min_wait_seconds?: number;
    max_wait_seconds?: number;
    stdev_wait_seconds?: number;
    sum_wait_seconds?: number;
  }[];
  concurrency_statistics?: {
    first: number;
    second: number;
    frequency: number;
  }[];
  raw_activity_statistics?: {
    activity: number;
    occurrences: number;
    mean_sojourn_seconds: number;
  }[];
  raw_concurrency_statistics?: {
    first: number;
    second: number;
    frequency: number;
  }[];
  raw_dfg?: { source: number; target: number; count: number }[];
  cleaned_dfg?: { source: number; target: number; count: number }[];
  window_two_dfg?: { source: number; target: number; count: number }[];
  frequency_triples?: {
    first: number;
    middle: number;
    last: number;
    count: number;
  }[];
  output_bundles?: { activity: number; bundles: number[][] }[];
  input_bundles?: { activity: number; bundles: number[][] }[];
}

export interface VariantConformance {
  activities: number[];
  count: number;
  fitness: number;
}

export interface Conformance {
  /** Token-replay fitness in [0, 1]: does the model reproduce the log? */
  fitness: number;
  /** Escaping-edges precision in [0, 1]: how much extra behavior the model allows. */
  precision: number;
  /** Token-replay generalization in [0, 1]: how evenly the log exercises the model. Absent in older exports. */
  generalization?: number;
  /** Inverse arc-degree simplicity in (0, 1]: how lean the net itself is. Absent in older exports. */
  simplicity?: number;
  fitting_traces: number;
  trace_count: number;
  produced: number;
  consumed: number;
  missing: number;
  remaining: number;
  /** Variants worst-fitting first. */
  variants: VariantConformance[];
}

/** One move of an optimal alignment between a trace and the model. */
export type AlignmentMoveType = "sync" | "log" | "model";

export interface AlignmentMove {
  type: AlignmentMoveType;
  /** Activity id of the move; indexes into activities. */
  activity: number;
}

export interface VariantAlignment {
  activities: number[];
  count: number;
  /** Optimal alignment, in execution order. */
  moves: AlignmentMove[];
  /** Total cost: one per log-only or visible model-only move. */
  cost: number;
  /** 1 - cost / worst-case cost, in [0, 1]. */
  fitness: number;
}

export interface Alignments {
  /**
   * False when the model could not be aligned; the other fields are then
   * meaningless and variants empty. `exhausted` tells the two causes apart.
   */
  computed: boolean;
  /** When !computed: true = search bound hit (reachable but too large), false = unsound. */
  exhausted: boolean;
  /** Cost-based fitness in [0, 1]: how cheaply the log aligns to the model. */
  fitness: number;
  /**
   * Alignment-based escaping-edges precision in [0, 1]: how much extra
   * behavior the model allows at the states the optimal alignments visit.
   * Absent in exports from older minera versions.
   */
  precision?: number;
  fitting_traces: number;
  trace_count: number;
  /** Summed optimal alignment cost over all traces. */
  total_cost: number;
  /** Variants worst-fitting first. */
  variants: VariantAlignment[];
  /**
   * Per-activity move tallies across the log, most deviating first. Absent
   * in exports from older minera versions.
   */
  activity_deviations?: ActivityDeviation[];
}

/** One directly-follows succession (b follows a); indexes into activities. */
export interface FootprintRelation {
  source: number;
  target: number;
}

/** Footprint (directly-follows relation) comparison of the log against a model. */
export interface Footprints {
  /** Share of the log's successions the model also allows, in [0, 1]. */
  fitness: number;
  /** Share of the model's successions the log also shows, in [0, 1]. */
  precision: number;
  log_relations: number;
  model_relations: number;
  shared_relations: number;
  /** True when model exploration hit its bound and the relation is partial. */
  exhausted: boolean;
  /** Successions in the log the model forbids (fitness gaps). */
  log_only: FootprintRelation[];
  /** Successions the model allows the log never shows (precision gaps). */
  model_only: FootprintRelation[];
}

export const FOOTPRINTS_DISCOVERY_VARIANTS = [
  "entire_event_log", "trace_by_trace", "dfg", "petri_reach_graph",
  "process_tree", "powl",
] as const;
export type FootprintsDiscoveryVariant = (typeof FOOTPRINTS_DISCOVERY_VARIANTS)[number];
export const FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS: FootprintsDiscoveryVariant[] = [
  "entire_event_log", "dfg", "process_tree", "powl",
];
export type FootprintsModelVariant = "process_tree" | "powl" | "inductive" | "alpha" | "heuristic" | "dfg";
export const FOOTPRINTS_CONFORMANCE_VARIANTS = [
  "log_model", "log_extensive", "trace_extensive",
] as const;
export type FootprintsConformanceVariant = (typeof FOOTPRINTS_CONFORMANCE_VARIANTS)[number];

/** A PM4Py aggregate footprint record; optional keys are variant-dependent. */
export interface AggregateFootprint {
  sequence: FootprintRelation[];
  parallel: FootprintRelation[];
  dfg?: EdgeStat[];
  start_activities?: number[];
  end_activities?: number[];
  activities?: number[];
  skippable?: boolean;
  activities_always_happening?: number[];
  min_trace_length?: number;
  max_trace_length_without_loops?: number;
  trace?: number[];
}
export interface FootprintsLogModelConformance {
  strict: boolean;
  violations: FootprintRelation[];
  is_fit: boolean;
}
export interface FootprintsExtensiveConformance {
  footprints: FootprintRelation[];
  start_activities: number[];
  end_activities: number[];
  activities_always_happening: number[];
  min_length_fit: boolean;
  is_footprints_fit: boolean;
  is_fit: boolean;
}
export interface FootprintsTraceExtensiveConformance extends FootprintsExtensiveConformance {
  trace_index: number;
  case_id: string;
}
export interface FootprintsTraceDiscovery {
  trace_index: number;
  case_id: string;
  footprint: AggregateFootprint;
}
export interface AggregateFootprintsDiscovery {
  entire_event_log?: AggregateFootprint;
  trace_by_trace?: FootprintsTraceDiscovery[];
  dfg?: AggregateFootprint;
  petri_reach_graph?: AggregateFootprint;
  process_tree?: AggregateFootprint;
  powl?: AggregateFootprint;
}
export interface AggregateFootprintsConformance {
  model: FootprintsModelVariant;
  log_model?: FootprintsLogModelConformance;
  log_extensive?: FootprintsExtensiveConformance;
  trace_extensive?: FootprintsTraceExtensiveConformance[];
}
export interface AggregateFootprintsMetrics {
  precision?: number;
  log_fitness?: number;
  perc_fit_traces?: number;
}
export interface AggregateFootprints {
  discovery?: AggregateFootprintsDiscovery;
  conformance?: AggregateFootprintsConformance;
  metrics?: AggregateFootprintsMetrics;
}

export type DfgStatisticalVariant =
  | "native"
  | "frequency"
  | "frequency_greedy"
  | "frequency_triples"
  | "performance"
  | "performance_greedy"
  | "case_attributes"
  | "clean"
  | "clean_polars"
  | "clean_time";
export type DfgCausalVariant = "alpha" | "heuristic";

export interface DfgPerformanceSummary {
  count: number;
  mean: number;
  median: number;
  min: number;
  max: number;
  stdev: number;
  sum: number;
}
export interface DfgPerformanceEdge {
  source: number;
  target: number;
  summary: DfgPerformanceSummary;
  raw_values: number[];
  value: number | number[] | DfgPerformanceSummary;
}
export interface DfgAttributeValueCount {
  value: string;
  count: number;
}
export interface DfgNamedAttributeCounts {
  attribute: string;
  values: DfgAttributeValueCount[];
}
export interface DfgCaseAttributeEdge {
  source: number;
  target: number;
  attributes: DfgNamedAttributeCounts[];
}
export interface DfgCaseAttributeNode {
  activity: number;
  attributes: DfgNamedAttributeCounts[];
}
export interface DfgBoundaryActivity {
  activity: number;
  count: number;
}
export interface DfgStatisticalAnalysis {
  variant: DfgStatisticalVariant;
  frequency_dfg?: EdgeStat[];
  frequency_triples?: Array<{
    first: number;
    middle: number;
    last: number;
    count: number;
  }>;
  performance_dfg?: DfgPerformanceEdge[];
  case_attributes?: {
    edges: DfgCaseAttributeEdge[];
    nodes: DfgCaseAttributeNode[];
  };
  clean?: {
    dfg: EdgeStat[];
    start_activities: DfgBoundaryActivity[];
    end_activities: DfgBoundaryActivity[];
  };
  clean_time?: Array<{
    activity: number;
    mean_seconds_from_case_start: number;
  }>;
}
export interface DfgCausalAnalysis {
  variant: DfgCausalVariant;
  relations: Array<{ source: number; target: number; value: number }>;
}
export interface DfgVariantsAnalysis {
  statistical: DfgStatisticalAnalysis[];
  causal: DfgCausalAnalysis[];
}

/** Conformance results for one discovered model. */
export interface ModelConformance {
  /** Whether the model's final marking is reachable — the bar for alignment. */
  sound: boolean;
  token_replay: Conformance;
  /** Footprint comparison; absent in older exports. */
  footprints?: Footprints;
  /** Present in CLI exports; the server omits it and serves it from /api/align. */
  alignments?: Alignments;
}

/** Per-miner conformance, keyed by miner id ("inductive", "alpha", "heuristic"). */
export type ConformanceModels = Record<string, ModelConformance>;

/** Summary statistics over a set of durations, in seconds. */
export interface DurationStats {
  count: number;
  total: number;
  mean: number;
  median: number;
  min: number;
  max: number;
}

/** Waiting-time statistics for one directly-follows edge. */
export interface EdgePerformance {
  source: number;
  target: number;
  stats: DurationStats;
}

/** Service-time statistics for one activity (lifecycle start/complete pairs). */
export interface ActivityPerformance {
  /** Index into MineraExport.activities. */
  activity: number;
  stats: DurationStats;
}

/** The time perspective: per-edge waiting times and case throughput. */
export interface Performance {
  /** Traces with at least two parseable timestamps. */
  timed_traces: number;
  /** First-to-last event duration across timed traces. */
  case_duration: DurationStats;
  /**
   * Gaps between consecutive case start instants — the case arrival rate.
   * Absent in exports from older minera versions.
   */
  case_arrival?: DurationStats;
  /** Edge timings, in the same order as MineraExport.edges. */
  edges: EdgePerformance[];
  /**
   * Activity service times from lifecycle start/complete pairs; sparse (only
   * paired activities appear) and absent in exports from older versions.
   */
  activities?: ActivityPerformance[];
}

export interface ResourceStat {
  name: string;
  event_count: number;
}

/** A handover of work from one resource to another (indexes into resources). */
export interface ResourceHandover {
  source: number;
  target: number;
  count: number;
}

/** The organizational perspective: who works, and how work is handed over. */
export interface ResourceNetwork {
  total_handovers: number;
  resources: ResourceStat[];
  handovers: ResourceHandover[];
}

export interface TraceEvent {
  activity: number;
  timestamp: string;
  /** Performing resource; present only when the log carries one. */
  resource?: string;
  /** Lifecycle transition ("start", "complete", …); present when recorded. */
  lifecycle?: string;
  /** Generic event attributes (extra CSV columns, non-standard XES keys). */
  attributes?: Record<string, string>;
}

/** One observed value of a generic event attribute. */
export interface AttributeValueStat {
  value: string;
  count: number;
}

/** Per-key summary of a generic event attribute across the log. */
export interface EventAttributeStat {
  key: string;
  /** Events carrying the key. */
  events: number;
  /** Total distinct values; `values` lists only the most frequent ones. */
  distinct_values: number;
  values: AttributeValueStat[];
}

/** Per-key summary of a case attribute across the log. */
export interface CaseAttributeStat {
  key: string;
  /** Cases carrying the key. */
  cases: number;
  /** Total distinct values; `values` lists only the most frequent ones. */
  distinct_values: number;
  values: AttributeValueStat[];
}

export interface Trace {
  case_id: string;
  events: TraceEvent[];
  /** Case attributes (XES trace attributes, `case:` CSV columns). */
  attributes?: Record<string, string>;
}

/**
 * Per-activity tally of alignment moves across the whole log: how often the
 * activity aligned synchronously, was inserted (log move), or skipped (model
 * move), weighted by trace counts. Most deviating first.
 */
export interface ActivityDeviation {
  /** Index into MineraExport.activities. */
  activity: number;
  synchronous: number;
  log_moves: number;
  model_moves: number;
}

/**
 * A node of a mined decision tree. A split node carries `key` plus either
 * `value` (equality) or `threshold` (numeric ≤) and both subtrees; a leaf
 * carries `branch`, the predicted branch index. Both carry the per-branch
 * observation counts the node saw.
 */
export interface DecisionTreeNode {
  counts: number[];
  branch?: number;
  key?: string;
  value?: string;
  threshold?: number;
  then?: DecisionTreeNode;
  else?: DecisionTreeNode;
}

/** One alternative of a decision point, with its observed frequency. */
export interface DecisionBranch {
  label: string;
  count: number;
}

/** One place of the net where replay must choose among several transitions. */
export interface DecisionPoint {
  place: number;
  observations: number;
  /** Share of observations the mined tree predicts correctly. */
  accuracy: number;
  /** Baseline: always guessing the most frequent branch. */
  majority_share: number;
  branches: DecisionBranch[];
  tree: DecisionTreeNode;
}

/** Decision mining of one model: which attributes steer each choice. */
export interface Decisions {
  /** Miner id the decisions belong to; absent in on-demand server responses
   * from older versions. */
  miner?: string;
  /** Traces that replayed cleanly and contributed observations. */
  observed_traces: number;
  trace_count: number;
  decision_points: DecisionPoint[];
}

export interface MineraExport {
  dataset: string;
  generator?: string;
  trace_count: number;
  event_count: number;
  /** Raw event log; absent in exports from older minera_export versions. */
  traces?: Trace[];
  activities: ActivityStat[];
  edges: EdgeStat[];
  variants: VariantStat[];
  /** Discovered process tree; absent in exports from older minera versions. */
  process_tree?: ProcessTreeNode;
  /**
   * Conformance of the log against each discovered model, keyed by miner id;
   * every entry carries a token-replay and an alignment result. Absent in
   * exports from older minera versions.
   */
  conformance?: ConformanceModels;
  /** Alpha Miner Petri net; absent in exports from older minera versions. */
  petri_net?: PetriNetModel;
  /**
   * Replay decoration of `petri_net` (firing counts, place waiting times);
   * absent in exports from older minera versions.
   */
  petri_net_decoration?: PetriDecoration;
  /**
   * The Inductive and Heuristic models as workflow nets, each with its own
   * replay decoration — so any miner's model can be drawn (and annotated) as
   * a Petri net. Absent in exports from older minera versions.
   */
  inductive_petri_net?: PetriNetModel;
  inductive_petri_net_decoration?: PetriDecoration;
  heuristic_petri_net?: PetriNetModel;
  heuristic_petri_net_decoration?: PetriDecoration;
  /** POWL discovery model and its converted workflow net, when available. */
  powl?: PowlNode;
  powl_petri_net?: PetriNetModel;
  powl_petri_net_decoration?: PetriDecoration;
  /** Classic log-skeleton relations and allowed activity frequencies. */
  log_skeleton?: LogSkeletonModel;
  /** Trace-order log-skeleton conformance results. */
  log_skeleton_conformance?: LogSkeletonConformanceTrace[];
  /** Classic declarative Declare model and trace diagnostics. */
  declare?: DeclareModel;
  declare_conformance?: DeclareConformanceTrace[];
  /** Aggregate footprints discovery and relational conformance. */
  footprints?: AggregateFootprints;
  /** Opt-in PM4Py statistical DFG and causal-discovery variants. */
  dfg_variants?: DfgVariantsAnalysis;
  /** Heuristic Miner dependency graph; absent in exports from older versions. */
  heuristics_net?: HeuristicsNet;
  /** Time perspective (edge waiting times, case throughput); absent in older exports. */
  performance?: Performance;
  /** Organizational perspective (handover-of-work network); absent in older exports. */
  resources?: ResourceNetwork;
  /**
   * Eventually-follows graph: for each activity pair (a, b), how often a is
   * followed by b — directly or transitively — later in the same case. Same
   * shape as `edges`. Absent in exports from older minera versions.
   */
  eventually_follows?: EdgeStat[];
  /** Temporal-profile timing statistics; absent in exports from older versions. */
  temporal_profile?: TemporalProfileEntry[];
  /** Minimum self-distance and witness activities; absent in older exports. */
  minimum_self_distances?: MinimumSelfDistanceEntry[];
  /** PM4Py view-based transition system; absent in older exports. */
  transition_system?: TransitionSystemExport;
  /**
   * Generic event attributes by key, with each key's most frequent values.
   * Absent when the log has none and in exports from older minera versions.
   */
  event_attributes?: EventAttributeStat[];
  /**
   * Case attributes by key, with each key's most frequent values. Absent when
   * the log has none and in exports from older minera versions.
   */
  case_attributes?: CaseAttributeStat[];
  /**
   * Decision mining of one model (its id in `decisions.miner`), embedded by
   * the CLI's --decisions flag; the server serves it on demand instead.
   */
  decisions?: Decisions;
}

export function isMineraExport(value: unknown): value is MineraExport {
  const data = value as MineraExport;
  return (
    typeof data === "object" &&
    data !== null &&
    Array.isArray(data.activities) &&
    Array.isArray(data.edges) &&
    Array.isArray(data.variants) &&
    Number.isFinite(data.trace_count)
  );
}

/** Virtual node ids for the trace entry and exit points. */
export const START = -1;
export const END = -2;

export const numberFormat = new Intl.NumberFormat("en-US");

/**
 * Formats a duration in seconds as a compact human label, picking the largest
 * fitting unit: "12s", "3.4m", "2.1h", "5.0d". Non-positive or non-finite
 * inputs render as an em dash so untimed edges read as "no data".
 */
export function formatDuration(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds <= 0) return "—";
  if (seconds < 60) return `${Math.round(seconds)}s`;
  if (seconds < 3600) return `${(seconds / 60).toFixed(1)}m`;
  if (seconds < 86400) return `${(seconds / 3600).toFixed(1)}h`;
  return `${(seconds / 86400).toFixed(1)}d`;
}
