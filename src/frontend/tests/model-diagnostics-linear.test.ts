import {describe,expect,it} from 'vitest';
import {nativeObjectTables,type DiagnosticTable} from '../src/model-diagnostics';
import {nativeCell,nativeJson,parseNativeJson} from '../src/native-json';
// Node modules typed locally: the frontend build carries no Node declarations.
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {readdirSync,readFileSync,statSync} = await nodeModule("node:fs") as {readdirSync(path:string):string[];readFileSync(path:string,encoding:"utf8"):string;statSync(path:string):{size:number}};
const {join} = await nodeModule("node:path") as {join(...parts:string[]):string};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url:URL|string):string};

// The previous algorithm: every field record is a text round trip of its value.
function previousNativeObjectTables(result:object):DiagnosticTable[] {
  const record=(value:unknown):value is Record<string,unknown>=>value!==null&&typeof value==='object'&&!Array.isArray(value);
  const pointer=(key:string)=>key.replace(/~/g,'~0').replace(/\//g,'~1');
  const tables:DiagnosticTable[]=[];
  const visit=(value:unknown,path:string)=>{
    if(record(value)){
      const keys=Object.keys(value);
      tables.push({id:path||'summary',title:path||'Native summary',columns:['field','type','value'],
        rows:keys.map(key=>[key,value[key]===null?'null':Array.isArray(value[key])?'array':typeof value[key],nativeCell(value,key)]),
        records:keys.map(key=>{const cell=nativeCell(value,key);const encoded=typeof value[key]==='number'&&typeof cell==='string'?cell:nativeJson(value[key]);return parseNativeJson(`{"field":${JSON.stringify(key)},"value":${encoded}}`);})});
      for(const key of keys)if(value[key]!==null&&typeof value[key]==='object')visit(value[key],`${path}/${pointer(key)}`);
    }else if(Array.isArray(value)){
      const columns=[...new Set(value.flatMap(row=>record(row)?Object.keys(row):['value']))];
      tables.push({id:path,title:path,columns:['row_index',...columns],records:value,
        rows:value.map((row,i)=>[i,...columns.map(key=>record(row)?nativeCell(row,key):key==='value'?nativeCell(value,i):undefined)])});
    }
  };
  visit(result,'');
  return tables;
}

// Token-preserving equality: the same structure and values, and the same
// native text for every record and for every record's value field.
function expectSameTables(actual:DiagnosticTable[],expected:DiagnosticTable[]) {
  expect(actual.length).toBe(expected.length);
  for(let t=0;t<expected.length;++t){
    const a=actual[t],e=expected[t];
    expect(a.id).toBe(e.id);expect(a.title).toBe(e.title);expect(a.columns).toEqual(e.columns);
    expect(a.rows).toEqual(e.rows);
    expect(a.records).toEqual(e.records);
    expect(nativeJson(a.records)).toBe(nativeJson(e.records));
    expect(nativeJson(a.records,2)).toBe(nativeJson(e.records,2));
    for(let r=0;r<(e.records?.length??0);++r){
      const ar=a.records![r],er=e.records![r];
      if(er!==null&&typeof er==='object'&&!Array.isArray(er)&&'field'in er)expect(nativeCell(ar as object,'value')).toEqual(nativeCell(er,'value'));
    }
  }
}

const fixtures=join(fileURLToPath(new URL('.',import.meta.url)),'fixtures');
const samples=readdirSync(fixtures).filter((name:string)=>name.endsWith('.json')&&statSync(join(fixtures,name)).size<=200_000).sort();

describe('linear native object tables',()=>{
  it('found fixture results to compare',()=>expect(samples.length).toBeGreaterThan(50));
  it.each(samples)('equals the text round-trip algorithm on fixture %s',(name:string)=>{
    let value:unknown;
    try{value=parseNativeJson(readFileSync(join(fixtures,name),'utf8'));}catch{return;}
    if(value===null||typeof value!=='object')return;
    expectSameTables(nativeObjectTables(value),previousNativeObjectTables(value));
  },60_000);
  it('keeps native tokens and normalizes non-JSON values exactly as the round trip does',()=>{
    const value=parseNativeJson('{"big":18446744073709551615,"real":1.0,"exp":1e400,"neg":-0,"nested":{"id":12345678901234567890,"list":[1.50,2,{"x":3.0}],"empty":{},"none":[]},"text":"a\\u0000b","flag":false,"nil":null}') as Record<string,unknown>;
    const nested=value.nested as Record<string,unknown>;
    nested.nan=Number.NaN;nested.inf=-Infinity;nested.missing=undefined;nested.zero=-0;
    (nested.list as unknown[]).push(undefined,()=>1,new Date(0));
    value.map=new Map([[1,2]]);value.when=new Date(0);
    Object.defineProperty(nested,'__proto__',{value:{p:1},enumerable:true,configurable:true,writable:true});
    const shared=parseNativeJson('{"a":[1.0,2.50]}') as Record<string,unknown>;value.twice=shared;value.again=shared;
    expectSameTables(nativeObjectTables(value),previousNativeObjectTables(value));
  });
  it('shares canonical subtrees instead of copying them',()=>{
    const value=parseNativeJson('{"tree":{"node":{"leaf":[1.0,{"deep":2.50}]}}}') as {tree:object};
    const tables=nativeObjectTables(value);
    expect((tables[0].records![0] as {value:unknown}).value).toBe(value.tree);
  });
});
