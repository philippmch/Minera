// Parsed values remain usable for validation/math. Keep numeric tokens alongside
// their original holders so inspection and CSV do not round native uint64 values.
const literals=new WeakMap<object,Map<string,string>>();
export function parseNativeJson(raw:string):unknown {
  const value:unknown=JSON.parse(raw);
  // A linear scanner: JSON.parse above already validated the text, so tokens only
  // need to be delimited. Regular expressions over string tokens backtrack per
  // character and overflow the engine's stack on multi-megabyte strings.
  // A mine response has millions of tokens, so the scan allocates only for what it
  // keeps: a key is read only when its member is a container or a number JavaScript
  // would print differently, and a plain integer of at most 15 digits (which always
  // prints as written) is never cut out of the text at all.
  let p=0;
  const space=():number=>{for(;;){const c=raw.charCodeAt(p);if(c===32||c===9||c===10||c===13)++p;else return c;}};
  // The end of the string token opening at `start`.
  const stringEnd=(start:number):number=>{let i=start+1;for(;;){const quote=raw.indexOf('"',i);if(quote<0)throw new Error("Invalid native JSON token.");let slashes=0;for(let j=quote-1;j>start&&raw.charCodeAt(j)===92;--j)++slashes;i=quote+1;if(slashes%2===0)return i;}};
  const keyText=(start:number,end:number):string=>{const body=raw.slice(start+1,end-1);return body.indexOf("\\")<0?body:JSON.parse(raw.slice(start,end)) as string;};
  // Moves past the number token at p; true when the token is an integer of at most
  // 15 digits other than -0, which String(number) reproduces exactly.
  const number=():boolean=>{
    const start=p;let c=raw.charCodeAt(p);if(c===45)c=raw.charCodeAt(++p);
    if(c<48||c>57)throw new Error("Invalid native JSON token.");
    const digits=p;while(c>=48&&c<=57)c=raw.charCodeAt(++p);
    let plain=p-digits<=15&&!(raw.charCodeAt(start)===45&&p-digits===1&&raw.charCodeAt(digits)===48);
    if(c===46){plain=false;c=raw.charCodeAt(++p);while(c>=48&&c<=57)c=raw.charCodeAt(++p);}
    if(c===101||c===69){plain=false;c=raw.charCodeAt(++p);if(c===43||c===45)c=raw.charCodeAt(++p);while(c>=48&&c<=57)c=raw.charCodeAt(++p);}
    return plain;
  };
  // Records (or forgets) the number token raw[start, p) of holder[key], as the
  // holder's kept literal when JavaScript would print the parsed number differently.
  const keep=(holder:object,kept:Map<string,string>|undefined,key:string,member:unknown,start:number):Map<string,string>|undefined=>{
    const token=raw.slice(start,p);
    if(typeof member==="number"&&String(member)!==token){if(!kept){kept=new Map<string,string>();literals.set(holder,kept);}kept.set(key,token);}
    else kept?.delete(key);
    return kept;
  };
  // Moves past a string, true, false or null token at p (its first character is c).
  const skip=(c:number)=>{if(c===34)p=stringEnd(p);else if(c===116||c===110)p+=4;else if(c===102)p+=5;else throw new Error("Invalid native JSON token.");};
  const walk=(current:unknown):void=>{
    const c=space();
    if(c===123){
      ++p;
      const holder=current&&typeof current==="object"&&!Array.isArray(current)?current as Record<string,unknown>:undefined;
      let kept=holder?literals.get(holder):undefined;
      if(space()===125){++p;return;}
      for(;;){
        space();const keyStart=p;p=stringEnd(p);const keyEnd=p;
        space();++p;
        const v=space();
        if(v===123||v===91)walk(holder?.[keyText(keyStart,keyEnd)]);
        else if(v===45||v>=48&&v<=57){
          const start=p;
          if(number()){if(kept)kept.delete(keyText(keyStart,keyEnd));}
          else if(holder){const key=keyText(keyStart,keyEnd);kept=keep(holder,kept,key,holder[key],start);}
        }
        else {skip(v);if(kept)kept.delete(keyText(keyStart,keyEnd));}
        const separator=space();++p;if(separator!==44)return;
      }
    }
    if(c===91){
      ++p;
      const holder=Array.isArray(current)?current:undefined;
      let kept=holder?literals.get(holder):undefined;
      if(space()===93){++p;return;}
      for(let i=0;;++i){
        const v=space();
        if(v===123||v===91)walk(holder?.[i]);
        else if(v===45||v>=48&&v<=57){
          const start=p;
          if(number()){if(kept)kept.delete(String(i));}
          else if(holder)kept=keep(holder,kept,String(i),holder[i],start);
        }
        else {skip(v);if(kept)kept.delete(String(i));}
        const separator=space();++p;if(separator!==44)return;
      }
    }
    if(c===45||c>=48&&c<=57)number();else skip(c);
  };
  walk(value);return value;
}
export function nativeCell(holder:object,key:string|number):unknown {return literals.get(holder)?.get(String(key))??(holder as Record<string,unknown>)[String(key)];}
export function nativeJson(value:unknown,indent=0):string {
  const encode=(current:unknown,level:number):string=>{
    if(!current||typeof current!=="object")return JSON.stringify(current)??"null";
    const array=Array.isArray(current),keys=array?current.map((_,i)=>String(i)):Object.keys(current).filter(key=>(current as Record<string,unknown>)[key]!==undefined);const parts=keys.map(key=>{const literal=literals.get(current)?.get(key);const encoded=literal??encode((current as Record<string,unknown>)[key],level+1);return (array?"":JSON.stringify(key)+":"+(indent?" ":""))+encoded;});const open=array?"[":"{",close=array?"]":"}";return !parts.length?open+close:indent?open+"\n"+parts.map(part=>" ".repeat((level+1)*indent)+part).join(",\n")+"\n"+" ".repeat(level*indent)+close:open+parts.join(",")+close;
  };return encode(value,0);
}
// Whether parseNativeJson(nativeJson(value)) would rebuild `value` exactly:
// plain objects and dense arrays of finite numbers (not -0), strings,
// booleans and null, every member defined, and each kept numeric token one
// the parser would keep again. Answers are memoized per node, so one pass
// over a result decides every subtree in linear time.
function canonicalNative(value:unknown,memo:WeakMap<object,boolean>):boolean {
  if(value===null)return true;
  if(typeof value==="string"||typeof value==="boolean")return true;
  if(typeof value==="number")return Number.isFinite(value)&&!Object.is(value,-0);
  if(typeof value!=="object")return false;
  const known=memo.get(value);if(known!==undefined)return known;
  memo.set(value,false);
  const kept=literals.get(value),keys=Object.keys(value),array=Array.isArray(value);
  const token=(member:unknown,literal:string)=>typeof member==="number"&&/^-?\d/.test(literal)&&String(member)!==literal&&Object.is(JSON.parse(literal),member);
  let ok=Object.getPrototypeOf(value)===(array?Array.prototype:Object.prototype)&&Object.getOwnPropertySymbols(value).length===0&&Object.getOwnPropertyNames(value).length===keys.length+(array?1:0)&&(!array||keys.length===value.length);
  if(ok&&kept)for(const key of kept.keys())if(!Object.prototype.hasOwnProperty.call(value,key)){ok=false;break;}
  if(ok)for(let i=0;i<keys.length;++i){
    const key=keys[i];
    if(array?key!==String(i):key==="__proto__"){ok=false;break;}
    const member=(value as Record<string,unknown>)[key],literal=kept?.get(key);
    if(literal!==undefined?!token(member,literal):member===undefined||!canonicalNative(member,memo)){ok=false;break;}
  }
  memo.set(value,ok);return ok;
}
/** parseNativeJson(`{"field":<field>,"value":<token>}`) where the token is the
 * holder's kept numeric token for a number, else nativeJson(holder[key]). A
 * canonical value is shared rather than copied through text; `memo` carries
 * canonicity answers between calls over one result. */
