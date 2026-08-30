import { describe, expect, it, vi } from "vitest";

import { logConversionQuery, requestLogConversion } from "../src/api";
import { isLogConversionResult } from "../src/types";

const attribute = { key: "concept:name", value: "A", kind: "string" as const };
const metadata = {
  extensions: [{ name: "Concept", prefix: "concept", uri: "https://example.invalid/concept" }],
  classifiers: [{ name: "Activity", keys: ["concept:name"] }],
  trace_globals: [],
  event_globals: [{ key: "org:resource", value: "", kind: "string" as const, null: true as const }],
  properties: [{ key: "case_id_key", value: "case:concept:name", kind: "string" as const }],
};

describe("traditional log conversion API", () => {
  it("serializes every option deterministically and escapes attribute lists", () => {
    const query = logConversionQuery({
      log_conversion_variant: "to_nx",
      log_conversion_case_attributes_as_nodes: ["region", "segment,detail"],
      log_conversion_compress: true,
      log_conversion_deep_copy: false,
      log_conversion_include_df: false,
      log_conversion_lazy_output: true,
      log_conversion_max_graph_nodes: 12,
      log_conversion_max_rows_to_materialize: 50,
      log_conversion_max_variants_to_return: 3,
      log_conversion_stream_postprocessing: true,
    });
    expect(query).toBe(
      "log_conversion_case_attributes_as_nodes=region%2Csegment%5C%2Cdetail"
      + "&log_conversion_compress=true&log_conversion_deep_copy=false"
      + "&log_conversion_include_df=false&log_conversion_lazy_output=true"
      + "&log_conversion_max_graph_nodes=12&log_conversion_max_rows_to_materialize=50"
      + "&log_conversion_max_variants_to_return=3&log_conversion_stream_postprocessing=true"
      + "&log_conversion_variant=to_nx",
    );
    expect(() => logConversionQuery({ log_conversion_variant: "future" as never }))
      .toThrow(/variant/);
    expect(() => logConversionQuery({ log_conversion_case_id_key: "" }))
      .toThrow(/must not be empty/);
    expect(() => logConversionQuery({ log_conversion_event_attributes_as_nodes: [] }))
      .toThrow(/non-empty/);
    expect(() => logConversionQuery({ log_conversion_max_graph_edges: 400_001 }))
      .toThrow(/safety limit/);
    expect(() => logConversionQuery({ log_conversion_max_variants_to_return: -1 }))
      .toThrow(/non-negative/);
    expect(() => logConversionQuery({ log_conversion_max_rows_to_materialize: 1_000_001 }))
      .toThrow(/safety limit/);
    expect(logConversionQuery({ log_conversion_max_variants_to_return: 0 }))
      .toBe("log_conversion_max_variants_to_return=0");
  });

  it("validates all payload shapes and graph endpoints", () => {
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_EVENT_LOG",
      log: { attributes: [], metadata, traces: [] },
    })).toBe(true);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_EVENT_STREAM",
      stream: { attributes: [], metadata, events: [{ fields: [attribute] }] },
    })).toBe(true);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_DATA_FRAME",
      table: {
        columns: ["concept:name", "cost"],
        attributes: [],
        metadata,
        lazy: true,
        rows: [[{ value: "A", kind: "string" }, { value: "", kind: "float", null: true }]],
      },
    })).toBe(true);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_NX",
      graph: {
        nodes: [{ id: "CASE=c1", type: "CASE", attributes: [] }],
        edges: [{ source: 0, target: 0, type: "BELONGS_TO" }],
      },
    })).toBe(true);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "DF_TO_EVENT_LOG_NV",
      log: { attributes: [], traces: [] },
      variants: [{ activities: ["A"], count: 2, trace_indexes: [0, 1] }],
    })).toBe(true);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_DATA_FRAME",
      table: { columns: ["a", "b"], rows: [[attribute]] },
    })).toBe(false);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_EVENT_STREAM",
      stream: { attributes: [], metadata: { ...metadata, classifiers: [{ name: "bad", keys: [1] }] }, events: [] },
    })).toBe(false);
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_NX",
      graph: {
        nodes: [{ id: "CASE=c1", type: "CASE", attributes: [] }],
        edges: [{ source: 0, target: 1, type: "BELONGS_TO" }],
      },
    })).toBe(false);
  });

  it("requests the cached conversion and rejects malformed responses", async () => {
    const result = {
      family: "LOG_CONVERSION",
      variant: "TO_EVENT_STREAM",
      stream: { attributes: [], events: [{ fields: [attribute] }] },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/log-convert?log_conversion_variant=to_event_stream");
      expect(init).toEqual({ method: "POST" });
      return { ok: true, json: async () => result };
    }));
    await expect(requestLogConversion({ log_conversion_variant: "to_event_stream" }))
      .resolves.toEqual(result);

    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true,
      json: async () => ({ family: "LOG_CONVERSION", variant: "TO_EVENT_STREAM" }),
    })));
    await expect(requestLogConversion()).rejects.toThrow(/invalid log conversion/);

    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: false,
      status: 409,
      json: async () => ({ error: "no cached log" }),
    })));
    await expect(requestLogConversion()).rejects.toThrow(/no cached log/);
    vi.unstubAllGlobals();
  });
});
