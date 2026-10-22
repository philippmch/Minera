import {afterEach,describe,expect,it,vi} from "vitest";
import {nativeCell,parseNativeJson} from "../src/native-json";
import {POLARS_VARIANTS_ORIGINAL_OPERATIONS,POLARS_VARIANTS_ORIGINAL_KEYS,defaultPolarsVariantsOriginalForm,polarsVariantsOriginalExample,polarsVariantsOriginalFields,polarsVariantsOriginalCallText,isPolarsVariantsOriginalEnvelope,isPolarsVariantsOriginalControls,polarsVariantsReturnedMetadata,requestPolarsVariantsOriginal,polarsVariantsOriginalArtifactFile,polarsVariantsOriginalHeaderContextFile,polarsVariantsOriginalResponseFile,PolarsVariantsOriginalRequestGate,type PolarsVariantsOriginalEnvelope,type PolarsVariantsOriginalRun} from "../src/polars-variants-original-call";
const source=()=>new File(['{"columns":[],"rows":[]}'],"source.json",{type:"application/json"});
const controls=()=>({input_kind:"LazyFrame",keyword_order:["admitted_variants"],argument:{key:"admitted_variants",present:true,value_json:'{"actual":"criterion"}'},parameter_state:"omitted",parameters:{native_arm:"raw_attribute",raw_json:'{"actual":"null operand"}',entries:[]},invoked_arm:"criterion_raw_parameters",parameters_passed:true,native_options_present:false});
const metadata=(kind:"EventTable"|"LazyEventTable"="LazyEventTable",count:number|null=null)=>JSON.stringify({kind,row_count_known:count!==null,row_count:count,table_context_json:'{"encoding":"raw-native","rows":[],"context":"full\\u0000Ω"}'});
function envelope(call=defaultPolarsVariantsOriginalForm().callJson):PolarsVariantsOriginalEnvelope{return {family:"TABLE_VARIANTS_FILTER",engine:"polars",operation:"membership",http_status:200,phase:"complete",apply_returned:true,source:{rows:4,columns:3},result:{rows:2,columns:3},source_unchanged:true,case_id_key:null,attribute_key:null,timestamp_key:null,source_table_json:'{"actual":"source"}',source_after_table_json:'{"actual":"source-after"}',filtered_table_json:'{"actual":"filtered"}',variants_table_json:null,variants_source_unchanged:null,error_type:null,error:null,call_mode:"original",call_json:call,controls_before_json:JSON.stringify(controls()),controls_after_json:JSON.stringify(controls()),controls_unchanged:true,returned_table_metadata_json:metadata()};}
function result(value=envelope()):PolarsVariantsOriginalRun{return {value,sourceFile:source(),fields:[],raw:JSON.stringify(value),responseBytes:new TextEncoder().encode(JSON.stringify(value))};}
const respond=(value:PolarsVariantsOriginalEnvelope)=>vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));
afterEach(()=>vi.unstubAllGlobals());

