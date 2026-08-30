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
  variant?: "bose";
  activity_attribute?: string;
  timestamp_attribute?: string;
  case_id_attribute?: string;
  activities: string[];
  feature_vectors: number[][];
  sublogs: ConceptDriftSublogSummary[];
  returned_sublogs: ConceptDriftSublogSummary[];
  cumulative_sublogs: ConceptDriftSublogSummary[];
  sublog_start_timestamps: number[];
  change_points: ConceptDriftChangePoint[];
  change_timestamps: number[];
}

export function isConceptDriftResult(value: unknown): value is ConceptDriftResult {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const candidate = value as Partial<ConceptDriftResult>;
  if (candidate.family !== "CONCEPT_DRIFT" ||
      (candidate.variant !== undefined && candidate.variant !== "bose") ||
      !Array.isArray(candidate.activities) ||
      !candidate.activities.every((x) => typeof x === "string") ||
      !Array.isArray(candidate.feature_vectors) ||
      !candidate.feature_vectors.every((row) => Array.isArray(row) &&
        row.every((x) => typeof x === "number" && Number.isFinite(x))) ||
      !Array.isArray(candidate.sublogs) ||
      !candidate.sublogs.every((row) => !!row && typeof row === "object" &&
        Number.isInteger(row.trace_count) && (row.trace_count as number) >= 0 &&
        Number.isInteger(row.event_count) && (row.event_count as number) >= 0) ||
      !Array.isArray(candidate.returned_sublogs) ||
      !candidate.returned_sublogs.every((row) => !!row && typeof row === "object" &&
        Number.isInteger(row.trace_count) && (row.trace_count as number) >= 0 &&
        Number.isInteger(row.event_count) && (row.event_count as number) >= 0) ||
      !Array.isArray(candidate.cumulative_sublogs) ||
      !candidate.cumulative_sublogs.every((row) => !!row && typeof row === "object" &&
        Number.isInteger(row.trace_count) && (row.trace_count as number) >= 0 &&
        Number.isInteger(row.event_count) && (row.event_count as number) >= 0) ||
      !Array.isArray(candidate.sublog_start_timestamps) ||
      !candidate.sublog_start_timestamps.every((x) => typeof x === "number" && Number.isFinite(x)) ||
      !Array.isArray(candidate.change_points) ||
      !candidate.change_points.every((point) => !!point && typeof point === "object" &&
        Number.isInteger(point.sublog_index) && (point.sublog_index as number) >= 0 &&
        typeof point.p_value === "number" && Number.isFinite(point.p_value)) ||
      !Array.isArray(candidate.change_timestamps) ||
      !candidate.change_timestamps.every((x) => typeof x === "number" && Number.isFinite(x))) {
    return false;
  }
  return [candidate.activity_attribute, candidate.timestamp_attribute,
    candidate.case_id_attribute].every((x) => x === undefined || typeof x === "string");
}

export interface ILPMinerTransition {
  // Visible transitions use the numeric activity-table index emitted by the
  // native Petri-net serializer; null denotes a silent transition.
  activity: number | null;
  inputs: number[];
  outputs: number[];
  reset_inputs?: number[];
  inhibitor_inputs?: number[];
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
  artificial_start_activity?: string;
  artificial_end_activity?: string;
  alpha?: number;
  causal_relation?: string;
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
    if (!t || typeof t !== "object" ||
        !(t.activity === null || (Number.isInteger(t.activity) &&
          (t.activity as number) >= 0 &&
          (t.activity as number) < activities.length)) ||
        !isFiniteIntegerArray(t.inputs) || !isFiniteIntegerArray(t.outputs)) return false;
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
  reset_inputs?: number[];
  inhibitor_inputs?: number[];
}
export interface GeneticMinerNet {
  place_count: number;
  source: number;
  sink: number;
  activities: string[];
  transitions: GeneticMinerTransition[];
}
export interface GeneticMinerMarkings { initial: number[]; final: number[]; }
export interface GeneticCausalMatrix {
  activities: string[];
  input_map: number[][][];
  output_map: number[][][];
}
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
  matrix: GeneticCausalMatrix;
  net: GeneticMinerNet;
  markings: GeneticMinerMarkings;
  diagnostics: GeneticMinerDiagnostics;
}
export interface GeneticMinerOptions {
  activity_key?: string;
  timestamp_key?: string;
  case_id_key?: string;
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
export interface GeneticMatrixConversionResult {
  family: "GENETIC_MATRIX_CONVERSION";
  variant: "TO_PETRI_NET";
  matrix: GeneticCausalMatrix;
  net: GeneticMinerNet;
  markings: GeneticMinerMarkings;
}
function isNonnegativeFiniteNumber(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value) && value >= 0;
}
export function isGeneticCausalMatrix(value: unknown): value is GeneticCausalMatrix {
  if (!value || typeof value !== "object") return false;
  const matrix = value as Partial<GeneticCausalMatrix>;
  if (!Array.isArray(matrix.activities) ||
      !matrix.activities.every((activity) => typeof activity === "string") ||
      !Array.isArray(matrix.input_map) || !Array.isArray(matrix.output_map) ||
      matrix.input_map.length !== matrix.activities.length ||
      matrix.output_map.length !== matrix.activities.length) return false;
  const validMap = (map: number[][][]) => map.every((groups) =>
    Array.isArray(groups) && groups.every((group) =>
      Array.isArray(group) && group.length > 0 && group.every((member) =>
        Number.isInteger(member) && member >= 0 &&
        member < (matrix.activities as string[]).length)));
  return validMap(matrix.input_map) && validMap(matrix.output_map);
}
function isGeneticNetAndMarkings(net: unknown, markings: unknown): boolean {
  if (!net || typeof net !== "object" || !markings || typeof markings !== "object")
    return false;
  const n = net as Partial<GeneticMinerNet>;
  const placeCount = n.place_count;
  if (!Number.isInteger(placeCount) || (placeCount as number) <= 0 ||
      !Number.isInteger(n.source) || !Number.isInteger(n.sink) ||
      (n.source as number) < 0 || (n.sink as number) < 0 ||
      (n.source as number) >= (placeCount as number) ||
      (n.sink as number) >= (placeCount as number) ||
      !Array.isArray(n.activities) ||
      !n.activities.every((activity) => typeof activity === "string") ||
      !Array.isArray(n.transitions)) return false;
  const activities = n.activities as string[];
  if (!n.transitions.every((transition) => {
    if (!transition || typeof transition !== "object" ||
        !(transition.activity === null ||
          (Number.isInteger(transition.activity) &&
           (transition.activity as number) >= 0 &&
           (transition.activity as number) < activities.length)) ||
        !isFiniteIntegerArray(transition.inputs) ||
        !isFiniteIntegerArray(transition.outputs)) return false;
    return transition.inputs.every((place) => place < (placeCount as number)) &&
      transition.outputs.every((place) => place < (placeCount as number));
  })) return false;
  const m = markings as Partial<GeneticMinerMarkings>;
  return isFiniteIntegerArray(m.initial) && isFiniteIntegerArray(m.final) &&
    m.initial.length === placeCount && m.final.length === placeCount;
}
export function isGeneticMinerResult(value: unknown): value is GeneticMinerResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<GeneticMinerResult>;
  if (c.family !== "GENETIC" || !isGeneticCausalMatrix(c.matrix) ||
      !isGeneticNetAndMarkings(c.net, c.markings) ||
      !c.diagnostics || typeof c.diagnostics !== "object") return false;
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

export function isGeneticMatrixConversionResult(
  value: unknown,
): value is GeneticMatrixConversionResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<GeneticMatrixConversionResult>;
  return result.family === "GENETIC_MATRIX_CONVERSION" &&
    result.variant === "TO_PETRI_NET" &&
    isGeneticCausalMatrix(result.matrix) &&
    isGeneticNetAndMarkings(result.net, result.markings) &&
    JSON.stringify(result.matrix.activities) ===
      JSON.stringify((result.net as GeneticMinerNet).activities);
}

export type OrganizationalSnaVariant =
  | "handover" | "working_together" | "subcontracting" | "jointactivities"
  | "handover_pandas" | "working_together_pandas"
  | "subcontracting_pandas" | "jointactivities_pandas";
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
  sna_variant?: OrganizationalSnaVariant; sna_activity_attribute?: string; sna_resource_attribute?: string;
  sna_case_id_attribute?: string;
  sna_beta?: number; sna_n?: number;
  sna_metric_normalization?: boolean; sna_max_resources?: number; sna_max_edges?: number;
}

export interface NetworkAnalysisEdge { source: string; target: string; activity: string; count: number; performance: number[]; }
export interface NetworkAnalysisDiagnostics { computed: boolean; output_truncated: boolean; trace_count: number; event_count: number; link_count: number; row_count: number; performance_value_count: number; message: string; }
export type NetworkAnalysisVariant = "dataframe";
export interface NetworkAnalysisResult { family: "NETWORK_ANALYSIS"; variant: NetworkAnalysisVariant; edges: NetworkAnalysisEdge[]; diagnostics: NetworkAnalysisDiagnostics; }
export interface NetworkAnalysisOptions {
  network_analysis_variant?: NetworkAnalysisVariant;
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
  if (v.family !== "NETWORK_ANALYSIS" || v.variant !== "dataframe" || !Array.isArray(v.edges) || !v.diagnostics) return false;
  if (!v.edges.every((e) => !!e && typeof e.source === "string" && typeof e.target === "string" && typeof e.activity === "string" && Number.isInteger(e.count) && e.count >= 0 && Array.isArray(e.performance) && e.performance.every((x) => typeof x === "number" && Number.isFinite(x)))) return false;
  const d = v.diagnostics as Partial<NetworkAnalysisDiagnostics>;
  return typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" && ["trace_count", "event_count", "link_count", "row_count", "performance_value_count"].every((k) => { const n = d[k as keyof NetworkAnalysisDiagnostics]; return typeof n === "number" && Number.isInteger(n) && n >= 0; }) && typeof d.message === "string";
}
export function isOrganizationalSnaResult(value: unknown): value is OrganizationalSnaResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<OrganizationalSnaResult>;
  if (c.family !== "ORGANIZATIONAL_SNA" || typeof c.directed !== "boolean" ||
      !(["handover", "working_together", "subcontracting", "jointactivities",
         "handover_pandas", "working_together_pandas", "subcontracting_pandas",
         "jointactivities_pandas"] as unknown[]).includes(c.variant) ||
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
export type RoleDiscoveryVariant = "log" | "pandas";
export interface RoleDiscoveryResult { family: "ROLE_DISCOVERY"; variant: RoleDiscoveryVariant; roles: DiscoveredRole[]; diagnostics: RoleDiscoveryDiagnostics; }
export interface RoleDiscoveryOptions { roles_variant?: RoleDiscoveryVariant; roles_activity_attribute?: string; roles_resource_attribute?: string; roles_threshold?: number; roles_max_resources?: number; roles_max_activities?: number; }
export function isRoleDiscoveryResult(value: unknown): value is RoleDiscoveryResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<RoleDiscoveryResult>;
  if (c.family !== "ROLE_DISCOVERY" || (c.variant !== "log" && c.variant !== "pandas") || !Array.isArray(c.roles) || !c.diagnostics || typeof c.diagnostics !== "object") return false;
  if (!c.roles.every((r) => !!r && typeof r === "object" && Array.isArray(r.activities) && r.activities.every((a) => typeof a === "string") && Array.isArray(r.resources) && r.resources.every((x) => !!x && typeof x === "object" && typeof x.name === "string" && Number.isInteger(x.count) && (x.count as number) > 0))) return false;
  const d = c.diagnostics as Partial<RoleDiscoveryDiagnostics>;
  return typeof d.computed === "boolean" && typeof d.output_truncated === "boolean" && [d.trace_count, d.event_count, d.activity_count, d.resource_count, d.role_count].every((n) => Number.isInteger(n) && (n as number) >= 0) && typeof d.message === "string";
}
export const isRoleDiscovery = isRoleDiscoveryResult;

export type ResourceProfilesMetric = "distinct_activities" | "activity_frequency" | "activity_completions" | "case_completions" | "fraction_case_completions" | "average_workload" | "multitasking" | "average_duration_activity" | "average_case_duration" | "interaction_two_resources" | "social_position";
export interface ResourceProfilesRow { resource: string; activity: string; resource2: string; value: number; count: number; }
export interface ResourceProfilesResult { family: "RESOURCE_PROFILES"; metric: ResourceProfilesMetric; rows: ResourceProfilesRow[]; diagnostics: { computed: boolean; output_truncated: boolean; trace_count: number; event_count: number; resource_count: number; message: string }; }
export interface ResourceProfilesOptions { resource_profiles_metric?: ResourceProfilesMetric; resource_profiles_activity_attribute?: string; resource_profiles_resource_attribute?: string; resource_profiles_timestamp_attribute?: string; resource_profiles_start_timestamp_attribute?: string; resource_profiles_case_id_attribute?: string; resource_profiles_t1?: string; resource_profiles_t2?: string; resource_profiles_resource?: string; resource_profiles_activity?: string; resource_profiles_resource2?: string; resource_profiles_max_resources?: number; resource_profiles_max_rows?: number; }

