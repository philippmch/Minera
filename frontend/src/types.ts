export interface ActivityStat {
  name: string;
  count: number;
  start_count: number;
  end_count: number;
  /**
   * In how many cases this activity occurs more than once (rework). Absent
   * in exports from older minera versions.
   */
  rework_cases?: number;
}

export interface EdgeStat {
  source: number;
  target: number;
  count: number;
}

export interface ConceptDriftSublogSummary {
  trace_count: number;
  event_count: number;
}
export interface ConceptDriftChangePoint {
  sublog_index: number;
  p_value: number;
}
export interface ConceptDriftResult {
  family: "CONCEPT_DRIFT";
  activities: string[];
  feature_vectors: number[][];
  sublogs: ConceptDriftSublogSummary[];
  cumulative_sublogs: ConceptDriftSublogSummary[];
  sublog_start_timestamps: number[];
  change_points: ConceptDriftChangePoint[];
  change_timestamps: number[];
}

export interface ILPMinerTransition {
  activity: string | null;
  inputs: number[];
  outputs: number[];
}
export interface ILPMinerNet {
  place_count: number;
  source: number;
  sink: number;
  activities: string[];
  transitions: ILPMinerTransition[];
}
export interface ILPMinerMarkings { initial: number[]; final: number[]; }
export interface ILPMinerDiagnostics {
  computed: boolean;
  exhausted: boolean;
  visited_nodes: number;
  lp_solved: number;
  branch_and_bound_nodes: number;
  message: string;
}
export interface ILPMinerResult {
  family: "ILP";
  net: ILPMinerNet;
  markings: ILPMinerMarkings;
  diagnostics: ILPMinerDiagnostics;
}
export interface ILPMinerOptions {
  activity_key?: string;
  noise_threshold?: number;
  max_nodes?: number;
}
// Compatibility spellings for consumers that use title-case ILP names.
export type IlpMinerResult = ILPMinerResult;
export type IlpMinerOptions = ILPMinerOptions;
function isFiniteIntegerArray(value: unknown): value is number[] {
  return Array.isArray(value) && value.every((item) => typeof item === "number" && Number.isInteger(item) && item >= 0);
}
export function isILPMinerResult(value: unknown): value is ILPMinerResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<ILPMinerResult>;
  if (c.family !== "ILP" || !c.net || typeof c.net !== "object" || !c.markings || typeof c.markings !== "object" || !c.diagnostics || typeof c.diagnostics !== "object") return false;
  const n = c.net as Partial<ILPMinerNet>;
  const placeCount = n.place_count;
  if (!Number.isInteger(placeCount) || (placeCount as number) <= 0 || !Number.isInteger(n.source) || !Number.isInteger(n.sink) || (n.source as number) < 0 || (n.sink as number) < 0 || (n.source as number) >= (placeCount as number) || (n.sink as number) >= (placeCount as number) || !Array.isArray(n.activities) || !n.activities.every((x) => typeof x === "string") || !Array.isArray(n.transitions)) return false;
  const activities = n.activities as string[];
  if (!n.transitions.every((t) => {
    if (!t || typeof t !== "object" || !(t.activity === null || (typeof t.activity === "string" && activities.includes(t.activity))) || !isFiniteIntegerArray(t.inputs) || !isFiniteIntegerArray(t.outputs)) return false;
    return t.inputs.every((place) => place < (placeCount as number)) && t.outputs.every((place) => place < (placeCount as number));
  })) return false;
  const m = c.markings as Partial<ILPMinerMarkings>;
  if (!isFiniteIntegerArray(m.initial) || !isFiniteIntegerArray(m.final) || m.initial.length !== placeCount || m.final.length !== placeCount) return false;
  const d = c.diagnostics as Partial<ILPMinerDiagnostics>;
  const visited = d.visited_nodes;
  const solved = d.lp_solved;
  const branchNodes = d.branch_and_bound_nodes;
  return typeof d.computed === "boolean" && typeof d.exhausted === "boolean" && Number.isInteger(visited) && (visited as number) >= 0 && Number.isInteger(solved) && (solved as number) >= 0 && Number.isInteger(branchNodes) && (branchNodes as number) >= 0 && typeof d.message === "string";
}
export const isIlpMinerResult = isILPMinerResult;

export interface GeneticMinerTransition {
  activity: number | null;
  inputs: number[];
  outputs: number[];
}
export interface GeneticMinerNet {
  place_count: number;
  source: number;
  sink: number;
  activities: string[];
  transitions: GeneticMinerTransition[];
}
export interface GeneticMinerMarkings { initial: number[]; final: number[]; }
export interface GeneticMinerDiagnostics {
  computed: boolean;
  exhausted: boolean;
  generations_completed: number;
  candidates_evaluated: number;
  population_size: number;
  activity_count: number;
  best_fitness: number;
  seed: number;
  message: string;
}
export interface GeneticMinerResult {
  family: "GENETIC";
  net: GeneticMinerNet;
  markings: GeneticMinerMarkings;
  diagnostics: GeneticMinerDiagnostics;
}
export interface GeneticMinerOptions {
  activity_key?: string;
  seed?: number;
  population_size?: number;
  generations?: number;
  elitism_rate?: number;
  crossover_rate?: number;
  mutation_rate?: number;
  elitism_min_sample?: number;
  max_activities?: number;
  max_states?: number;
}
function isNonnegativeFiniteNumber(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value) && value >= 0;
}
export function isGeneticMinerResult(value: unknown): value is GeneticMinerResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<GeneticMinerResult>;
  if (c.family !== "GENETIC" || !c.net || typeof c.net !== "object" ||
      !c.markings || typeof c.markings !== "object" ||
      !c.diagnostics || typeof c.diagnostics !== "object") return false;
  const n = c.net as Partial<GeneticMinerNet>;
  const placeCount = n.place_count;
  if (!Number.isInteger(placeCount) || (placeCount as number) <= 0 ||
      !Number.isInteger(n.source) || !Number.isInteger(n.sink) ||
      (n.source as number) < 0 || (n.sink as number) < 0 ||
      (n.source as number) >= (placeCount as number) ||
      (n.sink as number) >= (placeCount as number) ||
      !Array.isArray(n.activities) || !n.activities.every((x) => typeof x === "string") ||
      !Array.isArray(n.transitions)) return false;
  const activities = n.activities as string[];
  if (!n.transitions.every((t) => {
    if (!t || typeof t !== "object" ||
        !(t.activity === null || (Number.isInteger(t.activity) &&
          (t.activity as number) >= 0 && (t.activity as number) < activities.length)) ||
        !isFiniteIntegerArray(t.inputs) || !isFiniteIntegerArray(t.outputs)) return false;
    return t.inputs.every((place) => place < (placeCount as number)) &&
      t.outputs.every((place) => place < (placeCount as number));
  })) return false;
  const m = c.markings as Partial<GeneticMinerMarkings>;
  if (!isFiniteIntegerArray(m.initial) || !isFiniteIntegerArray(m.final) ||
      m.initial.length !== placeCount || m.final.length !== placeCount) return false;
  const d = c.diagnostics as Partial<GeneticMinerDiagnostics>;
  return typeof d.computed === "boolean" && typeof d.exhausted === "boolean" &&
    Number.isInteger(d.generations_completed) && (d.generations_completed as number) >= 0 &&
    Number.isInteger(d.candidates_evaluated) && (d.candidates_evaluated as number) >= 0 &&
    Number.isInteger(d.population_size) && (d.population_size as number) >= 0 &&
    Number.isInteger(d.activity_count) && (d.activity_count as number) >= 0 &&
    isNonnegativeFiniteNumber(d.best_fitness) && isNonnegativeFiniteNumber(d.seed) &&
    typeof d.message === "string";
}
export const isGeneticMiner = isGeneticMinerResult;

export type OrganizationalSnaVariant = "handover" | "working_together" | "subcontracting" | "jointactivities";
export interface OrganizationalSnaEdge { source: number; target: number; weight: number; }
export interface OrganizationalSnaDiagnostics {
  computed: boolean; output_truncated: boolean; trace_count: number;
  event_count: number; resource_count: number; edge_count: number; message: string;
}
export interface OrganizationalSnaResult {
  family: "ORGANIZATIONAL_SNA"; variant: OrganizationalSnaVariant;
  directed: boolean;
  resources: string[]; edges: OrganizationalSnaEdge[];
  diagnostics: OrganizationalSnaDiagnostics;
}
export interface OrganizationalSnaOptions {
  sna_variant?: OrganizationalSnaVariant; sna_beta?: number; sna_n?: number;
  sna_metric_normalization?: boolean; sna_max_resources?: number; sna_max_edges?: number;
}

export interface NetworkAnalysisEdge { source: string; target: string; activity: string; count: number; performance: number[]; }
export interface NetworkAnalysisDiagnostics { computed: boolean; output_truncated: boolean; trace_count: number; event_count: number; link_count: number; row_count: number; performance_value_count: number; message: string; }
export interface NetworkAnalysisResult { family: "NETWORK_ANALYSIS"; edges: NetworkAnalysisEdge[]; diagnostics: NetworkAnalysisDiagnostics; }
export interface NetworkAnalysisOptions {
  network_analysis_sorting_attribute?: string; network_analysis_timestamp_attribute?: string;
  network_analysis_source_node_attribute?: string; network_analysis_target_node_attribute?: string;
  network_analysis_edge_attribute?: string; network_analysis_edge_reference?: "_out" | "_in";
  network_analysis_out_relation_attribute?: string; network_analysis_in_relation_attribute?: string;
  network_analysis_include_performance?: boolean; network_analysis_business_hours?: boolean;
  network_analysis_business_hour_slots?: string; network_analysis_max_rows?: number; network_analysis_max_values?: number;
}
export function isNetworkAnalysisResult(value: unknown): value is NetworkAnalysisResult {
  if (!value || typeof value !== "object") return false;
  const v = value as Partial<NetworkAnalysisResult>;
  if (v.family !== "NETWORK_ANALYSIS" || !Array.isArray(v.edges) || !v.diagnostics) return false;
  if (!v.edges.every((e) => !!e && typeof e.source === "string" && typeof e.target === "string" && typeof e.activity === "string" && Number.isInteger(e.count) && e.count >= 0 && Array.isArray(e.performance) && e.performance.every((x) => typeof x === "number" && Number.isFinite(x)))) return false;
  const d = v.diagnostics as Partial<NetworkAnalysisDiagnostics>;
  return typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" && ["trace_count", "event_count", "link_count", "row_count", "performance_value_count"].every((k) => { const n = d[k as keyof NetworkAnalysisDiagnostics]; return typeof n === "number" && Number.isInteger(n) && n >= 0; }) && typeof d.message === "string";
}
export function isOrganizationalSnaResult(value: unknown): value is OrganizationalSnaResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<OrganizationalSnaResult>;
  if (c.family !== "ORGANIZATIONAL_SNA" || typeof c.directed !== "boolean" ||
      !(["handover", "working_together", "subcontracting", "jointactivities"] as unknown[]).includes(c.variant) ||
      !Array.isArray(c.resources) || !c.resources.every((x) => typeof x === "string") ||
      !Array.isArray(c.edges) || !c.diagnostics || typeof c.diagnostics !== "object") return false;
  const resourceCount = (c.resources as string[]).length;
  if (!c.edges.every((e) => !!e && typeof e === "object" && Number.isInteger(e.source) &&
      Number.isInteger(e.target) && typeof e.weight === "number" && Number.isFinite(e.weight) &&
      (e.source as number) >= 0 && (e.target as number) >= 0 &&
       (e.source as number) < resourceCount && (e.target as number) < resourceCount)) return false;
  const d = c.diagnostics as Partial<OrganizationalSnaDiagnostics>;
  return typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" &&
    [d.trace_count, d.event_count, d.resource_count, d.edge_count].every((x) => Number.isInteger(x) && (x as number) >= 0) &&
    typeof d.message === "string";
}
export const isOrganizationalSna = isOrganizationalSnaResult;

export interface RoleDiscoveryResource { name: string; count: number; }
export interface DiscoveredRole { activities: string[]; resources: RoleDiscoveryResource[]; }
export interface RoleDiscoveryDiagnostics { computed: boolean; output_truncated: boolean; trace_count: number; event_count: number; activity_count: number; resource_count: number; role_count: number; message: string; }
export interface RoleDiscoveryResult { family: "ROLE_DISCOVERY"; roles: DiscoveredRole[]; diagnostics: RoleDiscoveryDiagnostics; }
export interface RoleDiscoveryOptions { roles_threshold?: number; roles_max_resources?: number; roles_max_activities?: number; }
export function isRoleDiscoveryResult(value: unknown): value is RoleDiscoveryResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<RoleDiscoveryResult>;
  if (c.family !== "ROLE_DISCOVERY" || !Array.isArray(c.roles) || !c.diagnostics || typeof c.diagnostics !== "object") return false;
  if (!c.roles.every((r) => !!r && typeof r === "object" && Array.isArray(r.activities) && r.activities.every((a) => typeof a === "string") && Array.isArray(r.resources) && r.resources.every((x) => !!x && typeof x === "object" && typeof x.name === "string" && Number.isInteger(x.count) && (x.count as number) > 0))) return false;
  const d = c.diagnostics as Partial<RoleDiscoveryDiagnostics>;
  return typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" && [d.trace_count, d.event_count, d.activity_count, d.resource_count, d.role_count].every((n) => Number.isInteger(n) && (n as number) >= 0) && typeof d.message === "string";
}
export const isRoleDiscovery = isRoleDiscoveryResult;

export type ResourceProfilesMetric = "distinct_activities" | "activity_frequency" | "activity_completions" | "case_completions" | "fraction_case_completions" | "average_workload" | "multitasking" | "average_duration_activity" | "average_case_duration" | "interaction_two_resources" | "social_position";
export interface ResourceProfilesRow { resource: string; activity: string; resource2: string; value: number; count: number; }
export interface ResourceProfilesResult { family: "RESOURCE_PROFILES"; metric: ResourceProfilesMetric; rows: ResourceProfilesRow[]; diagnostics: { computed: boolean; output_truncated: boolean; trace_count: number; event_count: number; resource_count: number; message: string }; }
export interface ResourceProfilesOptions { resource_profiles_metric?: ResourceProfilesMetric; resource_profiles_t1?: string; resource_profiles_t2?: string; resource_profiles_resource?: string; resource_profiles_activity?: string; resource_profiles_resource2?: string; resource_profiles_max_resources?: number; resource_profiles_max_rows?: number; }
export function isResourceProfilesResult(value: unknown): value is ResourceProfilesResult {
  if (!value || typeof value !== "object") return false; const c = value as Partial<ResourceProfilesResult>; const d = c.diagnostics;
  const metrics = new Set<ResourceProfilesMetric>(["distinct_activities", "activity_frequency", "activity_completions", "case_completions", "fraction_case_completions", "average_workload", "multitasking", "average_duration_activity", "average_case_duration", "interaction_two_resources", "social_position"]);
  return c.family === "RESOURCE_PROFILES" && typeof c.metric === "string" && metrics.has(c.metric as ResourceProfilesMetric) && Array.isArray(c.rows) && c.rows.every((r) => !!r && typeof r.resource === "string" && typeof r.activity === "string" && typeof r.resource2 === "string" && typeof r.value === "number" && Number.isFinite(r.value) && Number.isInteger(r.count) && r.count >= 0) && !!d && typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" && [d.trace_count,d.event_count,d.resource_count].every((n) => Number.isInteger(n) && (n as number) >= 0) && typeof d.message === "string";
}

