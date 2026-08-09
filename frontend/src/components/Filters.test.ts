import { describe, expect, it } from "vitest";
import { pathValidationMessage, repetitionValidationMessage, segmentValidationMessage, temporalValidationMessage } from "./Filters";

const valid = {
  spectrumConfigured: true,
  spectrumActivityCount: 2,
  spectrumSampleSize: "10000",
  batches: true,
  batchMergeDistanceSeconds: "900",
  batchMinSize: "2",
  overlap: false,
  overlapEpsilon: "0.00001",
};

describe("temporal filter validation", () => {
  it("accepts active defaults and ignores dependent values when inactive", () => {
    expect(temporalValidationMessage(valid)).toBeNull();
    expect(temporalValidationMessage({ ...valid, spectrumConfigured: false, spectrumActivityCount: 0, batches: false, spectrumSampleSize: "0", batchMergeDistanceSeconds: "-1", batchMinSize: "1" })).toBeNull();
  });

  it("rejects exactly one nonblank spectrum activity", () => {
    expect(temporalValidationMessage({ ...valid, spectrumConfigured: false, spectrumActivityCount: 1 })).toContain("at least two activities");
  });

  it("rejects invalid active spectrum and batch numbers", () => {
    expect(temporalValidationMessage({ ...valid, spectrumSampleSize: "0" })).toContain("Spectrum sample size");
    expect(temporalValidationMessage({ ...valid, spectrumSampleSize: "2.5" })).toContain("Spectrum sample size");
    expect(temporalValidationMessage({ ...valid, batchMergeDistanceSeconds: "-1" })).toContain("merge distance");
    expect(temporalValidationMessage({ ...valid, batchMergeDistanceSeconds: "Infinity" })).toContain("merge distance");
    expect(temporalValidationMessage({ ...valid, batchMinSize: "1" })).toContain("minimum size");
    expect(temporalValidationMessage({ ...valid, batchMinSize: "2.1" })).toContain("minimum size");
  });

  it("validates active overlap epsilon", () => {
    expect(temporalValidationMessage({ ...valid, overlap: true, overlapEpsilon: "-1" })).toContain("Overlap epsilon");
    expect(temporalValidationMessage({ ...valid, overlap: true, overlapEpsilon: "Infinity" })).toContain("Overlap epsilon");
    expect(temporalValidationMessage({ ...valid, overlap: true, overlapEpsilon: "0" })).toBeNull();
  });
});

describe("segment filter validation", () => {
  it("requires both between endpoints", () => {
    expect(segmentValidationMessage({ betweenStart: "A", betweenEnd: "" })).toContain("both");
    expect(segmentValidationMessage({ betweenStart: "", betweenEnd: "B" })).toContain("both");
    expect(segmentValidationMessage({ betweenStart: "A", betweenEnd: "B" })).toBeNull();
    expect(segmentValidationMessage({ betweenStart: "", betweenEnd: "" })).toBeNull();
  });
});

describe("adjacent path validation", () => {
  const validPath = {
    predecessor: "A",
    successor: "B",
    minPerformance: "",
    maxPerformance: "",
    positive: true,
  };

  it("requires both activities and rejects max-only negative ranges", () => {
    expect(pathValidationMessage({ ...validPath, predecessor: "" })).toContain("both");
    expect(pathValidationMessage(validPath)).toBeNull();
    expect(pathValidationMessage({ ...validPath, maxPerformance: "-1" })).toContain("must not exceed");
    expect(pathValidationMessage({ ...validPath, minPerformance: "-2", maxPerformance: "-1" })).toBeNull();
  });
});

describe("attribute repetition validation", () => {
  it("accepts the default lower bound and optional upper bound", () => {
    expect(repetitionValidationMessage({ value: "A", min: "", max: "" })).toBeNull();
    expect(repetitionValidationMessage({ value: "A", min: "0", max: "2" })).toBeNull();
    expect(repetitionValidationMessage({ value: "", min: "bad", max: "-1" })).toBeNull();
  });

  it("rejects non-integer and inverted active bounds", () => {
    expect(repetitionValidationMessage({ value: "A", min: "1.5", max: "" })).toContain("minimum");
    expect(repetitionValidationMessage({ value: "A", min: "3", max: "2" })).toContain("must not exceed");
    expect(repetitionValidationMessage({ value: "A", min: "", max: "-1" })).toContain("maximum");
  });
});