describe("Polars original variants frozen wire and opaque byte identity",()=>{
  it("offers all4 source-kind descriptors with original keyword-only operands",()=>{
    expect(POLARS_VARIANTS_ORIGINAL_OPERATIONS).toEqual(["membership","from_list","top_k","auto"]);
    for(const operation of POLARS_VARIANTS_ORIGINAL_OPERATIONS){const form=polarsVariantsOriginalExample(operation),value=JSON.parse(form.callJson);expect(Object.keys(value)).toEqual(["schema","input_kind","keyword_entries"]);expect(value.schema).toBe("minera-polars-variants-original-call-v1");expect(value.input_kind).toBe("LazyFrame");expect(value).not.toHaveProperty("args");expect(polarsVariantsOriginalFields(form)).toEqual([["engine","polars"],["operation",operation],["call_json",form.callJson]]);expect(JSON.parse(polarsVariantsOriginalExample(operation,"eager").callJson).input_kind).toBe("DataFrame");}
  });
  it("retains missing-vs-None, unused malformed parameters and ordered enum collisions",()=>{
    const missing=JSON.parse(polarsVariantsOriginalExample("membership","missing").callJson);expect(missing.keyword_entries.map((v:{key:string})=>v.key)).toEqual(["parameters"]);expect(missing.keyword_entries[0].value.type).toBe("str");
    const none=JSON.parse(polarsVariantsOriginalExample("membership","none").callJson);expect(none.keyword_entries[0]).toEqual({key:"admitted_variants",value:{type:"NoneType",value:null}});
    const entries=JSON.parse(polarsVariantsOriginalExample("top_k","enum").callJson).keyword_entries[1].value.items;expect(entries[0][0].type).toBe("str");expect(entries[1][0].module).toBe("pm4py.algo.filtering.polars.variants.variants_filter");expect(entries[1][0].name).toBe("ACTIVITY_KEY");expect(JSON.parse(polarsVariantsOriginalExample("auto","raw").callJson).keyword_entries[0].value.type).toBe("str");
  });
  it("sends exactly4 fields, preserves genuine source File and BOM/CRLF/wide numeric text",async()=>{
    const raw='\uFEFF\r\n{"schema":"minera-polars-variants-original-call-v1","input_kind":"DataFrame","keyword_entries":[{"key":"k","value":{"type":"int","value":9007199254740993}}]}\r\n';
    respond({...envelope(raw),operation:"top_k"});const file=source();await requestPolarsVariantsOriginal(file,{operation:"top_k",callJson:raw});const body=vi.mocked(fetch).mock.calls[0][1]!.body as FormData;
    expect([...body.keys()]).toEqual(["source_file","engine","operation","call_json"]);expect(body.get("source_file")).toBe(file);const field=body.get("call_json")as File;expect(field.name).toBe("call_json.txt");expect(field.type).toBe("application/octet-stream");expect(new Uint8Array(await field.arrayBuffer())).toEqual(new TextEncoder().encode(raw));expect(body.has("variants_file")).toBe(false);expect(body.has("header_file")).toBe(false);
  });
  it("imports strict UTF8 without BOM stripping, CRLF normalization or numeric parsing",async()=>{
    const raw='\uFEFF{\r\n"wide":18446744073709551615\r\n}\r\n';expect(await polarsVariantsOriginalCallText(new File([raw],"call.json"))).toBe(raw);await expect(polarsVariantsOriginalCallText(new File([new Uint8Array([255])],"bad.json"))).rejects.toThrow("valid UTF-8");
  });
  it("forwards malformed and empty opaque text to native control diagnostics",async()=>{
    for(const callJson of ["", "{"]){respond({...envelope(callJson),http_status:400,phase:"control",apply_returned:false,returned_table_metadata_json:null,error_type:"native_error",error:""});const run=await requestPolarsVariantsOriginal(source(),{operation:"membership",callJson});expect(run.value.call_json).toBe(callJson);expect((vi.mocked(fetch).mock.calls[0][1]!.body as FormData).get("call_json")).toBe(callJson);}
  });
  it("validates exactly25 fields and rejects friendly18/pandas24/auxiliary/selector drift",()=>{
    const value=envelope();expect(Object.keys(value).sort()).toEqual([...POLARS_VARIANTS_ORIGINAL_KEYS].sort());expect(isPolarsVariantsOriginalEnvelope(value)).toBe(true);
    for(const changed of [{engine:"pandas"},{case_id_key:"case:concept:name"},{variants_table_json:"{}"},{variants_source_unchanged:true},{timestamp_key:""},{extra:null},{phase:{toString:"complete"}},{source:{rows:-1,columns:2}}])expect(isPolarsVariantsOriginalEnvelope({...value,...changed})).toBe(false);
    const pandas={...value}as Record<string,unknown>;delete pandas.returned_table_metadata_json;expect(isPolarsVariantsOriginalEnvelope(pandas)).toBe(false);for(const key of ["call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json"])delete pandas[key];expect(isPolarsVariantsOriginalEnvelope(pandas)).toBe(false);
  });
  it("requires complete8 controls and preserves missing/raw/ordered actual caller states",()=>{
    const raw=controls();expect(isPolarsVariantsOriginalControls(JSON.stringify(raw))).toBe(true);const missing={...raw,argument:{key:"admitted_variants",present:false,value_json:null},keyword_order:["parameters"],parameter_state:"raw",invoked_arm:"missing_required_default_options",parameters_passed:false};expect(isPolarsVariantsOriginalControls(JSON.stringify(missing))).toBe(true);
    const ordered={...raw,parameter_state:"entries",invoked_arm:"criterion_ordered_parameters",parameters:{native_arm:"ordered_parameter_entries",raw_json:null,entries:[{key:"positive",enum_key:true,value_json:"{}"},{key:"positive",enum_key:false,value_json:"{}"}]}};expect(isPolarsVariantsOriginalControls(JSON.stringify(ordered))).toBe(true);
    for(const changed of [{native_options_present:true},{parameters_passed:false},{argument:{key:["k"],present:true,value_json:"{}"}},{parameters:{native_arm:"raw_attribute",raw_json:null,entries:[]}}])expect(isPolarsVariantsOriginalControls(JSON.stringify({...raw,...changed}))).toBe(false);const partial={...raw}as Record<string,unknown>;delete partial.input_kind;expect(isPolarsVariantsOriginalEnvelope({...envelope(),controls_after_json:JSON.stringify(partial)})).toBe(false);
  });
  it("distinguishes unknown lazy row count, known zero and exact wide count metadata",()=>{
    expect(polarsVariantsReturnedMetadata(metadata())?.row_count_known).toBe(false);expect(polarsVariantsReturnedMetadata(metadata("EventTable",0))?.row_count).toBe(0);
    const wide=metadata("EventTable",0).replace('"row_count":0','"row_count":9007199254740993');expect(String(nativeCell(polarsVariantsReturnedMetadata(wide)!,"row_count"))).toBe("9007199254740993");
    for(const value of ['{"kind":"DataFrame","row_count_known":false,"row_count":null,"table_context_json":"{}"}','{"kind":"EventTable","row_count_known":false,"row_count":null,"table_context_json":"{}"}',metadata().replace('"row_count":null','"row_count":0'),metadata().replace('"kind"','"extra":null,"kind"')])expect(polarsVariantsReturnedMetadata(value)).toBe(null);
  });
  it("keeps native collect diagnostics and independently validates synthetic returned flags",()=>{
    const collect={...envelope(),http_status:400,phase:"collect",result:null,filtered_table_json:null,error_type:"ColumnNotFoundError",error:"full\0diagnostic\ncontext"};expect(isPolarsVariantsOriginalEnvelope(collect)).toBe(true);
    const syntheticUnreturned={...collect,apply_returned:false,returned_table_metadata_json:null,error:""};expect(isPolarsVariantsOriginalEnvelope(syntheticUnreturned)).toBe(true);expect(isPolarsVariantsOriginalEnvelope({...syntheticUnreturned,returned_table_metadata_json:metadata()})).toBe(false);expect(isPolarsVariantsOriginalEnvelope({...collect,error:null})).toBe(false);
  });
  it("rejects wrong engine/operation/call bytes/mode and permits only earlycontrol null capture",async()=>{
    const form=defaultPolarsVariantsOriginalForm();for(const changed of [{operation:"top_k"},{call_json:form.callJson+" "},{engine:"pandas"},{call_mode:null,call_json:null}]){respond({...envelope(),...changed}as PolarsVariantsOriginalEnvelope);await expect(requestPolarsVariantsOriginal(source(),form)).rejects.toThrow();}
    respond({...envelope(),engine:null,operation:null,http_status:400,phase:"control",apply_returned:false,returned_table_metadata_json:null,call_mode:null,call_json:null,error_type:"native_error",error:"duplicate engine"});expect((await requestPolarsVariantsOriginal(source(),form)).value.call_json).toBe(null);
  });
  it("requires genuine source File and checks abort before fetch and after a held response",async()=>{
    const fetcher=vi.fn();vi.stubGlobal("fetch",fetcher);await expect(requestPolarsVariantsOriginal({}as File,defaultPolarsVariantsOriginalForm())).rejects.toThrow("actual native");const controller=new AbortController();controller.abort();await expect(requestPolarsVariantsOriginal(source(),defaultPolarsVariantsOriginalForm(),controller.signal)).rejects.toThrow();expect(fetcher).not.toHaveBeenCalled();
    let release!:(v:Response)=>void;vi.stubGlobal("fetch",vi.fn(()=>new Promise<Response>(resolve=>{release=resolve;})));const active=new AbortController(),pending=requestPolarsVariantsOriginal(source(),defaultPolarsVariantsOriginalForm(),active.signal);active.abort();release(new Response(JSON.stringify(envelope())));await expect(pending).rejects.toThrow();
  });
  it("downloads exact artifacts/raw response/header codec without extracting or rewriting context",async()=>{
    const run=result();run.value.source_after_table_json={native_bytes:"hex-v1",data:"ff000a"};expect(new Uint8Array(await polarsVariantsOriginalArtifactFile(run,"source_after_table_json").arrayBuffer())).toEqual(new Uint8Array([255,0,10]));
    const captured=polarsVariantsReturnedMetadata(run.value.returned_table_metadata_json)!;expect(await polarsVariantsOriginalHeaderContextFile(run).text()).toBe(captured.table_context_json);expect(await polarsVariantsOriginalArtifactFile(run,"returned_table_metadata_json").text()).toBe(run.value.returned_table_metadata_json);expect(new Uint8Array(await polarsVariantsOriginalResponseFile(run).arrayBuffer())).toEqual(run.responseBytes);
    run.value={...run.value,http_status:400,error_type:"TypeError",error:""};expect(()=>polarsVariantsOriginalArtifactFile(run,"filtered_table_json")).toThrow("unavailable");
  });
  it("retains native count tokens and complete response whitespace",async()=>{
    const raw=' \n'+JSON.stringify(envelope()).replace('"rows":4','"rows":9007199254740993')+'\n';vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw)));const run=await requestPolarsVariantsOriginal(source(),defaultPolarsVariantsOriginalForm());expect(String(nativeCell(run.value.source!,"rows"))).toBe("9007199254740993");expect(await polarsVariantsOriginalResponseFile(run).text()).toBe(raw);expect(parseNativeJson(run.raw)).toBeTypeOf("object");
  });
  it("invalidates source,operation,opaque input_kind/call edits and generations; reuses filtered data File",async()=>{
    const form=defaultPolarsVariantsOriginalForm(),identity={source:source(),form},gate=new PolarsVariantsOriginalRequestGate(),ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);
    for(const changed of [{...identity,source:source()},{...identity,form:{...form,operation:"auto"as const}},{...identity,form:{...form,callJson:form.callJson.replace("LazyFrame","DataFrame")}},{...identity,form:{...form,callJson:form.callJson+"\r\n"}}])expect(gate.accepts(ticket,changed)).toBe(false);gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);
    const filtered=polarsVariantsOriginalArtifactFile(result(),"filtered_table_json");respond(envelope());await requestPolarsVariantsOriginal(filtered,form);expect((vi.mocked(fetch).mock.calls[0][1]!.body as FormData).get("source_file")).toBe(filtered);expect(await filtered.text()).toBe('{"actual":"filtered"}');
  });
});
