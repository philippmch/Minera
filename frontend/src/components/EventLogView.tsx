import { useMemo, useState } from "react";
import { type MineraExport, type Trace, numberFormat } from "../types";
import { fetchXes } from "../api";

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
function XesButton({ dataset }: { dataset: string }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = () => {
    setBusy(true);
    setError(null);
    fetchXes()
      .then((xml) => {
        const blob = new Blob([xml], { type: "application/xml" });
        const url = URL.createObjectURL(blob);
        const anchor = document.createElement("a");
        const base = dataset.replace(/\.[^.]+$/, "") || "log";
        anchor.href = url;
        anchor.download = `${base}.xes`;
        anchor.click();
        URL.revokeObjectURL(url);
      })
      .catch((cause: Error) => setError(cause.message))
      .finally(() => setBusy(false));
  };

  return (
    <span className="pnml-export">
      <button className="btn-ghost" onClick={download} disabled={busy}>
        {busy ? "Exporting…" : "Download log as XES"}
      </button>
      {error && <span className="pnml-error">{error}</span>}
    </span>
  );
}

export function EventLogView({ data }: { data: MineraExport }) {
  const traces = data.traces ?? [];
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

  const filtered = useMemo(() => {
    const needle = query.trim().toLowerCase();
    if (!needle) return traces;
    return traces.filter((trace) =>
      trace.case_id.toLowerCase().includes(needle),
    );
  }, [traces, query]);

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
          {numberFormat.format(filtered.length)}
          {query ? ` of ${numberFormat.format(traces.length)}` : ""} cases
          {timeSpan &&
            ` · ${formatTimestamp(timeSpan.min)} — ${formatTimestamp(timeSpan.max)}`}
        </span>
        <XesButton dataset={data.dataset} />
        <input
          className="case-search"
          type="search"
          placeholder="Filter case id…"
          value={query}
          onChange={(event) => {
            setQuery(event.target.value);
            setLimit(PAGE_SIZE);
          }}
        />
      </div>
      <ol className="case-list">
        {filtered.slice(0, limit).map((trace) => (
          <CaseRow
            key={trace.case_id}
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
      {filtered.length === 0 && (
        <div className="case-empty">No cases match “{query}”.</div>
      )}
    </section>
  );
}
