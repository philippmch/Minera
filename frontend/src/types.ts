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

export interface ConceptDriftSublogSummary {
  trace_count: number;
  event_count: number;
}
export interface ConceptDriftChangePoint {
  sublog_index: number;
  p_value: number;
}
export interface ConceptDriftResult {
  family: "CONCEPT_DRIFT";
  activities: string[];
  feature_vectors: number[][];
  sublogs: ConceptDriftSublogSummary[];
  cumulative_sublogs: ConceptDriftSublogSummary[];
  sublog_start_timestamps: number[];
  change_points: ConceptDriftChangePoint[];
  change_timestamps: number[];
}

export type LogToTargetVariant = "next_activity" | "next_time" | "remaining_time";
export interface LogToTargetResult {
  family: "LOG_TO_TARGET";
  variant: LogToTargetVariant;
  classes: string[];
  rows: number[][][];
}
export function isLogToTargetResult(value: unknown): value is LogToTargetResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<LogToTargetResult>;
  return candidate.family === "LOG_TO_TARGET"
    && (candidate.variant === "next_activity" || candidate.variant === "next_time" || candidate.variant === "remaining_time")
    && Array.isArray(candidate.classes) && candidate.classes.every((entry) => typeof entry === "string")
    && Array.isArray(candidate.rows)
    && candidate.rows.every((trace) => Array.isArray(trace) && trace.every((row) => Array.isArray(row) && row.every((entry) => typeof entry === "number" && Number.isFinite(entry))));
}

export interface TrieNode {
  label: string;
  depth: number;
  final: boolean;
  children: TrieNode[];
}
export interface TrieResult {
  family: "LOG_TO_TRIE";
  root: TrieNode;
}
function isTrieNode(value: unknown): value is TrieNode {
  if (!value || typeof value !== "object") return false;
  const node = value as Partial<TrieNode>;
  return typeof node.label === "string" && typeof node.depth === "number" && Number.isInteger(node.depth) && node.depth >= 0
    && typeof node.final === "boolean" && Array.isArray(node.children)
    && node.children.every(isTrieNode);
}
export function isTrieResult(value: unknown): value is TrieResult {
  return !!value && typeof value === "object" && (value as Partial<TrieResult>).family === "LOG_TO_TRIE"
    && isTrieNode((value as Partial<TrieResult>).root);
}

export interface OpenPathInterval {
  start: number;
  end: number;
  case_id: string;
  source_activity: string;
  target_activity: string;
  trace_index: number;
  source_event_index: number;
  target_event_index: number;
}
export interface IntervalTreeResult {
  family: "LOG_TO_INTERVAL_TREE";
  intervals: OpenPathInterval[];
}
export function isIntervalTreeResult(value: unknown): value is IntervalTreeResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<IntervalTreeResult>;
  return candidate.family === "LOG_TO_INTERVAL_TREE" && Array.isArray(candidate.intervals)
    && candidate.intervals.every((interval) => !!interval && typeof interval === "object"
      && Number.isFinite((interval as OpenPathInterval).start)
      && Number.isFinite((interval as OpenPathInterval).end)
      && typeof (interval as OpenPathInterval).case_id === "string"
      && typeof (interval as OpenPathInterval).source_activity === "string"
      && typeof (interval as OpenPathInterval).target_activity === "string"
      && Number.isInteger((interval as OpenPathInterval).trace_index)
      && Number.isInteger((interval as OpenPathInterval).source_event_index)
      && Number.isInteger((interval as OpenPathInterval).target_event_index));
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

