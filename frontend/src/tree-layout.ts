import { type ViewBox } from "./layout";
import {
  type MineraExport,
  type ProcessTreeNode,
  type ProcessTreeOperator,
} from "./types";

export interface TreeNodeLayout {
  id: number;
  kind: "operator" | "activity" | "tau";
  /** Operator glyph, activity name, or "τ". */
  label: string;
  operator: ProcessTreeOperator;
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface TreeEdgeLayout {
  key: string;
  path: string;
  /** True for the redo branches of a loop (drawn dashed). */
  redo: boolean;
}

export interface TreeLayout {
  nodes: TreeNodeLayout[];
  edges: TreeEdgeLayout[];
  fit: ViewBox;
}

const LEVEL_Y = 92; // centre-to-centre distance between depth levels
const OP_R = 18; // operator node radius
const LEAF_H = 38;
const TAU_W = 34;
const TAU_H = 30;
const GAP_X = 26; // horizontal gap between adjacent leaves

const OPERATOR_GLYPH: Record<ProcessTreeOperator, string> = {
  sequence: "→",
  exclusive: "×",
  parallel: "∧",
  loop: "↺",
  activity: "",
  tau: "τ",
};

const measureCtx = document.createElement("canvas").getContext("2d")!;
const leafFont = '600 12.5px "Familjen Grotesk", sans-serif';

function leafWidth(name: string): number {
  measureCtx.font = leafFont;
  return Math.max(64, Math.ceil(measureCtx.measureText(name).width) + 26);
}

function linkPath(x1: number, y1: number, x2: number, y2: number): string {
  const my = (y1 + y2) / 2;
  return `M ${x1} ${y1} C ${x1} ${my}, ${x2} ${my}, ${x2} ${y2}`;
}

interface Placed {
  id: number;
  cx: number;
  topY: number;
}

export function computeTreeLayout(
  root: ProcessTreeNode,
  data: MineraExport,
): TreeLayout {
  const nodes: TreeNodeLayout[] = [];
  const edges: TreeEdgeLayout[] = [];
  let nextId = 0;
  let cursorX = 0;

  const place = (node: ProcessTreeNode, depth: number): Placed => {
    const id = nextId++;
    const y = depth * LEVEL_Y;
    const children = node.children ?? [];

    if (children.length === 0) {
      const isTau = node.operator === "tau";
      const label = isTau
        ? "τ"
        : (data.activities[node.activity ?? -1]?.name ?? "?");
      const w = isTau ? TAU_W : leafWidth(label);
      const h = isTau ? TAU_H : LEAF_H;
      const cx = cursorX + w / 2;
      cursorX += w + GAP_X;
      nodes.push({
        id,
        kind: isTau ? "tau" : "activity",
        label,
        operator: node.operator,
        x: cx,
        y,
        w,
        h,
      });
      return { id, cx, topY: y - h / 2 };
    }

    const placedChildren = children.map((child) => place(child, depth + 1));
    const cx =
      (placedChildren[0].cx + placedChildren[placedChildren.length - 1].cx) / 2;
    nodes.push({
      id,
      kind: "operator",
      label: OPERATOR_GLYPH[node.operator],
      operator: node.operator,
      x: cx,
      y,
      w: OP_R * 2,
      h: OP_R * 2,
    });

    placedChildren.forEach((child, index) => {
      edges.push({
        key: `${id}-${child.id}`,
        path: linkPath(cx, y + OP_R, child.cx, child.topY),
        // For a loop the first child is the "do" body; the rest are redo paths.
        redo: node.operator === "loop" && index > 0,
      });
    });

    return { id, cx, topY: y - OP_R };
  };

  place(root, 0);

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
  const pad = 48;
  const fit: ViewBox = {
    x: minX - pad,
    y: minY - pad,
    w: maxX - minX + pad * 2,
    h: maxY - minY + pad * 2,
  };

  return { nodes, edges, fit };
}
