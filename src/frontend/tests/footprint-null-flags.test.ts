import {describe, expect, it} from "vitest";
import {ORIGINAL_FOOTPRINTS_DEFAULT_MODEL, isOriginalFootprint, originalFootprintMatches} from "../src/original-footprints-workbench";
import {parseNativeJson} from "../src/native-json";

// The native footprint reader accepts the Polars null-activity flags, and the
// writer emits each one only when it is true.
const model = () => parseNativeJson(ORIGINAL_FOOTPRINTS_DEFAULT_MODEL) as Record<string, unknown>;
const FLAGS = ["activities_include_null", "start_activities_include_null", "end_activities_include_null"];

describe("footprint null-activity flags", () => {
  it.each(FLAGS)("accepts a supplied %s echoed back as true", (flag) => {
    const supplied = {...model(), [flag]: true};
    expect(isOriginalFootprint({...supplied})).toBe(true);
    expect(originalFootprintMatches(supplied, {...supplied})).toBe(true);
  });
  it.each(FLAGS)("accepts a supplied false %s that the writer omits", (flag) => {
    expect(originalFootprintMatches({...model(), [flag]: false}, model())).toBe(true);
  });
  it("rejects a flag the response changed or invented", () => {
    expect(originalFootprintMatches({...model(), activities_include_null: true}, model())).toBe(false);
    expect(originalFootprintMatches(model(), {...model(), start_activities_include_null: true})).toBe(false);
    expect(originalFootprintMatches({...model(), end_activities_include_null: "yes"}, {...model(), end_activities_include_null: true})).toBe(false);
  });
  it("type-checks the flags", () => {
    expect(isOriginalFootprint({...model(), activities_include_null: 1})).toBe(false);
    expect(isOriginalFootprint({...model(), end_activities_include_null: false})).toBe(true);
  });
});
