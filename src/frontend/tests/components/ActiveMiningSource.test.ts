import { describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { ActiveMiningSource } from "../../src/components/ActiveMiningSource";
import type { ActiveMiningSourceDescriptor, ActiveMiningSourceSnapshot } from "../../src/active-mining-source";

function descriptor(detail: "summary" | undefined): ActiveMiningSourceDescriptor {
  const table = {columns: ["case_id", "activity"], dtypes: ["object", "object"], rows: [[{key: "case_id", kind: "string", value: "c1"}, {key: "activity", kind: "string", value: "A"}]]};
  const snapshot = {format: "auto", detected_format: "csv", shape: "log", source_id: "7", original_dataset: "log.csv", original_input_bytes: 300000, source_unchanged: true,
    source_selectors: {case_id_key: "", activity_key: "", timestamp_key: "", resource_key: "", lifecycle_key: ""},
    input_before: table, input_after: null, checked_table: null, original_log: null, checked_log: null, prepared_stream: null, prepared_lazy_table: null,
    source_row_ordinals: [], representative_row_ordinals: [], projection_changes: [], ...(detail ? {detail} : {})} as unknown as ActiveMiningSourceSnapshot;
  return {file: new File(["x".repeat(300000)], "log.csv"), sha256: "00", options: {}, format: "auto", shape: "log", selectors: {}, source_id: "7", snapshot};
}

const render = (detail: "summary" | undefined) => renderToStaticMarkup(createElement(ActiveMiningSource, {
  source: descriptor(detail), outcome: {family: "ACTIVE_SOURCE_MINING", source_id: "7", active_source: descriptor(detail).snapshot!, analysis: null, family_results: {}, error: null},
  mining: false, available: true, onOpen: () => {}, onCancel: () => {},
}));
const button = (markup: string, label: string) => markup.match(new RegExp(`<button[^>]*>${label}</button>`))?.[0];

describe("active source inspector downloads for a summary snapshot", () => {
  it("disables the complete table CSV downloads and labels the summary mining results", () => {
    const summary = render("summary");
    expect(button(summary, "Download complete source table CSV")).toContain('disabled=""');
    expect(button(summary, "Download all source tables CSV")).toContain('disabled=""');
    expect(summary).toContain("Download native mining results (source summary only)");
    expect(summary).not.toContain("Download complete native mining results");
  });
  it("keeps them enabled for a complete snapshot", () => {
    const complete = render(undefined);
    expect(button(complete, "Download complete source table CSV")).not.toContain("disabled");
    expect(button(complete, "Download all source tables CSV")).not.toContain("disabled");
    expect(complete).toContain("Download complete native mining results");
  });
});
