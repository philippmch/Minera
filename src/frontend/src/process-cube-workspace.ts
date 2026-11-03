import { processCubeQuery, type ProcessCubeOptions } from "./api";
import { backendFetch } from "./backend-fetch";
import { isProcessCubeTable, isProcessCubeResult, type ProcessCubeTable, type ProcessCubeResult } from "./types";
import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { type OcelPreparedTable } from "./ocel-preparation";
import { isTransformationRun, type TransformationRun } from "./transformation-workspace";

export const CUBE_AGGREGATIONS = ["mean", "sum", "min", "max", "count", "median", "first", "last"] as const;
export interface CubeAxisForm { mode: "single" | "composite"; name: string; names: string; divisions: string; bins: string; }
export interface CubeForm { x: CubeAxisForm; y: CubeAxisForm; value: string; aggregation: typeof CUBE_AGGREGATIONS[number]; }
export function defaultCubeForm(): CubeForm {
  const axis = (): CubeAxisForm => ({ mode: "single", name: "", names: "[]", divisions: "4", bins: "" });
  return { x: axis(), y: axis(), value: "", aggregation: "mean" };
}
export function cubeOptions(form: CubeForm): ProcessCubeOptions {
  const options: ProcessCubeOptions = { agg_col: form.value, aggregation: form.aggregation };
  for (const axis of ["x", "y"] as const) {
    const value = form[axis];
    if (value.mode === "single") options[axis === "x" ? "x_col" : "y_col"] = value.name;
    else {
      let names: unknown;
      try { names = JSON.parse(value.names); } catch { throw new Error(axis.toUpperCase() + " dimensions must be a JSON array of names."); }
      if (!Array.isArray(names) || !names.every(name => typeof name === "string"))
        throw new Error(axis.toUpperCase() + " dimensions must be a JSON array of names.");
      options[axis === "x" ? "x_cols" : "y_cols"] = names;
    }
    if (value.divisions.trim()) options[axis === "x" ? "max_divisions_x" : "max_divisions_y"] = Number(value.divisions);
    if (value.bins.trim()) {
      let edges: unknown;
      try { edges = JSON.parse(value.bins); } catch { throw new Error(axis.toUpperCase() + " bin edges must be a JSON array of numbers."); }
      if (!Array.isArray(edges) || !edges.every(edge => typeof edge === "number"))
        throw new Error(axis.toUpperCase() + " bin edges must be a JSON array of numbers.");
      options[axis === "x" ? "x_bins" : "y_bins"] = edges;
    }
  }
  processCubeQuery(options);
  return options;
}
export interface CubeRun {
  result: ProcessCubeResult; input: ProcessCubeTable; raw: string; report_json: string; query: string;
  source: { name: string; size: number; sha256: string };
  tables: Record<string, OcelPreparedTable>;
}
export function cubeTables(result: ProcessCubeResult, input?: ProcessCubeTable): Record<string, OcelPreparedTable> {
  const width = result.x_bins.length, height = result.y_bins.length;
  const positions = (index: number) => [Math.floor(index / height), index % height] as const;
  const cells: OcelPreparedTable = {
    columns: ["x_index", "x_label", "y_index", "y_label", "aggregation", "value", "distinct_case_ids"], count: width * height,
    rowAt: index => { const [x, y] = positions(index); return [x, result.x_bins[x].label, y, result.y_bins[y].label, result.aggregation, nativeCell(result.cells[x], y), result.case_ids[x][y].length]; },
  };
  const ends: number[] = []; let members = 0;
  for (const column of result.case_ids) for (const ids of column) { members += ids.length; ends.push(members); }
  const memberships: OcelPreparedTable = {
    columns: ["x_index", "x_label", "y_index", "y_label", "member_position", "case_id"], count: members,
    rowAt: index => {
      let left = 0, right = ends.length;
      while (left < right) { const middle = (left + right) >>> 1; if (ends[middle] <= index) left = middle + 1; else right = middle; }
      const [x, y] = positions(left), position = index - (left ? ends[left - 1] : 0);
      return [x, result.x_bins[x].label, y, result.y_bins[y].label, position, result.case_ids[x][y][position]];
    },
  };
  const tables: Record<string, OcelPreparedTable> = { cells, memberships };
  for (const axis of ["x", "y"] as const) {
    const bins = result[axis === "x" ? "x_bins" : "y_bins"], edges = result[axis === "x" ? "x_edges" : "y_edges"];
    tables[axis + "_bins"] = { columns: ["bin_index", "label", "numeric", "lower", "upper"], count: bins.length,
      rowAt: index => { const bin = bins[index]; return [index, bin.label, bin.numeric, nativeCell(bin, "lower"), nativeCell(bin, "upper")]; } };
    tables[axis + "_edges"] = { columns: ["edge_index", "value"], count: edges.length, rowAt: index => [index, nativeCell(edges, index)] };
  }
  if (input) {
    const columns = [...Object.entries(input.columns), ...Object.entries(input.categorical_columns ?? {})];
    tables.input_columns = { columns: ["name", "kind", "rows"], count: columns.length, rowAt: index => {
      const [name, values] = columns[index]; return [name, Object.prototype.hasOwnProperty.call(input.categorical_columns ?? {}, name) || typeof values[0] === "string" ? "categorical" : "numeric", values.length];
    } };
    tables.input_rows = { columns: ["row_position", "case_id", ...columns.map(([name]) => name)], count: input.case_ids.length,
      rowAt: index => [index, input.case_ids[index], ...columns.map(([, values]) => nativeCell(values, index))] };
  }
  return tables;
}
export function cubeCellMembers(result: ProcessCubeResult, x: number, y: number): OcelPreparedTable {
  const ids = result.case_ids[x][y];
  return { columns: ["x_index", "x_label", "y_index", "y_label", "member_position", "case_id"], count: ids.length,
    rowAt: index => [x, result.x_bins[x].label, y, result.y_bins[y].label, index, ids[index]] };
}
export async function requestCubeFile(file: File, form: CubeForm, signal?: AbortSignal): Promise<CubeRun> {
  signal?.throwIfAborted();
  const options = cubeOptions(form), query = processCubeQuery(options), bytes = await file.arrayBuffer();
  const sha256 = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), value => value.toString(16).padStart(2, "0")).join("");
  signal?.throwIfAborted();
  const response = await backendFetch("/api/process-cube?" + query, { method: "POST", headers: { "Content-Type": "application/json" }, body: bytes, ...(signal ? { signal } : {}) });
  const raw = await response.text(); signal?.throwIfAborted();
  let value: unknown;
  try { value = parseNativeJson(raw); } catch { throw new Error("The backend returned invalid process-cube JSON."); }
  if (value && typeof value === "object" && "error" in value) throw new Error(String((value as { error: unknown }).error));
  if (!response.ok) throw new Error("Process cube returned " + response.status + ".");
  if (!isProcessCubeResult(value) || value.aggregation !== form.aggregation) throw new Error("The backend returned an invalid or different process cube.");
  const text = new TextDecoder().decode(bytes); let input: unknown;
  try { input = parseNativeJson(text); } catch { throw new Error("The cube input could not be inspected."); }
  if (!isProcessCubeTable(input)) throw new Error("The cube input must contain rectangular finite numeric or categorical columns.");
  const ids = new Set(input.case_ids);
  if (!value.case_ids.every(column => column.every(cell => new Set(cell).size === cell.length && cell.every(id => ids.has(id)))))
    throw new Error("The backend returned invalid process-cube membership.");
  const source = { name: file.name, size: bytes.byteLength, sha256 };
  return { result: value, input, raw, query, source, tables: cubeTables(value, input),
    report_json: '{"family":"PROCESS_CUBE_REPORT","source":' + JSON.stringify(source) + ',"query":' + JSON.stringify(query) + ',"input":' + text + ',"response":' + raw + "}" };
}
/** Preserve the case feature matrix's native numeric tokens when transposing it. */
export function cubeFeatureFile(run: TransformationRun): File {
  const value = parseNativeJson(run.data_json);
  if (!isTransformationRun(value) || value.source_id !== run.source_id || value.result.family !== "LOG_TO_FEATURES" || value.result.variant !== "trace_based")
    throw new Error("Process cubes require a valid feature result with one row per case.");
  const result = value.result, rows = result.trace_rows!;
  const names = result.feature_names;
  const identifier = result.case_ids && names[0] === "@@case_id_column" ? 0 : -1;
  const selected = names.map((name, index) => ({ name, index })).filter(({ index }) => index !== identifier);
  if (!selected.length || selected.some(({ name }) => !name) || new Set(selected.map(({ name }) => name)).size !== selected.length)
    throw new Error("Cube feature dimensions must have non-empty unique names.");
  if (value.cases.some(trace => !trace.case_id)) throw new Error("Cube case IDs must not be empty.");
  const columns = selected.map(({ name, index }) => {
    if (!rows.every(row => typeof row[index] === "number" && Number.isFinite(row[index])))
      throw new Error("Cube features must be finite numeric values: " + name);
    return JSON.stringify(name) + ":[" + rows.map(row => String(nativeCell(row, index))).join(",") + "]";
  });
  const provenance = { source_id: value.source_id, case_positions: value.cases.map(trace => trace.index), omitted_identifier_column: identifier >= 0 ? names[identifier] : null };
  const text = '{"case_ids":' + JSON.stringify(value.cases.map(trace => trace.case_id)) + ',"columns":{' + columns.join(",") + '},"minera_feature_source":' + JSON.stringify(provenance) + "}";
  return new File([text], "minera-case-features-cube.json", { type: "application/json" });
}
export function cubeDefaultColumns(input: ProcessCubeTable): Pick<CubeForm, "x" | "y" | "value"> {
  const form = defaultCubeForm(), names = [...Object.keys(input.columns), ...Object.keys(input.categorical_columns ?? {})];
  form.x.name = names[0] ?? ""; form.y.name = names[1] ?? names[0] ?? "";
  form.value = Object.keys(input.columns).find(name => input.columns[name].every(value => typeof value === "number")) ?? "";
  return { x: form.x, y: form.y, value: form.value };
}
export function cubeValueText(result: ProcessCubeResult, x: number, y: number): string { return String(nativeCell(result.cells[x], y)); }
export function cubeColourScale(result: ProcessCubeResult): (value: number | null) => string {
  let min = Infinity, max = -Infinity;
  for (const column of result.cells) for (const value of column) if (value !== null) { min = Math.min(min, value); max = Math.max(max, value); }
  return value => {
    if (value === null) return "transparent";
    const range = max - min;
    const fraction = min === max ? .5 : Number.isFinite(range) ? (value - min) / range : (value / 2 - min / 2) / (max / 2 - min / 2);
    return "rgba(77, 154, 126, " + (.08 + .54 * Math.max(0, Math.min(1, fraction))) + ")";
  };
}
export function cubeInputText(input: ProcessCubeTable): string { return nativeJson(input, 2); }
