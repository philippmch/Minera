import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { runInNewContext } from "node:vm";

// Exercise the actual pure scanner without importing the CLI or auditing any
// reference checkout. Fail explicitly if its extraction boundaries change.
const source = readFileSync(new URL("./verify-reference-parity.mjs", import.meta.url), "utf8");
const begin = source.indexOf("function maskRange(");
const end = source.indexOf("function cmakeTestRegistry(");
assert(begin >= 0 && end > begin);
const { maskCommentsAndLiterals, functionBody, codeContainsSymbol } = runInNewContext(
  source.slice(begin, end) + "\n({maskCommentsAndLiterals,functionBody,codeContainsSymbol})",
);

for (const decoration of ["plain", "α", "😀", "😀🚀", "𐀀"]) {
  const cpp = `// ${decoration} { misleading_call(); }
void before() { const char* text = "${decoration} } hidden_call();"; }
int main() {
  const char* raw = R"json({"label":"${decoration}","brace":"}"})json";
  const char* escaped = "\\\" }";
  const auto count = 1'000;
  /* ${decoration} } commented_call(); */
  actual_contract();
}
void later() { other_contract(); }`;
  const masked = maskCommentsAndLiterals(cpp);
  assert.equal(masked.length, cpp.length, `UTF-16 offset preservation: ${decoration}`);
  const main = functionBody(cpp, "main");
  assert(main !== undefined);
  assert(codeContainsSymbol(main, "actual_contract"));
  for (const absent of ["misleading_call", "hidden_call", "commented_call", "other_contract"])
    assert(!codeContainsSymbol(main, absent));
}

// These real contracts exposed the regression: their main contains non-BMP
// strings before the two description acceptance calls.
const ocel = readFileSync(new URL("../src/backend/tests/test_ocel.cpp", import.meta.url), "utf8");
const main = functionBody(ocel, "main");
assert(main !== undefined);
assert(codeContainsSymbol(main, "variant1_text_contract"));
assert(codeContainsSymbol(main, "variant2_tuple_contract"));
console.log("Parity source parser: Unicode offsets, literal masking and OCEL calls passed.");
