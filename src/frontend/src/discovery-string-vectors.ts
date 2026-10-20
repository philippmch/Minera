/** Exact native string vectors shared by configured discovery and main mining.
 * The hex-v1 carrier represents arbitrary std::string bytes. No list member is
 * trimmed, split, sorted, or deduplicated before native discovery receives it.
 */
export type DiscoveryNativeString = string | {native_bytes: "hex-v1"; data: string};
export type DiscoveryVectorMode = "legacy" | "literal_json";
const nativeString = (value: unknown): value is DiscoveryNativeString => typeof value === "string" ||
  !!value && typeof value === "object" && !Array.isArray(value) &&
  (value as Record<string, unknown>).native_bytes === "hex-v1" &&
  typeof (value as Record<string, unknown>).data === "string" &&
  /^(?:[0-9a-fA-F]{2})*$/.test((value as Record<string, unknown>).data as string) &&
  Object.keys(value).length === 2;
export function discoveryStringVectorJson(value: unknown, label: string): string {
  if (!Array.isArray(value) || !Array.from(value).every(nativeString))
    throw new Error(label + ' must be a JSON array of strings or {"native_bytes":"hex-v1","data":"..."} byte strings. Use [] for an explicit empty selection.');
  return JSON.stringify(value);
}
export function parseDiscoveryStringVector(text: string, label: string): DiscoveryNativeString[] {
  let value: unknown;
  try {value = JSON.parse(text);} catch {throw new Error(label + " must be a valid JSON array. Use [] for an explicit empty selection.");}
  discoveryStringVectorJson(value, label);
  return value as DiscoveryNativeString[];
}
export function discoveryStringVectorValidation(text: string, label: string): string | null {
  try {parseDiscoveryStringVector(text, label);return null;} catch (error) {return (error as Error).message;}
}