export type KnowledgeInjectionVariant = "description" | "process_mining" | "database";
export interface KnowledgeInjectionOptions {
  knowledge_variant?: KnowledgeInjectionVariant;
  knowledge_description?: string;
  knowledge_query?: string;
  knowledge_database_schema?: string;
  knowledge_max_input_bytes?: number;
  knowledge_max_prompt_bytes?: number;
}
export interface KnowledgeInjectionResult {
  family: "KNOWLEDGE_INJECTION";
  variant: KnowledgeInjectionVariant;
  computed: boolean;
  prompt: string;
  input_bytes: number;
  error: string;
}
export function isKnowledgeInjectionResult(value: unknown): value is KnowledgeInjectionResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<KnowledgeInjectionResult>;
  return result.family === "KNOWLEDGE_INJECTION" &&
    (result.variant === "description" || result.variant === "process_mining" || result.variant === "database") &&
    typeof result.computed === "boolean" && typeof result.prompt === "string" &&
    Number.isSafeInteger(result.input_bytes) && (result.input_bytes as number) >= 0 &&
    typeof result.error === "string";
}
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
  clustering_activity_attribute?: string;
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
export interface ProfileClusteringOptions {
  profile_variant?: "sklearn_profiles";
  profile_activity_attribute?: string;
  profile_cluster_count?: number;
  profile_max_iterations?: number;
  profile_max_traces?: number;
  profile_max_features?: number;
  profile_seed?: number;
  profile_tolerance?: number;
  profile_include_activity?: boolean;
  profile_include_succession?: boolean;
  profile_trace_string_attributes?: string[];
  profile_event_string_attributes?: string[];
  profile_trace_numeric_attributes?: string[];
  profile_event_numeric_attributes?: string[];
  profile_feature_names?: string[];
}
export interface ProfileClusteringResult {
  family: "PROFILE_CLUSTERING";
  variant?: "sklearn_profiles";
  labels: number[];
  clusters: number[][];
  feature_names: string[];
  inertia: number;
  diagnostics: {
    computed: boolean;
    converged: boolean;
    trace_count: number;
    feature_count: number;
    cluster_count: number;
    iterations: number;
    message: string;
  };
}
export function isProfileClusteringResult(value: unknown): value is ProfileClusteringResult {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const candidate = value as Partial<ProfileClusteringResult>;
  const diagnostics = candidate.diagnostics;
  if (candidate.family !== "PROFILE_CLUSTERING" ||
      (candidate.variant !== undefined && candidate.variant !== "sklearn_profiles") ||
      !Array.isArray(candidate.labels) ||
      !candidate.labels.every((label) => Number.isSafeInteger(label) && label >= 0) ||
      !Array.isArray(candidate.clusters) ||
      !candidate.clusters.every((cluster) => Array.isArray(cluster) &&
        cluster.every((index) => Number.isSafeInteger(index) && index >= 0)) ||
      !Array.isArray(candidate.feature_names) ||
      !candidate.feature_names.every((name) => typeof name === "string") ||
      typeof candidate.inertia !== "number" || !Number.isFinite(candidate.inertia) ||
      candidate.inertia < 0 || !diagnostics || typeof diagnostics !== "object") return false;
  const d = diagnostics as Partial<ProfileClusteringResult["diagnostics"]>;
  return typeof d.computed === "boolean" && typeof d.converged === "boolean" &&
    [d.trace_count, d.feature_count, d.cluster_count, d.iterations].every((n) =>
      Number.isSafeInteger(n) && (n as number) >= 0) && typeof d.message === "string";
}
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
  selected_violation_count?: number;
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
      || (d.selected_violation_count !== undefined &&
        (!Number.isInteger(d.selected_violation_count) || (d.selected_violation_count as number) < 0))
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
  case_ids?: string[];
  event_rows?: number[][][];
  trace_rows?: number[][];
  temporal_rows?: LogToFeaturesTemporalRow[];
}
export function isLogToFeaturesResult(value: unknown): value is LogToFeaturesResult {
  if (!value || typeof value !== "object") return false;
  const c = value as Partial<LogToFeaturesResult>;
  if (c.family !== "LOG_TO_FEATURES" || !(["event_based", "trace_based", "temporal"] as const).includes(c.variant as LogToFeaturesVariant)) return false;
  if (!Array.isArray(c.feature_names) || !c.feature_names.every((x) => typeof x === "string")) return false;
  if (c.case_ids !== undefined && (!Array.isArray(c.case_ids) || !c.case_ids.every((x) => typeof x === "string"))) return false;
  const rows = (r: unknown): r is number[] => Array.isArray(r) && r.every((x) => typeof x === "number" && Number.isFinite(x));
  if (c.variant === "event_based") return Array.isArray(c.event_rows) && c.event_rows.every((t) => Array.isArray(t) && t.every(rows));
  if (c.variant === "trace_based") return Array.isArray(c.trace_rows) && c.trace_rows.every(rows);
  return Array.isArray(c.temporal_rows) && c.temporal_rows.every((r) => !!r && typeof r === "object" && typeof (r as LogToFeaturesTemporalRow).timestamp === "number" && Number.isFinite((r as LogToFeaturesTemporalRow).timestamp) && rows((r as LogToFeaturesTemporalRow).values));
}

export type LogConversionVariant =
  | "TO_EVENT_LOG"
  | "TO_EVENT_STREAM"
  | "TO_DATA_FRAME"
  | "TO_NX"
  | "DF_TO_EVENT_LOG_1V"
  | "DF_TO_EVENT_LOG_NV";
export type LogConversionAttributeKind = "string" | "date" | "int" | "float" | "boolean" | "id";
export interface LogConversionAttribute { key: string; value: string; kind: LogConversionAttributeKind; null?: true; }
export interface LogConversionExtension { name: string; prefix: string; uri: string; }
export interface LogConversionClassifier { name: string; keys: string[]; }
export interface LogConversionMetadata {
  extensions: LogConversionExtension[];
  classifiers: LogConversionClassifier[];
  trace_globals: LogConversionAttribute[];
  event_globals: LogConversionAttribute[];
  properties: LogConversionAttribute[];
}
export interface LogConversionEvent {
  case_id: string;
  activity: string;
  timestamp: string;
  resource: string;
  lifecycle: string;
  input_order: number;
  attributes: LogConversionAttribute[];
}
export interface LogConversionTrace { case_id: string; attributes: LogConversionAttribute[]; properties?: LogConversionAttribute[]; events: LogConversionEvent[]; }
export interface LogConversionLog { attributes: LogConversionAttribute[]; metadata?: LogConversionMetadata; traces: LogConversionTrace[]; }
export interface LogConversionStreamRecord { fields: LogConversionAttribute[]; }
export interface LogConversionStream { attributes: LogConversionAttribute[]; metadata?: LogConversionMetadata; events: LogConversionStreamRecord[]; }
export interface LogConversionTableCell { value: string; kind: LogConversionAttributeKind; null?: true; }
export interface LogConversionTable {
  columns: string[];
  rows: (LogConversionTableCell | null)[][];
  attributes?: LogConversionAttribute[];
  metadata?: LogConversionMetadata;
  lazy?: boolean;
}
export type LogConversionGraphNodeType = "CASE" | "EVENT" | "ATTRIBUTE_NODE";
export type LogConversionGraphEdgeType = "BELONGS_TO" | "DF" | "ATTRIBUTE_EDGE";
export interface LogConversionGraphNode {
  id: string;
  type: LogConversionGraphNodeType;
  attributes: LogConversionAttribute[];
}
export interface LogConversionGraphEdge {
  source: number;
  target: number;
  type: LogConversionGraphEdgeType;
  name?: string;
}
export interface LogConversionGraph { nodes: LogConversionGraphNode[]; edges: LogConversionGraphEdge[]; }
export interface LogConversionVariantProjection { activities: string[]; count: number; trace_indexes: number[]; }
interface LogConversionResultBase {
  family: "LOG_CONVERSION";
  variant: LogConversionVariant;
  variants?: LogConversionVariantProjection[];
}
export type LogConversionResult =
  | (LogConversionResultBase & { variant: "TO_EVENT_LOG" | "DF_TO_EVENT_LOG_1V" | "DF_TO_EVENT_LOG_NV"; log: LogConversionLog })
  | (LogConversionResultBase & { variant: "TO_EVENT_STREAM"; stream: LogConversionStream })
  | (LogConversionResultBase & { variant: "TO_DATA_FRAME"; table: LogConversionTable })
  | (LogConversionResultBase & { variant: "TO_NX"; graph: LogConversionGraph });

function isLogConversionAttribute(value: unknown): value is LogConversionAttribute {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<LogConversionAttribute>;
  return typeof candidate.key === "string" && typeof candidate.value === "string"
    && (candidate.null === undefined || candidate.null === true)
    && (["string", "date", "int", "float", "boolean", "id"] as const)
      .includes(candidate.kind as LogConversionAttributeKind);
}
function isLogConversionMetadata(value: unknown): value is LogConversionMetadata {
  if (!value || typeof value !== "object") return false;
  const metadata = value as Partial<LogConversionMetadata>;
  return Array.isArray(metadata.extensions) && metadata.extensions.every((extension) => !!extension
    && typeof extension === "object" && typeof extension.name === "string"
    && typeof extension.prefix === "string" && typeof extension.uri === "string")
    && Array.isArray(metadata.classifiers) && metadata.classifiers.every((classifier) => !!classifier
      && typeof classifier === "object" && typeof classifier.name === "string"
      && Array.isArray(classifier.keys) && classifier.keys.every((key) => typeof key === "string"))
    && Array.isArray(metadata.trace_globals) && metadata.trace_globals.every(isLogConversionAttribute)
    && Array.isArray(metadata.event_globals) && metadata.event_globals.every(isLogConversionAttribute)
    && Array.isArray(metadata.properties) && metadata.properties.every(isLogConversionAttribute);
}
function isLogConversionLog(value: unknown): value is LogConversionLog {
  if (!value || typeof value !== "object") return false;
  const log = value as Partial<LogConversionLog>;
  return Array.isArray(log.attributes) && log.attributes.every(isLogConversionAttribute)
    && (log.metadata === undefined || isLogConversionMetadata(log.metadata))
    && Array.isArray(log.traces) && log.traces.every((trace) => !!trace && typeof trace === "object"
      && typeof trace.case_id === "string"
      && Array.isArray(trace.attributes) && trace.attributes.every(isLogConversionAttribute)
      && (trace.properties === undefined || (Array.isArray(trace.properties) && trace.properties.every(isLogConversionAttribute)))
      && Array.isArray(trace.events) && trace.events.every((event) => !!event && typeof event === "object"
        && typeof event.case_id === "string" && typeof event.activity === "string"
        && typeof event.timestamp === "string" && typeof event.resource === "string"
        && typeof event.lifecycle === "string" && Number.isSafeInteger(event.input_order) && event.input_order >= 0
        && Array.isArray(event.attributes) && event.attributes.every(isLogConversionAttribute)));
}
function isLogConversionVariants(value: unknown): value is LogConversionVariantProjection[] {
  return Array.isArray(value) && value.every((variant) => !!variant && typeof variant === "object"
    && Array.isArray(variant.activities) && variant.activities.every((activity: unknown) => typeof activity === "string")
    && Number.isSafeInteger(variant.count) && variant.count >= 0
    && Array.isArray(variant.trace_indexes)
    && variant.trace_indexes.every((index: unknown) => Number.isSafeInteger(index) && (index as number) >= 0));
}
export function isLogConversionResult(value: unknown): value is LogConversionResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<LogConversionResult> & Record<string, unknown>;
  if (candidate.family !== "LOG_CONVERSION"
      || !(["TO_EVENT_LOG", "TO_EVENT_STREAM", "TO_DATA_FRAME", "TO_NX", "DF_TO_EVENT_LOG_1V", "DF_TO_EVENT_LOG_NV"] as const)
        .includes(candidate.variant as LogConversionVariant)
      || (candidate.variants !== undefined && !isLogConversionVariants(candidate.variants))) return false;
  if (candidate.variant === "TO_EVENT_LOG" || candidate.variant === "DF_TO_EVENT_LOG_1V" || candidate.variant === "DF_TO_EVENT_LOG_NV")
    return isLogConversionLog(candidate.log);
  if (candidate.variant === "TO_EVENT_STREAM") {
    const stream = candidate.stream as Partial<LogConversionStream> | undefined;
    return !!stream && Array.isArray(stream.attributes) && stream.attributes.every(isLogConversionAttribute)
      && (stream.metadata === undefined || isLogConversionMetadata(stream.metadata))
      && Array.isArray(stream.events) && stream.events.every((event) => !!event && typeof event === "object"
        && Array.isArray(event.fields) && event.fields.every(isLogConversionAttribute));
  }
  if (candidate.variant === "TO_DATA_FRAME") {
    const table = candidate.table as Partial<LogConversionTable> | undefined;
    return !!table && Array.isArray(table.columns) && table.columns.every((column) => typeof column === "string")
      && (table.attributes === undefined || (Array.isArray(table.attributes) && table.attributes.every(isLogConversionAttribute)))
      && (table.metadata === undefined || isLogConversionMetadata(table.metadata))
      && (table.lazy === undefined || typeof table.lazy === "boolean")
      && Array.isArray(table.rows) && table.rows.every((row) => Array.isArray(row) && row.length === table.columns!.length
        && row.every((cell) => cell === null || (!!cell && typeof cell === "object"
          && typeof cell.value === "string"
          && (cell.null === undefined || cell.null === true)
          && (["string", "date", "int", "float", "boolean", "id"] as const)
            .includes(cell.kind as LogConversionAttributeKind))));
  }
  const graph = candidate.graph as Partial<LogConversionGraph> | undefined;
  if (!graph || !Array.isArray(graph.nodes) || !graph.nodes.every((node) => !!node && typeof node === "object"
      && typeof node.id === "string" && (["CASE", "EVENT", "ATTRIBUTE_NODE"] as const).includes(node.type as LogConversionGraphNodeType)
      && Array.isArray(node.attributes) && node.attributes.every(isLogConversionAttribute))
      || !Array.isArray(graph.edges)) return false;
  return graph.edges.every((edge) => !!edge && typeof edge === "object"
    && Number.isSafeInteger(edge.source) && edge.source >= 0 && edge.source < graph.nodes!.length
    && Number.isSafeInteger(edge.target) && edge.target >= 0 && edge.target < graph.nodes!.length
    && (["BELONGS_TO", "DF", "ATTRIBUTE_EDGE"] as const).includes(edge.type as LogConversionGraphEdgeType)
    && (edge.name === undefined || typeof edge.name === "string"));
}

export type LogUtilityVariant = "SPLIT_TRAIN_TEST" | "FIXED_PREFIXES" | "ALL_PREFIXES" | "UNTIL_ACTIVITY" | "TO_ACTIVITIES" | "FEATURE_SELECTION";
export interface LogUtilityAttribute { key: string; value: string; kind: string; null?: true; }
export interface LogUtilityEvent {
  case_id: string; activity: string; timestamp: string; resource: string;
  lifecycle: string; input_order: number; attributes: LogUtilityAttribute[];
}
export interface LogUtilityTrace { case_id: string; attributes: LogUtilityAttribute[]; properties?: LogUtilityAttribute[]; events: LogUtilityEvent[]; }
export interface LogUtilityLog { attributes: LogUtilityAttribute[]; metadata?: LogConversionMetadata; traces: LogUtilityTrace[]; }
export interface LogUtilityResult {
  family: "LOG_UTIL";
  variant: LogUtilityVariant;
  training?: LogUtilityLog;
  testing?: LogUtilityLog;
  log?: LogUtilityLog;
  change_indexes?: number[][];
  elapsed_seconds?: number[];
  activities?: string[];
  logs?: LogUtilityLog[];
  event_attributes?: string[];
  case_attributes?: string[];
}
function isLogUtilityAttribute(value: unknown): value is LogUtilityAttribute {
  return !!value && typeof value === "object" && typeof (value as LogUtilityAttribute).key === "string" &&
    typeof (value as LogUtilityAttribute).value === "string" && typeof (value as LogUtilityAttribute).kind === "string" &&
    ((value as LogUtilityAttribute).null === undefined || (value as LogUtilityAttribute).null === true);
}
function isLogUtilityLog(value: unknown): value is LogUtilityLog {
  if (!value || typeof value !== "object") return false;
  const log = value as Partial<LogUtilityLog>;
  if (!Array.isArray(log.attributes) || !log.attributes.every(isLogUtilityAttribute) ||
      (log.metadata !== undefined && !isLogConversionMetadata(log.metadata)) || !Array.isArray(log.traces)) return false;
  return log.traces.every((trace) => !!trace && typeof trace === "object" && typeof trace.case_id === "string" &&
    Array.isArray(trace.attributes) && trace.attributes.every(isLogUtilityAttribute) && Array.isArray(trace.events) &&
    (trace.properties === undefined || (Array.isArray(trace.properties) && trace.properties.every(isLogUtilityAttribute))) &&
    trace.events.every((event) => !!event && typeof event === "object" && typeof event.case_id === "string" &&
      typeof event.activity === "string" && typeof event.timestamp === "string" && typeof event.resource === "string" &&
      typeof event.lifecycle === "string" && Number.isSafeInteger(event.input_order) && event.input_order >= 0 &&
      Array.isArray(event.attributes) && event.attributes.every(isLogUtilityAttribute)));
}
export function isLogUtilityResult(value: unknown): value is LogUtilityResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<LogUtilityResult>;
  if (candidate.family !== "LOG_UTIL" || !["SPLIT_TRAIN_TEST", "FIXED_PREFIXES", "ALL_PREFIXES", "UNTIL_ACTIVITY", "TO_ACTIVITIES", "FEATURE_SELECTION"].includes(candidate.variant as string)) return false;
  if (candidate.variant === "SPLIT_TRAIN_TEST") return isLogUtilityLog(candidate.training) && isLogUtilityLog(candidate.testing);
  if (candidate.variant === "FEATURE_SELECTION") return Array.isArray(candidate.event_attributes) && candidate.event_attributes.every((key) => typeof key === "string") && Array.isArray(candidate.case_attributes) && candidate.case_attributes.every((key) => typeof key === "string");
  if (candidate.variant === "TO_ACTIVITIES") return Array.isArray(candidate.activities) && candidate.activities.every((activity) => typeof activity === "string") && Array.isArray(candidate.logs) && candidate.logs.length === candidate.activities.length && candidate.logs.every(isLogUtilityLog);
  if (!isLogUtilityLog(candidate.log)) return false;
  if (candidate.variant === "ALL_PREFIXES") return Array.isArray(candidate.change_indexes) && candidate.change_indexes.every((row) => Array.isArray(row) && row.every((index) => Number.isSafeInteger(index) && index >= 0));
  if (candidate.variant === "UNTIL_ACTIVITY") return Array.isArray(candidate.elapsed_seconds) && candidate.elapsed_seconds.length === candidate.log.traces.length && candidate.elapsed_seconds.every((value) => typeof value === "number" && Number.isFinite(value) && value >= 0);
  return true;
}