export function nativeFieldRecord(field:string,holder:object,key:string,memo:WeakMap<object,boolean>):unknown {
  const member=(holder as Record<string,unknown>)[key],literal=literals.get(holder)?.get(key);
  if(typeof member==="number"&&literal!==undefined)return parseNativeJson(`{"field":${JSON.stringify(field)},"value":${literal}}`);
  if(member!==null&&typeof member==="object"&&!canonicalNative(member,memo))return parseNativeJson(`{"field":${JSON.stringify(field)},"value":${nativeJson(member)}}`);
  if(member===null||typeof member!=="object")return {field,value:JSON.parse(JSON.stringify(member)??"null") as unknown};
  return {field,value:member};
}

const STOP_NATIVE_JSON=Symbol("stop native JSON");
/** Writes exactly nativeJson(value, indent) to `sink` as pieces of about
 * `pieceSize` characters, in order, without building the whole text (which
 * can exceed the engine's string limit). A piece never ends inside a
 * surrogate pair. `sink` returning false stops the write; the result says
 * whether the whole text was written. */
export function writeNativeJson(value:unknown,indent:number,sink:(piece:string)=>boolean|void,pieceSize=1<<20):boolean {
  let buffer="";
  const flush=(force:boolean)=>{
    if(!buffer||(!force&&buffer.length<pieceSize))return;
    let cut=buffer.length;const last=buffer.charCodeAt(cut-1);if(!force&&last>=0xd800&&last<=0xdbff)cut-=1;
    if(!cut)return;
    const piece=cut===buffer.length?buffer:buffer.slice(0,cut);buffer=cut===buffer.length?"":buffer.slice(cut);
    if(sink(piece)===false)throw STOP_NATIVE_JSON;
  };
  const push=(text:string)=>{buffer+=text;flush(false);};
  const encode=(current:unknown,level:number):void=>{
    if(!current||typeof current!=="object"){push(JSON.stringify(current)??"null");return;}
    const array=Array.isArray(current),record=current as Record<string,unknown>,kept=literals.get(current);
    const keys=array?undefined:Object.keys(current).filter(key=>record[key]!==undefined);
    const length=array?(current as unknown[]).length:keys!.length,open=array?"[":"{",close=array?"]":"}";
    if(!length){push(open+close);return;}
    push(indent?open+"\n":open);
    for(let i=0;i<length;++i){
      if(i)push(indent?",\n":",");
      // nativeJson maps over the array, so a hole leaves an empty part.
      if(array&&!(i in current))continue;
      const key=array?String(i):keys![i];
      if(indent)push(" ".repeat((level+1)*indent));
      if(!array)push(JSON.stringify(key)+":"+(indent?" ":""));
      const literal=kept?.get(key);
      if(literal!==undefined)push(literal);else encode(record[key],level+1);
    }
    push(indent?"\n"+" ".repeat(level*indent)+close:close);
  };
  try{encode(value,0);flush(true);return true;}
  catch(error){if(error===STOP_NATIVE_JSON)return false;throw error;}
}
/** The first `limit` characters of nativeJson(value, indent) (never ending
 * inside a surrogate pair) and whether the text was longer. */
