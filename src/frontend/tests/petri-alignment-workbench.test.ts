import { describe, expect, it, vi } from "vitest";
import {
  defaultPetriAlignmentWorkbenchForm, PETRI_ALIGNMENT_VARIANTS, PETRI_ALIGNMENT_XES_VARIANTS,
  isPetriAlignmentWorkflow, petriAlignmentWorkbenchFields, petriAlignmentWorkbenchTables, petriAlignmentResultSummary,
  runPetriAlignmentWorkbench, petriAlignmentWorkbenchReport,
  type PetriAlignmentWorkflow,
} from "../src/petri-alignment-workbench";
import { diagnosticCsv } from "../src/model-diagnostics";
import { nativeJson, parseNativeJson } from "../src/native-json";

function workflow(): PetriAlignmentWorkflow {
  return {
    family: "PETRI_ALIGNMENT_WORKFLOW",
    input_context: {adapter: "log", origin: "uploaded_csv_rows", dataset: "orders.csv", model: "orders.pnml", log_bytes: 91, model_bytes: 389,
      source_case_count: 2, source_event_count: 4, evaluated_case_count: 2, evaluated_event_count: 4, activity_attribute: "concept:name", case_id_attribute: "case_id"},
    model_context: {
      places: [{index: 0, id: "start", pnml_initial_tokens: 1, pnml_final_tokens: 0, effective_initial_tokens: 1, effective_final_tokens: 0, initial_key: true, final_key: false},
        {index: 1, id: "end", pnml_initial_tokens: 0, pnml_final_tokens: 1, effective_initial_tokens: 0, effective_final_tokens: 1, initial_key: false, final_key: true}],
      transitions: [{index: 0, id: "visible", label: "A"}, {index: 1, id: "hidden", label: null}],
    },
    activities: ["A", "B"],
    result: {computed: true, exhausted: false, fitness: 0.5, precision: 0.75, fitting_traces: 0, trace_count: 2, total_cost: 6,
      variant: "dijkstra_less_memory", activity_attribute: "concept:name", diagnostics: {visited_states: 5},
      variants: [{activities: [0, 1], activity_names: ["A", "B"], count: 2, cost: 3, fitness: 0.5,
        moves: [{type: "sync", activity: 0, activity_name: "A", silent: false}, {type: "model", activity: null, silent: true}, {type: "log", activity: 1, activity_name: "B"}],
        alternatives: [[], [{type: "log", activity: 1, activity_name: "B"}]], diagnostics: {queued_states: 9}}]},
    variant_details: [{variant_index: 0, worst_cost: 6,
      complete_moves: [{type: "sync", activity_name: "A", transition: 0}, {type: "model", activity_name: null, transition: 1}, {type: "log", activity_name: "B", transition: null}],
      projected_model_moves: [{activity_name: "A", transition: 0, marking_before: [1, 0], marking_after: [0, 1]},
        {activity_name: null, transition: 1, marking_before: [0, 1], marking_after: [0, 1]}],
      optimal_alignments: [[], [{type: "sync", activity_name: "A", transition: 0}]]}],
    trace_groups: [{trace_index: 0, case_id: "one", event_count: 2, activities: ["A", "B"], variant_index: 0},
      {trace_index: 1, case_id: "two", event_count: 2, activities: ["A", "B"], variant_index: 0}],
  };
}

