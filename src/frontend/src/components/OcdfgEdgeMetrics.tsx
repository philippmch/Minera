import { useEffect, useMemo, useState } from "react";
import { type OcdfgEdgeMetricsEdge, type OcdfgEdgeMetricsReport } from "../types";
import { OCDFG_METRIC_PROJECTIONS, downloadOcdfgEdgeMetrics, ocdfgMetricColumns,
  ocdfgMetricEdges, ocdfgMetricPage, ocdfgMetricRows, type OcdfgMetricProjection } from "../ocdfg-edge-metrics";

function Literal({value}:{value:string}) {
  return <code style={{whiteSpace:"pre-wrap",overflowWrap:"anywhere"}}>{JSON.stringify(value)}</code>;
}

function Pages({page,pageCount,total,noun,onPage}:{page:number;pageCount:number;total:number;noun:string;onPage:(page:number)=>void}) {
  return pageCount > 1 && <div className="query-actions" aria-label={`${noun} pagination`}>
    <button className="btn-ghost" type="button" disabled={page===0} onClick={()=>onPage(page-1)}>Previous {noun}</button>
    <span>Page {page+1} of {pageCount} · {total} {noun}</span>
    <button className="btn-ghost" type="button" disabled={page+1===pageCount} onClick={()=>onPage(page+1)}>Next {noun}</button>
  </div>;
}

export function OcdfgEdgeMetricRows({edge,projection,page,onPage}:{edge:OcdfgEdgeMetricsEdge;projection:OcdfgMetricProjection;page:number;onPage:(page:number)=>void}) {
  const shown = ocdfgMetricPage(ocdfgMetricRows(edge,projection),page);
  return <>
    <p className="panel-meta">{shown.total} rows · showing {shown.total ? shown.offset+1 : 0}–{Math.min(shown.total,shown.offset+shown.rows.length)}</p>
    <div className="query-table-wrap"><table className="activity-table"><thead><tr>
      <th>Row</th>{ocdfgMetricColumns(projection).map(column=><th key={column}>{column}</th>)}
    </tr></thead><tbody>{shown.rows.map((row,index)=><tr key={shown.offset+index}>
      <td>{shown.offset+index+1}</td>{row.map((cell,column)=><td key={column}><Literal value={cell}/></td>)}
    </tr>)}</tbody></table></div>
    {!shown.total && <p>No rows in this projection.</p>}
    <Pages {...shown} noun="metric rows" onPage={onPage}/>
  </>;
}

export function OcdfgEdgeMetrics({report}:{report:OcdfgEdgeMetricsReport}) {
  const edges = useMemo(()=>ocdfgMetricEdges(report),[report]);
  const [edgePage,setEdgePage] = useState(0);
  const [selected,setSelected] = useState(0);
  const [projection,setProjection] = useState<OcdfgMetricProjection>("associations");
  const [rowPage,setRowPage] = useState(0);
  useEffect(()=>{setEdgePage(0);setSelected(0);setRowPage(0);},[report]);
  const shown = ocdfgMetricPage(edges,edgePage);
  const inspected = edges[Math.min(selected,edges.length-1)];
  const associations = edges.reduce((count,item)=>count+item.edge.associations.length,0);
  return <section className="panel" aria-label="OCDFG edge association metrics">
    <div className="panel-head"><h2>Edge association metrics</h2>
      <button className="btn-ghost" type="button" onClick={()=>downloadOcdfgEdgeMetrics(report)}>Download complete edge metrics JSON</button>
    </div>
    <p>{report.object_types.length} object types · {edges.length} edges · {associations} raw association rows</p>
    <p>Raw associations retain source order and duplicates. Distinct projections show the returned event couples, event/object triples or objects.</p>
    {!edges.length ? <p>No edge associations were returned.</p> : <>
      <div className="query-table-wrap"><table className="activity-table"><thead><tr>
        <th>Object type</th><th>Source activity</th><th>Target activity</th><th>Raw associations</th><th>Event couples</th><th>Event/object triples</th><th>Objects</th><th>Inspect</th>
      </tr></thead><tbody>{shown.rows.map((item,index)=>{
        const id=shown.offset+index;
        return <tr key={id} aria-selected={id===selected}>
          <td><Literal value={item.objectType}/></td><td><Literal value={item.edge.source_activity}/></td><td><Literal value={item.edge.target_activity}/></td>
          <td>{item.edge.associations.length}</td><td>{item.edge.event_couples.length}</td><td>{item.edge.total_objects.length}</td><td>{item.edge.unique_objects.length}</td>
          <td><button className="btn-ghost" type="button" aria-pressed={id===selected} onClick={()=>{setSelected(id);setRowPage(0);}}>Inspect edge {id+1}</button></td>
        </tr>;
      })}</tbody></table></div>
      <Pages {...shown} noun="edges" onPage={setEdgePage}/>
      {inspected && <>
        <h3>Selected edge</h3><p>Object type <Literal value={inspected.objectType}/> · source <Literal value={inspected.edge.source_activity}/> · target <Literal value={inspected.edge.target_activity}/></p>
        <p className="panel-meta">{inspected.edge.associations.length} raw associations · {inspected.edge.event_couples.length} event couples · {inspected.edge.total_objects.length} event/object triples · {inspected.edge.unique_objects.length} objects</p>
        <label className="filters-field"><span>Edge metric projection</span><select aria-label="Edge metric projection" value={projection} onChange={event=>{setProjection(event.target.value as OcdfgMetricProjection);setRowPage(0);}}>
          {OCDFG_METRIC_PROJECTIONS.map(option=><option key={option.id} value={option.id}>{option.label}</option>)}
        </select></label>
        <OcdfgEdgeMetricRows edge={inspected.edge} projection={projection} page={rowPage} onPage={setRowPage}/>
      </>}
    </>}
  </section>;
}