export interface LocalDiagnosticsOptions { local_diagnostics_group_attribute?: string; local_diagnostics_resource_attribute?: string; local_diagnostics_activity_attribute?: string; local_diagnostics_groups?: string; local_diagnostics_max_groups?: number; local_diagnostics_max_resources?: number; local_diagnostics_max_rows?: number; }
export interface LocalDiagnosticsResult { family: "LOCAL_DIAGNOSTICS"; group_relative_focus: Record<string, Record<string, number>>; group_relative_stake: Record<string, Record<string, number>>; group_coverage: Record<string, Record<string, number>>; group_member_contribution: Record<string, Record<string, Record<string, number>>>; diagnostics: { computed: boolean; output_truncated: boolean; trace_count: number; event_count: number; group_count: number; resource_count: number; row_count: number; skipped_events: number; message: string }; }
export type ClusteringVariant = "DMM_LEVEN" | "AVG_LEVEN" | "DMM_VEC" | "AVG_VEC" | "DFG";
export interface ClusteringOptions {
  clustering_trace_attribute?: string;
  clustering_variant?: ClusteringVariant;
  clustering_alpha?: number;
  clustering_percent?: number;
  clustering_max_groups?: number;
  clustering_max_output_nodes?: number;
}
export interface ClusteringNode {
  name: string;
  distance: number;
  size: number;
  leaf: boolean;
  children: ClusteringNode[];
}
export interface ClusteringResult {
  family: "TRACE_ATTRIBUTE_CLUSTERING";
  tree: ClusteringNode;
  leaf_names: string[];
  diagnostics: {
    computed: boolean;
    output_truncated: boolean;
    trace_count: number;
    group_count: number;
    node_count: number;
    message: string;
  };
}
function isClusteringNode(value: unknown): value is ClusteringNode {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const n = value as Partial<ClusteringNode>;
  const children = n.children;
  const size = n.size;
  const leaf = n.leaf;
  return typeof n.name === "string" && typeof n.distance === "number" && Number.isFinite(n.distance) &&
    typeof size === "number" && Number.isInteger(size) && size >= 0 && typeof leaf === "boolean" &&
    Array.isArray(children) && children.every(isClusteringNode) &&
    (leaf ? children.length === 0 : children.length >= 2);
}
export const isClusteringResult = (value: unknown): value is ClusteringResult => {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const x = value as Partial<ClusteringResult>;
  const d = x.diagnostics;
  if (x.family !== "TRACE_ATTRIBUTE_CLUSTERING" || !isClusteringNode(x.tree) ||
      !Array.isArray(x.leaf_names) || !x.leaf_names.every((name) => typeof name === "string") ||
      !d || typeof d !== "object") return false;
  return typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" &&
    [d.trace_count, d.group_count, d.node_count].every((n) => Number.isInteger(n) && (n as number) >= 0) &&
    typeof d.message === "string";
};
export function isLocalDiagnosticsResult(value: unknown): value is LocalDiagnosticsResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<LocalDiagnosticsResult>; const d = c.diagnostics;
  const map = (x: unknown) => !!x && typeof x === "object" && !Array.isArray(x) && Object.values(x as Record<string, unknown>).every((v) => !!v && typeof v === "object" && !Array.isArray(v) && Object.values(v as Record<string, unknown>).every((n) => typeof n === "number" && Number.isFinite(n) && n >= 0 && n <= 1));
  const contribution = (x: unknown) => !!x && typeof x === "object" && !Array.isArray(x) && Object.values(x as Record<string, unknown>).every((resources) => !!resources && typeof resources === "object" && !Array.isArray(resources) && Object.values(resources as Record<string, unknown>).every((activities) => !!activities && typeof activities === "object" && !Array.isArray(activities) && Object.values(activities as Record<string, unknown>).every((n) => typeof n === "number" && Number.isInteger(n) && n >= 0)));
  return c.family === "LOCAL_DIAGNOSTICS" && map(c.group_relative_focus) && map(c.group_relative_stake) && map(c.group_coverage) && contribution(c.group_member_contribution) && !!d && typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" && [d.trace_count,d.event_count,d.group_count,d.resource_count,d.row_count,d.skipped_events].every((n) => Number.isInteger(n) && (n as number) >= 0) && typeof d.message === "string";
}
export const isLocalDiagnostics = isLocalDiagnosticsResult;

export type TraceVariantQueryVariant = "laplace" | "sacofa";
export interface TraceVariantQueryFrequency { activities: string[]; count: number; }
export interface TraceVariantQueryEvent {
  activity: string;
  timestamp: string;
  resource: string;
  lifecycle: string;
  input_order: number;
  attributes?: Record<string, string>;
}
export interface TraceVariantQueryTrace {
  case_id: string;
  events: TraceVariantQueryEvent[];
  attributes?: Record<string, string>;
}
export interface TraceVariantQueryDiagnostics {
  candidate_count: number;
  retained_variant_count: number;
  released_trace_count: number;
  secure_random?: boolean;
  output_truncated: boolean;
  message: string;
}
export interface TraceVariantQueryResult {
  family: "TRACE_VARIANT_QUERY";
  variant: TraceVariantQueryVariant;
  epsilon: number;
  k: number;
  p: number;
  seed: number;
  frequencies: TraceVariantQueryFrequency[];
  diagnostics: TraceVariantQueryDiagnostics;
  log: { traces: TraceVariantQueryTrace[]; attributes?: Record<string, string> };
}
export interface TraceVariantQueryOptions {
  tvq_variant?: TraceVariantQueryVariant;
  tvq_epsilon?: number;
  tvq_k?: number;
  tvq_p?: number;
  tvq_seed?: number;
  tvq_secure_random?: boolean;
  tvq_max_output_traces?: number;
  tvq_max_output_variants?: number;
}
function isStringMap(value: unknown): value is Record<string, string> {
  return !!value && typeof value === "object" && !Array.isArray(value)
    && Object.values(value).every((item) => typeof item === "string");
}
export function isTraceVariantQueryResult(value: unknown): value is TraceVariantQueryResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<TraceVariantQueryResult>;
  if (c.family !== "TRACE_VARIANT_QUERY" || (c.variant !== "laplace" && c.variant !== "sacofa")
      || typeof c.epsilon !== "number" || !Number.isFinite(c.epsilon) || c.epsilon <= 0
      || !Number.isInteger(c.k) || (c.k as number) < 0 || !Number.isInteger(c.p) || (c.p as number) < 1
      || !Number.isInteger(c.seed) || (c.seed as number) < 0 || !Array.isArray(c.frequencies)
      || !c.diagnostics || typeof c.diagnostics !== "object" || !c.log || typeof c.log !== "object") return false;
  if (!c.frequencies.every((entry) => !!entry && typeof entry === "object"
      && Array.isArray(entry.activities) && entry.activities.every((item) => typeof item === "string")
      && Number.isInteger(entry.count) && (entry.count as number) >= 1)) return false;
  const d = c.diagnostics as Partial<TraceVariantQueryDiagnostics>;
  if (!Number.isInteger(d.candidate_count) || (d.candidate_count as number) < 0
      || !Number.isInteger(d.retained_variant_count) || (d.retained_variant_count as number) < 0
      || !Number.isInteger(d.released_trace_count) || (d.released_trace_count as number) < 0
      || (d.secure_random !== undefined && typeof d.secure_random !== "boolean")
      || typeof d.output_truncated !== "boolean" || typeof d.message !== "string") return false;
  const l = c.log as Partial<TraceVariantQueryResult["log"]>;
  if (!Array.isArray(l.traces) || (l.attributes !== undefined && !isStringMap(l.attributes))) return false;
  return l.traces.every((trace) => !!trace && typeof trace === "object"
    && typeof trace.case_id === "string" && Array.isArray(trace.events)
    && (trace.attributes === undefined || isStringMap(trace.attributes))
    && trace.events.every((event) => !!event && typeof event === "object"
      && typeof event.activity === "string" && typeof event.timestamp === "string"
      && typeof event.resource === "string" && typeof event.lifecycle === "string"
      && Number.isInteger(event.input_order)
      && (event.attributes === undefined || isStringMap(event.attributes))));
}
export const isTraceVariantQuery = isTraceVariantQueryResult;

export interface PripelDiagnostics {
  epsilon: number;
  seed: number;
  query_trace_count: number;
  released_trace_count: number;
  exact_matches: number;
  nearest_matches: number;
  synthesized_traces: number;
  synthesized_events: number;
  secure_random?: boolean;
  output_truncated: boolean;
  message: string;
}
export interface PripelResult {
  family: "PRIPEL";
  diagnostics: PripelDiagnostics;
  log: { traces: TraceVariantQueryTrace[]; attributes?: Record<string, string> };
}
export interface PripelOptions {
  pripel_epsilon?: number;
  pripel_blocklist?: string[];
  pripel_seed?: number;
  pripel_secure_random?: boolean;
  pripel_max_output_traces?: number;
  pripel_max_distance?: number;
}
export function isPripelResult(value: unknown): value is PripelResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<PripelResult>;
  if (c.family !== "PRIPEL" || !c.diagnostics || typeof c.diagnostics !== "object"
      || !c.log || typeof c.log !== "object") return false;
  const d = c.diagnostics as Partial<PripelDiagnostics>;
  const nonnegative = (n: unknown): n is number => typeof n === "number" && Number.isInteger(n) && n >= 0;
  if (typeof d.epsilon !== "number" || !Number.isFinite(d.epsilon) || d.epsilon <= 0
      || !nonnegative(d.seed) || !nonnegative(d.query_trace_count)
      || !nonnegative(d.released_trace_count) || !nonnegative(d.exact_matches)
      || !nonnegative(d.nearest_matches) || !nonnegative(d.synthesized_traces)
      || !nonnegative(d.synthesized_events)
      || (d.secure_random !== undefined && typeof d.secure_random !== "boolean")
      || typeof d.output_truncated !== "boolean"
      || typeof d.message !== "string") return false;
  const l = c.log as Partial<PripelResult["log"]>;
  if (!Array.isArray(l.traces) || (l.attributes !== undefined && !isStringMap(l.attributes))) return false;
  return l.traces.every((trace) => !!trace && typeof trace === "object"
    && typeof trace.case_id === "string" && Array.isArray(trace.events)
    && (trace.attributes === undefined || isStringMap(trace.attributes))
    && trace.events.every((event) => !!event && typeof event === "object"
      && typeof event.activity === "string" && typeof event.timestamp === "string"
      && typeof event.resource === "string" && typeof event.lifecycle === "string"
      && Number.isInteger(event.input_order)
      && (event.attributes === undefined || isStringMap(event.attributes))));
}

export type LogToTargetVariant = "next_activity" | "next_time" | "remaining_time";
export interface LogToTargetResult {
  family: "LOG_TO_TARGET";
  variant: LogToTargetVariant;
  classes: string[];
  rows: number[][][];
}
export function isLogToTargetResult(value: unknown): value is LogToTargetResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<LogToTargetResult>;
  return candidate.family === "LOG_TO_TARGET"
    && (candidate.variant === "next_activity" || candidate.variant === "next_time" || candidate.variant === "remaining_time")
    && Array.isArray(candidate.classes) && candidate.classes.every((entry) => typeof entry === "string")
    && Array.isArray(candidate.rows)
    && candidate.rows.every((trace) => Array.isArray(trace) && trace.every((row) => Array.isArray(row) && row.every((entry) => typeof entry === "number" && Number.isFinite(entry))));
}

export type LogToFeaturesVariant = "event_based" | "trace_based" | "temporal";
export interface LogToFeaturesTemporalRow { timestamp: number; values: number[]; }
export interface LogToFeaturesResult {
  family: "LOG_TO_FEATURES";
  variant: LogToFeaturesVariant;
  feature_names: string[];
  event_rows?: number[][][];
  trace_rows?: number[][];
  temporal_rows?: LogToFeaturesTemporalRow[];
}
export function isLogToFeaturesResult(value: unknown): value is LogToFeaturesResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<LogToFeaturesResult>;
  if (c.family !== "LOG_TO_FEATURES" || !(["event_based", "trace_based", "temporal"] as const).includes(c.variant as LogToFeaturesVariant)) return false;
  if (!Array.isArray(c.feature_names) || !c.feature_names.every((x) => typeof x === "string")) return false;
  const rows = (r: unknown): r is number[] => Array.isArray(r) && r.every((x) => typeof x === "number" && Number.isFinite(x));
  if (c.variant === "event_based") return Array.isArray(c.event_rows) && c.event_rows.every((t) => Array.isArray(t) && t.every(rows));
  if (c.variant === "trace_based") return Array.isArray(c.trace_rows) && c.trace_rows.every(rows);
  return Array.isArray(c.temporal_rows) && c.temporal_rows.every((r) => !!r && typeof r === "object" && typeof (r as LogToFeaturesTemporalRow).timestamp === "number" && Number.isFinite((r as LogToFeaturesTemporalRow).timestamp) && rows((r as LogToFeaturesTemporalRow).values));
}

export type EmbeddingVariant = "cases_transformers" | "events_transformers";
export interface EmbeddingItem { id: string; embedding: number[]; }
export interface EmbeddingResult {
  family: "TO_EMBEDDINGS";
  variant: EmbeddingVariant;
  embedding_model: string;
  attribute_key: string;
  dimensions: number;
  items: EmbeddingItem[];
}
export interface EmbeddingSimilarityItem { id: string; score: number; }
export interface EmbeddingSimilarityResult {
  family: "TO_EMBEDDINGS_SIMILARITY";
  variant: EmbeddingVariant;
  target: string;
  top_k: number;
  items: EmbeddingSimilarityItem[];
}
export type EmbeddingApiResult = EmbeddingResult | EmbeddingSimilarityResult;
export function isEmbeddingResult(value: unknown): value is EmbeddingResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<EmbeddingResult>;
  const dimensions = c.dimensions;
  return c.family === "TO_EMBEDDINGS"
    && (c.variant === "cases_transformers" || c.variant === "events_transformers")
    && typeof c.embedding_model === "string" && typeof c.attribute_key === "string"
    && typeof dimensions === "number" && Number.isInteger(dimensions) && dimensions > 0
    && Array.isArray(c.items) && c.items.every((item) => !!item && typeof item === "object"
      && typeof (item as EmbeddingItem).id === "string"
      && Array.isArray((item as EmbeddingItem).embedding)
      && (item as EmbeddingItem).embedding.length === dimensions
      && (item as EmbeddingItem).embedding.every((entry) => typeof entry === "number" && Number.isFinite(entry)));
}
export function isEmbeddingSimilarityResult(value: unknown): value is EmbeddingSimilarityResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<EmbeddingSimilarityResult>;
  const topK = c.top_k;
  return c.family === "TO_EMBEDDINGS_SIMILARITY"
    && (c.variant === "cases_transformers" || c.variant === "events_transformers")
    && typeof c.target === "string" && typeof topK === "number" && Number.isInteger(topK) && topK >= 0
    && Array.isArray(c.items) && c.items.every((item) => !!item && typeof item === "object"
      && typeof (item as EmbeddingSimilarityItem).id === "string"
      && typeof (item as EmbeddingSimilarityItem).score === "number"
      && Number.isFinite((item as EmbeddingSimilarityItem).score));
}
export function isEmbeddingApiResult(value: unknown): value is EmbeddingApiResult {
  return isEmbeddingResult(value) || isEmbeddingSimilarityResult(value);
}

export interface LabelSplittingEvent { activity: string; timestamp: string; resource: string; lifecycle: string; input_order: number; attributes?: Record<string, string>; }
export interface LabelSplittingTrace { case_id: string; events: LabelSplittingEvent[]; attributes?: Record<string, string>; }
export interface LabelSplittingResult { family: "LABEL_SPLITTING"; variant: "contextual"; log: { traces: LabelSplittingTrace[]; attributes?: Record<string, string> }; }
export function isLabelSplittingResult(value: unknown): value is LabelSplittingResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<LabelSplittingResult>;
  const strings = (item: unknown): item is Record<string, string> => !!item && typeof item === "object" && !Array.isArray(item)
    && Object.values(item).every((value) => typeof value === "string");
  if (c.family !== "LABEL_SPLITTING" || c.variant !== "contextual" || !c.log || typeof c.log !== "object" || !Array.isArray(c.log.traces)) return false;
  if (c.log.attributes !== undefined && !strings(c.log.attributes)) return false;
  return c.log.traces.every((trace) => !!trace && typeof trace === "object" && typeof trace.case_id === "string" && Array.isArray(trace.events)
    && (trace.attributes === undefined || strings(trace.attributes))
    && trace.events.every((event) => !!event && typeof event === "object" && typeof event.activity === "string" && typeof event.timestamp === "string" && typeof event.resource === "string" && typeof event.lifecycle === "string" && Number.isInteger(event.input_order)
      && (event.attributes === undefined || strings(event.attributes))));
}

