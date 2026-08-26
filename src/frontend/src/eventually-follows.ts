import { type ActivityStat, type EdgeStat } from "./types";

/** The eventually-follows matrix, ordered for display. */
export interface EfgMatrix {
  /** Activity ids (indexes into MineraExport.activities), most frequent first. */
  order: number[];
  /** counts[row][col]: how often order[row] eventually precedes order[col]. */
  counts: number[][];
  /** The largest cell, for shading; 0 when the matrix is empty. */
  max: number;
}

/**
 * Arranges the eventually-follows edges into a dense matrix over the observed
 * activities (zero-count registry entries — e.g. from an imported model — are
 * left out), sorted most-frequent-first on both axes so the busy pairs sit in
 * the top-left corner.
 */
export function buildEfgMatrix(
  activities: ActivityStat[],
  edges: EdgeStat[],
): EfgMatrix {
  const order = activities
    .map((activity, id) => ({ id, count: activity.count }))
    .filter((entry) => entry.count > 0)
    .sort((a, b) => b.count - a.count)
    .map((entry) => entry.id);

  const row = new Map(order.map((id, index) => [id, index]));
  const counts = order.map(() => order.map(() => 0));
  let max = 0;
  for (const edge of edges) {
    const source = row.get(edge.source);
    const target = row.get(edge.target);
    if (source === undefined || target === undefined) continue;
    counts[source][target] = edge.count;
    if (edge.count > max) max = edge.count;
  }
  return { order, counts, max };
}
