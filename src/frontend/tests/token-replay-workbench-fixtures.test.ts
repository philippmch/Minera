import { describe,it,expect } from "vitest";
import { isTokenReplayWorkflow,tokenReplayWorkbenchTables } from "../src/token-replay-workbench";
import { diagnosticCsv } from "../src/model-diagnostics";
import { parseNativeJson,nativeJson } from "../src/native-json";
import requests from "./fixtures/token-workbench-6-request-parameters.json";
const fixtures=import.meta.glob<string>("./fixtures/token-workbench-*.json",{eager:true,query:"?raw",import:"default"});

describe("actual independent token-replay HTTP fixtures",()=>{
  for(const [name,request] of Object.entries(requests))it(`retains the native ${name} result, complete diagnostics and submitted options`,()=>{
    const raw=fixtures[`./fixtures/token-workbench-${name}.json`];
    const value=parseNativeJson(raw);expect(isTokenReplayWorkflow(value)).toBe(true);
    if(!isTokenReplayWorkflow(value))throw new Error("Invalid actual native fixture");
    expect(value.variant).toBe(request.fields.token_replay_variant);
    expect(value.input_context.adapter).toBe(request.fields.token_replay_input_adapter);
    expect(value.input_context.dataset).toBe(request.log);expect(value.input_context.model).toBe(request.model);
    const tables=tokenReplayWorkbenchTables(value,Object.entries(request.fields));
    expect(tables.find(table=>table.id==="/diagnostics/traces")?.rows.length).toBe(value.input_context.evaluated_case_count);
    expect(tables.find(table=>table.id==="/model_context/places")?.rows.length).toBe(value.model_context.places.length);
    expect(tables.find(table=>table.id==="request")?.rows).toEqual(Object.entries(request.fields));
    if(value.variant==="forward")expect(tables.find(table=>table.id==="public_trace_rows")?.rows.length).toBe(value.diagnostics.traces.length);
    if(name.startsWith("4-wide-")){
      expect(diagnosticCsv(tables.find(table=>table.id==="/diagnostics/traces")!)).toContain("18446744073709551619");
      expect(diagnosticCsv(tables.find(table=>table.id==="tagged_values")!)).toContain("18446744073709551619");
      expect(nativeJson(value)).toContain("18446744073709551617");
    }
  });
  it("rejects a response detached from its native trace count",()=>{
    const raw=fixtures["./fixtures/token-workbench-4-forward-stream.json"];
    const value=JSON.parse(raw);value.input_context.evaluated_case_count=59;expect(isTokenReplayWorkflow(value)).toBe(false);
    value.input_context.evaluated_case_count=60;value.full_result=null;expect(isTokenReplayWorkflow(value)).toBe(false);
  });
});
