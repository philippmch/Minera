import { describe, expect, it } from "vitest";
import { flattenDecisionRules, splitCondition } from "./conformance";
import { type DecisionTreeNode } from "./types";

describe("splitCondition", () => {
  it("renders numeric splits as thresholds and categorical ones as equality", () => {
    expect(
      splitCondition({ counts: [], key: "amount", threshold: 1150 }),
    ).toBe("amount ≤ 1150");
    expect(splitCondition({ counts: [], key: "channel", value: "web" })).toBe(
      "channel = web",
    );
  });
});

describe("flattenDecisionRules", () => {
  it("yields one rule per leaf, negating else-path conditions", () => {
    const tree: DecisionTreeNode = {
      counts: [24, 16],
      key: "amount",
      threshold: 1150,
      then: { counts: [24, 0], branch: 0 },
      else: {
        counts: [0, 16],
        key: "channel",
        value: "web",
        then: { counts: [0, 6], branch: 1 },
        else: { counts: [0, 10], branch: 1 },
      },
    };
    expect(flattenDecisionRules(tree)).toEqual([
      { conditions: ["amount ≤ 1150"], branch: 0, hit: 24, total: 24 },
      {
        conditions: ["not amount ≤ 1150", "channel = web"],
        branch: 1,
        hit: 6,
        total: 6,
      },
      {
        conditions: ["not amount ≤ 1150", "not channel = web"],
        branch: 1,
        hit: 10,
        total: 10,
      },
    ]);
  });

  it("maps a lone leaf to a single unconditional rule", () => {
    expect(flattenDecisionRules({ counts: [3, 9], branch: 1 })).toEqual([
      { conditions: [], branch: 1, hit: 9, total: 12 },
    ]);
  });
});
