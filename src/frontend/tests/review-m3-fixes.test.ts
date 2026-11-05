// Review round M3: large-log rendering, keyboard access and focus fixes.
import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { markLiveTraces, retainTraces } from "../src/mine-traces";
import type { MineraExport, Trace } from "../src/types";
import { VARIANT_PAGE_SIZE, VariantList, maxVariantCount } from "../src/components/VariantList";
import { DOTTED_SVG_LIMIT, DottedChartView } from "../src/components/DottedChart";
import { Conformance, REDUCTION_SELECT_LIMIT, ReductionTraceField } from "../src/components/Conformance";
import { focusLostStep } from "../src/step-focus";
import { syncScrollableFocus } from "../src/scrollable-focus";

afterEach(() => { vi.unstubAllGlobals(); vi.restoreAllMocks(); retainTraces(null); });

const render = (element: Parameters<typeof renderToStaticMarkup>[0]) => renderToStaticMarkup(element);
const count = (html: string, needle: string) => html.split(needle).length - 1;

const activities = [
  { name: "Register", count: 1, start_count: 1, end_count: 0 },
  { name: "Ship", count: 1, start_count: 0, end_count: 1 },
];

function withVariants(n: number): MineraExport {
  const variants = Array.from({ length: n }, (_, i) => ({ activities: [0, 1], count: n - i }));
  return { dataset: "big.csv", trace_count: n * (n + 1) / 2, event_count: n * (n + 1), activities, edges: [], variants } as unknown as MineraExport;
}

describe("variant list on a log with many distinct paths", () => {
  it("renders one page of variants and offers the rest", () => {
    const data = withVariants(VARIANT_PAGE_SIZE * 3 + 7);
    const html = render(createElement(VariantList, { data, selected: null, onSelect: () => undefined }));
    expect(count(html, 'class="variant-select"')).toBe(VARIANT_PAGE_SIZE);
    expect(html).toContain(`Show more · ${VARIANT_PAGE_SIZE * 2 + 7} remaining`);
    expect(html).toContain(`${VARIANT_PAGE_SIZE * 3 + 7} distinct paths`);
  });
  it("keeps a selected variant beyond the first page on screen", () => {
    const data = withVariants(VARIANT_PAGE_SIZE * 2);
    const html = render(createElement(VariantList, { data, selected: VARIANT_PAGE_SIZE + 4, onSelect: () => undefined }));
    expect(count(html, 'class="variant-select"')).toBe(VARIANT_PAGE_SIZE + 5);
    expect(count(html, 'aria-pressed="true"')).toBe(1);
  });
  it("selects variants with buttons, so the keyboard reaches them", () => {
    const data = withVariants(3);
    const html = render(createElement(VariantList, { data, selected: 1, onSelect: () => undefined }));
    expect(count(html, '<button type="button" class="variant-select" aria-pressed="false"')).toBe(2);
    expect(html).toContain('<li class="variant active"><button type="button" class="variant-select" aria-pressed="true"');
    expect(html).not.toContain("Show more");
    // No block elements inside the buttons.
    expect(html).not.toMatch(/<button[^>]*class="variant-select"[^>]*>(?:(?!<\/button>).)*<div/s);
  });
  it("finds the largest count without spreading every variant into Math.max", () => {
    const variants = Array.from({ length: 300_000 }, (_, i) => ({ count: i % 1000 }));
    expect(maxVariantCount(variants)).toBe(999);
    expect(maxVariantCount([])).toBe(0);
  });
});

