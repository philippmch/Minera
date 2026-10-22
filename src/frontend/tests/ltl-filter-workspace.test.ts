import {expect,it,vi,afterEach} from "vitest";
import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {LtlFilterWorkspace,LtlFilterInspection} from "../src/components/LtlFilterWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {defaultLtlFilterForm,ltlFilterSourceExample,type LtlFilterEnvelope,type LtlFilterRun} from "../src/ltl-filters";
const display=(element:ReturnType<typeof createElement>)=>renderToStaticMarkup(element);
afterEach(()=>vi.unstubAllGlobals());
function run(failure=false):LtlFilterRun{const value:LtlFilterEnvelope={family:"REFERENCE_LTL_FILTER",call_mode:"friendly",call_json:null,engine:"log",input_kind:null,operation:"eventually",result_kind:failure?null:"log",http_status:failure?400:200,phase:failure?"apply":"complete",apply_returned:!failure,source_format:"native_log",source:{traces:2,events:6},result:failure?null:{traces:1,events:3},source_unchanged:!failure,activity_key:"work",case_id_key:null,attribute_key:"kind",resource_key:"performer",timestamp_key:"time",source_json:"before",source_after_json:"after",result_json:failure?null:"result",filtered_xes:failure?null:"<log/>",xes_error:null,helper_result_json:null,error_type:failure?"TypeError":null,error:failure?"complete diagnostic":null};const raw=JSON.stringify(value);return {value,sourceFile:new File(["source"],"source.json"),fields:[],raw,responseBytes:new TextEncoder().encode(raw)};}
it("mounts the new event-log family while preserving the default existing workspace",()=>{const existing=display(createElement(ReferenceLogFilterWorkspace));expect(existing).toContain("Filter an event log");expect(existing).not.toContain('id="ltl-filter-heading"');const relationships=display(createElement(ReferenceLogFilterWorkspace,{initialLogFamily:"ltl"}));expect(relationships).toContain("Filter activity relationships");expect(relationships).not.toContain('id="reference-log-filter-heading"');});
it("renders engine-specific operands without an eager network request",()=>{const fetch=vi.fn();vi.stubGlobal("fetch",fetch);for(const engine of ["log","pandas","polars"]as const){const html=display(createElement(LtlFilterWorkspace,{engine,initialForm:defaultLtlFilterForm(engine,"next")}));if(engine==="polars"){expect(html).toContain("Ordered activities as JSON");expect(html).not.toContain("Third typed activity");}else {expect(html).toContain("Third typed activity as JSON");if(engine==="pandas")expect(html).toContain("last B and C");}expect(html.includes('value="timestamp_ge"')).toBe(engine==="log");}expect(fetch).not.toHaveBeenCalled();});
it("shows performer roles only for social rules and supports literal keys",()=>{const form={...defaultLtlFilterForm("pandas","four_eyes"),resourceKeyMode:"custom"as const,resourceKey:" custom Ω "};const social=display(createElement(LtlFilterWorkspace,{engine:"pandas",initialForm:form}));expect(social).toContain("Exact performer attribute");expect(social).toContain("custom Ω");expect(social).toContain("Cases missing required activities");const sequence=display(createElement(LtlFilterWorkspace,{engine:"pandas"}));expect(sequence).not.toContain("Performer attribute");expect(sequence).toContain("Case ID column");});
it("runs public timestamp helpers without requiring a file",()=>{const html=display(createElement(LtlFilterWorkspace,{engine:"log",initialForm:defaultLtlFilterForm("log","timestamp_le")}));expect(html).toContain("Typed timestamp differences");expect(html).toContain("Typed difference intervals");expect(html).not.toContain('type="file"');expect(html).not.toContain('disabled=""');expect(html).not.toContain("Relationship selection");});
it("provides usable sequence and performer sources with complete typed table declarations",async()=>{expect(await ltlFilterSourceExample("log").text()).toContain("case_id,activity,resource,timestamp");for(const engine of ["pandas","polars"]as const){const table=JSON.parse(await ltlFilterSourceExample(engine).text());expect(table.columns).toEqual(["case:concept:name","concept:name","org:resource","time:timestamp"]);expect(table.dtypes).toEqual([...Array(3).fill(engine==="polars"?"String":"object"),engine==="polars"?"Datetime(time_unit='us', time_zone='UTC')":"datetime64[ns, UTC]"]);expect(table.rows).toHaveLength(6);}});
it("offers complete source artifacts on errors and result/open actions after success",()=>{const failed=display(createElement(LtlFilterInspection,{run:run(true),onOpenLog:vi.fn()}));for(const text of ["complete diagnostic","Download complete parsed source","Download source after processing","Download relationship response"])expect(failed).toContain(text);expect(failed).not.toContain("Download complete filtered result");expect(failed).not.toContain("Open filtered relationship log");const success=display(createElement(LtlFilterInspection,{run:run(),onOpenLog:vi.fn()}));for(const text of ["Source and filtered relationship data","Download complete filtered result","Download filtered relationship XES","Open filtered relationship log"])expect(success).toContain(text);});
it("retains native result inspection when XES projection fails",()=>{const value=run();value.value.filtered_xes=null;value.value.xes_error="typed export failure";const html=display(createElement(LtlFilterInspection,{run:value,onOpenLog:vi.fn()}));expect(html).toContain("typed export failure");expect(html).toContain("Download complete filtered result");expect(html).not.toContain("Open filtered relationship log");});

