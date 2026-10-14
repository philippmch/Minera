import { afterEach, describe, expect, it, vi } from "vitest";
import {
  ORGANIZATIONAL_CONTROLS, ORGANIZATIONAL_FAMILIES, ORGANIZATIONAL_PATHS, RESOURCE_PROFILE_METRICS,
  OrganizationalRequestGate, canHandoffOrganizationalRoles, defaultOrganizationalWorkbenchForm,
  isOrganizationalWorkbenchResult, organizationalAllTablesCsv, organizationalResultSummary,
  organizationalRoleGroups, organizationalSourceFromSchema, organizationalWorkbenchFields,
  organizationalWorkbenchReport, organizationalWorkbenchTables, parseOrganizationalGroups,
  runOrganizationalWorkbench, sameOrganizationalSource,
  type OrganizationalFamily, type OrganizationalSource, type OrganizationalWorkbenchRun,
} from "../src/organizational-workbench";
import { diagnosticCsv, diagnosticPage } from "../src/model-diagnostics";
import { nativeCell, nativeJson, parseNativeJson } from "../src/native-json";
import type { MineraExport } from "../src/types";
import type { LogQuerySchema } from "../src/log-query";

const source: OrganizationalSource = {source_id: "cached:42", dataset: "orders.xes", case_count: 2, event_count: 4};
const diagnosticBase = () => ({computed: true, output_truncated: false, trace_count: 2, event_count: 4, message: "ok"});
function payload(family: OrganizationalFamily): Record<string, unknown> {
  const common = {input_context: {...source}};
  if (family === "roles") return {...common, family: "ROLE_DISCOVERY", variant: "log", roles: [{activities: ["A", "B"], resources: [{name: "alice", count: 3}, {name: "bob", count: 1}]}], diagnostics: {...diagnosticBase(), activity_count: 2, resource_count: 2, role_count: 1}};
  if (family === "profiles") return {...common, family: "RESOURCE_PROFILES", metric: "distinct_activities", rows: [{resource: "alice", activity: "", resource2: "", value: 2, count: 2}], diagnostics: {...diagnosticBase(), resource_count: 2}};
  if (family === "diagnostics") return {...common, family: "LOCAL_DIAGNOSTICS", group_relative_focus: {Team: {A: 1}}, group_relative_stake: {Team: {A: 1}}, group_coverage: {Team: {alice: 1}}, group_member_contribution: {Team: {alice: {A: 3}}}, diagnostics: {...diagnosticBase(), group_count: 1, resource_count: 1, row_count: 4, skipped_events: 1}};
  return {...common, family: "NETWORK_ANALYSIS", variant: "dataframe", edges: [{source: "alice", target: "bob", activity: "A", count: 1, performance: [0.1, -2]}], diagnostics: {...diagnosticBase(), link_count: 1, row_count: 1, performance_value_count: 2}};
}
function asRun(family: OrganizationalFamily, value: unknown = payload(family)): OrganizationalWorkbenchRun {
  if (!isOrganizationalWorkbenchResult(value, family)) throw new Error("Invalid test fixture");
  const fields = organizationalWorkbenchFields(family, defaultOrganizationalWorkbenchForm(), source);
  return {family, source: {...source}, value, raw: nativeJson(value), fields, tables: organizationalWorkbenchTables(value, fields)};
}
afterEach(() => vi.unstubAllGlobals());

