import { describe, expect, it } from "vitest";
import { SERVER_EXTENSIVE_MAX_LENGTH, extensiveTreeTooLong, processTreeShortestWordLength } from "../src/process-tree-playout-bounds";
import type { ProcessTreeNode } from "../src/types";

const leaf = (activity: number): ProcessTreeNode => ({ operator: "activity", activity });
const tau: ProcessTreeNode = { operator: "tau" };
const node = (operator: ProcessTreeNode["operator"], ...children: ProcessTreeNode[]): ProcessTreeNode => ({ operator, children });

describe("process-tree EXTENSIVE server bounds", () => {
  it("reads the shortest trace length as the server's default max_length", () => {
    expect(processTreeShortestWordLength(node("sequence", leaf(0), node("exclusive", tau, leaf(1)), node("parallel", leaf(2), leaf(3))))).toBe(3);
    expect(processTreeShortestWordLength(node("loop", node("sequence", leaf(0), leaf(1)), leaf(2)))).toBe(2);
    expect(processTreeShortestWordLength(node("inclusive_or", leaf(0), node("sequence", leaf(1), leaf(2))))).toBe(1);
    expect(processTreeShortestWordLength(tau)).toBe(0);
    // Deep trees use an explicit stack.
    let deep: ProcessTreeNode = leaf(0);
    for (let i = 0; i < 20000; ++i) deep = node("sequence", deep, tau);
    expect(processTreeShortestWordLength(deep)).toBe(1);
  });
  it("flags only EXTENSIVE on a tree whose shortest trace is above the server limit", () => {
    const long = node("sequence", ...Array.from({ length: 70 }, (_, i) => leaf(i)));
    const fits = node("sequence", ...Array.from({ length: SERVER_EXTENSIVE_MAX_LENGTH }, (_, i) => leaf(i)));
    expect(extensiveTreeTooLong(long, { variant: "EXTENSIVE" })).toBe(70);
    expect(extensiveTreeTooLong(long, { variant: "EXTENSIVE", max_length: 64 })).toBe(70);
    expect(extensiveTreeTooLong(long, { variant: "TOPBOTTOM" })).toBeNull();
    expect(extensiveTreeTooLong(long, {})).toBeNull();
    expect(extensiveTreeTooLong(fits, { variant: "EXTENSIVE" })).toBeNull();
    expect(extensiveTreeTooLong(undefined, { variant: "EXTENSIVE" })).toBeNull();
  });
});
