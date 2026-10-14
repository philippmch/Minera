import { describe, expect, it } from "vitest";
import {
  isPetriAlignmentWorkflow, petriAlignmentWorkbenchTables, petriAlignmentResultSummary,
  PETRI_ALIGNMENT_VARIANTS, type PetriAlignmentWorkflow,
} from "../src/petri-alignment-workbench";
import { diagnosticCsv } from "../src/model-diagnostics";
import { nativeJson, parseNativeJson } from "../src/native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "../src/native-response";
import capturedRequests from "./fixtures/petri-alignment-workbench-2-request-parameters.json";
import observedRequests from "./fixtures/petri-alignment-workbench-3-request-parameters.json";

interface ActualRequest {
  fields: Record<string, string>; log: string; model: string; log_bytes: number; model_bytes: number;
  log_sha256: string; model_sha256: string; response_sha256: string;
}
const requests = capturedRequests as Record<string, ActualRequest>;
const fixtures = import.meta.glob<string>("./fixtures/petri-alignment-workbench-2-*.json", {eager: true, query: "?raw", import: "default"});
const observedFixtures = import.meta.glob<string>("./fixtures/petri-alignment-workbench-3-*.json", {eager: true, query: "?raw", import: "default"});
function raw(name: string): string {
  const value = fixtures[`./fixtures/petri-alignment-workbench-2-${name}.json`];
  if (!value) throw new Error(`Missing captured native fixture: ${name}`);
  return value;
}
function actual(name: string): PetriAlignmentWorkflow {
  const value = parseNativeJson(raw(name));
  if (!isPetriAlignmentWorkflow(value)) throw new Error(`Invalid captured native workflow: ${name}`);
  return value;
}

