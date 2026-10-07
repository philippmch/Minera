import { useEffect, useId, useMemo, useRef, useState, type ReactNode } from "react";
import { modelGraphLayout, type ModelGraph as Graph } from "../model-graph";

export function ModelGraph({ graph, title, style = "flow", selected, onSelect, actions, minimumLabel = "Minimum absolute tie weight" }: { graph: Graph; title: string; style?: "flow" | "network"; selected?: string; onSelect?: (id: string) => void; actions?: ReactNode; minimumLabel?: string }) {
  const [minimum, setMinimum] = useState(0);
  const layout = useMemo(() => modelGraphLayout(graph, style, minimum, selected), [graph, style, minimum, selected]);
  const [view, setView] = useState(layout.fit);
  const svg = useRef<SVGSVGElement>(null);
  const pan = useRef<{ x: number; y: number } | null>(null);
  const arrow = useId().replace(/:/g, "") + "arrow";
  useEffect(() => setView(layout.fit), [layout.fit]);
  useEffect(() => {
    const element = svg.current; if (!element) return;
    const wheel = (event: WheelEvent) => {
      event.preventDefault(); const rect = element.getBoundingClientRect(); if (!rect.width) return;
      const px = (event.clientX-rect.left)/rect.width, py = (event.clientY-rect.top)/rect.height;
      setView(current => { const w = Math.min(layout.fit.w * 4, Math.max(100, current.w * Math.exp(event.deltaY*0.0015))); const h = current.h*w/current.w; return { x: current.x+(current.w-w)*px, y: current.y+(current.h-h)*py, w, h }; });
    };
    element.addEventListener("wheel", wheel, { passive: false }); return () => element.removeEventListener("wheel", wheel);
  }, [layout.fit]);
  const zoom = (factor: number) => setView(current => { const w = Math.min(layout.fit.w*4, Math.max(100, current.w*factor)); const h = current.h*w/current.w; return { x: current.x+(current.w-w)/2, y: current.y+(current.h-h)/2, w, h }; });
  return <section className="panel model-graph-panel">
    <div className="panel-head"><h2>{title}</h2><div className="model-graph-toolbar">
      <button className="btn-ghost" aria-label={`Zoom in ${title}`} onClick={() => zoom(0.8)}>+</button>
      <button className="btn-ghost" aria-label={`Zoom out ${title}`} onClick={() => zoom(1.25)}>−</button>
      <button className="btn-ghost" onClick={() => setView(layout.fit)}>Fit graph</button>{actions}
    </div></div>
    <div className="model-graph-controls"><span className="panel-meta">{layout.nodes.length} / {layout.totalNodes} nodes · {layout.edges.length} / {layout.totalEdges} native entries</span>
      <label>{minimumLabel} <input type="number" min="0" step="any" value={minimum} onChange={event => { const value = Number(event.target.value); setMinimum(Number.isFinite(value) && value >= 0 ? value : 0); }} /></label>
    </div>
    {(layout.nodes.length < layout.totalNodes || layout.edges.length < layout.qualifiedEdges) && <p className="model-graph-note">The diagram shows a bounded selection. Tables and JSON retain every native entry; selecting a node brings its neighborhood into view.</p>}
    {!layout.nodes.length ? <p className="model-empty">No nodes in this model.</p> : <div className="model-graph-wrap">
      <svg ref={svg} role="group" aria-label={`${title} diagram`} viewBox={`${view.x} ${view.y} ${view.w} ${view.h}`}
        onPointerDown={event => { if (event.button !== 0 || (event.target as Element).closest(".model-graph-node")) return; pan.current = {x:event.clientX,y:event.clientY}; event.currentTarget.setPointerCapture(event.pointerId); }}
        onPointerMove={event => { const p = pan.current; if (!p) return; const rect = event.currentTarget.getBoundingClientRect(); if (!rect.width) return; setView(current => ({...current,x:current.x-(event.clientX-p.x)*current.w/rect.width,y:current.y-(event.clientY-p.y)*current.w/rect.width})); pan.current = {x:event.clientX,y:event.clientY}; }}
        onPointerUp={() => {pan.current=null;}} onPointerCancel={() => {pan.current=null;}} onDoubleClick={() => setView(layout.fit)}>
        <defs><marker id={arrow} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 Z" fill="currentColor" /></marker></defs>
        {layout.edges.map(edge => <g key={edge.id} className={edge.value < 0 ? "model-graph-edge negative" : "model-graph-edge"}><title>{edge.source} → {edge.target}: {edge.label}</title><path d={edge.path} fill="none" strokeWidth={edge.width} markerEnd={edge.directed ? `url(#${arrow})` : undefined} /><text x={edge.x} y={edge.y} textAnchor="middle">{edge.label.length > 32 ? edge.label.slice(0,31)+"…" : edge.label}</text></g>)}
        {layout.nodes.map(node => <g key={node.id} role={onSelect ? "button" : undefined} tabIndex={onSelect ? 0 : undefined} aria-label={`${node.id}: ${node.label}`} className={`model-graph-node${selected === node.id ? " selected" : ""}${node.outsideRoot ? " outside-root" : ""}`} onClick={() => onSelect?.(node.id)} onKeyDown={event => { if (event.key === "Enter" || event.key === " ") {event.preventDefault();onSelect?.(node.id);} }}>
          <title>{node.id}: {node.label}{node.detail ? ` — ${node.detail}` : ""}</title>
          <rect x={node.x-node.w/2} y={node.y-node.h/2} width={node.w} height={node.h} rx={style === "network" ? 32 : 8} />
          {node.start && <path className="model-graph-boundary" d={`M ${node.x-node.w/2-18} ${node.y} L ${node.x-node.w/2-4} ${node.y} M ${node.x-node.w/2-9} ${node.y-4} L ${node.x-node.w/2-4} ${node.y} L ${node.x-node.w/2-9} ${node.y+4}`} />}
          {node.end && <rect className="model-graph-end" x={node.x-node.w/2+5} y={node.y-node.h/2+5} width={node.w-10} height={node.h-10} rx={5} />}
          <text x={node.x} y={node.y-7} textAnchor="middle">{node.label.length > 26 ? node.label.slice(0,25)+"…" : node.label}</text>
          <text x={node.x} y={node.y+16} textAnchor="middle" className="model-graph-detail">{node.detail?.slice(0,32) ?? node.id}</text>
        </g>)}
      </svg>
    </div>}
    <p className="model-graph-note">Scroll to zoom, drag the background to pan.{graph.edges.some(edge=>edge.value<0) ? " Dashed ties have negative weights." : ""}{graph.nodes.some(node=>node.outsideRoot) ? " Dashed nodes are outside the root state set." : ""}</p>
  </section>;
}
