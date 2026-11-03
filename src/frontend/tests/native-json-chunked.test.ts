import {describe,expect,it} from "vitest";
import {nativeJson,nativeJsonBlob,nativeJsonPreview,parseNativeJson,writeNativeJson} from "../src/native-json";
import {nativeStringBytes} from "../src/native-multipart";

// A deterministic generator of nested values: literals kept by the parser
// (big integers, 1.0, -0), undefined members, holes, surrogate pairs and lone
// surrogates, empty containers.
function random(seed:number){let state=seed>>>0||1;return ()=>{state^=state<<13;state>>>=0;state^=state>>>17;state^=state<<5;state>>>=0;return state/0x100000000;};}
function sample(next:()=>number,depth:number):unknown{
  const pick=Math.floor(next()*(depth>4?6:9));
  switch(pick){
    case 0:return null;
    case 1:return next()<0.5;
    case 2:return Math.floor(next()*2000)-1000;
    case 3:return ["a","é","😀","\ud800","x\"y\\z","\n",""][Math.floor(next()*7)];
    case 4:return next()*10;
    case 5:return undefined;
    case 6:case 7:{const a:unknown[]=[];const n=Math.floor(next()*5);for(let i=0;i<n;++i)a.push(sample(next,depth+1));if(n&&next()<0.2)delete a[0];return a;}
    default:{const o:Record<string,unknown>={};const n=Math.floor(next()*5);for(let i=0;i<n;++i)o[["k","2","😀",String(i),"__x"][Math.floor(next()*5)]+i]=sample(next,depth+1);return o;}
  }
}
const pieces=(value:unknown,indent:number,size:number)=>{const out:string[]=[];expect(writeNativeJson(value,indent,piece=>{out.push(piece);},size)).toBe(true);return out;};

describe("chunked native JSON",()=>{
  it("writes exactly nativeJson for random values, indents and piece sizes",()=>{
    for(let seed=1;seed<=400;++seed){
      const next=random(seed),value=sample(next,0);
      for(const indent of [0,2])for(const size of [1,2,7,1<<20]){
        const parts=pieces(value,indent,size);
        expect(parts.join("")).toBe(nativeJson(value,indent));
        for(const part of parts.slice(0,-1)){const last=part.charCodeAt(part.length-1);expect(last>=0xd800&&last<=0xdbff&&part!=="\ud800").toBe(false);}
      }
    }
  });
  it("keeps the parser's numeric literals and big integers",()=>{
    const raw='{"big":18446744073709551615,"f":1.0,"z":-0,"e":1E2,"list":[12345678901234567890,2.50],"nested":{"n":0.10}}';
    const value=parseNativeJson(raw);
    for(const indent of [0,2])expect(pieces(value,indent,3).join("")).toBe(nativeJson(value,indent));
    expect(pieces(value,0,1<<20).join("")).toBe('{"big":18446744073709551615,"f":1.0,"z":-0,"e":1E2,"list":[12345678901234567890,2.50],"nested":{"n":0.10}}');
  });
  it("stops when the sink refuses",()=>{
    const seen:string[]=[];
    expect(writeNativeJson({a:"x".repeat(100),b:[1,2,3]},0,piece=>{seen.push(piece);return seen.length<2;},10)).toBe(false);
    expect(seen.length).toBe(2);
  });
  it("previews a bounded prefix and says whether the text was longer",()=>{
    const value={rows:Array.from({length:2000},(_,i)=>({i,label:"😀".repeat(i%3)}))},full=nativeJson(value,2);
    const whole=nativeJsonPreview(value,2,full.length);
    expect(whole).toEqual({text:full,truncated:false});
    for(const limit of [0,1,999,1000,4096,full.length-1]){
      const preview=nativeJsonPreview(value,2,limit);
      expect(preview.truncated).toBe(true);
      expect(full.startsWith(preview.text)).toBe(true);
      expect(preview.text.length).toBeLessThanOrEqual(limit);
      expect(preview.text.length).toBeGreaterThanOrEqual(limit-1);
      const last=preview.text.charCodeAt(preview.text.length-1);expect(last>=0xd800&&last<=0xdbff).toBe(false);
    }
  });
  it("builds the WTF-8 bytes of nativeJson as a Blob",async()=>{
    for(let seed=1;seed<=60;++seed){
      const value=sample(random(seed*7),0),expected=nativeStringBytes(nativeJson(value,2));
      const blob=nativeJsonBlob(value,2,nativeStringBytes);
      expect(blob.type).toBe("application/json");
      expect(new Uint8Array(await blob.arrayBuffer())).toEqual(expected);
    }
  });
});
