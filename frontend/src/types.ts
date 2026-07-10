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
}

export interface HeuristicsNet {
  edges: HeuristicEdge[];
  start_activities: number[];
  end_activities: number[];
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
