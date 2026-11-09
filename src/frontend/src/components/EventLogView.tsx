import { useMemo, useState } from "react";
import { downloadBlob } from "../download";
import { caseRowEntries } from "../app-logic";
import { type MineraExport, type Trace, numberFormat } from "../types";
import { loadedTraces, useTraces } from "../mine-traces";
import { TraceLoadNote } from "./TraceLoadNote";
import {
  type DfgExportOptions,
  type XesExportOptions,
  fetchDfg,
  fetchXes,
} from "../api";

const PAGE_SIZE = 50;

function formatTimestamp(value: string): string {
  return value ? value.replace("T", " ") : "—";
}

function CaseRow({
  trace,
  data,
  showResource,
  showLifecycle,
  showAttributes,
}: {
  trace: Trace;
  data: MineraExport;
  showResource: boolean;
  showLifecycle: boolean;
  showAttributes: boolean;
}) {
  const [open, setOpen] = useState(false);
  const first = trace.events[0]?.timestamp;
  const last = trace.events[trace.events.length - 1]?.timestamp;

  return (
    <li className={open ? "case open" : "case"}>
      <div className="case-row" onClick={() => setOpen(!open)}>
        <span className="case-toggle">{open ? "−" : "+"}</span>
        <span className="case-id">{trace.case_id}</span>
        <span className="case-events">
          {numberFormat.format(trace.events.length)} events
        </span>
        <span className="case-span">
          {formatTimestamp(first)}
          {last && last !== first ? ` → ${formatTimestamp(last)}` : ""}
        </span>
        <span className="case-seq">
          {trace.events.map((event, i) => (
            <i
              key={i}
              className="case-seq-dot"
              title={data.activities[event.activity].name}
            />
          ))}
        </span>
      </div>
      {open && trace.attributes && (
        <div className="case-attrs">
          {Object.entries(trace.attributes)
            .map(([key, value]) => `${key}: ${value}`)
            .join(" · ")}
        </div>
      )}
      {open && (
        <table className="case-events-table">
          <tbody>
            {trace.events.map((event, i) => (
              <tr key={i}>
                <td className="num">{i + 1}</td>
                <td>{data.activities[event.activity].name}</td>
                {showLifecycle && (
                  <td className="case-event-lifecycle">
                    {event.lifecycle ?? "—"}
                  </td>
                )}
                {showResource && (
                  <td className="case-event-resource">{event.resource ?? "—"}</td>
                )}
                {showAttributes && (
                  <td className="case-event-attrs">
                    {event.attributes
                      ? Object.entries(event.attributes)
                          .map(([key, value]) => `${key}: ${value}`)
                          .join(" · ")
                      : "—"}
                  </td>
                )}
                <td className="case-event-time">{formatTimestamp(event.timestamp)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </li>
  );
}

// Downloads the mined log — after any filters applied at mine time — as
// standard XES, the interchange format read by process-mining tools. The
// file is serialized server-side from the cached session, so it matches what
// Minera actually analyzed.
export function buildXesExportOptions(
  variant: XesExportOptions["variant"],
  compress: boolean,
  inferMetadata: boolean,
  preserveNulls = false,
): XesExportOptions {
  return { variant, compress, inferMetadata, preserveNulls };
}

export function XesButton({ dataset, available = true }: { dataset: string; available?: boolean }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [variant, setVariant] = useState<"line_by_line" | "etree">("line_by_line");
  const [compress, setCompress] = useState(false);
  const [inferMetadata, setInferMetadata] = useState(false);
  const [preserveNulls, setPreserveNulls] = useState(false);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchXes(buildXesExportOptions(variant, compress, inferMetadata, preserveNulls))
      .then((blob) => {
        const base = dataset.replace(/\.[^.]+$/, "") || "log";
        downloadBlob(blob, `${base}.xes${compress ? ".gz" : ""}`);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <select aria-label="XES export variant" value={variant} onChange={(event) => setVariant(event.target.value as "line_by_line" | "etree")}>
        <option value="line_by_line">Line by line</option>
        <option value="etree">Element tree</option>
      </select>
      <label><input type="checkbox" checked={compress} onChange={(event) => setCompress(event.target.checked)} /> gzip</label>
      <label><input aria-label="Infer XES metadata" type="checkbox" checked={inferMetadata} onChange={(event) => setInferMetadata(event.target.checked)} /> infer metadata</label>
      <label><input aria-label="Preserve null markers" type="checkbox" checked={preserveNulls} onChange={(event) => setPreserveNulls(event.target.checked)} /> preserve null markers</label>
      <button className="btn-ghost" onClick={download} disabled={busy || !available} title={available ? undefined : UNAVAILABLE_EXPORT} aria-describedby={available ? undefined : EXPORT_NOTE_ID}>
        {busy ? "Exporting…" : "Download log as XES"}
      </button>
      {error && <span className="pnml-error" role="alert">{error}</span>}
    </span>
  );
}

// The DFG filter controls as the panel labels them; an error that names a
// control by its wire name reads with the label instead.
const DFG_CONTROL_LABELS: ReadonlyArray<[string, string]> = [
  ["keep_all_activities", "Keep all activities"], ["activities_percentage", "Activities percentage"],
  ["paths_percentage", "Paths percentage"], ["most_common_paths", "Most common paths"],
  ["noise_threshold", "Noise threshold"], ["keep_connected", "Connected threshold"],
  ["to_activity", "To activity"], ["from_activity", "From activity"], ["contain_activity", "Contain activity"],
];
/** A DFG export error as the panel words it: controls by their labels, and a missing activity (the reference's
 * KeyError, kept verbatim in parentheses) named as such. */
export function dfgErrorText(message: string): string {
  const missing = /^KeyError: (['"])(.*)\1$/s.exec(message);
  if (missing) return `No activity “${missing[2]}” in this log (${message}).`;
  let text = message;
  for (const [wire, label] of DFG_CONTROL_LABELS) text = text.split(wire).join(`“${label}”`);
  return text;
}

function DfgButton({ dataset, available = true }: { dataset: string; available?: boolean }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [options, setOptions] = useState<DfgExportOptions>({});

  const setNumber = (name: keyof DfgExportOptions, value: string) => {
    setError(null);
    setOptions((current) => ({
      ...current,
      [name]: value.trim() ? Number(value) : undefined,
    }));
  };
  const setText = (name: keyof DfgExportOptions, value: string) => {
    // An emptied text field means "not set", as it does for the numeric fields.
    setError(null);
    setOptions((current) => ({ ...current, [name]: value === "" ? undefined : value }));
  };

  const download = () => {
    setBusy(true);
    setError(null);
    fetchDfg(options)
      .then((text) => {
        const blob = new Blob([text], { type: "text/plain" });
        const base = dataset.replace(/\.[^.]+$/, "") || "log";
        downloadBlob(blob, `${base}.dfg`);
      })
      .catch((cause: Error) => setError(dfgErrorText(cause.message)))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <details className="dfg-options">
        <summary>DFG filters</summary>
        <div className="dfg-options-grid">
          <label>
            Noise threshold
            <input
              type="number"
              min="0"
              step="any"
              value={options.noise_threshold ?? ""}
              onChange={(event) => setNumber("noise_threshold", event.target.value)}
            />
          </label>
          <label>
            Most common paths
            <input
              type="text"
              placeholder="A -> B"
              value={options.most_common_paths ?? ""}
              onChange={(event) => setText("most_common_paths", event.target.value)}
            />
          </label>
          <label>
            Activities percentage
            <input
              type="number"
              min="0"
              max="1"
              step="any"
              value={options.activities_percentage ?? ""}
              onChange={(event) => setNumber("activities_percentage", event.target.value)}
            />
          </label>
          <label>
            Paths percentage
            <input
              type="number"
              min="0"
              max="1"
              step="any"
              value={options.paths_percentage ?? ""}
              onChange={(event) => setNumber("paths_percentage", event.target.value)}
            />
          </label>
          <label>
            Connected threshold
            <input
              type="number"
              step="any"
              value={options.keep_connected ?? ""}
              onChange={(event) => setNumber("keep_connected", event.target.value)}
            />
          </label>
          <label>
            To activity
            <input
              type="text"
              value={options.to_activity ?? ""}
              onChange={(event) => setText("to_activity", event.target.value)}
            />
          </label>
          <label>
            From activity
            <input
              type="text"
              value={options.from_activity ?? ""}
              onChange={(event) => setText("from_activity", event.target.value)}
            />
          </label>
          <label>
            Contain activity
            <input
              type="text"
              value={options.contain_activity ?? ""}
              onChange={(event) => setText("contain_activity", event.target.value)}
            />
          </label>
          <label>
            <input
              type="checkbox"
              checked={options.keep_all_activities ?? false}
              onChange={(event) =>
                setOptions((current) => ({
                  ...current,
                  keep_all_activities: event.target.checked,
                }))
              }
            />
            Keep all activities
          </label>
        </div>
      </details>
      <button className="btn-ghost" onClick={download} disabled={busy || !available} title={available ? undefined : UNAVAILABLE_EXPORT} aria-describedby={available ? undefined : EXPORT_NOTE_ID}>
        {busy ? "Exporting…" : "Download DFG"}
      </button>
      {error && <span className="pnml-error" role="alert">{error}</span>}
    </span>
  );
}

// /api/xes and /api/dfg export the backend's cached log, which belongs to this analysis only while its source is live.
const UNAVAILABLE_EXPORT = "Open the original CSV or XES log to export it: this analysis is not the backend's active log.";
const EXPORT_NOTE_ID = "log-export-unavailable";

export function EventLogView({ data, available = true }: { data: MineraExport; available?: boolean }) {
  const load = useTraces(data);
  const traces = loadedTraces(load);
  // "unavailable" with a summary: a file saved without its cases.
  const waiting = load.status === "loading" || load.status === "error" || (load.status === "unavailable" && !!data.trace_summary);
  const [query, setQuery] = useState("");
  const [limit, setLimit] = useState(PAGE_SIZE);

  // Show the resource and attribute columns only when the log carries them.
  const showResource = useMemo(
    () => traces.some((trace) => trace.events.some((event) => event.resource)),
    [traces],
  );
  const showLifecycle = useMemo(
    () => traces.some((trace) => trace.events.some((event) => event.lifecycle)),
    [traces],
  );
  const showAttributes = useMemo(
    () =>
      traces.some((trace) => trace.events.some((event) => event.attributes)),
    [traces],
  );

  const filtered = useMemo(() => caseRowEntries(traces, query), [traces, query]);

  const timeSpan = useMemo(() => {
    let min = "";
    let max = "";
    for (const trace of traces) {
      for (const event of trace.events) {
        if (!event.timestamp) continue;
        if (!min || event.timestamp < min) min = event.timestamp;
        if (!max || event.timestamp > max) max = event.timestamp;
      }
    }
    return min ? { min, max } : null;
  }, [traces]);

  return (
    <section className="panel log-panel reveal">
      <div className="panel-head">
        <h2>Cases</h2>
        <span className="panel-meta">
          {numberFormat.format(waiting ? data.trace_count : filtered.length)}
          {query && !waiting ? ` of ${numberFormat.format(traces.length)}` : ""} cases
          {timeSpan &&
            ` · ${formatTimestamp(timeSpan.min)} — ${formatTimestamp(timeSpan.max)}`}
        </span>
        <XesButton dataset={data.dataset} available={available} />
        <DfgButton dataset={data.dataset} available={available} />
        {!available && <p className="export-note" id={EXPORT_NOTE_ID}>Exports are unavailable: {UNAVAILABLE_EXPORT}</p>}
        <input
          className="case-search"
          type="search"
          placeholder="Filter case id…"
          aria-label="Filter case id"
          value={query}
          onChange={(event) => {
            setQuery(event.target.value);
            setLimit(PAGE_SIZE);
          }}
        />
      </div>
      {waiting && <TraceLoadNote load={load} cases={data.trace_count} />}
      <ol className="case-list">
        {filtered.slice(0, limit).map(({ key, trace }) => (
          <CaseRow
            key={key}
            trace={trace}
            data={data}
            showResource={showResource}
            showLifecycle={showLifecycle}
            showAttributes={showAttributes}
          />
        ))}
      </ol>
      {filtered.length > limit && (
        <button className="btn-ghost case-more" onClick={() => setLimit(limit + 200)}>
          Show more · {numberFormat.format(filtered.length - limit)} remaining
        </button>
      )}
      {filtered.length === 0 && !waiting && (
        <div className="case-empty">No cases match “{query}”.</div>
      )}
    </section>
  );
}
