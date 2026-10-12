import { afterEach,describe,expect,it,vi } from "vitest";
import { defaultGenerationForm,generateTrees,generationReport,generationTables,isTreeGenerationResult,treeGenerationQuery,type TreeGenerationResult } from "../src/tree-generation";
import { nativeResponseJson,nativeResponseRequest } from "../src/native-response";
import { nativeJson,parseNativeJson } from "../src/native-json";
import { diagnosticCsv } from "../src/model-diagnostics";
const fixtures=import.meta.glob<string>("./fixtures/tree-generation-*.json",{eager:true,query:"?raw",import:"default"});
const result=(name:string)=>parseNativeJson(fixtures[`./fixtures/tree-generation-${name}.json`]) as TreeGenerationResult;
afterEach(()=>vi.unstubAllGlobals());
describe("native tree generation workflow",()=>{
  it.each(Object.entries(fixtures))("retains complete real generation result %s",async(_,raw)=>{
    const fetch=vi.fn(async(_path:string)=>new Response(raw));vi.stubGlobal("fetch",fetch);
    const value=await generateTrees(defaultGenerationForm());
    expect(isTreeGenerationResult(value)).toBe(true);expect(nativeResponseJson(value)).toBe(raw);
    expect(nativeResponseRequest(value)?.path).toBe(fetch.mock.calls[0][0]);
    const report=JSON.parse(generationReport(value));expect(report.native_result).toEqual(JSON.parse(raw));expect(report.origin).toBe("native_http_response");
  });
  it("retains all thirty-two models and their independent local registries",()=>{
    const value=result("population");expect(value.models).toHaveLength(32);expect(value.models.at(-1)?.model_index).toBe(31);
    for(const model of value.models){const nodes=generationTables(model).find(t=>t.id==="tree_nodes")!;
      for(const row of nodes.rows)if(row[2]==="activity")expect(row[4]).toBe(model.tree.activities[Number(row[3])]);
      expect(model.ptml).toContain("<ptml");expect(model.pnml).toContain("<pnml");expect(model.bpmn).toContain("definitions");
    }
  });
  it("retains every sampled trace/event, subsecond timestamps and native attributes",()=>{
    const model=result("sampled").models[2];expect(model.playout?.variant).toBe("TOPBOTTOM");
    const table=generationTables(model).find(t=>t.id==="generated_events")!;
    expect(table.rows).toHaveLength(120);expect(table.rows.at(-1)?.slice(0,3)).toEqual([29,3,"case_30"]);
    expect(table.columns).toContain("activity_state");expect(table.columns).toContain("attributes");
    expect(table.rows[1][table.columns.indexOf("timestamp")]).toBe("2026-01-01T00:00:00.250Z");
    expect(diagnosticCsv(table)).toContain('"case_30"');expect(diagnosticCsv(table)).toContain("00:00:00.250Z");
    expect(new Set(table.columns).size).toBe(table.columns.length);
  });
  it("preserves enumerated words and native loop/truncation metadata",()=>{
    const model=result("loop").models[0];expect(model.playout?.variant).toBe("EXTENSIVE");
    expect(model.playout?.return_set_strings).toBe(true);expect(model.playout?.language?.family).toBe("PROCESS_TREE_PLAYOUT");
    expect(generationTables(model).find(t=>t.id==="/playout/language/traces")?.rows.length).toBeGreaterThan(0);
    expect(model.playout?.truncated).toBe(false);
  });
  it("preserves native BASIC Unicode labels and inclusive-choice operators",()=>{
    expect(nativeJson(result("basic-unicode"))).toContain("東京");expect(nativeJson(result("basic-unicode"))).toContain("α,β");
    const nodes=generationTables(result("inclusive").models[0]).find(t=>t.id==="tree_nodes")!;
    expect(nodes.rows.some(row=>row[2]==="inclusive_or")).toBe(true);expect(result("inclusive").tree_variant).toBe("transition_bordered");
  });
  it("keeps an empty population empty and marks uncaptured provenance honestly",()=>{
    const value=result("empty");expect(value.models).toEqual([]);expect(value.generator.trees).toEqual([]);
    expect(JSON.parse(generationReport(value)).origin).toBe("embedded_result");
  });
  it("sends full-width seeds as text and hides unused family controls",()=>{
    const form=defaultGenerationForm();form.generator.seed="18446744073709551615";form.variant="basic";form.generator.activities=JSON.stringify(["α,β","B\\C","東京","D"]);
    const query=new URLSearchParams(treeGenerationQuery(form));expect(query.get("random_tree_seed")).toBe("18446744073709551615");
    expect(query.has("random_tree_mode")).toBe(false);expect(query.has("random_tree_no_models")).toBe(false);
    expect(query.get("random_tree_activities")).toBe("α\\,β,B\\\\C,東京,D");
    expect(query.has("playout_traces")).toBe(false);
  });
  it("preserves omitted defaults and zero models without inventing playout settings",()=>{
    const form=defaultGenerationForm();form.generator.seed="";form.generator.no_models="0";
    const query=new URLSearchParams(treeGenerationQuery(form));expect(query.has("random_tree_seed")).toBe(false);expect(query.get("random_tree_no_models")).toBe("0");
    expect(query.has("playout_seed")).toBe(false);
    form.withLog=true;form.playoutVariant="EXTENSIVE";form.returnStrings=true;
    const extended=new URLSearchParams(treeGenerationQuery(form));expect(extended.has("playout_traces")).toBe(false);expect(extended.get("playout_return_set_strings")).toBe("true");
  });
  it.each(['["A",null]','[]','A,B','[""]'])("rejects malformed activity label input %s",activities=>{
    const form=defaultGenerationForm();form.variant="basic";form.generator.activities=activities;expect(()=>treeGenerationQuery(form)).toThrow(/Activity labels/);
  });
  it("retains original generated timestamp spelling and rejects malformed returned structures",()=>{
    const value=result("sampled");expect(generationTables(value.models[0]).find(t=>t.id==="/playout")?.rows.some(row=>row[0]==="truncated")).toBe(true);
    expect(isTreeGenerationResult({...value,models:[]})).toBe(false);
    expect(isTreeGenerationResult({...value,models:[{...value.models[0],tree:{...value.models[0].tree,root:{operator:"activity",activity:999}}},...value.models.slice(1)]})).toBe(false);
    expect(isTreeGenerationResult({...value,models:[{...value.models[0],pnml:undefined},...value.models.slice(1)]})).toBe(false);
  });
  it("retains native request errors instead of replacing them with generated models",async()=>{
    vi.stubGlobal("fetch",vi.fn(async()=>new Response('{"error":"generated logs exceed the one-million event budget"}',{status:400})));
    await expect(generateTrees(defaultGenerationForm())).rejects.toThrow("one-million event budget");
  });
});
