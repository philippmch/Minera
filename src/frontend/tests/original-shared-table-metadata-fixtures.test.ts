import {describe,expect,it} from "vitest";
import {nativeCell,nativeJson,parseNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";
import {nativeResponseJson,readNativeResponse} from "../src/native-response";
import {diagnosticCsv,type DiagnosticTable} from "../src/model-diagnostics";
import {isOriginalEmbeddingFilterWorkbench,isOriginalEmbeddingFilterTable,isOriginalEmbeddingFilterRawLog,ORIGINAL_EMBEDDING_FILTER_ARTIFACTS,originalEmbeddingFilterArtifactFile,originalEmbeddingFilterOriginalBytes,originalEmbeddingFilterStringBytes,originalEmbeddingFilterTables,originalEmbeddingFilterReport,type OriginalEmbeddingFilterBytes,type OriginalEmbeddingFilterArtifact,type OriginalEmbeddingFilterNativeString as NativeString,type OriginalEmbeddingFilterWorkbench,type OriginalEmbeddingFilterRun} from "../src/original-embedding-filter-workbench";
import {isOriginalEditDistanceWorkbench,originalEditDistanceArtifactFile,originalEditDistanceOriginalBytes,originalEditDistanceTables,originalEditDistanceReport,originalEditDistanceAllTablesCsv,originalEditDistanceSelectedRecordsJson,type OriginalEditDistanceWorkbench,type OriginalEditDistanceRun,type OriginalEditDistanceBytes} from "../src/original-edit-distance-workbench";
import {isOriginalBoseWorkbench,originalBoseCarrierFile,originalBoseTables,originalBoseReport,type OriginalBoseWorkbench,type OriginalBoseRun} from "../src/original-bose-workbench";
import {isOriginalDiscoveryWorkbench,originalDiscoveryCarrierFile,originalDiscoveryTables,originalDiscoveryReport,type OriginalDiscoveryWorkbench,type OriginalDiscoveryRun} from "../src/original-discovery-workbench";
const nodeModule=(name:string):Promise<unknown>=>import(/* @vite-ignore */ name);
const {createHash}=await nodeModule("node:crypto") as {createHash(name:string):{update(value:Uint8Array|string):{digest(kind:"hex"):string}}};
const {readFileSync}=await nodeModule("node:fs") as {readFileSync:{(path:string):Uint8Array;(path:string,encoding:"utf8"):string}};
const {dirname,resolve}=await nodeModule("node:path") as {dirname(path:string):string;resolve(...paths:string[]):string};
const {fileURLToPath}=await nodeModule("node:url") as {fileURLToPath(url:string):string};
const environment=(globalThis as unknown as {process:{env:Record<string,string|undefined>}}).process.env;

interface Bytes {encoding:"utf8"|"hex";data:string;bytes:number;}
type Route="embedding"|"edit_distance"|"bose"|"discovery";
interface Part {field:NativeString;filename:NativeString|null;filename_attribute_present:boolean;content_type:string|null;bytes:number;sha256:string;header_offset:number;header_bytes:number;payload_offset:number;}
interface SnapshotAssertion {pointer:string;kind:"table"|"lazy"|"log";representation:"legacy"|"raw";input_blob:string;absent?:boolean;metadata_only?:boolean;}
interface Expected {status:number;executed:boolean;returned:boolean;phase:string|null;error_contains:string|null;}
interface Adaptive {kind:"native_artifact";source_case:string;artifact:string;target_role:string;}
interface Handoff {route:Route;artifact:string;role:string;response_sha256:string;actual_ui_helpers:string[];produced_by_actual_ui_helpers:boolean;source_artifact:{sha256:string;bytes:number;pointer:Array<string|number>};file_name:string;file_mime:string;file_bytes:{sha256:string;bytes:number};}
interface Call {id:string;route:Route;operation:string;physical_call_index:number;selection_kind:string;passed:boolean;checks:Array<{passed:boolean}>;expected:Expected;snapshot_assertions:SnapshotAssertion[];returned_tables_metadata_default_blob?:string;minimum_returned_tables?:number;request:{method:string;path:string;mime:string|null;body:string;parts:Part[];query_parts:Array<{field:NativeString;value:NativeString;bytes:number;sha256:string}>};response:{status:number;headers:Array<[string,string]>;body:string};artifacts:Array<{name:string;bytes:number;sha256:string;file_name?:string;file_mime?:string}>;adaptive_capture?:Adaptive;actual_ui_handoff?:Handoff;}
interface Manifest {complete:boolean;feature:number;schema_version:number;bundle:{file_name:string;bytes:number;sha256:string};entries:Array<{id:string;record_index:number;physical_call_index:number}>;counts:Record<string,number>;metadata_fields:string[];}
const directory=dirname(fileURLToPath(import.meta.url));
const manifestPath=environment.MINERA_TABLE_METADATA82_FIXTURE_MANIFEST??resolve(directory,"fixtures/original-shared-table-metadata-manifest.json");
const manifest=JSON.parse(readFileSync(manifestPath,"utf8")) as Manifest;
const bundleBytes=readFileSync(resolve(dirname(manifestPath),manifest.bundle.file_name));
const digest=(value:Uint8Array|string):string=>createHash("sha256").update(value).digest("hex");
if(!manifest.complete||manifest.feature!==82||manifest.schema_version!==1||digest(bundleBytes)!==manifest.bundle.sha256||bundleBytes.length!==manifest.bundle.bytes)throw Error("Complete immutable actual82 fixture corpus required.");
const bundle=parseNativeJson(new TextDecoder("utf8",{fatal:true}).decode(bundleBytes)) as {feature:number;schema_version:number;blobs:Record<string,Bytes>;entries:Call[];auxiliary_calls:Call[];chronology:Array<{physical_call_index:number;kind:"entry"|"auxiliary";index:number}>;history:Record<string,unknown>};
const entries=bundle.entries;
if(bundle.feature!==82||bundle.schema_version!==1||entries.length!==manifest.entries.length||!manifest.entries.every((x,i)=>x.id===entries[i].id&&x.record_index===i&&x.physical_call_index===entries[i].physical_call_index))throw Error("Actual fixture index changed.");
const object=(v:unknown):v is Record<string,unknown>=>v!==null&&typeof v==="object"&&!Array.isArray(v);
const obj=(v:unknown)=>{if(!object(v))throw Error("Native object required.");return v;};
const hex=(bytes:Uint8Array):string=>Array.from(bytes,byte=>byte.toString(16).padStart(2,"0")).join("");
function fromHex(raw:string):Uint8Array<ArrayBuffer>{if(!/^(?:[0-9a-f]{2})*$/i.test(raw))throw Error("Invalid hex.");return Uint8Array.from(raw.match(/../g)??[],pair=>parseInt(pair,16));}
const stringBytes=(value:NativeString):Uint8Array<ArrayBuffer>=>originalEmbeddingFilterStringBytes(value);
function blob(key:string):Uint8Array<ArrayBuffer>{const value=bundle.blobs[key];if(!value)throw Error("Missing pooled bytes.");const bytes=value.encoding==="hex"?fromHex(value.data):nativeStringBytes(value.data);expect(bytes.length).toBe(value.bytes);expect(digest(bytes)).toBe(key);return bytes;}
function scalarText(bytes:Uint8Array):string|null{try{return new TextDecoder("utf8",{fatal:true,ignoreBOM:true}).decode(bytes);}catch{return null;}}
function responseText(call:Call):string{const raw=scalarText(blob(call.response.body));if(raw===null)throw Error("Native response JSON lost UTF8.");return raw;}
function partBytes(call:Call,part:Part):Uint8Array<ArrayBuffer>{const bytes=blob(call.request.body).slice(part.payload_offset,part.payload_offset+part.bytes);expect(bytes.length).toBe(part.bytes);expect(digest(bytes)).toBe(part.sha256);return bytes;}
// Independent numeric token walk includes original holders, signed zero and uint64.
type Numeric={path:Array<string|number>;token:string};
function numericTokens(raw:string):Numeric[]{const result:Numeric[]=[];let position=0;const whitespace=()=>{while(position<raw.length&&/\s/.test(raw[position]))++position;};const string=():string=>{const start=position++;while(position<raw.length){const char=raw[position++];if(char==="\\")++position;else if(char==='"')return JSON.parse(raw.slice(start,position)) as string;}throw new Error("Unterminated JSON string.");};const value=(path:Array<string|number>):void=>{whitespace();const char=raw[position];if(char==='"'){string();return;}if(char==="{"){++position;whitespace();if(raw[position]==="}"){++position;return;}for(;;){whitespace();const name=string();whitespace();if(raw[position++]!==":")throw new Error("Bad object separator.");value([...path,name]);whitespace();const separator=raw[position++];if(separator==="}")return;if(separator!==",")throw new Error("Bad object separator.");}}if(char==="["){++position;whitespace();if(raw[position]==="]"){++position;return;}let index=0;for(;;){value([...path,index++]);whitespace();const separator=raw[position++];if(separator==="]")return;if(separator!==",")throw new Error("Bad array separator.");}}const tail=raw.slice(position),literal=/^(?:true|false|null)/.exec(tail);if(literal){position+=literal[0].length;return;}const numeric=/^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/.exec(tail);if(!numeric)throw new Error("Invalid JSON value.");result.push({path,token:numeric[0]});position+=numeric[0].length;};value([]);whitespace();if(position!==raw.length)throw new Error("Trailing JSON bytes.");return result;}
function numericAt(value:unknown,path:Array<string|number>):string{let holder:unknown=value;for(const key of path.slice(0,-1)){if(!holder||typeof holder!=="object")throw new Error("Lost numeric holder.");holder=(holder as Record<string,unknown>)[key];}if(!holder||typeof holder!=="object"||!path.length)throw new Error("A scalar token requires its original holder.");return String(nativeCell(holder,path[path.length-1]));}
function verifyNumeric(raw:string,value:unknown):void{const tokens=numericTokens(raw);for(const item of tokens)expect(numericAt(value,item.path),JSON.stringify(item.path)).toBe(item.token);expect(numericTokens(nativeJson(value))).toEqual(tokens);}
function parseCsv(raw:string):string[][]{const rows:string[][]=[];let row:string[]=[],cell="",quoted=false;for(let index=0;index<raw.length;++index){const char=raw[index];if(quoted){if(char==='"'&&raw[index+1]==='"'){cell+='"';++index;}else if(char==='"')quoted=false;else cell+=char;}else if(char==='"')quoted=true;else if(char===","){row.push(cell);cell="";}else if(char==="\r"&&raw[index+1]==="\n"){row.push(cell);rows.push(row);row=[];cell="";++index;}else cell+=char;}if(quoted||cell||row.length)throw new Error("Incomplete CSV row.");return rows;}
const cellText=(value:unknown):string=>value===undefined?"":typeof value==="string"?value:typeof value==="object"?nativeJson(value):String(value);
function percentBytes(value:string):Uint8Array<ArrayBuffer>{const bytes:number[]=[];for(let index=0;index<value.length;++index){if(value[index]==="%"){const pair=value.slice(index+1,index+3);if(!/^[0-9a-f]{2}$/i.test(pair))throw new Error("Bad query escape.");bytes.push(parseInt(pair,16));index+=2;}else bytes.push(value[index]==="+"?32:value.charCodeAt(index));}return Uint8Array.from(bytes);}
function queryParts(call:Call):Array<{field:Uint8Array<ArrayBuffer>;bytes:Uint8Array<ArrayBuffer>}>{const query=call.request.path.split("?",2)[1];return !query?[]:query.split("&").map(item=>{const split=item.indexOf("=");return {field:percentBytes(split<0?item:item.slice(0,split)),bytes:percentBytes(split<0?"":item.slice(split+1))};});}
function quotedBytes(value:Uint8Array):Uint8Array<ArrayBuffer>{return Uint8Array.from(Array.from(value).flatMap(byte=>byte===34||byte===92?[92,byte]:[byte]));}
function verifyPhysical(call:Call):void{const body=blob(call.request.body);let cursor=0;for(const part of call.request.parts){expect(part.header_offset).toBe(cursor);expect(part.payload_offset).toBe(cursor+part.header_bytes);const header=body.slice(cursor,part.payload_offset),latin=Array.from(header,byte=>String.fromCharCode(byte)).join("");expect(latin).toMatch(/^--[^\r\n]+\r\nContent-Disposition: form-data;/i);expect(hex(header)).toContain(hex(nativeStringBytes('name="'))+hex(quotedBytes(stringBytes(part.field)))+"22");expect(/(?:;\s*)filename(?:\*?)=/i.test(latin)).toBe(part.filename_attribute_present);if(part.filename_attribute_present){expect(part.filename).not.toBeNull();expect(hex(header)).toContain(hex(nativeStringBytes('filename="'))+hex(quotedBytes(stringBytes(part.filename!)))+"22");}else expect(part.filename).toBeNull();partBytes(call,part);cursor=part.payload_offset+part.bytes;expect(hex(body.slice(cursor,cursor+2))).toBe("0d0a");cursor+=2;}if(call.request.parts.length){const boundary=/boundary=(?:"([^"]+)"|([^;]+))/.exec(call.request.mime??"");expect(boundary).not.toBeNull();expect(scalarText(body.slice(cursor))).toBe("--"+(boundary![1]??boundary![2])+"--\r\n");}const query=queryParts(call);expect(query).toHaveLength(call.request.query_parts.length);call.request.query_parts.forEach((part,index)=>{expect(hex(query[index].field)).toBe(hex(stringBytes(part.field)));expect(query[index].bytes.length).toBe(part.bytes);expect(digest(query[index].bytes)).toBe(part.sha256);expect(hex(stringBytes(part.value))).toBe(hex(query[index].bytes));});expect(call.request.method).toBe("POST");}
// Appended to the pooled-byte loader by the build-only preparation helper.
const METADATA_FIELDS=["extensions","classifiers","trace_globals","event_globals","properties","globals","compatibility_snapshot_valid","compatibility_globals_snapshot","compatibility_trace_globals_snapshot","compatibility_event_globals_snapshot"] as const;
const TABLE_FIELDS=["columns","rows","attributes","metadata","dtypes","index","categorical_dtypes"] as const;
const PRIVATE_DEFAULTS={compatibility_snapshot_valid:false,compatibility_globals_snapshot:[],compatibility_trace_globals_snapshot:[],compatibility_event_globals_snapshot:[]} as const;
function at(value:unknown,pointer:string):unknown{let node=value;for(const part of pointer.split("/").slice(1).map(x=>x.replace(/~1/g,"/").replace(/~0/g,"~"))){if(!node||typeof node!=="object")throw Error("Missing actual snapshot pointer "+pointer);node=(node as Record<string,unknown>)[part];}return node;}
function nativeText(value:NativeString):string{const text=scalarText(stringBytes(value));if(text===null)throw Error("This structural key must be scalar UTF8.");return text;}
function rawConstructor(value:unknown,kind:"table"|"lazy"|"log"):{payload:Record<string,unknown>;hex:boolean;raw:boolean}{
 if(!object(value))throw Error("Complete actual native constructor required.");
 if(kind==="lazy"){expect(Object.keys(value).sort()).toEqual(["row_count","table","valid"]);expect(typeof value.valid).toBe("boolean");if(value.row_count!==null)expect(/^(?:0|[1-9]\d*)$/.test(String(nativeCell(value,"row_count")))).toBe(true);return rawConstructor(value.table,"table");}
 const raw=value.native_byte_strings==="hex-v1";
 if(raw){if(kind==="table")expect(value.native_schema).toBe("event-table-raw-v1");else expect(Object.hasOwn(value,"native_schema")).toBe(false);if(!object(value.value))throw Error("Raw constructor value required.");return {payload:value.value,hex:true,raw:true};}
 return {payload:value,hex:false,raw:false};
}
function compareInputSubset(actual:unknown,input:unknown,actualHex:boolean,inputHex:boolean,ah?:object,ak?:string|number,ih?:object,ik?:string|number):void{
 if(typeof actual==="number"&&typeof input==="number"){const left=String(ah&&ak!==undefined?nativeCell(ah,ak):actual),right=String(ih&&ik!==undefined?nativeCell(ih,ik):input),floating=object(ih)&&ik==="value"&&ih.index===2;if(!floating&&/^-?\d+$/.test(left)&&/^-?\d+$/.test(right))expect(BigInt(left)).toBe(BigInt(right));else expect(Object.is(Number(left),Number(right))).toBe(true);return;}
 const nativeString=(v:unknown):v is NativeString=>typeof v==="string"||object(v)&&v.native_bytes==="hex-v1"&&typeof v.data==="string";
 if(nativeString(actual)&&nativeString(input)){const tagged=object(ih)&&ik==="value"&&(ih.index===2&&["NaN","INF","-INF"].includes(String(input))||ih.index===6&&input==="NA"),bytes=(v:NativeString,isHex:boolean)=>isHex&&typeof v==="string"&&!tagged?fromHex(v):stringBytes(v);expect(hex(bytes(actual,actualHex))).toBe(hex(bytes(input,inputHex)));return;}
 if(Array.isArray(actual)&&Array.isArray(input)){expect(actual.length).toBe(input.length);input.forEach((v,i)=>compareInputSubset(actual[i],v,actualHex,inputHex,actual,i,input,i));return;}
 if(object(actual)&&object(input)){for(const key of Object.keys(input)){expect(Object.hasOwn(actual,key),key).toBe(true);compareInputSubset(actual[key],input[key],actualHex,inputHex,actual,key,input,key);}return;}
 expect(actual).toBe(input);
}
function inspectSnapshot(value:unknown,input:unknown,spec:SnapshotAssertion):void{
 if(spec.absent){expect(at(value,spec.pointer)).toBeNull();return;}
 const target=at(value,spec.pointer),actual=rawConstructor(target,spec.kind),source=rawConstructor(input,spec.kind);
 if(spec.kind==="lazy"){expect(obj(target).valid).toBe(obj(input).valid);expect(obj(target).row_count===null).toBe(obj(input).row_count===null);if(obj(input).row_count!==null)expect(String(nativeCell(obj(target),"row_count"))).toBe(String(nativeCell(obj(input),"row_count")));}
 if(spec.kind!=="log")expect(isOriginalEmbeddingFilterTable(spec.kind==="lazy"?obj(target).table:target)).toBe(true);else expect(isOriginalEmbeddingFilterRawLog(target)).toBe(true);
 if(spec.representation==="raw")expect(actual.raw).toBe(true);else if(spec.representation==="legacy")expect(actual.raw).toBe(false);
 if(spec.kind!=="log"){const keys=Object.keys(actual.payload);if(actual.raw)expect(keys.sort()).toEqual([...TABLE_FIELDS].sort());else{expect(TABLE_FIELDS.slice(0,4).every(key=>Object.hasOwn(actual.payload,key))).toBe(true);expect(keys.every(key=>(TABLE_FIELDS as readonly string[]).includes(key))).toBe(true);}}
 if(!object(actual.payload.metadata)||!object(source.payload.metadata))throw Error("Complete native metadata required.");
 const metadata=actual.payload.metadata,original=source.payload.metadata;
 if(actual.raw||spec.kind==="log")expect(Object.keys(metadata).sort()).toEqual([...METADATA_FIELDS].sort());
 else expect(Object.keys(metadata).sort()).toEqual([...METADATA_FIELDS.slice(0,6)].sort());
 const effective={...PRIVATE_DEFAULTS,...metadata};const wanted={...PRIVATE_DEFAULTS,...original};
 compareInputSubset(effective,wanted,actual.hex,source.hex);
 if(spec.kind!=="log"&&!spec.metadata_only)for(const key of TABLE_FIELDS.filter(x=>x!=="metadata")){const optional=key==="dtypes"||key==="index"||key==="categorical_dtypes";if(Object.hasOwn(source.payload,key)||optional){const empty=key==="index"?null:[];compareInputSubset(Object.hasOwn(actual.payload,key)?actual.payload[key]:empty,Object.hasOwn(source.payload,key)?source.payload[key]:empty,actual.hex,source.hex,actual.payload,key,source.payload,key);}}
}
type Guarded=OriginalEmbeddingFilterWorkbench|OriginalEditDistanceWorkbench|OriginalBoseWorkbench|OriginalDiscoveryWorkbench;
function verifyFreshReturnedTableMetadata(call:Call,value:Guarded):void{
 if(call.returned_tables_metadata_default_blob===undefined)return;
 expect(call.route).toBe("bose");const sourceText=scalarText(blob(call.returned_tables_metadata_default_blob));if(sourceText===null)throw Error("Exact default table oracle JSON required.");const source=rawConstructor(parseNativeJson(sourceText),"table");expect(source.raw).toBe(false);const expected=obj(source.payload.metadata);expect(Object.keys(expected).sort()).toEqual([...METADATA_FIELDS.slice(0,6)].sort());
 const result=obj((value as OriginalBoseWorkbench).native_result),tables=result.returned_tables;if(!Array.isArray(tables))throw Error("Actual fresh native BOSE tables required.");expect(tables.length).toBeGreaterThanOrEqual(call.minimum_returned_tables??1);
 const inspect=(table:unknown):void=>{expect(isOriginalEmbeddingFilterTable(table)).toBe(true);const actual=rawConstructor(table,"table");expect(actual.raw).toBe(false);const metadata=obj(actual.payload.metadata);expect(Object.keys(metadata).sort()).toEqual([...METADATA_FIELDS.slice(0,6)].sort());compareInputSubset({...PRIVATE_DEFAULTS,...metadata},{...PRIVATE_DEFAULTS,...expected},actual.hex,source.hex);};
 tables.forEach(inspect);
 for(const artifact of call.artifacts.filter(x=>/^returned_table_\d+\.json$/.test(x.name))){const bytes=selectedCarrier(call,value,artifact.name).bytes,text=scalarText(bytes);if(text===null)throw Error("Native returned table artifact JSON required.");inspect(parseNativeJson(text));}
}

function guarded(call:Call,value:unknown):Guarded{
 if(call.route==="embedding"){if(!isOriginalEmbeddingFilterWorkbench(value))throw Error("Existing complete embedding UI guard failed.");}
 else if(call.route==="edit_distance"){if(!isOriginalEditDistanceWorkbench(value))throw Error("Existing complete edit-distance UI guard failed.");}
 else if(call.route==="bose"){if(!isOriginalBoseWorkbench(value))throw Error("Existing complete BOSE UI guard failed.");}
 else if(!isOriginalDiscoveryWorkbench(value))throw Error("Existing complete discovery UI guard failed.");
 return value as Guarded;
}
function inspectionTables(call:Call,value:Guarded):DiagnosticTable[]{
 if(call.route==="embedding")return originalEmbeddingFilterTables(value as OriginalEmbeddingFilterWorkbench);
 if(call.route==="edit_distance")return originalEditDistanceTables(value as OriginalEditDistanceWorkbench);
 if(call.route==="bose")return originalBoseTables(value as OriginalBoseWorkbench);
 return originalDiscoveryTables(value as OriginalDiscoveryWorkbench);
}
function verifyTablesAndReports(call:Call,value:Guarded):void{
 const tables=inspectionTables(call,value),selected=parseNativeJson(originalEditDistanceSelectedRecordsJson(tables));if(!object(selected))throw Error("Complete selected records required.");
 expect(Object.keys(selected)).toEqual(tables.map(x=>x.id));expect(new Set(tables.map(x=>x.id)).size).toBe(tables.length);
 for(const table of tables){expect(nativeJson(selected[table.id])).toBe(nativeJson(table.records??[]));expect(parseCsv(diagnosticCsv(table))).toEqual([table.columns,...table.rows.map(row=>row.map(cellText))]);}
 const csv=parseCsv(originalEditDistanceAllTablesCsv(tables));expect(csv[0]).toEqual(["table_id","table_title","row_index","column_index","column","value"]);
 const findMetadata=(node:unknown,pointer:string):void=>{if(!node||typeof node!=="object"||Array.isArray(node))return;if(object(node)&&object(node.metadata)&&METADATA_FIELDS.slice(0,6).every(k=>Object.hasOwn(node.metadata as object,k))){const id=(pointer+"/metadata"),table=tables.find(x=>x.id===id);expect(table,id).toBeDefined();expect(table!.rows.map(row=>row[0])).toEqual(Object.keys(node.metadata));for(const field of Object.keys(node.metadata))expect(table!.records?.some(row=>object(row)&&row.field===field)).toBe(true);}Object.entries(node).forEach(([key,x])=>findMetadata(x,pointer+"/"+key.replace(/~/g,"~0").replace(/\//g,"~1")));};findMetadata(value,"");
 // Report helpers consume exactly these inspected response/request fields.
 // Request identity comes from the frozen submitted case, never an error echo.
 const request={path:call.request.path,operation:call.operation,originals:[]},http_status=call.response.status;
 let raw:string;
 if(call.route==="embedding")raw=originalEmbeddingFilterReport({value:value as OriginalEmbeddingFilterWorkbench,fields:[],http_status,input:{uploads:{},hashes:{}}} as unknown as OriginalEmbeddingFilterRun);
 else if(call.route==="edit_distance")raw=originalEditDistanceReport({value:value as OriginalEditDistanceWorkbench,request,http_status} as unknown as OriginalEditDistanceRun);
 else if(call.route==="bose")raw=originalBoseReport({value:value as OriginalBoseWorkbench,request,http_status} as unknown as OriginalBoseRun);
 else raw=originalDiscoveryReport({value:value as OriginalDiscoveryWorkbench,request,http_status} as unknown as OriginalDiscoveryRun);
 const report=parseNativeJson(raw);if(!object(report))throw Error("Complete actual report required.");expect(nativeJson(report[call.route==="embedding"?"native_result":"native_response"])).toBe(nativeJson(value));
}
function selectedCarrier(call:Call,value:Guarded,name:string):{carrier:OriginalEditDistanceBytes|OriginalEmbeddingFilterBytes;bytes:Uint8Array;file:File;helpers:string[]}{
 if(call.route==="embedding"){const v=value as OriginalEmbeddingFilterWorkbench;if(!(ORIGINAL_EMBEDDING_FILTER_ARTIFACTS as readonly string[]).includes(name))throw Error("No such existing embedding export.");const key=name as OriginalEmbeddingFilterArtifact,carrier=v.artifact_originals[key];if(!carrier)throw Error("Actual embedding artifact absent.");return {carrier,bytes:originalEmbeddingFilterOriginalBytes(carrier),file:originalEmbeddingFilterArtifactFile({value:v} as OriginalEmbeddingFilterRun,key),helpers:["parseNativeJson","isOriginalEmbeddingFilterWorkbench","originalEmbeddingFilterArtifactFile"]};}
 const v=value as OriginalEditDistanceWorkbench|OriginalBoseWorkbench|OriginalDiscoveryWorkbench,index=v.artifacts.findIndex(x=>nativeText(x.name)===name);if(index<0)throw Error("Actual selected artifact absent.");const carrier=v.artifacts[index],file=call.route==="edit_distance"?originalEditDistanceArtifactFile({value:v} as OriginalEditDistanceRun,index):call.route==="bose"?originalBoseCarrierFile(carrier):originalDiscoveryCarrierFile(carrier),guard=call.route==="edit_distance"?"isOriginalEditDistanceWorkbench":call.route==="bose"?"isOriginalBoseWorkbench":"isOriginalDiscoveryWorkbench",helper=call.route==="edit_distance"?"originalEditDistanceArtifactFile":call.route==="bose"?"originalBoseCarrierFile":"originalDiscoveryCarrierFile";return {carrier,bytes:originalEditDistanceOriginalBytes(carrier),file,helpers:["parseNativeJson",guard,helper]};
}
async function verifyArtifacts(call:Call,value:Guarded):Promise<void>{
 for(const saved of call.artifacts){const actual=selectedCarrier(call,value,saved.name),bytes=new Uint8Array(await actual.file.arrayBuffer());expect(bytes.length).toBe(saved.bytes);expect(digest(bytes)).toBe(saved.sha256);expect(hex(bytes)).toBe(hex(actual.bytes));expect(actual.carrier.sha256).toBe(saved.sha256);expect(String(nativeCell(actual.carrier,"bytes"))).toBe(String(bytes.length));if(saved.file_name!==undefined)expect(actual.file.name).toBe(saved.file_name);if(saved.file_mime!==undefined)expect(actual.file.type).toBe(saved.file_mime);
  if(saved.name==="source_log_json"||saved.name==="source_constructor.json"||/^returned_table_\d+\.json$/.test(saved.name)){const raw=scalarText(bytes);if(raw===null)throw Error("Canonical constructor artifact must be UTF8 JSON.");const decoded=parseNativeJson(raw);verifyNumeric(raw,decoded);if(!object(decoded))throw Error("Constructor artifact object required.");const kind=saved.name==="source_log_json"?"log":Object.hasOwn(decoded,"table")?"lazy":isOriginalEmbeddingFilterTable(decoded)?"table":"log",constructor=rawConstructor(decoded,kind);expect(object(constructor.payload.metadata)).toBe(true);if(constructor.raw||kind==="log")expect(Object.keys(obj(constructor.payload.metadata)).sort()).toEqual([...METADATA_FIELDS].sort());}
 }
}
function verifyExpectedAndOriginals(call:Call,value:Guarded):void{
 const expected=call.expected;expect(call.response.status).toBe(expected.status);expect(value.executed).toBe(expected.executed);const returned=call.route==="embedding"?(value as OriginalEmbeddingFilterWorkbench).native_calls.some(x=>x.stage==="primary"&&x.returned):(value as OriginalEditDistanceWorkbench|OriginalBoseWorkbench|OriginalDiscoveryWorkbench).returned;expect(returned).toBe(expected.returned);expect(value.error===null?null:nativeText(obj(value.error).phase as NativeString)).toBe(expected.phase);if(expected.error_contains){expect(value.error).not.toBeNull();expect(hex(stringBytes(value.error!.message))).toContain(hex(nativeStringBytes(expected.error_contains)));}
 if(call.route==="embedding"){const v=value as OriginalEmbeddingFilterWorkbench,part=call.request.parts.find(x=>nativeText(x.field)==="source_file");if(v.source&&part){const bytes=originalEmbeddingFilterOriginalBytes(v.source.original);expect(hex(bytes)).toBe(hex(partBytes(call,part)));expect(v.source.original.sha256).toBe(part.sha256);}for(const control of v.raw_controls){const part=call.request.parts.find(x=>nativeText(x.field)===nativeText(control.name));expect(part).toBeDefined();expect(hex(originalEmbeddingFilterOriginalBytes(control.original))).toBe(hex(partBytes(call,part!)));}}
 else{const v=value as OriginalEditDistanceWorkbench|OriginalBoseWorkbench|OriginalDiscoveryWorkbench;expect(v.originals).toHaveLength(call.request.parts.length);v.originals.forEach((original,i)=>{const part=call.request.parts[i];expect(nativeText(original.field)).toBe(nativeText(part.field));expect(original.filename===null?null:nativeText(original.filename)).toBe(part.filename===null?null:nativeText(part.filename));expect(hex(originalEditDistanceOriginalBytes(original.original))).toBe(hex(partBytes(call,part)));expect(original.original.sha256).toBe(part.sha256);});}
}
async function verifyHandoff(call:Call):Promise<void>{
 const a=call.adaptive_capture;if(!a)return;const source=entries.find(x=>x.id===a.source_case);expect(source).toBeDefined();expect(source!.physical_call_index).toBeLessThan(call.physical_call_index);const value=guarded(source!,parseNativeJson(responseText(source!))),selected=selectedCarrier(source!,value,a.artifact),file=selected.file,bytes=new Uint8Array(await file.arrayBuffer()),proof=call.actual_ui_handoff!;
 expect(proof.produced_by_actual_ui_helpers).toBe(true);expect(proof.response_sha256).toBe(source!.response.body);expect(proof.actual_ui_helpers).toEqual(selected.helpers);expect(proof.artifact).toBe(a.artifact);expect(proof.route).toBe(source!.route);expect(proof.role).toBe(a.target_role);expect(proof.file_name).toBe(file.name);expect(proof.file_mime).toBe(file.type);expect(proof.file_bytes.sha256).toBe(digest(bytes));expect(proof.file_bytes.bytes).toBe(bytes.length);expect(proof.source_artifact.sha256).toBe(selected.carrier.sha256);
 const part=call.request.parts.find(x=>nativeText(x.field)===a.target_role);expect(part).toBeDefined();expect(hex(partBytes(call,part!))).toBe(hex(bytes));expect(part!.filename_attribute_present).toBe(true);expect(nativeText(part!.filename!)).toBe(file.name);expect(part!.content_type).toBe(file.type);
}
describe("portable repaired shared table metadata across existing workbenches",()=>{
 for(const call of entries)it(call.physical_call_index+": "+call.id,async()=>{
  expect(call.passed).toBe(true);expect(call.checks.length).toBeGreaterThan(0);expect(call.checks.every(x=>x.passed)).toBe(true);verifyPhysical(call);const raw=responseText(call),decoded=await readNativeResponse(new Response(raw,{status:call.response.status}),{path:call.request.path});if(!object(decoded))throw Error("Saved route response root must be object.");expect(nativeResponseJson(decoded)).toBe(raw);verifyNumeric(raw,decoded);const value=guarded(call,decoded);
  verifyExpectedAndOriginals(call,value);for(const assertion of call.snapshot_assertions){const input=parseNativeJson(scalarText(blob(assertion.input_blob))!);inspectSnapshot(value,input,assertion);}
  verifyFreshReturnedTableMetadata(call,value);verifyTablesAndReports(call,value);await verifyArtifacts(call,value);await verifyHandoff(call);
 });
 it("retains actual chronology, all four routes and bounded authentic Files",()=>{
  expect(manifest.counts.entries).toBe(entries.length);expect(manifest.counts.expected_fixture_tests).toBe(entries.length+1);expect(bundle.chronology.map(x=>x.physical_call_index)).toEqual(Array.from({length:manifest.counts.actual_http_calls},(_,i)=>i+1));expect(new Set(entries.map(x=>x.route))).toEqual(new Set(["embedding","edit_distance","bose","discovery"]));
  expect(Object.keys(bundle.blobs)).toHaveLength(manifest.counts.distinct_body_blobs);Object.keys(bundle.blobs).forEach(blob);
  const adaptive=entries.filter(x=>x.adaptive_capture);expect(adaptive.length).toBe(manifest.counts.actual_UI_argument_File_handoffs);expect(new Set(adaptive.map(x=>entries.find(source=>source.id===x.adaptive_capture!.source_case)!.route))).toEqual(new Set(["embedding","edit_distance","bose","discovery"]));
  const raw=entries.flatMap(x=>x.snapshot_assertions).filter(x=>!x.absent&&x.representation==="raw"),legacy=entries.flatMap(x=>x.snapshot_assertions).filter(x=>!x.absent&&x.representation==="legacy");expect(raw.length).toBeGreaterThan(0);expect(legacy.length).toBeGreaterThan(0);
  expect(manifest.metadata_fields).toEqual(METADATA_FIELDS);expect(bundle.history.prepatch_validation).toBeDefined();expect(bundle.history.parent_checkpoint).toBeDefined();expect(manifest.counts.parent_corpora_replayed).toBe(0);
 });
});
