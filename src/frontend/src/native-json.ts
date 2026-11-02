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
