import { describe, expect, it } from "vitest";
import {
  decorationCoverage,
  decorationNotice,
  hasTimedDecoration,
  placeTooltip,
  placeWaitLabel,
  transitionCountLabel,
} from "../src/petri-decoration";
import { type DurationStats, type PetriDecoration } from "../src/types";

const wait = (overrides: Partial<DurationStats> = {}): DurationStats => ({
  count: 0,
  total: 0,
  mean: 0,
  median: 0,
  min: 0,
  max: 0,
  ...overrides,
});

const decoration: PetriDecoration = {
  decorated_traces: 245,
  trace_count: 320,
  transitions: [1200, 0, 3],
  places: [
    { count: 1200, wait: wait() }, // traversed but untimed
    {
      count: 320,
      wait: { count: 320, total: 38400, mean: 120, median: 90, min: 60, max: 7200 },
    },
    { count: 0, wait: wait() }, // dead place
  ],
};

describe("transitionCountLabel", () => {
  it("formats counts with grouping and shows dead transitions as 0", () => {
    expect(transitionCountLabel(decoration, 0)).toBe("1,200");
    expect(transitionCountLabel(decoration, 1)).toBe("0");
    expect(transitionCountLabel(decoration, 2)).toBe("3");
  });

  it("treats an index beyond the decoration as never fired", () => {
    expect(transitionCountLabel(decoration, 99)).toBe("0");
  });
});

describe("placeWaitLabel", () => {
  it("shows the mean wait for timed places only", () => {
    expect(placeWaitLabel(decoration, 1)).toBe("2.0m");
    expect(placeWaitLabel(decoration, 0)).toBeNull(); // untimed
    expect(placeWaitLabel(decoration, 2)).toBeNull(); // never traversed
    expect(placeWaitLabel(decoration, 99)).toBeNull();
  });
});

describe("placeTooltip", () => {
  it("reports token counts alone when untimed, with waits when timed", () => {
    expect(placeTooltip(decoration, 0)).toBe("1,200 tokens");
    expect(placeTooltip(decoration, 1)).toBe(
      "320 tokens · waits 1.5m median · 2.0m mean · up to 2.0h",
    );
    expect(placeTooltip(decoration, 99)).toBeNull();
  });

  it("uses the singular for a single token", () => {
    const single: PetriDecoration = {
      ...decoration,
      places: [{ count: 1, wait: wait() }],
    };
    expect(placeTooltip(single, 0)).toBe("1 token");
  });
});

describe("hasTimedDecoration", () => {
  it("is true only when some place measured a wait", () => {
    expect(hasTimedDecoration(decoration)).toBe(true);
    expect(
      hasTimedDecoration({ ...decoration, places: [{ count: 5, wait: wait() }] }),
    ).toBe(false);
    expect(hasTimedDecoration(null)).toBe(false);
    expect(hasTimedDecoration(undefined)).toBe(false);
  });
});

describe("decorationCoverage", () => {
  it("distinguishes full coverage, partial coverage, and none", () => {
    expect(decorationCoverage(decoration)).toBe("replayed 245 of 320 cases");
    expect(
      decorationCoverage({ ...decoration, decorated_traces: 320 }),
    ).toBe("replayed all 320 cases");
    expect(
      decorationCoverage({ ...decoration, decorated_traces: 0 }),
    ).toBeNull();
  });
});

describe("decorationNotice", () => {
  it("surfaces the server's explanation only when it sent one", () => {
    expect(decorationNotice(decoration)).toBeNull();
    expect(decorationNotice(null)).toBeNull();
    const note = "no trace replays cleanly on this model, so it carries no decoration";
    expect(decorationNotice({ ...decoration, decorated_traces: 0, message: note })).toBe(note);
  });
});