export interface TrieNode {
  label: string;
  depth: number;
  final: boolean;
  children: TrieNode[];
}
export interface TrieResult {
  family: "LOG_TO_TRIE";
  root: TrieNode;
}
export interface TriePetriConversionTransition {
  activity: number | null;
  inputs: number[];
  outputs: number[];
}
export interface TriePetriConversionResult {
  family: "TRIE_CONVERSION";
  variant: "TO_PETRI_NET";
  source: number;
  sink: number;
  activities: string[];
  place_count: number;
  transitions: TriePetriConversionTransition[];
}
function isTrieNode(value: unknown): value is TrieNode {
  if (!value || typeof value !== "object") return false;
  const node = value as Partial<TrieNode>;
  return typeof node.label === "string" && typeof node.depth === "number" && Number.isInteger(node.depth) && node.depth >= 0
    && typeof node.final === "boolean" && Array.isArray(node.children)
    && node.children.every(isTrieNode);
}
export function isTrieResult(value: unknown): value is TrieResult {
  return !!value && typeof value === "object" && (value as Partial<TrieResult>).family === "LOG_TO_TRIE"
    && isTrieNode((value as Partial<TrieResult>).root);
}
export function isTriePetriConversionResult(value: unknown): value is TriePetriConversionResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<TriePetriConversionResult>;
  if (candidate.family !== "TRIE_CONVERSION" || candidate.variant !== "TO_PETRI_NET" ||
      !Number.isInteger(candidate.source) || (candidate.source as number) < 0 ||
      !Number.isInteger(candidate.sink) || (candidate.sink as number) < 0 ||
      !Number.isInteger(candidate.place_count) || (candidate.place_count as number) <= 0 ||
      !Array.isArray(candidate.activities) || !Array.isArray(candidate.transitions)) return false;
  const placeCount = candidate.place_count as number;
  const activities = candidate.activities as string[];
  if ((candidate.source as number) >= placeCount || (candidate.sink as number) >= placeCount ||
      !activities.every((activity) => typeof activity === "string")) return false;
  return candidate.transitions.every((transition) => {
    if (!transition || typeof transition !== "object") return false;
    const item = transition as Partial<TriePetriConversionTransition>;
    const places = (values: unknown): values is number[] => Array.isArray(values) &&
      values.every((place) => Number.isInteger(place) && (place as number) >= 0 && (place as number) < placeCount);
    return (item.activity === null ||
      (Number.isInteger(item.activity) && (item.activity as number) >= 0 &&
       (item.activity as number) < activities.length)) &&
      places(item.inputs) && places(item.outputs);
  });
}

export interface OpenPathInterval {
  start: number;
  end: number;
  case_id: string;
  source_activity: string;
  target_activity: string;
  trace_index: number;
  source_event_index: number;
  target_event_index: number;
}
export interface IntervalTreeResult {
  family: "LOG_TO_INTERVAL_TREE";
  intervals: OpenPathInterval[];
}
export function isIntervalTreeResult(value: unknown): value is IntervalTreeResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<IntervalTreeResult>;
  return candidate.family === "LOG_TO_INTERVAL_TREE" && Array.isArray(candidate.intervals)
    && candidate.intervals.every((interval) => !!interval && typeof interval === "object"
      && Number.isFinite((interval as OpenPathInterval).start)
      && Number.isFinite((interval as OpenPathInterval).end)
      && typeof (interval as OpenPathInterval).case_id === "string"
      && typeof (interval as OpenPathInterval).source_activity === "string"
      && typeof (interval as OpenPathInterval).target_activity === "string"
      && Number.isInteger((interval as OpenPathInterval).trace_index)
      && Number.isInteger((interval as OpenPathInterval).source_event_index)
      && Number.isInteger((interval as OpenPathInterval).target_event_index));
}

export interface CaseRelationsEvent {
  activity: string;
  timestamp: string;
  resource: string;
  lifecycle: string;
  input_order: number;
  attributes?: Record<string, string>;
}
export interface CaseRelationsTrace {
  case_id: string;
  events: CaseRelationsEvent[];
  attributes?: Record<string, string>;
}
export interface CaseRelationsResult {
  family: "MERGE_CASE_RELATIONS";
  log: { traces: CaseRelationsTrace[]; attributes?: Record<string, string> };
}
function isStringRecord(value: unknown): value is Record<string, string> {
  return !!value && typeof value === "object" && !Array.isArray(value)
    && Object.values(value).every((item) => typeof item === "string");
}
export function isCaseRelationsResult(value: unknown): value is CaseRelationsResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<CaseRelationsResult>;
  if (candidate.family !== "MERGE_CASE_RELATIONS" || !candidate.log || typeof candidate.log !== "object" || !Array.isArray(candidate.log.traces)) return false;
  if (candidate.log.attributes !== undefined && !isStringRecord(candidate.log.attributes)) return false;
  return candidate.log.traces.every((trace) => {
    if (!trace || typeof trace !== "object" || typeof trace.case_id !== "string" || !Array.isArray(trace.events)) return false;
    if (trace.attributes !== undefined && !isStringRecord(trace.attributes)) return false;
    return trace.events.every((event) => event && typeof event === "object" && typeof event.activity === "string" && typeof event.timestamp === "string" && typeof event.resource === "string" && typeof event.lifecycle === "string" && Number.isInteger(event.input_order) && (event.attributes === undefined || isStringRecord(event.attributes)));
  });
}

export interface LinkAnalysisEvent {
  index: number;
  trace_index: number;
  event_index: number;
  case_id: string;
  activity: string;
  timestamp: string;
  value: string;
}
export interface LinkAnalysisLink {
  source: LinkAnalysisEvent;
  target: LinkAnalysisEvent;
}
export interface LinkAnalysisResult {
  family: "LINK_ANALYSIS";
  variant: "CLASSIC";
  out_column: string;
  in_column: string;
  sorting_column: string;
  look_forward: boolean;
  keep_first_occurrence: boolean;
  propagate: boolean;
  event_count: number;
  links: LinkAnalysisLink[];
}
function isLinkAnalysisEvent(value: unknown): value is LinkAnalysisEvent {
  if (!value || typeof value !== "object") return false;
  const event = value as Partial<LinkAnalysisEvent>;
  return Number.isInteger(event.index) && Number.isInteger(event.trace_index) &&
    Number.isInteger(event.event_index) && typeof event.case_id === "string" &&
    typeof event.activity === "string" && typeof event.timestamp === "string" &&
    typeof event.value === "string";
}
export function isLinkAnalysisResult(value: unknown): value is LinkAnalysisResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<LinkAnalysisResult>;
  return result.family === "LINK_ANALYSIS" && result.variant === "CLASSIC" &&
    typeof result.out_column === "string" && typeof result.in_column === "string" &&
    typeof result.sorting_column === "string" && typeof result.look_forward === "boolean" &&
    typeof result.keep_first_occurrence === "boolean" && typeof result.propagate === "boolean" &&
    typeof result.event_count === "number" && Number.isInteger(result.event_count) && result.event_count >= 0 &&
    Array.isArray(result.links) && result.links.every((link) => !!link &&
      typeof link === "object" && isLinkAnalysisEvent(link.source) &&
      isLinkAnalysisEvent(link.target));
}

export type TimestampInterleavingDirection = "LR" | "RL";
export interface TimestampInterleaving {
  source_activity: string;
  target_activity: string;
  source_timestamp: string;
  target_timestamp: string;
  left_index: number;
  right_index: number;
  direction: TimestampInterleavingDirection;
  timestamp_diff: number;
}
export interface TimestampInterleavingsResult {
  family: "TIMESTAMP_INTERLEAVINGS";
  variant: "TIMESTAMP";
  timestamp_key: string;
  max_interleavings: number;
  interleavings: TimestampInterleaving[];
}
export function isTimestampInterleavingsResult(value: unknown): value is TimestampInterleavingsResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<TimestampInterleavingsResult>;
  return result.family === "TIMESTAMP_INTERLEAVINGS" && result.variant === "TIMESTAMP" &&
    typeof result.timestamp_key === "string" &&
    typeof result.max_interleavings === "number" && Number.isInteger(result.max_interleavings) && result.max_interleavings > 0 &&
    Array.isArray(result.interleavings) && result.interleavings.every((row) => {
      if (!row || typeof row !== "object") return false;
      const item = row as Partial<TimestampInterleaving>;
      return typeof item.source_activity === "string" && typeof item.target_activity === "string" &&
        typeof item.source_timestamp === "string" && typeof item.target_timestamp === "string" &&
        typeof item.left_index === "number" && Number.isInteger(item.left_index) && item.left_index >= 0 &&
        typeof item.right_index === "number" && Number.isInteger(item.right_index) && item.right_index >= 0 &&
        (item.direction === "LR" || item.direction === "RL") &&
        typeof item.timestamp_diff === "number" && Number.isFinite(item.timestamp_diff);
    });
}

export interface RandomTreeNode {
  operator: "activity" | "tau" | "sequence" | "exclusive" | "parallel" | "loop";
  activity?: string;
  children: RandomTreeNode[];
}
export interface RandomTreeResult {
  family: "RANDOM_TREE";
  variant: "BASIC" | "PTANDLOGGENERATOR";
  activities: string[];
  tree: RandomTreeNode;
}
function isRandomTreeNode(value: unknown, activities: string[], root = false,
                          basicRoot = false): value is RandomTreeNode {
  if (!value || typeof value !== "object") return false;
  const node = value as Partial<RandomTreeNode>;
  if (!["activity", "tau", "sequence", "exclusive", "parallel", "loop"].includes(String(node.operator)) || !Array.isArray(node.children)) return false;
  if (node.operator === "activity") return node.children.length === 0 && typeof node.activity === "string" && activities.includes(node.activity);
  if (node.activity !== undefined) return false;
  if (node.operator === "tau") return node.children.length === 0;
  if (node.operator === "loop") {
    if (basicRoot && (node.children.length !== 3 ||
        node.children[2]?.operator !== "tau")) return false;
    return node.children.length >= 2 &&
      node.children.every((child) => isRandomTreeNode(child, activities, false,
                                                       basicRoot));
  }
  const minimumChildren = 2;
  if (node.children.length < minimumChildren ||
      (root && basicRoot && (node.operator !== "sequence" || node.children.length !== 3))) return false;
  if (!node.children.every((child) => isRandomTreeNode(child, activities, false,
                                                       basicRoot))) return false;
  return !root || !basicRoot ||
    (node.children[0].operator === "activity" && node.children[2].operator === "activity");
}
export function isRandomTreeResult(value: unknown): value is RandomTreeResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<RandomTreeResult>;
  if (candidate.family !== "RANDOM_TREE" ||
      !["BASIC", "PTANDLOGGENERATOR"].includes(String(candidate.variant)) ||
      !Array.isArray(candidate.activities)) return false;
  if (candidate.activities.length === 0 || candidate.activities.some((activity) => typeof activity !== "string" || activity.length === 0) || new Set(candidate.activities).size !== candidate.activities.length) return false;
  return isRandomTreeNode(candidate.tree, candidate.activities, true,
                         candidate.variant === "BASIC");
}

export type OcelDiscoveryVariant = "etot" | "ocdfg" | "ocpn" | "otg" | "saw";
export type OcelConformanceVariant = "etot" | "ocdfg" | "otg";
export type OcelSplitVariant = "CONNECTED_COMPONENTS" | "ANCESTORS_DESCENDANTS";
export interface OcelDiscoveryResult {
  family: "OCEL_DISCOVERY";
  variant: OcelDiscoveryVariant;
  result: Record<string, unknown>;
}
export interface OcelConformanceResult {
  family: "OCEL_CONFORMANCE";
  variant: OcelConformanceVariant;
  result: Record<string, unknown>;
}
export type OcelFilterVariant =
  | "EVENT_ATTRIBUTE" | "OBJECT_ATTRIBUTE" | "OBJECT_TYPES" | "OBJECTS"
  | "EVENTS" | "TIMESTAMP" | "ACTIVITY_TYPE_MATCHING" | "OBJECT_COUNT"
  | "START_EVENTS" | "END_EVENTS" | "ACTIVITIES_OBJECT_TYPE" | "CC_OBJECT"
  | "CC_LENGTH" | "CC_OBJECT_TYPE" | "CC_ACTIVITY";
export interface OcelFilterResult {
  family: "OCEL_FILTER";
  variant: OcelFilterVariant;
  log: Record<string, unknown>;
}
export interface OcelSplitResult {
  family: "OCEL_SPLIT";
  variant: OcelSplitVariant;
  logs: Array<Record<string, unknown>>;
}
export type OcelPlayoutVariant = "EXTENSIVE";
export interface OcelPetriPlayoutResult {
  family: "OCEL_PLAYOUT";
  variant: OcelPlayoutVariant;
  log: Record<string, unknown>;
}
export interface OcelCausalNetPlayoutResult {
  family: "OC_CAUSAL_NET_PLAYOUT";
  variant: OcelPlayoutVariant;
  log: Record<string, unknown>;
}
export type OcelPlayoutResult = OcelPetriPlayoutResult | OcelCausalNetPlayoutResult;
function isOcelRecord(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}
export function isOcelDiscoveryResult(value: unknown): value is OcelDiscoveryResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_DISCOVERY") return false;
  return ["etot", "ocdfg", "ocpn", "otg", "saw"].includes(String(value.variant))
    && isOcelRecord(value.result);
}
export function isOcelConformanceResult(value: unknown): value is OcelConformanceResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_CONFORMANCE") return false;
  return ["etot", "ocdfg", "otg"].includes(String(value.variant))
    && isOcelRecord(value.result);
}
export function isOcelFilterResult(value: unknown): value is OcelFilterResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_FILTER" ||
      !["EVENT_ATTRIBUTE", "OBJECT_ATTRIBUTE", "OBJECT_TYPES", "OBJECTS",
        "EVENTS", "TIMESTAMP", "ACTIVITY_TYPE_MATCHING", "OBJECT_COUNT",
        "START_EVENTS", "END_EVENTS", "ACTIVITIES_OBJECT_TYPE", "CC_OBJECT",
        "CC_LENGTH", "CC_OBJECT_TYPE", "CC_ACTIVITY"].includes(String(value.variant)) ||
      !isOcelRecord(value.log)) return false;
  const log = value.log;
  return isOcelRecord(log["ocel:events"]) && isOcelRecord(log["ocel:objects"]);
}
export function isOcelSplitResult(value: unknown): value is OcelSplitResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_SPLIT" ||
      !["CONNECTED_COMPONENTS", "ANCESTORS_DESCENDANTS"].includes(String(value.variant)) ||
      !Array.isArray(value.logs)) return false;
  return value.logs.every((log) => isOcelRecord(log));
}
export function isOcelPlayoutResult(value: unknown): value is OcelPlayoutResult {
  if (!isOcelRecord(value) ||
      (value.family !== "OCEL_PLAYOUT" && value.family !== "OC_CAUSAL_NET_PLAYOUT") ||
      value.variant !== "EXTENSIVE") return false;
  return isOcelRecord(value.log);
}
export type OcelFeatureVariant = "EVENTS" | "OBJECTS" | "EVENTS_OBJECTS";
export interface OcelFeatureResult {
  family: "OCEL_FEATURES";
  variant: OcelFeatureVariant;
  row_ids: string[];
  row_types?: string[];
  feature_names: string[];
  values: number[][];
}
export function isOcelFeatureResult(value: unknown): value is OcelFeatureResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_FEATURES" ||
      !["EVENTS", "OBJECTS", "EVENTS_OBJECTS"].includes(String(value.variant)) ||
      !Array.isArray(value.row_ids) || !value.row_ids.every((id) => typeof id === "string") ||
      !Array.isArray(value.feature_names) ||
      !value.feature_names.every((name) => typeof name === "string") ||
      !Array.isArray(value.values)) return false;
  const rowIds = value.row_ids;
  const featureNames = value.feature_names;
  const values = value.values;
  if (value.row_types !== undefined &&
      (!Array.isArray(value.row_types) ||
       !value.row_types.every((type) => typeof type === "string") ||
       value.row_types.length !== rowIds.length)) return false;
  return values.length === rowIds.length &&
    values.every((row) => Array.isArray(row) &&
      row.length === featureNames.length &&
      row.every((entry) => typeof entry === "number" && Number.isFinite(entry)));
}

