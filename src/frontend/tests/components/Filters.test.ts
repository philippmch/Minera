import { describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { alphaValidationMessage, tokenReplayInputOptionsFromForm, tokenReplayInputValidationMessage } from "../../src/components/Filters";
import { TOKEN_REPLAY_POLICY_DEFAULTS, TokenReplayPolicyControls, tokenReplayPolicyOptionsFromForm,
  tokenReplayPoliciesAreDirty } from "../../src/components/Filters";
import { TokenReplayRootCauseControls, tokenReplayRootCauseOptionsFromForm } from "../../src/components/Filters";
import { alphaOptionsFromForm, alphaSettingsAreDirty, attributeOccurrenceValidationMessage, caseGroupingValidationMessage, correlationValidationMessage, footprintsValidationMessage, heuristicBusinessHourSlotsValidationMessage, heuristicBusinessHoursOptionsAreActive, heuristicStartTimestampOptionsFromForm, heuristicStartTimestampPlaceholder, inductiveOptionsFromForm, inductiveSettingsAreDirty, pathValidationMessage, powlValidationMessage, repetitionValidationMessage, relativeAttributeValidationMessage, segmentValidationMessage, temporalValidationMessage, topActivitiesValidationMessage, variantCoverageValidationMessage } from "../../src/components/Filters";

describe("token replay policy controls", () => {
  it("disables only forward-only policies backwards without changing retained values", () => {
    const policies = { ...TOKEN_REPLAY_POLICY_DEFAULTS, tokenReplayConsiderRemainingInFitness: false, tokenReplayCleaningTokenFlood: true, tokenReplayExhaustiveInvisibleExploration: true };
    const render = (variant: "forward" | "backwards") => renderToStaticMarkup(
      createElement(TokenReplayPolicyControls, { variant, policies, onChange: () => {} }),
    );
    const backwards = render("backwards");
    expect(backwards.match(/disabled=""/g)).toHaveLength(5);
    expect(backwards).toContain("ignored in backwards replay");
    expect(backwards).toContain("Require no remaining tokens for a fitting trace");
    const labels = backwards.match(/<label[\s\S]*?<\/label>/g)!;
    expect(labels[7]).toContain("Clean token flooding during replay");
    expect(labels[7]).toContain('type="checkbox"');
    expect(labels[7]).toContain('checked=""');
    expect(labels[7]).toContain('disabled=""');
    expect(labels[8]).toContain("Exhaustive hidden exploration");
    expect(labels[8]).toContain('checked=""');
    expect(labels[8]).toContain('disabled=""');
    expect(labels[0]).toContain('<option value="false" selected="">');
    expect(labels[3]).not.toContain("disabled");
    expect(labels[4]).not.toContain("disabled");
    expect(labels[5]).toContain("Include replay duration diagnostics");
    expect(labels[5]).not.toContain("disabled");
    expect(labels[5]).toContain('<option value="false" selected="">no (default)</option>');
    const forward = render("forward");
    expect(forward).not.toContain("disabled");
    expect(forward).not.toContain("ignored in backwards replay");
    expect(forward.match(/<label[\s\S]*?<\/label>/g)![0]).toContain('<option value="false" selected="">');
    expect(forward.match(/<label[\s\S]*?<\/label>/g)![7]).toContain('checked=""');
    expect(policies.tokenReplayCleaningTokenFlood).toBe(true);
    expect(forward.match(/<label[\s\S]*?<\/label>/g)![8]).toContain('checked=""');
    expect(policies.tokenReplayExhaustiveInvisibleExploration).toBe(true);
    expect(policies.tokenReplayConsiderRemainingInFitness).toBe(false);
  });

  it("keeps defaults implicit and clears dirty state on reset", () => {
    const defaults = { ...TOKEN_REPLAY_POLICY_DEFAULTS };
    expect(tokenReplayPolicyOptionsFromForm(defaults)).toEqual({});
    expect(tokenReplayPoliciesAreDirty(defaults)).toBe(false);
    for (const key of Object.keys(defaults) as (keyof typeof defaults)[]) {
      const changed = { ...defaults, [key]: !defaults[key] };
      expect(tokenReplayPoliciesAreDirty(changed)).toBe(true);
      expect(tokenReplayPolicyOptionsFromForm(changed)).toEqual({ [key]: !defaults[key] });
      Object.assign(changed, TOKEN_REPLAY_POLICY_DEFAULTS);
      expect(tokenReplayPoliciesAreDirty(changed)).toBe(false);
      expect(tokenReplayPolicyOptionsFromForm(changed)).toEqual({});
    }
  });

  it("applies disabled fitness/navigation policies together with requested diagnostics", () => {
    const changed = {
      tokenReplayIncludeRootCauseDiagnostics: true,
      tokenReplayConsiderRemainingInFitness: false,
      tokenReplayTryToReachFinalMarkingThroughHidden: false,
      tokenReplayConsiderActivitiesNotInModelInFitness: true,
      tokenReplayCleaningTokenFlood: true,
      tokenReplayExhaustiveInvisibleExploration: true,
      tokenReplayWalkThroughHiddenTransitions: false,
      tokenReplayIncludeTraceDiagnostics: true,
      tokenReplayIncludeDurationDiagnostics: true,
    };
    expect(tokenReplayPolicyOptionsFromForm(changed)).toEqual(changed);
    expect(TOKEN_REPLAY_POLICY_DEFAULTS.tokenReplayConsiderRemainingInFitness).toBe(true);
    expect(TOKEN_REPLAY_POLICY_DEFAULTS.tokenReplayTryToReachFinalMarkingThroughHidden).toBe(true);
    expect(TOKEN_REPLAY_POLICY_DEFAULTS.tokenReplayConsiderActivitiesNotInModelInFitness).toBe(false);
    expect(TOKEN_REPLAY_POLICY_DEFAULTS.tokenReplayWalkThroughHiddenTransitions).toBe(true);
    expect(TOKEN_REPLAY_POLICY_DEFAULTS.tokenReplayIncludeTraceDiagnostics).toBe(false);
    expect(TOKEN_REPLAY_POLICY_DEFAULTS.tokenReplayIncludeDurationDiagnostics).toBe(false);
  });
});

describe("heuristic start-time control", () => {
  it("keeps automatic selection empty and preserves explicit selectors", () => {
    expect(heuristicStartTimestampOptionsFromForm("")).toEqual({});
    expect(heuristicStartTimestampOptionsFromForm("  ")).toEqual({});
    expect(heuristicStartTimestampOptionsFromForm(" start_timestamp ")).toEqual({
      heuristicStartTimestampAttribute: "start_timestamp",
    });
    expect(heuristicStartTimestampOptionsFromForm(" began ")).toEqual({
      heuristicStartTimestampAttribute: "began",
    });
  });

  it("shows each variant's conventional automatic selector", () => {
    expect(heuristicStartTimestampPlaceholder("classic")).toBe("time:timestamp");
    expect(heuristicStartTimestampPlaceholder("classic", " finished_at ")).toBe("finished_at");
    expect(heuristicStartTimestampPlaceholder("classic", "  ")).toBe("time:timestamp");
    expect(heuristicStartTimestampPlaceholder("plusplus")).toBe("start_timestamp");
    expect(heuristicStartTimestampPlaceholder("plusplus", "finished_at")).toBe("start_timestamp");
  });

  it("enables schedules for Classic performance and all Miner++ decorations", () => {
    expect(heuristicBusinessHoursOptionsAreActive("classic", "frequency")).toBe(false);
    expect(heuristicBusinessHoursOptionsAreActive("classic", "performance")).toBe(true);
    expect(heuristicBusinessHoursOptionsAreActive("plusplus", "frequency")).toBe(true);
    expect(heuristicBusinessHoursOptionsAreActive("plusplus", "performance")).toBe(true);
  });

  it("validates weekly schedules only when business hours are enabled", () => {
    expect(heuristicBusinessHourSlotsValidationMessage("", true)).toBeNull();
    expect(heuristicBusinessHourSlotsValidationMessage("none", true)).toBeNull();
    expect(heuristicBusinessHourSlotsValidationMessage(" 32400-61200,378000-406800 ", true)).toBeNull();
    expect(heuristicBusinessHourSlotsValidationMessage("0-0", true)).toMatch(/within one week/);
    expect(heuristicBusinessHourSlotsValidationMessage("0:3600", true)).toMatch(/start-end/);
    expect(heuristicBusinessHourSlotsValidationMessage("0-0", false)).toBeNull();
  });
});

describe("token replay input settings", () => {
  it("validates selected root-cause attributes while retaining disabled form values", () => {
    expect(tokenReplayRootCauseOptionsFromForm(false, "a,,b", "", true)).toEqual({});
    expect(() => tokenReplayRootCauseOptionsFromForm(true, "", " ")).toThrow(/at least one/);
    expect(() => tokenReplayRootCauseOptionsFromForm(true, "a,,b", "")).toThrow(/non-empty/);
    expect(tokenReplayRootCauseOptionsFromForm(true, " region, team, region ", "")).toEqual({
      tokenReplayRootCauseStringAttributes: ["region", "team", "region"],
      tokenReplayRootCauseNumericAttributes: [], tokenReplayRootCauseEnableMultiplier: false,
    });
    const renderControls = (enabled: boolean) => renderToStaticMarkup(createElement(TokenReplayRootCauseControls, {
      enabled, strings: "region", numerics: "amount", multiplier: false,
      onStrings: () => {}, onNumerics: () => {}, onMultiplier: () => {},
    }));
    expect(renderControls(false).match(/disabled=""/g)).toHaveLength(3);
    expect(renderControls(false)).toContain('value="region"');
    expect(renderControls(false)).toContain('value="amount"');
    expect(renderControls(false)).toContain('<option value="false" selected="">no (default)</option>');
    expect(renderControls(true)).not.toContain("disabled");
  });
  it("only applies custom duration timestamps when enabled and preserves implicit defaults", () => {
    expect(tokenReplayInputOptionsFromForm("", "time:timestamp", true)).toEqual({});
    expect(tokenReplayInputOptionsFromForm("", " completed_at ", true))
      .toEqual({ tokenReplayTimestampAttribute: "completed_at" });
    expect(tokenReplayInputOptionsFromForm("", "completed_at", false)).toEqual({});
    expect(tokenReplayInputOptionsFromForm("", " ", false)).toEqual({});
    expect(tokenReplayInputValidationMessage("", " ", true)).toMatch(/timestamp attribute/);
    expect(() => tokenReplayInputOptionsFromForm("", " ", true)).toThrow(/timestamp attribute/);
    expect(tokenReplayInputOptionsFromForm("0", "completed_at", true)).toEqual({
      tokenReplayMaxRowsToMaterialize: 0, tokenReplayTimestampAttribute: "completed_at",
    });
  });
  it("retains zero event cap and rejects invalid or unsafe integers", () => {
    expect(tokenReplayInputOptionsFromForm("  ")).toEqual({});
    expect(tokenReplayInputOptionsFromForm("0")).toEqual({ tokenReplayMaxRowsToMaterialize: 0 });
    expect(tokenReplayInputOptionsFromForm(" 25 ")).toEqual({ tokenReplayMaxRowsToMaterialize: 25 });
    for (const value of ["-1", "0.5", "NaN", "Infinity", "9007199254740992"]) {
      expect(tokenReplayInputValidationMessage(value)).not.toBeNull();
      expect(() => tokenReplayInputOptionsFromForm(value)).toThrow(/event cap/);
    }
  });
});

const valid = {
  spectrumConfigured: true,
  spectrumActivityCount: 2,
  spectrumSampleSize: "10000",
  batches: true,
  batchVariant: "pandas" as const,
  batchMergeDistanceSeconds: "900",
  batchMinSize: "2",
  overlap: false,
  overlapEpsilon: "0.00001",
};

describe("correlation selector validation", () => {
  const selectors = {
    enabled: true,
    activityAttribute: "concept:name",
    timestampAttribute: "time:timestamp",
    startTimestampAttribute: "",
    caseIdAttribute: "case:concept:name",
    traceEnabled: false,
    splitEnabled: false,
    sampleSize: "100000",
  };

  it("accepts defaults and an empty start selector fallback", () => {
    expect(correlationValidationMessage(selectors)).toBeNull();
    expect(correlationValidationMessage({ ...selectors, enabled: false,
      activityAttribute: "", timestampAttribute: "", caseIdAttribute: "" })).toBeNull();
  });

  it("rejects blank required selectors", () => {
    expect(correlationValidationMessage({ ...selectors, activityAttribute: " " }))
      .toMatch(/activity/);
    expect(correlationValidationMessage({ ...selectors, timestampAttribute: "\t" }))
      .toMatch(/timestamp/);
    expect(correlationValidationMessage({ ...selectors, traceEnabled: true,
      caseIdAttribute: "  " }))
      .toMatch(/case-id/);
    expect(correlationValidationMessage({ ...selectors, caseIdAttribute: "  " }))
      .toBeNull();
  });

  it("validates the enabled split sample size exactly", () => {
    for (const sampleSize of ["", "0", "-1", "2.5", "9007199254740992", "junk"]) {
      expect(correlationValidationMessage({ ...selectors, splitEnabled: true,
        sampleSize })).toMatch(/sample size/);
    }
    expect(correlationValidationMessage({ ...selectors, splitEnabled: true,
      sampleSize: "2" })).toBeNull();
    expect(correlationValidationMessage({ ...selectors, splitEnabled: false,
      sampleSize: "junk" })).toBeNull();
  });
});

describe("temporal filter validation", () => {
  it("accepts active defaults and ignores dependent values when inactive", () => {
    expect(temporalValidationMessage(valid)).toBeNull();
    expect(temporalValidationMessage({ ...valid, spectrumConfigured: false, spectrumActivityCount: 0, batches: false, spectrumSampleSize: "0", batchMergeDistanceSeconds: "-1", batchMinSize: "1" })).toBeNull();
  });

  it("rejects exactly one nonblank spectrum activity", () => {
    expect(temporalValidationMessage({ ...valid, spectrumConfigured: false, spectrumActivityCount: 1 })).toContain("at least two activities");
    expect(temporalValidationMessage({
      ...valid,
      spectrumHasEmptyActivity: true,
    })).toContain("empty entries");
  });

  it("rejects invalid active spectrum and batch numbers", () => {
    expect(temporalValidationMessage({ ...valid, spectrumSampleSize: "0" })).toBeNull();
    expect(temporalValidationMessage({ ...valid, spectrumSampleSize: "2.5" })).toContain("Spectrum sample size");
    expect(temporalValidationMessage({ ...valid, spectrumSampleSize: "-1" })).toContain("Spectrum sample size");
    expect(temporalValidationMessage({ ...valid, spectrumSampleSize: "9007199254740992" })).toContain("Spectrum sample size");
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

  it("requires nonblank behavioral selectors when supplied", () => {
    expect(temporalValidationMessage({ ...valid, transitionActivityKey: " " }))
      .toContain("activity selector");
    expect(temporalValidationMessage({ ...valid, transitionCaseIdKey: "\t" }))
      .toContain("case-id selector");
    expect(temporalValidationMessage({ ...valid, batches: false, batchEventIdAttribute: " " }))
      .toBeNull();
    expect(temporalValidationMessage({ ...valid, batchVariant: "log", batchEventIdAttribute: " " , batches: true }))
      .toBeNull();
    expect(temporalValidationMessage({ ...valid, batchVariant: "pandas", batchEventIdAttribute: " " , batches: true }))
      .toContain("event-id selector");
  });

  it("validates transition-system controls against their backend defaults", () => {
    expect(temporalValidationMessage({
      ...valid,
      transitionView: "sequence",
      transitionDirection: "forward",
      transitionWindow: "2",
    })).toBeNull();
    expect(temporalValidationMessage({
      ...valid,
      transitionView: "unknown" as never,
    })).toContain("view");
    expect(temporalValidationMessage({
      ...valid,
      transitionDirection: "reverse" as never,
    })).toContain("direction");
    expect(temporalValidationMessage({
      ...valid,
      transitionWindow: "-1",
    })).toContain("window");
    expect(temporalValidationMessage({
      ...valid,
      transitionWindow: "2.5",
    })).toContain("window");
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

describe("case grouping validation", () => {
  it("requires a separator only for concatenation", () => {
    expect(caseGroupingValidationMessage({ timestampGrouping: "first", timestampGroupingSeparator: "" })).toBeNull();
    expect(caseGroupingValidationMessage({ timestampGrouping: "concat", timestampGroupingSeparator: " & " })).toBeNull();
    expect(caseGroupingValidationMessage({ timestampGrouping: "concat", timestampGroupingSeparator: "" })).toContain("separator");
  });
});

describe("POWL option validation", () => {
  const validPowl = {
    filteringThreshold: "0",
    orderFrequencyRatio: "1",
    bruteForceActivityLimit: "",
    activityKey: "concept:name",
    timestampKey: "time:timestamp",
    caseIdKey: "case:concept:name",
  };

  it("accepts defaults and valid boundary values", () => {
    expect(powlValidationMessage(validPowl)).toBeNull();
    expect(powlValidationMessage({
      ...validPowl,
      filteringThreshold: "0.999",
      orderFrequencyRatio: "0.5001",
      bruteForceActivityLimit: "0",
    })).toBeNull();
  });

  it("rejects invalid numbers instead of silently choosing defaults", () => {
    expect(powlValidationMessage({ ...validPowl, filteringThreshold: "1" }))
      .toContain("threshold");
    expect(powlValidationMessage({ ...validPowl, filteringThreshold: "0.2x" }))
      .toContain("threshold");
    expect(powlValidationMessage({ ...validPowl, orderFrequencyRatio: "0.5" }))
      .toContain("ratio");
    expect(powlValidationMessage({ ...validPowl, bruteForceActivityLimit: "1.5" }))
      .toContain("limit");
  });

  it("rejects each blank selector", () => {
    expect(powlValidationMessage({ ...validPowl, activityKey: " " }))
      .toContain("activity");
    expect(powlValidationMessage({ ...validPowl, timestampKey: "\t" }))
      .toContain("timestamp");
    expect(powlValidationMessage({ ...validPowl, caseIdKey: "" }))
      .toContain("case-id");
  });
});

describe("Footprints option validation", () => {
  const defaults = {
    activityAttribute: "concept:name",
    caseIdAttribute: "case:concept:name",
    timestampAttribute: "time:timestamp",
    startTimestampAttribute: "",
    indexAttribute: "@@index",
    initialMarking: "",
  };

  it("accepts defaults, optional start time, and uint64 boundaries", () => {
    expect(footprintsValidationMessage(defaults)).toBeNull();
    expect(footprintsValidationMessage({
      ...defaults,
      startTimestampAttribute: "started_at",
      indexAttribute: " ",
      initialMarking: "000,18446744073709551615",
    })).toBeNull();
  });

  it("rejects blank selectors and malformed marking entries", () => {
    expect(footprintsValidationMessage({ ...defaults, activityAttribute: " " }))
      .toContain("activity");
    expect(footprintsValidationMessage({ ...defaults, initialMarking: "1,,2" }))
      .toContain("non-negative");
    expect(footprintsValidationMessage({ ...defaults, initialMarking: "18446744073709551616" }))
      .toContain("uint64");
  });
});

describe("Inductive control contract", () => {
  const defaults = {
    disableStrictSequenceCut: false,
    disableFallthroughs: false,
    multiprocessing: false,
    sortEventsByTimestamp: false,
    activityKey: "concept:name",
    timestampKey: "time:timestamp",
    caseIdKey: "case:concept:name",
  };

  it("elides all backend defaults", () => {
    expect(inductiveOptionsFromForm(defaults)).toEqual({
      inductiveDisableStrictSequenceCut: undefined,
      inductiveDisableFallthroughs: undefined,
      inductiveMultiprocessing: undefined,
      inductiveSortEventsByTimestamp: undefined,
      inductiveActivityKey: undefined,
      inductiveTimestampKey: undefined,
      inductiveCaseIdKey: undefined,
    });
  });

  it("keeps enabled switches and trims custom selectors", () => {
    expect(inductiveOptionsFromForm({
      disableStrictSequenceCut: true,
      disableFallthroughs: true,
      multiprocessing: true,
      sortEventsByTimestamp: true,
      activityKey: " phase ",
      timestampKey: " occurred_at ",
      caseIdKey: " case_ref ",
    })).toEqual({
      inductiveDisableStrictSequenceCut: true,
      inductiveDisableFallthroughs: true,
      inductiveMultiprocessing: true,
      inductiveSortEventsByTimestamp: true,
      inductiveActivityKey: "phase",
      inductiveTimestampKey: "occurred_at",
      inductiveCaseIdKey: "case_ref",
    });
  });

  it("treats blank selectors as effective defaults for dirty state", () => {
    expect(inductiveSettingsAreDirty({
      ...defaults,
      activityKey: " ",
      timestampKey: "\t",
      caseIdKey: "",
    })).toBe(false);
    expect(inductiveSettingsAreDirty({ ...defaults, sortEventsByTimestamp: true })).toBe(true);
    expect(inductiveSettingsAreDirty({ ...defaults, multiprocessing: true })).toBe(true);
    expect(inductiveSettingsAreDirty({ ...defaults, activityKey: "phase" })).toBe(true);
  });
});

describe("Alpha control contract", () => {
  it("retains a zero event cap and validates cap input", () => {
    const defaults = { activityAttribute: "concept:name", removeUnconnected: false };
    expect(alphaOptionsFromForm({ ...defaults, maxRowsToMaterialize: " 0 " }))
      .toMatchObject({ alphaMaxRowsToMaterialize: 0 });
    expect(alphaSettingsAreDirty({ ...defaults, maxRowsToMaterialize: "0" }))
      .toBe(true);
    expect(alphaSettingsAreDirty({ ...defaults, maxRowsToMaterialize: " " }))
      .toBe(false);
    for (const cap of ["-1", "1.5", "Infinity", "invalid", "9007199254740992"])
      expect(() => alphaOptionsFromForm({ ...defaults, maxRowsToMaterialize: cap }))
        .toThrow(/Alpha event cap/);
    for (const cap of ["-1", "1.5", "Infinity", "invalid", "9007199254740992"])
      expect(alphaValidationMessage({ ...defaults, maxRowsToMaterialize: cap }))
        .toMatch(/Alpha event cap/);
    for (const cap of [undefined, "", " ", "0", "25"])
      expect(alphaValidationMessage({ ...defaults, maxRowsToMaterialize: cap }))
        .toBeNull();
  });
  it("elides defaults and trims a custom activity field", () => {
    expect(alphaOptionsFromForm({
      activityAttribute: "concept:name",
      removeUnconnected: false,
    })).toEqual({
      alphaActivityAttribute: undefined,
      alphaRemoveUnconnected: undefined,
    });
    expect(alphaOptionsFromForm({
      activityAttribute: " phase ",
      removeUnconnected: true,
    })).toEqual({
      alphaActivityAttribute: "phase",
      alphaRemoveUnconnected: true,
    });
  });

  it("marks only effective Alpha changes active", () => {
    expect(alphaSettingsAreDirty({
      activityAttribute: " ", removeUnconnected: false,
    })).toBe(false);
    expect(alphaSettingsAreDirty({
      activityAttribute: "phase", removeUnconnected: false,
    })).toBe(true);
    expect(alphaSettingsAreDirty({
      activityAttribute: "concept:name", removeUnconnected: true,
    })).toBe(true);
  });
});

describe("frequency filter validation", () => {
  it("accepts non-negative top limits and relative fractions", () => {
    expect(topActivitiesValidationMessage({ max: "0", attributeKey: "" })).toBeNull();
    expect(topActivitiesValidationMessage({ max: "3", attributeKey: "channel" })).toBeNull();
    expect(relativeAttributeValidationMessage({ key: "", stake: "0.5", keepOncePerCase: true })).toBeNull();
    expect(relativeAttributeValidationMessage({ key: "channel", stake: "", keepOncePerCase: false })).toBeNull();
  });

  it("rejects a missing top limit and out-of-range stakes", () => {
    expect(topActivitiesValidationMessage({ max: "", attributeKey: "channel" })).toContain("maximum");
    expect(topActivitiesValidationMessage({ max: "1.5", attributeKey: "" })).toContain("integer");
    expect(relativeAttributeValidationMessage({ key: "channel", stake: "1.1", keepOncePerCase: true })).toContain("[0, 1]");
    expect(relativeAttributeValidationMessage({ key: "channel", stake: "nan", keepOncePerCase: true })).toContain("finite");
  });

  it("validates per-variant coverage and percentage targets", () => {
    expect(variantCoverageValidationMessage({ min: "0", max: "100", percentage: "0" })).toBeNull();
    expect(variantCoverageValidationMessage({ min: "60", max: "50", percentage: "" })).toContain("must not exceed");
    expect(variantCoverageValidationMessage({ min: "", max: "101", percentage: "" })).toContain("[0, 100]");
  });

  it("validates absolute occurrence thresholds", () => {
    expect(attributeOccurrenceValidationMessage({ key: "", min: "0" })).toBeNull();
    expect(attributeOccurrenceValidationMessage({ key: "channel", min: "" })).toContain("minimum");
    expect(attributeOccurrenceValidationMessage({ key: "", min: "1.5" })).toContain("integer");
  });
});
