import { describe, expect, it } from "vitest";
import { monteCarloQuery } from "../src/api";

describe("Monte Carlo scheduler resource controls", () => {
  it("passes zero capacities through the same request fields as positive capacities", () => {
    const query = new URLSearchParams(monteCarloQuery({
      default_resources_per_place: 0,
      resources_per_place: { 2: 3, 0: 0 },
      enable_diagnostics: false,
    }));
    expect(query.get("monte_carlo_default_resources_per_place")).toBe("0");
    expect(query.get("monte_carlo_resources_per_place")).toBe("0=0,2=3");
    expect(query.get("monte_carlo_enable_diagnostics")).toBe("false");
  });

  it.each([-1, 0.5, Infinity, NaN])("rejects unsupported capacity %s", (capacity) => {
    expect(() => monteCarloQuery({ default_resources_per_place: capacity })).toThrow();
    expect(() => monteCarloQuery({ resources_per_place: { 0: capacity } })).toThrow();
  });
});
