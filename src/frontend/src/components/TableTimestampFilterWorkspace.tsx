import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile, referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText, type TableRepetitionEngine} from "../table-attribute-repetition";
import {defaultTableTimestampForm, requestTableTimestamp, tableTimestampArtifactFile, tableTimestampResponseFile, tableTimestampSourceExample,
  tableTimestampOperations, TABLE_TIMESTAMP_LABELS, TableTimestampRequestGate, type TableTimestampForm, type TableTimestampRun} from "../table-timestamp-filters";
import {PandasTimestampOriginalCallWorkspace} from "./PandasTimestampOriginalCallWorkspace";

export function TableTimestampFilterWorkspace({engine, initialForm, initialCallMode = "friendly"}: {engine: TableRepetitionEngine; initialForm?: TableTimestampForm; initialCallMode?: "friendly" | "original"}) {
  const [mode, setMode] = useState(initialCallMode);
  useEffect(() => {setMode(initialCallMode);}, [engine, initialCallMode]);
  if (engine !== "pandas") return <TableTimestampFriendlyWorkspace key={engine} engine={engine} initialForm={initialForm}/>;
  return <><section className="panel query-workspace"><label className="filters-field"><span>Pandas timestamp call mode</span><select value={mode} onChange={event => setMode(event.target.value as "friendly" | "original")}><option value="friendly">Choose a time window with form controls</option><option value="original">Use an original typed call</option></select></label></section>{mode === "original" ? <PandasTimestampOriginalCallWorkspace/> : <TableTimestampFriendlyWorkspace key={engine} engine={engine} initialForm={initialForm}/>}</>;
}
function TableTimestampFriendlyWorkspace({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TableTimestampForm}) {
  const [source, setSource] = useState<File | null>(null), [form, setForm] = useState(() => initialForm ?? defaultTableTimestampForm(engine));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), [run, setRun] = useState<TableTimestampRun | null>(null);
  const gate = useRef(new TableTimestampRequestGate()), request = useRef<AbortController | null>(null), current = useRef({source, form}); current.current = {source, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof TableTimestampForm>(key: K, value: TableTimestampForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current; if (!identity.source) return; cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity); request.current = controller; setPending(true);
    try {const result = await requestTableTimestamp(identity.source, identity.form, controller.signal); if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);}
    catch (cause) {if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (request.current === controller) {request.current = null; setPending(false);}}
  };
  return <div className="reference-table-timestamp-workspace"><section className="panel query-workspace" aria-labelledby="table-timestamp-heading">
    <div className="panel-head"><h2 id="table-timestamp-heading">Filter by a time window</h2></div>
    <p className="query-intro">Keep individual events or complete cases using the selected timestamp column and the engine’s time comparisons.</p>
    <fieldset className="query-fields"><legend>Source table</legend>
      <label className="filters-field"><span>Native EventTable JSON file</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose complete native table JSON with column types and metadata."}</small></label>
      <button className="btn-ghost" onClick={() => {const file = tableTimestampSourceExample(engine); downloadQueryBlob(file, file.name);}}>Download {engine} timestamp example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Time window</legend>
      <label className="filters-field"><span>Timestamp filter</span><select value={form.operation} onChange={event => change("operation", event.target.value as TableTimestampForm["operation"])}>{tableTimestampOperations(engine).map(operation => <option value={operation} key={operation}>{TABLE_TIMESTAMP_LABELS[operation]}</option>)}</select></label>
      <div className="query-options"><label className="filters-field"><span>Lower endpoint</span><input spellCheck={false} value={form.lower} onChange={event => change("lower", event.target.value)}/></label><label className="filters-field"><span>Upper endpoint</span><input spellCheck={false} value={form.upper} onChange={event => change("upper", event.target.value)}/></label></div>
      <p>Use YYYY-MM-DD HH:mm:ss, for example 2026-06-01 00:00:01. String bounds use UTC; endpoint text is sent unchanged.</p>
      {form.operation === "intersecting" ? <p>Intersection uses strict boundary comparisons.{engine === "polars" ? " A case whose start and end exactly equal the two window endpoints is excluded." : " Cases touching only one endpoint are excluded."}</p> : <p>Both boundaries are included.</p>}
      <div className="query-options">{([['caseKeyMode', 'caseKey', 'Case ID column', 'case:concept:name'], ['timestampKeyMode', 'timestampKey', 'Timestamp column', 'time:timestamp'], ['attributeKeyMode', 'attributeKey', 'Matched attribute column', 'concept:name']] as const).filter(([mode]) => (mode !== "caseKeyMode" || form.operation !== "events") && (mode !== "attributeKeyMode" || form.operation === "attribute")).map(([mode, key, label, fallback]) => <label className="filters-field" key={key}><span>{label}</span><select value={form[mode]} onChange={event => change(mode, event.target.value as "default" | "custom")}><option value="default">{fallback}</option><option value="custom">Use a literal column name</option></select>{form[mode] === "custom" && <textarea rows={2} aria-label={`Exact ${label.toLowerCase()}`} value={form[key]} onChange={event => change(key, event.target.value)}/>}</label>)}</div>
      {form.operation === "attribute" && <label className="query-text"><span>Exact typed value as JSON</span><textarea rows={4} spellCheck={false} value={form.targetJson} onChange={event => change("targetJson", event.target.value)}/><small>{'For example {"type":"string","value":"A"} or {"type":"integer","value":"9007199254740993"}. The matching event must fall in the window; the result contains its complete case.'}</small></label>}
      {(form.operation === "starting" || form.operation === "completing") && <label className="filters-field"><span>Selection</span><select value={String(form.positive)} onChange={event => change("positive", event.target.value === "true")}><option value="true">Keep matching cases</option><option value="false">Keep cases outside the condition</option></select></label>}
      {engine === "pandas" && <label className="query-check"><input type="checkbox" checked={form.keepMissing} onChange={event => change("keepMissing", event.target.checked)}/>Keep events or cases without temporal values</label>}
      <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Filter table timestamps</button>{pending && <><span role="status">Filtering timestamps…</span><button className="btn-ghost" onClick={cancel}>Cancel timestamp filtering</button></>}</div>
      {!source && <p>Choose a native table file to apply this filter.</p>}{error && <div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run && <TableTimestampFilterInspection run={run}/>}</div>;
}
export function TableTimestampFilterInspection({run}: {run: TableTimestampRun}) {
  const value = run.value, success = value.http_status === 200 && value.error === null;
  const download = (kind: "source" | "result") => {const file = tableTimestampArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel" aria-labelledby="table-timestamp-result-heading"><div className="panel-head"><h2 id="table-timestamp-result-heading">{success ? "Filtered table timestamps" : "Table timestamps could not be filtered"}</h2></div><div className="query-results">
    {value.error !== null && <div className="error-banner" role="alert">{value.error_type !== null ? `${referenceLogFilterStringText(value.error_type)}: ` : ""}{referenceLogFilterStringText(value.error)}</div>}
    <p>Engine: {value.engine ?? "unavailable"}; filter: {value.operation ?? "unavailable"}; stage: {value.phase}; apply returned: {value.apply_returned ? "yes" : "no"}.</p>
    {value.source && value.result && <table><caption>Source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([['Source', value.source], ['Filtered', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count, "rows")}</td><td>{tableRepetitionCountText(count, "columns")}</td></tr>)}</tbody></table>}
    {success && value.result?.rows === 0 && <p>No rows matched this timestamp condition.</p>}
    <p>{run.sourceFile.name} · timestamp column: {referenceLogFilterStringText(value.timestamp_key)}</p>
    {value.source_unchanged === true && <p>The source table was preserved.</p>}{value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source table. Download the response for inspection.</div>}
    <div className="query-actions">{value.source_table_json !== null && <button className="btn-ghost" onClick={() => download("source")}>Download complete parsed source table</button>}{success && value.filtered_table_json !== null && <button className="btn-ghost" onClick={() => download("result")}>Download filtered native table</button>}<button className="btn-ghost" onClick={() => {const file = tableTimestampResponseFile(run); downloadQueryBlob(file, file.name);}}>Download timestamp filter response</button></div>
  </div></section>;
}
