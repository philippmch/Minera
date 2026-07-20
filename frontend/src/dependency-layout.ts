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
  const selected =
    net.active_activities ??
    data.activities.map((_, id) => id);
  const remap = new Map(selected.map((id, compact) => [id, compact]));
  const starts = new Set(
    net.start_activities
      .map((id) => remap.get(id))
      .filter((id): id is number => id !== undefined),
  );
  const ends = new Set(
    net.end_activities
      .map((id) => remap.get(id))
      .filter((id): id is number => id !== undefined),
  );

  const activities = selected.map((original, id) => ({
    ...data.activities[original],
    start_count: starts.has(id) ? 1 : 0,
    end_count: ends.has(id) ? 1 : 0,
  }));
  const edges = net.edges.flatMap((edge) => {
    const source = remap.get(edge.source);
    const target = remap.get(edge.target);
    return source === undefined || target === undefined
      ? []
      : [{ source, target, count: edge.frequency }];
  });

  const synthetic: MineraExport = { ...data, activities, edges };

  const dependency = new Map<string, number>();
  for (const edge of net.edges) {
    const source = remap.get(edge.source);
    const target = remap.get(edge.target);
    if (source !== undefined && target !== undefined)
      dependency.set(edgeKey(source, target), edge.dependency);
  }

  return { graph: computeLayout(synthetic), dependency };
}