export interface ProcessCubeTable {
  case_ids: string[];
  columns: Record<string, number[] | string[]>;
  categorical_columns?: Record<string, string[]>;
}
export interface ProcessCubeBin {
  label: string;
  lower: number;
  upper: number;
  numeric: boolean;
}
export interface ProcessCubeResult {
  family: "PROCESS_CUBE";
  x_dimension: string;
  y_dimension: string;
  aggregation: string;
  x_bins: ProcessCubeBin[];
  y_bins: ProcessCubeBin[];
  x_edges: number[];
  y_edges: number[];
  cells: Array<Array<number | null>>;
  case_ids: Array<Array<string[]>>;
}
function isProcessCubeBins(value: unknown): value is ProcessCubeBin[] {
  return Array.isArray(value) && value.every((bin) => isOcelRecord(bin) &&
    typeof bin.label === "string" && finite(bin.lower) && finite(bin.upper) &&
    typeof bin.numeric === "boolean");
}
export function isProcessCubeTable(value: unknown): value is ProcessCubeTable {
  if (!isOcelRecord(value) || !Array.isArray(value.case_ids) ||
      !value.case_ids.every((id) => typeof id === "string") ||
      !isOcelRecord(value.columns)) return false;
  const columns = value.columns;
  const rows = value.case_ids.length;
  const isColumn = (column: unknown): column is number[] | string[] =>
    Array.isArray(column) && column.length === rows &&
    (column.every((entry) => finite(entry)) ||
     column.every((entry) => typeof entry === "string"));
  if (!Object.values(columns).every(isColumn)) return false;
  if (value.categorical_columns !== undefined) {
    if (!isOcelRecord(value.categorical_columns) ||
        Object.keys(value.categorical_columns).some((name) => name in columns) ||
        !Object.values(value.categorical_columns).every((column) =>
          Array.isArray(column) && column.length === rows &&
          column.every((entry) => typeof entry === "string"))) return false;
  }
  return true;
}
export function isProcessCubeResult(value: unknown): value is ProcessCubeResult {
  if (!isOcelRecord(value) || value.family !== "PROCESS_CUBE" ||
      typeof value.x_dimension !== "string" || typeof value.y_dimension !== "string" ||
      typeof value.aggregation !== "string") return false;
  const xBins = value.x_bins;
  const yBins = value.y_bins;
  if (!isProcessCubeBins(xBins) || !isProcessCubeBins(yBins) ||
      !Array.isArray(value.x_edges) || !value.x_edges.every((edge) => finite(edge)) ||
      !Array.isArray(value.y_edges) || !value.y_edges.every((edge) => finite(edge)) ||
      !Array.isArray(value.cells) || !Array.isArray(value.case_ids) ||
      value.cells.length !== xBins.length || value.case_ids.length !== xBins.length) return false;
  if (!value.cells.every((row) => Array.isArray(row) && row.length === yBins.length &&
      row.every((cell) => cell === null || finite(cell)))) return false;
  return value.case_ids.every((row) => Array.isArray(row) && row.length === yBins.length &&
    row.every((cell) => Array.isArray(cell) && cell.every((id) => typeof id === "string")));
}

export type OcelStatisticsVariant =
  | "OBJECT_TYPE_ACTIVITIES"
  | "OBJECTS_OT_COUNT"
  | "ACTIVITY_ASSOCIATIONS"
  | "OBJECT_TYPE_ACTIVITY_ASSOCIATIONS"
  | "EDGE_ASSOCIATIONS";
export type OcelAssociationPair = [string, string];
export interface OcelEdgeAssociationMetric {
  event_couples: Array<[string, string]>;
  unique_objects: string[];
  event_objects: Array<[string, string, string]>;
  performance: number[];
}
export interface OcelStatisticsResult {
  family: "OCEL_STATISTICS";
  variant: OcelStatisticsVariant;
  object_type_activities?: Record<string, string[]>;
  objects_ot_count?: Record<string, Record<string, number>>;
  associations?: Record<string, OcelAssociationPair[]> |
    Record<string, Record<string, OcelAssociationPair[]>>;
  edges?: Record<string, Record<string, OcelEdgeAssociationMetric>>;
}
export function isOcelStatisticsResult(value: unknown): value is OcelStatisticsResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_STATISTICS" ||
      !["OBJECT_TYPE_ACTIVITIES", "OBJECTS_OT_COUNT", "ACTIVITY_ASSOCIATIONS",
        "OBJECT_TYPE_ACTIVITY_ASSOCIATIONS", "EDGE_ASSOCIATIONS"].includes(String(value.variant))) return false;
  if (value.variant === "OBJECT_TYPE_ACTIVITIES") {
    if (!isOcelRecord(value.object_type_activities)) return false;
    return Object.values(value.object_type_activities).every((activities) =>
      Array.isArray(activities) && activities.every((activity) => typeof activity === "string"));
  }
  if (value.variant === "OBJECTS_OT_COUNT") {
    if (!isOcelRecord(value.objects_ot_count)) return false;
    return Object.values(value.objects_ot_count).every((counts) =>
      isOcelRecord(counts) && Object.values(counts).every((count) =>
        typeof count === "number" && Number.isInteger(count) && count >= 0));
  }
  if (value.variant === "ACTIVITY_ASSOCIATIONS") {
    if (!isOcelRecord(value.associations)) return false;
    return Object.values(value.associations).every((pairs) =>
      Array.isArray(pairs) && pairs.every((pair) => Array.isArray(pair) && pair.length === 2 &&
        pair.every((item) => typeof item === "string")));
  }
  if (value.variant === "OBJECT_TYPE_ACTIVITY_ASSOCIATIONS") {
    if (!isOcelRecord(value.associations)) return false;
    return Object.values(value.associations).every((activities) =>
      isOcelRecord(activities) && Object.values(activities).every((pairs) =>
        Array.isArray(pairs) && pairs.every((pair) => Array.isArray(pair) && pair.length === 2 &&
          pair.every((item) => typeof item === "string"))));
  }
  if (!isOcelRecord(value.edges)) return false;
  return Object.values(value.edges).every((edges) =>
    isOcelRecord(edges) && Object.values(edges).every((metric) =>
      isOcelRecord(metric) && Array.isArray(metric.event_couples) &&
      metric.event_couples.every((pair) => Array.isArray(pair) && pair.length === 2 &&
        pair.every((item) => typeof item === "string")) &&
      Array.isArray(metric.unique_objects) &&
      metric.unique_objects.every((item) => typeof item === "string") &&
      Array.isArray(metric.event_objects) &&
      metric.event_objects.every((triple) => Array.isArray(triple) && triple.length === 3 &&
        triple.every((item) => typeof item === "string")) &&
      Array.isArray(metric.performance) &&
      metric.performance.every((item) => typeof item === "number" && Number.isFinite(item))));
}

export type TransitionSystemView = "sequence" | "set" | "multiset";
export type TransitionSystemDirection = "forward" | "backward";
export interface TransitionOccurrence {
  trace_index: number;
  event_index: number;
}
export interface TransitionSystemState {
  id: number;
  /** Sequence/set payloads are activity ids; multiset uses activity/count pairs. */
  payload: number[] | Array<{ activity: number; count: number }>;
  start_count: number;
  end_count: number;
  incoming_occurrences?: TransitionOccurrence[];
  outgoing_occurrences?: TransitionOccurrence[];
}
export interface TransitionSystemTransition {
  source: number;
  target: number;
  activity: number;
  count: number;
  occurrences?: TransitionOccurrence[];
}
export interface TransitionSystemExport {
  view: TransitionSystemView;
  direction: TransitionSystemDirection;
  window: number;
  include_data: boolean;
  states: TransitionSystemState[];
  transitions: TransitionSystemTransition[];
}

/** Timing statistics for an ordered activity pair in the temporal profile. */
export interface TemporalProfileEntry {
  /** Source activity id; indexes into MineraExport.activities. */
  source: number;
  /** Target activity id; indexes into MineraExport.activities. */
  target: number;
  count: number;
  mean_seconds: number;
  standard_deviation_seconds: number;
}

export interface MinimumSelfDistanceEntry {
  activity: number;
  distance: number;
  witnesses: number[];
}

export interface VariantStat {
  activities: number[];
  count: number;
  /**
   * Case throughput-time statistics for this variant, in seconds. Absent in
   * exports from older minera versions and on logs with no parseable
   * timestamps; `duration.count === 0` likewise means no timing was observed.
   */
  duration?: DurationStats;
}

export type ProcessTreeOperator =
  | "sequence"
  | "exclusive"
  | "parallel"
  | "loop"
  | "activity"
  | "tau";

export interface ProcessTreeNode {
  operator: ProcessTreeOperator;
  /** Present iff operator === "activity"; indexes into activities. */
  activity?: number;
  children?: ProcessTreeNode[];
}

export interface PetriTransition {
  /** Activity id this transition fires; null for a silent (tau) transition. */
  activity: number | null;
  /** Place ids consumed when the transition fires. */
  inputs: number[];
  /** Place ids produced when the transition fires. */
  outputs: number[];
}

export interface PetriNetModel {
  /** Places are ids 0..place_count-1. */
  place_count: number;
  source: number;
  sink: number;
  transitions: PetriTransition[];
}

export interface PetriUsageRecord {
  id: number;
  log1_occurrences: number;
  log2_occurrences: number;
  total_occurrences: number;
  direction: number;
}
export interface PetriArcUsageRecord extends PetriUsageRecord {
  transition: number;
  place: number;
  input: boolean;
}
export interface PetriUsageComparison {
  family: "PETRI_USAGE_COMPARISON";
  places: PetriUsageRecord[];
  transitions: PetriUsageRecord[];
  arcs: PetriArcUsageRecord[];
}

/** Recursive POWL model emitted by the POWL discovery miner. */
export type PowlNode =
  | { kind: "transition"; activity: number; frequency: { minimum: number; maximum: number | null } }
  | { kind: "silent" }
  | { kind: "xor" | "loop"; children: PowlNode[] }
  | { kind: "strict_partial_order"; children: PowlNode[]; order: Array<{ source: number; target: number }> };

/** Runtime validation for the recursive model used by the POWL renderer. */
export function isPowlNode(value: unknown, activityCount?: number, depth = 0): value is PowlNode {
  // Native discovery bounds recursive model traversal at 512 levels.
  if (depth > 512 || typeof value !== "object" || value === null) return false;
  const node = value as Record<string, unknown>;
  if (node.kind === "silent") return true;
  if (node.kind === "transition") {
    const frequency = node.frequency;
    if (typeof node.activity !== "number" || !Number.isInteger(node.activity) || node.activity < 0) return false;
    if (activityCount !== undefined && node.activity >= activityCount) return false;
    if (typeof frequency !== "object" || frequency === null) return false;
    const range = frequency as Record<string, unknown>;
    if (typeof range.minimum !== "number" || !Number.isInteger(range.minimum) ||
        (range.minimum !== 0 && range.minimum !== 1)) return false;
    if (range.maximum !== null && (typeof range.maximum !== "number" || !Number.isInteger(range.maximum) || range.maximum !== 1 || range.maximum < range.minimum)) return false;
    return true;
  }
  if (node.kind === "xor" || node.kind === "loop") {
    const children = node.children;
    if (!Array.isArray(children) ||
        (node.kind === "xor" ? children.length < 2 : children.length !== 2)) return false;
    return children.every((child) => isPowlNode(child, activityCount, depth + 1));
  }
  if (node.kind !== "strict_partial_order") return false;
  const children = node.children;
  const order = node.order;
  if (!Array.isArray(children) || !Array.isArray(order) ||
      !children.every((child) => isPowlNode(child, activityCount, depth + 1))) return false;
  const adjacency: number[][] = children.map(() => []);
  if (!order.every((edge) => {
    if (typeof edge !== "object" || edge === null) return false;
    const pair = edge as Record<string, unknown>;
    const valid = typeof pair.source === "number" && Number.isInteger(pair.source) &&
      typeof pair.target === "number" && Number.isInteger(pair.target) &&
      pair.source >= 0 && pair.target >= 0 &&
      pair.source < children.length && pair.target < children.length &&
      pair.source !== pair.target;
    if (valid) adjacency[pair.source as number].push(pair.target as number);
    return valid;
  })) return false;
  // Native strict partial orders reject cyclic relations (including self
  // loops, handled above), while an empty order is valid for zero children.
  const indegree = adjacency.map(() => 0);
  for (const targets of adjacency) for (const target of targets) indegree[target]++;
  const ready = indegree.reduce<number[]>((queue, degree, index) => {
    if (degree === 0) queue.push(index);
    return queue;
  }, []);
  let visited = 0;
  while (ready.length > 0) {
    const source = ready.pop() as number;
    visited++;
    for (const target of adjacency[source]) if (--indegree[target] === 0) ready.push(target);
  }
  return visited === children.length;
}

/** One ordered relation in a classic log-skeleton model. */
export interface LogSkeletonRelation {
  source: number;
  target: number;
}

/** The six constraint families emitted by log-skeleton discovery. */
export interface LogSkeletonModel {
  equivalence: LogSkeletonRelation[];
  always_after: LogSkeletonRelation[];
  always_before: LogSkeletonRelation[];
  never_together: LogSkeletonRelation[];
  directly_follows: LogSkeletonRelation[];
  /** Allowed occurrence counts per activity id (as JSON object keys). */
  activ_freq: Record<string, number[]>;
}

export interface LogSkeletonPairDeviation {
  constraint: string;
  source: number;
  target: number;
}

export interface LogSkeletonFrequencyDeviation {
  constraint: string;
  activity: number;
  observed_frequency: number;
  allowed_frequencies: number[];
}

export type LogSkeletonDeviation =
  | LogSkeletonPairDeviation
  | LogSkeletonFrequencyDeviation;

/** Per-trace conformance result for a log-skeleton model. */
export interface LogSkeletonConformanceTrace {
  case_id: string;
  deviations: LogSkeletonDeviation[];
  no_dev_total: number;
  no_constr_total: number;
  dev_fitness: number;
  is_fit: boolean;
}

export const DECLARE_TEMPLATES = [
  "existence", "exactly_one", "init", "responded_existence", "response",
  "precedence", "succession", "altresponse", "altprecedence", "altsuccession",
  "chainresponse", "chainprecedence", "chainsuccession", "absence", "coexistence",
  "noncoexistence", "nonsuccession", "nonchainsuccession",
] as const;
export type DeclareTemplate = (typeof DECLARE_TEMPLATES)[number];
export interface DeclareUnaryRule {
  activity: number;
  support: number;
  confidence: number;
}
export interface DeclareBinaryRule {
  source: number;
  target: number;
  support: number;
  confidence: number;
}
export type DeclareRule = DeclareUnaryRule | DeclareBinaryRule;
export type DeclareModel = Partial<Record<DeclareTemplate, DeclareRule[]>>;
export type DeclareDeviation =
  | { template: DeclareTemplate | string; activity: number }
  | { template: DeclareTemplate | string; source: number; target: number };
export interface DeclareConformanceTrace {
  trace_index?: number;
  case_id: string;
  deviations: DeclareDeviation[];
  no_constr_total: number;
  no_dev_total: number;
  dev_fitness: number;
  is_fit: boolean;
}

/** One place's replay decoration: tokens through it, and how long they rest. */
export interface PetriPlaceDecoration {
  /** Tokens consumed from this place across the cleanly replaying cases. */
  count: number;
  /** Waiting time between a token's production and its consumption. */
  wait: DurationStats;
}

/**
 * Frequency/performance annotations for the Petri net, measured by token
 * replay of the log's cleanly fitting cases; indices match `petri_net`.
 */
export interface PetriDecoration {
  /** Cases whose replay was clean — the ones the annotations describe. */
  decorated_traces: number;
  trace_count: number;
  /** Firing count per transition, aligned with petri_net.transitions. */
  transitions: number[];
  /** Per place id: token count and waiting time. */
  places: PetriPlaceDecoration[];
}

export interface HeuristicEdge {
  source: number;
  target: number;
  /** Dependency measure in (-1, 1]: how reliably source is followed by target. */
  dependency: number;
  /** Direct successions observed (|source > target|). */
  frequency: number;
  /** Frequency count or mean wait, according to the selected decoration. */
  representation?: number;
  /** True when classic discovery rescued this relation as a length-two loop. */
  length_two_loop?: boolean;
}