export type EmbeddingVariant = "cases_transformers" | "events_transformers";

export type BrowserHistoryVariant = "chrome_history" | "firefox_history";
export interface BrowserHistoryEvent {
  activity: string;
  timestamp: string;
  attributes: Record<string, string>;
}
export interface BrowserHistoryTrace {
  case_id: string;
  events: BrowserHistoryEvent[];
}
export interface BrowserHistoryResult {
  type: "BROWSER_HISTORY";
  variant: BrowserHistoryVariant;
  trace_count: number;
  event_count: number;
  traces: BrowserHistoryTrace[];
}

export interface WindowsEventRecord {
  activity: string;
  timestamp: string;
  resource: string;
  attributes: Record<string, string>;
}
export interface WindowsEventTrace {
  case_id: string;
  events: WindowsEventRecord[];
}
export interface WindowsEventsResult {
  type: "WINDOWS_EVENTS";
  variant: "windows_events";
  trace_count: number;
  event_count: number;
  traces: WindowsEventTrace[];
}
export interface RepositoryHistoryEvent {
  activity: string;
  timestamp: string;
  resource: string;
}
export interface RepositoryHistoryTrace {
  case_id: string;
  attributes: Record<string, string>;
  events: RepositoryHistoryEvent[];
}
export interface RepositoryHistoryResult {
  type: "REPOSITORY_HISTORY";
  variant: "issues";
  owner: string;
  repository: string;
  trace_count: number;
  event_count: number;
  traces: RepositoryHistoryTrace[];
}
export interface WorkflowHistoryResult {
  type: "WORKFLOW_HISTORY";
  variant: "activity_instances";
  trace_count: number;
  event_count: number;
  traces: Array<{ case_id: string; process_id: string; events: RepositoryHistoryEvent[] }>;
}
export function isWorkflowHistoryResult(value: unknown): value is WorkflowHistoryResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<WorkflowHistoryResult>;
  if (result.type !== "WORKFLOW_HISTORY" || result.variant !== "activity_instances" ||
      !Number.isSafeInteger(result.trace_count) || (result.trace_count as number) < 0 ||
      !Number.isSafeInteger(result.event_count) || (result.event_count as number) < 0 ||
      !Array.isArray(result.traces) || result.traces.length !== result.trace_count) return false;
  let count = 0;
  for (const trace of result.traces) {
    if (!trace || typeof trace.case_id !== "string" || typeof trace.process_id !== "string" ||
        !Array.isArray(trace.events) || !trace.events.every((event) => !!event &&
          typeof event.activity === "string" && typeof event.timestamp === "string" &&
          typeof event.resource === "string")) return false;
    count += trace.events.length;
  }
  return count === result.event_count;
}
export interface EnterpriseHistoryResult {
  type: "ENTERPRISE_HISTORY";
  variant: "order_to_cash" | "accounting";
  trace_count: number;
  event_count: number;
  traces: Array<{ case_id: string; events: Array<RepositoryHistoryEvent & { attributes: Record<string, string> }> }>;
}
export function isEnterpriseHistoryResult(value: unknown): value is EnterpriseHistoryResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<EnterpriseHistoryResult>;
  if (result.type !== "ENTERPRISE_HISTORY" || (result.variant !== "order_to_cash" && result.variant !== "accounting") ||
      !Number.isSafeInteger(result.trace_count) || (result.trace_count as number) < 0 ||
      !Number.isSafeInteger(result.event_count) || (result.event_count as number) < 0 ||
      !Array.isArray(result.traces) || result.traces.length !== result.trace_count) return false;
  let count = 0;
  for (const trace of result.traces) {
    if (!trace || typeof trace.case_id !== "string" || !Array.isArray(trace.events) ||
        !trace.events.every((event) => !!event && typeof event.activity === "string" &&
          typeof event.timestamp === "string" && typeof event.resource === "string" &&
          !!event.attributes && typeof event.attributes === "object" && !Array.isArray(event.attributes) &&
          Object.values(event.attributes).every((attribute) => typeof attribute === "string"))) return false;
    count += trace.events.length;
  }
  return count === result.event_count;
}
export interface CommunicationHistoryResult {
  type: "COMMUNICATION_HISTORY"; variant: "mailbox" | "calendar";
  trace_count: number; event_count: number;
  traces: Array<{ case_id: string; events: Array<RepositoryHistoryEvent & { attributes: Record<string, string> }> }>;
}
export function isCommunicationHistoryResult(value: unknown): value is CommunicationHistoryResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<CommunicationHistoryResult>;
  if (result.type !== "COMMUNICATION_HISTORY" || (result.variant !== "mailbox" && result.variant !== "calendar") || !Number.isSafeInteger(result.trace_count) || (result.trace_count as number) < 0 || !Number.isSafeInteger(result.event_count) || (result.event_count as number) < 0 || !Array.isArray(result.traces) || result.traces.length !== result.trace_count) return false;
  let count = 0;
  for (const trace of result.traces) { if (!trace || typeof trace.case_id !== "string" || !Array.isArray(trace.events) || !trace.events.every((event) => !!event && typeof event.activity === "string" && typeof event.timestamp === "string" && typeof event.resource === "string" && !!event.attributes && typeof event.attributes === "object" && !Array.isArray(event.attributes) && Object.values(event.attributes).every((item) => typeof item === "string"))) return false; count += trace.events.length; }
  return count === result.event_count;
}
export function isRepositoryHistoryResult(value: unknown): value is RepositoryHistoryResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<RepositoryHistoryResult>;
  if (result.type !== "REPOSITORY_HISTORY" || result.variant !== "issues" ||
      typeof result.owner !== "string" || typeof result.repository !== "string" ||
      !Number.isSafeInteger(result.trace_count) || (result.trace_count as number) < 0 ||
      !Number.isSafeInteger(result.event_count) || (result.event_count as number) < 0 ||
      !Array.isArray(result.traces) || result.traces.length !== result.trace_count) return false;
  let count = 0;
  for (const trace of result.traces) {
    if (!trace || typeof trace !== "object" || typeof trace.case_id !== "string" ||
        !trace.attributes || typeof trace.attributes !== "object" || Array.isArray(trace.attributes) ||
        !Object.values(trace.attributes).every((item) => typeof item === "string") ||
        !Array.isArray(trace.events)) return false;
    count += trace.events.length;
    if (!trace.events.every((event) => !!event && typeof event === "object" &&
        typeof event.activity === "string" && typeof event.timestamp === "string" &&
        typeof event.resource === "string")) return false;
  }
  return count === result.event_count;
}
export function isWindowsEventsResult(value: unknown): value is WindowsEventsResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<WindowsEventsResult>;
  if (result.type !== "WINDOWS_EVENTS" || result.variant !== "windows_events" ||
      !Number.isSafeInteger(result.trace_count) || (result.trace_count as number) < 0 ||
      !Number.isSafeInteger(result.event_count) || (result.event_count as number) < 0 ||
      !Array.isArray(result.traces) || result.traces.length !== result.trace_count) return false;
  let events = 0;
  for (const trace of result.traces) {
    if (!trace || typeof trace !== "object" || typeof trace.case_id !== "string" ||
        !Array.isArray(trace.events)) return false;
    events += trace.events.length;
    for (const event of trace.events) {
      if (!event || typeof event !== "object" || typeof event.activity !== "string" ||
          typeof event.timestamp !== "string" || typeof event.resource !== "string" ||
          !event.attributes || typeof event.attributes !== "object" ||
          Array.isArray(event.attributes) ||
          !Object.values(event.attributes).every((item) => typeof item === "string")) return false;
    }
  }
  return events === result.event_count;
}
export function isBrowserHistoryResult(value: unknown): value is BrowserHistoryResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<BrowserHistoryResult>;
  if (result.type !== "BROWSER_HISTORY" ||
      (result.variant !== "chrome_history" && result.variant !== "firefox_history") ||
      !Number.isSafeInteger(result.trace_count) || (result.trace_count as number) < 0 ||
      !Number.isSafeInteger(result.event_count) || (result.event_count as number) < 0 ||
      !Array.isArray(result.traces) || result.traces.length !== result.trace_count) return false;
  let events = 0;
  for (const trace of result.traces) {
    if (!trace || typeof trace !== "object" || typeof trace.case_id !== "string" ||
        !Array.isArray(trace.events)) return false;
    events += trace.events.length;
    for (const event of trace.events) {
      if (!event || typeof event !== "object" || typeof event.activity !== "string" ||
          typeof event.timestamp !== "string" || !event.attributes ||
          typeof event.attributes !== "object" || Array.isArray(event.attributes) ||
          !Object.values(event.attributes).every((item) => typeof item === "string")) return false;
    }
  }
  return events === result.event_count;
}

export type EmbeddingProvider = "hashing" | "transformer";
export interface EmbeddingItem { id: string; embedding: number[]; }
export interface EmbeddingResult {
  family: "TO_EMBEDDINGS";
  variant: EmbeddingVariant;
  /** Echoed by newer backends; absent means the legacy hashing provider. */
  embedding_provider?: EmbeddingProvider;
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
  /** Echoed by newer backends; optional for backwards-compatible payloads. */
  embedding_provider?: EmbeddingProvider;
  /** Echoed by newer backends; optional for backwards-compatible payloads. */
  embedding_model?: string;
  dimensions?: number;
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
    && (c.embedding_provider === undefined || c.embedding_provider === "hashing" || c.embedding_provider === "transformer")
    && typeof c.embedding_model === "string" && typeof c.attribute_key === "string"
    && typeof dimensions === "number" && Number.isInteger(dimensions) && dimensions > 0 && dimensions <= 4096
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
  const dimensions = c.dimensions;
  return c.family === "TO_EMBEDDINGS_SIMILARITY"
    && (c.variant === "cases_transformers" || c.variant === "events_transformers")
    && typeof c.target === "string"
    && (c.embedding_provider === undefined || c.embedding_provider === "hashing" || c.embedding_provider === "transformer")
    && (c.embedding_model === undefined || typeof c.embedding_model === "string")
    && (dimensions === undefined || (typeof dimensions === "number" && Number.isInteger(dimensions) && dimensions > 0 && dimensions <= 4096))
    && typeof topK === "number" && Number.isInteger(topK) && topK >= 0
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
  point_query?: number;
  point_matches?: number[];
  range_query?: [number, number];
  range_matches?: number[];
}
export function isIntervalTreeResult(value: unknown): value is IntervalTreeResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<IntervalTreeResult>;
  if (candidate.family !== "LOG_TO_INTERVAL_TREE" || !Array.isArray(candidate.intervals)
    || !candidate.intervals.every((interval) => !!interval && typeof interval === "object"
      && Number.isFinite((interval as OpenPathInterval).start)
      && Number.isFinite((interval as OpenPathInterval).end)
      && typeof (interval as OpenPathInterval).case_id === "string"
      && typeof (interval as OpenPathInterval).source_activity === "string"
      && typeof (interval as OpenPathInterval).target_activity === "string"
      && Number.isInteger((interval as OpenPathInterval).trace_index)
      && Number.isInteger((interval as OpenPathInterval).source_event_index)
      && Number.isInteger((interval as OpenPathInterval).target_event_index))) return false;
  const validMatches = (matches: unknown): matches is number[] =>
    Array.isArray(matches) && matches.every((item) => Number.isInteger(item) && item >= 0);
  if (candidate.point_query !== undefined) {
    if (!Number.isFinite(candidate.point_query) || !validMatches(candidate.point_matches)) return false;
  } else if (candidate.point_matches !== undefined) return false;
  if (candidate.range_query !== undefined) {
    if (!Array.isArray(candidate.range_query) || candidate.range_query.length !== 2
      || !candidate.range_query.every((item) => typeof item === "number" && Number.isFinite(item))
      || candidate.range_query[0] > candidate.range_query[1]
      || !validMatches(candidate.range_matches)) return false;
  } else if (candidate.range_matches !== undefined) return false;
  return true;
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
  variant: "pandas";
  log: { traces: CaseRelationsTrace[]; attributes?: Record<string, string> };
}
function isStringRecord(value: unknown): value is Record<string, string> {
  return !!value && typeof value === "object" && !Array.isArray(value)
    && Object.values(value).every((item) => typeof item === "string");
}
export function isCaseRelationsResult(value: unknown): value is CaseRelationsResult {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<CaseRelationsResult>;
  if (candidate.family !== "MERGE_CASE_RELATIONS" || candidate.variant !== "pandas" || !candidate.log || typeof candidate.log !== "object" || !Array.isArray(candidate.log.traces)) return false;
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
  index_column?: string;
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
    (result.index_column === undefined || typeof result.index_column === "string") &&
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
  operator: "activity" | "tau" | "sequence" | "exclusive" | "inclusive_or" | "parallel" | "interleaving" | "loop";
  activity?: string;
  children: RandomTreeNode[];
}
export interface RandomTreeResult {
  family: "RANDOM_TREE";
  variant: "BASIC" | "PTANDLOGGENERATOR";
  activities: string[];
  tree?: RandomTreeNode;
  trees?: RandomTreeNode[];
}
function isRandomTreeNode(value: unknown, activities: string[], root = false,
                          basicRoot = false): value is RandomTreeNode {
  if (!value || typeof value !== "object") return false;
  const node = value as Partial<RandomTreeNode>;
  if (!["activity", "tau", "sequence", "exclusive", "inclusive_or", "parallel", "interleaving", "loop"].includes(String(node.operator)) || !Array.isArray(node.children)) return false;
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
  const hasTree = candidate.tree !== undefined;
  const hasTrees = candidate.trees !== undefined;
  if (hasTree === hasTrees) return false;
  if (candidate.activities.length === 0) {
    if (!hasTrees || !Array.isArray(candidate.trees) || candidate.trees.length !== 0)
      return false;
  } else if (candidate.activities.some((activity) => typeof activity !== "string" || activity.length === 0) ||
             new Set(candidate.activities).size !== candidate.activities.length) {
    return false;
  }
  if (hasTree)
    return isRandomTreeNode(candidate.tree, candidate.activities, true,
                            candidate.variant === "BASIC");
  if (!Array.isArray(candidate.trees)) return false;
  return candidate.trees!.every((tree) =>
    isRandomTreeNode(tree, candidate.activities!, true,
                     candidate.variant === "BASIC"));
}

