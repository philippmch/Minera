import { describe, expect, it } from "vitest";
import { buildEfgMatrix } from "./eventually-follows";
import { type ActivityStat } from "./types";

const activity = (name: string, count: number): ActivityStat => ({
  name,
  count,
  start_count: 0,
  end_count: 0,
});

describe("buildEfgMatrix", () => {
  it("orders both axes most-frequent-first and places counts", () => {
    // Registry order a, b, c but frequencies make the display order b, a, c.
    const activities = [activity("a", 5), activity("b", 9), activity("c", 2)];
    const edges = [
      { source: 0, target: 1, count: 4 }, // a ↝ b
      { source: 1, target: 2, count: 7 }, // b ↝ c
    ];
    const matrix = buildEfgMatrix(activities, edges);
    expect(matrix.order).toEqual([1, 0, 2]);
    // b ↝ c lands at row of b (0), column of c (2).
    expect(matrix.counts[0][2]).toBe(7);
    // a ↝ b lands at row of a (1), column of b (0).
    expect(matrix.counts[1][0]).toBe(4);
    expect(matrix.max).toBe(7);
    // Unobserved pairs stay zero.
    expect(matrix.counts[2][0]).toBe(0);
  });

  it("drops zero-count registry entries such as imported-model activities", () => {
    const activities = [activity("a", 3), activity("ghost", 0)];
    const matrix = buildEfgMatrix(activities, [
      { source: 0, target: 0, count: 1 },
      { source: 0, target: 1, count: 9 }, // references the dropped activity
    ]);
    expect(matrix.order).toEqual([0]);
    expect(matrix.counts).toEqual([[1]]);
    expect(matrix.max).toBe(1);
  });

  it("handles an empty log", () => {
    const matrix = buildEfgMatrix([], []);
    expect(matrix.order).toEqual([]);
    expect(matrix.max).toBe(0);
  });
});
