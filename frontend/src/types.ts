export interface ActivityStat {
  name: string;
  count: number;
  start_count: number;
  end_count: number;
}

export interface EdgeStat {
  source: number;
  target: number;
  count: number;
}

export interface VariantStat {
  activities: number[];
  count: number;
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
   * False when the model is not a sound workflow net (e.g. an Alpha net with an
   * unreachable sink); the other fields are then meaningless and variants empty.
   */
  computed: boolean;
  /** Cost-based fitness in [0, 1]: how cheaply the log aligns to the model. */
  fitness: number;
  fitting_traces: number;
  trace_count: number;
  /** Summed optimal alignment cost over all traces. */
  total_cost: number;
  /** Variants worst-fitting first. */
  variants: VariantAlignment[];
}

/** Both conformance results for one discovered model. */
export interface ModelConformance {
  token_replay: Conformance;
  alignments: Alignments;
}

/** Per-miner conformance, keyed by miner id ("inductive", "alpha", "heuristic"). */
export type ConformanceModels = Record<string, ModelConformance>;

export interface TraceEvent {
  activity: number;
  timestamp: string;
}

export interface Trace {
  case_id: string;
  events: TraceEvent[];
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
  /** Heuristic Miner dependency graph; absent in exports from older versions. */
  heuristics_net?: HeuristicsNet;
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
