import type { DfgAlignmentResult, EditDistanceResult, ProcessTreeAlignmentResult,
  DecomposedAlignmentResult, PetriAntiAlignmentResult, PetriMultiAlignmentResult,
  ProcessTreeReductionResult, EmdResult } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";

export type AlignmentKind = "dfg" | "edit" | "process-tree" | "decomposed" | "anti" | "multi" | "reduction" | "emd";
export type InspectableAlignment = DfgAlignmentResult | EditDistanceResult | ProcessTreeAlignmentResult |
  DecomposedAlignmentResult | PetriAntiAlignmentResult | PetriMultiAlignmentResult | ProcessTreeReductionResult | EmdResult;
export interface AlignmentEntry { kind: AlignmentKind; label: string; result: InspectableAlignment; }
export interface AlignmentRequest {
  path: string;
  fields?: Array<[string, string]>;
  reference?: { name: string; size: number; last_modified: number };
}
interface Capture { raw: string; request: AlignmentRequest; }
const captures = new WeakMap<object, Capture>();

/** Retain the actual response and submitted settings, including native numeric tokens. */
export async function readAlignmentResponse(response: Response, request: AlignmentRequest): Promise<unknown> {
  const raw = await response.text().catch(() => "");
  try {
    const value = parseNativeJson(raw);
    if (value && typeof value === "object") captures.set(value, { raw, request: structuredClone(request) });
    return value;
  } catch { return null; }
}
export function alignmentRequest(result: InspectableAlignment): AlignmentRequest | undefined { return captures.get(result)?.request; }
export function alignmentJson(result: InspectableAlignment): string { return captures.get(result)?.raw ?? nativeJson(result, 2); }
export function alignmentReport(entry: AlignmentEntry, dataset: string): string {
  const capture = captures.get(entry.result);
  return nativeJson({ family: "ALIGNMENT_INSPECTION", kind: entry.kind, dataset,
    origin: capture ? "native_http_response" : "embedded_result",
    request: capture?.request ?? null, native_result: entry.result }, 2);
}
export function alignmentCell(value: unknown): string {
  if (value === undefined) return "";
  return typeof value === "object" ? nativeJson(value) : typeof value === "string" ? value : String(value);
}
export interface AlignmentTable { id: string; title: string; columns: string[]; rows: unknown[][]; }
const record = (v: unknown): v is Record<string, unknown> => !!v && typeof v === "object" && !Array.isArray(v);
const cell = (row: object, key: string | number): unknown => {
  const value = nativeCell(row, key);
  return value && typeof value === "object" ? nativeJson(value) : value;
};
function rowsTable(id: string, title: string, rows: unknown[], prefix: string[] = [], identities: unknown[][] = [], omit: string[] = []): AlignmentTable {
  const columns = Array.from(new Set(rows.flatMap(row => record(row) ? Object.keys(row).filter(k => !omit.includes(k)) : ["value"])));
  return { id, title, columns: [...prefix, ...columns], rows: rows.map((row, i) => [
    ...(identities[i] ?? []), ...columns.map(key => record(row) ? cell(row, key) : key === "value" ? cell(rows, i) : undefined),
  ]) };
}
function properties(id: string, title: string, value: Record<string, unknown>, omit: string[] = []): AlignmentTable {
  return { id, title, columns: ["field", "value"], rows: Object.keys(value).filter(key => !omit.includes(key)).map(key => [key, cell(value, key)]) };
}
export function alignmentTraceRows(result: InspectableAlignment): Record<string, unknown>[] {
  const root = result as unknown as Record<string, unknown>;
  return (Array.isArray(root.alignments) ? root.alignments : Array.isArray(root.traces) ? root.traces : []).filter(record);
}
export function alignmentMoves(trace: Record<string, unknown>): unknown[] {
  return Array.isArray(trace.moves) ? trace.moves : Array.isArray(trace.alignment) ? trace.alignment : [];
}