describe("dotted chart of a large log", () => {
  const trace = (i: number, events: number): Trace => ({
    case_id: `c${i}`,
    events: Array.from({ length: events }, (_, e) => ({ activity: e % 2, timestamp: new Date(Date.UTC(2024, 0, 1) + (i * 10 + e) * 60_000).toISOString() })),
  });
  const log = (traces: Trace[]) => ({ dataset: "x.csv", trace_count: traces.length, event_count: traces.reduce((n, t) => n + t.events.length, 0), activities, edges: [], variants: [], traces }) as unknown as MineraExport;

  it("draws one circle per event up to the limit", () => {
    const html = render(createElement(DottedChartView, { data: log([trace(0, 2), trace(1, 3)]) }));
    expect(count(html, "<circle")).toBe(5);
    expect(html).not.toContain("drawn as one image");
  });
  it("draws more events than the limit as one image instead of one node each", () => {
    const perCase = 10, cases = Math.ceil((DOTTED_SVG_LIMIT + 1) / perCase);
    const html = render(createElement(DottedChartView, { data: log(Array.from({ length: cases }, (_, i) => trace(i, perCase))) }));
    expect(html).not.toContain("<circle");
    // Server rendering has no canvas: the chart says it is being drawn.
    expect(html).toContain(`Drawing ${(cases * perCase).toLocaleString("en-US")} events…`);
    expect(html).toContain('class="dotted-chart"');
  });
});

describe("process-tree reduction trace on a large log", () => {
  it("asks for a trace number instead of listing every case, without loading the cases", () => {
    const fetch = vi.fn();
    vi.stubGlobal("fetch", fetch);
    const n = REDUCTION_SELECT_LIMIT + 1;
    const data = { dataset: "big.csv", trace_count: n, event_count: n, activities, edges: [], variants: [], source_id: "7",
      trace_summary: { traces: "on_demand", trace_count: n, event_count: n, max_trace_length: 1, timestamped_trace_count: 0, has_timestamps: false, has_lifecycles: false, lifecycles: [], run_id: "0123456789abcdef" } } as unknown as MineraExport;
    markLiveTraces(data);
    const html = render(createElement(ReductionTraceField, { data, traceIndex: 41, onChange: () => undefined }));
    expect(html).not.toContain("<select");
    expect(html).toContain('type="number"');
    expect(html).toContain('value="42"');
    expect(html).toContain(`max="${n}"`);
    expect(fetch).not.toHaveBeenCalled();
  });
});

describe("keyboard access to file pickers", () => {
  it("keeps the conformance model upload card a tab stop", () => {
    const data = { dataset: "x.csv", trace_count: 1, event_count: 2, activities, edges: [], variants: [], conformance: {} } as unknown as MineraExport;
    const html = render(createElement(Conformance, { data, onRequestAlignment: async () => { throw new Error("unused"); }, onCheckModel: async () => { throw new Error("unused"); } }));
    const input = html.match(/<label class="miner-card miner-card-upload">(?:(?!<\/label>).)*?(<input[^>]*type="file"[^>]*>)/s)?.[1];
    expect(input).toBeDefined();
    expect(input).not.toMatch(/\shidden/);
    expect(input).toContain('aria-label="Upload your model (PNML, BPMN or PTML)"');
  });
  it("shows the focus of transparent file inputs on their label", async () => {
    // Vitest blanks imported CSS, so the stylesheet is read from disk.
    const fs: { readFileSync(path: URL, encoding: string): string } = await import(/* @vite-ignore */ String("node:fs"));
    const css = fs.readFileSync(new URL("../src/styles.css", import.meta.url), "utf8");
    expect(css).toContain(':is(.landing-drop, label.btn-open, .miner-card-upload):has(> input[type="file"]:focus-visible)');
    expect(css).toMatch(/\.miner-card-upload input\[type="file"\] \{[^}]*opacity: 0/);
  });
});

