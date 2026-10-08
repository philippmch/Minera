import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import meanRaw from "./fixtures/process-cube-aggregation-mean.json?raw";
import categoricalRaw from "./fixtures/process-cube-categorical.json?raw";
import compositeRaw from "./fixtures/process-cube-composite.json?raw";
import numericRaw from "./fixtures/process-cube-numeric-explicit.json?raw";
import largeRaw from "./fixtures/process-cube-large.json?raw";
import emptyRaw from "./fixtures/process-cube-empty.json?raw";
import { CUBE_AGGREGATIONS, defaultCubeForm, cubeOptions, cubeTables, cubeCellMembers, requestCubeFile, cubeFeatureFile, cubeColourScale, cubeValueText } from "../src/process-cube-workspace";
import { processCubeQuery } from "../src/api";
import { parseNativeJson, nativeJson } from "../src/native-json";
import { isProcessCubeResult, isProcessCubeTable, type ProcessCubeResult } from "../src/types";
import { type TransformationRun } from "../src/transformation-workspace";
import { ocelPreparedCsv } from "../src/ocel-preparation";
import { ProcessCubeWorkspace } from "../src/components/ProcessCubeWorkspace";

const parse = (raw: string) => parseNativeJson(raw) as ProcessCubeResult;
const form = () => { const f = defaultCubeForm(); f.x.name = "x"; f.y.name = "y"; f.value = "value"; return f; };
const source = new File(['{"case_ids":["same","same","a","b"],"columns":{"x":[9007199254740993,2,3,4],"y":["North","North","North","North"],"value":[0.10000000000000001,10,20,30]}}\n'], "cube-東京.json");
afterEach(() => vi.unstubAllGlobals());
describe("native process cube workspace", () => {
  it("accepts six actual native numeric, categorical, composite, large and empty results", () => {
    for (const raw of [meanRaw, categoricalRaw, compositeRaw, numericRaw, largeRaw, emptyRaw]) expect(isProcessCubeResult(parse(raw))).toBe(true);
  });
  it("exposes every native aggregation and preserves escaped composite names", () => {
    const f = form(); f.x.mode = "composite"; f.x.names = '["x,東京","slash\\\\name"]'; f.y.mode = "composite"; f.y.names = '["toString","y"]';
    for (const aggregation of CUBE_AGGREGATIONS) {
      f.aggregation = aggregation;
      const query = new URLSearchParams(processCubeQuery(cubeOptions(f)));
      expect(query.get("aggregation")).toBe(aggregation);
      expect(query.get("x_cols")).toBe("x\\,東京,slash\\\\name"); expect(query.has("x_col")).toBe(false);
      expect(query.get("y_cols")).toBe("toString,y");
    }
  });
  it("passes explicit bins and native division defaults without an artificial grid cap", () => {
    const f = form(); f.x.divisions = "10000"; f.y.divisions = ""; f.x.bins = "[2,0,1]";
    expect(cubeOptions(f)).toMatchObject({ x_bins: [2,0,1], max_divisions_x: 10000 }); expect(cubeOptions(f).max_divisions_y).toBeUndefined();
    f.x.divisions = "0"; expect(() => cubeOptions(f)).toThrow(/10000/);
    f.x.divisions = "4"; f.y.bins = '["1",2]'; expect(() => cubeOptions(f)).toThrow(/numbers/);
    f.y.bins = ""; f.x.mode = "composite"; f.x.names = '[1]'; expect(() => cubeOptions(f)).toThrow(/names/);
  });
  it("maps X-major native matrices to complete cell tables without confusing null and zero", async () => {
    const result = parse(numericRaw), tables = cubeTables(result);
    expect(tables.cells.count).toBe(4);
    expect([0,1,2,3].map(index => tables.cells.rowAt(index).slice(0,5))).toEqual([
      [0,result.x_bins[0].label,0,result.y_bins[0].label,"sum"], [0,result.x_bins[0].label,1,result.y_bins[1].label,"sum"],
      [1,result.x_bins[1].label,0,result.y_bins[0].label,"sum"], [1,result.x_bins[1].label,1,result.y_bins[1].label,"sum"],
    ]);
    expect(cubeValueText(result,0,0)).toBe("0"); expect(cubeValueText(result,0,1)).toBe("null");
    expect(await ocelPreparedCsv(tables.cells).text()).toContain('"sum","null","0"');
  });
  it("exports the full 31 by 12 grid and all members beyond both display pages", async () => {
    const result = parse(largeRaw), tables = cubeTables(result);
    expect(tables.cells.count).toBe(372); expect(tables.memberships.count).toBe(399);
    expect((await ocelPreparedCsv(tables.cells).text()).split("\r\n")).toHaveLength(374);
    expect((await ocelPreparedCsv(tables.memberships).text()).split("\r\n")).toHaveLength(401);
    let target: [number,number] = [0,0];
    for (let x=0;x<result.x_bins.length;x++) for(let y=0;y<result.y_bins.length;y++) if(result.case_ids[x][y].length===28) target=[x,y];
    const selected = cubeCellMembers(result,...target); expect(selected.count).toBe(28);
    expect(selected.rowAt(27)[5]).toBe(result.case_ids[target[0]][target[1]][27]);
    expect((await ocelPreparedCsv(selected).text()).split("\r\n")).toHaveLength(30);
  });
  it("keeps repeated input row IDs distinct from native membership sets", () => {
    const result = parse(meanRaw), tables = cubeTables(result);
    expect(result.case_ids[0][0]).toEqual(["a","b","same"]); expect(tables.memberships.count).toBe(3);
    expect(tables.memberships.rowAt(2)[5]).toBe("same"); expect(tables.cells.rowAt(0)[6]).toBe(3);
  });
  it("retains prototype-like categorical column names and rejects actual collisions", () => {
    const table = JSON.parse('{"case_ids":["a"],"columns":{"x":[1]},"categorical_columns":{"toString":["A"],"__proto__":["B"]}}');
    expect(isProcessCubeTable(table)).toBe(true); table.categorical_columns.x=["A"]; expect(isProcessCubeTable(table)).toBe(false);
  });
  it("uploads original bytes and preserves exact native numeric tokens in inspection and reports", async () => {
    const fetcher = vi.fn(async () => new Response(meanRaw)); vi.stubGlobal("fetch", fetcher);
    const controller = new AbortController(), run = await requestCubeFile(source, form(), controller.signal);
    const [url, init] = fetcher.mock.calls[0] as unknown as [string,RequestInit];
    expect(url).toContain("/api/process-cube?"); expect(init.signal).toBe(controller.signal);
    expect(new TextDecoder().decode(init.body as ArrayBuffer)).toBe(await source.text());
    expect(run.raw).toBe(meanRaw); expect(run.source.sha256).toMatch(/^[a-f0-9]{64}$/);
    expect(run.report_json).toContain("9007199254740993"); expect(run.report_json).toContain("0.10000000000000001");
    expect(run.tables.input_rows.rowAt(0)[2]).toBe("9007199254740993");
    expect(await ocelPreparedCsv(run.tables.input_rows).text()).toContain('"0.10000000000000001"');
  });
  it("rejects mismatched aggregation and foreign or duplicated cell memberships", async () => {
    const result = JSON.parse(meanRaw); result.aggregation="count";
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(result))));
    await expect(requestCubeFile(source,form())).rejects.toThrow(/different/);
    result.aggregation="mean"; result.case_ids[0][0]=["foreign"];
    await expect(requestCubeFile(source,form())).rejects.toThrow(/membership/);
    result.case_ids[0][0]=["same","same"];
    await expect(requestCubeFile(source,form())).rejects.toThrow(/membership/);
  });
  it("respects cancellation before fetch and after response and exposes native errors", async () => {
    const controller = new AbortController(); controller.abort();
    const fetcher = vi.fn(async () => new Response(meanRaw)); vi.stubGlobal("fetch",fetcher);
    await expect(requestCubeFile(source,form(),controller.signal)).rejects.toThrow(); expect(fetcher).not.toHaveBeenCalled();
    const later = new AbortController(); vi.stubGlobal("fetch", vi.fn(async () => { later.abort(); return new Response(meanRaw); }));
    await expect(requestCubeFile(source,form(),later.signal)).rejects.toThrow();
    vi.stubGlobal("fetch",vi.fn(async () => new Response('{"error":"unknown process cube dimension"}',{status:400})));
    await expect(requestCubeFile(source,form())).rejects.toThrow(/unknown process/);
  });
  it("transposes case features without rounding native numbers or deduplicating case rows", async () => {
    const raw = '{"family":"LOG_TRANSFORMATION","source_id":"42","cases":[{"index":0,"case_id":"same","event_count":2},{"index":1,"case_id":"same","event_count":0}],"result":{"family":"LOG_TO_FEATURES","variant":"trace_based","feature_names":["@@case_id_column","cost","one_hot,東京"],"case_ids":["same","same"],"trace_rows":[["same",9007199254740993,0.10000000000000001],["same",0,1]]},"transformed_xes":null}';
    const run = { ...JSON.parse(raw), data_json:raw, report_json:"{}" } as TransformationRun;
    const file = cubeFeatureFile(run), text = await file.text();
    expect(text).toContain('"cost":[9007199254740993,0]'); expect(text).toContain('"one_hot,東京":[0.10000000000000001,1]');
    expect(JSON.parse(text).case_ids).toEqual(["same","same"]); expect(JSON.parse(text).columns).not.toHaveProperty("@@case_id_column");
    expect(JSON.parse(text).minera_feature_source).toEqual({source_id:"42",case_positions:[0,1],omitted_identifier_column:"@@case_id_column"});
    expect(nativeJson(parseNativeJson(text))).toContain("9007199254740993");
    run.source_id="43"; expect(() => cubeFeatureFile(run)).toThrow(/valid feature/);
  });
  it("rejects ambiguous or non-numeric feature schemas and non-case projections", () => {
    const base = {family:"LOG_TRANSFORMATION",source_id:"1",cases:[{index:0,case_id:"a",event_count:1}],result:{family:"LOG_TO_FEATURES",variant:"trace_based",feature_names:["cost","cost"],trace_rows:[[1,2]]},transformed_xes:null};
    const run = () => ({...base,data_json:JSON.stringify(base),report_json:"{}"}) as TransformationRun;
    expect(() => cubeFeatureFile(run())).toThrow(/unique/);
    base.result.feature_names=["cost",""]; expect(() => cubeFeatureFile(run())).toThrow(/non-empty/);
    base.result.feature_names=["cost","other"]; (base.result.trace_rows[0] as unknown[])[1]="Infinity";
    expect(() => cubeFeatureFile(run())).toThrow(/finite numeric/);
    base.result.variant="temporal"; expect(() => cubeFeatureFile(run())).toThrow(/one row per case/);
  });
  it("keeps colour calculations finite for equal, extreme and subnormal native values", () => {
    const result = parse(numericRaw);
    for (const values of [[0,0],[-1e308,1e308],[0,Number.MIN_VALUE]]) {
      result.cells=[[values[0],null],[null,values[1]]]; const colour=cubeColourScale(result);
      for(const value of values) expect(colour(value)).not.toMatch(/NaN|Infinity/); expect(colour(null)).toBe("transparent");
    }
  });
  it("renders accessible independent inputs and every aggregation choice", () => {
    const html=renderToStaticMarkup(createElement(ProcessCubeWorkspace));
    expect(html).toContain('aria-label="Cube table JSON"'); expect(html).toContain("Composite dimensions");
    for(const aggregation of CUBE_AGGREGATIONS) expect(html).toContain('value="'+aggregation+'"');
    expect(html).toContain("distinct IDs"); expect(html).toContain("no event log is required");
  });
});
