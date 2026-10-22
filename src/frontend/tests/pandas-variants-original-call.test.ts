import {afterEach,describe,expect,it,vi} from "vitest";
import {nativeCell} from "../src/native-json";
import {PANDAS_VARIANTS_ORIGINAL_OPERATIONS,PANDAS_VARIANTS_ORIGINAL_KEYS,defaultPandasVariantsOriginalForm,pandasVariantsOriginalExample,pandasVariantsOriginalFields,pandasVariantsOriginalCallText,isPandasVariantsOriginalEnvelope,requestPandasVariantsOriginal,pandasVariantsOriginalArtifactFile,pandasVariantsOriginalResponseFile,PandasVariantsOriginalRequestGate,type PandasVariantsOriginalEnvelope,type PandasVariantsOriginalRun} from "../src/pandas-variants-original-call";
const source=()=>new File(['{"columns":[],"rows":[]}'],"source.json",{type:"application/json"});
function envelope(call=defaultPandasVariantsOriginalForm().callJson):PandasVariantsOriginalEnvelope{return {family:"TABLE_VARIANTS_FILTER",engine:"pandas",operation:"membership",http_status:200,phase:"complete",apply_returned:true,source:{rows:4,columns:2},result:{rows:2,columns:2},source_unchanged:true,case_id_key:null,attribute_key:null,timestamp_key:null,source_table_json:'{"actual":"source"}',filtered_table_json:'{"actual":"filtered"}',variants_table_json:null,variants_source_unchanged:null,error_type:null,error:null,call_mode:"original",call_json:call,controls_before_json:'{"actual":"before"}',controls_after_json:'{"actual":"after"}',controls_unchanged:true,source_after_table_json:'{"actual":"source-after"}'};}
function result(value=envelope()):PandasVariantsOriginalRun{return {value,sourceFile:source(),variantsFile:null,fields:[],raw:JSON.stringify(value),responseBytes:new TextEncoder().encode(JSON.stringify(value))};}
const respond=(value:PandasVariantsOriginalEnvelope)=>vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(value),{status:value.http_status})));
afterEach(()=>vi.unstubAllGlobals());
describe("pandas original variants complete wire and byte identity",()=>{
  it("covers all4 one-criterion Packed examples and exclusive fields",()=>{
    expect(PANDAS_VARIANTS_ORIGINAL_OPERATIONS).toHaveLength(4);
    for(const operation of PANDAS_VARIANTS_ORIGINAL_OPERATIONS){const form=pandasVariantsOriginalExample(operation),v=JSON.parse(form.callJson);expect(Object.keys(v)).toEqual(["schema","args","keyword_entries"]);expect(v.schema).toBe("minera-pandas-variants-original-call-v1");expect(v.args).toHaveLength(1);expect(v.keyword_entries).toEqual([]);expect(pandasVariantsOriginalFields(form)).toEqual([["engine","pandas"],["operation",operation],["call_json",form.callJson]]);}
    expect(JSON.parse(pandasVariantsOriginalExample("membership").callJson).args[0].values[0].type).toBe("tuple");
  });
  it("keeps real auxiliary references, enum order, wide integer dictionary and raw parameters available",()=>{
    const mapping=JSON.parse(pandasVariantsOriginalExample("membership","mapping").callJson);expect(mapping.keyword_entries[0].value.items).toEqual([[{type:"str",value:"variants_df"},{type:"pandas.DataFrame",file:"variants_file"}]]);
    const entries=JSON.parse(pandasVariantsOriginalExample("membership","enum").callJson).keyword_entries[0].value.items;expect(entries[0][0].type).toBe("str");expect(entries[1][0].module).toBe("pm4py.algo.filtering.pandas.variants.variants_filter");expect(entries[1][0].name).toBe("ACTIVITY_KEY");
    const wide=pandasVariantsOriginalExample("top_k","dictionary");expect(wide.operation).toBe("membership");expect(wide.callJson).toContain('"type":"int","value":9007199254740993');expect(wide.callJson).not.toContain("9007199254740992");
    expect(JSON.parse(pandasVariantsOriginalExample("min_coverage","raw").callJson).keyword_entries[0].value.type).toBe("str");
  });
  it("sends exact BOM/CRLF/large tokens in canonical File and preserves source/aux File identity",async()=>{
    const raw='\uFEFF\r\n{"schema":"minera-pandas-variants-original-call-v1","args":[{"type":"int","value":9007199254740993}],"keyword_entries":[]}\r\n';
    respond(envelope(raw));const file=source(),aux=new File(["auxiliary"],"variants.json");const run=await requestPandasVariantsOriginal(file,{operation:"membership",callJson:raw},aux);
    const mock=vi.mocked(fetch),body=mock.mock.calls[0][1]!.body as FormData;expect(mock.mock.calls[0][0]).toBe("/api/reference-table-variants");expect([...body.keys()]).toEqual(["source_file","variants_file","engine","operation","call_json"]);expect(body.get("source_file")).toBe(file);expect(body.get("variants_file")).toBe(aux);const callFile=body.get("call_json") as File;expect(callFile).toBeInstanceOf(File);expect(callFile.name).toBe("call_json.txt");expect(callFile.type).toBe("application/octet-stream");expect(new Uint8Array(await callFile.arrayBuffer())).toEqual(new TextEncoder().encode(raw));expect(run.value.call_json).toBe(raw);
  });
  it("imports strict UTF8 without BOM stripping or number rewriting",async()=>{
    const raw='\uFEFF{\r\n"integer":18446744073709551616\r\n}\r\n';expect(await pandasVariantsOriginalCallText(new File([raw],"call.json"))).toBe(raw);
    await expect(pandasVariantsOriginalCallText(new File([new Uint8Array([0xff])],"bad.json"))).rejects.toThrow("valid UTF-8");
  });
  it("forwards malformed and empty call text for native errors without local JSON parsing",async()=>{
    for(const raw of ["", "{"]){const value:PandasVariantsOriginalEnvelope={...envelope(raw),http_status:400,phase:"control",apply_returned:false,source:null,result:null,source_table_json:null,source_after_table_json:null,filtered_table_json:null,controls_before_json:null,controls_after_json:null,source_unchanged:null,controls_unchanged:null,error_type:"native_error",error:"exact original control diagnostic"};respond(value);const run=await requestPandasVariantsOriginal(source(),{operation:"membership",callJson:raw});expect(run.value.call_json).toBe(raw);expect((vi.mocked(fetch).mock.calls[0][1]!.body as FormData).get("call_json")).toBe(raw);}
  });
  it("validates all24 fields, nullable original roles, native artifacts and rejects friendly18/foreign engines",()=>{
    const value=envelope();expect(Object.keys(value).sort()).toEqual([...PANDAS_VARIANTS_ORIGINAL_KEYS].sort());expect(isPandasVariantsOriginalEnvelope(value)).toBe(true);
    expect(isPandasVariantsOriginalEnvelope({...value,engine:"polars"})).toBe(false);expect(isPandasVariantsOriginalEnvelope({...value,case_id_key:"case:concept:name"})).toBe(false);expect(isPandasVariantsOriginalEnvelope({...value,timestamp_key:""})).toBe(false);expect(isPandasVariantsOriginalEnvelope({...value,extra:null})).toBe(false);
    const missing={...value} as Partial<PandasVariantsOriginalEnvelope>;delete missing.source_after_table_json;expect(isPandasVariantsOriginalEnvelope(missing)).toBe(false);expect(isPandasVariantsOriginalEnvelope({...value,phase:{toString:"complete"}})).toBe(false);expect(isPandasVariantsOriginalEnvelope({...value,source:{rows:-1,columns:2}})).toBe(false);expect(isPandasVariantsOriginalEnvelope({...value,source:{rows:Infinity,columns:2}})).toBe(false);
    const friendly={...value} as Record<string,unknown>;for(const key of ["call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json"])delete friendly[key];expect(isPandasVariantsOriginalEnvelope(friendly)).toBe(false);
  });
  it("accepts native errors including empty text while retaining actual source/control/aux artifacts",()=>{
    const failure={...envelope(),http_status:400,phase:"apply",apply_returned:false,result:null,filtered_table_json:null,error_type:"TypeError",error:"",variants_table_json:{native_bytes:"hex-v1",data:"ff000a"},variants_source_unchanged:true};expect(isPandasVariantsOriginalEnvelope(failure)).toBe(true);expect(isPandasVariantsOriginalEnvelope({...failure,error:null})).toBe(false);
    expect(isPandasVariantsOriginalEnvelope({...failure,phase:"result_export",apply_returned:true,result:{rows:2,columns:2}})).toBe(true);
  });
  it("rejects response identity drift and accepts only earlycontrol null capture",async()=>{
    const form=defaultPandasVariantsOriginalForm();for(const changed of [{operation:"top_k"},{call_json:form.callJson+" "},{call_mode:null,call_json:null}]){respond({...envelope(),...changed} as PandasVariantsOriginalEnvelope);await expect(requestPandasVariantsOriginal(source(),form)).rejects.toThrow();}
    const early:PandasVariantsOriginalEnvelope={...envelope(),http_status:400,phase:"control",engine:null,operation:null,apply_returned:false,source:null,result:null,source_table_json:null,source_after_table_json:null,filtered_table_json:null,controls_before_json:null,controls_after_json:null,source_unchanged:null,controls_unchanged:null,call_mode:null,call_json:null,error_type:"native_error",error:"duplicate field"};respond(early);expect((await requestPandasVariantsOriginal(source(),form)).value.call_json).toBe(null);
  });
  it("requires actual Files and honors aborted calls before fetch",async()=>{
    const fetcher=vi.fn();vi.stubGlobal("fetch",fetcher);await expect(requestPandasVariantsOriginal(null as unknown as File,defaultPandasVariantsOriginalForm())).rejects.toThrow("actual native");await expect(requestPandasVariantsOriginal(source(),defaultPandasVariantsOriginalForm(),{} as File)).rejects.toThrow("actual File");const controller=new AbortController();controller.abort();await expect(requestPandasVariantsOriginal(source(),defaultPandasVariantsOriginalForm(),null,controller.signal)).rejects.toThrow();expect(fetcher).not.toHaveBeenCalled();
  });
  it("preserves opaque artifact bytes and complete original response bytes, with result unavailable on errors",async()=>{
    const run=result();run.value.controls_after_json={native_bytes:"hex-v1",data:"ff000a"};expect(new Uint8Array(await pandasVariantsOriginalArtifactFile(run,"controls_after_json").arrayBuffer())).toEqual(new Uint8Array([255,0,10]));expect(new Uint8Array(await pandasVariantsOriginalResponseFile(run).arrayBuffer())).toEqual(run.responseBytes);
    run.value={...run.value,http_status:400,error_type:"TypeError",error:"fullmessage"};expect(()=>pandasVariantsOriginalArtifactFile(run,"filtered_table_json")).toThrow("unavailable");expect(await pandasVariantsOriginalArtifactFile(run,"source_after_table_json").text()).toBe(run.value.source_after_table_json);
  });
  it("retains exact count tokens and response whitespace without a numeric JSON rewrite",async()=>{
    const raw=JSON.stringify(envelope()).replace('"rows":4','"rows":9007199254740993');vi.stubGlobal("fetch",vi.fn(async()=>new Response(' \n'+raw+'\n',{status:200})));const run=await requestPandasVariantsOriginal(source(),defaultPandasVariantsOriginalForm());expect(String(nativeCell(run.value.source!,"rows"))).toBe("9007199254740993");expect(await pandasVariantsOriginalResponseFile(run).text()).toBe(' \n'+raw+'\n');
  });
  it("binds source+aux+operation+descriptor identities and reuses the genuine filtered File on the next request",async()=>{
    const file=source(),aux=new File(["aux"],"variants.json"),form=defaultPandasVariantsOriginalForm(),identity={source:file,variants:aux,form},gate=new PandasVariantsOriginalRequestGate(),ticket=gate.issue(identity);expect(gate.accepts(ticket,identity)).toBe(true);
    for(const changed of [{...identity,source:source()},{...identity,variants:new File(["aux"],"variants.json")},{...identity,form:{...form,operation:"top_k" as const}},{...identity,form:{...form,callJson:form.callJson+" "}}])expect(gate.accepts(ticket,changed)).toBe(false);gate.invalidate();expect(gate.accepts(ticket,identity)).toBe(false);
    const filtered=pandasVariantsOriginalArtifactFile(result(),"filtered_table_json");respond(envelope());await requestPandasVariantsOriginal(filtered,form);expect((vi.mocked(fetch).mock.calls[0][1]!.body as FormData).get("source_file")).toBe(filtered);expect(await filtered.text()).toBe('{"actual":"filtered"}');
  });
});
