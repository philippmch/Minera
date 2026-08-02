import { describe, expect, it } from "vitest";
import { formatDuration, isEmdResult, isMineraExport, isModelEvaluation, isLogToFeaturesResult, isLabelSplittingResult, isILPMinerResult, isGeneticMinerResult, isTraceVariantQueryResult, isOrganizationalSnaResult, isRoleDiscoveryResult, isResourceProfilesResult, isLocalDiagnosticsResult, isNetworkAnalysisResult, isClusteringResult, type MineraExport } from "./types";

describe("isILPMinerResult", () => {
  const valid = { family: "ILP", net: { place_count: 2, source: 0, sink: 1, activities: ["A"], transitions: [{ activity: "A", inputs: [0], outputs: [1] }] }, markings: { initial: [1, 0], final: [0, 1] }, diagnostics: { computed: true, exhausted: false, visited_nodes: 1, lp_solved: 1, branch_and_bound_nodes: 1, message: "done" } };
  it("accepts the standalone net contract", () => expect(isILPMinerResult(valid)).toBe(true));
  it("rejects malformed markings and diagnostics", () => expect(isILPMinerResult({ ...valid, markings: { initial: [1], final: [0, 1] } })).toBe(false));
  it("rejects out-of-range net references", () => {
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, source: 2 } })).toBe(false);
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, transitions: [{ activity: "A", inputs: [2], outputs: [1] }] } })).toBe(false);
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, transitions: [{ activity: "missing", inputs: [0], outputs: [1] }] } })).toBe(false);
  });
});

describe("isGeneticMinerResult", () => {
  const valid = {
    family: "GENETIC",
    net: { place_count: 2, source: 0, sink: 1, activities: ["A"], transitions: [{ activity: 0, inputs: [0], outputs: [1] }] },
    markings: { initial: [1, 0], final: [0, 1] },
    diagnostics: { computed: true, exhausted: false, generations_completed: 2, candidates_evaluated: 8, population_size: 4, activity_count: 1, best_fitness: 1, seed: 7, message: "done" },
  };
  it("accepts the standalone contract", () => expect(isGeneticMinerResult(valid)).toBe(true));
  it("rejects invalid activity and marking references", () => {
    expect(isGeneticMinerResult({ ...valid, net: { ...valid.net, transitions: [{ activity: 2, inputs: [0], outputs: [1] }] } })).toBe(false);
    expect(isGeneticMinerResult({ ...valid, markings: { initial: [1], final: [0, 1] } })).toBe(false);
  });
});

describe("isOrganizationalSnaResult", () => {
  const valid = {
    family: "ORGANIZATIONAL_SNA", variant: "handover", directed: true, resources: ["alice", "bob"],
    edges: [{ source: 0, target: 1, weight: 0.5 }],
    diagnostics: { computed: true, output_truncated: false, trace_count: 1, event_count: 2, resource_count: 2, edge_count: 1, message: "ok" },
  };
  it("accepts a bounded SNA result", () => expect(isOrganizationalSnaResult(valid)).toBe(true));
  it("rejects out-of-range edges", () => expect(isOrganizationalSnaResult({ ...valid, edges: [{ source: 2, target: 0, weight: 1 }] })).toBe(false));
  it("requires the directedness contract", () => expect(isOrganizationalSnaResult({ ...valid, directed: undefined })).toBe(false));
});
describe("isRoleDiscoveryResult", () => {
  it("accepts deterministic roles and rejects malformed counts", () => {
    const value = { family: "ROLE_DISCOVERY", roles: [{ activities: ["A"], resources: [{ name: "r", count: 2 }] }], diagnostics: { computed: true, output_truncated: false, trace_count: 1, event_count: 1, activity_count: 1, resource_count: 1, role_count: 1, message: "ok" } };
    expect(isRoleDiscoveryResult(value)).toBe(true);
    expect(isRoleDiscoveryResult({ ...value, roles: [{ ...value.roles[0], resources: [{ name: "r", count: 0 }] }] })).toBe(false);
  });
});

describe("isResourceProfilesResult", () => {
  it("rejects unknown metric names", () => {
    const value = { family: "RESOURCE_PROFILES", metric: "not-a-metric", rows: [], diagnostics: { computed: true, output_truncated: false, trace_count: 0, event_count: 0, resource_count: 0, message: "ok" } };
    expect(isResourceProfilesResult(value)).toBe(false);
  });
});

describe("isLocalDiagnosticsResult", () => {
  const valid = {
    family: "LOCAL_DIAGNOSTICS",
    group_relative_focus: { g: { A: 1 } }, group_relative_stake: { g: { A: 1 } },
    group_coverage: { g: { r: 1 } }, group_member_contribution: { g: { r: { A: 2 } } },
    diagnostics: { computed: true, output_truncated: false, trace_count: 1, event_count: 1, group_count: 1, resource_count: 1, row_count: 1, skipped_events: 0, message: "ok" },
  };
  it("accepts nested diagnostics maps", () => expect(isLocalDiagnosticsResult(valid)).toBe(true));
  it("rejects negative member counts", () => expect(isLocalDiagnosticsResult({ ...valid, group_member_contribution: { g: { r: { A: -1 } } } })).toBe(false));
  it("rejects array maps and out-of-range ratios", () => {
    expect(isLocalDiagnosticsResult({ ...valid, group_relative_focus: [] })).toBe(false);
    expect(isLocalDiagnosticsResult({ ...valid, group_relative_focus: { g: { A: 1.1 } } })).toBe(false);
  });
});

