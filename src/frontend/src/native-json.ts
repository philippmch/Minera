// Parsed values remain usable for validation/math. Keep numeric tokens alongside
// their original holders so inspection and CSV do not round native uint64 values.
const literals=new WeakMap<object,Map<string,string>>();
export function parseNativeJson(raw:string):unknown {
  const value:unknown=JSON.parse(raw);const end=raw.trimEnd().length;
  // A linear scanner: JSON.parse above already validated the text, so tokens only
  // need to be delimited. Regular expressions over string tokens backtrack per
  // character and overflow the engine's stack on multi-megabyte strings.
  const number=/-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/y;let position=0;
  const next=():string=>{
    while(position<end){const c=raw.charCodeAt(position);if(c===32||c===9||c===10||c===13)++position;else break;}
    const start=position,c=raw[position];
    if(c==='"'){let i=position+1;for(;;){const quote=raw.indexOf('"',i);if(quote<0)throw new Error("Invalid native JSON token.");let slashes=0;for(let j=quote-1;j>start&&raw.charCodeAt(j)===92;--j)++slashes;i=quote+1;if(slashes%2===0)break;}position=i;return raw.slice(start,position);}
    if(c==="{"||c==="}"||c==="["||c==="]"||c===","||c===":"){++position;return c;}
    for(const word of ["true","false","null"])if(raw.startsWith(word,position)){position+=word.length;return word;}
    number.lastIndex=position;const match=number.exec(raw);if(!match)throw new Error("Invalid native JSON token.");position=number.lastIndex;return match[0];
  };let token=next();
  const advance=()=>{if(position<end)token=next();else token="";};
  const walk=(current:unknown,parent?:object,key?:string):void=>{
    const start=token;
    if(start==="{"){advance();while(token!=="}"){const name=JSON.parse(token) as string;advance();advance();const holder=current&&typeof current==="object"&&!Array.isArray(current)?current as Record<string,unknown>:undefined;walk(holder?.[name],holder,name);if(token!==",")break;advance();}advance();}
    else if(start==="["){advance();let i=0;while(token!=="]"){const holder=Array.isArray(current)?current:undefined;walk(holder?.[i],holder,String(i++));if(token!==",")break;advance();}advance();}
    else {if(parent&&key!==undefined){if(typeof current==="number"&&/^-?\d/.test(start)&&String(current)!==start){const map=literals.get(parent)??new Map<string,string>();map.set(key,start);literals.set(parent,map);}else literals.get(parent)?.delete(key);}advance();}
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