export type OcelDiscoveryVariant = "etot" | "ocdfg" | "ocpn" | "otg" | "saw";
export type OcelConformanceVariant = "etot" | "ocdfg" | "otg";
export type OcelSplitVariant = "CONNECTED_COMPONENTS" | "ANCESTORS_DESCENDANTS";
export interface OcelDiscoveryResult {
  family: "OCEL_DISCOVERY";
  variant: OcelDiscoveryVariant;
  result: Record<string, unknown>;
}
export interface OcpnTokenReplayPlaceDiagnostics {
  m: number;
  r: number;
  c: number;
  p: number;
}
export interface OcpnTokenReplayDiagnostics {
  enabled: boolean;
  trace_count: number;
  fitting_traces: number;
  places: OcpnTokenReplayPlaceDiagnostics[];
  transition_activations: number[];
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
export interface OcelSqliteResult {
  family: "OCEL_SQLITE";
  variant: "IMPORT";
  log: Record<string, unknown>;
}
export type OcelExchangeFormat =
  | "json" | "json_standard" | "xml" | "xml_standard"
  | "csv" | "csv_objects";
export type OcelExchangeImportVariant =
  | "IMPORT_JSON"
  | "IMPORT_JSON_STANDARD"
  | "IMPORT_XML"
  | "IMPORT_XML_STANDARD"
  | "IMPORT_CSV";
export interface OcelExchangeResult {
  family: "OCEL_IO";
  variant: OcelExchangeImportVariant;
  log: Record<string, unknown>;
}
export interface OcelSplitResult {
  family: "OCEL_SPLIT";
  variant: OcelSplitVariant;
  logs: Array<Record<string, unknown>>;
}
export type OcelEnrichmentGraph =
  | "object_interaction_graph"
  | "object_descendants_graph"
  | "object_inheritance_graph"
  | "object_cobirth_graph"
  | "object_codeath_graph";
export interface OcelEnrichmentResult {
  family: "OCEL_ENRICHMENT";
  variant: "CLASSIC";
  graphs: OcelEnrichmentGraph[];
  lifecycle_qualifiers: boolean;
  log: Record<string, unknown>;
}
export type OcelObjectGraphVariant = OcelEnrichmentGraph;
export interface OcelObjectGraphResult {
  family: "OCEL_OBJECT_GRAPH";
  variant: OcelObjectGraphVariant;
  object_ids: string[];
  edges: Array<[string, string]>;
}
export type OcelHeterogeneousGraphNodeType = "EVENT" | "OBJECT" | "CHANGE";
export interface OcelHeterogeneousGraphNode {
  id: string;
  type: OcelHeterogeneousGraphNodeType;
  activity?: string;
  timestamp?: string;
  object_type?: string;
  object_id?: string;
  field?: string;
  value?: unknown;
  attributes?: Record<string, unknown>;
}
export type OcelHeterogeneousGraphEdgeType = "E2O" | "O2O" | "DF" | "CHANGE";
export interface OcelHeterogeneousGraphEdge {
  source: string;
  target: string;
  type: OcelHeterogeneousGraphEdgeType;
  qualifier?: string;
  object?: string;
}
export interface OcelHeterogeneousGraphResult {
  family: "OCEL_GRAPH";
  variant: "HETEROGENEOUS";
  include_df: boolean;
  include_object_changes: boolean;
  nodes: OcelHeterogeneousGraphNode[];
  edges: OcelHeterogeneousGraphEdge[];
}
export type OcelPlayoutVariant = "EXTENSIVE";
export interface OcelPlayoutBinding {
  activity: string;
  objects: Array<{ type: string; id: string }>;
}
export interface OcelPlayoutFiredTransition {
  object_type: string;
  object_id: string;
  transition_index: number;
}
export interface OcelPlayoutTraceEvent {
  activity: string;
  objects: Array<{ type: string; id: string }>;
  transitions: OcelPlayoutFiredTransition[];
}
export interface OcelPetriPlayoutResult {
  family: "OCEL_PLAYOUT";
  variant: OcelPlayoutVariant;
  event_binding?: "independent" | "shared_activity" | "synchronized";
  exists_trace?: boolean;
  sequences?: OcelPlayoutBinding[][];
  traces?: OcelPlayoutTraceEvent[][];
  log: Record<string, unknown>;
}
export interface OcelCausalNetPlayoutResult {
  family: "OC_CAUSAL_NET_PLAYOUT";
  variant: OcelPlayoutVariant;
  event_binding?: "independent" | "shared_activity" | "synchronized";
  exists_trace?: boolean;
  sequences?: OcelPlayoutBinding[][];
  traces?: OcelPlayoutTraceEvent[][];
  log: Record<string, unknown>;
}
export type OcelPlayoutResult = OcelPetriPlayoutResult | OcelCausalNetPlayoutResult;
function isOcelRecord(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}
function isNonNegativeSafeInteger(value: unknown): value is number {
  return Number.isSafeInteger(value) && (value as number) >= 0;
}
export function isOcelDiscoveryResult(value: unknown): value is OcelDiscoveryResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_DISCOVERY") return false;
  if (!["etot", "ocdfg", "ocpn", "otg", "saw"].includes(String(value.variant)) ||
      !isOcelRecord(value.result)) return false;
  if (value.variant !== "ocpn") return true;
  const tbr = value.result.tbr_results;
  const petriNets = value.result.petri_nets;
  if (!isOcelRecord(tbr) || !isOcelRecord(petriNets)) return false;
  return Object.entries(tbr).every(([type, diagnostic]) => {
    if (!type || !isOcelRecord(diagnostic) || diagnostic.enabled !== true)
      return false;
    const traceCount = diagnostic.trace_count;
    const fittingTraces = diagnostic.fitting_traces;
    const places = diagnostic.places;
    const transitionActivations = diagnostic.transition_activations;
    if (typeof traceCount !== "number" || !Number.isSafeInteger(traceCount) ||
        traceCount < 0 || typeof fittingTraces !== "number" ||
        !Number.isSafeInteger(fittingTraces) || fittingTraces < 0 ||
        !Array.isArray(places) || !Array.isArray(transitionActivations))
      return false;
    const net = petriNets[type];
    if (!isOcelRecord(net) || !isNonNegativeSafeInteger(net.place_count) ||
        !Array.isArray(net.transitions) || fittingTraces > traceCount ||
        places.length !== net.place_count ||
        transitionActivations.length !== net.transitions.length)
      return false;
    const placeFields = ["m", "r", "c", "p"];
    const validPlace = (place: unknown) => {
      if (!isOcelRecord(place)) return false;
      return placeFields.every((field) => {
        const count = place[field];
        return isNonNegativeSafeInteger(count);
      });
    };
    return places.every(validPlace) &&
      transitionActivations.every((count) =>
        isNonNegativeSafeInteger(count));
  });
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
export function isOcelSqliteResult(value: unknown): value is OcelSqliteResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_SQLITE" ||
      value.variant !== "IMPORT" || !isOcelRecord(value.log)) return false;
  return isOcelRecord(value.log["ocel:events"]) &&
    isOcelRecord(value.log["ocel:objects"]);
}
export function isOcelExchangeResult(value: unknown): value is OcelExchangeResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_IO" ||
      !["IMPORT_JSON", "IMPORT_JSON_STANDARD", "IMPORT_XML",
        "IMPORT_XML_STANDARD", "IMPORT_CSV"].includes(String(value.variant)) ||
      !isOcelRecord(value.log)) return false;
  return isOcelRecord(value.log["ocel:events"]) &&
    isOcelRecord(value.log["ocel:objects"]);
}
export function isOcelSplitResult(value: unknown): value is OcelSplitResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_SPLIT" ||
      !["CONNECTED_COMPONENTS", "ANCESTORS_DESCENDANTS"].includes(String(value.variant)) ||
      !Array.isArray(value.logs)) return false;
  return value.logs.every((log) => isOcelRecord(log));
}
export function isOcelEnrichmentResult(value: unknown): value is OcelEnrichmentResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_ENRICHMENT" ||
      value.variant !== "CLASSIC" || !Array.isArray(value.graphs) ||
      !value.graphs.every((graph) => [
        "object_interaction_graph", "object_descendants_graph",
        "object_inheritance_graph", "object_cobirth_graph",
        "object_codeath_graph",
      ].includes(String(graph))) || typeof value.lifecycle_qualifiers !== "boolean" ||
      !isOcelRecord(value.log)) return false;
  return isOcelRecord(value.log["ocel:events"]) &&
    isOcelRecord(value.log["ocel:objects"]);
}
export function isOcelObjectGraphResult(value: unknown): value is OcelObjectGraphResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_OBJECT_GRAPH" ||
      ![
        "object_interaction_graph", "object_descendants_graph",
        "object_inheritance_graph", "object_cobirth_graph",
        "object_codeath_graph",
      ].includes(String(value.variant)) || !Array.isArray(value.object_ids) ||
      !value.object_ids.every((id) => typeof id === "string") ||
      !Array.isArray(value.edges)) return false;
  return value.edges.every((edge) => Array.isArray(edge) && edge.length === 2 &&
    edge.every((id) => typeof id === "string"));
}
function isOcelHeterogeneousGraphNode(value: unknown): value is OcelHeterogeneousGraphNode {
  if (!isOcelRecord(value) || typeof value.id !== "string" ||
      !["EVENT", "OBJECT", "CHANGE"].includes(String(value.type))) return false;
  const type = value.type;
  if (type === "EVENT") {
    return typeof value.activity === "string" &&
      typeof value.timestamp === "string" && isOcelRecord(value.attributes);
  }
  if (type === "OBJECT") {
    return typeof value.object_type === "string" &&
      isOcelRecord(value.attributes);
  }
  return typeof value.object_id === "string" &&
    typeof value.object_type === "string" &&
    typeof value.timestamp === "string" &&
    typeof value.field === "string" && "value" in value &&
    (value.value === null || typeof value.value === "string" ||
      typeof value.value === "number" || typeof value.value === "boolean");
}
function isOcelHeterogeneousGraphEdge(value: unknown): value is OcelHeterogeneousGraphEdge {
  if (!isOcelRecord(value) || typeof value.source !== "string" ||
      typeof value.target !== "string" ||
      !["E2O", "O2O", "DF", "CHANGE"].includes(String(value.type))) return false;
  if (value.type === "E2O" || value.type === "O2O")
    return typeof value.qualifier === "string";
  if (value.type === "DF") return typeof value.object === "string";
  return true;
}
export function isOcelHeterogeneousGraphResult(
  value: unknown,
): value is OcelHeterogeneousGraphResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_GRAPH" ||
      value.variant !== "HETEROGENEOUS" || typeof value.include_df !== "boolean" ||
      typeof value.include_object_changes !== "boolean" || !Array.isArray(value.nodes) ||
      !Array.isArray(value.edges)) return false;
  return value.nodes.every(isOcelHeterogeneousGraphNode) &&
    value.edges.every(isOcelHeterogeneousGraphEdge);
}
export function isOcelPlayoutResult(value: unknown): value is OcelPlayoutResult {
  if (!isOcelRecord(value) ||
      (value.family !== "OCEL_PLAYOUT" && value.family !== "OC_CAUSAL_NET_PLAYOUT") ||
      value.variant !== "EXTENSIVE") return false;
  if (value.event_binding !== undefined &&
      value.event_binding !== "independent" &&
      value.event_binding !== "shared_activity" &&
      value.event_binding !== "synchronized") return false;
  if (value.exists_trace !== undefined && typeof value.exists_trace !== "boolean") return false;
  if (value.sequences !== undefined) {
    if (!Array.isArray(value.sequences) || !value.sequences.every((sequence) =>
      Array.isArray(sequence) && sequence.every((binding) =>
        isOcelRecord(binding) && typeof binding.activity === "string" &&
        Array.isArray(binding.objects) && binding.objects.every((object) =>
          isOcelRecord(object) && typeof object.type === "string" &&
        typeof object.id === "string")))) return false;
  }
  if (value.traces !== undefined) {
    if (!Array.isArray(value.traces)) return false;
    for (const trace of value.traces) {
      if (!Array.isArray(trace)) return false;
      for (const event of trace) {
        if (!isOcelRecord(event) || typeof event.activity !== "string" ||
            !Array.isArray(event.objects) || !Array.isArray(event.transitions))
          return false;
        if (!event.objects.every((object) =>
              isOcelRecord(object) && typeof object.type === "string" &&
              typeof object.id === "string")) return false;
        if (!event.transitions.every((transition) =>
              isOcelRecord(transition) &&
              typeof transition.object_type === "string" &&
              typeof transition.object_id === "string" &&
              isNonNegativeSafeInteger(transition.transition_index)))
          return false;
      }
    }
  }
  return isOcelRecord(value.log);
}
export type OcelFeatureVariant = "EVENTS" | "OBJECTS" | "EVENTS_OBJECTS";
export type OcelInteractionGraphSource = "event_cooccurrence" | "event_and_o2o";
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

export interface StreamingDfgEdge {
  source: string;
  target: string;
  count: number;
}
export interface StreamingDfgResult {
  family: "STREAMING_DFG";
  events: number;
  graph: StreamingDfgEdge[];
  activities: Record<string, number>;
  start_activities: Record<string, number>;
  end_activities: Record<string, number>;
  open_cases: Record<string, string>;
}
function isNonNegativeCountMap(value: unknown): value is Record<string, number> {
  return isOcelRecord(value) && Object.values(value).every((entry) =>
    Number.isSafeInteger(entry) && (entry as number) >= 0);
}
export function isStreamingDfgResult(value: unknown): value is StreamingDfgResult {
  if (!isOcelRecord(value) || value.family !== "STREAMING_DFG" ||
      !Number.isSafeInteger(value.events) || (value.events as number) < 0 ||
      !Array.isArray(value.graph) || !isNonNegativeCountMap(value.activities) ||
      !isNonNegativeCountMap(value.start_activities) ||
      !isNonNegativeCountMap(value.end_activities) ||
      !isStringMap(value.open_cases)) return false;
  return value.graph.every((edge) => isOcelRecord(edge) &&
    typeof edge.source === "string" && typeof edge.target === "string" &&
    Number.isSafeInteger(edge.count) && (edge.count as number) > 0);
}

