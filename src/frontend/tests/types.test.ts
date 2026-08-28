import { describe, expect, it } from "vitest";
import { formatDuration, isDfgPetriConversionResult, isDfgPrecisionResult, isDfgPlayoutVariantsResult, isBasicPlayoutVisitedResult, isProcessTreePlayoutSetResult, isTriePetriConversionResult, isEmdResult, isMineraExport, isPowlNode, isRandomTreeResult, isTimestampInterleavingsResult, isMonteCarloResult, isModelEvaluation, isLogToFeaturesResult, isLogUtilityResult, isEmbeddingResult, isEmbeddingSimilarityResult, isLabelSplittingResult, isILPMinerResult, isGeneticMinerResult, isTraceVariantQueryResult, isOrganizationalSnaResult, isRoleDiscoveryResult, isResourceProfilesResult, isLocalDiagnosticsResult, isNetworkAnalysisResult, isClusteringResult, isProfileClusteringResult, isOcelDiscoveryResult, isOcelPlayoutResult, isOcelEnrichmentResult, isOcelObjectGraphResult, isOcelHeterogeneousGraphResult, isOcelFeatureResult, isOcelStatisticsResult, isOcelDescriptionResult, isOcelSqliteResult, isOcelExchangeResult, isProcessCubeResult, isProcessCubeTable, isStreamingDfgResult, isStreamingFootprintsResult, isStreamingTemporalResult, isStreamingTbrResult, isDataPetriNetResult, isChaoticActivitiesResult, isPassedTimeResult, isServiceTimeResult, isReworkResult, isCycleTimeResult, isCaseArrivalResult, isCaseStatisticsResult, isAttributeStatisticsResult, isAttributeDistributionResult, isAttributeKdeResult, isBoundaryActivitiesResult, isVariantStatisticsResult, isPetriAnalysisResult, isConceptDriftResult, type MineraExport } from "../src/types";

describe("isProcessTreePlayoutSetResult", () => {
  const valid = {
    family: "PROCESS_TREE_PLAYOUT", variant: "EXTENSIVE",
    return_set_strings: true, traces: [[], ["A", "B"]],
  };
  it("accepts sorted activity words", () => {
    expect(isProcessTreePlayoutSetResult(valid)).toBe(true);
  });
  it("rejects malformed words and variants", () => {
    expect(isProcessTreePlayoutSetResult({ ...valid, traces: [["A", 2]] })).toBe(false);
    expect(isProcessTreePlayoutSetResult({ ...valid, variant: "TOPBOTTOM" })).toBe(false);
    expect(isProcessTreePlayoutSetResult({ ...valid, traces: [["A"], ["A"]] })).toBe(false);
    expect(isProcessTreePlayoutSetResult({ ...valid, traces: [["A", "B"], ["A"]] })).toBe(false);
  });
});

describe("isBasicPlayoutVisitedResult", () => {
  const valid = {
    family: "PETRI_PLAYOUT_VISITED", variant: "BASIC_PLAYOUT",
    traces: [[
      { kind: "marking", marking: [1, 0] },
      { kind: "transition", transition: 0, activity: null },
      { kind: "marking", marking: [0, 1] },
    ]],
  };
  it("accepts the discriminated visitation envelope", () => {
    expect(isBasicPlayoutVisitedResult(valid)).toBe(true);
  });
  it("rejects malformed elements", () => {
    expect(isBasicPlayoutVisitedResult({ ...valid, traces: [[
      { kind: "transition", transition: -1, activity: null },
    ]] })).toBe(false);
    expect(isBasicPlayoutVisitedResult({ ...valid, traces: [[
      { kind: "marking", marking: [1, -1] },
    ]] })).toBe(false);
    expect(isBasicPlayoutVisitedResult({ ...valid, traces: [[
      { kind: "transition", transition: 0, activity: null },
      { kind: "marking", marking: [1, 0] },
    ]] })).toBe(false);
    expect(isBasicPlayoutVisitedResult({ ...valid, traces: [[
      { kind: "marking", marking: [1, 0] },
      { kind: "transition", transition: 0, activity: null },
    ]] })).toBe(false);
  });
});

describe("OCPN token replay result guard", () => {
  const diagnostic = { enabled: true, trace_count: 1, fitting_traces: 1,
    places: [{ m: 0, r: 0, c: 1, p: 1 }],
    transition_activations: [1] };
  it("accepts deterministic per-type diagnostics and disabled empty results", () => {
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: { order: { place_count: 1, transitions: [{}] } },
        tbr_results: { order: diagnostic } } })).toBe(true);
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: {}, tbr_results: {} } })).toBe(true);
  });
  it("rejects malformed counters", () => {
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: { order: { place_count: 1, transitions: [{}] } },
        tbr_results: { order: { ...diagnostic, places: [{ ...diagnostic.places[0], m: -1 }] } } } })).toBe(false);
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: { order: { place_count: 1, transitions: [{}] } },
        tbr_results: { order: { ...diagnostic, enabled: false } } } })).toBe(false);
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: { order: { place_count: 2, transitions: [{}] } },
        tbr_results: { order: diagnostic } } })).toBe(false);
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: { order: { place_count: 1, transitions: [{}] } },
        tbr_results: { item: diagnostic } } })).toBe(false);
    expect(isOcelDiscoveryResult({ family: "OCEL_DISCOVERY", variant: "ocpn",
      result: { petri_nets: { order: { place_count: 1, transitions: [{}] } },
        tbr_results: { order: { ...diagnostic, fitting_traces: 2 } } } })).toBe(false);
  });
});

describe("inclusive process-tree JSON", () => {
  it("accepts inclusive OR nodes in random-tree responses", () => {
    const result = {
      family: "RANDOM_TREE", variant: "PTANDLOGGENERATOR", activities: ["A", "B"],
      tree: {
        operator: "inclusive_or", children: [
          { operator: "activity", activity: "A", children: [] },
          { operator: "activity", activity: "B", children: [] },
        ],
      },
    };
    expect(isRandomTreeResult(result)).toBe(true);
  });
});

describe("isConceptDriftResult", () => {
  const valid = {
    family: "CONCEPT_DRIFT", variant: "bose", activity_attribute: "event:type",
    timestamp_attribute: "started_at", case_id_attribute: "case:order",
    activities: ["A"], feature_vectors: [[1, 0, 0]],
    sublogs: [{ trace_count: 1, event_count: 1 }],
    cumulative_sublogs: [{ trace_count: 1, event_count: 1 }],
    sublog_start_timestamps: [1], change_points: [{ sublog_index: 1, p_value: 0.25 }],
    change_timestamps: [1],
  };
  it("accepts selector metadata and bounded numeric arrays", () => {
    expect(isConceptDriftResult(valid)).toBe(true);
  });
  it("rejects malformed drift diagnostics", () => {
    expect(isConceptDriftResult({ ...valid, change_points: [{ sublog_index: -1, p_value: 0.25 }] })).toBe(false);
    expect(isConceptDriftResult({ ...valid, feature_vectors: [[Number.NaN]] })).toBe(false);
    expect(isConceptDriftResult({ ...valid, activity_attribute: 7 })).toBe(false);
    expect(isConceptDriftResult({ ...valid, variant: "other" })).toBe(false);
  });
});

