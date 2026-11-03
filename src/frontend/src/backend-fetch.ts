// fetch() for calls to the Minera backend. A network failure (server stopped,
// wrong port, connection refused) surfaces in browsers as a bare
// "TypeError: Failed to fetch"; this turns it into the same actionable message
// the main API client uses. Aborts, HTTP error statuses and every other error
// pass through unchanged. fetch() also throws TypeError for bad arguments (a
// header value that is not ISO-8859-1 or holds a line break, a GET with a
// body, a malformed URL); those keep their own message, so a mistyped token
// is not reported as a stopped server.

export const BACKEND_UNREACHABLE = "could not reach the Minera backend — is minera_server running?";

function isAbort(cause: unknown): boolean {
  return typeof cause === "object" && cause !== null && (cause as { name?: unknown }).name === "AbortError";
}

// Argument errors as Chromium, Firefox, WebKit and Node word them.
const ARGUMENT_ERROR = /^(Failed to execute 'fetch'|Window\.fetch:|Failed to parse URL|Request with GET\/HEAD|Headers\.|Request constructor|Invalid (header|URL|value))/i;
// Network failures as the same engines word them.
const NETWORK_ERROR = /failed to fetch|networkerror|load failed|network connection was lost|fetch failed|network request failed|could not connect|connection refused/i;

/** True for the TypeError a fetch() rejects with when the request never reached a server. */
export function isNetworkFailure(cause: unknown): boolean {
  if (!(cause instanceof TypeError)) return false;
  return !ARGUMENT_ERROR.test(cause.message) && NETWORK_ERROR.test(cause.message);
}

function headerEntries(headers: HeadersInit | undefined): Array<[string, string]> {
  if (!headers) return [];
  if (Array.isArray(headers)) return headers.map(([name, value]) => [String(name), String(value)]);
  if (typeof Headers !== "undefined" && headers instanceof Headers) return Array.from(headers.entries());
  return Object.entries(headers as Record<string, string>).map(([name, value]) => [name, String(value)]);
}

/**
 * The argument problems fetch() would only report as an anonymous TypeError,
 * named before the request is made: header values must be ISO-8859-1 without
 * NUL, CR or LF, and a GET or HEAD request cannot carry a body.
 */
export function backendFetchArgumentError(init: RequestInit | undefined): string | null {
  if (!init) return null;
  for (const [name, value] of headerEntries(init.headers)) {
    for (const character of value) {
      const code = character.codePointAt(0)!;
      if (code > 0xff) return `The ${name} value contains “${character}”, which an HTTP header cannot carry; use only Latin-1 characters.`;
      if (code === 0 || code === 0x0a || code === 0x0d) return `The ${name} value contains a line break or NUL character, which an HTTP header cannot carry.`;
    }
  }
  const method = (init.method ?? "GET").toUpperCase();
  if ((method === "GET" || method === "HEAD") && init.body !== undefined && init.body !== null)
    return `A ${method} request cannot carry a body.`;
  return null;
}

export async function backendFetch(...args: Parameters<typeof fetch>): Promise<Response> {
  const problem = backendFetchArgumentError(args[1]);
  if (problem) throw new TypeError(problem);
  try {
    return await fetch(...args);
  } catch (cause) {
    if (!isAbort(cause) && isNetworkFailure(cause)) throw new Error(BACKEND_UNREACHABLE, { cause });
    throw cause;
  }
}
