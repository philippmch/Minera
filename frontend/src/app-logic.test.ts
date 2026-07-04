import { describe, expect, it } from "vitest";
import { type FilterOptions } from "./api";
import {
  delimiterForFile,
  hasActiveFilters,
  initialTab,
  isJsonExport,
  maxEdgeCount,
} from "./app-logic";
import { type EdgeStat, type MineraExport, type Trace } from "./types";

function exp(over: Partial<MineraExport> = {}): MineraExport {
  return {
    dataset: "t",
    trace_count: 0,
    event_count: 0,
    activities: [],
    edges: [],
    variants: [],
    ...over,
  };
}

const trace: Trace = { case_id: "1", events: [] };

describe("initialTab", () => {
  it("opens on the event log when traces are present", () => {
    expect(initialTab(exp({ traces: [trace] }))).toBe("log");
  });

  it("falls back to the graph for trace-less (older) exports", () => {
    expect(initialTab(exp())).toBe("dfg");
    expect(initialTab(exp({ traces: [] }))).toBe("dfg");
  });
});

describe("hasActiveFilters", () => {
  it("is false for an empty filter set", () => {
    expect(hasActiveFilters({})).toBe(false);
    expect(hasActiveFilters({ keepActivities: [], minLength: 0 })).toBe(false);
    // A mode without bounds constrains nothing.
    expect(hasActiveFilters({ timeMode: "events" })).toBe(false);
    // A rework minimum without an activity, or a half-set between pair,
    // constrains nothing either.
    expect(hasActiveFilters({ reworkMin: 3 })).toBe(false);
    expect(hasActiveFilters({ betweenStart: "a" })).toBe(false);
    expect(hasActiveFilters({ betweenEnd: "b" })).toBe(false);
  });

  it("is true when any constraint is set", () => {
    const cases: FilterOptions[] = [
      { topVariants: 5 },
      { minVariantFrequency: 2 },
      { removeActivities: ["x"] },
      { keepActivities: ["a"] },
      { startActivities: ["a"] },
      { endActivities: ["z"] },
      { minLength: 1 },
      { maxLength: 9 },
      { removeResources: ["Alice"] },
      { keepResources: ["Bob"] },
      { timeFrom: "2024-01-01T00:00" },
      { timeTo: "2024-01-31T23:59" },
      { reworkActivity: "a" },
      { betweenStart: "a", betweenEnd: "b" },
    ];
    for (const filters of cases) expect(hasActiveFilters(filters)).toBe(true);
  });
});

describe("maxEdgeCount", () => {
  it("floors at 1 so an edge-less log never divides by zero", () => {
    expect(maxEdgeCount(exp())).toBe(1);
  });

  it("returns the largest edge frequency", () => {
    const edges: EdgeStat[] = [
      { source: 0, target: 1, count: 3 },
      { source: 1, target: 2, count: 7 },
    ];
    expect(maxEdgeCount(exp({ edges }))).toBe(7);
  });
});

describe("file routing", () => {
  it("recognizes JSON exports case-insensitively", () => {
    expect(isJsonExport("export.json")).toBe(true);
    expect(isJsonExport("EXPORT.JSON")).toBe(true);
    expect(isJsonExport("log.csv")).toBe(false);
    expect(isJsonExport("log.tsv")).toBe(false);
  });

  it("uses a tab delimiter only for .tsv uploads", () => {
    expect(delimiterForFile("log.tsv")).toBe("\t");
    expect(delimiterForFile("LOG.TSV")).toBe("\t");
    expect(delimiterForFile("log.csv")).toBeUndefined();
    expect(delimiterForFile("log")).toBeUndefined();
  });
});