it("shows independent boundaries and the engine's active timestamp selectors",()=>{
 const base=defaultLtlFilterForm("pandas");const pandas=display(createElement(LtlFilterWorkspace,{engine:"pandas",initialForm:{...base,boundariesMode:"supplied"}}));expect(pandas).toContain("Typed timestamp difference boundaries");expect(pandas).toContain("Sequence timestamp attribute");
 const disabled=display(createElement(LtlFilterWorkspace,{engine:"pandas",initialForm:{...base,boundariesMode:"supplied",enableTimestamp:"false"}}));expect(disabled).toContain("Typed timestamp difference boundaries");expect(disabled).not.toContain("Sequence timestamp attribute");
 const log=display(createElement(LtlFilterWorkspace,{engine:"log",initialForm:{...defaultLtlFilterForm("log"),enableTimestamp:"true"}}));expect(log).toContain("beginning of each trace");
 const polars=display(createElement(LtlFilterWorkspace,{engine:"polars"}));expect(polars).toContain("Sequence ordering timestamp attribute");expect(polars).not.toContain("Sequence timestamp constraints");expect(polars).not.toContain("Timestamp difference intervals");
});

it("offers eager and lazy Polars inputs and reports the effective type on errors",()=>{
 const form={...defaultLtlFilterForm("polars"),polarsInputKind:"DataFrame"as const};
 const html=display(createElement(LtlFilterWorkspace,{engine:"polars",initialForm:form}));
 expect(html).toContain("Polars input type");expect(html).toContain('<option value="DataFrame" selected="">');
 expect(html).toContain("LazyFrame · deferred filtering");expect(html).toContain("DataFrame · eager filtering");
 for(const engine of ["log","pandas"]as const)expect(display(createElement(LtlFilterWorkspace,{engine}))).not.toContain("Polars input type");
 const failed=run(true);failed.value.engine="polars";failed.value.input_kind="DataFrame";
 const inspection=display(createElement(LtlFilterInspection,{run:failed}));
 expect(inspection).toContain("Polars input type: DataFrame.");expect(inspection).not.toContain("Download complete filtered result");
});

