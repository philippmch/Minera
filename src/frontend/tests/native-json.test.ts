import { describe, expect, it } from "vitest";
import { nativeCell, nativeJson, parseNativeJson } from "../src/native-json";

describe("parseNativeJson", () => {
  it("keeps exact integer literals beside their holders", () => {
    const value = parseNativeJson('{"big":18446744073709551615,"list":[9007199254740993,1.5],"s":"x"}') as {big: number; list: number[]};
    expect(nativeCell(value, "big")).toBe("18446744073709551615");
    expect(nativeCell(value.list, 0)).toBe("9007199254740993");
    expect(nativeJson(value)).toBe('{"big":18446744073709551615,"list":[9007199254740993,1.5],"s":"x"}');
  });

  it("handles escaped quotes and backslashes inside strings and keys", () => {
    const raw = '{"a\\"b":"c\\\\","d":["\\\\\\"",12345678901234567890]}';
    const value = parseNativeJson(raw) as Record<string, unknown>;
    expect(value['a"b']).toBe("c\\");
    expect(nativeCell(value.d as object, 1)).toBe("12345678901234567890");
  });

  it("parses strings larger than 20 MB without exhausting the stack", () => {
    // Escapes make a backtracking string pattern recurse per character.
    const chunk = 'x\\"y\\\\z ';
    const text = chunk.repeat(Math.ceil(21_000_000 / chunk.length));
    expect(text.length).toBeGreaterThan(20_000_000);
    const raw = `{"body":"${text}","after":18446744073709551615}`;
    const value = parseNativeJson(raw) as {body: string; after: number};
    expect(value.body.length).toBeGreaterThan(15_000_000);
    expect(nativeCell(value, "after")).toBe("18446744073709551615");
  });
});
