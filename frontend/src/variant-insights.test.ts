import { describe, expect, it } from "vitest";
import type { VariantStat } from "./types";
import { cumulativeCoverage } from "./variant-insights";

const variant = (count: number): VariantStat => ({ activities: [], count });

describe("cumulativeCoverage", () => {
  it("accumulates frequency shares to 1 across all variants", () => {
    const coverage = cumulativeCoverage([variant(6), variant(3), variant(1)], 10);
    expect(coverage).toEqual([0.6, 0.9, 1]);
  });

  it("returns an empty array for no variants", () => {
    expect(cumulativeCoverage([], 10)).toEqual([]);
  });

  it("avoids dividing by zero when there are no cases", () => {
    expect(cumulativeCoverage([variant(0), variant(0)], 0)).toEqual([0, 0]);
  });

  it("treats each variant's own share independently of order assumptions", () => {
    // Single dominant variant covers everything on the first step.
    expect(cumulativeCoverage([variant(10)], 10)).toEqual([1]);
  });
});
