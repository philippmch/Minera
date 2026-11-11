import { describe, expect, it, vi } from "vitest";
import {
  isOrganizationalWorkbenchResult, organizationalWorkbenchTables,
  organizationalSourceFromSchema, runOrganizationalWorkbench,
  organizationalRoleGroups, organizationalWorkbenchFields,
  organizationalWorkbenchReport, organizationalAllTablesCsv,
  type OrganizationalFamily, type OrganizationalSource,
  type OrganizationalWorkbenchRun,
} from "../src/organizational-workbench";
import { diagnosticCsv, diagnosticPage } from "../src/model-diagnostics";
import { parseNativeJson, nativeJson } from "../src/native-json";
import type { LogQuerySchema } from "../src/log-query";
import type { MineraExport } from "../src/types";

interface CapturedEntry {
  name: string; family: OrganizationalFamily; fields: Record<string, string>;
  source: OrganizationalSource; response: string; sha256: string;
}
interface CapturedManifest {
  prefix: string; origin: string; source_file: string; entries: CapturedEntry[];
  auxiliary: Record<string, string>;
}
const rawFiles = import.meta.glob<string>("./fixtures/organizational-workbench-*.json", {eager: true, query: "?raw", import: "default"});
const manifests = Object.entries(rawFiles).filter(([name]) => name.endsWith("-requests.json"))
  .map(([path, raw]) => ({path, value: JSON.parse(raw) as CapturedManifest}));
const rawResponse = (name: string): string => {
  const raw = rawFiles[`./fixtures/${name}`];
  if (raw === undefined) throw new Error(`Missing captured own-server response: ${name}`);
  return raw;
};
const readEntry = (entry: CapturedEntry) => {
  const value = parseNativeJson(rawResponse(entry.response));
  if (!isOrganizationalWorkbenchResult(value, entry.family)) throw new Error(`Invalid actual native fixture ${entry.name}`);
  return value;
};
const readAux = (manifest: CapturedManifest, name: string): unknown => parseNativeJson(rawResponse(manifest.auxiliary[name]));
const runFromEntry = (entry: CapturedEntry): OrganizationalWorkbenchRun => {
  const value = readEntry(entry), fields = Object.entries(entry.fields);
  return {family: entry.family, source: entry.source, fields, value, raw: rawResponse(entry.response), tables: organizationalWorkbenchTables(value, fields)};
};

