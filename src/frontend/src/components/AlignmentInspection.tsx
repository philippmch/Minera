import { useMemo, useRef, useState } from "react";
import { alignmentCell, alignmentCsv, alignmentJson, alignmentReport, alignmentTables,
  alignmentTraceRows, alignmentMoves, alignmentPage, type AlignmentEntry, type AlignmentKind } from "../alignment-inspection";
import { nativeJson } from "../native-json";
import { downloadQueryBlob } from "../log-query";

export function AlignmentInspection({ entries, selected, onSelect, dataset }: {
  entries: AlignmentEntry[]; selected: AlignmentKind; onSelect: (kind: AlignmentKind) => void; dataset: string;
}) {
  const entry = entries.find(item => item.kind === selected) ?? entries[0];
  if (!entry) return null;
  return <section className="alignment-inspection" aria-label="Complete alignment inspection">
    <div className="panel-head"><h3>Complete alignment results</h3><span className="panel-meta">{dataset}</span></div>
    <label className="filters-field"><span>Result to inspect</span><select value={entry.kind} onChange={event => onSelect(event.target.value as AlignmentKind)}>
      {entries.map(item => <option key={item.kind} value={item.kind}>{item.label}</option>)}
    </select></label>
    <AlignmentResult key={entry.kind} entry={entry} dataset={dataset} />
  </section>;
}

function AlignmentResult({ entry, dataset }: { entry: AlignmentEntry; dataset: string }) {
  const tables = useMemo(() => alignmentTables(entry), [entry.result]);
  const identity = useRef({ result: entry.result, id: 0 });
  if (identity.current.result !== entry.result) identity.current = { result: entry.result, id: identity.current.id + 1 };
  // Each returned result owns its inspection state, so new runs start on the first row.
  return <ResultTables key={identity.current.id} entry={entry} dataset={dataset} tables={tables} />;
}
function ResultTables({ entry, dataset, tables }: { entry: AlignmentEntry; dataset: string; tables: ReturnType<typeof alignmentTables> }) {
  const [tableId, setTableId] = useState(tables.some(table => table.id === "traces") ? "traces" : "summary");
  const [page, setPage] = useState(0); const [columnPage, setColumnPage] = useState(0); const [traceIndex, setTraceIndex] = useState(0); const [movePage, setMovePage] = useState(0);
  const table = tables.find(item => item.id === tableId) ?? tables[0];
  const traces = alignmentTraceRows(entry.result); const trace = traces[traceIndex]; const moves = trace ? alignmentMoves(trace) : [];
  const download = (raw: string, suffix: string, type: string) => downloadQueryBlob(new Blob([raw], { type }), `minera-${entry.kind}-${suffix}`);
  const pages = Math.max(1, Math.ceil(table.rows.length / 25)); const columnPages = Math.max(1, Math.ceil(table.columns.length / 8));
  const columns = alignmentPage(table.columns, columnPage, 8);
  const display = (value: unknown) => value === undefined ? "(absent)" : value === "" ? '""' : alignmentCell(value);
  return <>
    <p>Every returned entry is available below. Row indices are zero-based. Decomposed entries retain their native case counts; a blank or incomplete result stays visible.</p>
    <div className="query-actions">
      <button className="btn-ghost" onClick={() => download(alignmentJson(entry.result), "native.json", "application/json")}>Download complete native JSON</button>
      <button className="btn-ghost" onClick={() => download(alignmentReport(entry, dataset), "report.json", "application/json")}>Download alignment report</button>
      <button className="btn-ghost" onClick={() => download(alignmentCsv(table), table.id + ".csv", "text/csv;charset=utf-8")}>Download complete table CSV</button>
    </div>
    <label className="filters-field"><span>Result table</span><select value={table.id} onChange={event => { setTableId(event.target.value); setPage(0); setColumnPage(0); }}>
      {tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}
    </select></label>
    <p>{table.rows.length} rows · {table.columns.length} columns. CSV downloads include every row and column, including nested attributes as JSON.</p>
    <div className="query-table-wrap"><table className="activity-table"><caption>{table.title}</caption><thead><tr>{columns.map(column => <th key={column} scope="col">{column}</th>)}</tr></thead>
      <tbody>{alignmentPage(table.rows, page).map((row, i) => <tr key={page * 25 + i}>{columns.map((column, j) => <td key={column}>{display(row[columnPage * 8 + j])}</td>)}</tr>)}</tbody>
    </table>{!table.rows.length && <p>No rows were returned for this table.</p>}</div>
    <div className="query-actions" aria-label="Result table paging">
      <button className="btn-ghost" disabled={page === 0} onClick={() => setPage(value => value - 1)}>Previous result rows</button><span>Page {page + 1} of {pages}</span><button className="btn-ghost" disabled={page + 1 >= pages} onClick={() => setPage(value => value + 1)}>Next result rows</button>
      {columnPages > 1 && <><button className="btn-ghost" disabled={columnPage === 0} onClick={() => setColumnPage(value => value - 1)}>Previous result columns</button><span>Columns {columnPage * 8 + 1}–{Math.min((columnPage + 1) * 8, table.columns.length)} of {table.columns.length}</span><button className="btn-ghost" disabled={columnPage + 1 >= columnPages} onClick={() => setColumnPage(value => value + 1)}>Next result columns</button></>}
    </div>
    {traces.length > 0 && <details open><summary>Inspect one alignment entry and its moves</summary>
      <label className="filters-field"><span>Alignment entry index (0–{traces.length - 1})</span><input type="number" min={0} max={traces.length - 1} value={traceIndex} onChange={event => { const value = Number(event.target.value); if (Number.isInteger(value) && value >= 0 && value < traces.length) { setTraceIndex(value); setMovePage(0); } }} /></label>
      <p>Entry #{traceIndex} · {moves.length} moves{entry.kind === "decomposed" ? ` · native case count ${display(trace.count)}` : ""}</p>
      <div className="align-moves">{alignmentPage(moves, movePage, 100).map((move, i) => <span className="align-move" key={movePage * 100 + i}>{alignmentCell(move)}</span>)}</div>
      {!moves.length && <p>No moves were returned for this entry.</p>}
      {moves.length > 100 && <div className="query-actions"><button className="btn-ghost" disabled={!movePage} onClick={() => setMovePage(value => value - 1)}>Previous entry moves</button><span>Move page {movePage + 1} of {Math.ceil(moves.length / 100)}</span><button className="btn-ghost" disabled={(movePage + 1) * 100 >= moves.length} onClick={() => setMovePage(value => value + 1)}>Next entry moves</button></div>}
      <details><summary>Complete selected entry JSON</summary><pre>{nativeJson(trace, 2)}</pre></details>
    </details>}
    <details><summary>Complete native result JSON</summary><pre>{alignmentJson(entry.result)}</pre></details>
  </>;
}