export interface HeuristicsNet {
  variant?: "classic" | "plusplus";
  decoration?: "frequency" | "performance";
  performance_aggregation?:
    | "mean"
    | "median"
    | "min"
    | "max"
    | "stdev"
    | "sum"
    | "raw_values"
    | "all";
  edges: HeuristicEdge[];
  start_activities: number[];
  end_activities: number[];
  active_activities?: number[];
  activity_statistics?: {
    activity: number;
    occurrences: number;
    start_occurrences: number;
    end_occurrences: number;
    mean_sojourn_seconds: number;
  }[];
  relation_statistics?: {
    source: number;
    target: number;
    eventually_follows_frequency: number;
    direct_adjacency_frequency: number;
    mean_wait_seconds: number;
    raw_frequency?: number;
    cleaned_frequency?: number;
    representation?: number;
    length_two_loop?: boolean;
    wait_samples?: number[];
    median_wait_seconds?: number;
    min_wait_seconds?: number;
    max_wait_seconds?: number;
    stdev_wait_seconds?: number;
    sum_wait_seconds?: number;
  }[];
  concurrency_statistics?: {
    first: number;
    second: number;
    frequency: number;
  }[];
  raw_activity_statistics?: {
    activity: number;
    occurrences: number;
    mean_sojourn_seconds: number;
  }[];
  raw_concurrency_statistics?: {
    first: number;
    second: number;
    frequency: number;
  }[];
  raw_dfg?: { source: number; target: number; count: number }[];
  cleaned_dfg?: { source: number; target: number; count: number }[];
  window_two_dfg?: { source: number; target: number; count: number }[];
  frequency_triples?: {
    first: number;
    middle: number;
    last: number;
    count: number;
  }[];
  output_bundles?: { activity: number; bundles: number[][] }[];
  input_bundles?: { activity: number; bundles: number[][] }[];
}

export interface VariantConformance {
  activities: number[];
  count: number;
  fitness: number;
}

export interface Conformance {
  /** Token-replay fitness in [0, 1]: does the model reproduce the log? */
  fitness: number;
  /** Escaping-edges precision in [0, 1]: how much extra behavior the model allows. */
  precision: number;
  /** Token-replay generalization in [0, 1]: how evenly the log exercises the model. Absent in older exports. */
  generalization?: number;
  /** Inverse arc-degree simplicity in (0, 1]: how lean the net itself is. Absent in older exports. */
  simplicity?: number;
  fitting_traces: number;
  trace_count: number;
  produced: number;
  consumed: number;
  missing: number;
  remaining: number;
  /** Variants worst-fitting first. */
  variants: VariantConformance[];
}

/** One move of an optimal alignment between a trace and the model. */
export type AlignmentMoveType = "sync" | "log" | "model";

/** Petri-net alignment search variants exposed by the compatible API. */
export type AlignmentVariant =
  | "state_equation_a_star"
  | "dijkstra_no_heuristics"
  | "dijkstra_less_memory"
  | "discounted_a_star"
  | "generator_dijkstra_no_heuristics"
  | "generator_dijkstra_less_memory";

export type AlignmentCostProfile = "legacy" | "standard";

/** Optional controls for one deferred alignment request. */
export interface AlignmentOptions {
  variant?: AlignmentVariant;
  max_states?: number;
  max_align_time?: number;
  max_align_time_trace?: number;
  max_alignments?: number;
  exponent?: number;
  synchronous_dijkstra?: boolean;
  cost_profile?: AlignmentCostProfile;
  trace_cost?: number;
  model_cost?: number;
  silent_model_cost?: number;
  sync_cost?: number;
}

export const DEFAULT_ALIGNMENT_OPTIONS: Required<Pick<AlignmentOptions, "variant" | "cost_profile">> = {
  variant: "state_equation_a_star",
  cost_profile: "legacy",
};

export interface AlignmentMove {
  type: AlignmentMoveType;
  /** Activity id of the move; null for a silent model (tau) transition. */
  activity: number | null;
  /** Explicit marker for a silent model move; absent on older exports. */
  silent?: boolean;
}

export interface VariantAlignment {
  activities: number[];
  count: number;
  /** Optimal alignment, in execution order. */
  moves: AlignmentMove[];
  /** Total cost: one per log-only or visible model-only move. */
  cost: number;
  /** 1 - cost / worst-case cost, in [0, 1]. */
  fitness: number;
  /** Equal-optimal alternatives returned by generator variants, when present. */
  alternatives?: AlignmentMove[][];
  /** Compatible spelling accepted for older/newer server exports. */
  optimal_alignments?: AlignmentMove[][];
  /** Search work for this trace variant, absent in older exports. */
  diagnostics?: AlignmentDiagnostics;
}

export interface AlignmentDiagnostics {
  visited_states?: number;
  queued_states?: number;
  traversed_arcs?: number;
  /** Number of marking-equation LP solves performed (0 when no LP was used). */
  lp_solved?: number;
  closed_set_length?: number;
  num_visited_markings?: number;
}

export interface Alignments {
  /**
   * False when the model could not be aligned; the other fields are then
   * meaningless and variants empty. `exhausted` tells the two causes apart.
   */
  computed: boolean;
  /** When !computed: true = search bound hit (reachable but too large), false = unsound. */
  exhausted: boolean;
  /** Cost-based fitness in [0, 1]: how cheaply the log aligns to the model. */
  fitness: number;
  /**
   * Alignment-based escaping-edges precision in [0, 1]: how much extra
   * behavior the model allows at the states the optimal alignments visit.
   * Absent in exports from older minera versions.
   */
  precision?: number;
  fitting_traces: number;
  trace_count: number;
  /** Summed optimal alignment cost over all traces. */
  total_cost: number;
  /** Variants worst-fitting first. */
  variants: VariantAlignment[];
  /** Search variant selected by the caller (absent in older exports). */
  variant?: AlignmentVariant;
  /** Canonical server spelling, accepted alongside `variant`. */
  alignment_variant?: AlignmentVariant;
  diagnostics?: AlignmentDiagnostics;
  /**
   * Per-activity move tallies across the log, most deviating first. Absent
   * in exports from older minera versions.
   */
  activity_deviations?: ActivityDeviation[];
}

/** One directly-follows succession (b follows a); indexes into activities. */
export interface FootprintRelation {
  source: number;
  target: number;
}

/** Footprint (directly-follows relation) comparison of the log against a model. */
export interface Footprints {
  /** Share of the log's successions the model also allows, in [0, 1]. */
  fitness: number;
  /** Share of the model's successions the log also shows, in [0, 1]. */
  precision: number;
  log_relations: number;
  model_relations: number;
  shared_relations: number;
  /** True when model exploration hit its bound and the relation is partial. */
  exhausted: boolean;
  /** Successions in the log the model forbids (fitness gaps). */
  log_only: FootprintRelation[];
  /** Successions the model allows the log never shows (precision gaps). */
  model_only: FootprintRelation[];
}

/** Classic directly-follows-graph alignment request controls. */
export type DfgAlignmentVariant = "CLASSIC";
export type DfgAlignmentProjection = "none" | "log" | "metrics";
export type DfgCostMap = Record<string, number>;
export interface DfgAlignmentOptions {
  variant?: DfgAlignmentVariant;
  max_states?: number;
  max_time_ms?: number;
  sync_cost?: DfgCostMap;
  model_move_cost?: DfgCostMap;
  log_move_cost?: DfgCostMap;
  internal_log_move_cost?: DfgCostMap;
  projection?: DfgAlignmentProjection;
}
export interface DfgAlignmentMove { log: string; model: string; }
export interface DfgAlignmentTrace {
  cost: number;
  internal_cost: number;
  fitness: number;
  best_worst_cost: number;
  visited_states: number;
  closed: number;
  closed_states: number;
  moves: DfgAlignmentMove[];
}
export interface DfgProjectedEdge { source: string; target: string; sync: number; model_move: number; }
export interface DfgProjectedActivity { activity: string; sync: number; model_move: number; log_move: number; }
export interface DfgAlignmentProjectionResult {
  edges: DfgProjectedEdge[];
  activities: DfgProjectedActivity[];
}
export interface DfgAlignmentResult {
  family: "DFG";
  variant: "CLASSIC";
  unique_variants: number;
  alignments: DfgAlignmentTrace[];
  projected_dfg?: DfgAlignmentProjectionResult;
  projected_log?: unknown;
}

export type DfgPetriConversionVariant =
  | "ACTIVITY_DEFINES_PLACE"
  | "INVISIBLES_NO_DUPLICATES";
export interface DfgPetriConversionOptions {
  variant?: DfgPetriConversionVariant;
  artificial_start_activity?: string;
  artificial_end_activity?: string;
}
export interface DfgPetriConversionTransition {
  activity: number | null;
  inputs: number[];
  outputs: number[];
}
export interface DfgPetriConversionResult {
  family: "DFG_CONVERSION";
  variant: DfgPetriConversionVariant;
  source: number;
  sink: number;
  activities: string[];
  place_count: number;
  transitions: DfgPetriConversionTransition[];
}

export type EditDistanceVariant = "EDIT_DISTANCE";
export interface EditDistanceOptions {
  variant?: EditDistanceVariant;
  anti?: boolean;
  deviation_cost?: number;
  projection?: "none" | "log";
}
export interface EditDistanceMove {
  type: "sync" | "log" | "model";
  log: string;
  model: string;
}
export interface EditDistanceTrace {
  source_variant: string[];
  target_variant: string[];
  target_index: number;
  cost: number;
  fitness: number;
  best_worst_cost: number;
  bwc: number;
  denominator: number;
  moves: EditDistanceMove[];
}
export interface EditDistanceResult {
  family: "EDIT_DISTANCE";
  variant: "EDIT_DISTANCE";
  anti: boolean;
  source_count: number;
  alignments: EditDistanceTrace[];
  projected_log?: unknown;
}

/** Process-tree alignment search variants exposed by the deferred API. */
export type ProcessTreeAlignmentVariant =
  | "SEARCH_GRAPH_PT"
  | "DYNAMIC_PROGRAMMING"
  | "MILP"
  | "APPROXIMATED_ORIGINAL";

export interface ProcessTreeAlignmentOptions {
  variant?: ProcessTreeAlignmentVariant;
  max_states?: number;
  max_time_ms?: number;
  max_total_time_ms?: number;
  use_global_cache?: boolean;
  batch_size?: number;
  clear_cache?: boolean;
  max_trace_length?: number;
  max_process_tree_height?: number;
  max_trace_time_ms?: number;
  max_per_trace_time_ms?: number;
}

export interface ProcessTreeAlignmentMove {
  type: "sync" | "log" | "model";
  log: string;
  model: string;
  activity: string;
}

export interface ProcessTreeAlignmentTrace {
  trace: string[];
  cost: number;
  fitness: number;
  completed: boolean;
  timed_out: boolean;
  moves: ProcessTreeAlignmentMove[];
}

export interface ProcessTreeAlignmentDiagnostics {
  visited_states: number;
  queued_states: number;
  traversed_arcs: number;
  lp_solved: number;
  traces_aligned: number;
  message: string;
}

export interface ProcessTreeAlignmentResult {
  family: "PROCESS_TREE";
  variant: ProcessTreeAlignmentVariant;
  computed: boolean;
  completed: boolean;
  timed_out: boolean;
  exhausted: boolean;
  total_cost: number;
  fitness: number;
  diagnostics: ProcessTreeAlignmentDiagnostics;
  traces: ProcessTreeAlignmentTrace[];
}

/** Decomposed Petri-net alignment (recomposition variant). */
export type DecomposedAlignmentVariant = "recompos_maximal";
export interface DecomposedAlignmentOptions {
  variant?: DecomposedAlignmentVariant;
  threshold_border_agreement?: number;
  max_time_trace?: number;
  max_time_total?: number;
  max_states?: number;
  max_total_states?: number;
  use_cache?: boolean;
  clear_cache?: boolean;
  log_move_cost?: number;
  model_move_cost?: number;
  tau_model_cost?: number;
  sync_cost?: number;
  initial_marking?: number[];
  final_marking?: number[];
  activity_log_costs?: Record<string, number>;
  activity_model_costs?: Record<string, number>;
  activity_sync_costs?: Record<string, number>;
}
export interface DecomposedAlignmentMove {
  log?: string | null;
  model?: string | null;
  transition?: number;
}
export interface DecomposedAlignmentTrace {
  activities: string[];
  count: number;
  complete: boolean;
  timed_out: boolean;
  exhausted: boolean;
  cost: number;
  fitness: number;
  best_worst_cost: number;
  /** Native serializer calls this field `moves`; `alignment` is retained for older exports. */
  moves?: DecomposedAlignmentMove[];
  alignment?: DecomposedAlignmentMove[];
}
export interface DecomposedAlignmentDiagnostics {
  decompositions: number;
  components: number;
  merges: number;
  cache_hits: number;
  cache_misses: number;
  border_disagreements: number;
  agreement_iterations: number;
  visited_states: number;
  queued_states: number;
  traversed_arcs: number;
  whole_net_fallback: boolean;
  note: string;
}
export interface DecomposedAlignmentResult {
  algorithm: "decomposed_alignment";
  variant: DecomposedAlignmentVariant;
  complete: boolean;
  timed_out: boolean;
  exhausted: boolean;
  fitness: number;
  best_worst_cost: number;
  trace_count: number;
  total_cost: number;
  diagnostics: DecomposedAlignmentDiagnostics;
  traces: DecomposedAlignmentTrace[];
}

export type PetriAdvancedAlignmentVariant = "version_discounted_a_star";
export interface PetriAntiAlignmentOptions {
  variant?: PetriAdvancedAlignmentVariant;
  exponent?: number;
  epsilon?: number;
  marking_limit?: number;
  max_states?: number;
  initial_marking?: number[];
  final_marking?: number[];
}
export interface PetriMultiAlignmentOptions {
  variant?: PetriAdvancedAlignmentVariant;
  exponent?: number;
  marking_limit?: number;
  max_states?: number;
  initial_marking?: number[];
  final_marking?: number[];
}
export interface PetriAdvancedAlignmentDiagnostics {
  visited_states: number;
  queued_states: number;
  traversed_arcs: number;
  exhausted: boolean;
}
export interface PetriAntiAlignmentResult {
  algorithm: "petri_anti_alignment";
  variant: PetriAdvancedAlignmentVariant;
  activities: string[];
  transitions: number[];
  cost: number;
  objective: number;
  precision: number;
  diagnostics: PetriAdvancedAlignmentDiagnostics;
}
export interface PetriMultiAlignmentResult {
  algorithm: "petri_multi_alignment";
  variant: PetriAdvancedAlignmentVariant;
  activities: string[];
  transitions: number[];
  cost: number;
  objective: number;
  max_distance_to_log: number;
  diagnostics: PetriAdvancedAlignmentDiagnostics;
}

export type ProcessTreeReductionVariant = "tree_tr_based";
export interface ProcessTreeReductionOptions { variant?: ProcessTreeReductionVariant; activity_key?: string; }
export interface ProcessTreeReductionResult {
  algorithm: "process_tree_reduction";
  variant: ProcessTreeReductionVariant;
  trace_index: number;
  process_tree: ProcessTreeNode;
}

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null;
const finite = (value: unknown): value is number => typeof value === "number" && Number.isFinite(value);
const isPetriUsageRecord = (value: unknown): value is PetriUsageRecord => {
  if (!isRecord(value)) return false;
  const rec = value as Record<string, unknown>;
  return Number.isInteger(rec.id) && (rec.id as number) >= 0 &&
    Number.isInteger(rec.log1_occurrences) && (rec.log1_occurrences as number) >= 0 &&
    Number.isInteger(rec.log2_occurrences) && (rec.log2_occurrences as number) >= 0 &&
    Number.isInteger(rec.total_occurrences) && (rec.total_occurrences as number) >= 0 &&
    finite(rec.direction);
};
export function isPetriUsageComparison(value: unknown): value is PetriUsageComparison {
  if (!isRecord(value) || value.family !== "PETRI_USAGE_COMPARISON" ||
      !Array.isArray(value.places) || !Array.isArray(value.transitions) || !Array.isArray(value.arcs)) return false;
  return value.places.every(isPetriUsageRecord) && value.transitions.every(isPetriUsageRecord) &&
    value.arcs.every((arc) => {
      if (!isPetriUsageRecord(arc) || !isRecord(arc)) return false;
      const rec = arc as Record<string, unknown>;
      return Number.isInteger(rec.transition) && (rec.transition as number) >= 0 &&
        Number.isInteger(rec.place) && (rec.place as number) >= 0 &&
        typeof rec.input === "boolean";
    });
}
export function isDfgAlignmentResult(value: unknown): value is DfgAlignmentResult {
  if (!isRecord(value) || value.family !== "DFG" || value.variant !== "CLASSIC" ||
      !Number.isInteger(value.unique_variants) || !Array.isArray(value.alignments)) return false;
  return value.alignments.every((a) => isRecord(a) && finite(a.cost) && finite(a.internal_cost) &&
    finite(a.fitness) && Array.isArray(a.moves) && a.moves.every((m) => isRecord(m) && typeof m.log === "string" && typeof m.model === "string"));
}
const nonNegativeIntArray = (value: unknown): value is number[] =>
  Array.isArray(value) && value.every((item) => Number.isInteger(item) && (item as number) >= 0);
