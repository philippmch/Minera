import { afterEach, describe, expect, it, vi } from "vitest";
import { evaluationQuery, evaluateModel } from "../src/api";
import { isModelEvaluation, type ModelEvaluation } from "../src/types";
afterEach(()=>vi.unstubAllGlobals());
const result=():ModelEvaluation=>({miner:"inductive",input_context:{adapter:"dataframe",origin:"cached_model_log",source_id:"9",dataset:"資料.csv",case_count:3,event_count:6,activity_attribute:"activity",case_id_attribute:"case:bucket"},generalization:{variant:"generalization_token",computed:true,error:null,score:.5,executed_transitions:2,transition_count:2,transition_frequencies:[2,2]}});
describe("native model quality input adapters",()=>{
  it("retains native omission and every explicit input shape",()=>{
    expect(evaluationQuery()).toBe("evaluation_family=composite");
    for(const adapter of ["log","stream","dataframe","lazyframe"] as const)expect(new URLSearchParams(evaluationQuery({evaluation_input_adapter:adapter})).get("evaluation_input_adapter")).toBe(adapter);
  });
  it("preserves source identities, custom grouping and explicit zero row caps",()=>{
    const query=new URLSearchParams(evaluationQuery({evaluation_input_adapter:"lazyframe",source_id:"9",evaluation_case_id_attribute:"case:bucket",evaluation_max_rows_to_materialize:0}));
    expect(query.get("source_id")).toBe("9");expect(query.get("evaluation_case_id_attribute")).toBe("case:bucket");expect(query.get("evaluation_max_rows_to_materialize")).toBe("0");
  });
  it("rejects unsupported shapes and empty source identities",()=>{
    expect(()=>evaluationQuery({evaluation_input_adapter:"other" as never})).toThrow("input_adapter");
    expect(()=>evaluationQuery({source_id:" "})).toThrow("source_id");
  });
  it("keeps legacy evaluations and validates additive source/input context",()=>{
    const value=result();expect(isModelEvaluation(value)).toBe(true);delete value.input_context;expect(isModelEvaluation(value)).toBe(true);
    const invalid=result();invalid.input_context!.origin="model_only";expect(isModelEvaluation(invalid)).toBe(false);
    const only=result();only.input_context!.adapter="model_only";only.input_context!.origin="model_only";expect(isModelEvaluation(only)).toBe(true);
  });
  it("binds remote model quality to the requested shape and source",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(result()))));
    const value=await evaluateModel("inductive",{evaluation_family:"generalization",evaluation_input_adapter:"dataframe",source_id:"9"});expect(value.input_context?.adapter).toBe("dataframe");
    await expect(evaluateModel("inductive",{evaluation_input_adapter:"stream"})).rejects.toThrow("does not match");
    await expect(evaluateModel("inductive",{evaluation_input_adapter:"dataframe",source_id:"10"})).rejects.toThrow("does not match");
  });
});
