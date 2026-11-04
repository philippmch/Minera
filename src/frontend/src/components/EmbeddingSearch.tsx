import { useEffect, useMemo, useRef, useState } from "react";
import { downloadBlob } from "../download";
import { requestEmbeddings } from "../api";
import { DEFAULT_EMBEDDING_SEARCH, LOCAL_EMBEDDING_MODEL, embeddingSearchOptions, embeddingSearchError, type EmbeddingSearchSettings } from "../embedding-search";
import { type EmbeddingApiResult, type MineraExport, type Trace, numberFormat } from "../types";
import { loadedTraces, useTraces, type TraceLoad } from "../mine-traces";
import { TraceLoadNote } from "./TraceLoadNote";

const PAGE_SIZE = 20;

/** The activity sequence a case result shows, once the cases are loaded. */
export function embeddingCaseSequence(data: MineraExport, load: TraceLoad, trace: Trace | undefined): string {
  if (trace) return trace.events.map(event => data.activities[event.activity]?.name ?? "?").join(" → ");
  if (load.status === "loading" || load.status === "idle") return "Loading the sequence…";
  if (load.status === "error") return "Sequence unavailable: the cases could not be loaded";
  return "Sequence not included in this export";
}

export function EmbeddingSearch({ data, available, mining }: { data: MineraExport; available: boolean; mining: boolean }) {
  const [settings, setSettings] = useState({ ...DEFAULT_EMBEDDING_SEARCH });
  const [target, setTarget] = useState("");
  const [result, setResult] = useState<EmbeddingApiResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [pending, setPending] = useState(false);
  const [page, setPage] = useState(0);
  const request = useRef<AbortController | null>(null);
  const disabled = !available || mining || pending;

  useEffect(() => () => { request.current?.abort(); request.current = null; }, []);
  useEffect(() => {
    if (!available || mining) {
      request.current?.abort(); request.current = null;
      setPending(false); setResult(null); setError(null);
    }
  }, [available, mining]);

  const change = (key: keyof EmbeddingSearchSettings, value: string) => setSettings(previous => ({ ...previous, [key]: value }));
  const run = async (search: boolean) => {
    if (disabled) return;
    setError(null);
    let options;
    try { options = embeddingSearchOptions(settings, search ? target : undefined); }
    catch (cause) { setError(cause instanceof Error ? cause.message : String(cause)); return; }
    request.current?.abort();
    const controller = new AbortController(); request.current = controller;
    setPending(true); setResult(null);
    try {
      const next = await requestEmbeddings(options, controller.signal);
      if (request.current === controller && !controller.signal.aborted) { setResult(next); setPage(0); }
    } catch (cause) {
      if (request.current === controller && !controller.signal.aborted)
        setError(embeddingSearchError(cause));
    } finally {
      if (request.current === controller) { request.current = null; setPending(false); }
    }
  };
  const cancel = () => { request.current?.abort(); request.current = null; setPending(false); };
  const download = () => {
    if (!result) return;
    downloadBlob(new Blob([JSON.stringify(result, null, 2)], { type: "application/json" }), result.family === "TO_EMBEDDINGS" ? "minera-embeddings.json" : "minera-embedding-matches.json");
  };
  const isSearch = result?.family === "TO_EMBEDDINGS_SIMILARITY";
  const pageCount = Math.ceil((result?.items.length ?? 0) / PAGE_SIZE);
  // Case results show each case's activity sequence, read from the cases.
  const load = useTraces(data, result?.variant === "cases_transformers");
  const loaded = loadedTraces(load);
  const traces = useMemo(() => new Map(loaded.map(trace => [trace.case_id, trace])), [loaded]);

  return <section className="panel embedding-panel reveal">
    <div className="panel-head">
      <h2>Search cases and events</h2>
      <span className="panel-meta">{numberFormat.format(data.trace_count)} cases · {numberFormat.format(data.event_count)} events</span>
    </div>
    <p className="embedding-intro">Find similar activity sequences or event text, and export their vectors.</p>
    {!available && <p className="model-empty">Open a CSV or XES log to search it. Saved JSON exports and the bundled sample do not provide an active log for embedding requests.</p>}
    <form onSubmit={event => { event.preventDefault(); void run(true); }}>
      <fieldset disabled={disabled} className="embedding-controls">
        <div className="embedding-options">
          <label className="filters-field"><span>Search scope</span><select value={settings.variant} onChange={event => change("variant", event.target.value)}>
            <option value="cases_transformers">Cases · activity sequences</option>
            <option value="events_transformers">Events · individual text</option>
          </select></label>
          <label className="filters-field"><span>Encoder</span><select value={settings.provider} onChange={event => {
            const provider = event.target.value as EmbeddingSearchSettings["provider"];
            setSettings(previous => ({ ...previous, provider, model: provider === "transformer" ? LOCAL_EMBEDDING_MODEL : "hashing-v1", dimensions: provider === "transformer" ? "0" : "64" }));
          }}><option value="hashing">Token hashing · built in</option><option value="transformer">Local transformer · semantic</option></select></label>
          <label className="filters-field embedding-model"><span>Model name</span><input value={settings.model} onChange={event => change("model", event.target.value)} spellCheck={false} /></label>
        </div>
        {settings.provider === "transformer"
          ? <p className="embedding-hint">Start the local encoder and connect the Minera server to it. The model name must match the loaded model. Zero dimensions uses its full vector width.</p>
          : <p className="embedding-hint">Token hashing compares shared words. Select a local transformer for semantic search.</p>}
        <details className="embedding-advanced"><summary>Text fields and limits</summary><div className="embedding-options">
          <label className="filters-field"><span>Text attribute</span><input value={settings.attribute} onChange={event => change("attribute", event.target.value)} spellCheck={false} /></label>
          {settings.variant === "events_transformers" && <label className="filters-field"><span>Event ID attribute</span><input value={settings.eventId} placeholder="Event ordinal" onChange={event => change("eventId", event.target.value)} spellCheck={false} /></label>}
          <label className="filters-field"><span>Vector dimensions</span><input type="number" min={settings.provider === "transformer" ? 0 : 1} max="4096" value={settings.dimensions} onChange={event => change("dimensions", event.target.value)} /></label>
          <label className="filters-field"><span>Item limit</span><input type="number" min="1" max="1000000" value={settings.maxItems} onChange={event => change("maxItems", event.target.value)} /></label>
        </div><p className="embedding-hint">The item limit applies to the whole log. Increase it if your case or event count exceeds it.</p></details>
        <label className="filters-field embedding-target"><span>Find text</span><textarea rows={2} value={target} placeholder="For example: approve a refund" onChange={event => setTarget(event.target.value)} /></label>
        <div className="embedding-actions">
          <label className="filters-field"><span>Matches · 0 shows all</span><input type="number" min="0" max="10000" value={settings.topK} onChange={event => change("topK", event.target.value)} /></label>
          <button className="btn-open" type="submit">Find matches</button>
          <button className="btn-ghost" type="button" onClick={() => void run(false)}>Build embeddings</button>
        </div>
      </fieldset>
    </form>
    {pending && <div className="embedding-status" role="status">Encoding the log… <button type="button" className="btn-ghost" onClick={cancel}>Cancel</button></div>}
    {error && <div className="error-banner" role="alert">{error}</div>}
    {result && <div className="embedding-result">
      <div className="embedding-result-head">
        <div><h3>{isSearch ? "Matching text" : "Embedding vectors"}</h3><p className="panel-meta">{numberFormat.format(result.items.length)} {result.variant === "cases_transformers" ? "cases" : "events"} · {result.dimensions ?? "unknown"} dimensions · {result.embedding_model ?? "legacy encoder"}</p></div>
        <button className="btn-ghost" type="button" onClick={download}>Download JSON</button>
      </div>
      {result.variant === "cases_transformers" && load.status === "error" && <TraceLoadNote load={load} />}
      {result.family === "TO_EMBEDDINGS_SIMILARITY" && <p className="embedding-query">Query: <strong>{result.target}</strong><span>Cosine similarity · higher scores mean closer text</span></p>}
      {result.items.length === 0 ? <p className="model-empty">No items in this log.</p> : <div className="embedding-table-wrap"><table className="activity-table">
        <thead><tr><th>{result.variant === "cases_transformers" ? "Case ID" : "Event ID"}</th><th>{isSearch ? "Cosine score" : "Vector preview · first 8 values"}</th>{result.variant === "cases_transformers" && <th>Activity sequence</th>}</tr></thead>
        <tbody>{result.items.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE).map((item, index) => <tr key={`${index}-${item.id}`}>
          <td>{item.id}</td><td className="embedding-values">{"score" in item ? item.score.toFixed(6) : item.embedding.slice(0, 8).map(value => value.toFixed(5)).join(", ")}</td>
          {result.variant === "cases_transformers" && <td>{embeddingCaseSequence(data, load, traces.get(item.id))}</td>}
        </tr>)}</tbody>
      </table></div>}
      {pageCount > 1 && <div className="embedding-pagination"><button className="btn-ghost" disabled={page === 0} onClick={() => setPage(page - 1)}>Previous</button><span className="panel-meta">Page {page + 1} of {pageCount}</span><button className="btn-ghost" disabled={page + 1 >= pageCount} onClick={() => setPage(page + 1)}>Next</button></div>}
    </div>}
  </section>;
}
