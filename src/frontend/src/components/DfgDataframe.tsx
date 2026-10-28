import { useState } from "react";
import {
  dfgDataframeQuery,
  requestDfgDataframe,
} from "../api";
import {
  type DfgDataframeBackend,
  type DfgDataframeConcurrentResult,
  type DfgDataframeEvent,
  type DfgDataframeGraphResult,
  type DfgDataframeMeasure,
  type DfgDataframeOperation,
  type DfgDataframeOptions,
  type DfgDataframePartialOrderResult,
  type DfgDataframePerformanceEdge,
  type DfgDataframeResult,
  numberFormat,
} from "../types";

type BusinessSchedule = "default" | "none" | "custom";

export interface DfgDataframeActiveOptions {
  graph: boolean;
  performance: boolean;
  partial: boolean;
  concurrent: boolean;
}

export function dfgDataframeActiveOptions(
  operation: DfgDataframeOperation,
  measure: DfgDataframeMeasure,
): DfgDataframeActiveOptions {
  const graph = operation === "graph";
  return {
    graph,
    performance: graph && (measure === "performance" || measure === "both" || measure === "cost"),
    partial: operation === "partial_order",
    concurrent: operation === "concurrent_events",
  };
}

const seconds = (value: number) => `${numberFormat.format(value)}s`;

function EventLabel({ event }: { event: DfgDataframeEvent }) {
  return (
    <span title={event.timestamp || undefined}>
      {event.activity || "(missing activity)"}
    </span>
  );
}

