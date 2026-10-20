import {ExactLiteralInput} from "./OriginalLtlFilter";
import {LTL_COMPLETE_EXAMPLE,HEURISTIC_COMPLETE_EXAMPLE,literalFilterEditor,filterEditorControl,exactFilterFields,type ExactFilterControls} from "../exact-filter-controls";
import {DiscoveryVectorInput} from "./DiscoveryVectorInput";
import {parseDiscoveryStringVector, discoveryStringVectorValidation, type DiscoveryVectorMode} from "../discovery-string-vectors";
import { useState } from "react";
import {
  type AttributeFilterMode,
  type FilterOptions,
  type LtlVariant,
  type MinerOptions,
  type TimeFilterMode,
} from "../api";
import {
  DECLARE_TEMPLATES,
  FOOTPRINTS_CONFORMANCE_VARIANTS,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
  FOOTPRINTS_DISCOVERY_VARIANTS,
  type FootprintsConformanceVariant,
  type FootprintsDiscoveryVariant,
} from "../types";
import { type CaseAttributeStat, type EventAttributeStat } from "../types";

export const TOKEN_REPLAY_POLICY_DEFAULTS = {
  tokenReplayConsiderRemainingInFitness: true,
  tokenReplayTryToReachFinalMarkingThroughHidden: true,
  tokenReplayConsiderActivitiesNotInModelInFitness: false,
  tokenReplayCleaningTokenFlood: false,
  tokenReplayExhaustiveInvisibleExploration: false,
  tokenReplayWalkThroughHiddenTransitions: true,
  tokenReplayIncludeTraceDiagnostics: false,
  tokenReplayIncludeDurationDiagnostics: false,
  tokenReplayIncludeRootCauseDiagnostics: false,
};

type TokenReplayPolicies = typeof TOKEN_REPLAY_POLICY_DEFAULTS;

export function tokenReplayInputValidationMessage(
  cap: string, timestampAttribute = "time:timestamp", includeDurationDiagnostics = false,
): string | null {
  const value = cap.trim();
  if (value && (!Number.isSafeInteger(Number(value)) || Number(value) < 0))
    return "Token replay event cap must be a non-negative safe integer.";
  if (includeDurationDiagnostics && !timestampAttribute.trim())
    return "Token replay timestamp attribute must not be empty.";
  return null;
}

export function tokenReplayInputOptionsFromForm(
  cap: string, timestampAttribute = "time:timestamp", includeDurationDiagnostics = false,
): MinerOptions {
  const error = tokenReplayInputValidationMessage(cap, timestampAttribute, includeDurationDiagnostics);
  if (error) throw new Error(error);
  return {
    ...(cap.trim() ? { tokenReplayMaxRowsToMaterialize: Number(cap) } : {}),
    ...(includeDurationDiagnostics && timestampAttribute.trim() !== "time:timestamp"
      ? { tokenReplayTimestampAttribute: timestampAttribute.trim() } : {}),
  };
}

export function tokenReplayPolicyOptionsFromForm(input: TokenReplayPolicies): MinerOptions {
  return Object.fromEntries(
    (Object.keys(TOKEN_REPLAY_POLICY_DEFAULTS) as (keyof TokenReplayPolicies)[])
      .filter((key) => input[key] !== TOKEN_REPLAY_POLICY_DEFAULTS[key])
      .map((key) => [key, input[key]]),
  );
}

export function tokenReplayPoliciesAreDirty(input: TokenReplayPolicies): boolean {
  return Object.keys(tokenReplayPolicyOptionsFromForm(input)).length > 0;
}

export function TokenReplayPolicyControls({ variant, policies, onChange }: {
  variant: "forward" | "backwards";
  policies: TokenReplayPolicies;
  onChange: (key: keyof TokenReplayPolicies, value: boolean) => void;
}) {
  const backwards = variant === "backwards";
  return <>
    {backwards && <p className="conf-inline-note">Final-state fit classification, unknown-activity classification, final-marking routing, token-flood cleaning, and exhaustive hidden exploration policies apply only to forward replay and are ignored in backwards replay. Their settings are retained when switching back.</p>}
    <div className="filters-row">
      {([
        ["tokenReplayConsiderRemainingInFitness", "Require no remaining tokens for a fitting trace", true],
        ["tokenReplayTryToReachFinalMarkingThroughHidden", "Reach final marking through hidden transitions", true],
        ["tokenReplayConsiderActivitiesNotInModelInFitness", "Treat unknown activities as an unfit trace", true],
        ["tokenReplayWalkThroughHiddenTransitions", "Walk through hidden transitions", false],
        ["tokenReplayIncludeTraceDiagnostics", "Include per-trace replay diagnostics", false],
        ["tokenReplayIncludeDurationDiagnostics", "Include replay duration diagnostics", false],
        ["tokenReplayIncludeRootCauseDiagnostics", "Include replay root-cause diagnostics", false],
      ] as const).map(([key, label, forwardOnly]) => (
        <label className="filters-field" key={key}>
          <span>{label}</span>
          <select value={String(policies[key])} disabled={backwards && forwardOnly}
            onChange={(event) => onChange(key, event.target.value === "true")}>
            <option value="true">yes{TOKEN_REPLAY_POLICY_DEFAULTS[key] ? " (default)" : ""}</option>
            <option value="false">no{!TOKEN_REPLAY_POLICY_DEFAULTS[key] ? " (default)" : ""}</option>
          </select>
        </label>
      ))}
      <label className="filters-field">
        <span>Clean token flooding during replay</span>
        <input type="checkbox" checked={policies.tokenReplayCleaningTokenFlood} disabled={backwards}
          onChange={(event) => onChange("tokenReplayCleaningTokenFlood", event.target.checked)} />
      </label>
      <label className="filters-field">
        <span>Exhaustive hidden exploration</span>
        <input type="checkbox" checked={policies.tokenReplayExhaustiveInvisibleExploration} disabled={backwards}
          onChange={(event) => onChange("tokenReplayExhaustiveInvisibleExploration", event.target.checked)} />
      </label>
    </div>
  </>;
}

export function tokenReplayRootCauseOptionsFromForm(
  enabled: boolean, strings: string, numerics: string, multiplier = false,
): MinerOptions {
  if (!enabled) return {};
  const parse = (value: string) => {
    if (!value.trim()) return [];
    const names = value.split(",").map((name) => name.trim());
    if (names.some((name) => !name || /[\r\n]/.test(name)))
      throw new Error("Root-cause attributes must be non-empty comma-separated names.");
    return names;
  };
  const stringAttributes = parse(strings);
  const numericAttributes = parse(numerics);
  if (!stringAttributes.length && !numericAttributes.length)
    throw new Error("Root-cause diagnostics require at least one string or numeric attribute.");
  return {
    tokenReplayRootCauseStringAttributes: stringAttributes,
    tokenReplayRootCauseNumericAttributes: numericAttributes,
    tokenReplayRootCauseEnableMultiplier: multiplier,
  };
}

export function TokenReplayRootCauseControls({ enabled, strings, numerics, multiplier,
  onStrings, onNumerics, onMultiplier }: {
  enabled: boolean; strings: string; numerics: string; multiplier: boolean;
  onStrings: (value: string) => void; onNumerics: (value: string) => void;
  onMultiplier: (value: boolean) => void;
}) {
  return <div className="filters-row">
    <label className="filters-field"><span>Root-cause string attributes <small>(comma-separated)</small></span>
      <input disabled={!enabled} value={strings} onChange={(event) => onStrings(event.target.value)} /></label>
    <label className="filters-field"><span>Root-cause numeric attributes <small>(comma-separated)</small></span>
      <input disabled={!enabled} value={numerics} onChange={(event) => onNumerics(event.target.value)} /></label>
    <label className="filters-field"><span>Balance fitting and problem samples</span>
      <select disabled={!enabled} value={String(multiplier)} onChange={(event) => onMultiplier(event.target.value === "true")}>
        <option value="true">yes</option><option value="false">no (default)</option>
      </select></label>
  </div>;
}

interface FiltersProps {
  /** Activity names from the original, unfiltered log. */
  activities: string[];
  /** Resource names from the original log; empty when the log has no resources. */
  resources: string[];
  /** Lifecycle transitions of the original log; empty for atomic events. */
  lifecycles: string[];
  /** Generic event attributes of the original log; empty when it has none. */
  attributes: EventAttributeStat[];
  /** Case attributes of the original log; empty when it has none. */
  caseAttributes: CaseAttributeStat[];
  /** Whether any filter or miner setting is applied (drives the badge). */
  active: boolean;
  mining: boolean;
  onApply: (options: FilterOptions & MinerOptions) => void;
  onExactEdit?: () => void;
}

/** Validation for temporal controls that are only relevant once enabled. */
export function temporalValidationMessage(input: {
  spectrumConfigured: boolean;
  spectrumActivityCount: number;
  spectrumHasEmptyActivity?: boolean;
  spectrumSampleSize: string;
  batches: boolean;
  batchVariant?: "log" | "pandas" | "polars";
  batchMergeDistanceSeconds: string;
  batchMinSize: string;
  transitionActivityKey?: string;
  transitionCaseIdKey?: string;
  transitionView?: "sequence" | "set" | "multiset";
  transitionDirection?: "forward" | "backward";
  transitionWindow?: string;
  batchEventIdAttribute?: string;
  overlap?: boolean;
  overlapEpsilon?: string;
}): string | null {
  if (input.spectrumHasEmptyActivity)
    return "Performance spectrum activities must not contain empty entries.";
  if (input.spectrumActivityCount === 1)
    return "Spectrum needs at least two activities.";
  if (input.spectrumConfigured) {
    const raw = input.spectrumSampleSize.trim();
    const value = Number(raw);
    if (!raw || !Number.isFinite(value) || !Number.isSafeInteger(value) || value < 0)
      return "Spectrum sample size must be a non-negative integer.";
  }
  if (input.batches) {
    const mergeRaw = input.batchMergeDistanceSeconds.trim();
    const merge = Number(mergeRaw);
    if (!mergeRaw || !Number.isFinite(merge) || merge < 0)
      return "Batch merge distance must be a finite, non-negative number.";
    const minRaw = input.batchMinSize.trim();
    const min = Number(minRaw);
    if (!minRaw || !Number.isFinite(min) || !Number.isInteger(min) || min < 2)
      return "Batch minimum size must be an integer of at least 2.";
  }
  if (input.transitionActivityKey !== undefined &&
      !input.transitionActivityKey.trim())
    return "Transition-system activity selector must not be empty.";
  if (input.transitionCaseIdKey !== undefined &&
      !input.transitionCaseIdKey.trim())
    return "Transition-system case-id selector must not be empty.";
  if (input.transitionView !== undefined &&
      !["sequence", "set", "multiset"].includes(input.transitionView))
    return "Transition-system view is not supported.";
  if (input.transitionDirection !== undefined &&
      !["forward", "backward"].includes(input.transitionDirection))
    return "Transition-system direction is not supported.";
  if (input.transitionWindow !== undefined) {
    const raw = input.transitionWindow.trim();
    const value = Number(raw);
    if (!raw || !Number.isSafeInteger(value) || value < 0)
      return "Transition-system window must be a non-negative integer.";
  }
  if (input.batches && input.batchVariant !== undefined && input.batchVariant !== "log" &&
      input.batchEventIdAttribute !== undefined &&
      !input.batchEventIdAttribute.trim())
    return "Batch event-id selector must not be empty.";
  if (input.overlap) {
    const rawEpsilon = input.overlapEpsilon?.trim() ?? "";
    const epsilon = Number(rawEpsilon);
    if (!rawEpsilon || !Number.isFinite(epsilon) || epsilon < 0)
      return "Overlap epsilon must be a finite, non-negative number.";
  }
  return null;
}

/** Correlation selectors require names once at least one variant is enabled. */
export function correlationValidationMessage(input: {
  enabled: boolean;
  activityAttribute: string;
  timestampAttribute: string;
  startTimestampAttribute: string;
  caseIdAttribute: string;
  traceEnabled: boolean;
  splitEnabled: boolean;
  sampleSize: string;
}): string | null {
  if (!input.enabled) return null;
  if (!input.activityAttribute.trim())
    return "Correlation activity attribute must not be empty.";
  if (!input.timestampAttribute.trim())
    return "Correlation timestamp attribute must not be empty.";
  if (input.traceEnabled && !input.caseIdAttribute.trim())
    return "Correlation case-id attribute must not be empty.";
  if (input.splitEnabled) {
    const raw = input.sampleSize.trim();
    const value = Number(raw);
    if (!raw || !Number.isSafeInteger(value) || value <= 0)
      return "Correlation split sample size must be a positive safe integer.";
  }
  // An empty start selector intentionally requests the completion selector.
  return null;
}

/** Segment endpoints are a pair: a half-configured extraction is invalid. */
export function segmentValidationMessage(input: {
  betweenStart: string;
  betweenEnd: string;
}): string | null {
  if ((input.betweenStart === "") !== (input.betweenEnd === "")) {
    return "Segment-between filtering needs both a start and an end activity.";
  }
  return null;
}

/** Adjacent path bounds use zero as the implicit lower bound. */
export function pathValidationMessage(input: {
  predecessor: string;
  successor: string;
  minPerformance: string;
  maxPerformance: string;
  positive: boolean;
  activityAttribute?: string;
  timestampAttribute?: string;
}): string | null {
  const configured = input.predecessor || input.successor ||
    input.activityAttribute?.trim() || input.timestampAttribute?.trim() ||
    input.minPerformance.trim() || input.maxPerformance.trim() || !input.positive;
  if (!configured) return null;
  if (!input.predecessor || !input.successor)
    return "Path filtering needs both predecessor and successor activities.";
  const min = input.minPerformance.trim() ? Number(input.minPerformance) : undefined;
  const max = input.maxPerformance.trim() ? Number(input.maxPerformance) : undefined;
  if ((min !== undefined && !Number.isFinite(min)) ||
      (max !== undefined && !Number.isFinite(max)))
    return "Path performance bounds must be finite numbers.";
  if (max !== undefined && (min ?? 0) > max)
    return "Path minimum performance must not exceed the maximum.";
  return null;
}

/** Complete-case duration bounds use zero as the implicit lower bound. */
export function caseDurationValidationMessage(input: {
  min: string;
  max: string;
}): string | null {
  if (!input.min.trim() && !input.max.trim()) return null;
  const min = input.min.trim() ? Number(input.min) : 0;
  const max = input.max.trim() ? Number(input.max) : undefined;
  if (!Number.isFinite(min) || (max !== undefined && !Number.isFinite(max)))
    return "Case duration bounds must be finite numbers.";
  if (max !== undefined && (min > max || max < 0))
    return "Case duration minimum must not exceed a non-negative maximum.";
  return null;
}

/** Case-count and event-count bounds are non-negative integers. */
export function caseSelectionValidationMessage(input: {
  maxCases: string;
  minLength: string;
  maxLength: string;
}): string | null {
  const values = [
    ["Maximum cases", input.maxCases],
    ["Minimum events per case", input.minLength],
    ["Maximum events per case", input.maxLength],
  ] as const;
  for (const [label, raw] of values) {
    if (!raw.trim()) continue;
    const value = Number(raw);
    if (!Number.isFinite(value) || !Number.isInteger(value) || value < 0)
      return `${label} must be a non-negative integer.`;
  }
  const min = input.minLength.trim() ? Number(input.minLength) : 0;
  const max = input.maxLength.trim() ? Number(input.maxLength) : 0;
  if (max !== 0 && min > max)
    return "Minimum events per case must not exceed the maximum.";
  return null;
}

/** Numeric attribute ranges need a key and at least one finite bound. */
export function numericAttributeValidationMessage(input: {
  key: string;
  min: string;
  max: string;
}): string | null {
  const configured = input.key || input.min.trim() || input.max.trim();
  if (!configured) return null;
  if (!input.key) return "Numeric filtering needs an attribute key.";
  if (!input.min.trim() && !input.max.trim())
    return "Numeric filtering needs a minimum or maximum bound.";
  const min = input.min.trim() ? Number(input.min) : undefined;
  const max = input.max.trim() ? Number(input.max) : undefined;
  if ((min !== undefined && !Number.isFinite(min)) ||
      (max !== undefined && !Number.isFinite(max)))
    return "Numeric attribute bounds must be finite numbers.";
  if (min !== undefined && max !== undefined && min > max)
    return "Numeric attribute minimum must not exceed the maximum.";
  return null;
}

/** Top-activity limits are optional, but their limit must be an integer. */
export function topActivitiesValidationMessage(input: {
  max: string;
  attributeKey: string;
}): string | null {
  if (!input.max.trim() && !input.attributeKey.trim()) return null;
  if (!input.max.trim()) return "Top-activity filtering needs a maximum count.";
  const value = Number(input.max);
  if (!Number.isFinite(value) || !Number.isInteger(value) || value < 0)
    return "Top activities must be a non-negative integer.";
  return null;
}

/** Relative occurrence stakes are inclusive fractions in [0, 1]. */
export function relativeAttributeValidationMessage(input: {
  key: string;
  stake: string;
  keepOncePerCase: boolean;
}): string | null {
  const configured = input.key.trim() || input.stake.trim() || !input.keepOncePerCase;
  if (!configured) return null;
  if (!input.stake.trim()) return null;
  const value = Number(input.stake);
  if (!Number.isFinite(value) || value < 0 || value > 1)
    return "Relative occurrence stake must be finite and in [0, 1].";
  return null;
}

/** Absolute occurrence thresholds need a non-negative integer count. */
export function attributeOccurrenceValidationMessage(input: {
  key: string;
  min: string;
}): string | null {
  if (!input.key.trim() && !input.min.trim()) return null;
  if (!input.min.trim())
    return "Attribute occurrence filtering needs a minimum count.";
  const value = Number(input.min);
  if (!Number.isFinite(value) || !Number.isInteger(value) || value < 0)
    return "Attribute occurrence minimum must be a non-negative integer.";
  return null;
}

/** Variant share controls are entered as percentages in the UI. */
export function variantCoverageValidationMessage(input: {
  min: string;
  max: string;
  percentage: string;
}): string | null {
  const fields = [
    ["Minimum variant coverage", input.min],
    ["Maximum variant coverage", input.max],
    ["Variant percentage", input.percentage],
  ] as const;
  for (const [label, raw] of fields) {
    if (!raw.trim()) continue;
    const value = Number(raw);
    if (!Number.isFinite(value) || value < 0 || value > 100)
      return `${label} must be a finite percentage in [0, 100].`;
  }
  if (input.min.trim() && input.max.trim() &&
      Number(input.min) > Number(input.max))
    return "Minimum variant coverage must not exceed the maximum.";
  return null;
}

/** Repetition bounds are inclusive and default to [2, +infinity]. */
export function repetitionValidationMessage(input: {
  value: string;
  min: string;
  max: string;
}): string | null {
  if (!input.value.trim()) return null;
  const min = input.min.trim() ? Number(input.min) : 2;
  const max = input.max.trim() ? Number(input.max) : Number.MAX_SAFE_INTEGER;
  if (!Number.isFinite(min) || !Number.isInteger(min) || min < 0)
    return "Repetition minimum must be a non-negative integer.";
  if (!Number.isFinite(max) || !Number.isInteger(max) || max < 0)
    return "Repetition maximum must be a non-negative integer.";
  if (min > max) return "Repetition minimum must not exceed the maximum.";
  return null;
}

/** Concatenation needs a non-empty separator at the request boundary. */
export function caseGroupingValidationMessage(input: {
  timestampGrouping: "" | "first" | "last" | "concat";
  timestampGroupingSeparator: string;
}): string | null {
  if (input.timestampGrouping === "concat" &&
      input.timestampGroupingSeparator.length === 0)
    return "Timestamp-group concatenation needs a non-empty separator.";
  return null;
}

/** POWL numeric bounds and selectors are validated before defaults are elided. */
export function powlValidationMessage(input: {
  filteringThreshold: string;
  orderFrequencyRatio: string;
  bruteForceActivityLimit: string;
  activityKey: string;
  timestampKey: string;
  caseIdKey: string;
}): string | null {
  const thresholdText = input.filteringThreshold.trim();
  if (thresholdText) {
    const threshold = Number(thresholdText);
    if (!Number.isFinite(threshold) || threshold < 0 || threshold >= 1)
      return "POWL filter threshold must be finite and in [0, 1).";
  }
  const ratioText = input.orderFrequencyRatio.trim();
  if (ratioText) {
    const ratio = Number(ratioText);
    if (!Number.isFinite(ratio) || ratio <= 0.5 || ratio > 1)
      return "POWL order-frequency ratio must be finite and in (0.5, 1].";
  }
  const limitText = input.bruteForceActivityLimit.trim();
  if (limitText) {
    const limit = Number(limitText);
    if (!Number.isFinite(limit) || !Number.isInteger(limit) || limit < 0)
      return "POWL brute-force activity limit must be a non-negative integer.";
  }
  if (!input.activityKey.trim()) return "POWL activity field must not be blank.";
  if (!input.timestampKey.trim()) return "POWL timestamp field must not be blank.";
  if (!input.caseIdKey.trim()) return "POWL case-id field must not be blank.";
  return null;
}

