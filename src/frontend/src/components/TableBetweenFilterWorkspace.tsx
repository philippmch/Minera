import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile, referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText, type TableRepetitionEngine} from "../table-attribute-repetition";
import {defaultTableBetweenForm, requestTableBetween, tableBetweenArtifactFile, tableBetweenResponseFile,
  tableBetweenSourceExample, TableBetweenRequestGate, type TableBetweenForm, type TableBetweenRun} from "../table-between-filters";

export function TableBetweenFilterWorkspace({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TableBetweenForm}) {
  const [source, setSource] = useState<File | null>(null), [form, setForm] = useState(() => initialForm ?? defaultTableBetweenForm(engine));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), [run, setRun] = useState<TableBetweenRun | null>(null);
  const gate = useRef(new TableBetweenRequestGate()), request = useRef<AbortController | null>(null);
  const current = useRef({source, form}); current.current = {source, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof TableBetweenForm>(key: K, value: TableBetweenForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current; if (!identity.source) return;
    cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity); request.current = controller; setPending(true);
    try {
      const result = await requestTableBetween(identity.source, identity.form, controller.signal);
      if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);
    } catch (cause) {
      if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));
    } finally {if (request.current === controller) {request.current = null; setPending(false);}}
  };
  return <div className="reference-table-between-workspace">
    <section className="panel query-workspace" aria-labelledby="table-between-heading">
      <div className="panel-head"><h2 id="table-between-heading">Extract table intervals between activities</h2></div>
      <p className="query-intro">Choose start and end activities, then retain each completed interval with its subcase label. The selected engine determines boundary and ordering behavior.</p>
      <fieldset className="query-fields"><legend>Source table</legend>
        <label className="filters-field"><span>Native EventTable JSON file</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose complete native table JSON, including its column types and metadata."}</small></label>
        <button className="btn-ghost" onClick={() => {const example = tableBetweenSourceExample(engine); downloadQueryBlob(example, example.name);}}>Download {engine} between example table</button>
        {engine === "polars" ? <p>Polars requires declared column types and a time:timestamp column. It sorts rows by case and timestamp before extracting intervals. Dataframe index metadata is rejected.</p> : <p>pandas retains source index labels and its positional case-block behavior. For contiguous case rows, each completed interval keeps its source order.</p>}
      </fieldset>
      <fieldset className="query-fields"><legend>Interval criteria</legend>
        <div className="query-options">
          <label className="query-text"><span>Start activities as JSON</span><textarea rows={3} spellCheck={false} value={form.startJson} onChange={event => change("startJson", event.target.value)}/></label>
          <label className="query-text"><span>End activities as JSON</span><textarea rows={3} spellCheck={false} value={form.endJson} onChange={event => change("endJson", event.target.value)}/></label>
        </div>
        <p>{'Use a JSON string such as "A", or an array such as ["A", "C"]. Empty strings and arrays are valid. Spaces, commas and escaped newlines remain part of each label.'}</p>
        <div className="query-options">{([['caseKeyMode', 'caseKey', 'Case ID column', 'case:concept:name'], ['activityKeyMode', 'activityKey', 'Activity column', 'concept:name'], ['separatorMode', 'separator', 'Subcase separator', '##@@']] as const).map(([mode, key, label, fallback]) => <label className="filters-field" key={key}><span>{label}</span><select value={form[mode]} onChange={event => change(mode, event.target.value as "default" | "custom")}><option value="default">Default: {fallback}</option><option value="custom">Use a literal value</option></select>{form[mode] === "custom" && <textarea rows={2} aria-label={`Exact ${label.toLowerCase()}`} value={form[key]} onChange={event => change(key, event.target.value)}/>}<small>Literal values preserve empty strings, whitespace and newlines.</small></label>)}</div>
        <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Extract table intervals</button>{pending && <><span role="status">Extracting intervals…</span><button className="btn-ghost" onClick={cancel}>Cancel table extraction</button></>}</div>
        {!source && <p>Choose a native table file to apply this filter.</p>}{error && <div className="error-banner" role="alert">{error}</div>}
      </fieldset>
    </section>
    {run && <TableBetweenFilterInspection run={run}/>}
  </div>;
}

export function TableBetweenFilterInspection({run}: {run: TableBetweenRun}) {
  const value = run.value, success = value.http_status === 200 && value.error === null;
  const download = (kind: "source" | "result") => {const file = tableBetweenArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel reference-table-between-result" aria-labelledby="table-between-result-heading">
    <div className="panel-head"><h2 id="table-between-result-heading">{success ? "Extracted table intervals" : "Table intervals could not be extracted"}</h2></div>
    <div className="query-results">
      {value.error !== null && <div className="error-banner" role="alert">{value.error_type !== null ? `${referenceLogFilterStringText(value.error_type)}: ` : ""}{referenceLogFilterStringText(value.error)}</div>}
      <p>Engine: {value.engine ?? "unavailable"}; stage: {value.phase}; apply returned: {value.apply_returned ? "yes" : "no"}.</p>
      {value.source && value.result && <table><caption>Source and extracted table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([['Source', value.source], ['Extracted', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count, "rows")}</td><td>{tableRepetitionCountText(count, "columns")}</td></tr>)}</tbody></table>}
      {success && value.result?.rows === 0 && <p>No complete intervals matched these endpoints.</p>}
      <p>{run.sourceFile.name} · case column: {referenceLogFilterStringText(value.case_id_key)} · activity column: {referenceLogFilterStringText(value.activity_key)}</p>
      {value.source_unchanged === true && <p>The source table was preserved.</p>}
      {value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source table. Download the response for inspection.</div>}
      <p>Downloads contain the complete native table with its stored cells and metadata.</p>
      <div className="query-actions"><button className="btn-ghost" disabled={value.source_table_json === null} onClick={() => download("source")}>Download parsed source table</button><button className="btn-open" disabled={!success || value.filtered_table_json === null} onClick={() => download("result")}>Download extracted native table</button><button className="btn-ghost" onClick={() => {const file = tableBetweenResponseFile(run); downloadQueryBlob(file, file.name);}}>Download table between response</button></div>
    </div>
  </section>;
}
