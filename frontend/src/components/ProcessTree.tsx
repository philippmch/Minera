import { type ReactNode, useEffect, useRef, useState } from "react";
import { type ViewBox } from "../layout";
import { type TreeLayout } from "../tree-layout";

const OPERATOR_NAME: Record<string, string> = {
  sequence: "Sequence",
  exclusive: "Exclusive choice",
  inclusive_or: "Inclusive choice",
  parallel: "Parallel",
  loop: "Loop",
};

interface ProcessTreeProps {
  layout: TreeLayout;
  /** Caption shown next to the panel title, e.g. the selected miner. */
  title?: string;
  /** Extra controls (e.g. an export button) shown in the panel header. */
  actions?: ReactNode;
}

export function ProcessTree({ layout, title, actions }: ProcessTreeProps) {
  const svgRef = useRef<SVGSVGElement>(null);
  const [view, setView] = useState<ViewBox>(layout.fit);

  useEffect(() => setView(layout.fit), [layout]);

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
        const newW = Math.min(Math.max(view.w * factor, 160), layout.fit.w * 4);
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
  }, [layout]);

  const panRef = useRef<{ x: number; y: number } | null>(null);
  const [panning, setPanning] = useState(false);

  return (
    <section className="panel map-panel reveal">
      <div className="panel-head">
        <h2>Process tree</h2>
        {title && <span className="panel-meta">{title}</span>}
        <div className="map-controls">
          <div className="tree-legend">
            <span><i className="tree-legend-op">→</i> sequence</span>
            <span><i className="tree-legend-op">×</i> choice</span>
            <span><i className="tree-legend-op">⊻</i> inclusive choice</span>
            <span><i className="tree-legend-op">∧</i> parallel</span>
            <span><i className="tree-legend-op">↺</i> loop</span>
          </div>
          {actions}
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
          aria-label="Process tree"
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
          onDoubleClick={() => setView(layout.fit)}
        >
          <g>
            {layout.edges.map((edge) => (
              <path
                key={edge.key}
                className={edge.redo ? "tree-edge redo" : "tree-edge"}
                d={edge.path}
              />
            ))}
          </g>
          <g>
            {layout.nodes.map((node) => {
              if (node.kind === "operator") {
                return (
                  <g className={`tree-op op-${node.operator}`} key={node.id}>
                    <title>{OPERATOR_NAME[node.operator] ?? node.operator}</title>
                    <circle
                      className="tree-op-box"
                      cx={node.x}
                      cy={node.y}
                      r={node.w / 2}
                    />
                    <text
                      className="tree-op-symbol"
                      x={node.x}
                      y={node.y}
                      textAnchor="middle"
                      dominantBaseline="central"
                    >
                      {node.label}
                    </text>
                  </g>
                );
              }
              if (node.kind === "tau") {
                return (
                  <g className="tree-tau" key={node.id}>
                    <title>Silent step (τ)</title>
                    <rect
                      className="tree-tau-box"
                      x={node.x - node.w / 2}
                      y={node.y - node.h / 2}
                      width={node.w}
                      height={node.h}
                      rx={3}
                    />
                    <text
                      className="tree-tau-label"
                      x={node.x}
                      y={node.y}
                      textAnchor="middle"
                      dominantBaseline="central"
                    >
                      τ
                    </text>
                  </g>
                );
              }
              return (
                <g className="tree-leaf" key={node.id}>
                  <rect
                    className="tree-leaf-box"
                    x={node.x - node.w / 2}
                    y={node.y - node.h / 2}
                    width={node.w}
                    height={node.h}
                    rx={3}
                  />
                  <text
                    className="tree-leaf-name"
                    x={node.x}
                    y={node.y}
                    textAnchor="middle"
                    dominantBaseline="central"
                  >
                    {node.label}
                  </text>
                </g>
              );
            })}
          </g>
        </svg>
        <div className="map-hint">scroll to zoom · drag to pan</div>
      </div>
    </section>
  );
}
