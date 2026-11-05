// Files opened from the landing page or the sidebar go to the backend by
// content: JSON is classified (Minera export, OCEL, EventTable), anything else
// is mined as CSV or XES (gzip bytes are read as gzip XES). pm4py's own OCEL
// extensions and gzip CSV need a step first, so they do not end up on the CSV
// path: OCEL XML and SQLite are imported into standard OCEL JSON, and a gzip
// CSV/TSV is decompressed in the browser.
import { defaultOcelIo, importedOcelFile, requestOcelIo } from "./ocel-io";

export type OpenedFileRoute = "ocel_json" | "ocel_xml" | "ocel_sqlite" | "gzip_table" | "as_is";

/** How an opened file reaches the backend, decided by its extension. */
export function openedFileRoute(name: string): OpenedFileRoute {
  if (/\.jsonocel$/i.test(name)) return "ocel_json";
  if (/\.xmlocel$/i.test(name)) return "ocel_xml";
  if (/\.sqlite3?$/i.test(name)) return "ocel_sqlite";
  if (/\.(csv|tsv)\.gz$/i.test(name)) return "gzip_table";
  return "as_is";
}

/** OCEL 2.0 XML declares its object and event types; OCEL 1.0 XML does not. */
export function ocelXmlFormat(head: string): "xml_standard" | "xml" {
  return /<(object-types|event-types)\b/.test(head) ? "xml_standard" : "xml";
}

/** Whether a failure while decompressing means the browser ran out of memory. */
function outOfMemory(cause: unknown): boolean {
  return cause instanceof RangeError ||
    (cause instanceof Error && /out of memory|allocation (size|failed)|NS_ERROR_OUT_OF_MEMORY/i.test(`${cause.name} ${cause.message}`));
}

// Decompressed bytes are folded into a Blob every 16 MiB, so the browser can
// keep them in its blob store (on disk for large files) instead of the page
// holding every chunk until the end.
const BLOB_FOLD_BYTES = 16 << 20;

/**
 * Decompresses a gzip upload into a File with the given name. An abort of
 * `signal` cancels the decompression and rejects with the signal's reason;
 * running out of memory is told apart from a file that is not gzip.
 */
export async function gunzipFile(file: Blob, name: string, type = "text/csv", signal?: AbortSignal): Promise<File> {
  if (typeof DecompressionStream === "undefined") throw new Error("this browser cannot decompress gzip; extract the file first");
  signal?.throwIfAborted();
  const reader = file.stream().pipeThrough(new DecompressionStream("gzip")).getReader();
  const cancel = () => { reader.cancel().catch(() => undefined); };
  signal?.addEventListener("abort", cancel, { once: true });
  let folded = new Blob([]);
  // Decompressed chunks own plain (never shared) buffers.
  let pending: Uint8Array<ArrayBuffer>[] = [];
  let pendingBytes = 0;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      signal?.throwIfAborted();
      if (done) break;
      pending.push(value as Uint8Array<ArrayBuffer>);
      pendingBytes += value.byteLength;
      if (pendingBytes >= BLOB_FOLD_BYTES) {
        folded = new Blob([folded, ...pending]);
        pending = [];
        pendingBytes = 0;
      }
    }
    return new File([folded, ...pending], name, { type });
  } catch (cause) {
    if (signal?.aborted) throw signal.reason;
    if (outOfMemory(cause)) throw new Error("the browser ran out of memory while decompressing the file; extract it first");
    throw new Error("the file is not valid gzip");
  } finally {
    signal?.removeEventListener("abort", cancel);
  }
}

/** The file to open in place of `file`: converted when its extension needs it, otherwise itself. */
export async function openableLogFile(file: File, signal?: AbortSignal): Promise<File> {
  const route = openedFileRoute(file.name);
  if (route === "gzip_table") {
    const name = file.name.replace(/\.gz$/i, "");
    return gunzipFile(file, name, /\.tsv$/i.test(name) ? "text/tab-separated-values" : "text/csv", signal);
  }
  if (route === "ocel_xml" || route === "ocel_sqlite") {
    const format = route === "ocel_sqlite" ? "sqlite" : ocelXmlFormat(await file.slice(0, 1 << 16).text());
    const run = await requestOcelIo({ ...defaultOcelIo("import"), format }, file, undefined, signal);
    // The OCEL views post their source as JSON; the name keeps the original.
    return new File([importedOcelFile(run)], `${file.name}.json`, { type: "application/json" });
  }
  return file;
}
