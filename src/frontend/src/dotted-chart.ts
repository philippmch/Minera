import type { Trace } from "./types";

/** One plotted event: a dot at `time` on the row belonging to its case. */
export interface DottedEvent {
  /** Case row index, 0-based; row 0 is the earliest-starting case. */
  row: number;
  /** Event instant as epoch milliseconds. */
  time: number;
  /** Activity id (indexes MineraExport.activities). */
  activity: number;
}

/** Laid-out dotted chart: the dots plus the time domain and row count. */
export interface DottedChart {
  events: DottedEvent[];
  /** Earliest and latest plotted instant (epoch ms); both 0 when empty. */
  minTime: number;
  maxTime: number;
  /** Number of case rows with at least one timestamped event. */
  rowCount: number;
}

/** Parses an event timestamp to epoch milliseconds, or null when it isn't a date. */
export function parseInstant(timestamp: string): number | null {
  const time = Date.parse(timestamp);
  return Number.isNaN(time) ? null : time;
}

/**
 * Lays out a dotted chart from raw traces: one row per case, cases ordered by
 * their first event's instant (earliest on row 0), one dot per timestamped
 * event placed at its instant. Events whose timestamp does not parse are
 * skipped and cases left with no parseable event are dropped, so an untimed log
 * yields an empty chart rather than a fabricated one.
 */
export function buildDottedChart(traces: Trace[]): DottedChart {
  const rows: { first: number; events: { time: number; activity: number }[] }[] =
    [];

  for (const trace of traces) {
    const events: { time: number; activity: number }[] = [];
    let first = Infinity;
    for (const event of trace.events) {
      const time = parseInstant(event.timestamp);
      if (time === null) continue;
      events.push({ time, activity: event.activity });
      if (time < first) first = time;
    }
    if (events.length > 0) rows.push({ first, events });
  }

  rows.sort((a, b) => a.first - b.first);

  const events: DottedEvent[] = [];
  let minTime = Infinity;
  let maxTime = -Infinity;
  rows.forEach((row, index) => {
    for (const event of row.events) {
      events.push({ row: index, time: event.time, activity: event.activity });
      if (event.time < minTime) minTime = event.time;
      if (event.time > maxTime) maxTime = event.time;
    }
  });

  if (events.length === 0) {
    return { events: [], minTime: 0, maxTime: 0, rowCount: 0 };
  }
  return { events, minTime, maxTime, rowCount: rows.length };
}