describe("isNetworkAnalysisResult", () => {
  const valid = { family: "NETWORK_ANALYSIS", edges: [{ source: "alice", target: "bob", activity: "A", count: 1, performance: [2] }], diagnostics: { computed: true, output_truncated: false, trace_count: 1, event_count: 2, link_count: 1, row_count: 1, performance_value_count: 1, message: "ok" } };
  it("accepts deterministic edge rows", () => expect(isNetworkAnalysisResult(valid)).toBe(true));
  it("rejects negative diagnostics", () => expect(isNetworkAnalysisResult({ ...valid, diagnostics: { ...valid.diagnostics, row_count: -1 } })).toBe(false));
});

describe("isClusteringResult", () => {
  const valid = {
    family: "TRACE_ATTRIBUTE_CLUSTERING",
    tree: {
      name: "root", distance: 0.5, size: 2, leaf: false,
      children: [
        { name: "a", distance: 0, size: 1, leaf: true, children: [] },
        { name: "b", distance: 0, size: 1, leaf: true, children: [] },
      ],
    },
    leaf_names: ["a", "b"],
    diagnostics: { computed: true, output_truncated: false, trace_count: 2, group_count: 2, node_count: 3, message: "ok" },
  };
  it("accepts recursive trees", () => expect(isClusteringResult(valid)).toBe(true));
  it("rejects an internal node without children", () => expect(isClusteringResult({ ...valid, tree: { ...valid.tree, children: [] } })).toBe(false));
  it("rejects non-finite distances", () => expect(isClusteringResult({ ...valid, tree: { ...valid.tree, distance: NaN } })).toBe(false));
});

describe("isLogToFeaturesResult", () => {
  it("guards variant-specific rows", () => {
    expect(isLogToFeaturesResult({ family: "LOG_TO_FEATURES", variant: "trace_based", feature_names: ["wip"], trace_rows: [[1]] })).toBe(true);
    expect(isLogToFeaturesResult({ family: "LOG_TO_FEATURES", variant: "temporal", feature_names: ["arrivals"], temporal_rows: [{ timestamp: 1, values: [2] }] })).toBe(true);
    expect(isLogToFeaturesResult({ family: "LOG_TO_FEATURES", variant: "event_based", feature_names: [], event_rows: [[[{ value: 1 }]]] })).toBe(false);
  });
});

describe("isTraceVariantQueryResult", () => {
  const event = { activity: "A", timestamp: "1970-01-01T00:00:00Z", resource: "", lifecycle: "", input_order: 0 };
  const valid = {
    family: "TRACE_VARIANT_QUERY", variant: "sacofa", epsilon: 1, k: 1, p: 1, seed: 2,
    frequencies: [{ activities: ["A"], count: 1 }],
    diagnostics: { candidate_count: 1, retained_variant_count: 1, released_trace_count: 1, output_truncated: false, message: "ok" },
    log: { traces: [{ case_id: "tvq-1", events: [event] }] },
  };
  it("accepts a released trace distribution", () => expect(isTraceVariantQueryResult(valid)).toBe(true));
  it("rejects invalid privacy and event contracts", () => {
    expect(isTraceVariantQueryResult({ ...valid, epsilon: 0 })).toBe(false);
    expect(isTraceVariantQueryResult({ ...valid, frequencies: [{ activities: ["A"], count: 0 }] })).toBe(false);
    expect(isTraceVariantQueryResult({ ...valid, log: { traces: [{ case_id: "x", events: [{ activity: "A" }] }] } })).toBe(false);
  });
});

describe("isLabelSplittingResult", () => {
  it("guards the transformed event-log contract", () => {
    const event = { activity: "A_0", timestamp: "", resource: "", lifecycle: "", input_order: 0 };
    expect(isLabelSplittingResult({ family: "LABEL_SPLITTING", variant: "contextual", log: { traces: [{ case_id: "c", events: [event] }] } })).toBe(true);
    expect(isLabelSplittingResult({ family: "LABEL_SPLITTING", variant: "contextual", log: { traces: [{ case_id: "c", events: [{ activity: "A" }] }] } })).toBe(false);
  });
});

