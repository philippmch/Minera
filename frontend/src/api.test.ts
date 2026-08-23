import { describe, expect, it, vi } from "vitest";
import { labelSplittingQuery, requestLabelSplitting, logToFeaturesQuery, requestLogToFeatures, logUtilityQuery, requestLogUtility, embeddingsQuery, requestEmbeddings, ilpMinerQuery, requestIlpMine, geneticMinerQuery, requestGeneticMine, traceVariantQuery, requestTraceVariantQuery, pripelQuery, requestPripel, organizationalSnaQuery, requestOrganizationalSna, networkAnalysisQuery, requestNetworkAnalysis, clusteringQuery, requestClustering, profileClusteringQuery, requestProfileClustering, roleDiscoveryQuery, requestRoleDiscovery, resourceProfilesQuery, requestResourceProfiles, localDiagnosticsQuery, ocelDiscoveryQuery, requestOcelDiscovery, requestOcelSqlite, requestOcelSqliteExport, ocelExchangeQuery, requestOcelExchangeImport, requestOcelExchangeExport, ocelFilterQuery, requestOcelFilter, ocelSplitQuery, requestOcelSplit, ocelEnrichmentQuery, requestOcelEnrichment, ocelPlayoutQuery, requestOcelPlayout, ocelFeaturesQuery, requestOcelFeatures, ocelStatisticsQuery, requestOcelStatistics, ocelDescriptionQuery, requestOcelDescription, ocelConformanceQuery, requestOcelConformance, linkAnalysisQuery, requestLinkAnalysis, timestampInterleavingsQuery, requestTimestampInterleavings, monteCarloQuery, fetchMonteCarloResult, fetchDfg, fetchPtml, dfgExportQuery, importDfg, dfgPrecisionQuery, requestDfgPrecision, dfgPetriConversionQuery, convertDfgToPetriNet, requestTriePetriNet, processCubeQuery, requestProcessCube, streamingDfgQuery, requestStreamingDfg, streamingFootprintsQuery, requestStreamingFootprints, streamingTemporalQuery, requestStreamingTemporal, streamingTbrQuery, requestStreamingTbr, requestDataPetriNet, chaoticActivitiesQuery, requestChaoticActivities, passedTimeQuery, requestPassedTime, serviceTimeQuery, requestServiceTime, reworkQuery, requestRework, cycleTimeQuery, requestCycleTime, caseArrivalQuery, requestCaseArrival, caseStatisticsQuery, requestCaseStatistics, attributeStatisticsQuery, requestAttributeStatistics, attributeDistributionQuery, requestAttributeDistribution, attributeKdeQuery, requestAttributeKde, boundaryActivitiesQuery, requestBoundaryActivities, variantStatisticsQuery, requestVariantStatistics, comparePetriUsage } from "./api";

describe("Petri usage comparison API", () => {
  it("serializes explicit replay markings in the multipart request", async () => {
    const payload = {
      family: "PETRI_USAGE_COMPARISON", places: [], transitions: [], arcs: [],
    };
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      expect(path).toBe("/api/compare-petri");
      expect(init?.method).toBe("POST");
      const form = init?.body as FormData;
      expect(form.get("miner")).toBe("alpha");
      expect(form.get("initial_marking")).toBe("1,0,2");
      expect(form.get("final_marking")).toBe("0,0,1");
      return { ok: true, json: async () => payload };
    }));
    await expect(comparePetriUsage(
      new File(["case_id,activity\nc1,A\n"], "reference.csv"),
      "alpha",
      { initial_marking: [1, 0, 2], final_marking: [0, 0, 1] },
    )).resolves.toMatchObject({ family: "PETRI_USAGE_COMPARISON" });
    vi.unstubAllGlobals();
  });

  it("rejects malformed marking vectors before upload", async () => {
    await expect(comparePetriUsage(
      new File([""], "reference.csv"), "inductive", { final_marking: [1, Number.NaN] },
    )).rejects.toThrow(/uint32/);
  });
});