it("keeps friendly controls as the default and offers each engine's original mode",()=>{
 const polars=display(createElement(LtlFilterWorkspace,{engine:"polars"}));
 expect(polars).toContain("Polars call mode");expect(polars).toContain('<option value="friendly" selected="">');
 expect(polars).toContain("Ordered activities as JSON");expect(polars).not.toContain("Original operand presence");
 for(const engine of ["log","pandas"]as const)expect(display(createElement(LtlFilterWorkspace,{engine}))).not.toContain("Polars call mode");
 expect(display(createElement(LtlFilterWorkspace,{engine:"pandas"}))).toContain("pandas call mode");
 expect(display(createElement(LtlFilterWorkspace,{engine:"log"}))).not.toContain("pandas call mode");
 expect(display(createElement(LtlFilterWorkspace,{engine:"log"}))).toContain("Event-log call mode");
});

it("shows exclusive omitted, raw and ordered original controls with explicit fidelity limits",()=>{
 const base={...defaultLtlFilterForm("polars"),callMode:"original"as const,originalOperand:"omitted"as const,originalParameters:"raw"as const};
 const raw=display(createElement(LtlFilterWorkspace,{engine:"polars",initialForm:base}));
 for(const text of ["Original operand presence","Raw typed parameters JSON","Import complete original call JSON","explicit null argument","arbitrary Python keys"])expect(raw).toContain(text);
 for(const text of ["Original typed operand JSON","Relationship selection","Matched activity attribute","Activity encoding"])expect(raw).not.toContain(text);
 const entries=display(createElement(LtlFilterWorkspace,{engine:"polars",initialForm:{...base,originalOperand:"provided",originalParameters:"entries"}}));
 expect(entries).toContain("Original typed operand JSON");expect(entries).toContain("Ordered parameter entries JSON");expect(entries).not.toContain("Raw typed parameters JSON");
});

it("retains complete JSON editor text and original call error inspection/download actions",()=>{
 const raw=' {"schema":"minera-polars-ltl-original-call-v1","values":{"type":"integer","value":9007199254740993}}\r\n';
 const form={...defaultLtlFilterForm("polars"),callMode:"original"as const,originalEditor:"json"as const,originalCallJson:raw};
 const html=display(createElement(LtlFilterWorkspace,{engine:"polars",initialForm:form}));
 expect(html).toContain("Complete original call JSON");expect(html).toContain("9007199254740993");expect(html).not.toContain("Original operand presence");
 const failed=run(true);failed.value.engine="polars";failed.value.input_kind="LazyFrame";failed.value.call_mode="original";failed.value.call_json=raw;
 const inspection=display(createElement(LtlFilterInspection,{run:failed}));
 for(const text of ["Call mode: Original typed call.","Complete original typed call","Download exact original call","Download complete parsed source","9007199254740993"])expect(inspection).toContain(text);
 expect(inspection).not.toContain("Download complete filtered result");
});

it("shows pandas argument arrays and arity guidance without Polars operand or input controls",()=>{
 for(const [operation,guidance]of [["eventually","exactly one typed argument containing the activity collection"],["next","exactly three typed arguments in A, B, C order"],["four_eyes","exactly two typed arguments in A, B order"],["different_persons","exactly one typed activity argument"]]as const){
  const form={...defaultLtlFilterForm("pandas",operation),callMode:"original"as const};
  const html=display(createElement(LtlFilterWorkspace,{engine:"pandas",initialForm:form}));
  for(const text of ["pandas call mode","Original ordered arguments JSON",guidance,"minera-pandas-ltl-original-call-v1","pandas.NA","hexadecimal value"])expect(html).toContain(text);
  for(const text of ["Original operand presence","Polars input type","minera-polars-ltl-original-call-v1","Relationship selection","First typed activity as JSON"])expect(html).not.toContain(text);
 }
});

