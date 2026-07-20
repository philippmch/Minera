import {
  type DfgNamedAttributeCounts,
  type DfgStatisticalAnalysis,
  type EdgeStat,
  type MineraExport,
  numberFormat,
} from "../types";

const label = (value: string) => value.replaceAll("_", " ");
const activity = (data: MineraExport, id: number) =>
  data.activities[id]?.name ?? `activity ${id}`;
const seconds = (value: number) => `${numberFormat.format(value)}s`;

function EdgeRows({
  data,
  edges,
}: {
  data: MineraExport;
  edges: EdgeStat[];
}) {
  if (edges.length === 0) return <p className="muted">No relations.</p>;
  return (
    <table className="activity-table compact">
      <thead>
        <tr><th>Source</th><th>Target</th><th className="num">Count</th></tr>
      </thead>
      <tbody>
        {edges.map((edge, index) => (
          <tr key={`${edge.source}-${edge.target}-${index}`}>
            <td>{activity(data, edge.source)}</td>
            <td>{activity(data, edge.target)}</td>
            <td className="num">{numberFormat.format(edge.count)}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

function AttributeValues({ attributes }: { attributes: DfgNamedAttributeCounts[] }) {
  return attributes.length === 0 ? (
    <span className="muted">none</span>
  ) : (
    <span>
      {attributes.map((entry) =>
        `${entry.attribute}: ${entry.values
          .map((value) => `${value.value} (${numberFormat.format(value.count)})`)
          .join(", ")}`,
      ).join(" · ")}
    </span>
  );
}

function StatisticalResult({
  data,
  analysis,
}: {
  data: MineraExport;
  analysis: DfgStatisticalAnalysis;
}) {
  if (analysis.frequency_dfg) {
    return <EdgeRows data={data} edges={analysis.frequency_dfg} />;
  }
  if (analysis.frequency_triples) {
    return analysis.frequency_triples.length === 0 ? (
      <p className="muted">No triples.</p>
    ) : (
      <table className="activity-table compact">
        <thead>
          <tr><th>First</th><th>Middle</th><th>Last</th><th className="num">Count</th></tr>
        </thead>
        <tbody>
          {analysis.frequency_triples.map((triple, index) => (
            <tr key={`${triple.first}-${triple.middle}-${triple.last}-${index}`}>
              <td>{activity(data, triple.first)}</td>
              <td>{activity(data, triple.middle)}</td>
              <td>{activity(data, triple.last)}</td>
              <td className="num">{numberFormat.format(triple.count)}</td>
            </tr>
          ))}
        </tbody>
      </table>
    );
  }
  if (analysis.performance_dfg) {
    return analysis.performance_dfg.length === 0 ? (
      <p className="muted">No timed relations.</p>
    ) : (
      <table className="activity-table compact">
        <thead>
          <tr>
            <th>Source</th><th>Target</th><th className="num">Mean</th>
            <th className="num">Median</th><th className="num">Min–max</th>
            <th className="num">Samples</th>
          </tr>
        </thead>
        <tbody>
          {analysis.performance_dfg.map((edge, index) => (
            <tr key={`${edge.source}-${edge.target}-${index}`}>
              <td>{activity(data, edge.source)}</td>
              <td>{activity(data, edge.target)}</td>
              <td className="num">{seconds(edge.summary.mean)}</td>
              <td className="num">{seconds(edge.summary.median)}</td>
              <td className="num">{seconds(edge.summary.min)}–{seconds(edge.summary.max)}</td>
              <td className="num">{numberFormat.format(edge.summary.count)}</td>
            </tr>
          ))}
        </tbody>
      </table>
    );
  }
  if (analysis.case_attributes) {
    return (
      <div className="dfg-variant-tables">
        <table className="activity-table compact">
          <thead><tr><th>Edge</th><th>Case attributes</th></tr></thead>
          <tbody>
            {analysis.case_attributes.edges.map((edge, index) => (
              <tr key={`${edge.source}-${edge.target}-${index}`}>
                <td>{activity(data, edge.source)} → {activity(data, edge.target)}</td>
                <td><AttributeValues attributes={edge.attributes} /></td>
              </tr>
            ))}
          </tbody>
        </table>
        {analysis.case_attributes.nodes.length > 0 && (
          <table className="activity-table compact">
            <thead><tr><th>Activity</th><th>Case attributes</th></tr></thead>
            <tbody>
              {analysis.case_attributes.nodes.map((node) => (
                <tr key={node.activity}>
                  <td>{activity(data, node.activity)}</td>
                  <td><AttributeValues attributes={node.attributes} /></td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    );
  }
  if (analysis.clean) {
    return (
      <>
        <div className="dfg-boundaries">
          <span>starts: {analysis.clean.start_activities.map((item) =>
            `${activity(data, item.activity)} (${numberFormat.format(item.count)})`,
          ).join(", ") || "none"}</span>
          <span>ends: {analysis.clean.end_activities.map((item) =>
            `${activity(data, item.activity)} (${numberFormat.format(item.count)})`,
          ).join(", ") || "none"}</span>
        </div>
        <EdgeRows data={data} edges={analysis.clean.dfg} />
      </>
    );
  }
  if (analysis.clean_time) {
    return analysis.clean_time.length === 0 ? (
      <p className="muted">No timed activities.</p>
    ) : (
      <table className="activity-table compact">
        <thead><tr><th>Activity</th><th className="num">Mean from case start</th></tr></thead>
        <tbody>
          {analysis.clean_time.map((item) => (
            <tr key={item.activity}>
              <td>{activity(data, item.activity)}</td>
              <td className="num">{seconds(item.mean_seconds_from_case_start)}</td>
            </tr>
          ))}
        </tbody>
      </table>
    );
  }
  return <p className="muted">No result artifact.</p>;
}

export function DfgVariants({ data }: { data: MineraExport }) {
  const variants = data.dfg_variants;
  if (!variants || (variants.statistical.length === 0 && variants.causal.length === 0)) {
    return null;
  }
  return (
    <section className="panel dfg-variants-panel reveal">
      <div className="panel-head">
        <h2>DFG variants</h2>
        <span className="panel-meta">
          {numberFormat.format(variants.statistical.length)} statistical ·{" "}
          {numberFormat.format(variants.causal.length)} causal
        </span>
      </div>
      <div className="dfg-variant-list">
        {variants.statistical.map((analysis, index) => (
          <details className="dfg-variant-card" key={`${analysis.variant}-${index}`} open={index === 0}>
            <summary>{label(analysis.variant)}</summary>
            <StatisticalResult data={data} analysis={analysis} />
          </details>
        ))}
        {variants.causal.map((analysis, index) => (
          <details className="dfg-variant-card" key={`causal-${analysis.variant}-${index}`}>
            <summary>causal · {label(analysis.variant)}</summary>
            {analysis.relations.length === 0 ? <p className="muted">No causal relations.</p> : (
              <table className="activity-table compact">
                <thead><tr><th>Source</th><th>Target</th><th className="num">Strength</th></tr></thead>
                <tbody>
                  {analysis.relations.map((relation, relationIndex) => (
                    <tr key={`${relation.source}-${relation.target}-${relationIndex}`}>
                      <td>{activity(data, relation.source)}</td>
                      <td>{activity(data, relation.target)}</td>
                      <td className="num">{relation.value.toFixed(4)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </details>
        ))}
      </div>
    </section>
  );
}
