import { type OcpnDiscoveryFlags } from "./ocpn-discovery-options";
import { type OcelDiscoveryOptions } from "./api";
import { type OcelDiscoveryVariant } from "./types";

export const OCEL_MODEL_VARIANTS: readonly { id: OcelDiscoveryVariant; label: string }[] = [
  { id:"ocdfg",label:"Object-centric directly-follows" },
  { id:"etot",label:"Event-to-object-type" },
  { id:"otg",label:"Object-type relations" },
  { id:"ocpn",label:"Object-centric Petri net" },
  { id:"saw",label:"Stochastic arc-weight net" },
];
export interface OcdfgPerformanceSettings {
  enabled: boolean;
  businessHours: boolean;
  slotsMode: "default" | "custom";
  slotsJson: string;
}
export const DEFAULT_OCDFG_PERFORMANCE: OcdfgPerformanceSettings = {
  enabled: false, businessHours: true, slotsMode: "default", slotsJson: "[[25200,61200],[111600,147600],[198000,234000],[284400,320400],[370800,406800]]",
};
export const OCDFG_EDGE_METRIC_ROLES = [
  {id:"eventId",label:"Event ID",key:"ocdfg_edge_metrics_event_id_key",canonical:"ocel:eid"},
  {id:"objectId",label:"Object ID",key:"ocdfg_edge_metrics_object_id_key",canonical:"ocel:oid"},
  {id:"objectType",label:"Object type",key:"ocdfg_edge_metrics_object_type_key",canonical:"ocel:type"},
  {id:"activity",label:"Activity",key:"ocdfg_edge_metrics_activity_key",canonical:"ocel:activity"},
  {id:"timestamp",label:"Timestamp (does not affect associations)",key:"ocdfg_edge_metrics_timestamp_key",canonical:"ocel:timestamp"},
] as const;
export interface OcdfgEdgeMetricsSettings {
  enabled: boolean;
  roles: Record<typeof OCDFG_EDGE_METRIC_ROLES[number]["id"], {custom:boolean; key:string}>;
}
export function defaultOcdfgEdgeMetricsSettings(): OcdfgEdgeMetricsSettings {
  return {enabled:false,roles:{eventId:{custom:false,key:"ocel:eid"},objectId:{custom:false,key:"ocel:oid"},
    objectType:{custom:false,key:"ocel:type"},activity:{custom:false,key:"ocel:activity"},timestamp:{custom:false,key:"ocel:timestamp"}}};
}
export interface OcelExplorerSettings {
  activityAttribute: string;
  objectTypeAttribute: string;
  annotated: boolean;
  replayDiagnostics: boolean;
  ocpnDiscovery?: OcpnDiscoveryFlags;
  /** Omitted keeps the native inductive miner on the flattened log (IM). */
  ocpnVariant?: "log" | "dfg";
  ocdfgPerformance?: OcdfgPerformanceSettings;
  ocdfgEdgeMetrics?: OcdfgEdgeMetricsSettings;
}
export function ocelExplorerOptions(variant: OcelDiscoveryVariant, settings: OcelExplorerSettings): OcelDiscoveryOptions {
  const options: OcelDiscoveryOptions = { variant };
  const activity = settings.activityAttribute.trim(), type = settings.objectTypeAttribute.trim();
  if (variant === "ocpn") {
    Object.assign(options, settings.ocpnDiscovery);
    options.ocpn_annotation = settings.annotated ? "annotated" : "none";
    options.diagnostics_with_token_based_replay = settings.replayDiagnostics;
    if (settings.ocpnVariant) options.ocpn_variant = settings.ocpnVariant;
    if (activity) options.ocpn_activity_attribute = activity;
    if (type) options.ocpn_object_type_attribute = type;
  } else if (variant === "saw") {
    if (activity) options.saw_activity_attribute = activity;
    if (type) options.saw_object_type_attribute = type;
  } else if (variant === "ocdfg") {
    if (settings.ocdfgEdgeMetrics?.enabled) {
      options.include_edge_metrics = true;
      for (const role of OCDFG_EDGE_METRIC_ROLES) {
        const selected = settings.ocdfgEdgeMetrics.roles[role.id];
        if (selected.custom) options[role.key] = selected.key;
      }
    }
    if (settings.ocdfgPerformance?.enabled) {
      options.compute_edges_performance = true;
      options.ocdfg_business_hours = settings.ocdfgPerformance.businessHours;
      if (settings.ocdfgPerformance.businessHours && settings.ocdfgPerformance.slotsMode === "custom")
        options.ocdfg_business_hour_slots_json = settings.ocdfgPerformance.slotsJson;
    }
    if (activity) options.ocdfg_activity_attribute = activity;
    if (type) options.ocdfg_object_type_attribute = type;
  }
  return options;
}
