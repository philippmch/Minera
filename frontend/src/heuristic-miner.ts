import { type HeuristicsNet, type MineraExport } from "./types";

export interface HeuristicOptions {
  /** Minimum dependency measure for an edge to be accepted, in [0, 1]. */
  dependencyThreshold: number;
  /** Minimum direct successions for an edge to be considered. */
  minFrequency: number;
  /** Connect every task to its strongest dependency even below threshold. */
  connectAll?: boolean;
}

// Re-derives the Heuristic Miner's dependency graph in the browser from the
// directly-follows counts already in the export (data.edges carry |a > b| for
// every pair, self-loops included). This mirrors the native discover_heuristics_net
// so the threshold sliders can re-mine instantly — no backend round-trip, and it
// works for pre-mined JSON exports and the bundled sample too.
export function discoverHeuristicsNet(
  data: MineraExport,
  options: HeuristicOptions,
): HeuristicsNet {
  const n = data.activities.length;
  const connectAll = options.connectAll ?? true;

  // follows[a * n + b] = |a > b|.
  const follows = new Array<number>(n * n).fill(0);
  for (const edge of data.edges) {
    if (edge.source < n && edge.target < n) {
      follows[edge.source * n + edge.target] = edge.count;
    }
  }

  const dependency = (a: number, b: number): number => {
    const f = follows[a * n + b];
    if (a === b) return f / (f + 1);
    const r = follows[b * n + a];
    return (f - r) / (f + r + 1);
  };

  const starts: number[] = [];
  const ends: number[] = [];
  const isStart = new Array<boolean>(n).fill(false);
  const isEnd = new Array<boolean>(n).fill(false);
  data.activities.forEach((activity, id) => {
    if (activity.start_count > 0) {
      starts.push(id);
      isStart[id] = true;
    }
    if (activity.end_count > 0) {
      ends.push(id);
      isEnd[id] = true;
    }
  });

  // Accept each edge once, keyed by a * n + b, so the threshold and connectivity
  // passes never emit a duplicate.
  const accepted = new Set<number>();
  const edges: HeuristicsNet["edges"] = [];
  const accept = (a: number, b: number) => {
    const key = a * n + b;
    if (accepted.has(key)) return;
    accepted.add(key);
    edges.push({
      source: a,
      target: b,
      dependency: dependency(a, b),
      frequency: follows[a * n + b],
    });
  };

  for (let a = 0; a < n; a++) {
    for (let b = 0; b < n; b++) {
      if (
        follows[a * n + b] >= options.minFrequency &&
        dependency(a, b) >= options.dependencyThreshold
      ) {
        accept(a, b);
      }
    }
  }

  if (connectAll) {
    const best = (a: number, forward: boolean): number => {
      let pick = -1;
      let pickDep = 0;
      for (let b = 0; b < n; b++) {
        if (b === a) continue;
        const freq = forward ? follows[a * n + b] : follows[b * n + a];
        if (freq === 0) continue;
        const dep = forward ? dependency(a, b) : dependency(b, a);
        if (pick === -1 || dep > pickDep) {
          pick = b;
          pickDep = dep;
        }
      }
      return pick !== -1 && pickDep > 0 ? pick : -1;
    };

    const hasOut = new Array<boolean>(n).fill(false);
    const hasIn = new Array<boolean>(n).fill(false);
    for (const key of accepted) {
      hasOut[Math.floor(key / n)] = true;
      hasIn[key % n] = true;
    }

    for (let a = 0; a < n; a++) {
      if (!hasOut[a] && !isEnd[a]) {
        const b = best(a, true);
        if (b !== -1) accept(a, b);
      }
      if (!hasIn[a] && !isStart[a]) {
        const b = best(a, false);
        if (b !== -1) accept(b, a);
      }
    }
  }

  edges.sort((x, y) => x.source - y.source || x.target - y.target);
  return { edges, start_activities: starts, end_activities: ends };
}
