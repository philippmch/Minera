import { describe, expect, it } from "vitest";
import { mineFormEntries, type MineOptions } from "../src/api";
import { sourceImportOptions } from "../src/source-import-options";

describe("re-mining keeps only the source's import settings", () => {
  const previous: MineOptions = {
    delimiter: ";", caseColumn: "case", activityColumn: "task", timestampColumn: "when",
    miningInputShape: "log", miningSelectors: {case_id_key: "case"},
    topVariants: 1, removeActivities: ["B"], noiseThreshold: 0.4,
    keepResources: ["Alice"], miningRequestId: "00000000-0000-4000-8000-000000000000",
  };

  it("drops filters and miner settings but keeps delimiter and column mapping", () => {
    expect(sourceImportOptions(previous)).toEqual({
      delimiter: ";", caseColumn: "case", activityColumn: "task", timestampColumn: "when",
      miningInputShape: "log", miningSelectors: {case_id_key: "case"},
    });
  });

  it("sends a Reset request without the previous filters", () => {
    const fields = new Map(mineFormEntries({...sourceImportOptions(previous), ...{}}));
    for (const name of ["top_variants", "remove_activities", "remove_activities_json", "noise_threshold", "keep_resources_json"])
      expect(fields.has(name)).toBe(false);
    expect(fields.get("delimiter")).toBe(";");
    expect(fields.get("case_column")).toBe("case");
  });

  it("lets an Apply replace filters from the request alone", () => {
    const fields = new Map(mineFormEntries({...sourceImportOptions(previous), topVariants: 3}));
    expect(fields.get("top_variants")).toBe("3");
    expect(fields.has("remove_activities_json")).toBe(false);
  });

  it("serializes comma-containing labels unambiguously", () => {
    const fields = new Map(mineFormEntries({removeActivities: ["Review, approve"], keepLifecycles: ["a,b"]}));
    expect(JSON.parse(fields.get("remove_activities_json")!)).toEqual(["Review, approve"]);
    expect(JSON.parse(fields.get("keep_lifecycles_json")!)).toEqual(["a,b"]);
    expect(fields.has("remove_activities")).toBe(false);
  });
});