export interface StreamingFootprintsCase {
  case_id: string;
  events: number;
  deviations: number;
  is_fit: boolean;
  first_activity: string;
  last_activity: string;
}
export interface StreamingFootprintsResult {
  family: "STREAMING_FOOTPRINTS";
  total_events_processed: number;
  total_deviations: number;
  cases: StreamingFootprintsCase[];
}
export function isStreamingFootprintsResult(value: unknown): value is StreamingFootprintsResult {
  if (!isOcelRecord(value) || value.family !== "STREAMING_FOOTPRINTS" ||
      !Number.isSafeInteger(value.total_events_processed) ||
      (value.total_events_processed as number) < 0 ||
      !Number.isSafeInteger(value.total_deviations) ||
      (value.total_deviations as number) < 0 || !Array.isArray(value.cases)) return false;
  return value.cases.every((item) => isOcelRecord(item) &&
    typeof item.case_id === "string" && Number.isSafeInteger(item.events) &&
    (item.events as number) >= 0 && Number.isSafeInteger(item.deviations) &&
    (item.deviations as number) >= 0 && typeof item.is_fit === "boolean" &&
    typeof item.first_activity === "string" && typeof item.last_activity === "string");
}

export interface StreamingTemporalDeviation {
  source: string;
  target: string;
  elapsed_seconds: number;
  standard_deviation_distance: number;
}
export interface StreamingTemporalCase {
  events: number;
  deviations: number;
}
export interface StreamingTemporalResult {
  family: "STREAMING_TEMPORAL";
  total_events_processed: number;
  total_deviations: number;
  cases: Record<string, StreamingTemporalCase>;
  deviations: Record<string, StreamingTemporalDeviation[]>;
}
export function isStreamingTemporalResult(value: unknown): value is StreamingTemporalResult {
  if (!isOcelRecord(value) || value.family !== "STREAMING_TEMPORAL" ||
      !Number.isSafeInteger(value.total_events_processed) ||
      (value.total_events_processed as number) < 0 ||
      !Number.isSafeInteger(value.total_deviations) ||
      (value.total_deviations as number) < 0 ||
      !isOcelRecord(value.cases) || !isOcelRecord(value.deviations)) return false;
  const cases = Object.values(value.cases);
  if (!cases.every((item) => isOcelRecord(item) && Number.isSafeInteger(item.events) &&
      (item.events as number) >= 0 && Number.isSafeInteger(item.deviations) &&
      (item.deviations as number) >= 0)) return false;
  return Object.values(value.deviations).every((items) => Array.isArray(items) &&
    items.every((item) => isOcelRecord(item) && typeof item.source === "string" &&
      typeof item.target === "string" && typeof item.elapsed_seconds === "number" &&
      Number.isFinite(item.elapsed_seconds) && typeof item.standard_deviation_distance === "number" &&
      Number.isFinite(item.standard_deviation_distance) &&
      item.elapsed_seconds >= 0 && item.standard_deviation_distance >= 0));
}

export interface StreamingTbrCase {
  events: number;
  missing: number;
  remaining: number;
  is_fit: boolean;
  marking: number[];
}
export interface StreamingTbrResult {
  family: "STREAMING_TBR";
  variant: "FORWARD" | "BACKWARDS";
  total_events_processed: number;
  total_missing: number;
  cases: Record<string, StreamingTbrCase>;
}
export function isStreamingTbrResult(value: unknown): value is StreamingTbrResult {
  if (!isOcelRecord(value) || value.family !== "STREAMING_TBR" ||
      (value.variant !== "FORWARD" && value.variant !== "BACKWARDS") ||
      !Number.isSafeInteger(value.total_events_processed) ||
      (value.total_events_processed as number) < 0 ||
      !Number.isSafeInteger(value.total_missing) ||
      (value.total_missing as number) < 0 || !isOcelRecord(value.cases)) return false;
  return Object.values(value.cases).every((item) => isOcelRecord(item) &&
    Number.isSafeInteger(item.events) && (item.events as number) >= 0 &&
    Number.isSafeInteger(item.missing) && (item.missing as number) >= 0 &&
    Number.isSafeInteger(item.remaining) && (item.remaining as number) >= 0 &&
    typeof item.is_fit === "boolean" && Array.isArray(item.marking) &&
    item.marking.every((token) => Number.isSafeInteger(token) && (token as number) >= 0));
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
  /** Explicit object-centric association aliases emitted by newer exports. */
  total_objects?: Array<[string, string, string]>;
  performance_event_couples?: number[];
  performance_total_objects?: number[];
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
      metric.performance.every((item) => typeof item === "number" && Number.isFinite(item)) &&
      (metric.total_objects === undefined ||
        (Array.isArray(metric.total_objects) &&
          metric.total_objects.every((triple) => Array.isArray(triple) && triple.length === 3 &&
            triple.every((item) => typeof item === "string")))) &&
      (metric.performance_event_couples === undefined ||
        (Array.isArray(metric.performance_event_couples) &&
          metric.performance_event_couples.every((item) => typeof item === "number" && Number.isFinite(item)))) &&
      (metric.performance_total_objects === undefined ||
        (Array.isArray(metric.performance_total_objects) &&
          metric.performance_total_objects.every((item) => typeof item === "number" && Number.isFinite(item))))));
}

