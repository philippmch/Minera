import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it, vi } from "vitest";
import {
  dfgDataframeQuery,
  requestDfgDataframe,
} from "../src/api";
import {
  isDfgDataframeResult,
  type DfgDataframeEvent,
} from "../src/types";
import {
  DFG_DATAFRAME_UNAVAILABLE_HINT,
  DfgDataframe,
  DfgDataframeResultView,
  dfgDataframeActiveOptions,
  dfgDataframeRunnable,
} from "../src/components/DfgDataframe";

const event = (activity: string, inputOrder: number): DfgDataframeEvent => ({
  case_id: "case-1",
  case_id_state: "present",
  case_id_source_key: "case:concept:name",
  activity,
  activity_state: "present",
  activity_source_key: "concept:name",
  timestamp: `2024-01-01T00:00:0${inputOrder}Z`,
  timestamp_state: "present",
  timestamp_source_key: "time:timestamp",
  resource: "",
  resource_state: "missing",
  resource_source_key: "org:resource",
  lifecycle: "",
  lifecycle_state: "missing",
  lifecycle_source_key: "lifecycle:transition",
  input_order: inputOrder,
  attributes: [],
});

const partial = {
  operation: "partial_order" as const,
  backend: "pandas" as const,
  pairs: [{
    source_index: 0,
    target_index: 1,
    source_index_value: "0",
    target_index_value: "1",
    flow_time_seconds: 10,
    source: event("A", 0),
    target: event("B", 1),
  }],
};

describe("table-backed DFG query serialization", () => {
  it("serializes the complete graph option surface", () => {
    const query = new URLSearchParams(dfgDataframeQuery({
      operation: "graph", measure: "cost", backend: "polars",
      activity_attribute: " event:name ", target_activity_attribute: "event:target",
      timestamp_attribute: "event:when", start_timestamp_attribute: "event:start",
      cost_attribute: "event:cost", case_id_attribute: "case:id", window: 0,
      keep_once_per_case: true, performance_aggregation: "sum",
      business_hours: true, business_hour_slots: "none",
      sort_caseid_required: false, sort_timestamp_along_case_id: false,
      reduce_columns: false, workcalendar: "calendar-token",
    }));
    expect(Object.fromEntries(query)).toEqual({
      dfg_operation: "graph", dfg_measure: "cost", dfg_backend: "polars",
      dfg_activity_attribute: "event:name", dfg_target_activity_attribute: "event:target",
      dfg_timestamp_attribute: "event:when", dfg_start_timestamp_attribute: "event:start",
      dfg_cost_attribute: "event:cost", dfg_case_id_attribute: "case:id", dfg_window: "0",
      dfg_keep_once_per_case: "true", dfg_performance_aggregation: "sum",
      dfg_business_hours: "true", dfg_business_hour_slots: "none",
      dfg_sort_caseid_required: "false", dfg_sort_timestamp_along_case_id: "false",
      dfg_reduce_columns: "false", dfg_workcalendar: "calendar-token",
    });
  });

  it("keeps omitted schedule distinct from the explicit empty schedule", () => {
    expect(dfgDataframeQuery({ business_hours: true })).toBe("dfg_business_hours=true");
    expect(dfgDataframeQuery({ business_hours: true, business_hour_slots: "none" }))
      .toBe("dfg_business_hours=true&dfg_business_hour_slots=none");
  });

  it("accepts prefixed aliases and rejects conflicting aliases", () => {
    expect(dfgDataframeQuery({ dfg_operation: "partial_order", dfg_backend: "polars" }))
      .toBe("dfg_operation=partial_order&dfg_backend=polars");
    expect(() => dfgDataframeQuery({ operation: "graph", dfg_operation: "partial_order" }))
      .toThrow(/must not disagree/);
  });

  it("rejects invalid combinations before a network request", () => {
    expect(() => dfgDataframeQuery({ measure: "cost" })).toThrow(/cost measure requires/);
    expect(dfgDataframeQuery({ measure: "frequency", cost_attribute: "stale-cost" }))
      .toBe("dfg_measure=frequency");
    expect(() => dfgDataframeQuery({ business_hour_slots: "none" }))
      .toThrow(/requires business_hours/);
    expect(() => dfgDataframeQuery({ operation: "partial_order", measure: "performance" }))
      .toThrow(/applies only to graph/);
    expect(() => dfgDataframeQuery({ operation: "partial_order", cost_attribute: "cost" }))
      .toThrow(/applies only to graph/);
    expect(() => dfgDataframeQuery({ operation: "graph", event_index_attribute: "idx" }))
      .toThrow(/event_index_attribute/);
    expect(() => dfgDataframeQuery({ operation: "concurrent_events", event_index_attribute: "idx" }))
      .toThrow(/applies only to partial_order/);
    expect(() => dfgDataframeQuery({ operation: "concurrent_events", keep_first_following: true }))
      .toThrow(/applies only to partial_order/);
    expect(() => dfgDataframeQuery({ operation: "partial_order", overlap_column: "overlap" }))
      .toThrow(/applies only to concurrent_events/);
    expect(() => dfgDataframeQuery({ business_hours: true, business_hour_slots: "0-604801" }))
      .toThrow(/0 <= start < end <= 604800/);
  });
});

