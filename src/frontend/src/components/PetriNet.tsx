import { type ReactNode, useEffect, useId, useRef, useState } from "react";
import { type ViewBox } from "../layout";
import { panViewBox, zoomViewBoxAt } from "../graph-viewport";
import { type PetriLayout } from "../petri-layout";
import {
  decorationCoverage,
  hasTimedDecoration,
  placeTooltip,
  placeWaitLabel,
  transitionCountLabel,
} from "../petri-decoration";
import { type PetriDecoration } from "../types";

type NetMode = "plain" | "frequency" | "performance";

interface PetriNetProps {
  layout: PetriLayout;
  heading?: string;
  /** Caption shown next to the panel title, e.g. the selected miner. */
  title?: string;
  /** Extra controls (e.g. an export button) shown in the panel header. */
  actions?: ReactNode;
  /** Replay annotations (firing counts, place waits); null on older exports. */
  decoration?: PetriDecoration | null;
  annotations?: {
    variableTransitions?: ReadonlySet<number>;
    variableTransitionDescription?: string;
    transitionLabels?: ReadonlyMap<number, string>;
    transitionTooltips?: ReadonlyMap<number, string>;
    placeTooltips?: ReadonlyMap<number, string>;
    placeColors?: ReadonlyMap<number, string>;
  };
  disconnectedDescription?: string;
}

