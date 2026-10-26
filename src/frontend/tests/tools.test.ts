import { describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { groupTools, matchesQuery, searchTools, toolById, TOOL_GROUPS, TOOLS, VIEWS } from "../src/tools";
import { filterCommands, paletteCommands } from "../src/components/CommandPalette";
import { Landing } from "../src/components/Landing";
import { Sidebar } from "../src/components/Sidebar";

const noop = () => {};

describe("tool registry", () => {
  it("gives every tool a unique id, a known group and readable copy", () => {
    expect(new Set(TOOLS.map((tool) => tool.id)).size).toBe(TOOLS.length);
    const groups = new Set(TOOL_GROUPS.map((group) => group.id));
    for (const tool of TOOLS) {
      expect(groups.has(tool.group)).toBe(true);
      expect(tool.title.length).toBeGreaterThan(3);
      expect(tool.summary).toMatch(/\.$/);
      expect(toolById(tool.id)).toBe(tool);
    }
    expect(groupTools().flatMap((entry) => entry.tools)).toHaveLength(TOOLS.length);
    expect(new Set(VIEWS.map((view) => view.id)).size).toBe(VIEWS.length);
  });

  it("searches titles, summaries, keywords and group names word by word", () => {
    expect(matchesQuery("", "anything")).toBe(true);
    expect(matchesQuery("tree align", "Process tree alignment")).toBe(true);
    expect(matchesQuery("tree bpmn", "Process tree alignment")).toBe(false);
    expect(searchTools("sql").map((tool) => tool.id)).toContain("log-query");
    expect(searchTools("drift").map((tool) => tool.id)).toEqual(["bose"]);
    expect(searchTools("simulation").map((tool) => tool.id)).toEqual(expect.arrayContaining(["monte-carlo", "playout", "oc-playout"]));
    expect(searchTools("no such tool anywhere")).toEqual([]);
  });

  it("builds palette commands for log views and tools that run their action", () => {
    const opened: string[] = [];
    const commands = paletteCommands({ views: VIEWS, onSelectView: (view) => opened.push(`view:${view}`), onOpenTool: (tool) => opened.push(`tool:${tool}`) });
    expect(commands).toHaveLength(VIEWS.length + TOOLS.length);
    filterCommands(commands, "process map")[0].run();
    filterCommands(commands, "petri net alignment")[0].run();
    expect(opened).toEqual(["view:dfg", "tool:petri-alignment"]);
  });

  it("lists every tool on the landing page and in the sidebar", () => {
    const landing = renderToStaticMarkup(createElement(Landing, { onOpenFile: noop, onLoadSample: noop, onLoadOcelSample: noop, onLoadResourceSample: noop, onOpenTool: noop, error: null, mining: false }));
    const sidebar = renderToStaticMarkup(createElement(Sidebar, { dataset: { kind: "Event log", name: "orders.csv", detail: "3 cases" }, views: VIEWS, activeView: "dfg", activeTool: "declare", onOpenTool: noop, onHome: noop, onOpenFile: noop, onOpenPalette: noop }));
    for (const tool of TOOLS) {
      expect(landing).toContain(`<span class="tool-card-title">${tool.title}</span>`);
      expect(sidebar).toContain(`>${tool.title}</button>`);
    }
    expect(sidebar).toContain("orders.csv");
    expect(sidebar).toContain('aria-current="page" title="Check a log against a Declare model or play the model out.">Declare models');
    expect(sidebar).not.toContain('aria-current="page">Process map');
  });
});
