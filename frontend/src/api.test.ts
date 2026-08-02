import { describe, expect, it, vi } from "vitest";
import { labelSplittingQuery, requestLabelSplitting, logToFeaturesQuery, requestLogToFeatures, ilpMinerQuery, requestIlpMine, geneticMinerQuery, requestGeneticMine, traceVariantQuery, requestTraceVariantQuery, pripelQuery, requestPripel, organizationalSnaQuery, requestOrganizationalSna, roleDiscoveryQuery, requestRoleDiscovery, resourceProfilesQuery, requestResourceProfiles } from "./api";

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

describe("resource profiles API", () => {
  it("encodes strict fields", () => {
    expect(resourceProfilesQuery({ resource_profiles_metric: "social_position", resource_profiles_t1: "2024-01-01", resource_profiles_max_rows: 4 })).toBe("resource_profiles_metric=social_position&resource_profiles_t1=2024-01-01&resource_profiles_max_rows=4");
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "RESOURCE_PROFILES", rows: [] }) })));
    await expect(requestResourceProfiles()).rejects.toThrow(/invalid resource profiles/);
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

describe("genetic miner API", () => {
  it("encodes deterministic evolutionary bounds", () => {
    expect(geneticMinerQuery({ activity_key: "concept:name", seed: 7, population_size: 8, generations: 3, elitism_rate: 0.2, crossover_rate: 0.8, mutation_rate: 0.1, elitism_min_sample: 2, max_activities: 9, max_states: 64 }))
      .toBe("genetic_activity_key=concept%3Aname&genetic_seed=7&genetic_population_size=8&genetic_generations=3&genetic_elitism_rate=0.2&genetic_crossover_rate=0.8&genetic_mutation_rate=0.1&genetic_elitism_min_sample=2&genetic_max_activities=9&genetic_max_states=64");
  });
  it("rejects malformed genetic responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "GENETIC" }) })));
    await expect(requestGeneticMine()).rejects.toThrow(/invalid genetic miner/);
    vi.unstubAllGlobals();
  });
});

describe("organizational SNA API", () => {
  it("serializes bounded metric options", () => {
    expect(organizationalSnaQuery({ sna_variant: "working_together", sna_beta: 0.5, sna_n: 3, sna_metric_normalization: true, sna_max_edges: 12 }))
      .toBe("sna_variant=working_together&sna_beta=0.5&sna_n=3&sna_metric_normalization=true&sna_max_edges=12");
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ family: "bad" }), { status: 200 })));
    await expect(requestOrganizationalSna()).rejects.toThrow(/invalid organizational SNA/);
    vi.unstubAllGlobals();
  });
});

describe("role discovery API", () => {
  it("serializes strict threshold and safety options", () => {
    expect(roleDiscoveryQuery({ roles_threshold: 0.4, roles_max_resources: 8, roles_max_activities: 12 }))
      .toBe("roles_threshold=0.4&roles_max_resources=8&roles_max_activities=12");
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ family: "ROLE_DISCOVERY" }), { status: 200 })));
    await expect(requestRoleDiscovery()).rejects.toThrow(/invalid role discovery/);
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

describe("PRIPEL API", () => {
  it("encodes privacy, blocklist, and matching bounds", () => {
    expect(pripelQuery({ pripel_epsilon: 0.5, pripel_blocklist: ["case,id", "resource"], pripel_seed: 4, pripel_max_distance: 2 }))
      .toBe("pripel_epsilon=0.5&pripel_blocklist=case%5C%2Cid%2Cresource&pripel_seed=4&pripel_max_distance=2");
  });
  it("posts a query log and rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({ family: "PRIPEL" }) };
    }));
    await expect(requestPripel(new File(["case_id,activity\nc,A\n"], "query.csv"))).rejects.toThrow(/invalid PRIPEL/);
    vi.unstubAllGlobals();
  });
});