export type OcelDescriptionVariant = "VARIANT1" | "VARIANT2";
export interface OcelDescriptionResult {
  family: "OCEL_DESCRIPTION";
  variant: OcelDescriptionVariant;
  includes_timestamps: boolean;
  text?: string;
  events?: string[][];
  objects?: Array<Array<string | number>>;
}
export function isOcelDescriptionResult(value: unknown): value is OcelDescriptionResult {
  if (!isOcelRecord(value) || value.family !== "OCEL_DESCRIPTION" ||
      (value.variant !== "VARIANT1" && value.variant !== "VARIANT2") ||
      typeof value.includes_timestamps !== "boolean") return false;
  if (value.variant === "VARIANT1") return typeof value.text === "string";
  if (!Array.isArray(value.events) || !value.events.every((event) =>
      Array.isArray(event) && event.length >= 1 &&
      event.every((item) => typeof item === "string"))) return false;
  if (!Array.isArray(value.objects)) return false;
  return value.objects.every((object) => {
    if (!Array.isArray(object) || (value.includes_timestamps
        ? object.length !== 5 : object.length !== 2)) return false;
    return typeof object[0] === "string" && typeof object[1] === "string" &&
      (!value.includes_timestamps ||
        (typeof object[2] === "string" && typeof object[3] === "string" &&
         typeof object[4] === "number" && Number.isFinite(object[4])));
  });
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
  | "inclusive_or"
  | "parallel"
  | "interleaving"
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

export interface PetriNetReductionResult {
  family: "PETRI_NET_REDUCTION";
  variant: "complete";
  changed: boolean;
  place_count: number;
  transition_count: number;
  source: number;
  sink: number;
  initial_marking: number[];
  final_marking: number[];
  old_to_new_place: Array<number | null>;
  pnml: string;
}

export function isPetriNetReductionResult(
  value: unknown,
): value is PetriNetReductionResult {
  if (typeof value !== "object" || value === null) return false;
  const result = value as Partial<PetriNetReductionResult>;
  const nonnegativeInteger = (item: unknown): item is number =>
    Number.isSafeInteger(item) && (item as number) >= 0;
  if (result.family !== "PETRI_NET_REDUCTION" ||
      result.variant !== "complete" || typeof result.changed !== "boolean" ||
      !nonnegativeInteger(result.place_count) ||
      !nonnegativeInteger(result.transition_count) ||
      !nonnegativeInteger(result.source) || !nonnegativeInteger(result.sink) ||
      result.place_count === 0 || result.source >= result.place_count ||
      result.sink >= result.place_count ||
      !Array.isArray(result.initial_marking) ||
      !Array.isArray(result.final_marking) ||
      result.initial_marking.length !== result.place_count ||
      result.final_marking.length !== result.place_count ||
      !result.initial_marking.every(nonnegativeInteger) ||
      !result.final_marking.every(nonnegativeInteger) ||
      !Array.isArray(result.old_to_new_place) ||
      !result.old_to_new_place.every((place) =>
        place === null || (nonnegativeInteger(place) && place < result.place_count!)) ||
      typeof result.pnml !== "string" || !result.pnml.includes("<pnml")) return false;
  return true;
}

export type ProcessTreePetriNetVariant = "standard" | "transition_bordered";

export interface ProcessTreePetriNetConversionResult extends PetriNetModel {
  family: "PROCESS_TREE_PETRI_NET";
  variant: ProcessTreePetriNetVariant;
  activities: string[];
}

export function isProcessTreePetriNetConversionResult(
  value: unknown,
): value is ProcessTreePetriNetConversionResult {
  if (typeof value !== "object" || value === null) return false;
  const result = value as Partial<ProcessTreePetriNetConversionResult>;
  if (result.family !== "PROCESS_TREE_PETRI_NET" ||
      (result.variant !== "standard" && result.variant !== "transition_bordered")) {
    return false;
  }
  if (typeof result.place_count !== "number" ||
      !Number.isInteger(result.place_count) || result.place_count < 0 ||
      typeof result.source !== "number" || !Number.isInteger(result.source) ||
      result.source < 0 || typeof result.sink !== "number" ||
      !Number.isInteger(result.sink) || result.sink < 0 ||
      !Array.isArray(result.activities) ||
      !result.activities.every((activity) => typeof activity === "string") ||
      !Array.isArray(result.transitions)) return false;
  const placeCount = result.place_count;
  const source = result.source;
  const sink = result.sink;
  const activities = result.activities as string[];
  const transitions = result.transitions as PetriTransition[];
  if (source >= placeCount || sink >= placeCount) return false;
  return transitions.every((transition) => {
    if (typeof transition !== "object" || transition === null) return false;
    const item = transition as Partial<PetriTransition>;
    return (item.activity === null ||
            (typeof item.activity === "number" && Number.isInteger(item.activity) &&
             item.activity >= 0 && item.activity < activities.length)) &&
      Array.isArray(item.inputs) && item.inputs.every((place) => Number.isInteger(place) && place >= 0 && place < placeCount) &&
      Array.isArray(item.outputs) && item.outputs.every((place) => Number.isInteger(place) && place >= 0 && place < placeCount);
  });
}

export interface PetriUsageRecord {
  id: number;
  log1_occurrences: number;
  log2_occurrences: number;
  total_occurrences: number;
  direction: number;
  label?: string;
  color?: string;
}
export interface PetriArcUsageRecord extends PetriUsageRecord {
  transition: number;
  place: number;
  kind: "input" | "output" | "reset" | "inhibitor";
  /** Compatibility projection: true for every place-to-transition arc. */
  input: boolean;
}
export interface PetriUsageComparison {
  family: "PETRI_USAGE_COMPARISON";
  places: PetriUsageRecord[];
  transitions: PetriUsageRecord[];
  arcs: PetriArcUsageRecord[];
}
export interface PetriUsageComparisonOptions {
  /** Optional token counts by place; omitted entries use the workflow default. */
  initial_marking?: number[];
  /** Optional target token counts by place; omitted entries use the workflow default. */
  final_marking?: number[];
  /** Optional event attribute used as the replay activity label. */
  activity_attribute?: string;
  /** Route through silent transitions to enable visible events. */
  walk_through_hidden_trans?: boolean;
  /** Route through silent transitions after the last event. */
  try_to_reach_final_marking_through_hidden?: boolean;
  /** Stop replaying a trace after its first deviation. */
  stop_immediately_unfit?: boolean;
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
  /** Event fields used to build the net; generic labels use local ids. */
  activity_attribute?: string;
  timestamp_attribute?: string;
  activity_ids_are_local?: boolean;
  activities?: string[];
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
  /** Event field used to form replay variants; absent in older exports. */
  activity_attribute?: string;
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
  /** Selects the process-tree Petri construction for the inductive model. */
  process_tree_variant?: ProcessTreePetriNetVariant;
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
  /** Event field used to form alignment variants. */
  activity_attribute?: string;
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
  /** Event field used to form alignment variants; absent in older exports. */
  activity_attribute?: string;
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

export interface DfgPrecisionResult {
  family: "DFG_PRECISION";
  computed: boolean;
  precision: number;
  prefix_occurrences: number;
  allowed_prefix_occurrences: number;
  activated_transitions: number;
  escaping_transitions: number;
  error: string;
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
  /** Places cleared when this transition fires. */
  reset_inputs?: number[];
  /** Places that must be empty for this transition to fire. */
  inhibitor_inputs?: number[];
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
    finite(rec.direction) &&
    (rec.label === undefined || typeof rec.label === "string") &&
    (rec.color === undefined || (typeof rec.color === "string" && /^#[0-9A-Fa-f]{6}$/.test(rec.color)));
};
export function isPetriUsageComparison(value: unknown): value is PetriUsageComparison {
  if (!isRecord(value) || value.family !== "PETRI_USAGE_COMPARISON" ||
      !Array.isArray(value.places) || !Array.isArray(value.transitions) || !Array.isArray(value.arcs)) return false;
  return value.places.every(isPetriUsageRecord) && value.transitions.every(isPetriUsageRecord) &&
    value.arcs.every((arc) => {
      if (!isPetriUsageRecord(arc) || !isRecord(arc)) return false;
      const rec = arc as Record<string, unknown>;
      const kind = rec.kind;
      return Number.isInteger(rec.transition) && (rec.transition as number) >= 0 &&
        Number.isInteger(rec.place) && (rec.place as number) >= 0 &&
        ["input", "output", "reset", "inhibitor"].includes(kind as string) &&
        typeof rec.input === "boolean" &&
        rec.input === (kind !== "output");
    });
}
export function isDfgAlignmentResult(value: unknown): value is DfgAlignmentResult {
  if (!isRecord(value) || value.family !== "DFG" || value.variant !== "CLASSIC" ||
      !Number.isInteger(value.unique_variants) || !Array.isArray(value.alignments)) return false;
  return value.alignments.every((a) => isRecord(a) && finite(a.cost) && finite(a.internal_cost) &&
    finite(a.fitness) && Array.isArray(a.moves) && a.moves.every((m) => isRecord(m) && typeof m.log === "string" && typeof m.model === "string"));
}
export function isDfgPrecisionResult(value: unknown): value is DfgPrecisionResult {
  if (!isRecord(value) || value.family !== "DFG_PRECISION" ||
      typeof value.computed !== "boolean" || !finite(value.precision) ||
      value.precision < 0 || value.precision > 1 || typeof value.error !== "string") return false;
  return ["prefix_occurrences", "allowed_prefix_occurrences",
    "activated_transitions", "escaping_transitions"].every((key) =>
    Number.isSafeInteger(value[key]) && (value[key] as number) >= 0);
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

export interface CompositeEvaluation {
  computed: boolean;
  error: string | null;
  fitness: EvaluationMetric;
  precision: EvaluationMetric;
  generalization: EvaluationMetric;
  simplicity: EvaluationMetric;
  metrics_average_weight: EvaluationMetric;
  fscore: EvaluationMetric;
  weights: {
    fitness: EvaluationMetric;
    precision: EvaluationMetric;
    simplicity: EvaluationMetric;
    generalization: EvaluationMetric;
  };
}
function isCompositeEvaluation(value: unknown): value is CompositeEvaluation {
  if (!isRecord(value) || typeof value.computed !== "boolean" ||
      !nullableString(value.error) || !metric(value.fitness) ||
      !metric(value.precision) || !metric(value.generalization) ||
      !metric(value.simplicity) || !metric(value.metrics_average_weight) ||
      !metric(value.fscore) || !isRecord(value.weights)) return false;
  return metric(value.weights.fitness) && metric(value.weights.precision) &&
    metric(value.weights.simplicity) && metric(value.weights.generalization);
}

/** Validates a standalone response from the model-quality endpoint. */
export function isModelEvaluation(value: unknown): value is ModelEvaluation {
  if (!isRecord(value) || typeof value.miner !== "string") return false;
  const present = [value.replay_fitness, value.precision, value.generalization, value.simplicity, value.composite]
    .filter((item) => item !== undefined).length;
  if (present !== 1) return false;
  return (value.replay_fitness === undefined || isReplayFitnessEvaluation(value.replay_fitness)) &&
    (value.precision === undefined || isPrecisionEvaluation(value.precision)) &&
    (value.generalization === undefined || isGeneralizationEvaluation(value.generalization)) &&
    (value.simplicity === undefined || isSimplicityEvaluation(value.simplicity)) &&
    (value.composite === undefined || isCompositeEvaluation(value.composite));
}

/** Validates the stochastic-language distance endpoint response. */
export type EmdVariant = "exact" | "pyemd";
export function isEmdResult(value: unknown): value is EmdResult {
  return isRecord(value) && (value.variant === undefined || value.variant === "exact" || value.variant === "pyemd") &&
    metric(value.distance) && typeof value.computed === "boolean" &&
    typeof value.normalized === "boolean" &&
    (value.use_fast_emd === undefined || typeof value.use_fast_emd === "boolean") &&
    nullableString(value.error);
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
  if (!isRecord(value) || !["sequence", "exclusive", "inclusive_or", "parallel", "interleaving", "loop", "activity", "tau"].includes(value.operator as string)) return false;
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
  "entire_event_log", "entire_dataframe", "polars_lazyframes",
  "trace_by_trace", "dfg", "petri_reach_graph",
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
  atomic_interleaving?: FootprintRelation[];
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
  entire_dataframe?: AggregateFootprint;
  polars_lazyframes?: AggregateFootprint;
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
  | "simplicity"
  | "composite";
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
  /** Selects the process-tree Petri construction for the inductive model. */
  process_tree_variant?: ProcessTreePetriNetVariant;
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
  fitness_weight?: number;
  precision_weight?: number;
  simplicity_weight?: number;
  generalization_weight?: number;
}

export type PetriAnalysisFamily =
  | "marking_equation"
  | "extended_marking_equation"
  | "workflow_net"
  | "soundness"
  | "woflan";

export interface PetriAnalysisOptions {
  /** Selects the process-tree Petri construction for the inductive model. */
  process_tree_variant?: ProcessTreePetriNetVariant;
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
  invariant_search_limit?: number;
  full_bootstrap?: boolean;
  diagnostics?: boolean;
  return_asap_when_unsound?: boolean;
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
  return_set_strings?: boolean;
}

export interface DeclarePlayoutOptions {
  traces?: number;
  min_length?: number;
  max_length?: number;
  seed?: number;
}

export interface ProcessTreePlayoutSetResult {
  family: "PROCESS_TREE_PLAYOUT";
  variant: "EXTENSIVE";
  return_set_strings: true;
  truncated: boolean;
  traces: string[][];
}

export function isProcessTreePlayoutSetResult(
  value: unknown,
): value is ProcessTreePlayoutSetResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<ProcessTreePlayoutSetResult>;
  if (result.family !== "PROCESS_TREE_PLAYOUT" ||
      result.variant !== "EXTENSIVE" || result.return_set_strings !== true ||
      typeof result.truncated !== "boolean" ||
      !Array.isArray(result.traces) ||
      !result.traces.every((trace) =>
        Array.isArray(trace) && trace.every((activity) => typeof activity === "string"))) {
    return false;
  }

  // The native extensive enumerator emits unique words ordered by length,
  // then lexicographically within each length. Keep the runtime guard aligned
  // with that deterministic contract so callers can rely on stable results.
  const comesBefore = (left: string[], right: string[]): boolean => {
    if (left.length !== right.length) return left.length < right.length;
    for (let i = 0; i < left.length; ++i) {
      if (left[i] !== right[i]) return left[i] < right[i];
    }
    return false;
  };
  for (let i = 1; i < result.traces.length; ++i) {
    if (!comesBefore(result.traces[i - 1]!, result.traces[i]!)) return false;
  }
  return true;
}

/** Controls uniform random execution of a Petri-net model. */
export interface BasicPlayoutOptions {
  process_tree_variant?: ProcessTreePetriNetVariant;
  mode?: "basic" | "extensive";
  traces?: number;
  max_steps?: number;
  max_length?: number;
  seed?: number;
  initial_marking?: number[];
  final_marking?: number[];
  start?: number;
  case_interval?: number;
  event_interval?: number;
  add_only_if_fm_is_reached?: boolean;
  fm_leq_accepted?: boolean;
  return_visited_elements?: boolean;
  return_elements?: boolean;
  max_marking_occ?: number;
}

export interface PlayoutVisitedMarking {
  kind: "marking";
  marking: number[];
}

export interface PlayoutVisitedTransition {
  kind: "transition";
  transition: number;
  activity: string | null;
}

export type PlayoutVisitedElement =
  | PlayoutVisitedMarking
  | PlayoutVisitedTransition;

export interface BasicPlayoutVisitedResult {
  family: "PETRI_PLAYOUT_VISITED";
  variant: "BASIC_PLAYOUT";
  truncated: boolean;
  trace_limit_reached: boolean;
  state_limit_reached: boolean;
  traces: PlayoutVisitedElement[][];
}

export interface StochasticPlayoutVisitedResult {
  family: "PETRI_PLAYOUT_VISITED";
  variant: "STOCHASTIC_PLAYOUT";
  truncated: boolean;
  trace_limit_reached: boolean;
  state_limit_reached: boolean;
  traces: PlayoutVisitedElement[][];
}

export interface ExtensivePlayoutVisitedResult {
  family: "PETRI_PLAYOUT_VISITED";
  variant: "EXTENSIVE_PLAYOUT";
  truncated: boolean;
  trace_limit_reached: boolean;
  state_limit_reached: boolean;
  traces: PlayoutVisitedElement[][];
}

export function isBasicPlayoutVisitedResult(
  value: unknown,
): value is BasicPlayoutVisitedResult {
  return isPlayoutVisitedResult(value, "BASIC_PLAYOUT");
}

export function isStochasticPlayoutVisitedResult(
  value: unknown,
): value is StochasticPlayoutVisitedResult {
  return isPlayoutVisitedResult(value, "STOCHASTIC_PLAYOUT");
}

export function isExtensivePlayoutVisitedResult(
  value: unknown,
): value is ExtensivePlayoutVisitedResult {
  return isPlayoutVisitedResult(value, "EXTENSIVE_PLAYOUT");
}

function isPlayoutVisitedResult(
  value: unknown,
  variant: "BASIC_PLAYOUT" | "STOCHASTIC_PLAYOUT" | "EXTENSIVE_PLAYOUT",
): boolean {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<BasicPlayoutVisitedResult | StochasticPlayoutVisitedResult>;
  if (result.family !== "PETRI_PLAYOUT_VISITED" ||
      result.variant !== variant || typeof result.truncated !== "boolean" ||
      typeof result.trace_limit_reached !== "boolean" ||
      typeof result.state_limit_reached !== "boolean" ||
      !Array.isArray(result.traces)) {
    return false;
  }
  return result.traces.every((trace) => {
    if (!Array.isArray(trace) || trace.length === 0 || trace.length % 2 === 0) {
      return false;
    }
    return trace.every((element, index) => {
      if (!element || typeof element !== "object") return false;
      const item = element as {
        kind?: unknown;
        marking?: unknown;
        transition?: unknown;
        activity?: unknown;
      };
      if (index % 2 === 0) {
        return item.kind === "marking" && Array.isArray(item.marking) &&
          item.marking.length > 0 &&
          item.marking.every((token) => Number.isSafeInteger(token) && token >= 0);
      }
      return item.kind === "transition" && typeof item.transition === "number" &&
        Number.isSafeInteger(item.transition) && item.transition >= 0 &&
        (typeof item.activity === "string" || item.activity === null);
    });
  });
}

/** Controls weighted random execution of a Petri-net model. */
export interface StochasticPlayoutOptions {
  process_tree_variant?: ProcessTreePetriNetVariant;
  traces?: number;
  max_steps?: number;
  seed?: number;
  transition_weights?: number[];
  fit_from_log?: boolean;
  stop_at_final?: boolean;
  initial_marking?: number[];
  final_marking?: number[];
  start?: number;
  case_interval?: number;
  event_interval?: number;
  return_visited_elements?: boolean;
}

export interface StochasticPetriOptions {
  transition_rates?: number[];
  initial_marking?: number[];
  immediate_transitions?: number[];
  max_states?: number;
  analysis?: "reachability" | "transient" | "steady_state";
  time_seconds?: number;
  initial_state?: number;
  initial_probabilities?: number[];
}

export interface StochasticPetriResult {
  family: "STOCHASTIC_PETRI";
  variant: "TANGIBLE_CTMC";
  exhausted: boolean;
  diagnostic: string;
  states: number[][];
  edges: Array<{ source: number; target: number; transition: number; rate: number }>;
  generator: number[][];
  initial_state?: number | null;
  initial_probabilities?: number[];
}

export interface StochasticPetriDistributionResult {
  family: "STOCHASTIC_PETRI_DISTRIBUTION";
  variant: "TRANSIENT" | "STEADY_STATE";
  computed: boolean;
  unique: boolean;
  diagnostic: string;
  time_seconds: number;
  initial_state: number | null;
  probabilities: number[];
}

export interface StochasticMapOptions {
  source?: "log" | "performance_dfg";
  distribution?: "deterministic" | "exponential" | "constant0" | "immediate" |
    "normal" | "uniform" | "lognormal" | "gamma";
  default_parameter?: number;
  silent_transitions_immediate?: boolean;
  zero_duration_immediate?: boolean;
  fit_activation_weights?: boolean;
  fit_timing?: boolean;
  default_activation_weight?: number;
  frequency_activation_weights?: boolean;
  conversion_variant?: "activity_defines_place" | "invisibles_no_duplicates";
  initial_marking?: number[];
  max_states?: number;
}

export type StochasticMapDistribution =
  NonNullable<StochasticMapOptions["distribution"]>;

export interface StochasticMapTransition {
  transition: number;
  timing: "timed" | "immediate";
  distribution: StochasticMapDistribution;
  parameter: number;
  secondary_parameter: number;
  tertiary_parameter: number;
  activation_weight: number;
  priority: number;
}

export interface StochasticMapResult {
  family: "STOCHASTIC_MAP";
  source: "log" | "performance_dfg";
  map: { family: "STOCHASTIC_MAP"; transitions: StochasticMapTransition[] };
  net?: Record<string, unknown>;
  diagnostics?: { replayed_traces: number; skipped_traces: number };
  reachability: StochasticPetriResult;
}

export function isStochasticMapResult(
  value: unknown,
): value is StochasticMapResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<StochasticMapResult>;
  if ((result.family !== "STOCHASTIC_MAP") ||
      (result.source !== "log" && result.source !== "performance_dfg") ||
      !result.map || typeof result.map !== "object" ||
      (result.map as { family?: unknown }).family !== "STOCHASTIC_MAP" ||
      !Array.isArray((result.map as { transitions?: unknown }).transitions) ||
      !isStochasticPetriResult(result.reachability)) return false;
  const transitions = (result.map as { transitions: unknown[] }).transitions;
  if (!transitions.every((item) => {
    if (!item || typeof item !== "object") return false;
    const transition = item as Partial<StochasticMapTransition>;
    const distribution = transition.distribution;
    const distributions: StochasticMapDistribution[] = [
      "deterministic", "exponential", "constant0", "immediate", "normal",
      "uniform", "lognormal", "gamma",
    ];
    if (!distributions.includes(distribution as StochasticMapDistribution))
      return false;
    return Number.isSafeInteger(transition.transition) &&
      (transition.transition as number) >= 0 &&
      (transition.timing === "timed" || transition.timing === "immediate") &&
      Number.isFinite(transition.parameter) &&
      Number.isFinite(transition.secondary_parameter) &&
      (transition.secondary_parameter as number) >= 0 &&
      Number.isFinite(transition.tertiary_parameter) &&
      (transition.tertiary_parameter as number) >= 0 &&
      Number.isFinite(transition.activation_weight) &&
      (transition.activation_weight as number) >= 0 &&
      Number.isSafeInteger(transition.priority) &&
      (transition.priority as number) >= -2147483648 &&
      (transition.priority as number) <= 2147483647 &&
      (distribution === "lognormal" ||
       (transition.parameter as number) >= 0) &&
      (distribution !== "exponential" || transition.timing === "immediate" ||
       (transition.parameter as number) > 0) &&
      (distribution !== "uniform" ||
       (transition.secondary_parameter as number) >=
         (transition.parameter as number)) &&
      (distribution !== "gamma" ||
       ((transition.parameter as number) > 0 &&
        (transition.secondary_parameter as number) > 0)) &&
      ((distribution !== "constant0" && distribution !== "immediate") ||
       ((transition.parameter as number) === 0 &&
        (transition.secondary_parameter as number) === 0 &&
        (transition.tertiary_parameter as number) === 0));
  })) return false;
  if (result.diagnostics !== undefined) {
    if (!Number.isSafeInteger(result.diagnostics.replayed_traces) ||
        !Number.isSafeInteger(result.diagnostics.skipped_traces) ||
        result.diagnostics.replayed_traces < 0 ||
        result.diagnostics.skipped_traces < 0) return false;
  }
  return transitions.every((item, index) =>
    (item as StochasticMapTransition).transition === index);
}

export function isStochasticPetriResult(
  value: unknown,
): value is StochasticPetriResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<StochasticPetriResult>;
  if (result.family !== "STOCHASTIC_PETRI" ||
      result.variant !== "TANGIBLE_CTMC" ||
      typeof result.exhausted !== "boolean" ||
      typeof result.diagnostic !== "string" ||
      !Array.isArray(result.states) || !Array.isArray(result.edges) ||
      !Array.isArray(result.generator)) return false;
  if (!result.states.every((state) => Array.isArray(state) &&
      state.every((token) => Number.isSafeInteger(token) && token >= 0))) return false;
  if (!result.edges.every((edge) => edge && Number.isSafeInteger(edge.source) &&
      Number.isSafeInteger(edge.target) && Number.isSafeInteger(edge.transition) &&
      edge.source >= 0 && edge.target >= 0 &&
      edge.source < result.states!.length && edge.target < result.states!.length &&
      Number.isFinite(edge.rate) && edge.rate >= 0)) return false;
  if (result.initial_state !== undefined && result.initial_state !== null &&
      !(Number.isSafeInteger(result.initial_state) && result.initial_state >= 0 &&
        result.initial_state < result.states.length))
    return false;
  if (result.initial_probabilities !== undefined) {
    if (!Array.isArray(result.initial_probabilities) ||
        (result.initial_probabilities.length !== result.states.length &&
         !(result.exhausted && result.initial_probabilities.length === 0)) ||
        !result.initial_probabilities.every((probability) =>
          Number.isFinite(probability) && probability >= 0)) return false;
    const mass = result.initial_probabilities.reduce(
      (sum, probability) => sum + probability, 0);
    if (result.initial_probabilities.length > 0 &&
        (!Number.isFinite(mass) || Math.abs(mass - 1) > 1e-8)) return false;
  }
  return result.generator.length === result.states.length &&
    result.generator.every((row) => Array.isArray(row) &&
    row.length === result.states!.length && row.every(Number.isFinite));
}

export function isStochasticPetriDistributionResult(
  value: unknown,
): value is StochasticPetriDistributionResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<StochasticPetriDistributionResult>;
  if (result.family !== "STOCHASTIC_PETRI_DISTRIBUTION" ||
      (result.variant !== "TRANSIENT" && result.variant !== "STEADY_STATE") ||
      typeof result.computed !== "boolean" || typeof result.unique !== "boolean" ||
      typeof result.diagnostic !== "string" || !Number.isFinite(result.time_seconds) ||
      (result.initial_state !== null &&
       !(typeof result.initial_state === "number" &&
         Number.isSafeInteger(result.initial_state) && result.initial_state >= 0)) ||
      !Array.isArray(result.probabilities) ||
      !result.probabilities.every((probability) =>
        Number.isFinite(probability) && probability >= 0)) return false;
  const mass = result.probabilities.reduce((sum, probability) => sum + probability, 0);
  return !result.computed || (Number.isFinite(mass) && Math.abs(mass - 1) <= 1e-8);
}

/** Controls bounded virtual-time Petri-net Monte Carlo simulation. */
export interface MonteCarloOptions {
  process_tree_variant?: ProcessTreePetriNetVariant;
  simulations?: number;
  case_arrival_ratio?: number;
  case_arrival_seconds?: number;
  activity_key?: string;
  timestamp_key?: string;
  initial_marking?: number[];
  final_marking?: number[];
  max_steps?: number;
  seed?: number;
  start?: number;
  default_duration_seconds?: number;
  force_distribution?: "normal" | "uniform" | "exponential";
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
  provided_stochastic_map?: StochasticMapTransition[];
  token_replay_variant?: "forward" | "backwards";
  diagnostic_interval?: number;
  small_scale_factor?: number;
  max_thread_execution_time?: number;
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
  case_arrival_ratio: number;
  total_cases_time: number;
  median_cases_ex_time: number;
  cases_ex_time: number[];
}

export interface MonteCarloInterval {
  start: number;
  end: number;
}

export interface MonteCarloPlaceIntervalTree {
  place_index: number;
  intervals: MonteCarloInterval[];
}

export interface MonteCarloTransitionIntervalTree {
  transition_index: number;
  intervals: MonteCarloInterval[];
}

export interface MonteCarloResult {
  family: "MONTE_CARLO";
  variant: "PETRI_SEMAPH_FIFO";
  log: { trace_count: number; event_count: number };
  diagnostics: MonteCarloDiagnostics;
  output_places_interval_trees: MonteCarloPlaceIntervalTree[];
  output_transitions_interval_trees: MonteCarloTransitionIntervalTree[];
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
        d.median_case_execution_seconds, d.case_arrival_ratio,
        d.total_cases_time, d.median_cases_ex_time].every((item) =>
          Number.isFinite(item) && item >= 0) ||
      !Array.isArray(d.case_execution_seconds) ||
      d.case_execution_seconds.some((item) => !Number.isFinite(item) || item < 0) ||
      !Array.isArray(d.cases_ex_time) ||
      d.cases_ex_time.some((item) => !Number.isFinite(item) || item < 0)) return false;
  const validIntervals = (value: unknown, indexKey: "place_index" | "transition_index") =>
    Array.isArray(value) && value.every((item, index) => {
      if (!item || typeof item !== "object") return false;
      const tree = item as Record<string, unknown>;
      if (tree[indexKey] !== index || !Array.isArray(tree.intervals)) return false;
      return tree.intervals.every((interval) => {
        if (!interval || typeof interval !== "object") return false;
        const bounds = interval as Record<string, unknown>;
        return typeof bounds.start === "number" && Number.isFinite(bounds.start) &&
          typeof bounds.end === "number" && Number.isFinite(bounds.end) &&
          bounds.end >= bounds.start;
      });
    });
  return validIntervals(result.output_places_interval_trees, "place_index") &&
    validIntervals(result.output_transitions_interval_trees, "transition_index");
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
  activity_key?: string;
  timestamp_key?: string;
  case_id_key?: string;
  artificial_start_activity?: string;
  artificial_end_activity?: string;
}