export function nativeJsonPreview(value:unknown,indent:number,limit:number):{text:string;truncated:boolean}{
  const parts:string[]=[];let size=0,truncated=false;
  writeNativeJson(value,indent,piece=>{
    const room=limit-size;
    if(piece.length<=room){parts.push(piece);size+=piece.length;return true;}
    let end=room;const unit=piece.charCodeAt(end-1);if(end>0&&unit>=0xd800&&unit<=0xdbff)end-=1;
    parts.push(piece.slice(0,end));truncated=true;return false;
  },Math.max(1,Math.min(limit,1<<16)));
  return {text:parts.join(""),truncated};
}
/** nativeJson(value, indent) as WTF-8 bytes in a Blob (lone surrogates keep
 * their own three bytes, like nativeStringBytes), built piece by piece so a
 * text beyond the engine's string limit still downloads in full. */
export function nativeJsonBlob(value:unknown,indent:number,encode:(piece:string)=>Uint8Array<ArrayBuffer>,type="application/json"):Blob{
  const blobs:Blob[]=[];let pending:Uint8Array<ArrayBuffer>[]=[];
  writeNativeJson(value,indent,piece=>{pending.push(encode(piece).slice());if(pending.length>=64){blobs.push(new Blob(pending));pending=[];}});
  return new Blob([...blobs,...pending],{type});
}
