import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import {
  computeOcelGraphLayout,
  type OcelGraphInput,
  type OcelGraphKind,
  type OcelGraphLayoutOptions,
  type OcelGraphRankDirection,
} from "../ocel-graph-layout";

interface ViewBox {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface OcelGraphModelProps {
  result: OcelGraphInput;
  kind: OcelGraphKind;
  title?: string;
  background?: string;
  rankDirection?: OcelGraphRankDirection;
  options?: Omit<OcelGraphLayoutOptions, "kind" | "rankDirection">;
  actions?: ReactNode;
}

const DIRECTIONS: readonly OcelGraphRankDirection[] = ["TB", "BT", "LR", "RL"];

function maxFinite(values: readonly number[], fallback = 1): number {
  let maximum = fallback;
  for (const value of values) if (Number.isFinite(value)) maximum = Math.max(maximum, value);
  return maximum;
}

function controlValue(value: number, maximum: number): number {
  return Math.max(0, Math.min(maximum, Number.isFinite(value) ? value : 0));
}

/** Render a deterministic ETOT or OTG projection with bounded SVG controls. */
export function OcelGraphModel({
  result,
  kind,
  title = kind === "etot" ? "Event-to-object-type graph" : "Object-type graph",
  background,
  rankDirection: initialDirection,
  options,
  actions,
}: OcelGraphModelProps) {
  const [rankDirection, setRankDirection] = useState<OcelGraphRankDirection>(
    initialDirection ?? "TB",
  );
  const [frequencyThreshold, setFrequencyThreshold] = useState(
    options?.frequencyThreshold ?? 0,
  );
  const layout = useMemo(
    () => computeOcelGraphLayout(result, {
      ...options,
      kind,
      rankDirection,
      frequencyThreshold,
    }),
    [result, kind, options, rankDirection, frequencyThreshold],
  );
  const [view, setView] = useState<ViewBox>(layout.fit);
  const svgRef = useRef<SVGSVGElement>(null);
  const panRef = useRef<{ x: number; y: number } | null>(null);
  const [panning, setPanning] = useState(false);
  const [hoverNode, setHoverNode] = useState<number | null>(null);

  useEffect(() => setView(layout.fit), [layout.fit]);
  useEffect(() => {
    const svg = svgRef.current;
    if (!svg) return;
    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const factor = Math.exp(event.deltaY * 0.0016);
      const rect = svg.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) return;
      const px = (event.clientX - rect.left) / rect.width;
      const py = (event.clientY - rect.top) / rect.height;
      setView((current) => {
        const newW = Math.min(Math.max(current.w * factor, 120), layout.fit.w * 4);
        const newH = current.h * (newW / current.w);
        return {
          x: current.x + (current.w - newW) * px,
          y: current.y + (current.h - newH) * py,
          w: newW,
          h: newH,
        };
      });
    };
    svg.addEventListener("wheel", onWheel, { passive: false });
    return () => svg.removeEventListener("wheel", onWheel);
  }, [layout.fit]);

  const highlight = useMemo(() => {
    if (hoverNode === null) return null;
    const edges = new Set<string>();
    const nodes = new Set<number>([hoverNode]);
    for (const edge of layout.edges) {
      if (edge.source === hoverNode || edge.target === hoverNode) {
        edges.add(edge.key);
        nodes.add(edge.source);
        nodes.add(edge.target);
      }
    }
    return { edges, nodes };
  }, [hoverNode, layout.edges]);
  const maximum = maxFinite(layout.edges.map((edge) => edge.frequency));

  return (
    <section className="panel map-panel reveal">
      <div className="panel-head">
        <h2>{title}</h2>
        <span className="panel-meta">
          {layout.nodes.length}/{layout.inputNodeCount} nodes · {layout.edges.length}/{layout.inputEdgeCount} edges
        </span>
        {layout.truncatedNodes || layout.truncatedEdges ? (
          <span className="panel-meta">bounded view</span>
        ) : null}
        <div className="map-controls">
          <label className="threshold">
            <span className="threshold-label">edge ≥</span>
            <input
              aria-label="Edge frequency threshold"
              type="range"
              min={0}
              max={maximum}
              step={1}
              value={controlValue(frequencyThreshold, maximum)}
              onChange={(event) => setFrequencyThreshold(Number(event.target.value))}
            />
            <span className="threshold-value">{frequencyThreshold}</span>
          </label>
          <label className="threshold">
            <span className="threshold-label">rank</span>
            <select
              aria-label="Rank direction"
              value={rankDirection}
              onChange={(event) => setRankDirection(event.target.value as OcelGraphRankDirection)}
            >
              {DIRECTIONS.map((direction) => <option key={direction} value={direction}>{direction}</option>)}
            </select>
          </label>
          {actions}
          <button className="btn-ghost" onClick={() => setView(layout.fit)}>Fit</button>
        </div>
      </div>
      <div className="map-wrap" style={{ background }}>
        <svg
          ref={svgRef}
          className={panning ? "panning" : undefined}
          viewBox={`${view.x} ${view.y} ${view.w} ${view.h}`}
          role="img"
          aria-label={title}
          tabIndex={0}
          onPointerDown={(event) => {
            panRef.current = { x: event.clientX, y: event.clientY };
            setPanning(true);
            event.currentTarget.setPointerCapture(event.pointerId);
          }}
          onPointerMove={(event) => {
            const pan = panRef.current;
            if (!pan) return;
            const rect = event.currentTarget.getBoundingClientRect();
            if (rect.width <= 0) return;
            const scale = view.w / rect.width;
            setView({
              ...view,
              x: view.x - (event.clientX - pan.x) * scale,
              y: view.y - (event.clientY - pan.y) * scale,
            });
            panRef.current = { x: event.clientX, y: event.clientY };
          }}
          onPointerUp={() => {
            panRef.current = null;
            setPanning(false);
          }}
          onPointerCancel={() => {
            panRef.current = null;
            setPanning(false);
          }}
          onDoubleClick={() => setView(layout.fit)}
        >
          <title>{title}</title>
          <desc>{layout.nodes.length} nodes and {layout.edges.length} typed object-centric edges.</desc>
          <g>
            {layout.edges.map((edge) => {
              const dimmed = highlight !== null && !highlight.edges.has(edge.key);
              return (
                <g className={dimmed ? "edge dim" : "edge"} key={edge.key}>
                  <title>
                    {edge.sourceName} → {edge.targetName} · {edge.relation} · frequency: {edge.frequencyText}
                  </title>
                  <path
                    className="edge-path"
                    d={edge.path}
                    strokeWidth={edge.width.toFixed(2)}
                    strokeOpacity={edge.opacity.toFixed(3)}
                    strokeDasharray={edge.back ? "5 3" : undefined}
                  />
                  <polygon className="edge-head" points={edge.headPoints} />
                  {layout.showLabels && (
                    <text className="edge-label" x={edge.label.x} y={edge.label.y + 3}>
                      {edge.relation}: {edge.frequencyText}
                    </text>
                  )}
                </g>
              );
            })}
          </g>
          <g>
            {layout.nodes.map((node) => {
              const dimmed = highlight !== null && !highlight.nodes.has(node.id);
              return (
                <g
                  className={dimmed ? "node dim" : "node"}
                  key={node.id}
                  onMouseEnter={() => setHoverNode(node.id)}
                  onMouseLeave={() => setHoverNode(null)}
                >
                  <title>{node.name} · frequency: {node.metricText}</title>
                  <rect
                    className="node-box"
                    x={node.x - node.w / 2}
                    y={node.y - node.h / 2}
                    width={node.w}
                    height={node.h}
                    rx={3}
                  />
                  <text className="node-name" x={node.x} y={node.y - 3} textAnchor="middle">
                    {node.displayName}
                  </text>
                  <text className="node-count" x={node.x} y={node.y + 13} textAnchor="middle">
                    frequency: {node.metricText}
                  </text>
                </g>
              );
            })}
          </g>
        </svg>
        <div className="map-hint">scroll to zoom · drag to pan · labels show edge frequencies</div>
      </div>
    </section>
  );
}
