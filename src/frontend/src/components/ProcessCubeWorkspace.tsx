import { useEffect, useMemo, useRef, useState } from "react";
import { CUBE_AGGREGATIONS, defaultCubeForm, cubeDefaultColumns, requestCubeFile, cubeCellMembers, cubeValueText, cubeColourScale, type CubeAxisForm, type CubeForm, type CubeRun } from "../process-cube-workspace";
import { parseNativeJson, nativeJson } from "../native-json";
import { isProcessCubeTable } from "../types";
import { ocelPreparedCsv } from "../ocel-preparation";
import { cellText } from "../transformation-workspace";
import { downloadQueryBlob } from "../log-query";

function AxisFields({ label, value, onChange }: { label: string; value: CubeAxisForm; onChange: (value: CubeAxisForm) => void }) {
  const change = <K extends keyof CubeAxisForm>(key: K, next: CubeAxisForm[K]) => onChange({ ...value, [key]: next });
  return <div className="cube-axis query-options">
    <label className="filters-field"><span>{label} dimension mode</span><select value={value.mode} onChange={event => change("mode", event.target.value as CubeAxisForm["mode"])}><option value="single">Column or prefix group</option><option value="composite">Composite dimensions</option></select></label>
    {value.mode === "single" ? <label className="filters-field"><span>{label} dimension</span><input value={value.name} onChange={event => change("name", event.target.value)} /></label>
      : <label className="filters-field"><span>{label} dimensions JSON</span><textarea rows={2} value={value.names} onChange={event => change("names", event.target.value)} /></label>}
    <label className="filters-field"><span>{label} maximum divisions</span><input type="number" min="1" max="10000" value={value.divisions} onChange={event => change("divisions", event.target.value)} /></label>
    <label className="filters-field"><span>{label} bin edges JSON (optional)</span><input placeholder="[0, 10, 20]" value={value.bins} onChange={event => change("bins", event.target.value)} /></label>
  </div>;
}
export function ProcessCubeWorkspace({ initialFile = null, busy = false }: { initialFile?: File | null; busy?: boolean }) {
  const [file, setFile] = useState<File | null>(initialFile), [form, setForm] = useState(defaultCubeForm);
  const [run, setRun] = useState<CubeRun | null>(null), [pending, setPending] = useState(false), [error, setError] = useState<string | null>(null);
  const [preview, setPreview] = useState<{ rows: number; columns: string[] } | null>(null);
  const [xPage, setXPage] = useState(0), [yPage, setYPage] = useState(0), [cell, setCell] = useState<[number, number] | null>(null), [memberPage, setMemberPage] = useState(0);
  const [selection, setSelection] = useState("cells"), [rowPage, setRowPage] = useState(0), [columnPage, setColumnPage] = useState(0);
  const request = useRef<AbortController | null>(null), uploadVersion = useRef(0);
  const cancel = () => { request.current?.abort(); request.current = null; setPending(false); };
  useEffect(() => () => { request.current?.abort(); uploadVersion.current++; }, []);
  const upload = async (next: File | null) => {
    cancel(); const version = ++uploadVersion.current; setFile(next); setRun(null); setError(null); setPreview(null);
    if (!next) return;
    try {
      const input = parseNativeJson(await next.text());
      if (version !== uploadVersion.current) return;
      if (isProcessCubeTable(input)) {
        setPreview({ rows: input.case_ids.length, columns: [...Object.keys(input.columns), ...Object.keys(input.categorical_columns ?? {})] });
        setForm(old => ({ ...old, ...cubeDefaultColumns(input) }));
      }
    } catch { /* Send original bytes to the native parser for its diagnostic. */ }
  };
  useEffect(() => { void upload(initialFile); }, [initialFile]);
  const change = <K extends keyof CubeForm>(key: K, value: CubeForm[K]) => { cancel(); setForm(old => ({ ...old, [key]: value })); setError(null); };
  const execute = async () => {
    if (!file) return; cancel(); const controller = new AbortController(); request.current = controller; setPending(true); setError(null);
    try {
      const next = await requestCubeFile(file, form, controller.signal);
      if (request.current === controller && !controller.signal.aborted) { setRun(next); setXPage(0); setYPage(0); setCell(null); setMemberPage(0); setSelection("cells"); setRowPage(0); setColumnPage(0); }
    } catch (cause) { if (request.current === controller && !controller.signal.aborted) setError(cause instanceof Error ? cause.message : String(cause)); }
    finally { if (request.current === controller) { request.current = null; setPending(false); } }
  };
  const colour = useMemo(() => run ? cubeColourScale(run.result) : () => "transparent", [run]);
  const selected = run?.tables[selection], firstColumn = columnPage * 8;
  const membership = run && cell ? cubeCellMembers(run.result, cell[0], cell[1]) : null;
  const downloadJson = (text: string, name: string) => downloadQueryBlob(new Blob([text], { type: "application/json" }), name);
  return <>
    <section className="panel process-cube-workspace">
      <div className="panel-head"><h2>Explore process cubes</h2><span className="panel-meta">Dimensions, values & case membership</span></div>
      <p>Compare two numeric, categorical or composite dimensions. Upload a table with case_ids and columns, or send a case feature matrix here from log transformation.</p>
      <fieldset className="query-fields" disabled={busy || pending}>
        <label className="filters-field"><span>Cube table JSON</span><input aria-label="Cube table JSON" type="file" accept=".json,application/json" onChange={event => void upload(event.target.files?.[0] ?? null)} /><small>{file?.name ?? "Choose a cube table; no event log is required."}</small></label>
        {preview && <p>{preview.rows} input rows · Columns: {preview.columns.join(" · ")}</p>}
        <AxisFields label="X" value={form.x} onChange={value => change("x", value)} />
        <AxisFields label="Y" value={form.y} onChange={value => change("y", value)} />
        <div className="query-options"><label className="filters-field"><span>Aggregation column</span><input value={form.value} onChange={event => change("value", event.target.value)} /></label><label className="filters-field"><span>Cube aggregation</span><select value={form.aggregation} onChange={event => change("aggregation", event.target.value as CubeForm["aggregation"])}>{CUBE_AGGREGATIONS.map(value => <option key={value} value={value}>{value}</option>)}</select></label></div>
        <p>Prefix groups use positive one-hot columns, so a row may enter several bins. Explicit edges apply to numeric axes; rows outside them are omitted. First and last follow input row order.</p>
        <button className="btn-open" disabled={!file} onClick={() => void execute()}>Run process cube</button>
      </fieldset>
      <details><summary>Cube table format</summary><pre>{'{"case_ids":["case-1","case-2"],"columns":{"cost":[10,20],"duration":[2,4],"region":["North","South"]},"categorical_columns":{"team":["A","B"]}}'}</pre><p>Every column has one value per input row. Numeric values must be finite. Use exact column names in composite JSON lists. The table accepts repeated case IDs; numeric aggregation counts rows while cell membership contains distinct IDs.</p></details>
      {pending && <p role="status">Building cube… <button className="btn-ghost" onClick={cancel}>Cancel process cube</button></p>}
      {error && <div className="error-banner" role="alert">{error}</div>}
    </section>
    {run && <section className="panel process-cube-inspection">
      <div className="panel-head"><h2>Process cube</h2><span className="panel-meta">{run.result.aggregation} · {run.source.name}</span></div>
      <p>X: {run.result.x_dimension} · Y: {run.result.y_dimension} · {run.result.x_bins.length} × {run.result.y_bins.length} bins · {run.input.case_ids.length} input rows</p>
      <p>Cell values and memberships come from this run. Null means no numeric result and is distinct from zero. Repeated case IDs can contribute several rows but appear once in each cell. The heatmap pages through ten bins per axis; all exports are complete.</p>
      <div className="query-actions"><button className="btn-ghost" onClick={() => downloadJson(run.raw, "minera-cube-data.json")}>Download native cube data</button><button className="btn-ghost" onClick={() => downloadJson(run.report_json, "minera-cube-report.json")}>Download process cube report</button>{file && <button className="btn-ghost" onClick={() => downloadQueryBlob(file, file.name)}>Download original cube table</button>}</div>
      <div className="query-table-wrap cube-heatmap"><table className="activity-table"><caption>Native cube heatmap · X columns, Y rows</caption><thead><tr><th scope="col">{run.result.y_dimension} / {run.result.x_dimension}</th>{run.result.x_bins.slice(xPage * 10, xPage * 10 + 10).map((bin, i) => <th scope="col" key={i}>X {xPage * 10 + i}: {bin.label}</th>)}</tr></thead><tbody>
        {run.result.y_bins.slice(yPage * 10, yPage * 10 + 10).map((bin, j) => { const y = yPage * 10 + j; return <tr key={y}><th scope="row">Y {y}: {bin.label}</th>{run.result.x_bins.slice(xPage * 10, xPage * 10 + 10).map((_, i) => { const x = xPage * 10 + i, value = run.result.cells[x][y], ids = run.result.case_ids[x][y]; return <td key={x}><button className="cube-cell" aria-pressed={cell?.[0] === x && cell?.[1] === y} aria-label={"Cube cell X " + x + " Y " + y + ": " + cubeValueText(run.result, x, y) + "; " + ids.length + " distinct case IDs"} style={{ backgroundColor: colour(value) }} onClick={() => { setCell([x, y]); setMemberPage(0); }}><strong>{cubeValueText(run.result, x, y)}</strong><small>{ids.length} distinct IDs</small></button></td>; })}</tr>; })}
      </tbody></table></div>
      {(["x", "y"] as const).map(axis => { const bins = axis === "x" ? run.result.x_bins : run.result.y_bins, page = axis === "x" ? xPage : yPage, setPage = axis === "x" ? setXPage : setYPage; return bins.length > 10 && <div className="query-actions" key={axis}><button className="btn-ghost" disabled={!page} onClick={() => setPage(p => p - 1)}>Previous {axis.toUpperCase()} bins</button><span>{axis.toUpperCase()} bins {page * 10 + 1}–{Math.min(bins.length, page * 10 + 10)} / {bins.length}</span><button className="btn-ghost" disabled={(page + 1) * 10 >= bins.length} onClick={() => setPage(p => p + 1)}>Next {axis.toUpperCase()} bins</button></div>; })}
      {!run.result.cells.length || !run.result.y_bins.length ? <p>No native bins for this input; empty schemas remain available.</p> : null}
      {membership && cell && <div className="cube-members"><h3>Selected cell · X {cell[0]} · Y {cell[1]}</h3><p>Value: {cubeValueText(run.result, cell[0], cell[1])} · {membership.count} distinct case IDs</p><button className="btn-ghost" onClick={() => downloadQueryBlob(ocelPreparedCsv(membership), "minera-cube-selected-members.csv")}>Download selected cell membership</button><ul>{Array.from({ length: Math.max(0, Math.min(25, membership.count - memberPage * 25)) }, (_, i) => <li key={i}>{cellText(membership.rowAt(memberPage * 25 + i)[5])}</li>)}</ul>{!membership.count && <p>No native members in this cell.</p>}{membership.count > 25 && <div className="query-actions"><button className="btn-ghost" disabled={!memberPage} onClick={() => setMemberPage(p => p - 1)}>Previous cell members</button><span>Members {memberPage * 25 + 1}–{Math.min(membership.count, memberPage * 25 + 25)} / {membership.count}</span><button className="btn-ghost" disabled={(memberPage + 1) * 25 >= membership.count} onClick={() => setMemberPage(p => p + 1)}>Next cell members</button></div>}</div>}
      <div className="query-actions">{Object.entries(run.tables).map(([name, table]) => <button key={name} className={name === selection ? "btn-open" : "btn-ghost"} onClick={() => { setSelection(name); setRowPage(0); setColumnPage(0); }}>{name} · {table.count} rows</button>)}</div>
      {selected && <><button className="btn-ghost cube-export" onClick={() => downloadQueryBlob(ocelPreparedCsv(selected), "minera-cube-" + selection + ".csv")}>Download complete cube CSV</button><div className="query-table-wrap"><table className="activity-table"><thead><tr>{selected.columns.slice(firstColumn, firstColumn + 8).map((name, i) => <th key={i}>{name}</th>)}</tr></thead><tbody>{Array.from({ length: Math.max(0, Math.min(25, selected.count - rowPage * 25)) }, (_, i) => <tr key={i}>{selected.rowAt(rowPage * 25 + i).slice(firstColumn, firstColumn + 8).map((value, k) => <td key={k}>{cellText(value)}</td>)}</tr>)}</tbody></table></div>{!selected.count && <p>No rows in this category; its CSV schema remains available.</p>}
        {selected.count > 25 && <div className="query-actions"><button className="btn-ghost" disabled={!rowPage} onClick={() => setRowPage(p => p - 1)}>Previous cube rows</button><span>Rows {rowPage * 25 + 1}–{Math.min(selected.count, rowPage * 25 + 25)} / {selected.count}</span><button className="btn-ghost" disabled={(rowPage + 1) * 25 >= selected.count} onClick={() => setRowPage(p => p + 1)}>Next cube rows</button></div>}
        {selected.columns.length > 8 && <div className="query-actions"><button className="btn-ghost" disabled={!columnPage} onClick={() => setColumnPage(p => p - 1)}>Previous cube columns</button><span>Columns {firstColumn + 1}–{Math.min(selected.columns.length, firstColumn + 8)} / {selected.columns.length}</span><button className="btn-ghost" disabled={firstColumn + 8 >= selected.columns.length} onClick={() => setColumnPage(p => p + 1)}>Next cube columns</button></div>}
      </>}
      <details><summary>Process cube request and upload binding</summary><pre>{JSON.stringify({ source: run.source, query: run.query }, null, 2)}</pre></details>
      <details><summary>Complete native cube response</summary><pre>{nativeJson(run.result, 2)}</pre></details>
    </section>}
  </>;
}
