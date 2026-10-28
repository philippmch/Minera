import { describe, expect, it, vi } from "vitest";
import { fetchExtensivePlayoutVisited, fetchProcessTreePetriNet, processTreePetriNetQuery, fetchPnml, fetchBpmn } from "../src/api";
import { fetchStochasticPetri, fetchStochasticPetriDistribution, stochasticPetriQuery, fetchStochasticMap, stochasticMapQuery } from "../src/api";
import { isStochasticMapResult } from "../src/types";
import { fetchXes } from "../src/api";
import { browserHistoryQuery, requestBrowserHistory } from "../src/api";
import { requestWindowsEvents, windowsEventsQuery } from "../src/api";
import { requestWorkflowHistory } from "../src/api";
import { requestEnterpriseHistory } from "../src/api";
import { requestCommunicationHistory } from "../src/api";
import { knowledgeInjectionQuery, requestKnowledgeInjection } from "../src/api";
import { mineFormEntries, requestTemporalConformance, temporalConformanceQuery } from "../src/api";
import { alignModel, alignmentQuery, decideModel, decisionMiningQuery,
  decisionTablesQuery, evaluateModel, evaluationQuery,
  requestDecisionTables } from "../src/api";

describe("Token replay policy request options", () => {
  it("preserves root-cause attribute order and duplicates, trims names and clears explicit empty lists", () => {
    expect(mineFormEntries({ tokenReplayRootCauseStringAttributes: [" team ", "region", "team"],
      tokenReplayRootCauseNumericAttributes: [] })).toEqual([
      ["token_replay_root_cause_string_attributes", "team,region,team"],
      ["token_replay_root_cause_numeric_attributes", ""],
    ]);
    for (const key of ["tokenReplayRootCauseStringAttributes", "tokenReplayRootCauseNumericAttributes"]) {
      for (const value of [null, "team", [1], [""], [" "], ["a,b"], ["a\nb"]])
        expect(() => mineFormEntries({ [key]: value } as never)).toThrow(/attribute names/);
    }
  });
  it("trims duration timestamp selectors and rejects blank or nonstring values", () => {
    expect(mineFormEntries({ tokenReplayTimestampAttribute: " completed_at " }))
      .toEqual([["token_replay_timestamp_attribute", "completed_at"]]);
    expect(() => mineFormEntries({ tokenReplayTimestampAttribute: " \t " })).toThrow(/must not be empty/);
    for (const value of [null, 1, true, [], {}])
      expect(() => mineFormEntries({ tokenReplayTimestampAttribute: value } as never)).toThrow(/must be a string/);
  });
  it("serializes flat case selectors and exact event caps", () => {
    expect(mineFormEntries({ tokenReplayCaseIdAttribute: " group ", tokenReplayMaxRowsToMaterialize: 0 }))
      .toEqual([["token_replay_case_id_attribute", "group"], ["token_replay_max_rows_to_materialize", "0"]]);
    expect(() => mineFormEntries({ tokenReplayCaseIdAttribute: " " })).toThrow(/must not be empty/);
    for (const value of [-1, 0.5, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1])
      expect(() => mineFormEntries({ tokenReplayMaxRowsToMaterialize: value })).toThrow(/safe integer/);
  });
  const policies = [
    ["tokenReplayConsiderRemainingInFitness", "token_replay_consider_remaining_in_fitness"],
    ["tokenReplayTryToReachFinalMarkingThroughHidden", "token_replay_try_to_reach_final_marking_through_hidden"],
    ["tokenReplayConsiderActivitiesNotInModelInFitness", "token_replay_consider_activities_not_in_model_in_fitness"],
    ["tokenReplayCleaningTokenFlood", "token_replay_cleaning_token_flood"],
    ["tokenReplayExhaustiveInvisibleExploration", "token_replay_exhaustive_invisible_exploration"],
    ["tokenReplayWalkThroughHiddenTransitions", "token_replay_walk_through_hidden_transitions"],
    ["tokenReplayIncludeTraceDiagnostics", "token_replay_include_trace_diagnostics"],
    ["tokenReplayIncludeDurationDiagnostics", "token_replay_include_duration_diagnostics"],
    ["tokenReplayIncludeRootCauseDiagnostics", "token_replay_include_root_cause_diagnostics"],
    ["tokenReplayRootCauseEnableMultiplier", "token_replay_root_cause_enable_multiplier"],
  ] as const;

  it("omits unspecified policies and preserves each explicit true or false", () => {
    expect(mineFormEntries({})).toEqual([]);
    for (const [key, parameter] of policies) {
      for (const value of [true, false]) {
        expect(mineFormEntries({ [key]: value })).toEqual([[parameter, String(value)]]);
      }
    }
  });

  it("rejects nonboolean policy values without truthiness coercion", () => {
    for (const [key] of policies) {
      for (const value of ["false", "true", "", 0, 1, null, [], {}]) {
        expect(() => mineFormEntries({ [key]: value } as never)).toThrow(`${key} must be a boolean`);
      }
    }
  });
});

describe("Inductive request options", () => {
  it("serializes enabled switches and custom selectors", () => {
    expect(mineFormEntries({
      inductiveDisableStrictSequenceCut: true,
      inductiveDisableFallthroughs: true,
      inductiveMultiprocessing: true,
      inductiveSortEventsByTimestamp: true,
      inductiveActivityKey: " phase ",
      inductiveTimestampKey: " occurred_at ",
      inductiveCaseIdKey: " case_ref ",
    })).toEqual([
      ["inductive_disable_strict_sequence_cut", "true"],
      ["inductive_disable_fallthroughs", "true"],
      ["inductive_multiprocessing", "true"],
      ["inductive_sort_events_by_timestamp", "true"],
      ["inductive_activity_key", "phase"],
      ["inductive_timestamp_key", "occurred_at"],
      ["inductive_case_id_key", "case_ref"],
    ]);
  });

  it("elides false switches and default selectors", () => {
    expect(mineFormEntries({
      inductiveDisableStrictSequenceCut: false,
      inductiveDisableFallthroughs: false,
      inductiveMultiprocessing: false,
      inductiveSortEventsByTimestamp: false,
      inductiveActivityKey: "concept:name",
      inductiveTimestampKey: "time:timestamp",
      inductiveCaseIdKey: "case:concept:name",
    })).toEqual([]);
  });
});

describe("Alpha request options", () => {
  it("keeps an explicit zero event cap and rejects invalid caps", () => {
    expect(mineFormEntries({ alphaMaxRowsToMaterialize: 0 }))
      .toEqual([["alpha_max_rows_to_materialize", "0"]]);
    expect(mineFormEntries({ alphaMaxRowsToMaterialize: 12 }))
      .toEqual([["alpha_max_rows_to_materialize", "12"]]);
    for (const value of [-1, 0.5, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1])
      expect(() => mineFormEntries({ alphaMaxRowsToMaterialize: value }))
        .toThrow(/alphaMaxRowsToMaterialize/);
  });
  it("serializes custom activity fields and removal explicitly", () => {
    expect(mineFormEntries({
      alphaVariant: "plus",
      alphaActivityAttribute: " phase ",
      alphaRemoveUnconnected: true,
    })).toEqual([
      ["alpha_variant", "plus"],
      ["alpha_activity_attribute", "phase"],
      ["alpha_remove_unconnected", "true"],
    ]);
  });

  it("elides Alpha defaults and rejects a blank activity field", () => {
    expect(mineFormEntries({
      alphaVariant: "classic",
      alphaActivityAttribute: "concept:name",
      alphaRemoveUnconnected: false,
    })).toEqual([["alpha_variant", "classic"]]);
    expect(() => mineFormEntries({ alphaActivityAttribute: "  " }))
      .toThrow(/alphaActivityAttribute/);
  });
});

describe("Heuristic request options", () => {
  it("omits automatic start-time selection but preserves explicit conventional names", () => {
    expect(mineFormEntries({ heuristicStartTimestampAttribute: undefined })).toEqual([]);
    expect(mineFormEntries({ heuristicStartTimestampAttribute: "" })).toEqual([]);
    expect(mineFormEntries({ heuristicStartTimestampAttribute: "  " })).toEqual([]);
    expect(mineFormEntries({ heuristicStartTimestampAttribute: " start_timestamp " }))
      .toEqual([["heuristic_start_timestamp_attribute", "start_timestamp"]]);
    expect(mineFormEntries({ heuristicStartTimestampAttribute: " began " }))
      .toEqual([["heuristic_start_timestamp_attribute", "began"]]);
  });

  it("serializes business-hour schedules only when enabled", () => {
    expect(mineFormEntries({ heuristicBusinessHours: true })).toEqual([
      ["heuristic_business_hours", "true"],
    ]);
    expect(mineFormEntries({
      heuristicBusinessHours: true,
      heuristicBusinessHourSlots: " none ",
    })).toEqual([
      ["heuristic_business_hours", "true"],
      ["heuristic_business_hour_slots", "none"],
    ]);
    expect(mineFormEntries({
      heuristicBusinessHours: true,
      heuristicBusinessHourSlots: " 32400-61200,378000-406800 ",
    })).toEqual([
      ["heuristic_business_hours", "true"],
      ["heuristic_business_hour_slots", "32400-61200,378000-406800"],
    ]);
    expect(mineFormEntries({
      heuristicBusinessHours: false,
      heuristicBusinessHourSlots: "not-a-schedule",
    })).toEqual([]);
    expect(mineFormEntries({
      heuristicBusinessHourSlots: "none",
    })).toEqual([]);
  });

  it("rejects malformed enabled business-hour schedules", () => {
    for (const slots of ["0-0", "0-604801", "0:3600", "0-3600,", "none,0-3600"]) {
      expect(() => mineFormEntries({ heuristicBusinessHours: true,
        heuristicBusinessHourSlots: slots })).toThrow(/heuristicBusinessHourSlots/);
    }
  });
});

describe("Footprints request options", () => {
  it("serializes every discovery and conformance selector", () => {
    expect(mineFormEntries({
      footprintsDiscoveryVariants: [
        "entire_event_log", "entire_dataframe", "polars_lazyframes",
        "trace_by_trace", "dfg", "petri_reach_graph", "process_tree", "powl",
      ],
      footprintsModel: "powl",
      footprintsConformanceVariants: ["log_model", "log_extensive", "trace_extensive"],
      footprintsStrict: true,
      footprintsEnableActAlwaysExecuted: false,
    })).toEqual([
      ["footprints_discovery_variants", "entire_event_log,entire_dataframe,polars_lazyframes,trace_by_trace,dfg,petri_reach_graph,process_tree,powl"],
      ["footprints_model", "powl"],
      ["footprints_conformance_variants", "log_model,log_extensive,trace_extensive"],
      ["footprints_strict", "true"],
      ["footprints_enable_act_always_executed", "false"],
    ]);
  });

  it("keeps native defaults quiet and rejects incompatible selectors", () => {
    expect(mineFormEntries({
      footprintsDiscoveryVariants: ["entire_event_log", "dfg", "process_tree", "powl"],
      footprintsModel: "process_tree",
      footprintsConformanceVariants: ["log_model"],
    })).toEqual([]);
    expect(() => mineFormEntries({
      footprintsDiscoveryVariants: ["petri_reach_graph"],
      footprintsModel: "dfg",
    })).toThrow(/incompatible/);
    expect(() => mineFormEntries({
      footprintsDiscoveryVariants: ["unknown_variant" as never],
    })).toThrow(/Unknown footprints discovery variant/);
    expect(() => mineFormEntries({
      footprintsConformanceVariants: ["unknown_variant" as never],
    })).toThrow(/Unknown footprints conformance variant/);
    expect(() => mineFormEntries({
      footprintsModel: "unknown" as never,
    })).toThrow(/Unknown footprints model/);
    expect(() => mineFormEntries({
      footprintsDiscoveryVariants: [],
    })).toThrow(/must not be empty/);
  });

  it("serializes custom table selectors and canonicalizes markings", () => {
    expect(mineFormEntries({
      footprintsActivityAttribute: " phase ",
      footprintsCaseIdAttribute: " case_ref ",
      footprintsStartTimestampAttribute: " started_at ",
      footprintsTimestampAttribute: " finished_at ",
      footprintsIndexAttribute: " row_number ",
      footprintsSortRequired: false,
      footprintsInitialMarking: "000, 18446744073709551615",
    })).toEqual([
      ["footprints_activity_attribute", "phase"],
      ["footprints_case_id_attribute", "case_ref"],
      ["footprints_timestamp_attribute", "finished_at"],
      ["footprints_index_attribute", "row_number"],
      ["footprints_start_timestamp_attribute", "started_at"],
      ["footprints_sort_required", "false"],
      ["footprints_initial_marking", "0,18446744073709551615"],
    ]);
    expect(mineFormEntries({
      footprintsActivityAttribute: "concept:name",
      footprintsCaseIdAttribute: "case:concept:name",
      footprintsTimestampAttribute: "time:timestamp",
      footprintsIndexAttribute: "@@index",
      footprintsStartTimestampAttribute: "",
      footprintsSortRequired: true,
    })).toEqual([]);
    expect(mineFormEntries({ footprintsIndexAttribute: "  " })).toEqual([
      ["footprints_index_attribute", ""],
    ]);
  });

  it("rejects malformed footprint markings and selectors", () => {
    expect(() => mineFormEntries({ footprintsInitialMarking: "1,,2" }))
      .toThrow(/non-negative uint64/);
    expect(() => mineFormEntries({ footprintsInitialMarking: "18446744073709551616" }))
      .toThrow(/exceeds uint64/);
    expect(() => mineFormEntries({ footprintsActivityAttribute: " " }))
      .toThrow(/must not be empty/);
  });
});

describe("knowledge injection API", () => {
  it("serializes bounded provider-neutral prompt options", () => {
    expect(knowledgeInjectionQuery({
      knowledge_variant: "database", knowledge_query: "count cases",
      knowledge_database_schema: "events(case_id)", knowledge_max_input_bytes: 100,
    })).toBe("knowledge_variant=database&knowledge_query=count+cases&knowledge_database_schema=events%28case_id%29&knowledge_max_input_bytes=100");
    expect(() => knowledgeInjectionQuery({ knowledge_max_prompt_bytes: 0 })).toThrow(/positive integer/);
  });
  it("validates prompt-construction results", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({
      family: "KNOWLEDGE_INJECTION", variant: "description", computed: true,
      prompt: "Description\\nmodel", input_bytes: 5, error: "",
    }) })));
    await expect(requestKnowledgeInjection({ knowledge_description: "model" }))
      .resolves.toMatchObject({ family: "KNOWLEDGE_INJECTION", computed: true });
    vi.unstubAllGlobals();
  });
});

describe("browser history API", () => {
  it("serializes explicit local connector options", () => {
    expect(browserHistoryQuery({ browser_history_variant: "firefox_history",
      browser_history_path: "C:/profile/places.sqlite", browser_history_max_events: 50 }))
      .toBe("browser_history_max_events=50&browser_history_path=C%3A%2Fprofile%2Fplaces.sqlite&browser_history_variant=firefox_history");
  });
  it("validates connector output", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({
      type: "BROWSER_HISTORY", variant: "chrome_history", trace_count: 0,
      event_count: 0, traces: [],
    }) })));
    await expect(requestBrowserHistory({ browser_history_variant: "chrome_history",
      browser_history_path: "history.db" })).resolves.toMatchObject({ type: "BROWSER_HISTORY" });
    vi.unstubAllGlobals();
  });
});

describe("Windows events API", () => {
  it("serializes the explicit export path and bound", () => {
    expect(windowsEventsQuery({ windows_events_path: "C:/exports/system.xml",
      windows_events_max_events: 100 }))
      .toBe("windows_events_max_events=100&windows_events_path=C%3A%2Fexports%2Fsystem.xml");
  });
  it("validates imported event records", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({
      type: "WINDOWS_EVENTS", variant: "windows_events", trace_count: 0,
      event_count: 0, traces: [],
    }) })));
    await expect(requestWindowsEvents({ windows_events_path: "events.xml" }))
      .resolves.toMatchObject({ type: "WINDOWS_EVENTS" });
    vi.unstubAllGlobals();
  });
});

describe("workflow history API", () => {
  it("posts normalized CSV with a bounded event count", async () => {
    const csv = "process_id,case_id,activity,timestamp,resource\norder,c1,create,2024-01-01T00:00:00Z,alice\n";
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/workflow-history?workflow_max_events=50");
      expect(init?.body).toBe(csv);
      return { ok: true, json: async () => ({ type: "WORKFLOW_HISTORY", variant: "activity_instances", trace_count: 0, event_count: 0, traces: [] }) };
    }));
    await expect(requestWorkflowHistory(csv, 50)).resolves.toMatchObject({ type: "WORKFLOW_HISTORY" });
    vi.unstubAllGlobals();
  });
});
describe("enterprise history API", () => {
  it("posts the selected normalized enterprise variant", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/enterprise-history?enterprise_variant=accounting&enterprise_max_events=25");
      return { ok: true, json: async () => ({ type: "ENTERPRISE_HISTORY", variant: "accounting", trace_count: 0, event_count: 0, traces: [] }) };
    }));
    await expect(requestEnterpriseHistory("header\n", "accounting", 25)).resolves.toMatchObject({ variant: "accounting" });
    vi.unstubAllGlobals();
  });
});
describe("communication history API", () => {
  it("posts explicit calendar export controls", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => { expect(path).toBe("/api/communication-history?communication_include_reminders=false&communication_max_events=20&communication_variant=calendar"); return { ok: true, json: async () => ({ type: "COMMUNICATION_HISTORY", variant: "calendar", trace_count: 0, event_count: 0, traces: [] }) }; }));
    await expect(requestCommunicationHistory("header\n", { communication_variant: "calendar", communication_include_reminders: false, communication_max_events: 20 })).resolves.toMatchObject({ variant: "calendar" }); vi.unstubAllGlobals();
  });
});
import { geneticMatrixConversionQuery, requestGeneticMatrixConversion } from "../src/api";
import { labelSplittingQuery, requestLabelSplitting, logToFeaturesQuery, requestLogToFeatures, logUtilityQuery, requestLogUtility, embeddingsQuery, requestEmbeddings, ilpMinerQuery, requestIlpMine, geneticMinerQuery, requestGeneticMine, traceVariantQuery, requestTraceVariantQuery, pripelQuery, requestPripel, organizationalSnaQuery, requestOrganizationalSna, networkAnalysisQuery, requestNetworkAnalysis, clusteringQuery, requestClustering, profileClusteringQuery, requestProfileClustering, roleDiscoveryQuery, requestRoleDiscovery, resourceProfilesQuery, requestResourceProfiles, localDiagnosticsQuery, ocelDiscoveryQuery, requestOcelDiscovery, requestOcelSqlite, requestOcelSqliteExport, ocelExchangeQuery, requestOcelExchangeImport, requestOcelExchangeExport, ocelFlattenQuery, requestOcelFlatten, ocelFilterQuery, requestOcelFilter, ocelSplitQuery, requestOcelSplit, ocelEnrichmentQuery, requestOcelEnrichment, ocelObjectGraphQuery, requestOcelObjectGraph, ocelEventObjectGraphQuery, requestOcelEventObjectGraph, ocelPlayoutQuery, requestOcelPlayout, ocelFeaturesQuery, requestOcelFeatures, ocelStatisticsQuery, requestOcelStatistics, ocelDescriptionQuery, requestOcelDescription, ocelConformanceQuery, requestOcelConformance, linkAnalysisQuery, requestLinkAnalysis, timestampInterleavingsQuery, requestTimestampInterleavings, monteCarloQuery, fetchMonteCarloPlayout, fetchMonteCarloResult, fetchDfg, fetchDfgPlayout, fetchPlayout, fetchBasicPlayoutVisited, fetchStochasticPlayoutVisited, fetchProcessTreePlayoutSet, processTreePlayoutQuery, declarePlayoutQuery, fetchPtml, dfgExportQuery, importDfg, dfgPrecisionQuery, requestDfgPrecision, dfgPetriConversionQuery, convertDfgToPetriNet, requestTriePetriNet, processCubeQuery, requestProcessCube, streamingDfgQuery, requestStreamingDfg, streamingFootprintsQuery, requestStreamingFootprints, streamingTemporalQuery, requestStreamingTemporal, streamingTbrQuery, requestStreamingTbr, requestDataPetriNet, chaoticActivitiesQuery, requestChaoticActivities, passedTimeQuery, requestPassedTime, serviceTimeQuery, requestServiceTime, reworkQuery, requestRework, cycleTimeQuery, requestCycleTime, caseArrivalQuery, requestCaseArrival, caseStatisticsQuery, requestCaseStatistics, attributeStatisticsQuery, requestAttributeStatistics, attributeDistributionQuery, requestAttributeDistribution, attributeKdeQuery, requestAttributeKde, boundaryActivitiesQuery, requestBoundaryActivities, variantStatisticsQuery, requestVariantStatistics, comparePetriUsage, petriNetReductionQuery, reducePetriNet } from "../src/api";

