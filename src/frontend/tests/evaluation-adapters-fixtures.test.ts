import { describe, expect, it } from "vitest";
import { isModelEvaluation, type ModelEvaluation } from "../src/types";
import { parseNativeJson } from "../src/native-json";
import { diagnosticTables, diagnosticCsv, type DiagnosticEntry } from "../src/model-diagnostics";
import parameters from "./fixtures/evaluation-request-parameters.json";
const fixtures=import.meta.glob<string>("./fixtures/evaluation-input-*.json",{eager:true,query:"?raw",import:"default"});
describe("actual native quality evaluation adapters",()=>{
  it.each(Object.entries(fixtures))("preserves actual native shape and full diagnostics %s",(path,raw)=>{
    const value=parseNativeJson(raw);expect(isModelEvaluation(value)).toBe(true);
    const result=value as ModelEvaluation,name=path.slice(path.lastIndexOf("/")+1).replace(/^evaluation-input-/,"").replace(/\.json$/,"");
    const q=(parameters as Record<string,Record<string,string>>)[name];
    expect(result.miner).toBe(q.miner);expect(result.input_context?.source_id).toBe(q.source_id);
    expect(result.input_context?.adapter).toBe(q.evaluation_family==="simplicity"?"model_only":q.evaluation_input_adapter);
    const entry:DiagnosticEntry={id:0,kind:"evaluation",label:name,dataset:result.input_context?.dataset??"",model:result.miner,model_generation:1,result};
    for(const table of diagnosticTables(entry))expect(diagnosticCsv(table).endsWith("\r\n")).toBe(true);
  });
  it("retains forty-seven native evaluation fixtures",()=>expect(Object.keys(fixtures)).toHaveLength(47));
  it("preserves native regrouped trace counts independently of original model-input counts",()=>{
    const values=["log","stream","dataframe","lazyframe"].map(adapter=>parseNativeJson(fixtures[`./fixtures/evaluation-input-grouped-${adapter}.json`]) as ModelEvaluation);
    expect(values.map(value=>value.input_context?.case_count)).toEqual([60,60,60,60]);
    expect(values.map(value=>value.replay_fitness?.trace_count)).toEqual([60,3,3,3]);
  });
});
