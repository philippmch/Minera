import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import { type ViewBox } from "../layout";
import { computeDependencyLayout } from "../dependency-layout";
import { discoverHeuristicsNet } from "../heuristic-miner";
import { type MineraExport, numberFormat } from "../types";

interface DependencyGraphProps {
  data: MineraExport;
  /** Caption shown next to the panel title, e.g. the selected miner. */
  title?: string;
  /** Extra controls (e.g. an export button) shown in the panel header. */
  actions?: ReactNode;
}

// Renders a heuristics net: the dependency graph the Heuristic Miner keeps after
// thresholding. Boxes are activities, edges are accepted dependencies labelled
// with their dependency measure. New exports render the exact server-mined net;
// old exports that predate the variant-tagged artifact retain the browser-side
// classic threshold fallback. Pan/zoom mirrors the other model views.
export function DependencyGraph({ data, title, actions }: DependencyGraphProps) {
  const [dependencyThreshold, setDependencyThreshold] = useState(0.9);
  const [minFrequency, setMinFrequency] = useState(1);
  const plusPlus = data.heuristics_net?.variant === "plusplus";
  const fixedNet = data.heuristics_net?.variant !== undefined;

  const maxFrequency = useMemo(
    () => Math.max(1, ...data.edges.map((edge) => edge.count)),
    [data],
  );

  const { graph, dependency } = useMemo(() => {
    const net =
      fixedNet && data.heuristics_net
        ? data.heuristics_net
        : discoverHeuristicsNet(data, { dependencyThreshold, minFrequency });
    return computeDependencyLayout(net, data);
  }, [data, dependencyThreshold, minFrequency, fixedNet]);

  const svgRef = useRef<SVGSVGElement>(null);
  const [view, setView] = useState<ViewBox>(graph.fit);
  const [hoverNode, setHoverNode] = useState<number | null>(null);

  useEffect(() => setView(graph.fit), [graph]);

  // React attaches wheel listeners as passive; a manual non-passive listener is
  // needed to preventDefault page scrolling while zooming.
  useEffect(() => {
    const svg = svgRef.current;
    if (!svg) return;
    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const factor = Math.exp(event.deltaY * 0.0016);
      const rect = svg.getBoundingClientRect();
      const px = (event.clientX - rect.left) / rect.width;
      const py = (event.clientY - rect.top) / rect.height;
      setView((view) => {
        const newW = Math.min(Math.max(view.w * factor, 120), graph.fit.w * 4);
        const newH = view.h * (newW / view.w);
        return {
          x: view.x + (view.w - newW) * px,
          y: view.y + (view.h - newH) * py,
          w: newW,
          h: newH,
        };
      });
    };
    svg.addEventListener("wheel", onWheel, { passive: false });
    return () => svg.removeEventListener("wheel", onWheel);
  }, [graph]);

  const panRef = useRef<{ x: number; y: number } | null>(null);
  const [panning, setPanning] = useState(false);

  const highlight = useMemo(() => {
    if (hoverNode === null) return null;
    const edges = new Set<string>();
    const nodes = new Set<number>([hoverNode]);
    for (const edge of graph.edges) {
      if (edge.source === hoverNode || edge.target === hoverNode) {
        edges.add(edge.key);
        nodes.add(edge.source);
        nodes.add(edge.target);
      }
    }
    return { edges, nodes };
  }, [hoverNode, graph]);

  const edgeClass = (key: string) => {
    if (!highlight) return "edge";
    return highlight.edges.has(key) ? "edge lit" : "edge dim";
  };

  return (
    <section className="panel map-panel reveal">
      <div className="panel-head">
        <h2>Heuristics net</h2>
        {title && <span className="panel-meta">{title}</span>}
        <span className="panel-meta">
          {dependency.size} {dependency.size === 1 ? "dependency" : "dependencies"}
        </span>
        {plusPlus && <span className="panel-meta">Miner++ interval model</span>}
        <div className="map-controls">
          {!fixedNet && (
            <>
          <label className="threshold">
            <span className="threshold-label">dependency ≥</span>
            <input
              type="range"
              min={0}
              max={1}
              step={0.05}
              value={dependencyThreshold}
              onChange={(event) =>
                setDependencyThreshold(Number(event.target.value))
              }
            />
            <span className="threshold-value">{dependencyThreshold.toFixed(2)}</span>
          </label>
          <label className="threshold">
            <span className="threshold-label">freq ≥</span>
            <input
              type="range"
              min={1}
              max={maxFrequency}
              value={minFrequency}
              onChange={(event) => setMinFrequency(Number(event.target.value))}
            />
            <span className="threshold-value">{minFrequency}</span>
          </label>
            </>
          )}
          {actions}
          <button className="btn-ghost" onClick={() => setView(graph.fit)}>
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
          aria-label="Heuristics net"
          onPointerDown={(event) => {
            panRef.current = { x: event.clientX, y: event.clientY };
            setPanning(true);
            event.currentTarget.setPointerCapture(event.pointerId);
          }}
          onPointerMove={(event) => {
            const pan = panRef.current;
            if (!pan) return;
            const rect = event.currentTarget.getBoundingClientRect();
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
          onDoubleClick={() => setView(graph.fit)}
        >
          <g>
            {graph.edges.map((edge) => {
              const dep = dependency.get(edge.key);
              return (
                <g className={edgeClass(edge.key)} key={edge.key}>
                  <title>
                    {dep !== undefined
                      ? `dependency ${dep.toFixed(3)} · ${numberFormat.format(edge.count)} observations`
                      : `${numberFormat.format(edge.count)} observations`}
                  </title>
                  <path
                    className="edge-path"
                    d={edge.path}
                    strokeWidth={edge.width.toFixed(2)}
                    strokeOpacity={edge.opacity.toFixed(3)}
                  />
                  <polygon className="edge-head" points={edge.headPoints} />
                  {graph.showLabels && dep !== undefined && (
                    <text className="edge-label" x={edge.label.x} y={edge.label.y + 3}>
                      {dep.toFixed(2)}
                    </text>
                  )}
                </g>
              );
            })}
          </g>
          <g>
            {graph.nodes.map((node) => {
              const dimmed = highlight !== null && !highlight.nodes.has(node.id);
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
            {graph.terminals.map((terminal) => (
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
        <div className="map-hint">scroll to zoom · drag to pan · edge labels show dependency</div>
      </div>
    </section>
  );
}