describe("XES export API", () => {
  it("requests the selected exporter as a binary-safe blob", async () => {
    const payload = new Blob(["compressed"], { type: "application/gzip" });
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/xes?xes_export_variant=etree&xes_compress=true");
      return { ok: true, blob: async () => payload };
    }));
    await expect(fetchXes({ variant: "etree", compress: true }))
      .resolves.toBe(payload);
    vi.unstubAllGlobals();
  });
});

describe("Petri usage comparison API", () => {
  it.each([
    [true, false], [false, true], [undefined, undefined],
  ] as const)("preserves separate comparison cleaning=%s and exhaustive=%s policies", async (cleaning, exhaustive) => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init?: RequestInit) => {
      const form = init?.body as FormData;
      expect(form.get("cleaning_token_flood")).toBe(cleaning === undefined ? null : String(cleaning));
      expect(form.get("exhaustive_invisible_exploration")).toBe(exhaustive === undefined ? null : String(exhaustive));
      return { ok: true, json: async () => ({ family: "PETRI_USAGE_COMPARISON", places: [], transitions: [], arcs: [] }) };
    }));
    try {
      await comparePetriUsage(new File(["case_id,activity\nc1,A\n"], "reference.csv"), "alpha", {
        cleaning_token_flood: cleaning,
        exhaustive_invisible_exploration: exhaustive,
      });
    } finally { vi.unstubAllGlobals(); }
  });

  it("serializes explicit replay markings in the multipart request", async () => {
    const payload = {
      family: "PETRI_USAGE_COMPARISON", places: [], transitions: [], arcs: [],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/compare-petri");
      expect(init?.method).toBe("POST");
      const form = init?.body as FormData;
      expect(form.get("miner")).toBe("alpha");
      expect(form.get("activity_attribute")).toBe("business_activity");
      expect(form.get("initial_marking")).toBe("1,0,2");
      expect(form.get("final_marking")).toBe("0,0,1");
      expect(form.get("walk_through_hidden_trans")).toBe("false");
      expect(form.get("try_to_reach_final_marking_through_hidden")).toBe("true");
      expect(form.get("stop_immediately_unfit")).toBe("true");
      return { ok: true, json: async () => payload };
    }));
    await expect(comparePetriUsage(
      new File(["case_id,activity\nc1,A\n"], "reference.csv"),
      "alpha",
      {
        activity_attribute: " business_activity ",
        initial_marking: [1, 0, 2],
        final_marking: [0, 0, 1],
        walk_through_hidden_trans: false,
        try_to_reach_final_marking_through_hidden: true,
        stop_immediately_unfit: true,
      },
    )).resolves.toMatchObject({ family: "PETRI_USAGE_COMPARISON" });
    vi.unstubAllGlobals();
  });

  it("rejects malformed marking vectors before upload", async () => {
    await expect(comparePetriUsage(
      new File([""], "reference.csv"), "inductive", { final_marking: [1, Number.NaN] },
    )).rejects.toThrow(/uint32/);
  });

  it("rejects an empty custom activity attribute before upload", async () => {
    await expect(comparePetriUsage(
      new File([""], "reference.csv"), "inductive", { activity_attribute: "   " },
    )).rejects.toThrow(/activity_attribute/);
  });

  it("trims qualified first-class selectors before upload", async () => {
    const payload = {
      family: "PETRI_USAGE_COMPARISON", places: [], transitions: [], arcs: [],
    };
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init?: RequestInit) => {
      const form = init?.body as FormData;
      expect(form.get("activity_attribute")).toBe("org:resource");
      return { ok: true, json: async () => payload };
    }));
    await expect(comparePetriUsage(
      new File(["case_id,resource\nc1,A\n"], "reference.csv"),
      "inductive", { activity_attribute: "  org:resource  " },
    )).resolves.toMatchObject({ family: "PETRI_USAGE_COMPARISON" });
    vi.unstubAllGlobals();
  });
});

describe("classic DFG export API", () => {
  it("serializes optional noise cleaning settings", () => {
    expect(dfgExportQuery({ noise_threshold: 0.5, most_common_paths: "A -> B" }))
      .toBe("noise_threshold=0.5&most_common_paths=A+-%3E+B");
    expect(dfgExportQuery({
      activities_percentage: 0.5,
      paths_percentage: 0.75,
      keep_connected: 0.8,
      keep_all_activities: true,
      to_activity: "C",
      from_activity: "A",
      contain_activity: "B",
    })).toBe("activities_percentage=0.5&paths_percentage=0.75&keep_connected=0.8&keep_all_activities=true&to_activity=C&from_activity=A&contain_activity=B");
    expect(() => dfgExportQuery({ noise_threshold: Number.NaN })).toThrow(/finite/);
    expect(() => dfgExportQuery({ most_common_paths: " " })).toThrow(/empty/);
    expect(() => dfgExportQuery({ most_common_paths: "A -> B" })).toThrow(/requires noise_threshold/);
    expect(() => dfgExportQuery({ activities_percentage: -0.1 })).toThrow(/\[0, 1\]/);
    expect(() => dfgExportQuery({ paths_percentage: 1.1 })).toThrow(/\[0, 1\]/);
    expect(() => dfgExportQuery({ keep_connected: Number.NaN })).toThrow(/finite/);
    expect(() => dfgExportQuery({ keep_all_activities: true })).toThrow(/requires paths_percentage/);
    expect(() => dfgExportQuery({ to_activity: " " })).toThrow(/must not be empty/);
  });

  it("fetches the cached graph as text", async () => {
    const text = "2\nA\nB\n1\n0x1\n1\n1x1\n0>1x1\n";
    const fetchMock = vi.fn(async (path: string) => {
      expect(path).toBe("/api/dfg");
      return { ok: true, text: async () => text };
    });
    vi.stubGlobal("fetch", fetchMock);
    await expect(fetchDfg()).resolves.toBe(text);
    vi.unstubAllGlobals();
  });

  it("propagates backend export errors", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: false,
      status: 409,
      json: async () => ({ error: "no mined log; mine a log first" }),
    })));
    await expect(fetchDfg()).rejects.toThrow(/no mined log/);
    vi.unstubAllGlobals();
  });
});

describe("process-tree Petri conversion API", () => {
  it("serializes strict conversion selectors and fetches the typed model", async () => {
    expect(processTreePetriNetQuery()).toBe("");
    expect(processTreePetriNetQuery({ variant: "transition_bordered" }))
      .toBe("process_tree_variant=transition_bordered");
    expect(() => processTreePetriNetQuery({ variant: "future" as never }))
      .toThrow(/standard or transition_bordered/);
    const payload = {
      family: "PROCESS_TREE_PETRI_NET", variant: "transition_bordered",
      place_count: 2, source: 0, sink: 1, activities: ["A"],
      transitions: [{ activity: 0, inputs: [], outputs: [1] }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe(
        "/api/process-tree-petri?process_tree_variant=transition_bordered",
      );
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchProcessTreePetriNet({ variant: "transition_bordered" }))
      .resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("keeps export routes compatible while forwarding an explicit selector", async () => {
    const paths: string[] = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      paths.push(path);
      return { ok: true, text: async () => "<model/>" };
    }));
    await expect(fetchPnml("inductive")).resolves.toBe("<model/>");
    await expect(fetchBpmn("inductive", { variant: "transition_bordered" }))
      .resolves.toBe("<model/>");
    expect(paths).toEqual([
      "/api/pnml?miner=inductive",
      "/api/bpmn?miner=inductive&process_tree_variant=transition_bordered",
    ]);
    vi.unstubAllGlobals();
  });
});

describe("POWL export and deferred API routes", () => {
  it("selects the POWL workflow net for PNML and BPMN exports", async () => {
    const paths: string[] = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      paths.push(path);
      return { ok: true, text: async () => "<model/>" };
    }));
    await expect(fetchPnml("powl")).resolves.toBe("<model/>");
    await expect(fetchBpmn("powl")).resolves.toBe("<model/>");
    expect(paths).toEqual([
      "/api/pnml?miner=powl",
      "/api/bpmn?miner=powl",
    ]);
    vi.unstubAllGlobals();
  });

  it("serializes basic POWL playout as an XES request", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/playout?miner=powl&mode=basic&traces=2&max_steps=4");
      return { ok: true, text: async () => "<log/>" };
    }));
    await expect(fetchPlayout("powl", {
      mode: "basic", traces: 2, max_steps: 4,
    })).resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });

  it("serializes POWL decision and decision-table requests", async () => {
    const options = {
      activity_attribute: " region ", max_depth: 3,
      attributes: [" amount ", "status"], include_trace_attributes: true,
    };
    expect(decisionMiningQuery(options)).toBe(
      "decision_activity_attribute=region&decision_max_depth=3&decision_include_trace_attributes=true&decision_attributes=amount%2Cstatus",
    );
    expect(decisionTablesQuery({ ...options, lookback: 2 })).toBe(
      "decision_activity_attribute=region&decision_max_depth=3&decision_include_trace_attributes=true&decision_attributes=amount%2Cstatus&decision_lookback=2",
    );

    const paths: string[] = [];
    const tablePayload = {
      family: "DECISION_TABLES", variant: "encoded", trace_count: 1,
      observed_traces: 1, tables: [{ place: 0, classes: ["yes"],
        feature_names: [], rows: [] }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      paths.push(path);
      if (path.startsWith("/api/decision-tables"))
        return { ok: true, json: async () => tablePayload };
      return { ok: true, json: async () => ({ decision_points: [] }) };
    }));
    await expect(decideModel("powl", options))
      .resolves.toMatchObject({ decision_points: [] });
    await expect(requestDecisionTables("powl", { ...options, lookback: 2 }))
      .resolves.toEqual(tablePayload);
    expect(paths).toEqual([
      "/api/decide?decision_activity_attribute=region&decision_max_depth=3&decision_include_trace_attributes=true&decision_attributes=amount%2Cstatus&miner=powl",
      "/api/decision-tables?miner=powl&decision_activity_attribute=region&decision_max_depth=3&decision_include_trace_attributes=true&decision_attributes=amount%2Cstatus&decision_lookback=2",
    ]);
    vi.unstubAllGlobals();
  });

  it("serializes POWL evaluation and alignment requests", async () => {
    expect(evaluationQuery({
      evaluation_family: "generalization",
      generalization_variant: "generalization_token",
    })).toBe("evaluation_family=generalization&generalization_variant=generalization_token");
    expect(alignmentQuery({
      variant: "dijkstra_less_memory", max_states: 9,
      activity_attribute: " label ",
    })).toBe("alignment_variant=dijkstra_less_memory&max_states=9&alignment_activity_attribute=label");

    const paths: string[] = [];
    const evaluationPayload = {
      miner: "powl",
      replay_fitness: {
        variant: "token_based", computed: true, exhausted: false, error: null,
        fitness: 1, log_fitness: 1, average_trace_fitness: 1,
        percentage_of_fitting_traces: 100, fitting_traces: 1,
        trace_count: 1, total_cost: 0,
      },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      paths.push(path);
      if (path.startsWith("/api/evaluate"))
        return new Response(JSON.stringify(evaluationPayload));
      return { ok: true, json: async () => ({ computed: true }) };
    }));
    await expect(evaluateModel("powl", {
      evaluation_family: "replay_fitness",
      fitness_variant: "token_based",
      evaluation_activity_attribute: " phase ",
      evaluation_token_replay_variant: "forward",
      fitness_consider_unknown_activities: true,
    })).resolves.toEqual(evaluationPayload);
    await expect(alignModel("powl", {
      variant: "dijkstra_less_memory", max_states: 9,
      activity_attribute: " label ",
    })).resolves.toMatchObject({ computed: true });
    expect(paths).toEqual([
      "/api/evaluate?evaluation_family=replay_fitness&fitness_variant=token_based&evaluation_activity_attribute=phase&evaluation_token_replay_variant=forward&fitness_consider_unknown_activities=true&miner=powl",
      "/api/align?miner=powl&alignment_variant=dijkstra_less_memory&max_states=9&alignment_activity_attribute=label",
    ]);
    vi.unstubAllGlobals();
  });
});

describe("DFG playout API", () => {
  it("decodes structured classic variant counts", async () => {
    const payload = {
      family: "DFG_PLAYOUT", variant: "CLASSIC", return_variants: true,
      complete: true, interrupted: false,
      completed_traces: 0, truncated_traces: 0,
      variants: [{ activities: ["A", "B"], occurrences: 2 }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=dfg");
      expect(path).toContain("dfg_playout_return_variants=true");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchDfgPlayout({ return_variants: true }))
      .resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("retains XES text for ordinary playout", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true, text: async () => "<log/>",
    })));
    await expect(fetchDfgPlayout()).resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });
});