describe("isDfgPrecisionResult", () => {
  const valid = { family: "DFG_PRECISION", computed: true, precision: 0.75,
    prefix_occurrences: 4, allowed_prefix_occurrences: 3,
    activated_transitions: 8, escaping_transitions: 2, error: "" };
  it("accepts bounded precision diagnostics", () => expect(isDfgPrecisionResult(valid)).toBe(true));
  it("rejects invalid metrics and counters", () => {
    expect(isDfgPrecisionResult({ ...valid, precision: 2 })).toBe(false);
    expect(isDfgPrecisionResult({ ...valid, escaping_transitions: -1 })).toBe(false);
    expect(isDfgPrecisionResult({ ...valid, error: null })).toBe(false);
  });
});

describe("isDfgPlayoutVariantsResult", () => {
  const valid = {
    family: "DFG_PLAYOUT", variant: "CLASSIC", return_variants: true,
    complete: true, interrupted: false,
    variants: [{ activities: ["A", "B"], occurrences: 3 }],
  };
  it("accepts deterministic sequence/count envelopes", () => {
    expect(isDfgPlayoutVariantsResult(valid)).toBe(true);
  });
  it("rejects malformed entries and status fields", () => {
    expect(isDfgPlayoutVariantsResult({ ...valid, variants: [{ activities: [], occurrences: 1 }] })).toBe(false);
    expect(isDfgPlayoutVariantsResult({ ...valid, variants: [{ activities: ["A"], occurrences: -1 }] })).toBe(false);
    expect(isDfgPlayoutVariantsResult({ ...valid, interrupted: "false" })).toBe(false);
  });
});

describe("isLogUtilityResult", () => {
  const event = { case_id: "c1", activity: "A", timestamp: "1", resource: "", lifecycle: "", input_order: 0, attributes: [] };
  const log = { attributes: [], traces: [{ case_id: "c1", attributes: [], events: [event] }] };
  it("accepts split, prefix, and target envelopes", () => {
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "SPLIT_TRAIN_TEST", training: log, testing: log })).toBe(true);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "ALL_PREFIXES", log, change_indexes: [[0]] })).toBe(true);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "UNTIL_ACTIVITY", log, elapsed_seconds: [1] })).toBe(true);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "TO_ACTIVITIES", activities: ["C"], logs: [log] })).toBe(true);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "FEATURE_SELECTION", event_attributes: ["activity"], case_attributes: ["region"] })).toBe(true);
  });
  it("rejects malformed durations and event indexes", () => {
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "UNTIL_ACTIVITY", log, elapsed_seconds: [-1] })).toBe(false);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "FIXED_PREFIXES", log: { ...log, traces: [{ ...log.traces[0], events: [{ ...event, input_order: -1 }] }] } })).toBe(false);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "TO_ACTIVITIES", activities: ["C", "B"], logs: [log] })).toBe(false);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "FEATURE_SELECTION", event_attributes: ["activity"], case_attributes: "region" })).toBe(false);
  });
});

describe("isStreamingDfgResult", () => {
  const valid = {
    family: "STREAMING_DFG", events: 2,
    graph: [{ source: "A", target: "B", count: 1 }],
    activities: { A: 1, B: 1 }, start_activities: { A: 1 },
    end_activities: { B: 1 }, open_cases: { c1: "B" },
  };
  it("accepts deterministic snapshots", () => expect(isStreamingDfgResult(valid)).toBe(true));
  it("rejects negative counts and malformed edges", () => {
    expect(isStreamingDfgResult({ ...valid, events: -1 })).toBe(false);
    expect(isStreamingDfgResult({ ...valid, graph: [{ source: "A", target: "B", count: 0 }] })).toBe(false);
  });
});

describe("isDataPetriNetResult", () => {
  const valid = {
    family: "DATA_PETRI_NET", variant: "decision_tree", place_count: 3,
    source: 0, sink: 2, activities: ["A"],
    transitions: [{ index: 0, activity: "A", silent: false, inputs: [0], outputs: [2],
      guard: "true", read_variables: [], write_variables: [] }],
    decisions: { observed_traces: 1, replayed_traces: 1, aligned_traces: 0,
      alignment_failures: 0, trace_count: 1, decision_points: [] },
    decision_tables: { family: "DECISION_TABLES", variant: "encoded", trace_count: 1,
      observed_traces: 1, tables: [{ place: 1, classes: ["B", "C"],
        feature_names: ["amount"], rows: [{ features: [10], target: 0 }] }] },
  };
  it("accepts guarded net envelopes", () => expect(isDataPetriNetResult(valid)).toBe(true));
  it("rejects malformed guard transitions", () => {
    expect(isDataPetriNetResult({ ...valid, transitions: [{ ...valid.transitions[0], guard: 4 }] })).toBe(false);
    expect(isDataPetriNetResult({ ...valid, decisions: { ...valid.decisions, observed_traces: -1 } })).toBe(false);
    expect(isDataPetriNetResult({ ...valid, decisions: { ...valid.decisions, activity_attribute: 4 } })).toBe(false);
    expect(isDataPetriNetResult({ ...valid, decisions: { ...valid.decisions, aligned_traces: 2 } })).toBe(false);
    expect(isDataPetriNetResult({ ...valid, decision_tables: { ...valid.decision_tables, tables: [{ ...valid.decision_tables.tables[0], rows: [{ features: [], target: 3 }] }] } })).toBe(false);
  });
});

describe("isStreamingFootprintsResult", () => {
  const valid = {
    family: "STREAMING_FOOTPRINTS", total_events_processed: 2,
    total_deviations: 0,
    cases: [{ case_id: "c1", events: 2, deviations: 0, is_fit: true,
      first_activity: "A", last_activity: "B" }],
  };
  it("accepts snapshots and rejects invalid counters", () => {
    expect(isStreamingFootprintsResult(valid)).toBe(true);
    expect(isStreamingFootprintsResult({ ...valid, total_deviations: -1 })).toBe(false);
    expect(isStreamingFootprintsResult({ ...valid, cases: [{ ...valid.cases[0], deviations: 1.5 }] })).toBe(false);
  });
});