describe("table-backed DFG response guards", () => {
  it("guards each graph measure and rejects incoherent fields", () => {
    const summary = { count: 1, mean: 2, median: 2, min: 2, max: 2, stdev: 0, sum: 2 };
    const weighted = [{ source: "A", target: "B", summary, raw_values: [2], value: 2 }];
    expect(isDfgDataframeResult({ operation: "graph", backend: "pandas", measure: "frequency", frequency: [{ source: "A", target: "B", count: 1 }] })).toBe(true);
    expect(isDfgDataframeResult({ operation: "graph", backend: "pandas", measure: "performance", performance: weighted })).toBe(true);
    expect(isDfgDataframeResult({ operation: "graph", backend: "pandas", measure: "both", frequency: [{ source: "A", target: "B", count: 1 }], performance: weighted })).toBe(true);
    expect(isDfgDataframeResult({ operation: "graph", backend: "pandas", measure: "cost", cost: weighted })).toBe(true);
    expect(isDfgDataframeResult({ operation: "graph", backend: "pandas", measure: "performance", frequency: [] })).toBe(false);
    expect(isDfgDataframeResult({ operation: "graph", backend: "pandas", measure: "performance", performance: [{ ...weighted[0], raw_values: [] }] })).toBe(false);
  });

  it("guards pair operations and helper-column metadata", () => {
    expect(isDfgDataframeResult(partial)).toBe(true);
    expect(isDfgDataframeResult({ ...partial, pairs: [{ ...partial.pairs[0], flow_time_seconds: Infinity }] })).toBe(false);
    expect(isDfgDataframeResult({ operation: "concurrent_events", backend: "polars", columns: { max_start: "max", min_complete: "min", overlap: "diff" }, pairs: [{
      left_index: 0, right_index: 1, left_index_value: "0", right_index_value: "1",
      max_start_seconds: 2, min_complete_seconds: 4, overlap_seconds: 2,
      left: event("A", 0), right: event("B", 1),
    }] })).toBe(true);
    expect(isDfgDataframeResult({ operation: "concurrent_events", backend: "pandas", columns: { max_start: "max" }, pairs: [] })).toBe(false);
  });

  it("accepts scalar metadata and rejects malformed scalar flags", () => {
    const attribute = {
      key: "payload", value: "", kind: "string" as const, container: "scalar" as const,
      null: true, scalar_wrapper: true, scalar_children_list: false,
    };
    const withAttribute = {
      ...partial,
      pairs: [{ ...partial.pairs[0], source: { ...partial.pairs[0].source, attributes: [attribute] } }],
    };
    expect(isDfgDataframeResult(withAttribute)).toBe(true);
    expect(isDfgDataframeResult({
      ...withAttribute,
      pairs: [{ ...withAttribute.pairs[0], source: {
        ...withAttribute.pairs[0].source, attributes: [{ ...attribute, scalar_wrapper: "true" }],
      } }],
    })).toBe(false);
    expect(isDfgDataframeResult({
      ...withAttribute,
      pairs: [{ ...withAttribute.pairs[0], target: {
        ...withAttribute.pairs[0].target, attributes: [{ ...attribute, scalar_children_list: 1 }],
      } }],
    })).toBe(false);
  });

  it("accepts the typed request response and uses GET", async () => {
    const fetchMock = vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/dfg-dataframe?dfg_operation=partial_order");
      expect(init?.method).toBe("GET");
      return { ok: true, json: async () => partial };
    });
    vi.stubGlobal("fetch", fetchMock);
    await expect(requestDfgDataframe({ operation: "partial_order" })).resolves.toEqual(partial);
    vi.unstubAllGlobals();
  });
});