describe("basic Petri playout API", () => {
  it("serializes and fetches the tangible stochastic Petri result", async () => {
    expect(stochasticPetriQuery({
      transition_rates: [2, 3], initial_marking: [1, 0],
      immediate_transitions: [0], max_states: 8,
    })).toBe("stochastic_petri_rates=2%2C3&stochastic_petri_initial_marking=1%2C0&stochastic_petri_immediate_transitions=0&stochastic_petri_max_states=8");
    const payload = {
      family: "STOCHASTIC_PETRI", variant: "TANGIBLE_CTMC", exhausted: false,
      diagnostic: "", states: [[0, 1], [1, 0]],
      edges: [{ source: 1, target: 0, transition: 0, rate: 2 }],
      generator: [[0, 0], [2, -2]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/stochastic-petri?miner=inductive");
      expect(path).toContain("stochastic_petri_max_states=8");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchStochasticPetri("inductive", { max_states: 8 }))
      .resolves.toEqual(payload);
    vi.unstubAllGlobals();
    expect(() => stochasticPetriQuery({ max_states: 0 })).toThrow(/positive/);
    expect(() => stochasticPetriQuery({ max_states: 2001 })).toThrow(/2000/);
    expect(() => stochasticPetriQuery({ transition_rates: [Number.NaN] }))
      .toThrow(/finite/);
  });

  it("uses the generic Petri route for basic-only controls", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=inductive");
      expect(path).toContain("add_only_if_fm_is_reached=true");
      expect(path).toContain("initial_marking=0%2C1%2C0");
      return { ok: true, text: async () => "<log/>" };
    }));
    await expect(fetchPlayout("inductive", {
      traces: 2, initial_marking: [0, 1, 0],
      add_only_if_fm_is_reached: true,
    })).resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });

  it("uses the dedicated Declare playout contract", async () => {
    expect(declarePlayoutQuery({ traces: 0, min_length: 2, max_length: 8, seed: 7 }))
      .toBe("traces=0&min_length=2&max_length=8&seed=7");
    expect(() => declarePlayoutQuery({ min_length: 4, max_length: 2 }))
      .toThrow(/must not exceed/);
    expect(() => declarePlayoutQuery({ traces: -1 })).toThrow(/between 0/);
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=declare");
      expect(path).toContain("traces=3");
      expect(path).toContain("min_length=1");
      expect(path).not.toContain("playout_");
      return { ok: true, text: async () => "<log/>" };
    }));
    await expect(fetchPlayout("declare", { traces: 3, min_length: 1, max_length: 5 }))
      .resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });

  it("serializes and validates basic POWL visited playout", async () => {
    const payload = {
      family: "PETRI_PLAYOUT_VISITED", variant: "BASIC_PLAYOUT",
      truncated: false, trace_limit_reached: false, state_limit_reached: false,
      traces: [[
        { kind: "marking", marking: [1, 0] },
        { kind: "transition", transition: 0, activity: "finish" },
        { kind: "marking", marking: [0, 1] },
      ]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=powl");
      expect(path).toContain("mode=basic");
      expect(path).toContain("traces=2");
      expect(path).toContain("max_steps=7");
      expect(path).toContain("initial_marking=1%2C0");
      expect(path).toContain("final_marking=0%2C1");
      expect(path).toContain("return_visited_elements=true");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchBasicPlayoutVisited("powl", {
      mode: "basic", traces: 2, max_steps: 7,
      initial_marking: [1, 0], final_marking: [0, 1],
    })).resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("serializes and validates extensive POWL visited playout", async () => {
    const payload = {
      family: "PETRI_PLAYOUT_VISITED", variant: "EXTENSIVE_PLAYOUT",
      truncated: false, trace_limit_reached: false, state_limit_reached: false,
      traces: [[
        { kind: "marking", marking: [1, 0] },
        { kind: "transition", transition: 0, activity: null },
        { kind: "marking", marking: [0, 1] },
      ]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=powl");
      expect(path).toContain("mode=extensive");
      expect(path).toContain("max_length=3");
      expect(path).toContain("max_marking_occ=2");
      expect(path).toContain("return_elements=true");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchExtensivePlayoutVisited("powl", {
      max_length: 3, max_marking_occ: 2,
    })).resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("serializes transient controls and decodes probability distributions", async () => {
    const options = {
      analysis: "transient" as const, time_seconds: 1.5,
      initial_state: 1, max_states: 8,
    };
    const query = stochasticPetriQuery(options);
    expect(query).toContain("stochastic_petri_analysis=transient");
    expect(query).toContain("stochastic_petri_time=1.5");
    expect(query).toContain("stochastic_petri_initial_state=1");
    const payload = {
      family: "STOCHASTIC_PETRI_DISTRIBUTION",
      variant: "TRANSIENT", computed: true, unique: true,
      diagnostic: "", time_seconds: 1.5, initial_state: 1,
      probabilities: [0.25, 0.75],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("stochastic_petri_analysis=transient");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchStochasticPetriDistribution("inductive", options))
      .resolves.toEqual(payload);
    vi.unstubAllGlobals();
    expect(() => stochasticPetriQuery({ time_seconds: 1 })).toThrow(/requires transient/);
    expect(() => stochasticPetriQuery({ analysis: "transient", time_seconds: 1_000_001 }))
      .toThrow(/safety limit/);
    expect(() => stochasticPetriQuery({
      analysis: "transient", initial_probabilities: [0.2, 0.2],
    })).toThrow(/unit mass/);
    expect(() => stochasticPetriQuery({
      analysis: "steady_state", initial_state: 0,
    })).toThrow(/requires transient/);
  });

  it("fetches a fitted stochastic map and its tangible reachability", async () => {
    const options = {
      source: "performance_dfg" as const,
      distribution: "exponential" as const,
      conversion_variant: "invisibles_no_duplicates" as const,
      max_states: 8,
    };
    const query = stochasticMapQuery(options);
    expect(query).toContain("stochastic_map_source=performance_dfg");
    expect(query).toContain("stochastic_map_conversion_variant=invisibles_no_duplicates");
    const payload = {
      family: "STOCHASTIC_MAP", source: "performance_dfg",
      map: { family: "STOCHASTIC_MAP", transitions: [{
        transition: 0, timing: "timed", distribution: "exponential",
        parameter: 2, secondary_parameter: 0, tertiary_parameter: 0,
        activation_weight: 1, priority: 0,
      }] },
      reachability: {
        family: "STOCHASTIC_PETRI", variant: "TANGIBLE_CTMC", exhausted: false,
        diagnostic: "", states: [[1]], edges: [], generator: [[0]],
      },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/stochastic-map?miner=inductive");
      expect(path).toContain("stochastic_map_max_states=8");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchStochasticMap("inductive", options)).resolves.toEqual(payload);
    vi.unstubAllGlobals();
    expect(() => stochasticMapQuery({ max_states: 2001 })).toThrow(/2000/);
    expect(() => stochasticMapQuery({ distribution: "exponential", default_parameter: 0 }))
      .toThrow(/positive/);
    expect(() => stochasticMapQuery({ distribution: "gamma", default_parameter: 0 }))
      .toThrow(/positive/);
    const lognormal = {
      ...payload,
      map: { ...payload.map, transitions: [{
        ...payload.map.transitions[0], distribution: "lognormal",
        parameter: -1, secondary_parameter: 0,
      }] },
    };
    expect(isStochasticMapResult(lognormal)).toBe(true);
    expect(isStochasticMapResult({
      ...lognormal,
      map: { ...lognormal.map, transitions: [{
        ...lognormal.map.transitions[0], distribution: "unsupported",
      }] },
    })).toBe(false);
    expect(isStochasticMapResult({
      ...payload,
      reachability: { ...payload.reachability, initial_state: 1 },
    })).toBe(false);
    expect(isStochasticMapResult({
      ...payload,
      reachability: { ...payload.reachability, initial_probabilities: [] },
    })).toBe(false);
    expect(isStochasticMapResult({
      ...payload,
      reachability: {
        ...payload.reachability, exhausted: true, initial_probabilities: [],
      },
    })).toBe(true);
  });

  it("decodes structured visited markings and transitions", async () => {
    const payload = {
      family: "PETRI_PLAYOUT_VISITED", variant: "BASIC_PLAYOUT",
      truncated: false, trace_limit_reached: false, state_limit_reached: false,
      traces: [[
        { kind: "marking", marking: [1, 0] },
        { kind: "transition", transition: 0, activity: "finish" },
        { kind: "marking", marking: [0, 1] },
      ]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=inductive");
      expect(path).toContain("return_visited_elements=true");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchBasicPlayoutVisited("inductive", { traces: 1 }))
      .resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("decodes stochastic visited markings and preserves stochastic controls", async () => {
    const payload = {
      family: "PETRI_PLAYOUT_VISITED", variant: "STOCHASTIC_PLAYOUT",
      truncated: false, trace_limit_reached: false, state_limit_reached: false,
      traces: [[
        { kind: "marking", marking: [1, 0] },
        { kind: "transition", transition: 0, activity: null },
        { kind: "marking", marking: [0, 1] },
      ]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=stochastic");
      expect(path).toContain("stochastic_return_visited_elements=true");
      expect(path).toContain("stochastic_initial_marking=1%2C0");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchStochasticPlayoutVisited({
      traces: 1, initial_marking: [1, 0],
    })).resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("requests extensive Petri visited elements with strict mode controls", async () => {
    const payload = {
      family: "PETRI_PLAYOUT_VISITED", variant: "EXTENSIVE_PLAYOUT",
      truncated: true, trace_limit_reached: true, state_limit_reached: false,
      traces: [[
        { kind: "marking", marking: [1, 0] },
        { kind: "transition", transition: 0, activity: null },
        { kind: "marking", marking: [0, 1] },
      ]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=inductive");
      expect(path).toContain("mode=extensive");
      expect(path).toContain("return_elements=true");
      expect(path).toContain("max_marking_occ=2");
      expect(path).toContain("max_length=1");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchExtensivePlayoutVisited("inductive", {
      max_marking_occ: 2, max_length: 1,
    })).resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("fetches the extensive process-tree activity-word set", async () => {
    const payload = {
      family: "PROCESS_TREE_PLAYOUT", variant: "EXTENSIVE",
      return_set_strings: true, truncated: false, traces: [["C"], ["A", "B"]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toContain("/api/playout?miner=process_tree");
      expect(path).toContain("variant=EXTENSIVE");
      expect(path).toContain("return_set_strings=true");
      return { ok: true, json: async () => payload };
    }));
    await expect(fetchProcessTreePlayoutSet()).resolves.toEqual(payload);
    vi.unstubAllGlobals();
  });

  it("validates the process-tree set query variant", () => {
    expect(processTreePlayoutQuery({ variant: "EXTENSIVE", return_set_strings: true }))
      .toBe("variant=EXTENSIVE&return_set_strings=true");
    expect(() => processTreePlayoutQuery({ variant: "TOPBOTTOM", return_set_strings: true }))
      .toThrow(/EXTENSIVE/);
    expect(() => processTreePlayoutQuery({ return_set_strings: true }))
      .toThrow(/EXTENSIVE/);
  });
});

describe("PTML export API", () => {
  it("selects a workflow-net miner when requested", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/ptml?miner=powl");
      return { ok: true, text: async () => "<ptml/>" };
    }));
    await expect(fetchPtml("powl")).resolves.toBe("<ptml/>");
    vi.unstubAllGlobals();
  });
});

describe("DFG precision API", () => {
  it("serializes the optional activity attribute", () => {
    expect(dfgPrecisionQuery({ activity_attribute: "label" }))
      .toBe("activity_attribute=label");
    expect(dfgPrecisionQuery()).toBe("");
    expect(() => dfgPrecisionQuery({ activity_attribute: " " })).toThrow(/must not be empty/);
  });

  it("posts a DFG document and validates the result", async () => {
    const document = new Blob(["2\nA\nB\n1\n0x1\n1\n1x1\n0>1x1\n"], { type: "text/plain" });
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/dfg-precision?activity_attribute=label");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => ({ family: "DFG_PRECISION", computed: true,
        precision: 1, prefix_occurrences: 1, allowed_prefix_occurrences: 1,
        activated_transitions: 1, escaping_transitions: 0, error: "" }) };
    }));
    await expect(requestDfgPrecision(document, { activity_attribute: "label" }))
      .resolves.toMatchObject({ family: "DFG_PRECISION", precision: 1 });
    vi.unstubAllGlobals();
  });
});

describe("log utility API", () => {
  const log = { attributes: [], traces: [{ case_id: "c1", attributes: [], events: [{ case_id: "c1", activity: "A", timestamp: "1", resource: "", lifecycle: "", input_order: 0, attributes: [] }] }] };
  it("serializes operation-specific options", () => {
    expect(logUtilityQuery({ variant: "fixed_prefixes", prefix_length: 2 }))
      .toBe("prefix_length=2&variant=fixed_prefixes");
    expect(() => logUtilityQuery({ variant: "fixed_prefixes" })).toThrow(/prefix_length/);
    expect(() => logUtilityQuery({ variant: "until_activity" })).toThrow(/activity/);
    expect(logUtilityQuery({ variant: "to_activities", activities: ["C", "B"] }))
      .toBe("activities=C%2CB&variant=to_activities");
    expect(logUtilityQuery({
      variant: "feature_selection", feature_min_different_values: 2,
      feature_max_different_values: 3, feature_consider_all_attributes: false,
      feature_mandatory_attributes: ["score", "region"],
    })).toBe("feature_consider_all_attributes=false&feature_mandatory_attributes=score%2Cregion&feature_max_different_values=3&feature_min_different_values=2&variant=feature_selection");
    expect(() => logUtilityQuery({
      variant: "feature_selection", feature_min_different_values: 4,
      feature_max_different_values: 2,
    })).toThrow(/minimum/);
  });
  it("posts and validates transformed logs", async () => {
    const payload = { family: "LOG_UTIL", variant: "UNTIL_ACTIVITY", log, elapsed_seconds: [1] };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/log-util?activity=C&timestamp_key=timestamp&variant=until_activity");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestLogUtility({ variant: "until_activity", activity: "C", timestamp_key: "timestamp" }))
      .resolves.toMatchObject({ family: "LOG_UTIL", variant: "UNTIL_ACTIVITY" });
    vi.unstubAllGlobals();
  });
});

describe("data Petri net API", () => {
  const payload = {
    family: "DATA_PETRI_NET", variant: "decision_tree", miner: "inductive",
    place_count: 3, source: 0, sink: 2, activities: ["A", "B"],
    transitions: [{ index: 0, activity: "A", silent: false, inputs: [0], outputs: [1],
      guard: null, read_variables: [], write_variables: [] }],
    decisions: { observed_traces: 1, trace_count: 1, decision_points: [] },
  };
  it("requests and validates guarded models", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/data-petri-net?miner=inductive");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestDataPetriNet()).resolves.toMatchObject({ family: "DATA_PETRI_NET" });
    vi.unstubAllGlobals();
  });
  it("rejects an empty miner", async () => {
    await expect(requestDataPetriNet(" ")).rejects.toThrow(/must not be empty/);
  });
  it("shares decision controls with guarded-model projections", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe(
        "/api/data-petri-net?miner=alpha&decision_max_depth=1&decision_attributes=amount",
      );
      return { ok: true, json: async () => payload };
    }));
    await expect(requestDataPetriNet({
      miner: "alpha", max_depth: 1, attributes: ["amount"],
    })).resolves.toMatchObject({ family: "DATA_PETRI_NET" });
    vi.unstubAllGlobals();
  });
  it("serializes bounded table lookback without sending it to decisions", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe(
        "/api/data-petri-net?miner=alpha&decision_lookback=2",
      );
      return { ok: true, json: async () => payload };
    }));
    await expect(requestDataPetriNet({ miner: "alpha", lookback: 2 }))
      .resolves.toMatchObject({ family: "DATA_PETRI_NET" });
    vi.unstubAllGlobals();
  });
  it("rejects zero and oversized table lookback", async () => {
    await expect(requestDataPetriNet({ lookback: 0 })).rejects.toThrow(/lookback/);
    await expect(requestDataPetriNet({ lookback: 65 })).rejects.toThrow(/lookback/);
  });
});

describe("process cube API", () => {
  it("serializes bounded dimensions, aggregation, and explicit edges", () => {
    expect(processCubeQuery({
      x_col: "throughput",
      y_col: "cost",
      agg_col: "value",
      aggregation: "median",
      max_divisions_x: 3,
      x_bins: [0, 1.5, 4],
    })).toBe("x_col=throughput&y_col=cost&agg_col=value&aggregation=median&max_divisions_x=3&x_bins=0%2C1.5%2C4");
    expect(processCubeQuery({
      x_cols: ["region", "segment"],
      y_col: "score",
    })).toBe("x_cols=region%2Csegment&y_col=score");
    expect(processCubeQuery({ x_cols: ["a,b", "c\\d"] }))
      .toBe("x_cols=a%5C%2Cb%2Cc%5C%5Cd");
    expect(() => processCubeQuery({ x_col: "x", x_cols: ["x", "y"] })).toThrow(/mutually exclusive/);
  });

  it("posts a typed table and validates the result envelope", async () => {
    const payload = {
      family: "PROCESS_CUBE",
      x_dimension: "x",
      y_dimension: "y",
      aggregation: "mean",
      x_bins: [{ label: "[0,1]", lower: 0, upper: 1, numeric: true }],
      y_bins: [{ label: "[0,1]", lower: 0, upper: 1, numeric: true }],
      x_edges: [0, 1],
      y_edges: [0, 1],
      cells: [[2]],
      case_ids: [[['case-1']]],
    };
    const fetchMock = vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/process-cube?x_col=x&y_col=y&agg_col=v");
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({ "Content-Type": "application/json" });
      expect(init?.body).toBe(JSON.stringify({ case_ids: ["case-1"], columns: { x: [0.5], y: [0.5], v: [2] } }));
      return { ok: true, json: async () => payload };
    });
    vi.stubGlobal("fetch", fetchMock);
    await expect(requestProcessCube(
      { case_ids: ["case-1"], columns: { x: [0.5], y: [0.5], v: [2] } },
      { x_col: "x", y_col: "y", agg_col: "v" },
    )).resolves.toMatchObject({ family: "PROCESS_CUBE" });
    vi.unstubAllGlobals();
  });

  it("rejects unsafe explicit bins before making a request", () => {
    expect(() => processCubeQuery({ x_bins: [0, Number.POSITIVE_INFINITY] })).toThrow(/finite edges/);
    expect(() => processCubeQuery({ x_col: " " })).toThrow(/must not be empty/);
  });

  it("rejects malformed input tables before making a request", async () => {
    await expect(requestProcessCube(
      { case_ids: ["case-1"], columns: { x: [Number.NaN] } },
      { x_col: "x", y_col: "y", agg_col: "v" },
    )).rejects.toThrow(/rectangular finite numeric or categorical columns/);
  });
});

describe("streaming DFG API", () => {
  it("serializes column selectors", () => {
    expect(streamingDfgQuery({ streaming_case_id_column: "case", streaming_activity_column: "phase" }))
      .toBe("streaming_case_id_column=case&streaming_activity_column=phase");
    expect(() => streamingDfgQuery({ streaming_activity_column: " " })).toThrow(/empty/);
  });

  it("uploads CSV and validates the incremental result", async () => {
    const payload = {
      family: "STREAMING_DFG", events: 2,
      graph: [{ source: "A", target: "B", count: 1 }],
      activities: { A: 1, B: 1 }, start_activities: { A: 1 },
      end_activities: { B: 1 }, open_cases: { c1: "B" },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/streaming-dfg?streaming_case_id_column=case");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestStreamingDfg(new Blob(["case,activity\nc1,A\nc1,B\n"]), {
      streaming_case_id_column: "case",
    })).resolves.toMatchObject({ family: "STREAMING_DFG", events: 2 });
    vi.unstubAllGlobals();
  });
});

