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

/** Decompresses a gzip upload into a File with the given name. */
export async function gunzipFile(file: Blob, name: string, type = "text/csv"): Promise<File> {
  if (typeof DecompressionStream === "undefined") throw new Error("this browser cannot decompress gzip; extract the file first");
  try {
    const blob = await new Response(file.stream().pipeThrough(new DecompressionStream("gzip"))).blob();
    return new File([blob], name, { type });
  } catch {
    throw new Error("the file is not valid gzip");
  }
}

/** The file to open in place of `file`: converted when its extension needs it, otherwise itself. */
export async function openableLogFile(file: File, signal?: AbortSignal): Promise<File> {
  const route = openedFileRoute(file.name);
  if (route === "gzip_table") {
    const name = file.name.replace(/\.gz$/i, "");
    return gunzipFile(file, name, /\.tsv$/i.test(name) ? "text/tab-separated-values" : "text/csv");
  }
  if (route === "ocel_xml" || route === "ocel_sqlite") {
    const format = route === "ocel_sqlite" ? "sqlite" : ocelXmlFormat(await file.slice(0, 1 << 16).text());
    const run = await requestOcelIo({ ...defaultOcelIo("import"), format }, file, undefined, signal);
    // The OCEL views post their source as JSON; the name keeps the original.
    return new File([importedOcelFile(run)], `${file.name}.json`, { type: "application/json" });
  }
  return file;
}
