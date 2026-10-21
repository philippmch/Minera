import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile, referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText, type TableRepetitionEngine} from "../table-attribute-repetition";
import {defaultTablePathsForm, requestTablePaths, tablePathsArtifactFile, tablePathsResponseFile, tablePathsSourceExample,
  TablePathsRequestGate, type TablePathsForm, type TablePathsRun} from "../table-paths-filters";
export function TablePathsFilterWorkspace({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TablePathsForm}) {
  const [source, setSource] = useState<File | null>(null), [form, setForm] = useState(() => initialForm ?? defaultTablePathsForm(engine));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), [run, setRun] = useState<TablePathsRun | null>(null);
  const gate = useRef(new TablePathsRequestGate()), request = useRef<AbortController | null>(null), current = useRef({source, form}); current.current = {source, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof TablePathsForm>(key: K, value: TablePathsForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current; if (!identity.source) return; cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity); request.current = controller; setPending(true);
    try {const result = await requestTablePaths(identity.source, identity.form, controller.signal); if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);}
    catch (cause) {if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (request.current === controller) {request.current = null; setPending(false);}}
  };
  return <div className="reference-table-paths-workspace"><section className="panel query-workspace" aria-labelledby="table-paths-heading">
    <div className="panel-head"><h2 id="table-paths-heading">Filter cases by adjacent paths</h2></div>
    <p className="query-intro">Retain complete cases containing an adjacent activity pair, or select cases by a pair’s elapsed time.</p>
    <fieldset className="query-fields"><legend>Source table</legend>
      <label className="filters-field"><span>Native EventTable JSON file</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose complete native table JSON with column types and metadata."}</small></label>
      <button className="btn-ghost" onClick={() => {const file = tablePathsSourceExample(engine); downloadQueryBlob(file, file.name);}}>Download {engine} paths example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Path criteria</legend>
      <label className="filters-field"><span>Path filter</span><select value={form.operation} onChange={event => change("operation", event.target.value as "paths" | "performance")}><option value="paths">Match any adjacent path</option><option value="performance">Match an adjacent path within a duration</option></select></label>
      <label className="query-text"><span>{form.operation === "paths" ? "Adjacent pairs as JSON" : "Timed pair as JSON"}</span><textarea rows={4} spellCheck={false} value={form.operation === "paths" ? form.pathsJson : form.pathJson} onChange={event => change(form.operation === "paths" ? "pathsJson" : "pathJson", event.target.value)}/><small>{form.operation === "paths" ? 'Use pairs such as [["A", "B"], ["X", "Y"]]. An empty list is valid.' : 'Use one pair such as ["A", "B"].'} Empty labels, commas, whitespace and escaped newlines remain literal.</small></label>
      <label className="filters-field"><span>Selection</span><select value={String(form.positive)} onChange={event => change("positive", event.target.value === "true")}><option value="true">Keep matching cases</option><option value="false">Keep cases outside the condition</option></select></label>
      <div className="query-options">{([['caseKeyMode', 'caseKey', 'Case ID column', 'case:concept:name'], ['activityKeyMode', 'activityKey', 'Source activity column', 'concept:name'], ['targetKeyMode', 'targetKey', 'Target activity column', 'Use the source activity column']] as const).filter(([mode]) => mode !== "targetKeyMode" || form.operation === "paths").map(([mode, key, label, fallback]) => <label className="filters-field" key={key}><span>{label}</span><select value={form[mode]} onChange={event => change(mode, event.target.value as "default" | "custom")}><option value="default">{fallback}</option><option value="custom">Use a literal column name</option></select>{form[mode] === "custom" && <textarea rows={2} aria-label={`Exact ${label.toLowerCase()}`} value={form[key]} onChange={event => change(key, event.target.value)}/>}</label>)}</div>
      <label className="filters-field"><span>Timestamp column</span><select value={form.timestampKeyMode} onChange={event => change("timestampKeyMode", event.target.value as "default" | "custom")}><option value="default">time:timestamp</option><option value="custom">Use a literal column name</option></select>{form.timestampKeyMode === "custom" && <textarea rows={2} aria-label="Exact timestamp column" value={form.timestampKey} onChange={event => change("timestampKey", event.target.value)}/>}<small>Both table engines sort by case and timestamp before checking adjacency.</small></label>
      {form.operation === "performance" && <div className="query-options"><label className="filters-field"><span>Minimum seconds</span><input spellCheck={false} value={form.minimum} onChange={event => change("minimum", event.target.value)}/></label><label className="filters-field"><span>Maximum seconds</span><input spellCheck={false} value={form.maximum} onChange={event => change("maximum", event.target.value)}/></label></div>}
      <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Filter table paths</button>{pending && <><span role="status">Filtering paths…</span><button className="btn-ghost" onClick={cancel}>Cancel path filtering</button></>}</div>
      {!source && <p>Choose a native table file to apply this filter.</p>}{error && <div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run && <TablePathsFilterInspection run={run}/>}</div>;
}
export function TablePathsFilterInspection({run}: {run: TablePathsRun}) {
  const value = run.value, success = value.http_status === 200 && value.error === null;
  const download = (kind: "source" | "result") => {const file = tablePathsArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel" aria-labelledby="table-paths-result-heading"><div className="panel-head"><h2 id="table-paths-result-heading">{success ? "Filtered table paths" : "Table paths could not be filtered"}</h2></div><div className="query-results">
    {value.error !== null && <div className="error-banner" role="alert">{value.error_type !== null ? `${referenceLogFilterStringText(value.error_type)}: ` : ""}{referenceLogFilterStringText(value.error)}</div>}
    <p>Engine: {value.engine ?? "unavailable"}; filter: {value.operation ?? "unavailable"}; stage: {value.phase}; apply returned: {value.apply_returned ? "yes" : "no"}.</p>
    {value.source && value.result && <table><caption>Source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([['Source', value.source], ['Filtered', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count, "rows")}</td><td>{tableRepetitionCountText(count, "columns")}</td></tr>)}</tbody></table>}
    {success && value.result?.rows === 0 && <p>No cases matched this path condition.</p>}
    <p>{run.sourceFile.name} · case column: {referenceLogFilterStringText(value.case_id_key)} · source: {referenceLogFilterStringText(value.activity_key)} · target: {referenceLogFilterStringText(value.target_attribute_key)}</p>
    {value.source_unchanged === true && <p>The source table was preserved.</p>}{value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source table. Download the response for inspection.</div>}
    <p>Downloads contain complete native cells and table metadata.</p><div className="query-actions"><button className="btn-ghost" disabled={value.source_table_json === null} onClick={() => download("source")}>Download parsed source table</button><button className="btn-open" disabled={!success || value.filtered_table_json === null} onClick={() => download("result")}>Download filtered native table</button><button className="btn-ghost" onClick={() => {const file = tablePathsResponseFile(run); downloadQueryBlob(file, file.name);}}>Download table paths response</button></div>
  </div></section>;
}
