import {describe,it,expect} from 'vitest';
import records from './fixtures/original-saved-structures-records.json';
import manifest from './fixtures/original-saved-structures-manifest.json';
import {nativeJson,parseNativeJson} from '../src/native-json';
import {readNativeResponse,nativeResponseJson} from '../src/native-response';
import {isSavedStructuralResponse,savedStructuralCarrierFile,savedStructuralCarrierBytes,savedStructuralRequestFile,savedStructuralReusableInputs,savedStructuralReport,savedStructuralTables,type SavedStructuralRun,type SavedStructuralArtifact} from '../src/saved-structural-workbench';
interface Entry{id:string;status:number;response_raw:string;response_sha256:string;request_body:{path:string;bytes:number;sha256:string};originals:Array<{field:string;filename:string|null;file:boolean;hex:string}>;artifacts:Array<{name:SavedStructuralArtifact;hex:string}>;}
const corpus=records as unknown as {entries:Entry[]};
const hex=(b:Uint8Array)=>Array.from(b,x=>x.toString(16).padStart(2,'0')).join(''),unhex=(raw:string)=>Uint8Array.from(raw.match(/../g)??[],x=>parseInt(x,16));
const sha=async(raw:string)=>hex(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(raw))));
describe('Actual saved structural input responses and reusable Files',()=>{
 it('binds the complete retained selected capture without additional HTTP',()=>{
  expect(manifest.feature).toBe(83);expect(manifest.counts.entries).toBe(corpus.entries.length);expect(new Set(corpus.entries.map(x=>x.id)).size).toBe(corpus.entries.length);expect(manifest.counts.actual_http_calls).toBeGreaterThan(corpus.entries.length);expect(manifest.runtime_replay).toBe(false);
 });
 for(const item of corpus.entries)it(item.id,async()=>{
  expect(await sha(item.response_raw)).toBe(item.response_sha256);expect(item.request_body.bytes).toBeGreaterThan(0);expect(item.request_body.sha256).toMatch(/^[0-9a-f]{64}$/);
  const parsed=await readNativeResponse(new Response(item.response_raw,{status:item.status,headers:{'Content-Type':'application/json'}}),{path:'/api/structural-inputs'});
  expect(isSavedStructuralResponse(parsed)).toBe(true);if(!isSavedStructuralResponse(parsed))throw Error('Actual UI response guard rejected saved bytes');
  expect(nativeResponseJson(parsed)).toBe(item.response_raw);expect(parsed.originals).toHaveLength(item.originals.length);
  const originals=item.originals.map((x,i)=>{expect(hex(savedStructuralCarrierBytes(parsed.originals[i].original))).toBe(x.hex);expect(parsed.originals[i].file).toBe(x.file);return {field:x.field,filename:x.filename,file:x.file,bytes:unhex(x.hex),local_file:null};});
  const run={value:parsed,raw:item.response_raw,http_status:item.status,tables:savedStructuralTables(parsed),request:{path:'/api/structural-inputs',operation:parsed.operation,input_format:parsed.input_format,originals}} as unknown as SavedStructuralRun;
  for(let i=0;i<originals.length;i++){const file=savedStructuralRequestFile(run,i);expect(file).toBeInstanceOf(File);expect(hex(new Uint8Array(await file.arrayBuffer()))).toBe(item.originals[i].hex);}
  for(const artifact of item.artifacts){const carrier=parsed.artifacts[artifact.name];expect(carrier?.error).toBeNull();if(!carrier)throw Error('Saved artifact missing');const file=savedStructuralCarrierFile(carrier,artifact.name);expect(file).toBeInstanceOf(File);expect(hex(new Uint8Array(await file.arrayBuffer()))).toBe(artifact.hex);expect(file.type).toBe(artifact.name==='native_output_bytes'?'application/octet-stream':artifact.name==='pnml_bytes'?'application/xml':'application/json');
   if(artifact.name==='constructor_json')expect(nativeJson(parseNativeJson(new TextDecoder().decode(savedStructuralCarrierBytes(carrier))))).toBe(nativeJson(parsed.input.before));
   if(artifact.name==='result_json')expect(nativeJson(parseNativeJson(new TextDecoder().decode(savedStructuralCarrierBytes(carrier))))).toBe(nativeJson(parsed.result?.value));
  }
  if(parsed.native_entered){expect(parsed.input_unchanged).toBe(true);expect(nativeJson(parsed.input.after)).toBe(nativeJson(parsed.input.before));}
  for(const reuse of savedStructuralReusableInputs(run)){expect(reuse.file).toBeInstanceOf(File);expect((await reuse.file.arrayBuffer()).byteLength).toBeGreaterThan(0);}
  expect(savedStructuralReport(run)).toContain(item.response_raw);
 });
});
