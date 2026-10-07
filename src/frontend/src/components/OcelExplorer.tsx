import { useState } from "react";
import { type OcelDiscoveryResult } from "../types";
import { type OcelDiscoveryOptions } from "../api";
import { OCEL_MODEL_VARIANTS, ocelExplorerOptions, type OcelExplorerSettings } from "../ocel-explorer-options";
import { OcdfgModel } from "./OcdfgModel";
import { OcelGraphModel } from "./OcelGraphModel";
import { OcelNetModel } from "./OcelNetModel";

interface OcelExplorerProps {
  result: OcelDiscoveryResult;
  busy: boolean;
  onDiscover: (options: OcelDiscoveryOptions) => void;
}

/** Discover and inspect all five object-centric model families from one upload. */
export function OcelExplorer({ result, busy, onDiscover }: OcelExplorerProps) {
  const [settings,setSettings] = useState<OcelExplorerSettings>({activityAttribute:"",objectTypeAttribute:"",annotated:true,replayDiagnostics:false});
  const selected = result.variant;
  const hasSelectors = selected === "ocdfg" || selected === "ocpn" || selected === "saw";
  return (
    <>
      <form className="ocel-discovery-controls" onSubmit={event => {event.preventDefault();if(!busy)onDiscover(ocelExplorerOptions(selected,settings));}}>
      <div className="ocel-view-toolbar">
        <label className="threshold">
          <span className="threshold-label">model</span>
          <select
            aria-label="Object-centric model"
            value={selected}
            disabled={busy}
            onChange={(event) => {
              const variant = OCEL_MODEL_VARIANTS.find(v => v.id === event.target.value)?.id;
              if(variant)onDiscover(ocelExplorerOptions(variant,settings));
            }}
          >
            {OCEL_MODEL_VARIANTS.map((variant) => (
              <option key={variant.id} value={variant.id}>{variant.label}</option>
            ))}
          </select>
        </label>
        {busy ? <span className="panel-meta">discovering…</span> : null}
      </div>
      {hasSelectors && <div className="ocel-options-row">
        <label>Activity attribute<input aria-label="OCEL activity attribute" disabled={busy} value={settings.activityAttribute} placeholder="Default activity" onChange={e => setSettings({...settings,activityAttribute:e.target.value})} /></label>
        <label>Object-type attribute<input aria-label="OCEL object-type attribute" disabled={busy} value={settings.objectTypeAttribute} placeholder="Default object type" onChange={e => setSettings({...settings,objectTypeAttribute:e.target.value})} /></label>
        {selected === "ocpn" && <>
          <label className="ocel-check"><input type="checkbox" disabled={busy} checked={settings.annotated} onChange={e => setSettings({...settings,annotated:e.target.checked})} />Model annotations</label>
          <label className="ocel-check"><input type="checkbox" disabled={busy} checked={settings.replayDiagnostics} onChange={e => setSettings({...settings,replayDiagnostics:e.target.checked})} />Replay diagnostics</label>
        </>}
        <button className="btn-ghost" type="submit" disabled={busy}>Apply</button>
      </div>}
      </form>
      {selected === "ocdfg" ? (
        <OcdfgModel result={result} />
      ) : selected === "ocpn" || selected === "saw" ? (
        <OcelNetModel key={selected} result={result} />
      ) : (
        <OcelGraphModel result={result} kind={selected} />
      )}
    </>
  );
}