describe("isStreamingTemporalResult", () => {
  const valid = {
    family: "STREAMING_TEMPORAL", total_events_processed: 2,
    total_deviations: 1, cases: { c1: { events: 2, deviations: 1 } },
    deviations: { c1: [{ source: "A", target: "B", elapsed_seconds: 19,
      standard_deviation_distance: 4.5 }] },
  };
  it("accepts finite diagnostics and rejects malformed values", () => {
    expect(isStreamingTemporalResult(valid)).toBe(true);
    expect(isStreamingTemporalResult({ ...valid, deviations: { c1: [{ ...valid.deviations.c1[0], elapsed_seconds: Number.NaN }] } })).toBe(false);
    expect(isStreamingTemporalResult({ ...valid, cases: { c1: { events: 2.5, deviations: 1 } } })).toBe(false);
  });
});

describe("isStreamingTbrResult", () => {
  const valid = {
    family: "STREAMING_TBR", variant: "FORWARD", total_events_processed: 1,
    total_missing: 0, cases: { c1: { events: 1, missing: 0, remaining: 0,
      is_fit: true, marking: [0, 1] } },
  };
  it("accepts snapshots and rejects invalid markings", () => {
    expect(isStreamingTbrResult(valid)).toBe(true);
    expect(isStreamingTbrResult({ ...valid, variant: "unknown" })).toBe(false);
    expect(isStreamingTbrResult({ ...valid, cases: { c1: { ...valid.cases.c1, marking: [-1] } } })).toBe(false);
  });
});

describe("isDfgPetriConversionResult", () => {
  const valid = {
    family: "DFG_CONVERSION", variant: "ACTIVITY_DEFINES_PLACE",
    source: 0, sink: 1, activities: ["A"], place_count: 2,
    transitions: [{ activity: 0, inputs: [0], outputs: [1] }],
  };
  it("accepts the canonical conversion envelope", () => {
    expect(isDfgPetriConversionResult(valid)).toBe(true);
  });
  it("rejects invalid place and activity references", () => {
    expect(isDfgPetriConversionResult({ ...valid, sink: 2 })).toBe(false);
    expect(isDfgPetriConversionResult({ ...valid, transitions: [{ activity: 1, inputs: [0], outputs: [1] }] })).toBe(false);
    expect(isDfgPetriConversionResult({ ...valid, transitions: [{ activity: null, inputs: [2], outputs: [1] }] })).toBe(false);
  });
});

describe("isTriePetriConversionResult", () => {
  const valid = {
    family: "TRIE_CONVERSION", variant: "TO_PETRI_NET", source: 0,
    sink: 1, activities: ["A"], place_count: 2,
    transitions: [{ activity: 0, inputs: [0], outputs: [1] }],
  };
  it("accepts and rejects bounded references", () => {
    expect(isTriePetriConversionResult(valid)).toBe(true);
    expect(isTriePetriConversionResult({ ...valid, transitions: [{ activity: null, inputs: [2], outputs: [1] }] })).toBe(false);
  });
});

describe("isOcelPlayoutResult", () => {
  const valid = { family: "OCEL_PLAYOUT", variant: "EXTENSIVE", log: { "ocel:events": {} } };
  it("accepts the recursive OCEL log envelope", () => expect(isOcelPlayoutResult(valid)).toBe(true));
  it("accepts shared binding metadata and rejects unknown modes", () => {
    expect(isOcelPlayoutResult({ ...valid, event_binding: "shared_activity" })).toBe(true);
    expect(isOcelPlayoutResult({ ...valid, event_binding: "synchronized" })).toBe(true);
    expect(isOcelPlayoutResult({ ...valid, exists_trace: true, sequences: [[{
      activity: "Create", objects: [{ type: "order", id: "o1" }],
    }]] })).toBe(true);
    expect(isOcelPlayoutResult({ ...valid, sequences: [{ activity: "Create" }] })).toBe(false);
    expect(isOcelPlayoutResult({ ...valid, event_binding: "unknown" })).toBe(false);
  });
  it("accepts the causal-net envelope", () => expect(isOcelPlayoutResult({ ...valid, family: "OC_CAUSAL_NET_PLAYOUT" })).toBe(true));
  it("rejects an incomplete envelope", () => expect(isOcelPlayoutResult({ ...valid, log: [] })).toBe(false));
});

describe("isOcelEnrichmentResult", () => {
  const valid = {
    family: "OCEL_ENRICHMENT", variant: "CLASSIC",
    graphs: ["object_interaction_graph", "object_codeath_graph"],
    lifecycle_qualifiers: true,
    log: { "ocel:events": {}, "ocel:objects": {} },
  };
  it("accepts selected graph families", () => expect(isOcelEnrichmentResult(valid)).toBe(true));
  it("rejects unknown graph families and malformed logs", () => {
    expect(isOcelEnrichmentResult({ ...valid, graphs: ["unknown"] })).toBe(false);
    expect(isOcelEnrichmentResult({ ...valid, log: [] })).toBe(false);
  });
});

describe("isOcelObjectGraphResult", () => {
  const valid = {
    family: "OCEL_OBJECT_GRAPH", variant: "object_interaction_graph",
    object_ids: ["i1", "o1"], edges: [["i1", "o1"]],
  };
  it("accepts deterministic object ids and pair edges", () => {
    expect(isOcelObjectGraphResult(valid)).toBe(true);
  });
  it("rejects unknown variants and malformed edges", () => {
    expect(isOcelObjectGraphResult({ ...valid, variant: "unknown" })).toBe(false);
    expect(isOcelObjectGraphResult({ ...valid, edges: [["i1"]] })).toBe(false);
    expect(isOcelObjectGraphResult({ ...valid, object_ids: [1] })).toBe(false);
  });
});

