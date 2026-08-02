import { describe, expect, it, vi } from "vitest";
import { labelSplittingQuery, requestLabelSplitting, logToFeaturesQuery, requestLogToFeatures, ilpMinerQuery, requestIlpMine, geneticMinerQuery, requestGeneticMine, traceVariantQuery, requestTraceVariantQuery, pripelQuery, requestPripel, organizationalSnaQuery, requestOrganizationalSna, networkAnalysisQuery, requestNetworkAnalysis, clusteringQuery, requestClustering, roleDiscoveryQuery, requestRoleDiscovery, resourceProfilesQuery, requestResourceProfiles, localDiagnosticsQuery, ocelDiscoveryQuery, requestOcelDiscovery, ocelConformanceQuery, requestOcelConformance, linkAnalysisQuery, requestLinkAnalysis, timestampInterleavingsQuery, requestTimestampInterleavings, monteCarloQuery, fetchMonteCarloResult } from "./api";

describe("Monte Carlo API", () => {
  it("serializes bounded virtual-time controls", () => {
    expect(monteCarloQuery({
      simulations: 4,
      case_arrival_seconds: 2.5,
      max_steps: 8,
      seed: 7,
      default_duration_seconds: 3,
      transition_weights: [0.25, 1],
      transition_durations: [
        { distribution: "deterministic", value_seconds: 1 },
        { distribution: "exponential", value_seconds: 2 },
        { distribution: "normal", value_seconds: 3, secondary_seconds: 1 },
        { distribution: "lognormal", value_seconds: 0.5, secondary_seconds: 0, tertiary_seconds: 1 },
      ],
      resources_per_place: { 0: 2, 3: 4 },
      enable_diagnostics: false,
    })).toBe("monte_carlo_simulations=4&monte_carlo_max_steps=8&monte_carlo_seed=7&monte_carlo_case_arrival_seconds=2.5&monte_carlo_default_duration_seconds=3&monte_carlo_transition_weights=0.25%2C1&monte_carlo_transition_durations=deterministic%3A1%2Cexponential%3A2%2Cnormal%3A3%3B1%2Clognormal%3A0.5%3B0%3B1&monte_carlo_resources_per_place=0%3D2%2C3%3D4&monte_carlo_enable_diagnostics=false");
  });

  it("validates simulation results", async () => {
    const result = {
      family: "MONTE_CARLO", variant: "PETRI_SEMAPH_FIFO",
      log: { trace_count: 2, event_count: 3 },
      diagnostics: { simulated_cases: 2, completed_cases: 2, deadlocked_cases: 0, step_limited_cases: 0, case_arrival_seconds: 1, total_cases_seconds: 3, median_case_execution_seconds: 1, case_execution_seconds: [1, 1] },
    };
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => result })));
    await expect(fetchMonteCarloResult("inductive", { simulations: 2 })).resolves.toEqual(result);
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ ...result, diagnostics: { ...result.diagnostics, case_execution_seconds: ["bad"] } }) })));
    await expect(fetchMonteCarloResult()).rejects.toThrow(/invalid Monte Carlo/);
    vi.unstubAllGlobals();
  });
});

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

describe("network analysis API", () => {
  it("serializes selectors and bounded options", () => {
    expect(networkAnalysisQuery({ network_analysis_edge_reference: "_in", network_analysis_include_performance: true, network_analysis_max_rows: 3 }))
      .toBe("network_analysis_edge_reference=_in&network_analysis_include_performance=true&network_analysis_max_rows=3");
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ family: "NETWORK_ANALYSIS", edges: [] }), { status: 200 })));
    await expect(requestNetworkAnalysis()).rejects.toThrow(/invalid network analysis/);
    vi.unstubAllGlobals();
  });
});

describe("trace clustering API", () => {
  it("serializes variant, distance, and safety options", () => {
    expect(clusteringQuery({ clustering_trace_attribute: "segment", clustering_variant: "DMM_VEC", clustering_alpha: 0.25, clustering_percent: 0.8, clustering_max_groups: 4, clustering_max_output_nodes: 9 }))
      .toBe("clustering_trace_attribute=segment&clustering_variant=DMM_VEC&clustering_alpha=0.25&clustering_percent=0.8&clustering_max_groups=4&clustering_max_output_nodes=9");
  });
  it("rejects malformed recursive results", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({
      family: "TRACE_ATTRIBUTE_CLUSTERING",
      tree: { name: "root", distance: 0, size: 2, leaf: false, children: [] },
      leaf_names: ["a", "b"],
      diagnostics: { computed: true, output_truncated: false, trace_count: 2, group_count: 2, node_count: 3, message: "ok" },
    }), { status: 200 })));
    await expect(requestClustering()).rejects.toThrow(/invalid trace clustering/);
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