describe("streaming footprints API", () => {
  const options = {
    streaming_footprints_activities: ["A", "B", "C"],
    streaming_footprints_start_activities: ["A"],
    streaming_footprints_end_activities: ["C"],
    streaming_footprints_sequence: [["A", "B"]] as Array<[string, string]>,
  };
  it("serializes the model and rejects unsafe names", () => {
    expect(streamingFootprintsQuery(options)).toBe(
      "streaming_footprints_activities=A%2CB%2CC&streaming_footprints_start_activities=A&streaming_footprints_end_activities=C&streaming_footprints_sequence=A-%3EB",
    );
    expect(() => streamingFootprintsQuery({ streaming_footprints_activities: ["A,B"] })).toThrow(/activity names/);
  });
  it("uploads CSV and validates the snapshot", async () => {
    const payload = {
      family: "STREAMING_FOOTPRINTS", total_events_processed: 2,
      total_deviations: 0,
      cases: [{ case_id: "c1", events: 2, deviations: 0, is_fit: true,
        first_activity: "A", last_activity: "B" }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toContain("/api/streaming-footprints?");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestStreamingFootprints(new Blob(["case,activity\nc1,A\nc1,B\n"]), options))
      .resolves.toMatchObject({ family: "STREAMING_FOOTPRINTS", total_deviations: 0 });
    vi.unstubAllGlobals();
  });
});

describe("streaming temporal API", () => {
  const options = {
    streaming_temporal_profile: [["A", "B", 10, 2]] as Array<[string, string, number, number]>,
    streaming_temporal_zeta: 3,
  };
  it("serializes profiles and validates zeta", () => {
    expect(streamingTemporalQuery(options)).toBe(
      "streaming_temporal_profile=A-%3EB%3A10%3A2&streaming_temporal_zeta=3",
    );
    // Any finite zeta is accepted, as in the reference (zero and negative included).
    expect(streamingTemporalQuery({ streaming_temporal_profile: options.streaming_temporal_profile, streaming_temporal_zeta: 0 })).toBe(
      "streaming_temporal_profile=A-%3EB%3A10%3A2&streaming_temporal_zeta=0",
    );
    expect(streamingTemporalQuery({ streaming_temporal_profile: options.streaming_temporal_profile, streaming_temporal_zeta: -1 })).toBe(
      "streaming_temporal_profile=A-%3EB%3A10%3A2&streaming_temporal_zeta=-1",
    );
    expect(() => streamingTemporalQuery({ streaming_temporal_profile: options.streaming_temporal_profile, streaming_temporal_zeta: Infinity })).toThrow(/finite/);
  });
  it("uploads CSV and validates deviations", async () => {
    const payload = {
      family: "STREAMING_TEMPORAL", total_events_processed: 2,
      total_deviations: 1, cases: { c1: { events: 2, deviations: 1 } },
      deviations: { c1: [{ source: "A", target: "B", elapsed_seconds: 19,
        standard_deviation_distance: 4.5 }] },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toContain("/api/streaming-temporal?");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestStreamingTemporal(new Blob(["case_id,activity,timestamp\nc1,A,2024-01-01T00:00:00Z\nc1,B,2024-01-01T00:00:20Z\n"]), options))
      .resolves.toMatchObject({ family: "STREAMING_TEMPORAL", total_deviations: 1 });
    vi.unstubAllGlobals();
  });
});

describe("cached temporal conformance API", () => {
  const options = {
    temporal_profile: [["A", "B", 10, 2]] as Array<[string, string, number, number]>,
    temporal_variant: "dataframe" as const,
    temporal_activity_attribute: " phase ",
    temporal_case_id_attribute: " case_ref ",
    temporal_source_timestamp_attribute: " finished_at ",
    temporal_target_timestamp_attribute: " started_at ",
    temporal_business_hours: true,
    temporal_business_hour_slots: " 0-3600 ",
    temporal_strict_attributes: true,
    temporal_zeta: 0,
  };

  it("serializes the profile and all temporal options", () => {
    expect(temporalConformanceQuery(options)).toBe(
      "temporal_profile=A-%3EB%3A10%3A2&temporal_variant=dataframe&temporal_activity_attribute=phase&" +
      "temporal_case_id_attribute=case_ref&temporal_source_timestamp_attribute=finished_at&" +
      "temporal_target_timestamp_attribute=started_at&temporal_business_hour_slots=0-3600&" +
      "temporal_business_hours=true&temporal_strict_attributes=true&temporal_zeta=0",
    );
    expect(() => temporalConformanceQuery({
      temporal_profile: [["A", "B", -1, 0]],
    })).toThrow(/source\/target\/mean\/stddev/);
    expect(() => temporalConformanceQuery({
      temporal_profile: options.temporal_profile, temporal_zeta: Number.NaN,
    })).toThrow(/temporal_zeta must not be NaN/);
    expect(temporalConformanceQuery({
      temporal_profile: options.temporal_profile, temporal_zeta: Number.POSITIVE_INFINITY,
    })).toBe("temporal_profile=A-%3EB%3A10%3A2&temporal_zeta=Infinity");
    expect(temporalConformanceQuery({
      temporal_profile: options.temporal_profile, temporal_zeta: Number.NEGATIVE_INFINITY,
    })).toBe("temporal_profile=A-%3EB%3A10%3A2&temporal_zeta=-Infinity");
    expect(temporalConformanceQuery({
      temporal_profile: options.temporal_profile, temporal_variant: "log",
    })).toBe("temporal_profile=A-%3EB%3A10%3A2");
    expect(() => temporalConformanceQuery({
      temporal_profile: options.temporal_profile, temporal_variant: "other" as never,
    })).toThrow(/temporal_variant must be log or dataframe/);
  });

  it("requests and validates grouped cached-session conformance", async () => {
    const payload = {
      family: "TEMPORAL_PROFILE_CONFORMANCE", trace_count: 2,
      deviation_count: 1,
      traces: [[{ case_id: "c1", source: "A", target: "B", elapsed_seconds: 19,
        standard_deviation_distance: 4.5 }], []],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toContain("/api/temporal-conformance?");
      expect(init?.method).toBe("POST");
      expect(init?.body).toBeUndefined();
      return { ok: true, json: async () => payload };
    }));
    await expect(requestTemporalConformance(options)).resolves.toMatchObject({
      family: "TEMPORAL_PROFILE_CONFORMANCE", trace_count: 2, deviation_count: 1,
    });
    vi.unstubAllGlobals();
  });
});

describe("streaming token replay API", () => {
  it("serializes bounded variant settings", () => {
    expect(streamingTbrQuery({ streaming_tbr_variant: "backwards", streaming_tbr_max_invisible: 4, streaming_tbr_stop_immediately: true }))
      .toBe("streaming_tbr_variant=backwards&streaming_tbr_max_invisible=4&streaming_tbr_stop_immediately=true");
    expect(() => streamingTbrQuery({ streaming_tbr_max_invisible: -1 })).toThrow(/non-negative/);
  });
  it("uploads model and log and validates snapshots", async () => {
    const payload = {
      family: "STREAMING_TBR", variant: "FORWARD", total_events_processed: 1,
      total_missing: 0, cases: { c1: { events: 1, missing: 0, remaining: 0,
        is_fit: true, marking: [0, 1] } },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/streaming-tbr?streaming_tbr_variant=forward");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestStreamingTbr(new Blob(["<pnml/>"]), new Blob(["case_id,activity\nc1,A\n"]), {
      streaming_tbr_variant: "forward",
    })).resolves.toMatchObject({ family: "STREAMING_TBR", variant: "FORWARD" });
    vi.unstubAllGlobals();
  });
});

