import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { initialToolFromHash, parseRoute, routeFor, routeHash, sameHash } from "../src/routing";
import { TOOLS, VIEWS } from "../src/tools";
import { readStored, removeStored, writeStored } from "../src/storage";
import { BACKEND_UNREACHABLE, backendFetch, backendFetchArgumentError, isNetworkFailure } from "../src/backend-fetch";
import { releaseReaderQuietly, type ReaderRun } from "../src/reader-stream-workspace";
import { releaseLiveSessionQuietly, type LiveRun } from "../src/live-stream-workspace";
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
  it("keeps argument TypeErrors apart from network failures", async () => {
    // A header value fetch() refuses: named before the request, never "unreachable".
    const fetchMock = vi.fn();
    vi.stubGlobal("fetch", fetchMock);
    const token = backendFetch("/api/x", { headers: { Authorization: "Bearer tok\u20acen" } });
    await expect(token).rejects.toBeInstanceOf(TypeError);
    await expect(backendFetch("/api/x", { headers: { Authorization: "Bearer tok\u20acen" } })).rejects.toThrow(/Authorization value contains “€”/);
    await expect(backendFetch("/api/x", { headers: [["X-Key", "a\nb"]] })).rejects.toThrow(/line break or NUL/);
    await expect(backendFetch("/api/x", { method: "GET", body: "x" })).rejects.toThrow("A GET request cannot carry a body.");
    expect(fetchMock).not.toHaveBeenCalled();
    expect(backendFetchArgumentError({ headers: { "X-Name": "caf\u00e9" }, method: "POST", body: "x" })).toBeNull();
    // Argument errors fetch() itself raises keep their message.
    const argument = new TypeError("Failed to execute 'fetch' on 'Window': Invalid name");
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(argument));
    await expect(backendFetch("/api/x")).rejects.toBe(argument);
    const url = new TypeError("Failed to parse URL from http://[::1");
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(url));
    await expect(backendFetch("/api/x")).rejects.toBe(url);
    // Real network failures in each engine's wording still map.
    for (const message of ["Failed to fetch", "NetworkError when attempting to fetch resource.", "Load failed", "fetch failed"]) {
      expect(isNetworkFailure(new TypeError(message))).toBe(true);
      vi.stubGlobal("fetch", vi.fn().mockRejectedValue(new TypeError(message)));
      await expect(backendFetch("/api/x")).rejects.toThrow(BACKEND_UNREACHABLE);
    }
    expect(isNetworkFailure(new Error("Failed to fetch"))).toBe(false);
    expect(isNetworkFailure(new TypeError("x is not a function"))).toBe(false);
  });
});

describe("stream session release", () => {
  it("releases an open reader or live session once, in the background", async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response("{}"));
    vi.stubGlobal("fetch", fetchMock);
    releaseReaderQuietly({ result: { session_id: "r1", revision: 4, released: false } } as unknown as ReaderRun);
    releaseLiveSessionQuietly({ result: { session_id: "l1", revision: 7, released: false } } as unknown as LiveRun);
    releaseReaderQuietly({ result: { session_id: "r2", revision: 1, released: true } } as unknown as ReaderRun);
    releaseLiveSessionQuietly({ result: { session_id: "l2", revision: 1, released: true } } as unknown as LiveRun);
    expect(fetchMock.mock.calls).toEqual([
      ["/api/stream-reader/release?session_id=r1&expected_revision=4", { method: "POST", keepalive: true }],
      ["/api/live-stream/release?session_id=l1&expected_revision=7", { method: "POST", keepalive: true }],
    ]);
    // A failed release is left for the session list, never thrown into an unmount.
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(new TypeError("Failed to fetch")));
    expect(() => releaseReaderQuietly({ result: { session_id: "r3", revision: 1, released: false } } as unknown as ReaderRun)).not.toThrow();
    await new Promise((resolve) => setTimeout(resolve, 0));
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
    // The box takes focus itself when its text is clicked, so keys stay trapped.
    expect(html).toMatch(/role="alertdialog"[^>]*tabindex="-1"/);
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
    expect(html).toContain("Back to the start page");
    expect(html).toContain("Reload Minera");
    expect(html).toContain("was lost");
  });
  it("keeps the open data when only a view fails, and clears on a route change", () => {
    const boundary = new ErrorBoundary({ children: "the view", scope: "view", resetKey: "#/view/dfg" });
    boundary.state = { error: new Error("bad cell") };
    const html = renderToStaticMarkup(boundary.render() as never);
    expect(html).toContain('role="alert"');
    expect(html).toContain("Something went wrong in this view");
    expect(html).toContain("The open data is kept");
    expect(html).toContain("Try again");
    expect(html).not.toContain("Reload Minera");
    const cleared: unknown[] = [];
    boundary.setState = ((state: unknown) => void cleared.push(state)) as never;
    boundary.componentDidUpdate({ resetKey: "#/view/dfg" });
    expect(cleared).toEqual([]);
    (boundary as { props: unknown }).props = { children: "the view", scope: "view", resetKey: "#/tool/token-replay" };
    boundary.componentDidUpdate({ resetKey: "#/view/dfg" });
    expect(cleared).toEqual([{ error: null }]);
    boundary.state = { error: null };
    expect(boundary.render()).toBe("the view");
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
