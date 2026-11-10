import { type OrganizationalSnaOptions, type OrganizationalSnaVariant } from "./types";

export const SOCIAL_NETWORK_VARIANTS: {id:OrganizationalSnaVariant;label:string}[] = [
  {id:"handover",label:"Handover of work"},{id:"working_together",label:"Working together"},
  {id:"subcontracting",label:"Subcontracting"},{id:"jointactivities",label:"Joint activities"},
  {id:"handover_pandas",label:"Handover of work · table"},{id:"working_together_pandas",label:"Working together · table"},
  {id:"subcontracting_pandas",label:"Subcontracting · table"},{id:"jointactivities_pandas",label:"Joint activities · table"},
];
export interface SocialNetworkSettings {variant:OrganizationalSnaVariant;activity:string;resource:string;caseId:string;beta:string;gap:string;normalize:boolean;}
export const DEFAULT_SOCIAL_NETWORK:SocialNetworkSettings = {variant:"handover",activity:"concept:name",resource:"org:resource",caseId:"case:concept:name",beta:"0",gap:"2",normalize:false};
/** The resource key of the mined session: a CSV upload's resource column under its own header (pm4py's frame has no
 * org:resource then, so the organizational routes raise KeyError for that key), an XES upload's resource attribute. */
export function sessionResourceKey(data:{active_source?:{source_selectors?:Partial<Record<string,unknown>>}}|null|undefined):string|undefined {
  const key=data?.active_source?.source_selectors?.resource_key;
  return typeof key==="string" && key.trim()!=="" ? key : undefined;
}
/** Social network settings whose resource field starts at the session's resource key. */
export function defaultSocialNetwork(resourceKey?:string):SocialNetworkSettings {return {...DEFAULT_SOCIAL_NETWORK,resource:resourceKey ?? DEFAULT_SOCIAL_NETWORK.resource};}
export function socialNetworkOptions(settings:SocialNetworkSettings):OrganizationalSnaOptions {
  if (!settings.activity.trim() || !settings.resource.trim()) throw new Error("Enter activity and resource field names.");
  const options:OrganizationalSnaOptions = {sna_variant:settings.variant,sna_activity_attribute:settings.activity.trim(),sna_resource_attribute:settings.resource.trim(),sna_metric_normalization:settings.normalize};
  if (["handover_pandas","working_together_pandas","subcontracting_pandas"].includes(settings.variant)) {
    if (!settings.caseId.trim()) throw new Error("Enter a case field for this table variant.");
    options.sna_case_id_attribute=settings.caseId.trim();
  }
  if (settings.variant.startsWith("handover") || settings.variant.startsWith("subcontracting")) {
    const beta=settings.beta.trim()==="" ? NaN : Number(settings.beta);
    if (!Number.isFinite(beta) || beta<0 || beta>1) throw new Error("Distance decay must be from 0 to 1.");
    options.sna_beta=beta;
  }
  if (settings.variant.startsWith("subcontracting")) {
    const gap=settings.gap.trim()==="" ? NaN : Number(settings.gap);
    if (!Number.isInteger(gap) || gap<1 || gap>10000) throw new Error("Maximum gap must be a whole number from 1 to 10000.");
    options.sna_n=gap;
  }
  return options;
}
