import { afterEach, describe, expect, it, vi } from "vitest";
import { analyzePetriModel, evaluateModel, evaluationQuery } from "../src/api";
import { isModelEvaluation, isPetriAnalysisResult, type ModelEvaluation, type PetriAnalysisResult } from "../src/types";
import { diagnosticComparison, diagnosticComparisonReport, diagnosticCsv, diagnosticPage, diagnosticReport,
  diagnosticTables, parseDiagnosticWeight, publicEvaluationTable, type DiagnosticEntry } from "../src/model-diagnostics";
import { nativeJson, parseNativeJson } from "../src/native-json";
import { nativeResponseJson, nativeResponseRequest, readNativeResponse } from "../src/native-response";

const fixtures = import.meta.glob<string>("./fixtures/model-diagnostics-*.json", {eager:true,query:"?raw",import:"default"});
const raw = (name:string) => fixtures[`./fixtures/model-diagnostics-${name}.json`];
const entry = (name:string,id=1):DiagnosticEntry => ({id,kind:name.startsWith("petri-")?"petri":"evaluation",label:name,
  dataset:"owned-東京.csv",model:"inductive",model_generation:0,result:parseNativeJson(raw(name)) as ModelEvaluation|PetriAnalysisResult});
const table = (name:string,id:string) => diagnosticTables(entry(name)).find(item=>item.id===id)!;
afterEach(()=>vi.unstubAllGlobals());

