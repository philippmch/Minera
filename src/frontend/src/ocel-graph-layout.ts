import { type OcelDiscoveryResult } from "./types";

/** The two object-centric graph projections rendered by this module. */
export type OcelGraphKind = "etot" | "otg";
export type OcelGraphRankDirection = "TB" | "BT" | "LR" | "RL";

export interface EtotGraphData {
  activities?: unknown;
  object_types?: unknown;
  edges?: unknown;
}

export interface OtgGraphData {
  object_types?: unknown;
  edges?: unknown;
}

export type OcelGraphInput = OcelDiscoveryResult | EtotGraphData | OtgGraphData;

export interface OcelGraphLayoutOptions {
  kind?: OcelGraphKind;
  rankDirection?: OcelGraphRankDirection;
  frequencyThreshold?: number;
  maxNodes?: number;
  maxEdges?: number;
  annotations?: boolean;
}

export interface OcelGraphNodeLayout {
  id: number;
  name: string;
  displayName: string;
  kind: "activity" | "object_type";
  metric: number;
  metricText: string;
  x: number;
  y: number;
  w: number;
  h: number;
  rank: number;
}

export interface OcelGraphEdgeLayout {
  key: string;
  source: number;
  target: number;
  sourceName: string;
  targetName: string;
  relation: string;
  frequency: number;
  frequencyText: string;
  path: string;
  headPoints: string;
  label: { x: number; y: number };
  width: number;
  opacity: number;
  back: boolean;
}

export interface OcelGraphLayout {
  kind: OcelGraphKind;
  nodes: OcelGraphNodeLayout[];
  edges: OcelGraphEdgeLayout[];
  fit: { x: number; y: number; w: number; h: number };
  rankDirection: OcelGraphRankDirection;
  frequencyThreshold: number;
  inputNodeCount: number;
  inputEdgeCount: number;
  truncatedNodes: boolean;
  truncatedEdges: boolean;
  showLabels: boolean;
}

interface Point {
  x: number;
  y: number;
}

interface Box {
  x: number;
  y: number;
  w: number;
  h: number;
}

interface CandidateNode {
  key: string;
  name: string;
  kind: "activity" | "object_type";
  metric: number;
}

interface CandidateEdge {
  key: string;
  source: string;
  target: string;
  relation: string;
  frequency: number;
}

export const OCEL_GRAPH_MAX_NODES = 120;
export const OCEL_GRAPH_MAX_EDGES = 360;

function isRecord(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}

function asArray(value: unknown): unknown[] {
  return Array.isArray(value) ? value : [];
}

function asRecord(value: unknown): Record<string, unknown> {
  return isRecord(value) ? value : {};
}

function text(value: unknown): string | null {
  return typeof value === "string" && value.length > 0 ? value : null;
}

function finiteNonNegative(value: unknown): number {
  return typeof value === "number" && Number.isFinite(value)
    ? Math.max(0, value)
    : 0;
}

function compareText(a: string, b: string): number {
  return a < b ? -1 : a > b ? 1 : 0;
}

function displayName(name: string): string {
  const max = 28;
  return name.length > max ? `${name.slice(0, max - 1)}…` : name;
}

function normalizedLimit(value: number | undefined, fallback: number): number {
  if (value === undefined || !Number.isFinite(value)) return fallback;
  return Math.max(0, Math.min(Math.floor(value), fallback));
}

function normalizedThreshold(value: number | undefined): number {
  return value !== undefined && Number.isFinite(value) ? Math.max(0, value) : 0;
}

function unwrap(input: OcelGraphInput, requestedKind?: OcelGraphKind): {
  kind: OcelGraphKind;
  graph: EtotGraphData | OtgGraphData;
} {
  if (isRecord(input) && input.family === "OCEL_DISCOVERY" && isRecord(input.result)) {
    const kind = input.variant === "etot" || input.variant === "otg"
      ? input.variant
      : requestedKind ?? "etot";
    return { kind, graph: input.result as EtotGraphData | OtgGraphData };
  }
  return { kind: requestedKind ?? "etot", graph: input as EtotGraphData | OtgGraphData };
}