export function isDfgPetriConversionResult(
  value: unknown,
): value is DfgPetriConversionResult {
  if (!isRecord(value) || value.family !== "DFG_CONVERSION" ||
      !["ACTIVITY_DEFINES_PLACE", "INVISIBLES_NO_DUPLICATES"].includes(value.variant as string) ||
      !Number.isInteger(value.source) || (value.source as number) < 0 ||
      !Number.isInteger(value.sink) || (value.sink as number) < 0 ||
      !Number.isInteger(value.place_count) || (value.place_count as number) <= 0 ||
      !Array.isArray(value.activities) || !Array.isArray(value.transitions)) return false;
  const activities = value.activities as unknown[];
  const placeCount = value.place_count as number;
  if ((value.source as number) >= placeCount ||
      (value.sink as number) >= placeCount ||
      !activities.every((activity) => typeof activity === "string")) return false;
  return value.transitions.every((transition) => {
    if (!isRecord(transition) ||
        !(transition.activity === null ||
          (Number.isInteger(transition.activity) && (transition.activity as number) >= 0 &&
           (transition.activity as number) < activities.length)) ||
        !nonNegativeIntArray(transition.inputs) || !nonNegativeIntArray(transition.outputs)) return false;
    return transition.inputs.every((place) => place < placeCount) &&
      transition.outputs.every((place) => place < placeCount);
  });
}
export function isEditDistanceResult(value: unknown): value is EditDistanceResult {
  if (!isRecord(value) || value.family !== "EDIT_DISTANCE" || value.variant !== "EDIT_DISTANCE" ||
      typeof value.anti !== "boolean" || !Number.isInteger(value.source_count) || !Array.isArray(value.alignments)) return false;
  return value.alignments.every((a) => isRecord(a) && Array.isArray(a.source_variant) &&
    Array.isArray(a.target_variant) && finite(a.cost) && finite(a.fitness) && Array.isArray(a.moves) &&
    a.moves.every((m) => isRecord(m) && (m.type === "sync" || m.type === "log" || m.type === "model") && typeof m.log === "string" && typeof m.model === "string"));
}

const metric = (value: unknown): value is number | null => value === null || finite(value);
const nullableString = (value: unknown): value is string | null => value === null || typeof value === "string";
const optionalRecord = (value: unknown): boolean => value === undefined || isRecord(value);
const evaluationNonNegativeInt = (value: unknown): value is number => Number.isInteger(value) && (value as number) >= 0;

function isReplayFitnessEvaluation(value: unknown): value is ReplayFitnessEvaluation {
  return isRecord(value) && ["token_based", "alignment_based"].includes(value.variant as string) &&
    typeof value.computed === "boolean" && typeof value.exhausted === "boolean" && nullableString(value.error) &&
    metric(value.fitness) && metric(value.log_fitness) && metric(value.average_trace_fitness) &&
    metric(value.percentage_of_fitting_traces) && evaluationNonNegativeInt(value.fitting_traces) &&
    evaluationNonNegativeInt(value.trace_count) && metric(value.total_cost) && optionalRecord(value.token_replay) &&
    optionalRecord(value.alignments);
}

function isPrecisionEvaluation(value: unknown): value is PrecisionEvaluation {
  return isRecord(value) && ["etconformance_token", "align_etconformance", "automaton_after_align"].includes(value.variant as string) &&
    typeof value.computed === "boolean" && typeof value.exhausted === "boolean" && nullableString(value.error) &&
    metric(value.precision) && optionalRecord(value.token_replay) && optionalRecord(value.alignments);
}

function isGeneralizationEvaluation(value: unknown): value is GeneralizationEvaluation {
  return isRecord(value) && value.variant === "generalization_token" && typeof value.computed === "boolean" &&
    nullableString(value.error) && metric(value.score) && evaluationNonNegativeInt(value.executed_transitions) &&
    evaluationNonNegativeInt(value.transition_count) && Array.isArray(value.transition_frequencies) &&
    value.transition_frequencies.every(evaluationNonNegativeInt);
}

function isSimplicityEvaluation(value: unknown): value is SimplicityEvaluation {
  return isRecord(value) && ["arc_degree", "extended_cardoso", "extended_cyclomatic"].includes(value.variant as string) &&
    typeof value.computed === "boolean" && typeof value.exhausted === "boolean" && nullableString(value.error) &&
    metric(value.simplicity) && metric(value.complexity) && metric(value.value) && evaluationNonNegativeInt(value.vertices) &&
    evaluationNonNegativeInt(value.edges) && evaluationNonNegativeInt(value.strongly_connected_components);
}

/** Validates a standalone response from the model-quality endpoint. */
export function isModelEvaluation(value: unknown): value is ModelEvaluation {
  if (!isRecord(value) || typeof value.miner !== "string") return false;
  const present = [value.replay_fitness, value.precision, value.generalization, value.simplicity]
    .filter((item) => item !== undefined).length;
  if (present !== 1) return false;
  return (value.replay_fitness === undefined || isReplayFitnessEvaluation(value.replay_fitness)) &&
    (value.precision === undefined || isPrecisionEvaluation(value.precision)) &&
    (value.generalization === undefined || isGeneralizationEvaluation(value.generalization)) &&
    (value.simplicity === undefined || isSimplicityEvaluation(value.simplicity));
}

/** Validates the stochastic-language distance endpoint response. */
export function isEmdResult(value: unknown): value is EmdResult {
  return isRecord(value) && metric(value.distance) && typeof value.computed === "boolean" &&
    typeof value.normalized === "boolean" && nullableString(value.error);
}

export function isProcessTreeAlignmentResult(value: unknown): value is ProcessTreeAlignmentResult {
  if (!isRecord(value) || value.family !== "PROCESS_TREE" ||
      !["SEARCH_GRAPH_PT", "DYNAMIC_PROGRAMMING", "MILP", "APPROXIMATED_ORIGINAL"].includes(value.variant as string) ||
      typeof value.computed !== "boolean" || typeof value.completed !== "boolean" ||
      typeof value.timed_out !== "boolean" || typeof value.exhausted !== "boolean" ||
      !finite(value.total_cost) || !finite(value.fitness) || !isRecord(value.diagnostics) ||
      !Array.isArray(value.traces)) return false;
  const d = value.diagnostics;
  if (!["visited_states", "queued_states", "traversed_arcs", "lp_solved", "traces_aligned"].every((key) => Number.isInteger(d[key]) && (d[key] as number) >= 0) || typeof d.message !== "string") return false;
  return value.traces.every((trace) => isRecord(trace) && Array.isArray(trace.trace) &&
    trace.trace.every((activity) => typeof activity === "string") && finite(trace.cost) &&
    finite(trace.fitness) && typeof trace.completed === "boolean" && typeof trace.timed_out === "boolean" && Array.isArray(trace.moves) &&
    trace.moves.every((move) => isRecord(move) &&
      (move.type === "sync" || move.type === "log" || move.type === "model") &&
      typeof move.log === "string" && typeof move.model === "string" && typeof move.activity === "string"));
}

const nonNegativeInt = (value: unknown): value is number => Number.isInteger(value) && (value as number) >= 0;
const validDecomposedMove = (value: unknown): value is DecomposedAlignmentMove =>
  isRecord(value) && (value.log === undefined || value.log === null || typeof value.log === "string") &&
  (value.model === undefined || value.model === null || typeof value.model === "string") &&
  (value.transition === undefined || (Number.isInteger(value.transition) && (value.transition as number) >= -1));
const validAdvancedDiagnostics = (value: unknown): value is PetriAdvancedAlignmentDiagnostics =>
  isRecord(value) && nonNegativeInt(value.visited_states) && nonNegativeInt(value.queued_states) &&
  nonNegativeInt(value.traversed_arcs) && typeof value.exhausted === "boolean";

export function isDecomposedAlignmentResult(value: unknown): value is DecomposedAlignmentResult {
  if (!isRecord(value) || value.algorithm !== "decomposed_alignment" || value.variant !== "recompos_maximal" ||
      typeof value.complete !== "boolean" || typeof value.timed_out !== "boolean" || typeof value.exhausted !== "boolean" ||
      !finite(value.fitness) || !finite(value.best_worst_cost) || !nonNegativeInt(value.trace_count) || !finite(value.total_cost) ||
      !isRecord(value.diagnostics) || !Array.isArray(value.traces)) return false;
  const d = value.diagnostics;
  if (!["decompositions", "components", "merges", "cache_hits", "cache_misses", "border_disagreements", "agreement_iterations", "visited_states", "queued_states", "traversed_arcs"]
    .every((key) => nonNegativeInt(d[key])) || typeof d.whole_net_fallback !== "boolean" || typeof d.note !== "string") return false;
  return value.traces.every((trace) => isRecord(trace) && Array.isArray(trace.activities) && trace.activities.every((a) => typeof a === "string") &&
    nonNegativeInt(trace.count) && typeof trace.complete === "boolean" && typeof trace.timed_out === "boolean" && typeof trace.exhausted === "boolean" &&
    finite(trace.cost) && finite(trace.fitness) && finite(trace.best_worst_cost) &&
    ((Array.isArray(trace.moves) && trace.moves.every(validDecomposedMove)) || (Array.isArray(trace.alignment) && trace.alignment.every(validDecomposedMove))));
}

function isPetriAdvancedResult(value: unknown, algorithm: string): value is PetriAntiAlignmentResult | PetriMultiAlignmentResult {
  if (!isRecord(value) || value.algorithm !== algorithm || value.variant !== "version_discounted_a_star" ||
      !Array.isArray(value.activities) || !value.activities.every((a) => typeof a === "string") || !Array.isArray(value.transitions) ||
      !value.transitions.every((x) => Number.isInteger(x) && (x as number) >= -1) || !finite(value.cost) || !finite(value.objective) || !validAdvancedDiagnostics(value.diagnostics)) return false;
  if (algorithm === "petri_anti_alignment" && !finite(value.precision)) return false;
  if (algorithm === "petri_multi_alignment" && !finite(value.max_distance_to_log)) return false;
  return true;
}
export function isPetriAntiAlignmentResult(value: unknown): value is PetriAntiAlignmentResult { return isPetriAdvancedResult(value, "petri_anti_alignment"); }
export function isPetriMultiAlignmentResult(value: unknown): value is PetriMultiAlignmentResult { return isPetriAdvancedResult(value, "petri_multi_alignment"); }

function isProcessTreeNode(value: unknown): value is ProcessTreeNode {
  if (!isRecord(value) || !["sequence", "exclusive", "parallel", "loop", "activity", "tau"].includes(value.operator as string)) return false;
  if (value.operator === "activity" && !nonNegativeInt(value.activity)) return false;
  if (value.activity !== undefined && !nonNegativeInt(value.activity)) return false;
  if (value.children !== undefined && (!Array.isArray(value.children) || !value.children.every(isProcessTreeNode))) return false;
  return value.operator === "activity" || value.operator === "tau" ? value.children === undefined || value.children.length === 0 : Boolean(value.children && value.children.length > 0);
}
export function isProcessTreeReductionResult(value: unknown): value is ProcessTreeReductionResult {
  return isRecord(value) && value.algorithm === "process_tree_reduction" && value.variant === "tree_tr_based" &&
    nonNegativeInt(value.trace_index) && isProcessTreeNode(value.process_tree);
}

export const FOOTPRINTS_DISCOVERY_VARIANTS = [
  "entire_event_log", "trace_by_trace", "dfg", "petri_reach_graph",
  "process_tree", "powl",
] as const;
export type FootprintsDiscoveryVariant = (typeof FOOTPRINTS_DISCOVERY_VARIANTS)[number];
export const FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS: FootprintsDiscoveryVariant[] = [
  "entire_event_log", "dfg", "process_tree", "powl",
];
export type FootprintsModelVariant = "process_tree" | "powl" | "inductive" | "alpha" | "heuristic" | "dfg";
export const FOOTPRINTS_CONFORMANCE_VARIANTS = [
  "log_model", "log_extensive", "trace_extensive",
] as const;
export type FootprintsConformanceVariant = (typeof FOOTPRINTS_CONFORMANCE_VARIANTS)[number];

/** An aggregate footprint record; optional keys are variant-dependent. */
export interface AggregateFootprint {
  sequence: FootprintRelation[];
  parallel: FootprintRelation[];
  dfg?: EdgeStat[];
  start_activities?: number[];
  end_activities?: number[];
  activities?: number[];
  skippable?: boolean;
  activities_always_happening?: number[];
  min_trace_length?: number;
  max_trace_length_without_loops?: number;
  trace?: number[];
}
export interface FootprintsLogModelConformance {
  strict: boolean;
  violations: FootprintRelation[];
  is_fit: boolean;
}
export interface FootprintsExtensiveConformance {
  footprints: FootprintRelation[];
  start_activities: number[];
  end_activities: number[];
  activities_always_happening: number[];
  min_length_fit: boolean;
  is_footprints_fit: boolean;
  is_fit: boolean;
}
export interface FootprintsTraceExtensiveConformance extends FootprintsExtensiveConformance {
  trace_index: number;
  case_id: string;
}
export interface FootprintsTraceDiscovery {
  trace_index: number;
  case_id: string;
  footprint: AggregateFootprint;
}
export interface AggregateFootprintsDiscovery {
  entire_event_log?: AggregateFootprint;
  trace_by_trace?: FootprintsTraceDiscovery[];
  dfg?: AggregateFootprint;
  petri_reach_graph?: AggregateFootprint;
  process_tree?: AggregateFootprint;
  powl?: AggregateFootprint;
}
export interface AggregateFootprintsConformance {
  model: FootprintsModelVariant;
  log_model?: FootprintsLogModelConformance;
  log_extensive?: FootprintsExtensiveConformance;
  trace_extensive?: FootprintsTraceExtensiveConformance[];
}
export interface AggregateFootprintsMetrics {
  precision?: number;
  log_fitness?: number;
  perc_fit_traces?: number;
}
export interface AggregateFootprints {
  discovery?: AggregateFootprintsDiscovery;
  conformance?: AggregateFootprintsConformance;
  metrics?: AggregateFootprintsMetrics;
}

export type DfgStatisticalVariant =
  | "native"
  | "frequency"
  | "frequency_greedy"
  | "frequency_triples"
  | "performance"
  | "performance_greedy"
  | "case_attributes"
  | "clean"
  | "clean_polars"
  | "clean_time";
export type DfgCausalVariant = "alpha" | "heuristic";

export interface DfgPerformanceSummary {
  count: number;
  mean: number;
  median: number;
  min: number;
  max: number;
  stdev: number;
  sum: number;
}
export interface DfgPerformanceEdge {
  source: number;
  target: number;
  summary: DfgPerformanceSummary;
  raw_values: number[];
  value: number | number[] | DfgPerformanceSummary;
}
export interface DfgAttributeValueCount {
  value: string;
  count: number;
}
export interface DfgNamedAttributeCounts {
  attribute: string;
  values: DfgAttributeValueCount[];
}
export interface DfgCaseAttributeEdge {
  source: number;
  target: number;
  attributes: DfgNamedAttributeCounts[];
}
export interface DfgCaseAttributeNode {
  activity: number;
  attributes: DfgNamedAttributeCounts[];
}
export interface DfgBoundaryActivity {
  activity: number;
  count: number;
}
export interface DfgStatisticalAnalysis {
  variant: DfgStatisticalVariant;
  frequency_dfg?: EdgeStat[];
  frequency_triples?: Array<{
    first: number;
    middle: number;
    last: number;
    count: number;
  }>;
  performance_dfg?: DfgPerformanceEdge[];
  case_attributes?: {
    edges: DfgCaseAttributeEdge[];
    nodes: DfgCaseAttributeNode[];
  };
  clean?: {
    dfg: EdgeStat[];
    start_activities: DfgBoundaryActivity[];
    end_activities: DfgBoundaryActivity[];
  };
  clean_time?: Array<{
    activity: number;
    mean_seconds_from_case_start: number;
  }>;
}
export interface DfgCausalAnalysis {
  variant: DfgCausalVariant;
  relations: Array<{ source: number; target: number; value: number }>;
}
export interface DfgVariantsAnalysis {
  statistical: DfgStatisticalAnalysis[];
  causal: DfgCausalAnalysis[];
}

