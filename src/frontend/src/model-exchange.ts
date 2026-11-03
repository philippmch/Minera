import { nativeCell, nativeJson, parseNativeJson } from "./native-json";
import { backendFetch } from "./backend-fetch";
import { type OcelPreparedTable } from "./ocel-preparation";
import { type ModelGraph } from "./model-graph";

export const MODEL_FORMATS = ["pnml", "bpmn", "ptml", "dfg"] as const;
export type ModelFormat = typeof MODEL_FORMATS[number];
export const PNML_IMPORT_FLAGS = [
  ["pnml_infer_initial_marking", "Infer missing initial marking"],
  ["pnml_infer_final_marking", "Infer missing final marking"],
  ["pnml_import_stochastic_map", "Import stochastic map"],
  ["pnml_require_workflow_boundaries", "Require workflow boundaries"],
  ["pnml_public_marking_semantics", "Use public PNML marking semantics"],
] as const;
export interface ModelExchangeForm {
  format: ModelFormat;
  output: ModelFormat | "inspect";
  treeVariant: "standard" | "transition_bordered";
  dfgVariant: "activity_defines_place" | "invisibles_no_duplicates";
  startActivity: string;
  endActivity: string;
  flags: Record<typeof PNML_IMPORT_FLAGS[number][0], boolean>;
}
export function defaultModelExchange(): ModelExchangeForm {
  return { format: "pnml", output: "inspect", treeVariant: "standard",
    dfgVariant: "activity_defines_place", startActivity: "", endActivity: "",
    flags: { pnml_infer_initial_marking: true, pnml_infer_final_marking: true,
      pnml_import_stochastic_map: false, pnml_require_workflow_boundaries: true,
      pnml_public_marking_semantics: false } };
}
export function modelExchangeQuery(form: ModelExchangeForm): string {
  const query = new URLSearchParams({ format: form.format, output: form.output });
  if (form.format === "pnml")
    for (const [key] of PNML_IMPORT_FLAGS) query.set(key, String(form.flags[key]));
  if (form.format === "ptml") query.set("tree_variant", form.treeVariant);
  if (form.format === "dfg") {
    query.set("dfg_variant", form.dfgVariant);
    if (form.dfgVariant === "invisibles_no_duplicates") {
      if (form.startActivity) query.set("artificial_start_activity", form.startActivity);
      if (form.endActivity) query.set("artificial_end_activity", form.endActivity);
    }
  }
  return query.toString();
}
export interface ExchangeTreeNode { operator: string; activity?: number; children?: ExchangeTreeNode[]; }
export interface ExchangeTree { family: "PROCESS_TREE"; activities: string[]; root: ExchangeTreeNode; }
export interface ExchangeTransition {
  activity: string | null; inputs: number[]; outputs: number[];
  reset_inputs: number[]; inhibitor_inputs: number[];
  input_weights?: number[]; output_weights?: number[];
}
export interface ExchangeNet {
  family: "PNML_DOCUMENT"; variant: "pnml"; place_count: number; transition_count: number;
  source: number; sink: number; initial_marking: number[]; final_marking: number[];
  place_ids?: string[]; transition_ids?: string[];
  initial_marking_keys?: boolean[]; final_marking_keys?: boolean[];
  transitions: ExchangeTransition[]; stochastic_map: unknown;
}
export interface ExchangeArc { id: string; place: number; transition: number; input: boolean; type: "normal" | "reset" | "inhibitor"; weight: number; }
export interface ExchangeDfg {
  family: "DFG_DOCUMENT"; graph: { source: string; target: string; count: number }[];
  start_activities: { activity: string; count: number }[];
  end_activities: { activity: string; count: number }[];
}
export interface ModelExchangeResult {
  family: "MODEL_EXCHANGE"; format: ModelFormat; output_format: ModelFormat | "inspect";
  data: ExchangeNet | ExchangeTree | ExchangeDfg; net: ExchangeNet | null;
  tree: ExchangeTree | null; arcs: ExchangeArc[]; canonical: string;
  pnml: string | null; exported: string | null; conversion_error: string | null;
}
const record = (value: unknown): value is Record<string, unknown> =>
  !!value && typeof value === "object" && !Array.isArray(value);
