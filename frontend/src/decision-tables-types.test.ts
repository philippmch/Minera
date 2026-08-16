import { describe, expect, it } from "vitest";
import { isDecisionTablesResult, isMineraExport } from "./types";

const valid = {
  family: "DECISION_TABLES",
  variant: "encoded",
  trace_count: 1,
  observed_traces: 1,
  tables: [{
    place: 1,
    classes: ["B", "C"],
    feature_names: ["amount"],
    rows: [{ features: [10], target: 0 }],
  }],
};

describe("encoded decision table guards", () => {
  it("accepts standalone tables and standard exports", () => {
    expect(isDecisionTablesResult(valid)).toBe(true);
    expect(isMineraExport({
      dataset: "test",
      trace_count: 1,
      event_count: 1,
      activities: [],
      edges: [],
      variants: [],
      decision_tables: valid,
    })).toBe(true);
  });

  it("rejects invalid rows and counters", () => {
    expect(isDecisionTablesResult({ ...valid, observed_traces: 2 })).toBe(false);
    expect(isDecisionTablesResult({
      ...valid,
      tables: [{ ...valid.tables[0], rows: [{ features: [10], target: 2 }] }],
    })).toBe(false);
    expect(isMineraExport({
      dataset: "test",
      trace_count: 1,
      event_count: 1,
      activities: [],
      edges: [],
      variants: [],
      decision_tables: { ...valid, variant: "raw" },
    })).toBe(false);
  });
});
