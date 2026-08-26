import {
  type CorrelationMinerAnalysis,
  type MineraExport,
  numberFormat,
} from "../types";

const activity = (data: MineraExport, id: number) =>
  data.activities[id]?.name ?? `activity ${id}`;
const key = (source: number, target: number) => `${source}:${target}`;
const seconds = (value: number | undefined) =>
  value === undefined ? "—" : `${numberFormat.format(value)}s`;

function CorrelationResult({
  data,
  analysis,
}: {
  data: MineraExport;
  analysis: CorrelationMinerAnalysis;
}) {
  const performance = new Map(
    analysis.performance_dfg.map((edge) => [
      key(edge.source, edge.target), edge.seconds,
    ]),
  );
  const durations = new Map(
    analysis.duration_matrix.map((entry) => [
      key(entry.source, entry.target), entry.value,
    ]),
  );
  if (analysis.frequency_dfg.length === 0) {
    return <p className="muted">No inferred relations.</p>;
  }
  return (
    <>
      <table className="activity-table compact">
        <thead>
          <tr>
            <th>Source</th><th>Target</th>
            <th className="num">Flow</th><th className="num">Duration</th>
          </tr>
        </thead>
        <tbody>
          {analysis.frequency_dfg.map((edge, index) => (
            <tr key={`${edge.source}-${edge.target}-${index}`}>
              <td>{activity(data, edge.source)}</td>
              <td>{activity(data, edge.target)}</td>
              <td className="num">{numberFormat.format(edge.count)}</td>
              <td className="num">
                {seconds(performance.get(key(edge.source, edge.target)))}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
      <details className="correlation-diagnostics">
        <summary>Probability and duration diagnostics</summary>
        <table className="activity-table compact">
          <thead>
            <tr>
              <th>Source</th><th>Target</th>
              <th className="num">Precede/succeed</th>
              <th className="num">Matched duration</th>
            </tr>
          </thead>
          <tbody>
            {analysis.precede_succeed_probability.map((entry) => {
              const duration = durations.get(key(entry.source, entry.target));
              return (
                <tr key={key(entry.source, entry.target)}>
                  <td>{activity(data, entry.source)}</td>
                  <td>{activity(data, entry.target)}</td>
                  <td className="num">{entry.value.toFixed(4)}</td>
                  <td className="num">{seconds(duration)}</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </details>
    </>
  );
}

export function CorrelationMining({ data }: { data: MineraExport }) {
  const analyses = data.correlation_mining?.variants;
  if (!analyses || analyses.length === 0) return null;
  return (
    <section className="panel correlation-panel reveal">
      <div className="panel-head">
        <h2>Correlation mining</h2>
        <span className="panel-meta">
          {numberFormat.format(analyses.length)} variant{analyses.length === 1 ? "" : "s"}
        </span>
      </div>
      <div className="dfg-variant-list">
        {analyses.map((analysis, index) => (
          <details className="dfg-variant-card"
            key={`${analysis.variant}-${index}`} open={index === 0}>
            <summary>{analysis.variant.replaceAll("_", " ")}</summary>
            <CorrelationResult data={data} analysis={analysis} />
          </details>
        ))}
      </div>
    </section>
  );
}
