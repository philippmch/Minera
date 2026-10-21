import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile, referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText, type TableRepetitionEngine} from "../table-attribute-repetition";
import {defaultTableCaseForm, requestTableCase, tableCaseArtifactFile, tableCaseResponseFile, tableCaseSourceExample,
  TableCaseRequestGate, type TableCaseForm, type TableCaseOperation, type TableCaseRun} from "../table-case-filters";

export function TableCaseFilterWorkspace({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TableCaseForm}) {
  const [source, setSource] = useState<File | null>(null), [form, setForm] = useState(() => initialForm ?? defaultTableCaseForm(engine));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), [run, setRun] = useState<TableCaseRun | null>(null);
  const gate = useRef(new TableCaseRequestGate()), request = useRef<AbortController | null>(null);
  const current = useRef({source, form}); current.current = {source, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof TableCaseForm>(key: K, value: TableCaseForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current; if (!identity.source) return;
    cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity); request.current = controller; setPending(true);
    try {const result = await requestTableCase(identity.source, identity.form, controller.signal); if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);}
    catch (cause) {if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (request.current === controller) {request.current = null; setPending(false);}}
  };
  return <div className="reference-table-case-workspace"><section className="panel query-workspace" aria-labelledby="table-case-heading">
    <div className="panel-head"><h2 id="table-case-heading">Filter complete table cases</h2></div>
    <p className="query-intro">Keep complete cases by event count, case limit or duration. Results retain the selected engine’s case selection and row ordering.</p>
    <fieldset className="query-fields"><legend>Source table</legend>
      <label className="filters-field"><span>Native EventTable JSON file</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose complete native table JSON with column types and metadata."}</small></label>
      <button className="btn-ghost" onClick={() => {const file = tableCaseSourceExample(engine); downloadQueryBlob(file, file.name);}}>Download {engine} case example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Case criteria</legend>
      <label className="filters-field"><span>Case filter</span><select value={form.operation} onChange={event => {const operation = event.target.value as TableCaseOperation; edit(() => setForm(previous => ({...previous, operation, minimum: operation === "size" ? "2" : "0", maximum: operation === "size" ? "null" : "10000000000"})));}}><option value="size">Number of events per case</option><option value="count">Maximum number of cases</option><option value="performance">Case duration in seconds</option></select></label>
      {form.operation === "count" ? <label className="filters-field"><span>Maximum cases</span><input spellCheck={false} value={form.maxCases} onChange={event => change("maxCases", event.target.value)}/></label> : <div className="query-options"><label className="filters-field"><span>Minimum {form.operation === "size" ? "events" : "seconds"}</span><input spellCheck={false} value={form.minimum} onChange={event => change("minimum", event.target.value)}/></label><label className="filters-field"><span>Maximum {form.operation === "size" ? "events (null for no upper bound)" : "seconds"}</span><input spellCheck={false} value={form.maximum} onChange={event => change("maximum", event.target.value)}/></label></div>}
      <label className="filters-field"><span>Case ID column</span><select value={form.caseKeyMode} onChange={event => change("caseKeyMode", event.target.value as "default" | "custom")}><option value="default">Default: case:concept:name</option><option value="custom">Use a literal column name</option></select>{form.caseKeyMode === "custom" && <textarea rows={2} aria-label="Exact case ID column" value={form.caseKey} onChange={event => change("caseKey", event.target.value)}/>}</label>
      {form.operation === "performance" && <><label className="filters-field"><span>Timestamp column</span><select value={form.timestampKeyMode} onChange={event => change("timestampKeyMode", event.target.value as "default" | "custom")}><option value="default">Default: time:timestamp</option><option value="custom">Use a literal column name</option></select>{form.timestampKeyMode === "custom" && <textarea rows={2} aria-label="Exact timestamp column" value={form.timestampKey} onChange={event => change("timestampKey", event.target.value)}/>}</label><label className="query-check"><input type="checkbox" checked={form.businessHours} onChange={event => change("businessHours", event.target.checked)}/>Measure business hours</label><p>Business hours use the selected engine’s default schedule. Timestamp cells need their declared temporal type.</p></>}
      <p>Bounds use exact integer or decimal tokens; the engine validates their meaning. Literal column names preserve whitespace, newlines and empty names.</p>
      <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Filter table cases</button>{pending && <><span role="status">Filtering cases…</span><button className="btn-ghost" onClick={cancel}>Cancel case filtering</button></>}</div>
      {!source && <p>Choose a native table file to apply this filter.</p>}{error && <div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run && <TableCaseFilterInspection run={run}/>}</div>;
}
export function TableCaseFilterInspection({run}: {run: TableCaseRun}) {
  const value = run.value, success = value.http_status === 200 && value.error === null;
  const download = (kind: "source" | "result") => {const file = tableCaseArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel" aria-labelledby="table-case-result-heading"><div className="panel-head"><h2 id="table-case-result-heading">{success ? "Filtered table cases" : "Table cases could not be filtered"}</h2></div><div className="query-results">
    {value.error !== null && <div className="error-banner" role="alert">{value.error_type !== null ? `${referenceLogFilterStringText(value.error_type)}: ` : ""}{referenceLogFilterStringText(value.error)}</div>}
    <p>Engine: {value.engine ?? "unavailable"}; filter: {value.operation ?? "unavailable"}; stage: {value.phase}; apply returned: {value.apply_returned ? "yes" : "no"}.</p>
    {value.source && value.result && <table><caption>Source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([['Source', value.source], ['Filtered', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count, "rows")}</td><td>{tableRepetitionCountText(count, "columns")}</td></tr>)}</tbody></table>}
    {success && value.result?.rows === 0 && <p>No cases matched these bounds.</p>}
    <p>{run.sourceFile.name} · case column: {referenceLogFilterStringText(value.case_id_key)}{value.operation === "performance" ? ` · timestamp: ${referenceLogFilterStringText(value.timestamp_key)}` : ""}</p>
    {value.source_unchanged === true && <p>The source table was preserved.</p>}{value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source table. Download the response for inspection.</div>}
    <p>Downloads contain complete native cells and table metadata.</p><div className="query-actions"><button className="btn-ghost" disabled={value.source_table_json === null} onClick={() => download("source")}>Download parsed source table</button><button className="btn-open" disabled={!success || value.filtered_table_json === null} onClick={() => download("result")}>Download filtered native table</button><button className="btn-ghost" onClick={() => {const file = tableCaseResponseFile(run); downloadQueryBlob(file, file.name);}}>Download table cases response</button></div>
  </div></section>;
}
