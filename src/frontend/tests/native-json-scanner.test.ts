import { describe, expect, it } from "vitest";
import { nativeCell, nativeJson, parseNativeJson } from "../src/native-json";

// The scanner as it was before it stopped cutting every token out of the text:
// the kept literals of the fast scanner must match it exactly.
function referenceParse(raw:string,literals:WeakMap<object,Map<string,string>>):unknown {
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

// Every [holder, key] the reference kept a literal for, and every holder/key pair it did not.
function compare(raw: string): void {
  const literals = new WeakMap<object, Map<string, string>>();
  const expected = referenceParse(raw, literals), actual = parseNativeJson(raw);
  expect(actual).toEqual(expected);
  const visit = (want: unknown, got: unknown, path: string): void => {
    if (!want || typeof want !== "object") return;
    const holder = got as Record<string, unknown>, kept = literals.get(want);
    for (const key of Object.keys(want)) {
      expect([path, key, nativeCell(holder, key)]).toEqual([path, key, kept?.get(key) ?? holder[key]]);
      visit((want as Record<string, unknown>)[key], holder[key], `${path}/${key}`);
    }
  };
  visit(expected, actual, "");
  expect(nativeJson(actual)).toBe(nativeJson(expected) === "" ? "" : rebuild(expected, literals));
}
function rebuild(value: unknown, literals: WeakMap<object, Map<string, string>>): string {
  if (!value || typeof value !== "object") return JSON.stringify(value) ?? "null";
  const array = Array.isArray(value), keys = Object.keys(value);
  const parts = keys.map((key) => (array ? "" : JSON.stringify(key) + ":") + (literals.get(value)?.get(key) ?? rebuild((value as Record<string, unknown>)[key], literals)));
  return array ? `[${parts.join(",")}]` : `{${parts.join(",")}}`;
}

function random(seed: number) {
  let state = seed >>> 0;
  return () => ((state = (Math.imul(state, 1664525) + 1013904223) >>> 0) / 2 ** 32);
}
const NUMBERS = ["0", "-0", "7", "-12", "123456789012345", "-123456789012345", "1234567890123456", "9007199254740993", "18446744073709551615",
  "1.5", "1.0", "-0.0", "0.1", "22022.578277222889", "1e5", "1E+5", "2.5e-3", "-1.25E2", "100", "1e400", "0.30000000000000004"];
// JSON texts: "", "a", "a\\" (a backslash), "\\\"" (backslash, quote), "x\"y", "\u00e9", ...
const STRINGS = ['""', '"a"', '"a\\\\"', '"\\\\\\""', '"x\\"y"', '"\\u00e9"', '"12"', '"true"', '"1.0"'];
function value(next: () => number, depth: number): string {
  const pick = <T,>(items: readonly T[]) => items[Math.floor(next() * items.length)];
  const roll = next();
  const ws = () => pick(["", " ", "\n", "\t ", "\r\n"]);
  if (depth > 0 && roll < 0.25) {
    const keys = ["a", "b", "a\\\"b", "k\u0031", "x\\\\y", "12", "__x", "c d"];
    const n = Math.floor(next() * 5);
    return "{" + ws() + Array.from({ length: n }, () => `${ws()}"${pick(keys)}"${ws()}:${ws()}${value(next, depth - 1)}${ws()}`).join(",") + "}";
  }
  if (depth > 0 && roll < 0.5) return "[" + ws() + Array.from({ length: Math.floor(next() * 5) }, () => ws() + value(next, depth - 1) + ws()).join(",") + "]";
  if (roll < 0.8) return pick(NUMBERS);
  if (roll < 0.9) return pick(STRINGS);
  return pick(["true", "false", "null"]);
}

describe("parseNativeJson fast scanner", () => {
  it("keeps the same literals as the reference scanner on hand-picked documents", () => {
    for (const raw of [
      "-0", "1.0", "18446744073709551615", "[]", "{}", " [ 1.0 , -0 , 5 ] ", '{"a":1.0,"a":2}', '{"a":2,"a":1.0}', '{"a":1.0,"a":"x"}',
      '{"a\\"b":1.50,"c":[{"d":[1e2,"e",null,true,false,{}]},[]],"f":12345678901234567}', '{"x":[[[-0.0]]],"y":{"z":{"w":2.50}}}',
    ]) compare(raw);
  });

  it("keeps the same literals as the reference scanner on random documents", () => {
    const next = random(20261104);
    for (let i = 0; i < 3000; ++i) {
      const raw = value(next, 5);
      // The generator may repeat keys; JSON.parse keeps the last, as both scanners assume.
      compare(raw);
    }
  });
});
