import { afterEach, describe, expect, it, vi } from "vitest";
import dfg from "./fixtures/alignment-inspection-dfg-metrics.json?raw";
import dfgLog from "./fixtures/alignment-inspection-dfg-log.json?raw";
import search from "./fixtures/alignment-inspection-tree-search_graph_pt.json?raw";
import dynamic from "./fixtures/alignment-inspection-tree-dynamic_programming.json?raw";
import milp from "./fixtures/alignment-inspection-tree-milp.json?raw";
import approx from "./fixtures/alignment-inspection-tree-approximated_original.json?raw";
import decomposed from "./fixtures/alignment-inspection-decomposed.json?raw";
import edit from "./fixtures/alignment-inspection-edit-false.json?raw";
import editAnti from "./fixtures/alignment-inspection-edit-true.json?raw";
import anti from "./fixtures/alignment-inspection-petri-anti.json?raw";
import multi from "./fixtures/alignment-inspection-petri-multi.json?raw";
import reduction from "./fixtures/alignment-inspection-tree-reduction.json?raw";
import emd from "./fixtures/alignment-inspection-emd.json?raw";
import { alignDfg, alignEditDistance, alignProcessTree, alignDecomposed, alignPetriAnti,
  alignPetriMulti, reduceProcessTree, evaluateEmd } from "../src/api";
import { alignmentTables, alignmentCsv, alignmentPage, alignmentJson, alignmentReport, alignmentRequest,
  readAlignmentResponse, type AlignmentKind, type AlignmentEntry, type InspectableAlignment } from "../src/alignment-inspection";
import { parseNativeJson, nativeJson } from "../src/native-json";
import { isDfgAlignmentResult, isEditDistanceResult, isProcessTreeAlignmentResult, isDecomposedAlignmentResult,
  isPetriAntiAlignmentResult, isPetriMultiAlignmentResult, isProcessTreeReductionResult, isEmdResult } from "../src/types";

const entry = (kind: AlignmentKind, raw: string): AlignmentEntry => ({ kind, label: kind, result: parseNativeJson(raw) as InspectableAlignment });
const table = (kind: AlignmentKind, raw: string, id: string) => alignmentTables(entry(kind, raw)).find(t => t.id === id)!;
const reference = new File(["owned"], "reference-東京.csv", { lastModified: 123 });
const cases = [
  ["dfg", dfg, isDfgAlignmentResult, () => alignDfg({ projection: "metrics" })],
  ["dfg", dfgLog, isDfgAlignmentResult, () => alignDfg({ projection: "log" })],
  ["process-tree", search, isProcessTreeAlignmentResult, () => alignProcessTree()],
  ["process-tree", dynamic, isProcessTreeAlignmentResult, () => alignProcessTree({ variant: "DYNAMIC_PROGRAMMING" })],
  ["process-tree", milp, isProcessTreeAlignmentResult, () => alignProcessTree({ variant: "MILP" })],
  ["process-tree", approx, isProcessTreeAlignmentResult, () => alignProcessTree({ variant: "APPROXIMATED_ORIGINAL" })],
  ["decomposed", decomposed, isDecomposedAlignmentResult, () => alignDecomposed("inductive")],
  ["edit", edit, isEditDistanceResult, () => alignEditDistance(reference, { projection: "log", deviation_cost: 17 })],
  ["edit", editAnti, isEditDistanceResult, () => alignEditDistance(reference, { anti: true, projection: "log" })],
  ["anti", anti, isPetriAntiAlignmentResult, () => alignPetriAnti("inductive", { max_states: 2000 })],
  ["multi", multi, isPetriMultiAlignmentResult, () => alignPetriMulti("inductive", { max_states: 2000 })],
  ["reduction", reduction, isProcessTreeReductionResult, () => reduceProcessTree(45)],
  ["emd", emd, isEmdResult, () => evaluateEmd(reference, { emd_variant: "exact" })],
] as const;
afterEach(() => vi.unstubAllGlobals());