describe("complete organizational controls and exact submission", () => {
  it("exposes every own native option in all four families and all eleven profile metrics", () => {
    expect(ORGANIZATIONAL_FAMILIES).toEqual(["roles", "profiles", "diagnostics", "network"]);
    expect(ORGANIZATIONAL_CONTROLS.roles.map(control => control.key)).toEqual(["roles_variant", "roles_activity_attribute", "roles_resource_attribute", "roles_threshold", "roles_max_resources", "roles_max_activities"]);
    expect(ORGANIZATIONAL_CONTROLS.profiles.map(control => control.key)).toEqual(["resource_profiles_metric", "resource_profiles_activity_attribute", "resource_profiles_resource_attribute", "resource_profiles_timestamp_attribute", "resource_profiles_start_timestamp_attribute", "resource_profiles_case_id_attribute", "resource_profiles_t1", "resource_profiles_t2", "resource_profiles_resource", "resource_profiles_activity", "resource_profiles_resource2", "resource_profiles_max_resources", "resource_profiles_max_rows"]);
    expect(RESOURCE_PROFILE_METRICS).toHaveLength(11);
    expect(ORGANIZATIONAL_CONTROLS.diagnostics.map(control => control.key)).toEqual(["local_diagnostics_group_attribute", "local_diagnostics_resource_attribute", "local_diagnostics_activity_attribute", "local_diagnostics_groups_json", "local_diagnostics_max_groups", "local_diagnostics_max_resources", "local_diagnostics_max_rows"]);
    expect(ORGANIZATIONAL_CONTROLS.network.map(control => control.key)).toEqual(["network_analysis_variant", "network_analysis_sorting_attribute", "network_analysis_timestamp_attribute", "network_analysis_source_node_attribute", "network_analysis_target_node_attribute", "network_analysis_edge_attribute", "network_analysis_edge_reference", "network_analysis_out_relation_attribute", "network_analysis_in_relation_attribute", "network_analysis_include_performance", "network_analysis_business_hours", "network_analysis_business_hour_slots", "network_analysis_max_rows", "network_analysis_max_values"]);
  });
  it("preserves exact numeric, selector, JSON and slot text and omits only truly blank controls", () => {
    for (const family of ORGANIZATIONAL_FAMILIES) {
      const form = defaultOrganizationalWorkbenchForm();
      for (const control of ORGANIZATIONAL_CONTROLS[family]) form.fields[control.key] = control.kind === "enum" ? control.choices![0] : control.kind === "boolean" ? "false" : control.kind === "integer" ? " 18446744073709551615 " : control.kind === "number" ? " 0.650000000000000000001 " : control.kind === "json" ? ' {"__proto__":[],"Team:東京,blue;": ["  a:b,c;\u00e4  "]} ' : "  attribute:東京,blue;  ";
      const fields = Object.fromEntries(organizationalWorkbenchFields(family, form, source));
      expect(fields.source_id).toBe(source.source_id); expect(fields.include_source).toBe("true");
      for (const [key, value] of Object.entries(form.fields)) expect(fields[key]).toBe(value);
      expect(Object.fromEntries(organizationalWorkbenchFields(family, {fields: Object.fromEntries(ORGANIZATIONAL_CONTROLS[family].map(control => [control.key, ""]))}, source))).toEqual({source_id: source.source_id, include_source: "true"});
    }
    expect(Object.fromEntries(organizationalWorkbenchFields("diagnostics", {fields: {local_diagnostics_groups_json: "{}"}}, source)).local_diagnostics_groups_json).toBe("{}");
    const slots = "28800:43200, 46800-61200";
    expect(Object.fromEntries(organizationalWorkbenchFields("network", {fields: {network_analysis_business_hour_slots: slots}}, source)).network_analysis_business_hour_slots).toBe(slots);
  });
  it("rejects invalid JSON shape, legacy group fields and unsupported enum/boolean controls without mutation", () => {
    for (const text of ["[]", '{"team":1}', '{"team":[1]}', "{"]) expect(() => parseOrganizationalGroups(text)).toThrow();
    expect(() => organizationalWorkbenchFields("diagnostics", {fields: {local_diagnostics_groups_json: "{}", local_diagnostics_groups: "team:a"}}, source)).toThrow(/legacy/i);
    expect(() => organizationalWorkbenchFields("roles", {fields: {roles_variant: "dataframe"}}, source)).toThrow();
    expect(() => organizationalWorkbenchFields("roles", {fields: {roles_threshold: "Infinity"}}, source)).toThrow();
    expect(() => organizationalWorkbenchFields("network", {fields: {network_analysis_include_performance: "yes"}}, source)).toThrow();
    expect(() => organizationalWorkbenchFields("network", {fields: {network_analysis_max_rows: "1.5"}}, source)).toThrow();
    expect(() => organizationalWorkbenchFields("diagnostics", {fields: {}, roles_source_id: "old"}, source)).toThrow(/different source/);
  });
  it("leaves group-name and resource-member semantics to native code and preserves NBSP and empty members", () => {
    const raw = ' {"\u00a0": ["", "  ", "\u00a0", "東京"]} ';
    const groups = parseOrganizationalGroups(raw);
    expect(Object.keys(groups)).toEqual(["\u00a0"]);
    expect(groups["\u00a0"]).toEqual(["", "  ", "\u00a0", "東京"]);
    expect(Object.fromEntries(organizationalWorkbenchFields("diagnostics", {fields: {local_diagnostics_groups_json: raw}}, source)).local_diagnostics_groups_json).toBe(raw);
    expect(parseOrganizationalGroups('{"":[]}')).toEqual({"": []});
  });
});

