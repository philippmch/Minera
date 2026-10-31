import { type MineraExport, formatDuration, numberFormat } from "../types";

export function ActivityTable({ data }: { data: MineraExport }) {
  const sorted = [...data.activities].sort((a, b) => b.count - a.count);
  const maxCount = sorted[0]?.count ?? 1;
  // Older exports don't carry the rework statistic; hide the column then.
  const hasRework = sorted.some((activity) => activity.rework_cases !== undefined);
  // Mean service time per activity name, from lifecycle start/complete pairs.
  // The column only appears when the log recorded any — atomic logs stay as is.
  const serviceTimes = new Map<string, number>(
    (data.performance?.activities ?? []).map(({ activity, stats }) => [
      data.activities[activity].name,
      stats.mean,
    ]),
  );

  return (
    <section className="panel activities-panel reveal">
      <div className="panel-head">
        <h2>Activities</h2>
        <span className="panel-meta">
          {numberFormat.format(data.activities.length)} distinct
        </span>
      </div>
      <div className="activity-table-scroll">
      <table className="activity-table">
        <thead>
          <tr>
            <th>Activity</th>
            <th className="num">Frequency</th>
            <th className="bar-col"></th>
            <th className="num">Starts</th>
            <th className="num">Ends</th>
            {hasRework && (
              <th className="num" title="Cases where this activity occurs more than once">
                Rework
              </th>
            )}
            {serviceTimes.size > 0 && (
              <th
                className="num"
                title="Mean time between an activity's lifecycle start and complete events"
              >
                Service time
              </th>
            )}
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
              {hasRework && (
                <td className="num">
                  {(activity.rework_cases ?? 0) > 0
                    ? numberFormat.format(activity.rework_cases ?? 0)
                    : "·"}
                </td>
              )}
              {serviceTimes.size > 0 && (
                <td className="num">
                  {serviceTimes.has(activity.name)
                    ? formatDuration(serviceTimes.get(activity.name) ?? 0)
                    : "·"}
                </td>
              )}
            </tr>
          ))}
        </tbody>
      </table>
      </div>
    </section>
  );
}
