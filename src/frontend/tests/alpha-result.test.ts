import { describe, expect, it } from "vitest";
import { alphaPlusResultQuery, isAlphaPlusResult } from "../src/alpha-result";

describe("Alpha+ explicit markings", () => {
  const pair = { before: ["artificial_start"], after: ["artificial_end"] };
  const result = { family: "ALPHA_PLUS", places: [
    { id: 0, name: pair, initial: 0, final: 0 },
    { id: 1, name: { boundary: "end" }, initial: 0, final: 1 },
  ], transitions: [{ id: 0, label: "a", inputs: [0], outputs: [0] }],
    detached_initial_marking: [{ name: pair, tokens: 1 }], detached_final_marking: [] };
  it("preserves detached identity even with a same-named live place", () => {
    expect(isAlphaPlusResult(result)).toBe(true);
    expect(isAlphaPlusResult({ ...result, family: "DFG" })).toBe(false);
    expect(isAlphaPlusResult({ ...result, detached_initial_marking: [{ name: pair, tokens: 0 }] })).toBe(false);
    expect(isAlphaPlusResult({ ...result, places: [{ ...result.places[0], initial: -1 }] })).toBe(false);
    expect(isAlphaPlusResult({ ...result, transitions: [{ ...result.transitions[0], inputs: [2] }] })).toBe(false);
    expect(isAlphaPlusResult({ ...result, transitions: [{ ...result.transitions[0], id: 1 }] })).toBe(false);
    expect(isAlphaPlusResult({ ...result, detached_initial_marking: [{ name: { ...pair, boundary: "end" }, tokens: 1 }] })).toBe(false);
  });
  it("preserves explicit zero event caps and validates selectors", () => {
    const query = new URLSearchParams(alphaPlusResultQuery({ activity_attribute: " phase ", remove_unconnected: false, max_rows_to_materialize: 0 }));
    expect(Object.fromEntries(query)).toEqual({ alpha_activity_attribute: "phase", alpha_remove_unconnected: "false", alpha_max_rows_to_materialize: "0" });
    expect(alphaPlusResultQuery()).toBe("");
    expect(() => alphaPlusResultQuery({ activity_attribute: " " })).toThrow();
    expect(() => alphaPlusResultQuery({ max_rows_to_materialize: 0.5 })).toThrow();
  });
});