describe("isOcelHeterogeneousGraphResult", () => {
  const valid = {
    family: "OCEL_GRAPH", variant: "HETEROGENEOUS",
    include_df: true, include_object_changes: true,
    nodes: [
      { id: "e1", type: "EVENT", activity: "create", timestamp: "2024-01-01T00:00:00Z", attributes: {} },
      { id: "o1", type: "OBJECT", object_type: "order", attributes: { region: "EU" } },
      { id: "@@change##0", type: "CHANGE", object_id: "o1", object_type: "order", timestamp: "2024-01-01T00:00:00Z", field: "status", value: null },
    ],
    edges: [
      { source: "e1", target: "o1", type: "E2O", qualifier: "" },
      { source: "e1", target: "e1", type: "DF", object: "o1" },
    ],
  };
  it("accepts event, object, change and derived-flow records", () => {
    expect(isOcelHeterogeneousGraphResult(valid)).toBe(true);
  });
  it("rejects malformed node and edge records", () => {
    expect(isOcelHeterogeneousGraphResult({ ...valid, nodes: [{ id: "e1", type: "OTHER" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, nodes: [{ id: "e1", type: "EVENT", activity: "create" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, nodes: [{ id: "o1", type: "OBJECT", object_type: "order" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, nodes: [{ id: "@@change##0", type: "CHANGE", object_id: "o1" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, edges: [{ source: "e1", target: "o1", type: "OTHER" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, edges: [{ source: "e1", target: "o1", type: "E2O" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, edges: [{ source: "e1", target: "e2", type: "DF" }] }))
      .toBe(false);
    expect(isOcelHeterogeneousGraphResult({ ...valid, include_df: "true" })).toBe(false);
  });
});

describe("isOcelSqliteResult", () => {
  it("accepts a valid imported log", () => expect(isOcelSqliteResult({
    family: "OCEL_SQLITE", variant: "IMPORT",
    log: { "ocel:events": {}, "ocel:objects": {} },
  })).toBe(true));
  it("rejects a malformed imported log", () => expect(isOcelSqliteResult({
    family: "OCEL_SQLITE", variant: "IMPORT", log: [],
  })).toBe(false));
});

describe("isOcelExchangeResult", () => {
  it("accepts every JSON/XML import variant", () => {
    for (const variant of ["IMPORT_JSON", "IMPORT_JSON_STANDARD", "IMPORT_XML", "IMPORT_XML_STANDARD", "IMPORT_CSV"]) {
      expect(isOcelExchangeResult({
        family: "OCEL_IO", variant,
        log: { "ocel:events": {}, "ocel:objects": {} },
      })).toBe(true);
    }
  });
  it("rejects unknown formats and incomplete logs", () => {
    expect(isOcelExchangeResult({
      family: "OCEL_IO", variant: "IMPORT_DFG",
      log: { "ocel:events": {}, "ocel:objects": {} },
    })).toBe(false);
    expect(isOcelExchangeResult({
      family: "OCEL_IO", variant: "IMPORT_XML", log: {},
    })).toBe(false);
  });
});

describe("isOcelFeatureResult", () => {
  const valid = {
    family: "OCEL_FEATURES", variant: "OBJECTS", row_ids: ["o1"],
    row_types: ["order"], feature_names: ["@@object_lifecycle_length"], values: [[2]],
  };
  it("accepts rectangular numeric feature tables", () => expect(isOcelFeatureResult(valid)).toBe(true));
  it("rejects ragged and non-finite tables", () => {
    expect(isOcelFeatureResult({ ...valid, values: [[]] })).toBe(false);
    expect(isOcelFeatureResult({ ...valid, values: [[Number.NaN]] })).toBe(false);
  });
});

describe("isProcessCubeResult", () => {
  const valid = {
    family: "PROCESS_CUBE", x_dimension: "x", y_dimension: "y", aggregation: "mean",
    x_bins: [{ label: "[0,1]", lower: 0, upper: 1, numeric: true }],
    y_bins: [{ label: "[0,1]", lower: 0, upper: 1, numeric: true }],
    x_edges: [0, 1], y_edges: [0, 1], cells: [[2]], case_ids: [[['case-1']]],
  };
  it("accepts rectangular cells with finite bins and case sets", () => {
    expect(isProcessCubeResult(valid)).toBe(true);
  });
  it("rejects ragged cells, non-finite values, and null envelopes", () => {
    expect(isProcessCubeResult({ ...valid, cells: [[]] })).toBe(false);
    expect(isProcessCubeResult({ ...valid, cells: [[Number.NaN]] })).toBe(false);
    expect(isProcessCubeResult(null)).toBe(false);
  });
});

describe("isProcessCubeTable", () => {
  it("accepts rectangular numeric, categorical, and mixed columns", () => {
    expect(isProcessCubeTable({
      case_ids: ["a", "b"],
      columns: { region: ["US", "DE"], value: [1, 2] },
    })).toBe(true);
    expect(isProcessCubeTable({
      case_ids: ["a", "b"],
      columns: { value: [1, 2] },
      categorical_columns: { region: ["US", "DE"] },
    })).toBe(true);
  });
  it("rejects ragged, mixed-type, or duplicate categorical columns", () => {
    expect(isProcessCubeTable({ case_ids: ["a", "b"], columns: { x: [1] } })).toBe(false);
    expect(isProcessCubeTable({ case_ids: ["a", "b"], columns: { x: [1, "two"] } })).toBe(false);
    expect(isProcessCubeTable({
      case_ids: ["a", "b"], columns: { x: [1, 2] }, categorical_columns: { x: ["a", "b"] },
    })).toBe(false);
  });
});

describe("isPetriAnalysisResult structural diagnostics", () => {
  it("accepts soundness pair diagnostics and null optional fields", () => {
    expect(isPetriAnalysisResult({
      family: "soundness", status: "unsound",
      handled_pairs: [[1, 12]], not_well_handled_pairs: null,
    })).toBe(true);
    expect(isPetriAnalysisResult({ family: "soundness", status: "sound", handled_pairs: null })).toBe(true);
    expect(isPetriAnalysisResult({ family: "woflan", uniform_invariant_search_exhausted: true })).toBe(true);
    expect(isPetriAnalysisResult({ family: "soundness", dead_tasks: [0, 2], locking_scenarios: [[1], [0, 2]] })).toBe(true);
    expect(isPetriAnalysisResult({ family: "soundness", structural_node_labels: ["place:0:in", "transition:0:A:in"] })).toBe(true);
  });
  it("rejects malformed structural pair diagnostics", () => {
    expect(isPetriAnalysisResult({ family: "soundness", handled_pairs: [[1]] })).toBe(false);
    expect(isPetriAnalysisResult({ family: "soundness", not_well_handled_pairs: [[1, -1]] })).toBe(false);
    expect(isPetriAnalysisResult({ family: "soundness", handled_pairs: [[1, 2.5]] })).toBe(false);
    expect(isPetriAnalysisResult({ family: "soundness", dead_tasks: [-1] })).toBe(false);
    expect(isPetriAnalysisResult({ family: "soundness", locking_scenarios: [[0], [1, 2.5]] })).toBe(false);
    expect(isPetriAnalysisResult({ family: "soundness", structural_node_labels: ["ok", 2] })).toBe(false);
  });
});

describe("isPetriAnalysisResult soundness graph diagnostics", () => {
  it("accepts invariant families and compact graph projections", () => {
    expect(isPetriAnalysisResult({
      family: "soundness",
      uniform_place_invariants: [[1, 1, 0]],
      weighted_place_invariants: [[1, 2, 0]],
      uncovered_places_uniform: [],
      coverability_markings: [[1, 0], [-1, 0]],
      coverability_edges: [[0, 1, 0]],
      left: true,
      restricted_coverability_tree_exhausted: false,
      restricted_coverability_markings: [[1, 0], [-1, 0]],
      restricted_coverability_edges: [[0, 1, 0]],
      reachability_markings: [[1, 0], [0, 1]],
      reachability_edges: [[0, 1, 2]],
    })).toBe(true);
  });

  it("rejects malformed invariant and graph diagnostics", () => {
    expect(isPetriAnalysisResult({
      family: "soundness", uniform_place_invariants: [[1, -1]],
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "soundness", coverability_markings: [[1, -2]],
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "soundness", reachability_markings: [[1, -1]],
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "soundness", reachability_edges: [[0, 1]],
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "soundness", weighted_place_invariants: [[1, Number.NaN]],
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "woflan", left: "true",
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "woflan", restricted_coverability_tree_exhausted: "false",
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "woflan", restricted_coverability_markings: [[1, -2]],
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "woflan", restricted_coverability_edges: [[0, 1]],
    })).toBe(false);
  });

  it("accepts Woflan short-circuit projections", () => {
    expect(isPetriAnalysisResult({
      family: "woflan", variant: "woflan",
      short_circuit_valid: true,
      short_circuit_strongly_connected: true,
      short_circuit_transition: 2,
      short_circuit_diagnostic_messages: ["connected"],
      short_circuit_reachability_exhausted: false,
      short_circuit_reachability_strongly_connected: true,
      short_circuit_reachability_markings: [[1, 0], [0, 1]],
      short_circuit_reachability_edges: [[0, 1, 0]],
    })).toBe(true);
    expect(isPetriAnalysisResult({
      family: "woflan", short_circuit_transition: -1,
    })).toBe(false);
    expect(isPetriAnalysisResult({
      family: "woflan", short_circuit_diagnostic_messages: [2],
    })).toBe(false);
  });
});

describe("isOcelStatisticsResult", () => {
  it("accepts both deterministic summary variants", () => {
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "OBJECT_TYPE_ACTIVITIES",
      object_type_activities: { item: ["Create", "Ship"] },
    })).toBe(true);
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "OBJECTS_OT_COUNT",
      objects_ot_count: { e1: { item: 2 } },
    })).toBe(true);
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "ACTIVITY_ASSOCIATIONS",
      associations: { Create: [["e1", "o1"]] },
    })).toBe(true);
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "EDGE_ASSOCIATIONS",
      edges: { item: { "Create->Ship": {
        event_couples: [["e1", "e2"]], unique_objects: ["o1"],
        event_objects: [["e1", "e2", "o1"]], performance: [1],
        total_objects: [["e1", "e2", "o1"]],
        performance_event_couples: [1], performance_total_objects: [1],
      } } },
    })).toBe(true);
  });
  it("rejects malformed summary payloads", () => {
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "OBJECT_TYPE_ACTIVITIES",
      object_type_activities: { item: [1] },
    })).toBe(false);
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "OBJECTS_OT_COUNT",
      objects_ot_count: { e1: { item: -1 } },
    })).toBe(false);
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "ACTIVITY_ASSOCIATIONS",
      associations: { Create: [["e1"]] },
    })).toBe(false);
    expect(isOcelStatisticsResult({
      family: "OCEL_STATISTICS", variant: "EDGE_ASSOCIATIONS",
      edges: { item: { "Create->Ship": {
        event_couples: [], unique_objects: [], event_objects: [], performance: [],
        total_objects: [["e1", "e2"]],
      } } },
    })).toBe(false);
  });
});

