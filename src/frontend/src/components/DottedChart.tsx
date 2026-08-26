import { useMemo } from "react";
import { type MineraExport, numberFormat } from "../types";
import { buildDottedChart } from "../dotted-chart";

const WIDTH = 880;
const ROW_STEP = 3; // vertical pixels per case at the natural size
const MIN_PLOT_HEIGHT = 140;
const MAX_PLOT_HEIGHT = 560;
const PAD = 10;

// Golden-angle hue rotation gives adjacent activity ids visually distinct,
// stable colors without a hand-tuned palette.
function activityColor(id: number): string {
  const hue = (id * 137.508) % 360;
  return `hsl(${hue.toFixed(1)}, 55%, 48%)`;
}

interface DottedChartViewProps {
  data: MineraExport;
}

export function DottedChartView({ data }: DottedChartViewProps) {
  const chart = useMemo(() => buildDottedChart(data.traces ?? []), [data]);

  if (chart.rowCount === 0) {
    return (
      <section className="panel reveal">
        <div className="panel-head">
          <h2>Dotted Chart</h2>
        </div>
        <p className="empty-note">
          This log carries no parseable event timestamps, so there is nothing to
          plot over time.
        </p>
      </section>
    );
  }

  const span = Math.max(1, chart.maxTime - chart.minTime);
  const plotW = WIDTH - 2 * PAD;
  const plotH = Math.min(
    MAX_PLOT_HEIGHT,
    Math.max(MIN_PLOT_HEIGHT, chart.rowCount * ROW_STEP),
  );
  const height = plotH + 2 * PAD;

  const x = (time: number) => PAD + ((time - chart.minTime) / span) * plotW;
  const y = (row: number) =>
    PAD + (chart.rowCount <= 1 ? plotH / 2 : (row / (chart.rowCount - 1)) * plotH);

  const day = (time: number) => new Date(time).toISOString().slice(0, 10);

  return (
    <section className="panel reveal dotted-panel">
      <div className="panel-head">
        <h2>Dotted Chart</h2>
        <span className="panel-meta">
          {numberFormat.format(chart.rowCount)} cases ·{" "}
          {numberFormat.format(chart.events.length)} events · sorted by start
        </span>
      </div>
      <svg
        className="dotted-chart"
        viewBox={`0 0 ${WIDTH} ${height}`}
        role="img"
        aria-label="Events plotted over time, one row per case"
      >
        {chart.events.map((event, i) => (
          <circle
            key={i}
            cx={x(event.time).toFixed(1)}
            cy={y(event.row).toFixed(1)}
            r={1.5}
            fill={activityColor(event.activity)}
            fillOpacity={0.78}
          />
        ))}
      </svg>
      <div className="dotted-axis">
        <span>{day(chart.minTime)}</span>
        <span>{day(chart.maxTime)}</span>
      </div>
      <ul className="dotted-legend">
        {data.activities.map((activity, id) => (
          <li key={id}>
            <i style={{ background: activityColor(id) }} />
            {activity.name}
          </li>
        ))}
      </ul>
    </section>
  );
}
