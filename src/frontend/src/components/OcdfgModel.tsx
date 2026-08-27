import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import {
  aggregateOcdfgPerformance,
  computeOcdfgLayout,
  ocdfgActivityMetricLabel,
  ocdfgEdgeMetricLabel,
  type OcdfgActivityMetric,
  type OcdfgEdgeMetric,
  type OcdfgInput,
  type OcdfgLayoutOptions,
  type OcdfgPerformanceAggregation,
  type OcdfgRankDirection,
} from "../ocdfg-layout";

interface ViewBox {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface OcdfgModelProps {
  /** An OCEL discovery envelope, or its already-unwrapped graph result. */
  result: OcdfgInput;
  /** Caption shown beside the panel title. */
  title?: string;
  /** Optional CSS background for the map frame. */
  background?: string;
  /** Initial rank direction, using the conventional graph directions. */
  rankDirection?: OcdfgRankDirection;
  /** Initial activity and edge metrics; controls remain available in the view. */
  activityMetric?: OcdfgActivityMetric;
  edgeMetric?: OcdfgEdgeMetric;
  performanceAggregation?: OcdfgPerformanceAggregation;
  /** Additional layout limits and annotation preference. */
  options?: Omit<OcdfgLayoutOptions, "activityMetric" | "edgeMetric" | "performanceAggregation" | "rankDirection">;
  actions?: ReactNode;
}

const ACTIVITY_METRICS: readonly OcdfgActivityMetric[] = [
  "events",
  "unique_objects",
  "event_objects",
  "total_objects",
];
const EDGE_METRICS: readonly OcdfgEdgeMetric[] = [
  "event_couples",
  "unique_objects",
  "event_objects",
  "total_objects",
  "performance",
];
const PERFORMANCE_AGGREGATIONS: readonly OcdfgPerformanceAggregation[] = [
  "mean",
  "median",
  "min",
  "max",
];
const DIRECTIONS: readonly OcdfgRankDirection[] = ["TB", "BT", "LR", "RL"];

function inputPerformanceComputed(result: OcdfgInput): boolean {
  if (typeof result !== "object" || result === null) return false;
  const candidate = result as Record<string, unknown>;
  const graph = candidate.family === "OCEL_DISCOVERY" &&
      typeof candidate.result === "object" && candidate.result !== null
    ? candidate.result as Record<string, unknown>
    : candidate;
  return graph.performance_computed !== false;
}

function maxFinite(values: readonly number[], fallback = 1): number {
  let maximum = fallback;
  for (const value of values) if (Number.isFinite(value)) maximum = Math.max(maximum, value);
  return maximum;
}

function controlValue(value: number, maximum: number): number {
  return Math.max(0, Math.min(maximum, Number.isFinite(value) ? value : 0));
}

/**
 * Render the classic object-centric directly-follows projection as a
 * pannable, deterministic SVG.  Geometry is produced by the pure layout
 * helper, so loading an export never starts a force simulation or depends on
 * browser measurement.
 */
export function OcdfgModel({
  result,
  title = "Object-centric directly-follows graph",
  background,
  rankDirection: initialDirection,
  activityMetric: initialActivityMetric,
  edgeMetric: initialEdgeMetric,
  performanceAggregation: initialAggregation,
  options,
  actions,
}: OcdfgModelProps) {
  const [activityMetric, setActivityMetric] = useState<OcdfgActivityMetric>(
    initialActivityMetric ?? "events",
  );
  const [edgeMetric, setEdgeMetric] = useState<OcdfgEdgeMetric>(
    initialEdgeMetric ?? "event_couples",
  );
  const [performanceAggregation, setPerformanceAggregation] = useState<OcdfgPerformanceAggregation>(
    initialAggregation ?? "mean",
  );
  const [rankDirection, setRankDirection] = useState<OcdfgRankDirection>(
    initialDirection ?? "TB",
  );
  const [activityThreshold, setActivityThreshold] = useState(
    options?.activityThreshold ?? 0,
  );
  const [edgeThreshold, setEdgeThreshold] = useState(options?.edgeThreshold ?? 0);

  const layout = useMemo(
    () => computeOcdfgLayout(result, {
      ...options,
      activityMetric,
      edgeMetric,
      performanceAggregation,
      rankDirection,
      activityThreshold,
      edgeThreshold,
    }),
    [result, options, activityMetric, edgeMetric, performanceAggregation,
      rankDirection, activityThreshold, edgeThreshold],
  );

  const [view, setView] = useState<ViewBox>(layout.fit);
  const svgRef = useRef<SVGSVGElement>(null);
  const panRef = useRef<{ x: number; y: number } | null>(null);
  const [panning, setPanning] = useState(false);
  const [hoverNode, setHoverNode] = useState<number | null>(null);
  const performanceComputed = inputPerformanceComputed(result);

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

  const activityMaximum = maxFinite(layout.nodes.map((node) => node.metric));
  const edgeMaximum = maxFinite(layout.edges.map((edge) => edge.metric));

  return (
    <section className="panel map-panel reveal">
      <div className="panel-head">
        <h2>{title}</h2>
        <span className="panel-meta">
          {layout.nodes.length}/{layout.inputActivityCount} activities · {layout.edges.length}/{layout.inputEdgeCount} flows
        </span>
        {layout.truncatedActivities || layout.truncatedEdges ? (
          <span className="panel-meta">bounded view</span>
        ) : null}
        <div className="map-controls">
          <label className="threshold">
            <span className="threshold-label">activity</span>
            <select
              aria-label="Activity metric"
              value={activityMetric}
              onChange={(event) => setActivityMetric(event.target.value as OcdfgActivityMetric)}
            >
              {ACTIVITY_METRICS.map((metric) => (
                <option key={metric} value={metric}>{ocdfgActivityMetricLabel(metric)}</option>
              ))}
            </select>
          </label>
          <label className="threshold">
            <span className="threshold-label">edge</span>
            <select
              aria-label="Edge metric"
              value={edgeMetric}
              onChange={(event) => setEdgeMetric(event.target.value as OcdfgEdgeMetric)}
            >
              {EDGE_METRICS.map((metric) => (
                <option key={metric} value={metric}>{ocdfgEdgeMetricLabel(metric, performanceAggregation)}</option>
              ))}
            </select>
          </label>
          {edgeMetric === "performance" && (
            <label className="threshold">
              <span className="threshold-label">aggregate</span>
              <select
                aria-label="Performance aggregation"
                value={performanceAggregation}
                onChange={(event) => setPerformanceAggregation(event.target.value as OcdfgPerformanceAggregation)}
              >
                {PERFORMANCE_AGGREGATIONS.map((aggregation) => (
                  <option key={aggregation} value={aggregation}>{aggregation}</option>
                ))}
              </select>
            </label>
          )}
          <label className="threshold">
            <span className="threshold-label">activity ≥</span>
            <input
              aria-label="Activity threshold"
              type="range"
              min={0}
              max={activityMaximum}
              step={1}
              value={controlValue(activityThreshold, activityMaximum)}
              onChange={(event) => setActivityThreshold(Number(event.target.value))}
            />
            <span className="threshold-value">{activityThreshold}</span>
          </label>
          <label className="threshold">
            <span className="threshold-label">edge ≥</span>
            <input
              aria-label="Edge threshold"
              type="range"
              min={0}
              max={edgeMaximum}
              step={edgeMetric === "performance" ? 0.1 : 1}
              value={controlValue(edgeThreshold, edgeMaximum)}
              onChange={(event) => setEdgeThreshold(Number(event.target.value))}
            />
            <span className="threshold-value">{edgeThreshold}</span>
          </label>
          <label className="threshold">
            <span className="threshold-label">rank</span>
            <select
              aria-label="Rank direction"
              value={rankDirection}
              onChange={(event) => setRankDirection(event.target.value as OcdfgRankDirection)}
            >
              {DIRECTIONS.map((direction) => <option key={direction} value={direction}>{direction}</option>)}
            </select>
          </label>
          {actions}
          <button className="btn-ghost" onClick={() => setView(layout.fit)}>Fit</button>
        </div>
      </div>
      {!performanceComputed && edgeMetric === "performance" && (
        <div className="model-empty-inline">Performance samples were not computed for this graph.</div>
      )}
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
          <desc>
            {layout.nodes.length} activities and {layout.edges.length} object-centric flows;
            node metric {layout.activityMetricLabel}, edge metric {layout.edgeMetricLabel}.
          </desc>
          <g>
            {layout.edges.map((edge) => {
              const dimmed = highlight !== null && !highlight.edges.has(edge.key);
              const objectTypes = edge.objectTypes.length > 0
                ? ` · object types: ${edge.objectTypes.join(", ")}`
                : "";
              return (
                <g className={dimmed ? "edge dim" : "edge"} key={edge.key}>
                  <title>
                    {edge.sourceName} → {edge.targetName} · {layout.edgeMetricLabel}: {edge.metricText}{objectTypes}
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
                      {layout.edgeMetricLabel}: {edge.metricText}
                    </text>
                  )}
                </g>
              );
            })}
          </g>
          <g>
            {layout.nodes.map((node) => {
              const dimmed = highlight !== null && !highlight.nodes.has(node.id);
              const objectTypes = node.objectTypes.length > 0
                ? ` · object types: ${node.objectTypes.join(", ")}`
                : "";
              return (
                <g
                  className={dimmed ? "node dim" : "node"}
                  key={node.id}
                  onMouseEnter={() => setHoverNode(node.id)}
                  onMouseLeave={() => setHoverNode(null)}
                >
                  <title>
                    {node.name} · {layout.activityMetricLabel}: {node.metricText}{objectTypes}
                  </title>
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
                    {layout.activityMetricLabel}: {node.metricText}
                  </text>
                </g>
              );
            })}
          </g>
        </svg>
        <div className="map-hint">scroll to zoom · drag to pan · labels show selected metrics</div>
      </div>
    </section>
  );
}

/** Keep the performance helper discoverable alongside the visual component. */
export { aggregateOcdfgPerformance };
