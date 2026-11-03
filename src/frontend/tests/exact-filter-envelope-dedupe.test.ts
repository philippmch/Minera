import {afterEach,describe,expect,it,vi} from "vitest";
import {mineActiveDataset} from "../src/api";
import {nativeJson,parseNativeJson} from "../src/native-json";
import retainedMineStream from "./fixtures/exact-filter-discovery93/actual-mine-stream.response.json?raw";
import {EXACT_RAW_PREVIEW_CHARACTERS,exactRawPreview} from "../src/components/OriginalLtlFilter";
import {exactFilterAllTablesCsv,exactFilterArtifacts,exactFilterNativeStringBytes,exactFilterReport,exactFilterRun,withoutLastTopLevelMember,type ExactFilterAudit} from "../src/exact-filter-controls";
afterEach(()=>vi.unstubAllGlobals());

// The retained response rewritten as the deduplicating backend now writes
// it: the JSON-object legacy body is not repeated, and equal source
// snapshots after the first are references.
function deduplicated(raw:string):string{
 const legacy=raw.lastIndexOf('"legacy_response":{"status":');expect(legacy).toBeGreaterThan(0);
 let text=raw.slice(0,legacy)+'"legacy_response":{"status":200,"body":null,"json":null,"json_error":null}}'+raw.slice(raw.trimEnd().length-1);
 const head='"selected_native":{"source_before":',start=text.indexOf(head)+head.length,source=text.slice(start,text.indexOf(',"source_after":',start));
 const replace=(from:string,to:string)=>{const at=text.indexOf(from);expect(at).toBeGreaterThan(0);text=text.slice(0,at)+to+text.slice(at+from.length);};
 replace(`,"source_after":${source},"heuristics_net":`,',"source_after":{"same_as":"source_before"},"heuristics_net":');
 replace(`"source_before":${source},"source_after":${source},"options_before":`,'"source_before":{"same_as":"source_before"},"source_after":{"same_as":"source_after"},"options_before":');
 return text;
}
let files:{source:File;control:File}|undefined;
async function run(raw:string){
 const value=parseNativeJson(retainedMineStream) as {exact_controls:ExactFilterAudit},parts=value.exact_controls.original_inputs,part=(key:string)=>parts.find(p=>p.name===key)!;
 const text=(key:string)=>new TextDecoder("utf-8",{fatal:true}).decode(exactFilterNativeStringBytes(part(key).bytes));
 // The same File objects for both runs, so their reports share timestamps.
 files??={source:new File([exactFilterNativeStringBytes(part("log").bytes)],String(part("log").filename)),control:new File([exactFilterNativeStringBytes(part("ltl_options_file").bytes)],String(part("ltl_options_file").filename))};const {source,control}=files;
 vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw,{status:200})));
 const result=await mineActiveDataset(source,{miningInputShape:"stream",miningRequestId:text("mining_request_id"),ltl_options_file:control});
 return exactFilterRun(result)!;
}

describe("deduplicated exact-filter envelope",()=>{
 it("removes only the last top-level member, keeping every other byte",()=>{
  expect(withoutLastTopLevelMember('{"a":1,"exact_controls":{"x":"}\\",{"},"b":[2]}','exact_controls')).toBe('{"a":1,"b":[2]}');
  expect(withoutLastTopLevelMember('{"exact_controls":{"n":{"exact_controls":1}}}\n','exact_controls')).toBe('{}\n');
  expect(withoutLastTopLevelMember('{\n "a": "exact_controls",\n "b": {"exact_controls": 2}\n,"exact_controls":3}','exact_controls')).toBe('{\n "a": "exact_controls",\n "b": {"exact_controls": 2}\n}');
  expect(withoutLastTopLevelMember('{"a":1}','exact_controls')).toBeUndefined();
  // Whitespace inside an empty body is the body's own, before the appended member.
  expect(withoutLastTopLevelMember('{ "exact_controls":{"a":1}}','exact_controls')).toBe('{ }');
  expect(withoutLastTopLevelMember('{ \r\n\t"exact_controls":[]}\n','exact_controls')).toBe('{ \r\n\t}\n');
  expect(withoutLastTopLevelMember('{"exact_controls":1,"b":2}','exact_controls')).toBe('{"b":2}');
  expect(withoutLastTopLevelMember('{"a":1 ,"exact_controls":1}','exact_controls')).toBe('{"a":1 }');
 });
 it("restores the legacy body bytes, source snapshots, report and tables of the full envelope",async()=>{
  const compact=deduplicated(retainedMineStream);expect(compact.length).toBeLessThan(retainedMineStream.length);
  const full=await run(retainedMineStream),small=await run(compact);
  const legacy=(r:typeof full)=>exactFilterArtifacts(r).find(a=>a.name==="legacy-response.bin")!.bytes;
  expect(legacy(small)).toEqual(legacy(full));
  expect(legacy(small).length).toBeGreaterThan(1000);
  expect(exactFilterArtifacts(small).map(a=>[a.name,a.mime,Array.from(a.bytes).join(",")])).toEqual(exactFilterArtifacts(full).map(a=>[a.name,a.mime,Array.from(a.bytes).join(",")]));
  expect(nativeJson(small.value)).toBe(nativeJson(full.value));
  expect(exactFilterReport(small)).toBe(exactFilterReport(full));
  expect(exactFilterAllTablesCsv(small)).toBe(exactFilterAllTablesCsv(full));
  expect(small.raw).toBe(compact);
 });
 it("caps only the inline envelope preview, never splitting a surrogate pair",()=>{
  const small='{"a":"\ud83d\ude00"}';expect(exactRawPreview(small)).toBe(small);
  const exact="x".repeat(EXACT_RAW_PREVIEW_CHARACTERS);expect(exactRawPreview(exact)).toBe(exact);
  const big="x".repeat(EXACT_RAW_PREVIEW_CHARACTERS-1)+"\ud83d\ude00tail",preview=exactRawPreview(big);
  expect(preview.startsWith("x".repeat(EXACT_RAW_PREVIEW_CHARACTERS-1)+"\n")).toBe(true);
  expect(preview).toContain(`first ${EXACT_RAW_PREVIEW_CHARACTERS-1} of ${big.length} characters`);
 });
});
