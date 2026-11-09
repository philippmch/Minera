import { describe, expect, it } from "vitest";
import { defaultAlignmentOptionsFor } from "../src/components/Conformance";
import { DEFAULT_ALIGNMENT_OPTIONS } from "../src/types";

// Round AI: a supplied model's alignment view starts from the reference's
// public default variant (state-equation A*), which the backend uses when the
// request names none; the UI used to send less-memory Dijkstra for it.
describe("defaultAlignmentOptionsFor", () => {
  it("starts an imported model on state-equation A*", () => {
    expect(defaultAlignmentOptionsFor("imported")).toEqual({
      ...DEFAULT_ALIGNMENT_OPTIONS,
      variant: "state_equation_a_star",
    });
  });

  it("keeps the less-memory default for discovered models", () => {
    for (const miner of ["inductive", "alpha", "heuristics"])
      expect(defaultAlignmentOptionsFor(miner)).toEqual(DEFAULT_ALIGNMENT_OPTIONS);
    expect(DEFAULT_ALIGNMENT_OPTIONS.variant).toBe("dijkstra_less_memory");
  });
});
