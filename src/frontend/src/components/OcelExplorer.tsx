import { OcpnDiscoveryControls } from "./OcpnDiscoveryControls";
import { useState } from "react";
import { type OcelDiscoveryResult } from "../types";
import { type OcelDiscoveryOptions } from "../api";
import { OCEL_MODEL_VARIANTS, DEFAULT_OCDFG_PERFORMANCE, OCDFG_EDGE_METRIC_ROLES, defaultOcdfgEdgeMetricsSettings, ocelExplorerOptions, type OcelExplorerSettings, type OcdfgEdgeMetricsSettings } from "../ocel-explorer-options";
import { OcdfgEdgeMetrics } from "./OcdfgEdgeMetrics";
import { OcdfgModel } from "./OcdfgModel";
import { OcelGraphModel } from "./OcelGraphModel";
import { OcelNetModel } from "./OcelNetModel";

interface OcelExplorerProps {
  result: OcelDiscoveryResult;
  busy: boolean;
  onDiscover: (options: OcelDiscoveryOptions) => void;
}

export function OcdfgPerformanceControls({settings,disabled,onChange}:{settings:NonNullable<OcelExplorerSettings["ocdfgPerformance"]>;disabled:boolean;onChange:(settings:NonNullable<OcelExplorerSettings["ocdfgPerformance"]>)=>void}) {
  return <fieldset className="query-fields" disabled={disabled}>
    <label className="ocel-check"><input type="checkbox" checked={settings.enabled} onChange={event=>onChange({...settings,enabled:event.target.checked})}/>Configure edge performance timing</label>
    {settings.enabled && <>
      <label className="ocel-check"><input type="checkbox" checked={settings.businessHours} onChange={event=>onChange({...settings,businessHours:event.target.checked})}/>Count only working time</label>
      {settings.businessHours && <>
        <label className="filters-field"><span>Weekly working schedule</span><select value={settings.slotsMode} onChange={event=>onChange({...settings,slotsMode:event.target.value as "default"|"custom"})}><option value="default">Monday–Friday, 07:00–17:00</option><option value="custom">Custom weekly slots</option></select></label>
        {settings.slotsMode === "custom" && <label className="query-text"><span>Weekly slots JSON</span><textarea spellCheck={false} rows={4} value={settings.slotsJson} onChange={event=>onChange({...settings,slotsJson:event.target.value})}/></label>}
        <p>Slots are [start, end] seconds from Monday 00:00 in local wall time. Overlapping slots are merged. An empty list counts zero working seconds. Holiday calendars do not change these timings.</p>
      </>}
      <p>Apply to rediscover performance for the current OCEL. Disabling working time counts elapsed time.</p>
    </>}
  </fieldset>;
}

export function OcdfgEdgeMetricsControls({settings,disabled,onChange}:{settings:OcdfgEdgeMetricsSettings;disabled:boolean;onChange:(settings:OcdfgEdgeMetricsSettings)=>void}) {
  return <fieldset className="query-fields" disabled={disabled}>
    <label className="ocel-check"><input type="checkbox" checked={settings.enabled} onChange={event=>onChange({...settings,enabled:event.target.checked})}/>Include edge association metrics</label>
    {settings.enabled && <>
      <p>Include the complete association report when applying discovery. Role overrides preserve literal names, including empty names and line breaks.</p>
      <details><summary>Optional association role names</summary>{OCDFG_EDGE_METRIC_ROLES.map(role=>{
        const selected=settings.roles[role.id];
        const change=(next:{custom:boolean;key:string})=>onChange({...settings,roles:{...settings.roles,[role.id]:next}});
        return <div key={role.id}>
          <label className="ocel-check"><input type="checkbox" checked={selected.custom} onChange={event=>change({...selected,custom:event.target.checked})}/>Override association {role.label.toLowerCase()} key</label>
          {selected.custom && <label className="query-text"><span>{role.label} key</span><textarea rows={2} spellCheck={false} aria-label={`Association ${role.id} key`} value={selected.key} onChange={event=>change({...selected,key:event.target.value})}/></label>}
        </div>;
      })}</details>
    </>}
  </fieldset>;
}

/** Discover and inspect all five object-centric model families from one upload. */
export function OcelExplorer({ result, busy, onDiscover }: OcelExplorerProps) {
  const [settings,setSettings] = useState<OcelExplorerSettings>({activityAttribute:"",objectTypeAttribute:"",annotated:true,replayDiagnostics:false,ocdfgPerformance:{...DEFAULT_OCDFG_PERFORMANCE},ocdfgEdgeMetrics:defaultOcdfgEdgeMetricsSettings()});
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
          <OcpnDiscoveryControls scope="OCEL discovery" options={settings.ocpnDiscovery} disabled={busy} onChange={ocpnDiscovery => setSettings({...settings,ocpnDiscovery})} />
          <label className="ocel-check"><input type="checkbox" disabled={busy} checked={settings.annotated} onChange={e => setSettings({...settings,annotated:e.target.checked})} />Model annotations</label>
          <label className="ocel-check"><input type="checkbox" disabled={busy} checked={settings.replayDiagnostics} onChange={e => setSettings({...settings,replayDiagnostics:e.target.checked})} />Replay diagnostics</label>
        </>}
        <button className="btn-ghost" type="submit" disabled={busy}>Apply</button>
      </div>}
      {selected === "ocdfg" && <OcdfgPerformanceControls settings={settings.ocdfgPerformance ?? DEFAULT_OCDFG_PERFORMANCE} disabled={busy} onChange={ocdfgPerformance => setSettings({...settings,ocdfgPerformance})} />}
      {selected === "ocdfg" && <OcdfgEdgeMetricsControls settings={settings.ocdfgEdgeMetrics ?? defaultOcdfgEdgeMetricsSettings()} disabled={busy} onChange={ocdfgEdgeMetrics=>setSettings({...settings,ocdfgEdgeMetrics})}/>}
      </form>
      {selected === "ocdfg" ? (
        <OcdfgModel result={result} />
      ) : selected === "ocpn" || selected === "saw" ? (
        <OcelNetModel key={selected} result={result} />
      ) : (
        <OcelGraphModel result={result} kind={selected} />
      )}
      {selected === "ocdfg" && result.edge_metrics && <OcdfgEdgeMetrics report={result.edge_metrics}/>}
    </>
  );
}
