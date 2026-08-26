import { type ReactNode } from "react";
import { type MineraExport, type PowlNode } from "../types";

interface PowlModelProps {
  model: PowlNode;
  data: MineraExport;
  title?: string;
  actions?: ReactNode;
}

const frequencyLabel = (node: Extract<PowlNode, { kind: "transition" }>) => {
  const max = node.frequency.maximum;
  if (max === null) return `${node.frequency.minimum}–∞`;
  return max === node.frequency.minimum
    ? `${node.frequency.minimum}`
    : `${node.frequency.minimum}–${max}`;
};

/** Compact, nested rendering of a recursive POWL model. */
export function PowlModel({ model, data, title, actions }: PowlModelProps) {
  return (
    <section className="panel map-panel reveal">
      <div className="panel-head">
        <h2>POWL model</h2>
        {title && <span className="panel-meta">{title}</span>}
        <div className="map-controls">{actions}</div>
      </div>
      <div className="powl-model" aria-label="POWL model">
        <PowlNodeView node={model} data={data} path="root" />
      </div>
    </section>
  );
}

function PowlNodeView({ node, data, path }: { node: PowlNode; data: MineraExport; path: string }) {
  if (node.kind === "transition") {
    const label = data.activities[node.activity]?.name ?? `activity ${node.activity}`;
    return (
      <div className="powl-node powl-transition" data-path={path}>
        <strong>{label}</strong>
        <small>#{node.activity} · frequency {frequencyLabel(node)}</small>
      </div>
    );
  }
  if (node.kind === "silent") {
    return <div className="powl-node powl-silent" data-path={path}>τ <small>silent</small></div>;
  }
  const label = node.kind === "xor" ? "XOR" : node.kind === "loop" ? "LOOP" : "STRICT PARTIAL ORDER";
  return (
    <div className={`powl-node powl-composite powl-${node.kind}`} data-path={path}>
      <div className="powl-composite-title">{label}</div>
      <div className="powl-children">
        {node.children.map((child, index) => (
          <div className="powl-child" key={`${path}-${index}`}>
            <span className="powl-child-index">{index}</span>
            <PowlNodeView node={child} data={data} path={`${path}-${index}`} />
          </div>
        ))}
      </div>
      {node.kind === "strict_partial_order" && node.order.length > 0 && (
        <div className="powl-order" aria-label="strict partial-order edges">
          <span>Order edges</span>
          {node.order.map((edge, index) => (
            <span className="powl-order-edge" key={`${path}-edge-${index}`}>
              {edge.source} → {edge.target}
            </span>
          ))}
        </div>
      )}
    </div>
  );
}
