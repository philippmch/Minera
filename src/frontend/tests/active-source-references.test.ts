import {afterEach,describe,expect,it,vi} from "vitest";
import {activeMiningSourceAllTablesCsv,activeMiningSourceNativeBlob,activeMiningSourceText,isActiveMiningSourceSnapshot,requestActiveMiningSource} from "../src/active-mining-source";
import {nativeJson,parseNativeJson} from "../src/native-json";
// Node modules typed locally: the frontend build carries no Node declarations.
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {readdirSync,readFileSync} = await nodeModule("node:fs") as {readdirSync(path:string):string[];readFileSync(path:string,encoding:"utf8"):string};
const {join} = await nodeModule("node:path") as {join(...parts:string[]):string};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url:URL|string):string};
afterEach(()=>vi.unstubAllGlobals());

const captures=join(fileURLToPath(new URL(".",import.meta.url)),"fixtures/active-source-workbench/captures");
const complete=readdirSync(captures).filter((name:string)=>name.endsWith(".json")).sort().map(name=>[name,readFileSync(join(captures,name),"utf8")] as const).filter(([,raw])=>{try{const v=parseNativeJson(raw) as Record<string,unknown>;return v.family==="ACTIVE_EVENT_SOURCE";}catch{return false;}});

// The backend's member references: a view equal to the named earlier member.
function referenced(raw:string):string{
 const envelope=parseNativeJson(raw) as {active_source:Record<string,unknown>},s=envelope.active_source,same=(a:unknown,b:unknown)=>a!==null&&nativeJson(a)===nativeJson(b);
 const out={...s};
 if(same(s.input_after,s.input_before))out.input_after={same_as:"input_before"};
 if(same(s.checked_table,s.input_before))out.checked_table={same_as:"input_before"};
 if(same(s.checked_log,s.original_log))out.checked_log={same_as:"original_log"};
 if(same(s.prepared_lazy_table,s.checked_table))out.prepared_lazy_table={same_as:"checked_table"};
 return nativeJson({...envelope,active_source:out});
}
async function inspect(raw:string,sourceId:string){vi.stubGlobal("fetch",vi.fn(async()=>new Response(raw)));return requestActiveMiningSource(sourceId);}

describe("active-source member references",()=>{
 it("has complete captures to compare",()=>expect(complete.length).toBeGreaterThan(20));
 it.each(complete)("inspects %s identically with referenced members",async(_:string,raw:string)=>{
  const compact=referenced(raw),sourceId=(parseNativeJson(raw) as {source_id:string}).source_id;
  expect(compact.length).toBeLessThan(nativeJson(parseNativeJson(raw)).length);
  expect(isActiveMiningSourceSnapshot((parseNativeJson(compact) as {active_source:unknown}).active_source)).toBe(true);
  const full=await inspect(raw,sourceId),small=await inspect(compact,sourceId);
  expect(nativeJson(small.value)).toBe(nativeJson(full.value));
  expect(small.tables.map(t=>t.id)).toEqual(full.tables.map(t=>t.id));
  expect(activeMiningSourceAllTablesCsv(small.tables)).toBe(activeMiningSourceAllTablesCsv(full.tables));
  expect(small.value.input_after).toBe(small.value.input_before);
  // The complete native download writes every referenced member out in full.
  expect(small.referenced).toBe(true);
  expect(full.referenced).toBe(false);
  const downloaded=activeMiningSourceText(await activeMiningSourceNativeBlob(small).arrayBuffer());
  expect(downloaded).not.toContain('"same_as"');
  expect(nativeJson(parseNativeJson(downloaded))).toBe(nativeJson(parseNativeJson(raw)));
  expect(activeMiningSourceText(await activeMiningSourceNativeBlob(full).arrayBuffer())).toBe(raw);
 });
});