describe("focus after a wizard step change", () => {
  type Fake = { tagName: string; attrs: Map<string, string>; focused: boolean; isConnected: boolean;
    hasAttribute(k: string): boolean; setAttribute(k: string, v: string): void; focus(): void };
  const element = (tagName: string, attrs: Record<string, string> = {}): Fake => ({
    tagName, attrs: new Map(Object.entries(attrs)), focused: false, isConnected: true,
    hasAttribute(k) { return this.attrs.has(k); }, setAttribute(k, v) { this.attrs.set(k, v); }, focus() { this.focused = true; },
  });
  const rootWith = (found: Record<string, Fake | null>) => ({ querySelector: (selector: string) => found[selector] ?? null }) as unknown as HTMLElement;
  const body = element("BODY");
  const doc = (active: Fake | null) => ({ activeElement: active, body }) as unknown as Document;

  it("moves lost focus to the new step's title", () => {
    const title = element("H2"), trail = element("BUTTON");
    const root = rootWith({ ".miner-step-title": title, ".miner-trail .trail-item.active": trail });
    expect(focusLostStep(root, doc(body))).toBe(true);
    expect(title.focused).toBe(true);
    expect(title.attrs.get("tabindex")).toBe("-1");
    expect(trail.focused).toBe(false);
  });
  it("falls back to the breadcrumb's current step, and to a card that left the page", () => {
    const current = element("SPAN"), gone = element("BUTTON");
    gone.isConnected = false;
    expect(focusLostStep(rootWith({ ".miner-trail .trail-item.current": current }), doc(gone))).toBe(true);
    expect(current.focused).toBe(true);
  });
  it("leaves focus that is still on the page alone", () => {
    const title = element("H2"), trail = element("BUTTON");
    expect(focusLostStep(rootWith({ ".miner-step-title": title }), doc(trail))).toBe(false);
    expect(title.focused).toBe(false);
    expect(focusLostStep(null, doc(body))).toBe(false);
  });

  // useStepFocus under a minimal hook runtime that mounts as StrictMode does:
  // refs are attached before effects run, and the mount's effects run, are
  // cleaned up and run again, with refs kept.
  it("does not move focus when a wizard mounts under StrictMode, only when its step changes", async () => {
    type Effect = { fn: () => void | (() => void); deps: unknown[]; cleanup?: void | (() => void) };
    const refs: { current: unknown }[] = [], effects: Effect[] = [];
    let refAt = 0, effectAt = 0, mounting = true, pending: (() => void)[] = [];
    vi.resetModules();
    vi.doMock("react", () => ({
      useRef: (initial: unknown) => (refs[refAt++] ??= { current: initial }),
      useEffect: (fn: Effect["fn"], deps: unknown[]) => {
        const at = effectAt++, before = effects[at];
        if (before && deps.every((d, i) => Object.is(d, before.deps[i]))) return;
        pending.push(() => {
          if (before?.cleanup) before.cleanup();
          const effect: Effect = { fn, deps, cleanup: fn() };
          if (mounting) { if (effect.cleanup) effect.cleanup(); effect.cleanup = fn(); }
          effects[at] = effect;
        });
      },
    }));
    try {
      const { useStepFocus } = await import("../src/step-focus");
      const title = element("H2");
      const root = rootWith({ ".miner-step-title": title });
      const focus = vi.fn();
      vi.stubGlobal("document", { activeElement: body, body });
      title.focus = () => { focus(); title.focused = true; };
      const render = (step: string) => {
        refAt = effectAt = 0; pending = [];
        useStepFocus<HTMLElement>(step).current = root;
        for (const run of pending) run();
      };
      render("miner");
      expect(focus).not.toHaveBeenCalled();
      mounting = false;
      render("miner");
      expect(focus).not.toHaveBeenCalled();
      render("variant");
      expect(focus).toHaveBeenCalledTimes(1);
      render("variant");
      expect(focus).toHaveBeenCalledTimes(1);
      render("miner");
      expect(focus).toHaveBeenCalledTimes(2);
    } finally {
      vi.doUnmock("react");
      vi.resetModules();
    }
  });
});

describe("keyboard-scrollable activity table", () => {
  it("is one of the managed scroll boxes", () => {
    let selector = "";
    vi.stubGlobal("getComputedStyle", () => ({ overflowX: "auto", overflowY: "visible" }));
    syncScrollableFocus({ querySelectorAll: (s: string) => { selector = s; return []; } } as unknown as ParentNode);
    expect(selector.split(",").map((s) => s.trim())).toContain(".activity-table-scroll");
  });
});