describe("isOcelDescriptionResult", () => {
  it("accepts textual and tuple envelopes", () => {
    expect(isOcelDescriptionResult({
      family: "OCEL_DESCRIPTION", variant: "VARIANT1",
      includes_timestamps: true, text: "events:",
    })).toBe(true);
    expect(isOcelDescriptionResult({
      family: "OCEL_DESCRIPTION", variant: "VARIANT2",
      includes_timestamps: true,
      events: [["Create", "o1", "2024-01-01T00:00:00Z"]],
      objects: [["o1", "order", "2024-01-01T00:00:00Z",
        "2024-01-02T00:00:00Z", 86400]],
    })).toBe(true);
  });
  it("rejects malformed tuple fields", () => {
    expect(isOcelDescriptionResult({
      family: "OCEL_DESCRIPTION", variant: "VARIANT2",
      includes_timestamps: true, events: [["Create"]], objects: [["o1", "order"]],
    })).toBe(false);
    expect(isOcelDescriptionResult({
      family: "OCEL_DESCRIPTION", variant: "VARIANT1",
      includes_timestamps: true, text: 2,
    })).toBe(false);
  });
});

describe("isChaoticActivitiesResult", () => {
  const valid = {
    family: "CHAOTIC_ACTIVITIES", variant: "NIEK_SIDOROVA", alpha: 0.5,
    activities: [{ activity: "A", frequency: 2, entropy: 0,
      entropy_smooth: 1, entropy_gain: 0.25, chaotic_score: 0.625 }],
    diagnostics: { activity_count: 1, event_count: 2,
      output_truncated: false, message: "ok" },
  };
  it("accepts the deterministic envelope", () => {
    expect(isChaoticActivitiesResult(valid)).toBe(true);
  });
  it("rejects non-finite metrics and malformed diagnostics", () => {
    expect(isChaoticActivitiesResult({ ...valid,
      activities: [{ ...valid.activities[0], entropy: Number.NaN }] })).toBe(false);
    expect(isChaoticActivitiesResult({ ...valid,
      diagnostics: { ...valid.diagnostics, event_count: -1 } })).toBe(false);
  });
});

describe("isPassedTimeResult", () => {
  const valid = {
    family: "PASSED_TIME", variant: "PREPOST", activity: "B",
    pre: [{ activity: "A", performance: 1200, frequency: 2 }],
    post: [{ activity: "C", performance: 900, frequency: 2 }],
    pre_avg_perf: 1200, post_avg_perf: 900,
  };
  it("accepts all three-variant envelope fields", () => {
    expect(isPassedTimeResult(valid)).toBe(true);
  });
  it("rejects malformed performance and frequencies", () => {
    expect(isPassedTimeResult({ ...valid,
      pre: [{ ...valid.pre[0], performance: Number.NaN }] })).toBe(false);
    expect(isPassedTimeResult({ ...valid,
      post: [{ ...valid.post[0], frequency: -1 }] })).toBe(false);
  });
});

describe("isServiceTimeResult", () => {
  const valid = { family: "SERVICE_TIME", variant: "CLASSIC", aggregation: "MEAN", values: { A: 900, B: 2100 } };
  it("accepts finite activity values", () => expect(isServiceTimeResult(valid)).toBe(true));
  it("rejects non-finite values and wrong variants", () => {
    expect(isServiceTimeResult({ ...valid, values: { A: Number.NaN } })).toBe(false);
    expect(isServiceTimeResult({ ...valid, variant: "OTHER" })).toBe(false);
  });
});

