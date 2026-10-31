import { describe, expect, it } from "vitest";
import type { VariantStat } from "../src/types";
import { cumulativeCoverage } from "../src/variant-insights";
import { variantHighlight } from "../src/components/ProcessMap";
import { edgeKey } from "../src/layout";
import { END, START } from "../src/types";

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

describe("variantHighlight", () => {
  it("highlights the empty variant (cases without events) as start to end", () => {
    const highlight = variantHighlight([]);
    expect([...highlight.edges]).toEqual([edgeKey(START, END)]);
    expect(highlight.nodes.size).toBe(0);
  });

  it("walks a non-empty variant from start through its steps to end", () => {
    const highlight = variantHighlight([0, 1]);
    expect([...highlight.edges]).toEqual([edgeKey(START, 0), edgeKey(0, 1), edgeKey(1, END)]);
    expect([...highlight.nodes]).toEqual([0, 1]);
  });
});
