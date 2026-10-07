import { type ViewBox } from "./layout";
import { type PetriNetModel } from "./types";

export interface PetriPlaceLayout {
  id: number;
  kind: "source" | "sink" | "place";
  x: number;
  y: number;
  r: number;
}

export interface PetriTransitionLayout {
  id: number;
  label: string;
  silent: boolean;
  /** True for activities the Alpha Miner could not place (short loops). */
  disconnected: boolean;
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface PetriArcLayout {
  key: string;
  path: string;
  transition: number;
  place: number;
  input: boolean;
}

export interface PetriLayout {
  places: PetriPlaceLayout[];
  transitions: PetriTransitionLayout[];
  arcs: PetriArcLayout[];
  fit: ViewBox;
  /** Y of the divider above the unconnected lane, or null when none exist. */
  unconnectedY: number | null;
}

const COL_GAP = 78; // horizontal gap between the inner edges of adjacent layers
const ROW_H = 84; // vertical distance between nodes within a layer
const PLACE_R = 14;
const TRANS_H = 38;
const LANE_GAP = 88; // distance from the main net down to the unconnected lane

const measureCtx = document.createElement("canvas").getContext("2d")!;
const transFont = '600 12.5px "Familjen Grotesk", sans-serif';

function transitionWidth(label: string): number {
  measureCtx.font = transFont;
  return Math.max(72, Math.ceil(measureCtx.measureText(label).width) + 34);
}

interface ActivityLabels { activities: readonly { name: string }[]; }

function activityLabel(activity: number | null, data: ActivityLabels): string {
  return activity === null
    ? "τ"
    : (data.activities[activity]?.name ?? "?");
}

// A horizontal S-curve between two points, tangent to the layer axis at both
// ends so arcs read as a left-to-right flow.
function arcPath(x1: number, y1: number, x2: number, y2: number): string {
  const mx = (x1 + x2) / 2;
  return `M ${x1} ${y1} C ${mx} ${y1}, ${mx} ${y2}, ${x2} ${y2}`;
}

// Routes a long "skip" edge through a curved lane bottoming out near `floor`,
// well below the node band, so it clears every node it would otherwise cut
// across on a straight path. Control points are lifted so the curve actually
// reaches the lane depth at its lowest point.
function skipPath(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  floor: number,
): string {
  const cy = (floor - 0.125 * (y1 + y2)) / 0.75;
  const c1 = x1 + (x2 - x1) * 0.16;
  const c2 = x2 - (x2 - x1) * 0.16;
  return `M ${x1} ${y1} C ${c1} ${cy}, ${c2} ${cy}, ${x2} ${y2}`;
}

// Unified node used during layering; a place or a transition keyed uniquely.
interface Node {
  key: string;
  isPlace: boolean;
  ref: number; // place id or transition index
  halfW: number;
  halfH: number;
  layer: number;
  order: number;
  x: number;
  y: number;
}

export function computePetriLayout(
  net: PetriNetModel,
  data: ActivityLabels,
): PetriLayout {
  const placeKey = (id: number) => `p${id}`;
  const transKey = (index: number) => `t${index}`;

  // Transitions with no arcs are short-loop activities the Alpha Miner could
  // not place; keep them out of the flow layout and show them in a lane below.
  const connected = (index: number) =>
    net.transitions[index].inputs.length > 0 ||
    net.transitions[index].outputs.length > 0;

  const nodes = new Map<string, Node>();
  for (let id = 0; id < net.place_count; id++) {
    nodes.set(placeKey(id), {
      key: placeKey(id),
      isPlace: true,
      ref: id,
      halfW: PLACE_R,
      halfH: PLACE_R,
      layer: -1,
      order: 0,
      x: 0,
      y: 0,
    });
  }
  net.transitions.forEach((transition, index) => {
    if (!connected(index)) return;
    const label = activityLabel(transition.activity, data);
    const w = transition.activity === null ? TRANS_H : transitionWidth(label);
    nodes.set(transKey(index), {
      key: transKey(index),
      isPlace: false,
      ref: index,
      halfW: w / 2,
      halfH: TRANS_H / 2,
      layer: -1,
      order: 0,
      x: 0,
      y: 0,
    });
  });

  // Directed arcs: input places feed a transition, which feeds its output
  // places. Adjacency in both directions drives layering and crossing
  // reduction.
  const succ = new Map<string, string[]>();
  const pred = new Map<string, string[]>();
  const link = (from: string, to: string) => {
    (succ.get(from) ?? succ.set(from, []).get(from)!).push(to);
    (pred.get(to) ?? pred.set(to, []).get(to)!).push(from);
  };
  const arcs: { from: string; to: string }[] = [];
  net.transitions.forEach((transition, index) => {
    if (!connected(index)) return;
    for (const place of transition.inputs) {
      link(placeKey(place), transKey(index));
      arcs.push({ from: placeKey(place), to: transKey(index) });
    }
    for (const place of transition.outputs) {
      link(transKey(index), placeKey(place));
      arcs.push({ from: transKey(index), to: placeKey(place) });
    }
  });

  // Layer by longest path from the source so the sink lands to the right of
  // every activity that reaches it. Layering by shortest distance would pull
  // the sink left whenever a short branch (e.g. an early reject) reaches it
  // before the long main flow, forcing later transitions to send arcs back
  // leftward. Back edges (loops) are dropped from this pass via a DFS so cycles
  // cannot stall it; the net stays bipartite, so the parity of the longest path
  // still puts places on even layers and transitions on odd. Every arc is still
  // drawn — this only decides column order.
  const forwardSucc = new Map<string, string[]>();
  const onStack = new Set<string>();
  const visited = new Set<string>();
  const walk = (u: string) => {
    visited.add(u);
    onStack.add(u);
    for (const v of succ.get(u) ?? []) {
      if (onStack.has(v)) continue; // back edge: skip for layering
      (forwardSucc.get(u) ?? forwardSucc.set(u, []).get(u)!).push(v);
      if (!visited.has(v)) walk(v);
    }
    onStack.delete(u);
  };
  walk(placeKey(net.source));
  for (const key of nodes.keys()) if (!visited.has(key)) walk(key);

  // Topological order of the back-edge-free graph, then relax longest paths.
  const topo: string[] = [];
  const finished = new Set<string>();
  const order = (u: string) => {
    if (finished.has(u)) return;
    finished.add(u);
    for (const v of forwardSucc.get(u) ?? []) order(v);
    topo.push(u);
  };
  for (const key of nodes.keys()) order(key);
  topo.reverse();

  for (const node of nodes.values()) node.layer = 0;
  for (const u of topo) {
    const nu = nodes.get(u)!;
    for (const v of forwardSucc.get(u) ?? []) {
      const nv = nodes.get(v)!;
      if (nv.layer < nu.layer + 1) nv.layer = nu.layer + 1;
    }
  }

  const maxLayer = Math.max(0, ...[...nodes.values()].map((n) => n.layer));
  const layers: Node[][] = Array.from({ length: maxLayer + 1 }, () => []);
  for (const node of nodes.values()) layers[node.layer].push(node);
  for (const layer of layers) layer.sort((a, b) => a.ref - b.ref);

  // Barycenter sweeps: order each layer by the mean position of its neighbours
  // in the adjacent layer to pull connected nodes level and cut crossings.
  const setOrder = (layer: Node[]) =>
    layer.forEach((node, index) => (node.order = index));
  layers.forEach(setOrder);
  const barycenter = (node: Node, neighbours: Map<string, string[]>) => {
    const ns = neighbours.get(node.key) ?? [];
    if (ns.length === 0) return node.order;
    const sum = ns.reduce((acc, key) => acc + nodes.get(key)!.order, 0);
    return sum / ns.length;
  };
  for (let pass = 0; pass < 4; pass++) {
    for (let l = 1; l <= maxLayer; l++) {
      layers[l].sort((a, b) => barycenter(a, pred) - barycenter(b, pred));
      setOrder(layers[l]);
    }
    for (let l = maxLayer - 1; l >= 0; l--) {
      layers[l].sort((a, b) => barycenter(a, succ) - barycenter(b, succ));
      setOrder(layers[l]);
    }
  }

  // Width-aware columns: each layer's x accounts for the widest node in it and
  // its neighbour, so wide activity labels never overlap the next column.
  const layerHalfW = layers.map((layer) =>
    Math.max(PLACE_R, ...layer.map((n) => n.halfW)),
  );
  const layerX: number[] = [];
  for (let l = 0; l <= maxLayer; l++) {
    layerX[l] =
      l === 0
        ? 0
        : layerX[l - 1] + layerHalfW[l - 1] + COL_GAP + layerHalfW[l];
  }
  for (let l = 0; l <= maxLayer; l++) {
    const layer = layers[l];
    const offset = ((layer.length - 1) * ROW_H) / 2;
    layer.forEach((node, index) => {
      node.x = layerX[l];
      node.y = index * ROW_H - offset;
    });
  }

  const places: PetriPlaceLayout[] = [];
  const transitions: PetriTransitionLayout[] = [];
  for (const node of nodes.values()) {
    if (node.isPlace) {
      places.push({
        id: node.ref,
        kind:
          node.ref === net.source
            ? "source"
            : node.ref === net.sink
              ? "sink"
              : "place",
        x: node.x,
        y: node.y,
        r: node.halfW,
      });
    } else {
      const transition = net.transitions[node.ref];
      transitions.push({
        id: node.ref,
        label: activityLabel(transition.activity, data),
        silent: transition.activity === null,
        disconnected: false,
        x: node.x,
        y: node.y,
        w: node.halfW * 2,
        h: node.halfH * 2,
      });
    }
  }

  // Bounds of the connected net so far, used to seat the unconnected lane.
  const bounds = () => {
    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;
    for (const node of nodes.values()) {
      minX = Math.min(minX, node.x - node.halfW);
      maxX = Math.max(maxX, node.x + node.halfW);
      minY = Math.min(minY, node.y - node.halfH);
      maxY = Math.max(maxY, node.y + node.halfH);
    }
    return { minX, maxX, minY, maxY };
  };
  const b = bounds();

  // Skip edges span more than one column (e.g. an early-exit branch jumping to
  // the far-right sink). Drawn straight they cut through every node between
  // their ends, so route each through a curved lane below the net — a deeper
  // lane for a longer span so nested skip edges never overlap one another.
  const layerOf = (key: string) => nodes.get(key)!.layer;
  const SKIP_BASE = 46;
  const SKIP_STEP = 30;
  const skipFloor = new Map<number, number>();
  arcs
    .map((arc, index) => ({
      index,
      span: Math.abs(layerOf(arc.to) - layerOf(arc.from)),
    }))
    .filter(({ span }) => span > 1)
    .sort((p, q) => q.span - p.span)
    .forEach(({ index }, rank) =>
      skipFloor.set(index, b.maxY + SKIP_BASE + rank * SKIP_STEP),
    );
  const deepestSkip = Math.max(b.maxY, ...skipFloor.values());

  // Unconnected lane: short-loop transitions laid out in a row below everything.
  const orphans = net.transitions
    .map((transition, index) => ({ transition, index }))
    .filter(({ index }) => !connected(index));
  let unconnectedY: number | null = null;
  if (orphans.length > 0) {
    unconnectedY = deepestSkip + LANE_GAP * 0.55;
    const laneY = deepestSkip + LANE_GAP;
    let cursor = b.minX;
    for (const { transition, index } of orphans) {
      const label = activityLabel(transition.activity, data);
      const w = transition.activity === null ? TRANS_H : transitionWidth(label);
      transitions.push({
        id: index,
        label,
        silent: transition.activity === null,
        disconnected: true,
        x: cursor + w / 2,
        y: laneY,
        w,
        h: TRANS_H,
      });
      cursor += w + 24;
    }
  }

  // Distribute attachment points along each node's edge: every arc touching a
  // given side gets its own point, fanned out by the vertical position of its
  // other end. Without this, all arcs on a side stack on the edge midpoint, so
  // a returning arc's head lands exactly where the next arc starts (and the
  // arcs converging on the sink pile onto one spot).
  interface Attach {
    side: 1 | -1; // +1 right edge, -1 left edge
    index: number;
    isSource: boolean;
    otherY: number;
  }
  const attachments = new Map<string, Attach[]>();
  const addAttach = (key: string, att: Attach) =>
    (attachments.get(key) ?? attachments.set(key, []).get(key)!).push(att);
  arcs.forEach(({ from, to }, index) => {
    const a = nodes.get(from)!;
    const c = nodes.get(to)!;
    const sourceSide: 1 | -1 = c.x >= a.x ? 1 : -1;
    addAttach(from, { side: sourceSide, index, isSource: true, otherY: c.y });
    addAttach(to, {
      side: (-sourceSide) as 1 | -1,
      index,
      isSource: false,
      otherY: a.y,
    });
  });

  const srcPoint: { x: number; y: number }[] = new Array(arcs.length);
  const tgtPoint: { x: number; y: number }[] = new Array(arcs.length);
  for (const [key, list] of attachments) {
    const node = nodes.get(key)!;
    for (const side of [1, -1] as const) {
      const group = list
        .filter((att) => att.side === side)
        .sort((p, q) => p.otherY - q.otherY);
      const spread = Math.min(node.halfH * 0.8, 14);
      group.forEach((att, i) => {
        const t = group.length <= 1 ? 0 : (i / (group.length - 1)) * 2 - 1;
        let point: { x: number; y: number };
        if (node.isPlace) {
          // Sit the point on the circle's perimeter, fanned vertically.
          const dirY = t * 0.85;
          const norm = Math.hypot(side, dirY) || 1;
          point = {
            x: node.x + (side / norm) * node.halfW,
            y: node.y + (dirY / norm) * node.halfW,
          };
        } else {
          point = { x: node.x + side * node.halfW, y: node.y + t * spread };
        }
        if (att.isSource) srcPoint[att.index] = point;
        else tgtPoint[att.index] = point;
      });
    }
  }

  const arcLayouts: PetriArcLayout[] = arcs.map(({ from, to }, index) => {
    const s = srcPoint[index];
    const t = tgtPoint[index];
    const floor = skipFloor.get(index);
    const path =
      floor === undefined
        ? arcPath(s.x, s.y, t.x, t.y)
        : skipPath(s.x, s.y, t.x, t.y, floor);
    const input = from.startsWith("p");
    return {
      key: `${from}-${to}-${index}`, path, input,
      transition: Number((input ? to : from).slice(1)),
      place: Number((input ? from : to).slice(1)),
    };
  });

  const pad = 52;
  let minX = b.minX;
  let maxX = b.maxX;
  let maxY = Math.max(b.maxY, deepestSkip);
  for (const t of transitions) {
    if (!t.disconnected) continue;
    minX = Math.min(minX, t.x - t.w / 2);
    maxX = Math.max(maxX, t.x + t.w / 2);
    maxY = Math.max(maxY, t.y + t.h / 2);
  }
  const fit: ViewBox = {
    x: minX - pad,
    y: b.minY - pad,
    w: maxX - minX + pad * 2,
    h: maxY - b.minY + pad * 2,
  };

  return { places, transitions, arcs: arcLayouts, fit, unconnectedY };
}
