import { type MineraExport, START, END, numberFormat } from "./types";

export interface NodeLayout {
  id: number;
  name: string;
  count: number;
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface TerminalLayout {
  id: number;
  label: string;
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface EdgeLayout {
  key: string;
  source: number;
  target: number;
  count: number;
  back: boolean;
  path: string;
  headPoints: string;
  label: { x: number; y: number };
  width: number;
  opacity: number;
}

export interface ViewBox {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface GraphLayout {
  nodes: NodeLayout[];
  terminals: TerminalLayout[];
  edges: EdgeLayout[];
  fit: ViewBox;
  showLabels: boolean;
}

interface Box {
  x: number;
  y: number;
  w: number;
  h: number;
}

interface WorkEdge {
  source: number;
  target: number;
  count: number;
  back?: boolean;
  x1?: number;
  y1?: number;
  x2?: number;
  y2?: number;
}

interface Point {
  x: number;
  y: number;
}

const NODE_H = 46;
const TERMINAL_H = 24;
const GAP_X = 42;
const GAP_Y = 86;

const measureCtx = document.createElement("canvas").getContext("2d")!;

function textWidth(text: string, font: string): number {
  measureCtx.font = font;
  return measureCtx.measureText(text).width;
}

function cubicPoint(p0: Point, p1: Point, p2: Point, p3: Point, t: number): Point {
  const u = 1 - t;
  return {
    x: u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
    y: u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y,
  };
}

function edgeGeometry(
  edge: WorkEdge,
  boxes: Map<number, Box>,
  backIndex: number,
  maxX: number,
): { p0: Point; p1: Point; p2: Point; p3: Point; label: Point } {
  if (edge.source === edge.target) {
    const box = boxes.get(edge.source)!;
    const x = box.x + box.w / 2;
    const r = 28;
    return {
      p0: { x, y: box.y - box.h * 0.28 },
      p1: { x: x + r, y: box.y - box.h * 0.62 },
      p2: { x: x + r, y: box.y + box.h * 0.62 },
      p3: { x, y: box.y + box.h * 0.28 },
      label: { x: x + r * 0.82, y: box.y },
    };
  }

  const src = boxes.get(edge.source)!;
  const dst = boxes.get(edge.target)!;

  if (edge.back) {
    // Rework loops sweep around the right margin of the whole graph so they
    // never cut through same-layer nodes.
    const bulge = maxX + 64 + backIndex * 34;
    const p0 = { x: src.x + src.w * 0.25, y: src.y + src.h / 2 };
    const p3 = { x: dst.x + dst.w * 0.25, y: dst.y - dst.h / 2 };
    const p1 = { x: bulge, y: p0.y + 64 };
    const p2 = { x: bulge, y: p3.y - 64 };
    const mid = cubicPoint(p0, p1, p2, p3, 0.5);
    return { p0, p1, p2, p3, label: { x: mid.x + 8, y: mid.y } };
  }

  const p0 = { x: edge.x1!, y: edge.y1! };
  const p3 = { x: edge.x2!, y: edge.y2! };
  const stretch = Math.min(52, Math.abs(p3.y - p0.y) * 0.45);
  const p1 = { x: p0.x, y: p0.y + stretch };
  const p2 = { x: p3.x, y: p3.y - stretch };
  const mid = cubicPoint(p0, p1, p2, p3, 0.5);
  return { p0, p1, p2, p3, label: { x: mid.x + 7, y: mid.y } };
}

export function edgeKey(source: number, target: number): string {
  return `${source}>${target}`;
}

export function computeLayout(data: MineraExport): GraphLayout {
  const ids = [START, ...data.activities.map((_, id) => id), END];

  const edges: WorkEdge[] = data.edges.map((edge) => ({ ...edge }));
  data.activities.forEach((activity, id) => {
    if (activity.start_count > 0) {
      edges.push({ source: START, target: id, count: activity.start_count });
    }
    if (activity.end_count > 0) {
      edges.push({ source: id, target: END, count: activity.end_count });
    }
  });

  // Forward adjacency; back edges (cycles) are detected with a DFS and
  // excluded from layering, then drawn as side loops.
  const out = new Map<number, WorkEdge[]>(ids.map((id) => [id, []]));
  for (const edge of edges) {
    if (edge.source !== edge.target) out.get(edge.source)!.push(edge);
  }

  const color = new Map<number, number>(ids.map((id) => [id, 0]));
  const finish: number[] = [];
  const dfs = (root: number) => {
    const stack: [number, number][] = [[root, 0]];
    color.set(root, 1);
    while (stack.length > 0) {
      const frame = stack[stack.length - 1];
      const targets = out.get(frame[0])!;
      if (frame[1] < targets.length) {
        const edge = targets[frame[1]++];
        const next = edge.target;
        if (color.get(next) === 1) {
          edge.back = true;
        } else if (color.get(next) === 0) {
          color.set(next, 1);
          stack.push([next, 0]);
        }
      } else {
        color.set(frame[0], 2);
        finish.push(frame[0]);
        stack.pop();
      }
    }
  };
  dfs(START);
  for (const id of ids) {
    if (color.get(id) === 0) dfs(id);
  }

  // Longest-path layering over forward edges, in topological order.
  const layer = new Map<number, number>(ids.map((id) => [id, 0]));
  finish.reverse();
  for (const id of finish) {
    for (const edge of out.get(id)!) {
      if (edge.back || edge.target === END) continue;
      layer.set(edge.target, Math.max(layer.get(edge.target)!, layer.get(id)! + 1));
    }
  }
  let maxLayer = 0;
  for (const id of ids) {
    if (id !== END) maxLayer = Math.max(maxLayer, layer.get(id)!);
  }
  layer.set(END, maxLayer + 1);

  // Group into layers and order with a few barycenter sweeps.
  const layers: number[][] = [];
  for (const id of ids) {
    const depth = layer.get(id)!;
    (layers[depth] ??= []).push(id);
  }
  const neighbors = new Map<number, number[]>(ids.map((id) => [id, []]));
  for (const edge of edges) {
    if (edge.source === edge.target) continue;
    neighbors.get(edge.source)!.push(edge.target);
    neighbors.get(edge.target)!.push(edge.source);
  }
  const pos = new Map<number, number>();
  const reindex = () => {
    for (const row of layers) row.forEach((id, i) => pos.set(id, i));
  };
  reindex();
  const sortRow = (row: number[], refLayer: number) => {
    row.sort((a, b) => {
      const mean = (id: number) => {
        const refs = neighbors.get(id)!.filter((n) => layer.get(n) === refLayer);
        if (refs.length === 0) return pos.get(id)!;
        return refs.reduce((sum, n) => sum + pos.get(n)!, 0) / refs.length;
      };
      return mean(a) - mean(b);
    });
  };
  for (let sweep = 0; sweep < 4; ++sweep) {
    for (let i = 1; i < layers.length; ++i) {
      sortRow(layers[i], i - 1);
      reindex();
    }
    for (let i = layers.length - 2; i >= 0; --i) {
      sortRow(layers[i], i + 1);
      reindex();
    }
  }

  // Coordinates.
  const nameFont = '600 12.5px "Familjen Grotesk", sans-serif';
  const countFont = '10.5px "Spline Sans Mono", monospace';
  const boxes = new Map<number, Box>();

  let y = 0;
  for (const row of layers) {
    const widths = row.map((id) => {
      if (id === START || id === END) return 64;
      const activity = data.activities[id];
      const label = Math.max(
        textWidth(activity.name, nameFont),
        textWidth(numberFormat.format(activity.count), countFont),
      );
      return Math.max(96, Math.ceil(label) + 32);
    });
    const total = widths.reduce((a, b) => a + b, 0) + GAP_X * (row.length - 1);
    let x = -total / 2;
    const rowH = row.length === 1 && (row[0] === START || row[0] === END)
      ? TERMINAL_H
      : NODE_H;
    row.forEach((id, i) => {
      const boxH = id === START || id === END ? TERMINAL_H : NODE_H;
      boxes.set(id, { x: x + widths[i] / 2, y: y + boxH / 2, w: widths[i], h: boxH });
      x += widths[i] + GAP_X;
    });
    y += rowH + GAP_Y;
  }

  // Fan edge attach points across node tops/bottoms so paths don't overlap.
  const outBy = new Map<number, WorkEdge[]>(ids.map((id) => [id, []]));
  const inBy = new Map<number, WorkEdge[]>(ids.map((id) => [id, []]));
  for (const edge of edges) {
    if (edge.source === edge.target || edge.back) continue;
    outBy.get(edge.source)!.push(edge);
    inBy.get(edge.target)!.push(edge);
  }
  for (const [id, list] of outBy) {
    const box = boxes.get(id)!;
    list.sort((a, b) => boxes.get(a.target)!.x - boxes.get(b.target)!.x);
    list.forEach((edge, i) => {
      const t = list.length === 1 ? 0.5 : i / (list.length - 1);
      edge.x1 = box.x + (t - 0.5) * box.w * 0.64;
      edge.y1 = box.y + box.h / 2;
    });
  }
  for (const [id, list] of inBy) {
    const box = boxes.get(id)!;
    list.sort((a, b) => boxes.get(a.source)!.x - boxes.get(b.source)!.x);
    list.forEach((edge, i) => {
      const t = list.length === 1 ? 0.5 : i / (list.length - 1);
      edge.x2 = box.x + (t - 0.5) * box.w * 0.64;
      edge.y2 = box.y - box.h / 2;
    });
  }

  // Bounds, including room for the loop sweeps on the right.
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  for (const box of boxes.values()) {
    minX = Math.min(minX, box.x - box.w / 2);
    maxX = Math.max(maxX, box.x + box.w / 2);
    minY = Math.min(minY, box.y - box.h / 2);
    maxY = Math.max(maxY, box.y + box.h / 2);
  }
  const backCount = edges.filter((e) => e.back).length;
  const loopReach = backCount > 0 ? 64 + (backCount - 1) * 34 + 24 : 0;
  const pad = 60;
  const fit: ViewBox = {
    x: minX - pad,
    y: minY - pad,
    w: maxX + loopReach - minX + pad * 2,
    h: maxY - minY + pad * 2,
  };

  // Render geometry.
  const maxCount = Math.max(...edges.map((e) => e.count));
  let backIndex = 0;
  const edgeLayouts: EdgeLayout[] = edges.map((edge) => {
    const geo = edgeGeometry(edge, boxes, edge.back ? backIndex++ : 0, maxX);
    const width = 1 + 4.5 * Math.sqrt(edge.count / maxCount);

    const tip = geo.p3;
    const before = cubicPoint(geo.p0, geo.p1, geo.p2, geo.p3, 0.96);
    const angle = Math.atan2(tip.y - before.y, tip.x - before.x);
    const size = 5 + width * 1.1;
    const half = 2.6 + width * 0.55;
    const bx = tip.x - Math.cos(angle) * size;
    const by = tip.y - Math.sin(angle) * size;
    const nx = -Math.sin(angle);
    const ny = Math.cos(angle);

    return {
      key: edgeKey(edge.source, edge.target),
      source: edge.source,
      target: edge.target,
      count: edge.count,
      back: edge.back ?? false,
      path: `M ${geo.p0.x} ${geo.p0.y} C ${geo.p1.x} ${geo.p1.y}, ${geo.p2.x} ${geo.p2.y}, ${geo.p3.x} ${geo.p3.y}`,
      headPoints: `${tip.x},${tip.y} ${bx + nx * half},${by + ny * half} ${bx - nx * half},${by - ny * half}`,
      label: geo.label,
      width,
      opacity: 0.3 + 0.65 * (edge.count / maxCount),
    };
  });

  const nodes: NodeLayout[] = data.activities.map((activity, id) => {
    const box = boxes.get(id)!;
    return { id, name: activity.name, count: activity.count, ...box };
  });

  const terminals: TerminalLayout[] = [
    { id: START, label: "START", ...boxes.get(START)! },
    { id: END, label: "END", ...boxes.get(END)! },
  ];

  return { nodes, terminals, edges: edgeLayouts, fit, showLabels: edges.length <= 80 };
}