describe("actual independent Petri alignment HTTP fixtures", () => {
  for (const [name, request] of Object.entries(observedRequests as Record<string, ActualRequest & {status: number}>)) it(`preserves repeated native boundary ${name} including HTTP${request.status}`, async () => {
    const original = observedFixtures[`./fixtures/petri-alignment-workbench-3-${name}.json`];
    expect(original).toBeDefined();
    const fields = Object.entries(request.fields);
    const value = await readNativeResponse(new Response(original, {status: request.status}), {path: "/api/align-petri-workbench", fields});
    expect(value).not.toBeNull();
    expect(nativeResponseJson(value as object)).toBe(original);
    expect(nativeResponseRequest(value as object)?.fields).toEqual(fields);
    const checksum = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(original))), byte => byte.toString(16).padStart(2, "0")).join("");
    expect(checksum).toBe(request.response_sha256);
    if (request.status === 400) {
      expect(isPetriAlignmentWorkflow(value)).toBe(false);
      expect(value).toEqual({error: "conformance: event is missing activity attribute 'phase'"});
      expect(nativeResponseRequest(value as object)?.fields).toContainEqual(["alignment_activity_attribute", "phase"]);
    } else {
      expect(isPetriAlignmentWorkflow(value)).toBe(true);
      if (!isPetriAlignmentWorkflow(value)) throw new Error("Invalid observed blank-XES native result");
      expect(nativeJson(value)).toBe(nativeJson(actual("blank-xes-activity")));
      expect(value.trace_groups[0].activities).toEqual(["", "B"]);
      expect(value.variant_details[0].complete_moves.some(move => move.type === "log")).toBe(false);
      const groups = petriAlignmentWorkbenchTables(value).find(table => table.id === "/trace_groups")!;
      expect(nativeJson(groups.records?.[0])).toContain('"activities":["","B"]');
    }
  });
  for (const [name, request] of Object.entries(requests)) it(`retains complete native ${name} output and its submitted options`, async () => {
    const original = raw(name), fields = Object.entries(request.fields);
    const value = await readNativeResponse(new Response(original, {status: 200}), {path: "/api/align-petri-workbench", fields});
    expect(isPetriAlignmentWorkflow(value)).toBe(true);
    if (!isPetriAlignmentWorkflow(value)) throw new Error("Invalid captured workflow");
    expect(nativeResponseJson(value)).toBe(original);
    expect(nativeResponseRequest(value)).toEqual({path: "/api/align-petri-workbench", fields});
    const checksum = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(original))), byte => byte.toString(16).padStart(2, "0")).join("");
    expect(checksum).toBe(request.response_sha256);
    expect(request.log_sha256).toMatch(/^[0-9a-f]{64}$/); expect(request.model_sha256).toMatch(/^[0-9a-f]{64}$/);
    expect(value.result.variant).toBe(request.fields.alignment_variant); expect(value.input_context.adapter).toBe(request.fields.alignment_input_adapter);
    expect(value.input_context.dataset).toBe(request.log); expect(value.input_context.model).toBe(request.model);
    expect(value.input_context.log_bytes).toBe(request.log_bytes); expect(value.input_context.model_bytes).toBe(request.model_bytes);
    if (request.fields.alignment_activity_attribute !== undefined) expect(value.input_context.activity_attribute).toBe(request.fields.alignment_activity_attribute.trim());
    if (request.fields.alignment_case_id_attribute !== undefined) expect(value.input_context.case_id_attribute).toBe(request.fields.alignment_case_id_attribute.trim());
    const tables = petriAlignmentWorkbenchTables(value);
    expect(tables.find(table => table.id === "request")?.rows).toEqual(fields);
    expect(tables.find(table => table.id === "/trace_groups")?.records).toBe(value.trace_groups);
    expect(tables.find(table => table.id === "/trace_groups")?.rows).toHaveLength(value.input_context.evaluated_case_count);
    expect(tables.find(table => table.id === "/model_context/places")?.rows).toHaveLength(value.model_context.places.length);
    expect(tables.find(table => table.id === "complete_moves")?.rows).toHaveLength(value.variant_details.reduce((count, detail) => count + detail.complete_moves.length, 0));
    expect(tables.find(table => table.id === "projected_model_moves")?.rows).toHaveLength(value.variant_details.reduce((count, detail) => count + detail.projected_model_moves.length, 0));
    expect(tables.find(table => table.id === "/variant_details")?.records).toBe(value.variant_details);
    const lastCase = value.trace_groups.at(-1);
    if (lastCase) expect(diagnosticCsv(tables.find(table => table.id === "/trace_groups")!)).toContain(lastCase.case_id);
    // The original parsed holders retain every native nested value and numeric token.
    expect(nativeJson(value)).toBe(nativeJson(parseNativeJson(original)));
  });

  it("covers six native algorithms over four original input shapes with every case retained", () => {
    expect(Object.keys(requests).length).toBe(78);
    for (const variant of PETRI_ALIGNMENT_VARIANTS) for (const adapter of ["log", "stream", "dataframe", "lazyframe"]) {
      const value = actual(`${variant}-${adapter}`);
      expect(value.input_context.evaluated_case_count).toBe(60); expect(value.input_context.evaluated_event_count).toBe(120);
      expect(value.result.computed).toBe(true); expect(value.result.trace_count).toBe(60); expect(value.result.total_cost).toBe(0);
      expect(value.trace_groups).toHaveLength(60); expect(value.trace_groups.every(group => group.activities.join(",") === "A,B")).toBe(true);
      expect(value.variant_details[0].complete_moves.some(move => move.type === "model" && move.activity_name === null && move.transition === 0)).toBe(true);
      const table = petriAlignmentWorkbenchTables(value).find(item => item.id === "/trace_groups")!;
      expect(diagnosticCsv(table)).toContain(value.trace_groups[59].case_id);
      expect(nativeJson(table.records?.[59])).toContain(value.trace_groups[59].case_id);
    }
  });
  it("preserves actual cost changes from scalars, activity maps and indexed arrays", () => {
    const totals: Record<string, number> = {"deviant-native": 2, "scalar-costs": 14.25, "activity-trace-costs": 6, "activity-model-costs": 8,
      "activity-sync-costs": 4, "position-trace-costs": 5, "index-model-costs": 8, "index-sync-costs": 4, "empty-containers": 2};
    for (const [name, cost] of Object.entries(totals)) {
      const value = actual(name); expect(value.result.computed).toBe(true); expect(value.result.trace_count).toBe(3); expect(value.result.total_cost).toBe(cost);
      const table = petriAlignmentWorkbenchTables(value, Object.entries(requests[name].fields)).find(item => item.id === "request")!;
      for (const [, submitted] of Object.entries(requests[name].fields)) expect(diagnosticCsv(table)).toContain(submitted.replace(/"/g, '""'));
    }
    expect(actual("native-silent-map").result.total_cost).toBe(0);
    expect(actual("silent-scalar").result.total_cost).toBe(120);
    expect(actual("standard-cost-profile").result.total_cost).toBe(60);
  });
  it("retains imported boundaries, custom selectors, flat regrouping and generator alternatives", () => {
    for (const adapter of ["log", "stream", "dataframe", "lazyframe"]) {
      expect(actual(`xes-${adapter}`).result.trace_count).toBe(3); expect(actual(`gzip-${adapter}`).result.trace_count).toBe(3);
      expect(actual(`tsv-custom-${adapter}`).input_context.activity_attribute).toBe("step");
      expect(actual(`selectors-${adapter}`).input_context.activity_attribute).toBe("phase");
      const regrouped = actual(`regroup-${adapter}`);
      expect(regrouped.input_context.source_case_count).toBe(60);
      expect(regrouped.input_context.evaluated_case_count).toBe(adapter === "log" ? 60 : 3);
      expect(regrouped.result.total_cost).toBe(adapter === "log" ? 0 : 114);
    }
    for (const variant of ["generator_dijkstra_no_heuristics", "generator_dijkstra_less_memory"]) {
      const value = actual(`alternatives-${variant}`), alternatives = value.variant_details[0].optimal_alignments;
      expect(alternatives).toHaveLength(3);
      const table = petriAlignmentWorkbenchTables(value).find(item => item.id === "optimal_alignments")!;
      expect(table.rows).toHaveLength(6); // Native result alternatives and complete transition supplements remain independently available.
      expect(nativeJson(table.records?.[2])).toContain(nativeJson(alternatives[2]));
    }
  });
  it("preserves wide PNML literals and effective marking overrides in CSV and selected-row JSON", () => {
    for (const adapter of ["log", "stream", "dataframe", "lazyframe"]) {
      const value = actual(`wide-marking-${adapter}`), places = petriAlignmentWorkbenchTables(value).find(table => table.id === "/model_context/places")!;
      expect(value.model_context.places[0].effective_initial_tokens).toBe(1);
      expect(diagnosticCsv(places)).toContain("18446744073709551617");
      expect(nativeJson(places.records?.[0])).toContain('"pnml_initial_tokens":18446744073709551617');
      expect(nativeJson(value)).toContain('"pnml_initial_tokens":18446744073709551617');
    }
    const override = actual("marking-overrides");
    expect(override.model_context.places[0].pnml_initial_tokens).toBe(1); expect(override.model_context.places[0].effective_initial_tokens).toBe(0);
    expect(override.model_context.places[2].effective_final_tokens).toBe(1); expect(override.result.total_cost).toBe(60);
  });
  it("keeps native empty and uncomputed results visible with every uploaded case bound", () => {
    for (const adapter of ["log", "stream", "dataframe", "lazyframe"]) {
      const value = actual(`empty-log-${adapter}`);
      expect(value.input_context.evaluated_case_count).toBe(0); expect(value.result.trace_count).toBe(0);
      expect(value.trace_groups).toEqual([]); expect(value.variant_details).toEqual([]);
      expect(petriAlignmentWorkbenchTables(value).find(table => table.id === "complete_moves")?.rows).toEqual([]);
    }
    const bounded = actual("uncomputed-bound");
    expect(bounded.result.computed).toBe(false); expect(bounded.result.exhausted).toBe(true); expect(bounded.trace_groups).toHaveLength(60);
    expect(bounded.variant_details).toEqual([]); expect(petriAlignmentResultSummary(bounded.result)).toContain("not computed");
    expect(petriAlignmentResultSummary(bounded.result)).not.toMatch(/Fitness \d/);
  });
  it("retains original blank activity groups alongside the native omitted-move behavior", () => {
    const value = actual("blank-xes-activity");
    expect(value.trace_groups[0].activities).toEqual(["", "B"]); expect(value.result.total_cost).toBe(2);
    expect(value.variant_details[0].complete_moves.some(move => move.type === "model" && move.activity_name === null)).toBe(true);
    // The current native search omits an empty-label log move; inspection must preserve its returned result exactly.
    expect(value.variant_details[0].complete_moves.some(move => move.type === "log")).toBe(false);
    const table = petriAlignmentWorkbenchTables(value).find(item => item.id === "/trace_groups")!;
    expect(nativeJson(table.records?.[0])).toContain('"activities":["","B"]');
  });
});
