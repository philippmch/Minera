import {describe, expect, it} from "vitest";
import app from "../src/App.tsx?raw";
import {displayedTab} from "../src/app-logic";

describe("displayedTab", () => {
  it("keeps a tab the analysis still offers", () => {
    expect(displayedTab("conformance", ["log", "dfg", "conformance"])).toBe("conformance");
  });
  it("falls back to the first available view when a re-mine removed the tab", () => {
    expect(displayedTab("log", ["dfg", "model"])).toBe("dfg");
    expect(displayedTab("dotted", ["dfg"])).toBe("dfg");
  });
  it("falls back to the process map with no views", () => {
    expect(displayedTab("log", [])).toBe("dfg");
  });
  it("App renders the log views by the displayed tab, not the raw selection", () => {
    const body = app.slice(app.indexOf("const view = viewById("));
    expect(body).not.toMatch(/\btab === "/);
    expect(app).toContain("activeView={shownTab}");
  });
});
