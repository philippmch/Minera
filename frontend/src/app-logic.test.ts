import { describe, expect, it } from "vitest";
import { mineFormEntries, type FilterOptions } from "./api";
import {
  delimiterForFile,
  hasActiveFilters,
  hasActiveMinerOptions,
  initialTab,
  isJsonExport,
  logLifecycles,
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
      { attributeKey: "channel" },
      { caseAttributeKey: "region" },
      { variantCoverage: 0.8 },
      { keepDirectlyFollows: ["a -> b"] },
      { removeDirectlyFollows: ["a -> b"] },
      { keepEventuallyFollows: ["a -> b"] },
      { removeEventuallyFollows: ["a -> b"] },
      { prefixActivity: "a" },
      { suffixActivity: "b" },
      { removeLifecycles: ["start"] },
      { keepLifecycles: ["complete"] },
    ];
    for (const filters of cases) expect(hasActiveFilters(filters)).toBe(true);
  });
});

describe("logLifecycles", () => {
  it("collects distinct transitions in first-seen order", () => {
    const data = exp({
      traces: [
        {
          case_id: "1",
          events: [
            { activity: 0, timestamp: "", lifecycle: "start" },
            { activity: 0, timestamp: "", lifecycle: "complete" },
            { activity: 1, timestamp: "" },
            { activity: 1, timestamp: "", lifecycle: "start" },
          ],
        },
      ],
    });
    expect(logLifecycles(data)).toEqual(["start", "complete"]);
  });

  it("is empty for atomic and trace-less logs", () => {
    expect(logLifecycles(exp())).toEqual([]);
    expect(
      logLifecycles(
        exp({
          traces: [{ case_id: "1", events: [{ activity: 0, timestamp: "" }] }],
        }),
      ),
    ).toEqual([]);
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

describe("hasActiveMinerOptions", () => {
  it("is false for defaults", () => {
    expect(hasActiveMinerOptions({})).toBe(false);
    expect(hasActiveMinerOptions({ noiseThreshold: 0 })).toBe(false);
    expect(hasActiveMinerOptions({ inductiveVariant: "log" })).toBe(false);
    expect(hasActiveMinerOptions({ alphaVariant: "plus" })).toBe(false);
  });

  it("is true when any setting deviates", () => {
    expect(hasActiveMinerOptions({ noiseThreshold: 0.2 })).toBe(true);
    expect(hasActiveMinerOptions({ inductiveVariant: "dfg" })).toBe(true);
    expect(hasActiveMinerOptions({ alphaVariant: "classic" })).toBe(true);
    // Zero is a meaningful heuristic threshold, not "off".
    expect(hasActiveMinerOptions({ dependencyThreshold: 0 })).toBe(true);
    expect(hasActiveMinerOptions({ andThreshold: 0 })).toBe(true);
    expect(hasActiveMinerOptions({ minEdgeFrequency: 3 })).toBe(true);
  });
});

describe("mineFormEntries", () => {
  it("sends nothing for empty options", () => {
    expect(mineFormEntries({})).toEqual([]);
  });

  it("maps miner settings to the backend's form fields", () => {
    const entries = mineFormEntries({
      noiseThreshold: 0.2,
      inductiveVariant: "dfg",
      alphaVariant: "classic",
      dependencyThreshold: 0.75,
      andThreshold: 0,
      minEdgeFrequency: 2,
    });
    expect(entries).toEqual([
      ["noise_threshold", "0.2"],
      ["inductive_variant", "dfg"],
      ["alpha_variant", "classic"],
      ["dependency_threshold", "0.75"],
      ["and_threshold", "0"],
      ["min_edge_frequency", "2"],
    ]);
  });

  it("keeps the filter mappings intact", () => {
    const entries = mineFormEntries({
      topVariants: 5,
      removeActivities: ["a", "b"],
      timeFrom: "2024-01-01",
      timeMode: "events",
      reworkActivity: "check",
      reworkMin: 3,
    });
    expect(entries).toEqual([
      ["top_variants", "5"],
      ["remove_activities", "a,b"],
      ["rework_activity", "check"],
      ["rework_min_occurrences", "3"],
      ["time_from", "2024-01-01"],
      ["time_mode", "events"],
    ]);
  });

  it("drops a time mode without bounds and a rework minimum without activity", () => {
    expect(mineFormEntries({ timeMode: "traces_contained" })).toEqual([]);
    expect(mineFormEntries({ reworkMin: 3 })).toEqual([]);
  });

  it("maps the succession, cut, and coverage filters", () => {
    const entries = mineFormEntries({
      variantCoverage: 0.8,
      keepDirectlyFollows: ["a -> b", "b -> c"],
      removeEventuallyFollows: ["a -> d"],
      prefixActivity: "ship",
      suffixActivity: "approve",
    });
    expect(entries).toEqual([
      ["variant_coverage", "0.8"],
      ["keep_directly_follows", "a -> b,b -> c"],
      ["remove_eventually_follows", "a -> d"],
      ["prefix_activity", "ship"],
      ["suffix_activity", "approve"],
    ]);
    expect(mineFormEntries({ keepEventuallyFollows: [] })).toEqual([]);
  });

  it("maps the attribute filter and requires its key", () => {
    const entries = mineFormEntries({
      attributeKey: "channel",
      removeAttributeValues: ["fax", "mail"],
      attributeMode: "traces",
    });
    expect(entries).toEqual([
      ["attribute_key", "channel"],
      ["remove_attribute_values", "fax,mail"],
      ["attribute_mode", "traces"],
    ]);
    expect(
      mineFormEntries({
        attributeKey: "channel",
        keepAttributeValues: ["web"],
      }),
    ).toEqual([
      ["attribute_key", "channel"],
      ["keep_attribute_values", "web"],
    ]);
    // Value lists or a mode without a key send nothing.
    expect(
      mineFormEntries({
        removeAttributeValues: ["fax"],
        attributeMode: "traces",
      }),
    ).toEqual([]);
  });

  it("maps the lifecycle projection", () => {
    expect(
      mineFormEntries({
        keepLifecycles: ["complete"],
        removeLifecycles: ["schedule", "suspend"],
      }),
    ).toEqual([
      ["keep_lifecycles", "complete"],
      ["remove_lifecycles", "schedule,suspend"],
    ]);
  });

  it("maps the case-attribute filter and requires its key", () => {
    const entries = mineFormEntries({
      caseAttributeKey: "region",
      removeCaseAttributeValues: ["south", "west"],
    });
    expect(entries).toEqual([
      ["case_attribute_key", "region"],
      ["remove_case_attribute_values", "south,west"],
    ]);
    expect(
      mineFormEntries({
        caseAttributeKey: "region",
        keepCaseAttributeValues: ["north"],
      }),
    ).toEqual([
      ["case_attribute_key", "region"],
      ["keep_case_attribute_values", "north"],
    ]);
    // Value lists without a key send nothing.
    expect(
      mineFormEntries({ removeCaseAttributeValues: ["south"] }),
    ).toEqual([]);
  });
});
