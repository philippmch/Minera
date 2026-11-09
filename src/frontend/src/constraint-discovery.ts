import {parseDiscoveryStringVector, discoveryStringVectorJson, type DiscoveryVectorMode} from "./discovery-string-vectors";
import { parsePythonFloat } from "./app-logic";
import { backendFetch } from "./backend-fetch";
import { DECLARE_TEMPLATES, isMineraExport, type MineraExport } from "./types";
import type { LogQuerySchema } from "./log-query";
import { nativeJson } from "./native-json";
import { nativeResponseJson, readNativeResponse } from "./native-response";
import { nativeObjectTables, type DiagnosticTable } from "./model-diagnostics";

export type ConstraintFamily = "declare" | "log_skeleton";
export const SKELETON_FAMILIES = ["equivalence", "always_after", "always_before", "never_together", "directly_follows", "activ_freq"] as const;
export interface ConstraintForm {
  activity: string; conformanceActivity: string; diagnosticsCase: string;
  selection: "native" | "selected"; templates: string[];
  activitySelection: "native" | "selected"; activities: string; activitiesMode: DiscoveryVectorMode; activitiesJson: string;
  support: string; confidence: string; multiplier: string; noise: string;
  generateLog: boolean; traces: string; minimum: string; maximum: string; seed: string;
}
export function defaultConstraintForm(family: ConstraintFamily): ConstraintForm {
  return {activity:"", conformanceActivity:"", diagnosticsCase:"", selection:"native", templates:[...(family === "declare" ? DECLARE_TEMPLATES : SKELETON_FAMILIES)],
    activitySelection:"native", activities:"", activitiesMode:"legacy", activitiesJson:"[]", support:"", confidence:"", multiplier:"", noise:"", generateLog:false, traces:"", minimum:"", maximum:"", seed:""};
}
/** Whether Declare discovery picks its thresholds automatically (both explicit ratios blank), so the multiplier applies. */
export const declareAutoSelectionActive = (form: Pick<ConstraintForm, "support" | "confidence">): boolean => !form.support.trim() && !form.confidence.trim();
export function constraintQuery(family: ConstraintFamily, form: ConstraintForm, sourceId: string): URLSearchParams {
  if (!sourceId) throw new Error("Refresh the constraint source first.");
  const q = new URLSearchParams({family, source_id:sourceId});
  const field = (key: string, text: string) => {if (text.trim()) q.set(key,text.trim());};
  field(family === "declare" ? "declare_activity_attribute" : "log_skeleton_activity_attribute", form.activity);
  field("conformance_activity_key", form.conformanceActivity); field("diagnostics_case_id_key", form.diagnosticsCase);
  const allowed: readonly string[] = family === "declare" ? DECLARE_TEMPLATES : SKELETON_FAMILIES;
  if (form.selection === "selected") {
    if (form.templates.some(name => !allowed.includes(name))) throw new Error("Select a supported constraint family.");
    q.set(family === "declare" ? "declare_allowed_templates" : "log_skeleton_constraints", form.templates.join(","));
  }
  // The noise threshold stays in [0,1] (declared); Declare ratios take any
  // finite value, as the reference does. The text follows Python's float()
  // syntax, as the backend reads it (so no hexadecimal, unlike Number()).
  const ratio = (key: string, text: string, bounded = true) => {
    if (!text.trim()) return;
    const number = parsePythonFloat(text);
    if (number === null || !Number.isFinite(number)) throw new Error("Thresholds must be finite decimal numbers (for example 0.8 or 1e-3).");
    if (bounded && (number < 0 || number > 1)) throw new Error("Thresholds must be finite numbers in [0,1].");
    q.set(key,text.trim());
  };
  if (family === "declare") {
    if (form.activitySelection === "selected") {
      if (form.activitiesMode === "literal_json") q.set("declare_considered_activities_json", discoveryStringVectorJson(parseDiscoveryStringVector(form.activitiesJson, "Declare considered activities"), "Declare considered activities"));
      else q.set("declare_considered_activities", form.activities.split(",").map(x=>x.trim()).filter(Boolean).join(","));
    }
    ratio("declare_min_support_ratio",form.support,false); ratio("declare_min_confidence_ratio",form.confidence,false);
    // The multiplier scales the automatic thresholds, which apply only when
    // both explicit ratios are blank (an explicit one leaves the other at 0).
    if (declareAutoSelectionActive(form)) ratio("declare_auto_selection_multiplier",form.multiplier,false);
    if (form.generateLog) {
      q.set("generate_log","true");
      for (const [key,text] of [["traces",form.traces],["min_length",form.minimum],["max_length",form.maximum],["seed",form.seed]] as const) {
        if (!text.trim()) continue;
        if (!/^\d+$/.test(text.trim()) || BigInt(text.trim()) > 18446744073709551615n) throw new Error("Playout values must be unsigned 64-bit integers.");
        q.set("declare_playout_"+key,text.trim());
      }
    }
  } else ratio("log_skeleton_noise_threshold",form.noise);
  return q;
}
export interface ConstraintDiscovery {
  family:"CONSTRAINT_DISCOVERY"; method:ConstraintFamily; variant:"classic";
  source_id:string; dataset:string; activity_key:string; conformance_activity_key:string; diagnostics_case_id_key:string;
  source:{case_count:number;event_count:number}; result:MineraExport;
  diagnostics:Record<string,unknown>|null; diagnostics_error:string|null;
  playout_result:MineraExport|null; playout_xes:string|null; playout_error:string|null;
}
const record = (value:unknown):value is Record<string,unknown> => !!value && typeof value === "object" && !Array.isArray(value);
export function validConstraintDiscovery(value:unknown):value is ConstraintDiscovery {
  if (!record(value) || value.family !== "CONSTRAINT_DISCOVERY" || !["declare","log_skeleton"].includes(String(value.method)) || value.variant !== "classic" || !isMineraExport(value.result) || !record(value.source)) return false;
  if (!["source_id","dataset","activity_key","conformance_activity_key","diagnostics_case_id_key"].every(key=>typeof value[key] === "string" && value[key])) return false;
  if (value.source.case_count !== value.result.trace_count || value.source.event_count !== value.result.event_count) return false;
  if (value.method === "declare" ? !value.result.declare || !Array.isArray(value.result.declare_conformance) : !value.result.log_skeleton || !Array.isArray(value.result.log_skeleton_conformance)) return false;
  if (value.diagnostics !== null && !record(value.diagnostics)) return false;
  if (value.playout_result !== null && !isMineraExport(value.playout_result)) return false;
  if (value.playout_result !== null && (value.method !== "declare" || !value.playout_result.declare || !Array.isArray(value.playout_result.declare_conformance) || value.playout_result.declare_conformance.length !== value.playout_result.trace_count)) return false;
  if (value.playout_xes !== null && (typeof value.playout_xes !== "string" || !value.playout_xes.includes("<log"))) return false;
  if ((value.playout_xes === null) !== (value.playout_result === null)) return false;
  return ["diagnostics_error","playout_error"].every(key=>value[key] === null || typeof value[key] === "string");
}
export interface ConstraintRun {value:ConstraintDiscovery;raw:string;report:string;tables:DiagnosticTable[];}
export function constraintTables(value:ConstraintDiscovery, parameters:URLSearchParams):DiagnosticTable[] {
  const model = value.method === "declare" ? value.result.declare! : value.result.log_skeleton!;
  const conformance = value.method === "declare" ? value.result.declare_conformance! : value.result.log_skeleton_conformance!;
  const tables = nativeObjectTables({source:{method:value.method,variant:value.variant,source_id:value.source_id,dataset:value.dataset,activity_key:value.activity_key,conformance_activity_key:value.conformance_activity_key,diagnostics_case_id_key:value.diagnostics_case_id_key,...value.source},activities:value.result.activities,model,conformance,diagnostics:value.diagnostics,errors:{diagnostics_error:value.diagnostics_error,playout_error:value.playout_error}});
  if (value.playout_result) tables.push(...nativeObjectTables({playout:value.playout_result}).map(table=>table.id === "summary" ? {...table,id:"playout_summary",title:"Generated log summary"} : table));
  tables.push({id:"configuration",title:"Submitted configuration",columns:["parameter","value"],rows:[...parameters.entries()]});
  return tables;
}
export async function runConstraintDiscovery(family:ConstraintFamily, form:ConstraintForm, source:LogQuerySchema, signal?:AbortSignal):Promise<ConstraintRun> {
  const parameters=constraintQuery(family,form,source.source_id), path="/api/discover-constraints?"+parameters;
  const response=await backendFetch(path,{method:"POST",signal});
  const value=await readNativeResponse(response,{path});
  if (!response.ok || record(value)&&"error" in value) throw new Error(record(value)&&typeof value.error === "string" ? value.error : "Native constraint discovery failed.");
  if (!validConstraintDiscovery(value)) throw new Error("Native constraint discovery returned an invalid result.");
  if (value.method !== family || value.source_id !== source.source_id || value.source.case_count !== source.case_count || value.source.event_count !== source.event_count) throw new Error("Discovery result does not match the active source.");
  return {value,raw:nativeResponseJson(value),report:nativeJson({family:"CONSTRAINT_DISCOVERY_REPORT",request:{method:"POST",path:"/api/discover-constraints",parameters:Object.fromEntries(parameters)},response:value},2),tables:constraintTables(value,parameters)};
}
