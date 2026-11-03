import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { initialToolFromHash, parseRoute, routeFor, routeHash, sameHash } from "../src/routing";
import { TOOLS, VIEWS } from "../src/tools";
import { readStored, removeStored, writeStored } from "../src/storage";
import { BACKEND_UNREACHABLE, backendFetch } from "../src/backend-fetch";
import { DOWNLOAD_URL_LIFETIME_MS, downloadBlob } from "../src/download";
import { countLabel, ocelResultHasNoObjects } from "../src/app-logic";
import { CommandPalette, filterCommands, paletteCommands } from "../src/components/CommandPalette";
import { ConfirmDialog } from "../src/components/ConfirmDialog";
import { ErrorBoundary } from "../src/components/ErrorBoundary";
import { Landing } from "../src/components/Landing";
import { ThemeToggle, isApplePlatform, paletteShortcutLabel } from "../src/components/ui";
import { syncScrollableFocus } from "../src/scrollable-focus";
import { VISUALIZATION_CHROME } from "../src/visualization-explanation";

afterEach(() => { vi.unstubAllGlobals(); vi.restoreAllMocks(); vi.useRealTimers(); });

describe("hash routing", () => {
  it("round-trips every tool and log view", () => {
    for (const tool of TOOLS) expect(parseRoute(routeHash({ kind: "tool", id: tool.id }))).toEqual({ kind: "tool", id: tool.id });
    for (const view of VIEWS) expect(parseRoute(routeHash({ kind: "view", id: view.id }))).toEqual({ kind: "view", id: view.id });
    expect(parseRoute("#/workspace")).toEqual({ kind: "workspace" });
    expect(routeHash({ kind: "home" })).toBe("");
  });
  it("keeps tool and view ids apart even where they share a name", () => {
    expect(parseRoute("#/tool/clustering")).toEqual({ kind: "tool", id: "clustering" });
    expect(parseRoute("#/view/clustering")).toEqual({ kind: "view", id: "clustering" });
  });
  it("sends unknown or malformed hashes to the start page", () => {
    for (const hash of ["", "#", "#/", "#/tool/nope", "#/view/nope", "#/tool", "#/tool/token-replay/extra", "#/tool/%E0%A4%A", "#/workspace/x", "#tool/token-replay/x"]) expect(parseRoute(hash)).toEqual({ kind: "home" });
    expect(parseRoute("#tool/token-replay/")).toEqual({ kind: "tool", id: "token-replay" });
  });
  it("derives the route from what the app shows, with an open tool first", () => {
    expect(routeFor({ tool: "token-replay", view: "dfg", hasLog: true, hasOtherData: false })).toEqual({ kind: "tool", id: "token-replay" });
    expect(routeFor({ tool: null, view: "dfg", hasLog: true, hasOtherData: false })).toEqual({ kind: "view", id: "dfg" });
    expect(routeFor({ tool: null, view: null, hasLog: false, hasOtherData: true })).toEqual({ kind: "workspace" });
    expect(routeFor({ tool: null, view: "dfg", hasLog: false, hasOtherData: false })).toEqual({ kind: "home" });
  });
  it("opens only tool links on a fresh load, since views need data a reload drops", () => {
    expect(initialToolFromHash("#/tool/playout")).toBe("playout");
    expect(initialToolFromHash("#/view/dfg")).toBeNull();
    expect(initialToolFromHash("#/workspace")).toBeNull();
    expect(sameHash("#", "")).toBe(true);
    expect(sameHash("#/view/log", "#/view/dfg")).toBe(false);
  });
});

describe("storage guard", () => {
  it("treats a throwing localStorage getter as no storage", () => {
    const original = Object.getOwnPropertyDescriptor(globalThis, "localStorage");
    Object.defineProperty(globalThis, "localStorage", { configurable: true, get() { throw new DOMException("The operation is insecure.", "SecurityError"); } });
    try {
      expect(readStored("k")).toBeNull();
      expect(writeStored("k", "v")).toBe(false);
      expect(removeStored("k")).toBe(false);
    } finally {
      if (original) Object.defineProperty(globalThis, "localStorage", original); else delete (globalThis as { localStorage?: unknown }).localStorage;
    }
  });
  it("survives methods that throw (quota, private mode)", () => {
    vi.stubGlobal("localStorage", { getItem: () => { throw new Error("blocked"); }, setItem: () => { throw new DOMException("quota", "QuotaExceededError"); }, removeItem: () => { throw new Error("blocked"); } });
    expect(readStored("k")).toBeNull();
    expect(writeStored("k", "v")).toBe(false);
    expect(removeStored("k")).toBe(false);
  });
  it("reads and writes when storage works, and copes with no storage at all", () => {
    const map = new Map<string, string>();
    vi.stubGlobal("localStorage", { getItem: (k: string) => map.get(k) ?? null, setItem: (k: string, v: string) => void map.set(k, v), removeItem: (k: string) => void map.delete(k) });
    expect(writeStored("minera.x", "1")).toBe(true);
    expect(readStored("minera.x")).toBe("1");
    expect(removeStored("minera.x")).toBe(true);
    expect(readStored("minera.x")).toBeNull();
    vi.stubGlobal("localStorage", undefined);
    expect(readStored("minera.x")).toBeNull();
    expect(writeStored("minera.x", "1")).toBe(false);
  });
});

