import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile, referenceLogFilterStringText} from "../reference-log-filters";
import {TableRepetitionRequestGate, defaultTableRepetitionForm, requestTableRepetition,
  tableRepetitionArtifactFile, tableRepetitionCountText, tableRepetitionResponseFile,
  tableRepetitionSourceExample, type TableRepetitionEngine, type TableRepetitionForm, type TableRepetitionRun} from "../table-attribute-repetition";
import {TableBetweenFilterWorkspace} from "./TableBetweenFilterWorkspace";

export function TableAttributeRepetitionWorkspace({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TableRepetitionForm}) {
  const [operation, setOperation] = useState<"repetition" | "between">("repetition");
  return <>
    <label className="filters-field"><span>Table filter behavior</span><select value={operation} onChange={event => setOperation(event.target.value as "repetition" | "between")}><option value="repetition">Count an exact attribute value</option><option value="between">Extract intervals between activities</option></select></label>
    {operation === "between" ? <TableBetweenFilterWorkspace key={`${engine}-between`} engine={engine}/> : <TableAttributeRepetitionCriteria key={`${engine}-repetition`} engine={engine} initialForm={initialForm}/>}
  </>;
}

function TableAttributeRepetitionCriteria({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TableRepetitionForm}) {
  const [source, setSource] = useState<File | null>(null), [form, setForm] = useState(() => initialForm ?? defaultTableRepetitionForm(engine));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), [run, setRun] = useState<TableRepetitionRun | null>(null);
  const gate = useRef(new TableRepetitionRequestGate()), request = useRef<AbortController | null>(null);
  const current = useRef({source, form}); current.current = {source, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof TableRepetitionForm>(key: K, value: TableRepetitionForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current; if (!identity.source) return;
    cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity); request.current = controller; setPending(true);
    try {
      const result = await requestTableRepetition(identity.source, identity.form, controller.signal);
      if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);
    } catch (cause) {
      if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));
    } finally {if (request.current === controller) {request.current = null; setPending(false);}}
  };
  const name = engine === "pandas" ? "pandas dataframe" : "Polars lazy dataframe";
  return <div className="reference-table-repetition-workspace">
    <section className="panel query-workspace" aria-labelledby="table-repetition-heading">
      <div className="panel-head"><h2 id="table-repetition-heading">Count an attribute value in a {name}</h2></div>
      <p className="query-intro">Filter complete table rows by repetitions within their case. This uses the selected engine’s table comparisons and preserves the native typed table.</p>
      <fieldset className="query-fields"><legend>Source table</legend>
        <label className="filters-field"><span>Native EventTable JSON file</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose complete native table JSON, including its column types and metadata."}</small></label>
        <button className="btn-ghost" onClick={() => {const example = tableRepetitionSourceExample(engine); downloadQueryBlob(example, example.name);}}>Download {engine} example table</button>
        {engine === "polars" ? <p>Polars requires declared column types such as String, Int64 and Float64. Dataframe index metadata is rejected. The lazy filter is applied and then collected; errors identify the stage that failed.</p> : <p>pandas retains declared column types, index labels, category declarations and table attributes. Only cases with a matching value are eligible, including at a minimum of zero.</p>}
      </fieldset>
      <fieldset className="query-fields"><legend>Repetition criteria</legend>
        <div className="query-options">{([['caseKeyMode', 'caseKey', 'Case ID column', 'case:concept:name'], ['attributeKeyMode', 'attributeKey', 'Counted attribute column', 'concept:name']] as const).map(([mode, key, label, fallback]) => <label className="filters-field" key={key}><span>{label}</span><select value={form[mode]} onChange={event => change(mode, event.target.value as "default" | "custom")}><option value="default">Default: {fallback}</option><option value="custom">Use a literal column key</option></select>{form[mode] === "custom" && <textarea rows={2} aria-label={`Exact ${label.toLowerCase()} key`} value={form[key]} onChange={event => change(key, event.target.value)}/>}<small>Literal keys preserve empty strings, whitespace and newlines.</small></label>)}</div>
        <label className="query-text"><span>Exact typed value as JSON</span><textarea rows={5} spellCheck={false} value={form.targetJson} onChange={event => change("targetJson", event.target.value)}/><small>{'For example {"type":"string","value":"A"}, {"type":"integer","value":"9007199254740993"}, or {"type":"null"}. Bytes use {"type":"bytes","value":"ff00"} with hexadecimal data. Comparisons depend on the engine and declared column type. Other typed values use the existing repetition target schema.'}</small></label>
        <div className="query-options"><label className="filters-field"><span>Minimum repetitions</span><input value={form.minimum} spellCheck={false} onChange={event => change("minimum", event.target.value)}/></label><label className="filters-field"><span>Maximum repetitions</span><input value={form.maximum} spellCheck={false} onChange={event => change("maximum", event.target.value)}/></label></div>
        <p>Bounds are inclusive. Exact integer and fractional tokens are sent unchanged. Each engine applies its own null, nonfinite and invalid-bound behavior.</p>
        <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Filter table by repetitions</button>{pending && <><span role="status">Filtering table…</span><button className="btn-ghost" onClick={cancel}>Cancel table filtering</button></>}</div>
        {!source && <p>Choose a native table file to apply this filter.</p>}{error && <div className="error-banner" role="alert">{error}</div>}
      </fieldset>
    </section>
    {run && <TableAttributeRepetitionInspection run={run}/>}
  </div>;
}

export function TableAttributeRepetitionInspection({run}: {run: TableRepetitionRun}) {
  const value = run.value, success = value.http_status === 200 && value.error === null;
  const download = (kind: "source" | "result") => {const file = tableRepetitionArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel reference-table-repetition-result" aria-labelledby="table-repetition-result-heading">
    <div className="panel-head"><h2 id="table-repetition-result-heading">{success ? "Filtered table" : "Table filter could not be applied"}</h2></div>
    <div className="query-results">
      {value.error !== null && <div className="error-banner" role="alert">{value.error_type !== null ? `${referenceLogFilterStringText(value.error_type)}: ` : ""}{referenceLogFilterStringText(value.error)}</div>}
      <p>Engine: {value.engine ?? "unavailable"}; stage: {value.phase}; apply returned: {value.apply_returned ? "yes" : "no"}.</p>
      {value.source && value.result && <table><caption>Source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([['Source', value.source], ['Filtered', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count, "rows")}</td><td>{tableRepetitionCountText(count, "columns")}</td></tr>)}</tbody></table>}
      {success && value.result?.rows === 0 && <p>No rows met this engine’s repetition condition.</p>}
      <p>{run.sourceFile.name} · case column: {referenceLogFilterStringText(value.case_id_key)} · counted column: {referenceLogFilterStringText(value.attribute_key)}</p>
      {value.source_unchanged === true && <p>The source table was preserved.</p>}
      {value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source table. Download the response for inspection.</div>}
      <p>Native table JSON retains typed cells, column types, index and category metadata when supported by the selected engine. Downloads contain the complete table.</p>
      <div className="query-actions"><button className="btn-ghost" disabled={value.source_table_json === null} onClick={() => download("source")}>Download parsed source table</button><button className="btn-open" disabled={!success || value.filtered_table_json === null} onClick={() => download("result")}>Download filtered native table</button><button className="btn-ghost" onClick={() => {const file = tableRepetitionResponseFile(run); downloadQueryBlob(file, file.name);}}>Download table filter response</button></div>
    </div>
  </section>;
}
