import { computeLayout, edgeKey, type GraphLayout } from "./layout";
import { type HeuristicsNet, type MineraExport } from "./types";

export interface DependencyLayout {
  graph: GraphLayout;
  /** Dependency measure per edge, keyed like the layout's edge keys. */
  dependency: Map<string, number>;
}

// Lays out a heuristics net by reusing the directly-follows graph layout: the
// net's accepted edges stand in for the DFG edges (their frequency drives edge
// weight) and the net's own start/end activities drive the terminals. The
// dependency measure is returned alongside so the view can label each edge.
export function computeDependencyLayout(
  net: HeuristicsNet,
  data: MineraExport,
): DependencyLayout {
  const starts = new Set(net.start_activities);
  const ends = new Set(net.end_activities);

  const activities = data.activities.map((activity, id) => ({
    ...activity,
    start_count: starts.has(id) ? 1 : 0,
    end_count: ends.has(id) ? 1 : 0,
  }));
  const edges = net.edges.map((edge) => ({
    source: edge.source,
    target: edge.target,
    count: edge.frequency,
  }));

  const synthetic: MineraExport = { ...data, activities, edges };

  const dependency = new Map<string, number>();
  for (const edge of net.edges) {
    dependency.set(edgeKey(edge.source, edge.target), edge.dependency);
  }

  return { graph: computeLayout(synthetic), dependency };
}
