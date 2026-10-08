import { traceVariantQuery, pripelQuery } from "./api";
import { isTraceVariantQueryResult, isPripelResult, type TraceVariantQueryOptions, type PripelOptions, type TraceVariantQueryResult, type PripelResult } from "./types";

export interface AnonymizationRelease {
  family: "ANONYMIZATION_RELEASE";
  source_id: string;
  result: TraceVariantQueryResult | PripelResult;
  released_xes: string;
  // Keep the original JSON bytes: native uint64 seeds need not fit JS numbers.
  report_json: string;
}
export type NoiseMode = "secure" | "seeded";
export interface VariantForm { variant: "laplace" | "sacofa"; epsilon: string; length: string; threshold: string; seed: string; noise: NoiseMode; traces: string; variants: string; }
export interface PripelForm { epsilon: string; seed: string; noise: NoiseMode; traces: string; distance: string; blocklist: string; }

function integer(text: string, label: string, min: number, max: number): number {
  const value = Number(text);
  if (!text.trim() || !Number.isSafeInteger(value) || value < min || value > max)
    throw new Error(`${label} must be an integer from ${min} through ${max}.`);
  return value;
}
function epsilon(text: string): number {
  const value = Number(text);
  if (!text.trim() || !Number.isFinite(value) || value <= 0) throw new Error("Epsilon must be a positive finite number.");
  return value;
}
export function variantOptions(form: VariantForm): TraceVariantQueryOptions {
  return { tvq_variant: form.variant, tvq_epsilon: epsilon(form.epsilon), tvq_k: integer(form.length, "Maximum prefix length", 0, 100), tvq_p: integer(form.threshold, "Prune threshold", 1, Number.MAX_SAFE_INTEGER), tvq_secure_random: form.noise === "secure", tvq_seed: form.noise === "seeded" ? integer(form.seed, "Seed", 0, Number.MAX_SAFE_INTEGER) : 0, tvq_max_output_traces: integer(form.traces, "Output trace limit", 1, 100000), tvq_max_output_variants: integer(form.variants, "Candidate variant limit", 1, 100000) };
}
export function attributeOptions(form: PripelForm): PripelOptions {
  return { pripel_epsilon: epsilon(form.epsilon), pripel_secure_random: form.noise === "secure", pripel_seed: form.noise === "seeded" ? integer(form.seed, "Seed", 0, Number.MAX_SAFE_INTEGER) : 0, pripel_max_output_traces: integer(form.traces, "Output trace limit", 1, 100000), pripel_max_distance: integer(form.distance, "Maximum matching distance", 0, 1000000), pripel_blocklist: [...new Set(form.blocklist.split(/\r?\n/).map(value => value.trim()).filter(Boolean))] };
}
export function isAnonymizationRelease(value: unknown): value is Omit<AnonymizationRelease, "report_json"> {
  if (!value || typeof value !== "object") return false;
  const r = value as Partial<AnonymizationRelease>;
  return r.family === "ANONYMIZATION_RELEASE" && typeof r.source_id === "string" && !!r.source_id && typeof r.released_xes === "string" && r.released_xes.includes("<log") && (isTraceVariantQueryResult(r.result) || isPripelResult(r.result)) && r.result.diagnostics.released_trace_count === r.result.log.traces.length;
}
async function releaseRequest(path: string, sourceId: string, signal?: AbortSignal, body?: BodyInit): Promise<AnonymizationRelease> {
  if (!sourceId) throw new Error("Refresh the anonymization source before running.");
  const response = await fetch(path, { method: "POST", ...(body === undefined ? {} : { body }), ...(signal ? { signal } : {}) });
  const text = await response.text();
  let value: unknown;
  try { value = JSON.parse(text); } catch { throw new Error("The backend returned an invalid anonymization response."); }
  if (value && typeof value === "object" && "error" in value) throw new Error(String((value as {error:unknown}).error));
  if (!response.ok) throw new Error(`Anonymization returned ${response.status}.`);
  if (!isAnonymizationRelease(value)) throw new Error("The backend returned an invalid anonymization release.");
  if (value.source_id !== sourceId) throw new Error("The source log changed; refresh the anonymization workspace.");
  return { ...value, report_json: text };
}
export function requestVariantRelease(sourceId: string, options: TraceVariantQueryOptions, signal?: AbortSignal): Promise<AnonymizationRelease> {
  const params = new URLSearchParams(traceVariantQuery(options)); params.set("source_id", sourceId); params.set("include_xes", "true");
  return releaseRequest(`/api/trace-variant-query?${params}`, sourceId, signal).then(result => {
    if (result.result.family !== "TRACE_VARIANT_QUERY") throw new Error("The backend returned the wrong anonymization method.");
    return result;
  });
}
export function requestAttributeRelease(sourceId: string, query: File, options: PripelOptions, signal?: AbortSignal): Promise<AnonymizationRelease> {
  const params = new URLSearchParams(pripelQuery(options)); params.set("source_id", sourceId); params.set("include_xes", "true");
  const form = new FormData(); form.append("trace_variant_query", query, query.name);
  return releaseRequest(`/api/pripel?${params}`, sourceId, signal, form).then(result => {
    if (result.result.family !== "PRIPEL") throw new Error("The backend returned the wrong anonymization method.");
    return result;
  });
}
export function releasedFile(release: AnonymizationRelease): File {
  const name = release.result.family === "PRIPEL" ? "pripel" : release.result.variant;
  return new File([release.released_xes], `minera-${name}-released.xes`, { type: "application/xml" });
}
export function releaseEventCount(release: AnonymizationRelease): number {
  return release.result.log.traces.reduce((total, trace) => total + trace.events.length, 0);
}
