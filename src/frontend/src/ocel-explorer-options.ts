import { type OcelDiscoveryOptions } from "./api";
import { type OcelDiscoveryVariant } from "./types";

export const OCEL_MODEL_VARIANTS: readonly { id: OcelDiscoveryVariant; label: string }[] = [
  { id:"ocdfg",label:"Object-centric directly-follows" },
  { id:"etot",label:"Event-to-object-type" },
  { id:"otg",label:"Object-type relations" },
  { id:"ocpn",label:"Object-centric Petri net" },
  { id:"saw",label:"Stochastic arc-weight net" },
];
export interface OcelExplorerSettings {
  activityAttribute: string;
  objectTypeAttribute: string;
  annotated: boolean;
  replayDiagnostics: boolean;
}
export function ocelExplorerOptions(variant: OcelDiscoveryVariant, settings: OcelExplorerSettings): OcelDiscoveryOptions {
  const options: OcelDiscoveryOptions = { variant };
  const activity = settings.activityAttribute.trim(), type = settings.objectTypeAttribute.trim();
  if (variant === "ocpn") {
    options.ocpn_annotation = settings.annotated ? "annotated" : "none";
    options.diagnostics_with_token_based_replay = settings.replayDiagnostics;
    if (activity) options.ocpn_activity_attribute = activity;
    if (type) options.ocpn_object_type_attribute = type;
  } else if (variant === "saw") {
    if (activity) options.saw_activity_attribute = activity;
    if (type) options.saw_object_type_attribute = type;
  } else if (variant === "ocdfg") {
    if (activity) options.ocdfg_activity_attribute = activity;
    if (type) options.ocdfg_object_type_attribute = type;
  }
  return options;
}
