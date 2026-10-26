import { useEffect, useMemo, useRef, useState } from "react";
import { type GraphLayout, type ViewBox, edgeKey } from "../layout";
import { panViewBox, zoomViewBoxAt } from "../graph-viewport";
import { type DurationStats, formatDuration, numberFormat, START, END } from "../types";

interface Highlight {
  edges: Set<string>;
  nodes: Set<number>;
}

type MapMode = "frequency" | "performance";

interface ProcessMapProps {
  layout: GraphLayout;
  threshold: number;
  maxEdgeCount: number;
  onThresholdChange: (value: number) => void;
  /** Variant selection from the side panel, if any. */
  selection: Highlight | null;
  /** Case throughput stats, when the log is timed; enables the time view. */
  caseDuration?: DurationStats;
  /** Gaps between consecutive case starts — the case arrival rate. */
  caseArrival?: DurationStats;
}

export function ProcessMap({
  layout,
  threshold,
  maxEdgeCount,
  onThresholdChange,
  selection,
  caseDuration,
  caseArrival,
}: ProcessMapProps) {
  const svgRef = useRef<SVGSVGElement>(null);
  const [view, setView] = useState<ViewBox>(layout.fit);
  const [hoverNode, setHoverNode] = useState<number | null>(null);
  const [mode, setMode] = useState<MapMode>("frequency");

  // The time view is only meaningful when the log carried timestamps.
  const hasPerformance = (caseDuration?.count ?? 0) > 0;
  const showTime = mode === "performance" && hasPerformance;

  useEffect(() => setView(layout.fit), [layout]);

  // React registers wheel listeners as passive, so zooming needs a manual
  // non-passive listener to be able to preventDefault page scrolling.
  useEffect(() => {
    const svg = svgRef.current;
    if (!svg) return;
    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const factor = Math.exp(event.deltaY * 0.0016);
      const rect = svg.getBoundingClientRect();
      setView((view) => {
        const newW = Math.min(Math.max(view.w * factor, 120), layout.fit.w * 4);
        return zoomViewBoxAt(view, rect, event.clientX, event.clientY, newW);
      });
    };
    svg.addEventListener("wheel", onWheel, { passive: false });
    return () => svg.removeEventListener("wheel", onWheel);
  }, [layout]);

  const panRef = useRef<{ x: number; y: number } | null>(null);
  const [panning, setPanning] = useState(false);

  const highlight: Highlight | null = useMemo(() => {
    if (hoverNode !== null) {
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
    }
    return selection;
  }, [hoverNode, selection, layout]);

  const connected = useMemo(() => {
    const ids = new Set<number>();
    for (const edge of layout.edges) {
      if (edge.count >= threshold) {
        ids.add(edge.source);
        ids.add(edge.target);
      }
    }
    return ids;
  }, [layout, threshold]);

  const edgeClass = (key: string) => {
    if (!highlight) return "edge";
    return highlight.edges.has(key) ? "edge lit" : "edge dim";
  };

  return (
    <section className="panel map-panel reveal">
      <div className="panel-head">
        <h2>Process map</h2>
        <div className="map-controls">
          {hasPerformance && (
            <div className="map-mode" role="group" aria-label="Map metric">
              <button
                className={mode === "frequency" ? "mode-btn active" : "mode-btn"}
                onClick={() => setMode("frequency")}
              >
                Frequency
              </button>
              <button
                className={mode === "performance" ? "mode-btn active" : "mode-btn"}
                onClick={() => setMode("performance")}
              >
                Performance
              </button>
            </div>
          )}
          {showTime && caseDuration && (
            <span className="map-stat" title="Median / mean case throughput time">
              case ≈ {formatDuration(caseDuration.median)} ·{" "}
              {formatDuration(caseDuration.mean)} avg
            </span>
          )}
          {showTime && (caseArrival?.count ?? 0) > 0 && caseArrival && (
            <span
              className="map-stat"
              title="Mean gap between consecutive case starts (case arrival rate)"
            >
              new case ≈ every {formatDuration(caseArrival.mean)}
            </span>
          )}
          <label className="threshold">
            <span className="threshold-label">paths ≥</span>
            <input
              type="range"
              min={1}
              max={maxEdgeCount}
              value={threshold}
              onChange={(event) => onThresholdChange(Number(event.target.value))}
            />
            <span className="threshold-value">{threshold}</span>
          </label>
          <button className="btn-ghost" onClick={() => setView(layout.fit)}>
            Fit
          </button>
        </div>
      </div>
      <div className="map-wrap">
        <svg
          ref={svgRef}
          className={panning ? "panning" : undefined}
          viewBox={`${view.x} ${view.y} ${view.w} ${view.h}`}
          role="img"
          aria-label="Directly-follows graph"
          onPointerDown={(event) => {
            panRef.current = { x: event.clientX, y: event.clientY };
            setPanning(true);
            event.currentTarget.setPointerCapture(event.pointerId);
          }}
          onPointerMove={(event) => {
            const pan = panRef.current;
            if (!pan) return;
            const rect = event.currentTarget.getBoundingClientRect();
            setView(panViewBox(view, rect, event.clientX - pan.x, event.clientY - pan.y));
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
          <g>
            {layout.edges.map((edge) =>
              edge.count >= threshold ? (
                <g className={edgeClass(edge.key)} key={edge.key}>
                  <path
                    className="edge-path"
                    d={edge.path}
                    strokeWidth={edge.width.toFixed(2)}
                    strokeOpacity={edge.opacity.toFixed(3)}
                  />
                  <polygon className="edge-head" points={edge.headPoints} />
                  {layout.showLabels && (
                    <text
                      className="edge-label"
                      x={edge.label.x}
                      y={edge.label.y + 3}
                    >
                      {showTime
                        ? formatDuration(edge.duration ?? 0)
                        : numberFormat.format(edge.count)}
                    </text>
                  )}
                </g>
              ) : null,
            )}
          </g>
          <g>
            {layout.nodes.map((node) => {
              const dimmed =
                (highlight !== null && !highlight.nodes.has(node.id)) ||
                !connected.has(node.id);
              return (
                <g
                  className={dimmed ? "node dim" : "node"}
                  key={node.id}
                  onMouseEnter={() => setHoverNode(node.id)}
                  onMouseLeave={() => setHoverNode(null)}
                >
                  <rect
                    className="node-box"
                    x={node.x - node.w / 2}
                    y={node.y - node.h / 2}
                    width={node.w}
                    height={node.h}
                    rx={3}
                  />
                  <text className="node-name" x={node.x} y={node.y - 3} textAnchor="middle">
                    {node.name}
                  </text>
                  <text className="node-count" x={node.x} y={node.y + 13} textAnchor="middle">
                    {numberFormat.format(node.count)}
                  </text>
                </g>
              );
            })}
            {layout.terminals.map((terminal) => (
              <g className="terminal" key={terminal.id}>
                <rect
                  className={terminal.label === "START" ? "terminal-box start" : "terminal-box"}
                  x={terminal.x - terminal.w / 2}
                  y={terminal.y - terminal.h / 2}
                  width={terminal.w}
                  height={terminal.h}
                  rx={terminal.h / 2}
                />
                <text
                  className="terminal-label"
                  x={terminal.x + 1.5}
                  y={terminal.y + 3.5}
                  textAnchor="middle"
                >
                  {terminal.label}
                </text>
              </g>
            ))}
          </g>
        </svg>
        <div className="map-hint">scroll to zoom · drag to pan</div>
      </div>
    </section>
  );
}

export function variantHighlight(steps: number[]): Highlight {
  const edges = new Set<string>();
  edges.add(edgeKey(START, steps[0]));
  for (let i = 1; i < steps.length; ++i) {
    edges.add(edgeKey(steps[i - 1], steps[i]));
  }
  edges.add(edgeKey(steps[steps.length - 1], END));
  return { edges, nodes: new Set(steps) };
}
