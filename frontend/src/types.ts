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
  /** Conformance of the log against the process tree; absent in older exports. */
  conformance?: Conformance;
  /** Alpha Miner Petri net; absent in exports from older minera versions. */
  petri_net?: PetriNetModel;
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
