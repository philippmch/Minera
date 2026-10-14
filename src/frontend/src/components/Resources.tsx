import { type MineraExport, numberFormat } from "../types";
import { SocialNetwork } from "./SocialNetwork";
import { OrganizationalWorkspace } from "./OrganizationalWorkspace";

// The organizational perspective: who performs the work, and how cases are
// handed from one resource to the next (the handover-of-work social network).
export function Resources({ data, available = false, mining = false }: { data: MineraExport; available?: boolean; mining?: boolean }) {
  const network = data.resources;
  if (!network || network.resources.length === 0) {
    return (
      <div className="layout-resources"><div className="organizational-workspace"><OrganizationalWorkspace data={data} available={available} mining={mining} /></div><SocialNetwork available={available} mining={mining} /><section className="panel map-panel reveal">
        <div className="model-empty">
          No resource information in this log. Add a <code>resource</code> column
          (CSV) or an <code>org:resource</code> attribute (XES) to see who
          performs each activity and how work is handed over.
        </div>
      </section></div>
    );
  }

  const resources = network.resources
    .map((r, id) => ({ ...r, id }))
    .sort((a, b) => b.event_count - a.event_count);
  const maxEvents = resources[0]?.event_count ?? 1;

  const handovers = network.handovers
    .map((h) => ({
      from: network.resources[h.source].name,
      to: network.resources[h.target].name,
      self: h.source === h.target,
      count: h.count,
    }))
    .sort((a, b) => b.count - a.count);
  const maxHandover = handovers[0]?.count ?? 1;

  return (
    <div className="layout-resources">
      <div className="organizational-workspace"><OrganizationalWorkspace data={data} available={available} mining={mining} /></div>
      <SocialNetwork available={available} mining={mining} />
      <section className="panel activities-panel reveal">
        <div className="panel-head">
          <h2>Resources</h2>
          <span className="panel-meta">
            {numberFormat.format(resources.length)} distinct
          </span>
        </div>
        <table className="activity-table">
          <thead>
            <tr>
              <th>Resource</th>
              <th className="num">Events</th>
              <th className="bar-col"></th>
            </tr>
          </thead>
          <tbody>
            {resources.map((resource) => (
              <tr key={resource.name}>
                <td>{resource.name}</td>
                <td className="num">{numberFormat.format(resource.event_count)}</td>
                <td className="bar-col">
                  <div className="freq-bar">
                    <i
                      style={{
                        width: `${((resource.event_count / maxEvents) * 100).toFixed(1)}%`,
                      }}
                    />
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>

      <section className="panel activities-panel reveal">
        <div className="panel-head">
          <h2>Handovers of work</h2>
          <span className="panel-meta">
            {numberFormat.format(network.total_handovers)} total
          </span>
        </div>
        {handovers.length === 0 ? (
          <div className="model-empty">
            No handovers — every case stays with a single resource.
          </div>
        ) : (
          <table className="activity-table">
            <thead>
              <tr>
                <th>From → To</th>
                <th className="num">Count</th>
                <th className="bar-col"></th>
              </tr>
            </thead>
            <tbody>
              {handovers.map((handover) => (
                <tr key={`${handover.from}>${handover.to}`}>
                  <td>
                    {handover.from} <span className="trail-sep">→</span>{" "}
                    {handover.to}
                    {handover.self && <span className="tag">self</span>}
                  </td>
                  <td className="num">{numberFormat.format(handover.count)}</td>
                  <td className="bar-col">
                    <div className="freq-bar">
                      <i
                        style={{
                          width: `${((handover.count / maxHandover) * 100).toFixed(1)}%`,
                        }}
                      />
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </section>
    </div>
  );
}
