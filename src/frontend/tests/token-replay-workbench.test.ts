import { describe,it,expect } from "vitest";
import { defaultTokenReplayWorkbenchForm,tokenReplayWorkbenchFields,isReplayTaggedValue,replayTaggedTable } from "../src/token-replay-workbench";
import { diagnosticCsv } from "../src/model-diagnostics";
import { parseNativeJson,nativeJson } from "../src/native-json";

describe("independent token replay options",()=>{
  it("retains exact strings, explicit false, zero caps and empty native path caches",()=>{
    const form=defaultTokenReplayWorkbenchForm();
    form.adapter="lazyframe";form.replayFields.token_replay_max_rows_to_materialize="0";
    form.replayFields.token_replay_disable_variants="false";form.replayFields.token_replay_places_shortest_path_by_hidden="{}";
    form.replayFields.token_replay_case_id_attribute="bucket";
    const fields=Object.fromEntries(tokenReplayWorkbenchFields(form,"orders.tsv"));
    expect(fields.delimiter).toBe("\t");expect(fields.token_replay_max_rows_to_materialize).toBe("0");
    expect(fields.token_replay_disable_variants).toBe("false");expect(fields.token_replay_places_shortest_path_by_hidden).toBe("{}");
    expect(fields.token_replay_case_id_attribute).toBe("bucket");
  });
  it("sends backwards controls independently while retaining the editable forward choices",()=>{
    const form=defaultTokenReplayWorkbenchForm();form.variant="backwards";
    form.replayFields.token_replay_walk_through_hidden_transitions="false";form.replayFields.token_replay_return_names="true";
    const fields=Object.fromEntries(tokenReplayWorkbenchFields(form,"orders.xes"));
    expect(fields.token_replay_enable_pltr_fitness).toBeUndefined();expect(fields.token_replay_return_names).toBeUndefined();
    expect(fields.token_replay_walk_through_hidden_transitions).toBe("false");expect(form.replayFields.token_replay_return_names).toBe("true");
  });
  it("preserves a supplied path cache without reparsing its exact integers",()=>{
    const form=defaultTokenReplayWorkbenchForm();const cache='{"source":{"target":[18446744073709551617]}}';
    form.replayFields.token_replay_places_shortest_path_by_hidden=cache;
    expect(Object.fromEntries(tokenReplayWorkbenchFields(form,"file.csv")).token_replay_places_shortest_path_by_hidden).toBe(cache);
  });
  it("validates transport choices and malformed JSON before starting a run",()=>{
    const form=defaultTokenReplayWorkbenchForm();form.replayFields.token_replay_places_shortest_path_by_hidden="{";
    expect(()=>tokenReplayWorkbenchFields(form,"file.csv")).toThrow("valid JSON");
    form.replayFields={token_replay_return_names:"yes"};expect(()=>tokenReplayWorkbenchFields(form,"file.csv")).toThrow("true or false");
    form.replayFields={};form.importFields.delimiter="||";expect(()=>tokenReplayWorkbenchFields(form,"file.csv")).toThrow("one character");
  });
});
describe("typed replay result inspection",()=>{
  it("retains integer tokens and float hex values in inspection and CSV",()=>{
    const value=parseNativeJson('{"type":"dict","items":[[{"type":"str","value":"produced_tokens"},{"type":"int","value":18446744073709551617}],[{"type":"str","value":"trace_fitness"},{"type":"float","hex":"0x1.5555555555555p-1"}]]}') as Record<string,unknown>;
    expect(isReplayTaggedValue(value)).toBe(true);const table=replayTaggedTable(value);
    expect(table.rows.length).toBe(5);expect(diagnosticCsv(table)).toContain("18446744073709551617");
    expect(diagnosticCsv(table)).toContain("0x1.5555555555555p-1");expect(nativeJson(table.records?.[2])).toContain("18446744073709551617");
  });
  it("traverses native object dictionary keys, traces and nested event contexts",()=>{
    const value={type:"dict",items:[[{type:"Trace",index:null,attributes:{type:"dict",items:[]},events:[{type:"dict",items:[[{type:"str",value:"amount"},{type:"int",value:7}]]}]},{type:"list",values:[{type:"Transition",index:0,name:{type:"NoneType",value:null},label:{type:"str",value:"A"}}]}]]};
    expect(isReplayTaggedValue(value)).toBe(true);const table=replayTaggedTable(value);
    expect(table.rows.some(row=>row[0]==="/full_result/items/0/0/events/0/items/0/1"&&row[4]===7)).toBe(true);
    expect(table.rows.some(row=>row[0]==="/full_result/items/0/1/values/0/name"&&row[2]==="NoneType")).toBe(true);
  });
  it("rejects incomplete scalar and container carriers",()=>{
    for(const value of [{type:"int",value:"rounded"},{type:"float",hex:"0.5"},{type:"list",values:[null]},{type:"Transition",index:0},{type:"dict",items:[["name",{type:"int",value:2}]]}]) expect(isReplayTaggedValue(value)).toBe(false);
  });
});
