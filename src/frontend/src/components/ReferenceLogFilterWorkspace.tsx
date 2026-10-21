import {useEffect, useRef, useState} from "react";
import {TableAttributeRepetitionWorkspace} from "./TableAttributeRepetitionWorkspace";
import type {TableRepetitionEngine} from "../table-attribute-repetition";
import {downloadQueryBlob} from "../log-query";
import {REFERENCE_LOG_FILTER_CSV_FIELDS, REFERENCE_LOG_FILTER_LABELS, REFERENCE_LOG_FILTER_OPERATIONS,
  ReferenceLogFilterRequestGate, defaultReferenceLogFilterForm, referenceLogFilterArtifactFile,
  referenceLogFilterCountText, referenceLogFilterFullBetweenFile, referenceLogFilterPickedFile, referenceLogFilterResponseFile,
  referenceLogFilterSourceExample, referenceLogFilterStringText, requestReferenceLogFilter,
  openReferenceLogFilter, type ReferenceLogFilterOpenLog,
  type ReferenceLogFilterForm, type ReferenceLogFilterOperation, type ReferenceLogFilterRun,
  type ReferenceLogFilterSourceFormat} from "../reference-log-filters";

export interface ReferenceLogFilterWorkspaceProps {
  onOpenLog?: ReferenceLogFilterOpenLog;
  initialOperation?: ReferenceLogFilterOperation;
  initialForm?: ReferenceLogFilterForm;
  initialEngine?: "log" | TableRepetitionEngine;
}
export function ReferenceLogFilterWorkspace(props: ReferenceLogFilterWorkspaceProps) {
  const [engine, setEngine] = useState<"log" | TableRepetitionEngine>(props.initialEngine ?? "log");
  return <><section className="panel query-workspace"><label className="filters-field"><span>Filter data model</span><select value={engine} onChange={event => setEngine(event.target.value as "log" | TableRepetitionEngine)}><option value="log">Event log</option><option value="pandas">pandas dataframe</option><option value="polars">Polars lazy dataframe</option></select></label></section>{engine === "log" ? <ReferenceLogFilterLogWorkspace {...props}/> : <TableAttributeRepetitionWorkspace key={engine} engine={engine}/>}</>;
}
function ReferenceLogFilterLogWorkspace({onOpenLog, initialOperation = "rework", initialForm}: ReferenceLogFilterWorkspaceProps) {
  const [source, setSource] = useState<File | null>(null);
  const [form, setForm] = useState(() => initialForm ?? defaultReferenceLogFilterForm(initialOperation));
  const [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const [run, setRun] = useState<ReferenceLogFilterRun | null>(null);
  const gate = useRef(new ReferenceLogFilterRequestGate()), request = useRef<AbortController | null>(null);
  const current = useRef({source, form}); current.current = {source, form};
  const cancel = () => {gate.current.invalidate(); request.current?.abort(); request.current = null; setPending(false);};
  const edit = (action: () => void) => {cancel(); setError(null); setRun(null); action();};
  const change = <K extends keyof ReferenceLogFilterForm>(key: K, value: ReferenceLogFilterForm[K]) => edit(() => setForm(previous => ({...previous, [key]: value})));
  const caseFilter = form.operation.startsWith("case_");
  useEffect(() => () => {gate.current.invalidate(); request.current?.abort();}, []);
  const execute = async () => {
    const identity = current.current;
    if (!identity.source) return;
    cancel(); setRun(null); setError(null);
    const controller = new AbortController(), ticket = gate.current.issue(identity);
    request.current = controller; setPending(true);
    try {
      const result = await requestReferenceLogFilter(identity.source, identity.form, controller.signal);
      if (gate.current.accepts(ticket, current.current, controller.signal)) setRun(result);
    } catch (cause) {
      if (gate.current.accepts(ticket, current.current, controller.signal)) setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      if (request.current === controller) {request.current = null; setPending(false);}
    }
  };
  const segment = form.operation === "prefix" || form.operation === "suffix";
  const target = segment || form.operation === "rework";
  const csv = form.sourceFormat === "auto" || form.sourceFormat === "csv";
  return <div className="reference-log-filter-workspace">
    <section className="panel query-workspace" aria-labelledby="reference-log-filter-heading">
      <div className="panel-head"><h2 id="reference-log-filter-heading">Filter an event log</h2></div>
      <p className="query-intro">Choose a log, define which activities or attribute values to retain, then inspect or open the filtered log.</p>
      <fieldset className="query-fields"><legend>Source</legend>
        <div className="query-options">
          <label className="filters-field"><span>Event log file</span><input type="file" accept=".csv,.xes,.xes.gz,.json" onChange={event => {const file = referenceLogFilterPickedFile(event.target); edit(() => setSource(file));}}/><small>{source ? `${source.name} (${source.size} bytes)` : "Choose CSV, XES (including .xes.gz), or full native log JSON"}</small></label>
          <label className="filters-field"><span>Source format</span><select value={form.sourceFormat} onChange={event => change("sourceFormat", event.target.value as ReferenceLogFilterSourceFormat)}><option value="auto">Detect from file</option><option value="csv">CSV</option><option value="xes">XES</option><option value="native_log">Native log JSON</option></select></label>
        </div>
        <button className="btn-ghost" onClick={() => downloadQueryBlob(referenceLogFilterSourceExample(), "log-filter-example.csv")}>Download example CSV</button>
        {csv && <details><summary>CSV columns and delimiter</summary>
          <label className="query-check"><input type="checkbox" checked={form.csvOptions} onChange={event => change("csvOptions", event.target.checked)}/>Use custom CSV settings</label>
          {form.csvOptions ? <><div className="query-options">{REFERENCE_LOG_FILTER_CSV_FIELDS.map(([key, label]) => <label className="filters-field" key={key}><span>{label}</span><input value={form.csv[key]} onChange={event => change("csv", {...form.csv, [key]: event.target.value})}/></label>)}</div><label className="query-check"><input type="checkbox" checked={form.trimFields} onChange={event => change("trimFields", event.target.checked)}/>Trim CSV field whitespace</label></> : <p>Defaults: case_id, activity, timestamp, comma delimiter. Field whitespace is preserved.</p>}
        </details>}
      </fieldset>
      <fieldset className="query-fields"><legend>Criteria</legend>
        <div className="query-options">
          <label className="filters-field"><span>Filter behavior</span><select value={form.operation} onChange={event => change("operation", event.target.value as ReferenceLogFilterOperation)}>{REFERENCE_LOG_FILTER_OPERATIONS.map(operation => <option value={operation} key={operation}>{REFERENCE_LOG_FILTER_LABELS[operation]}</option>)}</select></label>
          {!caseFilter && <label className="filters-field"><span>{form.operation === "repetition" ? "Counted attribute" : "Activity attribute"}</span><select value={form.activityKeyMode} onChange={event => change("activityKeyMode", event.target.value as "default" | "custom")}><option value="default">Use the source activity</option><option value="custom">Use an exact attribute key</option></select><small>{form.activityKeyMode === "default" ? "Uses the CSV activity column, or concept:name for XES and native logs." : "The exact key is used, including an empty key."}</small></label>}
          {!caseFilter && form.activityKeyMode === "custom" && <label className="filters-field"><span>{form.operation === "repetition" ? "Exact attribute key" : "Exact activity key"}</span><textarea rows={2} value={form.activityKey} onChange={event => change("activityKey", event.target.value)}/></label>}
        </div>
        {(form.operation === "paths" || form.operation === "path_performance") && <>
          <label className="query-text"><span>{form.operation === "paths" ? "Adjacent pairs as JSON" : "Timed pair as JSON"}</span><textarea rows={4} spellCheck={false} value={form.operation === "paths" ? form.pathsJson : form.pathJson} onChange={event => change(form.operation === "paths" ? "pathsJson" : "pathJson", event.target.value)}/><small>{form.operation === "paths" ? 'Use pairs such as [["A", "B"], ["X", "Y"]]. An empty list is valid.' : 'Use one pair such as ["A", "B"].'} Empty labels, commas, whitespace and escaped newlines remain literal.</small></label>
          {form.operation === "path_performance" && <><div className="query-options"><label className="filters-field"><span>Minimum pair seconds</span><input value={form.minPathSeconds} onChange={event => change("minPathSeconds", event.target.value)} spellCheck={false}/></label><label className="filters-field"><span>Maximum pair seconds</span><input value={form.maxPathSeconds} onChange={event => change("maxPathSeconds", event.target.value)} spellCheck={false}/></label></div><label className="filters-field"><span>Timestamp attribute</span><select value={form.timestampKeyMode} onChange={event => change("timestampKeyMode", event.target.value as "default" | "custom")}><option value="default">Use the source timestamp</option><option value="custom">Use an exact attribute key</option></select>{form.timestampKeyMode === "custom" && <textarea rows={2} aria-label="Exact path timestamp attribute" value={form.timestampKey} onChange={event => change("timestampKey", event.target.value)}/>}</label></>}
        </>}
        {caseFilter && <>
          {form.operation === "case_count" ? <label className="filters-field"><span>Maximum cases</span><input value={form.maxCases} onChange={event => change("maxCases", event.target.value)} spellCheck={false}/></label> : <div className="query-options">
            <label className="filters-field"><span>{form.operation === "case_size" ? "Minimum events" : "Minimum seconds"}</span><input value={form.operation === "case_size" ? form.minCaseSize : form.minCaseSeconds} onChange={event => change(form.operation === "case_size" ? "minCaseSize" : "minCaseSeconds", event.target.value)} spellCheck={false}/></label>
            <label className="filters-field"><span>{form.operation === "case_size" ? "Maximum events (null for no upper bound)" : "Maximum seconds"}</span><input value={form.operation === "case_size" ? form.maxCaseSize : form.maxCaseSeconds} onChange={event => change(form.operation === "case_size" ? "maxCaseSize" : "maxCaseSeconds", event.target.value)} spellCheck={false}/></label>
          </div>}
          {form.operation === "case_performance" && <label className="filters-field"><span>Timestamp attribute</span><select value={form.timestampKeyMode} onChange={event => change("timestampKeyMode", event.target.value as "default" | "custom")}><option value="default">Use the source timestamp</option><option value="custom">Use an exact attribute key</option></select>{form.timestampKeyMode === "custom" && <textarea rows={2} aria-label="Exact timestamp attribute" value={form.timestampKey} onChange={event => change("timestampKey", event.target.value)}/>}<small>Uses the CSV timestamp column, or time:timestamp for XES and native logs. Duration uses the first and last event in each trace.</small></label>}
          <p>Selected traces retain their complete events and trace metadata. These case filters return an empty log-level context.</p>
        </>}
        {form.operation === "repetition" && <>
          <label className="query-text"><span>Exact typed value as JSON</span><textarea rows={5} spellCheck={false} value={form.repetitionTargetJson} onChange={event => change("repetitionTargetJson", event.target.value)}/><small>{'For example {"type":"string","value":"web"}, {"type":"integer","value":"9007199254740993"}, or {"type":"null"}. Use {"type":"bytes","value":"ff00"} for hexadecimal bytes and {"type":"na"} for native missing values; scalar NA comparison reports an error. Also supports decimal, boolean, date, list, tuple, map, set and frozenset. Container members are typed objects.'}</small></label>
          <div className="query-options"><label className="filters-field"><span>Minimum repetitions</span><input value={form.minRep} spellCheck={false} onChange={event => change("minRep", event.target.value)}/></label><label className="filters-field"><span>Maximum repetitions</span><input value={form.maxRep} spellCheck={false} onChange={event => change("maxRep", event.target.value)}/></label></div>
          <p>Both bounds are inclusive. Missing attributes do not count. Zero counts and empty cases can match; a minimum above the maximum retains no cases. Bounds accept exact integers, fractions, booleans, NaN, INF and -INF.</p>
        </>}
        {form.operation === "between" && <>
          <div className="query-options"><label className="query-text"><span>Start activities as JSON</span><textarea rows={3} spellCheck={false} value={form.betweenStartJson} onChange={event => change("betweenStartJson", event.target.value)}/></label><label className="query-text"><span>End activities as JSON</span><textarea rows={3} spellCheck={false} value={form.betweenEndJson} onChange={event => change("betweenEndJson", event.target.value)}/></label></div>
          <p>{'Use a literal string such as "A" or an array such as ["A", "X"]. Empty strings and arrays are valid. Both boundary events are retained. A closing event starts the next cut only when both endpoints have the same JSON shape and equal values; array order and duplicates matter.'}</p>
          <div className="query-options"><label className="filters-field"><span>Subcase ID attribute</span><select value={form.betweenCaseKeyMode} onChange={event => change("betweenCaseKeyMode", event.target.value as "default" | "custom")}><option value="default">Use the source case ID</option><option value="custom">Use a literal trace attribute key</option></select>{form.betweenCaseKeyMode === "custom" && <textarea rows={2} aria-label="Exact subcase ID attribute key" value={form.betweenCaseKey} onChange={event => change("betweenCaseKey", event.target.value)}/>}</label><label className="filters-field"><span>Subcase ID separator</span><select value={form.betweenSeparatorMode} onChange={event => change("betweenSeparatorMode", event.target.value as "default" | "custom")}><option value="default">Default: ##@@</option><option value="custom">Use a literal separator</option></select>{form.betweenSeparatorMode === "custom" && <textarea rows={2} aria-label="Literal subcase separator" value={form.betweenSeparator} onChange={event => change("betweenSeparator", event.target.value)}/>}</label></div>
          <p>Every cut gets a zero-based suffix, including a single cut. Missing case IDs use the source trace position. Trace attributes survive; trace properties reset. Case-key and separator errors can occur when a cut opens, even if it never closes.</p>
        </>}
        {target && <label className="filters-field"><span>Exact activity label</span><input value={form.activity} onChange={event => change("activity", event.target.value)}/><small>Empty labels are valid. Spaces, commas, and letter case are preserved.</small></label>}
        {form.operation === "traces" && <label className="query-text"><span>Activity sequences as JSON</span><textarea rows={5} spellCheck={false} value={form.patternsJson} onChange={event => change("patternsJson", event.target.value)}/><small>{'Use an array of sequences: [["A", "B"], ["A", "...", "C"]]. The exact token "..." allows intervening activities; [] matches every trace.'}</small></label>}
        {(form.operation === "start" || form.operation === "end") && <label className="query-text"><span>Admitted activity labels as JSON</span><textarea rows={4} spellCheck={false} value={form.admittedJson} onChange={event => change("admittedJson", event.target.value)}/><small>{'Use literal labels: ["A", "A,B", ""]. Commas inside a quoted label stay part of that label. Empty traces are omitted.'}</small></label>}
        {segment ? <div className="query-options">
          <label className="filters-field"><span>Activity occurrence</span><select value={form.occurrence} onChange={event => change("occurrence", event.target.value as "first" | "last")}><option value="first">First occurrence</option><option value="last">Last occurrence</option></select></label>
          <label className="filters-field"><span>Anchor activity</span><select value={String(form.strict)} onChange={event => change("strict", event.target.value === "true")}><option value="true">Exclude the activity itself</option><option value="false">Include the activity itself</option></select></label>
        </div> : !caseFilter && form.operation !== "repetition" && form.operation !== "between" && <label className="filters-field"><span>Selection</span><select value={String(form.positive)} onChange={event => change("positive", event.target.value === "true")}><option value="true">Keep matching cases</option><option value="false">Keep cases outside the condition</option></select></label>}
        {form.operation === "rework" && <><label className="filters-field"><span>Minimum occurrences</span><input value={form.minOccurrences} onChange={event => change("minOccurrences", event.target.value)} spellCheck={false}/><small>Default 2. Exact integer tokens, fractional values, NaN, INF, and -INF are accepted.</small></label><p>The activity must occur to match, even at a minimum of zero. With NaN, cases containing the activity are excluded under either selection.</p></>}
        <div className="query-actions"><button className="btn-open" disabled={pending || source === null} onClick={() => void execute()}>Apply log filter</button>{pending && <><span role="status">Filtering log…</span><button className="btn-ghost" onClick={cancel}>Cancel filtering</button></>}</div>
        {!source && <p>Choose a source file to apply this filter.</p>}
        {error && <div className="error-banner" role="alert">{error}</div>}
      </fieldset>
    </section>
    {run && <ReferenceLogFilterInspection run={run} onOpenLog={onOpenLog}/>}
  </div>;
}
export function ReferenceLogFilterInspection({run, onOpenLog}: {run: ReferenceLogFilterRun; onOpenLog?: ReferenceLogFilterOpenLog}) {
  const value = run.value;
  const success = value.http_status === 200 && value.error === null;
  const json = success && value.filtered_log_json !== null;
  const xes = success && value.filtered_xes !== null && value.xes_error === null;
  const nativeProjection = value.source_format === "native_log";
  const download = (kind: "json" | "xes") => {const file = referenceLogFilterArtifactFile(run, kind); downloadQueryBlob(file, file.name);};
  return <section className="panel reference-log-filter-result" aria-labelledby="reference-log-filter-result-heading">
    <div className="panel-head"><h2 id="reference-log-filter-result-heading">{success ? "Filtered log" : "Filter could not be applied"}</h2></div>
    <div className="query-results">
      {value.error !== null && <div className="error-banner" role="alert">{referenceLogFilterStringText(value.error)}</div>}
      {value.source && value.result && <table className="reference-log-filter-counts"><caption>Source and result</caption><thead><tr><th scope="col">Log</th><th scope="col">Cases</th><th scope="col">Events</th></tr></thead><tbody>{([['Source', value.source], ['Filtered', value.result]] as const).map(([label, count]) => <tr key={label}><th scope="row">{label}</th><td>{referenceLogFilterCountText(count, "traces")}</td><td>{referenceLogFilterCountText(count, "events")}</td></tr>)}</tbody></table>}
      {value.result?.traces === 0 && success && <p>No cases met this selection. Adjust the criteria and apply the filter again.</p>}
      <p>{run.sourceFile.name}{value.activity_key !== null ? ` · activity attribute: ${referenceLogFilterStringText(value.activity_key)}` : ""}</p>
      {value.case_id_key != null && <p>Subcase ID attribute: {referenceLogFilterStringText(value.case_id_key)}</p>}
      {value.filter_attribute_key != null && <p>{value.operation === "repetition" ? "Counted attribute" : "Filtered attribute"}: {referenceLogFilterStringText(value.filter_attribute_key)}</p>}
      {value.source_unchanged === true && <p>The source log was preserved.</p>}
      {value.source_unchanged === false && <div className="error-banner" role="alert">The filter reported a changed source log. Download the response for inspection.</div>}
      {success && nativeProjection && <p>XES can project bytes, tuples, and sets to text. Native log JSON preserves all stored types and values.</p>}
      <div className="query-actions">{value.operation === "between" && <button className="btn-open" disabled={!success || value.full_result_json == null} onClick={() => {const file = referenceLogFilterFullBetweenFile(run); downloadQueryBlob(file, file.name);}}>Download full between result</button>}<button className="btn-ghost" disabled={!json} onClick={() => download("json")}>Download native log JSON</button><button className="btn-ghost" disabled={!xes} onClick={() => download("xes")}>Download filtered XES</button><button className="btn-open" disabled={!xes || !onOpenLog} onClick={() => {if (onOpenLog) openReferenceLogFilter(run, onOpenLog);}}>{nativeProjection ? "Open XES projection" : "Open filtered log"}</button></div>
      {value.xes_error !== null && <p role="status">XES export is unavailable: {referenceLogFilterStringText(value.xes_error)}. The native log JSON retains the filtered result.</p>}
      <details><summary>Filter response details</summary><p>Phase: {value.phase}; HTTP {value.http_status}. Source format: {value.source_format ?? "unavailable"}.</p><button className="btn-ghost" onClick={() => {const file = referenceLogFilterResponseFile(run); downloadQueryBlob(file, file.name);}}>Download complete response</button></details>
    </div>
  </section>;
}
