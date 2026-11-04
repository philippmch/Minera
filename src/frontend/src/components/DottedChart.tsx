import { useEffect, useMemo, useState } from "react";
import { type MineraExport, numberFormat } from "../types";
import { buildDottedChart, type DottedChart } from "../dotted-chart";
import { loadedTraces, useTraces } from "../mine-traces";
import { TraceLoadNote } from "./TraceLoadNote";

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

/**
 * Above this many events the chart is drawn into one raster image instead of
 * one SVG circle per event: hundreds of thousands of circles freeze the page
 * for seconds and hold as many DOM nodes, while the plot is only a few hundred
 * pixels tall.
 */
export const DOTTED_SVG_LIMIT = 20_000;
const DOT_RADIUS = 1.5;
const DOT_OPACITY = 0.78;

/** The plot's height and the mapping of an event to its dot's centre. */
function dottedGeometry(chart: DottedChart) {
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
  return { height, x, y };
}

/**
 * The dots of a large chart as a PNG data URL: undefined while it is drawn,
 * null when this browser could not draw it.
 */
function useDottedRaster(chart: DottedChart | null): string | null | undefined {
  const [image, setImage] = useState<{ chart: DottedChart; url: string | null } | null>(null);
  useEffect(() => {
    if (!chart || typeof document === "undefined") return;
    const { height, x, y } = dottedGeometry(chart);
    const scale = 2;
    const canvas = document.createElement("canvas");
    canvas.width = WIDTH * scale;
    canvas.height = Math.ceil(height * scale);
    const context = canvas.getContext("2d");
    if (!context) { setImage({ chart, url: null }); return; }
    context.scale(scale, scale);
    context.globalAlpha = DOT_OPACITY;
    // One path per activity colour: hundreds of thousands of separate fills are slow.
    const byActivity = new Map<number, number[]>();
    chart.events.forEach((event, index) => {
      const list = byActivity.get(event.activity);
      if (list) list.push(index); else byActivity.set(event.activity, [index]);
    });
    for (const [activity, indices] of byActivity) {
      context.fillStyle = activityColor(activity);
      context.beginPath();
      for (const index of indices) {
        const event = chart.events[index];
        const cx = x(event.time), cy = y(event.row);
        context.moveTo(cx + DOT_RADIUS, cy);
        context.arc(cx, cy, DOT_RADIUS, 0, 2 * Math.PI);
      }
      context.fill();
    }
    let url: string | null;
    try { url = canvas.toDataURL("image/png"); } catch { url = null; }
    setImage({ chart, url });
  }, [chart]);
  return chart && image?.chart === chart ? image.url : undefined;
}

interface DottedChartViewProps {
  data: MineraExport;
}

export function DottedChartView({ data }: DottedChartViewProps) {
  const load = useTraces(data);
  const traces = loadedTraces(load);
  const chart = useMemo(() => buildDottedChart(traces), [traces]);
  const raster = chart.events.length > DOTTED_SVG_LIMIT;
  const image = useDottedRaster(raster ? chart : null);

  if (load.status === "loading" || load.status === "error" || (load.status === "unavailable" && !!data.trace_summary)) {
    return (
      <section className="panel reveal">
        <div className="panel-head">
          <h2>Dotted Chart</h2>
        </div>
        <TraceLoadNote load={load} />
      </section>
    );
  }

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

  const { height, x, y } = dottedGeometry(chart);
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
        {raster
          ? image && <image href={image} x={0} y={0} width={WIDTH} height={height} preserveAspectRatio="none" />
          : chart.events.map((event, i) => (
              <circle
                key={i}
                cx={x(event.time).toFixed(1)}
                cy={y(event.row).toFixed(1)}
                r={DOT_RADIUS}
                fill={activityColor(event.activity)}
                fillOpacity={DOT_OPACITY}
              />
            ))}
      </svg>
      <div className="dotted-axis">
        <span>{day(chart.minTime)}</span>
        <span>{day(chart.maxTime)}</span>
      </div>
      {raster && (
        <p className="empty-note dotted-raster-note" role="status">
          {image
            ? `The ${numberFormat.format(chart.events.length)} events are drawn as one image, which keeps the view responsive.`
            : image === null
              ? `This browser could not draw the ${numberFormat.format(chart.events.length)} events.`
              : `Drawing ${numberFormat.format(chart.events.length)} events…`}
        </p>
      )}
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