it("retains pandas complete-call text and original error downloads",()=>{
 const call=' {"schema":"minera-pandas-ltl-original-call-v1","args":[{"type":"bytes","value":"00ff"}]}\r\n';
 const form={...defaultLtlFilterForm("pandas"),callMode:"original"as const,originalEditor:"json"as const,originalCallJson:call};
 const html=display(createElement(LtlFilterWorkspace,{engine:"pandas",initialForm:form}));
 expect(html).toContain("Complete original call JSON");expect(html).toContain("00ff");expect(html).not.toContain("Original ordered arguments JSON");
 const failed=run(true);failed.value.engine="pandas";failed.value.call_mode="original";failed.value.call_json=call;
 const inspection=display(createElement(LtlFilterInspection,{run:failed}));
 expect(inspection).toContain("Call mode: Original typed call.");expect(inspection).toContain("Download exact original call");expect(inspection).not.toContain("Polars input type");expect(inspection).not.toContain("Download complete filtered result");
});

it("offers LOG original filters with native sources and typed keys only",()=>{
 const form={...defaultLtlFilterForm("log","next"),callMode:"original"as const,originalParameters:"entries"as const};
 const html=display(createElement(LtlFilterWorkspace,{engine:"log",initialForm:form}));
 for(const label of ["Event-log call mode","Original ordered arguments JSON","exactly three typed arguments","Ordered parameter entries JSON","minera-log-ltl-original-call-v1","Known LOG keys","repeated underlying names","LOG typed nat","Typed uuid","independently of typed matching selectors","complete native EventLog JSON File"])expect(html).toContain(label);
 for(const label of ["Relationship source format","CSV columns and delimiter","Download relationship example CSV","Relationship selection","Matched activity attribute","Original operand presence","Polars input type","pm4py:param:case_id_key"])expect(html).not.toContain(label);
 expect(html).toContain('accept=".json"');
});

it("offers source-less original LOG helper editors without filter parameters",()=>{
 for(const operation of ["timestamp_ge","timestamp_le"]as const){
  const form={...defaultLtlFilterForm("log",operation),callMode:"original"as const,originalParameters:"entries"as const};
  const html=display(createElement(LtlFilterWorkspace,{engine:"log",initialForm:form}));
  for(const label of ["Original ordered arguments JSON","exactly two typed arguments","Helpers accept no source or parameters","Import complete original call JSON","Check timestamp difference bounds"])expect(html).toContain(label);
  for(const label of ["Relationship event log file","Original parameters mode","Ordered parameter entries JSON","Typed timestamp differences","Relationship source format"])expect(html).not.toContain(label);
  expect(html).not.toContain('disabled=""');
  const complete=display(createElement(LtlFilterWorkspace,{engine:"log",initialForm:{...form,originalEditor:"json",originalCallJson:'{"schema":"minera-log-ltl-original-call-v1","args":[{"type":"nat"},{"type":"null"}]}'}}));
  expect(complete).toContain("Complete original call JSON");expect(complete).not.toContain("Original ordered arguments JSON");
 }
});

it("shows LOG original opening roles separately and keeps helper error artifacts",()=>{
 const selected=run();selected.value.call_mode="original";selected.value.call_json='{"schema":"minera-log-ltl-original-call-v1","args":[]}';selected.value.attribute_key=null;selected.value.resource_key=null;selected.value.timestamp_key=null;
 const html=display(createElement(LtlFilterInspection,{run:selected,onOpenLog:vi.fn()}));
 expect(html).toContain("Activity role for reopening:");expect(html).toContain("independent of typed matching selectors");expect(html).toContain("Open filtered relationship log");
 const failed=run(true);Object.assign(failed.value,{operation:"timestamp_ge",call_mode:"original",call_json:selected.value.call_json,source:null,result:null,source_json:null,source_after_json:null,activity_key:null});
 const error=display(createElement(LtlFilterInspection,{run:failed,onOpenLog:vi.fn()}));
 expect(error).toContain("Download exact original call");expect(error).toContain("Download relationship response");expect(error).not.toContain("Download complete parsed source");expect(error).not.toContain("Open filtered relationship log");
});
