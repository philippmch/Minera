import type { ModelEvaluation, PetriAnalysisResult, PublicEvaluationValue } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { hasNativeResponse, nativeResponseRequest } from "./native-response";

export interface DiagnosticEntry {
  id: number;
  kind: "evaluation" | "petri";
  label: string;
  dataset: string;
  model: string;
  model_generation: number;
  result: ModelEvaluation | PetriAnalysisResult;
}
export interface DiagnosticTable {
  id: string; title: string; columns: string[]; rows: unknown[][];
  records?: unknown[];
}
const record = (value: unknown): value is Record<string, unknown> =>
  value !== null && typeof value === "object" && !Array.isArray(value);
const pointer = (key: string) => key.replace(/~/g, "~0").replace(/\//g, "~1");
export const diagnosticCell = (value: unknown): string => value === undefined ? "" :
  typeof value === "string" ? value : typeof value === "object" ? nativeJson(value) : String(value);
export function diagnosticCsv(table: DiagnosticTable): string {
  const quote = (value: unknown) => '"' + diagnosticCell(value).replace(/"/g, '""') + '"';
  return [table.columns, ...table.rows].map(row => row.map(quote).join(",")).join("\r\n") + "\r\n";
}
export function diagnosticPage<T>(rows: T[], page: number, size = 25): T[] {
  return rows.slice(Math.max(0, page) * size, (Math.max(0, page) + 1) * size);
}
export function parseDiagnosticWeight(text: string): number | undefined {
  const value = text.trim();
  if (!value) return undefined;
  if (/^[+]?inf(?:inity)?$/i.test(value)) return Infinity;
  if (/^-inf(?:inity)?$/i.test(value)) return -Infinity;
  if (/^[+-]?nan$/i.test(value)) return NaN;
  if (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(value)) {
    throw new Error("Weights must be numbers, NaN, inf or -inf.");
  }
  return Number(value);
}

/** The ordered public value is inspected as tags and exact hex, never rescaled. */
export function publicEvaluationTable(value: PublicEvaluationValue): DiagnosticTable {
  const rows: unknown[][] = [];
  const visit = (node: PublicEvaluationValue, path: string, index?: number, key?: {type:string;value:string}) => {
    rows.push([path, index, key?.type, key?.value, node.type,
      node.type === "float" ? node.hex : undefined,
      node.type === "int" ? nativeCell(node, "value") : undefined]);
    if (node.type === "dict") node.items.forEach(([name, child], i) => visit(child, `${path}/items/${i}`, i, name));
  };
  visit(value, "/public_result");
  return { id: "public_value", title: "Typed public result (ordered entries)",
    columns: ["path", "entry_index", "key_type", "key", "value_type", "float_hex", "int_value"], rows };
}

/** Every property is retained; nested array cells include their complete JSON. */
export function nativeObjectTables(result: object): DiagnosticTable[] {
  const tables: DiagnosticTable[] = [];
  const visit = (value: unknown, path: string) => {
    if (record(value)) {
      const keys = Object.keys(value);
      tables.push({ id: path || "summary", title: path || "Native summary", columns: ["field", "type", "value"],
        rows: keys.map(key => [key, value[key] === null ? "null" : Array.isArray(value[key]) ? "array" : typeof value[key], nativeCell(value,key)]),
        records: keys.map(key => {
          const cell = nativeCell(value,key);
          const encoded = typeof value[key] === "number" && typeof cell === "string" ? cell : nativeJson(value[key]);
          return parseNativeJson(`{"field":${JSON.stringify(key)},"value":${encoded}}`);
        }) });
      for (const key of keys) if (value[key] !== null && typeof value[key] === "object") visit(value[key], `${path}/${pointer(key)}`);
    } else if (Array.isArray(value)) {
      const columns = [...new Set(value.flatMap(row => record(row) ? Object.keys(row) : ["value"]))];
      tables.push({ id: path, title: path, columns: ["row_index", ...columns], records: value,
        rows: value.map((row, i) => [i, ...columns.map(key => record(row) ? nativeCell(row,key) : key === "value" ? nativeCell(value,i) : undefined)]) });
    }
  };
  visit(result, "");
  return tables;
}
export function diagnosticTables(entry: DiagnosticEntry): DiagnosticTable[] {
  const tables = nativeObjectTables(entry.result);
  const publicResult = entry.kind === "evaluation" ? (entry.result as ModelEvaluation).public_result : undefined;
  if (publicResult) tables.push(publicEvaluationTable(publicResult));
  const request = nativeResponseRequest(entry.result);
  if (request) tables.push({ id: "request", title: "Submitted request", columns: ["field", "value"], rows: [
    ["method", "POST"], ["path", request.path.split("?")[0]],
    ...new URLSearchParams(request.path.split("?")[1] ?? ""), ...(request.fields ?? []),
  ] });
  return tables;
}
function savedEntry(entry: DiagnosticEntry) {
  return { id: entry.id, kind: entry.kind, label: entry.label, dataset: entry.dataset, model: entry.model,
    model_generation: entry.model_generation,
    origin: hasNativeResponse(entry.result) ? "native_http_response" : "embedded_result",
    request: nativeResponseRequest(entry.result) ?? null, native_result: entry.result };
}
export function diagnosticReport(entry: DiagnosticEntry): string {
  return nativeJson({ family: "MODEL_DIAGNOSTICS_INSPECTION", ...savedEntry(entry) }, 2);
}
export function diagnosticComparisonReport(entries: DiagnosticEntry[]): string {
  return nativeJson({ family: "MODEL_DIAGNOSTICS_COMPARISON", entries: entries.map(savedEntry) }, 2);
}
export function diagnosticComparison(entries: DiagnosticEntry[]): DiagnosticTable {
  const rows = entries.map(entry => {
    const values: Record<string,unknown> = { id: entry.id, kind: entry.kind, dataset: entry.dataset,
      model: entry.model, model_generation: entry.model_generation, label: entry.label };
    const walk = (obj: unknown, prefix: string) => {
      if (!record(obj)) return;
      for (const [key, value] of Object.entries(obj)) {
        if (key === "public_result" || Array.isArray(value)) continue;
        const path = prefix ? prefix + "." + key : key;
        if (record(value)) walk(value, path); else values[path] = nativeCell(obj,key);
      }
    };
    walk(entry.result, "");
    return values;
  });
  const columns = [...new Set(rows.flatMap(row => Object.keys(row)))];
  return { id: "comparison", title: "Saved diagnostic comparisons", columns,
    rows: rows.map(row => columns.map(key => row[key])), records: entries.map(savedEntry) };
}
