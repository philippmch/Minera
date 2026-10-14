import type { LocalDiagnosticsResult, MineraExport, NetworkAnalysisResult, ResourceProfilesResult, RoleDiscoveryResult } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "./native-response";
import { diagnosticCsv, nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";
import type { LogQuerySchema } from "./log-query";

export const ORGANIZATIONAL_FAMILIES = ["roles", "profiles", "diagnostics", "network"] as const;
export type OrganizationalFamily = typeof ORGANIZATIONAL_FAMILIES[number];
export const ORGANIZATIONAL_TITLES: Record<OrganizationalFamily, string> = {
  roles: "Role discovery", profiles: "Resource profiles", diagnostics: "Local diagnostics", network: "Attribute network",
};
export const ORGANIZATIONAL_PATHS: Record<OrganizationalFamily, string> = {
  roles: "/api/roles", profiles: "/api/resource-profiles", diagnostics: "/api/local-diagnostics", network: "/api/network-analysis",
};
export const RESOURCE_PROFILE_METRICS = ["distinct_activities", "activity_frequency", "activity_completions", "case_completions", "fraction_case_completions", "average_workload", "multitasking", "average_duration_activity", "average_case_duration", "interaction_two_resources", "social_position"] as const;
export interface OrganizationalControl {
  key: string; label: string; kind: "text" | "integer" | "number" | "boolean" | "json" | "enum"; choices?: readonly string[];
}
export const ORGANIZATIONAL_CONTROLS: Record<OrganizationalFamily, readonly OrganizationalControl[]> = {
  roles: [
    {key: "roles_variant", label: "Roles variant (LOG / PANDAS)", kind: "enum", choices: ["log", "pandas"]},
    {key: "roles_activity_attribute", label: "Roles activity attribute", kind: "text"},
    {key: "roles_resource_attribute", label: "Roles resource attribute", kind: "text"},
    {key: "roles_threshold", label: "Roles similarity threshold (0–1)", kind: "number"},
    {key: "roles_max_resources", label: "Roles resource cap", kind: "integer"},
    {key: "roles_max_activities", label: "Roles activity cap", kind: "integer"},
  ],
  profiles: [
    {key: "resource_profiles_metric", label: "Resource profile metric", kind: "enum", choices: RESOURCE_PROFILE_METRICS},
    {key: "resource_profiles_activity_attribute", label: "Profile activity attribute", kind: "text"},
    {key: "resource_profiles_resource_attribute", label: "Profile resource attribute", kind: "text"},
    {key: "resource_profiles_timestamp_attribute", label: "Profile completion timestamp attribute", kind: "text"},
    {key: "resource_profiles_start_timestamp_attribute", label: "Profile start timestamp attribute", kind: "text"},
    {key: "resource_profiles_case_id_attribute", label: "Profile case attribute", kind: "text"},
    {key: "resource_profiles_t1", label: "Profile time bound t1 (inclusive)", kind: "text"},
    {key: "resource_profiles_t2", label: "Profile time bound t2 (exclusive)", kind: "text"},
    {key: "resource_profiles_resource", label: "Profile resource (blank selects all)", kind: "text"},
    {key: "resource_profiles_activity", label: "Profile activity (blank selects all where supported)", kind: "text"},
    {key: "resource_profiles_resource2", label: "Profile second resource", kind: "text"},
    {key: "resource_profiles_max_resources", label: "Profile resource cap", kind: "integer"},
    {key: "resource_profiles_max_rows", label: "Profile row cap", kind: "integer"},
  ],
  diagnostics: [
    {key: "local_diagnostics_group_attribute", label: "Diagnostics group attribute", kind: "text"},
    {key: "local_diagnostics_resource_attribute", label: "Diagnostics resource attribute", kind: "text"},
    {key: "local_diagnostics_activity_attribute", label: "Diagnostics activity attribute", kind: "text"},
    {key: "local_diagnostics_groups_json", label: "Explicit diagnostics groups (JSON object of resource arrays)", kind: "json"},
    {key: "local_diagnostics_max_groups", label: "Diagnostics group cap", kind: "integer"},
    {key: "local_diagnostics_max_resources", label: "Diagnostics resource cap", kind: "integer"},
    {key: "local_diagnostics_max_rows", label: "Diagnostics scalar row cap", kind: "integer"},
  ],
  network: [
    {key: "network_analysis_variant", label: "Attribute network variant", kind: "enum", choices: ["dataframe"]},
    {key: "network_analysis_sorting_attribute", label: "Network sorting attribute", kind: "text"},
    {key: "network_analysis_timestamp_attribute", label: "Network performance timestamp attribute", kind: "text"},
    {key: "network_analysis_source_node_attribute", label: "Network source node attribute", kind: "text"},
    {key: "network_analysis_target_node_attribute", label: "Network target node attribute", kind: "text"},
    {key: "network_analysis_edge_attribute", label: "Network edge attribute", kind: "text"},
    {key: "network_analysis_edge_reference", label: "Network edge reference", kind: "enum", choices: ["_out", "_in"]},
    {key: "network_analysis_out_relation_attribute", label: "Network outgoing relation attribute", kind: "text"},
    {key: "network_analysis_in_relation_attribute", label: "Network incoming relation attribute", kind: "text"},
    {key: "network_analysis_include_performance", label: "Network performance values", kind: "boolean"},
    {key: "network_analysis_business_hours", label: "Network business hours", kind: "boolean"},
    {key: "network_analysis_business_hour_slots", label: "Network weekly business-hour slots (comma-separated start:end seconds)", kind: "text"},
    {key: "network_analysis_max_rows", label: "Network edge row cap", kind: "integer"},
    {key: "network_analysis_max_values", label: "Network performance value cap", kind: "integer"},
  ],
};
export interface OrganizationalWorkbenchForm { fields: Record<string, string>; roles_source_id?: string; }
export const defaultOrganizationalWorkbenchForm = (): OrganizationalWorkbenchForm => ({fields: {}});
export interface OrganizationalSource { source_id: string; dataset: string; case_count: number; event_count: number; }
type NativeOrganizationalResult = RoleDiscoveryResult | ResourceProfilesResult | LocalDiagnosticsResult | NetworkAnalysisResult;
export type OrganizationalWorkbenchResult = NativeOrganizationalResult & {input_context: OrganizationalSource};
export interface OrganizationalWorkbenchRun {
  family: OrganizationalFamily; source: OrganizationalSource; value: OrganizationalWorkbenchResult; raw: string;
  fields: Array<[string, string]>; tables: DiagnosticTable[];
}
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
const unsigned = (holder: object, key: string | number): bigint | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const value = nativeCell(holder, key);
  if (typeof value !== "number" && typeof value !== "string") return undefined;
  const text = String(value);
  if (!/^(?:0|[1-9]\d*)$/.test(text)) return undefined;
  const integer = BigInt(text);
  return integer <= 18446744073709551615n ? integer : undefined;
};
const numeric = (holder: object, key: string | number): number | undefined => {
  if (typeof (holder as Record<string, unknown>)[String(key)] !== "number") return undefined;
  const value = nativeCell(holder, key);
  if (typeof value !== "number" && typeof value !== "string") return undefined;
  if (typeof value === "string" && !/^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?$/.test(value)) return undefined;
  const number = Number(value);
  return Number.isFinite(number) ? number : undefined;
};
const strings = (value: unknown): value is string[] => Array.isArray(value) && value.every(item => typeof item === "string");
const equalCount = (holder: object, key: string, count: number | bigint) => unsigned(holder, key) === BigInt(count);
export class OrganizationalSourceChangedError extends Error {}
function diagnosticsShape(value: unknown, counts: readonly string[]): value is Record<string, unknown> {
  return object(value) && typeof value.computed === "boolean" && typeof value.output_truncated === "boolean" && typeof value.message === "string" && counts.every(key => unsigned(value, key) !== undefined);
}
/** Resolve a cached identity against the visible import; the native response also verifies the dataset. */
export function organizationalSourceFromSchema(schema: LogQuerySchema, data: MineraExport): OrganizationalSource {
  const metadata = data as MineraExport & {source_id?: string};
  const schemaDataset = (schema as LogQuerySchema & {dataset?: string}).dataset;
  if (!schema.source_id || schema.case_count !== data.trace_count || schema.event_count !== data.event_count ||
    (metadata.source_id !== undefined && metadata.source_id !== schema.source_id) || (schemaDataset !== undefined && schemaDataset !== data.dataset)) {
    throw new OrganizationalSourceChangedError("The cached log does not match the visible dataset; mine this log again.");
  }
  return {source_id: schema.source_id, dataset: data.dataset, case_count: data.trace_count, event_count: data.event_count};
}
export function sameOrganizationalSource(left: OrganizationalSource, right: OrganizationalSource): boolean {
  return left.source_id === right.source_id && left.dataset === right.dataset && left.case_count === right.case_count && left.event_count === right.event_count;
}
export function parseOrganizationalGroups(text: string): Record<string, string[]> {
  let parsed: unknown;
  try { parsed = parseNativeJson(text); } catch { throw new Error("Explicit diagnostics groups must be valid JSON."); }
  if (!object(parsed) || !Object.values(parsed).every(strings)) {
    throw new Error("Explicit diagnostics groups must be a JSON object mapping group names to arrays of resource labels.");
  }
  return parsed as Record<string, string[]>;
}
/** Blank controls are omitted; submitted numeric, selector and JSON text is never rewritten. */
export function organizationalWorkbenchFields(family: OrganizationalFamily, form: OrganizationalWorkbenchForm, source: OrganizationalSource): Array<[string, string]> {
  if (!source.source_id) throw new Error("Resolve the current source before running organizational analysis.");
  if (form.roles_source_id !== undefined && form.roles_source_id !== source.source_id) throw new Error("These role groups belong to a different source. Discover roles again or remove the role-source binding.");
  if (form.fields.local_diagnostics_groups !== undefined && form.fields.local_diagnostics_groups !== "") {
    throw new Error("Use local_diagnostics_groups_json; legacy delimiter groups cannot be combined with this workbench.");
  }
  const fields: Array<[string, string]> = [["source_id", source.source_id], ["include_source", "true"]];
  for (const control of ORGANIZATIONAL_CONTROLS[family]) {
    const value = form.fields[control.key];
    if (value === undefined || value === "") continue;
    if (control.kind === "enum" && !control.choices?.includes(value)) throw new Error(`${control.label}: select a supported value.`);
    if (control.kind === "boolean" && !["true", "false"].includes(value)) throw new Error(`${control.label} must be true or false.`);
    if (control.kind === "integer" && !/^\d+$/.test(value.trim())) throw new Error(`${control.label} must be an unsigned integer.`);
    if (control.kind === "number" && (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(value.trim()) || !Number.isFinite(Number(value)))) throw new Error(`${control.label} must be a finite number.`);
    if (control.kind === "json") parseOrganizationalGroups(value);
    fields.push([control.key, value]);
  }
  return fields;
}
/** Check the actual native containers and exact counts, allowing incomplete output when native limits apply. */
export function isOrganizationalWorkbenchResult(value: unknown, family?: OrganizationalFamily): value is OrganizationalWorkbenchResult {
  if (!object(value) || !object(value.input_context)) return false;
  const input = value.input_context;
  if (typeof input.source_id !== "string" || !input.source_id || typeof input.dataset !== "string" || ["case_count", "event_count"].some(key => unsigned(input, key) === undefined)) return false;
  const expected = {roles: "ROLE_DISCOVERY", profiles: "RESOURCE_PROFILES", diagnostics: "LOCAL_DIAGNOSTICS", network: "NETWORK_ANALYSIS"};
  if (family !== undefined && value.family !== expected[family]) return false;
  const d = value.diagnostics;
  if (!diagnosticsShape(d, ["trace_count", "event_count"])) return false;
  if (d.computed ? !equalCount(d, "trace_count", unsigned(input, "case_count")!) || !equalCount(d, "event_count", unsigned(input, "event_count")!) : unsigned(d, "trace_count")! > unsigned(input, "case_count")! || unsigned(d, "event_count")! > unsigned(input, "event_count")!) return false;
  if (value.family === "ROLE_DISCOVERY") {
    if (!["log", "pandas"].includes(String(value.variant)) || !Array.isArray(value.roles) || !diagnosticsShape(d, ["activity_count", "resource_count", "role_count"])) return false;
    const activities = new Set<string>(), resources = new Set<string>();
    for (const role of value.roles) {
      if (!object(role) || !strings(role.activities) || !Array.isArray(role.resources) || !role.activities.length || !role.resources.length) return false;
      for (const activity of role.activities) {if (!activity || activities.has(activity)) return false; activities.add(activity);}
      for (const resource of role.resources) {
        if (!object(resource) || typeof resource.name !== "string" || !resource.name || unsigned(resource, "count") === undefined || unsigned(resource, "count") === 0n) return false;
        resources.add(resource.name);
      }
    }
    return equalCount(d, "role_count", value.roles.length) && equalCount(d, "activity_count", activities.size) && equalCount(d, "resource_count", resources.size);
  }
  if (value.family === "RESOURCE_PROFILES") {
    if (!RESOURCE_PROFILE_METRICS.includes(value.metric as typeof RESOURCE_PROFILE_METRICS[number]) || !Array.isArray(value.rows) || !diagnosticsShape(d, ["resource_count"])) return false;
    const resources = new Set<string>(), keys = new Set<string>();
    for (const row of value.rows) {
      if (!object(row) || typeof row.resource !== "string" || !row.resource || typeof row.activity !== "string" || typeof row.resource2 !== "string" || numeric(row, "value") === undefined || unsigned(row, "count") === undefined) return false;
      const key = JSON.stringify([row.resource, row.activity, row.resource2]); if (keys.has(key)) return false; keys.add(key);
      const queriedZero = ["social_position", "interaction_two_resources"].includes(String(value.metric)) && unsigned(row, "count") === 0n && numeric(row, "value") === 0;
      if (!queriedZero) {resources.add(row.resource); if (row.resource2) resources.add(row.resource2);}
    }
    // Queried, unobserved resources may legitimately yield zero-valued rows.
    return unsigned(d, "resource_count")! >= BigInt(resources.size);
  }
  if (value.family === "LOCAL_DIAGNOSTICS") {
    if (!diagnosticsShape(d, ["group_count", "resource_count", "row_count", "skipped_events"]) || unsigned(d, "skipped_events")! > unsigned(d, "event_count")!) return false;
    const groups = new Set<string>(), resources = new Set<string>(); let leaves = 0;
    for (const field of ["group_relative_focus", "group_relative_stake", "group_coverage"]) {
      const map = value[field]; if (!object(map)) return false;
      for (const [group, metrics] of Object.entries(map)) {
        groups.add(group); if (!object(metrics)) return false;
        for (const key of Object.keys(metrics)) {
          const ratio = numeric(metrics, key); if (ratio === undefined || ratio < 0 || ratio > 1) return false;
          ++leaves; if (field === "group_coverage") resources.add(key);
        }
      }
    }
    if (!object(value.group_member_contribution)) return false;
    for (const [group, members] of Object.entries(value.group_member_contribution)) {
      groups.add(group); if (!object(members)) return false;
      for (const [resource, activities] of Object.entries(members)) {
        resources.add(resource); if (!object(activities)) return false;
        for (const key of Object.keys(activities)) {if (unsigned(activities, key) === undefined) return false; ++leaves;}
      }
    }
    return unsigned(d, "group_count")! >= BigInt(groups.size) && unsigned(d, "resource_count")! >= BigInt(resources.size) && equalCount(d, "row_count", leaves);
  }
  if (value.family === "NETWORK_ANALYSIS") {
    if (value.variant !== "dataframe" || !Array.isArray(value.edges) || !diagnosticsShape(d, ["link_count", "row_count", "performance_value_count"])) return false;
    let links = 0n, performance = 0;
    for (const edge of value.edges) {
      if (!object(edge) || !["source", "target", "activity"].every(key => typeof edge[key] === "string" && edge[key] !== "") || unsigned(edge, "count") === undefined || unsigned(edge, "count") === 0n || !Array.isArray(edge.performance) || !edge.performance.every((_, i) => numeric(edge.performance as unknown[], i) !== undefined)) return false;
      links += unsigned(edge, "count")!; performance += edge.performance.length;
    }
    return equalCount(d, "row_count", value.edges.length) && (d.output_truncated ? unsigned(d, "link_count")! >= links && unsigned(d, "performance_value_count")! >= BigInt(performance) : equalCount(d, "link_count", links) && equalCount(d, "performance_value_count", performance));
  }
  return false;
}

