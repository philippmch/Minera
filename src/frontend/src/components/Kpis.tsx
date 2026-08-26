import { type MineraExport, numberFormat } from "../types";

export function Kpis({ data }: { data: MineraExport }) {
  const items: [string, number][] = [
    ["cases", data.trace_count],
    ["events", data.event_count],
    ["activities", data.activities.length],
    ["variants", data.variants.length],
    ["transitions", data.edges.length],
  ];
  return (
    <section className="kpis reveal" aria-label="Log statistics">
      {items.map(([label, value]) => (
        <div className="kpi" key={label}>
          <div className="kpi-value">{numberFormat.format(value)}</div>
          <div className="kpi-label">{label}</div>
        </div>
      ))}
    </section>
  );
}
