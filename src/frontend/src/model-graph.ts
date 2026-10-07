export interface ModelGraphNode {
  id: string;
  label: string;
  detail?: string;
  start?: boolean;
  end?: boolean;
  outsideRoot?: boolean;
}
export interface ModelGraphEdge {
  id: string;
  source: string;
  target: string;
  label: string;
  value: number;
  directed: boolean;
}
export interface ModelGraph { nodes: ModelGraphNode[]; edges: ModelGraphEdge[]; }
export interface ModelGraphNodeLayout extends ModelGraphNode { x: number; y: number; w: number; h: number; }
export interface ModelGraphEdgeLayout extends ModelGraphEdge { path: string; x: number; y: number; width: number; }

/** Geometry only: native identities, parallel edges and signed weights survive. */
export function modelGraphLayout(graph: ModelGraph, style: "flow" | "network", minimum = 0, focusId?: string, maxNodes = 120, maxEdges = 360) {
  const qualified = graph.edges.filter(edge => Number.isFinite(edge.value) && Math.abs(edge.value) >= minimum);
  const priority = new Set<string>();
  if (focusId && graph.nodes.some(node => node.id === focusId)) {
    priority.add(focusId);
    for (const edge of qualified) {
      if (edge.source === focusId) priority.add(edge.target);
      if (edge.target === focusId) priority.add(edge.source);
    }
  }
  const candidates = [...graph.nodes.filter(node => node.id === focusId), ...graph.nodes.filter(node => node.id !== focusId && priority.has(node.id)), ...graph.nodes.filter(node => !priority.has(node.id) && node.id !== focusId)];
  const selected = candidates.slice(0, Math.max(1, maxNodes));
  const ids = new Set(selected.map(node => node.id));
  const available = qualified.filter(edge => ids.has(edge.source) && ids.has(edge.target));
  const incident = (edge: ModelGraphEdge) => edge.source === focusId || edge.target === focusId;
  const edges = [...available.filter(incident), ...available.filter(edge => !incident(edge))].slice(0, Math.max(0, maxEdges));
  const nodes: ModelGraphNodeLayout[] = selected.map(node => ({ ...node, x: 0, y: 0, w: 190, h: 76 }));
  if (style === "network") {
    const radius = nodes.length > 1 ? Math.max(160, nodes.length * 26) : 0;
    nodes.forEach((node, i) => {
      const angle = -Math.PI / 2 + i * 2 * Math.PI / nodes.length;
      node.x = radius + 160 + radius * Math.cos(angle);
      node.y = radius + 130 + radius * Math.sin(angle);
    });
  } else {
    const rank = new Map<string, number>();
    let seeds = nodes.filter(node => node.start).map(node => node.id);
    if (!seeds.length) seeds = nodes.filter(node => !edges.some(edge => edge.target === node.id && edge.source !== node.id)).map(node => node.id);
    if (!seeds.length && nodes.length) seeds = [nodes[0].id];
    const queue = [...seeds]; seeds.forEach(id => rank.set(id, 0));
    for (let i = 0; i < queue.length; ++i) {
      for (const edge of edges) {
        if (edge.source !== queue[i] || rank.has(edge.target)) continue;
        rank.set(edge.target, rank.get(edge.source)! + 1); queue.push(edge.target);
      }
    }
    let last = Math.max(0, ...rank.values());
    for (const node of nodes) if (!rank.has(node.id)) rank.set(node.id, ++last);
    const columns = new Map<number, ModelGraphNodeLayout[]>();
    for (const node of nodes) { const r = rank.get(node.id)!; const list = columns.get(r) ?? []; list.push(node); columns.set(r, list); }
    const height = Math.max(1, ...[...columns.values()].map(list => list.length)) * 132;
    for (const [r, list] of columns) list.forEach((node, i) => {
      const row = Math.floor(r / 4), column = row % 2 ? 3-r%4 : r%4;
      node.x = 140 + column * 290;
      node.y = 100 + row * (height+100) + (height-list.length*132)/2 + i*132;
    });
  }
  const byId = new Map(nodes.map(node => [node.id, node]));
  const pairs = new Map<string, ModelGraphEdge[]>();
  for (const edge of edges) { const key = JSON.stringify([edge.source, edge.target].sort()); const pair = pairs.get(key) ?? []; pair.push(edge); pairs.set(key, pair); }
  const maxValue = Math.max(1, ...edges.map(edge => Math.abs(edge.value)));
  const routeBounds: { x: number; y: number }[] = [];
  const routed: ModelGraphEdgeLayout[] = edges.map(edge => {
    const source = byId.get(edge.source)!; const target = byId.get(edge.target)!;
    if (source.id === target.id) {
      const x = source.x + source.w / 2, y = source.y;
      const lane = pairs.get(JSON.stringify([edge.source,edge.target]))!.indexOf(edge);
      const extent = 85 + lane * 24;
      routeBounds.push({x:x+extent,y:y-65},{x:x+extent,y:y+65});
      return { ...edge, path: `M ${x} ${y-15} C ${x+extent} ${y-65} ${x+extent} ${y+65} ${x} ${y+15}`, x: x+extent*0.75, y: y-12+lane*14, width: 1.2 + 3.8 * Math.sqrt(Math.abs(edge.value) / maxValue) };
    }
    const dx = target.x - source.x, dy = target.y - source.y;
    const border = (node: ModelGraphNodeLayout, x: number, y: number) => { const scale = Math.min(x ? node.w / 2 / Math.abs(x) : Infinity, y ? node.h / 2 / Math.abs(y) : Infinity); return { x: node.x + x * scale, y: node.y + y * scale }; };
    const a = border(source, dx, dy), b = border(target, -dx, -dy);
    const pair = pairs.get(JSON.stringify([edge.source, edge.target].sort()))!;
    const lane = (pair.indexOf(edge) - (pair.length - 1) / 2) * 34;
    const orientation = source.id < target.id ? 1 : -1;
    const length = Math.hypot(dx, dy);
    const cx = (a.x+b.x)/2 - dy/length * lane * orientation, cy = (a.y+b.y)/2 + dx/length * lane * orientation;
    routeBounds.push(a,b,{x:cx,y:cy});
    return { ...edge, path: `M ${a.x} ${a.y} Q ${cx} ${cy} ${b.x} ${b.y}`, x: (a.x+2*cx+b.x)/4, y: (a.y+2*cy+b.y)/4 - 7, width: 1.2 + 3.8 * Math.sqrt(Math.abs(edge.value)/maxValue) };
  });
  const minX = Math.min(0, ...nodes.map(node => node.x-node.w/2), ...routeBounds.map(point => point.x), ...routed.map(edge => edge.x-130));
  const minY = Math.min(0, ...nodes.map(node => node.y-node.h/2), ...routeBounds.map(point => point.y), ...routed.map(edge => edge.y-20));
  const maxX = Math.max(300, ...nodes.map(node => node.x+node.w/2), ...routeBounds.map(point => point.x), ...routed.map(edge => edge.x+130));
  const maxY = Math.max(200, ...nodes.map(node => node.y+node.h/2), ...routeBounds.map(point => point.y), ...routed.map(edge => edge.y+20));
  return { nodes, edges: routed, fit: { x: minX-50, y: minY-70, w: maxX-minX+100, h: maxY-minY+140 }, totalNodes: graph.nodes.length, totalEdges: graph.edges.length, qualifiedEdges: qualified.length };
}