const index = (value: unknown): value is number => Number.isSafeInteger(value) && (value as number) >= 0;
/** Integer values can exceed double precision; validate their retained JSON tokens. */
function integerToken(holder: object, key: string | number, signed = false): boolean {
  const value = (holder as Record<string, unknown>)[key];
  return typeof value === "number" && (signed ? /^-?(?:0|[1-9]\d*)$/ : /^(?:0|[1-9]\d*)$/).test(String(nativeCell(holder, key)));
}
function integerVector(value: unknown, size?: number): value is number[] {
  return Array.isArray(value) && (size === undefined || value.length === size) &&
    value.every((_, i) => integerToken(value, i));
}
function validNet(value: unknown): value is ExchangeNet {
  if (!record(value) || value.family !== "PNML_DOCUMENT" || value.variant !== "pnml" ||
      !index(value.place_count) || !index(value.transition_count) ||
      !integerToken(value, "source") || !integerToken(value, "sink") ||
      !integerVector(value.initial_marking, value.place_count) || !integerVector(value.final_marking, value.place_count) ||
      !Array.isArray(value.transitions) || value.transitions.length !== value.transition_count) return false;
  for (const key of ["place_ids", "transition_ids", "initial_marking_keys", "final_marking_keys"] as const) {
    const items = value[key]; if (items === undefined) continue;
    const size = key === "transition_ids" ? value.transition_count : value.place_count;
    if (!Array.isArray(items) || items.length !== size ||
        !items.every(item => typeof item === (key.endsWith("_keys") ? "boolean" : "string"))) return false;
  }
  return value.transitions.every(t => record(t) && (t.activity === null || typeof t.activity === "string") &&
    ["inputs", "outputs", "reset_inputs", "inhibitor_inputs"].every(key =>
      Array.isArray(t[key]) && (t[key] as unknown[]).every(v => index(v) && v < (value.place_count as number))) &&
    ["input_weights", "output_weights"].every(key => t[key] === undefined ||
      (Array.isArray(t[key]) && (t[key] as unknown[]).length === (t[key === "input_weights" ? "inputs" : "outputs"] as unknown[]).length &&
        (t[key] as unknown[]).every((_, i) => integerToken(t[key] as object, i, true)))));
}
function validTree(value: unknown): value is ExchangeTree {
  if (!record(value) || value.family !== "PROCESS_TREE" || !Array.isArray(value.activities) ||
      !value.activities.every(v => typeof v === "string")) return false;
  const activities = value.activities;
  const node = (v: unknown): boolean => {
    if (!record(v) || !["activity", "tau", "sequence", "exclusive", "inclusive_or", "parallel", "interleaving", "loop"].includes(String(v.operator))) return false;
    if (v.operator === "activity") return index(v.activity) && v.activity < activities.length && v.children === undefined;
    if (v.operator === "tau") return v.activity === undefined && v.children === undefined;
    return v.activity === undefined && Array.isArray(v.children) && v.children.length > 0 && v.children.every(node);
  };
  return node(value.root);
}
function validDfg(value: unknown): value is ExchangeDfg {
  return record(value) && value.family === "DFG_DOCUMENT" && Array.isArray(value.graph) &&
    value.graph.every(v => record(v) && typeof v.source === "string" && typeof v.target === "string" && integerToken(v, "count")) &&
    ["start_activities", "end_activities"].every(key => Array.isArray(value[key]) &&
      (value[key] as unknown[]).every(v => record(v) && typeof v.activity === "string" && integerToken(v, "count")));
}
export function validModelExchange(value: unknown, form: ModelExchangeForm): value is ModelExchangeResult {
  if (!record(value) || value.family !== "MODEL_EXCHANGE" || value.format !== form.format || value.output_format !== form.output ||
      typeof value.canonical !== "string" || (value.pnml !== null && typeof value.pnml !== "string") ||
      (value.exported !== null && typeof value.exported !== "string") ||
      (value.conversion_error !== null && typeof value.conversion_error !== "string") ||
      (value.net !== null && !validNet(value.net)) || (value.tree !== null && !validTree(value.tree)) ||
      !Array.isArray(value.arcs)) return false;
  if (form.output === "inspect" ? value.exported !== null || value.conversion_error !== null
      : value.exported === null ? !value.conversion_error : value.conversion_error !== null) return false;
  if ((value.net === null) !== (value.pnml === null)) return false;
  if (form.format === "ptml" && !validTree(value.data)) return false;
  if (form.format === "dfg" && !validDfg(value.data)) return false;
  if ((form.format === "pnml" || form.format === "bpmn") && !validNet(value.data)) return false;
  const net = value.net as ExchangeNet | null;
  return value.arcs.every(arc => record(arc) && !!net && typeof arc.id === "string" &&
    index(arc.place) && arc.place < net.place_count && index(arc.transition) && arc.transition < net.transition_count &&
    typeof arc.input === "boolean" && ["normal", "reset", "inhibitor"].includes(String(arc.type)) && integerToken(arc, "weight", true));
}
const rows = (columns: string[], values: unknown[][]): OcelPreparedTable =>
  ({ columns, count: values.length, rowAt: i => values[i] });
