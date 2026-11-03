import { backendFetch } from "./backend-fetch";
// Binds requests on the imported model to the model the server installed.
//
// Every /api/check-model answer carries a fresh `imported_model_id`. Requests
// that name `miner=imported` (or a prefixed selector such as
// `petri_analysis_miner=imported`) echo it in the X-Minera-Imported-Model header, and
// the server refuses them (409) unless that model is its imported model for
// the whole request. That catches a replacement whose answer has not reached
// this page yet, and one made from another tab, which a page-local counter
// cannot see.

export const IMPORTED_MODEL_HEADER = "X-Minera-Imported-Model";

// The server's 409 for a bound request whose model is not the server's
// imported model. Re-running cannot help, since this page still holds the
// old id: the model was replaced or dropped elsewhere (another tab, a new
// mining run), so only checking the model again binds a new one.
export const STALE_IMPORTED_MODEL_MESSAGE =
  "The server's imported model is no longer the one this page checked; check the model again, then rerun this request.";
// The same refusal when this page itself checked a new model while the
// request was pending: the page already holds the new id, so a rerun works.
export const REPLACED_WHILE_PENDING_MESSAGE =
  "The imported model changed while this request was pending; run it again.";

let importedModelId: string | undefined;

/** Records the id from a check-model answer; undefined forgets it. */
export function setImportedModelId(id: number | string | undefined): void {
  importedModelId = id === undefined ? undefined : String(id);
}
export function currentImportedModelId(): string | undefined { return importedModelId; }

/** Reads `imported_model_id` from a check-model payload (a non-negative integer), if present. */
export function importedModelIdOf(value: unknown): number | undefined {
  if (value === null || typeof value !== "object") return undefined;
  const id = (value as Record<string, unknown>).imported_model_id;
  return Number.isSafeInteger(id) && (id as number) > 0 ? id as number : undefined;
}

// A model selector is `miner` or a prefixed form of it (`petri_analysis_miner`,
// ...); a request names the imported model when any selector says "imported".
const isMinerField = (name: string): boolean => name === "miner" || name.endsWith("_miner");
function namesImportedModel(input: string, body: RequestInit["body"]): boolean {
  const query = input.indexOf("?");
  if (query >= 0) {
    for (const [name, value] of new URLSearchParams(input.slice(query + 1).split("#")[0]))
      if (isMinerField(name) && value === "imported") return true;
  }
  if (typeof FormData !== "undefined" && body instanceof FormData) {
    for (const [name, value] of body.entries())
      if (isMinerField(name) && value === "imported") return true;
  }
  if (typeof URLSearchParams !== "undefined" && body instanceof URLSearchParams) {
    for (const [name, value] of body)
      if (isMinerField(name) && value === "imported") return true;
  }
  return false;
}

/**
 * The init to send for `input`: unchanged unless it names the imported model
 * and an id is known, in which case the id header is added.
 */
export function importedModelRequestInit(input: string, init?: RequestInit): RequestInit | undefined {
  if (importedModelId === undefined || !namesImportedModel(input, init?.body)) return init;
  const headers = new Headers(init?.headers);
  headers.set(IMPORTED_MODEL_HEADER, importedModelId);
  return { ...init, headers };
}

/**
 * fetch(), bound to the imported model when the request names it. A stale-model
 * refusal of a bound request says what fixes it: rerun when this page checked
 * a newer model meanwhile, otherwise check the model again.
 */
export async function importedModelFetch(input: string, init?: RequestInit): Promise<Response> {
  const bound = importedModelRequestInit(input, init);
  const sentId = bound !== init ? importedModelId : undefined;
  const response = await (bound === undefined ? backendFetch(input) : backendFetch(input, bound));
  if (sentId === undefined || response.status !== 409 || typeof response.clone !== "function") return response;
  const body: unknown = await response.clone().json().catch(() => null);
  if (body === null || typeof body !== "object" || (body as {error?: unknown}).error !== STALE_IMPORTED_MODEL_MESSAGE) return response;
  if (importedModelId === undefined || importedModelId === sentId) return response;
  const headers = new Headers(response.headers);
  headers.set("Content-Type", "application/json");
  return new Response(JSON.stringify({ error: REPLACED_WHILE_PENDING_MESSAGE }), { status: 409, statusText: response.statusText, headers });
}
