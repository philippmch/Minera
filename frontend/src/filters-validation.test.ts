import { describe, expect, it } from "vitest";
import { caseDurationValidationMessage } from "./components/Filters";

describe("case duration filter validation", () => {
  it("accepts inclusive one-sided and ordered bounds", () => {
    expect(caseDurationValidationMessage({ min: "", max: "30" })).toBeNull();
    expect(caseDurationValidationMessage({ min: "10", max: "30" })).toBeNull();
    expect(caseDurationValidationMessage({ min: "10", max: "" })).toBeNull();
  });

  it("rejects non-finite, negative maximum, and reversed bounds", () => {
    expect(caseDurationValidationMessage({ min: "nan", max: "" })).toMatch(/finite/);
    expect(caseDurationValidationMessage({ min: "", max: "-1" })).toMatch(/non-negative/);
    expect(caseDurationValidationMessage({ min: "31", max: "30" })).toMatch(/not exceed/);
  });
});
