export type AlphaPlaceName = { boundary: string } | { before: (string | null)[]; after: (string | null)[] };
export interface AlphaPlusResult {
  family: "ALPHA_PLUS";
  places: { id: number; name: AlphaPlaceName; initial: number; final: number }[];
  transitions: { id: number; label: string | null; inputs: number[]; outputs: number[] }[];
  detached_initial_marking: { name: AlphaPlaceName; tokens: number }[];
  detached_final_marking: { name: AlphaPlaceName; tokens: number }[];
}
export interface AlphaPlusResultOptions {
  activity_attribute?: string;
  remove_unconnected?: boolean;
  max_rows_to_materialize?: number;
}

const record = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);
const count = (value: unknown): value is number =>
  typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
const activities = (value: unknown): value is (string | null)[] =>
  Array.isArray(value) && value.every(item => item === null || typeof item === "string");
function placeName(value: unknown): value is AlphaPlaceName {
  if (!record(value)) return false;
  if (typeof value.boundary === "string" && value.boundary.length > 0)
    return value.before === undefined && value.after === undefined;
  return value.boundary === undefined && activities(value.before) && activities(value.after);
}
export function isAlphaPlusResult(value: unknown): value is AlphaPlusResult {
  if (!record(value) || value.family !== "ALPHA_PLUS" || !Array.isArray(value.places) || !Array.isArray(value.transitions)) return false;
  const size = value.places.length;
  const refs = (items: unknown) => Array.isArray(items) && items.every(item => count(item) && item < size);
  const detached = (items: unknown) => Array.isArray(items) && items.every(item =>
    record(item) && placeName(item.name) && count(item.tokens) && item.tokens > 0);
  return value.places.every((place, index) => record(place) && place.id === index &&
      placeName(place.name) && count(place.initial) && count(place.final)) &&
    value.transitions.every((transition, index) => record(transition) && transition.id === index &&
      (transition.label === null || typeof transition.label === "string") && refs(transition.inputs) && refs(transition.outputs)) &&
    detached(value.detached_initial_marking) && detached(value.detached_final_marking);
}

export function alphaPlusResultQuery(options: AlphaPlusResultOptions = {}): string {
  const query = new URLSearchParams();
  if (options.activity_attribute !== undefined) {
    const name = options.activity_attribute.trim();
    if (!name) throw new Error("Alpha+ activity attribute must not be blank");
    query.set("alpha_activity_attribute", name);
  }
  if (options.remove_unconnected !== undefined) {
    if (typeof options.remove_unconnected !== "boolean") throw new Error("Alpha+ cleanup must be boolean");
    query.set("alpha_remove_unconnected", String(options.remove_unconnected));
  }
  if (options.max_rows_to_materialize !== undefined) {
    if (!count(options.max_rows_to_materialize)) throw new Error("Alpha+ event cap must be a nonnegative safe integer");
    query.set("alpha_max_rows_to_materialize", String(options.max_rows_to_materialize));
  }
  return query.toString();
}

export async function discoverAlphaPlusResult(options: AlphaPlusResultOptions = {}, file?: File): Promise<AlphaPlusResult> {
  const query = alphaPlusResultQuery(options);
  const body = file ? new FormData() : undefined;
  if (body && file) body.set("log", file);
  const response = await fetch(`/api/discover-alpha-plus${query ? `?${query}` : ""}`, { method: "POST", body });
  const result: unknown = await response.json();
  if (record(result) && typeof result.error === "string") throw new Error(result.error);
  if (!response.ok) throw new Error(`Alpha+ discovery failed with status ${response.status}`);
  if (!isAlphaPlusResult(result)) throw new Error("Invalid Alpha+ discovery result");
  return result;
}