describe("backend fetch", () => {
  it("turns a network failure into the actionable backend message", async () => {
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(new TypeError("Failed to fetch")));
    await expect(backendFetch("/api/x")).rejects.toThrow(BACKEND_UNREACHABLE);
  });
  it("passes aborts and responses through and forwards the exact arguments", async () => {
    const abort = new DOMException("stopped", "AbortError");
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(abort));
    await expect(backendFetch("/api/x", { method: "POST" })).rejects.toBe(abort);
    const response = new Response("{}", { status: 500 });
    const fetchMock = vi.fn().mockResolvedValue(response);
    vi.stubGlobal("fetch", fetchMock);
    await expect(backendFetch("/api/y")).resolves.toBe(response);
    expect(fetchMock.mock.calls[0]).toEqual(["/api/y"]);
  });
});

describe("downloads", () => {
  it("clicks an attached anchor and revokes the URL only later", () => {
    vi.useFakeTimers();
    const appended: unknown[] = [];
    const anchor = { href: "", download: "", rel: "", click: vi.fn(), remove: vi.fn() };
    vi.stubGlobal("document", { createElement: () => anchor, body: { appendChild: (node: unknown) => appended.push(node) } });
    vi.spyOn(URL, "createObjectURL").mockReturnValue("blob:x");
    const revoke = vi.spyOn(URL, "revokeObjectURL").mockImplementation(() => {});
    downloadBlob(new Blob(["a"], { type: "text/plain" }), "log.xes");
    expect(appended).toEqual([anchor]);
    expect(anchor.download).toBe("log.xes");
    expect(anchor.click).toHaveBeenCalledOnce();
    expect(anchor.remove).toHaveBeenCalledOnce();
    expect(revoke).not.toHaveBeenCalled();
    vi.advanceTimersByTime(DOWNLOAD_URL_LIFETIME_MS);
    expect(revoke).toHaveBeenCalledWith("blob:x");
  });
});

describe("labels and empty states", () => {
  it("pluralises counts", () => {
    expect(countLabel(1, "case")).toBe("1 case");
    expect(countLabel(0, "event")).toBe("0 events");
    expect(countLabel(2585, "event")).toBe("2,585 events");
    expect(countLabel(2, "activity", "activities")).toBe("2 activities");
  });
  it("recognises an object-centric result without objects", () => {
    expect(ocelResultHasNoObjects({ activities: [], object_types: [] })).toBe(true);
    expect(ocelResultHasNoObjects({ object_types: ["order"] })).toBe(false);
    expect(ocelResultHasNoObjects({ places: [] })).toBe(false);
    expect(ocelResultHasNoObjects(null)).toBe(false);
  });
  it("names the palette shortcut for the platform", () => {
    expect(paletteShortcutLabel(true)).toBe("⌘K");
    expect(paletteShortcutLabel(false)).toBe("Ctrl K");
    expect(isApplePlatform({ platform: "MacIntel" })).toBe(true);
    expect(isApplePlatform({ userAgentData: { platform: "Windows" }, platform: "MacIntel" })).toBe(false);
    expect(isApplePlatform({ platform: "Linux x86_64" })).toBe(false);
    expect(isApplePlatform(undefined)).toBe(false);
  });
  it("starts the theme toggle's accessible name with its visible text", () => {
    const html = renderToStaticMarkup(createElement(ThemeToggle));
    const visible = />([^<]+)<\/button>/.exec(html)![1];
    expect(html).toContain(`aria-label="${visible} theme.`);
  });
});

