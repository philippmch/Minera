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
