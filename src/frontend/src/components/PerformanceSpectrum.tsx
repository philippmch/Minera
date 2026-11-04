import { useMemo, useState } from "react";
import { type MineraExport, formatDuration, numberFormat } from "../types";
import { buildSpectrum, serverSpectrumToSpectrum } from "../performance-spectrum";
import { loadedTraces, useTraces } from "../mine-traces";
import { TraceLoadNote } from "./TraceLoadNote";

const WIDTH = 880;
const BAND_H = 96; // vertical distance between two pattern steps
const PAD_X = 10;
const PAD_Y = 26;
const MAX_DRAWN_LINES = 2000;

interface PerformanceSpectrumViewProps {
  data: MineraExport;
  nativeOnly?: boolean;
  /** The pattern to start from (activity ids); empty by default. */
  initialPattern?: number[];
}

// The performance spectrum: pick two or more activities as a pattern, and
// every case that performs them as an uninterrupted run draws one line from
// step to step, placed at the instants the steps actually happened. Where an
// averaged waiting time gives one number, the spectrum shows the flow times
// evolving over the log's lifetime — batching as line bundles, overtaking as
// crossings, slow cases as the flattest diagonals (colored by tercile).
export function PerformanceSpectrumView({ data,nativeOnly=false,initialPattern }: PerformanceSpectrumViewProps) {
  const activities = data.activities;
  // The pattern under construction: activity ids, in order. Starts as a
  // sensible two-step default so the view is never an empty form.
  const [pattern, setPattern] = useState<number[]>(() => initialPattern ?? []);
  // A client-side spectrum reads the cases; they load once a step is picked.
  const load = useTraces(data, !nativeOnly && pattern.length > 0);
  const traces = loadedTraces(load);

  const spectrum = useMemo(
    () => pattern.length > 0
      ? buildSpectrum(traces, pattern)
      : data.performance_spectrum
        ? serverSpectrumToSpectrum(data.performance_spectrum)
        : buildSpectrum(traces, pattern),
    [data, traces, pattern],
  );
  const waiting = pattern.length >= 2 && (load.status === "loading" || load.status === "error");
  const serverMode = pattern.length === 0 && Boolean(data.performance_spectrum);
  const serverActivities = spectrum.activities ?? [];
  const axisNames = serverMode
    ? serverActivities
    : pattern.map((activityId) => activities[activityId]?.name ?? "?");

  const setStep = (index: number, value: string) => {
    setPattern((previous) => {
      const next = [...previous];
      if (value === "") {
        next.splice(index, 1); // "remove this step"
      } else {
        next[index] = Number(value);
      }
      return next;
    });
  };

  const span = Math.max(1, spectrum.maxTime - spectrum.minTime);
  const plotW = WIDTH - 2 * PAD_X;
  const height = 2 * PAD_Y + BAND_H * Math.max(1, axisNames.length - 1);
  const x = (time: number) =>
    PAD_X + ((time - spectrum.minTime) / span) * plotW;
  const y = (step: number) => PAD_Y + step * BAND_H;
  const drawn = spectrum.lines.slice(0, MAX_DRAWN_LINES);

  return (
    <section className="panel reveal">
      <div className="panel-head">
        <h2>Performance Spectrum</h2>
        {spectrum.lines.length > 0 && (
          <span className="panel-meta">
            {numberFormat.format(spectrum.lines.length)} occurrence
            {spectrum.lines.length === 1 ? "" : "s"}
            {spectrum.lines.length > MAX_DRAWN_LINES
              ? ` (first ${numberFormat.format(MAX_DRAWN_LINES)} drawn)`
              : ""}
          </span>
        )}
      </div>

      {serverMode && spectrum.variant && (
        <p className="panel-meta spectrum-server-note">
          {nativeOnly?`Native spectrum (${spectrum.variant}) from this completed discovery. Change the pattern in the discovery controls.`:`Server spectrum (${spectrum.variant}); choose activities below for an interactive client-side spectrum.`}
          {spectrum.variant.includes("disconnected") && " Partial lines are labeled by their contiguous subpattern."}
          {nativeOnly && " Returned timestamp values retain the selected adapter's scale."}
        </p>
      )}
      {!nativeOnly&&<div className="spectrum-pattern">
        <span className="spectrum-pattern-label">Steps</span>
        {[...pattern, -1].map((selected, index) => (
          <select
            key={index}
            aria-label={index < pattern.length ? `Spectrum step ${index + 1}` : "Add a spectrum step"}
            value={selected === -1 ? "" : String(selected)}
            onChange={(e) => setStep(index, e.target.value)}
          >
            <option value="">
              {index < pattern.length ? "remove step" : "add step…"}
            </option>
            {activities.map((activity, id) => (
              <option key={id} value={id}>
                {activity.name}
              </option>
            ))}
          </select>
        ))}
        {pattern.length > 0 && (
          <button className="btn-ghost" onClick={() => setPattern([])}>
            Clear
          </button>
        )}
      </div>}

      {waiting ? (
        <TraceLoadNote load={load} />
      ) : !serverMode && pattern.length < 2 ? (
        <p className="empty-note">
          Pick two or more activities: each case that performs them in a row
          draws one line from step to step at the instants they happened.
        </p>
      ) : spectrum.lines.length === 0 ? (
        <p className="empty-note">
          No case performs these steps as an uninterrupted, timestamped run —
          try another order or fewer steps.
        </p>
      ) : (
        <div className="spectrum-wrap">
          <svg
            viewBox={`0 0 ${WIDTH} ${height}`}
            role="img"
            aria-label="Performance spectrum"
          >
            {axisNames.map((name, step) => (
              <g key={step}>
                <line
                  className="spectrum-axis"
                  x1={PAD_X}
                  y1={y(step)}
                  x2={WIDTH - PAD_X}
                  y2={y(step)}
                />
                <text className="spectrum-axis-label" x={PAD_X} y={y(step) - 6}>
                  {name}
                </text>
              </g>
            ))}
            {drawn.map((line, index) => (
              <polyline
                key={index}
                className={`spectrum-line speed-${line.speed}`}
                points={line.times
                  .map((time, step) => {
                    // Server disconnected observations carry an explicit
                    // offset; repeated activity labels must not be resolved
                    // by searching the axis names.
                    const axisStep = (line.patternOffset ?? 0) + step;
                    return `${x(time)},${y(axisStep)}`;
                  })
                  .join(" ")}
              >
                <title>{`${line.activityNames?.length ? `${line.activityNames.join(" → ")} · ` : ""}${nativeOnly?`span ${line.duration / 1000} native timestamp units`:`flow time ${formatDuration(line.duration / 1000)}`}`}</title>
              </polyline>
            ))}
          </svg>
          <div className="spectrum-legend">
            <span>
              <i className="speed-0" /> fastest third
            </span>
            <span>
              <i className="speed-1" /> middle
            </span>
            <span>
              <i className="speed-2" /> slowest third
            </span>
          </div>
        </div>
      )}
    </section>
  );
}
