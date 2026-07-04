import { useMemo } from "react";
import { type MineraExport, numberFormat } from "../types";
import { buildEfgMatrix } from "../eventually-follows";

// Shortens an activity name for a matrix axis; the full name stays available
// via the cell tooltips.
const short = (name: string) =>
  name.length > 14 ? `${name.slice(0, 13)}…` : name;

/**
 * The eventually-follows matrix: how often each activity (row) is followed —
 * directly or transitively — by each other activity (column) later in the
 * same case. Complements the directly-follows map with long-range order:
 * a bright cell far from the diagonal is a dependency the map's local edges
 * never show, and an empty cell means the pair never co-occurs in that order.
 */
export function EventuallyFollows({ data }: { data: MineraExport }) {
  const edges = data.eventually_follows;
  const matrix = useMemo(
    () => buildEfgMatrix(data.activities, edges ?? []),
    [data.activities, edges],
  );

  if (!edges) {
    return (
      <section className="panel efg-panel reveal">
        <div className="panel-head">
          <h2>Eventually Follows</h2>
        </div>
        <div className="model-empty">
          No eventually-follows data. This export predates the
          eventually-follows graph — re-open the event log to compute it.
        </div>
      </section>
    );
  }

  return (
    <section className="panel efg-panel reveal">
      <div className="panel-head">
        <h2>Eventually Follows</h2>
        <span className="panel-meta">
          row ↝ column · {numberFormat.format(edges.length)} observed pairs
        </span>
      </div>
      <div className="efg-scroll">
        <table className="efg-table">
          <thead>
            <tr>
              <th className="efg-corner">from ╲ to</th>
              {matrix.order.map((id) => (
                <th key={id} title={data.activities[id].name}>
                  {short(data.activities[id].name)}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {matrix.order.map((rowId, row) => (
              <tr key={rowId}>
                <th title={data.activities[rowId].name}>
                  {short(data.activities[rowId].name)}
                </th>
                {matrix.order.map((colId, col) => {
                  const count = matrix.counts[row][col];
                  return (
                    <td
                      key={colId}
                      className={count ? "efg-cell" : "efg-cell empty"}
                      title={`${data.activities[rowId].name} ↝ ${
                        data.activities[colId].name
                      }: ${numberFormat.format(count)}`}
                      style={
                        count
                          ? { "--efg": count / matrix.max } as React.CSSProperties
                          : undefined
                      }
                    >
                      {count ? numberFormat.format(count) : "·"}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}
