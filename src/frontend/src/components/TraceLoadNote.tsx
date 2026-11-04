import { type TraceLoad } from "../mine-traces";
import { numberFormat } from "../types";

/** The loading and error states of a view whose cases are loaded on demand. */
export function TraceLoadNote({ load, cases }: { load: TraceLoad & { retry?: () => void }; cases?: number }) {
  if (load.status === "loading")
    return <p className="empty-note trace-load" role="status">Loading {cases === undefined ? "the" : numberFormat.format(cases)} cases…</p>;
  if (load.status === "error")
    return <div className="error-banner trace-load" role="alert">{load.error}{load.retry && <> <button type="button" className="btn-ghost" onClick={load.retry}>Try again</button></>}</div>;
  return null;
}
