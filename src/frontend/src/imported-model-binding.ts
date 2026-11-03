// Binds requests on the imported model to the model the server installed.
//
// Every /api/check-model answer carries a fresh `imported_model_id`. Requests
// that name `miner=imported` echo it in the X-Minera-Imported-Model header, and
// the server refuses them (409) unless that model is its imported model for
// the whole request. That catches a replacement whose answer has not reached
// this page yet, and one made from another tab, which a page-local counter
// cannot see.

export const IMPORTED_MODEL_HEADER = "X-Minera-Imported-Model";

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

function namesImportedModel(input: string, body: RequestInit["body"]): boolean {
  const query = input.indexOf("?");
  if (query >= 0 && new URLSearchParams(input.slice(query + 1).split("#")[0]).get("miner") === "imported") return true;
  return typeof FormData !== "undefined" && body instanceof FormData && body.get("miner") === "imported";
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

/** fetch(), bound to the imported model when the request names it. */
export function importedModelFetch(input: string, init?: RequestInit): Promise<Response> {
  const bound = importedModelRequestInit(input, init);
  return bound === undefined ? fetch(input) : fetch(input, bound);
}