export function exchangeTreeRows(tree: ExchangeTree) {
  const values: { id: string; parent: string | null; child: number | null; node: ExchangeTreeNode }[] = [];
  const visit = (node: ExchangeTreeNode, id: string, parent: string | null, child: number | null) => {
    values.push({ id, parent, child, node });
    node.children?.forEach((v, i) => visit(v, id + "/" + i, id, i));
  };
  visit(tree.root, "root", null, null); return values;
}
export function modelExchangeTables(result: ModelExchangeResult): Record<string, OcelPreparedTable> {
  const tables: Record<string, OcelPreparedTable> = {};
  if (result.data.family === "DFG_DOCUMENT") {
    const dfg = result.data;
    tables.dfg_edges = rows(["source", "target", "count"], dfg.graph.map(v => [v.source, v.target, nativeCell(v, "count")]));
    for (const key of ["start_activities", "end_activities"] as const)
      tables[key] = rows(["activity", "count"], dfg[key].map(v => [v.activity, nativeCell(v, "count")]));
  }
  if (result.net) {
    const net = result.net;
    tables.places = rows(["place_index", "place_id", "initial_tokens", "initial_key", "final_tokens", "final_key"],
      Array.from({ length: net.place_count }, (_, i) => [i, net.place_ids?.[i] ?? null, nativeCell(net.initial_marking, i),
        net.initial_marking_keys?.[i] ?? null, nativeCell(net.final_marking, i), net.final_marking_keys?.[i] ?? null]));
    tables.transitions = rows(["transition_index", "transition_id", "activity", "inputs", "outputs", "reset_inputs", "inhibitor_inputs", "input_weights", "output_weights"],
      net.transitions.map((t, i) => [i, net.transition_ids?.[i] ?? null, t.activity, nativeJson(t.inputs), nativeJson(t.outputs),
        nativeJson(t.reset_inputs), nativeJson(t.inhibitor_inputs), t.input_weights ? nativeJson(t.input_weights) : null, t.output_weights ? nativeJson(t.output_weights) : null]));
    tables.arcs = rows(["arc_index", "arc_id", "place_index", "transition_index", "direction", "type", "weight"],
      result.arcs.map((arc, i) => [i, arc.id, arc.place, arc.transition, arc.input ? "place_to_transition" : "transition_to_place", arc.type, nativeCell(arc, "weight")]));
    const metadata: unknown[][] = [];
    const visit = (value: unknown, path: string, holder?: object, key?: string | number) => {
      if (value && typeof value === "object" && Object.keys(value).length)
        Object.entries(value).forEach(([name, child]) => visit(child, path + "/" + name, value, name));
      else metadata.push([path, typeof value, holder && key !== undefined ? nativeCell(holder, key) : nativeJson(value)]);
    };
    visit(net.stochastic_map, "stochastic_map");
    tables.stochastic_map = rows(["path", "kind", "value"], metadata);
  }
  if (result.tree) tables.tree_nodes = rows(["node_id", "parent_id", "child_position", "operator", "activity_index", "activity", "child_count"],
    exchangeTreeRows(result.tree).map(v => [v.id, v.parent, v.child, v.node.operator, v.node.activity ?? null,
      v.node.activity === undefined ? null : result.tree!.activities[v.node.activity], v.node.children?.length ?? 0]));
  return tables;
}
export function modelExchangeGraph(result: ModelExchangeResult): ModelGraph {
  if (result.net) {
    const net = result.net;
    return {
      nodes: [...Array.from({ length: net.place_count }, (_, i) => ({
        id: "p/" + i, label: net.place_ids?.[i] ?? "Place " + i,
        detail: "Initial " + nativeCell(net.initial_marking, i) + " · final " + nativeCell(net.final_marking, i),
        start: BigInt(String(nativeCell(net.initial_marking, i))) > 0n,
        end: BigInt(String(nativeCell(net.final_marking, i))) > 0n })),
        ...net.transitions.map((t, i) => ({ id: "t/" + i, label: t.activity ?? "τ", detail: net.transition_ids?.[i] ?? "Transition " + i }))],
      edges: result.arcs.map((arc, i) => ({ id: String(i), source: (arc.input ? "p/" + arc.place : "t/" + arc.transition),
        target: (arc.input ? "t/" + arc.transition : "p/" + arc.place), label: arc.type + " · " + nativeCell(arc, "weight"), value: 1, directed: true })),
    };
  }
  if (result.data.family === "DFG_DOCUMENT") {
    const dfg = result.data, names = [...new Set([...dfg.graph.flatMap(e => [e.source, e.target]),
      ...dfg.start_activities.map(e => e.activity), ...dfg.end_activities.map(e => e.activity)])];
    return { nodes: names.map(id => ({ id, label: id, start: dfg.start_activities.some(e => e.activity === id),
      end: dfg.end_activities.some(e => e.activity === id) })), edges: dfg.graph.map((edge, i) =>
      ({ id: String(i), source: edge.source, target: edge.target, label: String(nativeCell(edge, "count")), value: 1, directed: true })) };
  }
  return { nodes: [], edges: [] };
}
export function modelExchangeTreeGraph(tree: ExchangeTree): ModelGraph {
  const values = exchangeTreeRows(tree);
  return { nodes: values.map(v => ({ id: v.id, label: v.node.activity === undefined ? v.node.operator : tree.activities[v.node.activity],
    detail: v.node.activity === undefined ? "" : "Activity index " + v.node.activity, start: v.parent === null })),
    edges: values.filter(v => v.parent !== null).map(v => ({ id: v.id, source: v.parent!, target: v.id, label: String(v.child), value: 1, directed: true })) };
}
export interface ModelExchangeRun {
  result: ModelExchangeResult; raw: string; report_json: string; query: string;
  source: { name: string; size: number; sha256: string }; tables: Record<string, OcelPreparedTable>;
}
export async function requestModelExchange(file: File, form: ModelExchangeForm, signal?: AbortSignal): Promise<ModelExchangeRun> {
  const query = modelExchangeQuery(form);
  const digest = await crypto.subtle.digest("SHA-256", await file.arrayBuffer());
  const source = { name: file.name, size: file.size, sha256: [...new Uint8Array(digest)].map(v => v.toString(16).padStart(2, "0")).join("") };
  signal?.throwIfAborted();
  const body = new FormData(); body.append("model", file, file.name);
  const response = await backendFetch("/api/model-exchange?" + query, { method: "POST", body, signal });
  const raw = await response.text(); signal?.throwIfAborted();
  let result: unknown;
  try { result = parseNativeJson(raw); } catch { throw new Error("The backend returned invalid model exchange JSON."); }
  if (!response.ok || (record(result) && "error" in result))
    throw new Error(record(result) && typeof result.error === "string" ? result.error : "Model exchange failed (" + response.status + ").");
  if (!validModelExchange(result, form)) throw new Error("The backend returned an invalid or mismatched model exchange result.");
  const report_json = '{"family":"MODEL_EXCHANGE_REPORT","request":' + JSON.stringify({ source, query, parameters: Object.fromEntries(new URLSearchParams(query)) }) + ',"response":' + raw + '}';
  return { result, raw, report_json, query, source, tables: modelExchangeTables(result) };
}
export function exchangedPnml(run: ModelExchangeRun): File {
  if (!run.result.pnml) throw new Error("This run has no native PNML.");
  return new File([run.result.pnml], "exchanged-model.pnml", { type: "application/xml" });
}
