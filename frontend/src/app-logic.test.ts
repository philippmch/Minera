import { describe, expect, it } from "vitest";
import { alignmentQuery, dfgAlignmentQuery, editDistanceFormEntries, mineFormEntries, parseDfgCostMapInput, parseMarkingInput, processTreeAlignmentQuery, decomposedAlignmentQuery, petriAntiAlignmentQuery, petriMultiAlignmentQuery, processTreeReductionQuery, evaluationQuery, emdQuery, petriAnalysisQuery, type FilterOptions } from "./api";
import {
  delimiterForFile,
  evaluationModelIds,
  hasActiveFilters,
  hasActiveMinerOptions,
  initialTab,
  isJsonExport,
  logLifecycles,
  maxEdgeCount,
} from "./app-logic";
import { isDfgAlignmentResult, isEditDistanceResult, isProcessTreeAlignmentResult, isDecomposedAlignmentResult, isPetriAntiAlignmentResult, isPetriMultiAlignmentResult, isProcessTreeReductionResult, isPetriAnalysisResult, type EdgeStat, type MineraExport, type Trace } from "./types";

describe("native alignment adapters", () => {
  it("serializes advanced options deterministically", () => {
    expect(decomposedAlignmentQuery({ max_states: 10, final_marking: [2, 1], initial_marking: [0], tau_model_cost: 1, sync_cost: 0 })).toBe("final_marking=2%2C1&initial_marking=0&max_states=10&sync_cost=0&tau_model_cost=1&variant=recompos_maximal");
    expect(new URLSearchParams(decomposedAlignmentQuery({ activity_log_costs: { "A,B\\C": 2 } })).get("activity_log_costs")).toBe("A\\,B\\\\C=2");
    expect(new URLSearchParams(petriAntiAlignmentQuery({ exponent: 2, epsilon: 0.005 })).get("variant")).toBe("version_discounted_a_star");
    expect(new URLSearchParams(petriMultiAlignmentQuery({ marking_limit: 5 })).get("marking_limit")).toBe("5");
    expect(new URLSearchParams(processTreeReductionQuery({ activity_key: "concept:name" })).get("activity_key")).toBe("concept:name");
    expect(parseMarkingInput(" 0, 2,3 ")).toEqual([0, 2, 3]);
    expect(() => parseMarkingInput("0,-1")).toThrow(/non-negative/);
  });
  it("guards native result discriminators and diagnostics", () => {
    const diagnostics = { visited_states: 1, queued_states: 0, traversed_arcs: 1, exhausted: false };
    const anti = { algorithm: "petri_anti_alignment", variant: "version_discounted_a_star", activities: ["A"], transitions: [0], cost: -1, objective: -1, precision: 0.5, diagnostics };
    const multi = { algorithm: "petri_multi_alignment", variant: "version_discounted_a_star", activities: ["A"], transitions: [0], cost: 1, objective: 1, max_distance_to_log: 1, diagnostics };
    expect(isPetriAntiAlignmentResult(anti)).toBe(true);
    expect(isPetriMultiAlignmentResult(multi)).toBe(true);
    expect(isPetriAntiAlignmentResult({ ...anti, algorithm: "petri_multi_alignment" })).toBe(false);
    expect(isPetriMultiAlignmentResult({ ...multi, diagnostics: { ...diagnostics, visited_states: -1 } })).toBe(false);
    const decomposed = { algorithm: "decomposed_alignment", variant: "recompos_maximal", complete: true, timed_out: false, exhausted: false, fitness: 1, best_worst_cost: 0, trace_count: 1, total_cost: 0, diagnostics: { decompositions: 1, components: 1, merges: 0, cache_hits: 0, cache_misses: 1, border_disagreements: 0, agreement_iterations: 1, visited_states: 1, queued_states: 1, traversed_arcs: 1, whole_net_fallback: false, note: "" }, traces: [{ activities: ["A"], count: 1, complete: true, timed_out: false, exhausted: false, cost: 0, fitness: 1, best_worst_cost: 0, alignment: [{ log: "A", model: "A", transition: 0 }] }] };
    expect(isDecomposedAlignmentResult(decomposed)).toBe(true);
    expect(isDecomposedAlignmentResult({ ...decomposed, traces: [{ ...decomposed.traces[0], alignment: [{ log: 1 }] }] })).toBe(false);
    const reduced = { algorithm: "process_tree_reduction", variant: "tree_tr_based", trace_index: 0, process_tree: { operator: "activity", activity: 0 } };
    expect(isProcessTreeReductionResult(reduced)).toBe(true);
  });
});

