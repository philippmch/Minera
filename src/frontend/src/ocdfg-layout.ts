import { type OcelDiscoveryResult } from "./types";

/** The activity-side metrics exposed by the object-centric graph export. */
export type OcdfgActivityMetric =
  | "events"
  | "unique_objects"
  | "event_objects"
  | "total_objects";

/** The edge-side metrics exposed by the object-centric graph export. */
export type OcdfgEdgeMetric =
  | "event_couples"
  | "unique_objects"
  | "event_objects"
  | "total_objects"
  | "performance";

export type OcdfgPerformanceAggregation = "mean" | "median" | "min" | "max";
export type OcdfgRankDirection = "TB" | "BT" | "LR" | "RL";

/**
 * A deliberately loose structural view of the validated JSON graph.  The
 * public result envelope currently keeps `result` as a generic record, while
 * the graph serializer has a stable shape.  Keeping this view local lets the
 * renderer tolerate older exports without weakening the shared result
 * validator.
 */
export interface OcdfgActivityMetricData {
  events?: unknown;
  unique_objects?: unknown;
  event_objects?: unknown;
  total_objects?: unknown;
}

export interface OcdfgEdgeMetricData extends OcdfgActivityMetricData {
  source_activity?: unknown;
  target_activity?: unknown;
  event_couples?: unknown;
  performance?: unknown;
  performance_event_couples?: unknown;
  performance_total_objects?: unknown;
}

export interface OcdfgGraphData {
  activities?: unknown;
  object_types?: unknown;
  activities_indep?: unknown;
  activities_ot?: unknown;
  edges?: unknown;
  performance_computed?: unknown;
}

export type OcdfgInput = OcelDiscoveryResult | OcdfgGraphData;

export interface OcdfgLayoutOptions {
  activityMetric?: OcdfgActivityMetric;
  edgeMetric?: OcdfgEdgeMetric;
  activityThreshold?: number;
  edgeThreshold?: number;
  performanceAggregation?: OcdfgPerformanceAggregation;
  rankDirection?: OcdfgRankDirection;
  /** Maximum number of visible activities. Defaults to MAX_NODES. */
  maxNodes?: number;
  /** Maximum number of visible edges. Defaults to MAX_EDGES. */
  maxEdges?: number;
  /** Force labels on/off; otherwise labels are bounded for dense graphs. */
  annotations?: boolean;
}

export const OCDFG_MAX_NODES = 80;
export const OCDFG_MAX_EDGES = 240;

export interface OcdfgNodeLayout {
  id: number;
  name: string;
  /** Short label used inside the SVG box; the full name is in the title. */
  displayName: string;
  metric: number;
  metricText: string;
  x: number;
  y: number;
  w: number;
  h: number;
  rank: number;
  objectTypes: string[];
}

export interface OcdfgEdgeLayout {
  key: string;
  source: number;
  target: number;
  sourceName: string;
  targetName: string;
  metric: number;
  metricText: string;
  metricAvailable: boolean;
  objectTypes: string[];
  path: string;
  headPoints: string;
  label: { x: number; y: number };
  width: number;
  opacity: number;
  back: boolean;
}

export interface OcdfgLayout {
  nodes: OcdfgNodeLayout[];
  edges: OcdfgEdgeLayout[];
  fit: { x: number; y: number; w: number; h: number };
  activityMetric: OcdfgActivityMetric;
  edgeMetric: OcdfgEdgeMetric;
  performanceAggregation: OcdfgPerformanceAggregation;
  rankDirection: OcdfgRankDirection;
  activityMetricLabel: string;
  edgeMetricLabel: string;
  activityThreshold: number;
  edgeThreshold: number;
  showLabels: boolean;
  inputActivityCount: number;
  inputEdgeCount: number;
  truncatedActivities: boolean;
  truncatedEdges: boolean;
}

interface Point {
  x: number;
  y: number;
}

