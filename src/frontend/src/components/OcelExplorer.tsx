import { type OcelDiscoveryResult, type OcelDiscoveryVariant } from "../types";
import { OcdfgModel } from "./OcdfgModel";
import { OcelGraphModel } from "./OcelGraphModel";

interface OcelExplorerProps {
  result: OcelDiscoveryResult;
  busy: boolean;
  onVariantChange: (variant: OcelDiscoveryVariant) => void;
}

const GRAPH_VARIANTS: readonly { id: "ocdfg" | "etot" | "otg"; label: string }[] = [
  { id: "ocdfg", label: "Object-centric directly-follows" },
  { id: "etot", label: "Event-to-object-type" },
  { id: "otg", label: "Object-type relations" },
];

/** Selects among the graph projections that have deterministic visual views. */
export function OcelExplorer({ result, busy, onVariantChange }: OcelExplorerProps) {
  const graphVariant = result.variant === "etot" || result.variant === "otg"
    ? result.variant
    : "ocdfg";
  return (
    <>
      <div className="ocel-view-toolbar">
        <label className="threshold">
          <span className="threshold-label">projection</span>
          <select
            aria-label="Object-centric graph projection"
            value={graphVariant}
            disabled={busy}
            onChange={(event) => onVariantChange(event.target.value as OcelDiscoveryVariant)}
          >
            {GRAPH_VARIANTS.map((variant) => (
              <option key={variant.id} value={variant.id}>{variant.label}</option>
            ))}
          </select>
        </label>
        {busy ? <span className="panel-meta">discovering…</span> : null}
      </div>
      {graphVariant === "ocdfg" ? (
        <OcdfgModel result={result} />
      ) : (
        <OcelGraphModel result={result} kind={graphVariant} />
      )}
    </>
  );
}