describe("evaluation adapters", () => {
  it("unions evaluator model ids from nets, existing results, and uploads", () => {
    const data = {
      dataset: "models", trace_count: 0, event_count: 0, activities: [], edges: [], variants: [],
      petri_net: { place_count: 1, source: 0, sink: 0, transitions: [] },
      powl_petri_net: { place_count: 1, source: 0, sink: 0, transitions: [] },
      conformance: { heuristic: {} },
      evaluation: { archived: {} },
    } as unknown as MineraExport;
    expect(evaluationModelIds(data, true)).toEqual(["alpha", "archived", "heuristic", "imported", "powl"]);
  });

  it("serializes every evaluation family setting with stable wire names", () => {
    expect(evaluationQuery({
      evaluation_family: "simplicity",
      fitness_variant: "alignment_based",
      precision_variant: "automaton_after_align",
      precision_use_task_ids: true,
      precision_max_states: 200,
      generalization_variant: "generalization_token",
      simplicity_variant: "extended_cyclomatic",
      simplicity_arc_degree_k: 2.5,
      simplicity_max_states: 300,
      simplicity_initial_marking: [1, 0],
      simplicity_final_marking: [0, 1],
    })).toBe("evaluation_family=simplicity&fitness_variant=alignment_based&precision_variant=automaton_after_align&precision_use_task_ids=true&precision_max_states=200&generalization_variant=generalization_token&simplicity_variant=extended_cyclomatic&simplicity_arc_degree_k=2.5&simplicity_max_states=300&simplicity_initial_marking=1%2C0&simplicity_final_marking=0%2C1");
    expect(evaluationQuery()).toBe("evaluation_family=replay_fitness");
  });

  it("serializes and validates every Petri-analysis setting", () => {
    const query = new URLSearchParams(petriAnalysisQuery({
      family: "extended_marking_equation", miner: "powl", trace_index: 2,
      initial_marking: [1, 0], final_marking: [0, 1], transition_costs: [3.5, 4],
      model_costs: [5, 6], sync_costs: [0, 1], log_costs: [9], split_indices: [1, 3],
      max_k: 2, max_states: 100, full_bootstrap: false, diagnostics: true,
    }));
    expect(query.get("petri_analysis_family")).toBe("extended_marking_equation");
    expect(query.get("petri_analysis_model_costs")).toBe("5,6");
    expect(query.get("petri_analysis_split_indices")).toBe("1,3");
    expect(query.get("petri_analysis_full_bootstrap")).toBe("false");
    expect(() => petriAnalysisQuery({ max_states: 0 })).toThrow(/valid/);
    expect(() => petriAnalysisQuery({ model_costs: [-1] })).toThrow(/costs/);
    expect(isPetriAnalysisResult({ family: "soundness", status: "sound" })).toBe(true);
    expect(isPetriAnalysisResult({ family: "unknown" })).toBe(false);
  });

  it("validates supplied settings even when another family is selected", () => {
    expect(() => evaluationQuery({ evaluation_family: "precision", simplicity_max_states: 0 })).toThrow(/positive integer/);
    expect(() => evaluationQuery({ evaluation_family: "generalization", simplicity_initial_marking: [] })).toThrow(/non-negative integers/);
    expect(() => evaluationQuery({ simplicity_arc_degree_k: Number.POSITIVE_INFINITY })).toThrow(/finite/);
  });

  it("serializes strict stochastic-language controls", () => {
    expect(emdQuery({ normalize_masses: false, strict_equal_mass: true })).toBe("normalize_masses=false&strict_equal_mass=true");
    expect(() => emdQuery({ normalize_masses: "yes" as unknown as boolean })).toThrow(/boolean/);
  });
});