interface MergedEdge {
  source: string;
  target: string;
  objectTypes: Set<string>;
  metric: {
    event_couples: unknown[];
    unique_objects: unknown[];
    event_objects: unknown[];
    total_objects: unknown[];
    performance: number[];
  };
}

interface CandidateEdge {
  source: string;
  target: string;
  objectTypes: string[];
  value: number | null;
}

const ACTIVITY_LABELS: Record<OcdfgActivityMetric, string> = {
  events: "events",
  unique_objects: "unique objects",
  event_objects: "event-object pairs",
  total_objects: "total-object pairs",
};

const EDGE_LABELS: Record<Exclude<OcdfgEdgeMetric, "performance">, string> = {
  event_couples: "event couples",
  unique_objects: "unique objects",
  event_objects: "event-object triples",
  total_objects: "total-object triples",
};

const ACTIVITY_METRICS: readonly OcdfgActivityMetric[] = [
  "events", "unique_objects", "event_objects", "total_objects",
];
const EDGE_METRICS: readonly OcdfgEdgeMetric[] = [
  "event_couples", "unique_objects", "event_objects", "total_objects", "performance",
];
const PERFORMANCE_AGGREGATIONS: readonly OcdfgPerformanceAggregation[] = [
  "mean", "median", "min", "max",
];
const RANK_DIRECTIONS: readonly OcdfgRankDirection[] = ["TB", "BT", "LR", "RL"];

function isRecord(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}

function asArray(value: unknown): unknown[] {
  return Array.isArray(value) ? value : [];
}

function asRecord(value: unknown): Record<string, unknown> {
  return isRecord(value) ? value : {};
}

function asMetric(value: unknown): OcdfgActivityMetricData {
  return isRecord(value) ? value as OcdfgActivityMetricData : {};
}

function asEdgeMetric(value: unknown): OcdfgEdgeMetricData {
  return isRecord(value) ? value as OcdfgEdgeMetricData : {};
}

function stringValue(value: unknown): string | null {
  return typeof value === "string" && value.length > 0 ? value : null;
}

function uniqueKey(value: unknown): string {
  if (typeof value === "string") return `s:${value}`;
  if (typeof value === "number" && Number.isFinite(value)) return `n:${value}`;
  try {
    return `j:${JSON.stringify(value)}`;
  } catch {
    return `x:${String(value)}`;
  }
}

function uniqueValues(values: readonly unknown[]): unknown[] {
  const seen = new Set<string>();
  const result: unknown[] = [];
  for (const value of values) {
    const key = uniqueKey(value);
    if (seen.has(key)) continue;
    seen.add(key);
    result.push(value);
  }
  return result;
}

function uniqueStrings(values: unknown): string[] {
  const result = new Set<string>();
  for (const value of asArray(values)) {
    const text = stringValue(value);
    if (text !== null) result.add(text);
  }
  return [...result].sort(compareText);
}

function compareText(a: string, b: string): number {
  return a < b ? -1 : a > b ? 1 : 0;
}