export type DfgPlayoutResultVariant = "CLASSIC" | "PERFORMANCE";
export interface DfgPlayoutVariantEntry {
  activities: string[];
  occurrences: number;
}
export interface DfgPlayoutVariantsResult {
  family: "DFG_PLAYOUT";
  variant: DfgPlayoutResultVariant;
  return_variants: true;
  complete: boolean;
  interrupted: boolean;
  completed_traces: number;
  truncated_traces: number;
  variants: DfgPlayoutVariantEntry[];
}

export function isDfgPlayoutVariantsResult(
  value: unknown,
): value is DfgPlayoutVariantsResult {
  if (!value || typeof value !== "object") return false;
  const result = value as Partial<DfgPlayoutVariantsResult>;
  if (result.family !== "DFG_PLAYOUT" ||
      (result.variant !== "CLASSIC" && result.variant !== "PERFORMANCE") ||
      result.return_variants !== true ||
      typeof result.complete !== "boolean" ||
      typeof result.interrupted !== "boolean" ||
      !Number.isSafeInteger(result.completed_traces) || result.completed_traces! < 0 ||
      !Number.isSafeInteger(result.truncated_traces) || result.truncated_traces! < 0 ||
      !Array.isArray(result.variants)) return false;
  return result.variants.every((entry) => {
    if (!entry || typeof entry !== "object") return false;
    const item = entry as Partial<DfgPlayoutVariantEntry>;
    const occurrences = item.occurrences;
    return Array.isArray(item.activities) && item.activities.length > 0 &&
      item.activities.every((activity) => typeof activity === "string") &&
      typeof occurrences === "number" && Number.isSafeInteger(occurrences) &&
      occurrences >= 0;
  });
}

/** Response from the standalone Petri-analysis endpoint. */
export interface PetriAnalysisResult {
  family: PetriAnalysisFamily;
  variant?: string;
  result?: Record<string, unknown>;
  computed?: boolean;
  feasible?: boolean;
  exhausted?: boolean;
  returned_asap?: boolean;
  objective?: number;
  lower_bound?: number;
  status?: "sound" | "unsound" | "unknown";
  dead_transitions?: number[];
  dead_tasks?: number[];
  deadlock_witnesses?: number[][];
  improper_witnesses?: number[][];
  completion_witness?: number[];
  locking_witness?: number[];
  locking_scenarios?: number[][];
  handled_pairs?: number[][] | null;
  not_well_handled_pairs?: number[][] | null;
  structural_node_labels?: string[];
  place_invariant_basis?: number[][];
  uniform_place_invariants?: number[][];
  weighted_place_invariants?: number[][];
  uncovered_places?: number[];
  uncovered_places_uniform?: number[];
  uniform_invariant_search_exhausted?: boolean;
  uncovered_places_weighted?: number[];
  uncovered_places_s_component?: number[];
  left?: boolean | null;
  coverability_markings?: number[][];
  coverability_edges?: number[][];
  restricted_coverability_tree_exhausted?: boolean;
  restricted_coverability_markings?: number[][];
  restricted_coverability_edges?: number[][];
  reachability_markings?: number[][];
  reachability_edges?: number[][];
  short_circuit_valid?: boolean;
  short_circuit_strongly_connected?: boolean;
  short_circuit_transition?: number | null;
  short_circuit_diagnostic_messages?: string[];
  short_circuit_reachability_exhausted?: boolean;
  short_circuit_reachability_strongly_connected?: boolean;
  short_circuit_reachability_markings?: number[][];
  short_circuit_reachability_edges?: number[][];
  error?: string;
  [key: string]: unknown;
}

export function isPetriAnalysisResult(value: unknown): value is PetriAnalysisResult {
  if (!value || typeof value !== "object") return false;
  const family = (value as { family?: unknown }).family;
  if (family !== "marking_equation" && family !== "extended_marking_equation" &&
      family !== "workflow_net" && family !== "soundness" && family !== "woflan") return false;
  if (family !== "soundness" && family !== "woflan") return true;
  if ("variant" in value && value.variant !== undefined &&
      value.variant !== "soundness" && value.variant !== "woflan") return false;
  if ("returned_asap" in value && value.returned_asap !== undefined &&
      typeof value.returned_asap !== "boolean") return false;
  if ("short_circuit_valid" in value && value.short_circuit_valid !== undefined &&
      typeof value.short_circuit_valid !== "boolean") return false;
  if ("short_circuit_strongly_connected" in value &&
      value.short_circuit_strongly_connected !== undefined &&
      typeof value.short_circuit_strongly_connected !== "boolean") return false;
  if ("short_circuit_transition" in value &&
      value.short_circuit_transition !== undefined &&
      value.short_circuit_transition !== null &&
      (typeof value.short_circuit_transition !== "number" ||
       !Number.isSafeInteger(value.short_circuit_transition) ||
       value.short_circuit_transition < 0)) return false;
  if ("short_circuit_reachability_exhausted" in value &&
      value.short_circuit_reachability_exhausted !== undefined &&
      typeof value.short_circuit_reachability_exhausted !== "boolean") return false;
  if ("uniform_invariant_search_exhausted" in value &&
      value.uniform_invariant_search_exhausted !== undefined &&
      typeof value.uniform_invariant_search_exhausted !== "boolean") return false;
  if ("short_circuit_reachability_strongly_connected" in value &&
      value.short_circuit_reachability_strongly_connected !== undefined &&
      typeof value.short_circuit_reachability_strongly_connected !== "boolean") return false;
  if ("left" in value && value.left !== undefined && value.left !== null &&
      typeof value.left !== "boolean") return false;
  if ("restricted_coverability_tree_exhausted" in value &&
      value.restricted_coverability_tree_exhausted !== undefined &&
      typeof value.restricted_coverability_tree_exhausted !== "boolean") return false;
  const result = value as {
    handled_pairs?: unknown; not_well_handled_pairs?: unknown;
    structural_node_labels?: unknown;
    dead_transitions?: unknown; dead_tasks?: unknown;
    deadlock_witnesses?: unknown; improper_witnesses?: unknown;
    completion_witness?: unknown;
    locking_witness?: unknown; locking_scenarios?: unknown;
    place_invariant_basis?: unknown; uniform_place_invariants?: unknown;
    weighted_place_invariants?: unknown;
    uncovered_places?: unknown; uncovered_places_uniform?: unknown;
    uncovered_places_weighted?: unknown; uncovered_places_s_component?: unknown;
    coverability_markings?: unknown; coverability_edges?: unknown;
    restricted_coverability_markings?: unknown;
    restricted_coverability_edges?: unknown;
    reachability_markings?: unknown; reachability_edges?: unknown;
    short_circuit_diagnostic_messages?: unknown;
    short_circuit_reachability_markings?: unknown;
    short_circuit_reachability_edges?: unknown;
  };
  const validIndices = (indices: unknown) => indices === undefined ||
    (Array.isArray(indices) && indices.every((item) => Number.isSafeInteger(item) && item >= 0));
  const validPairs = (pairs: unknown) => pairs === undefined || pairs === null ||
    (Array.isArray(pairs) && pairs.every((pair) => Array.isArray(pair) && pair.length === 2 &&
      pair.every((item) => Number.isSafeInteger(item) && item >= 0)));
  const validScenarios = result.locking_scenarios === undefined ||
    (Array.isArray(result.locking_scenarios) && result.locking_scenarios.every(validIndices));
  const validWitnesses = (witnesses: unknown) => witnesses === undefined ||
    (Array.isArray(witnesses) && witnesses.every(validIndices));
  const validInvariantFamily = (family: unknown) => family === undefined ||
    (Array.isArray(family) && family.every((vector) => Array.isArray(vector) &&
      vector.every((item) => typeof item === "number" && Number.isFinite(item) && item >= 0)));
  const validPlaces = (places: unknown) => validIndices(places);
  const validMarkings = (markings: unknown, omega: boolean) => markings === undefined ||
    (Array.isArray(markings) && markings.every((marking) => Array.isArray(marking) &&
      marking.every((item) => Number.isSafeInteger(item) && (omega ? item >= -1 : item >= 0))));
  const validGraphEdges = (edges: unknown) => edges === undefined ||
    (Array.isArray(edges) && edges.every((edge) => Array.isArray(edge) && edge.length === 3 &&
      edge.every((item) => Number.isSafeInteger(item) && item >= 0)));
  const validLabels = result.structural_node_labels === undefined ||
    (Array.isArray(result.structural_node_labels) &&
      result.structural_node_labels.every((label) => typeof label === "string"));
  const validMessages = result.short_circuit_diagnostic_messages === undefined ||
    (Array.isArray(result.short_circuit_diagnostic_messages) &&
      result.short_circuit_diagnostic_messages.every((message) => typeof message === "string"));
  return validIndices(result.dead_transitions) && validIndices(result.dead_tasks) &&
    validIndices(result.completion_witness) && validIndices(result.locking_witness) &&
    validWitnesses(result.deadlock_witnesses) && validWitnesses(result.improper_witnesses) &&
    validScenarios &&
    validPairs(result.handled_pairs) && validPairs(result.not_well_handled_pairs) && validLabels &&
    validInvariantFamily(result.place_invariant_basis) &&
    validInvariantFamily(result.uniform_place_invariants) &&
    validInvariantFamily(result.weighted_place_invariants) &&
    validPlaces(result.uncovered_places) &&
    validPlaces(result.uncovered_places_uniform) &&
    validPlaces(result.uncovered_places_weighted) &&
    validPlaces(result.uncovered_places_s_component) &&
    validMarkings(result.coverability_markings, true) &&
    validMarkings(result.restricted_coverability_markings, true) &&
    validMarkings(result.reachability_markings, false) &&
    validGraphEdges(result.coverability_edges) &&
    validGraphEdges(result.restricted_coverability_edges) &&
    validGraphEdges(result.reachability_edges) && validMessages &&
    validMarkings(result.short_circuit_reachability_markings, false) &&
    validGraphEdges(result.short_circuit_reachability_edges);
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
  composite?: CompositeEvaluation;
}

