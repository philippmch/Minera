import type { PerformanceSpectrumExport, Trace } from "./types";
import { parseInstant } from "./dotted-chart";

// Client-side counterpart of the backend's performance spectrum: every time a
// case performs the chosen activities as an uninterrupted run — ignoring
// events outside the pattern — one line records when each step happened.
// Computed from the traces the export already carries, like the dotted chart.

/** One occurrence of the pattern in a case. */
export interface SpectrumLine {
  /** Instant per pattern step, epoch milliseconds, in pattern order. */
  times: number[];
  /** Flow time from the first to the last step, milliseconds. */
  duration: number;
  /** Speed class by duration tercile: 0 fastest third … 2 slowest third. */
  speed: 0 | 1 | 2;
  /** Server spectra may contain partial/disconnected subpatterns. */
  activityNames?: string[];
  caseId?: string;
  /** Start index in the requested pattern (server disconnected spectra). */
  patternOffset?: number;
}

export interface Spectrum {
  lines: SpectrumLine[];
  /** Earliest and latest instant across all lines (epoch ms); 0 when empty. */
  minTime: number;
  maxTime: number;
  variant?: "connected" | "disconnected";
  activities?: string[];
}

/** Convert a server-returned PM4Py spectrum into the renderer's line model. */
export function serverSpectrumToSpectrum(value: PerformanceSpectrumExport): Spectrum {
  const lines: SpectrumLine[] = [];
  let minTime = Infinity;
  let maxTime = -Infinity;
  for (const observation of value.observations ?? []) {
    // The JSON contract uses epoch seconds; the client renderer uses epoch ms
    // (the same convention as parseInstant/buildSpectrum).
    const times = observation.instants
      .filter((instant) => Number.isFinite(instant))
      .map((instant) => instant * 1000);
    if (times.length < 2) continue;
    const first = times[0];
    const last = times[times.length - 1];
    lines.push({
      times,
      duration: Math.max(0, last - first),
      speed: 0,
      activityNames: observation.activities,
      caseId: observation.case_id,
      patternOffset: Number.isInteger(observation.pattern_offset) && observation.pattern_offset >= 0
        ? observation.pattern_offset : 0,
    });
    minTime = Math.min(minTime, first);
    maxTime = Math.max(maxTime, last);
  }
  if (lines.length >= 3) {
    const sorted = lines.map((line) => line.duration).sort((a, b) => a - b);
    const t1 = sorted[Math.floor(sorted.length / 3)];
    const t2 = sorted[Math.floor((2 * sorted.length) / 3)];
    for (const line of lines) line.speed = line.duration >= t2 ? 2 : line.duration >= t1 ? 1 : 0;
  }
  return {
    lines,
    minTime: lines.length ? minTime : 0,
    maxTime: lines.length ? maxTime : 0,
    variant: value.variant,
    activities: value.activities,
  };
}

/**
 * Builds the spectrum of `traces` over `pattern` (activity ids, in order).
 * Each trace is projected onto the pattern's activities (only events with a
 * parseable timestamp), and every window of the projection that spells the
 * pattern yields one line — windows overlap, so a looping case contributes
 * one line per iteration. A pattern shorter than two steps yields an empty
 * spectrum: there is no flow to show between fewer than two steps.
 */
export function buildSpectrum(traces: Trace[], pattern: number[]): Spectrum {
  if (pattern.length < 2) return { lines: [], minTime: 0, maxTime: 0 };
  const wanted = new Set(pattern);

  const lines: SpectrumLine[] = [];
  let minTime = Infinity;
  let maxTime = -Infinity;

  for (const trace of traces) {
    const projected: { activity: number; time: number }[] = [];
    for (const event of trace.events) {
      if (!wanted.has(event.activity)) continue;
      const time = parseInstant(event.timestamp);
      if (time === null) continue;
      projected.push({ activity: event.activity, time });
    }
    for (let start = 0; start + pattern.length <= projected.length; start++) {
      let matches = true;
      for (let i = 0; i < pattern.length; i++) {
        if (projected[start + i].activity !== pattern[i]) {
          matches = false;
          break;
        }
      }
      if (!matches) continue;
      const times = pattern.map((_, i) => projected[start + i].time);
      const first = times[0];
      const last = times[times.length - 1];
      lines.push({ times, duration: last - first, speed: 0, patternOffset: 0 });
      if (first < minTime) minTime = first;
      if (last > maxTime) maxTime = last;
    }
  }

  // Grade each line by duration tercile so slow occurrences stand out; with
  // fewer than three lines everything reads as the fastest class.
  if (lines.length >= 3) {
    const sorted = lines.map((line) => line.duration).sort((a, b) => a - b);
    const t1 = sorted[Math.floor(sorted.length / 3)];
    const t2 = sorted[Math.floor((2 * sorted.length) / 3)];
    for (const line of lines) {
      line.speed = line.duration >= t2 ? 2 : line.duration >= t1 ? 1 : 0;
    }
  }

  return lines.length > 0
    ? { lines, minTime, maxTime }
    : { lines, minTime: 0, maxTime: 0 };
}