function finiteOrNull(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function safeNonNegative(value: number | null): number {
  return value !== null && Number.isFinite(value) ? Math.max(0, value) : 0;
}

function arrayForMetric(metric: OcdfgActivityMetricData, name: Exclude<OcdfgActivityMetric, "events">): unknown[] {
  const selected = asArray(metric[name]);
  if (selected.length > 0 || metric[name] !== undefined) return selected;
  // Older graph exports did not always carry the explicit total-object field.
  if (name === "total_objects") return asArray(metric.event_objects);
  return selected;
}

function countMetric(metric: OcdfgActivityMetricData, name: OcdfgActivityMetric): number {
  if (name === "events") return uniqueValues(asArray(metric.events)).length;
  return uniqueValues(arrayForMetric(metric, name)).length;
}

function mergedCollection(metric: MergedEdge["metric"], name: Exclude<OcdfgEdgeMetric, "performance">): unknown[] {
  return uniqueValues(metric[name]);
}

function performanceSamples(metric: OcdfgEdgeMetricData): number[] {
  // The compact `performance` field is the canonical total-object projection
  // emitted by the backend.  Specialized projections are compatibility
  // fallbacks for older or hand-authored graph payloads.
  const candidates = [
    metric.performance,
    metric.performance_total_objects,
    metric.performance_event_couples,
  ];
  for (const candidate of candidates) {
    if (candidate === undefined) continue;
    const values = asArray(candidate)
      .map(finiteOrNull)
      .filter((value): value is number => value !== null);
    // Prefer the first non-empty projection, but allow a legacy field to
    // supply samples when a newer projection is present but empty.
    if (values.length > 0) return values;
  }
  return [];
}

/** Aggregate finite performance samples without mutating the source array. */
export function aggregateOcdfgPerformance(
  samples: readonly unknown[],
  aggregation: OcdfgPerformanceAggregation = "mean",
): number | null {
  const values = samples
    .map(finiteOrNull)
    .filter((value): value is number => value !== null);
  if (values.length === 0) return null;
  if (aggregation === "min") {
    let minimum = values[0];
    for (let index = 1; index < values.length; ++index) minimum = Math.min(minimum, values[index]);
    return minimum;
  }
  if (aggregation === "max") {
    let maximum = values[0];
    for (let index = 1; index < values.length; ++index) maximum = Math.max(maximum, values[index]);
    return maximum;
  }
  if (aggregation === "median") {
    const sorted = [...values].sort((a, b) => a - b);
    const middle = Math.floor(sorted.length / 2);
    const median = sorted.length % 2 === 0
      ? (sorted[middle - 1] + sorted[middle]) / 2
      : sorted[middle];
    return Number.isFinite(median) ? median : null;
  }
  const sum = values.reduce((total, value) => total + value, 0);
  return Number.isFinite(sum) ? sum / values.length : null;
}

export function ocdfgActivityMetricLabel(metric: OcdfgActivityMetric): string {
  return ACTIVITY_LABELS[metric];
}

export function ocdfgEdgeMetricLabel(
  metric: OcdfgEdgeMetric,
  aggregation: OcdfgPerformanceAggregation = "mean",
): string {
  return metric === "performance"
    ? `${aggregation} performance`
    : EDGE_LABELS[metric];
}

/** Friendly aliases for callers that prefer the shorter names. */
export const activityMetricLabel = ocdfgActivityMetricLabel;
export const edgeMetricLabel = ocdfgEdgeMetricLabel;

function unwrapGraph(input: OcdfgInput): OcdfgGraphData {
  if (isRecord(input) && input.family === "OCEL_DISCOVERY" && isRecord(input.result)) {
    return input.result as OcdfgGraphData;
  }
  return input as OcdfgGraphData;
}

function normalizedLimit(value: number | undefined, fallback: number): number {
  if (value === undefined || !Number.isFinite(value)) return fallback;
  return Math.max(0, Math.min(Math.floor(value), fallback));
}

function normalizedThreshold(value: number | undefined): number {
  return value !== undefined && Number.isFinite(value) ? Math.max(0, value) : 0;
}

function activityMetrics(graph: OcdfgGraphData): Record<string, OcdfgActivityMetricData> {
  const result: Record<string, OcdfgActivityMetricData> = {};
  const independent = asRecord(graph.activities_indep);
  for (const [name, metric] of Object.entries(independent)) result[name] = asMetric(metric);
  // If an older export omitted the independent projection, retain a useful
  // aggregate from the per-object-type projection instead.
  if (Object.keys(result).length === 0) {
    const byType = asRecord(graph.activities_ot);
    for (const activities of Object.values(byType)) {
      for (const [name, rawMetric] of Object.entries(asRecord(activities))) {
        const target = result[name] ?? (result[name] = {});
        const metric = asMetric(rawMetric);
        for (const field of ["events", "unique_objects", "event_objects", "total_objects"] as const) {
          const values = uniqueValues([
            ...asArray(target[field]),
            ...asArray(metric[field]),
          ]);
          target[field] = values;
        }
      }
    }
  }
  return result;
}

function edgeSourceTarget(key: string, metric: OcdfgEdgeMetricData): [string, string] | null {
  const source = stringValue(metric.source_activity);
  const target = stringValue(metric.target_activity);
  if (source !== null && target !== null) return [source, target];
  const separator = key.indexOf("->");
  if (separator <= 0 || separator + 2 >= key.length) return null;
  const fallbackSource = key.slice(0, separator);
  const fallbackTarget = key.slice(separator + 2);
  return fallbackSource && fallbackTarget ? [fallbackSource, fallbackTarget] : null;
}

function collectEdges(graph: OcdfgGraphData): MergedEdge[] {
  const merged = new Map<string, MergedEdge>();
  const edgesByType = asRecord(graph.edges);
  const typeEntries = Object.entries(edgesByType).sort(([a], [b]) => compareText(a, b));
  for (const [objectType, rawEdges] of typeEntries) {
    const edgeEntries = Object.entries(asRecord(rawEdges)).sort(([a], [b]) => compareText(a, b));
    for (const [key, rawMetric] of edgeEntries) {
      const metric = asEdgeMetric(rawMetric);
      const pair = edgeSourceTarget(key, metric);
      if (pair === null) continue;
      const [source, target] = pair;
      const mergedKey = `${source}\u0000${target}`;
      let destination = merged.get(mergedKey);
      if (!destination) {
        destination = {
          source,
          target,
          objectTypes: new Set<string>(),
          metric: {
            event_couples: [],
            unique_objects: [],
            event_objects: [],
            total_objects: [],
            performance: [],
          },
        };
        merged.set(mergedKey, destination);
      }
      if (objectType.length > 0) destination.objectTypes.add(objectType);
      for (const field of ["event_couples", "unique_objects", "event_objects", "total_objects"] as const) {
        destination.metric[field].push(...asArray(metric[field]));
        if (field === "total_objects" && metric[field] === undefined) {
          destination.metric[field].push(...asArray(metric.event_objects));
        }
      }
      destination.metric.performance.push(...performanceSamples(metric));
    }
  }
  return [...merged.values()].sort((a, b) =>
    compareText(a.source, b.source) || compareText(a.target, b.target));
}

function edgeValue(
  edge: MergedEdge,
  metric: OcdfgEdgeMetric,
  aggregation: OcdfgPerformanceAggregation,
): number | null {
  if (metric === "performance") return aggregateOcdfgPerformance(edge.metric.performance, aggregation);
  return uniqueValues(mergedCollection(edge.metric, metric)).length;
}

function displayName(name: string): string {
  const max = 28;
  return name.length > max ? `${name.slice(0, max - 1)}…` : name;
}

function metricText(value: number | null, metric: OcdfgActivityMetric | OcdfgEdgeMetric): string {
  if (value === null || !Number.isFinite(value)) return "—";
  if (metric === "performance") {
    const rounded = Math.abs(value) <= Number.MAX_VALUE / 1000
      ? Math.round(value * 1000) / 1000
      : value;
    return `${Number.isFinite(rounded) ? rounded : "—"} s`;
  }
  const rounded = Math.max(0, Math.round(value));
  return String(rounded);
}

function cubicPoint(p0: Point, p1: Point, p2: Point, p3: Point, t: number): Point {
  const u = 1 - t;
  return {
    x: u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
    y: u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y,
  };
}

function finitePoint(point: Point): Point {
  return {
    x: Number.isFinite(point.x) ? point.x : 0,
    y: Number.isFinite(point.y) ? point.y : 0,
  };
}

function arrowPoints(end: Point, before: Point): string {
  const dx = end.x - before.x;
  const dy = end.y - before.y;
  const length = Math.hypot(dx, dy);
  if (!Number.isFinite(length) || length < 0.001) {
    return `${end.x},${end.y - 5} ${end.x + 5},${end.y + 5} ${end.x - 5},${end.y + 5}`;
  }
  const ux = dx / length;
  const uy = dy / length;
  const size = 8;
  const baseX = end.x - ux * size;
  const baseY = end.y - uy * size;
  const px = -uy * 4;
  const py = ux * 4;
  return `${end.x},${end.y} ${baseX + px},${baseY + py} ${baseX - px},${baseY - py}`;
}

interface Box {
  x: number;
  y: number;
  w: number;
  h: number;
}

function edgeGeometry(source: Box, target: Box, back: boolean, direction: OcdfgRankDirection): { path: string; headPoints: string; label: Point } {
  if (source.x === target.x && source.y === target.y) {
    const radius = 34;
    const p0 = { x: source.x + source.w / 2, y: source.y - source.h * 0.2 };
    const p1 = { x: source.x + source.w / 2 + radius, y: source.y - source.h * 0.9 };
    const p2 = { x: source.x + source.w / 2 + radius, y: source.y + source.h * 0.9 };
    const p3 = { x: source.x + source.w / 2, y: source.y + source.h * 0.2 };
    const mid = cubicPoint(p0, p1, p2, p3, 0.5);
    return {
      path: `M ${p0.x} ${p0.y} C ${p1.x} ${p1.y}, ${p2.x} ${p2.y}, ${p3.x} ${p3.y}`,
      headPoints: arrowPoints(p3, p2),
      label: { x: mid.x + 5, y: mid.y },
    };
  }

  const horizontal = direction === "LR" || direction === "RL";
  let p0: Point;
  let p3: Point;
  if (horizontal) {
    const sign = target.x >= source.x ? 1 : -1;
    p0 = { x: source.x + sign * source.w / 2, y: source.y };
    p3 = { x: target.x - sign * target.w / 2, y: target.y };
  } else {
    const sign = target.y >= source.y ? 1 : -1;
    p0 = { x: source.x, y: source.y + sign * source.h / 2 };
    p3 = { x: target.x, y: target.y - sign * target.h / 2 };
  }
  // Back edges get a modest orthogonal bow so cycles remain visible without
  // growing with graph size.  All geometry stays local to the two boxes.
  const bend = back ? 46 : Math.min(54, Math.max(22, Math.abs(horizontal ? p3.x - p0.x : p3.y - p0.y) * 0.35));
  const p1 = horizontal ? { x: p0.x + (p3.x >= p0.x ? bend : -bend), y: p0.y } : { x: p0.x, y: p0.y + (p3.y >= p0.y ? bend : -bend) };
  const p2 = horizontal ? { x: p3.x - (p3.x >= p0.x ? bend : -bend), y: p3.y } : { x: p3.x, y: p3.y - (p3.y >= p0.y ? bend : -bend) };
  const mid = finitePoint(cubicPoint(p0, p1, p2, p3, 0.5));
  return {
    path: `M ${p0.x} ${p0.y} C ${p1.x} ${p1.y}, ${p2.x} ${p2.y}, ${p3.x} ${p3.y}`,
    headPoints: arrowPoints(p3, p2),
    label: { x: mid.x + (horizontal ? 0 : 7), y: mid.y + (horizontal ? -6 : 0) },
  };
}

function rankNodes(names: string[], edges: CandidateEdge[]): { ranks: number[]; backKeys: Set<string> } {
  const ids = new Map(names.map((name, index) => [name, index]));
  const adjacency = names.map(() => [] as number[]);
  for (const edge of edges) {
    const source = ids.get(edge.source);
    const target = ids.get(edge.target);
    if (source === undefined || target === undefined) continue;
    adjacency[source].push(target);
  }
  for (const row of adjacency) row.sort((a, b) => a - b);
  const state = names.map(() => 0);
  const backKeys = new Set<string>();
  const visit = (source: number) => {
    state[source] = 1;
    for (const target of adjacency[source]) {
      const key = `${source}\u0000${target}`;
      if (state[target] === 1) backKeys.add(key);
      else if (state[target] === 0) visit(target);
    }
    state[source] = 2;
  };
  for (let id = 0; id < names.length; ++id) if (state[id] === 0) visit(id);

  const forward = names.map(() => [] as number[]);
  for (const edge of edges) {
    const source = ids.get(edge.source);
    const target = ids.get(edge.target);
    if (source === undefined || target === undefined || backKeys.has(`${source}\u0000${target}`)) continue;
    forward[source].push(target);
  }
  const indegree = forward.map(() => 0);
  for (const row of forward) for (const target of row) indegree[target]++;
  const queue = indegree.map((degree, id) => degree === 0 ? id : -1).filter((id) => id >= 0);
  const ranks = names.map(() => 0);
  for (let cursor = 0; cursor < queue.length; ++cursor) {
    const source = queue[cursor];
    for (const target of forward[source]) {
      ranks[target] = Math.max(ranks[target], ranks[source] + 1);
      if (--indegree[target] === 0) queue.push(target);
    }
  }
  return { ranks, backKeys };
}

function finiteLayoutNumber(value: number, fallback = 0): number {
  return Number.isFinite(value) ? value : fallback;
}

/** Build a deterministic, bounded SVG geometry model from a graph result. */
export function computeOcdfgLayout(input: OcdfgInput, options: OcdfgLayoutOptions = {}): OcdfgLayout {
  const graph = unwrapGraph(input);
  const requestedActivityMetric = options.activityMetric ?? "events";
  const requestedEdgeMetric = options.edgeMetric ?? "event_couples";
  const requestedAggregation = options.performanceAggregation ?? "mean";
  const activityMetric = ACTIVITY_METRICS.includes(requestedActivityMetric) ? requestedActivityMetric : "events";
  const edgeMetric = EDGE_METRICS.includes(requestedEdgeMetric) ? requestedEdgeMetric : "event_couples";
  const performanceAggregation = PERFORMANCE_AGGREGATIONS.includes(requestedAggregation) ? requestedAggregation : "mean";
  const requestedDirection = options.rankDirection ?? "TB";
  const rankDirection = RANK_DIRECTIONS.includes(requestedDirection) ? requestedDirection : "TB";
  const activityThreshold = normalizedThreshold(options.activityThreshold);
  const edgeThreshold = normalizedThreshold(options.edgeThreshold);
  const maxNodes = normalizedLimit(options.maxNodes, OCDFG_MAX_NODES);
  const maxEdges = normalizedLimit(options.maxEdges, OCDFG_MAX_EDGES);
  const metricMap = activityMetrics(graph);
  const listedActivities = uniqueStrings(graph.activities);
  const activityNames = listedActivities.length > 0
    ? listedActivities
    : Object.keys(metricMap).sort(compareText);
  const inputActivityCount = activityNames.length;

  const rankedActivities = activityNames
    .map((name) => ({ name, value: countMetric(metricMap[name] ?? {}, activityMetric) }))
    .filter((entry) => entry.value >= activityThreshold)
    .sort((a, b) => b.value - a.value || compareText(a.name, b.name));
  const selectedActivities = rankedActivities.slice(0, maxNodes);
  const selectedNames = new Set(selectedActivities.map((entry) => entry.name));
  const truncatedActivities = rankedActivities.length > selectedActivities.length;

  const mergedEdges = collectEdges(graph);
  const candidateEdges = mergedEdges
    .map((edge): CandidateEdge => ({
      source: edge.source,
      target: edge.target,
      objectTypes: [...edge.objectTypes].sort(compareText),
      value: edgeValue(edge, edgeMetric, performanceAggregation),
    }))
    .filter((edge) => edge.value !== null && edge.value >= edgeThreshold && selectedNames.has(edge.source) && selectedNames.has(edge.target))
    .sort((a, b) => (b.value! - a.value!) || compareText(a.source, b.source) || compareText(a.target, b.target) || compareText(a.objectTypes.join("\u0000"), b.objectTypes.join("\u0000")));
  const inputEdgeCount = mergedEdges.length;
  const selectedEdges = candidateEdges.slice(0, maxEdges);
  const truncatedEdges = candidateEdges.length > selectedEdges.length;

  // Put nodes into deterministic longest-path ranks after removing DFS back
  // edges.  This keeps cycles finite while retaining their visible return arc.
  const names = selectedActivities.map((entry) => entry.name).sort(compareText);
  const { ranks: rawRanks, backKeys } = rankNodes(names, selectedEdges);
  const rows = new Map<number, number[]>();
  for (let id = 0; id < names.length; ++id) (rows.get(rawRanks[id]) ?? (rows.set(rawRanks[id], []), rows.get(rawRanks[id])!)).push(id);

  const nodeWidths = names.map((name) => {
    const value = countMetric(metricMap[name] ?? {}, activityMetric);
    const textWidth = Math.max(
      name.length * 7.1,
      `${ocdfgActivityMetricLabel(activityMetric)}: ${value}`.length * 6.4,
    );
    return Math.max(96, Math.min(260, Math.ceil(textWidth + 30)));
  });
  const rowGap = 44;
  const horizontal = rankDirection === "LR" || rankDirection === "RL";
  const rowHeight = 46;
  const rankGap = 72;
  const rankOffsets = new Map<number, number>();
  let rankCursor = 0;
  const orderedRanks = [...rows.keys()].sort((a, b) => a - b);
  for (const rank of orderedRanks) {
    const row = rows.get(rank)!;
    rankOffsets.set(rank, rankCursor);
    // In horizontal layouts the node width is along the rank axis.  Advance
    // by the widest node in the rank so long labels cannot overlap the next
    // rank.  Vertical layouts use the fixed node height instead.
    const rankSize = horizontal
      ? Math.max(rowHeight, ...row.map((id) => nodeWidths[id]))
      : rowHeight;
    rankCursor += rankSize + rankGap;
  }
  const extentWidth = Math.max(
    96,
    ...[...rows.values()].map((row) =>
      row.reduce((sum, id) => sum + nodeWidths[id], 0) +
      rowGap * Math.max(0, row.length - 1)),
  );
  const rankExtent = Math.max(0, rankCursor - (rows.size > 0 ? rankGap : 0));
  const extentHeight = horizontal
    ? Math.max(rowHeight, ...[...rows.values()].map((row) =>
      row.length * rowHeight + rowGap * Math.max(0, row.length - 1)))
    : Math.max(rowHeight, rankExtent);
  const horizontalExtent = horizontal ? Math.max(96, rankExtent) : extentWidth;
  const coordinates = new Map<number, Point>();
  const boxes = new Map<number, Box>();
  for (const rank of orderedRanks) {
    const row = rows.get(rank)!;
    if (horizontal) {
      const totalHeight = row.length * rowHeight + rowGap * Math.max(0, row.length - 1);
      let y = (extentHeight - totalHeight) / 2;
      for (const id of row) {
        const rankSize = Math.max(rowHeight, ...row.map((candidate) => nodeWidths[candidate]));
        const offset = rankOffsets.get(rank) ?? 0;
        const x = offset + rankSize / 2;
        const point = {
          x: rankDirection === "RL" ? horizontalExtent - x : x,
          y: y + rowHeight / 2,
        };
        coordinates.set(id, point);
        boxes.set(id, { x: point.x, y: point.y, w: nodeWidths[id], h: rowHeight });
        y += rowHeight + rowGap;
      }
    } else {
      const rowWidth = row.reduce((sum, id) => sum + nodeWidths[id], 0) + rowGap * Math.max(0, row.length - 1);
      let x = (extentWidth - rowWidth) / 2;
      for (const id of row) {
        const width = nodeWidths[id];
        const y = (rankOffsets.get(rank) ?? 0) + rowHeight / 2;
        const point = {
          x: x + width / 2,
          y: rankDirection === "BT" ? extentHeight - y : y,
        };
        coordinates.set(id, point);
        boxes.set(id, { x: point.x, y: point.y, w: width, h: rowHeight });
        x += width + rowGap;
      }
    }
  }

  const idByName = new Map(names.map((name, id) => [name, id]));
  const maxMetric = Math.max(1, ...selectedEdges.map((edge) => safeNonNegative(edge.value)));
  const visibleEdges: OcdfgEdgeLayout[] = selectedEdges.map((edge) => {
    const source = idByName.get(edge.source)!;
    const target = idByName.get(edge.target)!;
    const back = backKeys.has(`${source}\u0000${target}`);
    const geometry = edgeGeometry(boxes.get(source)!, boxes.get(target)!, back, rankDirection);
    const numericValue = finiteLayoutNumber(edge.value ?? 0);
    const width = finiteLayoutNumber(1.1 + 3.1 * Math.sqrt(safeNonNegative(edge.value) / maxMetric), 1.1);
    return {
      key: `${edge.source}\u0000${edge.target}`,
      source,
      target,
      sourceName: edge.source,
      targetName: edge.target,
      metric: numericValue,
      metricText: metricText(edge.value, edgeMetric),
      metricAvailable: edge.value !== null && Number.isFinite(edge.value),
      objectTypes: edge.objectTypes,
      path: geometry.path,
      headPoints: geometry.headPoints,
      label: {
        x: finiteLayoutNumber(geometry.label.x),
        y: finiteLayoutNumber(geometry.label.y),
      },
      width,
      opacity: finiteLayoutNumber(0.52 + 0.44 * Math.sqrt(safeNonNegative(edge.value) / maxMetric), 0.52),
      back,
    };
  });

  const nodes: OcdfgNodeLayout[] = names.map((name, id) => {
    const value = countMetric(metricMap[name] ?? {}, activityMetric);
    const objectTypes = new Set<string>();
    for (const edge of selectedEdges) {
      if (edge.source === name || edge.target === name) for (const type of edge.objectTypes) objectTypes.add(type);
    }
    const box = boxes.get(id)!;
    const point = coordinates.get(id)!;
    return {
      id,
      name,
      displayName: displayName(name),
      metric: finiteLayoutNumber(value),
      metricText: metricText(value, activityMetric),
      x: finiteLayoutNumber(point.x),
      y: finiteLayoutNumber(point.y),
      w: finiteLayoutNumber(box.w, 96),
      h: finiteLayoutNumber(box.h, 46),
      rank: rawRanks[id],
      objectTypes: [...objectTypes].sort(compareText),
    };
  });

  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  for (const node of nodes) {
    minX = Math.min(minX, node.x - node.w / 2);
    maxX = Math.max(maxX, node.x + node.w / 2);
    minY = Math.min(minY, node.y - node.h / 2);
    maxY = Math.max(maxY, node.y + node.h / 2);
  }
  if (nodes.length === 0) {
    minX = -48;
    maxX = 48;
    minY = -32;
    maxY = 32;
  }
  const pad = 58;
  const fit = {
    x: finiteLayoutNumber(minX - pad, -100),
    y: finiteLayoutNumber(minY - pad, -100),
    w: finiteLayoutNumber(Math.max(1, maxX - minX + pad * 2), 200),
    h: finiteLayoutNumber(Math.max(1, maxY - minY + pad * 2), 180),
  };
  const showLabels = options.annotations ?? selectedEdges.length <= 80;
  return {
    nodes,
    edges: visibleEdges,
    fit,
    activityMetric,
    edgeMetric,
    performanceAggregation,
    rankDirection,
    activityMetricLabel: ocdfgActivityMetricLabel(activityMetric),
    edgeMetricLabel: ocdfgEdgeMetricLabel(edgeMetric, performanceAggregation),
    activityThreshold,
    edgeThreshold,
    showLabels,
    inputActivityCount,
    inputEdgeCount,
    truncatedActivities,
    truncatedEdges,
  };
}