/** Footprint selectors are shared by log and table adapters. */
export function footprintsValidationMessage(input: {
  activityAttribute: string;
  caseIdAttribute: string;
  timestampAttribute: string;
  startTimestampAttribute: string;
  indexAttribute: string;
  initialMarking: string;
}): string | null {
  if (!input.activityAttribute.trim())
    return "Footprints activity field must not be blank.";
  if (!input.caseIdAttribute.trim())
    return "Footprints case-id field must not be blank.";
  if (!input.timestampAttribute.trim())
    return "Footprints completion field must not be blank.";
  const marking = input.initialMarking.trim();
  if (!marking) return null;
  const maximum = 18446744073709551615n;
  for (const raw of marking.split(",")) {
    const token = raw.trim();
    if (!/^\d+$/.test(token))
      return "Footprints initial marking needs non-negative uint64 entries.";
    if (BigInt(token) > maximum)
      return "Footprints initial marking entries must fit uint64.";
  }
  return null;
}

export interface InductiveSettingsForm {
  disableStrictSequenceCut: boolean;
  disableFallthroughs: boolean;
  multiprocessing: boolean;
  sortEventsByTimestamp: boolean;
  activityKey: string;
  timestampKey: string;
  caseIdKey: string;
}

/** Converts the visible Inductive controls while eliding backend defaults. */
export function inductiveOptionsFromForm(
  input: InductiveSettingsForm,
): Pick<MinerOptions,
  | "inductiveDisableStrictSequenceCut"
  | "inductiveDisableFallthroughs"
  | "inductiveMultiprocessing"
  | "inductiveSortEventsByTimestamp"
  | "inductiveActivityKey"
  | "inductiveTimestampKey"
  | "inductiveCaseIdKey"> {
  return {
    inductiveDisableStrictSequenceCut:
      input.disableStrictSequenceCut || undefined,
    inductiveDisableFallthroughs: input.disableFallthroughs || undefined,
    inductiveMultiprocessing: input.multiprocessing || undefined,
    inductiveSortEventsByTimestamp: input.sortEventsByTimestamp || undefined,
    inductiveActivityKey:
      input.activityKey.trim() !== "concept:name"
        ? input.activityKey.trim() || undefined
        : undefined,
    inductiveTimestampKey:
      input.timestampKey.trim() !== "time:timestamp"
        ? input.timestampKey.trim() || undefined
        : undefined,
    inductiveCaseIdKey:
      input.caseIdKey.trim() !== "case:concept:name"
        ? input.caseIdKey.trim() || undefined
        : undefined,
  };
}

/** Reports whether an Inductive control differs from its effective default. */
export function inductiveSettingsAreDirty(input: InductiveSettingsForm): boolean {
  return input.disableStrictSequenceCut ||
    input.disableFallthroughs ||
    input.multiprocessing ||
    input.sortEventsByTimestamp ||
    (input.activityKey.trim() || "concept:name") !== "concept:name" ||
    (input.timestampKey.trim() || "time:timestamp") !== "time:timestamp" ||
    (input.caseIdKey.trim() || "case:concept:name") !== "case:concept:name";
}

export interface AlphaSettingsForm {
  activityAttribute: string;
  removeUnconnected: boolean;
  maxRowsToMaterialize?: string;
}

export function alphaValidationMessage(input: AlphaSettingsForm): string | null {
  const text = input.maxRowsToMaterialize?.trim();
  if (!text) return null;
  const cap = Number(text);
  return !Number.isSafeInteger(cap) || cap < 0
    ? "Alpha event cap must be a non-negative safe integer."
    : null;
}

/** Converts the visible Alpha controls while eliding backend defaults. */
export function alphaOptionsFromForm(
  input: AlphaSettingsForm,
): Pick<MinerOptions, "alphaActivityAttribute" | "alphaRemoveUnconnected" | "alphaMaxRowsToMaterialize"> {
  const cap = input.maxRowsToMaterialize?.trim();
  const parsedCap = cap ? Number(cap) : undefined;
  const validationError = alphaValidationMessage(input);
  if (validationError) throw new Error(validationError);
  return {
    alphaActivityAttribute:
      input.activityAttribute.trim() !== "concept:name"
        ? input.activityAttribute.trim() || undefined
        : undefined,
    alphaRemoveUnconnected: input.removeUnconnected || undefined,
    ...(parsedCap === undefined ? {} : { alphaMaxRowsToMaterialize: parsedCap }),
  };
}

/** Reports whether an Alpha control differs from its effective default. */
export function alphaSettingsAreDirty(input: AlphaSettingsForm): boolean {
  return input.removeUnconnected ||
    Boolean(input.maxRowsToMaterialize?.trim()) ||
    (input.activityAttribute.trim() || "concept:name") !== "concept:name";
}

/** Converts the optional heuristic interval-start selector from the form. */
export function heuristicStartTimestampOptionsFromForm(
  value: string,
): Pick<MinerOptions, "heuristicStartTimestampAttribute"> {
  const normalized = value.trim();
  return normalized
    ? { heuristicStartTimestampAttribute: normalized }
    : {};
}

/** Returns the conventional automatic selector shown for each heuristic variant. */
export function heuristicStartTimestampPlaceholder(
  variant: "classic" | "plusplus",
  completionTimestamp = "time:timestamp",
): string {
  if (variant === "plusplus") return "start_timestamp";
  return completionTimestamp.trim() || "time:timestamp";
}

/** Whether heuristic business-hour controls affect the selected variant/decor. */
export function heuristicBusinessHoursOptionsAreActive(
  variant: "classic" | "plusplus",
  decoration: "frequency" | "performance",
): boolean {
  return variant === "plusplus" || decoration === "performance";
}

/** Validates the optional weekly schedule while the business-hours control is active. */
export function heuristicBusinessHourSlotsValidationMessage(
  value: string,
  enabled: boolean,
): string | null {
  const slots = value.trim();
  if (!enabled || !slots || slots.toLowerCase() === "none") return null;
  const valid = slots.split(",").every((slot) => {
    const match = slot.trim().match(/^(\d+(?:\.\d+)?)-(\d+(?:\.\d+)?)$/);
    if (!match) return false;
    const start = Number(match[1]);
    const end = Number(match[2]);
    return Number.isFinite(start) && Number.isFinite(end) &&
      start >= 0 && start < end && end <= 604800;
  });
  return valid
    ? null
    : "Heuristic business-hour slots must be `none` or start-end pairs within one week.";
}