describe("organizational source binding, cancellation and handoff", () => {
  const data = {dataset: source.dataset, trace_count: 2, event_count: 4, activities: [], edges: [], variants: []} as MineraExport;
  const schema: LogQuerySchema = {family: "LOG_QUERY_SCHEMA", source_id: source.source_id, case_count: 2, event_count: 4, table_name: "dataframe", event_index_column: "index", sqlite_available: true, duckdb_configured: false, columns: []};
  it("binds cached source identity/counts and optional schema dataset to the visible dataset", () => {
    expect(organizationalSourceFromSchema(schema, data)).toEqual(source);
    expect(() => organizationalSourceFromSchema({...schema, case_count: 9}, data)).toThrow(/visible dataset/);
    expect(() => organizationalSourceFromSchema(schema, {...data, source_id: "other"} as MineraExport)).toThrow();
    expect(() => organizationalSourceFromSchema({...schema, dataset: "other"} as LogQuerySchema, data)).toThrow();
    expect(sameOrganizationalSource(source, {...source, dataset: "other"})).toBe(false);
  });
  it("invalidates superseded requests, cancelled signals and host-source changes independently", () => {
    const roles = new OrganizationalRequestGate(), profiles = new OrganizationalRequestGate(), controller = new AbortController();
    const old = roles.issue(), active = roles.issue(), independent = profiles.issue();
    expect(roles.accepts(old)).toBe(false); expect(roles.accepts(active, controller.signal)).toBe(true);
    controller.abort(); expect(roles.accepts(active, controller.signal)).toBe(false);
    roles.invalidate(); expect(roles.accepts(active)).toBe(false); expect(profiles.accepts(independent)).toBe(true);
  });
  it("hands off every exact resource label using editable JSON and completed role selectors", () => {
    const value = payload("roles");
    const labels = ["a,b", "a:b", "a;b", "  東京 ä  ", 'quote"back\\slash', "line\nfeed", "__proto__", "constructor", "toString"];
    value.roles = [{activities: ["活動"], resources: labels.map(name => ({name, count: 1}))}, {activities: ["other"], resources: [{name: "a,b", count: 2}]}];
    value.diagnostics = {...diagnosticBase(), role_count: 2, activity_count: 2, resource_count: labels.length};
    const run = asRun("roles", value); run.fields.push(["roles_activity_attribute", "team:activity"], ["roles_resource_attribute", "team:resource"]);
    const original = nativeJson(run.value), groups = organizationalRoleGroups(run, source), decoded = parseOrganizationalGroups(groups.fields.local_diagnostics_groups_json);
    expect(Object.keys(decoded)).toEqual(["Role 1", "Role 2"]); expect(decoded["Role 1"]).toEqual(labels); expect(decoded["Role 2"]).toEqual(["a,b"]);
    expect(groups.fields.local_diagnostics_activity_attribute).toBe("team:activity"); expect(groups.fields.local_diagnostics_resource_attribute).toBe("team:resource"); expect(groups.roles_source_id).toBe(source.source_id);
    expect(nativeJson(run.value)).toBe(original); expect(canHandoffOrganizationalRoles(run, {...source, source_id: "other"})).toBe(false);
    expect(() => organizationalRoleGroups(run, {...source, source_id: "other"})).toThrow();
    expect(Object.hasOwn(parseOrganizationalGroups('{"__proto__":["constructor"],"constructor":[],"toString":["__proto__"]}'), "__proto__")).toBe(true);
    run.value.diagnostics.computed = false; expect(canHandoffOrganizationalRoles(run, source)).toBe(false);
    expect(canHandoffOrganizationalRoles(null, source)).toBe(false);
  });
  it("submits all four existing POST endpoints with source identity and captures exact independent run snapshots", async () => {
    for (const family of ORGANIZATIONAL_FAMILIES) {
      const form = defaultOrganizationalWorkbenchForm(), controller = new AbortController();
      const raw = JSON.stringify(payload(family)); const fetcher = vi.fn().mockResolvedValue(new Response(raw)); vi.stubGlobal("fetch", fetcher);
      const run = await runOrganizationalWorkbench(family, source, form, controller.signal);
      const [url, request] = fetcher.mock.calls[0]; expect(String(url).split("?")[0]).toBe(ORGANIZATIONAL_PATHS[family]);
      const params = new URLSearchParams(String(url).split("?")[1]); expect(params.get("source_id")).toBe(source.source_id); expect(params.get("include_source")).toBe("true");
      expect(request.method).toBe("POST"); expect(request.signal).toBe(controller.signal); expect(run.raw).toBe(raw);
      form.fields.roles_threshold = "0.1"; expect(run.fields.some(([key]) => key === "roles_threshold")).toBe(false);
      expect(organizationalWorkbenchReport(run)).toContain('"native_result"');
    }
  });
  it("rejects a wrong-source response, wrong variant and invalid native payload", async () => {
    const wrong = payload("roles"); wrong.input_context = {...source, dataset: "other"};
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify(wrong))));
    await expect(runOrganizationalWorkbench("roles", source, defaultOrganizationalWorkbenchForm())).rejects.toThrow(/source dataset/);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify(payload("roles")))));
    await expect(runOrganizationalWorkbench("roles", source, {fields: {roles_variant: "pandas"}})).rejects.toThrow(/options/);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response('{"family":"ROLE_DISCOVERY"}')));
    await expect(runOrganizationalWorkbench("roles", source, defaultOrganizationalWorkbenchForm())).rejects.toThrow(/valid native/);
  });
  it("does not publish a response whose transport ignored cancellation", async () => {
    const controller = new AbortController();
    vi.stubGlobal("fetch", vi.fn().mockImplementation(async () => {controller.abort(); return new Response(JSON.stringify(payload("roles")));}));
    await expect(runOrganizationalWorkbench("roles", source, defaultOrganizationalWorkbenchForm(), controller.signal)).rejects.toThrow();
    const early = new AbortController(); early.abort(); const fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher);
    await expect(runOrganizationalWorkbench("roles", source, defaultOrganizationalWorkbenchForm(), early.signal)).rejects.toThrow(); expect(fetcher).not.toHaveBeenCalled();
  });
});