describe("isReworkResult", () => {
  it("accepts activity and case variants", () => {
    expect(isReworkResult({ family: "REWORK", variant: "ACTIVITIES", activity_cases: { A: 1 } })).toBe(true);
    expect(isReworkResult({ family: "REWORK", variant: "CASES", cases: { c1: { number_activities: 3, rework: 1 } } })).toBe(true);
  });
  it("rejects impossible counts", () => {
    expect(isReworkResult({ family: "REWORK", variant: "ACTIVITIES", activity_cases: { A: -1 } })).toBe(false);
    expect(isReworkResult({ family: "REWORK", variant: "CASES", cases: { c1: { number_activities: 1, rework: 2 } } })).toBe(false);
  });
});

describe("isCycleTimeResult", () => {
  const valid = {
    family: "CYCLE_TIME", variant: "CLASSIC",
    cycle_time: 10, instances: 2, timed_events: 4,
  };
  it("accepts the bounded timing envelope", () => expect(isCycleTimeResult(valid)).toBe(true));
  it("rejects negative or non-integral diagnostics", () => {
    expect(isCycleTimeResult({ ...valid, cycle_time: -1 })).toBe(false);
    expect(isCycleTimeResult({ ...valid, instances: 1.5 })).toBe(false);
  });
});

describe("isCaseArrivalResult", () => {
  const valid = {
    family: "CASE_ARRIVAL", variant: "CLASSIC",
    arrival_average: 7200, dispersion_average: 7200,
    timed_starts: 3, timed_ends: 3,
  };
  it("accepts the arrival envelope", () => expect(isCaseArrivalResult(valid)).toBe(true));
  it("rejects invalid averages and counts", () => {
    expect(isCaseArrivalResult({ ...valid, arrival_average: -1 })).toBe(false);
    expect(isCaseArrivalResult({ ...valid, timed_ends: 1.5 })).toBe(false);
  });
});

describe("isCaseStatisticsResult", () => {
  const valid = {
    family: "CASE_STATISTICS", variant: "CLASSIC",
    case_id_key: "case_id", timestamp_key: "timestamp", enable_sort: true,
    sort_by: "CASE_ID", sort_ascending: true, max_cases: 0,
    use_business_hours: false,
    cases: [{ case_id: "c1", start_time: 1, end_time: 3, case_duration: 2 }],
  };
  it("accepts per-case timing rows", () => expect(isCaseStatisticsResult(valid)).toBe(true));
  it("rejects invalid durations and limits", () => {
    expect(isCaseStatisticsResult({ ...valid, max_cases: -1 })).toBe(false);
    expect(isCaseStatisticsResult({ ...valid, cases: [{ ...valid.cases[0], case_duration: -1 }] })).toBe(false);
  });
});

describe("isAttributeStatisticsResult", () => {
  const valid = {
    family: "ATTRIBUTE_STATISTICS", scope: "EVENTS",
    attribute_key: "activity", keep_once_per_case: true,
    values: { A: 2 },
  };
  it("accepts deterministic attribute counts", () => expect(isAttributeStatisticsResult(valid)).toBe(true));
  it("rejects negative or fractional counts", () => {
    expect(isAttributeStatisticsResult({ ...valid, values: { A: -1 } })).toBe(false);
    expect(isAttributeStatisticsResult({ ...valid, values: { A: 1.5 } })).toBe(false);
  });
});

describe("isAttributeDistributionResult", () => {
  const valid = {
    family: "ATTRIBUTE_DISTRIBUTION", variant: "DAYS_MONTH",
    timestamp_key: "timestamp", labels: ["01", "02"], counts: [2, 1],
    timed_events: 3,
  };
  it("accepts aligned calendar bins", () => expect(isAttributeDistributionResult(valid)).toBe(true));
  it("rejects misaligned or fractional bins", () => {
    expect(isAttributeDistributionResult({ ...valid, labels: ["01"] })).toBe(false);
    expect(isAttributeDistributionResult({ ...valid, counts: [2, 1.5] })).toBe(false);
  });
});

describe("isAttributeKdeResult", () => {
  const valid = {
    family: "ATTRIBUTE_KDE", variant: "NUMERIC", attribute_key: "amount",
    x: [1, 2, 3], y: [0.1, 0.2, 0.1],
  };
  it("accepts finite aligned density points", () => expect(isAttributeKdeResult(valid)).toBe(true));
  it("rejects invalid alignment, order, and density", () => {
    expect(isAttributeKdeResult({ ...valid, y: [0.1] })).toBe(false);
    expect(isAttributeKdeResult({ ...valid, x: [1, 3, 2] })).toBe(false);
    expect(isAttributeKdeResult({ ...valid, y: [0.1, -0.2, 0.1] })).toBe(false);
  });
});

describe("isBoundaryActivitiesResult", () => {
  const valid = {
    family: "BOUNDARY_ACTIVITIES", variant: "BOTH",
    starts: { A: 2 }, ends: { B: 1 },
  };
  it("accepts the boundary envelope", () => expect(isBoundaryActivitiesResult(valid)).toBe(true));
  it("rejects invalid counts and variants", () => {
    expect(isBoundaryActivitiesResult({ ...valid, variant: "OTHER" })).toBe(false);
    expect(isBoundaryActivitiesResult({ ...valid, starts: { A: -1 } })).toBe(false);
  });
});

describe("isVariantStatisticsResult", () => {
  const valid = {
    family: "VARIANT_STATISTICS", variant: "CLASSIC", activity_key: "activity",
    variants: [{ activities: ["A"], count: 1, probability: 1 }],
  };
  it("accepts variant counts", () => expect(isVariantStatisticsResult(valid)).toBe(true));
  it("rejects invalid probabilities", () => {
    expect(isVariantStatisticsResult({ ...valid, variants: [{ ...valid.variants[0], probability: 2 }] })).toBe(false);
    expect(isVariantStatisticsResult({ ...valid, variants: [{ ...valid.variants[0], count: 1.5 }] })).toBe(false);
  });
});

describe("isMonteCarloResult", () => {
  const valid = {
    family: "MONTE_CARLO", variant: "PETRI_SEMAPH_FIFO",
    log: { trace_count: 1, event_count: 2 },
    diagnostics: { simulated_cases: 1, completed_cases: 1, deadlocked_cases: 0, step_limited_cases: 0, case_arrival_seconds: 1, total_cases_seconds: 2, median_case_execution_seconds: 2, case_execution_seconds: [2] },
  };
  it("accepts bounded diagnostics", () => expect(isMonteCarloResult(valid)).toBe(true));
  it("rejects negative and malformed counts", () => {
    expect(isMonteCarloResult({ ...valid, log: { trace_count: -1, event_count: 2 } })).toBe(false);
    expect(isMonteCarloResult({ ...valid, diagnostics: { ...valid.diagnostics, case_execution_seconds: ["bad"] } })).toBe(false);
  });
});