export function organizationalWorkbenchTables(value: OrganizationalWorkbenchResult, fields?: Array<[string, string]>): DiagnosticTable[] {
  const tables = nativeObjectTables(value);
  if (value.family === "ROLE_DISCOVERY") {
    const resourceRows: unknown[][] = [], resourceRecords: unknown[] = [], activityRows: unknown[][] = [], activityRecords: unknown[] = [];
    value.roles.forEach((role, role_index) => {
      role.resources.forEach((resource, resource_index) => {resourceRows.push([role_index, resource_index, resource.name, nativeCell(resource, "count"), role.activities, resource]); resourceRecords.push({role_index, resource_index, resource, activities: role.activities});});
      role.activities.forEach((activity, activity_index) => {activityRows.push([role_index, activity_index, activity, role.resources]); activityRecords.push({role_index, activity_index, activity, resources: role.resources});});
    });
    tables.push({id: "role_resources", title: "Every role resource membership", columns: ["role_index", "resource_index", "resource", "count", "activities", "native_resource"], rows: resourceRows, records: resourceRecords});
    tables.push({id: "role_activities", title: "Every role activity membership", columns: ["role_index", "activity_index", "activity", "resources"], rows: activityRows, records: activityRecords});
  } else if (value.family === "LOCAL_DIAGNOSTICS") {
    const rows: unknown[][] = [], records: unknown[] = [], contributions: unknown[][] = [], contributionRecords: unknown[] = [];
    for (const metric of ["group_relative_focus", "group_relative_stake", "group_coverage"] as const) for (const [group, metrics] of Object.entries(value[metric])) for (const key of Object.keys(metrics)) {
      const cell = nativeCell(metrics, key); rows.push([metric, group, key, cell]);
      records.push(parseNativeJson(`{"metric":${JSON.stringify(metric)},"group":${JSON.stringify(group)},"attribute":${JSON.stringify(key)},"value":${typeof cell === "string" ? cell : nativeJson(cell)}}`));
    }
    for (const [group, members] of Object.entries(value.group_member_contribution)) for (const [resource, activities] of Object.entries(members)) for (const activity of Object.keys(activities)) {
      const count = nativeCell(activities, activity); contributions.push([group, resource, activity, count]);
      contributionRecords.push(parseNativeJson(`{"group":${JSON.stringify(group)},"resource":${JSON.stringify(resource)},"activity":${JSON.stringify(activity)},"count":${typeof count === "string" ? count : nativeJson(count)}}`));
    }
    tables.push({id: "group_metrics", title: "Every local group metric", columns: ["metric", "group", "activity_or_resource", "value"], rows, records});
    tables.push({id: "group_contributions", title: "Every group member activity contribution", columns: ["group", "resource", "activity", "count"], rows: contributions, records: contributionRecords});
  } else if (value.family === "NETWORK_ANALYSIS") {
    const rows: unknown[][] = [], records: unknown[] = [];
    value.edges.forEach((edge, edge_index) => edge.performance.forEach((_, performance_index) => {
      const seconds = nativeCell(edge.performance, performance_index); rows.push([edge_index, edge.source, edge.target, edge.activity, performance_index, seconds]);
      records.push(parseNativeJson(`{"edge_index":${edge_index},"source":${JSON.stringify(edge.source)},"target":${JSON.stringify(edge.target)},"activity":${JSON.stringify(edge.activity)},"performance_index":${performance_index},"seconds":${typeof seconds === "string" ? seconds : nativeJson(seconds)}}`));
    }));
    tables.push({id: "network_performance", title: "Every network performance value (seconds)", columns: ["edge_index", "source", "target", "activity", "performance_index", "seconds"], rows, records});
  }
  const submitted = fields ?? nativeResponseRequest(value)?.fields;
  if (submitted) tables.push({id: "request", title: "Submitted organizational options", columns: ["field", "value"], rows: submitted, records: submitted.map(([field, item]) => ({field, value: item}))});
  return tables;
}
export async function runOrganizationalWorkbench(family: OrganizationalFamily, source: OrganizationalSource, form: OrganizationalWorkbenchForm, signal?: AbortSignal): Promise<OrganizationalWorkbenchRun> {
  const snapshot = {...source}, fields = organizationalWorkbenchFields(family, form, snapshot);
  signal?.throwIfAborted();
  const path = ORGANIZATIONAL_PATHS[family] + "?" + new URLSearchParams(fields);
  const response = await fetch(path, {method: "POST", signal});
  const value = await readNativeResponse(response, {path, fields});
  signal?.throwIfAborted();
  if (!response.ok) throw new Error(object(value) && typeof value.error === "string" ? value.error : `${ORGANIZATIONAL_TITLES[family]} failed (${response.status}).`);
  if (!isOrganizationalWorkbenchResult(value, family)) throw new Error(`The server did not return a valid native ${ORGANIZATIONAL_TITLES[family].toLowerCase()} result.`);
  if (!sameOrganizationalSource(snapshot, value.input_context)) throw new OrganizationalSourceChangedError("The organizational response does not match the current source dataset and counts.");
  const submitted = Object.fromEntries(fields);
  if ((value.family === "ROLE_DISCOVERY" && value.variant !== (submitted.roles_variant ?? "log")) || (value.family === "RESOURCE_PROFILES" && value.metric !== (submitted.resource_profiles_metric ?? "distinct_activities")) || (value.family === "NETWORK_ANALYSIS" && value.variant !== (submitted.network_analysis_variant ?? "dataframe"))) throw new Error("The organizational response does not match the submitted analysis options.");
  return {family, source: snapshot, fields, value, raw: nativeResponseJson(value), tables: organizationalWorkbenchTables(value, fields)};
}
/** Each issue invalidates an earlier request; cancellation and host changes cannot commit late results. */
export class OrganizationalRequestGate {
  private generation = 0;
  issue(): number {return ++this.generation;}
  invalidate(): void {++this.generation;}
  accepts(ticket: number, signal?: AbortSignal): boolean {return ticket === this.generation && !signal?.aborted;}
}
export function canHandoffOrganizationalRoles(run: OrganizationalWorkbenchRun | null, source: OrganizationalSource | null): boolean {
  return !!run && !!source && run.value.family === "ROLE_DISCOVERY" && run.value.diagnostics.computed && run.value.roles.length > 0 && run.value.roles.some(role => role.resources.length > 0) && sameOrganizationalSource(run.source, source);
}
export function organizationalRoleGroups(run: OrganizationalWorkbenchRun, source: OrganizationalSource): OrganizationalWorkbenchForm {
  if (!canHandoffOrganizationalRoles(run, source) || run.value.family !== "ROLE_DISCOVERY") throw new Error("Complete nonempty role discovery on this source before generating diagnostics groups.");
  const groups: Record<string, string[]> = Object.create(null);
  run.value.roles.forEach((role, index) => {groups[`Role ${index + 1}`] = role.resources.map(resource => resource.name);});
  const submitted = Object.fromEntries(run.fields);
  return {roles_source_id: run.source.source_id, fields: {
    local_diagnostics_groups_json: JSON.stringify(groups, null, 2),
    local_diagnostics_activity_attribute: submitted.roles_activity_attribute ?? "concept:name",
    local_diagnostics_resource_attribute: submitted.roles_resource_attribute ?? "org:resource",
  }};
}
export function organizationalWorkbenchReport(run: OrganizationalWorkbenchRun): string {
  return nativeJson({family: "ORGANIZATIONAL_WORKBENCH_REPORT", analysis: run.family, source: run.source,
    request: nativeResponseRequest(run.value) ?? {path: ORGANIZATIONAL_PATHS[run.family], fields: run.fields}, native_result: run.value}, 2);
}
export function organizationalAllTablesCsv(run: OrganizationalWorkbenchRun): string {
  const rows: unknown[][] = [];
  for (const table of run.tables) {
    if (!table.rows.length) table.columns.forEach((column, column_index) => rows.push([table.id, table.title, undefined, column_index, column, undefined]));
    table.rows.forEach((row, row_index) => table.columns.forEach((column, column_index) => rows.push([table.id, table.title, row_index, column_index, column, row[column_index]])));
  }
  return diagnosticCsv({id: "all_tables", title: "All complete organizational tables", columns: ["table_id", "table_title", "row_index", "column_index", "column", "value"], rows});
}
export function organizationalResultSummary(run: OrganizationalWorkbenchRun): string {
  const d = run.value.diagnostics;
  return `${d.computed ? "Computed" : "Not computed"}${d.output_truncated ? " · Native output limit applied" : ""} · ${nativeCell(d, "trace_count")} cases · ${nativeCell(d, "event_count")} events · ${d.message || "No native message"}`;
}