export type CorrelationMinerVariant =
  | "classic"
  | "classic_split"
  | "trace_based";
export interface CorrelationMatrixEntry {
  source: number;
  target: number;
  value: number;
}
export interface CorrelationMinerAnalysis {
  variant: CorrelationMinerVariant;
  frequency_dfg: EdgeStat[];
  performance_dfg: Array<{
    source: number;
    target: number;
    seconds: number;
  }>;
  precede_succeed_probability: CorrelationMatrixEntry[];
  duration_matrix: CorrelationMatrixEntry[];
}
export interface CorrelationVariantsAnalysis {
  variants: CorrelationMinerAnalysis[];
}

/** Conformance results for one discovered model. */
export interface ModelConformance {
  /** Whether the model's final marking is reachable — the bar for alignment. */
  sound: boolean;
  token_replay: Conformance;
  /** Footprint comparison; absent in older exports. */
  footprints?: Footprints;
  /** Present in CLI exports; the server omits it and serves it from /api/align. */
  alignments?: Alignments;
}

/** Per-miner conformance, keyed by discovered or imported model id. */
export type ConformanceModels = Record<string, ModelConformance>;

/** Model-quality evaluator families and their complete native variant set. */
export type EvaluationFamily =
  | "replay_fitness"
  | "precision"
  | "generalization"
  | "simplicity";
export type ReplayFitnessVariant = "auto" | "token_based" | "alignment_based";
export type PrecisionVariant =
  | "auto"
  | "etconformance_token"
  | "align_etconformance"
  | "automaton_after_align";
export type GeneralizationVariant = "generalization_token";
export type SimplicityVariant =
  | "arc_degree"
  | "extended_cardoso"
  | "extended_cyclomatic";

/** Query options accepted by the model-quality endpoint. */
export interface EvaluationOptions {
  evaluation_family?: EvaluationFamily;
  fitness_variant?: ReplayFitnessVariant;
  precision_variant?: PrecisionVariant;
  precision_use_task_ids?: boolean;
  precision_max_states?: number;
  generalization_variant?: GeneralizationVariant;
  simplicity_variant?: SimplicityVariant;
  simplicity_arc_degree_k?: number;
  simplicity_max_states?: number;
  simplicity_initial_marking?: number[];
  simplicity_final_marking?: number[];
}

export type PetriAnalysisFamily =
  | "marking_equation"
  | "extended_marking_equation"
  | "workflow_net"
  | "soundness";

export interface PetriAnalysisOptions {
  family?: PetriAnalysisFamily;
  miner?: string;
  trace_index?: number;
  initial_marking?: number[];
  final_marking?: number[];
  transition_costs?: number[];
  model_costs?: number[];
  sync_costs?: number[];
  log_costs?: number[];
  split_indices?: number[];
  max_k?: number;
  max_states?: number;
  full_bootstrap?: boolean;
  diagnostics?: boolean;
}

export type ProcessTreePlayoutVariant = "BASIC_PLAYOUT" | "EXTENSIVE" | "TOPBOTTOM";
export interface ProcessTreePlayoutOptions {
  variant?: ProcessTreePlayoutVariant;
  traces?: number;
  max_steps?: number;
  seed?: number;
  min_length?: number;
  max_length?: number;
  max_loop_occurrences?: number;
  max_trace_cap?: number;
}

/** Controls weighted random execution of a Petri-net model. */
export interface StochasticPlayoutOptions {
  traces?: number;
  max_steps?: number;
  seed?: number;
  transition_weights?: number[];
  stop_at_final?: boolean;
  start?: number;
  case_interval?: number;
  event_interval?: number;
}

/** Controls bounded virtual-time Petri-net Monte Carlo simulation. */
export interface MonteCarloOptions {
  simulations?: number;
  case_arrival_seconds?: number;
  max_steps?: number;
  seed?: number;
  start?: number;
  default_duration_seconds?: number;
  transition_weights?: number[];
  transition_durations?: Array<{
    distribution: "deterministic" | "exponential" | "constant0" | "immediate" |
      "normal" | "uniform" | "lognormal" | "gamma";
    value_seconds: number;
    secondary_seconds?: number;
    tertiary_seconds?: number;
  }>;
  resources_per_place?: Record<number, number>;
  default_resources_per_place?: number;
  enable_diagnostics?: boolean;
  stop_at_final?: boolean;
}

export interface MonteCarloDiagnostics {
  simulated_cases: number;
  completed_cases: number;
  deadlocked_cases: number;
  step_limited_cases: number;
  case_arrival_seconds: number;
  total_cases_seconds: number;
  median_case_execution_seconds: number;
  case_execution_seconds: number[];
}

export interface MonteCarloResult {
  family: "MONTE_CARLO";
  variant: "PETRI_SEMAPH_FIFO";
  log: { trace_count: number; event_count: number };
  diagnostics: MonteCarloDiagnostics;
}

export function isMonteCarloResult(value: unknown): value is MonteCarloResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<MonteCarloResult>;
  if (result.family !== "MONTE_CARLO" || result.variant !== "PETRI_SEMAPH_FIFO") return false;
  if (!result.log || typeof result.log !== "object" ||
      !Number.isSafeInteger(result.log.trace_count) || result.log.trace_count < 0 ||
      !Number.isSafeInteger(result.log.event_count) || result.log.event_count < 0) return false;
  const d = result.diagnostics;
  if (!d || typeof d !== "object") return false;
  for (const name of ["simulated_cases", "completed_cases", "deadlocked_cases", "step_limited_cases"] as const) {
    if (!Number.isSafeInteger(d[name]) || d[name] < 0) return false;
  }
  if (![d.case_arrival_seconds, d.total_cases_seconds,
        d.median_case_execution_seconds].every(Number.isFinite) ||
      !Array.isArray(d.case_execution_seconds) ||
      d.case_execution_seconds.some((item) => !Number.isFinite(item) || item < 0)) return false;
  return true;
}

export type DfgPlayoutVariant = "classic" | "performance";
export interface DfgPlayoutOptions {
  variant?: DfgPlayoutVariant;
  max_variants?: number;
  min_weighted_probability?: number;
  max_occurrences_per_activity?: number;
  interrupt_when_dfg_complete?: boolean;
  add_trace_if_takes_new_edges?: boolean;
  max_execution_time_ms?: number;
  return_variants?: boolean;
  return_only_if_complete?: boolean;
  min_variant_occurrences?: number;
  traces?: number;
  case_arrival_rate?: number;
  seed?: number;
  max_steps?: number;
  max_events_per_trace?: number;
  start_time?: string;
}

/** Response from the standalone Petri-analysis endpoint. */
export interface PetriAnalysisResult {
  family: PetriAnalysisFamily;
  variant?: string;
  result?: Record<string, unknown>;
  computed?: boolean;
  feasible?: boolean;
  exhausted?: boolean;
  objective?: number;
  lower_bound?: number;
  status?: "sound" | "unsound" | "unknown";
  error?: string;
  [key: string]: unknown;
}

export function isPetriAnalysisResult(value: unknown): value is PetriAnalysisResult {
  if (!value || typeof value !== "object") return false;
  const family = (value as { family?: unknown }).family;
  return family === "marking_equation" || family === "extended_marking_equation" ||
    family === "workflow_net" || family === "soundness";
}

type EvaluationMetric = number | null;
export interface ReplayFitnessEvaluation {
  variant: Exclude<ReplayFitnessVariant, "auto">;
  computed: boolean;
  exhausted: boolean;
  error: string | null;
  fitness: EvaluationMetric;
  log_fitness: EvaluationMetric;
  average_trace_fitness: EvaluationMetric;
  percentage_of_fitting_traces: EvaluationMetric;
  fitting_traces: number;
  trace_count: number;
  total_cost: EvaluationMetric;
  token_replay?: Conformance;
  alignments?: Alignments;
}
export interface PrecisionEvaluation {
  variant: Exclude<PrecisionVariant, "auto">;
  computed: boolean;
  exhausted: boolean;
  error: string | null;
  precision: EvaluationMetric;
  token_replay?: Conformance;
  alignments?: Alignments;
}
export interface GeneralizationEvaluation {
  variant: GeneralizationVariant;
  computed: boolean;
  error: string | null;
  score: EvaluationMetric;
  executed_transitions: number;
  transition_count: number;
  transition_frequencies: number[];
}
export interface SimplicityEvaluation {
  variant: SimplicityVariant;
  computed: boolean;
  exhausted: boolean;
  error: string | null;
  simplicity: EvaluationMetric;
  complexity: EvaluationMetric;
  value: EvaluationMetric;
  vertices: number;
  edges: number;
  strongly_connected_components: number;
}
export interface ModelEvaluation {
  miner: string;
  replay_fitness?: ReplayFitnessEvaluation;
  precision?: PrecisionEvaluation;
  generalization?: GeneralizationEvaluation;
  simplicity?: SimplicityEvaluation;
}

export interface EmdOptions {
  normalize_masses?: boolean;
  strict_equal_mass?: boolean;
}
export interface EmdResult {
  distance: EvaluationMetric;
  computed: boolean;
  normalized: boolean;
  error: string | null;
}

/** Summary statistics over a set of durations, in seconds. */
export interface DurationStats {
  count: number;
  total: number;
  mean: number;
  median: number;
  min: number;
  max: number;
}

/** Waiting-time statistics for one directly-follows edge. */
export interface EdgePerformance {
  source: number;
  target: number;
  stats: DurationStats;
}

/** Service-time statistics for one activity (lifecycle start/complete pairs). */
export interface ActivityPerformance {
  /** Index into MineraExport.activities. */
  activity: number;
  stats: DurationStats;
}

/** The time perspective: per-edge waiting times and case throughput. */
export interface Performance {
  /** Traces with at least two parseable timestamps. */
  timed_traces: number;
  /** First-to-last event duration across timed traces. */
  case_duration: DurationStats;
  /**
   * Gaps between consecutive case start instants — the case arrival rate.
   * Absent in exports from older minera versions.
   */
  case_arrival?: DurationStats;
  /** Edge timings, in the same order as MineraExport.edges. */
  edges: EdgePerformance[];
  /**
   * Activity service times from lifecycle start/complete pairs; sparse (only
   * paired activities appear) and absent in exports from older versions.
   */
  activities?: ActivityPerformance[];
}

/** Server-computed performance spectrum observations. */
export interface PerformanceSpectrumObservation {
  case_id: string;
  activities: string[];
  instants: number[];
  /** Start index of this contiguous sub-pattern in the requested pattern. */
  pattern_offset: number;
}
export interface PerformanceSpectrumExport {
  variant: "connected" | "disconnected";
  activities: string[];
  observations: PerformanceSpectrumObservation[];
}

export type BatchType =
  | "simultaneous"
  | "batching_on_start"
  | "batching_on_end"
  | "sequential"
  | "concurrent";
export interface BatchEvent {
  start: number;
  complete: number;
  case_id: string;
  trace_index: number;
  event_index: number;
}
export interface BatchInstance {
  type: BatchType;
  start: number;
  complete: number;
  events: BatchEvent[];
}
export interface BatchGroup {
  activity: number;
  resource: string;
  batch_count: number;
  event_count: number;
  batches: BatchInstance[];
}
export interface BatchDiscoveryExport {
  groups: BatchGroup[];
}

/** Optional overlap perspective, emitted only when explicitly requested. */
export interface ConcurrentActivityOverlapExport {
  first: string;
  second: string;
  count: number;
}
export interface OverlapExport {
  interval_events: number[];
  cases: number[];
  concurrent_activities: ConcurrentActivityOverlapExport[];
}

export interface ChaoticActivityMetric {
  activity: string;
  frequency: number;
  entropy: number;
  entropy_smooth: number;
  entropy_gain: number;
  chaotic_score: number;
}

export interface ChaoticActivitiesOptions {
  chaotic_activity_key?: string;
  chaotic_alpha?: number;
  chaotic_max_activities?: number;
  chaotic_max_events?: number;
}

export interface ChaoticActivitiesDiagnostics {
  activity_count: number;
  event_count: number;
  output_truncated: boolean;
  message: string;
}

export interface ChaoticActivitiesResult {
  family: "CHAOTIC_ACTIVITIES";
  variant: "NIEK_SIDOROVA";
  alpha: number;
  activities: ChaoticActivityMetric[];
  diagnostics: ChaoticActivitiesDiagnostics;
}

export interface PassedTimeEntry {
  activity: string;
  performance: number;
  frequency: number;
}

export interface PassedTimeOptions {
  passed_time_activity?: string;
  passed_time_variant?: "pre" | "post" | "prepost";
  passed_time_activity_key?: string;
  passed_time_timestamp_key?: string;
  passed_time_start_timestamp_key?: string;
  passed_time_aggregation?: "mean" | "median" | "min" | "max" | "stdev" | "sum";
  passed_time_business_hours?: boolean;
  passed_time_business_hour_slots?: string;
}

export interface PassedTimeResult {
  family: "PASSED_TIME";
  variant: "PRE" | "POST" | "PREPOST";
  activity: string;
  pre: PassedTimeEntry[];
  post: PassedTimeEntry[];
  pre_avg_perf: number;
  post_avg_perf: number;
}

function isPassedTimeEntries(value: unknown): value is PassedTimeEntry[] {
  return Array.isArray(value) && value.every((item) => {
    if (typeof item !== "object" || item === null) return false;
    const entry = item as Partial<PassedTimeEntry>;
    return typeof entry.activity === "string" &&
      typeof entry.performance === "number" && Number.isFinite(entry.performance) &&
      typeof entry.frequency === "number" && Number.isInteger(entry.frequency) &&
      entry.frequency >= 0;
  });
}

export function isPassedTimeResult(value: unknown): value is PassedTimeResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<PassedTimeResult>;
  return data.family === "PASSED_TIME" &&
    (data.variant === "PRE" || data.variant === "POST" || data.variant === "PREPOST") &&
    typeof data.activity === "string" && isPassedTimeEntries(data.pre) &&
    isPassedTimeEntries(data.post) && typeof data.pre_avg_perf === "number" &&
    Number.isFinite(data.pre_avg_perf) && typeof data.post_avg_perf === "number" &&
    Number.isFinite(data.post_avg_perf);
}

export interface ServiceTimeOptions {
  service_time_activity_key?: string;
  service_time_start_timestamp_key?: string;
  service_time_timestamp_key?: string;
  service_time_aggregation?: "mean" | "median" | "min" | "max" | "sum";
  service_time_business_hours?: boolean;
  service_time_business_hour_slots?: string;
}

export interface ServiceTimeResult {
  family: "SERVICE_TIME";
  variant: "CLASSIC";
  aggregation: "MEAN" | "MEDIAN" | "MIN" | "MAX" | "SUM";
  values: Record<string, number>;
}

export function isServiceTimeResult(value: unknown): value is ServiceTimeResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<ServiceTimeResult>;
  if (data.family !== "SERVICE_TIME" || data.variant !== "CLASSIC" ||
      !["MEAN", "MEDIAN", "MIN", "MAX", "SUM"].includes(data.aggregation ?? "") ||
      typeof data.values !== "object" || data.values === null || Array.isArray(data.values))
    return false;
  return Object.values(data.values as Record<string, unknown>)
    .every((entry) => typeof entry === "number" && Number.isFinite(entry));
}

export interface ReworkOptions {
  rework_variant?: "activities" | "cases";
  rework_activity_key?: string;
  rework_case_id_key?: string;
}

export interface ReworkCaseStats {
  number_activities: number;
  rework: number;
}

export interface ReworkResult {
  family: "REWORK";
  variant: "ACTIVITIES" | "CASES";
  activity_cases?: Record<string, number>;
  cases?: Record<string, ReworkCaseStats>;
}

export interface CycleTimeOptions {
  cycle_time_timestamp_key?: string;
  cycle_time_start_timestamp_key?: string;
}

export interface CycleTimeResult {
  family: "CYCLE_TIME";
  variant: "CLASSIC";
  cycle_time: number;
  instances: number;
  timed_events: number;
}

export function isCycleTimeResult(value: unknown): value is CycleTimeResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<CycleTimeResult>;
  return data.family === "CYCLE_TIME" && data.variant === "CLASSIC" &&
    typeof data.cycle_time === "number" && Number.isFinite(data.cycle_time) &&
    data.cycle_time >= 0 && typeof data.instances === "number" &&
    Number.isInteger(data.instances) && data.instances >= 0 &&
    typeof data.timed_events === "number" &&
    Number.isInteger(data.timed_events) && data.timed_events >= 0;
}

