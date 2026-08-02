import { describe, expect, it, vi } from "vitest";
import { labelSplittingQuery, requestLabelSplitting, logToFeaturesQuery, requestLogToFeatures, ilpMinerQuery, requestIlpMine, traceVariantQuery, requestTraceVariantQuery } from "./api";

describe("log-to-features API", () => {
  it("encodes deterministic sorted options and escaped lists", () => {
    expect(logToFeaturesQuery({ features_event_attributes: ["a,b", "x"], features_variant: "event_based" }))
      .toBe("features_event_attributes=a%5C%2Cb%2Cx&features_variant=event_based");
  });

  it("rejects malformed backend payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "LOG_TO_FEATURES", variant: "trace_based", feature_names: [], trace_rows: [["bad"]] }) })));
    await expect(requestLogToFeatures()).rejects.toThrow(/invalid feature extraction/);
    vi.unstubAllGlobals();
  });
});

describe("label-splitting API", () => {
  it("encodes sorted options and escaped target labels", () => {
    expect(labelSplittingQuery({ label_splitting_target_activities: ["A,B", "C"], label_splitting_prefix_length: 1 }))
      .toBe("label_splitting_prefix_length=1&label_splitting_target_activities=A%5C%2CB%2CC");
  });

  it("rejects malformed backend payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "LABEL_SPLITTING", variant: "contextual", log: { traces: [{ case_id: "x", events: [{ activity: "A" }] }] } }) })));
    await expect(requestLabelSplitting()).rejects.toThrow(/invalid label splitting/);
    vi.unstubAllGlobals();
  });
});

describe("ILP miner API", () => {
  it("encodes strict cached-session options", () => {
    expect(ilpMinerQuery({ activity_key: "concept:name", noise_threshold: 0.2, max_nodes: 42 }))
      .toBe("ilp_activity_key=concept%3Aname&ilp_noise_threshold=0.2&ilp_max_nodes=42");
  });
  it("rejects malformed ILP responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "ILP" }) })));
    await expect(requestIlpMine()).rejects.toThrow(/invalid ILP miner/);
    vi.unstubAllGlobals();
  });
});

describe("trace-variant query API", () => {
  it("encodes deterministic privacy and bound options", () => {
    expect(traceVariantQuery({ tvq_variant: "laplace", tvq_epsilon: 0.5, tvq_k: 3, tvq_p: 2, tvq_seed: 9, tvq_max_output_traces: 20 }))
      .toBe("tvq_variant=laplace&tvq_epsilon=0.5&tvq_k=3&tvq_p=2&tvq_seed=9&tvq_max_output_traces=20");
  });
  it("rejects malformed backend payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "TRACE_VARIANT_QUERY", variant: "sacofa" }) })));
    await expect(requestTraceVariantQuery()).rejects.toThrow(/invalid trace-variant query/);
    vi.unstubAllGlobals();
  });
});