describe("chaotic-activity statistics API", () => {
  it("serializes bounded entropy controls", () => {
    expect(chaoticActivitiesQuery({
      chaotic_activity_key: "phase",
      chaotic_alpha: 0.25,
      chaotic_max_activities: 8,
      chaotic_max_events: 100,
    })).toBe("chaotic_activity_key=phase&chaotic_alpha=0.25&chaotic_max_activities=8&chaotic_max_events=100");
    expect(() => chaoticActivitiesQuery({ chaotic_alpha: -1 })).toThrow(/non-negative/);
    expect(() => chaoticActivitiesQuery({ chaotic_max_events: 0 })).toThrow(/positive integer/);
  });

  it("posts and validates the deterministic result envelope", async () => {
    const payload = {
      family: "CHAOTIC_ACTIVITIES", variant: "NIEK_SIDOROVA", alpha: 0.5,
      activities: [{ activity: "A", frequency: 2, entropy: 0,
        entropy_smooth: 1, entropy_gain: 0.25, chaotic_score: 0.625 }],
      diagnostics: { activity_count: 1, event_count: 2,
        output_truncated: false, message: "ok" },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/chaotic-activities?chaotic_alpha=0.5");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestChaoticActivities({ chaotic_alpha: 0.5 }))
      .resolves.toMatchObject({ family: "CHAOTIC_ACTIVITIES" });
    vi.unstubAllGlobals();
  });
});

describe("passed-time statistics API", () => {
  it("serializes the selected variant and target", () => {
    expect(passedTimeQuery({
      passed_time_activity: "B",
      passed_time_variant: "prepost",
      passed_time_activity_key: "phase",
      passed_time_aggregation: "median",
      passed_time_business_hours: true,
    })).toBe("passed_time_activity=B&passed_time_variant=prepost&passed_time_activity_key=phase&passed_time_aggregation=median&passed_time_business_hours=true");
    expect(() => passedTimeQuery({ passed_time_activity: "" })).toThrow(/must not be empty/);
    expect(() => passedTimeQuery({ passed_time_variant: "bad" as never })).toThrow(/not supported/);
    expect(() => passedTimeQuery({ passed_time_aggregation: "all" as never })).toThrow(/not supported/);
  });

  it("posts and validates the passed-time envelope", async () => {
    const payload = {
      family: "PASSED_TIME", variant: "PREPOST", activity: "B",
      pre: [{ activity: "A", performance: 1200, frequency: 2 }],
      post: [{ activity: "C", performance: 900, frequency: 2 }],
      pre_avg_perf: 1200, post_avg_perf: 900,
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/passed-time?passed_time_activity=B&passed_time_variant=prepost");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestPassedTime({ passed_time_activity: "B", passed_time_variant: "prepost" }))
      .resolves.toMatchObject({ family: "PASSED_TIME", variant: "PREPOST" });
    vi.unstubAllGlobals();
  });
});

describe("service-time statistics API", () => {
  it("serializes timestamp and aggregation controls", () => {
    expect(serviceTimeQuery({
      service_time_start_timestamp_key: "started_at",
      service_time_timestamp_key: "finished_at",
      service_time_aggregation: "sum",
      service_time_business_hours: true,
    })).toBe("service_time_start_timestamp_key=started_at&service_time_timestamp_key=finished_at&service_time_aggregation=sum&service_time_business_hours=true");
  });

  it("posts and validates the service-time envelope", async () => {
    const payload = {
      family: "SERVICE_TIME", variant: "CLASSIC", aggregation: "MEAN",
      values: { A: 900, B: 2100 },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/service-time?service_time_aggregation=sum");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestServiceTime({ service_time_aggregation: "sum" }))
      .resolves.toMatchObject({ family: "SERVICE_TIME", variant: "CLASSIC" });
    vi.unstubAllGlobals();
  });
});

describe("rework statistics API", () => {
  it("serializes the selected rework variant", () => {
    expect(reworkQuery({ rework_variant: "cases", rework_case_id_key: "region" }))
      .toBe("rework_variant=cases&rework_case_id_key=region");
    expect(() => reworkQuery({ rework_variant: "other" as never })).toThrow(/not supported/);
  });

  it("posts and validates activity rework", async () => {
    const payload = { family: "REWORK", variant: "ACTIVITIES", activity_cases: { A: 2 } };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/rework?rework_variant=activities");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestRework({ rework_variant: "activities" }))
      .resolves.toMatchObject({ family: "REWORK", variant: "ACTIVITIES" });
    vi.unstubAllGlobals();
  });
});

describe("cycle-time statistics API", () => {
  it("serializes timestamp keys", () => {
    expect(cycleTimeQuery({
      cycle_time_timestamp_key: "finished_at",
      cycle_time_start_timestamp_key: "started_at",
    })).toBe("cycle_time_timestamp_key=finished_at&cycle_time_start_timestamp_key=started_at");
    expect(() => cycleTimeQuery({ cycle_time_timestamp_key: "" })).toThrow(/must not be empty/);
  });

  it("posts and validates the cycle-time envelope", async () => {
    const payload = {
      family: "CYCLE_TIME", variant: "CLASSIC",
      cycle_time: 10, instances: 2, timed_events: 4,
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/cycle-time?cycle_time_timestamp_key=finished_at");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestCycleTime({ cycle_time_timestamp_key: "finished_at" }))
      .resolves.toMatchObject({ family: "CYCLE_TIME", variant: "CLASSIC" });
    vi.unstubAllGlobals();
  });
});

describe("case-arrival statistics API", () => {
  it("serializes timestamp and business-hour settings", () => {
    expect(caseArrivalQuery({
      case_arrival_timestamp_key: "occurred_at",
      case_arrival_business_hours: true,
      case_arrival_business_hour_slots: "28800-43200",
    })).toBe("case_arrival_timestamp_key=occurred_at&case_arrival_business_hour_slots=28800-43200&case_arrival_business_hours=true");
    expect(() => caseArrivalQuery({ case_arrival_business_hours: "true" as never })).toThrow(/must be boolean/);
  });

  it("posts and validates the case-arrival envelope", async () => {
    const payload = {
      family: "CASE_ARRIVAL", variant: "CLASSIC",
      arrival_average: 7200, dispersion_average: 7200,
      timed_starts: 3, timed_ends: 3,
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/case-arrival?case_arrival_business_hours=true");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestCaseArrival({ case_arrival_business_hours: true }))
      .resolves.toMatchObject({ family: "CASE_ARRIVAL", variant: "CLASSIC" });
    vi.unstubAllGlobals();
  });
});

describe("case statistics API", () => {
  it("serializes sorting and bounds", () => {
    expect(caseStatisticsQuery({
      case_statistics_case_id_key: "case:concept:name",
      case_statistics_timestamp_key: "occurred_at",
      case_statistics_sort_by: "duration",
      case_statistics_sort_ascending: false,
      case_statistics_max_cases: 2,
      case_statistics_business_hours: true,
      case_statistics_business_hour_slots: "28800-43200",
    })).toBe("case_statistics_case_id_key=case%3Aconcept%3Aname&case_statistics_timestamp_key=occurred_at&case_statistics_sort_by=duration&case_statistics_business_hour_slots=28800-43200&case_statistics_sort_ascending=false&case_statistics_business_hours=true&case_statistics_max_cases=2");
    expect(() => caseStatisticsQuery({ case_statistics_max_cases: -1 })).toThrow(/non-negative/);
    expect(() => caseStatisticsQuery({ case_statistics_sort_by: "other" as never })).toThrow(/not supported/);
  });

  it("posts and validates case rows", async () => {
    const payload = {
      family: "CASE_STATISTICS", variant: "CLASSIC",
      case_id_key: "case_id", timestamp_key: "timestamp",
      enable_sort: true, sort_by: "CASE_ID", sort_ascending: true,
      max_cases: 0, use_business_hours: false,
      cases: [{ case_id: "c1", start_time: 1, end_time: 3, case_duration: 2 }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/case-statistics?case_statistics_max_cases=1");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestCaseStatistics({ case_statistics_max_cases: 1 }))
      .resolves.toMatchObject({ family: "CASE_STATISTICS", variant: "CLASSIC" });
    vi.unstubAllGlobals();
  });
});

describe("attribute statistics API", () => {
  it("serializes scope and once-per-case settings", () => {
    expect(attributeStatisticsQuery({
      attribute_statistics_key: "activity",
      attribute_statistics_scope: "events",
      attribute_statistics_keep_once_per_case: true,
    })).toBe("attribute_statistics_key=activity&attribute_statistics_scope=events&attribute_statistics_keep_once_per_case=true");
    expect(() => attributeStatisticsQuery({ attribute_statistics_scope: "other" as never })).toThrow(/not supported/);
  });

  it("posts and validates attribute counts", async () => {
    const payload = {
      family: "ATTRIBUTE_STATISTICS", scope: "TRACES",
      attribute_key: "region", keep_once_per_case: false,
      values: { north: 1, south: 1 },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/attribute-statistics?attribute_statistics_key=region&attribute_statistics_scope=traces");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestAttributeStatistics({
      attribute_statistics_key: "region", attribute_statistics_scope: "traces",
    })).resolves.toMatchObject({ family: "ATTRIBUTE_STATISTICS", scope: "TRACES" });
    vi.unstubAllGlobals();
  });
});

describe("attribute distribution API", () => {
  it("serializes distribution and timestamp settings", () => {
    expect(attributeDistributionQuery({
      attribute_distribution_type: "days_week",
      attribute_distribution_timestamp_key: "occurred_at",
    })).toBe("attribute_distribution_type=days_week&attribute_distribution_timestamp_key=occurred_at");
    expect(() => attributeDistributionQuery({ attribute_distribution_type: "bad" as never })).toThrow(/not supported/);
    expect(attributeDistributionQuery({ attribute_distribution_timestamp_key: "" }))
      .toBe("attribute_distribution_timestamp_key=");
  });

  it("posts and validates calendar bins", async () => {
    const payload = {
      family: "ATTRIBUTE_DISTRIBUTION", variant: "DAYS_WEEK",
      timestamp_key: "timestamp", labels: ["Monday"], counts: [2],
      timed_events: 2,
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/attribute-distribution?attribute_distribution_type=days_week");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestAttributeDistribution({ attribute_distribution_type: "days_week" }))
      .resolves.toMatchObject({ family: "ATTRIBUTE_DISTRIBUTION", variant: "DAYS_WEEK" });
    vi.unstubAllGlobals();
  });
});

describe("attribute KDE API", () => {
  it("serializes variant and bounded sampling settings", () => {
    expect(attributeKdeQuery({
      attribute_kde_key: "amount",
      attribute_kde_variant: "date",
      attribute_kde_graph_points: 17,
      attribute_kde_points_to_sample: 31,
    })).toBe("attribute_kde_key=amount&attribute_kde_variant=date&attribute_kde_graph_points=17&attribute_kde_points_to_sample=31");
    expect(() => attributeKdeQuery({ attribute_kde_variant: "other" as never })).toThrow(/not supported/);
    expect(() => attributeKdeQuery({ attribute_kde_graph_points: 0 })).toThrow(/positive integer/);
  });

  it("posts and validates numeric density points", async () => {
    const payload = {
      family: "ATTRIBUTE_KDE", variant: "NUMERIC", attribute_key: "amount",
      x: [1, 2], y: [0.25, 0.5],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/attribute-kde?attribute_kde_key=amount");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestAttributeKde({ attribute_kde_key: "amount" }))
      .resolves.toMatchObject({ family: "ATTRIBUTE_KDE", variant: "NUMERIC" });
    vi.unstubAllGlobals();
  });
});

describe("boundary-activities statistics API", () => {
  it("serializes variant and activity key", () => {
    expect(boundaryActivitiesQuery({
      boundary_activities_variant: "start",
      boundary_activities_activity_key: "phase",
    })).toBe("boundary_activities_variant=start&boundary_activities_activity_key=phase");
    expect(() => boundaryActivitiesQuery({ boundary_activities_variant: "other" as never })).toThrow(/not supported/);
  });

  it("posts and validates start/end counts", async () => {
    const payload = {
      family: "BOUNDARY_ACTIVITIES", variant: "BOTH",
      starts: { A: 2 }, ends: { B: 1 },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/boundary-activities?boundary_activities_variant=both");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestBoundaryActivities({ boundary_activities_variant: "both" }))
      .resolves.toMatchObject({ family: "BOUNDARY_ACTIVITIES", variant: "BOTH" });
    vi.unstubAllGlobals();
  });
});

describe("variant statistics API", () => {
  it("serializes custom keys and limits", () => {
    expect(variantStatisticsQuery({
      variant_statistics_activity_key: "phase",
      variant_statistics_max_variants: 3,
    })).toBe("variant_statistics_activity_key=phase&variant_statistics_max_variants=3");
    expect(() => variantStatisticsQuery({ variant_statistics_max_variants: -1 })).toThrow(/non-negative/);
  });

  it("posts and validates variant probabilities", async () => {
    const payload = {
      family: "VARIANT_STATISTICS", variant: "CLASSIC", activity_key: "activity",
      variants: [{ activities: ["A", "B"], count: 2, probability: 1 }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/variant-statistics?variant_statistics_max_variants=1");
      expect(init?.method).toBe("POST");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestVariantStatistics({ variant_statistics_max_variants: 1 }))
      .resolves.toMatchObject({ family: "VARIANT_STATISTICS", variant: "CLASSIC" });
    vi.unstubAllGlobals();
  });
});

describe("DFG import API", () => {
  it("posts DFG text for server-side validation and normalization", async () => {
    const text = "2\nA\nB\n1\n0x1\n1\n1x1\n0>1x1\n";
    const fetchMock = vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/dfg");
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({ "Content-Type": "text/plain" });
      expect(init?.body).toBeInstanceOf(Blob);
      expect(await (init?.body as Blob).text()).toBe(text);
      return { ok: true, text: async () => text };
    });
    vi.stubGlobal("fetch", fetchMock);
    await expect(importDfg(new Blob([text], { type: "text/plain" }))).resolves.toBe(text);
    vi.unstubAllGlobals();
  });
});

describe("DFG-to-Petri conversion API", () => {
  it("serializes the selected construction and validates the JSON net", async () => {
    const text = "2\nA\nB\n1\n0x1\n1\n1x1\n0>1x1\n";
    const payload = {
      family: "DFG_CONVERSION", variant: "INVISIBLES_NO_DUPLICATES",
      source: 4, sink: 7, activities: ["A", "B"], place_count: 8,
      transitions: [{ activity: 0, inputs: [0], outputs: [1] }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/dfg-to-petri?variant=INVISIBLES_NO_DUPLICATES&artificial_start_activity=%40%40s%40%40");
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({ "Content-Type": "text/plain" });
      expect(await (init?.body as Blob).text()).toBe(text);
      return { ok: true, json: async () => payload };
    }));
    await expect(convertDfgToPetriNet(new Blob([text], { type: "text/plain" }), {
      variant: "INVISIBLES_NO_DUPLICATES", artificial_start_activity: "@@s@@",
    })).resolves.toMatchObject({ family: "DFG_CONVERSION" });
    expect(() => dfgPetriConversionQuery({ artificial_end_activity: " " })).toThrow(/must not be empty/);
    vi.unstubAllGlobals();
  });
});

describe("trie-to-Petri conversion API", () => {
  it("requests the cached conversion with trie controls", async () => {
    const payload = {
      family: "TRIE_CONVERSION", variant: "TO_PETRI_NET", source: 0,
      sink: 1, activities: ["A"], place_count: 2,
      transitions: [{ activity: 0, inputs: [0], outputs: [1] }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/trie-to-petri?trie_activity_key=concept%3Aname&trie_max_path_length=2");
      return { ok: true, json: async () => payload };
    }));
    await expect(requestTriePetriNet({ trie_activity_key: "concept:name", trie_max_path_length: 2 }))
      .resolves.toMatchObject({ family: "TRIE_CONVERSION" });
    vi.unstubAllGlobals();
  });
});

describe("Monte Carlo API", () => {
  it("accepts shifted and zero-scale lognormal/gamma services", () => {
    for (const distribution of ["lognormal", "gamma"] as const) {
      const query = new URLSearchParams(monteCarloQuery({ transition_durations: [
        { distribution, value_seconds: 2, secondary_seconds: -1, tertiary_seconds: 3 },
        { distribution, value_seconds: 2, secondary_seconds: 3, tertiary_seconds: 0 },
      ] }));
      expect(query.get("monte_carlo_transition_durations"))
        .toBe(`${distribution}:2;-1;3,${distribution}:2;3;0`);
      for (const [shape, location, scale] of [[0, 0, 0], [1, -1, 0], [1, 0, -1]]) {
        expect(() => monteCarloQuery({ transition_durations: [
          { distribution, value_seconds: shape, secondary_seconds: location, tertiary_seconds: scale },
        ] })).toThrow(/valid distribution parameters/);
      }
    }
    const record = { transition: 0, timing: "timed" as const, distribution: "gamma" as const,
      parameter: 0, secondary_parameter: 2, tertiary_parameter: 0, activation_weight: 1, priority: 0 };
    expect(new URLSearchParams(monteCarloQuery({ provided_stochastic_map: [record] }))
      .get("monte_carlo_provided_stochastic_map")).toBe("0:timed:gamma:0:2:0:1:0");
    expect(() => monteCarloQuery({ provided_stochastic_map: [{ ...record, secondary_parameter: 0 }] }))
      .toThrow(/shape positive/);
  });

  it("accepts signed service laws with nonnegative support", () => {
    const query = new URLSearchParams(monteCarloQuery({ transition_durations: [
      { distribution: "normal", value_seconds: -2, secondary_seconds: 1 },
      { distribution: "uniform", value_seconds: -2, secondary_seconds: 3 },
      { distribution: "normal", value_seconds: 0, secondary_seconds: 0 },
      { distribution: "uniform", value_seconds: 0, secondary_seconds: 0 },
    ] }));
    expect(query.get("monte_carlo_transition_durations"))
      .toBe("normal:-2;1,uniform:-2;3,normal:0;0,uniform:0;0");
    for (const distribution of ["normal", "uniform"] as const) {
      expect(() => monteCarloQuery({ transition_durations: [
        { distribution, value_seconds: -2, secondary_seconds: 0 },
      ] })).toThrow(/valid distribution parameters/);
    }
    for (const [lower, width] of [[-2, 2], [-2, -1], [1e308, 1e308]]) {
      expect(() => monteCarloQuery({ transition_durations: [
        { distribution: "uniform", value_seconds: lower, secondary_seconds: width },
      ] })).toThrow(/valid distribution parameters/);
    }
  });

  it("serializes bounded virtual-time controls", () => {
    expect(monteCarloQuery({
      simulations: 4,
      case_arrival_seconds: 2.5,
      max_steps: 8,
      seed: 7,
      default_duration_seconds: 3,
      force_distribution: "exponential",
      transition_weights: [0.25, 1],
      transition_durations: [
        { distribution: "deterministic", value_seconds: 1 },
        { distribution: "exponential", value_seconds: 2 },
        { distribution: "normal", value_seconds: 3, secondary_seconds: 1 },
        { distribution: "lognormal", value_seconds: 0.5, secondary_seconds: 0, tertiary_seconds: 1 },
      ],
      resources_per_place: { 0: 2, 3: 4 },
      enable_diagnostics: false,
    })).toBe("monte_carlo_simulations=4&monte_carlo_max_steps=8&monte_carlo_seed=7&monte_carlo_case_arrival_seconds=2.5&monte_carlo_default_duration_seconds=3&monte_carlo_force_distribution=exponential&monte_carlo_transition_weights=0.25%2C1&monte_carlo_transition_durations=deterministic%3A1%2Cexponential%3A2%2Cnormal%3A3%3B1%2Clognormal%3A0.5%3B0%3B1&monte_carlo_resources_per_place=0%3D2%2C3%3D4&monte_carlo_enable_diagnostics=false");
  });

  it("serializes exponential locations and preserves the single-scale form", () => {
    const query = new URLSearchParams(monteCarloQuery({ transition_durations: [
      { distribution: "exponential", value_seconds: 2 },
      { distribution: "exponential", value_seconds: 2, secondary_seconds: -3 },
      { distribution: "exponential", value_seconds: 1, secondary_seconds: 2 },
      { distribution: "exponential", value_seconds: 0, secondary_seconds: 3 },
      { distribution: "exponential", value_seconds: 0, secondary_seconds: 0 },
    ] }));
    expect(query.get("monte_carlo_transition_durations"))
      .toBe("exponential:2,exponential:2;-3,exponential:1;2,exponential:0;3,exponential:0;0");
    for (const [scale, location] of [[-1, 0], [0, -1], [1, Infinity], [1, NaN]]) {
      expect(() => monteCarloQuery({ transition_durations: [
        { distribution: "exponential", value_seconds: scale, secondary_seconds: location },
      ] })).toThrow(/valid distribution parameters/);
    }
  });

  it("passes finite signed Monte Carlo weights to runtime selection", () => {
    for (const weights of [[-1, -3], [-1, 0], [-1, 1], [-1, 2], [0, 0]]) {
      expect(new URLSearchParams(monteCarloQuery({ transition_weights: weights }))
        .get("monte_carlo_transition_weights")).toBe(weights.join(","));
    }
    for (const weight of [Infinity, -Infinity, NaN]) {
      expect(() => monteCarloQuery({ transition_weights: [1, weight] }))
        .toThrow(/finite numbers/);
    }
  });

  it("accepts finite signed map weights alongside explicit overrides", () => {
    const transition = { transition: 0, timing: "timed" as const,
      distribution: "normal" as const, parameter: 0, secondary_parameter: 0,
      tertiary_parameter: 0, activation_weight: -1, priority: 0 };
    const query = new URLSearchParams(monteCarloQuery({
      provided_stochastic_map: [transition], transition_weights: [1, 3],
    }));
    expect(query.get("monte_carlo_provided_stochastic_map"))
      .toBe("0:timed:normal:0:0:0:-1:0");
    expect(query.get("monte_carlo_transition_weights")).toBe("1,3");
    expect(() => monteCarloQuery({ provided_stochastic_map: [
      { ...transition, activation_weight: NaN },
    ] })).toThrow(/invalid numeric fields/);
  });

  it("serializes selectors, markings, sparse maps, and compatibility controls", () => {
    const query = new URLSearchParams(monteCarloQuery({
      case_arrival_ratio: 15,
      activity_key: " custom:name ", timestamp_key: "custom:time",
      initial_marking: [1, 0, 2], final_marking: [0, 1, 0],
      token_replay_variant: "backwards", diagnostic_interval: 16,
      small_scale_factor: 1000, max_thread_execution_time: 30,
      provided_stochastic_map: [
        { transition: 1, timing: "timed", distribution: "deterministic",
          parameter: 4, secondary_parameter: 0, tertiary_parameter: 0,
          activation_weight: 2, priority: 0 },
        { transition: 0, timing: "immediate", distribution: "immediate",
          parameter: 0, secondary_parameter: 0, tertiary_parameter: 0,
          activation_weight: 1, priority: 3 },
      ],
    }));
    expect(Object.fromEntries(query)).toEqual({
      monte_carlo_case_arrival_ratio: "15",
      monte_carlo_small_scale_factor: "1000",
      monte_carlo_max_thread_execution_time: "30",
      monte_carlo_activity_key: "custom:name",
      monte_carlo_timestamp_key: "custom:time",
      monte_carlo_initial_marking: "1,0,2",
      monte_carlo_final_marking: "0,1,0",
      monte_carlo_token_replay_variant: "backwards",
      monte_carlo_diagnostic_interval: "16",
      monte_carlo_provided_stochastic_map:
        "0:immediate:immediate:0:0:0:1:3,1:timed:deterministic:4:0:0:2:0",
    });
    expect(new URLSearchParams(monteCarloQuery({ provided_stochastic_map: [] }))
      .get("monte_carlo_provided_stochastic_map")).toBe("empty");
  });

  it("rejects conflicting and malformed extended controls", () => {
    expect(() => monteCarloQuery({ case_arrival_ratio: 1, case_arrival_seconds: 1 }))
      .toThrow(/supply only one/);
    expect(() => monteCarloQuery({ initial_marking: [] })).toThrow(/non-empty/);
    expect(() => monteCarloQuery({ final_marking: [0x1_0000_0000] })).toThrow(/uint32/);
    expect(() => monteCarloQuery({ activity_key: " " })).toThrow(/must not be empty/);
    expect(() => monteCarloQuery({ diagnostic_interval: 0 })).toThrow(/positive/);
    expect(() => monteCarloQuery({ small_scale_factor: 0 })).toThrow(/positive/);
    const transition = { transition: 0, timing: "timed" as const,
      distribution: "exponential" as const, parameter: 0,
      secondary_parameter: 0, tertiary_parameter: 0,
      activation_weight: 1, priority: 0 };
    expect(() => monteCarloQuery({ provided_stochastic_map: [transition] }))
      .toThrow(/rates must be positive/);
    expect(() => monteCarloQuery({ provided_stochastic_map: [
      { ...transition, parameter: 1 }, { ...transition, parameter: 2 },
    ] })).toThrow(/unique/);
    expect(() => monteCarloQuery({ provided_stochastic_map: [
      { ...transition, priority: -2147483649 },
    ] })).toThrow(/invalid numeric fields/);
    expect(() => monteCarloQuery({ provided_stochastic_map: [
      { ...transition, priority: 2147483648 },
    ] })).toThrow(/invalid numeric fields/);
  });

  it("accepts the signed 32-bit priority boundaries", () => {
    const transition = { transition: 0, timing: "immediate" as const,
      distribution: "immediate" as const, parameter: 0,
      secondary_parameter: 0, tertiary_parameter: 0,
      activation_weight: 1, priority: -2147483648 };
    expect(new URLSearchParams(monteCarloQuery({
      provided_stochastic_map: [transition],
    })).get("monte_carlo_provided_stochastic_map")).toBe(
      "0:immediate:immediate:0:0:0:1:-2147483648",
    );
    expect(new URLSearchParams(monteCarloQuery({
      provided_stochastic_map: [{ ...transition, priority: 2147483647 }],
    })).get("monte_carlo_provided_stochastic_map")).toBe(
      "0:immediate:immediate:0:0:0:1:2147483647",
    );
  });

  it("posts every ambiguous Monte Carlo control explicitly", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/playout");
      expect(path.length).toBeLessThan(8192);
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({
        "Content-Type": "application/x-www-form-urlencoded",
      });
      const body = new URLSearchParams(String(init?.body));
      expect(body.get("miner")).toBe("inductive");
      expect(body.get("mode")).toBe("monte_carlo");
      expect(body.get("monte_carlo_activity_key")).toBe("custom:activity");
      expect(body.get("monte_carlo_timestamp_key")).toBe("custom:time");
      expect(body.get("monte_carlo_initial_marking")).toBe("1,0");
      expect(body.get("monte_carlo_final_marking")).toBe("0,1");
      expect(body.get("monte_carlo_stop_at_final")).toBe("true");
      expect(body.get("monte_carlo_token_replay_variant")).toBe("backwards");
      expect(body.get("monte_carlo_case_arrival_ratio")).toBe("3");
      expect(body.get("monte_carlo_enable_diagnostics")).toBe("false");
      return { ok: true, text: async () => "<log/>" };
    }));
    await expect(fetchMonteCarloPlayout("inductive", {
      activity_key: "custom:activity", timestamp_key: "custom:time",
      initial_marking: [1, 0], final_marking: [0, 1], stop_at_final: true,
      token_replay_variant: "backwards", case_arrival_ratio: 3,
      enable_diagnostics: false,
    })).resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });

  it("keeps the legacy Monte Carlo discriminator on the explicit POST path", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/playout");
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({
        "Content-Type": "application/x-www-form-urlencoded",
      });
      const body = new URLSearchParams(String(init?.body));
      expect(body.get("miner")).toBe("inductive");
      expect(body.get("mode")).toBe("monte_carlo");
      expect(body.get("monte_carlo_simulations")).toBe("2");
      expect(body.get("monte_carlo_case_arrival_seconds")).toBe("3");
      expect(body.get("monte_carlo_provided_stochastic_map")).toBe("empty");
      return { ok: true, text: async () => "<log/>" };
    }));
    await expect(fetchPlayout("monte_carlo", {
      simulations: 2,
      case_arrival_seconds: 3,
      provided_stochastic_map: [],
    })).resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });

  it("keeps Monte Carlo-only controls out of ordinary playout dispatch", async () => {
    const fetchMock = vi.fn();
    vi.stubGlobal("fetch", fetchMock);
    await expect(fetchPlayout("inductive", { simulations: 1 } as never))
      .rejects.toThrow(/fetchMonteCarloPlayout/);
    await expect(fetchPlayout("inductive", { case_arrival_seconds: 3 } as never))
      .rejects.toThrow(/fetchMonteCarloPlayout/);
    await expect(fetchPlayout("inductive", { provided_stochastic_map: [] } as never))
      .rejects.toThrow(/fetchMonteCarloPlayout/);
    expect(fetchMock).not.toHaveBeenCalled();
    vi.unstubAllGlobals();
  });

  it("keeps a large supplied stochastic map in the POST body", async () => {
    const provided_stochastic_map = Array.from({ length: 500 }, (_, transition) => ({
      transition, timing: "immediate" as const, distribution: "immediate" as const,
      parameter: 0, secondary_parameter: 0, tertiary_parameter: 0,
      activation_weight: 1, priority: 0,
    }));
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/playout");
      expect(path.length).toBeLessThan(8192);
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({
        "Content-Type": "application/x-www-form-urlencoded",
      });
      const body = new URLSearchParams(String(init?.body));
      const encodedMap = body.get("monte_carlo_provided_stochastic_map");
      expect(encodedMap).not.toBeNull();
      expect(encodedMap!.length).toBeGreaterThan(8192);
      expect(encodedMap!.split(",")).toHaveLength(provided_stochastic_map.length);
      expect(body.get("monte_carlo_simulations")).toBe("2");
      return { ok: true, text: async () => "<log/>" };
    }));
    await expect(fetchMonteCarloPlayout("inductive", {
      simulations: 2, provided_stochastic_map,
    })).resolves.toBe("<log/>");
    vi.unstubAllGlobals();
  });

  it("validates simulation results", async () => {
    const result = {
      family: "MONTE_CARLO", variant: "PETRI_SEMAPH_FIFO",
      log: { trace_count: 2, event_count: 3 },
      diagnostics: { simulated_cases: 2, completed_cases: 2, deadlocked_cases: 0, step_limited_cases: 0, case_arrival_seconds: 1, total_cases_seconds: 3, median_case_execution_seconds: 1, case_execution_seconds: [1, 1], case_arrival_ratio: 1, total_cases_time: 3, median_cases_ex_time: 1, cases_ex_time: [1, 1] },
      output_places_interval_trees: [{ place_index: 0, intervals: [{ start: 1, end: 2 }] }],
      output_transitions_interval_trees: [{ transition_index: 0, intervals: [] }],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/monte-carlo");
      expect(init?.method).toBe("POST");
      expect(init?.headers).toEqual({
        "Content-Type": "application/x-www-form-urlencoded",
      });
      const body = new URLSearchParams(String(init?.body));
      expect(body.get("miner")).toBe("inductive");
      expect(body.get("monte_carlo_simulations")).toBe("2");
      return { ok: true, json: async () => result };
    }));
    await expect(fetchMonteCarloResult("inductive", { simulations: 2 })).resolves.toEqual(result);
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ ...result, diagnostics: { ...result.diagnostics, case_execution_seconds: ["bad"] } }) })));
    await expect(fetchMonteCarloResult()).rejects.toThrow(/invalid Monte Carlo/);
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ ...result, output_places_interval_trees: [{ place_index: 1, intervals: [] }] }) })));
    await expect(fetchMonteCarloResult()).rejects.toThrow(/invalid Monte Carlo/);
    vi.unstubAllGlobals();
  });
});

