import {afterEach,describe,expect,it,vi} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {miningSourceHash} from "../src/active-mining-source";
import {nativeCell,nativeJson,parseNativeJson} from "../src/native-json";
import {appendNativeMultipartField,nativeStringBytes} from "../src/native-multipart";
import {originalEmbeddingFilterOriginalBytes} from "../src/original-embedding-filter-workbench";
import {originalEditDistanceAllTablesCsv,originalEditDistanceSelectedRecordsJson,originalEditDistanceWireFilename} from "../src/original-edit-distance-workbench";
import {ORIGINAL_QUALITY_ARTIFACTS,ORIGINAL_QUALITY_CALLBACK_EXAMPLE,ORIGINAL_QUALITY_CALLBACK_SLOTS,ORIGINAL_QUALITY_DISPATCH,ORIGINAL_QUALITY_OPERATIONS,ORIGINAL_QUALITY_OPTION_MEMBERS,ORIGINAL_QUALITY_PUBLIC_OVERLOADS,ORIGINAL_QUALITY_RECORD_SCHEMAS,ORIGINAL_QUALITY_SPECS,OriginalQualityRequestGate,defaultOriginalQualityForm,isOriginalQualityBytes,isOriginalQualityCallbackInvocation,isOriginalQualityCallbackTables,isOriginalQualityEnum,isOriginalQualityModel,isOriginalQualityNativeType,isOriginalQualitySource,isOriginalQualityWorkbench,originalQualityCarrierFile,originalQualityControls,originalQualityDefaultModel,originalQualityDefaultSource,originalQualityEditPath,originalQualityMemberGuides,originalQualityPrimaryCall,originalQualitySelectedTypes,originalQualityReport,originalQualityRequestFile,originalQualityResultFile,originalQualityReusableInputs,originalQualityTables,runOriginalQualityWorkbench,type OriginalQualityBytes,type OriginalQualityDispatch,type OriginalQualityForm,type OriginalQualityRole,type OriginalQualityRun,type OriginalQualityWorkbench} from "../src/original-quality-workbench";
import {OriginalQualityWorkspace} from "../src/components/OriginalQualityWorkspace";
import {Landing} from "../src/components/Landing";
import {Sidebar} from "../src/components/Sidebar";
import {toolById} from "../src/tools";
// Synthetic complete storage fixtures verify transport/ownership/UI contracts;
// they are not captured algorithm outcomes or browser-interaction evidence.
const STORAGE_DEFAULTS={
 "ReplayFitnessOptions": {
  "consider_activities_not_in_model_in_fitness": false,
  "activity_attribute": "concept:name",
  "token_replay_variant": {
   "name": "FORWARD",
   "index": 0
  },
  "alignment_options": {
   "variant": {
    "name": "STATE_EQUATION_A_STAR",
    "index": 0
   },
   "cost_profile": {
    "name": "STANDARD",
    "index": 1
   },
   "max_states": 200000,
   "max_time_seconds": 0.0,
   "max_total_time_seconds": 0.0,
   "max_alignments": 32,
   "exponent": 2.0,
   "synchronous_dijkstra": true,
   "default_trace_cost": null,
   "default_model_cost": null,
   "default_sync_cost": null,
   "default_silent_cost": null,
   "trace_costs": [],
   "model_costs": [],
   "sync_costs": [],
   "trace_costs_by_position": [],
   "model_costs_by_transition": [],
   "sync_costs_by_transition": [],
   "trace_cost_function": null,
   "model_cost_function": null,
   "sync_cost_function": null,
   "activity_attribute": "concept:name",
   "initial_marking": [],
   "final_marking": []
  },
  "decomposed_alignment_options": null,
  "initial_marking": [],
  "final_marking": [],
  "case_id_attribute": "case:concept:name",
  "max_rows_to_materialize": null,
  "cleaning_token_flood": false,
  "exact_initial_marking": null,
  "exact_final_marking": null,
  "initial_marking_keys": [],
  "final_marking_keys": [],
  "place_names": [],
  "transition_names": []
 },
 "PrecisionOptions": {
  "use_task_ids": false,
  "max_states": 100000,
  "activity_attribute": "concept:name",
  "token_replay_variant": {
   "name": "FORWARD",
   "index": 0
  },
  "initial_marking": [],
  "final_marking": [],
  "case_id_attribute": "case:concept:name",
  "max_rows_to_materialize": null,
  "exact_initial_marking": null,
  "exact_final_marking": null,
  "initial_marking_keys": [],
  "final_marking_keys": [],
  "place_names": [],
  "transition_names": []
 },
 "GeneralizationOptions": {
  "activity_attribute": "concept:name",
  "initial_marking": [],
  "final_marking": [],
  "case_id_attribute": "case:concept:name",
  "max_rows_to_materialize": null,
  "exact_initial_marking": null,
  "exact_final_marking": null,
  "initial_marking_keys": [],
  "final_marking_keys": [],
  "place_names": [],
  "transition_names": []
 },
 "SimplicityOptions": {
  "initial_marking": [],
  "final_marking": [],
  "max_states": 100000,
  "arc_degree_k": 2.0,
  "presentation_initial_marking": null
 },
 "CompositeEvaluationOptions": {
  "fitness_weight": 0.25,
  "precision_weight": 0.25,
  "simplicity_weight": 0.25,
  "generalization_weight": 0.25,
  "activity_attribute": "concept:name",
  "initial_marking": [],
  "final_marking": [],
  "case_id_attribute": "case:concept:name",
  "max_rows_to_materialize": null,
  "exact_initial_marking": null,
  "exact_final_marking": null,
  "initial_marking_keys": [],
  "final_marking_keys": [],
  "place_names": [],
  "transition_names": []
 },
 "ReplayFitnessResult": {
  "variant": {
   "name": "AUTO",
   "index": 0
  },
  "fitness": 0.0,
  "log_fitness": 0.0,
  "average_trace_fitness": 0.0,
  "percentage_fit_traces": 0.0,
  "fitting_traces": 0,
  "trace_count": 0,
  "total_cost": 0.0,
  "computed": true,
  "exhausted": false,
  "error": "",
  "token_result": {
   "fitness": 1.0,
   "precision": 1.0,
   "generalization": 1.0,
   "simplicity": 1.0,
   "fitting_traces": 0,
   "trace_count": 0,
   "produced": 0,
   "consumed": 0,
   "missing": 0,
   "remaining": 0,
   "transition_frequencies": [],
   "variants": [],
   "activity_attribute": "concept:name",
   "trace_diagnostics": null,
   "duration_diagnostics": null,
   "root_cause_diagnostics": null
  },
  "alignment_result": {
   "computed": true,
   "exhausted": false,
   "fitness": 1.0,
   "precision": 1.0,
   "fitting_traces": 0,
   "trace_count": 0,
   "total_cost": 0.0,
   "variant": {
    "name": "STATE_EQUATION_A_STAR",
    "index": 0
   },
   "diagnostics": {
    "visited_states": 0,
    "queued_states": 0,
    "traversed_arcs": 0,
    "lp_solved": 0,
    "closed_set_length": 0,
    "num_visited_markings": 0
   },
   "variants": [],
   "activity_attribute": "concept:name"
  },
  "decomposed_alignment_result": null,
  "log_fitness_is_integer_zero": false,
  "token_result_is_fitness_only": false
 },
 "PrecisionResult": {
  "variant": {
   "name": "AUTO",
   "index": 0
  },
  "precision": 1.0,
  "computed": true,
  "exhausted": false,
  "error": "",
  "token_result": {
   "fitness": 1.0,
   "precision": 1.0,
   "generalization": 1.0,
   "simplicity": 1.0,
   "fitting_traces": 0,
   "trace_count": 0,
   "produced": 0,
   "consumed": 0,
   "missing": 0,
   "remaining": 0,
   "transition_frequencies": [],
   "variants": [],
   "activity_attribute": "concept:name",
   "trace_diagnostics": null,
   "duration_diagnostics": null,
   "root_cause_diagnostics": null
  },
  "alignment_result": {
   "computed": true,
   "exhausted": false,
   "fitness": 1.0,
   "precision": 1.0,
   "fitting_traces": 0,
   "trace_count": 0,
   "total_cost": 0.0,
   "variant": {
    "name": "STATE_EQUATION_A_STAR",
    "index": 0
   },
   "diagnostics": {
    "visited_states": 0,
    "queued_states": 0,
    "traversed_arcs": 0,
    "lp_solved": 0,
    "closed_set_length": 0,
    "num_visited_markings": 0
   },
   "variants": [],
   "activity_attribute": "concept:name"
  },
  "alignment_detail_present": false,
  "token_result_is_precision_only": false
 },
 "GeneralizationResult": {
  "variant": {
   "name": "GENERALIZATION_TOKEN",
   "index": 0
  },
  "generalization": 0.0,
  "executed_transitions": 0,
  "transition_count": 0,
  "transition_frequencies": [],
  "computed": true,
  "error": ""
 },
 "SimplicityResult": {
  "variant": {
   "name": "ARC_DEGREE",
   "index": 0
  },
  "simplicity": 0.0,
  "complexity": 0.0,
  "value": 0.0,
  "vertices": 0,
  "edges": 0,
  "strongly_connected_components": 0,
  "computed": true,
  "exhausted": false,
  "error": ""
 },
 "CompositeEvaluationResult": {
  "fitness": 0.0,
  "precision": 0.0,
  "generalization": 0.0,
  "simplicity": 0.0,
  "metrics_average_weight": 0.0,
  "fscore": 0.0,
  "fitness_weight": 0.25,
  "precision_weight": 0.25,
  "simplicity_weight": 0.25,
  "generalization_weight": 0.25,
  "computed": true,
  "error": "",
  "fitness_details": {
   "variant": {
    "name": "AUTO",
    "index": 0
   },
   "fitness": 0.0,
   "log_fitness": 0.0,
   "average_trace_fitness": 0.0,
   "percentage_fit_traces": 0.0,
   "fitting_traces": 0,
   "trace_count": 0,
   "total_cost": 0.0,
   "computed": true,
   "exhausted": false,
   "error": "",
   "token_result": {
    "fitness": 1.0,
    "precision": 1.0,
    "generalization": 1.0,
    "simplicity": 1.0,
    "fitting_traces": 0,
    "trace_count": 0,
    "produced": 0,
    "consumed": 0,
    "missing": 0,
    "remaining": 0,
    "transition_frequencies": [],
    "variants": [],
    "activity_attribute": "concept:name",
    "trace_diagnostics": null,
    "duration_diagnostics": null,
    "root_cause_diagnostics": null
   },
   "alignment_result": {
    "computed": true,
    "exhausted": false,
    "fitness": 1.0,
    "precision": 1.0,
    "fitting_traces": 0,
    "trace_count": 0,
    "total_cost": 0.0,
    "variant": {
     "name": "STATE_EQUATION_A_STAR",
     "index": 0
    },
    "diagnostics": {
     "visited_states": 0,
     "queued_states": 0,
     "traversed_arcs": 0,
     "lp_solved": 0,
     "closed_set_length": 0,
     "num_visited_markings": 0
    },
    "variants": [],
    "activity_attribute": "concept:name"
   },
   "decomposed_alignment_result": null,
   "log_fitness_is_integer_zero": false,
   "token_result_is_fitness_only": false
  },
  "fitness_score": 0.0
 }
} as Record<string,Record<string,unknown>>;

