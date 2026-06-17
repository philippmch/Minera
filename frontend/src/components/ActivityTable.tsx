import { type MineraExport, numberFormat } from "../types";

export function ActivityTable({ data }: { data: MineraExport }) {
  const sorted = [...data.activities].sort((a, b) => b.count - a.count);
  const maxCount = sorted[0]?.count ?? 1;

  return (
    <section className="panel activities-panel reveal">
      <div className="panel-head">
        <h2>Activities</h2>
        <span className="panel-meta">
          {numberFormat.format(data.activities.length)} distinct
        </span>
      </div>
      <table className="activity-table">
        <thead>
          <tr>
            <th>Activity</th>
            <th className="num">Frequency</th>
            <th className="bar-col"></th>
            <th className="num">Starts</th>
            <th className="num">Ends</th>
          </tr>
        </thead>
        <tbody>
          {sorted.map((activity) => (
            <tr key={activity.name}>
              <td>
                {activity.name}
                {activity.start_count > 0 && <span className="tag start">entry</span>}
                {activity.end_count > 0 && <span className="tag end">exit</span>}
              </td>
              <td className="num">{numberFormat.format(activity.count)}</td>
              <td className="bar-col">
                <div className="freq-bar">
                  <i style={{ width: `${((activity.count / maxCount) * 100).toFixed(1)}%` }} />
                </div>
              </td>
              <td className="num">
                {activity.start_count > 0 ? numberFormat.format(activity.start_count) : "·"}
              </td>
              <td className="num">
                {activity.end_count > 0 ? numberFormat.format(activity.end_count) : "·"}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </section>
  );
}