describe("log-to-features API", () => {
  it("preserves trace empty groups and exact schema elements", () => {
    const query = new URLSearchParams(logToFeaturesQuery({ features_case_string_attributes: [], features_case_numeric_attributes: [], features_event_successor_attributes: [], features_feature_names: ["", " event:n "] }));
    expect(query.get("features_case_string_attributes")).toBe("");
    expect(query.get("features_case_numeric_attributes")).toBe("");
    expect(query.get("features_event_successor_attributes")).toBe("");
    expect(query.get("features_feature_names")).toBe("\\e, event:n ");
  });
  it("preserves exact event schema elements", () => {
    const query = new URLSearchParams(logToFeaturesQuery({ features_variant: "event_based", features_feature_names: ["", " event:n ", "a,b", "\\e"] }));
    expect(query.get("features_feature_names")).toBe("\\e, event:n ,a\\,b,\\\\e");
  });
  it("preserves explicitly empty event lists and schemas", () => {
    expect(logToFeaturesQuery({})).toBe("");
    expect(logToFeaturesQuery({ features_event_string_attributes: [], features_event_numeric_attributes: [], features_feature_names: [], features_variant: "event_based" }))
      .toBe("features_event_numeric_attributes=&features_event_string_attributes=&features_feature_names=&features_variant=event_based");
  });
  it("encodes deterministic sorted options and escaped lists", () => {
    expect(logToFeaturesQuery({ features_event_attributes: ["a,b", "x"], features_variant: "event_based" }))
      .toBe("features_event_attributes=a%5C%2Cb%2Cx&features_variant=event_based");
  });

  it("preserves explicit false extra overrides and ignored trace cardinality bounds", () => {
    expect(logToFeaturesQuery({ features_enable_all_extra_features: true,
      features_enable_case_duration: false, features_enable_resource_workload: false,
      features_min_num_different_string_values: 3, features_max_num_different_string_values: 2 }))
      .toBe("features_enable_all_extra_features=true&features_enable_case_duration=false&features_enable_resource_workload=false&features_max_num_different_string_values=2&features_min_num_different_string_values=3");
  });

  it("requests a custom typed case identifier column", async () => {
    expect(logToFeaturesQuery({ features_case_id_key: "custom:id", features_add_case_identifier_column: true }))
      .toBe("features_add_case_identifier_column=true&features_case_id_key=custom%3Aid");
    const result = { family: "LOG_TO_FEATURES", variant: "trace_based", feature_names: ["@@case_id_column", "event:n"], trace_rows: [["c1", 2], [null, 3]] };
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => result })));
    await expect(requestLogToFeatures()).resolves.toEqual(result);
    vi.unstubAllGlobals();
  });

  it("allows ignored calendar-temporal selectors while preserving fixed-window validation", () => {
    expect(logToFeaturesQuery({ features_variant: "temporal", features_event_string_attributes: ["", ""], features_case_attributes: [" "] }))
      .toBe("features_case_attributes=+&features_event_string_attributes=%2C&features_variant=temporal");
    expect(logToFeaturesQuery({ features_variant: "temporal", features_feature_names: [], features_event_string_attributes: [], features_min_num_different_string_values: 3, features_max_num_different_string_values: 2 }))
      .toBe("features_event_string_attributes=&features_feature_names=&features_max_num_different_string_values=2&features_min_num_different_string_values=3&features_variant=temporal");
    expect(() => logToFeaturesQuery({ features_variant: "temporal", features_window_seconds: 60, features_min_num_different_string_values: 3, features_max_num_different_string_values: 2 })).toThrow(/bounds/);
  });

  it("serializes temporal metric destinations including an empty column name", () => {
    expect(logToFeaturesQuery({ features_variant: "temporal", features_arrival_rate: "arrival", features_finish_rate: "finish", features_service_time: "", features_waiting_time: "wait", features_sojourn_time: "sojourn", features_diff_start_end: "diff" }))
      .toBe("features_arrival_rate=arrival&features_diff_start_end=diff&features_finish_rate=finish&features_service_time=&features_sojourn_time=sojourn&features_variant=temporal&features_waiting_time=wait");
  });

  it("serializes trace representation and extra feature controls", () => {
    expect(logToFeaturesQuery({
      features_enable_all_extra_features: true,
      features_grouper_freq: "W-MON",
      features_event_successor_attributes: ["concept:name"],
      features_epsilon: 0.25,
      features_min_num_different_string_values: 1,
      features_max_num_different_string_values: 8,
    })).toBe("features_enable_all_extra_features=true&features_epsilon=0.25&features_event_successor_attributes=concept%3Aname&features_grouper_freq=W-MON&features_max_num_different_string_values=8&features_min_num_different_string_values=1");
    expect(() => logToFeaturesQuery({ features_epsilon: -1 })).toThrow(/epsilon/);
    expect(() => logToFeaturesQuery({ features_variant: "event_based", features_min_num_different_string_values: 3, features_max_num_different_string_values: 2 })).toThrow(/bounds/);
  });

  it("rejects malformed backend payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "LOG_TO_FEATURES", variant: "trace_based", feature_names: [], trace_rows: [["bad"]] }) })));
    await expect(requestLogToFeatures()).rejects.toThrow(/invalid feature extraction/);
    vi.unstubAllGlobals();
  });

  it("accepts typed case identifiers and rejects malformed identifiers", async () => {
    const result = { family: "LOG_TO_FEATURES", variant: "trace_based", feature_names: [], case_ids: ["c1"], trace_rows: [[]] };
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => result })));
    await expect(requestLogToFeatures()).resolves.toEqual(result);
    vi.unstubAllGlobals();
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ ...result, case_ids: [1] }) })));
    await expect(requestLogToFeatures()).rejects.toThrow(/invalid feature extraction/);
    vi.unstubAllGlobals();
  });
});

describe("embedding API", () => {
  it("serializes deterministic embedding settings", () => {
    expect(embeddingsQuery({
      embedding_variant: "events_transformers", embedding_dimensions: 32,
      embedding_target: "Approve Ship", embedding_top_k: 2,
      embedding_keep_cases: true,
    })).toBe("embedding_dimensions=32&embedding_keep_cases=true&embedding_target=Approve+Ship&embedding_top_k=2&embedding_variant=events_transformers");
    expect(embeddingsQuery({ embedding_provider: "transformer", embedding_model: "mock_transformer" }))
      .toBe("embedding_model=mock_transformer&embedding_provider=transformer");
    expect(embeddingsQuery({ embedding_provider: "transformer", embedding_dimensions: 0 }))
      .toBe("embedding_dimensions=0&embedding_provider=transformer");
    expect(() => embeddingsQuery({ embedding_dimensions: 0 })).toThrow(/transformer/);
    expect(() => embeddingsQuery({ embedding_provider: "hashing", embedding_dimensions: 0 })).toThrow(/transformer/);
    expect(() => embeddingsQuery({ embedding_provider: "transformer", embedding_dimensions: 4097 })).toThrow(/4096/);
    expect(() => embeddingsQuery({ embedding_provider: "other" as never })).toThrow(/hashing or transformer/);
  });

  it("rejects malformed embedding responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "TO_EMBEDDINGS", variant: "cases_transformers", dimensions: 2, items: [{ id: "c", embedding: [1] }] }) })));
    await expect(requestEmbeddings()).rejects.toThrow(/invalid embedding result/);
    vi.unstubAllGlobals();
  });
});

describe("Petri-net reduction API", () => {
  it("encodes markings and advanced rules", () => {
    expect(petriNetReductionQuery({ initial_marking: [1, 0], final_marking: [0, 1], parallel_place_fusion: true, abstraction: false }))
      .toBe("petri_reduction_initial_marking=1%2C0&petri_reduction_final_marking=0%2C1&petri_reduction_parallel_place_fusion=true&petri_reduction_abstraction=false");
    expect(() => petriNetReductionQuery({ initial_marking: [] })).toThrow(/non-empty/);
    expect(() => petriNetReductionQuery({ final_marking: [0, -1] })).toThrow(/non-negative/);
  });

  it("posts PNML as multipart and validates the response", async () => {
    const result = { family: "PETRI_NET_REDUCTION", variant: "complete", changed: false, place_count: 2, transition_count: 1, source: 0, sink: 1, initial_marking: [1, 0], final_marking: [0, 1], old_to_new_place: [0, 1], pnml: "<pnml></pnml>" };
    const fetchMock = vi.fn(async () => ({ ok: true, json: async () => result }));
    vi.stubGlobal("fetch", fetchMock);
    await expect(reducePetriNet(new Blob(["<pnml></pnml>"]), { abstraction: true })).resolves.toEqual(result);
    expect(fetchMock).toHaveBeenCalledWith("/api/reduce-petri-net?petri_reduction_abstraction=true", expect.objectContaining({ method: "POST", body: expect.any(FormData) }));
    vi.unstubAllGlobals();
  });
});

describe("resource profiles API", () => {
  it("encodes strict fields", () => {
    expect(resourceProfilesQuery({ resource_profiles_metric: "social_position", resource_profiles_activity_attribute: "team:activity", resource_profiles_resource_attribute: "team:resource", resource_profiles_timestamp_attribute: "observed_at", resource_profiles_start_timestamp_attribute: "started_at", resource_profiles_case_id_attribute: "case:custom", resource_profiles_t1: "2024-01-01", resource_profiles_max_rows: 4 })).toBe("resource_profiles_metric=social_position&resource_profiles_activity_attribute=team%3Aactivity&resource_profiles_resource_attribute=team%3Aresource&resource_profiles_timestamp_attribute=observed_at&resource_profiles_start_timestamp_attribute=started_at&resource_profiles_case_id_attribute=case%3Acustom&resource_profiles_t1=2024-01-01&resource_profiles_max_rows=4");
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
  const validBackendResult = {
    family: "ILP",
    net: {
      place_count: 2, source: 0, sink: 1, activities: ["A"],
      transitions: [
        { activity: 0, inputs: [0], outputs: [1] },
        { activity: null, inputs: [1], outputs: [0] },
      ],
    },
    markings: { initial: [1, 0], final: [0, 1] },
    diagnostics: {
      computed: true, exhausted: false, visited_nodes: 1, lp_solved: 1,
      branch_and_bound_nodes: 1, message: "done",
    },
  };
  it("encodes strict cached-session options", () => {
    expect(ilpMinerQuery({ activity_key: "concept:name", noise_threshold: 0.2, max_nodes: 42 }))
      .toBe("ilp_activity_key=concept%3Aname&ilp_noise_threshold=0.2&ilp_max_nodes=42");
  });
  it("serializes boundary, alpha, and causal settings", () => {
    expect(ilpMinerQuery({
      artificial_start_activity: "<begin>", artificial_end_activity: "<finish>",
      alpha: 0.75, causal_relation: "<begin>=>A,A=>B,B=><finish>",
    })).toBe("ilp_artificial_start_activity=%3Cbegin%3E&ilp_artificial_end_activity=%3Cfinish%3E&ilp_alpha=0.75&ilp_causal_relation=%3Cbegin%3E%3D%3EA%2CA%3D%3EB%2CB%3D%3E%3Cfinish%3E");
    expect(() => ilpMinerQuery({ alpha: 1.1 })).toThrow(/\[0,1\]/);
    expect(() => ilpMinerQuery({ artificial_start_activity: "" })).toThrow(/must not be empty/);
  });
  it("accepts backend numeric activity indexes in responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true, json: async () => validBackendResult,
    })));
    const result = await requestIlpMine();
    expect(result.family).toBe("ILP");
    expect(result.net.transitions[0].activity).toBe(0);
    vi.unstubAllGlobals();
  });
  it("rejects visible activity indexes outside the activity table", async () => {
    const invalid = {
      ...validBackendResult,
      net: {
        ...validBackendResult.net,
        transitions: [{ activity: 1, inputs: [0], outputs: [1] }],
      },
    };
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => invalid })));
    await expect(requestIlpMine()).rejects.toThrow(/invalid ILP miner/);
    vi.unstubAllGlobals();
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
  it("encodes strict timestamp and case selectors", () => {
    expect(geneticMinerQuery({ timestamp_key: "occurred_at", case_id_key: "case:ref" }))
      .toBe("genetic_timestamp_key=occurred_at&genetic_case_id_key=case%3Aref");
    expect(() => geneticMinerQuery({ timestamp_key: "" })).toThrow(/must not be empty/);
    expect(() => geneticMinerQuery({ case_id_key: "" })).toThrow(/must not be empty/);
  });
  it("rejects malformed genetic responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "GENETIC" }) })));
    await expect(requestGeneticMine()).rejects.toThrow(/invalid genetic miner/);
    vi.unstubAllGlobals();
  });
  it("requests the explicit causal-matrix conversion selector", async () => {
    expect(geneticMatrixConversionQuery({ seed: 7, population_size: 4 }))
      .toBe("genetic_seed=7&genetic_population_size=4&genetic_conversion_variant=to_petri_net");
    expect(() => geneticMatrixConversionQuery({}, "unknown" as "to_petri_net"))
      .toThrow(/not supported/);
    const result = {
      family: "GENETIC_MATRIX_CONVERSION", variant: "TO_PETRI_NET",
      matrix: { activities: ["A"], input_map: [[]], output_map: [[]] },
      net: { place_count: 2, source: 0, sink: 1, activities: ["A"], transitions: [{ activity: 0, inputs: [0], outputs: [1] }] },
      markings: { initial: [1, 0], final: [0, 1] },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/genetic-matrix-convert?genetic_seed=7&genetic_conversion_variant=to_petri_net");
      return { ok: true, json: async () => result };
    }));
    await expect(requestGeneticMatrixConversion({ seed: 7 })).resolves.toEqual(result);
    vi.unstubAllGlobals();
  });
});

describe("organizational SNA API", () => {
  it("serializes bounded metric options", () => {
    expect(organizationalSnaQuery({ sna_variant: "working_together_pandas", sna_activity_attribute: "team:activity", sna_resource_attribute: "team:resource", sna_case_id_attribute: "case:batch", sna_beta: 0.5, sna_n: 3, sna_metric_normalization: true, sna_max_edges: 12 }))
      .toBe("sna_variant=working_together_pandas&sna_activity_attribute=team%3Aactivity&sna_resource_attribute=team%3Aresource&sna_case_id_attribute=case%3Abatch&sna_beta=0.5&sna_n=3&sna_metric_normalization=true&sna_max_edges=12");
    expect(organizationalSnaQuery({ sna_variant: "jointactivities_pandas", sna_beta: 0.25 }))
      .toBe("sna_variant=jointactivities_pandas&sna_beta=0.25");
    expect(() => organizationalSnaQuery({ sna_variant: "working_together", sna_case_id_attribute: "case:batch" }))
      .toThrow(/requires a handover/);
    expect(() => organizationalSnaQuery({ sna_variant: "jointactivities_pandas", sna_case_id_attribute: "case:batch" }))
      .toThrow(/requires a handover/);
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ family: "bad" }), { status: 200 })));
    await expect(requestOrganizationalSna()).rejects.toThrow(/invalid organizational SNA/);
    vi.unstubAllGlobals();
  });
});

describe("network analysis API", () => {
  it("serializes selectors and bounded options", () => {
    expect(networkAnalysisQuery({ network_analysis_variant: "dataframe", network_analysis_edge_reference: "_in", network_analysis_include_performance: true, network_analysis_max_rows: 3 }))
      .toBe("network_analysis_variant=dataframe&network_analysis_edge_reference=_in&network_analysis_include_performance=true&network_analysis_max_rows=3");
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ family: "NETWORK_ANALYSIS", edges: [] }), { status: 200 })));
    await expect(requestNetworkAnalysis()).rejects.toThrow(/invalid network analysis/);
    vi.unstubAllGlobals();
  });
});

describe("trace clustering API", () => {
  it("serializes variant, distance, and safety options", () => {
    expect(clusteringQuery({ clustering_trace_attribute: "segment", clustering_activity_attribute: "phase", clustering_variant: "DMM_VEC", clustering_alpha: 0.25, clustering_percent: 0.8, clustering_max_groups: 4, clustering_max_output_nodes: 9 }))
      .toBe("clustering_trace_attribute=segment&clustering_activity_attribute=phase&clustering_variant=DMM_VEC&clustering_alpha=0.25&clustering_percent=0.8&clustering_max_groups=4&clustering_max_output_nodes=9");
    expect(() => clusteringQuery({ clustering_activity_attribute: "" })).toThrow(/non-empty/);
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

describe("profile clustering API", () => {
  it("serializes deterministic profile settings and escaped lists", () => {
    expect(profileClusteringQuery({
      profile_variant: "sklearn_profiles",
      profile_activity_attribute: "phase",
      profile_cluster_count: 3, profile_seed: 7,
      profile_include_activity: false,
      profile_trace_string_attributes: ["segment", "region"],
      profile_feature_names: ["a,b", "c"],
    })).toBe("profile_activity_attribute=phase&profile_cluster_count=3&profile_feature_names=a%5C%2Cb%2Cc&profile_include_activity=false&profile_seed=7&profile_trace_string_attributes=segment%2Cregion&profile_variant=sklearn_profiles");
    expect(() => profileClusteringQuery({ profile_cluster_count: 0 })).toThrow(/positive/);
    expect(() => profileClusteringQuery({ profile_activity_attribute: "" })).toThrow(/non-empty/);
    expect(() => profileClusteringQuery({ profile_variant: "other" as "sklearn_profiles" })).toThrow(/profile_variant/);
    expect(() => profileClusteringQuery({ profile_feature_names: ["a", "a"] })).toThrow(/duplicates/);
  });
  it("validates the profile result envelope", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/profile-clustering?profile_cluster_count=2");
      return new Response(JSON.stringify({
        family: "PROFILE_CLUSTERING", variant: "sklearn_profiles", labels: [0, 1], clusters: [[0], [1]],
        feature_names: ["event:concept:name@A"], inertia: 0,
        diagnostics: { computed: true, converged: true, trace_count: 2,
          feature_count: 1, cluster_count: 2, iterations: 1, message: "converged" },
      }), { status: 200 });
    }));
    await expect(requestProfileClustering({ profile_cluster_count: 2 }))
      .resolves.toMatchObject({ family: "PROFILE_CLUSTERING", labels: [0, 1] });
    vi.unstubAllGlobals();
  });
});

describe("role discovery API", () => {
  it("serializes strict threshold and safety options", () => {
    expect(roleDiscoveryQuery({ roles_variant: "pandas", roles_activity_attribute: "team:activity", roles_resource_attribute: "team:resource", roles_threshold: 0.4, roles_max_resources: 8, roles_max_activities: 12 }))
      .toBe("roles_variant=pandas&roles_activity_attribute=team%3Aactivity&roles_resource_attribute=team%3Aresource&roles_threshold=0.4&roles_max_resources=8&roles_max_activities=12");
  });
  it("rejects malformed responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ family: "ROLE_DISCOVERY" }), { status: 200 })));
    await expect(requestRoleDiscovery()).rejects.toThrow(/invalid role discovery/);
    vi.unstubAllGlobals();
  });
});