function numberText(value: number): string {
  return Number.isInteger(value) ? String(value) : String(Math.round(value * 1000) / 1000);
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

function cubicPoint(p0: Point, p1: Point, p2: Point, p3: Point, t: number): Point {
  const u = 1 - t;
  return {
    x: u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
    y: u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y,
  };
}

function edgeGeometry(source: Box, target: Box, back: boolean,
                     direction: OcelGraphRankDirection): {
  path: string;
  headPoints: string;
  label: Point;
} {
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
  const bend = back
    ? 46
    : Math.min(54, Math.max(22, Math.abs(horizontal ? p3.x - p0.x : p3.y - p0.y) * 0.35));
  const p1 = horizontal
    ? { x: p0.x + (p3.x >= p0.x ? bend : -bend), y: p0.y }
    : { x: p0.x, y: p0.y + (p3.y >= p0.y ? bend : -bend) };
  const p2 = horizontal
    ? { x: p3.x - (p3.x >= p0.x ? bend : -bend), y: p3.y }
    : { x: p3.x, y: p3.y - (p3.y >= p0.y ? bend : -bend) };
  const mid = cubicPoint(p0, p1, p2, p3, 0.5);
  return {
    path: `M ${p0.x} ${p0.y} C ${p1.x} ${p1.y}, ${p2.x} ${p2.y}, ${p3.x} ${p3.y}`,
    headPoints: arrowPoints(p3, p2),
    label: { x: mid.x + (horizontal ? 0 : 7), y: mid.y + (horizontal ? -6 : 0) },
  };
}

function rankNodes(nodes: CandidateNode[], edges: CandidateEdge[]): {
  ranks: number[];
  backKeys: Set<string>;
} {
  const ids = new Map(nodes.map((node, index) => [node.key, index]));
  const adjacency = nodes.map(() => [] as number[]);
  for (const edge of edges) {
    const source = ids.get(edge.source);
    const target = ids.get(edge.target);
    if (source === undefined || target === undefined) continue;
    adjacency[source].push(target);
  }
  for (const row of adjacency) row.sort((a, b) => a - b);
  const state = nodes.map(() => 0);
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
  for (let id = 0; id < nodes.length; ++id) if (state[id] === 0) visit(id);

  const forward = nodes.map(() => [] as number[]);
  for (const edge of edges) {
    const source = ids.get(edge.source);
    const target = ids.get(edge.target);
    if (source === undefined || target === undefined ||
        backKeys.has(`${source}\u0000${target}`)) continue;
    forward[source].push(target);
  }
  const indegree = forward.map(() => 0);
  for (const row of forward) for (const target of row) indegree[target]++;
  const queue = indegree.map((degree, id) => degree === 0 ? id : -1)
    .filter((id) => id >= 0);
  // A strongly connected component can leave no zero-indegree node after
  // removing DFS back edges. Starting those components at rank zero keeps the
  // geometry finite and still leaves the original edge visible as a return.
  if (queue.length === 0 && nodes.length > 0) queue.push(0);
  const ranks = nodes.map(() => 0);
  for (let cursor = 0; cursor < queue.length; ++cursor) {
    const source = queue[cursor];
    for (const target of forward[source]) {
      ranks[target] = Math.max(ranks[target], ranks[source] + 1);
      if (--indegree[target] === 0) queue.push(target);
    }
  }
  for (let id = 0; id < nodes.length; ++id) {
    if (indegree[id] > 0 && !queue.includes(id)) {
      queue.push(id);
      ranks[id] = 0;
    }
  }
  for (let cursor = 0; cursor < queue.length; ++cursor) {
    const source = queue[cursor];
    for (const target of forward[source]) {
      ranks[target] = Math.max(ranks[target], ranks[source] + 1);
      if (indegree[target] > 0) indegree[target] = 0;
    }
  }
  return { ranks, backKeys };
}

function graphCandidates(kind: OcelGraphKind, graph: EtotGraphData | OtgGraphData): {
  nodes: CandidateNode[];
  edges: CandidateEdge[];
} {
  const nodes = new Map<string, CandidateNode>();
  const edges: CandidateEdge[] = [];
  const ensure = (key: string, name: string, nodeKind: CandidateNode["kind"]) => {
    const existing = nodes.get(key);
    if (existing) return existing;
    const created: CandidateNode = { key, name, kind: nodeKind, metric: 0 };
    nodes.set(key, created);
    return created;
  };
  if (kind === "etot") {
    const etot = graph as EtotGraphData;
    for (const activity of asArray(etot.activities)) {
      const name = text(activity);
      if (name !== null) ensure(`activity:${name}`, name, "activity");
    }
    for (const objectType of asArray(etot.object_types)) {
      const name = text(objectType);
      if (name !== null) ensure(`object:${name}`, name, "object_type");
    }
    for (const raw of asArray(etot.edges)) {
      const edge = asRecord(raw);
      const activity = text(edge.activity);
      const objectType = text(edge.object_type);
      if (activity === null || objectType === null) continue;
      const source = ensure(`activity:${activity}`, activity, "activity");
      const target = ensure(`object:${objectType}`, objectType, "object_type");
      const frequency = finiteNonNegative(edge.frequency);
      source.metric += frequency;
      target.metric += frequency;
      edges.push({
        key: `${source.key}\u0000${target.key}`,
        source: source.key,
        target: target.key,
        relation: "event-to-object-type",
        frequency,
      });
    }
  } else {
    const otg = graph as OtgGraphData;
    for (const objectType of asArray(otg.object_types)) {
      const name = text(objectType);
      if (name !== null) ensure(`object:${name}`, name, "object_type");
    }
    for (const raw of asArray(otg.edges)) {
      const edge = asRecord(raw);
      const sourceType = text(edge.source_type);
      const targetType = text(edge.target_type);
      if (sourceType === null || targetType === null) continue;
      const source = ensure(`object:${sourceType}`, sourceType, "object_type");
      const target = ensure(`object:${targetType}`, targetType, "object_type");
      const frequency = finiteNonNegative(edge.frequency);
      source.metric += frequency;
      target.metric += frequency;
      edges.push({
        key: `${source.key}\u0000${edge.relation ?? ""}\u0000${target.key}`,
        source: source.key,
        target: target.key,
        relation: text(edge.relation) ?? "relation",
        frequency,
      });
    }
  }
  const deduplicated = new Map<string, CandidateEdge>();
  for (const edge of edges) {
    const existing = deduplicated.get(edge.key);
    if (existing) existing.frequency += edge.frequency;
    else deduplicated.set(edge.key, { ...edge });
  }
  return {
    nodes: [...nodes.values()].sort((a, b) => compareText(a.key, b.key)),
    edges: [...deduplicated.values()].sort((a, b) =>
      compareText(a.source, b.source) || compareText(a.target, b.target) ||
      compareText(a.relation, b.relation)),
  };
}

/** Build deterministic, bounded SVG geometry for an ETOT or OTG projection. */
export function computeOcelGraphLayout(
  input: OcelGraphInput,
  options: OcelGraphLayoutOptions = {},
): OcelGraphLayout {
  const { kind, graph } = unwrap(input, options.kind);
  const requestedDirection = options.rankDirection ?? "TB";
  const rankDirection: OcelGraphRankDirection =
    ["TB", "BT", "LR", "RL"].includes(requestedDirection)
      ? requestedDirection
      : "TB";
  const frequencyThreshold = normalizedThreshold(options.frequencyThreshold);
  const maxNodes = normalizedLimit(options.maxNodes, OCEL_GRAPH_MAX_NODES);
  const maxEdges = normalizedLimit(options.maxEdges, OCEL_GRAPH_MAX_EDGES);
  const candidates = graphCandidates(kind, graph);
  const rankedNodes = candidates.nodes
    .filter((node) => node.metric >= 0)
    .sort((a, b) => b.metric - a.metric || compareText(a.key, b.key));
  // Select by metric first, then restore lexical key order so node ids and
  // serialized geometry do not depend on the ranking scores.
  const selectedNodes = rankedNodes.slice(0, maxNodes)
    .sort((a, b) => compareText(a.key, b.key));
  const selectedKeys = new Set(selectedNodes.map((node) => node.key));
  const truncatedNodes = rankedNodes.length > selectedNodes.length;
  const rankedEdges = candidates.edges
    .filter((edge) => edge.frequency >= frequencyThreshold &&
      selectedKeys.has(edge.source) && selectedKeys.has(edge.target))
    .sort((a, b) => b.frequency - a.frequency || compareText(a.key, b.key));
  const selectedEdges = rankedEdges.slice(0, maxEdges);
  const truncatedEdges = rankedEdges.length > selectedEdges.length;
  const nodeByKey = new Map(selectedNodes.map((node) => [node.key, node]));
  const { ranks: rawRanks, backKeys } = rankNodes(selectedNodes, selectedEdges);

  const nodeWidths = selectedNodes.map((node) => {
    const labelWidth = Math.max(node.name.length * 7.1,
      `frequency: ${numberText(node.metric)}`.length * 6.4);
    return Math.max(96, Math.min(260, Math.ceil(labelWidth + 30)));
  });
  const rowGap = 44;
  const rowHeight = 46;
  const rankGap = 72;
  const horizontal = rankDirection === "LR" || rankDirection === "RL";
  const rows = new Map<number, number[]>();
  for (let id = 0; id < selectedNodes.length; ++id) {
    const row = rows.get(rawRanks[id]);
    if (row) row.push(id);
    else rows.set(rawRanks[id], [id]);
  }
  const orderedRanks = [...rows.keys()].sort((a, b) => a - b);
  const rankOffsets = new Map<number, number>();
  let rankCursor = 0;
  for (const rank of orderedRanks) {
    const row = rows.get(rank)!;
    rankOffsets.set(rank, rankCursor);
    const rankSize = horizontal
      ? Math.max(rowHeight, ...row.map((id) => nodeWidths[id]))
      : rowHeight;
    rankCursor += rankSize + rankGap;
  }
  const extentWidth = Math.max(96, ...[...rows.values()].map((row) =>
    row.reduce((sum, id) => sum + nodeWidths[id], 0) +
    rowGap * Math.max(0, row.length - 1)));
  const rankExtent = Math.max(0, rankCursor - (rows.size > 0 ? rankGap : 0));
  const extentHeight = horizontal
    ? Math.max(rowHeight, ...[...rows.values()].map((row) =>
      row.length * rowHeight + rowGap * Math.max(0, row.length - 1)))
    : Math.max(rowHeight, rankExtent);
  const horizontalExtent = horizontal ? Math.max(96, rankExtent) : extentWidth;
  const boxes = new Map<number, Box>();
  for (const rank of orderedRanks) {
    const row = rows.get(rank)!;
    if (horizontal) {
      const totalHeight = row.length * rowHeight + rowGap * Math.max(0, row.length - 1);
      let y = (extentHeight - totalHeight) / 2;
      const rankSize = Math.max(rowHeight, ...row.map((id) => nodeWidths[id]));
      for (const id of row) {
        const x = (rankOffsets.get(rank) ?? 0) + rankSize / 2;
        const point = { x: rankDirection === "RL" ? horizontalExtent - x : x,
          y: y + rowHeight / 2 };
        boxes.set(id, { x: point.x, y: point.y, w: nodeWidths[id], h: rowHeight });
        y += rowHeight + rowGap;
      }
    } else {
      const rowWidth = row.reduce((sum, id) => sum + nodeWidths[id], 0) +
        rowGap * Math.max(0, row.length - 1);
      let x = (extentWidth - rowWidth) / 2;
      for (const id of row) {
        const width = nodeWidths[id];
        const y = (rankOffsets.get(rank) ?? 0) + rowHeight / 2;
        const point = { x: x + width / 2,
          y: rankDirection === "BT" ? extentHeight - y : y };
        boxes.set(id, { x: point.x, y: point.y, w: width, h: rowHeight });
        x += width + rowGap;
      }
    }
  }
  const maxFrequency = Math.max(1, ...selectedEdges.map((edge) => edge.frequency));
  const nodeIndex = new Map(selectedNodes.map((node, index) => [node.key, index]));
  const edges: OcelGraphEdgeLayout[] = selectedEdges.map((edge) => {
    const source = nodeIndex.get(edge.source)!;
    const target = nodeIndex.get(edge.target)!;
    const back = backKeys.has(`${source}\u0000${target}`);
    const geometry = edgeGeometry(boxes.get(source)!, boxes.get(target)!, back, rankDirection);
    return {
      key: edge.key,
      source,
      target,
      sourceName: nodeByKey.get(edge.source)!.name,
      targetName: nodeByKey.get(edge.target)!.name,
      relation: edge.relation,
      frequency: edge.frequency,
      frequencyText: numberText(edge.frequency),
      path: geometry.path,
      headPoints: geometry.headPoints,
      label: geometry.label,
      width: 1.1 + 3.1 * Math.sqrt(edge.frequency / maxFrequency),
      opacity: 0.52 + 0.44 * Math.sqrt(edge.frequency / maxFrequency),
      back,
    };
  });
  const nodes: OcelGraphNodeLayout[] = selectedNodes.map((node, id) => {
    const box = boxes.get(id) ?? { x: 0, y: 0, w: 96, h: rowHeight };
    return {
      id,
      name: node.name,
      displayName: displayName(node.name),
      kind: node.kind,
      metric: node.metric,
      metricText: numberText(node.metric),
      x: box.x,
      y: box.y,
      w: box.w,
      h: box.h,
      rank: rawRanks[id] ?? 0,
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
  return {
    kind,
    nodes,
    edges,
    fit: {
      x: minX - pad,
      y: minY - pad,
      w: Math.max(1, maxX - minX + pad * 2),
      h: Math.max(1, maxY - minY + pad * 2),
    },
    rankDirection,
    frequencyThreshold,
    inputNodeCount: candidates.nodes.length,
    inputEdgeCount: candidates.edges.length,
    truncatedNodes,
    truncatedEdges,
    showLabels: options.annotations ?? selectedEdges.length <= 100,
  };
}
