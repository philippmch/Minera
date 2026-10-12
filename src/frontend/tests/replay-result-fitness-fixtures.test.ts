import { describe, expect, it } from "vitest";
import { nativeJson, parseNativeJson } from "../src/native-json";
import { diagnosticCsv } from "../src/model-diagnostics";
import { isReplayResultFitnessValue, replayResultFitnessTables, replayResultRows, type ReplayResultFitnessValue } from "../src/replay-result-fitness";

import requestsJson from "./fixtures/replay-result-request-inputs.json";
const requests=requestsJson as Record<string,{input_kind:"token"|"alignment";fitness_variant:string|null;input_json:string}>;
const fixtureText=import.meta.glob("./fixtures/replay-result-fitness-*.json",{query:"?raw",import:"default",eager:true}) as Record<string,string>;
const fixtures=Object.keys(fixtureText).map(path=>path.slice(path.lastIndexOf("/")+1));
const load=(name:string)=>fixtureText["./fixtures/"+name];
describe("actual native precomputed replay fitness outputs",()=>{
  for(const name of fixtures) it(name,()=>{
    const text=load(name),value=parseNativeJson(text);
    expect(isReplayResultFitnessValue(value)).toBe(true);
    const result=value as ReplayResultFitnessValue,request=requests[name.slice("replay-result-fitness-".length,-5)];
    expect(result.input_kind).toBe(request.input_kind);
    expect(result.result.replay_fitness.variant).toBe(request.fitness_variant??(request.input_kind==="token"?"token_based":"alignment_based"));
    expect(nativeJson(result.input)).toBe(nativeJson(parseNativeJson(request.input_json)));
    const rows=replayResultRows(result.input,result.input_kind),tables=replayResultFitnessTables(result),submitted=tables.find(table=>table.id==="submitted_rows")!;
    expect(submitted.rows.length).toBe(rows.length);expect(submitted.records).toEqual(rows);
    for(const table of tables)expect(diagnosticCsv(table).endsWith("\r\n")).toBe(true);
  });
  it("retains all thirty actual native fixtures",()=>expect(fixtures).toHaveLength(30));
  it("preserves unbounded integer tokens in native input and aggregate CSV",()=>{
    const result=parseNativeJson(load("replay-result-fitness-token-huge.json")) as ReplayResultFitnessValue;
    const token="1"+"0".repeat(199)+"1";
    expect(nativeJson(result.input)).toContain(token);
    expect(replayResultFitnessTables(result).some(table=>diagnosticCsv(table).includes(token))).toBe(true);
  });
  it("retains every submitted record beyond previews, including nested metadata and null rows",()=>{
    for(const kind of ["token","alignment"]){
      const result=parseNativeJson(load(`replay-result-fitness-${kind}-many.json`)) as ReplayResultFitnessValue;
      const table=replayResultFitnessTables(result).find(value=>value.id==="submitted_rows")!;
      expect(table.rows).toHaveLength(180);expect(table.records).toHaveLength(180);
    }
  });
});