describe("classic DFG export API", () => {
  it("serializes optional noise cleaning settings", () => {
    expect(dfgExportQuery({ noise_threshold: 0.5, most_common_paths: "A -> B" }))
      .toBe("noise_threshold=0.5&most_common_paths=A+-%3E+B");
    expect(() => dfgExportQuery({ noise_threshold: Number.NaN })).toThrow(/finite/);
    expect(() => dfgExportQuery({ most_common_paths: " " })).toThrow(/empty/);
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
    expect(() => streamingTemporalQuery({ streaming_temporal_profile: options.streaming_temporal_profile, streaming_temporal_zeta: 0 })).toThrow(/greater than zero/);
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

  it("serializes trace representation and extra feature controls", () => {
    expect(logToFeaturesQuery({
      features_enable_all_extra_features: true,
      features_event_successor_attributes: ["concept:name"],
      features_epsilon: 0.25,
      features_min_num_different_string_values: 1,
      features_max_num_different_string_values: 8,
    })).toBe("features_enable_all_extra_features=true&features_epsilon=0.25&features_event_successor_attributes=concept%3Aname&features_max_num_different_string_values=8&features_min_num_different_string_values=1");
    expect(() => logToFeaturesQuery({ features_epsilon: -1 })).toThrow(/epsilon/);
    expect(() => logToFeaturesQuery({ features_min_num_different_string_values: 3, features_max_num_different_string_values: 2 })).toThrow(/bounds/);
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
  });

  it("rejects malformed embedding responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: async () => ({ family: "TO_EMBEDDINGS", variant: "cases_transformers", dimensions: 2, items: [{ id: "c", embedding: [1] }] }) })));
    await expect(requestEmbeddings()).rejects.toThrow(/invalid embedding result/);
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
});

describe("organizational SNA API", () => {
  it("serializes bounded metric options", () => {
    expect(organizationalSnaQuery({ sna_variant: "working_together", sna_activity_attribute: "team:activity", sna_resource_attribute: "team:resource", sna_beta: 0.5, sna_n: 3, sna_metric_normalization: true, sna_max_edges: 12 }))
      .toBe("sna_variant=working_together&sna_activity_attribute=team%3Aactivity&sna_resource_attribute=team%3Aresource&sna_beta=0.5&sna_n=3&sna_metric_normalization=true&sna_max_edges=12");
    expect(organizationalSnaQuery({ sna_variant: "jointactivities_pandas", sna_beta: 0.25 }))
      .toBe("sna_variant=jointactivities_pandas&sna_beta=0.25");
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

describe("profile clustering API", () => {
  it("serializes deterministic profile settings and escaped lists", () => {
    expect(profileClusteringQuery({
      profile_cluster_count: 3, profile_seed: 7,
      profile_include_activity: false,
      profile_trace_string_attributes: ["segment", "region"],
      profile_feature_names: ["a,b", "c"],
    })).toBe("profile_cluster_count=3&profile_feature_names=a%5C%2Cb%2Cc&profile_include_activity=false&profile_seed=7&profile_trace_string_attributes=segment%2Cregion");
    expect(() => profileClusteringQuery({ profile_cluster_count: 0 })).toThrow(/positive/);
    expect(() => profileClusteringQuery({ profile_feature_names: ["a", "a"] })).toThrow(/duplicates/);
  });
  it("validates the profile result envelope", async () => {
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/profile-clustering?profile_cluster_count=2");
      return new Response(JSON.stringify({
        family: "PROFILE_CLUSTERING", labels: [0, 1], clusters: [[0], [1]],
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
    expect(ocelDiscoveryQuery({ variant: "ocpn", ocpn_annotation: "none" }))
      .toBe("variant=ocpn&ocpn_annotation=none");
    expect(ocelDiscoveryQuery({ variant: "saw" }))
      .toBe("variant=saw");
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
    expect(ocelFeaturesQuery({
      variant: "objects", max_features: 12,
      include_lifecycle_features: false,
      numeric_event_attributes: ["z", "a"],
      string_object_attributes: ["color"],
    })).toBe("variant=objects&max_features=12&include_lifecycle_features=false&numeric_event_attributes=a%2Cz&string_object_attributes=color");
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
      event_binding: "synchronized", branching_factor_transitions: 3,
      branching_factor_bindings: 4, exists_trace: true, return_sequences: true,
    })).toBe("branching_factor_transitions=3&branching_factor_bindings=4&event_binding=synchronized&exists_trace=true&return_sequences=true");
    expect(ocelPlayoutQuery({
      event_binding: "synchronized", branching_factor_transitions: 1.5,
      branching_factor_bindings: 0.5, seed: 42,
    })).toBe("branching_factor_transitions=1.5&branching_factor_bindings=0.5&seed=42&event_binding=synchronized");
    expect(() => ocelPlayoutQuery({ branching_factor_bindings: -0.1 }))
      .toThrow(/finite non-negative/);
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
      expect(path).toBe("/api/ocel/io?format=csv&mode=import");
      expect(init?.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => imported };
    }));
    await expect(requestOcelExchangeImport(
      new File(["ocel:eid,ocel:activity,ocel:timestamp\n"], "events.csv"),
      "csv",
      new File(["ocel:oid,ocel:type\n"], "objects.csv"),
    )).resolves.toMatchObject({ family: "OCEL_IO", variant: "IMPORT_CSV" });
    vi.unstubAllGlobals();

    const csv = new Blob(["ocel:eid,ocel:activity,ocel:timestamp\n"], { type: "text/csv" });
    vi.stubGlobal("fetch", vi.fn(async (path: string) => {
      expect(path).toBe("/api/ocel/io?format=csv_objects&mode=export");
      return { ok: true, blob: async () => csv };
    }));
    await expect(requestOcelExchangeExport({}, "csv_objects")).resolves.toBe(csv);
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

  it("posts an OCEL split request and validates the response", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({
      ok: true,
      json: async () => ({ family: "OCEL_SPLIT", variant: "CONNECTED_COMPONENTS", logs: [] }),
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

  it("posts an OCEL feature request and validates the table", async () => {
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init: RequestInit) => {
      expect(init.body).toBeInstanceOf(FormData);
      return { ok: true, json: async () => ({
        family: "OCEL_FEATURES", variant: "EVENTS", row_ids: ["e1"],
        feature_names: ["@@event_num_rel_objs"], values: [[1]],
      }) };
    }));
    await expect(requestOcelFeatures(new File(["{}"], "log.json"), {
      variant: "events", include_lifecycle_features: false,
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
