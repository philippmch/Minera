import {useEffect, useRef, useState} from "react";
import {downloadQueryBlob} from "../log-query";
import {referenceLogFilterPickedFile, referenceLogFilterStringText} from "../reference-log-filters";
import {tableRepetitionCountText, type TableRepetitionEngine} from "../table-attribute-repetition";
import {defaultTableVariantsForm, requestTableVariants, tableVariantsArtifactFile, tableVariantsResponseFile, tableVariantsSourceExample,
  tableVariantsOperations, tableVariantsMappingExample, TABLE_VARIANTS_LABELS, TableVariantsRequestGate, type TableVariantsForm, type TableVariantsRun} from "../table-variants-filters";
import {PandasVariantsOriginalCallWorkspace} from "./PandasVariantsOriginalCallWorkspace";
import {PolarsVariantsOriginalCallWorkspace} from "./PolarsVariantsOriginalCallWorkspace";

export function TableVariantsFilterWorkspace({engine,initialForm,initialCallMode="friendly"}:{engine:TableRepetitionEngine;initialForm?:TableVariantsForm;initialCallMode?:"friendly"|"original"}){
  const [mode,setMode]=useState(initialCallMode);
  useEffect(()=>{setMode(initialCallMode);},[engine,initialCallMode]);
  return <><section className="panel query-workspace"><label className="filters-field"><span>{engine==="pandas"?"Pandas":"Polars"} variants call mode</span><select value={mode} onChange={event=>setMode(event.target.value as "friendly"|"original")}><option value="friendly">Choose variants with form controls</option><option value="original">Use an original typed call</option></select></label></section>{mode==="original"?engine==="pandas"?<PandasVariantsOriginalCallWorkspace/>:<PolarsVariantsOriginalCallWorkspace/>:<TableVariantsFriendlyWorkspace key={engine} engine={engine} initialForm={initialForm}/>}</>;
}
function TableVariantsFriendlyWorkspace({engine, initialForm}: {engine: TableRepetitionEngine; initialForm?: TableVariantsForm}) {
  const [variants, setVariants] = useState<File | null>(null);
  const [source, setSource] = useState<File | null>(null), [form, setForm] = useState(() => initialForm ?? defaultTableVariantsForm(engine));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null), [run, setRun] = useState<TableVariantsRun | null>(null);
  const gate = useRef(new TableVariantsRequestGate()), request = useRef<AbortController | null>(null), current = useRef({source, variants, form}); current.current = {source, variants, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof TableVariantsForm>(key: K, value: TableVariantsForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current; if (!identity.source) return; cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity); request.current = controller; setPending(true);
    try {const result = await requestTableVariants(identity.source, identity.form, identity.form.engine === "pandas" ? identity.variants : null, controller.signal); if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);}
    catch (cause) {if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));}
    finally {if (request.current === controller) {request.current = null; setPending(false);}}
  };
  return <div className="reference-table-variants-workspace"><section className="panel query-workspace" aria-labelledby="table-variants-heading">
    <div className="panel-head"><h2 id="table-variants-heading">Filter complete activity variants</h2></div>
    <p className="query-intro">Select complete cases by their activity sequence or the frequency of their variant.</p>
    <fieldset className="query-fields"><legend>Source table</legend>
      <label className="filters-field"><span>Native EventTable JSON file</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose complete native table JSON with column types and metadata."}</small></label>
      <button className="btn-ghost" onClick={() => {const file = tableVariantsSourceExample(engine); downloadQueryBlob(file, file.name);}}>Download {engine} variants example table</button>
    </fieldset>
    <fieldset className="query-fields"><legend>Variant selection</legend>
      <label className="filters-field"><span>Variants filter</span><select value={form.operation} onChange={event => change("operation", event.target.value as TableVariantsForm["operation"])}>{tableVariantsOperations(engine).map(operation => <option value={operation} key={operation}>{TABLE_VARIANTS_LABELS[operation]}</option>)}</select></label>
      {form.operation === "membership" ? <>
        <label className="query-text"><span>Admitted variants as JSON</span><textarea rows={4} spellCheck={false} value={form.admittedJson} onChange={event => change("admittedJson", event.target.value)}/><small>{'For example [["A", "B"], ["X"]]. Empty labels, commas and escaped characters remain literal.'}</small></label>
        <label className="filters-field"><span>Inner sequence type</span><select value={form.sequenceKind} onChange={event => change("sequenceKind", event.target.value as "list" | "tuple")}><option value="tuple">Interpret inner arrays as tuples</option><option value="list">Keep inner arrays as lists</option></select></label>
        <p>{engine === "pandas" ? "Tuple sequences match pandas variants. List sequences are preserved and do not match tuple variants." : "Polars joins each admitted sequence with commas. A comma inside a label can collide with a sequence boundary."}</p>
      </> : <label className="filters-field"><span>{form.operation === "top_k" ? "Number of variants (k)" : "Coverage fraction"}</span><input spellCheck={false} value={form.criterion} onChange={event => change("criterion", event.target.value)}/><small>Exact numeric text is sent unchanged, including large integers and fractional thresholds.</small></label>}
        {engine === "pandas" && <><label className="filters-field"><span>Optional precomputed case-to-variant table</span><input type="file" accept=".json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setVariants(file));}}/><small>{variants?.name ?? "Use the table index for case identities and a variant column containing tuples."}</small></label>{variants && <button className="btn-ghost" onClick={() => edit(() => setVariants(null))}>Remove supplied variants table</button>}<button className="btn-ghost" onClick={() => {const file = tableVariantsMappingExample(); downloadQueryBlob(file, file.name);}}>Download example case-to-variant mapping</button></>}
      <div className="query-options">{([['caseKeyMode', 'caseKey', 'Case ID column', 'case:concept:name'], ['activityKeyMode', 'activityKey', 'Activity column', 'concept:name'], ['timestampKeyMode', 'timestampKey', 'Ranking timestamp column', 'time:timestamp']] as const).filter(([mode]) => mode !== "timestampKeyMode" || engine === "polars" && form.operation === "top_k").map(([mode, key, label, fallback]) => <label className="filters-field" key={key}><span>{label}</span><select value={form[mode]} onChange={event => change(mode, event.target.value as "default" | "custom")}><option value="default">{fallback}</option><option value="custom">Use a literal column name</option></select>{form[mode] === "custom" && <textarea rows={2} aria-label={`Exact ${label.toLowerCase()}`} value={form[key]} onChange={event => change(key, event.target.value)}/>}</label>)}</div>
      {engine === "polars" ? <p>Membership always orders activities by canonical time:timestamp. Top-k uses its selected timestamp for ranking, then canonical time for final membership. The pinned serialized-list and automatic helpers are unavailable: they raise an AttributeError.</p> : <p>Membership uses each case’s actual rows. Ranking and coverage use contiguous slices starting at each case’s first row; group cases together before relying on those statistics.</p>}
      <label className="filters-field"><span>Selection</span><select value={String(form.positive)} onChange={event => change("positive", event.target.value === "true")}><option value="true">Keep matching cases</option><option value="false">Keep cases outside the condition</option></select></label>
      <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Filter table variants</button>{pending && <><span role="status">Filtering variants…</span><button className="btn-ghost" onClick={cancel}>Cancel variants filtering</button></>}</div>
      {!source && <p>Choose a native table file to apply this filter.</p>}{error && <div className="error-banner" role="alert">{error}</div>}
    </fieldset>
  </section>{run && <TableVariantsFilterInspection run={run}/>}</div>;
}
export function TableVariantsFilterInspection({run}: {run: TableVariantsRun}) {
  const value = run.value, success = value.http_status === 200 && value.error === null;
  const download = (kind: "source" | "result" | "variants") => {const file = tableVariantsArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel" aria-labelledby="table-variants-result-heading"><div className="panel-head"><h2 id="table-variants-result-heading">{success ? "Filtered table variants" : "Table variants could not be filtered"}</h2></div><div className="query-results">
    {value.error !== null && <div className="error-banner" role="alert">{value.error_type !== null ? `${referenceLogFilterStringText(value.error_type)}: ` : ""}{referenceLogFilterStringText(value.error)}</div>}
    <p>Engine: {value.engine ?? "unavailable"}; filter: {value.operation ?? "unavailable"}; stage: {value.phase}; apply returned: {value.apply_returned ? "yes" : "no"}.</p>
    {value.source && value.result && <table><caption>Source and filtered table</caption><thead><tr><th scope="col">Table</th><th scope="col">Rows</th><th scope="col">Columns</th></tr></thead><tbody>{([['Source', value.source], ['Filtered', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{tableRepetitionCountText(count, "rows")}</td><td>{tableRepetitionCountText(count, "columns")}</td></tr>)}</tbody></table>}
    {success && value.result?.rows === 0 && <p>No rows matched this variant condition.</p>}
    <p>{run.sourceFile.name} · activity column: {referenceLogFilterStringText(value.attribute_key)}</p>
    {value.source_unchanged === true && <p>The source table was preserved.</p>}{value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source table. Download the response for inspection.</div>}
    <div className="query-actions">{value.source_table_json !== null && <button className="btn-ghost" onClick={() => download("source")}>Download complete parsed source table</button>}{success && value.filtered_table_json !== null && <button className="btn-ghost" onClick={() => download("result")}>Download filtered native table</button>}{value.variants_table_json !== null && <button className="btn-ghost" onClick={() => download("variants")}>Download supplied variants table</button>}<button className="btn-ghost" onClick={() => {const file = tableVariantsResponseFile(run); downloadQueryBlob(file, file.name);}}>Download variants filter response</button></div>
  </div></section>;
}
