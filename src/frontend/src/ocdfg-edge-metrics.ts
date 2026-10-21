import { type OcdfgEdgeMetricsEdge, type OcdfgEdgeMetricsReport } from "./types";
import { downloadModelJson } from "./model-view-data";

export const OCDFG_METRICS_PAGE_SIZE = 25;
export const OCDFG_METRIC_PROJECTIONS = [
  {id:"associations",label:"Raw associations (ordered, duplicates retained)"},
  {id:"event_couples",label:"Distinct event couples"},
  {id:"total_objects",label:"Distinct event/object triples"},
  {id:"unique_objects",label:"Distinct objects"},
] as const;
export type OcdfgMetricProjection = typeof OCDFG_METRIC_PROJECTIONS[number]["id"];

export function ocdfgMetricPage<T>(values:readonly T[], requestedPage:number) {
  const pageCount = Math.max(1, Math.ceil(values.length / OCDFG_METRICS_PAGE_SIZE));
  const page = Math.min(pageCount-1,Math.max(0,Number.isFinite(requestedPage) ? Math.floor(requestedPage) : 0));
  const offset = page * OCDFG_METRICS_PAGE_SIZE;
  return {page,pageCount,offset,total:values.length,rows:values.slice(offset,offset+OCDFG_METRICS_PAGE_SIZE)};
}

export function ocdfgMetricEdges(report:OcdfgEdgeMetricsReport) {
  return report.object_types.flatMap(type => type.edges.map(edge => ({objectType:type.object_type,edge})));
}

export function ocdfgMetricRows(edge:OcdfgEdgeMetricsEdge,projection:OcdfgMetricProjection):readonly (readonly string[])[] {
  return projection === "unique_objects" ? edge.unique_objects.map(id=>[id]) : edge[projection];
}

export function ocdfgMetricColumns(projection:OcdfgMetricProjection):readonly string[] {
  if (projection === "unique_objects") return ["Object ID"];
  return projection === "event_couples" ? ["Source event ID","Target event ID"]
    : ["Source event ID","Target event ID","Object ID"];
}

export function downloadOcdfgEdgeMetrics(report:OcdfgEdgeMetricsReport) {
  downloadModelJson(report,"minera-ocdfg-edge-metrics.json");
}