describe("actual own-server organizational workbench fixtures", () => {
  it("requires completed real HTTP fixture captures for the full four-family application", () => {
    expect(manifests.length).toBeGreaterThan(0);
    for (const {value: manifest} of manifests) {
      expect(new Set(manifest.entries.map(entry => entry.family))).toEqual(new Set(["roles", "profiles", "diagnostics", "network"]));
      expect(manifest.entries.filter(entry => entry.name.startsWith("profiles-") && !entry.name.includes("absent") && !entry.name.includes("cap")).length).toBe(11);
    }
  });
  for (const {value: manifest} of manifests) {
    for (const entry of manifest.entries) it(`${manifest.prefix} preserves every native ${entry.name} field and submitted option`, () => {
      const value = readEntry(entry), tables = organizationalWorkbenchTables(value, Object.entries(entry.fields));
      expect(value.input_context).toEqual(entry.source);
      expect(nativeJson(value)).toBeTruthy();
      expect(tables.find(table => table.id === "request")?.rows).toEqual(Object.entries(entry.fields));
      if (value.family === "ROLE_DISCOVERY") {
        expect(tables.find(table => table.id === "role_resources")?.rows.length).toBe(value.roles.reduce((sum, role) => sum + role.resources.length, 0));
        expect(tables.find(table => table.id === "role_activities")?.rows.length).toBe(value.roles.reduce((sum, role) => sum + role.activities.length, 0));
      } else if (value.family === "RESOURCE_PROFILES") {
        expect(tables.find(table => table.id === "/rows")?.rows.length ?? 0).toBe(value.rows.length);
      } else if (value.family === "LOCAL_DIAGNOSTICS") {
        expect((tables.find(table => table.id === "group_metrics")?.rows.length ?? 0) + (tables.find(table => table.id === "group_contributions")?.rows.length ?? 0)).toBe(value.diagnostics.row_count);
      } else {
        expect(tables.find(table => table.id === "/edges")?.rows.length ?? 0).toBe(value.edges.length);
        expect(tables.find(table => table.id === "network_performance")?.rows.length ?? 0).toBe(value.edges.reduce((sum, edge) => sum + edge.performance.length, 0));
      }
    });

    it(`${manifest.prefix} keeps all rows after the first UI page and in exports`, () => {
      for (const [name, tableId, expected] of [["roles-log", "role_resources", 30], ["profiles-distinct_activities", "/rows", 30], ["network-performance", "/edges", 30], ["diagnostics-typed", "group_metrics", 210]] as const) {
        const entry = manifest.entries.find(item => item.name === name)!;
        const run = runFromEntry(entry), table = run.tables.find(item => item.id === tableId)!;
        expect(table.rows.length).toBe(expected);
        expect(diagnosticPage(table.rows, 0)).toHaveLength(25);
        expect(diagnosticPage(table.rows, 1)).toHaveLength(Math.min(25, expected - 25));
        expect(diagnosticCsv(table)).toContain(String(table.rows.at(-1)![0]));
        expect(organizationalAllTablesCsv(run)).toContain(table.title);
        const report = parseNativeJson(organizationalWorkbenchReport(run)) as {source: unknown; native_result: unknown};
        expect(report.source).toEqual(entry.source);
        expect(report.native_result).toEqual(run.value);
      }
    });

    it(`${manifest.prefix} preserves Unicode, delimiter identities, zeros, overlapping memberships and empty groups`, () => {
      const typed = readEntry(manifest.entries.find(entry => entry.name === "diagnostics-typed")!);
      const explicit = readEntry(manifest.entries.find(entry => entry.name === "diagnostics-groups")!);
      if (typed.family !== "LOCAL_DIAGNOSTICS" || explicit.family !== "LOCAL_DIAGNOSTICS") throw new Error("Wrong actual family");
      expect(Object.keys(typed.group_relative_focus)).toEqual(expect.arrayContaining(["7", "true"]));
      const delimiterGroup = Object.keys(explicit.group_member_contribution).find(key => key.includes("東京"))!;
      const delimiterResource = Object.keys(explicit.group_member_contribution[delimiterGroup]).find(key => key.includes("東京"))!;
      const delimiterActivity = Object.keys(explicit.group_member_contribution[delimiterGroup][delimiterResource]).find(key => key.includes("診察"))!;
      expect(delimiterGroup).toContain("\n"); expect(delimiterResource).toContain(";"); expect(delimiterActivity).toContain('"');
      expect(explicit.group_member_contribution["__proto__"][delimiterResource][delimiterActivity]).toBeGreaterThan(0);
      expect(Object.values(explicit.group_relative_focus.empty).every(value => value === 0)).toBe(true);
      expect(Object.values(explicit.group_coverage.unseen)).toEqual([0]);
      expect(typed.diagnostics.skipped_events).toBe(1);
      const empty = readEntry(manifest.entries.find(entry => entry.name === "diagnostics-empty-group")!);
      expect(empty.diagnostics).toMatchObject({computed: true, group_count: 1, row_count: 0, output_truncated: false});
    });

    it(`${manifest.prefix} permits aggregate counts larger than capped native output and rejects deficient counts`, () => {
      const networkEntry = manifest.entries.find(entry => entry.name === "network-row-cap")!;
      const network = readEntry(networkEntry);
      if (network.family !== "NETWORK_ANALYSIS") throw new Error("Wrong actual family");
      expect(network.edges).toHaveLength(2); expect(network.diagnostics.link_count).toBe(40);
      expect(network.diagnostics.performance_value_count).toBeGreaterThan(network.edges.reduce((sum, edge) => sum + edge.performance.length, 0));
      const deficient = JSON.parse(rawResponse(networkEntry.response)); deficient.diagnostics.link_count = 0;
      expect(isOrganizationalWorkbenchResult(deficient, "network")).toBe(false);
      const localEntry = manifest.entries.find(entry => entry.name === "diagnostics-row-cap")!;
      const local = readEntry(localEntry);
      expect(local.diagnostics).toMatchObject({group_count: 3, row_count: 1, output_truncated: true});
      const malformed = JSON.parse(rawResponse(localEntry.response)); malformed.diagnostics.row_count = 2;
      expect(isOrganizationalWorkbenchResult(malformed, "diagnostics")).toBe(false);
    });

    it(`${manifest.prefix} retains negative and fractional raw seconds while frequency has no values`, () => {
      const perf = readEntry(manifest.entries.find(entry => entry.name === "network-performance")!);
      const frequency = readEntry(manifest.entries.find(entry => entry.name === "network-frequency")!);
      if (perf.family !== "NETWORK_ANALYSIS" || frequency.family !== "NETWORK_ANALYSIS") throw new Error("Wrong actual family");
      const all = perf.edges.flatMap(edge => edge.performance);
      expect(all.some(seconds => seconds !== null && seconds < 0)).toBe(true); expect(all).toContain(0.125);
      expect(frequency.edges.every(edge => edge.performance.length === 0)).toBe(true);
      expect(organizationalWorkbenchTables(perf).find(table => table.id === "network_performance")?.rows.length).toBe(40);
    });

    it(`${manifest.prefix} retains queried zero profiles without inflating source resources`, () => {
      for (const name of ["profiles-absent-social", "profiles-absent-pair"]) {
        const value = readEntry(manifest.entries.find(entry => entry.name === name)!);
        if (value.family !== "RESOURCE_PROFILES") throw new Error("Wrong actual family");
        expect(value.diagnostics.resource_count).toBe(0); expect(value.rows).toHaveLength(1);
        expect(value.rows[0]).toMatchObject({value: 0, count: 0});
      }
    });

    it(`${manifest.prefix} binds role-to-diagnostics groups to the exact source and selected columns`, () => {
      const run = runFromEntry(manifest.entries.find(entry => entry.name === "roles-pandas")!);
      const form = organizationalRoleGroups(run, run.source), groups = JSON.parse(form.fields.local_diagnostics_groups_json);
      expect(Object.keys(groups)).toHaveLength(30);
      expect(form.fields.local_diagnostics_resource_attribute).toBe("team:resource");
      expect(form.fields.local_diagnostics_activity_attribute).toBe("team:activity");
      expect(organizationalWorkbenchFields("diagnostics", form, run.source)).toContainEqual(["local_diagnostics_groups_json", form.fields.local_diagnostics_groups_json]);
      expect(() => organizationalWorkbenchFields("diagnostics", form, {...run.source, source_id: "future-source"})).toThrow(/different source/);
    });

    it(`${manifest.prefix} rejects future source identities despite matching dataset and counts`, () => {
      const original = readAux(manifest, "mine-original") as MineraExport;
      const schema = readAux(manifest, "mine-original-schema") as LogQuerySchema;
      const future = readAux(manifest, "remine-same-bytes-schema") as LogQuerySchema;
      expect(future.source_id).not.toBe(schema.source_id);
      expect(organizationalSourceFromSchema(schema, original)).toMatchObject({source_id: schema.source_id, dataset: manifest.source_file, case_count: 4, event_count: 46});
      expect(() => organizationalSourceFromSchema(future, original)).toThrow(/does not match/);
    });

    it(`${manifest.prefix} rejects missing, malformed and detached source metadata using real raw responses`, async () => {
      const entry = manifest.entries.find(item => item.name === "roles-log")!;
      const raw = rawResponse(entry.response), missing = JSON.parse(raw); delete missing.input_context;
      expect(isOrganizationalWorkbenchResult(missing)).toBe(false);
      const inconsistent = JSON.parse(raw); inconsistent.input_context.event_count = 45;
      expect(isOrganizationalWorkbenchResult(inconsistent)).toBe(false);
      const quoted = JSON.parse(raw); quoted.diagnostics.role_count = "30";
      expect(isOrganizationalWorkbenchResult(quoted)).toBe(false);
      for (const source of [{...entry.source, source_id: "future-source"}, {...entry.source, dataset: "same-count-other-source.xes"}, {...entry.source, event_count: 45}]) {
        vi.stubGlobal("fetch", vi.fn(async () => new Response(raw, {status: 200})));
        try {
          await expect(runOrganizationalWorkbench("roles", source, {fields: Object.fromEntries(Object.entries(entry.fields).filter(([key]) => key !== "source_id" && key !== "include_source"))})).rejects.toThrow(/does not match/);
        } finally {vi.unstubAllGlobals();}
      }
    });

    it(`${manifest.prefix} retains original response bytes and exact submitted fields in workflow exports`, async () => {
      const entry = manifest.entries.find(item => item.name === "network-performance")!;
      const raw = rawResponse(entry.response);
      vi.stubGlobal("fetch", vi.fn(async () => new Response(raw, {status: 200})));
      try {
        const run = await runOrganizationalWorkbench(entry.family, entry.source, {fields: Object.fromEntries(Object.entries(entry.fields).filter(([key]) => key !== "source_id" && key !== "include_source"))});
        expect(run.raw).toBe(raw);
        const report = JSON.parse(organizationalWorkbenchReport(run));
        expect(Object.fromEntries(report.request.fields)).toEqual(entry.fields);
        expect(report.native_result).toEqual(JSON.parse(raw));
      } finally {vi.unstubAllGlobals();}
    });
  }
});