export interface CaseArrivalOptions {
  case_arrival_timestamp_key?: string;
  case_arrival_business_hours?: boolean;
  case_arrival_business_hour_slots?: string;
}

export interface CaseArrivalResult {
  family: "CASE_ARRIVAL";
  variant: "CLASSIC";
  arrival_average: number;
  dispersion_average: number;
  timed_starts: number;
  timed_ends: number;
}

export function isCaseArrivalResult(value: unknown): value is CaseArrivalResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<CaseArrivalResult>;
  return data.family === "CASE_ARRIVAL" && data.variant === "CLASSIC" &&
    typeof data.arrival_average === "number" && Number.isFinite(data.arrival_average) &&
    data.arrival_average >= 0 && typeof data.dispersion_average === "number" &&
    Number.isFinite(data.dispersion_average) && data.dispersion_average >= 0 &&
    typeof data.timed_starts === "number" && Number.isInteger(data.timed_starts) &&
    data.timed_starts >= 0 && typeof data.timed_ends === "number" &&
    Number.isInteger(data.timed_ends) && data.timed_ends >= 0;
}

export interface AttributeStatisticsOptions {
  attribute_statistics_key?: string;
  attribute_statistics_scope?: "events" | "traces";
  attribute_statistics_keep_once_per_case?: boolean;
}

export interface AttributeStatisticsResult {
  family: "ATTRIBUTE_STATISTICS";
  scope: "EVENTS" | "TRACES";
  attribute_key: string;
  keep_once_per_case: boolean;
  values: Record<string, number>;
}

export function isAttributeStatisticsResult(
  value: unknown,
): value is AttributeStatisticsResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<AttributeStatisticsResult>;
  if (data.family !== "ATTRIBUTE_STATISTICS" ||
      (data.scope !== "EVENTS" && data.scope !== "TRACES") ||
      typeof data.attribute_key !== "string" ||
      typeof data.keep_once_per_case !== "boolean" ||
      typeof data.values !== "object" || data.values === null ||
      Array.isArray(data.values)) return false;
  return Object.values(data.values as Record<string, unknown>)
    .every((count) => typeof count === "number" && Number.isInteger(count) && count >= 0);
}

export interface BoundaryActivitiesOptions {
  boundary_activities_variant?: "start" | "end" | "both";
  boundary_activities_activity_key?: string;
}

export interface BoundaryActivitiesResult {
  family: "BOUNDARY_ACTIVITIES";
  variant: "START" | "END" | "BOTH";
  starts: Record<string, number>;
  ends: Record<string, number>;
}

export function isBoundaryActivitiesResult(
  value: unknown,
): value is BoundaryActivitiesResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<BoundaryActivitiesResult>;
  if (data.family !== "BOUNDARY_ACTIVITIES" ||
      !["START", "END", "BOTH"].includes(data.variant ?? "") ||
      typeof data.starts !== "object" || data.starts === null || Array.isArray(data.starts) ||
      typeof data.ends !== "object" || data.ends === null || Array.isArray(data.ends))
    return false;
  const valid = (map: Record<string, unknown>) =>
    Object.values(map).every((count) => typeof count === "number" && Number.isInteger(count) && count >= 0);
  return valid(data.starts as Record<string, unknown>) &&
    valid(data.ends as Record<string, unknown>);
}

export interface VariantStatisticsOptions {
  variant_statistics_activity_key?: string;
  variant_statistics_max_variants?: number;
}

export interface VariantStatisticsEntry {
  activities: string[];
  count: number;
  probability: number;
}

export interface VariantStatisticsResult {
  family: "VARIANT_STATISTICS";
  variant: "CLASSIC";
  activity_key: string;
  variants: VariantStatisticsEntry[];
}

export function isVariantStatisticsResult(
  value: unknown,
): value is VariantStatisticsResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<VariantStatisticsResult>;
  if (data.family !== "VARIANT_STATISTICS" || data.variant !== "CLASSIC" ||
      typeof data.activity_key !== "string" || !Array.isArray(data.variants)) return false;
  return data.variants.every((entry) => {
    if (typeof entry !== "object" || entry === null) return false;
    const item = entry as Partial<VariantStatisticsEntry>;
    return Array.isArray(item.activities) && item.activities.every((activity) => typeof activity === "string") &&
      typeof item.count === "number" && Number.isInteger(item.count) && item.count >= 0 &&
      typeof item.probability === "number" && Number.isFinite(item.probability) &&
      item.probability >= 0 && item.probability <= 1;
  });
}

export function isReworkResult(value: unknown): value is ReworkResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<ReworkResult>;
  if (data.family !== "REWORK" ||
      (data.variant !== "ACTIVITIES" && data.variant !== "CASES")) return false;
  if (data.variant === "ACTIVITIES") {
    if (typeof data.activity_cases !== "object" || data.activity_cases === null ||
        Array.isArray(data.activity_cases)) return false;
    return Object.values(data.activity_cases as Record<string, unknown>)
      .every((count) => typeof count === "number" && Number.isInteger(count) && count >= 0);
  }
  if (typeof data.cases !== "object" || data.cases === null || Array.isArray(data.cases))
    return false;
  return Object.values(data.cases as Record<string, unknown>).every((entry) => {
    if (typeof entry !== "object" || entry === null) return false;
    const stats = entry as Partial<ReworkCaseStats>;
    return typeof stats.number_activities === "number" &&
      Number.isInteger(stats.number_activities) && stats.number_activities >= 0 &&
      typeof stats.rework === "number" && Number.isInteger(stats.rework) &&
      stats.rework >= 0 && stats.rework <= stats.number_activities;
  });
}

export function isChaoticActivitiesResult(
  value: unknown,
): value is ChaoticActivitiesResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<ChaoticActivitiesResult>;
  if (data.family !== "CHAOTIC_ACTIVITIES" ||
      data.variant !== "NIEK_SIDOROVA" ||
      typeof data.alpha !== "number" || !Number.isFinite(data.alpha) ||
      data.alpha < 0 || !Array.isArray(data.activities)) return false;
  if (!data.activities.every((row) => {
    if (typeof row !== "object" || row === null) return false;
    const metric = row as Partial<ChaoticActivityMetric>;
    return typeof metric.activity === "string" &&
      typeof metric.frequency === "number" && Number.isInteger(metric.frequency) &&
      metric.frequency >= 0 &&
      typeof metric.entropy === "number" && Number.isFinite(metric.entropy) &&
      typeof metric.entropy_smooth === "number" && Number.isFinite(metric.entropy_smooth) &&
      typeof metric.entropy_gain === "number" && Number.isFinite(metric.entropy_gain) &&
      typeof metric.chaotic_score === "number" && Number.isFinite(metric.chaotic_score);
  })) return false;
  const diagnostics = data.diagnostics;
  return typeof diagnostics === "object" && diagnostics !== null &&
    Number.isInteger(diagnostics.activity_count) && diagnostics.activity_count >= 0 &&
    Number.isInteger(diagnostics.event_count) && diagnostics.event_count >= 0 &&
    typeof diagnostics.output_truncated === "boolean" &&
    typeof diagnostics.message === "string";
}

export interface ResourceStat {
  name: string;
  event_count: number;
}

/** A handover of work from one resource to another (indexes into resources). */
export interface ResourceHandover {
  source: number;
  target: number;
  count: number;
}

/** The organizational perspective: who works, and how work is handed over. */
export interface ResourceNetwork {
  total_handovers: number;
  resources: ResourceStat[];
  handovers: ResourceHandover[];
}

export interface TraceEvent {
  activity: number;
  timestamp: string;
  /** Performing resource; present only when the log carries one. */
  resource?: string;
  /** Lifecycle transition ("start", "complete", …); present when recorded. */
  lifecycle?: string;
  /** Generic event attributes (extra CSV columns, non-standard XES keys). */
  attributes?: Record<string, string>;
}

/** One observed value of a generic event attribute. */
export interface AttributeValueStat {
  value: string;
  count: number;
}

/** Per-key summary of a generic event attribute across the log. */
export interface EventAttributeStat {
  key: string;
  /** Events carrying the key. */
  events: number;
  /** Total distinct values; `values` lists only the most frequent ones. */
  distinct_values: number;
  values: AttributeValueStat[];
}

/** Per-key summary of a case attribute across the log. */
export interface CaseAttributeStat {
  key: string;
  /** Cases carrying the key. */
  cases: number;
  /** Total distinct values; `values` lists only the most frequent ones. */
  distinct_values: number;
  values: AttributeValueStat[];
}

export interface Trace {
  case_id: string;
  events: TraceEvent[];
  /** Case attributes (XES trace attributes, `case:` CSV columns). */
  attributes?: Record<string, string>;
}

/**
 * Per-activity tally of alignment moves across the whole log: how often the
 * activity aligned synchronously, was inserted (log move), or skipped (model
 * move), weighted by trace counts. Most deviating first.
 */
export interface ActivityDeviation {
  /** Index into MineraExport.activities. */
  activity: number;
  synchronous: number;
  log_moves: number;
  model_moves: number;
}

/**
 * A node of a mined decision tree. A split node carries `key` plus either
 * `value` (equality) or `threshold` (numeric ≤) and both subtrees; a leaf
 * carries `branch`, the predicted branch index. Both carry the per-branch
 * observation counts the node saw.
 */
export interface DecisionTreeNode {
  counts: number[];
  branch?: number;
  key?: string;
  value?: string;
  threshold?: number;
  then?: DecisionTreeNode;
  else?: DecisionTreeNode;
}

/** One alternative of a decision point, with its observed frequency. */
export interface DecisionBranch {
  label: string;
  count: number;
}

/** One place of the net where replay must choose among several transitions. */
export interface DecisionPoint {
  place: number;
  observations: number;
  /** Share of observations the mined tree predicts correctly. */
  accuracy: number;
  /** Baseline: always guessing the most frequent branch. */
  majority_share: number;
  branches: DecisionBranch[];
  tree: DecisionTreeNode;
}

/** Decision mining of one model: which attributes steer each choice. */
export interface Decisions {
  /** Miner id the decisions belong to; absent in on-demand server responses
   * from older versions. */
  miner?: string;
  /** Traces that replayed cleanly and contributed observations. */
  observed_traces: number;
  trace_count: number;
  decision_points: DecisionPoint[];
}

export interface MineraExport {
  dataset: string;
  generator?: string;
  trace_count: number;
  event_count: number;
  /** Raw event log; absent in exports from older minera_export versions. */
  traces?: Trace[];
  activities: ActivityStat[];
  edges: EdgeStat[];
  variants: VariantStat[];
  /** Discovered process tree; absent in exports from older minera versions. */
  process_tree?: ProcessTreeNode;
  /**
   * Conformance of the log against each discovered model, keyed by miner id;
   * every entry carries a token-replay and an alignment result. Absent in
   * exports from older minera versions.
   */
  conformance?: ConformanceModels;
  /** Optional model-quality evaluations embedded by a CLI export. */
  evaluation?: Record<string, Omit<ModelEvaluation, "miner">>;
  /** Optional stochastic-language distance embedded by a CLI export. */
  earth_movers_distance?: EmdResult;
  /** Alpha Miner Petri net; absent in exports from older minera versions. */
  petri_net?: PetriNetModel;
  /**
   * Replay decoration of `petri_net` (firing counts, place waiting times);
   * absent in exports from older minera versions.
   */
  petri_net_decoration?: PetriDecoration;
  /**
   * The Inductive and Heuristic models as workflow nets, each with its own
   * replay decoration — so any miner's model can be drawn (and annotated) as
   * a Petri net. Absent in exports from older minera versions.
   */
  inductive_petri_net?: PetriNetModel;
  inductive_petri_net_decoration?: PetriDecoration;
  heuristic_petri_net?: PetriNetModel;
  heuristic_petri_net_decoration?: PetriDecoration;
  /** POWL discovery model and its converted workflow net, when available. */
  powl?: PowlNode;
  powl_petri_net?: PetriNetModel;
  powl_petri_net_decoration?: PetriDecoration;
  /** Classic log-skeleton relations and allowed activity frequencies. */
  log_skeleton?: LogSkeletonModel;
  /** Trace-order log-skeleton conformance results. */
  log_skeleton_conformance?: LogSkeletonConformanceTrace[];
  /** Classic declarative Declare model and trace diagnostics. */
  declare?: DeclareModel;
  declare_conformance?: DeclareConformanceTrace[];
  /** Aggregate footprints discovery and relational conformance. */
  footprints?: AggregateFootprints;
  /** Opt-in statistical DFG and causal-discovery variants. */
  dfg_variants?: DfgVariantsAnalysis;
  /** Opt-in classic/split/trace-based correlation-miner variants. */
  correlation_mining?: CorrelationVariantsAnalysis;
  /** Heuristic Miner dependency graph; absent in exports from older versions. */
  heuristics_net?: HeuristicsNet;
  /** Time perspective (edge waiting times, case throughput); absent in older exports. */
  performance?: Performance;
  /** Optional performance spectrum analysis. */
  performance_spectrum?: PerformanceSpectrumExport;
  /** Optional batch discovery analysis. */
  batch_discovery?: BatchDiscoveryExport;
  /** Optional interval/case/concurrent overlap statistics. */
  overlap?: OverlapExport;
  /** Organizational perspective (handover-of-work network); absent in older exports. */
  resources?: ResourceNetwork;
  /**
   * Eventually-follows graph: for each activity pair (a, b), how often a is
   * followed by b — directly or transitively — later in the same case. Same
   * shape as `edges`. Absent in exports from older minera versions.
   */
  eventually_follows?: EdgeStat[];
  /** Temporal-profile timing statistics; absent in exports from older versions. */
  temporal_profile?: TemporalProfileEntry[];
  /** Minimum self-distance and witness activities; absent in older exports. */
  minimum_self_distances?: MinimumSelfDistanceEntry[];
  /** View-based transition system; absent in older exports. */
  transition_system?: TransitionSystemExport;
  /**
   * Generic event attributes by key, with each key's most frequent values.
   * Absent when the log has none and in exports from older minera versions.
   */
  event_attributes?: EventAttributeStat[];
  /**
   * Case attributes by key, with each key's most frequent values. Absent when
   * the log has none and in exports from older minera versions.
   */
  case_attributes?: CaseAttributeStat[];
  /**
   * Decision mining of one model (its id in `decisions.miner`), embedded by
   * the CLI's --decisions flag; the server serves it on demand instead.
   */
  decisions?: Decisions;
}

export function isMineraExport(value: unknown): value is MineraExport {
  if (typeof value !== "object" || value === null) return false;
  const data = value as MineraExport;
  const overlap = data.overlap;
  return Array.isArray(data.activities) &&
    Array.isArray(data.edges) &&
    Array.isArray(data.variants) &&
    Number.isFinite(data.trace_count) &&
    (data.powl === undefined || isPowlNode(data.powl, data.activities.length)) &&
    (overlap === undefined || (
      typeof overlap === "object" && overlap !== null &&
      Array.isArray(overlap.interval_events) &&
      overlap.interval_events.every((n) => Number.isInteger(n) && n >= 0) &&
      Array.isArray(overlap.cases) &&
      overlap.cases.every((n) => Number.isInteger(n) && n >= 0) &&
      Array.isArray(overlap.concurrent_activities) &&
      overlap.concurrent_activities.every((row) => {
        return typeof row === "object" && row !== null &&
          typeof row.first === "string" && typeof row.second === "string" &&
          Number.isInteger(row.count) && row.count >= 0;
      })));
}

/** Virtual node ids for the trace entry and exit points. */
export const START = -1;
export const END = -2;

export const numberFormat = new Intl.NumberFormat("en-US");

/**
 * Formats a duration in seconds as a compact human label, picking the largest
 * fitting unit: "12s", "3.4m", "2.1h", "5.0d". Non-positive or non-finite
 * inputs render as an em dash so untimed edges read as "no data".
 */
export function formatDuration(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds <= 0) return "—";
  if (seconds < 60) return `${Math.round(seconds)}s`;
  if (seconds < 3600) return `${(seconds / 60).toFixed(1)}m`;
  if (seconds < 86400) return `${(seconds / 3600).toFixed(1)}h`;
  return `${(seconds / 86400).toFixed(1)}d`;
}
