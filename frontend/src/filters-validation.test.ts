import { describe, expect, it } from "vitest";
import {
  caseDurationValidationMessage,
  caseSelectionValidationMessage,
} from "./components/Filters";

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

describe("case count and size filter validation", () => {
  it("accepts zero case limits and open or ordered size bounds", () => {
    expect(caseSelectionValidationMessage({ maxCases: "0", minLength: "", maxLength: "" })).toBeNull();
    expect(caseSelectionValidationMessage({ maxCases: "5", minLength: "2", maxLength: "" })).toBeNull();
    expect(caseSelectionValidationMessage({ maxCases: "", minLength: "2", maxLength: "4" })).toBeNull();
    expect(caseSelectionValidationMessage({ maxCases: "", minLength: "2", maxLength: "0" })).toBeNull();
  });

  it("rejects non-integers, negatives, and reversed size bounds", () => {
    expect(caseSelectionValidationMessage({ maxCases: "1.5", minLength: "", maxLength: "" })).toMatch(/integer/);
    expect(caseSelectionValidationMessage({ maxCases: "-1", minLength: "", maxLength: "" })).toMatch(/non-negative/);
    expect(caseSelectionValidationMessage({ maxCases: "", minLength: "5", maxLength: "4" })).toMatch(/not exceed/);
  });
});