describe("table-backed DFG controls and rendering", () => {
  it("reports which options are active for each operation", () => {
    expect(dfgDataframeActiveOptions("graph", "cost")).toEqual({ graph: true, performance: true, partial: false, concurrent: false });
    expect(dfgDataframeActiveOptions("partial_order", "frequency")).toEqual({ graph: false, performance: false, partial: true, concurrent: false });
    expect(dfgDataframeActiveOptions("concurrent_events", "frequency")).toEqual({ graph: false, performance: false, partial: false, concurrent: true });
  });

  it("renders frequency, partial-order, and concurrency results readably", () => {
    const frequency = { operation: "graph" as const, backend: "pandas" as const, measure: "frequency" as const, frequency: [{ source: "A", target: "B", count: 3 }] };
    expect(renderToStaticMarkup(createElement(DfgDataframeResultView, { result: frequency }))).toContain("Frequency edges");
    expect(renderToStaticMarkup(createElement(DfgDataframeResultView, { result: partial }))).toContain("A");
    const concurrent = { operation: "concurrent_events" as const, backend: "pandas" as const, pairs: [{ left_index: 0, right_index: 1, left_index_value: "0", right_index_value: "1", max_start_seconds: 2, min_complete_seconds: 4, overlap_seconds: 2, left: event("A", 0), right: event("B", 1) }] };
    expect(renderToStaticMarkup(createElement(DfgDataframeResultView, { result: concurrent }))).toContain("Overlap");
    const summary = { count: 1, mean: 2, median: 2, min: 2, max: 2, stdev: 0, sum: 2 };
    const cost = { operation: "graph" as const, backend: "pandas" as const, measure: "cost" as const, cost: [{ source: "A", target: "B", summary, raw_values: [2], value: 2 }] };
    const costMarkup = renderToStaticMarkup(createElement(DfgDataframeResultView, { result: cost }));
    expect(costMarkup).toContain("Cost edges");
    expect(costMarkup).not.toContain("2s");
  });
});

describe("dataframe DFG source gating", () => {
  it("runs only on a live mined source that is not busy", () => {
    expect(dfgDataframeRunnable(true, false, false)).toBe(true);
    expect(dfgDataframeRunnable(false, false, false)).toBe(false);
    expect(dfgDataframeRunnable(true, true, false)).toBe(false);
    expect(dfgDataframeRunnable(true, false, true)).toBe(false);
  });

  it("disables the run button and explains why without a live source", async () => {
    const cases: { available?: boolean }[] = [{}, { available: false }];
    for (const props of cases) {
      const html = renderToStaticMarkup(createElement(DfgDataframe, props));
      expect(html).toMatch(/<button[^>]*disabled=""[^>]*>Run dataframe DFG<\/button>/);
      expect(html).toContain(DFG_DATAFRAME_UNAVAILABLE_HINT.replace(/'/g, "&#x27;"));
    }
  });

  it("enables the run button without the hint on a live source", async () => {
    const html = renderToStaticMarkup(createElement(DfgDataframe, { available: true }));
    expect(html).toMatch(/<button type="button" class="button">Run dataframe DFG<\/button>/);
    expect(html).not.toContain(DFG_DATAFRAME_UNAVAILABLE_HINT);
    const mining = renderToStaticMarkup(createElement(DfgDataframe, { available: true, mining: true }));
    expect(mining).toMatch(/<button[^>]*disabled=""[^>]*>Run dataframe DFG<\/button>/);
  });
});
