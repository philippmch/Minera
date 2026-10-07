import { afterEach,describe,expect,it,vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { QueryWorkspace } from "../src/components/QueryWorkspace";
import { generatedSql,generateLogSql,isLogQuerySchema,isLogQueryResult,queryCellText,queryPrompt,requestLogQuery,requestQueryFilter,type LogQuerySchema,type LogQueryResult } from "../src/log-query";
import { type MineraExport } from "../src/types";

const schema:LogQuerySchema={family:"LOG_QUERY_SCHEMA",source_id:"source 7",event_count:3,case_count:2,table_name:"dataframe",event_index_column:"__minera_event_index",columns:["case:concept:name","activity","cost","__minera_event_index"],sqlite_available:true,duckdb_configured:true};
const result:LogQueryResult={family:"LOG_QUERY",source_id:schema.source_id,engine:"sqlite",sql:"SELECT * FROM dataframe",event_index_column:schema.event_index_column,columns:["activity","__minera_event_index"],rows:[["Review α",1]],output_truncated:false,can_filter:true,selected_event_indices:[1]};
afterEach(()=>vi.unstubAllGlobals());

describe("log query and filter delivery",()=>{
  it("rejects malformed schema, nonfinite cells and mismatched result rows",()=>{
    expect(isLogQuerySchema(schema)).toBe(true);
    expect(isLogQuerySchema({...schema,event_count:-1})).toBe(false);
    expect(isLogQueryResult(result)).toBe(true);
    expect(isLogQueryResult({...result,rows:[["Review α"]]})).toBe(false);
    expect(isLogQueryResult({...result,rows:[[NaN,1]]})).toBe(false);
    expect(isLogQueryResult({...result,selected_event_indices:[-1]})).toBe(false);
  });
  it("preserves exact integers, dates, nested values, nulls and empty strings in the table",()=>{
    expect(queryCellText({type:"integer",value:"18446744073709551615"})).toBe("18446744073709551615");
    expect(queryCellText({type:"date",value:"2026-10-01T09:00:00+00:00"})).toBe("2026-10-01T09:00:00+00:00");
    expect(queryCellText({type:"blob",value:"00ff"})).toBe("0x00ff");
    expect(queryCellText(null)).toBe("NULL");expect(queryCellText("")).toBe('""');
    expect(queryCellText(["東京",null])).toBe('["東京",null]');
  });
  it("requires the public SQL fence and restores encoded column punctuation",()=>{
    expect(generatedSql('Here is the query:\n```sql\nSELECT "caseordabcchrordabc58ordabcchrconceptordabcchrordabc58ordabcchrname" FROM dataframe\n```')).toBe('SELECT "case:concept:name" FROM dataframe');
    expect(()=>generatedSql("SELECT * FROM dataframe")).toThrow(/SQL block/);
    expect(()=>generatedSql("```sql\n \n```")).toThrow(/nonempty/);
  });
  it("generates schema-only instructions for the selected dialect and whole-case filters",()=>{
    const prompt=queryPrompt(schema,"duckdb","Cases with Review α",true);
    expect(prompt).toContain("DuckDB SELECT");expect(prompt).toContain('"case:concept:name"');
    expect(prompt).toContain("all events in matching cases");expect(prompt).toContain("Cases with Review α");
    expect(prompt).not.toContain("source 7");expect(prompt).not.toContain("3 events");
    expect(queryPrompt(schema,"sqlite","Count events",false)).toContain("SQLite SELECT");
    expect(()=>queryPrompt(schema,"sqlite"," ",false)).toThrow(/question/);
  });
  it("sends literal Unicode SQL with a source revision, engine and validated limit",async()=>{
    const fetch=vi.fn(async(_path:string,_options:RequestInit)=>({ok:true,json:async()=>result}));vi.stubGlobal("fetch",fetch);
    const signal=new AbortController().signal;const sql="SELECT * FROM dataframe WHERE activity='Review α'";
    expect(await requestLogQuery(schema,"sqlite",sql,7,signal)).toEqual(result);
    const [url,options]=fetch.mock.calls[0];expect(url).toBe("/api/log-query?source_id=source+7&engine=sqlite&limit=7");
    expect(options).toEqual({method:"POST",headers:{"Content-Type":"text/plain; charset=utf-8"},body:sql,signal});
    await expect(requestLogQuery(schema,"sqlite",sql,0)).rejects.toThrow(/Row limit/);
    await expect(requestLogQuery(schema,"sqlite"," ",7)).rejects.toThrow(/Enter SQL/);
    expect(fetch).toHaveBeenCalledTimes(1);
  });
  it("refuses a response from a different source and reports engine errors",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:true,json:async()=>({...result,source_id:"new-source"})})));
    await expect(requestLogQuery(schema,"sqlite",result.sql,100)).rejects.toThrow(/source log changed/);
    vi.stubGlobal("fetch",vi.fn(async()=>({ok:false,status:400,json:async()=>({error:"only one SQL query is accepted"})})));
    await expect(requestLogQuery(schema,"sqlite",result.sql,100)).rejects.toThrow(/one SQL/);
  });
  it("exports the complete SQL selection independently of the display limit",async()=>{
    const blob=new Blob(["<log/>"],{type:"application/xml"});const fetch=vi.fn(async(_path:string,_options:RequestInit)=>({ok:true,blob:async()=>blob}));vi.stubGlobal("fetch",fetch);
    expect(await requestQueryFilter(schema,"duckdb",result.sql,"cases","xes")).toBe(blob);
    const [url,options]=fetch.mock.calls[0];expect(url).toBe("/api/log-query/export?source_id=source+7&engine=duckdb&mode=cases&format=xes");
    expect(url).not.toContain("limit");expect(options.body).toBe(result.sql);
  });
  it("keeps generation separate from execution and uses only the displayed schema prompt",async()=>{
    const fetch=vi.fn(async(_path:string,_options:RequestInit)=>({ok:true,json:async()=>({family:"LLM_COMPLETION",provider:"openai",model:"fixture",text:"```sql\nSELECT COUNT(*) FROM dataframe\n```",finish_reason:"stop",usage:{input_tokens:1,output_tokens:1},response:{}})}));vi.stubGlobal("fetch",fetch);
    expect(await generateLogSql(schema,"sqlite","Count events",false,"openai","fixture")).toBe("SELECT COUNT(*) FROM dataframe");
    expect(fetch).toHaveBeenCalledTimes(1);expect(fetch.mock.calls[0][0]).toBe("/api/llm/complete");
    expect(JSON.parse(fetch.mock.calls[0][1].body as string).prompt).toBe(queryPrompt(schema,"sqlite","Count events",false));
  });
  it("lets an active source retry schema loading and disables saved-export query execution",()=>{
    const data={dataset:"fixture.csv",trace_count:2,event_count:3,activities:[],edges:[],variants:[]} as MineraExport;
    const markup=renderToStaticMarkup(createElement(QueryWorkspace,{data,available:true,mining:false,onOpenFiltered:vi.fn()}));
    expect(markup).toContain('class="btn-ghost">Refresh source and runtime</button>');
    expect(markup).toContain("Generate SQL");expect(markup).toContain("Generate and run");
    expect(renderToStaticMarkup(createElement(QueryWorkspace,{data,available:false,mining:false,onOpenFiltered:vi.fn()}))).toContain("Saved exports do not provide an active query source");
  });
});
