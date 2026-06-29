import { describe, expect, it } from "vitest";
import { formatDuration, isMineraExport, type MineraExport } from "./types";

describe("formatDuration", () => {
  it("renders a dash for non-positive or non-finite input", () => {
    // Untimed edges read as "no data" rather than "0s".
    expect(formatDuration(0)).toBe("—");
    expect(formatDuration(-5)).toBe("—");
    expect(formatDuration(NaN)).toBe("—");
    expect(formatDuration(Infinity)).toBe("—");
  });

  it("picks the largest fitting unit and rounds seconds", () => {
    expect(formatDuration(12)).toBe("12s");
    expect(formatDuration(59.4)).toBe("59s");
    expect(formatDuration(90)).toBe("1.5m");
    expect(formatDuration(3600)).toBe("1.0h");
    expect(formatDuration(86400)).toBe("1.0d");
  });

  it("switches units exactly at the unit boundaries", () => {
    expect(formatDuration(59)).toBe("59s");
    expect(formatDuration(60)).toBe("1.0m");
    expect(formatDuration(3599)).toBe("60.0m");
    expect(formatDuration(86399)).toBe("24.0h");
  });
});

describe("isMineraExport", () => {
  const valid: MineraExport = {
    dataset: "t",
    trace_count: 1,
    event_count: 1,
    activities: [],
    edges: [],
    variants: [],
  };

  it("accepts a minimal well-formed export", () => {
    expect(isMineraExport(valid)).toBe(true);
  });

  it("rejects non-objects", () => {
    expect(isMineraExport(null)).toBe(false);
    expect(isMineraExport(42)).toBe(false);
    expect(isMineraExport("{}")).toBe(false);
    expect(isMineraExport(undefined)).toBe(false);
  });

  it("rejects an object missing any required array", () => {
    expect(isMineraExport({ ...valid, activities: undefined })).toBe(false);
    expect(isMineraExport({ ...valid, edges: undefined })).toBe(false);
    expect(isMineraExport({ ...valid, variants: undefined })).toBe(false);
  });

  it("rejects a non-finite trace_count", () => {
    expect(isMineraExport({ ...valid, trace_count: NaN })).toBe(false);
    expect(isMineraExport({ ...valid, trace_count: "1" })).toBe(false);
  });
});
