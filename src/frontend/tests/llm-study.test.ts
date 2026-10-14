import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { AskModel } from "../src/components/AskModel";
import { LlmStudies } from "../src/components/LlmStudies";
import { clusterDefinitions,hypothesisDefinitions,studyPrompt,isClusterSummary,requestCluster,requestClusterExport,executeHypotheses } from "../src/llm-study";
import { type LogQuerySchema } from "../src/log-query";
import { type MineraExport } from "../src/types";

const schema:LogQuerySchema={family:"LOG_QUERY_SCHEMA",source_id:"source α",event_count:5,case_count:3,table_name:"dataframe",event_index_column:"__minera_event_index",columns:["case:concept:name","activity","cost","__minera_event_index"],sqlite_available:true,duckdb_configured:true,cluster_runtime_configured:true};
const data={dataset:"fixture.csv",trace_count:3,event_count:5,activities:[{name:"Open",count:2,start_count:2,end_count:0},{name:"Review α",count:1,start_count:0,end_count:1}],edges:[{source:0,target:1,count:1}],variants:[{activities:[0,1],count:1}]} as MineraExport;
const summary={family:"LOG_CLUSTER",source_id:schema.source_id,name:"Review α",regex:"Review",case_count:1,event_count:2,selected_event_indices:[0,1]};
afterEach(()=>vi.unstubAllGlobals());
describe("case clustering and executable hypotheses",()=>{
 it("retains public cluster order, duplicate names and empty lists",()=>{
  expect(clusterDefinitions('```json\n[{"name":"α","regex":"Open"},{"name":"α","regex":"Close"}]\n```')).toEqual([{name:"α",regex:"Open"},{name:"α",regex:"Close"}]);
  expect(clusterDefinitions('```json\n[]\n```')).toEqual([]);
  expect(()=>clusterDefinitions('[{"name":"α","regex":"Open"}]')).toThrow(/json block/);
  expect(()=>clusterDefinitions('```json\n[{"name":"α"}]\n```')).toThrow(/regex fields/);
 });
 it("pairs descriptions with all SQL blocks and restores column punctuation",()=>{
  expect(hypothesisDefinitions('<description>Total α</description>```sql\nSELECT COUNT(*) FROM dataframe\n```<description>Cases</description>```sql\nSELECT "caseordabcchrordabc58ordabcchrconceptordabcchrordabc58ordabcchrname" FROM dataframe\n```')).toEqual([{description:"Total α",sql:"SELECT COUNT(*) FROM dataframe"},{description:"Cases",sql:'SELECT "case:concept:name" FROM dataframe'}]);
  expect(()=>hypothesisDefinitions('<description>Missing</description>')).toThrow(/matching/);
  expect(()=>hypothesisDefinitions('```sql\nSELECT 1\n```')).toThrow(/matching/);
 });
 it("uses real case variants and keeps format instructions after context truncation",()=>{
  const prompt=studyPrompt("clusters",data,schema,"sqlite",10000);expect(prompt).toContain("Open -> Review α ( frequency = 1 )");expect(prompt).toContain("Cases may match");
  expect(studyPrompt("clusters",data,schema,"sqlite",30)).toContain("'regex'");
  const hypotheses=studyPrompt("hypotheses",data,schema,"duckdb",10000);expect(hypotheses).toContain("DuckDB");expect(hypotheses).toContain('"cost"');expect(hypotheses).not.toContain('"__minera_event_index"');
  expect(()=>studyPrompt("clusters",data,schema,"sqlite",0)).toThrow(/limit/);
 });
 it("rejects malformed counts and event identities in cluster summaries",()=>{
  expect(isClusterSummary(summary)).toBe(true);expect(isClusterSummary({...summary,event_count:-1})).toBe(false);expect(isClusterSummary({...summary,selected_event_indices:[false]})).toBe(false);
 });
 it("sends the literal regex and Unicode label with a source revision",async()=>{
  const fetch=vi.fn(async(_path:string,_options:RequestInit)=>({ok:true,json:async()=>summary}));vi.stubGlobal("fetch",fetch);const signal=new AbortController().signal;
  expect(await requestCluster(schema,{name:"Review α",regex:"Review"},signal)).toEqual(summary);
  const [url,options]=fetch.mock.calls[0];expect(url).toBe("/api/log-cluster?source_id=source+%CE%B1&name=Review+%CE%B1&format=summary");expect(options.body).toBe("Review");expect(options.signal).toBe(signal);
 });
 it("refuses stale source results and preserves export failures",async()=>{
  vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,json:async()=>({...summary,source_id:"changed"})})));await expect(requestCluster(schema,{name:"Review",regex:"Review"})).rejects.toThrow(/stale/);
  vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,status:400,json:async()=>({error:"bad regular expression"})})));await expect(requestClusterExport(schema,{name:"Review",regex:"["},"xes")).rejects.toThrow(/bad regular/);
 });
 it("keeps failed hypotheses alongside later real query results",async()=>{
  const fetch=vi.fn(async(_path:string,options:RequestInit)=>{
   expect(options.body).toBeInstanceOf(Uint8Array);const sql=new TextDecoder().decode(options.body as Uint8Array);
   return sql==="SELECT missing"?{ok:false,status:400,json:async()=>({error:"missing field"})}:{ok:true,json:async()=>({family:"LOG_QUERY",source_id:schema.source_id,engine:"sqlite",sql,event_index_column:schema.event_index_column,columns:["events"],rows:[[5]],output_truncated:false,can_filter:false,selected_event_indices:[]})};
  });vi.stubGlobal("fetch",fetch);
  const results=await executeHypotheses(schema,"sqlite",[{description:"Bad",sql:"SELECT missing"},{description:"Count",sql:"SELECT COUNT(*) FROM dataframe"}],7);
  expect(results[0]).toMatchObject({result:null,error:"missing field"});expect(results[1].result?.rows).toEqual([[5]]);expect(fetch).toHaveBeenCalledTimes(2);
 });
 it("stops execution immediately when cancelled",async()=>{
  const fetch=vi.fn();vi.stubGlobal("fetch",fetch);const controller=new AbortController();controller.abort();await expect(executeHypotheses(schema,"sqlite",[{description:"Count",sql:"SELECT 1"}],100,controller.signal)).rejects.toThrow(/cancelled/);expect(fetch).not.toHaveBeenCalled();
 });
 it("exposes all model tasks and guards studies without a source",()=>{
  const markup=renderToStaticMarkup(createElement(AskModel,{data,mining:false}));expect(markup).toContain("Case clustering");expect(markup).toContain("Hypotheses");expect(markup).toContain("Ask a question");
  const study=renderToStaticMarkup(createElement(LlmStudies,{task:"clusters",data,available:false,mining:false}));expect(study).toContain("active study source");expect(study).toContain("disabled");expect(study).toContain("Parse study definitions");
 });
});
