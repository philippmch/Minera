import { afterEach, describe, expect, it, vi } from "vitest";
import { nativeJson, parseNativeJson } from "../src/native-json";
import { diagnosticCsv } from "../src/model-diagnostics";
import { defaultReplayResultFitnessForm, isReplayResultFitnessValue, replayResultFitnessRequest, replayResultFitnessTables, replayResultRows, runReplayResultFitness, type ReplayResultFitnessValue } from "../src/replay-result-fitness";

function value(): ReplayResultFitnessValue {
  return { family:"REPLAY_RESULT_FITNESS", input_kind:"token",submitted_rows:1,input:[{trace_is_fit:true,trace_fitness:1,produced:4,consumed:4}],
    result:{miner:"precomputed",replay_fitness:{variant:"token_based",computed:true,exhausted:false,error:null,fitness:1,log_fitness:1,average_trace_fitness:1,percentage_of_fitting_traces:100,fitting_traces:1,trace_count:1,total_cost:0}},
    token_totals:{missing:0,remaining:0,produced:4,consumed:4},transition_frequencies:[] };
}
describe("precomputed replay-result fitness", () => {
  afterEach(()=>vi.unstubAllGlobals());
  it("preserves the original request and omits the native default variant", () => {
    const form=defaultReplayResultFitnessForm(); form.json='[ {"missing":9007199254740993,"consumed":"340282366920938463463374607431768211456"} ]';
    expect(replayResultFitnessRequest(form)).toEqual({input_kind:"token",input_json:form.json});
  });
  it("retains explicit automatic and incompatible native selections for native evaluation", () => {
    const form=defaultReplayResultFitnessForm();form.variant="auto";
    expect(replayResultFitnessRequest(form).fitness_variant).toBe("auto");form.variant="alignment_based";
    expect(replayResultFitnessRequest(form).fitness_variant).toBe("alignment_based");
  });
  it("accepts native token trace containers and empty inputs", () => {
    expect(replayResultRows({trace_diagnostics:[{case_id:"資料☃🚀"}]},"token")).toHaveLength(1);
    expect(replayResultRows({traces:[]},"token")).toEqual([]);
    expect(replayResultRows({replay_fitness:{token_replay:{trace_diagnostics:[{trace_fitness:1}]}}},"token")).toHaveLength(1);
    expect(replayResultFitnessRequest({...defaultReplayResultFitnessForm(),json:"[]"}).input_json).toBe("[]");
  });
  it("preserves null alignment records while rejecting null token records", () => {
    const form={...defaultReplayResultFitnessForm(),kind:"alignment" as const,json:"[null,{\"computed\":false}]"};
    expect(replayResultFitnessRequest(form).input_json).toBe(form.json);
    expect(()=>replayResultFitnessRequest({...form,kind:"token"})).toThrow("Every replay row");
  });
  it("reports malformed documents and unsupported selectors", () => {
    expect(()=>replayResultFitnessRequest({...defaultReplayResultFitnessForm(),json:"["})).toThrow("valid JSON");
    expect(()=>replayResultFitnessRequest({...defaultReplayResultFitnessForm(),json:"{}"})).toThrow("token rows");
    expect(()=>replayResultFitnessRequest({...defaultReplayResultFitnessForm(),variant:"other" as never})).toThrow("native fitness variant");
  });
  it("validates complete source and metric shapes, including null nonfinite summaries", () => {
    const result=value();expect(isReplayResultFitnessValue(result)).toBe(true);
    result.result.replay_fitness.average_trace_fitness=null;expect(isReplayResultFitnessValue(result)).toBe(true);
    result.submitted_rows=2;expect(isReplayResultFitnessValue(result)).toBe(false);
  });
  it("accepts actual native uncomputed auto results without converting them to transport errors", () => {
    const result=value();result.result.replay_fitness.variant="auto";result.result.replay_fitness.computed=false;result.result.replay_fitness.error="incompatible native evaluator";
    expect(isReplayResultFitnessValue(result)).toBe(true);
  });
  it("keeps every original row, Unicode metadata, exact large integers and long arrays in CSV and row JSON", () => {
    const text=JSON.stringify(value()).replace('"produced":4','"produced":9007199254740993');
    const result=parseNativeJson(text) as ReplayResultFitnessValue;
    const row=result.input as Record<string,unknown>[];row[0].case_id='資料,"☃🚀';row[0].extra=Array.from({length:80},(_,i)=>i);
    const table=replayResultFitnessTables(result).find(t=>t.id==="submitted_rows")!;
    expect(table.rows).toHaveLength(1);expect(table.records?.[0]).toBe(row[0]);
    expect(diagnosticCsv(table)).toContain("9007199254740993");expect(diagnosticCsv(table)).toContain('資料,""☃🚀');
    expect(nativeJson(table.records?.[0])).toContain("9007199254740993");
  });
  it("exports explicit null alignment records", () => {
    const result=value();result.input_kind="alignment";result.input=[null,{fitness:1,cost:0,bwc:0}];result.submitted_rows=2;
    const table=replayResultFitnessTables(result).find(t=>t.id==="submitted_rows")!;
    expect(table.records).toEqual(result.input);expect(table.rows[0]).toEqual([0,"null",null,null,null]);
  });
  it("binds the response to the submitted input kind and native variant", async () => {
    const response=value();vi.stubGlobal("fetch",vi.fn(async()=>new Response(JSON.stringify(response),{headers:{"Content-Type":"application/json"}})));
    const form=defaultReplayResultFitnessForm();form.json=JSON.stringify(response.input);const run=await runReplayResultFitness(form);
    expect(run.request.input_json).toBe(form.json);
    expect(vi.mocked(fetch).mock.calls[0][1]?.body).toBe(form.json);
    await expect(runReplayResultFitness({...form,variant:"auto"})).rejects.toThrow("does not match");
    await expect(runReplayResultFitness({...form,json:"[]"})).rejects.toThrow("does not match");vi.unstubAllGlobals();
  });
  it("preserves native HTTP errors and rejects malformed successful responses", async () => {
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"token integer contains a non-decimal digit"}',{status:400})));
    await expect(runReplayResultFitness(defaultReplayResultFitnessForm())).rejects.toThrow("non-decimal digit");
    vi.stubGlobal("fetch",vi.fn(async()=>new Response("invalid")));
    await expect(runReplayResultFitness(defaultReplayResultFitnessForm())).rejects.toThrow("valid native");
  });
});
