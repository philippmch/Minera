import { describe, expect, it } from "vitest";
import { buildSpectrum } from "./performance-spectrum";
import { type Trace } from "./types";

const trace = (
  caseId: string,
  events: Array<[number, string]>,
): Trace => ({
  case_id: caseId,
  events: events.map(([activity, timestamp]) => ({ activity, timestamp })),
});

describe("buildSpectrum", () => {
  it("records one line per pattern occurrence, skipping other activities", () => {
    const traces = [
      // a … b with an unrelated activity 9 in between: still one clean run.
      trace("c1", [
        [0, "2024-01-01T10:00:00Z"],
        [9, "2024-01-01T10:05:00Z"],
        [1, "2024-01-01T10:10:00Z"],
      ]),
      // The pattern's steps out of order: no line.
      trace("c2", [
        [1, "2024-01-02T10:00:00Z"],
        [0, "2024-01-02T10:10:00Z"],
      ]),
    ];
    const spectrum = buildSpectrum(traces, [0, 1]);
    expect(spectrum.lines).toHaveLength(1);
    expect(spectrum.lines[0].times).toEqual([
      Date.parse("2024-01-01T10:00:00Z"),
      Date.parse("2024-01-01T10:10:00Z"),
    ]);
    expect(spectrum.lines[0].duration).toBe(10 * 60 * 1000);
    expect(spectrum.minTime).toBe(Date.parse("2024-01-01T10:00:00Z"));
    expect(spectrum.maxTime).toBe(Date.parse("2024-01-01T10:10:00Z"));
  });

  it("counts overlapping windows, so each loop iteration is one line", () => {
    const traces = [
      trace("loop", [
        [0, "2024-01-01T10:00:00Z"],
        [1, "2024-01-01T10:01:00Z"],
        [0, "2024-01-01T10:02:00Z"],
        [1, "2024-01-01T10:03:00Z"],
      ]),
    ];
    // Projection a b a b: windows [a b], [b a], [a b] → two matches of a→b.
    expect(buildSpectrum(traces, [0, 1]).lines).toHaveLength(2);
  });

  it("grades lines into speed terciles once there are three or more", () => {
    const traces = [1, 2, 10].map((minutes, index) =>
      trace(`c${index}`, [
        [0, "2024-01-01T10:00:00Z"],
        [1, `2024-01-01T10:${String(minutes).padStart(2, "0")}:00Z`],
      ]),
    );
    const speeds = buildSpectrum(traces, [0, 1]).lines.map((l) => l.speed);
    expect(speeds).toEqual([0, 1, 2]);
  });

  it("needs two pattern steps, timestamps, and matching cases", () => {
    const timed = [trace("c", [[0, "2024-01-01T10:00:00Z"]])];
    expect(buildSpectrum(timed, [0]).lines).toHaveLength(0);
    expect(buildSpectrum([], [0, 1]).lines).toHaveLength(0);
    const untimed = [trace("c", [[0, "not a date"], [1, "also not"]])];
    expect(buildSpectrum(untimed, [0, 1])).toEqual({
      lines: [],
      minTime: 0,
      maxTime: 0,
    });
  });
});