describe("independent Petri alignment options", () => {
  it("offers all six native variants and preserves numeric text, zero caps, maps, arrays and markings", () => {
    expect(PETRI_ALIGNMENT_VARIANTS).toHaveLength(6);
    const form = defaultPetriAlignmentWorkbenchForm();
    form.adapter = "lazyframe";
    form.alignmentFields = {alignment_activity_attribute: "task", alignment_case_id_attribute: "bucket", alignment_max_rows_to_materialize: "0",
      max_states: "18446744073709551617", max_align_time: "0.100000000000000005551115", trace_cost: "9007199254740993",
      model_cost: "2.0000000000000000001", silent_model_cost: "1e-30", sync_cost: "0", cost_profile: "standard",
      alignment_trace_costs: "{}", alignment_model_costs: '{"visible":18446744073709551617}', alignment_sync_costs: "{}",
      alignment_trace_costs_by_position: "[]", alignment_model_costs_by_transition: "[18446744073709551617]", alignment_sync_costs_by_transition: "[]",
      alignment_initial_marking: "18446744073709551617,0", alignment_final_marking: "0,1"};
    const fields = Object.fromEntries(petriAlignmentWorkbenchFields(form, "orders.TSV"));
    expect(fields.delimiter).toBe("\t"); expect(fields.alignment_variant).toBe("dijkstra_less_memory"); expect(fields.alignment_input_adapter).toBe("lazyframe");
    for (const [key, value] of Object.entries(form.alignmentFields)) expect(fields[key]).toBe(value);
    const defaults = Object.fromEntries(petriAlignmentWorkbenchFields(defaultPetriAlignmentWorkbenchForm(), "orders.csv"));
    expect(defaults).toEqual({alignment_variant: "dijkstra_less_memory", alignment_input_adapter: "log"});
  });
  it("retains inactive variant settings while submitting only applicable controls", () => {
    const form = defaultPetriAlignmentWorkbenchForm();
    form.alignmentFields = {max_alignments: "9007199254740993", exponent: "0.9000000000000000001", synchronous_dijkstra: "false", trace_cost: "2"};
    for (const variant of PETRI_ALIGNMENT_VARIANTS) {
      form.variant = variant;
      const fields = Object.fromEntries(petriAlignmentWorkbenchFields(form, "orders.csv"));
      expect(fields.max_alignments).toBe(variant.startsWith("generator_") ? "9007199254740993" : undefined);
      expect(fields.exponent).toBe(variant === "discounted_a_star" ? "0.9000000000000000001" : undefined);
      expect(fields.synchronous_dijkstra).toBe(variant === "discounted_a_star" ? "false" : undefined);
      expect(fields.trace_cost).toBe("2");
    }
    expect(form.alignmentFields.max_alignments).toBe("9007199254740993");
    expect(form.alignmentFields.synchronous_dijkstra).toBe("false");
    form.variant = "dijkstra_less_memory"; form.alignmentFields.synchronous_dijkstra = "invalid but inactive";
    expect(() => petriAlignmentWorkbenchFields(form, "orders.csv")).not.toThrow();
  });
  it("supports all XES importers and the shared original-log import settings", () => {
    const form = defaultPetriAlignmentWorkbenchForm();
    form.importFields = {xes_case_attribute: "case", xes_activity_attribute: "event", xes_timestamp_attribute: "time", xes_resource_attribute: "worker",
      xes_lifecycle_attribute: "stage", xes_max_traces: "0", xes_attributes_to_read: "event,amount", xes_max_decompressed_bytes: "18446744073709551617",
      xes_require_timestamp: "false", xes_timestamp_sort: "true", xes_reverse_sort: "false"};
    expect(PETRI_ALIGNMENT_XES_VARIANTS).toHaveLength(6);
    for (const xes_variant of PETRI_ALIGNMENT_XES_VARIANTS) {
      form.importFields.xes_variant = xes_variant;
      const fields = Object.fromEntries(petriAlignmentWorkbenchFields(form, "orders.xes.gz"));
      for (const [key, value] of Object.entries(form.importFields)) expect(fields[key]).toBe(value);
    }
  });
  it("validates JSON syntax and shape, select values, delimiters and integer caps before transport", () => {
    const invalid: Array<[string, string, string]> = [
      ["alignment_trace_costs", "{", "valid JSON"], ["alignment_model_costs", "[]", "JSON object"],
      ["alignment_sync_costs_by_transition", "{}", "JSON array"], ["cost_profile", "custom", "supported"],
      ["alignment_max_rows_to_materialize", "0.5", "integer"], ["max_states", "-1", "integer"],
    ];
    for (const [key, value, message] of invalid) {
      const form = defaultPetriAlignmentWorkbenchForm(); form.alignmentFields[key] = value;
      expect(() => petriAlignmentWorkbenchFields(form, "orders.csv")).toThrow(message);
    }
    const form = defaultPetriAlignmentWorkbenchForm(); form.variant = "discounted_a_star"; form.alignmentFields.synchronous_dijkstra = "yes";
    expect(() => petriAlignmentWorkbenchFields(form, "orders.csv")).toThrow("true or false");
    form.alignmentFields = {}; form.importFields.xes_variant = "unknown";
    expect(() => petriAlignmentWorkbenchFields(form, "orders.xes")).toThrow("supported XES importer");
    form.importFields = {xes_require_timestamp: "yes"}; expect(() => petriAlignmentWorkbenchFields(form, "orders.xes")).toThrow("true or false");
    form.importFields = {delimiter: "||"}; expect(() => petriAlignmentWorkbenchFields(form, "orders.csv")).toThrow("one character");
    form.importFields = {}; form.variant = "invalid" as typeof form.variant;
    expect(() => petriAlignmentWorkbenchFields(form, "orders.csv")).toThrow("supported Petri alignment variant");
    form.variant = "dijkstra_less_memory"; form.adapter = "invalid" as typeof form.adapter;
    expect(() => petriAlignmentWorkbenchFields(form, "orders.csv")).toThrow("supported alignment input shape");
  });
});