describe("trace-variant query API", () => {
  it("encodes deterministic privacy and bound options", () => {
    expect(traceVariantQuery({ tvq_variant: "laplace", tvq_epsilon: 0.5, tvq_k: 3, tvq_p: 2, tvq_seed: 9, tvq_secure_random: true, tvq_max_output_traces: 20 }))
      .toBe("tvq_variant=laplace&tvq_epsilon=0.5&tvq_k=3&tvq_p=2&tvq_seed=9&tvq_secure_random=true&tvq_max_output_traces=20");
  });
  it("rejects malformed backend payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "TRACE_VARIANT_QUERY", variant: "sacofa" }) })));
    await expect(requestTraceVariantQuery()).rejects.toThrow(/invalid trace-variant query/);
    vi.unstubAllGlobals();
  });
});

describe("PRIPEL API", () => {
  it("encodes privacy, blocklist, and matching bounds", () => {
    expect(pripelQuery({ pripel_epsilon: 0.5, pripel_blocklist: ["case,id", "resource"], pripel_seed: 4, pripel_secure_random: true, pripel_max_distance: 2 }))
      .toBe("pripel_epsilon=0.5&pripel_blocklist=case%5C%2Cid%2Cresource&pripel_seed=4&pripel_secure_random=true&pripel_max_distance=2");
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
    expect(ocelDiscoveryQuery({ variant: "ocpn", ocpn_annotation: "none", diagnostics_with_token_based_replay: true }))
      .toBe("variant=ocpn&ocpn_annotation=none&diagnostics_with_token_based_replay=true");
    expect(ocelDiscoveryQuery({
      variant: "ocpn", ocpn_activity_attribute: "  event_kind ",
      ocpn_object_type_attribute: "object_class",
    })).toBe("variant=ocpn&ocpn_activity_attribute=event_kind&ocpn_object_type_attribute=object_class");
    expect(ocelDiscoveryQuery({
      variant: "ocdfg", compute_edges_performance: false,
      ocdfg_activity_attribute: " event_kind ",
      ocdfg_object_type_attribute: " object_class ",
      saw_activity_attribute: "saw_kind", saw_object_type_attribute: "saw_class",
    })).toBe("variant=ocdfg&compute_edges_performance=false&ocdfg_activity_attribute=event_kind&ocdfg_object_type_attribute=object_class&saw_activity_attribute=saw_kind&saw_object_type_attribute=saw_class");
    expect(() => ocelDiscoveryQuery({ ocpn_activity_attribute: "  " }))
      .toThrow(/must not be empty/);
    expect(() => ocelDiscoveryQuery({ ocdfg_object_type_attribute: "  " }))
      .toThrow(/must not be empty/);
    expect(ocelDiscoveryQuery({ variant: "saw" }))
      .toBe("variant=saw");
    expect(() => ocelDiscoveryQuery({
      diagnostics_with_token_based_replay: "yes" as never,
    })).toThrow(/must be boolean/);
    expect(ocelSplitQuery({ variant: "ancestors_descendants", object_type: "order", max_objects: 2, max_degree_centrality: 0.5 }))
      .toBe("variant=ancestors_descendants&object_type=order&max_objects=2&max_degree_centrality=0.5");
    expect(ocelEnrichmentQuery({
      graphs: ["object_interaction_graph", "object_cobirth_graph"],
      lifecycle_qualifiers: false,
    })).toBe("graphs=object_cobirth_graph%2Cobject_interaction_graph&lifecycle_qualifiers=false");
    expect(ocelPlayoutQuery({
      model: "causal_net", variant: "extensive", max_bindings_per_activity: 2, max_traces: 3,
      max_states: 40, objects: { item: ["i,1"], order: ["o1", "o2"] },
      objects_unique_per_trace: true, ocpn_variant: "dfg", ocpn_annotation: "none", event_interval: 1.5,
    })).toBe("model=causal_net&variant=extensive&max_bindings_per_activity=2&max_traces=3&max_states=40&objects=item%3Ai%5C%2C1%3Border%3Ao1%2Co2&objects_unique_per_trace=true&event_interval=1.5&ocpn_variant=dfg&ocpn_annotation=none");
    expect(ocelPlayoutQuery({
      model: "ocpn", ocpn_activity_attribute: "event_kind",
      ocpn_object_type_attribute: "object_class",
    })).toBe("model=ocpn&ocpn_activity_attribute=event_kind&ocpn_object_type_attribute=object_class");
    expect(() => ocelPlayoutQuery({ ocpn_object_type_attribute: "" }))
      .toThrow(/must not be empty/);
    expect(ocelFeaturesQuery({
      variant: "objects", max_features: 12,
      include_lifecycle_features: false,
      include_degree_centrality_features: true,
      include_interaction_by_type_features: true,
      numeric_event_attributes: ["z", "a"],
      string_object_attributes: ["color"],
      filter_per_type: "item",
    })).toBe("variant=objects&max_features=12&include_degree_centrality_features=true&include_interaction_by_type_features=true&include_lifecycle_features=false&numeric_event_attributes=a%2Cz&string_object_attributes=color&ocel_features_filter_per_type=item");
    expect(ocelFeaturesQuery({
      variant: "objects", include_all_object_features: true,
    })).toBe("variant=objects&include_all_object_features=true");
    expect(ocelFeaturesQuery({
      variant: "objects", include_all_object_features: true,
      include_general_interaction_features: false,
    })).toBe("variant=objects&include_all_object_features=true&include_general_interaction_features=false");
    expect(ocelFeaturesQuery({
      variant: "events", feature_name_style: "compatibility",
      string_event_attributes: ["kind"],
    })).toBe("variant=events&feature_name_style=compatibility&string_event_attributes=kind");
    expect(ocelFeaturesQuery({
      variant: "objects", include_all_object_features: false,
    })).toBe("variant=objects&include_all_object_features=false");
    expect(ocelFeaturesQuery({
      variant: "objects", interaction_graph_source: "event_cooccurrence",
    })).toBe("variant=objects&interaction_graph_source=event_cooccurrence");
    expect(ocelFeaturesQuery({
      variant: "objects", interaction_graph_source: "event_and_o2o",
    })).toBe("variant=objects&interaction_graph_source=event_and_o2o");
    expect(ocelFeaturesQuery({
      variant: "objects", include_obj_con_in_graph_features: true,
      object_neighbor_graph: "cobirth",
    })).toBe("variant=objects&object_neighbor_graph=cobirth&include_obj_con_in_graph_features=true");
    expect(ocelFeaturesQuery({
      variant: "objects", include_lifecycle_length_features: false,
      include_lifecycle_unique_activity_features: false,
      include_lifecycle_duration_features: false,
      include_lifecycle_activity_features: false,
      include_lifecycle_path_features: false,
      include_lifecycle_wip_features: false,
    })).toBe("variant=objects&include_lifecycle_length_features=false&include_lifecycle_unique_activity_features=false&include_lifecycle_duration_features=false&include_lifecycle_activity_features=false&include_lifecycle_path_features=false&include_lifecycle_wip_features=false");
    expect(ocelFeaturesQuery({
      variant: "objects", include_general_interaction_features: true,
      include_general_descendants_features: true,
      include_general_inheritance_features: true,
      include_cobirth_features: true, include_codeath_features: true,
    })).toBe("variant=objects&include_general_interaction_features=true&include_general_descendants_features=true&include_general_inheritance_features=true&include_cobirth_features=true&include_codeath_features=true");
    expect(ocelFeaturesQuery({
      variant: "objects", include_related_events_features: true,
      include_related_activities_features: true,
      include_obj_con_in_graph_features: true,
      include_lifecycle_path_features: false,
    })).toBe("variant=objects&include_related_events_features=true&include_related_activities_features=true&include_obj_con_in_graph_features=true&include_lifecycle_path_features=false");
    expect(ocelFeaturesQuery({
      variant: "events", include_related_objects_features: true,
    })).toBe("variant=events&include_related_objects_features=true");
    expect(ocelFeaturesQuery({
      variant: "events", include_related_objects_features: true,
      include_related_events_features: true,
      include_obj_con_in_graph_features: true,
      include_general_interaction_features: true,
    })).toBe("variant=events&include_general_interaction_features=true&include_related_events_features=true&include_related_objects_features=true&include_obj_con_in_graph_features=true");
    expect(ocelFeaturesQuery({
      variant: "events", include_relation_features: false,
      include_event_num_rel_objs_features: true,
      include_event_num_rel_objs_type_features: false,
      include_endpoint_features: false,
      include_event_start_ot_features: true,
      include_event_end_ot_features: false,
      include_new_interactions_features: true,
    })).toBe("variant=events&include_relation_features=false&include_event_num_rel_objs_features=true&include_event_num_rel_objs_type_features=false&include_endpoint_features=false&include_event_start_ot_features=true&include_event_end_ot_features=false&include_new_interactions_features=true");
    expect(ocelFeaturesQuery({
      variant: "events_objects", include_prefix_features: true,
      include_prefix_length_features: true,
      include_prefix_timediff_features: false,
      include_prefix_one_hot_features: true,
    })).toBe("variant=events_objects&include_prefix_features=true&include_prefix_length_features=true&include_prefix_timediff_features=false&include_prefix_one_hot_features=true");
    expect(ocelFeaturesQuery({
      variant: "events_objects", include_prefix_features: false,
    })).toBe("variant=events_objects&include_prefix_features=false");
    expect(ocelFeaturesQuery({
      variant: "events_objects", include_prefix_features: false,
      include_prefix_length_features: true,
    })).toBe("variant=events_objects&include_prefix_features=false&include_prefix_length_features=true");
    expect(ocelFeaturesQuery({
      variant: "events_objects", include_event_pointwise_features: false,
      include_object_pointwise_features: true,
    })).toBe("variant=events_objects&include_event_pointwise_features=false&include_object_pointwise_features=true");
    expect(ocelFeaturesQuery({ variant: "events", filter_per_type: "" }))
      .toBe("variant=events&ocel_features_filter_per_type=");
    expect(() => ocelFeaturesQuery({ variant: "events", include_interaction_by_type_features: true }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_interaction_by_type_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_general_interaction_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_degree_centrality_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_all_object_features: true }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({
      variant: "events", interaction_graph_source: "event_cooccurrence",
    })).toThrow(/objects/);
    expect(() => ocelFeaturesQuery({
      variant: "objects", interaction_graph_source: "other" as never,
    })).toThrow(/event_cooccurrence/);
    expect(() => ocelFeaturesQuery({
      variant: "objects", include_obj_con_in_graph_features: true,
      object_neighbor_graph: "other" as never,
    })).toThrow(/object_neighbor_graph/);
    expect(() => ocelFeaturesQuery({
      variant: "objects", object_neighbor_graph: "interaction",
    })).toThrow(/requires include_obj_con/);
    expect(() => ocelFeaturesQuery({ variant: "events_objects", include_all_object_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({
      variant: "events_objects", feature_name_style: "compatibility",
    })).toThrow(/events or objects/);
    expect(() => ocelFeaturesQuery({
      variant: "events", feature_name_style: "invalid" as never,
    })).toThrow(/feature_name_style/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_codeath_features: true }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_related_events_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "objects", include_related_objects_features: true }))
      .toThrow(/events/);
    expect(() => ocelFeaturesQuery({ variant: "events_objects", include_related_objects_features: false }))
      .toThrow(/events/);
    expect(() => ocelFeaturesQuery({ variant: "objects", include_event_num_rel_objs_features: true }))
      .toThrow(/events/);
    expect(() => ocelFeaturesQuery({ variant: "events_objects", include_new_interactions_features: false }))
      .toThrow(/events/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_lifecycle_duration_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_lifecycle_features: false }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_lifecycle_wip_features: true }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_lifecycle_path_features: true }))
      .toThrow(/objects/);
    expect(() => ocelFeaturesQuery({ variant: "objects", include_prefix_features: false }))
      .toThrow(/events_objects/);
    expect(() => ocelFeaturesQuery({ variant: "events", include_event_pointwise_features: false }))
      .toThrow(/events_objects/);
    expect(() => ocelFeaturesQuery({ variant: "objects", include_object_pointwise_features: true }))
      .toThrow(/events_objects/);
    expect(ocelStatisticsQuery({ variant: "activity_associations", prefilter: "start" }))
      .toBe("variant=activity_associations&prefilter=start");
    expect(ocelStatisticsQuery({
      variant: "edge_associations", business_hours: true,
      business_hour_slots: "0-3600,86400:90000",
    })).toBe("variant=edge_associations&business_hours=true&business_hour_slots=0-3600%2C86400%3A90000");
    expect(ocelDescriptionQuery({ variant: "variant2", include_timestamps: false }))
      .toBe("variant=variant2&include_timestamps=false");
    expect(() => ocelStatisticsQuery({
      variant: "edge_associations", business_hour_slots: "0-604801",
    })).toThrow(/<= 604800/);
    expect(() => ocelStatisticsQuery({
      variant: "edge_associations", business_hours: false,
      business_hour_slots: "0-3600",
    })).toThrow(/business_hours=true/);
    expect(ocelConformanceQuery({ variant: "etot", alpha: 0.5, etot_theta: 0.2 }))
      .toBe("variant=etot&alpha=0.5&etot_theta=0.2");
    expect(ocelConformanceQuery({
      variant: "ocdfg", ocdfg_activity_attribute: " event_kind ",
      ocdfg_object_type_attribute: " object_class ",
    })).toBe("variant=ocdfg&ocdfg_activity_attribute=event_kind&ocdfg_object_type_attribute=object_class");
    expect(() => ocelConformanceQuery({
      ocdfg_activity_attribute: "  ",
    })).toThrow(/must not be empty/);
    expect(() => ocelConformanceQuery({
      ocdfg_object_type_attribute: "\t",
    })).toThrow(/must not be empty/);
    expect(ocelConformanceQuery({
      variant: "otg", otg_theta: 0.4, otg_theta_interaction: 0.1,
      otg_theta_descendants: 0.2, otg_theta_inheritance: 0.3,
      otg_theta_cobirth: 0.4, otg_theta_codeath: 0.5,
    })).toBe("variant=otg&otg_theta=0.4&otg_theta_interaction=0.1&otg_theta_descendants=0.2&otg_theta_inheritance=0.3&otg_theta_cobirth=0.4&otg_theta_codeath=0.5");
    expect(() => ocelConformanceQuery({ otg_theta_interaction: -0.1 }))
      .toThrow(/finite non-negative/);
    expect(ocelConformanceQuery({
      variant: "etot", real: "graph", real_graph_kind: "etot",
    })).toBe("variant=etot&real=graph&real_graph_kind=etot");
    expect(ocelConformanceQuery({ real: "ocel" })).toBe("real=ocel");
    expect(ocelConformanceQuery({ real: "graph" })).toBe("real=graph");
    expect(() => ocelConformanceQuery({ real: "ocel", real_graph_kind: "otg" }))
      .toThrow(/applies only/);
    expect(() => ocelConformanceQuery({
      variant: "otg", real: "graph", real_graph_kind: "etot",
    })).toThrow(/must match/);
    expect(() => ocelConformanceQuery({ real: "graph", real_graph_kind: "invalid" as never }))
      .toThrow(/must be etot/);
    expect(ocelFilterQuery({
      variant: "object_count", minimum_counts: { item: 2, order: 1 }, positive: true,
      level: 2, object_type: "order", min_size: 1, max_size: 4,
      allowed: { item: ["Inspect", "Pack"], order: ["Create"] },
    })).toBe("variant=object_count&positive=true&object_type=order&level=2&minimum_counts=item%3A2%2Corder%3A1&min_size=1&max_size=4&allowed=item%3AInspect%7CPack%3Border%3ACreate");
    expect(ocelFilterQuery({
      variant: "event_attribute", attribute_key: "label",
      attribute_values: ["a,b", 2, true, null], ids: ["id,1", "sales:2"],
    })).toBe("variant=event_attribute&attribute_key=label&attribute_values=s%3Aa%5C%2Cb%2Ci%3A2%2Cb%3Atrue%2Cn%3A&ids=id%5C%2C1%2Csales%5C%3A2");
    const delimiterQuery = new URLSearchParams(ocelFilterQuery({
      variant: "event_attribute", attribute_key: "label",
      attribute_values: ["a:b;c|d\\e"], ids: ["id:1;2|3\\4"],
      allowed: { "sales:order": ["Approve|Reject", "Pack;Back", "Slash\\Name"] },
    }));
    expect(delimiterQuery.get("attribute_values")).toBe("s:a\\:b\\;c\\|d\\\\e");
    expect(delimiterQuery.get("ids")).toBe("id\\:1\\;2\\|3\\\\4");
    expect(delimiterQuery.get("allowed")).toBe("sales\\:order:Approve\\|Reject|Pack\\;Back|Slash\\\\Name");
    const playoutDelimiterQuery = new URLSearchParams(ocelPlayoutQuery({
      objects: { "sales:order": ["id:1;2|3\\4"] },
    }));
    expect(playoutDelimiterQuery.get("objects")).toBe("sales\\:order:id\\:1\\;2|3\\\\4");
    expect(ocelPlayoutQuery({
      model: "ocpn", event_binding: "shared_activity", objects_unique_per_trace: true,
    })).toBe("model=ocpn&objects_unique_per_trace=true&event_binding=shared_activity");
    expect(ocelPlayoutQuery({
      model: "causal_net", event_binding: "synchronized", branching_factor_transitions: 3,
      branching_factor_bindings: 4, branching_factor_activities: 1,
      exists_trace: true, return_sequences: true,
    })).toBe("model=causal_net&branching_factor_transitions=3&branching_factor_bindings=4&branching_factor_activities=1&event_binding=synchronized&exists_trace=true&return_sequences=true");
    expect(ocelPlayoutQuery({
      event_binding: "synchronized", branching_factor_transitions: 1.5,
      branching_factor_bindings: 0.5, seed: 42,
    })).toBe("branching_factor_transitions=1.5&branching_factor_bindings=0.5&seed=42&event_binding=synchronized");
    expect(() => ocelPlayoutQuery({ branching_factor_bindings: -0.1 }))
      .toThrow(/finite non-negative/);
    expect(() => ocelPlayoutQuery({ branching_factor_activities: Number.NaN }))
      .toThrow(/finite non-negative/);
    expect(() => ocelPlayoutQuery({ model: "ocpn", branching_factor_activities: 1 }))
      .toThrow(/requires model=causal_net/);
    expect(ocelPlayoutQuery({
      initial_marking: { order: [0, 1, 0], item: [1, 0, 0, 0] },
      final_marking: { order: [0, 0, 1], item: [0, 0, 0, 1] },
    })).toBe("initial_marking=item%3A1%2C0%2C0%2C0%3Border%3A0%2C1%2C0&final_marking=item%3A0%2C0%2C0%2C1%3Border%3A0%2C0%2C1");
    expect(() => ocelPlayoutQuery({ initial_marking: { order: [] } }))
      .toThrow(/non-empty uint32 vectors/);
    expect(() => ocelPlayoutQuery({ final_marking: { order: [0, -1] } }))
      .toThrow(/non-empty uint32 vectors/);
    expect(() => ocelPlayoutQuery({ event_binding: "invalid" as never }))
      .toThrow(/event_binding is invalid/);
  });

  it("posts OCEL JSON as multipart data and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({ family: "OCEL_DISCOVERY", variant: "etot", result: { activities: [] } }) };
    }));
    await expect(requestOcelDiscovery(new File(["{}"], "log.json"), { variant: "etot" })).resolves.toMatchObject({ family: "OCEL_DISCOVERY" });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL SQLite upload and validates the imported log", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_SQLITE", variant: "IMPORT",
        log: { "ocel:events": {}, "ocel:objects": {} },
      }) };
    }));
    await expect(requestOcelSqlite(new File(["sqlite"], "log.sqlite")))
      .resolves.toMatchObject({ family: "OCEL_SQLITE", variant: "IMPORT" });
    vi.unstubAllGlobals();
  });

  it("exports an OCEL JSON document as a validated SQLite blob", async () => {
    const sqlite = new Blob(["SQLite format 3\0\x00"], {
      type: "application/vnd.sqlite3",
    });
    vi.stubGlobal("fetch", vi.fn(async (path: string, init: RequestInit) => {
      expect(path).toBe("/api/ocel/sqlite?mode=export");
      expect(init.method).toBe("POST");
      expect(init.headers).toEqual({ "Content-Type": "application/json" });
      expect(init.body).toBe(JSON.stringify({ "ocel:events": {}, "ocel:objects": {} }));
      return { ok: true, blob: async () => sqlite };
    }));
    await expect(requestOcelSqliteExport({ "ocel:events": {}, "ocel:objects": {} }))
      .resolves.toBe(sqlite);
    vi.unstubAllGlobals();
  });

  it("rejects a non-SQLite export response", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true,
      blob: async () => new Blob(["not a database"]),
    })));
    await expect(requestOcelSqliteExport({})).rejects.toThrow(/invalid SQLite export/);
    vi.unstubAllGlobals();
  });

  it("round-trips classic and standard OCEL JSON/XML through the exchange adapter", async () => {
    expect(ocelExchangeQuery("xml_standard", "import"))
      .toBe("format=xml_standard&mode=import");
    expect(ocelExchangeQuery("csv", "import", {
      ocel_csv_event_id_column: "event_id",
      ocel_csv_activity_column: "activity_name",
      ocel_csv_timestamp_column: "started_at",
      ocel_csv_object_type_prefix: "type:",
      ocel_csv_object_id_column: "object_id",
      ocel_csv_object_type_column: "object_type",
      ocel_csv_delimiter: ";",
      ocel_csv_trim_fields: false,
      ocel_csv_sort_events_by_timestamp: false,
      ocel_csv_reject_unsupported: false,
    })).toBe("format=csv&mode=import&ocel_csv_event_id_column=event_id&ocel_csv_activity_column=activity_name&ocel_csv_timestamp_column=started_at&ocel_csv_object_type_prefix=type%3A&ocel_csv_object_id_column=object_id&ocel_csv_object_type_column=object_type&ocel_csv_delimiter=%3B&ocel_csv_trim_fields=false&ocel_csv_sort_events_by_timestamp=false&ocel_csv_reject_unsupported=false");
    const imported = {
      family: "OCEL_IO", variant: "IMPORT_XML_STANDARD",
      log: { "ocel:events": {}, "ocel:objects": {} },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/ocel/io?format=xml_standard&mode=import");
      expect(init?.method).toBe("POST");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => imported };
    }));
    await expect(requestOcelExchangeImport(new File(["<log/ >"], "log.xml"), "xml_standard"))
      .resolves.toMatchObject({ family: "OCEL_IO", variant: "IMPORT_XML_STANDARD" });
    vi.unstubAllGlobals();

    const json = new Blob([JSON.stringify({ "ocel:events": {}, "ocel:objects": {} })], {
      type: "application/json",
    });
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/ocel/io?format=json_standard&mode=export");
      expect(init?.headers).toEqual({ "Content-Type": "application/json" });
      return { ok: true, blob: async () => json };
    }));
    await expect(requestOcelExchangeExport({ "ocel:events": {}, "ocel:objects": {} }, "json_standard"))
      .resolves.toBe(json);
    vi.unstubAllGlobals();
  });

  it("uploads an event CSV with an optional object table and exports CSV text", async () => {
    const imported = {
      family: "OCEL_IO", variant: "IMPORT_CSV",
      log: { "ocel:events": {}, "ocel:objects": {} },
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/ocel/io?format=csv&mode=import&ocel_csv_delimiter=%3B&ocel_csv_trim_fields=true");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => imported };
    }));
    await expect(requestOcelExchangeImport(
      new File(["ocel:eid,ocel:activity,ocel:timestamp\n"], "events.csv"),
      "csv",
      new File(["ocel:oid,ocel:type\n"], "objects.csv"),
      { ocel_csv_delimiter: ";", ocel_csv_trim_fields: true },
    )).resolves.toMatchObject({ family: "OCEL_IO", variant: "IMPORT_CSV" });
    vi.unstubAllGlobals();

    const csv = new Blob(["ocel:eid,ocel:activity,ocel:timestamp\n"], { type: "text/csv" });
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/ocel/io?format=csv_objects&mode=export&ocel_csv_delimiter=%3B&ocel_csv_allow_lossy=true");
      return { ok: true, blob: async () => csv };
    }));
    await expect(requestOcelExchangeExport({}, "csv_objects", {
      ocel_csv_delimiter: ";",
      ocel_csv_allow_lossy: true,
    })).resolves.toBe(csv);
    vi.unstubAllGlobals();
  });

  it("rejects unsupported CSV object-table imports", async () => {
    await expect(requestOcelExchangeImport(new File([""], "objects.csv"), "csv_objects"))
      .rejects.toThrow(/export-only/);
  });

  it("rejects malformed OCEL exchange downloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true,
      blob: async () => new Blob(["not XML"]),
    })));
    await expect(requestOcelExchangeExport({}, "xml")).rejects.toThrow(/invalid OCEL XML/);
    vi.unstubAllGlobals();
  });

  it("serializes the required object type and returns the flattening XES blob", async () => {
    expect(ocelFlattenQuery({
      object_type: "  order items  ",
      event_activity_attribute: "  custom activity  ",
      event_timestamp_attribute: "custom:timestamp",
    })).toBe("object_type=++order+items++&event_activity_attribute=++custom+activity++&event_timestamp_attribute=custom%3Atimestamp");
    expect(() => ocelFlattenQuery()).toThrow(/non-empty string/);
    expect(() => ocelFlattenQuery({ object_type: "   " })).toThrow(/non-empty string/);
    expect(() => ocelFlattenQuery({ object_type: 7 as never })).toThrow(/non-empty string/);
    expect(() => ocelFlattenQuery({ object_type: "order", event_activity_attribute: "   " }))
      .toThrow(/event_activity_attribute.*non-empty string/);
    expect(() => ocelFlattenQuery({ object_type: "order", event_timestamp_attribute: "" }))
      .toThrow(/event_timestamp_attribute.*non-empty string/);
    expect(() => ocelFlattenQuery({ object_type: "order", event_activity_attribute: 4 as never }))
      .toThrow(/event_activity_attribute.*non-empty string/);

    const xes = new Blob(["<log></log>"], { type: "application/xml" });
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/ocel/flatten?object_type=++order+items++&event_activity_attribute=custom+activity&event_timestamp_attribute=custom%3Atimestamp");
      expect(init?.method).toBe("POST");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, blob: async () => xes };
    }));
    await expect(requestOcelFlatten(new File(["{}"], "log.json"), {
      object_type: "  order items  ",
      event_activity_attribute: "custom activity",
      event_timestamp_attribute: "custom:timestamp",
    })).resolves.toBe(xes);
    vi.unstubAllGlobals();
  });

  it("rejects a failed flattening response", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: false, status: 422 })));
    await expect(requestOcelFlatten(new File(["{}"], "log.json"), {
      object_type: "order",
    })).rejects.toThrow(/status 422/);
    vi.unstubAllGlobals();
  });

  it("posts an OCEL split request and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true,
      json: async () => ({ family: "OCEL_SPLIT", variant: "CONNECTED_COMPONENTS",
        candidate_count: 0, returned_count: 0, truncated: false, logs: [] }),
    })));
    await expect(requestOcelSplit(new File(["{}"], "log.json"), { variant: "connected_components" }))
      .resolves.toMatchObject({ family: "OCEL_SPLIT", variant: "CONNECTED_COMPONENTS" });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL enrichment request and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/ocel/enrich?graphs=object_interaction_graph&lifecycle_qualifiers=false");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_ENRICHMENT", variant: "CLASSIC",
        graphs: ["object_interaction_graph"], lifecycle_qualifiers: false,
        log: { "ocel:events": {}, "ocel:objects": {} },
      }) };
    }));
    await expect(requestOcelEnrichment(new File(["{}"], "log.json"), {
      graphs: ["object_interaction_graph"], lifecycle_qualifiers: false,
    })).resolves.toMatchObject({ family: "OCEL_ENRICHMENT", variant: "CLASSIC" });
    vi.unstubAllGlobals();
  });

  it("serializes and posts a deterministic OCEL object graph request", async () => {
    expect(ocelObjectGraphQuery({ variant: "object_codeath_graph" }))
      .toBe("variant=object_codeath_graph");
    expect(() => ocelObjectGraphQuery({ variant: "unknown" as never }))
      .toThrow(/variant is invalid/);
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/ocel/graph?variant=object_codeath_graph");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_OBJECT_GRAPH", variant: "object_codeath_graph",
        object_ids: ["i1", "o1"], edges: [["i1", "o1"]],
      }) };
    }));
    await expect(requestOcelObjectGraph(new File(["{}"], "log.json"), {
      variant: "object_codeath_graph",
    })).resolves.toMatchObject({ family: "OCEL_OBJECT_GRAPH" });
    vi.unstubAllGlobals();
  });

  it("serializes event/object graph toggles and validates the response", async () => {
    expect(ocelEventObjectGraphQuery()).toBe(
      "variant=heterogeneous&include_df=true&include_object_changes=true",
    );
    expect(ocelEventObjectGraphQuery({ include_df: false, include_object_changes: false }))
      .toBe("variant=heterogeneous&include_df=false&include_object_changes=false");
    expect(() => ocelEventObjectGraphQuery({ include_df: "yes" as never }))
      .toThrow(/include_df must be boolean/);
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe(
        "/api/ocel/event-object-graph?variant=heterogeneous&include_df=false&include_object_changes=true",
      );
      expect(init?.method).toBe("POST");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_GRAPH", variant: "HETEROGENEOUS",
        include_df: false, include_object_changes: true,
        nodes: [
          { id: "e1", type: "EVENT", activity: "create", timestamp: "2024-01-01T00:00:00Z", attributes: {} },
          { id: "o1", type: "OBJECT", object_type: "order", attributes: {} },
          { id: "@@change##0", type: "CHANGE", object_id: "o1", object_type: "order", timestamp: "2024-01-01T00:00:00Z", field: "status", value: "new" },
        ],
        edges: [
          { source: "e1", target: "o1", type: "E2O", qualifier: "" },
          { source: "@@change##0", target: "o1", type: "CHANGE" },
        ],
      }) };
    }));
    await expect(requestOcelEventObjectGraph(new File(["{}"], "log.json"), {
      include_df: false,
    })).resolves.toMatchObject({ family: "OCEL_GRAPH", variant: "HETEROGENEOUS" });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL filter request and validates the returned log", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_FILTER", variant: "CC_LENGTH",
        log: { "ocel:events": {}, "ocel:objects": {} },
      }) };
    }));
    await expect(requestOcelFilter(new File(["{}"], "log.json"), {
      variant: "cc_length", min_size: 2, max_size: 4,
    })).resolves.toMatchObject({ family: "OCEL_FILTER", variant: "CC_LENGTH" });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL playout request and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_PLAYOUT", variant: "EXTENSIVE", event_binding: "shared_activity",
        log: { "ocel:events": {} },
      }) };
    }));
    await expect(requestOcelPlayout(new File(["{}"], "log.json"), {
      max_traces: 1,
    })).resolves.toMatchObject({ family: "OCEL_PLAYOUT", variant: "EXTENSIVE" });
    vi.unstubAllGlobals();
  });

  it("accepts the causal-net playout family", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({
      family: "OC_CAUSAL_NET_PLAYOUT", variant: "EXTENSIVE", log: { "ocel:events": {} },
    }) })));
    await expect(requestOcelPlayout(new File(["{}"], "log.json"), {
      model: "causal_net", variant: "extensive",
    })).resolves.toMatchObject({ family: "OC_CAUSAL_NET_PLAYOUT" });
    vi.unstubAllGlobals();
  });

  it("serializes and validates OCPN trace details", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/ocel/playout?model=ocpn&return_traces=true");
      return { ok: true, json: async () => ({
        family: "OCEL_PLAYOUT", variant: "EXTENSIVE",
        traces: [[{
          activity: "Ship", objects: [{ type: "order", id: "o1" }],
          transitions: [{ object_type: "order", object_id: "o1", transition_index: 2 }],
        }]],
        log: { "ocel:events": {}, "ocel:objects": {} },
      }) };
    }));
    await expect(requestOcelPlayout(new File(["{}"], "log.json"), {
      model: "ocpn", return_traces: true,
    })).resolves.toMatchObject({ family: "OCEL_PLAYOUT", traces: expect.any(Array) });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL feature request and validates the table", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_FEATURES", variant: "EVENTS", row_count: 1, feature_count: 1, row_ids: ["e1"],
        feature_names: ["@@event_num_rel_objs"], values: [[1]],
      }) };
    }));
    await expect(requestOcelFeatures(new File(["{}"], "log.json"), {
      variant: "events", include_relation_features: false,
    })).resolves.toMatchObject({ family: "OCEL_FEATURES", variant: "EVENTS" });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL statistics request and validates the summary", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string, init: RequestInit) => {
      expect(path).toBe("/api/ocel/statistics?variant=edge_associations&business_hours=true&business_hour_slots=0-3600");
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_STATISTICS", variant: "EDGE_ASSOCIATIONS",
        edges: { Item: { "Create->Ship": {
          event_couples: [["e1", "e2"]], unique_objects: ["o1"],
          event_objects: [["e1", "e2", "o1"]], performance: [3600],
        } } },
      }) };
    }));
    await expect(requestOcelStatistics(new File(["{}"], "log.json"), {
      variant: "edge_associations", business_hours: true,
      business_hour_slots: "0-3600",
    })).resolves.toMatchObject({ family: "OCEL_STATISTICS", variant: "EDGE_ASSOCIATIONS" });
    vi.unstubAllGlobals();
  });

  it("posts an OCEL description request and validates tuple output", async () => {
    const payload = {
      family: "OCEL_DESCRIPTION", variant: "VARIANT2", includes_timestamps: false,
      event_count: 1, object_count: 1, event_object_counts: [1],
      events: [["Create", "o1"]], objects: [["o1", "order"]],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init: RequestInit) => {
      expect(path).toBe("/api/ocel/description?variant=variant2&include_timestamps=false");
      expect(init.method).toBe("POST");
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => payload };
    }));
    await expect(requestOcelDescription(new File(["{}"], "log.json"), {
      variant: "variant2", include_timestamps: false,
    })).resolves.toMatchObject({ family: "OCEL_DESCRIPTION", variant: "VARIANT2" });
    vi.unstubAllGlobals();
  });

  it("rejects malformed OCEL conformance payloads", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "OCEL_CONFORMANCE", variant: "otg" }) })));
    await expect(requestOcelConformance(new File(["{}"], "log.json"), new File(["{}"], "normative.json")))
      .rejects.toThrow(/invalid OCEL conformance/);
    vi.unstubAllGlobals();
  });

  it("keeps the default OCEL upload field and switches the real-side graph field explicitly", async () => {
    const payload = { family: "OCEL_CONFORMANCE", variant: "etot", result: {} };
    const calls: Array<{ path: string; form: FormData }> = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      calls.push({ path, form: init?.body as FormData });
      return { ok: true, json: async () => payload };
    }));

    await expect(requestOcelConformance(
      new File(["{}"], "real.json"), new File(["{}"], "normative.json"),
    )).resolves.toMatchObject({ family: "OCEL_CONFORMANCE" });
    await expect(requestOcelConformance(
      new File(["{}"], "real.etot.json"), new File(["{}"], "normative.json"),
      { variant: "etot", real: "graph", real_graph_kind: "etot" },
    )).resolves.toMatchObject({ family: "OCEL_CONFORMANCE" });

    expect(calls).toHaveLength(2);
    expect(calls[0].path).toBe("/api/ocel/conformance");
    expect(calls[0].form.get("log")).toBeInstanceOf(File);
    expect((calls[0].form.get("log") as File).name).toBe("real.json");
    expect(calls[0].form.get("real_graph")).toBeNull();
    expect(calls[0].form.get("normative")).toBeInstanceOf(File);
    expect(calls[1].path).toBe("/api/ocel/conformance?variant=etot&real=graph&real_graph_kind=etot");
    expect(calls[1].form.get("log")).toBeNull();
    expect(calls[1].form.get("real_graph")).toBeInstanceOf(File);
    expect((calls[1].form.get("real_graph") as File).name).toBe("real.etot.json");
    expect(calls[1].form.get("normative")).toBeInstanceOf(File);
    vi.unstubAllGlobals();
  });
});

describe("link-analysis API", () => {
  it("serializes relation, ordering, propagation, and bound options", () => {
    expect(linkAnalysisQuery({
      link_analysis_out_column: "source_id",
      link_analysis_in_column: "target_id",
      link_analysis_sorting_column: "started_at",
      link_analysis_index_column: "row_id",
      link_analysis_look_forward: false,
      link_analysis_keep_first_occurrence: true,
      link_analysis_propagate: true,
      link_analysis_max_links: 12,
    })).toBe("link_analysis_out_column=source_id&link_analysis_in_column=target_id&link_analysis_sorting_column=started_at&link_analysis_index_column=row_id&link_analysis_look_forward=false&link_analysis_keep_first_occurrence=true&link_analysis_propagate=true&link_analysis_max_links=12");
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
