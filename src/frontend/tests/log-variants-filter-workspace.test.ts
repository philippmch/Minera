import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {describe,expect,it} from "vitest";
import {LogVariantsFilterWorkspace,LogVariantsFilterInspection} from "../src/components/LogVariantsFilterWorkspace";
import {ReferenceLogFilterWorkspace} from "../src/components/ReferenceLogFilterWorkspace";
import {defaultLogVariantsForm,LOG_VARIANTS_OPERATIONS,LOG_VARIANTS_LABELS,type LogVariantsEnvelope,type LogVariantsRun} from "../src/log-variants-filters";
const render=(node:Parameters<typeof renderToStaticMarkup>[0])=>renderToStaticMarkup(node);
function result(scalar=false):LogVariantsRun {const value:LogVariantsEnvelope={family:"REFERENCE_LOG_VARIANTS",operation:scalar?"find_auto_threshold":"apply",http_status:200,phase:"complete",source_format:"native_log",source:{traces:3,events:6},result:scalar?null:{traces:2,events:4},source_unchanged:true,call_mode:"original",call_json:"{}",controls_before_json:"{}",controls_after_json:"{}",controls_unchanged:true,source_log_json:"{}",source_after_log_json:"{}",filtered_log_json:scalar?null:"{}",filtered_xes:scalar?null:"<log/>",xes_error:null,value_json:scalar?'{"actual":"typed float storage"}':null,error_type:null,error:null};return {value,sourceFile:new File(["{}"],"source.json"),fields:[],raw:"{}",responseBytes:new Uint8Array([123,125])};}
describe("LOG variants source-required original workspace (SSR, no browser event execution)",()=>{
  it("lists all7 operations and raw editor/import without friendly selectors or source-format conversion",()=>{
    const html=render(createElement(LogVariantsFilterWorkspace));for(const operation of LOG_VARIANTS_OPERATIONS)expect(html).toContain(LOG_VARIANTS_LABELS[operation]);for(const text of ["Variants native event log file","Complete original log variants call JSON","Import original log variants call JSON file","Run original variants call","Download variants example native log"])expect(html).toContain(text);expect(html).toContain('disabled=""');expect(html).not.toContain("CSV columns");expect(html).not.toContain("Source format");expect(html).not.toContain("Selection</span>");
  });
  it("keeps source File required for scalar helper and explains actual source indices",()=>{
    const html=render(createElement(LogVariantsFilterWorkspace,{initialForm:defaultLogVariantsForm("find_auto_threshold")}));expect(html).toContain("This helper returns a typed numeric artifact");expect(html).toContain("Variants native event log file");expect(html).toContain("source_trace_index");expect(html).toContain('disabled=""');
  });
  it("adds an explicit workflow family without replacing the old friendly default",()=>{
    const original=render(createElement(ReferenceLogFilterWorkspace,{initialLogFamily:"variants"}));expect(original).toContain("Complete original log variants call JSON");
    const friendly=render(createElement(ReferenceLogFilterWorkspace));expect(friendly).toContain("Sequences, cases, paths, time and variants");expect(friendly).toContain("Apply log filter");expect(friendly).not.toContain("Complete original log variants call JSON");
  });
  it("offers full source/controls/call/log artifacts and actual native-source reuse",()=>{
    const html=render(createElement(LogVariantsFilterInspection,{run:result(),onUseLog:()=>{}}));for(const text of ["Download exact original variants call","Download complete controls before processing","Download complete controls after processing","Download complete parsed source log","Download source log after processing","Download filtered native log JSON","Use filtered native log as source","Download filtered XES","Download complete variants response"])expect(html).toContain(text);expect(html).not.toContain("Download complete typed threshold");
  });
  it("renders complete typed scalar artifact while omitting every filtered-File/reuse action",()=>{
    const html=render(createElement(LogVariantsFilterInspection,{run:result(true),onUseLog:()=>{}}));expect(html).toContain("Automatic variant threshold");expect(html).toContain("typed float storage");expect(html).toContain("Download complete typed threshold");expect(html).not.toContain("Download filtered native log JSON");expect(html).not.toContain("Use filtered native log as source");expect(html).not.toContain("Download filtered XES");expect(html).toContain("Download complete parsed source log");
  });
  it("retains error artifacts and full diagnostics with no successful-result actions",()=>{
    const run=result();run.value={...run.value,http_status:400,phase:"filter",result:null,filtered_log_json:null,filtered_xes:null,error_type:"TypeError",error:"full typed diagnostic"};const html=render(createElement(LogVariantsFilterInspection,{run,onUseLog:()=>{}}));expect(html).toContain("TypeError: ");expect(html).toContain("full typed diagnostic");expect(html).toContain('role="alert"');expect(html).toContain("Download source log after processing");expect(html).toContain("Download complete controls after processing");expect(html).not.toContain("Use filtered native log as source");
  });
});