describe("formatDuration", () => {
  it("renders a dash for non-positive or non-finite input", () => {
    // Untimed edges read as "no data" rather than "0s".
    expect(formatDuration(0)).toBe("—");
    expect(formatDuration(-5)).toBe("—");
    expect(formatDuration(NaN)).toBe("—");
    expect(formatDuration(Infinity)).toBe("—");
  });

  it("picks the largest fitting unit and rounds seconds", () => {
    expect(formatDuration(12)).toBe("12s");
    expect(formatDuration(59.4)).toBe("59s");
    expect(formatDuration(90)).toBe("1.5m");
    expect(formatDuration(3600)).toBe("1.0h");
    expect(formatDuration(86400)).toBe("1.0d");
  });

  it("switches units exactly at the unit boundaries", () => {
    expect(formatDuration(59)).toBe("59s");
    expect(formatDuration(60)).toBe("1.0m");
    expect(formatDuration(3599)).toBe("60.0m");
    expect(formatDuration(86399)).toBe("24.0h");
  });
});

describe("isMineraExport", () => {
  const valid: MineraExport = {
    dataset: "t",
    trace_count: 1,
    event_count: 1,
    activities: [],
    edges: [],
    variants: [],
  };

  it("accepts a minimal well-formed export", () => {
    expect(isMineraExport(valid)).toBe(true);
  });

  it("accepts an export with a temporal profile", () => {
    expect(
      isMineraExport({
        ...valid,
        temporal_profile: [
          {
            source: 0,
            target: 1,
            count: 2,
            mean_seconds: 30,
            standard_deviation_seconds: 5,
          },
        ],
      }),
    ).toBe(true);
  });

  it("accepts an export with minimum self-distances", () => {
    expect(
      isMineraExport({
        ...valid,
        minimum_self_distances: [
          { activity: 0, distance: 1, witnesses: [1, 2] },
        ],
      }),
    ).toBe(true);
  });

  it("accepts a transition-system export with multiset payloads", () => {
    expect(isMineraExport({
      ...valid,
      transition_system: {
        view: "multiset", direction: "forward", window: 2,
        include_data: true,
        states: [{ id: 0, payload: [{ activity: 0, count: 2 }], start_count: 1, end_count: 0,
          incoming_occurrences: [], outgoing_occurrences: [{ trace_index: 0, event_index: 0 }] }],
        transitions: [{ source: 0, target: 0, activity: 0, count: 1,
          occurrences: [{ trace_index: 0, event_index: 0 }] }],
      },
    })).toBe(true);
  });

  it("rejects non-objects", () => {
    expect(isMineraExport(null)).toBe(false);
    expect(isMineraExport(42)).toBe(false);
    expect(isMineraExport("{}")).toBe(false);
    expect(isMineraExport(undefined)).toBe(false);
  });

  it("rejects an object missing any required array", () => {
    expect(isMineraExport({ ...valid, activities: undefined })).toBe(false);
    expect(isMineraExport({ ...valid, edges: undefined })).toBe(false);
    expect(isMineraExport({ ...valid, variants: undefined })).toBe(false);
  });

  it("rejects a non-finite trace_count", () => {
    expect(isMineraExport({ ...valid, trace_count: NaN })).toBe(false);
    expect(isMineraExport({ ...valid, trace_count: "1" })).toBe(false);
  });
});

describe("model-quality result guards", () => {
  it("accepts each evaluator family's standalone result", () => {
    const common = { computed: true, error: null };
    expect(isModelEvaluation({ miner: "inductive", replay_fitness: {
      ...common, variant: "token_based", exhausted: false, fitness: 1, log_fitness: 1,
      average_trace_fitness: 1, percentage_of_fitting_traces: 100, fitting_traces: 2,
      trace_count: 2, total_cost: 0,
    } })).toBe(true);
    expect(isModelEvaluation({ miner: "alpha", precision: {
      ...common, variant: "automaton_after_align", exhausted: false, precision: 0.75,
      alignments: {},
    } })).toBe(true);
    expect(isModelEvaluation({ miner: "heuristic", generalization: {
      ...common, variant: "generalization_token", score: 0.5, executed_transitions: 2,
      transition_count: 3, transition_frequencies: [4, 0, 2],
    } })).toBe(true);
    expect(isModelEvaluation({ miner: "powl", simplicity: {
      ...common, variant: "extended_cyclomatic", exhausted: false, simplicity: null,
      complexity: null, value: null, vertices: 3, edges: 2, strongly_connected_components: 1,
    } })).toBe(true);
  });

  it("rejects missing, multiple, or malformed evaluator families", () => {
    expect(isModelEvaluation({ miner: "inductive" })).toBe(false);
    expect(isModelEvaluation({ miner: "inductive", precision: {}, simplicity: {} })).toBe(false);
    expect(isModelEvaluation({ miner: "inductive", precision: {
      variant: "auto", computed: true, exhausted: false, error: null, precision: 1,
    } })).toBe(false);
  });

  it("accepts computed and failed stochastic-language distances", () => {
    expect(isEmdResult({ distance: 0.125, computed: true, normalized: true, error: null })).toBe(true);
    expect(isEmdResult({ distance: null, computed: false, normalized: false, error: "unequal mass" })).toBe(true);
    expect(isEmdResult({ distance: NaN, computed: false, normalized: false, error: null })).toBe(false);
  });
});