// A compact, collapsible bar for the log filters and the miner settings.
// Every apply re-mines the original upload with the full spec, so nothing
// compounds and every setting can always be widened again.
export function Filters({
  activities,
  resources,
  lifecycles,
  attributes,
  caseAttributes,
  active,
  mining,
  onApply,
  onExactEdit,
}: FiltersProps) {
  const [open, setOpen] = useState(false);
  const [topVariants, setTopVariants] = useState("");
  const [maxActivities, setMaxActivities] = useState("");
  const [topActivitiesAttributeKey, setTopActivitiesAttributeKey] = useState("");
  const [minFrequency, setMinFrequency] = useState("");
  const [variantMinCoverage, setVariantMinCoverage] = useState("");
  const [variantMaxCoverage, setVariantMaxCoverage] = useState("");
  const [variantPercentage, setVariantPercentage] = useState("");
  const [startActivities, setStartActivities] = useState("");
  const [endActivities, setEndActivities] = useState("");
  const [startActivitiesPositive, setStartActivitiesPositive] = useState(true);
  const [endActivitiesPositive, setEndActivitiesPositive] = useState(true);
  const [timeFrom, setTimeFrom] = useState("");
  const [timeTo, setTimeTo] = useState("");
  const [timeMode, setTimeMode] = useState<TimeFilterMode>("events");
  const [caseDurationMin, setCaseDurationMin] = useState("");
  const [caseDurationMax, setCaseDurationMax] = useState("");
  const [maxCases, setMaxCases] = useState("");
  const [minLength, setMinLength] = useState("");
  const [maxLength, setMaxLength] = useState("");
  const [reworkActivity, setReworkActivity] = useState("");
  const [reworkMin, setReworkMin] = useState("");
  const [betweenStart, setBetweenStart] = useState("");
  const [betweenEnd, setBetweenEnd] = useState("");
  const [activitySplitActivity, setActivitySplitActivity] = useState("");
  const [activitySplitSeparator, setActivitySplitSeparator] = useState("##@@");
  const [activitySplitCutMode, setActivitySplitCutMode] = useState<"this" | "next">("this");
  const [repetitionValue, setRepetitionValue] = useState("");
  const [repetitionAttributeKey, setRepetitionAttributeKey] = useState("");
  const [repetitionMin, setRepetitionMin] = useState("");
  const [repetitionMax, setRepetitionMax] = useState("");
  const [consecutiveGrouping, setConsecutiveGrouping] = useState<"" | "first" | "last">("");
  const [consecutiveGroupingActivityKey, setConsecutiveGroupingActivityKey] = useState("");
  const [timestampGrouping, setTimestampGrouping] = useState<"" | "first" | "last" | "concat">("");
  const [timestampGroupingActivityKey, setTimestampGroupingActivityKey] = useState("");
  const [timestampGroupingTimestampKey, setTimestampGroupingTimestampKey] = useState("");
  const [timestampGroupingSeparator, setTimestampGroupingSeparator] = useState(" & ");
  const [variantCoverage, setVariantCoverage] = useState(""); // percent, 1–100
  const [prefixActivity, setPrefixActivity] = useState("");
  const [suffixActivity, setSuffixActivity] = useState("");
  const [prefixStrict, setPrefixStrict] = useState(true);
  const [prefixOccurrence, setPrefixOccurrence] =
    useState<"first" | "last">("first");
  const [suffixStrict, setSuffixStrict] = useState(true);
  const [suffixOccurrence, setSuffixOccurrence] =
    useState<"first" | "last">("first");
  const [segmentPrefixActivity, setSegmentPrefixActivity] = useState("");
  const [segmentPrefixStrict, setSegmentPrefixStrict] = useState(true);
  const [segmentPrefixOccurrence, setSegmentPrefixOccurrence] = useState<"first" | "last">("first");
  const [segmentSuffixActivity, setSegmentSuffixActivity] = useState("");
  const [segmentSuffixStrict, setSegmentSuffixStrict] = useState(true);
  const [segmentSuffixOccurrence, setSegmentSuffixOccurrence] = useState<"first" | "last">("first");
  const [segmentBetweenStart, setSegmentBetweenStart] = useState("");
  const [segmentBetweenEnd, setSegmentBetweenEnd] = useState("");
  const [pathPredecessor, setPathPredecessor] = useState("");
  const [pathSuccessor, setPathSuccessor] = useState("");
  const [pathActivityAttribute, setPathActivityAttribute] = useState("");
  const [pathTimestampAttribute, setPathTimestampAttribute] = useState("");
  const [pathMinPerformance, setPathMinPerformance] = useState("");
  const [pathMaxPerformance, setPathMaxPerformance] = useState("");
  const [pathPositive, setPathPositive] = useState(true);
  // One succession constraint: keep or drop cases where `from` is directly /
  // eventually followed by `to` (the CLI accepts several pairs; the UI one).
  const [successionAction, setSuccessionAction] = useState<
    "off" | "keep" | "remove"
  >("off");
  const [successionRelation, setSuccessionRelation] = useState<
    "directly" | "eventually"
  >("directly");
  const [successionFrom, setSuccessionFrom] = useState("");
  const [successionTo, setSuccessionTo] = useState("");
  const [tracePattern, setTracePattern] = useState("");
  const [tracePatternPositive, setTracePatternPositive] = useState(true);
  const [admittedVariants, setAdmittedVariants] = useState("");
  const [admittedVariantsPositive, setAdmittedVariantsPositive] = useState(true);
  // Activities and resources start all-selected; deselected ones are removed.
  const [removed, setRemoved] = useState<Set<string>>(new Set());
  const [removedResources, setRemovedResources] = useState<Set<string>>(
    new Set(),
  );
  const [removedLifecycles, setRemovedLifecycles] = useState<Set<string>>(
    new Set(),
  );
  // Attribute filter: one key, its dropped values, and whether dropping acts
  // on events or whole cases. Values start all-selected, like the chips above.
  const [attributeKey, setAttributeKey] = useState("");
  const [removedValues, setRemovedValues] = useState<Set<string>>(new Set());
  const [attributeMode, setAttributeMode] =
    useState<AttributeFilterMode>("events");
  const [numericAttributeKey, setNumericAttributeKey] = useState("");
  const [numericAttributeMin, setNumericAttributeMin] = useState("");
  const [numericAttributeMax, setNumericAttributeMax] = useState("");
  const [numericAttributeMode, setNumericAttributeMode] =
    useState<AttributeFilterMode>("events");
  const [numericAttributePositive, setNumericAttributePositive] =
    useState(true);
  const [relativeAttributeKey, setRelativeAttributeKey] = useState("");
  const [relativeAttributeStake, setRelativeAttributeStake] = useState("");
  const [relativeAttributeKeepOncePerCase, setRelativeAttributeKeepOncePerCase] =
    useState(true);
  const [attributeOccurrenceKey, setAttributeOccurrenceKey] = useState("");
  const [attributeOccurrenceMin, setAttributeOccurrenceMin] = useState("");
  // Case-attribute filter: one key and its dropped values. Dropping a value
  // always removes the whole case — the attribute lives on the case itself.
  const [caseAttributeKey, setCaseAttributeKey] = useState("");
  const [removedCaseValues, setRemovedCaseValues] = useState<Set<string>>(
    new Set(),
  );
  // Miner settings; an empty field leaves the backend default in charge.
  const [noiseThreshold, setNoiseThreshold] = useState("");
  const [inductiveVariant, setInductiveVariant] = useState<"log" | "imf" | "dfg">("log");
  const [inductiveDisableStrictSequenceCut, setInductiveDisableStrictSequenceCut] = useState(false);
  const [inductiveDisableFallthroughs, setInductiveDisableFallthroughs] = useState(false);
  const [inductiveMultiprocessing, setInductiveMultiprocessing] = useState(false);
  const [inductiveSortEventsByTimestamp, setInductiveSortEventsByTimestamp] = useState(false);
  const [inductiveActivityKey, setInductiveActivityKey] = useState("concept:name");
  const [inductiveTimestampKey, setInductiveTimestampKey] = useState("time:timestamp");
  const [inductiveCaseIdKey, setInductiveCaseIdKey] = useState("case:concept:name");
  const [alphaVariant, setAlphaVariant] = useState<"plus" | "classic">("classic");
  const [alphaActivityAttribute, setAlphaActivityAttribute] =
    useState("concept:name");
  const [alphaRemoveUnconnected, setAlphaRemoveUnconnected] = useState(false);
  const [alphaMaxRowsToMaterialize, setAlphaMaxRowsToMaterialize] = useState("");
  const [heuristicVariant, setHeuristicVariant] =
    useState<"classic" | "plusplus">("classic");
  const [heuristicActivityAttribute, setHeuristicActivityAttribute] =
    useState("concept:name");
  const [heuristicTimestampAttribute, setHeuristicTimestampAttribute] =
    useState("time:timestamp");
  const [dependencyThreshold, setDependencyThreshold] = useState("");
  const [andThreshold, setAndThreshold] = useState("");
  const [minEdgeFrequency, setMinEdgeFrequency] = useState("");
  const [heuristicMinActivityCount, setHeuristicMinActivityCount] = useState("");
  const [heuristicDecoration, setHeuristicDecoration] =
    useState<"frequency" | "performance">("frequency");
  const [
    heuristicPerformanceAggregation,
    setHeuristicPerformanceAggregation,
  ] = useState<
    "mean" | "median" | "min" | "max" | "stdev" | "sum" | "raw_values" | "all"
  >("mean");
  const [heuristicBusinessHours, setHeuristicBusinessHours] = useState(false);
  const [heuristicBusinessHourSlots, setHeuristicBusinessHourSlots] =
    useState("");
  const [
    heuristicDfgPreCleaningNoiseThreshold,
    setHeuristicDfgPreCleaningNoiseThreshold,
  ] = useState("0.05");
  const [
    heuristicLoopLengthTwoThreshold,
    setHeuristicLoopLengthTwoThreshold,
  ] = useState("0.5");
  const [heuristicKeepOncePerCase, setHeuristicKeepOncePerCase] =
    useState(false);
  const [heuristicMostCommonPaths, setHeuristicMostCommonPaths] = useState("");
  const [
    heuristicStartTimestampAttribute,
    setHeuristicStartTimestampAttribute,
  ] = useState("");
  const [
    heuristicLifecycleInstanceAttribute,
    setHeuristicLifecycleInstanceAttribute,
  ] = useState("concept:instance");
  const heuristicBusinessHoursOptionsActive = heuristicBusinessHoursOptionsAreActive(
    heuristicVariant,
    heuristicDecoration,
  );
  const [powlVariant, setPowlVariant] = useState<"tree" | "brute_force" | "maximal" | "dynamic_clustering" | "dynamic_clustering_plain">("maximal");
  const [powlFilteringType, setPowlFilteringType] = useState<"decreasing_factor" | "dynamic">("decreasing_factor");
  const [powlFilteringThreshold, setPowlFilteringThreshold] = useState("");
  const [powlOrderFrequencyRatio, setPowlOrderFrequencyRatio] = useState("1");
  const [powlBruteForceActivityLimit, setPowlBruteForceActivityLimit] = useState("");
  const [powlActivityKey, setPowlActivityKey] = useState("concept:name");
  const [powlTimestampKey, setPowlTimestampKey] = useState("time:timestamp");
  const [powlCaseIdKey, setPowlCaseIdKey] = useState("case:concept:name");
  const [tokenReplayVariant, setTokenReplayVariant] =
    useState<"forward" | "backwards">("forward");
  const [
    tokenReplayStopImmediatelyUnfit,
    setTokenReplayStopImmediatelyUnfit,
  ] = useState(false);
  const [tokenReplayActivityAttribute, setTokenReplayActivityAttribute] =
    useState("");
  const [tokenReplayPolicies, setTokenReplayPolicies] =
    useState({ ...TOKEN_REPLAY_POLICY_DEFAULTS });
  const [tokenReplayMaxRows, setTokenReplayMaxRows] = useState("");
  const [tokenReplayTimestampAttribute, setTokenReplayTimestampAttribute] = useState("time:timestamp");
  const [rootCauseStrings, setRootCauseStrings] = useState("");
  const [rootCauseNumerics, setRootCauseNumerics] = useState("");
  const [rootCauseMultiplier, setRootCauseMultiplier] = useState(false);
  const logSkeletonFamilies = [
    ["equivalence", "Equivalence"],
    ["always_after", "Always-after"],
    ["always_before", "Always-before"],
    ["never_together", "Never-together"],
    ["directly_follows", "Directly-follows"],
    ["activ_freq", "Activity frequency"],
  ] as const;
  const [logSkeletonNoiseThreshold, setLogSkeletonNoiseThreshold] = useState("");
  const [logSkeletonActivityAttribute, setLogSkeletonActivityAttribute] =
    useState("concept:name");
  const [logSkeletonConstraints, setLogSkeletonConstraints] = useState<Set<string>>(
    () => new Set(logSkeletonFamilies.map(([name]) => name)),
  );
  const [ltlExactKind,setLtlExactKind] = useState<"values"|"options">("values");
  const [ltlValuesExact,setLtlValuesExact] = useState(()=>literalFilterEditor("[]"));
  const [ltlOptionsExact,setLtlOptionsExact] = useState(()=>literalFilterEditor(LTL_COMPLETE_EXAMPLE));
  const ltlExact = ltlExactKind === "values" ? ltlValuesExact : ltlOptionsExact;
  const setLtlExact = (value: typeof ltlExact) => {onExactEdit?.(); (ltlExactKind === "values" ? setLtlValuesExact : setLtlOptionsExact)(value);};
  const [pathsExact,setPathsExact] = useState(()=>literalFilterEditor("[]"));
  const [heuristicOptionsExact,setHeuristicOptionsExact]=useState(()=>literalFilterEditor(HEURISTIC_COMPLETE_EXAMPLE));
  const [heuristicExactKind,setHeuristicExactKind]=useState<"pairs"|"options">("pairs");
  const heuristicExact=heuristicExactKind==="options"?heuristicOptionsExact:pathsExact,heuristicFullExact=heuristicExactKind==="options"&&heuristicExact.mode!=="omitted";
  const [ltlVariant, setLtlVariant] = useState<LtlVariant | "">("");
  const [ltlAttributeKey, setLtlAttributeKey] = useState("");
  const [ltlAttributeValues, setLtlAttributeValues] = useState("");
  const [ltlPositive, setLtlPositive] = useState(true);
  const [ltlEnableTimestamp, setLtlEnableTimestamp] = useState(false);
  const [ltlTimestampBounds, setLtlTimestampBounds] = useState("");
  const [ltlResourceKey, setLtlResourceKey] = useState("");
  const declareFamilies = [
    ["existence", "Existence"], ["exactly_one", "Exactly-one"], ["init", "Init"],
    ["responded_existence", "Responded existence"], ["response", "Response"],
    ["precedence", "Precedence"], ["succession", "Succession"], ["altresponse", "Alternate response"],
    ["altprecedence", "Alternate precedence"], ["altsuccession", "Alternate succession"],
    ["chainresponse", "Chain response"], ["chainprecedence", "Chain precedence"],
    ["chainsuccession", "Chain succession"], ["absence", "Absence"], ["coexistence", "Coexistence"],
    ["noncoexistence", "Non-coexistence"], ["nonsuccession", "Non-succession"],
    ["nonchainsuccession", "Non-chain succession"],
  ] as const;
  const [declareConsideredActivities, setDeclareConsideredActivities] = useState("");
  const [declareActivitiesMode, setDeclareActivitiesMode] = useState<DiscoveryVectorMode>("legacy");
  const [declareActivitiesJson, setDeclareActivitiesJson] = useState("[]");
  const [declareActivityAttribute, setDeclareActivityAttribute] =
    useState("concept:name");
  const [declareAllowedTemplates, setDeclareAllowedTemplates] = useState<Set<string>>(
    () => new Set(DECLARE_TEMPLATES),
  );
  const [declareMinSupportRatio, setDeclareMinSupportRatio] = useState("");
  const [declareMinConfidenceRatio, setDeclareMinConfidenceRatio] = useState("");
  const [declareAutoSelectionMultiplier, setDeclareAutoSelectionMultiplier] = useState("0.8");
  const [footprintsDiscoveryVariants, setFootprintsDiscoveryVariants] = useState<Set<string>>(
    () => new Set(FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS),
  );
  const [footprintsModel, setFootprintsModel] = useState<MinerOptions["footprintsModel"]>("process_tree");
  const [footprintsConformanceVariants, setFootprintsConformanceVariants] = useState<Set<string>>(
    () => new Set(["log_model"]),
  );
  const [footprintsStrict, setFootprintsStrict] = useState(false);
  const [footprintsEnableActAlwaysExecuted, setFootprintsEnableActAlwaysExecuted] = useState(true);
  const [footprintsActivityAttribute, setFootprintsActivityAttribute] = useState("concept:name");
  const [footprintsCaseIdAttribute, setFootprintsCaseIdAttribute] = useState("case:concept:name");
  const [footprintsStartTimestampAttribute, setFootprintsStartTimestampAttribute] = useState("");
  const [footprintsTimestampAttribute, setFootprintsTimestampAttribute] = useState("time:timestamp");
  const [footprintsIndexAttribute, setFootprintsIndexAttribute] = useState("@@index");
  const [footprintsSortRequired, setFootprintsSortRequired] = useState(true);
  const [footprintsInitialMarking, setFootprintsInitialMarking] = useState("");
  const dfgStatisticalNames = [
    "native", "frequency", "frequency_greedy", "frequency_triples", "performance",
    "performance_greedy", "case_attributes", "clean", "clean_polars", "clean_time",
  ] as const;
  const dfgCausalNames = ["alpha", "heuristic"] as const;
  const [dfgStatisticalVariants, setDfgStatisticalVariants] = useState<Set<string>>(new Set());
  const [dfgCausalVariants, setDfgCausalVariants] = useState<Set<string>>(new Set());
  const [dfgWindow, setDfgWindow] = useState("1");
  const [dfgKeepOncePerCase, setDfgKeepOncePerCase] = useState(false);
  const [dfgActivityAttribute, setDfgActivityAttribute] = useState("concept:name");
  const [dfgTimestampAttribute, setDfgTimestampAttribute] = useState("time:timestamp");
  const [dfgPerformanceAggregation, setDfgPerformanceAggregation] = useState<NonNullable<MinerOptions["dfgPerformanceAggregation"]>>("mean");
  // An empty selector preserves the adapter's completion-timestamp default.
  const [dfgStartTimestampAttribute, setDfgStartTimestampAttribute] = useState("");
  const [dfgBusinessHours, setDfgBusinessHours] = useState(false);
  const [dfgBusinessHourSlots, setDfgBusinessHourSlots] = useState("");
  const [dfgCaseAttributes, setDfgCaseAttributes] = useState("concept:name");
  const [dfgAttributesMode, setDfgAttributesMode] = useState<DiscoveryVectorMode>("legacy");
  const [dfgAttributesJson, setDfgAttributesJson] = useState('["concept:name"]');
  const [dfgReturnNodeAttributes, setDfgReturnNodeAttributes] = useState(false);
  const dfgFrequencyOptionsActive =
    dfgCausalVariants.size > 0 ||
    ["native", "frequency", "frequency_greedy"].some((name) =>
      dfgStatisticalVariants.has(name));
  const dfgPerformanceOptionsActive =
    dfgStatisticalVariants.has("performance") ||
    dfgStatisticalVariants.has("performance_greedy");
  const dfgActivityOptionsActive =
    dfgStatisticalVariants.size > 0 || dfgCausalVariants.size > 0;
  const dfgTimestampOptionsActive =
    dfgStatisticalVariants.has("performance") ||
    dfgStatisticalVariants.has("performance_greedy") ||
    dfgStatisticalVariants.has("clean") ||
    dfgStatisticalVariants.has("clean_polars") ||
    dfgStatisticalVariants.has("clean_time");
  const dfgCaseAttributeOptionsActive =
    dfgStatisticalVariants.has("case_attributes");
  const correlationNames = [
    "classic", "classic_split", "trace_based",
  ] as const;
  const [correlationVariants, setCorrelationVariants] =
    useState<Set<string>>(new Set());
  const [correlationActivityAttribute, setCorrelationActivityAttribute] =
    useState("concept:name");
  const [correlationTimestampAttribute, setCorrelationTimestampAttribute] =
    useState("time:timestamp");
  const [correlationStartTimestampAttribute,
    setCorrelationStartTimestampAttribute] = useState("");
  const [correlationCaseIdAttribute, setCorrelationCaseIdAttribute] =
    useState("case:concept:name");
  const [correlationExactTimeMatching,
    setCorrelationExactTimeMatching] = useState(false);
  const [correlationSampleSize, setCorrelationSampleSize] =
    useState("100000");
  const correlationExactOptionsActive =
    correlationVariants.has("classic") ||
    correlationVariants.has("classic_split");
  const correlationSplitOptionsActive =
    correlationVariants.has("classic_split");
  const [temporalActivityAttribute, setTemporalActivityAttribute] =
    useState("concept:name");
  const [temporalCaseIdAttribute, setTemporalCaseIdAttribute] =
    useState("case:concept:name");
  const [temporalSourceTimestampAttribute,
    setTemporalSourceTimestampAttribute] = useState("");
  const [temporalTargetTimestampAttribute,
    setTemporalTargetTimestampAttribute] = useState("");
  const [temporalBusinessHours, setTemporalBusinessHours] = useState(false);
  const [temporalBusinessHourSlots, setTemporalBusinessHourSlots] =
    useState("");
  const [temporalStrictAttributes, setTemporalStrictAttributes] =
    useState(false);
  const [transitionActivityKey, setTransitionActivityKey] =
    useState("concept:name");
  const [transitionCaseIdKey, setTransitionCaseIdKey] =
    useState("case:concept:name");
  const [transitionView, setTransitionView] = useState<
    "sequence" | "set" | "multiset"
  >("sequence");
  const [transitionDirection, setTransitionDirection] = useState<
    "forward" | "backward"
  >("forward");
  const [transitionWindow, setTransitionWindow] = useState("2");
  const [transitionIncludeData, setTransitionIncludeData] = useState(false);
  const [spectrumActivities, setSpectrumActivities] = useState("");
  const [spectrumVariant, setSpectrumVariant] = useState<
    "connected" | "disconnected" | "dataframe" | "log" |
    "dataframe_disconnected" | "log_disconnected" | "lazyframe" |
    "lazyframe_disconnected"
  >("connected");
  const [spectrumSampleSize, setSpectrumSampleSize] = useState("10000");
  const [spectrumSortLog, setSpectrumSortLog] = useState(true);
  const [spectrumActivityKey, setSpectrumActivityKey] = useState("");
  const [spectrumTimestampKey, setSpectrumTimestampKey] = useState("");
  const [spectrumCaseIdKey, setSpectrumCaseIdKey] = useState("");
  const [batches, setBatches] = useState(false);
  const [batchVariant, setBatchVariant] = useState<"log" | "pandas" | "polars">("log");
  const [batchMergeDistanceSeconds, setBatchMergeDistanceSeconds] = useState("900");
  const [batchMinSize, setBatchMinSize] = useState("2");
  const [batchActivityAttribute, setBatchActivityAttribute] = useState("");
  const [batchResourceAttribute, setBatchResourceAttribute] = useState("");
  const [batchTimestampAttribute, setBatchTimestampAttribute] = useState("");
  const [batchCaseIdAttribute, setBatchCaseIdAttribute] = useState("");
  const [batchStartTimestampAttribute, setBatchStartTimestampAttribute] = useState("");
  const [batchEventIdAttribute, setBatchEventIdAttribute] = useState("");
  const [minimumSelfDistanceVariant, setMinimumSelfDistanceVariant] =
    useState<"log" | "pandas" | "polars">("log");
  const [minimumSelfDistanceActivityKey, setMinimumSelfDistanceActivityKey] =
    useState("");
  const [minimumSelfDistanceCaseIdKey, setMinimumSelfDistanceCaseIdKey] =
    useState("");
  const [overlap, setOverlap] = useState(false);
  const [overlapActivityAttribute, setOverlapActivityAttribute] = useState("");
  const [overlapStartAttribute, setOverlapStartAttribute] = useState("");
  const [overlapCompletionAttribute, setOverlapCompletionAttribute] = useState("");
  const [overlapEpsilon, setOverlapEpsilon] = useState("0.00001");
  const [overlapStrict, setOverlapStrict] = useState(false);

  const toggle = (setter: typeof setRemoved) => (name: string) => {
    setter((prev) => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };
  const toggleActivity = toggle(setRemoved);
  const toggleResource = toggle(setRemovedResources);
  const toggleLifecycle = toggle(setRemovedLifecycles);
  const toggleValue = toggle(setRemovedValues);
  const toggleCaseValue = toggle(setRemovedCaseValues);
  const toggleLogSkeletonConstraint = (name: string) => {
    setLogSkeletonConstraints((previous) => {
      // Empty means backend default-all, so keep at least one selected.
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };
  const toggleDeclareTemplate = (name: string) => {
    setDeclareAllowedTemplates((previous) => {
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };
  const toggleFootprintsDiscovery = (name: string) => {
    setFootprintsDiscoveryVariants((previous) => {
      if (name === "petri_reach_graph" && footprintsModel === "dfg") return previous;
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name); else next.add(name);
      if (name === "petri_reach_graph" && next.has(name)) setFootprintsModel("process_tree");
      return next;
    });
  };
  const selectFootprintsModel = (model: MinerOptions["footprintsModel"]) => {
    setFootprintsModel(model);
    if (model === "dfg") {
      setFootprintsDiscoveryVariants((previous) => {
        const next = new Set(previous);
        next.delete("petri_reach_graph");
        if (next.size === 0) next.add("entire_event_log");
        return next;
      });
    }
  };
  const toggleFootprintsConformance = (name: string) => {
    setFootprintsConformanceVariants((previous) => {
      if (previous.size === 1 && previous.has(name)) return previous;
      const next = new Set(previous);
      if (next.has(name)) next.delete(name); else next.add(name);
      return next;
    });
  };
  const toggleDfgVariant = (
    setter: typeof setDfgStatisticalVariants,
    name: string,
  ) => setter((previous) => {
    const next = new Set(previous);
    if (next.has(name)) next.delete(name); else next.add(name);
    return next;
  });

  const pickAttributeKey = (key: string) => {
    setAttributeKey(key);
    setRemovedValues(new Set());
  };
  const selectedAttribute = attributes.find((a) => a.key === attributeKey);

  const pickCaseAttributeKey = (key: string) => {
    setCaseAttributeKey(key);
    setRemovedCaseValues(new Set());
  };
  const selectedCaseAttribute = caseAttributes.find(
    (a) => a.key === caseAttributeKey,
  );

  const toInt = (value: string) => {
    const n = Number.parseInt(value, 10);
    return Number.isFinite(n) && n > 0 ? n : undefined;
  };
  const toNonnegativeInt = (value: string) => {
    if (!value.trim()) return undefined;
    const n = Number(value);
    return Number.isSafeInteger(n) && n >= 0 ? n : undefined;
  };
  const toFloat = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) ? n : undefined;
  };
  const toPowlThreshold = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) && n >= 0 && n < 1 ? n : undefined;
  };
  const toPowlRatio = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) && n > 0.5 && n <= 1 ? n : undefined;
  };
  const toPowlActivityLimit = (value: string) => {
    if (value.trim() === "") return undefined;
    const n = Number(value);
    return Number.isInteger(n) && n >= 0 ? n : undefined;
  };
  const toLogSkeletonNoise = (value: string) => {
    const n = Number.parseFloat(value);
    return Number.isFinite(n) && n >= 0 && n <= 1 ? n : undefined;
  };
  const toDeclareRatio = (value: string) => {
    if (value.trim() === "") return undefined;
    const n = Number(value);
    return Number.isFinite(n) && n >= 0 && n <= 1 ? n : undefined;
  };
  const toDeclareActivities = (value: string) => {
    const names = value.split(",").map((name) => name.trim()).filter(Boolean);
    return names.length ? [...new Set(names)] : undefined;
  };
  const toLtlValues = (value: string) => {
    const values = value.split(",").map((item) => item.trim());
    return values.length && values.every(Boolean) ? values : undefined;
  };
  const toLtlBounds = (value: string) => {
    const result: Array<[number, number]> = [];
    for (const item of value.split(",").map((part) => part.trim()).filter(Boolean)) {
      const [lo, hi, ...rest] = item.split(":").map(Number);
      if (rest.length || !Number.isFinite(lo) || !Number.isFinite(hi) || lo > hi) return undefined;
      result.push([lo, hi]);
    }
    return result.length ? result : undefined;
  };

  const spectrumTokens = spectrumActivities.split(",").map((name) => name.trim());
  const spectrumHasEmptyActivity = spectrumActivities.trim() !== "" &&
    spectrumTokens.some((name) => !name);
  const spectrumNames = spectrumTokens.filter(Boolean);
  const spectrumConfigured = spectrumNames.length >= 2;
  const temporalValidationError = temporalValidationMessage({
    spectrumConfigured,
    spectrumActivityCount: spectrumNames.length,
    spectrumHasEmptyActivity,
    spectrumSampleSize,
    batches,
    batchVariant,
    batchMergeDistanceSeconds,
    batchMinSize,
    transitionActivityKey,
    transitionCaseIdKey,
    transitionView,
    transitionDirection,
    transitionWindow,
    batchEventIdAttribute: batches ? batchEventIdAttribute : undefined,
    overlap,
    overlapEpsilon,
  });
  const ltlExactActive = ltlExact.mode !== "omitted", ltlFullExact = ltlExactActive && ltlExactKind === "options";
  let exactOptions: ExactFilterControls = {}, exactValidationError: string|null = null;
  try {exactOptions = {...filterEditorControl(ltlExact, ltlExactKind === "options" ? "ltl_options" : "ltl_attribute_values"), ...filterEditorControl(heuristicExact,heuristicExactKind==="options"?"heuristic_options":"heuristic_most_common_paths")}; exactFilterFields(exactOptions);} catch(cause) {exactValidationError = cause instanceof Error ? cause.message : String(cause);}
  const ltlNames = ltlAttributeValues.split(",").map((value) => value.trim()).filter(Boolean);
  const ltlTimestampActive = ltlVariant === "eventually_follows" && ltlEnableTimestamp;
  const ltlValidationError = (() => {
    if (exactValidationError) return exactValidationError;
    if (ltlExactActive) {if (!ltlFullExact && ltlTimestampBounds && !toLtlBounds(ltlTimestampBounds)) return "Timestamp scalar syntax needs finite min:max pairs; use full LTL JSON for all IEEE values."; return null;}
    if (!ltlVariant) return null;
    const expected = ltlVariant === "eventually_follows" ? 1 :
      ltlVariant === "a_next_b_next_c" ? 3 : ltlVariant === "four_eyes_principle" ? 2 : 1;
    if (ltlNames.length !== expected && !(ltlVariant === "eventually_follows" && ltlNames.length >= 1))
      return `This LTL variant requires ${expected === 1 ? "one" : `exactly ${expected}`} value${expected === 1 ? "" : "s"}.`;
    if (ltlVariant !== "eventually_follows" && ltlTimestampBounds.trim())
      return "Timestamp bounds only apply to eventually-follows.";
    if (ltlTimestampActive) {
      const bounds = toLtlBounds(ltlTimestampBounds);
      if (!bounds || bounds.length !== ltlNames.length - 1)
        return "Timestamp bounds need one ordered finite min:max pair per adjacent value pair.";
    } else if (ltlTimestampBounds.trim()) {
      return "Clear timestamp bounds or enable timestamp filtering.";
    }
    return null;
  })();
  const segmentValidationError = segmentValidationMessage({
    betweenStart: segmentBetweenStart,
    betweenEnd: segmentBetweenEnd,
  });
  const pathValidationError = pathValidationMessage({
    predecessor: pathPredecessor,
    successor: pathSuccessor,
    minPerformance: pathMinPerformance,
    maxPerformance: pathMaxPerformance,
    positive: pathPositive,
    activityAttribute: pathActivityAttribute,
    timestampAttribute: pathTimestampAttribute,
  });
  const caseDurationValidationError = caseDurationValidationMessage({
    min: caseDurationMin,
    max: caseDurationMax,
  });
  const caseSelectionValidationError = caseSelectionValidationMessage({
    maxCases,
    minLength,
    maxLength,
  });
  const numericAttributeValidationError = numericAttributeValidationMessage({
    key: numericAttributeKey,
    min: numericAttributeMin,
    max: numericAttributeMax,
  });
  const topActivitiesValidationError = topActivitiesValidationMessage({
    max: maxActivities,
    attributeKey: topActivitiesAttributeKey,
  });
  const relativeAttributeValidationError = relativeAttributeValidationMessage({
    key: relativeAttributeKey,
    stake: relativeAttributeStake,
    keepOncePerCase: relativeAttributeKeepOncePerCase,
  });
  const attributeOccurrenceValidationError = attributeOccurrenceValidationMessage({
    key: attributeOccurrenceKey,
    min: attributeOccurrenceMin,
  });
  const variantCoverageValidationError = variantCoverageValidationMessage({
    min: variantMinCoverage,
    max: variantMaxCoverage,
    percentage: variantPercentage,
  });
  const repetitionValidationError = repetitionValidationMessage({
    value: repetitionValue,
    min: repetitionMin,
    max: repetitionMax,
  });
  const caseGroupingValidationError = caseGroupingValidationMessage({
    timestampGrouping,
    timestampGroupingSeparator,
  });
  const alphaValidationError = alphaValidationMessage({
    activityAttribute: alphaActivityAttribute,
    removeUnconnected: alphaRemoveUnconnected,
    maxRowsToMaterialize: alphaMaxRowsToMaterialize,
  });
  const heuristicBusinessHourSlotsValidationError = heuristicFullExact ? null :
    heuristicBusinessHourSlotsValidationMessage(
      heuristicBusinessHourSlots,
      heuristicBusinessHoursOptionsActive && heuristicBusinessHours,
    );
  const powlValidationError = powlValidationMessage({
    filteringThreshold: powlFilteringThreshold,
    orderFrequencyRatio: powlOrderFrequencyRatio,
    bruteForceActivityLimit: powlBruteForceActivityLimit,
    activityKey: powlActivityKey,
    timestampKey: powlTimestampKey,
    caseIdKey: powlCaseIdKey,
  });
  const footprintsValidationError = footprintsValidationMessage({
    activityAttribute: footprintsActivityAttribute,
    caseIdAttribute: footprintsCaseIdAttribute,
    timestampAttribute: footprintsTimestampAttribute,
    startTimestampAttribute: footprintsStartTimestampAttribute,
    indexAttribute: footprintsIndexAttribute,
    initialMarking: footprintsInitialMarking,
  });
  const correlationValidationError = correlationValidationMessage({
    enabled: correlationVariants.size > 0,
    activityAttribute: correlationActivityAttribute,
    timestampAttribute: correlationTimestampAttribute,
    startTimestampAttribute: correlationStartTimestampAttribute,
    caseIdAttribute: correlationCaseIdAttribute,
    traceEnabled: correlationVariants.has("trace_based"),
    splitEnabled: correlationSplitOptionsActive,
    sampleSize: correlationSampleSize,
  });
  let rootCauseValidationError: string | null = null;
  try {
    tokenReplayRootCauseOptionsFromForm(tokenReplayPolicies.tokenReplayIncludeRootCauseDiagnostics,
      rootCauseStrings, rootCauseNumerics, rootCauseMultiplier);
  } catch (error) {
    rootCauseValidationError = (error as Error).message;
  }
  const literalVectorValidationError = (declareActivitiesMode === "literal_json"
    ? discoveryStringVectorValidation(declareActivitiesJson, "Declare considered activities") : null) ||
    (dfgCaseAttributeOptionsActive && dfgAttributesMode === "literal_json"
      ? discoveryStringVectorValidation(dfgAttributesJson, "DFG case attributes") : null);
  const selectorValidationError = literalVectorValidationError || rootCauseValidationError ||
    tokenReplayInputValidationMessage(tokenReplayMaxRows, tokenReplayTimestampAttribute,
      tokenReplayPolicies.tokenReplayIncludeDurationDiagnostics) ||
    (!logSkeletonActivityAttribute.trim()
      ? "Log skeleton activity attribute must not be empty."
      : !declareActivityAttribute.trim()
        ? "Declare activity attribute must not be empty."
        : null);

  const apply = () => {
    if (temporalValidationError || ltlValidationError || segmentValidationError ||
        pathValidationError || caseDurationValidationError ||
        caseSelectionValidationError ||
        numericAttributeValidationError || topActivitiesValidationError ||
        relativeAttributeValidationError || attributeOccurrenceValidationError ||
        variantCoverageValidationError || repetitionValidationError ||
        caseGroupingValidationError || alphaValidationError ||
        heuristicBusinessHourSlotsValidationError || powlValidationError ||
        footprintsValidationError || correlationValidationError ||
        selectorValidationError) return;
    const removeList = activities.filter((a) => removed.has(a));
    const removeResourceList = resources.filter((r) => removedResources.has(r));
    const removeLifecycleList = lifecycles.filter((l) =>
      removedLifecycles.has(l),
    );
    // Only an actually dropped value activates the attribute filter; a picked
    // key alone is just browsing its values and must not change the mine.
    const removeValueList =
      selectedAttribute?.values
        .map((v) => v.value)
        .filter((v) => removedValues.has(v)) ?? [];
    const removeCaseValueList =
      selectedCaseAttribute?.values
        .map((v) => v.value)
        .filter((v) => removedCaseValues.has(v)) ?? [];
    // The UI takes coverage as a percentage; the backend expects (0, 1].
    const coveragePercent = toFloat(variantCoverage);
    const coverage =
      coveragePercent && coveragePercent > 0 && coveragePercent <= 100
        ? coveragePercent / 100
        : undefined;
    const toCoverageFraction = (value: string) => {
      if (!value.trim()) return undefined;
      const parsed = Number(value);
      return Number.isFinite(parsed) && parsed >= 0 && parsed <= 100
        ? parsed / 100 : undefined;
    };
    const successionPair =
      successionAction !== "off" && successionFrom && successionTo
        ? [`${successionFrom} -> ${successionTo}`]
        : undefined;
    const succession = (action: "keep" | "remove", relation: string) =>
      successionAction === action && successionRelation === relation
        ? successionPair
        : undefined;
    const declareAutoSelectionActive =
      declareMinSupportRatio.trim() === "" && declareMinConfidenceRatio.trim() === "";
    const emit = (options:FilterOptions & MinerOptions) => {if(heuristicFullExact)for(const key of Object.keys(options))if(key.startsWith("heuristic")&&!key.startsWith("heuristic_")||["dependencyThreshold","andThreshold","minEdgeFrequency"].includes(key))delete (options as Record<string,unknown>)[key];onApply(options);};
    emit({
      ...exactOptions,
      topVariants: toInt(topVariants),
      maxActivities: maxActivities.trim() ? Number(maxActivities) : undefined,
      topActivitiesAttributeKey: maxActivities.trim()
        ? topActivitiesAttributeKey.trim() || undefined : undefined,
      minVariantFrequency: toInt(minFrequency),
      variantCoverage: coverage,
      variantMinCoverage: toCoverageFraction(variantMinCoverage),
      variantMaxCoverage: toCoverageFraction(variantMaxCoverage),
      variantPercentage: toCoverageFraction(variantPercentage),
      startActivities: startActivities ? [startActivities] : undefined,
      endActivities: endActivities ? [endActivities] : undefined,
      startActivitiesPositive: startActivities ? startActivitiesPositive : undefined,
      endActivitiesPositive: endActivities ? endActivitiesPositive : undefined,
      keepDirectlyFollows: succession("keep", "directly"),
      removeDirectlyFollows: succession("remove", "directly"),
      keepEventuallyFollows: succession("keep", "eventually"),
      removeEventuallyFollows: succession("remove", "eventually"),
      tracePatterns: tracePattern.trim() ? [tracePattern.trim()] : undefined,
      tracePatternPositive: tracePattern.trim() && !tracePatternPositive
        ? false
        : undefined,
      admittedVariants: admittedVariants.trim()
        ? admittedVariants.split(";").map((value) => value.trim()) : undefined,
      admittedVariantsPositive: admittedVariants.trim() && !admittedVariantsPositive
        ? false : undefined,
      prefixActivity: prefixActivity || undefined,
      suffixActivity: suffixActivity || undefined,
      prefixStrict: prefixActivity ? prefixStrict : undefined,
      prefixOccurrence: prefixActivity ? prefixOccurrence : undefined,
      suffixStrict: suffixActivity ? suffixStrict : undefined,
      suffixOccurrence: suffixActivity ? suffixOccurrence : undefined,
      segmentPrefixActivity: segmentPrefixActivity || undefined,
      segmentPrefixStrict: segmentPrefixActivity ? segmentPrefixStrict : undefined,
      segmentPrefixOccurrence: segmentPrefixActivity ? segmentPrefixOccurrence : undefined,
      segmentSuffixActivity: segmentSuffixActivity || undefined,
      segmentSuffixStrict: segmentSuffixActivity ? segmentSuffixStrict : undefined,
      segmentSuffixOccurrence: segmentSuffixActivity ? segmentSuffixOccurrence : undefined,
      segmentBetweenStarts: segmentBetweenStart ? [segmentBetweenStart] : undefined,
      segmentBetweenEnds: segmentBetweenEnd ? [segmentBetweenEnd] : undefined,
      pathPredecessor: pathPredecessor || undefined,
      pathSuccessor: pathSuccessor || undefined,
      pathActivityAttribute: pathActivityAttribute.trim() || undefined,
      pathTimestampAttribute: pathTimestampAttribute.trim() || undefined,
      pathMinPerformance: pathMinPerformance.trim() ? Number(pathMinPerformance) : undefined,
      pathMaxPerformance: pathMaxPerformance.trim() ? Number(pathMaxPerformance) : undefined,
      pathPositive: pathPredecessor && pathSuccessor ? pathPositive : undefined,
      removeActivities: removeList.length > 0 ? removeList : undefined,
      removeResources:
        removeResourceList.length > 0 ? removeResourceList : undefined,
      removeLifecycles:
        removeLifecycleList.length > 0 ? removeLifecycleList : undefined,
      timeFrom: timeFrom || undefined,
      timeTo: timeTo || undefined,
      timeMode: timeFrom || timeTo ? timeMode : undefined,
      caseDurationMin: caseDurationMin.trim() ? Number(caseDurationMin) : undefined,
      caseDurationMax: caseDurationMax.trim() ? Number(caseDurationMax) : undefined,
      maxCases: maxCases.trim() ? Number(maxCases) : undefined,
      minLength: minLength.trim() ? Number(minLength) : undefined,
      maxLength: maxLength.trim() ? Number(maxLength) : undefined,
      numericAttributeKey: numericAttributeKey || undefined,
      numericAttributeMin: numericAttributeKey && numericAttributeMin.trim()
        ? Number(numericAttributeMin) : undefined,
      numericAttributeMax: numericAttributeKey && numericAttributeMax.trim()
        ? Number(numericAttributeMax) : undefined,
      numericAttributeMode: numericAttributeKey ? numericAttributeMode : undefined,
      numericAttributePositive: numericAttributeKey
        ? numericAttributePositive : undefined,
      relativeAttributeKey: relativeAttributeKey.trim() || undefined,
      relativeAttributeStake: relativeAttributeStake.trim()
        ? Number(relativeAttributeStake) : undefined,
      relativeAttributeKeepOncePerCase:
        (relativeAttributeKey.trim() || relativeAttributeStake.trim() ||
         !relativeAttributeKeepOncePerCase)
          ? relativeAttributeKeepOncePerCase : undefined,
      attributeOccurrenceKey: attributeOccurrenceKey.trim() || undefined,
      attributeOccurrenceMin: attributeOccurrenceMin.trim()
        ? Number(attributeOccurrenceMin) : undefined,
      reworkActivity: reworkActivity || undefined,
      reworkMin: reworkActivity ? toInt(reworkMin) : undefined,
      betweenStart: betweenStart && betweenEnd ? betweenStart : undefined,
      betweenEnd: betweenStart && betweenEnd ? betweenEnd : undefined,
      activitySplit: activitySplitActivity ? [activitySplitActivity] : undefined,
      activitySplitSeparator: activitySplitActivity ? activitySplitSeparator : undefined,
      activitySplitCutMode: activitySplitActivity ? activitySplitCutMode : undefined,
      repetitionValue: repetitionValue || undefined,
      repetitionAttributeKey: repetitionValue ? repetitionAttributeKey || undefined : undefined,
      repetitionMin: repetitionValue && repetitionMin.trim() ? Number(repetitionMin) : undefined,
      repetitionMax: repetitionValue && repetitionMax.trim() ? Number(repetitionMax) : undefined,
      consecutiveGrouping: consecutiveGrouping || undefined,
      consecutiveGroupingActivityKey: consecutiveGrouping ? consecutiveGroupingActivityKey || undefined : undefined,
      timestampGrouping: timestampGrouping || undefined,
      timestampGroupingActivityKey: timestampGrouping ? timestampGroupingActivityKey || undefined : undefined,
      timestampGroupingTimestampKey: timestampGrouping ? timestampGroupingTimestampKey || undefined : undefined,
      timestampGroupingSeparator: timestampGrouping ? timestampGroupingSeparator : undefined,
      attributeKey: removeValueList.length > 0 ? attributeKey : undefined,
      removeAttributeValues:
        removeValueList.length > 0 ? removeValueList : undefined,
      attributeMode:
        removeValueList.length > 0 && attributeMode !== "events"
          ? attributeMode
          : undefined,
      caseAttributeKey:
        removeCaseValueList.length > 0 ? caseAttributeKey : undefined,
      removeCaseAttributeValues:
        removeCaseValueList.length > 0 ? removeCaseValueList : undefined,
      ltlVariant: ltlFullExact ? undefined : ltlVariant || undefined,
      ltlAttributeKey: ltlFullExact ? undefined : ltlExactActive ? ltlAttributeKey : ltlVariant ? ltlAttributeKey || undefined : undefined,
      ltlAttributeValues: ltlExactActive ? undefined : ltlVariant ? toLtlValues(ltlAttributeValues) : undefined,
      ltlPositive: ltlFullExact ? undefined : ltlExactActive || ltlVariant ? ltlPositive : undefined,
      ltlEnableTimestamp: ltlFullExact ? undefined : ltlExactActive ? ltlEnableTimestamp : ltlTimestampActive ? true : undefined,
      ltlTimestampDiffBoundaries:
        ltlFullExact ? undefined : ltlExactActive ? (ltlTimestampBounds ? toLtlBounds(ltlTimestampBounds) : []) : ltlTimestampActive ? toLtlBounds(ltlTimestampBounds) : undefined,
      ltlResourceKey: ltlFullExact ? undefined : ltlExactActive ? ltlResourceKey : ltlVariant ? ltlResourceKey || undefined : undefined,
      noiseThreshold: toFloat(noiseThreshold),
      inductiveVariant: inductiveVariant !== "log" ? inductiveVariant : undefined,
      ...inductiveOptionsFromForm({
        disableStrictSequenceCut: inductiveDisableStrictSequenceCut,
        disableFallthroughs: inductiveDisableFallthroughs,
        multiprocessing: inductiveMultiprocessing,
        sortEventsByTimestamp: inductiveSortEventsByTimestamp,
        activityKey: inductiveActivityKey,
        timestampKey: inductiveTimestampKey,
        caseIdKey: inductiveCaseIdKey,
      }),
      alphaVariant: alphaVariant !== "classic" ? alphaVariant : undefined,
      ...alphaOptionsFromForm({
        activityAttribute: alphaActivityAttribute,
        removeUnconnected: alphaRemoveUnconnected,
        maxRowsToMaterialize: alphaMaxRowsToMaterialize,
      }),
      heuristicVariant:
        heuristicVariant !== "classic" ? heuristicVariant : undefined,
      heuristicActivityAttribute:
        heuristicActivityAttribute.trim() !== "concept:name"
          ? heuristicActivityAttribute.trim() || undefined
          : undefined,
      heuristicTimestampAttribute:
        heuristicTimestampAttribute.trim() !== "time:timestamp"
          ? heuristicTimestampAttribute.trim() || undefined
          : undefined,
      dependencyThreshold: toFloat(dependencyThreshold),
      andThreshold: toFloat(andThreshold),
      minEdgeFrequency: toInt(minEdgeFrequency),
      heuristicMinActivityCount: toInt(heuristicMinActivityCount),
      heuristicDecoration:
        heuristicDecoration !== "frequency" ? heuristicDecoration : undefined,
      heuristicPerformanceAggregation:
        heuristicPerformanceAggregation !== "mean"
          ? heuristicPerformanceAggregation
          : undefined,
      heuristicBusinessHours:
        heuristicBusinessHoursOptionsActive && heuristicBusinessHours
          ? true : undefined,
      heuristicBusinessHourSlots:
        heuristicBusinessHoursOptionsActive && heuristicBusinessHours
          ? heuristicBusinessHourSlots.trim() || undefined : undefined,
      heuristicDfgPreCleaningNoiseThreshold: toFloat(
        heuristicDfgPreCleaningNoiseThreshold,
      ),
      heuristicLoopLengthTwoThreshold: toFloat(
        heuristicLoopLengthTwoThreshold,
      ),
      heuristicKeepOncePerCase: heuristicKeepOncePerCase || undefined,
      heuristicMostCommonPaths:
        heuristicExact.mode !== "omitted" ? undefined : heuristicMostCommonPaths.trim() || undefined,
      ...heuristicStartTimestampOptionsFromForm(heuristicStartTimestampAttribute),
      heuristicLifecycleInstanceAttribute:
        heuristicLifecycleInstanceAttribute !== "concept:instance"
          ? heuristicLifecycleInstanceAttribute
          : undefined,
      powlVariant: powlVariant !== "maximal" ? powlVariant : undefined,
      powlFilteringType:
        powlFilteringType !== "decreasing_factor" ? powlFilteringType : undefined,
      powlFilteringThreshold: toPowlThreshold(powlFilteringThreshold),
      powlOrderFrequencyRatio: toPowlRatio(powlOrderFrequencyRatio),
      powlBruteForceActivityLimit: toPowlActivityLimit(powlBruteForceActivityLimit),
      powlActivityKey:
        powlActivityKey.trim() !== "concept:name"
          ? powlActivityKey.trim() || undefined
          : undefined,
      powlTimestampKey:
        powlTimestampKey.trim() !== "time:timestamp"
          ? powlTimestampKey.trim() || undefined
          : undefined,
      powlCaseIdKey:
        powlCaseIdKey.trim() !== "case:concept:name"
          ? powlCaseIdKey.trim() || undefined
          : undefined,
      tokenReplayVariant:
        tokenReplayVariant !== "forward" ? tokenReplayVariant : undefined,
      tokenReplayStopImmediatelyUnfit:
        tokenReplayStopImmediatelyUnfit || undefined,
      tokenReplayActivityAttribute:
        tokenReplayActivityAttribute.trim() || undefined,
      ...tokenReplayPolicyOptionsFromForm(tokenReplayPolicies),
      ...tokenReplayRootCauseOptionsFromForm(tokenReplayPolicies.tokenReplayIncludeRootCauseDiagnostics,
        rootCauseStrings, rootCauseNumerics, rootCauseMultiplier),
      ...tokenReplayInputOptionsFromForm(tokenReplayMaxRows, tokenReplayTimestampAttribute,
        tokenReplayPolicies.tokenReplayIncludeDurationDiagnostics),
      logSkeletonNoiseThreshold: toLogSkeletonNoise(logSkeletonNoiseThreshold),
      logSkeletonActivityAttribute:
        logSkeletonActivityAttribute.trim() !== "concept:name"
          ? logSkeletonActivityAttribute.trim() || undefined
          : undefined,
      logSkeletonConstraints:
        logSkeletonConstraints.size === logSkeletonFamilies.length
          ? undefined
          : [...logSkeletonConstraints],
      declareActivityAttribute:
        declareActivityAttribute.trim() !== "concept:name"
          ? declareActivityAttribute.trim() || undefined
          : undefined,
      declareConsideredActivities: declareActivitiesMode === "legacy"
        ? toDeclareActivities(declareConsideredActivities) : undefined,
      declareConsideredActivitiesExact: declareActivitiesMode === "literal_json"
        ? parseDiscoveryStringVector(declareActivitiesJson, "Declare considered activities") : undefined,
      declareAllowedTemplates:
        declareAllowedTemplates.size === declareFamilies.length
          ? undefined
          : [...declareAllowedTemplates],
      declareMinSupportRatio: toDeclareRatio(declareMinSupportRatio),
      declareMinConfidenceRatio: toDeclareRatio(declareMinConfidenceRatio),
      declareAutoSelectionMultiplier: declareAutoSelectionActive
        ? toDeclareRatio(declareAutoSelectionMultiplier)
        : undefined,
      footprintsDiscoveryVariants:
        footprintsDiscoveryVariants.size === FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length &&
        FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.every((name) => footprintsDiscoveryVariants.has(name))
          ? undefined : [...footprintsDiscoveryVariants] as FootprintsDiscoveryVariant[],
      footprintsModel: footprintsModel !== "process_tree" ? footprintsModel : undefined,
      footprintsConformanceVariants:
        footprintsConformanceVariants.size === 1 && footprintsConformanceVariants.has("log_model")
          ? undefined : [...footprintsConformanceVariants] as FootprintsConformanceVariant[],
      footprintsStrict: footprintsStrict || undefined,
      footprintsEnableActAlwaysExecuted: footprintsEnableActAlwaysExecuted ? undefined : false,
      footprintsActivityAttribute:
        footprintsActivityAttribute.trim() !== "concept:name"
          ? footprintsActivityAttribute.trim() || undefined
          : undefined,
      footprintsCaseIdAttribute:
        footprintsCaseIdAttribute.trim() !== "case:concept:name"
          ? footprintsCaseIdAttribute.trim() || undefined
          : undefined,
      footprintsStartTimestampAttribute:
        footprintsStartTimestampAttribute.trim() || undefined,
      footprintsTimestampAttribute:
        footprintsTimestampAttribute.trim() !== "time:timestamp"
          ? footprintsTimestampAttribute.trim() || undefined
          : undefined,
      footprintsIndexAttribute:
        footprintsIndexAttribute.trim() !== "@@index"
          ? footprintsIndexAttribute.trim()
          : undefined,
      footprintsSortRequired: footprintsSortRequired ? undefined : false,
      footprintsInitialMarking:
        footprintsInitialMarking.trim() || undefined,
      dfgStatisticalVariants: dfgStatisticalVariants.size ? [...dfgStatisticalVariants] : undefined,
      dfgCausalVariants: dfgCausalVariants.size ? [...dfgCausalVariants] : undefined,
      dfgWindow: dfgFrequencyOptionsActive
        ? toPowlActivityLimit(dfgWindow) : undefined,
      dfgKeepOncePerCase: dfgFrequencyOptionsActive && dfgKeepOncePerCase
        ? true : undefined,
      dfgActivityAttribute: dfgActivityOptionsActive
        ? dfgActivityAttribute.trim() || undefined : undefined,
      dfgTimestampAttribute: dfgTimestampOptionsActive
        ? dfgTimestampAttribute.trim() || undefined : undefined,
      dfgPerformanceAggregation: dfgPerformanceOptionsActive
        ? dfgPerformanceAggregation : undefined,
      dfgStartTimestampAttribute: dfgPerformanceOptionsActive
        ? dfgStartTimestampAttribute.trim() || undefined : undefined,
      dfgBusinessHours:
        dfgPerformanceOptionsActive && dfgBusinessHours ? true : undefined,
      dfgBusinessHourSlots:
        dfgPerformanceOptionsActive && dfgBusinessHours
          ? dfgBusinessHourSlots.trim() || undefined : undefined,
      dfgCaseAttributes: dfgCaseAttributeOptionsActive && dfgAttributesMode === "legacy"
        ? dfgCaseAttributes.split(",").map((name) => name.trim()).filter(Boolean)
        : undefined,
      dfgCaseAttributesExact: dfgCaseAttributeOptionsActive && dfgAttributesMode === "literal_json"
        ? parseDiscoveryStringVector(dfgAttributesJson, "DFG case attributes") : undefined,
      dfgReturnNodeAttributes:
        dfgCaseAttributeOptionsActive && dfgReturnNodeAttributes
          ? true : undefined,
      correlationVariants:
        correlationVariants.size > 0 ? [...correlationVariants] : undefined,
      correlationActivityAttribute:
        correlationVariants.size > 0 &&
        correlationActivityAttribute.trim() !== "concept:name"
          ? correlationActivityAttribute.trim() || undefined
          : undefined,
      correlationTimestampAttribute:
        correlationVariants.size > 0 &&
        correlationTimestampAttribute.trim() !== "time:timestamp"
          ? correlationTimestampAttribute.trim() || undefined
          : undefined,
      correlationStartTimestampAttribute:
        correlationVariants.size > 0
          ? correlationStartTimestampAttribute.trim() || undefined
          : undefined,
      correlationCaseIdAttribute:
        correlationVariants.size > 0 &&
        correlationCaseIdAttribute.trim() !== "case:concept:name"
          ? correlationCaseIdAttribute.trim() || undefined
          : undefined,
      correlationExactTimeMatching:
        correlationExactOptionsActive && correlationExactTimeMatching
          ? true : undefined,
      correlationSampleSize: correlationSplitOptionsActive
        ? Number(correlationSampleSize.trim()) : undefined,
      temporalActivityAttribute:
        temporalActivityAttribute.trim() !== "concept:name"
          ? temporalActivityAttribute.trim() || undefined : undefined,
      temporalCaseIdAttribute:
        temporalCaseIdAttribute.trim() !== "case:concept:name"
          ? temporalCaseIdAttribute.trim() || undefined : undefined,
      temporalSourceTimestampAttribute:
        temporalSourceTimestampAttribute.trim() || undefined,
      temporalTargetTimestampAttribute:
        temporalTargetTimestampAttribute.trim() || undefined,
      temporalBusinessHours: temporalBusinessHours || undefined,
      temporalBusinessHourSlots:
        temporalBusinessHourSlots.trim() || undefined,
      temporalStrictAttributes: temporalStrictAttributes ? true : undefined,
      transitionActivityKey:
        transitionActivityKey.trim() !== "concept:name"
          ? transitionActivityKey.trim() || undefined : undefined,
      transitionCaseIdKey:
        transitionCaseIdKey.trim() !== "case:concept:name"
          ? transitionCaseIdKey.trim() || undefined : undefined,
      transitionView: transitionView !== "sequence" ? transitionView : undefined,
      transitionDirection: transitionDirection !== "forward"
        ? transitionDirection : undefined,
      transitionWindow: transitionWindow.trim() !== "2"
        ? Number(transitionWindow) : undefined,
      transitionIncludeData: transitionIncludeData ? true : undefined,
      spectrumActivities: spectrumConfigured ? spectrumNames : undefined,
      spectrumVariant: spectrumConfigured && spectrumVariant !== "connected"
        ? spectrumVariant : undefined,
      spectrumSampleSize: spectrumConfigured
        ? toNonnegativeInt(spectrumSampleSize)
        : undefined,
      spectrumSortLog: spectrumConfigured && !spectrumSortLog
        ? false : undefined,
      spectrumActivityKey: spectrumConfigured
        ? spectrumActivityKey.trim() || undefined : undefined,
      spectrumTimestampKey: spectrumConfigured
        ? spectrumTimestampKey.trim() || undefined : undefined,
      spectrumCaseIdKey: spectrumConfigured
        ? spectrumCaseIdKey.trim() || undefined : undefined,
      batches: batches || undefined,
      batchVariant: batches && batchVariant !== "log" ? batchVariant : undefined,
      batchMergeDistanceSeconds: batches
        ? (() => { const n = Number(batchMergeDistanceSeconds); return Number.isFinite(n) && n >= 0 ? n : undefined; })()
        : undefined,
      batchMinSize: batches
        ? (() => { const n = Number(batchMinSize); return Number.isInteger(n) && n >= 2 ? n : undefined; })()
        : undefined,
      batchActivityAttribute: batches ? batchActivityAttribute.trim() || undefined : undefined,
      batchResourceAttribute: batches ? batchResourceAttribute.trim() || undefined : undefined,
      batchTimestampAttribute: batches ? batchTimestampAttribute.trim() || undefined : undefined,
      batchCaseIdAttribute: batches ? batchCaseIdAttribute.trim() || undefined : undefined,
      batchStartTimestampAttribute: batches ? batchStartTimestampAttribute.trim() || undefined : undefined,
      batchEventIdAttribute: batches && batchVariant !== "log"
        ? batchEventIdAttribute.trim() || undefined : undefined,
      minimumSelfDistanceVariant: minimumSelfDistanceVariant !== "log"
        ? minimumSelfDistanceVariant : undefined,
      minimumSelfDistanceActivityKey:
        minimumSelfDistanceActivityKey.trim() || undefined,
      minimumSelfDistanceCaseIdKey:
        minimumSelfDistanceCaseIdKey.trim() || undefined,
      overlap: overlap || undefined,
      overlapActivityAttribute: overlap ? overlapActivityAttribute.trim() || undefined : undefined,
      overlapStartAttribute: overlap ? overlapStartAttribute.trim() || undefined : undefined,
      overlapCompletionAttribute: overlap ? overlapCompletionAttribute.trim() || undefined : undefined,
      overlapEpsilon: overlap ? (() => { const n = Number(overlapEpsilon); return Number.isFinite(n) && n >= 0 ? n : undefined; })() : undefined,
      overlapStrict: overlap && overlapStrict ? true : undefined,
    });
  };

  const reset = () => {
    setTopVariants("");
    setMaxActivities("");
    setTopActivitiesAttributeKey("");
    setMinFrequency("");
    setVariantMinCoverage("");
    setVariantMaxCoverage("");
    setVariantPercentage("");
    setStartActivities("");
    setEndActivities("");
    setStartActivitiesPositive(true);
    setEndActivitiesPositive(true);
    setTimeFrom("");
    setTimeTo("");
    setTimeMode("events");
    setCaseDurationMin("");
    setCaseDurationMax("");
    setMaxCases("");
    setMinLength("");
    setMaxLength("");
    setReworkActivity("");
    setReworkMin("");
    setBetweenStart("");
    setBetweenEnd("");
    setActivitySplitActivity("");
    setActivitySplitSeparator("##@@");
    setActivitySplitCutMode("this");
    setRepetitionValue("");
    setRepetitionAttributeKey("");
    setRepetitionMin("");
    setRepetitionMax("");
    setConsecutiveGrouping("");
    setConsecutiveGroupingActivityKey("");
    setTimestampGrouping("");
    setTimestampGroupingActivityKey("");
    setTimestampGroupingTimestampKey("");
    setTimestampGroupingSeparator(" & ");
    setVariantCoverage("");
    setPrefixActivity("");
    setSuffixActivity("");
    setPrefixStrict(true);
    setPrefixOccurrence("first");
    setSuffixStrict(true);
    setSuffixOccurrence("first");
    setSuccessionAction("off");
    setSuccessionRelation("directly");
    setSuccessionFrom("");
    setSuccessionTo("");
    setTracePattern("");
    setTracePatternPositive(true);
    setAdmittedVariants("");
    setAdmittedVariantsPositive(true);
    setRemoved(new Set());
    setRemovedResources(new Set());
    setRemovedLifecycles(new Set());
    setAttributeKey("");
    setRemovedValues(new Set());
    setAttributeMode("events");
    setNumericAttributeKey("");
    setNumericAttributeMin("");
    setNumericAttributeMax("");
    setNumericAttributeMode("events");
    setNumericAttributePositive(true);
    setRelativeAttributeKey("");
    setRelativeAttributeStake("");
    setRelativeAttributeKeepOncePerCase(true);
    setAttributeOccurrenceKey("");
    setAttributeOccurrenceMin("");
    setCaseAttributeKey("");
    setRemovedCaseValues(new Set());
    setNoiseThreshold("");
    setInductiveVariant("log");
    setInductiveDisableStrictSequenceCut(false);
    setInductiveDisableFallthroughs(false);
    setInductiveMultiprocessing(false);
    setInductiveSortEventsByTimestamp(false);
    setInductiveActivityKey("concept:name");
    setInductiveTimestampKey("time:timestamp");
    setInductiveCaseIdKey("case:concept:name");
    setAlphaVariant("classic");
    setAlphaActivityAttribute("concept:name");
    setAlphaRemoveUnconnected(false);
    setAlphaMaxRowsToMaterialize("");
    setHeuristicVariant("classic");
    setDependencyThreshold("");
    setAndThreshold("");
    setMinEdgeFrequency("");
    setHeuristicMinActivityCount("");
    setHeuristicDecoration("frequency");
    setHeuristicPerformanceAggregation("mean");
    setHeuristicBusinessHours(false);
    setHeuristicBusinessHourSlots("");
    setHeuristicDfgPreCleaningNoiseThreshold("0.05");
    setHeuristicLoopLengthTwoThreshold("0.5");
    setHeuristicKeepOncePerCase(false);
    setHeuristicMostCommonPaths("");
    setHeuristicStartTimestampAttribute("");
    setHeuristicLifecycleInstanceAttribute("concept:instance");
    setPowlVariant("maximal");
    setPowlFilteringType("decreasing_factor");
    setPowlFilteringThreshold("");
    setPowlOrderFrequencyRatio("1");
    setPowlBruteForceActivityLimit("");
    setPowlActivityKey("concept:name");
    setPowlTimestampKey("time:timestamp");
    setPowlCaseIdKey("case:concept:name");
    setTokenReplayVariant("forward");
    setTokenReplayStopImmediatelyUnfit(false);
    setTokenReplayActivityAttribute("");
    setTokenReplayPolicies({ ...TOKEN_REPLAY_POLICY_DEFAULTS });
    setTokenReplayMaxRows("");
    setTokenReplayTimestampAttribute("time:timestamp");
    setRootCauseStrings("");
    setRootCauseNumerics("");
    setRootCauseMultiplier(false);
    setLogSkeletonNoiseThreshold("");
    setLogSkeletonActivityAttribute("concept:name");
    setLogSkeletonConstraints(new Set(logSkeletonFamilies.map(([name]) => name)));
    setDeclareConsideredActivities("");
    setDeclareActivitiesMode("legacy");
    setDeclareActivitiesJson("[]");
    setDeclareActivityAttribute("concept:name");
    setDeclareAllowedTemplates(new Set(DECLARE_TEMPLATES));
    setDeclareMinSupportRatio("");
    setDeclareMinConfidenceRatio("");
    setDeclareAutoSelectionMultiplier("0.8");
    setFootprintsDiscoveryVariants(new Set(FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS));
    setFootprintsModel("process_tree");
    setFootprintsConformanceVariants(new Set(["log_model"]));
    setFootprintsStrict(false);
    setFootprintsEnableActAlwaysExecuted(true);
    setFootprintsActivityAttribute("concept:name");
    setFootprintsCaseIdAttribute("case:concept:name");
    setFootprintsStartTimestampAttribute("");
    setFootprintsTimestampAttribute("time:timestamp");
    setFootprintsIndexAttribute("@@index");
    setFootprintsSortRequired(true);
    setFootprintsInitialMarking("");
    setDfgStatisticalVariants(new Set());
    setDfgCausalVariants(new Set());
    setDfgWindow("1");
    setDfgKeepOncePerCase(false);
    setDfgPerformanceAggregation("mean");
    setDfgStartTimestampAttribute("");
    setDfgBusinessHours(false);
    setDfgBusinessHourSlots("");
    setDfgCaseAttributes("concept:name");
    setDfgAttributesMode("legacy");
    setDfgAttributesJson('["concept:name"]');
    setDfgReturnNodeAttributes(false);
    setCorrelationVariants(new Set());
    setCorrelationActivityAttribute("concept:name");
    setCorrelationTimestampAttribute("time:timestamp");
    setCorrelationStartTimestampAttribute("");
    setCorrelationCaseIdAttribute("case:concept:name");
    setCorrelationExactTimeMatching(false);
    setCorrelationSampleSize("100000");
    setTemporalActivityAttribute("concept:name");
    setTemporalCaseIdAttribute("case:concept:name");
    setTemporalSourceTimestampAttribute("");
    setTemporalTargetTimestampAttribute("");
    setTemporalBusinessHours(false);
    setTemporalBusinessHourSlots("");
    setTemporalStrictAttributes(false);
    setTransitionActivityKey("concept:name");
    setTransitionCaseIdKey("case:concept:name");
    setTransitionView("sequence");
    setTransitionDirection("forward");
    setTransitionWindow("2");
    setTransitionIncludeData(false);
    setSpectrumActivities("");
    setSpectrumVariant("connected");
    setSpectrumSampleSize("10000");
    setSpectrumSortLog(true);
    setSpectrumActivityKey("");
    setSpectrumTimestampKey("");
    setSpectrumCaseIdKey("");
    setBatches(false);
    setBatchVariant("log");
    setBatchMergeDistanceSeconds("900");
    setBatchMinSize("2");
    setBatchActivityAttribute("");
    setBatchResourceAttribute("");
    setBatchTimestampAttribute("");
    setBatchCaseIdAttribute("");
    setBatchStartTimestampAttribute("");
    setBatchEventIdAttribute("");
    setMinimumSelfDistanceVariant("log");
    setMinimumSelfDistanceActivityKey("");
    setMinimumSelfDistanceCaseIdKey("");
    setOverlap(false);
    setOverlapActivityAttribute("");
    setOverlapStartAttribute("");
    setOverlapCompletionAttribute("");
    setOverlapEpsilon("0.00001");
    setOverlapStrict(false);
    setLtlValuesExact(literalFilterEditor("[]")); setLtlOptionsExact(literalFilterEditor(LTL_COMPLETE_EXAMPLE)); setPathsExact(literalFilterEditor("[]"));setHeuristicOptionsExact(literalFilterEditor(HEURISTIC_COMPLETE_EXAMPLE));setHeuristicExactKind("pairs"); setLtlExactKind("values");
    setLtlVariant("");
    setLtlAttributeKey("");
    setLtlAttributeValues("");
    setLtlPositive(true);
    setLtlEnableTimestamp(false);
    setLtlTimestampBounds("");
    setLtlResourceKey("");
    setSegmentPrefixActivity("");
    setSegmentPrefixStrict(true);
    setSegmentPrefixOccurrence("first");
    setSegmentSuffixActivity("");
    setSegmentSuffixStrict(true);
    setSegmentSuffixOccurrence("first");
    setSegmentBetweenStart("");
    setSegmentBetweenEnd("");
    setPathPredecessor("");
    setPathSuccessor("");
    setPathActivityAttribute("");
    setPathTimestampAttribute("");
    setPathMinPerformance("");
    setPathMaxPerformance("");
    setPathPositive(true);
    onApply({});
  };

  const dirty =
    topVariants !== "" ||
    maxActivities !== "" ||
    topActivitiesAttributeKey !== "" ||
    minFrequency !== "" ||
    variantCoverage !== "" ||
    variantMinCoverage !== "" ||
    variantMaxCoverage !== "" ||
    variantPercentage !== "" ||
    startActivities !== "" ||
    endActivities !== "" ||
    (startActivities !== "" && !startActivitiesPositive) ||
    (endActivities !== "" && !endActivitiesPositive) ||
    timeFrom !== "" ||
    timeTo !== "" ||
    caseDurationMin !== "" ||
    caseDurationMax !== "" ||
    maxCases !== "" ||
    minLength !== "" ||
    maxLength !== "" ||
    reworkActivity !== "" ||
    betweenStart !== "" ||
    betweenEnd !== "" ||
    activitySplitActivity !== "" ||
    (activitySplitActivity !== "" && activitySplitSeparator !== "##@@") ||
    (activitySplitActivity !== "" && activitySplitCutMode !== "this") ||
    repetitionValue !== "" ||
    (repetitionValue !== "" && repetitionAttributeKey !== "") ||
    (repetitionValue !== "" && repetitionMin !== "") ||
    (repetitionValue !== "" && repetitionMax !== "") ||
    consecutiveGrouping !== "" ||
    timestampGrouping !== "" ||
    prefixActivity !== "" ||
    suffixActivity !== "" ||
    (prefixActivity !== "" && (!prefixStrict || prefixOccurrence !== "first")) ||
    (suffixActivity !== "" && (!suffixStrict || suffixOccurrence !== "first")) ||
    segmentPrefixActivity !== "" ||
    segmentSuffixActivity !== "" ||
    segmentBetweenStart !== "" ||
    segmentBetweenEnd !== "" ||
    pathPredecessor !== "" ||
    pathSuccessor !== "" ||
    pathActivityAttribute !== "" ||
    pathTimestampAttribute !== "" ||
    pathMinPerformance !== "" ||
    pathMaxPerformance !== "" ||
    !pathPositive ||
    successionAction !== "off" ||
    tracePattern.trim() !== "" ||
    removed.size > 0 ||
    removedResources.size > 0 ||
    removedLifecycles.size > 0 ||
    attributeKey !== "" ||
    numericAttributeKey !== "" ||
    numericAttributeMin !== "" ||
    numericAttributeMax !== "" ||
    (numericAttributeKey !== "" && numericAttributeMode !== "events") ||
    (numericAttributeKey !== "" && !numericAttributePositive) ||
    relativeAttributeKey !== "" ||
    relativeAttributeStake !== "" ||
    !relativeAttributeKeepOncePerCase ||
    attributeOccurrenceKey !== "" ||
    attributeOccurrenceMin !== "" ||
    caseAttributeKey !== "" ||
    noiseThreshold !== "" ||
    inductiveVariant !== "log" ||
    inductiveSettingsAreDirty({
      disableStrictSequenceCut: inductiveDisableStrictSequenceCut,
      disableFallthroughs: inductiveDisableFallthroughs,
      multiprocessing: inductiveMultiprocessing,
      sortEventsByTimestamp: inductiveSortEventsByTimestamp,
      activityKey: inductiveActivityKey,
      timestampKey: inductiveTimestampKey,
      caseIdKey: inductiveCaseIdKey,
    }) ||
    alphaVariant !== "classic" ||
    alphaSettingsAreDirty({
      activityAttribute: alphaActivityAttribute,
      removeUnconnected: alphaRemoveUnconnected,
      maxRowsToMaterialize: alphaMaxRowsToMaterialize,
    }) ||
    heuristicVariant !== "classic" ||
    heuristicActivityAttribute.trim() !== "concept:name" ||
    heuristicTimestampAttribute.trim() !== "time:timestamp" ||
    dependencyThreshold !== "" ||
    andThreshold !== "" ||
    minEdgeFrequency !== "" ||
    heuristicMinActivityCount !== "" ||
    heuristicDecoration !== "frequency" ||
    heuristicPerformanceAggregation !== "mean" ||
    heuristicBusinessHoursOptionsActive && heuristicBusinessHours ||
    heuristicDfgPreCleaningNoiseThreshold !== "0.05" ||
    heuristicLoopLengthTwoThreshold !== "0.5" ||
    heuristicKeepOncePerCase ||
    heuristicMostCommonPaths.trim() !== "" ||
    heuristicStartTimestampAttribute.trim() !== "" ||
    heuristicLifecycleInstanceAttribute !== "concept:instance";
  const ltlDirty = ltlExact.mode !== "omitted" || heuristicExact.mode !== "omitted" || ltlVariant !== "" || ltlAttributeValues.trim() !== "" ||
    ltlAttributeKey.trim() !== "" || ltlResourceKey.trim() !== "" ||
    !ltlPositive || ltlEnableTimestamp || ltlTimestampBounds.trim() !== "";

  const powlDirty =
    powlVariant !== "maximal" ||
    powlFilteringType !== "decreasing_factor" ||
    powlFilteringThreshold !== "" && powlFilteringThreshold !== "0" ||
    powlOrderFrequencyRatio !== "" && powlOrderFrequencyRatio !== "1" ||
    powlBruteForceActivityLimit !== "" && powlBruteForceActivityLimit !== "0" ||
    powlActivityKey.trim() !== "concept:name" ||
    powlTimestampKey.trim() !== "time:timestamp" ||
    powlCaseIdKey.trim() !== "case:concept:name";
  const tokenReplayDirty =
    Boolean(rootCauseStrings.trim() || rootCauseNumerics.trim()) || rootCauseMultiplier ||
    tokenReplayVariant !== "forward" || tokenReplayStopImmediatelyUnfit ||
    tokenReplayActivityAttribute.trim() !== "" ||
    tokenReplayPoliciesAreDirty(tokenReplayPolicies) || Boolean(tokenReplayMaxRows.trim());
  const logSkeletonDirty =
    logSkeletonNoiseThreshold !== "" ||
    logSkeletonConstraints.size !== logSkeletonFamilies.length ||
    logSkeletonActivityAttribute.trim() !== "concept:name";
  const declareDirty =
    declareActivityAttribute.trim() !== "concept:name" ||
    declareActivitiesMode !== "legacy" || declareConsideredActivities.trim() !== "" ||
    declareAllowedTemplates.size !== declareFamilies.length ||
    declareMinSupportRatio !== "" || declareMinConfidenceRatio !== "" ||
    declareAutoSelectionMultiplier !== "" && declareAutoSelectionMultiplier !== "0.8";
  const footprintsDirty =
    footprintsModel !== "process_tree" || footprintsStrict || !footprintsEnableActAlwaysExecuted ||
    footprintsDiscoveryVariants.size !== FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length ||
    FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.some((name) => !footprintsDiscoveryVariants.has(name)) ||
    footprintsConformanceVariants.size !== 1 || !footprintsConformanceVariants.has("log_model") ||
    footprintsActivityAttribute.trim() !== "concept:name" ||
    footprintsCaseIdAttribute.trim() !== "case:concept:name" ||
    footprintsStartTimestampAttribute.trim() !== "" ||
    footprintsTimestampAttribute.trim() !== "time:timestamp" ||
    footprintsIndexAttribute.trim() !== "@@index" ||
    !footprintsSortRequired || footprintsInitialMarking.trim() !== "";
  const dfgDirty =
    dfgStatisticalVariants.size > 0 || dfgCausalVariants.size > 0 ||
    dfgActivityAttribute.trim() !== "concept:name" ||
    dfgTimestampAttribute.trim() !== "time:timestamp";
  const correlationDirty = correlationVariants.size > 0;
  const spectrumActive = spectrumActivities.trim() !== "";
  const temporalDirty =
    spectrumActive || batches || minimumSelfDistanceVariant !== "log" ||
    temporalActivityAttribute.trim() !== "concept:name" ||
    temporalCaseIdAttribute.trim() !== "case:concept:name" ||
    temporalSourceTimestampAttribute.trim() !== "" ||
    temporalTargetTimestampAttribute.trim() !== "" || temporalBusinessHours ||
    temporalBusinessHourSlots.trim() !== "" || temporalStrictAttributes ||
    transitionActivityKey.trim() !== "concept:name" ||
    transitionCaseIdKey.trim() !== "case:concept:name" ||
    transitionView !== "sequence" ||
    transitionDirection !== "forward" ||
    transitionWindow.trim() !== "2" ||
    transitionIncludeData ||
    minimumSelfDistanceActivityKey.trim() !== "" ||
    minimumSelfDistanceCaseIdKey.trim() !== "" ||
    batchVariant !== "log" && batchEventIdAttribute.trim() !== "" || overlap;
  const formDirty = dirty || powlDirty || tokenReplayDirty || logSkeletonDirty ||
    declareDirty || footprintsDirty || dfgDirty || correlationDirty || temporalDirty || ltlDirty;

  return (
    <section className="filters reveal" onChangeCapture={()=>{if(ltlExactActive||heuristicExact.mode!=="omitted")onExactEdit?.();}}>
      <div className="filters-bar">
        <button
          className={open ? "filters-toggle open" : "filters-toggle"}
          onClick={() => setOpen((v) => !v)}
        >
          <span className="filters-caret">{open ? "▾" : "▸"}</span> Filters &
          miner settings
          {active && <span className="filters-dot" aria-label="filters applied" />}
        </button>
        {mining && <span className="filters-status">re-mining…</span>}
      </div>

      {open && (
        <div className="filters-body">
          <div className="filters-row">
            <label className="filters-field">
              <span>Top variants</span>
              <input
                type="number"
                min={1}
                placeholder="all"
                value={topVariants}
                onChange={(e) => setTopVariants(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Min variant frequency</span>
              <input
                type="number"
                min={2}
                placeholder="none"
                value={minFrequency}
                onChange={(e) => setMinFrequency(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Variant coverage %</span>
              <input
                type="number"
                min={1}
                max={100}
                placeholder="all"
                title="Keep the most frequent variants that together cover this share of cases"
                value={variantCoverage}
                onChange={(e) => setVariantCoverage(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>From</span>
              <input
                type="datetime-local"
                value={timeFrom}
                onChange={(e) => setTimeFrom(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>To</span>
              <input
                type="datetime-local"
                value={timeTo}
                onChange={(e) => setTimeTo(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Time window keeps</span>
              <select
                value={timeMode}
                onChange={(e) => setTimeMode(e.target.value as TimeFilterMode)}
              >
                <option value="events">events in the window</option>
                <option value="traces_contained">cases fully inside</option>
                <option value="traces_intersecting">cases touching it</option>
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>POWL discovery algorithm</span>
              <select value={powlVariant} onChange={(e) => setPowlVariant(e.target.value as typeof powlVariant)}>
                <option value="maximal">maximal (default)</option>
                <option value="tree">tree</option>
                <option value="brute_force">brute force</option>
                <option value="dynamic_clustering">dynamic clustering</option>
                <option value="dynamic_clustering_plain">dynamic clustering (plain)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>POWL filtering</span>
              <select value={powlFilteringType} onChange={(e) => setPowlFilteringType(e.target.value as typeof powlFilteringType)}>
                <option value="decreasing_factor">decreasing factor (default)</option>
                <option value="dynamic">dynamic</option>
              </select>
            </label>
            <label className="filters-field">
              <span>POWL filter threshold</span>
              <input type="number" min={0} max={0.99} step={0.05} placeholder="0" value={powlFilteringThreshold} onChange={(e) => setPowlFilteringThreshold(e.target.value)} />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>POWL order-frequency ratio</span>
              <input type="number" min={0.51} max={1} step={0.01} placeholder="1" value={powlOrderFrequencyRatio} onChange={(e) => setPowlOrderFrequencyRatio(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>POWL brute-force activity limit</span>
              <input type="number" min={0} step={1} placeholder="0 = unlimited" value={powlBruteForceActivityLimit} onChange={(e) => setPowlBruteForceActivityLimit(e.target.value)} />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>POWL activity field</span>
              <input
                value={powlActivityKey}
                placeholder="concept:name"
                onChange={(e) => setPowlActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>POWL timestamp field</span>
              <input
                value={powlTimestampKey}
                placeholder="time:timestamp"
                onChange={(e) => setPowlTimestampKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>POWL case-id field</span>
              <input
                value={powlCaseIdKey}
                placeholder="case:concept:name"
                onChange={(e) => setPowlCaseIdKey(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Token replay</span>
              <select
                value={tokenReplayVariant}
                onChange={(e) =>
                  setTokenReplayVariant(
                    e.target.value as typeof tokenReplayVariant,
                  )
                }
              >
                <option value="forward">forward (default)</option>
                <option value="backwards">backwards</option>
              </select>
            </label>
            <label className="filters-field">
              <span>On an unfit event</span>
              <select
                value={tokenReplayStopImmediatelyUnfit ? "stop" : "continue"}
                onChange={(e) =>
                  setTokenReplayStopImmediatelyUnfit(e.target.value === "stop")
                }
              >
                <option value="continue">continue replay (default)</option>
                <option value="stop">stop the trace immediately</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Activity field</span>
              <input
                value={tokenReplayActivityAttribute}
                placeholder="Automatic (imported activity field)"
                onChange={(e) => setTokenReplayActivityAttribute(e.target.value)}
              />
            </label>
          </div>

          <TokenReplayPolicyControls variant={tokenReplayVariant} policies={tokenReplayPolicies}
            onChange={(key, value) => setTokenReplayPolicies((current) => ({ ...current, [key]: value }))} />
          <TokenReplayRootCauseControls enabled={tokenReplayPolicies.tokenReplayIncludeRootCauseDiagnostics}
            strings={rootCauseStrings} numerics={rootCauseNumerics} multiplier={rootCauseMultiplier}
            onStrings={setRootCauseStrings} onNumerics={setRootCauseNumerics} onMultiplier={setRootCauseMultiplier} />
          <label className="filters-field">
            <span>Token replay timestamp attribute</span>
            <input value={tokenReplayTimestampAttribute} placeholder="time:timestamp"
              disabled={!tokenReplayPolicies.tokenReplayIncludeDurationDiagnostics}
              onChange={(event) => setTokenReplayTimestampAttribute(event.target.value)} />
          </label>
          <label className="filters-field">
            <span>Token replay event cap</span>
            <input type="number" min={0} step={1} placeholder="unlimited"
              value={tokenReplayMaxRows} onChange={(event) => setTokenReplayMaxRows(event.target.value)} />
          </label>

          <div className="filters-row">
            <label className="filters-field">
              <span>Case duration min (seconds)</span>
              <input
                type="number"
                min={0}
                step="any"
                placeholder="0"
                value={caseDurationMin}
                onChange={(e) => setCaseDurationMin(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Case duration max (seconds)</span>
              <input
                type="number"
                min={0}
                step="any"
                placeholder="unbounded"
                value={caseDurationMax}
                onChange={(e) => setCaseDurationMax(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Variant min share %</span>
              <input
                type="number"
                min={0}
                max={100}
                step="any"
                placeholder="none"
                value={variantMinCoverage}
                onChange={(e) => setVariantMinCoverage(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Variant max share %</span>
              <input
                type="number"
                min={0}
                max={100}
                step="any"
                placeholder="none"
                value={variantMaxCoverage}
                onChange={(e) => setVariantMaxCoverage(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Top-variant target %</span>
              <input
                type="number"
                min={0}
                max={100}
                step="any"
                placeholder="none"
                title="Keep enough top variants to reach this case share; ties at the boundary stay included"
                value={variantPercentage}
                onChange={(e) => setVariantPercentage(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Start activity</span>
              <select value={startActivities} onChange={(e) => setStartActivities(e.target.value)}>
                <option value="">any</option>
                {activities.map((activity) => <option key={`start-${activity}`} value={activity}>{activity}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>Start endpoint</span>
              <select value={startActivitiesPositive ? "keep" : "exclude"} disabled={!startActivities}
                onChange={(e) => setStartActivitiesPositive(e.target.value === "keep")}>
                <option value="keep">keep listed</option>
                <option value="exclude">exclude listed</option>
              </select>
            </label>
            <label className="filters-field">
              <span>End activity</span>
              <select value={endActivities} onChange={(e) => setEndActivities(e.target.value)}>
                <option value="">any</option>
                {activities.map((activity) => <option key={`end-${activity}`} value={activity}>{activity}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>End endpoint</span>
              <select value={endActivitiesPositive ? "keep" : "exclude"} disabled={!endActivities}
                onChange={(e) => setEndActivitiesPositive(e.target.value === "keep")}>
                <option value="keep">keep listed</option>
                <option value="exclude">exclude listed</option>
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Top activities</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="all"
                value={maxActivities}
                onChange={(e) => setMaxActivities(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Ranking attribute <small>(blank = activity)</small></span>
              <input
                type="text"
                value={topActivitiesAttributeKey}
                disabled={maxActivities === ""}
                onChange={(e) => setTopActivitiesAttributeKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Relative occurrence stake</span>
              <input
                type="number"
                min={0}
                max={1}
                step="any"
                placeholder="off"
                value={relativeAttributeStake}
                onChange={(e) => setRelativeAttributeStake(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Occurrence attribute <small>(blank = activity)</small></span>
              <input
                type="text"
                value={relativeAttributeKey}
                onChange={(e) => setRelativeAttributeKey(e.target.value)}
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={relativeAttributeKeepOncePerCase}
                onChange={(e) => setRelativeAttributeKeepOncePerCase(e.target.checked)}
              />
              <span>count once per case</span>
            </label>
            <label className="filters-field">
              <span>Absolute occurrence minimum</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="off"
                value={attributeOccurrenceMin}
                onChange={(e) => setAttributeOccurrenceMin(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Threshold attribute <small>(blank = activity)</small></span>
              <input
                type="text"
                value={attributeOccurrenceKey}
                onChange={(e) => setAttributeOccurrenceKey(e.target.value)}
              />
            </label>
          </div>
          {(topActivitiesValidationError || relativeAttributeValidationError ||
            attributeOccurrenceValidationError) && (
            <div className="filters-warning" role="alert">
              {topActivitiesValidationError || relativeAttributeValidationError ||
                attributeOccurrenceValidationError}
            </div>
          )}

          <div className="filters-row">
            <label className="filters-field">
              <span>Maximum cases</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="all"
                value={maxCases}
                onChange={(e) => setMaxCases(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Minimum events per case</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="0"
                value={minLength}
                onChange={(e) => setMinLength(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Maximum events per case</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="unbounded"
                value={maxLength}
                onChange={(e) => setMaxLength(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Log skeleton activity attribute</span>
              <input
                type="text"
                placeholder="concept:name"
                value={logSkeletonActivityAttribute}
                onChange={(event) => setLogSkeletonActivityAttribute(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Log skeleton noise threshold</span>
              <input
                type="number"
                min={0}
                max={1}
                step={0.05}
                placeholder="0 (default)"
                title="Ignore infrequent log-skeleton constraints; 0 keeps every observed constraint"
                value={logSkeletonNoiseThreshold}
                onChange={(event) => setLogSkeletonNoiseThreshold(event.target.value)}
              />
            </label>
            <div className="filters-field filters-field-wide">
              <span title="Constraint families to mine and check">Log skeleton constraints</span>
              <div className="filter-chips" role="group" aria-label="Log skeleton constraints">
                {logSkeletonFamilies.map(([name, label]) => {
                  const selected = logSkeletonConstraints.has(name);
                  return (
                    <button
                      type="button"
                      key={name}
                      className={selected ? "filter-chip active" : "filter-chip"}
                      aria-pressed={selected}
                      title={selected ? `Disable ${label}` : `Enable ${label}`}
                      onClick={() => toggleLogSkeletonConstraint(name)}
                    >
                      {label}
                    </button>
                  );
                })}
              </div>
              <small>At least one family stays selected; all six is the default.</small>
            </div>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Declare activity attribute</span>
              <input
                type="text"
                placeholder="concept:name"
                value={declareActivityAttribute}
                onChange={(event) => setDeclareActivityAttribute(event.target.value)}
              />
            </label>
            <DiscoveryVectorInput label="Declare considered activities" mode={declareActivitiesMode}
              legacyValue={declareConsideredActivities} literalValue={declareActivitiesJson}
              onMode={setDeclareActivitiesMode} onLegacy={setDeclareConsideredActivities}
              onLiteral={setDeclareActivitiesJson} legacyHint="Comma separated; blank infers activities from the source."/>
            <label className="filters-field">
              <span>Declare min support ratio</span>
              <input
                type="number" min={0} max={1} step={0.05} placeholder="auto"
                value={declareMinSupportRatio}
                onChange={(event) => setDeclareMinSupportRatio(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Declare min confidence ratio</span>
              <input
                type="number" min={0} max={1} step={0.05} placeholder="auto"
                value={declareMinConfidenceRatio}
                onChange={(event) => setDeclareMinConfidenceRatio(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span
                title="Used only when both minimum support and confidence are left blank"
              >
                Declare auto-selection multiplier
              </span>
              <input
                type="number" min={0} max={1} step={0.05} placeholder="0.8"
                value={declareAutoSelectionMultiplier}
                disabled={declareMinSupportRatio.trim() !== "" || declareMinConfidenceRatio.trim() !== ""}
                title="Active only when both explicit threshold fields are blank"
                onChange={(event) => setDeclareAutoSelectionMultiplier(event.target.value)}
              />
              {(declareMinSupportRatio.trim() !== "" || declareMinConfidenceRatio.trim() !== "") && (
                <small>Disabled while an explicit support or confidence threshold is set.</small>
              )}
            </label>
          </div>
          <div className="filters-activities">
            <span className="filters-field-label">
              Declare templates <small>(at least one; all eighteen is the default)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Declare templates">
              {declareFamilies.map(([name, label]) => {
                const selected = declareAllowedTemplates.has(name);
                return (
                  <button
                    type="button" key={name}
                    className={selected ? "filter-chip active" : "filter-chip"}
                    aria-pressed={selected}
                    title={selected ? `Disable ${label}` : `Enable ${label}`}
                    onClick={() => toggleDeclareTemplate(name)}
                  >
                    {label}
                  </button>
                );
              })}
            </div>
          </div>

          <div className="filters-activities">
            <span className="filters-field-label">
              Footprints discovery variants <small>(at least one; four common variants are the default)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Footprints discovery variants">
              {FOOTPRINTS_DISCOVERY_VARIANTS.map((name) => {
                const selected = footprintsDiscoveryVariants.has(name);
                const disabled = name === "petri_reach_graph" && footprintsModel === "dfg";
                return (
                  <button type="button" key={name}
                    className={selected ? "filter-chip active" : "filter-chip"}
                    aria-pressed={selected} disabled={disabled}
                    title={disabled ? "Petri reachability requires a Petri-net-backed model" : undefined}
                    onClick={() => toggleFootprintsDiscovery(name)}>
                    {name.replaceAll("_", " ")}
                  </button>
                );
              })}
            </div>
            {footprintsModel === "dfg" && <small>Petri reachability is unavailable with the DFG model.</small>}
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Footprints model</span>
              <select value={footprintsModel} onChange={(event) => selectFootprintsModel(event.target.value as MinerOptions["footprintsModel"])}>
                <option value="process_tree">process tree (default)</option>
                <option value="powl">POWL</option>
                <option value="inductive">inductive</option>
                <option value="alpha">Alpha</option>
                <option value="heuristic">heuristic</option>
                <option value="dfg" disabled={footprintsDiscoveryVariants.has("petri_reach_graph")}>DFG</option>
              </select>
              {footprintsDiscoveryVariants.has("petri_reach_graph") && <small>DFG is disabled while Petri reachability is selected.</small>}
            </label>
            <label className="filters-field">
              <span>Footprints flags</span>
              <label className="checkbox-field">
                <input type="checkbox" checked={footprintsStrict} onChange={(event) => setFootprintsStrict(event.target.checked)} /> strict log-model checks
              </label>
              <label className="checkbox-field">
                <input type="checkbox" checked={footprintsEnableActAlwaysExecuted} onChange={(event) => setFootprintsEnableActAlwaysExecuted(event.target.checked)} /> activity-always-executed checks
              </label>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Footprints activity field</span>
              <input
                value={footprintsActivityAttribute}
                placeholder="concept:name"
                onChange={(event) => setFootprintsActivityAttribute(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Footprints case-id field</span>
              <input
                value={footprintsCaseIdAttribute}
                placeholder="case:concept:name"
                onChange={(event) => setFootprintsCaseIdAttribute(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Footprints completion field</span>
              <input
                value={footprintsTimestampAttribute}
                placeholder="time:timestamp"
                onChange={(event) => setFootprintsTimestampAttribute(event.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Footprints start field <small>(optional)</small></span>
              <input
                value={footprintsStartTimestampAttribute}
                placeholder="none"
                onChange={(event) => setFootprintsStartTimestampAttribute(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Footprints index field <small>(compatibility)</small></span>
              <input
                value={footprintsIndexAttribute}
                placeholder="@@index"
                onChange={(event) => setFootprintsIndexAttribute(event.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Footprints table ordering</span>
              <label className="checkbox-field">
                <input
                  type="checkbox"
                  checked={footprintsSortRequired}
                  onChange={(event) => setFootprintsSortRequired(event.target.checked)}
                /> sort rows by case and timestamp
              </label>
            </label>
            <label className="filters-field">
              <span>Footprints initial marking <small>(optional)</small></span>
              <input
                value={footprintsInitialMarking}
                placeholder="p0,p1,…"
                inputMode="numeric"
                onChange={(event) => setFootprintsInitialMarking(event.target.value)}
              />
            </label>
          </div>
          <div className="filters-activities">
            <span className="filters-field-label">
              Footprints conformance variants <small>(at least one; log-model is the default)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Footprints conformance variants">
              {FOOTPRINTS_CONFORMANCE_VARIANTS.map((name) => {
                const selected = footprintsConformanceVariants.has(name);
                return (
                  <button type="button" key={name}
                    className={selected ? "filter-chip active" : "filter-chip"}
                    aria-pressed={selected} onClick={() => toggleFootprintsConformance(name)}>
                    {name.replaceAll("_", " ")}
                  </button>
                );
              })}
            </div>
          </div>

          <div className="filters-activities">
            <span className="filters-field-label">
              Statistical DFG analyses <small>(optional; select any variants to include)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Statistical DFG variants">
              {dfgStatisticalNames.map((name) => {
                const selected = dfgStatisticalVariants.has(name);
                return <button type="button" key={name}
                  className={selected ? "filter-chip active" : "filter-chip"}
                  aria-pressed={selected}
                  onClick={() => toggleDfgVariant(setDfgStatisticalVariants, name)}>
                  {name.replaceAll("_", " ")}
                </button>;
              })}
            </div>
          </div>
          <div className="filters-activities">
            <span className="filters-field-label">Causal DFG analyses <small>(optional)</small></span>
            <div className="filter-chips" role="group" aria-label="Causal DFG variants">
              {dfgCausalNames.map((name) => {
                const selected = dfgCausalVariants.has(name);
                return <button type="button" key={name}
                  className={selected ? "filter-chip active" : "filter-chip"}
                  aria-pressed={selected}
                  onClick={() => toggleDfgVariant(setDfgCausalVariants, name)}>{name}</button>;
              })}
            </div>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Activity attribute</span>
              <input type="text" value={dfgActivityAttribute}
                disabled={!dfgActivityOptionsActive}
                onChange={(event) => setDfgActivityAttribute(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>Timestamp attribute</span>
              <input type="text" value={dfgTimestampAttribute}
                disabled={!dfgTimestampOptionsActive}
                onChange={(event) => setDfgTimestampAttribute(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>DFG window</span>
              <input type="number" min={0} step={1} value={dfgWindow}
                disabled={!dfgFrequencyOptionsActive}
                onChange={(event) => setDfgWindow(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>Performance aggregation</span>
              <select value={dfgPerformanceAggregation}
                disabled={!dfgPerformanceOptionsActive}
                onChange={(event) => setDfgPerformanceAggregation(event.target.value as NonNullable<MinerOptions["dfgPerformanceAggregation"]>)}>
                {(["mean", "median", "min", "max", "stdev", "sum", "raw_values", "all"] as const).map((name) => <option key={name} value={name}>{name.replaceAll("_", " ")}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>Start timestamp attribute <small>(optional; completion timestamp default)</small></span>
              <input type="text" placeholder="completion timestamp"
                value={dfgStartTimestampAttribute}
                disabled={!dfgPerformanceOptionsActive}
                onChange={(event) => setDfgStartTimestampAttribute(event.target.value)} />
            </label>
            <DiscoveryVectorInput label="DFG case attributes" mode={dfgAttributesMode}
              legacyValue={dfgCaseAttributes} literalValue={dfgAttributesJson}
              onMode={setDfgAttributesMode} onLegacy={setDfgCaseAttributes}
              onLiteral={setDfgAttributesJson} disabled={!dfgCaseAttributeOptionsActive}/>
          </div>
          <div className="filters-row">
            <label className="filters-field filters-field-wide">
              <span>Business-hour slots <small>(weekly seconds, start-end pairs)</small></span>
              <input type="text" placeholder="25200-61200,111600-147600"
                value={dfgBusinessHourSlots}
                disabled={!dfgPerformanceOptionsActive || !dfgBusinessHours}
                onChange={(event) => setDfgBusinessHourSlots(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>DFG flags</span>
              <label className="checkbox-field"><input type="checkbox"
                checked={dfgKeepOncePerCase}
                disabled={!dfgFrequencyOptionsActive}
                onChange={(event) => setDfgKeepOncePerCase(event.target.checked)} /> keep each edge once per case</label>
              <label className="checkbox-field"><input type="checkbox"
                checked={dfgBusinessHours}
                disabled={!dfgPerformanceOptionsActive}
                onChange={(event) => setDfgBusinessHours(event.target.checked)} /> use business hours</label>
              <label className="checkbox-field"><input type="checkbox"
                checked={dfgReturnNodeAttributes}
                disabled={!dfgCaseAttributeOptionsActive}
                onChange={(event) => setDfgReturnNodeAttributes(event.target.checked)} /> return node attributes</label>
            </label>
          </div>

          <div className="filters-subhead">Trace-pattern filter</div>
          <div className="filters-row">
            <label className="filters-field filters-field-wide">
              <span>Admitted pattern <small>(comma-separated; ... is a wildcard)</small></span>
              <input
                type="text"
                placeholder="..., A, B, ..."
                value={tracePattern}
                onChange={(e) => setTracePattern(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Match polarity</span>
              <select
                value={tracePatternPositive ? "positive" : "negative"}
                disabled={!tracePattern.trim()}
                onChange={(e) => setTracePatternPositive(e.target.value === "positive")}
              >
                <option value="positive">positive (keep matches)</option>
                <option value="negative">negative (drop matches)</option>
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field filters-field-wide">
              <span>Admitted variants <small>(exact; comma-separated activities, semicolon-separated variants; [] is empty)</small></span>
              <input
                type="text"
                placeholder="A,B;C;[]"
                value={admittedVariants}
                onChange={(e) => setAdmittedVariants(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Variant polarity</span>
              <select
                value={admittedVariantsPositive ? "positive" : "negative"}
                disabled={!admittedVariants.trim()}
                onChange={(e) => setAdmittedVariantsPositive(e.target.value === "positive")}
              >
                <option value="positive">positive (keep matches)</option>
                <option value="negative">negative (drop matches)</option>
              </select>
            </label>
          </div>

          <div className="filters-activities">
            <span className="filters-field-label">
              Correlation mining <small>(optional; select any variants to include)</small>
            </span>
            <div className="filter-chips" role="group" aria-label="Correlation-miner variants">
              {correlationNames.map((name) => {
                const selected = correlationVariants.has(name);
                return <button type="button" key={name}
                  className={selected ? "filter-chip active" : "filter-chip"}
                  aria-pressed={selected}
                  onClick={() => toggleDfgVariant(setCorrelationVariants, name)}>
                  {name.replaceAll("_", " ")}
                </button>;
              })}
            </div>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Correlation activity attribute</span>
              <input type="text" placeholder="concept:name"
                value={correlationActivityAttribute}
                disabled={correlationVariants.size === 0}
                onChange={(event) =>
                  setCorrelationActivityAttribute(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>Correlation completion timestamp attribute</span>
              <input type="text" placeholder="time:timestamp"
                value={correlationTimestampAttribute}
                disabled={correlationVariants.size === 0}
                onChange={(event) =>
                  setCorrelationTimestampAttribute(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>Correlation case-id attribute</span>
              <input type="text" placeholder="case:concept:name"
                value={correlationCaseIdAttribute}
                disabled={correlationVariants.size === 0}
                onChange={(event) =>
                  setCorrelationCaseIdAttribute(event.target.value)} />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Correlation start timestamp attribute</span>
              <input type="text" placeholder="complete timestamp"
                value={correlationStartTimestampAttribute}
                disabled={correlationVariants.size === 0}
                onChange={(event) =>
                  setCorrelationStartTimestampAttribute(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>Split sample size</span>
              <input type="number" min={1} step={1}
                value={correlationSampleSize}
                disabled={!correlationSplitOptionsActive}
                onChange={(event) => setCorrelationSampleSize(event.target.value)} />
            </label>
            <label className="filters-field">
              <span>Correlation flags</span>
              <label className="checkbox-field"><input type="checkbox"
                checked={correlationExactTimeMatching}
                disabled={!correlationExactOptionsActive}
                onChange={(event) =>
                  setCorrelationExactTimeMatching(event.target.checked)} /> exact time matching</label>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Rework: activity repeats</span>
              <select
                value={reworkActivity}
                onChange={(e) => setReworkActivity(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>at least</span>
              <input
                type="number"
                min={2}
                placeholder="2"
                value={reworkMin}
                disabled={reworkActivity === ""}
                onChange={(e) => setReworkMin(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Cut cases from</span>
              <select
                value={betweenStart}
                onChange={(e) => setBetweenStart(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>to</span>
              <select
                value={betweenEnd}
                onChange={(e) => setBetweenEnd(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Head anchor</span>
              <select
                value={prefixActivity}
                onChange={(e) => setPrefixActivity(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>Tail anchor</span>
              <select
                value={suffixActivity}
                onChange={(e) => setSuffixActivity(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Head cut</span>
              <select
                value={prefixStrict ? "strict" : "non-strict"}
                disabled={!prefixActivity}
                onChange={(e) => setPrefixStrict(e.target.value === "strict")}
              >
                <option value="strict">include anchor</option>
                <option value="non-strict">exclude anchor</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Head occurrence</span>
              <select
                value={prefixOccurrence}
                disabled={!prefixActivity}
                onChange={(e) => setPrefixOccurrence(e.target.value as "first" | "last")}
              >
                <option value="first">first</option>
                <option value="last">last</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Tail cut</span>
              <select
                value={suffixStrict ? "strict" : "non-strict"}
                disabled={!suffixActivity}
                onChange={(e) => setSuffixStrict(e.target.value === "strict")}
              >
                <option value="strict">include anchor</option>
                <option value="non-strict">exclude anchor</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Tail occurrence</span>
              <select
                value={suffixOccurrence}
                disabled={!suffixActivity}
                onChange={(e) => setSuffixOccurrence(e.target.value as "first" | "last")}
              >
                <option value="first">first</option>
                <option value="last">last</option>
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Split cases at</span>
              <select
                value={activitySplitActivity}
                onChange={(e) => setActivitySplitActivity(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>{name}</option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>split mode</span>
              <select
                value={activitySplitCutMode}
                disabled={!activitySplitActivity}
                onChange={(e) => setActivitySplitCutMode(e.target.value as "this" | "next")}
              >
                <option value="this">anchor starts new case</option>
                <option value="next">following event starts new case</option>
              </select>
            </label>
            <label className="filters-field">
              <span>case-id separator</span>
              <input
                type="text"
                value={activitySplitSeparator}
                disabled={!activitySplitActivity}
                onChange={(e) => setActivitySplitSeparator(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Repeated value</span>
              <select
                value={repetitionValue}
                onChange={(e) => setRepetitionValue(e.target.value)}
              >
                <option value="">off</option>
                {activities.map((name) => (
                  <option key={name} value={name}>{name}</option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>attribute key <small>(blank = activity)</small></span>
              <input
                type="text"
                value={repetitionAttributeKey}
                disabled={!repetitionValue}
                onChange={(e) => setRepetitionAttributeKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>min repetitions</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="2"
                value={repetitionMin}
                disabled={!repetitionValue}
                onChange={(e) => setRepetitionMin(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>max repetitions</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="unbounded"
                value={repetitionMax}
                disabled={!repetitionValue}
                onChange={(e) => setRepetitionMax(e.target.value)}
              />
            </label>
          </div>
          {repetitionValidationError && (
            <div className="filters-warning" role="alert">{repetitionValidationError}</div>
          )}
          {caseGroupingValidationError && (
            <div className="filters-warning" role="alert">{caseGroupingValidationError}</div>
          )}

          <div className="filters-row">
            <label className="filters-field">
              <span>Consecutive activity groups</span>
              <select
                value={consecutiveGrouping}
                onChange={(e) => setConsecutiveGrouping(e.target.value as "" | "first" | "last")}
              >
                <option value="">off</option>
                <option value="first">keep first event</option>
                <option value="last">keep last event</option>
              </select>
            </label>
            <label className="filters-field">
              <span>activity key <small>(blank = activity)</small></span>
              <input
                type="text"
                value={consecutiveGroupingActivityKey}
                disabled={!consecutiveGrouping}
                onChange={(e) => setConsecutiveGroupingActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Same-timestamp groups</span>
              <select
                value={timestampGrouping}
                onChange={(e) => setTimestampGrouping(e.target.value as "" | "first" | "last" | "concat")}
              >
                <option value="">off</option>
                <option value="first">keep first event</option>
                <option value="last">keep last event</option>
                <option value="concat">concatenate activities</option>
              </select>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>group activity key</span>
              <input
                type="text"
                value={timestampGroupingActivityKey}
                disabled={!timestampGrouping}
                onChange={(e) => setTimestampGroupingActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>group timestamp key</span>
              <input
                type="text"
                value={timestampGroupingTimestampKey}
                disabled={!timestampGrouping}
                onChange={(e) => setTimestampGroupingTimestampKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>concat separator</span>
              <input
                type="text"
                value={timestampGroupingSeparator}
                disabled={timestampGrouping !== "concat"}
                onChange={(e) => setTimestampGroupingSeparator(e.target.value)}
              />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Segment head</span>
              <select value={segmentPrefixActivity} onChange={(e) => setSegmentPrefixActivity(e.target.value)}>
                <option value="">off</option>
                {activities.map((name) => <option key={name} value={name}>{name}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>occurrence</span>
              <select value={segmentPrefixOccurrence} onChange={(e) => setSegmentPrefixOccurrence(e.target.value as "first" | "last")} disabled={!segmentPrefixActivity}>
                <option value="first">first</option><option value="last">last</option>
              </select>
            </label>
            <label className="filters-field">
              <span>strict</span>
              <input type="checkbox" checked={segmentPrefixStrict} onChange={(e) => setSegmentPrefixStrict(e.target.checked)} disabled={!segmentPrefixActivity} />
            </label>
            <label className="filters-field">
              <span>Segment tail</span>
              <select value={segmentSuffixActivity} onChange={(e) => setSegmentSuffixActivity(e.target.value)}>
                <option value="">off</option>
                {activities.map((name) => <option key={name} value={name}>{name}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>occurrence</span>
              <select value={segmentSuffixOccurrence} onChange={(e) => setSegmentSuffixOccurrence(e.target.value as "first" | "last")} disabled={!segmentSuffixActivity}>
                <option value="first">first</option><option value="last">last</option>
              </select>
            </label>
            <label className="filters-field">
              <span>strict</span>
              <input type="checkbox" checked={segmentSuffixStrict} onChange={(e) => setSegmentSuffixStrict(e.target.checked)} disabled={!segmentSuffixActivity} />
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Segment between</span>
              <select value={segmentBetweenStart} onChange={(e) => setSegmentBetweenStart(e.target.value)}>
                <option value="">off</option>
                {activities.map((name) => <option key={name} value={name}>{name}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>and</span>
              <select value={segmentBetweenEnd} onChange={(e) => setSegmentBetweenEnd(e.target.value)}>
                <option value="">off</option>
                {activities.map((name) => <option key={name} value={name}>{name}</option>)}
              </select>
            </label>
          </div>

          <div className="filters-subhead">Adjacent path filter</div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Predecessor</span>
              <select value={pathPredecessor} onChange={(e) => setPathPredecessor(e.target.value)}>
                <option value="">off</option>
                {activities.map((name) => <option key={name} value={name}>{name}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>Successor</span>
              <select value={pathSuccessor} onChange={(e) => setPathSuccessor(e.target.value)}>
                <option value="">off</option>
                {activities.map((name) => <option key={name} value={name}>{name}</option>)}
              </select>
            </label>
            <label className="filters-field">
              <span>Min seconds</span>
              <input type="number" step="any" value={pathMinPerformance} onChange={(e) => setPathMinPerformance(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Max seconds</span>
              <input type="number" step="any" value={pathMaxPerformance} onChange={(e) => setPathMaxPerformance(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Activity key</span>
              <input placeholder="concept:name" value={pathActivityAttribute} onChange={(e) => setPathActivityAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Timestamp key</span>
              <input placeholder="time:timestamp" value={pathTimestampAttribute} onChange={(e) => setPathTimestampAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Match polarity</span>
              <select value={pathPositive ? "positive" : "negative"} onChange={(e) => setPathPositive(e.target.value === "positive")}>
                <option value="positive">positive (keep matches)</option>
                <option value="negative">negative (drop matches)</option>
              </select>
            </label>
          </div>

          <div className="filters-row">
            <label className="filters-field">
              <span>Succession</span>
              <select
                value={successionAction}
                onChange={(e) =>
                  setSuccessionAction(e.target.value as typeof successionAction)
                }
              >
                <option value="off">off</option>
                <option value="keep">keep cases where</option>
                <option value="remove">drop cases where</option>
              </select>
            </label>
            <label className="filters-field">
              <span>activity</span>
              <select
                value={successionFrom}
                disabled={successionAction === "off"}
                onChange={(e) => setSuccessionFrom(e.target.value)}
              >
                <option value="">—</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
            <label className="filters-field">
              <span>is</span>
              <select
                value={successionRelation}
                disabled={successionAction === "off"}
                onChange={(e) =>
                  setSuccessionRelation(
                    e.target.value as typeof successionRelation,
                  )
                }
              >
                <option value="directly">directly followed by</option>
                <option value="eventually">eventually followed by</option>
              </select>
            </label>
            <label className="filters-field">
              <span>activity</span>
              <select
                value={successionTo}
                disabled={successionAction === "off"}
                onChange={(e) => setSuccessionTo(e.target.value)}
              >
                <option value="">—</option>
                {activities.map((name) => (
                  <option key={name} value={name}>
                    {name}
                  </option>
                ))}
              </select>
            </label>
          </div>

          <div className="filters-activities">
            <span className="filters-field-label">
              Activities <small>(click to drop)</small>
            </span>
            <div className="filters-chips">
              {activities.map((name) => {
                const dropped = removed.has(name);
                return (
                  <button
                    key={name}
                    className={dropped ? "filters-chip dropped" : "filters-chip"}
                    onClick={() => toggleActivity(name)}
                    title={dropped ? `Add ${name} back` : `Drop ${name}`}
                  >
                    {name}
                  </button>
                );
              })}
            </div>
          </div>

          {resources.length > 0 && (
            <div className="filters-activities">
              <span className="filters-field-label">
                Resources <small>(click to drop)</small>
              </span>
              <div className="filters-chips">
                {resources.map((name) => {
                  const dropped = removedResources.has(name);
                  return (
                    <button
                      key={name}
                      className={dropped ? "filters-chip dropped" : "filters-chip"}
                      onClick={() => toggleResource(name)}
                      title={dropped ? `Add ${name} back` : `Drop ${name}`}
                    >
                      {name}
                    </button>
                  );
                })}
              </div>
            </div>
          )}

          {lifecycles.length > 0 && (
            <div className="filters-activities">
              <span className="filters-field-label">
                Lifecycle transitions{" "}
                <small>(click to drop; keep only “complete” to make a
                start/complete log atomic)</small>
              </span>
              <div className="filters-chips">
                {lifecycles.map((name) => {
                  const dropped = removedLifecycles.has(name);
                  return (
                    <button
                      key={name}
                      className={dropped ? "filters-chip dropped" : "filters-chip"}
                      onClick={() => toggleLifecycle(name)}
                      title={dropped ? `Add ${name} back` : `Drop ${name}`}
                    >
                      {name}
                    </button>
                  );
                })}
              </div>
            </div>
          )}

          {attributes.length > 0 && (
            <>
              <div className="filters-row">
                <label className="filters-field">
                  <span>Attribute</span>
                  <select
                    value={attributeKey}
                    onChange={(e) => pickAttributeKey(e.target.value)}
                  >
                    <option value="">off</option>
                    {attributes.map((attribute) => (
                      <option key={attribute.key} value={attribute.key}>
                        {attribute.key}
                      </option>
                    ))}
                  </select>
                </label>
                {selectedAttribute && (
                  <label className="filters-field">
                    <span>Dropping a value removes</span>
                    <select
                      value={attributeMode}
                      onChange={(e) =>
                        setAttributeMode(e.target.value as AttributeFilterMode)
                      }
                    >
                      <option value="events">the matching events</option>
                      <option value="traces">whole cases with a match</option>
                    </select>
                  </label>
                )}
              </div>
              {selectedAttribute && (
                <div className="filters-activities">
                  <span className="filters-field-label">
                    {selectedAttribute.key} values{" "}
                    <small>
                      (click to drop
                      {selectedAttribute.values.length <
                        selectedAttribute.distinct_values &&
                        `; top ${selectedAttribute.values.length} of ${selectedAttribute.distinct_values} shown`}
                      )
                    </small>
                  </span>
                  <div className="filters-chips">
                    {selectedAttribute.values.map(({ value, count }) => {
                      const dropped = removedValues.has(value);
                      return (
                        <button
                          key={value}
                          className={
                            dropped ? "filters-chip dropped" : "filters-chip"
                          }
                          onClick={() => toggleValue(value)}
                          title={
                            dropped
                              ? `Add ${value} back`
                              : `Drop ${value} (${count} events)`
                          }
                        >
                          {value}
                        </button>
                      );
                    })}
                  </div>
                </div>
              )}
            </>
          )}

          {attributes.length > 0 && (
            <div className="filters-row">
              <label className="filters-field">
                <span>Numeric attribute</span>
                <select
                  value={numericAttributeKey}
                  onChange={(e) => setNumericAttributeKey(e.target.value)}
                >
                  <option value="">off</option>
                  {attributes.map((attribute) => (
                    <option key={attribute.key} value={attribute.key}>
                      {attribute.key}
                    </option>
                  ))}
                </select>
              </label>
              <label className="filters-field">
                <span>Numeric range applies to</span>
                <select
                  value={numericAttributeMode}
                  onChange={(e) =>
                    setNumericAttributeMode(e.target.value as AttributeFilterMode)
                  }
                >
                  <option value="events">matching events</option>
                  <option value="traces">whole cases with a match</option>
                </select>
              </label>
              <label className="filters-field">
                <span>Numeric polarity</span>
                <select
                  value={numericAttributePositive ? "keep" : "remove"}
                  onChange={(e) => setNumericAttributePositive(e.target.value === "keep")}
                >
                  <option value="keep">keep in-range</option>
                  <option value="remove">keep outside range</option>
                </select>
              </label>
            </div>
          )}
          {numericAttributeKey && (
            <div className="filters-row">
              <label className="filters-field">
                <span>Numeric minimum</span>
                <input
                  type="number"
                  step="any"
                  placeholder="unbounded"
                  value={numericAttributeMin}
                  onChange={(e) => setNumericAttributeMin(e.target.value)}
                />
              </label>
              <label className="filters-field">
                <span>Numeric maximum</span>
                <input
                  type="number"
                  step="any"
                  placeholder="unbounded"
                  value={numericAttributeMax}
                  onChange={(e) => setNumericAttributeMax(e.target.value)}
                />
              </label>
            </div>
          )}

          {caseAttributes.length > 0 && (
            <>
              <div className="filters-row">
                <label className="filters-field">
                  <span>Case attribute</span>
                  <select
                    value={caseAttributeKey}
                    onChange={(e) => pickCaseAttributeKey(e.target.value)}
                  >
                    <option value="">off</option>
                    {caseAttributes.map((attribute) => (
                      <option key={attribute.key} value={attribute.key}>
                        {attribute.key}
                      </option>
                    ))}
                  </select>
                </label>
              </div>
              {selectedCaseAttribute && (
                <div className="filters-activities">
                  <span className="filters-field-label">
                    {selectedCaseAttribute.key} values{" "}
                    <small>
                      (click to drop the cases
                      {selectedCaseAttribute.values.length <
                        selectedCaseAttribute.distinct_values &&
                        `; top ${selectedCaseAttribute.values.length} of ${selectedCaseAttribute.distinct_values} shown`}
                      )
                    </small>
                  </span>
                  <div className="filters-chips">
                    {selectedCaseAttribute.values.map(({ value, count }) => {
                      const dropped = removedCaseValues.has(value);
                      return (
                        <button
                          key={value}
                          className={
                            dropped ? "filters-chip dropped" : "filters-chip"
                          }
                          onClick={() => toggleCaseValue(value)}
                          title={
                            dropped
                              ? `Add ${value} back`
                              : `Drop ${value} (${count} cases)`
                          }
                        >
                          {value}
                        </button>
                      );
                    })}
                  </div>
                </div>
              )}
            </>
          )}

          <div className="filters-activities">
            <span className="filters-field-label">
              Miner settings <small>(empty = default)</small>
            </span>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Inductive: noise threshold (IMf only)</span>
              <input
                type="number"
                min={0}
                max={1}
                step={0.05}
                placeholder="0 = exact"
                value={noiseThreshold}
                onChange={(e) => setNoiseThreshold(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Inductive: recursion</span>
              <select
                value={inductiveVariant}
                onChange={(e) =>
                  setInductiveVariant(e.target.value as "log" | "imf" | "dfg")
                }
              >
                <option value="log">split the log (default)</option>
                <option value="imf">infrequent (IMf)</option>
                <option value="dfg">graph only (IMd, huge logs)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Alpha variant</span>
              <select
                value={alphaVariant}
                onChange={(e) =>
                  setAlphaVariant(e.target.value as "plus" | "classic")
                }
              >
                <option value="classic">classic (default, textbook)</option>
                <option value="plus">Alpha+ (short loops)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Alpha: activity field</span>
              <input
                type="text"
                placeholder="concept:name"
                value={alphaActivityAttribute}
                onChange={(e) => setAlphaActivityAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={alphaRemoveUnconnected}
                onChange={(e) => setAlphaRemoveUnconnected(e.target.checked)}
              />
              <span>Alpha: remove unconnected transitions</span>
            </label>
            <label className="filters-field">
              <span>Alpha: event cap</span>
              <input
                type="number"
                min={0}
                step={1}
                placeholder="unlimited"
                value={alphaMaxRowsToMaterialize}
                onChange={(e) => setAlphaMaxRowsToMaterialize(e.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Inductive: activity field</span>
              <input
                type="text"
                placeholder="concept:name"
                value={inductiveActivityKey}
                onChange={(e) => setInductiveActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Inductive: timestamp field</span>
              <input
                type="text"
                placeholder="time:timestamp"
                value={inductiveTimestampKey}
                onChange={(e) => setInductiveTimestampKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Inductive: case-id field</span>
              <input
                type="text"
                placeholder="case:concept:name"
                value={inductiveCaseIdKey}
                onChange={(e) => setInductiveCaseIdKey(e.target.value)}
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={inductiveDisableStrictSequenceCut}
                onChange={(e) => setInductiveDisableStrictSequenceCut(e.target.checked)}
              />
              <span>Disable strict sequence cut</span>
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={inductiveDisableFallthroughs}
                onChange={(e) => setInductiveDisableFallthroughs(e.target.checked)}
              />
              <span>Disable specialized fall-throughs</span>
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={inductiveMultiprocessing}
                onChange={(e) => setInductiveMultiprocessing(e.target.checked)}
              />
              <span>Parallelize large fall-through searches</span>
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={inductiveSortEventsByTimestamp}
                onChange={(e) => setInductiveSortEventsByTimestamp(e.target.checked)}
              />
              <span>Sort events by timestamp</span>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Heuristic variant</span>
              <select
                value={heuristicVariant}
                onChange={(e) =>
                  setHeuristicVariant(
                    e.target.value as "classic" | "plusplus",
                  )
                }
              >
                <option value="classic">classic (default)</option>
                <option value="plusplus">Miner++ (interval concurrency)</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Heuristic: activity field</span>
              <input
                type="text"
                placeholder="concept:name"
                value={heuristicActivityAttribute}
                onChange={(e) => setHeuristicActivityAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: completion-time field</span>
              <input
                type="text"
                placeholder="time:timestamp"
                value={heuristicTimestampAttribute}
                onChange={(e) => setHeuristicTimestampAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: dependency threshold</span>
              <input
                type="number"
                min={-1}
                max={1}
                step={0.05}
                placeholder="0.5"
                value={dependencyThreshold}
                onChange={(e) => setDependencyThreshold(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: AND threshold</span>
              <input
                type="number"
                min={0}
                step={0.05}
                placeholder="0.65"
                value={andThreshold}
                onChange={(e) => setAndThreshold(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: min path frequency</span>
              <input
                type="number"
                min={1}
                placeholder="1"
                value={minEdgeFrequency}
                onChange={(e) => setMinEdgeFrequency(e.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Heuristic: min activity count</span>
              <input
                type="number"
                min={1}
                placeholder="1"
                value={heuristicMinActivityCount}
                onChange={(e) => setHeuristicMinActivityCount(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic decoration</span>
              <select
                value={heuristicDecoration}
                onChange={(e) =>
                  setHeuristicDecoration(
                    e.target.value as "frequency" | "performance",
                  )
                }
              >
                <option value="frequency">frequency (default)</option>
                <option value="performance">performance</option>
              </select>
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={heuristicBusinessHours}
                disabled={!heuristicBusinessHoursOptionsActive}
                onChange={(e) => setHeuristicBusinessHours(e.target.checked)}
              />
              <span>Use business hours for performance/sojourn</span>
            </label>
            <label className="filters-field filters-field-wide">
              <span>Heuristic business-hour slots <small>(weekly seconds, start-end pairs)</small></span>
              <input
                type="text"
                placeholder="none or 25200-61200,..."
                value={heuristicBusinessHourSlots}
                disabled={!heuristicBusinessHoursOptionsActive || !heuristicBusinessHours}
                onChange={(e) => setHeuristicBusinessHourSlots(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Heuristic: start-time attribute</span>
              <input
                type="text"
                placeholder={heuristicStartTimestampPlaceholder(
                  heuristicVariant,
                  heuristicTimestampAttribute,
                )}
                value={heuristicStartTimestampAttribute}
                onChange={(e) =>
                  setHeuristicStartTimestampAttribute(e.target.value)
                }
              />
            </label>
            <label className="filters-field">
              <span>Miner++: lifecycle instance</span>
              <input
                type="text"
                value={heuristicLifecycleInstanceAttribute}
                disabled={heuristicVariant !== "plusplus"}
                onChange={(e) =>
                  setHeuristicLifecycleInstanceAttribute(e.target.value)
                }
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Classic: DFG pre-clean noise</span>
              <input
                type="number"
                min={0}
                step={0.01}
                value={heuristicDfgPreCleaningNoiseThreshold}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) =>
                  setHeuristicDfgPreCleaningNoiseThreshold(e.target.value)
                }
              />
            </label>
            <label className="filters-field">
              <span>Classic: length-two loop threshold</span>
              <input
                type="number"
                min={0}
                max={1}
                step={0.05}
                value={heuristicLoopLengthTwoThreshold}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) =>
                  setHeuristicLoopLengthTwoThreshold(e.target.value)
                }
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={heuristicKeepOncePerCase}
                disabled={heuristicVariant !== "classic"}
                onChange={(e) => setHeuristicKeepOncePerCase(e.target.checked)}
              />
              <span>Classic: one path count per case</span>
            </label>
            <label className="filters-field">
              <span>Classic: pre-clean path exemptions</span>
              <input
                type="text"
                placeholder="A -> B, C -> D"
                value={heuristicMostCommonPaths}
                disabled={heuristicVariant !== "classic" || heuristicExact.mode !== "omitted"}
                onChange={(e) => setHeuristicMostCommonPaths(e.target.value)}
              />
            </label>
          </div>
          <label className="filters-field"><span>Exact Heuristic container</span><select value={heuristicExactKind} onChange={e=>{onExactEdit?.();setHeuristicExactKind(e.target.value as "pairs"|"options");}}><option value="pairs">Literal protected path pairs</option><option value="options">All 24 native options</option></select></label>
          <ExactLiteralInput label={heuristicExactKind==="options"?"Complete 24-field Heuristic options":"Heuristic literal protected path pairs (CLASSIC use; PLUSPLUS retains state)"} value={heuristicExact} onChange={v=>{onExactEdit?.();(heuristicExactKind==="options"?setHeuristicOptionsExact:setPathsExact)(v);}}/>
          {heuristicFullExact&&<p>Complete native options supply every Heuristic setting, including inactive state. Scalar Heuristic controls below and above are retained locally and omitted from this request.</p>}

          <div className="filters-row">
            <label className="filters-field">
              <span>Performance aggregation</span>
              <select
                value={heuristicPerformanceAggregation}
                disabled={heuristicDecoration !== "performance"}
                onChange={(e) =>
                  setHeuristicPerformanceAggregation(
                    e.target.value as
                      | "mean"
                      | "median"
                      | "min"
                      | "max"
                      | "stdev"
                      | "sum"
                      | "raw_values"
                      | "all",
                  )
                }
              >
                <option value="mean">mean (default)</option>
                <option value="median">median</option>
                <option value="min">minimum</option>
                <option value="max">maximum</option>
                <option value="stdev">standard deviation</option>
                <option value="sum">sum</option>
                <option value="raw_values">raw values</option>
                <option value="all">all summaries</option>
              </select>
            </label>
          </div>

          <div className="filters-subhead">LTL trace filter</div>
          <label className="filters-field"><span>Exact LTL container</span><select value={ltlExactKind} onChange={e=>{const kind=e.target.value as "values"|"options";onExactEdit?.();setLtlExactKind(kind);}}><option value="values">Ordered values with scalar options</option><option value="options">All seven native options</option></select></label>
          <ExactLiteralInput label={ltlExactKind==="options"?"Complete seven-field LTL options":"Ordered LTL values"} value={ltlExact} onChange={setLtlExact}/>
          <fieldset disabled={ltlFullExact}>
          <div className="filters-row">
            <label className="filters-field">
              <span>Predicate variant</span>
              <select value={ltlVariant} onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlVariant(e.target.value as LtlVariant | "");}}>
                <option value="">off</option>
                <option value="eventually_follows">eventually follows</option>
                <option value="a_next_b_next_c">A next B next C</option>
                <option value="four_eyes_principle">four-eyes principle</option>
                <option value="attr_value_different_persons">different persons</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Attribute key <small>(blank = activity)</small></span>
              <input type="text" value={ltlAttributeKey} disabled={!ltlVariant && !ltlExactActive}
                onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlAttributeKey(e.target.value);}} />
            </label>
            <label className="filters-field filters-field-wide">
              <span>Ordered values <small>(comma-separated)</small></span>
              <input type="text" placeholder="A, B" value={ltlAttributeValues} disabled={!ltlVariant || ltlExactActive}
                onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlAttributeValues(e.target.value);}} />
            </label>
            <label className="filters-field">
              <span>Match polarity</span>
              <select value={ltlPositive ? "positive" : "negative"} disabled={!ltlVariant && !ltlExactActive}
                onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlPositive(e.target.value === "positive");}}>
                <option value="positive">positive (keep matches)</option>
                <option value="negative">negative (drop matches)</option>
              </select>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Resource key <small>(blank = resource)</small></span>
              <input type="text" value={ltlResourceKey} disabled={!ltlExactActive && (!ltlVariant || ltlVariant === "eventually_follows" || ltlVariant === "a_next_b_next_c")}
                onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlResourceKey(e.target.value);}} />
            </label>
            <label className="filters-field filters-check">
              <input type="checkbox" checked={ltlEnableTimestamp}
                disabled={!ltlExactActive && (!ltlVariant || ltlVariant !== "eventually_follows")}
                onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlEnableTimestamp(e.target.checked);}} />
              <span>Enable timestamp bounds</span>
            </label>
            <label className="filters-field filters-field-wide">
              <span>Timestamp bounds <small>(min:max pairs)</small></span>
              <input type="text" placeholder="0:10,0:30" value={ltlTimestampBounds}
                disabled={!ltlEnableTimestamp && !ltlExactActive} onChange={(e) => {if(ltlExactActive)onExactEdit?.();setLtlTimestampBounds(e.target.value);}} />
            </label>
          </div>

          </fieldset>
          <div className="filters-subhead">Temporal &amp; behavioral discovery</div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Temporal activity attribute</span>
              <input type="text" placeholder="concept:name"
                value={temporalActivityAttribute}
                onChange={(e) => setTemporalActivityAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Temporal flat case-id attribute</span>
              <input type="text" placeholder="case:concept:name"
                value={temporalCaseIdAttribute}
                onChange={(e) => setTemporalCaseIdAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Temporal source completion attribute</span>
              <input type="text" placeholder="time:timestamp (default)"
                value={temporalSourceTimestampAttribute}
                onChange={(e) => setTemporalSourceTimestampAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Temporal target start attribute</span>
              <input type="text" placeholder="completion time (default)"
                value={temporalTargetTimestampAttribute}
                onChange={(e) => setTemporalTargetTimestampAttribute(e.target.value)} />
            </label>
            <label className="filters-field filters-check">
              <input type="checkbox" checked={temporalBusinessHours}
                onChange={(e) => setTemporalBusinessHours(e.target.checked)} />
              <span>Use temporal business hours</span>
            </label>
            <label className="filters-field">
              <span>Temporal business-hour slots</span>
              <input type="text" placeholder="none or 25200-61200,..."
                value={temporalBusinessHourSlots}
                disabled={!temporalBusinessHours}
                onChange={(e) => setTemporalBusinessHourSlots(e.target.value)} />
            </label>
            <label className="filters-field filters-check">
              <input type="checkbox" checked={temporalStrictAttributes}
                onChange={(e) => setTemporalStrictAttributes(e.target.checked)} />
              <span>Strict temporal attributes</span>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Transition-system activity attribute</span>
              <input
                type="text"
                placeholder="concept:name"
                value={transitionActivityKey}
                onChange={(e) => setTransitionActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Transition-system case-id attribute</span>
              <input
                type="text"
                placeholder="case:concept:name"
                value={transitionCaseIdKey}
                onChange={(e) => setTransitionCaseIdKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Transition-system view</span>
              <select
                value={transitionView}
                onChange={(e) => setTransitionView(e.target.value as typeof transitionView)}
              >
                <option value="sequence">sequence (default)</option>
                <option value="set">set</option>
                <option value="multiset">multiset</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Transition-system direction</span>
              <select
                value={transitionDirection}
                onChange={(e) => setTransitionDirection(e.target.value as typeof transitionDirection)}
              >
                <option value="forward">forward (default)</option>
                <option value="backward">backward</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Transition-system window</span>
              <input
                type="number"
                min={0}
                step={1}
                value={transitionWindow}
                onChange={(e) => setTransitionWindow(e.target.value)}
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={transitionIncludeData}
                onChange={(e) => setTransitionIncludeData(e.target.checked)}
              />
              <span>Include transition occurrence data</span>
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field">
              <span>Performance spectrum activities</span>
              <input
                type="text"
                placeholder="A, B, C (optional)"
                value={spectrumActivities}
                onChange={(e) => setSpectrumActivities(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Spectrum variant</span>
              <select
                value={spectrumVariant}
                disabled={spectrumActivities.split(",").map((name) => name.trim()).filter(Boolean).length < 2}
                onChange={(e) => setSpectrumVariant(e.target.value as typeof spectrumVariant)}
              >
                <option value="connected">connected (default)</option>
                <option value="disconnected">disconnected</option>
                <option value="log">log</option>
                <option value="dataframe">dataframe</option>
                <option value="log_disconnected">log disconnected</option>
                <option value="dataframe_disconnected">dataframe disconnected</option>
                <option value="lazyframe">lazyframe</option>
                <option value="lazyframe_disconnected">lazyframe disconnected</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Spectrum sample size</span>
              <input
                type="number"
                min={0}
                step={1}
                value={spectrumSampleSize}
                disabled={spectrumActivities.split(",").map((name) => name.trim()).filter(Boolean).length < 2}
                onChange={(e) => setSpectrumSampleSize(e.target.value)}
              />
            </label>
            <label className="filters-field filters-check">
              <input
                type="checkbox"
                checked={spectrumSortLog}
                disabled={spectrumActivities.split(",").map((name) => name.trim()).filter(Boolean).length < 2}
                onChange={(e) => setSpectrumSortLog(e.target.checked)}
              />
              <span>Sort spectrum log (default)</span>
              </label>
            <label className="filters-field">
              <span>Spectrum activity attribute</span>
              <input
                type="text"
                placeholder="activity"
                value={spectrumActivityKey}
                disabled={spectrumActivities.split(",").map((name) => name.trim()).filter(Boolean).length < 2}
                onChange={(e) => setSpectrumActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Spectrum timestamp attribute</span>
              <input
                type="text"
                placeholder="timestamp"
                value={spectrumTimestampKey}
                disabled={spectrumActivities.split(",").map((name) => name.trim()).filter(Boolean).length < 2}
                onChange={(e) => setSpectrumTimestampKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Spectrum case-id attribute</span>
              <input
                type="text"
                placeholder="case_id"
                value={spectrumCaseIdKey}
                disabled={spectrumActivities.split(",").map((name) => name.trim()).filter(Boolean).length < 2}
                onChange={(e) => setSpectrumCaseIdKey(e.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field filters-check">
              <input type="checkbox" checked={batches} onChange={(e) => setBatches(e.target.checked)} />
              <span>Discover batches</span>
            </label>
            <label className="filters-field">
              <span>Batch variant</span>
              <select
                value={batchVariant}
                disabled={!batches}
                onChange={(e) => setBatchVariant(e.target.value as typeof batchVariant)}
              >
                <option value="log">log (default)</option>
                <option value="pandas">pandas</option>
                <option value="polars">polars</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Minimum self-distance variant</span>
              <select
                value={minimumSelfDistanceVariant}
                onChange={(e) => setMinimumSelfDistanceVariant(e.target.value as typeof minimumSelfDistanceVariant)}
              >
                <option value="log">log (default)</option>
                <option value="pandas">pandas</option>
                <option value="polars">polars</option>
              </select>
            </label>
            <label className="filters-field">
              <span>Self-distance activity attribute</span>
              <input
                type="text"
                placeholder="concept:name"
                value={minimumSelfDistanceActivityKey}
                onChange={(e) => setMinimumSelfDistanceActivityKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Self-distance case-id attribute</span>
              <input
                type="text"
                placeholder="case:concept:name"
                value={minimumSelfDistanceCaseIdKey}
                onChange={(e) => setMinimumSelfDistanceCaseIdKey(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch merge distance (seconds)</span>
              <input
                type="number"
                min={0}
                step={1}
                value={batchMergeDistanceSeconds}
                disabled={!batches}
                onChange={(e) => setBatchMergeDistanceSeconds(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch minimum size</span>
              <input
                type="number"
                min={2}
                step={1}
                value={batchMinSize}
                disabled={!batches}
                onChange={(e) => setBatchMinSize(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch activity attribute</span>
              <input
                type="text"
                placeholder="concept:name"
                value={batchActivityAttribute}
                disabled={!batches}
                onChange={(e) => setBatchActivityAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch resource attribute</span>
              <input
                type="text"
                placeholder="org:resource"
                value={batchResourceAttribute}
                disabled={!batches}
                onChange={(e) => setBatchResourceAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch timestamp attribute</span>
              <input
                type="text"
                placeholder="time:timestamp"
                value={batchTimestampAttribute}
                disabled={!batches}
                onChange={(e) => setBatchTimestampAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch case-id attribute</span>
              <input
                type="text"
                placeholder="case:concept:name"
                value={batchCaseIdAttribute}
                disabled={!batches}
                onChange={(e) => setBatchCaseIdAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch start timestamp attribute</span>
              <input
                type="text"
                placeholder="start_timestamp"
                value={batchStartTimestampAttribute}
                disabled={!batches}
                onChange={(e) => setBatchStartTimestampAttribute(e.target.value)}
              />
            </label>
            <label className="filters-field">
              <span>Batch event-id attribute <small>(pandas/polars only)</small></span>
              <input
                type="text"
                placeholder="event_id (optional)"
                value={batchEventIdAttribute}
                disabled={!batches || batchVariant === "log"}
                onChange={(e) => setBatchEventIdAttribute(e.target.value)}
              />
            </label>
          </div>
          <div className="filters-row">
            <label className="filters-field filters-check">
              <input type="checkbox" checked={overlap} onChange={(e) => setOverlap(e.target.checked)} />
              <span>Compute overlap statistics</span>
            </label>
            <label className="filters-field">
              <span>Overlap activity attribute</span>
              <input type="text" placeholder="event activity" value={overlapActivityAttribute} disabled={!overlap} onChange={(e) => setOverlapActivityAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Overlap start attribute</span>
              <input type="text" placeholder="event timestamp" value={overlapStartAttribute} disabled={!overlap} onChange={(e) => setOverlapStartAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Overlap completion attribute</span>
              <input type="text" placeholder="event timestamp" value={overlapCompletionAttribute} disabled={!overlap} onChange={(e) => setOverlapCompletionAttribute(e.target.value)} />
            </label>
            <label className="filters-field">
              <span>Overlap epsilon</span>
              <input type="number" min={0} step="any" value={overlapEpsilon} disabled={!overlap} onChange={(e) => setOverlapEpsilon(e.target.value)} />
            </label>
            <label className="filters-field filters-check">
              <input type="checkbox" checked={overlapStrict} disabled={!overlap} onChange={(e) => setOverlapStrict(e.target.checked)} />
              <span>Strict (exclude touching)</span>
            </label>
          </div>

          {(temporalValidationError || ltlValidationError || segmentValidationError ||
            pathValidationError || caseDurationValidationError || caseSelectionValidationError ||
            numericAttributeValidationError || topActivitiesValidationError ||
            relativeAttributeValidationError || attributeOccurrenceValidationError ||
            variantCoverageValidationError || repetitionValidationError ||
            caseGroupingValidationError || alphaValidationError || powlValidationError ||
            heuristicBusinessHourSlotsValidationError ||
            footprintsValidationError || correlationValidationError ||
            selectorValidationError) && (
            <p className="filters-validation" role="alert">{temporalValidationError || ltlValidationError || segmentValidationError || pathValidationError || caseDurationValidationError || caseSelectionValidationError || numericAttributeValidationError || topActivitiesValidationError || relativeAttributeValidationError || attributeOccurrenceValidationError || variantCoverageValidationError || repetitionValidationError || caseGroupingValidationError || alphaValidationError || heuristicBusinessHourSlotsValidationError || powlValidationError || footprintsValidationError || correlationValidationError || selectorValidationError}</p>
          )}
          <div className="filters-actions">
            <button className="filters-apply" onClick={apply} disabled={mining || Boolean(temporalValidationError || ltlValidationError || segmentValidationError || pathValidationError || caseDurationValidationError || caseSelectionValidationError || numericAttributeValidationError || topActivitiesValidationError || relativeAttributeValidationError || attributeOccurrenceValidationError || variantCoverageValidationError || repetitionValidationError || caseGroupingValidationError || alphaValidationError || heuristicBusinessHourSlotsValidationError || powlValidationError || footprintsValidationError || correlationValidationError || selectorValidationError)}>
              Apply
            </button>
            <button
              className="filters-reset"
              onClick={reset}
              disabled={mining || !formDirty}
            >
              Reset
            </button>
          </div>
        </div>
      )}
    </section>
  );
}
