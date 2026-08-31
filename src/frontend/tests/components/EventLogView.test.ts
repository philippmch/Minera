import { describe, expect, it } from "vitest";
import { buildXesExportOptions } from "../../src/components/EventLogView";

describe("EventLogView XES controls", () => {
  it("passes exporter, compression, and metadata inference controls together", () => {
    expect(buildXesExportOptions("etree", true, true)).toEqual({
      variant: "etree",
      compress: true,
      inferMetadata: true,
    });
    expect(buildXesExportOptions("line_by_line", false, false)).toEqual({
      variant: "line_by_line",
      compress: false,
      inferMetadata: false,
    });
  });
});
