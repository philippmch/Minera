import type { BatchDiscoveryExport, BatchGroup, BatchInstance, MineraExport } from "../types";

const LABELS: Record<BatchInstance["type"], string> = {
  simultaneous: "Simultaneous",
  batching_on_start: "Batching on start",
  batching_on_end: "Batching on end",
  sequential: "Sequential",
  concurrent: "Concurrent",
};

function instant(value: number): string {
  if (!Number.isFinite(value)) return "unknown time";
  // Batch JSON stores epoch seconds; tolerate millisecond values too.
  const date = new Date(Math.abs(value) < 1e12 ? value * 1000 : value);
  return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString();
}

function Group({ group, data }: { group: BatchGroup; data: MineraExport }) {
  const activity = group.activity_label ??
    (group.activity === null ? "(custom activity)" :
      data.activities[group.activity]?.name ?? `Activity ${group.activity}`);
  const resource = group.resource?.trim() || "(missing resource)";
  const batches = group.batches ?? [];
  const batchCount = Number.isFinite(group.batch_count) ? group.batch_count : batches.length;
  const eventCount = Number.isFinite(group.event_count) ? group.event_count : "—";
  return (
    <article className="batch-group">
      <div className="batch-group-head">
        <h3>{activity}</h3>
        <span className="panel-meta">{resource} · {batchCount} batch{batchCount === 1 ? "" : "es"} · {eventCount} events</span>
      </div>
      {batches.length === 0 ? (
        <p className="empty-note">No batches in this category.</p>
      ) : (
        <div className="batch-list">
          {batches.map((batch, index) => (
            <div className="batch-card" key={`${batch.type}-${index}`}>
              <div className="batch-card-head">
                <strong>{LABELS[batch.type] ?? batch.type}</strong>
                <span>{instant(batch.start)} – {instant(batch.complete)}</span>
              </div>
              <div className="batch-card-meta">
                {(batch.events ?? []).length} event{(batch.events ?? []).length === 1 ? "" : "s"}
              </div>
              {(batch.events ?? []).length > 0 && (
                <ul className="batch-evidence">
                  {(batch.events ?? []).map((event, eventIndex) => (
                    <li key={`${event.case_id}-${event.trace_index}-${event.event_index}-${eventIndex}`}>
                      <span>case {event.case_id || "(missing case)"}</span>
                      <span>{instant(event.start)} – {instant(event.complete)}</span>
                    </li>
                  ))}
                </ul>
              )}
            </div>
          ))}
        </div>
      )}
    </article>
  );
}

export function Batches({ data }: { data: MineraExport }) {
  const analysis: BatchDiscoveryExport | undefined = data.batch_discovery;
  return (
    <section className="panel reveal batches-panel">
      <div className="panel-head">
        <h2>Batches</h2>
        {analysis && <span className="panel-meta">{analysis.groups.length} categor{analysis.groups.length === 1 ? "y" : "ies"}</span>}
      </div>
      {!analysis ? (
        <p className="empty-note">No batch analysis was returned. Enable batch discovery in Filters &amp; miner settings.</p>
      ) : analysis.groups.length === 0 ? (
        <p className="empty-note">No batches found in this analysis.</p>
      ) : (
        <div className="batch-groups">
          {analysis.groups.map((group, index) => <Group key={`${group.activity}-${group.resource}-${index}`} group={group} data={data} />)}
        </div>
      )}
    </section>
  );
}
