import {createElement} from "react";
import {renderToStaticMarkup} from "react-dom/server";
import {describe,expect,it} from "vitest";
import {TableVariantsFilterWorkspace} from "../src/components/TableVariantsFilterWorkspace";
import {PandasVariantsOriginalCallWorkspace,PandasVariantsOriginalInspection} from "../src/components/PandasVariantsOriginalCallWorkspace";
import {PANDAS_VARIANTS_ORIGINAL_OPERATIONS,PANDAS_VARIANTS_ORIGINAL_LABELS,defaultPandasVariantsOriginalForm,type PandasVariantsOriginalEnvelope,type PandasVariantsOriginalRun} from "../src/pandas-variants-original-call";
const render=(node:Parameters<typeof renderToStaticMarkup>[0])=>renderToStaticMarkup(node);
function result():PandasVariantsOriginalRun {const value:PandasVariantsOriginalEnvelope={family:"TABLE_VARIANTS_FILTER",engine:"pandas",operation:"membership",http_status:200,phase:"complete",apply_returned:true,source:{rows:4,columns:3},result:{rows:2,columns:3},source_unchanged:true,case_id_key:null,attribute_key:null,timestamp_key:null,source_table_json:"{}",source_after_table_json:"{}",filtered_table_json:"{}",variants_table_json:"{}",variants_source_unchanged:true,error_type:null,error:null,call_mode:"original",call_json:"{}",controls_before_json:"{}",controls_after_json:"{}",controls_unchanged:true};return {value,sourceFile:new File(["{}"],"source.json"),variantsFile:null,fields:[],raw:"{}",responseBytes:new Uint8Array([123,125])};}
describe("pandas original variants rendering (SSR only; browser events accepted separately)",()=>{
  it("preserves friendly pandas defaults and keeps the original choice pandas-only",()=>{
    const friendly=render(createElement(TableVariantsFilterWorkspace,{engine:"pandas"}));expect(friendly).toContain("Pandas variants call mode");expect(friendly).toContain("Admitted variants as JSON");expect(friendly).not.toContain("Complete original pandas variants call JSON");
    const polars=render(createElement(TableVariantsFilterWorkspace,{engine:"polars",initialCallMode:"original"}));expect(polars).not.toContain("Pandas variants call mode");expect(polars).not.toContain("Complete original pandas variants call JSON");expect(polars).toContain("Variants filter");
  });
  it("shows all4 original operations, source/aux File inputs and exact raw import without friendly selectors",()=>{
    const html=render(createElement(TableVariantsFilterWorkspace,{engine:"pandas",initialCallMode:"original"}));for(const operation of PANDAS_VARIANTS_ORIGINAL_OPERATIONS)expect(html).toContain(PANDAS_VARIANTS_ORIGINAL_LABELS[operation]);for(const label of ["Original pandas variants source table","Original supplied variants table","Complete original pandas variants call JSON","Import original pandas variants call JSON file","Run original pandas variants call"])expect(html).toContain(label);expect(html).toContain('disabled=""');expect(html).not.toContain("Case ID column");expect(html).not.toContain("Selection</span>");expect(html).not.toContain("Admitted variants as JSON");
  });
  it("explains actual auxiliary table reference and exposes typed dictionary/enum/raw examples",()=>{
    const html=render(createElement(PandasVariantsOriginalCallWorkspace,{initialForm:defaultPandasVariantsOriginalForm()}));for(const label of ["pandas.DataFrame","variants_file","variants_df","Typed dictionary and wide integer","Ordered enum and string keys","Raw parameter diagnostic","Download original variants mapping example"])expect(html).toContain(label);expect(html).toContain("unused upload");
  });
  it("offers complete actual artifacts and filtered native File reuse without fabricated roles",()=>{
    const html=render(createElement(PandasVariantsOriginalInspection,{run:result(),onUseTable:()=>{}}));for(const label of ["Download exact original pandas variants call","Download complete original variants source table","Download original variants source after processing","Download complete original variants controls before processing","Download complete original variants controls after processing","Download actual supplied variants table","Download original filtered native variants table","Use filtered variants table as source","Download complete original pandas variants response"])expect(html).toContain(label);expect(html).not.toContain("activity column: null");expect(html).toContain("no inferred friendly column selectors");
  });
  it("keeps native error diagnostics/source-after/aux/controls and suppresses result reuse",()=>{
    const run=result();run.value={...run.value,http_status:400,phase:"apply",apply_returned:false,result:null,filtered_table_json:null,error_type:"TypeError",error:"complete original native diagnostic"};const html=render(createElement(PandasVariantsOriginalInspection,{run,onUseTable:()=>{}}));expect(html).toContain("TypeError: ");expect(html).toContain("complete original native diagnostic");expect(html).toContain("Download original variants source after processing");expect(html).toContain("Download complete original variants controls after processing");expect(html).toContain("Download actual supplied variants table");expect(html).not.toContain("Use filtered variants table as source");
  });
  it("reports genuine native mutation flags instead of assuming preservation",()=>{
    const run=result();run.value={...run.value,source_unchanged:false,controls_unchanged:false,variants_source_unchanged:false};const html=render(createElement(PandasVariantsOriginalInspection,{run}));expect(html).toContain("Complete source table changed.");expect(html).toContain("Complete caller controls changed.");expect(html).toContain("Actual supplied variants table changed.");
  });
});