// Renders an Alpha Miner workflow net: places as circles (source and sink
// marked), transitions as labeled boxes, arcs as arrowed curves. Pan/zoom
// behaviour mirrors the process-tree view. With a replay decoration the view
// gains the process map's frequency/performance modes: frequency badges each
// transition with how often it fired, performance labels each place with the
// mean time tokens rested there.
export function PetriNet({ layout, title, actions, decoration, annotations,
  heading = "Petri net", disconnectedDescription = "short-loop activities the Alpha Miner cannot place" }: PetriNetProps) {
  const arrowId = `petri-arrow-${useId().replace(/[^a-zA-Z0-9_-]/g, "")}`;
  const svgRef = useRef<SVGSVGElement>(null);
  const [view, setView] = useState<ViewBox>(layout.fit);
  const [mode, setMode] = useState<NetMode>("plain");
  const decorated = decoration && decoration.decorated_traces > 0 ? decoration : null;
  const timed = hasTimedDecoration(decorated);
  const showCounts = mode === "frequency" && decorated !== null;
  const showWaits = mode === "performance" && decorated !== null && timed;
  const zoom = (factor: number) => setView(current => {
    const w = Math.min(Math.max(current.w * factor, 120), layout.fit.w * 4);
    const h = current.h * w / current.w;
    return { x: current.x + (current.w-w)/2, y: current.y + (current.h-h)/2, w,h };
  });

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
      setView((view) => {
        const newW = Math.min(Math.max(view.w * factor, 160), layout.fit.w * 4);
        return zoomViewBoxAt(view, rect, event.clientX, event.clientY, newW);
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
        <h2>{heading}</h2>
        {title && <span className="panel-meta">{title}</span>}
        <div className="map-controls">
          {decorated && (
            <div className="map-mode" role="group" aria-label="Net annotation">
              <button
                className={mode === "plain" ? "mode-btn active" : "mode-btn"}
                onClick={() => setMode("plain")}
              >
                Plain
              </button>
              <button
                className={mode === "frequency" ? "mode-btn active" : "mode-btn"}
                onClick={() => setMode("frequency")}
              >
                Frequency
              </button>
              {timed && (
                <button
                  className={
                    mode === "performance" ? "mode-btn active" : "mode-btn"
                  }
                  onClick={() => setMode("performance")}
                >
                  Performance
                </button>
              )}
            </div>
          )}
          {decorated && mode !== "plain" && (
            <span
              className="map-stat"
              title="Annotations come from token replay of the cases that fit the model"
            >
              {decorationCoverage(decorated)}
            </span>
          )}
          <div className="tree-legend">
            <span><i className="petri-legend-place" /> place</span>
            <span><i className="petri-legend-trans" /> activity</span>
          </div>
          {actions}
          <button className="btn-ghost" aria-label="Zoom in" onClick={() => zoom(.72)}>+</button>
          <button className="btn-ghost" aria-label="Zoom out" onClick={() => zoom(1/.72)}>−</button>
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
          aria-label={heading}
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
          <defs>
            <marker
              id={arrowId}
              viewBox="0 0 10 10"
              refX="9"
              refY="5"
              markerWidth="7"
              markerHeight="7"
              orient="auto-start-reverse"
            >
              <path className="petri-arrowhead" d="M 0 0 L 10 5 L 0 10 z" />
            </marker>
          </defs>
          <g>
            {layout.arcs.map((arc) => (
              <path
                key={arc.key}
                className={`petri-arc${annotations?.variableTransitions?.has(arc.transition) ? " variable" : ""}`}
                d={arc.path}
                markerEnd={`url(#${arrowId})`}
              >
                {annotations?.variableTransitions?.has(arc.transition) && <title>{annotations.variableTransitionDescription??'Variable object count'}</title>}
              </path>
            ))}
          </g>
          <g>
            {layout.places.map((place) => {
              const tooltip = decorated ? placeTooltip(decorated, place.id) : null;
              const waitLabel =
                showWaits && decorated ? placeWaitLabel(decorated, place.id) : null;
              return (
                <g className={`petri-place ${place.kind}`} key={`p${place.id}`}>
                  <title>
                    {(place.kind === "source"
                      ? "Source place (initial marking)"
                      : place.kind === "sink"
                        ? "Sink place (final marking)"
                        : "Place") + (tooltip ? ` — ${tooltip}` : "") +
                        (annotations?.placeTooltips?.has(place.id) ? ` — ${annotations.placeTooltips.get(place.id)}` : "")}
                  </title>
                  <circle
                    className="petri-place-box"
                    cx={place.x}
                    cy={place.y}
                    r={place.r}
                    style={annotations?.placeColors?.has(place.id) ? { stroke: annotations.placeColors.get(place.id) } : undefined}
                  />
                  {place.kind === "source" && (
                    <circle
                      className="petri-token"
                      cx={place.x}
                      cy={place.y}
                      r={place.r * 0.4}
                    />
                  )}
                  {waitLabel && (
                    <text
                      className="petri-wait-label"
                      x={place.x}
                      y={place.y + place.r + 13}
                      textAnchor="middle"
                    >
                      {waitLabel}
                    </text>
                  )}
                </g>
              );
            })}
          </g>
          {layout.unconnectedY !== null && (
            <g className="petri-lane">
              <line
                x1={layout.fit.x + 12}
                y1={layout.unconnectedY}
                x2={layout.fit.x + layout.fit.w - 12}
                y2={layout.unconnectedY}
              />
              <text
                className="petri-lane-label"
                x={layout.fit.x + 16}
                y={layout.unconnectedY + 16}
              >
                unconnected — {disconnectedDescription}
              </text>
            </g>
          )}
          <g>
            {layout.transitions.map((transition) => {
              const classes = ["petri-trans"];
              if (transition.silent) classes.push("silent");
              if (transition.disconnected) classes.push("disconnected");
              const count =
                showCounts && decorated
                  ? transitionCountLabel(decorated, transition.id)
                  : null;
              if (count === "0") classes.push("unfired");
              return (
                <g className={classes.join(" ")} key={`t${transition.id}`}>
                  <title>
                    {(transition.silent
                      ? "Silent step (τ)"
                      : transition.disconnected
                        ? `${transition.label} — ${disconnectedDescription}`
                        : transition.label) +
                      (count !== null ? ` — fired ${count}×` : "") +
                      (annotations?.transitionTooltips?.has(transition.id) ? ` — ${annotations.transitionTooltips.get(transition.id)}` : "")}
                  </title>
                  <rect
                    className="petri-trans-box"
                    x={transition.x - transition.w / 2}
                    y={transition.y - transition.h / 2}
                    width={transition.w}
                    height={transition.h}
                    rx={3}
                  />
                  <text
                    className="petri-trans-label"
                    x={transition.x}
                    y={transition.y}
                    textAnchor="middle"
                    dominantBaseline="central"
                  >
                    {transition.label}
                  </text>
                  {count !== null && (
                    <text
                      className="petri-count-label"
                      x={transition.x}
                      y={transition.y - transition.h / 2 - 6}
                      textAnchor="middle"
                    >
                      {count}
                    </text>
                  )}
                  {annotations?.transitionLabels?.has(transition.id) && (
                    <text className="petri-count-label" x={transition.x} y={transition.y - transition.h / 2 - 7} textAnchor="middle">
                      {annotations.transitionLabels.get(transition.id)}
                    </text>
                  )}
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
