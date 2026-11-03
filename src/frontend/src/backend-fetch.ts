// fetch() for calls to the Minera backend. A network failure (server stopped,
// wrong port, connection refused) surfaces in browsers as a bare
// "TypeError: Failed to fetch"; this turns it into the same actionable message
// the main API client uses. Aborts and HTTP error statuses pass through.

export const BACKEND_UNREACHABLE = "could not reach the Minera backend — is minera_server running?";

function isAbort(cause: unknown): boolean {
  return typeof cause === "object" && cause !== null && (cause as { name?: unknown }).name === "AbortError";
}

export async function backendFetch(...args: Parameters<typeof fetch>): Promise<Response> {
  try {
    return await fetch(...args);
  } catch (cause) {
    if (!isAbort(cause) && cause instanceof TypeError) throw new Error(BACKEND_UNREACHABLE, { cause });
    throw cause;
  }
}
