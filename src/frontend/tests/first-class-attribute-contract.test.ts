import { describe, expect, it } from "vitest";
import {
  isLogConversionResult,
  isLogUtilityResult,
  isMineraExport,
} from "../src/types";

const integerDetail = {
  key: "order.step",
  value: "7",
  kind: "int",
  container: "scalar",
  null: false,
  typed_value: 7,
  children: [{
    key: "order.step.date",
    value: "2026-08-31T10:00:00Z",
    kind: "date",
    container: "scalar",
    null: false,
    typed_value: "2026-08-31T10:00:00Z",
  }],
};

const dateDetail = {
  key: "order.when",
  value: "2026-08-31T10:00:00Z",
  kind: "date",
  container: "scalar",
  null: false,
  typed_value: "2026-08-31T10:00:00Z",
};

function conversionLog() {
  return {
    attributes: [],
    metadata: {
      extensions: [],
      classifiers: [],
      trace_globals: [],
      event_globals: [],
      properties: [],
    },
    traces: [{
      case_id: "42",
      case_id_attribute: integerDetail,
      attributes: [],
      events: [{
        case_id: "42",
        activity: "Review",
        activity_attribute: integerDetail,
        timestamp: "2026-08-31T10:00:00Z",
        timestamp_attribute: dateDetail,
        resource: "alice",
        resource_attribute: integerDetail,
        lifecycle: "complete",
        lifecycle_attribute: dateDetail,
        input_order: 0,
        attributes: [],
      }],
    }],
  };
}

describe("selected first-class attribute detail contracts", () => {
  it("accepts binary bytes as hexadecimal typed details", () => {
    const log = conversionLog();
    log.traces[0].events[0].activity_attribute = { ...integerDetail, kind: "bytes", value: "00ff", children: [] };
    expect(isLogConversionResult({ family: "LOG_CONVERSION", variant: "TO_EVENT_LOG", log })).toBe(true);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "FIXED_PREFIXES", log })).toBe(true);
  });
  it("accepts ordered tuple cells in conversion and utility results", () => {
    const log = conversionLog();
    log.traces[0].events[0].activity_attribute = { ...integerDetail, container: "tuple" };
    expect(isLogConversionResult({ family: "LOG_CONVERSION", variant: "TO_EVENT_LOG", log })).toBe(true);
    expect(isLogUtilityResult({ family: "LOG_UTIL", variant: "FIXED_PREFIXES", log })).toBe(true);
  });
  it("accepts recursive typed details in conversion and log-utility logs", () => {
    const log = conversionLog();
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_EVENT_LOG",
      log,
    })).toBe(true);
    expect(isLogUtilityResult({
      family: "LOG_UTIL",
      variant: "FIXED_PREFIXES",
      log,
    })).toBe(true);
  });

  it("accepts selected details in the primary export contract", () => {
    expect(isMineraExport({
      dataset: "typed",
      trace_count: 1,
      event_count: 1,
      activities: [{}],
      edges: [],
      variants: [],
      traces: [{
        case_id: "42",
        case_id_attribute: integerDetail,
        events: [{
          activity: 0,
          timestamp: "2026-08-31T10:00:00Z",
          activity_attribute: integerDetail,
          timestamp_attribute: dateDetail,
          resource: "alice",
          resource_attribute: integerDetail,
          lifecycle: "complete",
          lifecycle_attribute: dateDetail,
        }],
      }],
    })).toBe(true);
  });

  it("rejects malformed nested details instead of accepting a partial tree", () => {
    const malformed = {
      ...integerDetail,
      children: [{
        key: "nested",
        value: "1",
        kind: "int",
        container: "scalar",
        // The required recursive detail null marker is intentionally absent.
      }],
    };
    const baseLog = conversionLog();
    const log = {
      ...baseLog,
      traces: [{...baseLog.traces[0], case_id_attribute: malformed}],
    };
    expect(isLogConversionResult({
      family: "LOG_CONVERSION",
      variant: "TO_EVENT_LOG",
      log,
    })).toBe(false);
    expect(isLogUtilityResult({
      family: "LOG_UTIL",
      variant: "FIXED_PREFIXES",
      log,
    })).toBe(false);
    expect(isMineraExport({
      dataset: "typed",
      trace_count: 1,
      event_count: 1,
      activities: [{}],
      edges: [],
      variants: [],
      traces: [{
        case_id: "42",
        case_id_attribute: malformed,
        events: [{ activity: 0, timestamp: "2026-08-31T10:00:00Z" }],
      }],
    })).toBe(false);
  });
});