const MAX="18446744073709551615",HUGE="123456789012345678901234567890123456789012345678901234567890",WORD={native_bytes:"hex-v1",data:"ff00eda080edb080"} as const;
const record=(raw:string)=>parseNativeJson(raw) as Record<string,unknown>;
const file=(raw:string,name="input.json")=>new File([nativeStringBytes(raw)],name,{type:"application/json",lastModified:123456789});
const hex=(bytes:Uint8Array)=>Array.from(bytes,byte=>byte.toString(16).padStart(2,"0")).join("");
async function carrier(dataset:string,raw:string|Uint8Array<ArrayBuffer>):Promise<OriginalQualityBytes>{const bytes=typeof raw==="string"?nativeStringBytes(raw):raw;return {dataset,encoding:"hex",data:hex(bytes),bytes:bytes.length,sha256:await miningSourceHash(new File([bytes],"hash"))};}
function uploadsFor(form:OriginalQualityForm):Partial<Record<OriginalQualityRole,File>>{return {model:file(originalQualityDefaultModel(form.model_kind),"model.json"),...(form.source_kind===null?{}:{source:file(originalQualityDefaultSource(form.source_kind),"source.json")})};}
async function artifacts(value:OriginalQualityWorkbench){const snapshot=value.input;for(const [key,item] of Object.entries({source_input_json:snapshot.before.source,model_input_json:snapshot.before.model,source_after_json:snapshot.after.source,model_after_json:snapshot.after.model,configured_options_json:value.configured_options,configured_callback_tables_json:value.configured_callback_tables,callback_invocations_json:value.configured_callback_tables===null?null:value.callback_invocations,full_result_json:value.result}))value.artifact_bytes[key as typeof ORIGINAL_QUALITY_ARTIFACTS[number]]=item===null?null:await carrier(key+".json",nativeJson(item));return value;}
async function envelope(form=defaultOriginalQualityForm(),uploads=uploadsFor(form)):Promise<OriginalQualityWorkbench>{
 const controls=originalQualityControls(form),originals:OriginalQualityWorkbench["originals"]=[],body=new FormData();
 for(const item of controls){const bytes=item.file?new Uint8Array(await item.file.arrayBuffer()):nativeStringBytes(item.text);if(item.file)body.append(item.name,item.file,item.file.name);else appendNativeMultipartField(body,item.name,item.text);const part=body.get(item.name),filename=part!==null&&typeof part!=="string"?await originalEditDistanceWireFilename(part.name):null;originals.push({field:item.name,filename,file:filename!==null,original:await carrier(filename??item.name,bytes)});}
 for(const role of (form.source_kind===null?["model"]:["source","model"]) as OriginalQualityRole[]){const input=uploads[role]!,name=await originalEditDistanceWireFilename(input.name);originals.push({field:role+"_file",filename:name,file:name!==null,original:await carrier(name??role+"_file",new Uint8Array(await input.arrayBuffer()))});}
 const spec=ORIGINAL_QUALITY_SPECS[form.operation],source=form.source_kind===null?null:parseNativeJson(await uploads.source!.text()),model=parseNativeJson(await uploads.model!.text()),before={source,model},configured=form.call_form==="default_options"?null:record(nativeJson(STORAGE_DEFAULTS[spec.options]));
 const callbacks=form.operation==="evaluate_replay_fitness"&&form.call_form==="with_options"?{trace_cost_function:null,model_cost_function:null,sync_cost_function:null} as Record<typeof ORIGINAL_QUALITY_CALLBACK_SLOTS[number],unknown>:null;
 const value:OriginalQualityWorkbench={family:"ORIGINAL_QUALITY_WORKBENCH",version:"original-quality-workbench-v1",operation:form.operation,source_kind:form.source_kind,model_kind:form.model_kind,call_form:form.call_form,executed:true,native_entered:true,native_returned:true,native_calls:[{name:originalQualityPrimaryCall(form.operation,form),stage:"primary",input_kind:originalQualitySelectedTypes(form).source===null?originalQualitySelectedTypes(form).model:originalQualitySelectedTypes(form).source+"+"+originalQualitySelectedTypes(form).model,returned:true,error:null}],error:null,originals,parsed_controls:controls.map(item=>item.name),supplied_controls:controls.map(item=>({name:item.name,origin:"multipart"})),input_context:{size_t_bytes:8,int_bytes:4,input_type:form.source_kind===null?null:form.source_kind.startsWith("view_")?"EventDataView":({log:"EventLog",stream:"EventStream",table:"EventTable",lazy:"LazyEventTable"})[form.source_kind as "log"|"stream"|"table"|"lazy"],model_type:({petri_net:"PetriNet",process_tree:"ProcessTree",pnml_document:"PnmlDocument",presentation:"PetriNetPresentation"})[form.model_kind],source_prepared:source!==null,model_prepared:true,source_unchanged:source===null?null:true,model_unchanged:true,options_unchanged:configured===null?null:true,callback_binding_presence:{trace_cost_function:false,model_cost_function:false,sync_cost_function:false},lazy_scans:[]},input:{before,after:before},submitted_options:form.options.supplied?await carrier(form.options.file?(await originalEditDistanceWireFilename(form.options.file.name)??"options_json"):"options_json",form.options.file?new Uint8Array(await form.options.file.arrayBuffer()):form.options.text):null,configured_options:configured,supplied_options:form.options.supplied?Object.keys(parseNativeJson(form.options.file?await form.options.file.text():form.options.text) as object).sort():[],configured_callback_tables:callbacks,callback_invocations:[],result:{type:spec.result,value:record(nativeJson(STORAGE_DEFAULTS[spec.result]))},artifact_bytes:Object.fromEntries(ORIGINAL_QUALITY_ARTIFACTS.map(name=>[name,null])) as OriginalQualityWorkbench["artifact_bytes"],artifact_errors:[]};
 return artifacts(value);
}
const saved=(value:OriginalQualityWorkbench,form=defaultOriginalQualityForm()):OriginalQualityRun=>({value,raw:nativeJson(value),http_status:200,tables:originalQualityTables(value),request:{path:"/api/quality-workbench",operation:form.operation,source_kind:form.source_kind,model_kind:form.model_kind,call_form:form.call_form,originals:[]},uploads:{},controls:[]});
afterEach(()=>{vi.unstubAllGlobals();vi.restoreAllMocks();});
describe("original quality independent complete constructors and byte ownership",()=>{
 it("retains five evaluators,61 public declarations,97 dispatch choices,61 top options and56 top result fields",()=>{expect(ORIGINAL_QUALITY_OPERATIONS).toHaveLength(5);expect(ORIGINAL_QUALITY_PUBLIC_OVERLOADS).toHaveLength(61);expect(Object.values(ORIGINAL_QUALITY_DISPATCH).reduce((n,items)=>n+items.length,0)).toBe(97);expect(Object.values(ORIGINAL_QUALITY_OPTION_MEMBERS).reduce((n,items)=>n+items.length,0)).toBe(61);expect(Object.values(ORIGINAL_QUALITY_SPECS).reduce((n,spec)=>n+Object.keys(ORIGINAL_QUALITY_RECORD_SCHEMAS[spec.result]).length,0)).toBe(56);expect(ORIGINAL_QUALITY_ARTIFACTS).toHaveLength(8);});
 for(const operation of ORIGINAL_QUALITY_OPERATIONS)for(const dispatch of ORIGINAL_QUALITY_DISPATCH[operation] as readonly OriginalQualityDispatch[]){
  it("owns exact Files for "+operation+" "+nativeJson(dispatch),async()=>{const form={...defaultOriginalQualityForm(operation),...dispatch},uploads=uploadsFor(form),value=await envelope(form,uploads),raw="\n"+nativeJson(value)+"\n";expect(isOriginalQualityWorkbench(value)).toBe(true);const fetcher=vi.fn(async(path:string,init:RequestInit)=>{expect(path).toBe("/api/quality-workbench");expect(init.method).toBe("POST");const body=init.body as FormData;expect(Array.from(body.keys())).toEqual([...originalQualityControls(form).map(item=>item.name),...(form.source_kind===null?["model_file"]:["source_file","model_file"])]);for(const role of form.source_kind===null?["model"]:["source","model"])expect(await (body.get(role+"_file") as File).text()).toBe(await uploads[role as OriginalQualityRole]!.text());return new Response(raw);});vi.stubGlobal("fetch",fetcher);const run=await runOriginalQualityWorkbench(uploads,form);expect(run.raw).toBe(raw);expect(fetcher).toHaveBeenCalledTimes(1);expect(run.request.source_kind).toBe(form.source_kind);expect(await originalQualityResultFile(run).text()).toBe(nativeJson(value.result));expect(originalQualityReport(run)).toContain("raw_native_response");const handoffs=originalQualityReusableInputs(run);expect(handoffs.some(item=>item.role==="model"&&item.dispatch.model_kind===form.model_kind)).toBe(true);for(const handoff of handoffs.filter(item=>item.role==="source"))expect(handoff.dispatch.source_kind?.replace(/^view_/,"")).toBe(form.source_kind?.replace(/^view_/,""));});
 }
 for(const operation of ORIGINAL_QUALITY_OPERATIONS)for(const member of ORIGINAL_QUALITY_OPTION_MEMBERS[ORIGINAL_QUALITY_SPECS[operation].options])it("edits supplied/omitted native option "+operation+"."+member,()=>{const raw='{"keep":'+MAX+'}',changed=originalQualityEditPath(raw,[member],"null");expect(changed).toContain(MAX);expect(Object.hasOwn(record(changed),member)).toBe(true);expect(Object.hasOwn(record(originalQualityEditPath(changed,[member])),member)).toBe(false);});
 it("edits nested alignment/decomposed members without rounding siblings or injecting standalone defaults",()=>{const raw='{"initial_marking":['+HUGE+'],"alignment_options":{"max_states":'+MAX+',"default_sync_cost":-0.0}}';const changed=originalQualityEditPath(raw,["alignment_options","trace_costs"],'[{"key":'+nativeJson(WORD)+',"value":"NaN"}]');expect(changed).toContain(HUGE);expect(changed).toContain(MAX);expect(changed).toContain("-0.0");expect(changed).not.toContain("cost_profile");const paths=originalQualityMemberGuides("evaluate_replay_fitness").map(item=>item.path.join("."));expect(paths.filter(path=>path.startsWith("alignment_options."))).toHaveLength(24);expect(paths.filter(path=>path.startsWith("decomposed_alignment_options.")).length).toBeGreaterThanOrEqual(18);});
 it("keeps complete raw storage for all source kinds and all four model constructors",()=>{for(const kind of ["log","stream","table","lazy","view_log","view_stream","view_table","view_lazy"] as const)expect(isOriginalQualitySource(kind,parseNativeJson(originalQualityDefaultSource(kind)))).toBe(true);for(const kind of ["petri_net","process_tree","pnml_document","presentation"] as const)expect(isOriginalQualityModel(kind,parseNativeJson(originalQualityDefaultModel(kind)))).toBe(true);});
 it("retains full integers, arbitrary TokenInteger signs, enum storage and semantic nonfinite values",()=>{for(const [type,raw] of [["std::uint64_t",MAX],["TokenInteger",HUGE],["TokenInteger","-"+HUGE],["std::int64_t","-9223372036854775808"]]){const holder=record('{"value":'+raw+'}');expect(isOriginalQualityNativeType(type,holder.value,holder,"value")).toBe(true);expect(String(nativeCell(holder,"value"))).toBe(raw);}for(const raw of ['"NaN"','"INF"','"-INF"','-0.0'])expect(isOriginalQualityNativeType("double",parseNativeJson(raw))).toBe(true);expect(isOriginalQualityEnum("PrecisionVariant",record('{"name":null,"index":-2147483648}'))).toBe(true);expect(isOriginalQualityEnum("PrecisionVariant",{name:"AUTO",index:1})).toBe(false);const over=record('{"value":18446744073709551616}');expect(isOriginalQualityNativeType("std::uint64_t",over.value,over,"value")).toBe(false);expect(isOriginalQualityNativeType("std::size_t",4294967296,undefined,undefined,32)).toBe(false);});
 it("validates every nested result field while retaining inactive details, computedfalse and errors",async()=>{for(const operation of ORIGINAL_QUALITY_OPERATIONS){const form=defaultOriginalQualityForm(operation),value=await envelope(form);const result=value.result!.value as Record<string,unknown>;result.computed=false;result.error=WORD;if(Object.hasOwn(result,"exhausted"))result.exhausted=true;await artifacts(value);expect(isOriginalQualityWorkbench(value)).toBe(true);const missing=record(nativeJson(result));delete missing[Object.keys(missing)[0]];expect(isOriginalQualityNativeType(value.result!.type,missing)).toBe(false);}const value=await envelope(),result=value.result!.value as Record<string,unknown>,token=result.token_result as Record<string,unknown>;const changed=record(nativeJson(token).replace('"produced":0','"produced":'+HUGE));result.token_result=changed;await artifacts(value);expect(isOriginalQualityWorkbench(value)).toBe(true);const run=saved(value);expect(originalEditDistanceAllTablesCsv(run.tables)).toContain(HUGE);expect(originalEditDistanceSelectedRecordsJson(run.tables)).toContain(HUGE);});
 it("retains all seven nested EventAttribute scalar arms with independent byte strings and dates",()=>{const raw=record('{"key":'+nativeJson(WORD)+',"value":'+nativeJson(WORD)+',"kind_index":-2147483648,"container_index":0,"null":false,"scalar_wrapper":true,"scalar_children_list":false,"first_class_role_index":0,"first_class_state_index":0,"first_class_source_key":'+nativeJson(WORD)+',"typed_value":{"index":0,"value":null},"children":[]}');for(const [index,payload] of [[0,"null"],[1,"-9223372036854775808"],[2,'"NaN"'],[3,"true"],[4,'{"value":'+nativeJson(WORD)+',"resolution_index":-2147483648,"timezone":'+nativeJson(WORD)+'}'],[5,MAX],[6,'"NA"']] as const){const value=record(nativeJson(raw).replace('"typed_value":{"index":0,"value":null}','"typed_value":{"index":'+index+',"value":'+payload+'}'));expect(isOriginalQualityNativeType("EventAttribute",value)).toBe(true);expect(nativeJson(value)).toContain("ff00eda080edb080");}expect(isOriginalQualityNativeType("PetriTransitionNameList",[null,"",WORD])).toBe(true);expect(isOriginalQualityNativeType("PetriTransitionNameList",[{index:5,value:"name"}])).toBe(false);});
 it("keeps all callback slots, duplicate lookup entries, fallback, throws and exact invocation indices",()=>{const tables=record(ORIGINAL_QUALITY_CALLBACK_EXAMPLE);const trace=tables.trace_cost_function as Record<string,unknown>;trace.entries=[{label:WORD,index:0,outcome:{throw:WORD}},{label:WORD,index:0,outcome:{value:"NaN"}}];trace.fallback={value:"-INF"};expect(isOriginalQualityCallbackTables(tables)).toBe(true);const invocation=record('{"slot":"trace_cost_function","label":'+nativeJson(WORD)+',"index":'+MAX+',"entry_index":null,"used_fallback":true,"outcome":{"value":-0.0}}');expect(isOriginalQualityCallbackInvocation(invocation)).toBe(true);expect(nativeJson(invocation)).toContain(MAX);expect(nativeJson(invocation)).toContain("-0.0");});
 it("retains original options and callback tables as actual physical Files",async()=>{const form=defaultOriginalQualityForm();form.options={supplied:true,text:"ignored",file:file('{"alignment_options":{}}',"options.json")};form.callbacks={supplied:true,text:"ignored",file:file("{}","callbacks.json")};const uploads=uploadsFor(form),value=await envelope(form,uploads);vi.stubGlobal("fetch",vi.fn(async(_path:string,init:RequestInit)=>{const body=init.body as FormData;expect(body.get("options_json")).toBeInstanceOf(File);expect(body.get("callback_tables_json")).toBeInstanceOf(File);return new Response(nativeJson(value));}));const run=await runOriginalQualityWorkbench(uploads,form),reuse=originalQualityReusableInputs(run);expect(reuse.some(item=>item.role==="options"&&item.file.type==="application/json")).toBe(true);expect(reuse.some(item=>item.role==="callbacks"&&item.file.type==="application/json")).toBe(true);for(const item of reuse.filter(item=>item.role==="options"||item.role==="callbacks"))expect(hex(new Uint8Array(await item.file.arrayBuffer()))).toBe(run.value.artifact_bytes[item.role==="options"?"configured_options_json":"configured_callback_tables_json"]!.data);});
 for(const changed of ["hash","filename","order","control","artifact","fragment"])it("rejects changed "+changed+" ownership",async()=>{const form=defaultOriginalQualityForm(),uploads=uploadsFor(form),value=await envelope(form,uploads);if(changed==="hash")value.originals.at(-1)!.original.sha256="0".repeat(64);if(changed==="filename"){value.originals.at(-1)!.filename=null;value.originals.at(-1)!.file=false;}if(changed==="order")value.originals.reverse();if(changed==="control")value.supplied_controls[0].name="options_json";if(changed==="artifact")value.artifact_bytes.full_result_json!.sha256="0".repeat(64);if(changed==="fragment")value.artifact_bytes.model_input_json=await carrier("model_input_json.json","{}");vi.stubGlobal("fetch",vi.fn(async()=>new Response(nativeJson(value))));await expect(runOriginalQualityWorkbench(uploads,form)).rejects.toThrow(/ownership|original|hash|filename|artifact|fragment|carrier/);});
 it("preserves escaped wire filenames, empty filename presence and every original byte on download",async()=>{for(const name of ['line\n"name.json',""]){const form=defaultOriginalQualityForm(),uploads=uploadsFor(form);uploads.model=file(originalQualityDefaultModel(),name);const value=await envelope(form,uploads);vi.stubGlobal("fetch",vi.fn(async()=>new Response(nativeJson(value))));const run=await runOriginalQualityWorkbench(uploads,form),index=run.request.originals.length-1,item=run.request.originals[index];expect(item.filename).toBe(await originalEditDistanceWireFilename(name));expect(item.local_filename).toBe(name);const original=originalQualityRequestFile(run,index);expect(original.name).toBe(name);expect(original.lastModified).toBe(123456789);expect(await original.text()).toBe(await uploads.model!.text());}});
 it("preserves native errors and after-return failures without inventing a result",async()=>{for(const returned of [false,true]){const value=await envelope();value.error={phase:returned?"result_serialization_error":"native_error",exception_type:"runtime_error",message:WORD};value.native_returned=value.executed=returned;value.native_calls[0].returned=returned;value.native_calls[0].error=returned?null:value.error;value.result=null;await artifacts(value);expect(isOriginalQualityWorkbench(value)).toBe(true);expect(originalQualityReport(saved(value))).toContain("ff00eda080edb080");}});
 it("forbids inapplicable controls by presence while preserving omission and explicit empty options",()=>{const defaultForm={...defaultOriginalQualityForm(),call_form:"default_options" as const};defaultForm.options.supplied=true;expect(()=>originalQualityControls(defaultForm)).toThrow(/forbids/);const composite=defaultOriginalQualityForm("evaluate_composite");composite.variant.supplied=true;expect(()=>originalQualityControls(composite)).toThrow(/inapplicable/);const precision=defaultOriginalQualityForm("evaluate_precision");precision.callbacks.supplied=true;expect(()=>originalQualityControls(precision)).toThrow(/Callback/);const form=defaultOriginalQualityForm();expect(originalQualityControls(form).some(item=>item.name==="options_json")).toBe(false);form.options.supplied=true;expect(originalQualityControls(form).find(item=>item.name==="options_json")?.text).toBe("{}");});
 it("invalidates stale completion and aborts before sending a request",async()=>{const gate=new OriginalQualityRequestGate(),ticket=gate.issue();gate.invalidate();expect(gate.accepts(ticket)).toBe(false);const abort=new AbortController();abort.abort();const fetcher=vi.fn();vi.stubGlobal("fetch",fetcher);await expect(runOriginalQualityWorkbench(uploadsFor(defaultOriginalQualityForm()),defaultOriginalQualityForm(),abort.signal)).rejects.toThrow();expect(fetcher).not.toHaveBeenCalled();});
 it("downloads exact carrier bytes and rejects malformed carrier domains",async()=>{const bytes=Uint8Array.from([0,255,237,160,128]),item=await carrier("full_result_json.json",bytes);expect(hex(new Uint8Array(await originalQualityCarrierFile(item).arrayBuffer()))).toBe(hex(bytes));expect(hex(originalEmbeddingFilterOriginalBytes(item))).toBe(hex(bytes));expect(isOriginalQualityBytes({...item,bytes:true})).toBe(false);});
 it("renders all five evaluator entry points and model-only simplicity before mining",()=>{for(const operation of ORIGINAL_QUALITY_OPERATIONS){const html=renderToStaticMarkup(createElement(OriginalQualityWorkspace,{standalone:true,initialOperation:operation}));expect(html).toContain("Original quality evaluator");expect(html).toContain("Original quality overload");expect(html).toContain("Independent model File");expect(html.includes("Independent source File")).toBe(operation!=="evaluate_simplicity");}const callbacks={onOpenFile:()=>{},onHome:()=>{},onOriginalQuality:()=>{}};expect(renderToStaticMarkup(createElement(Sidebar,{dataset:null,activeTool:null,onOpenTool:()=>{},onHome:()=>{},onOpenFile:()=>{},onOpenPalette:()=>{}}))).toContain(toolById("quality").title);expect(renderToStaticMarkup(createElement(Landing,{...callbacks,onLoadSample:()=>{},onLoadOcelSample:()=>{},onLoadResourceSample:()=>{},error:null,mining:false,onOpenTool:()=>{}}))).toContain(toolById("quality").title);});
});