describe("isILPMinerResult", () => {
  const valid = { family: "ILP", net: { place_count: 2, source: 0, sink: 1, activities: ["A"], transitions: [{ activity: 0, inputs: [0], outputs: [1] }, { activity: null, inputs: [1], outputs: [0] }] }, markings: { initial: [1, 0], final: [0, 1] }, diagnostics: { computed: true, exhausted: false, visited_nodes: 1, lp_solved: 1, branch_and_bound_nodes: 1, message: "done" } };
  it("accepts backend activity indexes and silent transitions", () => expect(isILPMinerResult(valid)).toBe(true));
  it("rejects malformed markings and diagnostics", () => expect(isILPMinerResult({ ...valid, markings: { initial: [1], final: [0, 1] } })).toBe(false));
  it("rejects out-of-range net references", () => {
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, source: 2 } })).toBe(false);
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, transitions: [{ activity: "A", inputs: [2], outputs: [1] }] } })).toBe(false);
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, transitions: [{ activity: 1, inputs: [0], outputs: [1] }] } })).toBe(false);
    expect(isILPMinerResult({ ...valid, net: { ...valid.net, transitions: [{ activity: "A", inputs: [0], outputs: [1] }] } })).toBe(false);
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
  it("accepts dataframe-backed variant names", () => expect(isOrganizationalSnaResult({ ...valid, variant: "handover_pandas" })).toBe(true));
  it("rejects out-of-range edges", () => expect(isOrganizationalSnaResult({ ...valid, edges: [{ source: 2, target: 0, weight: 1 }] })).toBe(false));
  it("requires the directedness contract", () => expect(isOrganizationalSnaResult({ ...valid, directed: undefined })).toBe(false));
});
describe("isRoleDiscoveryResult", () => {
  it("accepts deterministic roles and rejects malformed counts", () => {
    const value = { family: "ROLE_DISCOVERY", variant: "pandas", roles: [{ activities: ["A"], resources: [{ name: "r", count: 2 }] }], diagnostics: { computed: true, output_truncated: false, trace_count: 1, event_count: 1, activity_count: 1, resource_count: 1, role_count: 1, message: "ok" } };
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
  const valid = { family: "NETWORK_ANALYSIS", variant: "dataframe", edges: [{ source: "alice", target: "bob", activity: "A", count: 1, performance: [2] }], diagnostics: { computed: true, output_truncated: false, trace_count: 1, event_count: 2, link_count: 1, row_count: 1, performance_value_count: 1, message: "ok" } };
  it("accepts deterministic edge rows", () => expect(isNetworkAnalysisResult(valid)).toBe(true));
  it("rejects an unknown adapter variant", () => expect(isNetworkAnalysisResult({ ...valid, variant: "log" })).toBe(false));
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

describe("isProfileClusteringResult", () => {
  const valid = {
    family: "PROFILE_CLUSTERING", variant: "sklearn_profiles", labels: [0, 1], clusters: [[0], [1]],
    feature_names: ["event:concept:name@A"], inertia: 0,
    diagnostics: { computed: true, converged: true, trace_count: 2,
      feature_count: 1, cluster_count: 2, iterations: 1, message: "converged" },
  };
  it("accepts deterministic labels and diagnostics", () => expect(isProfileClusteringResult(valid)).toBe(true));
  it("rejects negative or non-finite metrics", () => {
    expect(isProfileClusteringResult({ ...valid, inertia: -1 })).toBe(false);
    expect(isProfileClusteringResult({ ...valid, variant: "other" })).toBe(false);
    expect(isProfileClusteringResult({ ...valid, diagnostics: { ...valid.diagnostics, iterations: -1 } })).toBe(false);
  });
});

describe("isLogToFeaturesResult", () => {
  it("guards variant-specific rows", () => {
    expect(isLogToFeaturesResult({ family: "LOG_TO_FEATURES", variant: "trace_based", feature_names: ["wip"], trace_rows: [[1]] })).toBe(true);
    expect(isLogToFeaturesResult({ family: "LOG_TO_FEATURES", variant: "temporal", feature_names: ["arrivals"], temporal_rows: [{ timestamp: 1, values: [2] }] })).toBe(true);
    expect(isLogToFeaturesResult({ family: "LOG_TO_FEATURES", variant: "event_based", feature_names: [], event_rows: [[[{ value: 1 }]]] })).toBe(false);
  });
});

describe("embedding result guards", () => {
  it("accepts fixed-length vectors and similarity rows", () => {
    expect(isEmbeddingResult({ family: "TO_EMBEDDINGS", variant: "cases_transformers", embedding_provider: "hashing", embedding_model: "hashing-v1", attribute_key: "concept:name", dimensions: 2, items: [{ id: "c1", embedding: [0, 1] }] })).toBe(true);
    expect(isEmbeddingSimilarityResult({ family: "TO_EMBEDDINGS_SIMILARITY", variant: "events_transformers", embedding_provider: "transformer", target: "A", embedding_model: "hashing-v2", dimensions: 32, top_k: 1, items: [{ id: "0", score: 0.5 }] })).toBe(true);
    expect(isEmbeddingResult({ family: "TO_EMBEDDINGS", variant: "cases_transformers", embedding_model: "hashing-v1", attribute_key: "concept:name", dimensions: 2, items: [{ id: "c1", embedding: [0, 1] }] })).toBe(true);
  });
  it("rejects ragged vectors and non-finite scores", () => {
    expect(isEmbeddingResult({ family: "TO_EMBEDDINGS", variant: "cases_transformers", embedding_model: "hashing-v1", attribute_key: "concept:name", dimensions: 2, items: [{ id: "c1", embedding: [0] }] })).toBe(false);
    expect(isEmbeddingResult({ family: "TO_EMBEDDINGS", variant: "cases_transformers", embedding_provider: "unknown", embedding_model: "hashing-v1", attribute_key: "concept:name", dimensions: 2, items: [{ id: "c1", embedding: [0, 1] }] })).toBe(false);
    expect(isEmbeddingSimilarityResult({ family: "TO_EMBEDDINGS_SIMILARITY", variant: "events_transformers", target: "A", top_k: 1, items: [{ id: "0", score: Number.NaN }] })).toBe(false);
    expect(isEmbeddingSimilarityResult({ family: "TO_EMBEDDINGS_SIMILARITY", variant: "events_transformers", target: "A", dimensions: 0, top_k: 1, items: [] })).toBe(false);
  });
});

describe("isTraceVariantQueryResult", () => {
  const event = { activity: "A", timestamp: "1970-01-01T00:00:00Z", resource: "", lifecycle: "", input_order: 0 };
  const valid = {
    family: "TRACE_VARIANT_QUERY", variant: "sacofa", epsilon: 1, k: 1, p: 1, seed: 2,
    frequencies: [{ activities: ["A"], count: 1 }],
    diagnostics: { candidate_count: 1, selected_violation_count: 0, retained_variant_count: 1, released_trace_count: 1, output_truncated: false, message: "ok" },
    log: { traces: [{ case_id: "tvq-1", events: [event] }] },
  };
  it("accepts a released trace distribution", () => expect(isTraceVariantQueryResult(valid)).toBe(true));
  it("rejects invalid privacy and event contracts", () => {
    expect(isTraceVariantQueryResult({ ...valid, epsilon: 0 })).toBe(false);
    expect(isTraceVariantQueryResult({ ...valid, frequencies: [{ activities: ["A"], count: 0 }] })).toBe(false);
    expect(isTraceVariantQueryResult({ ...valid, log: { traces: [{ case_id: "x", events: [{ activity: "A" }] }] } })).toBe(false);
    expect(isTraceVariantQueryResult({ ...valid, diagnostics: { ...valid.diagnostics, selected_violation_count: -1 } })).toBe(false);
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

  it("rejects null or primitive overlap payloads", () => {
    expect(isMineraExport({ ...valid, overlap: null })).toBe(false);
    expect(isMineraExport({ ...valid, overlap: 4 })).toBe(false);
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

  it("accepts custom-selector batch groups and rejects malformed ids", () => {
    const batch = {
      activity: null,
      activity_label: "phase-A",
      resource: "team-1",
      batch_count: 1,
      event_count: 2,
      batches: [{
        type: "simultaneous", start: 0, complete: 1,
        events: [],
      }],
    };
    expect(isMineraExport({ ...valid,
      batch_discovery: {
        activity_attribute: "phase", groups: [batch],
      },
    })).toBe(true);
    expect(isMineraExport({ ...valid,
      batch_discovery: { groups: [{ ...batch, activity: -1 }] },
    })).toBe(false);
  });

  it("validates the minimum self-distance adapter selector", () => {
    expect(isMineraExport({
      ...valid,
      minimum_self_distance_variant: "polars",
      minimum_self_distances: [],
    })).toBe(true);
    expect(isMineraExport({
      ...valid,
      minimum_self_distance_variant: "other",
    })).toBe(false);
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

  it("validates recursive POWL nodes and activity references", () => {
    const powl = {
      kind: "strict_partial_order",
      children: [
        { kind: "transition", activity: 0, frequency: { minimum: 1, maximum: 1 } },
        { kind: "loop", children: [{ kind: "xor", children: [{ kind: "silent" }, { kind: "transition", activity: 0, frequency: { minimum: 0, maximum: null } }] }, { kind: "silent" }] },
      ],
      order: [{ source: 0, target: 1 }],
    };
    expect(isPowlNode(powl, 1)).toBe(true);
    expect(isMineraExport({ ...valid, activities: [{}], powl })).toBe(true);
  });

  it("rejects malformed recursive POWL payloads before rendering", () => {
    const base = { ...valid, activities: [{}] };
    expect(isMineraExport({ ...base, powl: { kind: "transition", activity: 1, frequency: { minimum: 1, maximum: 1 } } })).toBe(false);
    expect(isMineraExport({ ...base, powl: { kind: "transition", activity: 0, frequency: { minimum: 2, maximum: 1 } } })).toBe(false);
    expect(isMineraExport({ ...base, powl: { kind: "strict_partial_order", children: [{ kind: "silent" }], order: [{ source: 0, target: 1 }] } })).toBe(false);
    expect(isMineraExport({ ...base, powl: { kind: "xor", children: [{ kind: "unknown" }] } })).toBe(false);
  });

  it("matches native POWL node cardinality and empty-order invariants", () => {
    expect(isPowlNode({ kind: "strict_partial_order", children: [], order: [] })).toBe(true);
    expect(isPowlNode({ kind: "strict_partial_order", children: [], order: [{ source: 0, target: 0 }] })).toBe(false);
    expect(isPowlNode({ kind: "strict_partial_order", children: [{ kind: "silent" }, { kind: "silent" }], order: [{ source: 0, target: 1 }, { source: 1, target: 0 }] })).toBe(false);
    expect(isPowlNode({ kind: "xor", children: [{ kind: "silent" }] })).toBe(false);
    expect(isPowlNode({ kind: "loop", children: [{ kind: "silent" }] })).toBe(false);
    expect(isPowlNode({ kind: "loop", children: [{ kind: "silent" }, { kind: "silent" }, { kind: "silent" }] })).toBe(false);
  });

  it("enforces native transition frequencies and recursion depth", () => {
    const transition = (frequency: { minimum: number; maximum: number | null }) => ({ kind: "transition", activity: 0, frequency });
    expect(isPowlNode(transition({ minimum: 0, maximum: 1 }))).toBe(true);
    expect(isPowlNode(transition({ minimum: 1, maximum: null }))).toBe(true);
    expect(isPowlNode(transition({ minimum: 2, maximum: 1 }))).toBe(false);
    expect(isPowlNode(transition({ minimum: 2, maximum: null }))).toBe(false);
    expect(isPowlNode(transition({ minimum: 0, maximum: 2 }))).toBe(false);
    let deep: unknown = { kind: "silent" };
    for (let i = 0; i < 512; i++) deep = { kind: "loop", children: [deep, { kind: "silent" }] };
    expect(isPowlNode(deep)).toBe(true);
    deep = { kind: "loop", children: [deep, { kind: "silent" }] };
    expect(isPowlNode(deep)).toBe(false);
  });

  it("guards timestamp interleavings results", () => {
    const result = {
      family: "TIMESTAMP_INTERLEAVINGS", variant: "TIMESTAMP", timestamp_key: "timestamp",
      max_interleavings: 4,
      interleavings: [{ source_activity: "A", target_activity: "B", source_timestamp: "1", target_timestamp: "2", left_index: 0, right_index: 1, direction: "LR", timestamp_diff: 1 }],
    };
    expect(isTimestampInterleavingsResult(result)).toBe(true);
    expect(isTimestampInterleavingsResult({ ...result, interleavings: [{ ...result.interleavings[0], timestamp_diff: NaN }] })).toBe(false);
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
    expect(isEmdResult({ variant: "pyemd", distance: 0.125, computed: true, normalized: false, error: null })).toBe(true);
    expect(isEmdResult({ variant: "pyemd", distance: 0.125, computed: true, normalized: false, use_fast_emd: false, error: null })).toBe(true);
    expect(isEmdResult({ variant: "pyemd", distance: 0.125, computed: true, normalized: false, use_fast_emd: "false", error: null })).toBe(false);
    expect(isEmdResult({ variant: "unsupported", distance: 0.125, computed: true, normalized: false, error: null })).toBe(false);
    expect(isEmdResult({ distance: null, computed: false, normalized: false, error: "unequal mass" })).toBe(true);
    expect(isEmdResult({ distance: NaN, computed: false, normalized: false, error: null })).toBe(false);
  });
});
