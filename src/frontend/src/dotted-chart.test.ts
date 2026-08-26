import { describe, expect, it } from "vitest";
import type { Trace } from "./types";
import { buildDottedChart, parseInstant } from "./dotted-chart";

const trace = (caseId: string, events: [number, string][]): Trace => ({
  case_id: caseId,
  events: events.map(([activity, timestamp]) => ({ activity, timestamp })),
});

const at = (iso: string) => Date.parse(iso);

describe("parseInstant", () => {
  it("parses an ISO instant to epoch milliseconds", () => {
    expect(parseInstant("1970-01-01T00:00:00Z")).toBe(0);
  });

  it("returns null for an unparseable timestamp", () => {
    expect(parseInstant("not-a-date")).toBeNull();
    expect(parseInstant("")).toBeNull();
  });
});

describe("buildDottedChart", () => {
  it("orders rows by first event time, earliest on row 0", () => {
    const chart = buildDottedChart([
      trace("late", [[0, "2026-01-02T00:00:00Z"]]),
      trace("early", [[0, "2026-01-01T00:00:00Z"]]),
    ]);
    expect(chart.rowCount).toBe(2);
    const early = chart.events.find((e) => e.time === at("2026-01-01T00:00:00Z"));
    const late = chart.events.find((e) => e.time === at("2026-01-02T00:00:00Z"));
    expect(early?.row).toBe(0);
    expect(late?.row).toBe(1);
  });

  it("spans the global earliest and latest instant", () => {
    const chart = buildDottedChart([
      trace("c", [
        [0, "2026-01-01T00:00:00Z"],
        [1, "2026-01-01T01:00:00Z"],
      ]),
    ]);
    expect(chart.minTime).toBe(at("2026-01-01T00:00:00Z"));
    expect(chart.maxTime).toBe(at("2026-01-01T01:00:00Z"));
    expect(chart.events).toHaveLength(2);
  });

  it("skips unparseable events and drops fully untimed cases", () => {
    const chart = buildDottedChart([
      trace("untimed", [
        [0, ""],
        [1, "nope"],
      ]),
      trace("timed", [[0, "2026-01-01T00:00:00Z"]]),
    ]);
    expect(chart.rowCount).toBe(1);
    expect(chart.events).toHaveLength(1);
  });

  it("returns an empty chart for a fully untimed log", () => {
    const chart = buildDottedChart([trace("u", [[0, ""]])]);
    expect(chart).toEqual({ events: [], minTime: 0, maxTime: 0, rowCount: 0 });
  });
});