describe("local diagnostics API", () => {
  it("encodes deterministic group and safety options", () => {
    expect(localDiagnosticsQuery({ local_diagnostics_groups: "g1:r1,r2", local_diagnostics_max_rows: 4 }))
      .toBe("local_diagnostics_groups=g1%3Ar1%2Cr2&local_diagnostics_max_rows=4");
  });
});

describe("OCEL API", () => {
  it("serializes discovery and conformance options", () => {
    expect(ocelDiscoveryQuery({ variant: "ocdfg", compute_edges_performance: false }))
      .toBe("variant=ocdfg&compute_edges_performance=false");
    expect(ocelDiscoveryQuery({ variant: "saw" }))
      .toBe("variant=saw");
    expect(ocelConformanceQuery({ variant: "etot", alpha: 0.5, etot_theta: 0.2 }))
      .toBe("variant=etot&alpha=0.5&etot_theta=0.2");
  });

  it("posts OCEL JSON as multipart data and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({ family: "OCEL_DISCOVERY", variant: "etot", result: { activities: [] } }) };
    }));
    await expect(requestOcelDiscovery(new File(["{}"], "log.json"), { variant: "etot" })).resolves.toMatchObject({ family: "OCEL_DISCOVERY" });
    vi.unstubAllGlobals();
  });

  it("rejects malformed OCEL conformance payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "OCEL_CONFORMANCE", variant: "otg" }) })));
    await expect(requestOcelConformance(new File(["{}"], "log.json"), new File(["{}"], "normative.json")))
      .rejects.toThrow(/invalid OCEL conformance/);
    vi.unstubAllGlobals();
  });
});

describe("link-analysis API", () => {
  it("serializes relation, ordering, propagation, and bound options", () => {
    expect(linkAnalysisQuery({
      link_analysis_out_column: "source_id",
      link_analysis_in_column: "target_id",
      link_analysis_sorting_column: "started_at",
      link_analysis_look_forward: false,
      link_analysis_keep_first_occurrence: true,
      link_analysis_propagate: true,
      link_analysis_max_links: 12,
    })).toBe("link_analysis_out_column=source_id&link_analysis_in_column=target_id&link_analysis_sorting_column=started_at&link_analysis_look_forward=false&link_analysis_keep_first_occurrence=true&link_analysis_propagate=true&link_analysis_max_links=12");
  });

  it("validates the cached-session response shape", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "LINK_ANALYSIS", variant: "CLASSIC", out_column: "case_id", in_column: "case_id", sorting_column: "timestamp", look_forward: true, keep_first_occurrence: false, propagate: false, event_count: 1, links: [] }) })));
    await expect(requestLinkAnalysis()).resolves.toMatchObject({ family: "LINK_ANALYSIS" });
    vi.unstubAllGlobals();
  });

  it("rejects malformed link payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "LINK_ANALYSIS", variant: "CLASSIC", links: [] }) })));
    await expect(requestLinkAnalysis()).rejects.toThrow(/invalid link analysis/);
    vi.unstubAllGlobals();
  });
});

describe("timestamp interleavings API", () => {
  it("serializes timestamp, relation-column, and result-bound options", () => {
    expect(timestampInterleavingsQuery({
      interleavings_timestamp_key: "started_at",
      interleavings_max_results: 12,
      interleavings_left_column: "left_case",
      interleavings_right_column: "right_case",
    })).toBe("interleavings_timestamp_key=started_at&interleavings_max_results=12&interleavings_left_column=left_case&interleavings_right_column=right_case");
  });

  it("uploads both logs and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({
      family: "TIMESTAMP_INTERLEAVINGS", variant: "TIMESTAMP", timestamp_key: "timestamp",
      max_interleavings: 10, interleavings: [],
    }) })));
    await expect(requestTimestampInterleavings(new File([""], "left.csv"), new File([""], "right.csv"), new File([""], "relations.csv")))
      .resolves.toMatchObject({ family: "TIMESTAMP_INTERLEAVINGS" });
    vi.unstubAllGlobals();
  });

  it("rejects malformed timestamp interleavings payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "TIMESTAMP_INTERLEAVINGS", variant: "TIMESTAMP", interleavings: [] }) })));
    await expect(requestTimestampInterleavings(new File([""], "left.csv"), new File([""], "right.csv"), new File([""], "relations.csv")))
      .rejects.toThrow(/invalid timestamp interleavings/);
    vi.unstubAllGlobals();
  });
});