function PerformanceRows({
  edges,
  title,
  durationValues = true,
}: {
  edges: DfgDataframePerformanceEdge[];
  title: string;
  durationValues?: boolean;
}) {
  const formatValue = (value: number | null) => value === null
    ? "—"
    : durationValues ? seconds(value) : numberFormat.format(value);
  return (
    <div className="dfg-dataframe-result-table">
      <h3>{title}</h3>
      {edges.length === 0 ? <p className="muted">No weighted edges.</p> : (
        <table className="activity-table compact">
          <thead>
            <tr>
              <th>Source</th><th>Target</th><th className="num">Mean</th>
              <th className="num">Median</th><th className="num">Min–max</th>
              <th className="num">Samples</th>
            </tr>
          </thead>
          <tbody>
            {edges.map((edge, index) => (
              <tr key={`${edge.source}-${edge.target}-${index}`}>
                <td>{edge.source === null ? <span className="muted">null</span> : edge.source}</td>
                <td>{edge.target}</td>
                <td className="num">{formatValue(edge.summary.mean)}</td>
                <td className="num">{formatValue(edge.summary.median)}</td>
                <td className="num">{formatValue(edge.summary.min)}–{formatValue(edge.summary.max)}</td>
                <td className="num">{numberFormat.format(edge.summary.count)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}

function GraphResult({ result }: { result: DfgDataframeGraphResult }) {
  const frequency = result.frequency;
  const weighted = result.measure === "cost" ? result.cost : result.performance;
  return (
    <div className="dfg-dataframe-result" data-operation={result.operation}>
      <div className="panel-meta">{result.backend} · {result.measure}</div>
      {frequency !== undefined && (
        <div className="dfg-dataframe-result-table">
          <h3>Frequency edges</h3>
          {frequency.length === 0 ? <p className="muted">No frequency edges.</p> : (
            <table className="activity-table compact">
              <thead><tr><th>Source</th><th>Target</th><th className="num">Count</th></tr></thead>
              <tbody>
                {frequency.map((edge, index) => (
                  <tr key={`${edge.source}-${edge.target}-${index}`}>
                    <td>{edge.source}</td><td>{edge.target}</td>
                    <td className="num">{numberFormat.format(edge.count)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      )}
      {weighted !== undefined && (
        <PerformanceRows
          edges={weighted}
          title={result.measure === "cost" ? "Cost edges" : "Performance edges"}
          durationValues={result.measure !== "cost"}
        />
      )}
    </div>
  );
}

function PartialResult({ result }: { result: DfgDataframePartialOrderResult }) {
  return (
    <div className="dfg-dataframe-result" data-operation={result.operation}>
      <div className="panel-meta">{result.backend} · {result.pairs.length} ordered pairs</div>
      {result.pairs.length === 0 ? <p className="muted">No partial-order pairs.</p> : (
        <table className="activity-table compact">
          <thead>
            <tr><th>Source</th><th>Target</th><th>Source index</th><th>Target index</th><th className="num">Flow time</th></tr>
          </thead>
          <tbody>
            {result.pairs.map((pair, index) => (
              <tr key={`${pair.source_index_value}-${pair.target_index_value}-${index}`}>
                <td><EventLabel event={pair.source} /></td>
                <td><EventLabel event={pair.target} /></td>
                <td>{pair.source_index_value}</td>
                <td>{pair.target_index_value}</td>
                <td className="num">{seconds(pair.flow_time_seconds)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}

function ConcurrentResult({ result }: { result: DfgDataframeConcurrentResult }) {
  const columns = result.columns;
  const columnLabel = columns && [
    columns.max_start ?? columns.max_start_column,
    columns.min_complete ?? columns.min_complete_column,
    columns.overlap ?? columns.overlap_column,
  ].filter((value): value is string => value !== undefined).join(" · ");
  return (
    <div className="dfg-dataframe-result" data-operation={result.operation}>
      <div className="panel-meta">{result.backend} · {result.pairs.length} concurrent pairs</div>
      {columnLabel && <p className="muted">helper columns: {columnLabel}</p>}
      {result.pairs.length === 0 ? <p className="muted">No concurrent pairs.</p> : (
        <table className="activity-table compact">
          <thead>
            <tr><th>Left</th><th>Right</th><th>Left index</th><th>Right index</th><th className="num">Max start</th><th className="num">Min complete</th><th className="num">Overlap</th></tr>
          </thead>
          <tbody>
            {result.pairs.map((pair, index) => (
              <tr key={`${pair.left_index_value}-${pair.right_index_value}-${index}`}>
                <td><EventLabel event={pair.left} /></td>
                <td><EventLabel event={pair.right} /></td>
                <td>{pair.left_index_value}</td>
                <td>{pair.right_index_value}</td>
                <td className="num">{seconds(pair.max_start_seconds)}</td>
                <td className="num">{seconds(pair.min_complete_seconds)}</td>
                <td className="num">{seconds(pair.overlap_seconds)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}

function Result({ result }: { result: DfgDataframeResult }) {
  if (result.operation === "graph") return <GraphResult result={result} />;
  if (result.operation === "partial_order") return <PartialResult result={result} />;
  return <ConcurrentResult result={result} />;
}

export function DfgDataframeResultView({ result }: { result: DfgDataframeResult }) {
  return <Result result={result} />;
}

export function DfgDataframe() {
  const [operation, setOperation] = useState<DfgDataframeOperation>("graph");
  const [measure, setMeasure] = useState<DfgDataframeMeasure>("frequency");
  const [backend, setBackend] = useState<DfgDataframeBackend>("pandas");
  const [activityAttribute, setActivityAttribute] = useState("concept:name");
  const [targetActivityAttribute, setTargetActivityAttribute] = useState("");
  const [timestampAttribute, setTimestampAttribute] = useState("time:timestamp");
  const [startTimestampAttribute, setStartTimestampAttribute] = useState("");
  const [costAttribute, setCostAttribute] = useState("");
  const [caseIdAttribute, setCaseIdAttribute] = useState("case:concept:name");
  const [window, setWindow] = useState("1");
  const [keepOncePerCase, setKeepOncePerCase] = useState(false);
  const [performanceAggregation, setPerformanceAggregation] = useState<NonNullable<DfgDataframeOptions["performance_aggregation"]>>("mean");
  const [businessHours, setBusinessHours] = useState(false);
  const [businessSchedule, setBusinessSchedule] = useState<BusinessSchedule>("default");
  const [businessHourSlots, setBusinessHourSlots] = useState("");
  const [sortCaseIdRequired, setSortCaseIdRequired] = useState(true);
  const [sortTimestampAlongCaseId, setSortTimestampAlongCaseId] = useState(true);
  const [reduceColumns, setReduceColumns] = useState(true);
  const [workcalendar, setWorkcalendar] = useState("");
  const [eventIndexAttribute, setEventIndexAttribute] = useState("@@index");
  const [maxStartColumn, setMaxStartColumn] = useState("@@max_start_column");
  const [minCompleteColumn, setMinCompleteColumn] = useState("@@min_complete_column");
  const [overlapColumn, setOverlapColumn] = useState("@@diff_maxs_minc");
  const [strict, setStrict] = useState(false);
  const [keepFirstFollowing, setKeepFirstFollowing] = useState(true);
  const [result, setResult] = useState<DfgDataframeResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const active = dfgDataframeActiveOptions(operation, measure);

  const run = async () => {
    setError(null);
    setBusy(true);
    try {
      const options: DfgDataframeOptions = {
        operation,
        backend,
        activity_attribute: activityAttribute,
        timestamp_attribute: timestampAttribute,
        case_id_attribute: caseIdAttribute,
        sort_caseid_required: sortCaseIdRequired,
        sort_timestamp_along_case_id: sortTimestampAlongCaseId,
        reduce_columns: reduceColumns,
      };
      if (active.graph) {
        options.measure = measure;
        options.target_activity_attribute = targetActivityAttribute || undefined;
        if (measure === "cost") options.cost_attribute = costAttribute || undefined;
        options.window = Number(window);
        options.keep_once_per_case = keepOncePerCase;
        options.performance_aggregation = performanceAggregation;
      }
      if (startTimestampAttribute.trim()) options.start_timestamp_attribute = startTimestampAttribute;
      if (businessHours) {
        options.business_hours = true;
        if (businessSchedule === "none") options.business_hour_slots = "none";
        if (businessSchedule === "custom") options.business_hour_slots = businessHourSlots;
      }
      if (workcalendar.trim()) options.workcalendar = workcalendar;
      if (active.partial) {
        options.event_index_attribute = eventIndexAttribute;
        options.keep_first_following = keepFirstFollowing;
      }
      if (active.concurrent) {
        options.max_start_column = maxStartColumn;
        options.min_complete_column = minCompleteColumn;
        options.overlap_column = overlapColumn;
        options.strict = strict;
      }
      // Validate before changing the result so an invalid form leaves the
      // previous successful result visible for comparison.
      dfgDataframeQuery(options);
      setResult(await requestDfgDataframe(options));
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    } finally {
      setBusy(false);
    }
  };

  return (
    <section className="panel dfg-dataframe-panel reveal">
      <div className="panel-head">
        <h2>Dataframe DFG</h2>
        <span className="panel-meta">table-backed graph, partial order, and concurrency views</span>
      </div>
      <div className="filters-row dfg-dataframe-controls">
        <label className="filters-field">
          <span>Operation</span>
          <select value={operation} onChange={(event) => setOperation(event.target.value as DfgDataframeOperation)}>
            <option value="graph">graph</option>
            <option value="partial_order">partial order</option>
            <option value="concurrent_events">concurrent events</option>
          </select>
        </label>
        <label className="filters-field">
          <span>Backend</span>
          <select value={backend} onChange={(event) => setBackend(event.target.value as DfgDataframeBackend)}>
            <option value="pandas">pandas</option><option value="polars">polars</option>
          </select>
        </label>
        <label className="filters-field">
          <span>Measure</span>
          <select disabled={!active.graph} value={measure} onChange={(event) => setMeasure(event.target.value as DfgDataframeMeasure)}>
            <option value="frequency">frequency</option><option value="performance">performance</option>
            <option value="both">frequency + performance</option><option value="cost">cost</option>
          </select>
        </label>
      </div>
      <div className="filters-row dfg-dataframe-controls">
        <label className="filters-field"><span>Activity attribute</span><input value={activityAttribute} onChange={(event) => setActivityAttribute(event.target.value)} /></label>
        <label className="filters-field"><span>Target activity attribute <small>(optional)</small></span><input disabled={!active.graph} value={targetActivityAttribute} onChange={(event) => setTargetActivityAttribute(event.target.value)} /></label>
        <label className="filters-field"><span>Timestamp attribute</span><input value={timestampAttribute} onChange={(event) => setTimestampAttribute(event.target.value)} /></label>
        <label className="filters-field"><span>Start timestamp <small>(optional)</small></span><input value={startTimestampAttribute} onChange={(event) => setStartTimestampAttribute(event.target.value)} /></label>
        <label className="filters-field"><span>Case-id attribute</span><input value={caseIdAttribute} onChange={(event) => setCaseIdAttribute(event.target.value)} /></label>
      </div>
      <div className="filters-row dfg-dataframe-controls">
        <label className="filters-field"><span>Window</span><input type="number" min={0} step={1} disabled={!active.graph} value={window} onChange={(event) => setWindow(event.target.value)} /></label>
        <label className="filters-field"><span>Performance aggregation</span><select disabled={!active.performance} value={performanceAggregation} onChange={(event) => setPerformanceAggregation(event.target.value as NonNullable<DfgDataframeOptions["performance_aggregation"]>)}>
          {(["mean", "median", "min", "max", "stdev", "sum", "raw_values", "all"] as const).map((name) => <option key={name} value={name}>{name.replaceAll("_", " ")}</option>)}
        </select></label>
        <label className="filters-field"><span>Cost attribute</span><input disabled={!active.graph || measure !== "cost"} value={costAttribute} onChange={(event) => setCostAttribute(event.target.value)} /></label>
      </div>
      <div className="filters-row dfg-dataframe-controls">
        <label className="filters-field"><span>Business-time mode</span><label className="checkbox-field"><input type="checkbox" checked={businessHours} onChange={(event) => setBusinessHours(event.target.checked)} /> use business hours</label></label>
        <label className="filters-field"><span>Business schedule</span><select disabled={!businessHours} value={businessSchedule} onChange={(event) => setBusinessSchedule(event.target.value as BusinessSchedule)}>
          <option value="default">default weekly schedule</option><option value="none">none (empty schedule)</option><option value="custom">custom slots</option>
        </select></label>
        <label className="filters-field filters-field-wide"><span>Business-hour slots <small>(weekly seconds, start-end)</small></span><input disabled={!businessHours || businessSchedule !== "custom"} placeholder="25200-61200,111600-147600" value={businessHourSlots} onChange={(event) => setBusinessHourSlots(event.target.value)} /></label>
      </div>
      <div className="filters-row dfg-dataframe-controls">
        <label className="filters-field"><span>Ordering</span><label className="checkbox-field"><input type="checkbox" checked={sortCaseIdRequired} onChange={(event) => setSortCaseIdRequired(event.target.checked)} /> sort by case id</label><label className="checkbox-field"><input type="checkbox" checked={sortTimestampAlongCaseId} onChange={(event) => setSortTimestampAlongCaseId(event.target.checked)} /> sort timestamp within case</label></label>
        <label className="filters-field"><span>Column reduction</span><label className="checkbox-field"><input type="checkbox" checked={reduceColumns} onChange={(event) => setReduceColumns(event.target.checked)} /> retain reduced table behavior</label></label>
        <label className="filters-field"><span>Frequency retention</span><label className="checkbox-field"><input type="checkbox" disabled={!active.graph} checked={keepOncePerCase} onChange={(event) => setKeepOncePerCase(event.target.checked)} /> keep each edge once per case</label></label>
        <label className="filters-field"><span>Workcalendar token <small>(compatibility only)</small></span><input value={workcalendar} onChange={(event) => setWorkcalendar(event.target.value)} /><small>Preserved for callers; holiday/calendar effects are not applied.</small></label>
      </div>
      <div className="filters-row dfg-dataframe-controls">
        {active.partial && <label className="filters-field"><span>Event index attribute</span><input value={eventIndexAttribute} onChange={(event) => setEventIndexAttribute(event.target.value)} /></label>}
        <label className="filters-field"><span>Max-start column</span><input disabled={!active.concurrent} value={maxStartColumn} onChange={(event) => setMaxStartColumn(event.target.value)} /></label>
        <label className="filters-field"><span>Min-complete column</span><input disabled={!active.concurrent} value={minCompleteColumn} onChange={(event) => setMinCompleteColumn(event.target.value)} /></label>
        <label className="filters-field"><span>Overlap column</span><input disabled={!active.concurrent} value={overlapColumn} onChange={(event) => setOverlapColumn(event.target.value)} /></label>
        <label className="filters-field"><span>Pair options</span>{active.partial && <label className="checkbox-field"><input type="checkbox" checked={keepFirstFollowing} onChange={(event) => setKeepFirstFollowing(event.target.checked)} /> keep first following pair</label>}{active.concurrent && <label className="checkbox-field"><input type="checkbox" checked={strict} onChange={(event) => setStrict(event.target.checked)} /> strict overlap</label>}</label>
      </div>
      <div className="dfg-dataframe-actions">
        <button type="button" className="button" disabled={busy} onClick={() => void run()}>{busy ? "Running…" : "Run dataframe DFG"}</button>
        {error && <p className="error-banner" role="alert">{error}</p>}
      </div>
      {result && <Result result={result} />}
    </section>
  );
}