/** Tables retain all fields. Nested native records remain complete JSON cells. */
export function alignmentTables(entry: AlignmentEntry): AlignmentTable[] {
  const root = entry.result as unknown as Record<string, unknown>;
  const tables: AlignmentTable[] = [properties("summary", "Native summary", root,
    ["alignments", "traces", "diagnostics", "projected_log", "projected_dfg", "process_tree", "activities", "transitions"])];
  const request = captures.get(entry.result)?.request;
  if (request) {
    const params = [...new URLSearchParams(request.path.split("?")[1] ?? ""), ...(request.fields ?? [])];
    tables.push({ id: "request", title: "Submitted request", columns: ["field", "value"], rows: [
      ["method", "POST"], ["path", request.path.split("?")[0]], ...params,
      ...(request.reference ? Object.entries(request.reference).map(([key, value]) => ["reference_" + key, value]) : []),
    ] });
  }
  if (record(root.diagnostics)) tables.push(properties("diagnostics", "Search diagnostics", root.diagnostics));
  const traces = alignmentTraceRows(entry.result);
  if (Array.isArray(root.alignments) || Array.isArray(root.traces)) {
    tables.push(rowsTable("traces", entry.kind === "decomposed" ? "Decomposed entries (native counts)" : "Alignment traces", traces,
      ["entry_index"], traces.map((_, i) => [i]), ["moves", "alignment"]));
    const moves = traces.flatMap(alignmentMoves);
    const identities = traces.flatMap((trace, i) => alignmentMoves(trace).map((_, j) => [i, j]));
    tables.push(rowsTable("moves", "All alignment moves", moves, ["entry_index", "move_index"], identities));
  }
  if (record(root.projected_dfg)) {
    for (const [key, value] of Object.entries(root.projected_dfg)) if (Array.isArray(value))
      tables.push(rowsTable("projected_dfg_" + key, "Projected DFG " + key, value, ["row_index"], value.map((_, i) => [i])));
  }
  if (record(root.projected_log)) {
    tables.push(properties("projected_log_summary", "Projected log metadata", root.projected_log, ["traces"]));
    if (Array.isArray(root.projected_log.traces)) {
      const projected = root.projected_log.traces;
      tables.push(rowsTable("projected_traces", "Projected traces", projected, ["trace_index"], projected.map((_, i) => [i]), ["events"]));
      const events = projected.flatMap(trace => record(trace) && Array.isArray(trace.events) ? trace.events : []);
      const identities = projected.flatMap((trace, i) => record(trace) && Array.isArray(trace.events) ? trace.events.map((_, j) => [i, j]) : []);
      tables.push(rowsTable("projected_events", "Projected events (native attributes)", events, ["trace_index", "event_index"], identities));
    }
  }
  for (const name of ["activities", "transitions"]) if (Array.isArray(root[name]))
    tables.push(rowsTable(name, name === "activities" ? "Visible activity sequence" : "Transition sequence (includes silent moves)", root[name], ["sequence_index"], root[name].map((_, i) => [i])));
  if (record(root.process_tree)) {
    const nodes: unknown[] = []; const identities: unknown[][] = [];
    const visit = (node: Record<string, unknown>, path: string) => {
      nodes.push(node); identities.push([path]);
      if (Array.isArray(node.children)) node.children.forEach((child, i) => { if (record(child)) visit(child, path + "." + i); });
    };
    visit(root.process_tree, "root");
    tables.push(rowsTable("process_tree", "Reduced process-tree nodes", nodes, ["node_path"], identities, ["children"]));
  }
  return tables;
}
export function alignmentCsv(table: AlignmentTable): string {
  const encode = (v: unknown) => '"' + alignmentCell(v).replaceAll('"', '""') + '"';
  return [table.columns, ...table.rows].map(row => row.map(encode).join(",")).join("\r\n") + "\r\n";
}
export function alignmentPage<T>(values: readonly T[], page: number, size = 25): T[] { return values.slice(page * size, (page + 1) * size); }