describe("command palette", () => {
  const noop = () => {};
  it("never preselects the start-page command", () => {
    const run = vi.fn();
    const commands = paletteCommands({ views: VIEWS, onSelectView: noop, onOpenTool: noop, trailing: [{ id: "action:home", title: "Go to start", section: "Navigate", run }] });
    expect(commands.at(-1)?.id).toBe("action:home");
    expect(filterCommands(commands, "")[0].id).not.toBe("action:home");
  });
  it("is a combobox with a named listbox and no buttons nested in options", () => {
    const commands = paletteCommands({ views: VIEWS.slice(0, 2), onSelectView: noop, onOpenTool: noop });
    const html = renderToStaticMarkup(createElement(CommandPalette, { commands, onClose: noop }));
    expect(html).toContain('role="combobox"');
    expect(html).toMatch(/role="listbox"[^>]*aria-label="Matching views and tools"/);
    expect(html).toMatch(/aria-activedescendant="[^"]+-option-0"/);
    expect(html).not.toContain("<button");
  });
});

describe("confirmation before discarding data", () => {
  it("renders a labelled modal alert dialog with the safe choice first", () => {
    const html = renderToStaticMarkup(createElement(ConfirmDialog, { title: "Close the open data?", message: "This discards results.", confirmLabel: "Close and go to start", cancelLabel: "Keep working", onConfirm: () => {}, onCancel: () => {} }));
    expect(html).toContain('role="alertdialog"');
    expect(html).toContain('aria-modal="true"');
    expect(html).toMatch(/aria-labelledby="([^"]+)"[^>]*aria-describedby="([^"]+)"/);
    expect(html.indexOf("Keep working")).toBeLessThan(html.indexOf("Close and go to start"));
  });
  it("offers Cancel on the start page while a log is loading", () => {
    const props = { onOpenFile: () => {}, onLoadSample: () => {}, onLoadOcelSample: () => {}, onLoadResourceSample: () => {}, error: null, mining: true, onCancel: () => {} };
    expect(renderToStaticMarkup(createElement(Landing, props))).toMatch(/role="status"[^>]*>Mining the event log… <button[^>]*>Cancel<\/button>/);
    expect(renderToStaticMarkup(createElement(Landing, { ...props, mining: false }))).not.toContain(">Cancel<");
  });
});

describe("error boundary", () => {
  it("shows a recoverable message instead of a blank page", () => {
    expect(ErrorBoundary.getDerivedStateFromError(new Error("boom"))).toEqual({ error: new Error("boom") });
    expect(ErrorBoundary.getDerivedStateFromError("bad").error?.message).toBe("bad");
    const boundary = new ErrorBoundary({ children: null });
    boundary.state = { error: new Error("The operation is insecure.") };
    const html = renderToStaticMarkup(boundary.render() as never);
    expect(html).toContain('role="alert"');
    expect(html).toContain("The operation is insecure.");
    expect(html).toContain("Try again");
  });
});

describe("keyboard-scrollable regions", () => {
  it("adds and removes the tabindex it manages", () => {
    const attrs = (initial: Record<string, string> = {}) => { const map = new Map(Object.entries(initial)); return { hasAttribute: (k: string) => map.has(k), setAttribute: (k: string, v: string) => void map.set(k, v), removeAttribute: (k: string) => void map.delete(k), map }; };
    const overflowing = { ...attrs(), scrollWidth: 900, clientWidth: 300, scrollHeight: 10, clientHeight: 10 };
    const fitting = { ...attrs(), scrollWidth: 100, clientWidth: 300, scrollHeight: 10, clientHeight: 10 };
    const owned = { ...attrs({ tabindex: "-1" }), scrollWidth: 900, clientWidth: 300, scrollHeight: 10, clientHeight: 10 };
    vi.stubGlobal("getComputedStyle", () => ({ overflowX: "auto", overflowY: "auto" }));
    const root = { querySelectorAll: () => [overflowing, fitting, owned] } as unknown as ParentNode;
    expect(syncScrollableFocus(root)).toBe(1);
    expect(overflowing.map.get("tabindex")).toBe("0");
    expect(fitting.map.has("tabindex")).toBe(false);
    expect(owned.map.get("tabindex")).toBe("-1");
    overflowing.scrollWidth = 200;
    expect(syncScrollableFocus(root)).toBe(1);
    expect(overflowing.map.has("tabindex")).toBe(false);
  });
  it("keeps the active map mode in exported images but hides the other controls", () => {
    expect(VISUALIZATION_CHROME).toContain(".map-hint");
    expect(VISUALIZATION_CHROME).toContain(":not(.active)");
  });
});