describe("native organizational shape, incomplete results and complete exports", () => {
  it("validates all actual native shapes and accepts uncomputed empty native outcomes", () => {
    for (const family of ORGANIZATIONAL_FAMILIES) {
      const value = payload(family); expect(isOrganizationalWorkbenchResult(value, family)).toBe(true);
      const diagnostics = value.diagnostics as Record<string, unknown>;
      diagnostics.computed = false; for (const key of Object.keys(diagnostics)) if (typeof diagnostics[key] === "number") diagnostics[key] = 0;
      if (family === "roles") value.roles = []; else if (family === "profiles") value.rows = []; else if (family === "network") value.edges = []; else for (const key of ["group_relative_focus", "group_relative_stake", "group_coverage", "group_member_contribution"]) value[key] = {};
      expect(isOrganizationalWorkbenchResult(value, family)).toBe(true); expect(organizationalResultSummary(asRun(family, value))).toContain("Not computed");
      diagnostics.computed = "false"; expect(isOrganizationalWorkbenchResult(value, family)).toBe(false);
    }
    expect(isOrganizationalWorkbenchResult(null)).toBe(false); expect(isOrganizationalWorkbenchResult(payload("roles"), "network")).toBe(false);
  });
  it("accepts truncated aggregate totals beyond displayed rows and empty explicit groups", () => {
    const network = payload("network"), d = network.diagnostics as Record<string, unknown>;
    d.output_truncated = true; d.link_count = 99; d.performance_value_count = 72;
    expect(isOrganizationalWorkbenchResult(network, "network")).toBe(true);
    d.output_truncated = false; expect(isOrganizationalWorkbenchResult(network, "network")).toBe(false);
    const local = payload("diagnostics"), localD = local.diagnostics as Record<string, unknown>; localD.group_count = 22;
    expect(isOrganizationalWorkbenchResult(local, "diagnostics")).toBe(true); localD.row_count = 5; expect(isOrganizationalWorkbenchResult(local, "diagnostics")).toBe(false);
  });
  it("requires exact role counts even when a truncation flag is present", () => {
    for (const key of ["activity_count", "resource_count", "role_count"]) {
      const roles = payload("roles"), diagnostics = roles.diagnostics as Record<string, unknown>;
      diagnostics.output_truncated = true;
      expect(isOrganizationalWorkbenchResult(roles, "roles")).toBe(true);
      diagnostics[key] = Number(diagnostics[key]) + 1;
      expect(isOrganizationalWorkbenchResult(roles, "roles")).toBe(false);
    }
  });
  it("retains legitimate queried zero profile rows without inflating source resource counts", () => {
    for (const metric of ["social_position", "interaction_two_resources"]) {
      const value = payload("profiles"); value.metric = metric; value.rows = [{resource: "unobserved", activity: "", resource2: metric === "social_position" ? "" : "also unobserved", value: 0, count: 0}];
      (value.diagnostics as Record<string, unknown>).resource_count = 0; expect(isOrganizationalWorkbenchResult(value)).toBe(true);
      value.metric = "distinct_activities"; expect(isOrganizationalWorkbenchResult(value)).toBe(false);
    }
  });
  it("rejects malformed numeric/native containers and accepts valid negative network durations", () => {
    const network = payload("network"); expect(isOrganizationalWorkbenchResult(network)).toBe(true);
    const edge = (network.edges as Array<Record<string, unknown>>)[0]; edge.performance = [Infinity]; expect(isOrganizationalWorkbenchResult(network)).toBe(false);
    edge.performance = [null]; expect(isOrganizationalWorkbenchResult(network)).toBe(false);
    const profile = payload("profiles"); (profile.rows as Array<Record<string, unknown>>)[0].value = NaN; expect(isOrganizationalWorkbenchResult(profile)).toBe(false);
    const local = payload("diagnostics"); local.group_relative_focus = {Team: {A: 1.1}}; expect(isOrganizationalWorkbenchResult(local)).toBe(false);
    const role = payload("roles"); (role.roles as Array<Record<string, unknown>>)[0].resources = [{name: "a", count: 0}]; expect(isOrganizationalWorkbenchResult(role)).toBe(false);
    expect(isOrganizationalWorkbenchResult(parseNativeJson(JSON.stringify(payload("profiles")).replace('"value":2', '"value":1e999')))).toBe(false);
    expect(isOrganizationalWorkbenchResult(parseNativeJson(JSON.stringify(payload("profiles")).replace('"value":2', '"value":"2"')))).toBe(false);
    expect(isOrganizationalWorkbenchResult(parseNativeJson(JSON.stringify(payload("roles")).replace('"count":3', '"count":"3"')))).toBe(false);
  });
  it("preserves exact uint64/decimal literals in native JSON, report, CSV and selected full nested rows", () => {
    const raw = JSON.stringify(payload("roles")).replace('"count":3', '"count":18446744073709551615');
    const roles = parseNativeJson(raw); expect(isOrganizationalWorkbenchResult(roles)).toBe(true);
    if (!isOrganizationalWorkbenchResult(roles)) throw new Error("Invalid fixture");
    const run = {...asRun("roles", roles), raw}, table = run.tables.find(item => item.id === "role_resources")!;
    expect(diagnosticCsv(table)).toContain("18446744073709551615"); expect(nativeJson(table.records![0])).toContain("18446744073709551615"); expect(organizationalWorkbenchReport(run)).toContain("18446744073709551615");
    const profileRaw = JSON.stringify(payload("profiles")).replace('"value":2', '"value":0.50000000000000005551115').replace('"count":2', '"count":9007199254740993');
    const profile = parseNativeJson(profileRaw); if (!isOrganizationalWorkbenchResult(profile)) throw new Error("Invalid fixture");
    expect(nativeCell((profile as {rows: object[]}).rows[0], "count")).toBe("9007199254740993");
    const profileRun = asRun("profiles", profile), rows = profileRun.tables.find(item => item.id === "/rows")!;
    expect(diagnosticCsv(rows)).toContain("0.50000000000000005551115"); expect(nativeJson(rows.records![0])).toContain("9007199254740993");
    expect(organizationalAllTablesCsv(profileRun)).toContain("0.50000000000000005551115");
  });
  it("exports every row/column and complete long nested Unicode values beyond UI paging", () => {
    const value = payload("roles"), long = "東京,\";" + "z".repeat(600);
    value.roles = Array.from({length: 63}, (_, index) => ({activities: [`activity:${index}`], resources: [{name: index === 62 ? long : `resource:${index}`, count: 1}]}));
    value.diagnostics = {...diagnosticBase(), activity_count: 63, resource_count: 63, role_count: 63};
    const run = asRun("roles", value), table = run.tables.find(item => item.id === "role_resources")!;
    expect(table.rows).toHaveLength(63); expect(diagnosticPage(table.rows, 2)).toHaveLength(13);
    expect(diagnosticCsv(table)).toContain("z".repeat(600)); expect(organizationalAllTablesCsv(run)).toContain("z".repeat(600));
    expect(nativeJson(table.records![62])).toContain(long.replace('"', '\\"')); expect(organizationalWorkbenchReport(run)).toContain('"activity:62"');
  });
  it("flattens all local contributions and every network performance value without replacing native maps", () => {
    const raw = JSON.stringify(payload("diagnostics")).replace('"A":3', '"A":9007199254740993');
    const local = parseNativeJson(raw); if (!isOrganizationalWorkbenchResult(local)) throw new Error("Invalid fixture");
    const tables = organizationalWorkbenchTables(local); const contributions = tables.find(item => item.id === "group_contributions")!;
    expect(contributions.rows).toHaveLength(1); expect(diagnosticCsv(contributions)).toContain("9007199254740993"); expect(nativeJson(contributions.records![0])).toContain("9007199254740993");
    const network = payload("network"); const edge = (network.edges as Array<Record<string, unknown>>)[0]; edge.performance = Array.from({length: 80}, (_, index) => index - 20);
    (network.diagnostics as Record<string, unknown>).performance_value_count = 80;
    const run = asRun("network", network); expect(run.tables.find(item => item.id === "network_performance")!.rows).toHaveLength(80); expect(organizationalAllTablesCsv(run)).toContain('"79"');
  });
});