describe("complete model diagnostics",()=>{
  it.each(Object.entries(fixtures))("retains real native fixture %s through the public API",async(path,text)=>{
    const petri=path.includes("-petri-"), result=JSON.parse(text);
    const fetch=vi.fn(async(_path:string)=>new Response(text,{headers:{"Content-Type":"application/json"}}));
    vi.stubGlobal("fetch",fetch);
    const value=petri?await analyzePetriModel({miner:"inductive",family:result.family}):await evaluateModel("inductive");
    expect(petri?isPetriAnalysisResult(value):isModelEvaluation(value)).toBe(true);
    expect(nativeResponseJson(value)).toBe(text);
    expect(nativeResponseRequest(value)?.path).toBe(fetch.mock.calls[0][0]);
    const report=JSON.parse(diagnosticReport({...entry("composite"),result:value}));
    expect(report.origin).toBe("native_http_response");expect(report.native_result).toEqual(result);
    expect(report.dataset).toBe("owned-東京.csv");
  });
  it("preserves ordered public dictionaries and exact float hex",()=>{
    const result=entry("composite").result as ModelEvaluation;
    const typed=publicEvaluationTable(result.public_result!);
    const top=typed.rows.filter(row=>typeof row[0]==="string"&&/^\/public_result\/items\/\d+$/.test(row[0]));
    expect(top.map(row=>row[3])).toEqual(["fitness","precision","generalization","simplicity","metricsAverageWeight","fscore"]);
    expect(top[0][4]).toBe("dict");expect(top[2][5]).toBe("0x1.b036282d14b12p-1");
    expect(typed.rows.find(row=>row[3]==="perc_fit_traces")?.[5]).toBe("0x1.9000000000000p+6");
    expect(table("composite-inf","public_value").rows.find(row=>row[3]==="metricsAverageWeight")?.[5]).toBe("nan");
    expect(table("composite-nan","public_value").rows.find(row=>row[3]==="metricsAverageWeight")?.[5]).toBe("nan");
  });
  it("does not invent a typed public value for variants that omit it",()=>{
    for(const name of ["fitness-alignment","precision-alignment","composite-zero"]){
      expect(diagnosticTables(entry(name)).some(item=>item.id==="public_value")).toBe(false);
      expect(JSON.parse(diagnosticReport(entry(name))).native_result.public_result).toBeUndefined();
    }
  });
  it("retains every replay trace, nested diagnostic and full last row",()=>{
    const traces=table("fitness-token","/replay_fitness/token_replay/trace_diagnostics");
    expect(traces.rows).toHaveLength(60);expect(traces.rows.at(-1)?.[0]).toBe(59);
    expect(traces.columns).toContain("event_diagnostics");expect(traces.columns).toContain("places");
    expect(nativeJson(traces.records?.at(-1))).toContain("Owned-59-東京");
    expect(diagnosticCsv(traces)).toContain("Owned-59-東京");
    expect(diagnosticPage(traces.rows,2)).toHaveLength(10);
    const native=(entry("fitness-token").result as ModelEvaluation).replay_fitness!.token_replay!;
    expect(traces.records).toEqual(native.trace_diagnostics);
  });
  it("retains all Petri fields, empty witnesses and invariant rows",()=>{
    const native=entry("petri-soundness").result as PetriAnalysisResult;
    expect(table("petri-soundness","summary").rows.map(row=>row[0])).toEqual(Object.keys(native));
    expect(table("petri-soundness","/reachability_markings").records).toEqual(native.reachability_markings);
    expect(table("petri-soundness","/coverability_edges").rows).toHaveLength(4);
    expect(table("petri-soundness","/deadlock_witnesses").rows).toHaveLength(0);
    expect(table("petri-soundness","/place_invariant_basis").rows[0]).toEqual([0,[1,1,1,1]]);
  });
  it("compares actual returned scalar fields and retains failures and model generations",()=>{
    const entries=[entry("composite"),entry("composite-negative",2),entry("composite-inf",3),entry("composite-zero",4),
      {...entry("petri-woflan",5),model:"imported",model_generation:2}];
    const comparison=diagnosticComparison(entries);
    expect(comparison.rows).toHaveLength(5);
    const cell=(i:number,name:string)=>comparison.rows[i][comparison.columns.indexOf(name)];
    expect(cell(2,"composite.metricsAverageWeight")).toBe(null);
    expect(cell(3,"composite.computed")).toBe(false);expect(cell(3,"composite.error")).toBe("composite weights must have a non-zero total");
    expect(cell(4,"model_generation")).toBe(2);expect(cell(4,"composite.computed")).toBeUndefined();
    const saved=JSON.parse(diagnosticComparisonReport(entries));
    expect(saved.entries.map((item:{native_result:unknown})=>item.native_result)).toEqual(entries.map(item=>item.result));
    expect(saved.entries.every((item:{origin:string;request:unknown})=>item.origin==="embedded_result"&&item.request===null)).toBe(true);
  });
  it("exports exact wide integers and signed zero in result, report, properties, selected rows and typed values",async()=>{
    // Deliberately distinguish numeric precision transport from native parity evidence.
    const value=await readNativeResponse(new Response('{"miner":"owned","simplicity":{"complexity":9007199254740993,"value":-0},"public_result":{"type":"int","value":9007199254740993}}'),{path:"/api/evaluate?owned=true"}) as ModelEvaluation;
    const current={...entry("simplicity-cardoso"),result:value};
    expect(diagnosticReport(current)).toContain("9007199254740993");
    expect(diagnosticReport(current)).toContain('"value": -0');
    const properties=diagnosticTables(current).find(item=>item.id==="/simplicity")!;
    expect(diagnosticCsv(properties)).toContain("9007199254740993");
    expect(nativeJson(properties.records?.[0])).toContain("9007199254740993");
    expect(diagnosticCsv(publicEvaluationTable(value.public_result!))).toContain("9007199254740993");
  });
  it.each(["-0","-0.5","NaN","inf","Infinity","-inf","+inf","1e-7",""])("transports native weight input %s",text=>{
    const value=parseDiagnosticWeight(text),query=new URLSearchParams(evaluationQuery({fitness_weight:value}));
    const expected=text===""?null:text==="-0"?"-0":String(value);
    expect(query.get("fitness_weight")).toBe(expected);
  });
  it("reports malformed weights before submitting and preserves typed zero/inf values",()=>{
    for(const value of ["I","Infinityty","0x10","1,2","--1"])expect(()=>parseDiagnosticWeight(value)).toThrow(/Weights/);
    expect(Object.is(parseDiagnosticWeight("-0"),-0)).toBe(true);
    expect(evaluationQuery({fitness_weight:-1,precision_weight:Infinity})).toContain("fitness_weight=-1");
  });
  it("rejects backend error envelopes and invalid or non-success responses",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"missing cached model"}',{status:400})));
    await expect(analyzePetriModel()).rejects.toThrow("missing cached model");
    vi.stubGlobal("fetch",vi.fn(async()=>new Response("{}")));
    await expect(evaluateModel("inductive")).rejects.toThrow(/invalid model evaluation/);
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw("petri-soundness"),{status:500})));
    await expect(analyzePetriModel()).rejects.toThrow();
  });
});