describe("complete native alignment inspection", () => {
  it.each(cases)("retains the real %s HTTP response, submitted settings and report", async (kind, raw, guard, request) => {
    const fetch = vi.fn(async (_path: string) => new Response(raw, { headers: { "Content-Type": "application/json" } }));
    vi.stubGlobal("fetch", fetch);
    const result = await request();
    expect(guard(result)).toBe(true);
    expect(alignmentJson(result)).toBe(raw);
    expect(alignmentRequest(result)?.path).toBe(fetch.mock.calls[0][0]);
    const report = JSON.parse(alignmentReport({ kind, label: kind, result }, "owned.csv"));
    expect(report.native_result).toEqual(JSON.parse(raw));
    expect(report.origin).toBe("native_http_response");
    expect(report.dataset).toBe("owned.csv");
  });
  it("retains all sixty source entries and every native move, including the last trace", () => {
    for (const [kind, raw] of [["dfg", dfg], ["edit", edit], ["process-tree", search]] as const) {
      const traces = table(kind, raw, "traces"); const moves = table(kind, raw, "moves");
      expect(traces.rows).toHaveLength(60); expect(traces.rows.at(-1)?.[0]).toBe(59);
      const moveCount = kind === "edit" ? 210 : 180; const lastMove = kind === "edit" ? 3 : 2;
      expect(moves.rows).toHaveLength(moveCount); expect(moves.rows.at(-1)?.slice(0, 2)).toEqual([59, lastMove]);
      expect(alignmentCsv(moves).split("\r\n")).toHaveLength(moveCount + 2);
    }
  });
  it("preserves each decomposed entry and its native count without regrouping", () => {
    const traces = table("decomposed", decomposed, "traces"); const count = traces.columns.indexOf("count");
    expect(traces.rows).toHaveLength(60); expect(traces.rows.reduce((total, row) => total + Number(row[count]), 0)).toBe(60);
    expect(traces.rows.every(row => row[count] === 1)).toBe(true);
    expect(traces.title).toContain("native counts"); expect(table("decomposed", decomposed, "moves").rows).toHaveLength(180);
  });
  it("preserves all projection rows and nested typed attributes", () => {
    expect(table("dfg", dfg, "projected_dfg_edges").rows).toHaveLength(4);
    for (const [kind, raw] of [["dfg", dfgLog], ["edit", edit]] as const) {
      expect(table(kind, raw, "projected_traces").rows).toHaveLength(60);
      const events = table(kind, raw, "projected_events"); expect(events.rows).toHaveLength(180);
      expect(events.columns).toContain("attribute_details");
      expect(alignmentCsv(events)).toContain("@@is_conforming");
    }
  });
  it("retains silent transition positions, objectives and every reduced tree node", () => {
    for (const [kind, raw] of [["anti", anti], ["multi", multi]] as const) {
      const native = JSON.parse(raw); const transitions = table(kind, raw, "transitions");
      expect(transitions.rows.map(row => row[1])).toEqual(native.transitions);
      expect(table(kind, raw, "summary").rows.some(row => row[0] === "objective")).toBe(true);
    }
    const nodes = table("reduction", reduction, "process_tree");
    expect(nodes.rows).toHaveLength(6); expect(nodes.rows.map(row => row[0])).toContain("root.1.1");
  });
  it("keeps native number spelling in raw JSON, report and CSV beyond JavaScript integer precision", async () => {
    const raw = dfg.replace('"cost": 0', '"cost": 9007199254740993').replace('"internal_cost": 0', '"internal_cost": -0');
    const result = await readAlignmentResponse(new Response(raw), { path: "/api/align-dfg" }) as InspectableAlignment;
    const record = { kind: "dfg" as const, label: "DFG", result };
    expect(alignmentJson(result)).toBe(raw); expect(alignmentReport(record, "owned.csv")).toContain("9007199254740993");
    expect(alignmentCsv(alignmentTables(record).find(t => t.id === "traces")!)).toContain('"9007199254740993","-0"');
  });
  it("preserves source/target variants and reference identity in edit reports", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(edit)));
    const result = await alignEditDistance(reference, { anti: false, deviation_cost: 17 });
    expect(alignmentRequest(result)).toMatchObject({ reference: { name: reference.name, size: 5, last_modified: 123 }, fields: [["anti", "false"], ["deviation_cost", "17"]] });
    const traces = alignmentTables({ kind: "edit", label: "edit", result }).find(t => t.id === "traces")!;
    expect(traces.columns).toContain("source_variant"); expect(traces.columns).toContain("target_variant");
  });
  it("keeps the request snapshot independent of later control changes", async () => {
    const request = { path: "/api/align-dfg?projection=metrics", fields: [["x", "original"]] as Array<[string, string]> };
    const result = await readAlignmentResponse(new Response(dfg), request) as InspectableAlignment;
    request.fields[0][1] = "changed";
    expect(alignmentRequest(result)?.fields?.[0][1]).toBe("original");
  });
  it("exposes empty and uncomputed entries without dropping their index", () => {
    const item = entry("dfg", '{"family":"DFG","variant":"CLASSIC","unique_variants":1,"alignments":[{"computed":false}]}');
    expect(isDfgAlignmentResult(item.result)).toBe(true);
    expect(alignmentTables(item).find(t => t.id === "traces")?.rows).toEqual([[0, false]]);
    expect(alignmentTables(item).find(t => t.id === "moves")?.rows).toEqual([]);
    expect(alignmentPage([], 0)).toEqual([]);
  });
  it("retains all native schema fields and quotes delimiter-containing cells", () => {
    const item = entry("dfg", dfg); const result = item.result as unknown as { alignments: Array<Record<string, unknown>> };
    result.alignments[59].future_diagnostic = 'comma,quote"\n東京';
    const traces = alignmentTables(item).find(t => t.id === "traces")!;
    expect(traces.columns).toContain("future_diagnostic");
    expect(alignmentCsv(traces)).toContain('"comma,quote""\n東京"');
  });
  it("paginates every row exactly once across the 25-row boundary", () => {
    const traces = table("process-tree", search, "traces").rows;
    expect([0, 1, 2].flatMap(page => alignmentPage(traces, page))).toEqual(traces);
    expect(alignmentPage(traces, 2)).toHaveLength(10);
  });
  it("marks embedded native results honestly and retains their complete data", () => {
    const item = entry("decomposed", decomposed); const report = JSON.parse(alignmentReport(item, "saved.json"));
    expect(report.origin).toBe("embedded_result"); expect(report.request).toBeNull();
    expect(report.native_result).toEqual(JSON.parse(decomposed));
    expect(nativeJson(item.result)).toContain('"trace_count":60');
  });
  it("keeps native errors and malformed responses visible", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"error":"DFG alignment state limit exceeded"}', { status: 400 })));
    await expect(alignDfg({ max_states: 1 })).rejects.toThrow("DFG alignment state limit exceeded");
    vi.stubGlobal("fetch", vi.fn(async () => new Response("invalid json")));
    await expect(alignProcessTree()).rejects.toThrow("invalid process-tree alignment");
  });
});