export interface PetriUsageRecord {
  id: number;
  log1_occurrences: number;
  log2_occurrences: number;
  total_occurrences: number;
  direction: number;
}
export interface PetriArcUsageRecord extends PetriUsageRecord {
  transition: number;
  place: number;
  input: boolean;
}
export interface PetriUsageComparison {
  family: "PETRI_USAGE_COMPARISON";
  places: PetriUsageRecord[];
  transitions: PetriUsageRecord[];
  arcs: PetriArcUsageRecord[];
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

/** Petri-net alignment search variants exposed by the compatible API. */
export type AlignmentVariant =
  | "state_equation_a_star"
  | "dijkstra_no_heuristics"
  | "dijkstra_less_memory"
  | "discounted_a_star"
  | "generator_dijkstra_no_heuristics"
  | "generator_dijkstra_less_memory";

export type AlignmentCostProfile = "legacy" | "standard";

/** Optional controls for one deferred alignment request. */
export interface AlignmentOptions {
  variant?: AlignmentVariant;
  max_states?: number;
  max_align_time?: number;
  max_align_time_trace?: number;
  max_alignments?: number;
  exponent?: number;
  synchronous_dijkstra?: boolean;
  cost_profile?: AlignmentCostProfile;
  trace_cost?: number;
  model_cost?: number;
  silent_model_cost?: number;
  sync_cost?: number;
}

export const DEFAULT_ALIGNMENT_OPTIONS: Required<Pick<AlignmentOptions, "variant" | "cost_profile">> = {
  variant: "state_equation_a_star",
  cost_profile: "legacy",
};

export interface AlignmentMove {
  type: AlignmentMoveType;
  /** Activity id of the move; null for a silent model (tau) transition. */
  activity: number | null;
  /** Explicit marker for a silent model move; absent on older exports. */
  silent?: boolean;
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
  /** Equal-optimal alternatives returned by generator variants, when present. */
  alternatives?: AlignmentMove[][];
  /** Compatible spelling accepted for older/newer server exports. */
  optimal_alignments?: AlignmentMove[][];
  /** Search work for this trace variant, absent in older exports. */
  diagnostics?: AlignmentDiagnostics;
}

export interface AlignmentDiagnostics {
  visited_states?: number;
  queued_states?: number;
  traversed_arcs?: number;
  /** Number of marking-equation LP solves performed (0 when no LP was used). */
  lp_solved?: number;
  closed_set_length?: number;
  num_visited_markings?: number;
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
  /** Search variant selected by the caller (absent in older exports). */
  variant?: AlignmentVariant;
  /** Canonical server spelling, accepted alongside `variant`. */
  alignment_variant?: AlignmentVariant;
  diagnostics?: AlignmentDiagnostics;
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

/** Classic directly-follows-graph alignment request controls. */
export type DfgAlignmentVariant = "CLASSIC";
export type DfgAlignmentProjection = "none" | "log" | "metrics";
export type DfgCostMap = Record<string, number>;
export interface DfgAlignmentOptions {
  variant?: DfgAlignmentVariant;
  max_states?: number;
  max_time_ms?: number;
  sync_cost?: DfgCostMap;
  model_move_cost?: DfgCostMap;
  log_move_cost?: DfgCostMap;
  internal_log_move_cost?: DfgCostMap;
  projection?: DfgAlignmentProjection;
}
export interface DfgAlignmentMove { log: string; model: string; }
export interface DfgAlignmentTrace {
  cost: number;
  internal_cost: number;
  fitness: number;
  best_worst_cost: number;
  visited_states: number;
  closed: number;
  closed_states: number;
  moves: DfgAlignmentMove[];
}
export interface DfgProjectedEdge { source: string; target: string; sync: number; model_move: number; }
export interface DfgProjectedActivity { activity: string; sync: number; model_move: number; log_move: number; }
export interface DfgAlignmentProjectionResult {
  edges: DfgProjectedEdge[];
  activities: DfgProjectedActivity[];
}
export interface DfgAlignmentResult {
  family: "DFG";
  variant: "CLASSIC";
  unique_variants: number;
  alignments: DfgAlignmentTrace[];
  projected_dfg?: DfgAlignmentProjectionResult;
  projected_log?: unknown;
}

export type EditDistanceVariant = "EDIT_DISTANCE";
export interface EditDistanceOptions {
  variant?: EditDistanceVariant;
  anti?: boolean;
  deviation_cost?: number;
  projection?: "none" | "log";
}
export interface EditDistanceMove {
  type: "sync" | "log" | "model";
  log: string;
  model: string;
}
export interface EditDistanceTrace {
  source_variant: string[];
  target_variant: string[];
  target_index: number;
  cost: number;
  fitness: number;
  best_worst_cost: number;
  bwc: number;
  denominator: number;
  moves: EditDistanceMove[];
}
export interface EditDistanceResult {
  family: "EDIT_DISTANCE";
  variant: "EDIT_DISTANCE";
  anti: boolean;
  source_count: number;
  alignments: EditDistanceTrace[];
  projected_log?: unknown;
}

/** Process-tree alignment search variants exposed by the deferred API. */
export type ProcessTreeAlignmentVariant =
  | "SEARCH_GRAPH_PT"
  | "DYNAMIC_PROGRAMMING"
  | "MILP"
  | "APPROXIMATED_ORIGINAL";

export interface ProcessTreeAlignmentOptions {
  variant?: ProcessTreeAlignmentVariant;
  max_states?: number;
  max_time_ms?: number;
  max_total_time_ms?: number;
  use_global_cache?: boolean;
  batch_size?: number;
  clear_cache?: boolean;
  max_trace_length?: number;
  max_process_tree_height?: number;
  max_trace_time_ms?: number;
  max_per_trace_time_ms?: number;
}

export interface ProcessTreeAlignmentMove {
  type: "sync" | "log" | "model";
  log: string;
  model: string;
  activity: string;
}

export interface ProcessTreeAlignmentTrace {
  trace: string[];
  cost: number;
  fitness: number;
  completed: boolean;
  timed_out: boolean;
  moves: ProcessTreeAlignmentMove[];
}

export interface ProcessTreeAlignmentDiagnostics {
  visited_states: number;
  queued_states: number;
  traversed_arcs: number;
  lp_solved: number;
  traces_aligned: number;
  message: string;
}

export interface ProcessTreeAlignmentResult {
  family: "PROCESS_TREE";
  variant: ProcessTreeAlignmentVariant;
  computed: boolean;
  completed: boolean;
  timed_out: boolean;
  exhausted: boolean;
  total_cost: number;
  fitness: number;
  diagnostics: ProcessTreeAlignmentDiagnostics;
  traces: ProcessTreeAlignmentTrace[];
}

/** Decomposed Petri-net alignment (recomposition variant). */
export type DecomposedAlignmentVariant = "recompos_maximal";
export interface DecomposedAlignmentOptions {
  variant?: DecomposedAlignmentVariant;
  threshold_border_agreement?: number;
  max_time_trace?: number;
  max_time_total?: number;
  max_states?: number;
  max_total_states?: number;
  use_cache?: boolean;
  clear_cache?: boolean;
  log_move_cost?: number;
  model_move_cost?: number;
  tau_model_cost?: number;
  sync_cost?: number;
  initial_marking?: number[];
  final_marking?: number[];
  activity_log_costs?: Record<string, number>;
  activity_model_costs?: Record<string, number>;
  activity_sync_costs?: Record<string, number>;
}
export interface DecomposedAlignmentMove {
  log?: string | null;
  model?: string | null;
  transition?: number;
}
export interface DecomposedAlignmentTrace {
  activities: string[];
  count: number;
  complete: boolean;
  timed_out: boolean;
  exhausted: boolean;
  cost: number;
  fitness: number;
  best_worst_cost: number;
  /** Native serializer calls this field `moves`; `alignment` is retained for older exports. */
  moves?: DecomposedAlignmentMove[];
  alignment?: DecomposedAlignmentMove[];
}
export interface DecomposedAlignmentDiagnostics {
  decompositions: number;
  components: number;
  merges: number;
  cache_hits: number;
  cache_misses: number;
  border_disagreements: number;
  agreement_iterations: number;
  visited_states: number;
  queued_states: number;
  traversed_arcs: number;
  whole_net_fallback: boolean;
  note: string;
}
export interface DecomposedAlignmentResult {
  algorithm: "decomposed_alignment";
  variant: DecomposedAlignmentVariant;
  complete: boolean;
  timed_out: boolean;
  exhausted: boolean;
  fitness: number;
  best_worst_cost: number;
  trace_count: number;
  total_cost: number;
  diagnostics: DecomposedAlignmentDiagnostics;
  traces: DecomposedAlignmentTrace[];
}

export type PetriAdvancedAlignmentVariant = "version_discounted_a_star";
export interface PetriAntiAlignmentOptions {
  variant?: PetriAdvancedAlignmentVariant;
  exponent?: number;
  epsilon?: number;
  marking_limit?: number;
  max_states?: number;
  initial_marking?: number[];
  final_marking?: number[];
}
export interface PetriMultiAlignmentOptions {
  variant?: PetriAdvancedAlignmentVariant;
  exponent?: number;
  marking_limit?: number;
  max_states?: number;
  initial_marking?: number[];
  final_marking?: number[];
}
export interface PetriAdvancedAlignmentDiagnostics {
  visited_states: number;
  queued_states: number;
  traversed_arcs: number;
  exhausted: boolean;
}
export interface PetriAntiAlignmentResult {
  algorithm: "petri_anti_alignment";
  variant: PetriAdvancedAlignmentVariant;
  activities: string[];
  transitions: number[];
  cost: number;
  objective: number;
  precision: number;
  diagnostics: PetriAdvancedAlignmentDiagnostics;
}
export interface PetriMultiAlignmentResult {
  algorithm: "petri_multi_alignment";
  variant: PetriAdvancedAlignmentVariant;
  activities: string[];
  transitions: number[];
  cost: number;
  objective: number;
  max_distance_to_log: number;
  diagnostics: PetriAdvancedAlignmentDiagnostics;
}

export type ProcessTreeReductionVariant = "tree_tr_based";
export interface ProcessTreeReductionOptions { variant?: ProcessTreeReductionVariant; activity_key?: string; }
export interface ProcessTreeReductionResult {
  algorithm: "process_tree_reduction";
  variant: ProcessTreeReductionVariant;
  trace_index: number;
  process_tree: ProcessTreeNode;
}

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null;
const finite = (value: unknown): value is number => typeof value === "number" && Number.isFinite(value);
const isPetriUsageRecord = (value: unknown): value is PetriUsageRecord => {
  if (!isRecord(value)) return false;
  const rec = value as Record<string, unknown>;
  return Number.isInteger(rec.id) && (rec.id as number) >= 0 &&
    Number.isInteger(rec.log1_occurrences) && (rec.log1_occurrences as number) >= 0 &&
    Number.isInteger(rec.log2_occurrences) && (rec.log2_occurrences as number) >= 0 &&
    Number.isInteger(rec.total_occurrences) && (rec.total_occurrences as number) >= 0 &&
    finite(rec.direction);
};
export function isPetriUsageComparison(value: unknown): value is PetriUsageComparison {
  if (!isRecord(value) || value.family !== "PETRI_USAGE_COMPARISON" ||
      !Array.isArray(value.places) || !Array.isArray(value.transitions) || !Array.isArray(value.arcs)) return false;
  return value.places.every(isPetriUsageRecord) && value.transitions.every(isPetriUsageRecord) &&
    value.arcs.every((arc) => {
      if (!isPetriUsageRecord(arc) || !isRecord(arc)) return false;
      const rec = arc as Record<string, unknown>;
      return Number.isInteger(rec.transition) && (rec.transition as number) >= 0 &&
        Number.isInteger(rec.place) && (rec.place as number) >= 0 &&
        typeof rec.input === "boolean";
    });
}
export function isDfgAlignmentResult(value: unknown): value is DfgAlignmentResult {
  if (!isRecord(value) || value.family !== "DFG" || value.variant !== "CLASSIC" ||
      !Number.isInteger(value.unique_variants) || !Array.isArray(value.alignments)) return false;
  return value.alignments.every((a) => isRecord(a) && finite(a.cost) && finite(a.internal_cost) &&
    finite(a.fitness) && Array.isArray(a.moves) && a.moves.every((m) => isRecord(m) && typeof m.log === "string" && typeof m.model === "string"));
}
export function isEditDistanceResult(value: unknown): value is EditDistanceResult {
  if (!isRecord(value) || value.family !== "EDIT_DISTANCE" || value.variant !== "EDIT_DISTANCE" ||
      typeof value.anti !== "boolean" || !Number.isInteger(value.source_count) || !Array.isArray(value.alignments)) return false;
  return value.alignments.every((a) => isRecord(a) && Array.isArray(a.source_variant) &&
    Array.isArray(a.target_variant) && finite(a.cost) && finite(a.fitness) && Array.isArray(a.moves) &&
    a.moves.every((m) => isRecord(m) && (m.type === "sync" || m.type === "log" || m.type === "model") && typeof m.log === "string" && typeof m.model === "string"));
}

const metric = (value: unknown): value is number | null => value === null || finite(value);
const nullableString = (value: unknown): value is string | null => value === null || typeof value === "string";
const optionalRecord = (value: unknown): boolean => value === undefined || isRecord(value);
const evaluationNonNegativeInt = (value: unknown): value is number => Number.isInteger(value) && (value as number) >= 0;

function isReplayFitnessEvaluation(value: unknown): value is ReplayFitnessEvaluation {
  return isRecord(value) && ["token_based", "alignment_based"].includes(value.variant as string) &&
    typeof value.computed === "boolean" && typeof value.exhausted === "boolean" && nullableString(value.error) &&
    metric(value.fitness) && metric(value.log_fitness) && metric(value.average_trace_fitness) &&
    metric(value.percentage_of_fitting_traces) && evaluationNonNegativeInt(value.fitting_traces) &&
    evaluationNonNegativeInt(value.trace_count) && metric(value.total_cost) && optionalRecord(value.token_replay) &&
    optionalRecord(value.alignments);
}

function isPrecisionEvaluation(value: unknown): value is PrecisionEvaluation {
  return isRecord(value) && ["etconformance_token", "align_etconformance", "automaton_after_align"].includes(value.variant as string) &&
    typeof value.computed === "boolean" && typeof value.exhausted === "boolean" && nullableString(value.error) &&
    metric(value.precision) && optionalRecord(value.token_replay) && optionalRecord(value.alignments);
}

function isGeneralizationEvaluation(value: unknown): value is GeneralizationEvaluation {
  return isRecord(value) && value.variant === "generalization_token" && typeof value.computed === "boolean" &&
    nullableString(value.error) && metric(value.score) && evaluationNonNegativeInt(value.executed_transitions) &&
    evaluationNonNegativeInt(value.transition_count) && Array.isArray(value.transition_frequencies) &&
    value.transition_frequencies.every(evaluationNonNegativeInt);
}

function isSimplicityEvaluation(value: unknown): value is SimplicityEvaluation {
  return isRecord(value) && ["arc_degree", "extended_cardoso", "extended_cyclomatic"].includes(value.variant as string) &&
    typeof value.computed === "boolean" && typeof value.exhausted === "boolean" && nullableString(value.error) &&
    metric(value.simplicity) && metric(value.complexity) && metric(value.value) && evaluationNonNegativeInt(value.vertices) &&
    evaluationNonNegativeInt(value.edges) && evaluationNonNegativeInt(value.strongly_connected_components);
}

/** Validates a standalone response from the model-quality endpoint. */
export function isModelEvaluation(value: unknown): value is ModelEvaluation {
  if (!isRecord(value) || typeof value.miner !== "string") return false;
  const present = [value.replay_fitness, value.precision, value.generalization, value.simplicity]
    .filter((item) => item !== undefined).length;
  if (present !== 1) return false;
  return (value.replay_fitness === undefined || isReplayFitnessEvaluation(value.replay_fitness)) &&
    (value.precision === undefined || isPrecisionEvaluation(value.precision)) &&
    (value.generalization === undefined || isGeneralizationEvaluation(value.generalization)) &&
    (value.simplicity === undefined || isSimplicityEvaluation(value.simplicity));
}

/** Validates the stochastic-language distance endpoint response. */
export function isEmdResult(value: unknown): value is EmdResult {
  return isRecord(value) && metric(value.distance) && typeof value.computed === "boolean" &&
    typeof value.normalized === "boolean" && nullableString(value.error);
}

export function isProcessTreeAlignmentResult(value: unknown): value is ProcessTreeAlignmentResult {
  if (!isRecord(value) || value.family !== "PROCESS_TREE" ||
      !["SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP", "APPROXIMATED_ORIGINAL"].includes(value.variant as string) ||
      typeof value.computed !== "boolean" || typeof value.completed !== "boolean" ||
      typeof value.timed_out !== "boolean" || typeof value.exhausted !== "boolean" ||
      !finite(value.total_cost) || !finite(value.fitness) || !isRecord(value.diagnostics) ||
      !Array.isArray(value.traces)) return false;
  const d = value.diagnostics;
  if (!["visited_states", "queued_states", "traversed_arcs", "lp_solved", "traces_aligned"].every((key) => Number.isInteger(d[key]) && (d[key] as number) >= 0) || typeof d.message !== "string") return false;
  return value.traces.every((trace) => isRecord(trace) && Array.isArray(trace.trace) &&
    trace.trace.every((activity) => typeof activity === "string") && finite(trace.cost) &&
    finite(trace.fitness) && typeof trace.completed === "boolean" && typeof trace.timed_out === "boolean" && Array.isArray(trace.moves) &&
    trace.moves.every((move) => isRecord(move) &&
      (move.type === "sync" || move.type === "log" || move.type === "model") &&
      typeof move.log === "string" && typeof move.model === "string" && typeof move.activity === "string"));
}

const nonNegativeInt = (value: unknown): value is number => Number.isInteger(value) && (value as number) >= 0;
const validDecomposedMove = (value: unknown): value is DecomposedAlignmentMove =>
  isRecord(value) && (value.log === undefined || value.log === null || typeof value.log === "string") &&
  (value.model === undefined || value.model === null || typeof value.model === "string") &&
  (value.transition === undefined || (Number.isInteger(value.transition) && (value.transition as number) >= -1));
const validAdvancedDiagnostics = (value: unknown): value is PetriAdvancedAlignmentDiagnostics =>
  isRecord(value) && nonNegativeInt(value.visited_states) && nonNegativeInt(value.queued_states) &&
  nonNegativeInt(value.traversed_arcs) && typeof value.exhausted === "boolean";

export function isDecomposedAlignmentResult(value: unknown): value is DecomposedAlignmentResult {
  if (!isRecord(value) || value.algorithm !== "decomposed_alignment" || value.variant !== "recompos_maximal" ||
      typeof value.complete !== "boolean" || typeof value.timed_out !== "boolean" || typeof value.exhausted !== "boolean" ||
      !finite(value.fitness) || !finite(value.best_worst_cost) || !nonNegativeInt(value.trace_count) || !finite(value.total_cost) ||
      !isRecord(value.diagnostics) || !Array.isArray(value.traces)) return false;
  const d = value.diagnostics;
  if (!["decompositions", "components", "merges", "cache_hits", "cache_misses", "border_disagreements", "agreement_iterations", "visited_states", "queued_states", "traversed_arcs"]
    .every((key) => nonNegativeInt(d[key])) || typeof d.whole_net_fallback !== "boolean" || typeof d.note !== "string") return false;
  return value.traces.every((trace) => isRecord(trace) && Array.isArray(trace.activities) && trace.activities.every((a) => typeof a === "string") &&
    nonNegativeInt(trace.count) && typeof trace.complete === "boolean" && typeof trace.timed_out === "boolean" && typeof trace.exhausted === "boolean" &&
    finite(trace.cost) && finite(trace.fitness) && finite(trace.best_worst_cost) &&
    ((Array.isArray(trace.moves) && trace.moves.every(validDecomposedMove)) || (Array.isArray(trace.alignment) && trace.alignment.every(validDecomposedMove))));
}

function isPetriAdvancedResult(value: unknown, algorithm: string): value is PetriAntiAlignmentResult | PetriMultiAlignmentResult {
  if (!isRecord(value) || value.algorithm !== algorithm || value.variant !== "version_discounted_a_star" ||
      !Array.isArray(value.activities) || !value.activities.every((a) => typeof a === "string") || !Array.isArray(value.transitions) ||
      !value.transitions.every((x) => Number.isInteger(x) && (x as number) >= -1) || !finite(value.cost) || !finite(value.objective) || !validAdvancedDiagnostics(value.diagnostics)) return false;
  if (algorithm === "petri_anti_alignment" && !finite(value.precision)) return false;
  if (algorithm === "petri_multi_alignment" && !finite(value.max_distance_to_log)) return false;
  return true;
}
export function isPetriAntiAlignmentResult(value: unknown): value is PetriAntiAlignmentResult { return isPetriAdvancedResult(value, "petri_anti_alignment"); }
export function isPetriMultiAlignmentResult(value: unknown): value is PetriMultiAlignmentResult { return isPetriAdvancedResult(value, "petri_multi_alignment"); }

function isProcessTreeNode(value: unknown): value is ProcessTreeNode {
  if (!isRecord(value) || !["sequence", "exclusive", "parallel", "loop", "activity", "tau"].includes(value.operator as string)) return false;
  if (value.operator === "activity" && !nonNegativeInt(value.activity)) return false;
  if (value.activity !== undefined && !nonNegativeInt(value.activity)) return false;
  if (value.children !== undefined && (!Array.isArray(value.children) || !value.children.every(isProcessTreeNode))) return false;
  return value.operator === "activity" || value.operator === "tau" ? value.children === undefined || value.children.length === 0 : Boolean(value.children && value.children.length > 0);
}
export function isProcessTreeReductionResult(value: unknown): value is ProcessTreeReductionResult {
  return isRecord(value) && value.algorithm === "process_tree_reduction" && value.variant === "tree_tr_based" &&
    nonNegativeInt(value.trace_index) && isProcessTreeNode(value.process_tree);
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

/** An aggregate footprint record; optional keys are variant-dependent. */
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

export type CorrelationMinerVariant =
  | "classic"
  | "classic_split"
  | "trace_based";
export interface CorrelationMatrixEntry {
  source: number;
  target: number;
  value: number;
}
export interface CorrelationMinerAnalysis {
  variant: CorrelationMinerVariant;
  frequency_dfg: EdgeStat[];
  performance_dfg: Array<{
    source: number;
    target: number;
    seconds: number;
  }>;
  precede_succeed_probability: CorrelationMatrixEntry[];
  duration_matrix: CorrelationMatrixEntry[];
}
export interface CorrelationVariantsAnalysis {
  variants: CorrelationMinerAnalysis[];
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

/** Per-miner conformance, keyed by discovered or imported model id. */
export type ConformanceModels = Record<string, ModelConformance>;

/** Model-quality evaluator families and their complete native variant set. */
export type EvaluationFamily =
  | "replay_fitness"
  | "precision"
  | "generalization"
  | "simplicity";
export type ReplayFitnessVariant = "auto" | "token_based" | "alignment_based";
export type PrecisionVariant =
  | "auto"
  | "etconformance_token"
  | "align_etconformance"
  | "automaton_after_align";
export type GeneralizationVariant = "generalization_token";
export type SimplicityVariant =
  | "arc_degree"
  | "extended_cardoso"
  | "extended_cyclomatic";

/** Query options accepted by the model-quality endpoint. */
export interface EvaluationOptions {
  evaluation_family?: EvaluationFamily;
  fitness_variant?: ReplayFitnessVariant;
  precision_variant?: PrecisionVariant;
  precision_use_task_ids?: boolean;
  precision_max_states?: number;
  generalization_variant?: GeneralizationVariant;
  simplicity_variant?: SimplicityVariant;
  simplicity_arc_degree_k?: number;
  simplicity_max_states?: number;
  simplicity_initial_marking?: number[];
  simplicity_final_marking?: number[];
}

export type PetriAnalysisFamily =
  | "marking_equation"
  | "extended_marking_equation"
  | "workflow_net"
  | "soundness";

export interface PetriAnalysisOptions {
  family?: PetriAnalysisFamily;
  miner?: string;
  trace_index?: number;
  initial_marking?: number[];
  final_marking?: number[];
  transition_costs?: number[];
  model_costs?: number[];
  sync_costs?: number[];
  log_costs?: number[];
  split_indices?: number[];
  max_k?: number;
  max_states?: number;
  full_bootstrap?: boolean;
  diagnostics?: boolean;
}

export type ProcessTreePlayoutVariant = "BASIC_PLAYOUT" | "EXTENSIVE" | "TOPBOTTOM";
export interface ProcessTreePlayoutOptions {
  variant?: ProcessTreePlayoutVariant;
  traces?: number;
  max_steps?: number;
  seed?: number;
  min_length?: number;
  max_length?: number;
  max_loop_occurrences?: number;
  max_trace_cap?: number;
}

/** Controls weighted random execution of a Petri-net model. */
export interface StochasticPlayoutOptions {
  traces?: number;
  max_steps?: number;
  seed?: number;
  transition_weights?: number[];
  stop_at_final?: boolean;
  start?: number;
  case_interval?: number;
  event_interval?: number;
}

export type DfgPlayoutVariant = "classic" | "performance";
export interface DfgPlayoutOptions {
  variant?: DfgPlayoutVariant;
  max_variants?: number;
  min_weighted_probability?: number;
  max_occurrences_per_activity?: number;
  interrupt_when_dfg_complete?: boolean;
  add_trace_if_takes_new_edges?: boolean;
  max_execution_time_ms?: number;
  return_variants?: boolean;
  return_only_if_complete?: boolean;
  min_variant_occurrences?: number;
  traces?: number;
  case_arrival_rate?: number;
  seed?: number;
  max_steps?: number;
  max_events_per_trace?: number;
  start_time?: string;
}

/** Response from the standalone Petri-analysis endpoint. */
export interface PetriAnalysisResult {
  family: PetriAnalysisFamily;
  variant?: string;
  result?: Record<string, unknown>;
  computed?: boolean;
  feasible?: boolean;
  exhausted?: boolean;
  objective?: number;
  lower_bound?: number;
  status?: "sound" | "unsound" | "unknown";
  error?: string;
  [key: string]: unknown;
}

export function isPetriAnalysisResult(value: unknown): value is PetriAnalysisResult {
  if (!value || typeof value !== "object") return false;
  const family = (value as { family?: unknown }).family;
  return family === "marking_equation" || family === "extended_marking_equation" ||
    family === "workflow_net" || family === "soundness";
}

type EvaluationMetric = number | null;
export interface ReplayFitnessEvaluation {
  variant: Exclude<ReplayFitnessVariant, "auto">;
  computed: boolean;
  exhausted: boolean;
  error: string | null;
  fitness: EvaluationMetric;
  log_fitness: EvaluationMetric;
  average_trace_fitness: EvaluationMetric;
  percentage_of_fitting_traces: EvaluationMetric;
  fitting_traces: number;
  trace_count: number;
  total_cost: EvaluationMetric;
  token_replay?: Conformance;
  alignments?: Alignments;
}
export interface PrecisionEvaluation {
  variant: Exclude<PrecisionVariant, "auto">;
  computed: boolean;
  exhausted: boolean;
  error: string | null;
  precision: EvaluationMetric;
  token_replay?: Conformance;
  alignments?: Alignments;
}
export interface GeneralizationEvaluation {
  variant: GeneralizationVariant;
  computed: boolean;
  error: string | null;
  score: EvaluationMetric;
  executed_transitions: number;
  transition_count: number;
  transition_frequencies: number[];
}
export interface SimplicityEvaluation {
  variant: SimplicityVariant;
  computed: boolean;
  exhausted: boolean;
  error: string | null;
  simplicity: EvaluationMetric;
  complexity: EvaluationMetric;
  value: EvaluationMetric;
  vertices: number;
  edges: number;
  strongly_connected_components: number;
}
export interface ModelEvaluation {
  miner: string;
  replay_fitness?: ReplayFitnessEvaluation;
  precision?: PrecisionEvaluation;
  generalization?: GeneralizationEvaluation;
  simplicity?: SimplicityEvaluation;
}

export interface EmdOptions {
  normalize_masses?: boolean;
  strict_equal_mass?: boolean;
}
export interface EmdResult {
  distance: EvaluationMetric;
  computed: boolean;
  normalized: boolean;
  error: string | null;
}

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

/** Server-computed performance spectrum observations. */
export interface PerformanceSpectrumObservation {
  case_id: string;
  activities: string[];
  instants: number[];
  /** Start index of this contiguous sub-pattern in the requested pattern. */
  pattern_offset: number;
}
export interface PerformanceSpectrumExport {
  variant: "connected" | "disconnected";
  activities: string[];
  observations: PerformanceSpectrumObservation[];
}

export type BatchType =
  | "simultaneous"
  | "batching_on_start"
  | "batching_on_end"
  | "sequential"
  | "concurrent";
export interface BatchEvent {
  start: number;
  complete: number;
  case_id: string;
  trace_index: number;
  event_index: number;
}
export interface BatchInstance {
  type: BatchType;
  start: number;
  complete: number;
  events: BatchEvent[];
}
export interface BatchGroup {
  activity: number;
  resource: string;
  batch_count: number;
  event_count: number;
  batches: BatchInstance[];
}
export interface BatchDiscoveryExport {
  groups: BatchGroup[];
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
  /** Optional model-quality evaluations embedded by a CLI export. */
  evaluation?: Record<string, Omit<ModelEvaluation, "miner">>;
  /** Optional stochastic-language distance embedded by a CLI export. */
  earth_movers_distance?: EmdResult;
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
  /** Opt-in statistical DFG and causal-discovery variants. */
  dfg_variants?: DfgVariantsAnalysis;
  /** Opt-in classic/split/trace-based correlation-miner variants. */
  correlation_mining?: CorrelationVariantsAnalysis;
  /** Heuristic Miner dependency graph; absent in exports from older versions. */
  heuristics_net?: HeuristicsNet;
  /** Time perspective (edge waiting times, case throughput); absent in older exports. */
  performance?: Performance;
  /** Optional performance spectrum analysis. */
  performance_spectrum?: PerformanceSpectrumExport;
  /** Optional batch discovery analysis. */
  batch_discovery?: BatchDiscoveryExport;
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
  /** View-based transition system; absent in older exports. */
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
