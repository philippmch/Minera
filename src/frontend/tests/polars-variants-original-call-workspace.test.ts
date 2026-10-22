import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {describe,expect,it} from "vitest";
import {TableVariantsFilterWorkspace} from "../src/components/TableVariantsFilterWorkspace";
import {PolarsVariantsOriginalCallWorkspace,PolarsVariantsOriginalInspection} from "../src/components/PolarsVariantsOriginalCallWorkspace";
import {POLARS_VARIANTS_ORIGINAL_OPERATIONS,POLARS_VARIANTS_ORIGINAL_LABELS,defaultPolarsVariantsOriginalForm,type PolarsVariantsOriginalEnvelope,type PolarsVariantsOriginalRun} from "../src/polars-variants-original-call";
const render=(node:Parameters<typeof renderToStaticMarkup>[0])=>renderToStaticMarkup(node);
function result():PolarsVariantsOriginalRun {
  const controls=JSON.stringify({input_kind:"LazyFrame",keyword_order:[],argument:{key:null,present:false,value_json:null},parameter_state:"omitted",parameters:{native_arm:"raw_attribute",raw_json:"{}",entries:[]},invoked_arm:"auto_raw_parameters",parameters_passed:true,native_options_present:false});
  const value:PolarsVariantsOriginalEnvelope={family:"TABLE_VARIANTS_FILTER",engine:"polars",operation:"membership",http_status:200,phase:"complete",apply_returned:true,source:{rows:4,columns:3},result:{rows:2,columns:3},source_unchanged:true,case_id_key:null,attribute_key:null,timestamp_key:null,source_table_json:"{}",source_after_table_json:"{}",filtered_table_json:"{}",variants_table_json:null,variants_source_unchanged:null,error_type:null,error:null,call_mode:"original",call_json:defaultPolarsVariantsOriginalForm().callJson,controls_before_json:controls,controls_after_json:controls,controls_unchanged:true,returned_table_metadata_json:JSON.stringify({kind:"LazyEventTable",row_count_known:false,row_count:null,table_context_json:'{"encoding":"raw-native","rows":[]}'})};
  return {value,sourceFile:new File(["{}"],"source.json"),fields:[],raw:"{}",responseBytes:new Uint8Array([123,125])};
}
describe("Polars original variants SSR (browser events require separate root acceptance)",()=>{
  it("keeps friendly defaults and isolates original engine editors",()=>{
    const friendly=render(createElement(TableVariantsFilterWorkspace,{engine:"polars"}));expect(friendly).toContain("Polars variants call mode");expect(friendly).toContain("Variants filter");expect(friendly).not.toContain("Complete original Polars variants call JSON");
    const pandas=render(createElement(TableVariantsFilterWorkspace,{engine:"pandas",initialCallMode:"original"}));expect(pandas).toContain("Complete original pandas variants call JSON");expect(pandas).not.toContain("Complete original Polars variants call JSON");
  });
  it("offers all4 callable operations, exact raw editor/import and one native source upload",()=>{
    const html=render(createElement(TableVariantsFilterWorkspace,{engine:"polars",initialCallMode:"original"}));for(const operation of POLARS_VARIANTS_ORIGINAL_OPERATIONS)expect(html).toContain(POLARS_VARIANTS_ORIGINAL_LABELS[operation]);for(const label of ["Original Polars variants source table","Complete original Polars variants call JSON","Import original Polars variants call JSON file","Run original Polars variants call"])expect(html).toContain(label);
    expect(html).toContain('disabled=""');expect(html).not.toContain("Original supplied variants table");expect(html).not.toContain("Case ID column");expect(html).not.toContain("Admitted variants as JSON");expect(html).not.toContain("header_file");
  });
  it("keeps source kind inside descriptor and explains intentional example replacement",()=>{
    const html=render(createElement(PolarsVariantsOriginalCallWorkspace));for(const label of ["input_kind","DataFrame","LazyFrame","Missing required control with unused parameters","Explicit None criterion","Ordered enum and string keys","Raw parameter diagnostic","Loading an example intentionally replaces the editor","importing a call never rewrites it","native AttributeError"])expect(html).toContain(label);
  });
  it("exposes all complete artifacts, download-only header context and filtered data reuse",()=>{
    const html=render(createElement(PolarsVariantsOriginalInspection,{run:result(),onUseTable:()=>{}}));for(const label of ["Download exact original Polars variants call","Download complete original Polars variants source table","Download original Polars variants source after processing","Download complete original Polars variants controls before processing","Download complete original Polars variants controls after processing","Download returned Polars variants metadata","Download native returned header context","Download original filtered native Polars variants table","Use filtered Polars variants table as source","Download complete original Polars variants response"])expect(html).toContain(label);
    expect(html).toContain("row count: unknown");expect(html).toContain("cannot be reused as source data");expect(html).not.toContain("Use header");expect(html).not.toContain("activity column: null");
  });
  it("retains collect-error metadata and actual flags while suppressing filtered reuse",()=>{
    const run=result();run.value={...run.value,http_status:400,phase:"collect",result:null,filtered_table_json:null,error_type:"ColumnNotFoundError",error:"complete native diagnostic\ncontext"};const html=render(createElement(PolarsVariantsOriginalInspection,{run,onUseTable:()=>{}}));expect(html).toContain("ColumnNotFoundError: ");expect(html).toContain("complete native diagnostic");expect(html).toContain("phase: collect; apply returned: yes");expect(html).toContain("Download returned Polars variants metadata");expect(html).toContain("Download original Polars variants source after processing");expect(html).not.toContain("Use filtered Polars variants table as source");
    // Synthetic renderer independence check; the retained411 bank has no eager collect-error observation.
    run.value={...run.value,apply_returned:false,returned_table_metadata_json:null,error:""};const synthetic=render(createElement(PolarsVariantsOriginalInspection,{run}));expect(synthetic).toContain("phase: collect; apply returned: no");expect(synthetic).not.toContain("Download native returned header context");
  });
  it("distinguishes known zero returned rows from unknown header rows",()=>{
    const run=result();run.value={...run.value,result:{rows:0,columns:3},returned_table_metadata_json:JSON.stringify({kind:"EventTable",row_count_known:true,row_count:0,table_context_json:'{"rows":[]}'})};const html=render(createElement(PolarsVariantsOriginalInspection,{run}));expect(html).toContain("EventTable; row count: 0");expect(html).toContain("No rows matched");expect(html).toContain("empty rows to describe actual native context");
  });
  it("reports actual native mutation flags with full snapshots available",()=>{
    const run=result();run.value={...run.value,source_unchanged:false,controls_unchanged:false};const html=render(createElement(PolarsVariantsOriginalInspection,{run}));expect(html).toContain("Complete source table changed.");expect(html).toContain("Complete caller controls changed.");expect(html).toContain("Download complete original Polars variants controls after processing");
  });
});