describe("DFG and edit-distance contracts", () => {
  it("serializes DFG options and cost maps deterministically", () => {
    const p = new URLSearchParams(dfgAlignmentQuery({ variant: "CLASSIC", max_states: 10, projection: "metrics", sync_cost: { B: 2, A: 1 } }));
    expect(p.get("dfg_variant")).toBe("CLASSIC");
    expect(p.get("dfg_sync_cost")).toBe("A=1,B=2");
    expect(p.get("projection")).toBe("metrics");
    expect(new URLSearchParams(dfgAlignmentQuery({ max_states: 0, max_time_ms: -1 })).toString()).toBe("");
    const escaped = new URLSearchParams(dfgAlignmentQuery({ sync_cost: { "A,B": 1, "A=B": 2, "A:B": 3, "A\\B": 4 } }));
    expect(escaped.get("dfg_sync_cost")).toBe("A\\,B=1,A\\:B=3,A\\=B=2,A\\\\B=4");
    expect(parseDfgCostMapInput(escaped.get("dfg_sync_cost")!)).toEqual({ "A,B": 1, "A=B": 2, "A:B": 3, "A\\B": 4 });
    const whitespace = new URLSearchParams(dfgAlignmentQuery({ sync_cost: { " A": 5, "A ": 6, "A\tB": 7 } })).get("dfg_sync_cost")!;
    expect(whitespace).toBe("\\sA=5,A\\tB=7,A\\s=6");
    expect(parseDfgCostMapInput(whitespace)).toEqual({ " A": 5, "A ": 6, "A\tB": 7 });
    expect(() => parseDfgCostMapInput("A=1,broken")).toThrow(/activity=value/);
    expect(() => parseDfgCostMapInput("A\\q=1")).toThrow(/invalid escape/);
  });
  it("prunes absent edit options", () => {
    expect(editDistanceFormEntries({ variant: "EDIT_DISTANCE", anti: false, projection: "none" })).toEqual([["edit_variant", "EDIT_DISTANCE"], ["anti", "false"], ["projection", "none"]]);
  });
  it("guards compatible alignment results and rejects wrong families", () => {
    const dfg = { family: "DFG", variant: "CLASSIC", unique_variants: 1, alignments: [{ cost: 0, internal_cost: 0, fitness: 1, best_worst_cost: 0, visited_states: 1, closed: 1, closed_states: 1, moves: [{ log: "A", model: "A" }] }] };
    expect(isDfgAlignmentResult(dfg)).toBe(true);
    expect(isDfgAlignmentResult({ ...dfg, family: "EDIT_DISTANCE" })).toBe(false);
    const edit = { family: "EDIT_DISTANCE", variant: "EDIT_DISTANCE", anti: false, source_count: 1, alignments: [{ source_variant: ["A"], target_variant: ["A"], target_index: 0, cost: 0, fitness: 1, best_worst_cost: 0, bwc: 0, denominator: 0, moves: [{ type: "sync", log: "A", model: "A" }] }] };
    expect(isEditDistanceResult(edit)).toBe(true);
  });
});

describe("process-tree alignment contract", () => {
  it("omits native defaults and prunes variant-specific controls", () => {
    expect(processTreeAlignmentQuery({})).toBe("");
    const params = new URLSearchParams(processTreeAlignmentQuery({
      variant: "APPROXIMATED_ORIGINAL", max_states: 0, max_time_ms: 0,
      max_trace_length: 0, max_process_tree_height: 4, batch_size: 2,
    }));
    expect(params.get("pt_alignment_variant")).toBe("APPROXIMATED_ORIGINAL");
    expect(params.get("pt_alignment_max_process_tree_height")).toBe("4");
    expect(params.get("pt_alignment_max_trace_length")).toBe("0");
    expect(params.has("pt_alignment_batch_size")).toBe(false);
  });
  it("rejects invalid bounds", () => {
    expect(() => processTreeAlignmentQuery({ max_states: -1 })).toThrow(/non-negative/);
    expect(() => processTreeAlignmentQuery({ batch_size: 0 })).toThrow(/positive/);
    expect(() => processTreeAlignmentQuery({ max_time_ms: Number.NaN })).toThrow(/finite/);
  });
  it("guards the process-tree response shape", () => {
    const result = {
      family: "PROCESS_TREE", variant: "MILP", computed: true, completed: true,
      timed_out: false, exhausted: false, total_cost: 0, fitness: 1,
      diagnostics: { visited_states: 1, queued_states: 0, traversed_arcs: 1, lp_solved: 1, traces_aligned: 1, message: "" },
      traces: [{ trace: ["A"], cost: 0, fitness: 1, completed: true, timed_out: false, moves: [{ type: "sync", log: "A", model: "A", activity: "A" }] }],
    };
    expect(isProcessTreeAlignmentResult(result)).toBe(true);
    expect(isProcessTreeAlignmentResult({ ...result, family: "DFG" })).toBe(false);
    expect(isProcessTreeAlignmentResult({ ...result, diagnostics: { ...result.diagnostics, lp_solved: -1 } })).toBe(false);
  });
});
import {
  alignmentMoveLabel,
  cleanAlignmentOptions,
  isInvalidAlignmentOptions,
} from "./components/Conformance";

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