export interface EmdOptions {
  emd_variant?: EmdVariant;
  normalize_masses?: boolean;
  strict_equal_mass?: boolean;
  use_fast_emd?: boolean;
}
export interface EmdResult {
  variant?: EmdVariant;
  distance: EvaluationMetric;
  computed: boolean;
  normalized: boolean;
  use_fast_emd?: boolean;
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
  variant:
    | "connected" | "disconnected" | "dataframe" | "log"
    | "dataframe_disconnected" | "log_disconnected";
  activities: string[];
  activity_attribute?: string;
  timestamp_attribute?: string;
  case_id_attribute?: string;
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
  activity: number | null;
  activity_label?: string;
  resource: string;
  batch_count: number;
  event_count: number;
  batches: BatchInstance[];
}
export interface BatchDiscoveryExport {
  variant?: "log" | "pandas" | "polars";
  activity_attribute?: string;
  resource_attribute?: string;
  timestamp_attribute?: string;
  case_id_attribute?: string;
  start_timestamp_attribute?: string;
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

export interface CaseStatisticsOptions {
  case_statistics_case_id_key?: string;
  case_statistics_timestamp_key?: string;
  case_statistics_enable_sort?: boolean;
  case_statistics_sort_by?: "case_id" | "start_time" | "end_time" | "duration";
  case_statistics_sort_ascending?: boolean;
  case_statistics_max_cases?: number;
  case_statistics_business_hours?: boolean;
  case_statistics_business_hour_slots?: string;
}

export interface CaseStatisticsEntry {
  case_id: string;
  start_time: number;
  end_time: number;
  case_duration: number;
}

export interface CaseStatisticsResult {
  family: "CASE_STATISTICS";
  variant: "CLASSIC";
  case_id_key: string;
  timestamp_key: string;
  enable_sort: boolean;
  sort_by: "CASE_ID" | "START_TIME" | "END_TIME" | "DURATION";
  sort_ascending: boolean;
  max_cases: number;
  use_business_hours: boolean;
  cases: CaseStatisticsEntry[];
}

export function isCaseStatisticsResult(
  value: unknown,
): value is CaseStatisticsResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<CaseStatisticsResult>;
  if (data.family !== "CASE_STATISTICS" || data.variant !== "CLASSIC" ||
      typeof data.case_id_key !== "string" || typeof data.timestamp_key !== "string" ||
      typeof data.enable_sort !== "boolean" ||
      !["CASE_ID", "START_TIME", "END_TIME", "DURATION"].includes(data.sort_by ?? "") ||
      typeof data.sort_ascending !== "boolean" ||
      typeof data.max_cases !== "number" || !Number.isInteger(data.max_cases) ||
      data.max_cases < 0 || typeof data.use_business_hours !== "boolean" ||
      !Array.isArray(data.cases)) return false;
  return data.cases.every((entry) => {
    if (typeof entry !== "object" || entry === null) return false;
    const item = entry as Partial<CaseStatisticsEntry>;
    return typeof item.case_id === "string" &&
      typeof item.start_time === "number" && Number.isFinite(item.start_time) &&
      typeof item.end_time === "number" && Number.isFinite(item.end_time) &&
      typeof item.case_duration === "number" && Number.isFinite(item.case_duration) &&
      item.case_duration >= 0;
  });
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

export interface AttributeDistributionOptions {
  attribute_distribution_type?:
    | "days_month" | "months" | "years" | "hours" | "days_week" | "weeks";
  attribute_distribution_timestamp_key?: string;
}

export interface AttributeDistributionResult {
  family: "ATTRIBUTE_DISTRIBUTION";
  variant: "DAYS_MONTH" | "MONTHS" | "YEARS" | "HOURS" | "DAYS_WEEK" | "WEEKS";
  timestamp_key: string;
  labels: string[];
  counts: number[];
  timed_events: number;
}

export function isAttributeDistributionResult(
  value: unknown,
): value is AttributeDistributionResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<AttributeDistributionResult>;
  if (data.family !== "ATTRIBUTE_DISTRIBUTION" ||
      !["DAYS_MONTH", "MONTHS", "YEARS", "HOURS", "DAYS_WEEK", "WEEKS"].includes(data.variant ?? "") ||
      typeof data.timestamp_key !== "string" || !Array.isArray(data.labels) ||
      !data.labels.every((label) => typeof label === "string") ||
      !Array.isArray(data.counts) || data.labels.length !== data.counts.length ||
      typeof data.timed_events !== "number" || !Number.isInteger(data.timed_events) ||
      data.timed_events < 0) return false;
  return data.counts.every((count) => typeof count === "number" &&
    Number.isInteger(count) && count >= 0);
}

export interface AttributeKdeOptions {
  attribute_kde_key?: string;
  attribute_kde_variant?: "numeric" | "date";
  attribute_kde_graph_points?: number;
  attribute_kde_points_to_sample?: number;
}

export interface AttributeKdeResult {
  family: "ATTRIBUTE_KDE";
  variant: "NUMERIC" | "DATE";
  attribute_key: string;
  x: number[];
  y: number[];
}

export function isAttributeKdeResult(
  value: unknown,
): value is AttributeKdeResult {
  if (typeof value !== "object" || value === null) return false;
  const data = value as Partial<AttributeKdeResult>;
  if (data.family !== "ATTRIBUTE_KDE" ||
      (data.variant !== "NUMERIC" && data.variant !== "DATE") ||
      typeof data.attribute_key !== "string" || !data.attribute_key ||
      !Array.isArray(data.x) || !Array.isArray(data.y) ||
      data.x.length !== data.y.length) return false;
  if (!data.x.every((point) => typeof point === "number" && Number.isFinite(point)) ||
      !data.y.every((density) => typeof density === "number" &&
        Number.isFinite(density) && density >= 0)) return false;
  for (let i = 1; i < data.x.length; ++i) {
    if (data.x[i] < data.x[i - 1]) return false;
  }
  return true;
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
  /** Traces whose replay or alignment path contributed observations. */
  observed_traces: number;
  /** Optional path diagnostics added by newer decision-mining responses. */
  replayed_traces?: number;
  aligned_traces?: number;
  alignment_failures?: number;
  trace_count: number;
  decision_points: DecisionPoint[];
  /** Event field used to form replay variants; absent in older responses. */
  activity_attribute?: string;
}

export interface DataPetriNetTransition {
  index: number;
  activity: string | null;
  silent: boolean;
  inputs: number[];
  outputs: number[];
  guard: string | null;
  read_variables: string[];
  write_variables: string[];
}

export interface DecisionTableRow {
  features: Array<number | null>;
  target: number;
}

export interface EncodedDecisionTable {
  place: number;
  classes: string[];
  feature_names: string[];
  rows: DecisionTableRow[];
}

export interface DecisionTablesResult {
  family: "DECISION_TABLES";
  variant: "encoded";
  /** Event field used to form replay variants; absent in older responses. */
  activity_attribute?: string;
  trace_count: number;
  observed_traces: number;
  replayed_traces?: number;
  aligned_traces?: number;
  alignment_failures?: number;
  tables: EncodedDecisionTable[];
}

function validDecisionDiagnostics(
  value: Record<string, unknown>,
  traceCount: number,
  observed: number,
): boolean {
  if (observed < 0 || observed > traceCount) return false;
  const keys = ["replayed_traces", "aligned_traces", "alignment_failures"] as const;
  const present = keys.map((key) => value[key] !== undefined);
  if (present.every((entry) => !entry)) return true;
  if (present.some((entry) => !entry)) return false;
  const replayed = value.replayed_traces;
  const aligned = value.aligned_traces;
  const failures = value.alignment_failures;
  if (![replayed, aligned, failures].every((entry) =>
    Number.isSafeInteger(entry) && (entry as number) >= 0 &&
    (entry as number) <= traceCount)) return false;
  return (replayed as number) + (aligned as number) === observed &&
    (failures as number) + observed === traceCount;
}

export function isDecisionTablesResult(value: unknown): value is DecisionTablesResult {
  if (!isOcelRecord(value) || value.family !== "DECISION_TABLES" ||
      value.variant !== "encoded" || !Number.isSafeInteger(value.trace_count) ||
      (value.trace_count as number) < 0 || !Number.isSafeInteger(value.observed_traces) ||
      (value.activity_attribute !== undefined &&
        typeof value.activity_attribute !== "string") ||
      !Array.isArray(value.tables)) return false;
  if (!validDecisionDiagnostics(value, value.trace_count as number,
                               value.observed_traces as number)) return false;
  return value.tables.every((rawTable) => {
    if (!isOcelRecord(rawTable)) return false;
    const table = rawTable;
    const classes = table.classes;
    const featureNames = table.feature_names;
    const rows = table.rows;
    if (!Number.isSafeInteger(table.place) || (table.place as number) < 0 ||
        !Array.isArray(classes) || !classes.every((entry) => typeof entry === "string") ||
        !Array.isArray(featureNames) ||
        !featureNames.every((entry) => typeof entry === "string") ||
        !Array.isArray(rows)) return false;
    return rows.every((rawRow) => {
      if (!isOcelRecord(rawRow)) return false;
      const row = rawRow;
      const features = row.features;
      return Array.isArray(features) && features.length === featureNames.length &&
        features.every((entry) => entry === null || finite(entry)) &&
        Number.isSafeInteger(row.target) && (row.target as number) >= 0 &&
        (row.target as number) < classes.length;
    });
  });
}

export interface DataPetriNetResult {
  family: "DATA_PETRI_NET";
  variant: string;
  miner?: string;
  place_count: number;
  source: number;
  sink: number;
  activities: string[];
  transitions: DataPetriNetTransition[];
  decisions: Decisions;
  decision_tables?: DecisionTablesResult;
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
  minimum_self_distance_variant?: "log" | "pandas" | "polars";
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
  /** Encoded decision tables embedded by the CLI's --decision-tables flag. */
  decision_tables?: DecisionTablesResult;
}

export function isMineraExport(value: unknown): value is MineraExport {
  if (typeof value !== "object" || value === null) return false;
  const data = value as MineraExport;
  const overlap = data.overlap;
  const batches = data.batch_discovery;
  const validBatches = batches === undefined || (
    typeof batches === "object" && batches !== null &&
    Array.isArray(batches.groups) &&
    (batches.variant === undefined ||
      batches.variant === "log" || batches.variant === "pandas" ||
      batches.variant === "polars") &&
    [batches.activity_attribute, batches.resource_attribute,
      batches.timestamp_attribute, batches.case_id_attribute,
      batches.start_timestamp_attribute].every((attribute) =>
        attribute === undefined || typeof attribute === "string") &&
    batches.groups.every((group) => {
      if (typeof group !== "object" || group === null ||
          !(typeof group.activity === "number" || group.activity === null) ||
          (typeof group.activity === "number" &&
            (!Number.isSafeInteger(group.activity) || group.activity < 0)) ||
          (group.activity_label !== undefined &&
            typeof group.activity_label !== "string") ||
          typeof group.resource !== "string" ||
          !Number.isSafeInteger(group.batch_count) || group.batch_count < 0 ||
          !Number.isSafeInteger(group.event_count) || group.event_count < 0 ||
          !Array.isArray(group.batches)) return false;
      return group.batches.every((batch) =>
        typeof batch === "object" && batch !== null &&
        (batch.type === "simultaneous" ||
          batch.type === "batching_on_start" ||
          batch.type === "batching_on_end" || batch.type === "sequential" ||
          batch.type === "concurrent") &&
        typeof batch.start === "number" && Number.isFinite(batch.start) &&
        typeof batch.complete === "number" && Number.isFinite(batch.complete) &&
        Array.isArray(batch.events));
    }));
  return Array.isArray(data.activities) &&
    Array.isArray(data.edges) &&
    Array.isArray(data.variants) &&
    Number.isFinite(data.trace_count) &&
    (data.minimum_self_distance_variant === undefined ||
      data.minimum_self_distance_variant === "log" ||
      data.minimum_self_distance_variant === "pandas" ||
      data.minimum_self_distance_variant === "polars") &&
    (data.powl === undefined || isPowlNode(data.powl, data.activities.length)) &&
    (data.decision_tables === undefined || isDecisionTablesResult(data.decision_tables)) &&
    validBatches &&
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

export function isDataPetriNetResult(value: unknown): value is DataPetriNetResult {
  if (!isOcelRecord(value) || value.family !== "DATA_PETRI_NET" ||
      typeof value.variant !== "string" ||
      !Number.isSafeInteger(value.place_count) || (value.place_count as number) < 1 ||
      !Number.isSafeInteger(value.source) || (value.source as number) < 0 ||
      !Number.isSafeInteger(value.sink) || (value.sink as number) < 0 ||
      !Array.isArray(value.activities) ||
      !value.activities.every((activity) => typeof activity === "string") ||
      !Array.isArray(value.transitions) || !isOcelRecord(value.decisions)) return false;
  if (!Number.isSafeInteger(value.decisions.observed_traces) ||
      (value.decisions.observed_traces as number) < 0 ||
      !Number.isSafeInteger(value.decisions.trace_count) ||
      (value.decisions.trace_count as number) < 0 ||
      !validDecisionDiagnostics(value.decisions,
                                value.decisions.trace_count as number,
                                value.decisions.observed_traces as number) ||
      !Array.isArray(value.decisions.decision_points) ||
      (value.decisions.activity_attribute !== undefined &&
        typeof value.decisions.activity_attribute !== "string") ||
      (value.decision_tables !== undefined && !isDecisionTablesResult(value.decision_tables))) return false;
  return value.transitions.every((transition) => isOcelRecord(transition) &&
    Number.isSafeInteger(transition.index) && (transition.index as number) >= 0 &&
    (typeof transition.activity === "string" || transition.activity === null) &&
    typeof transition.silent === "boolean" && Array.isArray(transition.inputs) &&
    transition.inputs.every((place) => Number.isSafeInteger(place) && (place as number) >= 0) &&
    Array.isArray(transition.outputs) &&
    transition.outputs.every((place) => Number.isSafeInteger(place) && (place as number) >= 0) &&
    (typeof transition.guard === "string" || transition.guard === null) &&
    Array.isArray(transition.read_variables) &&
    transition.read_variables.every((variable) => typeof variable === "string") &&
    Array.isArray(transition.write_variables) &&
    transition.write_variables.every((variable) => typeof variable === "string"));
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
