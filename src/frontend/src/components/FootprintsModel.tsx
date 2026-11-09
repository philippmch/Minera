import { useState } from "react";
import {
  FOOTPRINTS_DISCOVERY_VARIANTS,
  type AggregateFootprint,
  type AggregateFootprints,
  type FootprintsDiscoveryVariant,
  type FootprintsTraceDiscovery,
  type FootprintRelation,
  type MineraExport,
} from "../types";

interface Props { data: MineraExport; footprints: AggregateFootprints; title?: string; }

function name(data: MineraExport, id: unknown): string {
  return typeof id === "number" && Number.isInteger(id) && id >= 0 && id < data.activities.length
    ? data.activities[id]?.name ?? `unknown (${id})` : `unknown (${String(id)})`;
}
function pair(data: MineraExport, relation: FootprintRelation): string {
  return `${name(data, relation.source)} → ${name(data, relation.target)}`;
}
function relations(data: MineraExport, values: FootprintRelation[] | undefined, cap = 80) {
  return (values ?? []).slice(0, cap).map((relation, index) => <li key={`${relation.source}-${relation.target}-${index}`}>{pair(data, relation)}</li>);
}
function Table({ title, data, values }: { title: string; data: MineraExport; values?: FootprintRelation[] }) {
  return <div className="footprints-table"><h4>{title} <small>({values?.length ?? 0})</small></h4>{values?.length ? <ul>{relations(data, values)}</ul> : <p className="muted">None.</p>}</div>;
}

export function FootprintsModel({ data, footprints, title = "Footprints" }: Props) {
  const available = FOOTPRINTS_DISCOVERY_VARIANTS.filter((variant) => footprints.discovery?.[variant] !== undefined);
  const [selected, setSelected] = useState(available[0] ?? "");
  const selectedVariant = selected as FootprintsDiscoveryVariant;
  const selectedDiscovery = selected ? footprints.discovery?.[selectedVariant] : undefined;
  const current: AggregateFootprint | undefined =
    selectedVariant !== "trace_by_trace" && selectedDiscovery && !Array.isArray(selectedDiscovery)
      ? selectedDiscovery : undefined;
  const traceDiscovery: FootprintsTraceDiscovery[] =
    selectedVariant === "trace_by_trace" && Array.isArray(selectedDiscovery)
      ? selectedDiscovery : [];
  const metrics = footprints.metrics;
  return <section className="model-card footprints-model reveal">
    <div className="model-card-header"><div><p className="eyebrow">Aggregate discovery</p><h2>{title}</h2></div></div>
    {available.length ? <>
      <div className="map-mode" role="tablist" aria-label="Footprints discovery variant">
        {available.map((variant) => <button key={variant} role="tab" aria-selected={selected === variant} className={selected === variant ? "mode-btn active" : "mode-btn"} aria-pressed={selected === variant} onClick={() => setSelected(variant)}>{variant.replaceAll("_", " ")}</button>)}
      </div>
      {selectedVariant === "trace_by_trace"
        ? <TraceDiscovery data={data} rows={traceDiscovery} />
        : <FootprintCard data={data} footprint={current} />}
    </> : <p className="muted">No footprint discovery variant is available in this export.</p>}
    {metrics && <div className="footprints-metrics"><strong>Metrics</strong><span>precision {metrics.precision === undefined ? "—" : metrics.precision.toFixed(3)}</span><span>log fitness {metrics.log_fitness === undefined ? "—" : metrics.log_fitness.toFixed(3)}</span><span>fit traces {metrics.perc_fit_traces === undefined ? "—" : `${metrics.perc_fit_traces.toFixed(1)}%`}</span></div>}
    <FootprintsConformance data={data} footprints={footprints} />
  </section>;
}

function TraceDiscovery({ data, rows }: { data: MineraExport; rows: FootprintsTraceDiscovery[] }) {
  if (!rows.length) return <p className="muted">No trace footprints are available.</p>;
  return <div className="footprints-trace-discovery">
    <h3>Trace footprints <small>(first {Math.min(100, rows.length)} of {rows.length})</small></h3>
    {rows.slice(0, 100).map((row) => <article className="footprints-trace-card" key={row.trace_index}>
      <header><strong>{row.case_id || `trace ${row.trace_index}`}</strong><span>trace {row.trace_index}</span></header>
      <FootprintCard data={data} footprint={row.footprint} />
    </article>)}
  </div>;
}

// A null activity (Polars adapter) has no activity id and is shown as "null".
const withNull = (names: string[], includeNull?: boolean) => includeNull ? [...names, "null"] : names;

function FootprintCard({ data, footprint }: { data: MineraExport; footprint?: AggregateFootprint }) {
  if (!footprint) return null;
  return <div className="footprints-card">
    <div className="footprints-summary"><span>activities {(footprint.activities?.length ?? 0) + (footprint.activities_include_null ? 1 : 0)}</span><span>skippable {footprint.skippable === undefined ? "—" : String(footprint.skippable)}</span><span>min length {footprint.min_trace_length ?? "—"}</span><span>max without loops {footprint.max_trace_length_without_loops ?? "—"}</span></div>
    <div className="footprints-tables"><Table title="Sequence" data={data} values={footprint.sequence} /><Table title="Parallel" data={data} values={footprint.parallel} /><Table title="Atomic interleaving" data={data} values={footprint.atomic_interleaving} /></div>
    <div className="footprints-summary"><span>starts: {withNull((footprint.start_activities ?? []).map((id) => name(data, id)), footprint.start_activities_include_null).join(", ") || "—"}</span><span>ends: {withNull((footprint.end_activities ?? []).map((id) => name(data, id)), footprint.end_activities_include_null).join(", ") || "—"}</span><span>always: {(footprint.activities_always_happening ?? []).map((id) => name(data, id)).join(", ") || "—"}</span></div>
  </div>;
}

function FootprintsConformance({ data, footprints }: Props) {
  const conf = footprints.conformance;
  if (!conf) return null;
  const traces = conf.trace_extensive ?? [];
  return <section className="footprints-conformance"><h3>Conformance</h3>
    <div className="footprints-summary"><span>model: {conf.model.replaceAll("_", " ")}</span>{conf.log_model && <span>strict: {String(conf.log_model.strict)}</span>}</div>
    {conf.log_model && <Table title={`Log-model violations${conf.log_model.is_fit ? "" : " (unfit)"}`} data={data} values={conf.log_model.violations} />}
    {conf.log_extensive && <div className="footprints-summary"><span>Log-extensive: {conf.log_extensive.is_fit === false ? "unfit" : "fit"}</span><span>violations {conf.log_extensive.footprints?.length ?? 0}</span></div>}
    {traces.length > 0 && <div className="footprints-traces"><h4>Trace-extensive (first {Math.min(100, traces.length)})</h4>{traces.slice(0, 100).map((trace) => <div className="footprints-trace" key={trace.trace_index}><strong>{trace.case_id || `trace ${trace.trace_index}`}</strong><span>{trace.is_fit ? "fit" : "unfit"}</span></div>)}</div>}
  </section>;
}