describe("complete independent alignment inspection", () => {
  it("binds trimmed native selectors while retaining raw multipart, request and report values", async () => {
    const form = defaultPetriAlignmentWorkbenchForm();
    form.alignmentFields = {alignment_activity_attribute: "  task  ", alignment_case_id_attribute: " \tbucket\t ",
      alignment_trace_costs: '{"A":18446744073709551617}', trace_cost: "0.100000000000000005551115"};
    const value = workflow();
    value.input_context.activity_attribute = value.result.activity_attribute = "task";
    value.input_context.case_id_attribute = "bucket";
    const raw = JSON.stringify(value).replace('"total_cost":6', '"total_cost":9007199254740993');
    const log = new File(["x".repeat(91)], "orders.csv"), model = new File(["x".repeat(389)], "orders.pnml");
    const fetchMock = vi.fn(async (url: string | URL | Request, init?: RequestInit) => {
      expect(url).toBe("/api/align-petri-workbench"); expect(init?.method).toBe("POST");
      const body = init?.body as FormData;
      expect(body.get("alignment_activity_attribute")).toBe("  task  ");
      expect(body.get("alignment_case_id_attribute")).toBe(" \tbucket\t ");
      expect(body.get("alignment_trace_costs")).toBe('{"A":18446744073709551617}');
      expect(body.get("trace_cost")).toBe("0.100000000000000005551115");
      return new Response(raw, {status: 200, headers: {"content-type": "application/json"}});
    });
    vi.stubGlobal("fetch", fetchMock);
    try {
      const run = await runPetriAlignmentWorkbench(log, model, form);
      expect(fetchMock).toHaveBeenCalledOnce(); expect(run.raw).toBe(raw);
      expect(run.value.input_context.activity_attribute).toBe("task"); expect(run.value.input_context.case_id_attribute).toBe("bucket");
      expect(Object.fromEntries(run.fields)).toMatchObject(form.alignmentFields);
      expect(run.inputs.log).toBe(log); expect(run.inputs.model).toBe(model);
      expect(run.inputs.log_sha256).toMatch(/^[0-9a-f]{64}$/); expect(run.inputs.model_sha256).toMatch(/^[0-9a-f]{64}$/);
      const report = petriAlignmentWorkbenchReport(run), parsed = parseNativeJson(report) as {request: {fields: Array<[string, string]>}};
      expect(Object.fromEntries(parsed.request.fields)).toMatchObject(form.alignmentFields);
      expect(report).toContain("18446744073709551617"); expect(report).toContain("0.100000000000000005551115");
      expect(report).toContain('"total_cost": 9007199254740993');
      expect(diagnosticCsv(run.tables.find(table => table.id === "request")!)).toContain('"  task  "');
    } finally {vi.unstubAllGlobals();}
  });
  it("accepts a complete native shape and exposes silent transitions, projections and empty alternatives", () => {
    const value = workflow(); expect(isPetriAlignmentWorkflow(value)).toBe(true);
    const fields: Array<[string, string]> = [["alignment_trace_costs", "{}"], ["alignment_trace_costs_by_position", "[]"]];
    const tables = petriAlignmentWorkbenchTables(value, fields);
    expect(tables.find(table => table.id === "/trace_groups")?.records).toBe(value.trace_groups);
    const moves = tables.find(table => table.id === "complete_moves")!;
    expect(moves.rows[1]).toEqual([0, 1, "model", null, 1, value.variant_details[0].complete_moves[1]]);
    expect(tables.find(table => table.id === "optimal_alignments")?.rows.map(row => row[3])).toEqual([0, 1, 0, 1]);
    expect(tables.find(table => table.id === "projected_model_moves")?.rows[1][3]).toBe(1);
    expect(tables.find(table => table.id === "request")?.rows).toEqual(fields);
    expect(nativeJson(tables.find(table => table.id === "request")?.records?.[0])).toContain('"value":"{}"');
  });
  it("keeps all cases, moves, alternatives and supplemental nested fields beyond preview limits", () => {
    const value = workflow();
    value.input_context.evaluated_case_count = value.input_context.source_case_count = value.result.trace_count = 63;
    value.input_context.evaluated_event_count = value.input_context.source_event_count = 126;
    value.result.variants[0].count = 63;
    value.trace_groups = Array.from({length: 63}, (_, trace_index) => ({trace_index, case_id: `case-${trace_index}-東京`, event_count: 2, activities: ["A", "B"], variant_index: 0}));
    value.variant_details[0].complete_moves = Array.from({length: 81}, () => ({type: "sync", activity_name: "A", transition: 0}));
    value.variant_details[0].optimal_alignments = Array.from({length: 61}, (_, i) => i % 2 ? [] : [{type: "sync", activity_name: `alternative-${i}`, transition: 0}]);
    const finalMove = value.variant_details[0].complete_moves[80] as unknown as Record<string, unknown>;
    finalMove.native_supplement = {search: {long_context: "x".repeat(501)}, unused: [], note: "last move"};
    expect(isPetriAlignmentWorkflow(value)).toBe(true);
    const tables = petriAlignmentWorkbenchTables(value);
    expect(tables.find(table => table.id === "/trace_groups")?.rows).toHaveLength(63);
    expect(diagnosticCsv(tables.find(table => table.id === "/trace_groups")!)).toContain("case-62-東京");
    const moves = tables.find(table => table.id === "complete_moves")!;
    expect(moves.rows).toHaveLength(81); expect(diagnosticCsv(moves)).toContain("x".repeat(501));
    expect(nativeJson(moves.records?.[80])).toContain('"unused":[]'); expect(nativeJson(moves.records?.[80])).toContain("last move");
    const alternatives = tables.find(table => table.id === "optimal_alignments")!;
    expect(alternatives.rows).toHaveLength(63); expect(diagnosticCsv(alternatives)).toContain("alternative-60");
  });
  it("preserves exact native integer and decimal literals in tables, selected rows and summaries", () => {
    const raw = JSON.stringify(workflow()).replace('"worst_cost":6', '"worst_cost":18446744073709551617')
      .replace('"pnml_initial_tokens":1', '"pnml_initial_tokens":18446744073709551617')
      .replace('"marking_before":[1,0]', '"marking_before":[18446744073709551617,0]')
      .replace('"total_cost":6', '"total_cost":9007199254740993')
      .replace('"fitness":0.5', '"fitness":0.50000000000000005551115');
    const value = parseNativeJson(raw); expect(isPetriAlignmentWorkflow(value)).toBe(true);
    if (!isPetriAlignmentWorkflow(value)) throw new Error("Invalid synthetic workflow");
    const tables = petriAlignmentWorkbenchTables(value);
    const places = tables.find(table => table.id === "/model_context/places")!;
    expect(diagnosticCsv(places)).toContain("18446744073709551617"); expect(nativeJson(places.records?.[0])).toContain("18446744073709551617");
    const detail = tables.find(table => table.id === "/variant_details")!;
    expect(diagnosticCsv(detail)).toContain("18446744073709551617"); expect(nativeJson(detail.records?.[0])).toContain("18446744073709551617");
    const projection = tables.find(table => table.id === "projected_model_moves")!;
    expect(diagnosticCsv(projection)).toContain("18446744073709551617"); expect(nativeJson(projection.records?.[0])).toContain("18446744073709551617");
    expect(petriAlignmentResultSummary(value.result)).toContain("0.50000000000000005551115");
    expect(petriAlignmentResultSummary(value.result)).toContain("9007199254740993");
  });
  it("retains uncomputed and empty workflows without presenting placeholder fitness as measured", () => {
    const value = workflow(); value.result.computed = false; value.result.exhausted = true; value.result.trace_count = 0;
    value.result.variants = []; value.variant_details = []; value.trace_groups.forEach(group => {group.variant_index = null;});
    expect(isPetriAlignmentWorkflow(value)).toBe(true);
    expect(petriAlignmentResultSummary(value.result)).toContain("not computed"); expect(petriAlignmentResultSummary(value.result)).toContain("search bound");
    expect(petriAlignmentResultSummary(value.result)).not.toContain("Fitness 0.5");
    const tables = petriAlignmentWorkbenchTables(value);
    for (const id of ["/result/variants", "/variant_details", "complete_moves", "optimal_alignments", "projected_model_moves"]) expect(tables.find(table => table.id === id)?.rows).toEqual([]);
    expect(tables.find(table => table.id === "/trace_groups")?.rows).toHaveLength(2);
    value.result.computed = true; value.result.exhausted = false; value.trace_groups = [];
    value.input_context.evaluated_case_count = value.input_context.evaluated_event_count = 0;
    expect(isPetriAlignmentWorkflow(value)).toBe(true); expect(petriAlignmentWorkbenchTables(value).find(table => table.id === "/trace_groups")?.rows).toEqual([]);
  });
  it("rejects incomplete containers, detached counts, invalid indices and malformed markings", () => {
    const mutations: Array<(value: PetriAlignmentWorkflow) => void> = [
      value => {value.trace_groups.pop();}, value => {value.trace_groups[0].event_count = 1;},
      value => {value.result.trace_count = 0;}, value => {value.variant_details = [];},
      value => {value.trace_groups[0].variant_index = 9;}, value => {value.variant_details[0].complete_moves[0].transition = 9;},
      value => {value.variant_details[0].complete_moves[2].transition = 0;}, value => {value.variant_details[0].projected_model_moves[0].marking_after = [];},
      value => {value.result.variants[0].moves[0].activity = 9;}, value => {value.result.activity_attribute = "detached";},
      value => {value.model_context.places[0].effective_initial_tokens = 0.5;},
    ];
    for (const mutate of mutations) {const value = workflow(); mutate(value); expect(isPetriAlignmentWorkflow(value)).toBe(false);}
    expect(isPetriAlignmentWorkflow({family: "PETRI_ALIGNMENT_WORKFLOW"})).toBe(false);
    expect(isPetriAlignmentWorkflow(null)).toBe(false);
  });
});
