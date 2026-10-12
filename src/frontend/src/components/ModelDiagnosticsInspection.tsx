import { useMemo, useState } from "react";
import { diagnosticCell, diagnosticComparison, diagnosticComparisonReport, diagnosticCsv,
  diagnosticPage, diagnosticReport, diagnosticTables, type DiagnosticEntry, type DiagnosticTable } from "../model-diagnostics";
import { nativeJson } from "../native-json";
import { nativeResponseJson, nativeResponseRequest } from "../native-response";
import { downloadQueryBlob } from "../log-query";

export function ModelDiagnosticsInspection({ entries }: { entries: DiagnosticEntry[] }) {
  const [selected, setSelected] = useState<number>();
  const entry = entries.find(item => item.id === selected) ?? entries.at(-1);
  const comparison = useMemo(() => diagnosticComparison(entries), [entries]);
  if (!entry) return null;
  const download = (text: string, name: string, type: string) => downloadQueryBlob(new Blob([text], {type}), name);
  return <section className="panel" aria-label="Complete model diagnostics inspection">
    <div className="panel-head"><h2>Complete model diagnostics</h2><span className="panel-meta">{entries.length} saved results · {entry.dataset}</span></div>
    <p>Compare saved runs and inspect every returned field. Uncomputed results, errors, null values and empty tables stay visible. Indices are zero-based.</p>
    <div className="query-actions">
      <button className="btn-ghost" onClick={() => download(diagnosticComparisonReport(entries), "minera-diagnostics-comparison.json", "application/json")}>Download saved comparisons JSON</button>
      <button className="btn-ghost" onClick={() => download(diagnosticCsv(comparison), "minera-diagnostics-comparison.csv", "text/csv;charset=utf-8")}>Download saved comparisons CSV</button>
    </div>
    <details><summary>Compare all saved diagnostics</summary><DiagnosticTableView key={entries.length} table={comparison} /></details>
    <label className="filters-field"><span>Diagnostic result to inspect</span><select value={entry.id} onChange={event => setSelected(Number(event.target.value))}>
      {entries.map(item => <option key={item.id} value={item.id}>#{item.id} · {item.label} · {item.model}</option>)}
    </select></label>
    <DiagnosticResult key={entry.id} entry={entry} />
  </section>;
}
function DiagnosticResult({ entry }: { entry: DiagnosticEntry }) {
  const tables = useMemo(() => diagnosticTables(entry), [entry]);
  const [tableId, setTableId] = useState("summary");
  const table = tables.find(item => item.id === tableId) ?? tables[0];
  const download = (raw: string, suffix: string, type: string) => downloadQueryBlob(new Blob([raw], {type}), `minera-diagnostic-${entry.id}-${suffix}`);
  const request = nativeResponseRequest(entry.result);
  return <>
    <p>{entry.label} · {entry.model} · model generation {entry.model_generation}. {request ? "Captured native HTTP response." : "Result embedded in the analysis; no submitted request was captured."}</p>
    {request && <details><summary>Exact submitted diagnostics request</summary><pre>{nativeJson(request,2)}</pre></details>}
    <div className="query-actions">
      <button className="btn-ghost" onClick={() => download(nativeResponseJson(entry.result), "native.json", "application/json")}>Download complete diagnostics native JSON</button>
      <button className="btn-ghost" onClick={() => download(diagnosticReport(entry), "report.json", "application/json")}>Download diagnostics report</button>
      <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), encodeURIComponent(table.id) + ".csv", "text/csv;charset=utf-8")}>Download complete diagnostics table CSV</button>
    </div>
    <label className="filters-field"><span>Diagnostic result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>
      {tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}
    </select></label>
    <DiagnosticTableView key={table.id} table={table} />
    <details><summary>Complete diagnostics native result JSON</summary><pre>{nativeResponseJson(entry.result)}</pre></details>
  </>;
}
export function DiagnosticTableView({ table, noun = "diagnostic" }: { table: DiagnosticTable; noun?: string }) {
  const [page, setPage] = useState(0); const [columnPage, setColumnPage] = useState(0); const [rowIndex, setRowIndex] = useState(0);
  const pages = Math.max(1, Math.ceil(table.rows.length/25)), columnPages = Math.max(1, Math.ceil(table.columns.length/8));
  const columns = diagnosticPage(table.columns, columnPage, 8);
  const display = (value: unknown) => {
    const text = value === undefined ? "(absent)" : value === "" ? '""' : diagnosticCell(value);
    return text.length > 240 ? text.slice(0,240) + "… (full value in selected row JSON)" : text;
  };
  return <>
    <p>{table.rows.length} rows · {table.columns.length} columns. CSV includes every row and column. Long cells are previewed here; selected row JSON contains their complete values.</p>
    <div className="query-table-wrap"><table className="activity-table"><caption>{table.title}</caption><thead><tr>{columns.map(column => <th key={column} scope="col">{column}</th>)}</tr></thead><tbody>
      {diagnosticPage(table.rows,page).map((row,i) => <tr key={page*25+i}>{columns.map((column,j) => <td key={column}>{display(row[columnPage*8+j])}</td>)}</tr>)}
    </tbody></table>{!table.rows.length && <p>No rows were returned for this table.</p>}</div>
    <div className="query-actions" aria-label={`${noun} table paging`}>
      <button className="btn-ghost" disabled={page===0} onClick={() => setPage(value => value-1)}>Previous {noun} rows</button><span>Page {page+1} of {pages}</span><button className="btn-ghost" disabled={page+1>=pages} onClick={() => setPage(value => value+1)}>Next {noun} rows</button>
      {columnPages>1 && <><button className="btn-ghost" disabled={columnPage===0} onClick={() => setColumnPage(value => value-1)}>Previous {noun} columns</button><span>Columns {columnPage*8+1}–{Math.min((columnPage+1)*8,table.columns.length)} of {table.columns.length}</span><button className="btn-ghost" disabled={columnPage+1>=columnPages} onClick={() => setColumnPage(value => value+1)}>Next {noun} columns</button></>}
    </div>
    {table.records && table.records.length>0 && <details><summary>Complete selected {noun} row JSON</summary>
      <label className="filters-field"><span>{noun[0].toUpperCase()+noun.slice(1)} row index (0–{table.records.length-1})</span><input type="number" min={0} max={table.records.length-1} value={rowIndex} onChange={event => { const value=Number(event.target.value);if(Number.isInteger(value)&&value>=0&&value<table.records!.length)setRowIndex(value); }} /></label>
      <pre>{nativeJson(table.records[rowIndex],2)}</pre>
    </details>}
  </>;
}