describe("alignmentQuery", () => {
  it("serializes every alignment control for the deferred server request", () => {
    const query = alignmentQuery({
      variant: "discounted_a_star",
      max_states: 42,
      max_align_time: 1.5,
      max_align_time_trace: 0.2,
      max_alignments: 7,
      exponent: 1.1,
      synchronous_dijkstra: true,
      cost_profile: "standard",
      trace_cost: 10000,
      model_cost: 10000,
      silent_model_cost: 1,
      sync_cost: 0,
    });
    const params = new URLSearchParams(query);
    expect(params.get("alignment_variant")).toBe("discounted_a_star");
    expect(params.get("variant")).toBeNull();
    expect(params.get("max_states")).toBe("42");
    expect(params.get("max_align_time_trace")).toBe("0.2");
    expect(params.get("synchronous_dijkstra")).toBe("true");
    expect(params.get("cost_profile")).toBe("standard");
    expect(params.get("silent_model_cost")).toBe("1");
  });

  it("omits unset controls so the legacy request remains compatible", () => {
    expect(alignmentQuery({})).toBe("");
  });

  it("drops controls that do not apply after switching variants", () => {
    const generator = cleanAlignmentOptions({
      variant: "generator_dijkstra_less_memory",
      cost_profile: "legacy",
      max_alignments: 3,
      exponent: 2,
      synchronous_dijkstra: true,
    });
    expect(generator.max_alignments).toBe(3);
    expect(generator.exponent).toBeUndefined();
    expect(generator.synchronous_dijkstra).toBeUndefined();
    const discounted = cleanAlignmentOptions({
      variant: "discounted_a_star",
      cost_profile: "standard",
      max_alignments: 3,
      exponent: 1.5,
      synchronous_dijkstra: true,
    });
    expect(discounted.max_alignments).toBeUndefined();
    expect(discounted.exponent).toBe(1.5);
    expect(discounted.synchronous_dijkstra).toBe(true);
  });

  it("ignores stale invalid variant-specific values after switching away", () => {
    const generatorDraft = {
      variant: "generator_dijkstra_less_memory" as const,
      max_alignments: 0,
    };
    expect(isInvalidAlignmentOptions(generatorDraft)).toBe(true);
    expect(
      isInvalidAlignmentOptions({ ...generatorDraft, variant: "state_equation_a_star" }),
    ).toBe(false);

    const discountedDraft = {
      variant: "discounted_a_star" as const,
      exponent: 1,
    };
    expect(isInvalidAlignmentOptions(discountedDraft)).toBe(true);
    expect(
      isInvalidAlignmentOptions({ ...discountedDraft, variant: "state_equation_a_star" }),
    ).toBe(false);
  });

  it("omits the synchronous default but preserves an explicit false override", () => {
    const defaults = cleanAlignmentOptions({
      variant: "discounted_a_star",
      cost_profile: "legacy",
    });
    expect(defaults.synchronous_dijkstra).toBeUndefined();
    expect(new URLSearchParams(alignmentQuery(defaults)).has("synchronous_dijkstra")).toBe(false);

    const disabled = cleanAlignmentOptions({
      variant: "discounted_a_star",
      cost_profile: "legacy",
      synchronous_dijkstra: false,
    });
    expect(disabled.synchronous_dijkstra).toBe(false);
    expect(new URLSearchParams(alignmentQuery(disabled)).get("synchronous_dijkstra")).toBe("false");
  });

  it("labels silent tau moves without indexing the activity catalog", () => {
    const activities = [{ name: "A", count: 1, start_count: 1, end_count: 1 }];
    expect(alignmentMoveLabel({ type: "sync", activity: 0 }, activities)).toBe("A");
    expect(alignmentMoveLabel({ type: "model", activity: null, silent: true }, activities)).toBe("τ");
    expect(alignmentMoveLabel({ type: "model", activity: null }, activities)).toBe("τ");
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
    expect(hasActiveMinerOptions({
      heuristicVariant: "classic",
      heuristicDecoration: "frequency",
      heuristicPerformanceAggregation: "mean",
      heuristicDfgPreCleaningNoiseThreshold: 0.05,
      heuristicLoopLengthTwoThreshold: 0.5,
      heuristicStartTimestampAttribute: "start_timestamp",
      heuristicLifecycleInstanceAttribute: "concept:instance",
    })).toBe(false);
    expect(hasActiveMinerOptions({ dfgStatisticalVariants: [], dfgCausalVariants: [] })).toBe(false);
    expect(hasActiveMinerOptions({ dfgStatisticalVariants: ["unknown"], dfgCausalVariants: ["unknown"] })).toBe(false);
    expect(hasActiveMinerOptions({ correlationVariants: [] })).toBe(false);
    expect(hasActiveMinerOptions({ correlationVariants: ["unknown"] })).toBe(false);
    expect(hasActiveMinerOptions({ transitionView: "sequence", transitionDirection: "forward", transitionWindow: 2 })).toBe(false);
    expect(hasActiveMinerOptions({ powlVariant: "maximal", powlFilteringType: "decreasing_factor", powlFilteringThreshold: 0, powlOrderFrequencyRatio: 1, powlBruteForceActivityLimit: 0 })).toBe(false);
    expect(hasActiveMinerOptions({ tokenReplayVariant: "forward", tokenReplayStopImmediatelyUnfit: false })).toBe(false);
    expect(hasActiveMinerOptions({ logSkeletonNoiseThreshold: 0, logSkeletonConstraints: ["equivalence", "always_after", "always_before", "never_together", "directly_follows", "activ_freq"] })).toBe(false);
    expect(hasActiveMinerOptions({ declareAutoSelectionMultiplier: 0.8 })).toBe(false);
    expect(hasActiveMinerOptions({
      footprintsDiscoveryVariants: ["entire_event_log", "dfg", "process_tree", "powl"],
      footprintsModel: "process_tree",
      footprintsConformanceVariants: ["log_model"],
      footprintsEnableActAlwaysExecuted: true,
    })).toBe(false);
  });

  it("is true when any setting deviates", () => {
    expect(hasActiveMinerOptions({ noiseThreshold: 0.2 })).toBe(true);
    expect(hasActiveMinerOptions({ inductiveVariant: "dfg" })).toBe(true);
    expect(hasActiveMinerOptions({ alphaVariant: "classic" })).toBe(true);
    expect(hasActiveMinerOptions({ heuristicVariant: "plusplus" })).toBe(true);
    // Zero is a meaningful heuristic threshold, not "off".
    expect(hasActiveMinerOptions({ dependencyThreshold: 0 })).toBe(true);
    expect(hasActiveMinerOptions({ andThreshold: 0 })).toBe(true);
    expect(hasActiveMinerOptions({ minEdgeFrequency: 3 })).toBe(true);
    expect(hasActiveMinerOptions({ heuristicMinActivityCount: 3 })).toBe(true);
    expect(hasActiveMinerOptions({ heuristicDecoration: "performance" })).toBe(true);
    expect(hasActiveMinerOptions({
      heuristicPerformanceAggregation: "all",
    })).toBe(true);
    expect(hasActiveMinerOptions({
      heuristicDfgPreCleaningNoiseThreshold: 0,
    })).toBe(true);
    expect(hasActiveMinerOptions({
      heuristicLoopLengthTwoThreshold: 0.75,
    })).toBe(true);
    expect(hasActiveMinerOptions({ heuristicKeepOncePerCase: true })).toBe(true);
    expect(hasActiveMinerOptions({
      heuristicMostCommonPaths: "A -> B",
    })).toBe(true);
    expect(hasActiveMinerOptions({
      heuristicStartTimestampAttribute: "started_at",
    })).toBe(true);
    expect(hasActiveMinerOptions({
      heuristicLifecycleInstanceAttribute: "instance",
    })).toBe(true);
    expect(hasActiveMinerOptions({ transitionView: "set" })).toBe(true);
    expect(hasActiveMinerOptions({ transitionDirection: "backward" })).toBe(true);
    expect(hasActiveMinerOptions({ transitionWindow: 0 })).toBe(true);
    expect(hasActiveMinerOptions({ transitionIncludeData: true })).toBe(true);
    expect(hasActiveMinerOptions({ powlVariant: "tree" })).toBe(true);
    expect(hasActiveMinerOptions({ powlFilteringType: "dynamic" })).toBe(true);
    expect(hasActiveMinerOptions({ powlFilteringThreshold: 0.2 })).toBe(true);
    expect(hasActiveMinerOptions({ powlOrderFrequencyRatio: 0.8 })).toBe(true);
    expect(hasActiveMinerOptions({ powlBruteForceActivityLimit: 3 })).toBe(true);
    expect(hasActiveMinerOptions({ tokenReplayVariant: "backwards" })).toBe(true);
    expect(hasActiveMinerOptions({ tokenReplayStopImmediatelyUnfit: true })).toBe(true);
    expect(hasActiveMinerOptions({ logSkeletonNoiseThreshold: 0.2 })).toBe(true);
    expect(hasActiveMinerOptions({ logSkeletonConstraints: ["equivalence"] })).toBe(true);
    expect(hasActiveMinerOptions({ declareConsideredActivities: ["A"] })).toBe(true);
    expect(hasActiveMinerOptions({ declareAllowedTemplates: ["response"] })).toBe(true);
    expect(hasActiveMinerOptions({ declareMinSupportRatio: 0 })).toBe(true);
    expect(hasActiveMinerOptions({ footprintsDiscoveryVariants: ["dfg"] })).toBe(true);
    expect(hasActiveMinerOptions({ footprintsConformanceVariants: ["trace_extensive"] })).toBe(true);
    expect(hasActiveMinerOptions({ footprintsStrict: true })).toBe(true);
    expect(hasActiveMinerOptions({ footprintsEnableActAlwaysExecuted: false })).toBe(true);
    expect(hasActiveMinerOptions({ dfgStatisticalVariants: ["frequency_triples"] })).toBe(true);
    expect(hasActiveMinerOptions({ dfgCausalVariants: ["heuristic"] })).toBe(true);
    expect(hasActiveMinerOptions({ correlationVariants: ["trace_based"] })).toBe(true);
    expect(hasActiveMinerOptions({ spectrumActivities: ["A", "B"] })).toBe(true);
    expect(hasActiveMinerOptions({ spectrumActivities: ["A"] })).toBe(false);
    expect(hasActiveMinerOptions({ spectrumVariant: "disconnected" })).toBe(false);
    expect(hasActiveMinerOptions({ batches: true })).toBe(true);
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
      heuristicVariant: "plusplus",
      dependencyThreshold: 0.75,
      andThreshold: 0,
      minEdgeFrequency: 2,
      heuristicMinActivityCount: 3,
      heuristicDecoration: "performance",
      heuristicPerformanceAggregation: "all",
      heuristicDfgPreCleaningNoiseThreshold: 0.1,
      heuristicLoopLengthTwoThreshold: 0.6,
      heuristicKeepOncePerCase: true,
      heuristicMostCommonPaths: "A -> B, C -> D",
      heuristicStartTimestampAttribute: "started_at",
      heuristicLifecycleInstanceAttribute: "instance",
    });
    expect(entries).toEqual([
      ["noise_threshold", "0.2"],
      ["inductive_variant", "dfg"],
      ["alpha_variant", "classic"],
      ["heuristic_variant", "plusplus"],
      ["dependency_threshold", "0.75"],
      ["and_threshold", "0"],
      ["min_edge_frequency", "2"],
      ["heuristic_min_activity_count", "3"],
      ["heuristic_decoration", "performance"],
      ["heuristic_performance_aggregation", "all"],
      ["heuristic_dfg_pre_cleaning_noise_threshold", "0.1"],
      ["heuristic_loop_length_two_threshold", "0.6"],
      ["heuristic_keep_once_per_case", "true"],
      ["heuristic_most_common_paths", "A -> B, C -> D"],
      ["heuristic_start_timestamp_attribute", "started_at"],
      ["heuristic_lifecycle_instance_attribute", "instance"],
    ]);
  });

  it("maps temporal analyses and gates dependent defaults", () => {
    expect(mineFormEntries({ spectrumVariant: "disconnected", spectrumSampleSize: 3 })).toEqual([]);
    expect(mineFormEntries({
      spectrumActivities: [" A ", "B"], spectrumVariant: "disconnected",
      spectrumSampleSize: 3, spectrumSortLog: false,
      batches: true, batchMergeDistanceSeconds: 12, batchMinSize: 4,
      batchStartTimestampAttribute: "started_at",
    })).toEqual([
      ["spectrum_activities", "A,B"], ["spectrum_variant", "disconnected"],
      ["spectrum_sample_size", "3"], ["spectrum_sort_log", "false"],
      ["batches", "true"], ["batch_merge_distance_seconds", "12"],
      ["batch_min_size", "4"], ["batch_start_timestamp_attribute", "started_at"],
    ]);
    expect(mineFormEntries({ spectrumActivities: ["A"], spectrumSampleSize: 10000, spectrumSortLog: true, batches: true, batchMergeDistanceSeconds: 900, batchMinSize: 2 })).toEqual([["batches", "true"]]);
  });

  it("maps non-default transition-system settings", () => {
    expect(mineFormEntries({
      transitionView: "multiset",
      transitionDirection: "backward",
      transitionWindow: 0,
      transitionIncludeData: true,
    })).toEqual([
      ["transition_view", "multiset"],
      ["transition_direction", "backward"],
      ["transition_window", "0"],
      ["transition_include_data", "true"],
    ]);
    expect(mineFormEntries({ transitionView: "sequence", transitionDirection: "forward", transitionWindow: 2 })).toEqual([]);
  });

  it("maps non-default POWL settings and preserves meaningful zero values", () => {
    expect(mineFormEntries({
      powlVariant: "dynamic_clustering_plain",
      powlFilteringType: "dynamic",
      powlFilteringThreshold: 0.2,
      powlOrderFrequencyRatio: 0.8,
      powlBruteForceActivityLimit: 4,
    })).toEqual([
      ["powl_variant", "dynamic_clustering_plain"],
      ["powl_filtering_type", "dynamic"],
      ["powl_filtering_threshold", "0.2"],
      ["powl_order_frequency_ratio", "0.8"],
      ["powl_brute_force_activity_limit", "4"],
    ]);
    expect(mineFormEntries({
      powlVariant: "maximal",
      powlFilteringType: "decreasing_factor",
      powlFilteringThreshold: 0,
      powlOrderFrequencyRatio: 1,
      powlBruteForceActivityLimit: 0,
    })).toEqual([]);
  });

  it("maps non-default token-replay settings", () => {
    expect(mineFormEntries({
      tokenReplayVariant: "backwards",
      tokenReplayStopImmediatelyUnfit: true,
    })).toEqual([
      ["token_replay_variant", "backwards"],
      ["token_replay_stop_immediately_unfit", "true"],
    ]);
    expect(mineFormEntries({
      tokenReplayVariant: "forward",
      tokenReplayStopImmediatelyUnfit: false,
    })).toEqual([]);
  });

  it("maps non-default log-skeleton settings and leaves defaults implicit", () => {
    expect(mineFormEntries({
      logSkeletonNoiseThreshold: 0.25,
      logSkeletonConstraints: ["equivalence", "activ_freq"],
    })).toEqual([
      ["log_skeleton_noise_threshold", "0.25"],
      ["log_skeleton_constraints", "equivalence,activ_freq"],
    ]);
    expect(mineFormEntries({
      logSkeletonNoiseThreshold: 0,
      logSkeletonConstraints: ["equivalence", "always_after", "always_before", "never_together", "directly_follows", "activ_freq"],
    })).toEqual([]);
    // Empty or unknown-only selections cannot accidentally override defaults.
    expect(mineFormEntries({ logSkeletonConstraints: [] })).toEqual([]);
    expect(mineFormEntries({ logSkeletonNoiseThreshold: 1 })).toEqual([
      ["log_skeleton_noise_threshold", "1"],
    ]);
    expect(mineFormEntries({ logSkeletonNoiseThreshold: -0.1 })).toEqual([]);
    expect(mineFormEntries({ logSkeletonNoiseThreshold: 1.1 })).toEqual([]);
  });

  it("maps Declare settings in deterministic order and guards ratios/templates", () => {
    expect(mineFormEntries({
      declareConsideredActivities: ["B", "A"],
      declareAllowedTemplates: ["response", "existence", "not-a-template"],
      declareMinSupportRatio: 0.2,
      declareMinConfidenceRatio: 0.7,
      declareAutoSelectionMultiplier: 0.6,
    })).toEqual([
      ["declare_considered_activities", "B,A"],
      ["declare_allowed_templates", "response,existence"],
      ["declare_min_support_ratio", "0.2"],
      ["declare_min_confidence_ratio", "0.7"],
    ]);
    expect(mineFormEntries({
      declareAllowedTemplates: ["unknown"],
      declareMinSupportRatio: -0.1,
      declareMinConfidenceRatio: 1.1,
      declareAutoSelectionMultiplier: Number.NaN,
    })).toEqual([]);
    expect(mineFormEntries({ declareAutoSelectionMultiplier: 0.8 })).toEqual([]);
    expect(mineFormEntries({
      declareMinSupportRatio: 0.2,
      declareAutoSelectionMultiplier: 0.6,
    })).toEqual([["declare_min_support_ratio", "0.2"]]);
    expect(mineFormEntries({
      declareMinConfidenceRatio: 0.7,
      declareAutoSelectionMultiplier: 0.6,
    })).toEqual([["declare_min_confidence_ratio", "0.7"]]);
  });

  it("maps aggregate footprints variants in canonical order and omits defaults", () => {
    expect(mineFormEntries({
      footprintsDiscoveryVariants: ["powl", "trace_by_trace", "bogus"],
      footprintsModel: "dfg",
      footprintsConformanceVariants: ["trace_extensive", "log_model", "unknown"],
      footprintsStrict: true,
      footprintsEnableActAlwaysExecuted: false,
    })).toEqual([
      ["footprints_discovery_variants", "trace_by_trace,powl"],
      ["footprints_model", "dfg"],
      ["footprints_conformance_variants", "log_model,trace_extensive"],
      ["footprints_strict", "true"],
      ["footprints_enable_act_always_executed", "false"],
    ]);
    expect(mineFormEntries({
      footprintsDiscoveryVariants: ["entire_event_log", "dfg", "process_tree", "powl"],
      footprintsModel: "process_tree",
      footprintsConformanceVariants: ["log_model"],
      footprintsEnableActAlwaysExecuted: true,
    })).toEqual([]);
    expect(mineFormEntries({
      footprintsDiscoveryVariants: ["petri_reach_graph", "dfg"],
      footprintsModel: "dfg",
    })).toEqual([
      ["footprints_discovery_variants", "dfg"],
      ["footprints_model", "dfg"],
    ]);
  });

  it("maps optional statistical and causal DFG analyses in canonical order", () => {
    expect(mineFormEntries({
      dfgStatisticalVariants: [
        "clean_time", "frequency", "unknown", "performance", "case_attributes",
      ],
      dfgCausalVariants: ["heuristic", "bogus", "alpha"],
      dfgWindow: 0,
      dfgKeepOncePerCase: true,
      dfgPerformanceAggregation: "all",
      dfgStartTimestampAttribute: "time:start",
      dfgBusinessHours: true,
      dfgBusinessHourSlots: "25200-61200,111600-147600",
      dfgCaseAttributes: ["concept:name", "region"],
      dfgReturnNodeAttributes: true,
    })).toEqual([
      ["dfg_statistical_variants", "frequency,performance,case_attributes,clean_time"],
      ["dfg_causal_variants", "alpha,heuristic"],
      ["dfg_window", "0"],
      ["dfg_keep_once_per_case", "true"],
      ["dfg_performance_aggregation", "all"],
      ["dfg_start_timestamp_attribute", "time:start"],
      ["dfg_business_hours", "true"],
      ["dfg_business_hour_slots", "25200-61200,111600-147600"],
      ["dfg_case_attributes", "concept:name,region"],
      ["dfg_return_node_attributes", "true"],
    ]);
    expect(mineFormEntries({
      dfgStatisticalVariants: ["unknown"], dfgCausalVariants: ["unknown"],
      dfgWindow: 0, dfgBusinessHours: true,
    })).toEqual([]);
    expect(mineFormEntries({
      dfgCausalVariants: ["alpha"], dfgWindow: 0,
      dfgKeepOncePerCase: true, dfgPerformanceAggregation: "all",
    })).toEqual([
      ["dfg_causal_variants", "alpha"],
      ["dfg_window", "0"],
      ["dfg_keep_once_per_case", "true"],
    ]);
    expect(mineFormEntries({
      dfgStatisticalVariants: ["native"], dfgWindow: 1,
      dfgPerformanceAggregation: "mean", dfgStartTimestampAttribute: "start_timestamp",
      dfgCaseAttributes: ["concept:name"], dfgBusinessHourSlots: "0-604801",
    })).toEqual([["dfg_statistical_variants", "native"]]);
    expect(mineFormEntries({
      dfgStatisticalVariants: ["clean_time"], dfgWindow: 0,
      dfgKeepOncePerCase: true, dfgPerformanceAggregation: "all",
      dfgStartTimestampAttribute: "started", dfgBusinessHours: true,
      dfgBusinessHourSlots: "0-3600", dfgCaseAttributes: ["region"],
      dfgReturnNodeAttributes: true,
    })).toEqual([["dfg_statistical_variants", "clean_time"]]);
    expect(mineFormEntries({
      dfgStatisticalVariants: ["case_attributes"],
      dfgCaseAttributes: ["region"], dfgReturnNodeAttributes: true,
      dfgWindow: 0, dfgPerformanceAggregation: "all",
    })).toEqual([
      ["dfg_statistical_variants", "case_attributes"],
      ["dfg_case_attributes", "region"],
      ["dfg_return_node_attributes", "true"],
    ]);
    expect(mineFormEntries({
      dfgStatisticalVariants: ["performance"],
      dfgBusinessHours: true, dfgBusinessHourSlots: "0.5-3600.5",
    })).toEqual([
      ["dfg_statistical_variants", "performance"],
      ["dfg_business_hours", "true"],
      ["dfg_business_hour_slots", "0.5-3600.5"],
    ]);
  });

  it("maps correlation variants and only their relevant settings", () => {
    expect(mineFormEntries({
      correlationVariants: ["trace_based", "bogus", "classic_split", "classic"],
      correlationStartTimestampAttribute: "started",
      correlationExactTimeMatching: true,
      correlationSampleSize: 2,
    })).toEqual([
      ["correlation_variants", "classic,classic_split,trace_based"],
      ["correlation_start_timestamp_attribute", "started"],
      ["correlation_exact_time_matching", "true"],
      ["correlation_sample_size", "2"],
    ]);
    expect(mineFormEntries({
      correlationVariants: ["trace_based"],
      correlationExactTimeMatching: true,
      correlationSampleSize: 2,
    })).toEqual([["correlation_variants", "trace_based"]]);
    expect(mineFormEntries({
      correlationVariants: ["classic"],
      correlationSampleSize: 2,
    })).toEqual([["correlation_variants", "classic"]]);
    expect(mineFormEntries({
      correlationVariants: ["classic_split"],
      correlationSampleSize: 100000,
    })).toEqual([["correlation_variants", "classic_split"]]);
    expect(mineFormEntries({
      correlationVariants: ["unknown"],
      correlationStartTimestampAttribute: "started",
    })).toEqual([]);
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
