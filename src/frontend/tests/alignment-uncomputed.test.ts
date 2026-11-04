import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { alignDecomposed, alignModel, alignmentQuery, alignProcessTree, isUncomputedAlignmentPayload } from "../src/api";
import { AlignmentNotice } from "../src/components/Conformance";
import { UI_ALIGNMENT_TIME_BUDGET_SECONDS, withAlignmentTimeBudget } from "../src/types";

afterEach(() => vi.unstubAllGlobals());

// The shape /api/align answers with status 200 when the search bound is hit.
const exhausted = {
  computed: false, exhausted: true, error: "alignment search exhausted",
  fitness: null, precision: null, fitting_traces: 0, trace_count: 3, total_cost: 0, variants: [],
};

describe("uncomputed alignment results", () => {
  it("recognises computed:false with exhausted and/or error, and nothing else", () => {
    expect(isUncomputedAlignmentPayload(exhausted)).toBe(true);
    expect(isUncomputedAlignmentPayload({ computed: false, exhausted: false })).toBe(true);
    expect(isUncomputedAlignmentPayload({ computed: false, error: "final marking unreachable" })).toBe(true);
    for (const value of [null, [], "x", { computed: true, exhausted: false }, { computed: false },
      { error: "boom" }, { computed: false, exhausted: "yes" }, { computed: false, exhausted: true, error: 3 }])
      expect(isUncomputedAlignmentPayload(value)).toBe(false);
  });

  it("alignModel returns an exhausted result instead of throwing its error", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(exhausted))));
    await expect(alignModel("inductive", { max_states: 1 })).resolves.toEqual(exhausted);
  });

  it("alignModel returns the unreachable-final-marking result too", async () => {
    const unreachable = { ...exhausted, exhausted: false, error: "alignment not computed: final marking unreachable or invalid alignment options" };
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(unreachable))));
    await expect(alignModel("alpha")).resolves.toMatchObject({ computed: false, exhausted: false });
  });

  it("alignModel still throws plain errors and non-2xx uncomputed bodies", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ error: "no cached log; run /api/mine first" }), { status: 409 })));
    await expect(alignModel("inductive")).rejects.toThrow("no cached log");
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(exhausted), { status: 400 })));
    await expect(alignModel("inductive")).rejects.toThrow("alignment search exhausted");
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ error: "max_states must be an integer" }))));
    await expect(alignModel("inductive")).rejects.toThrow("max_states must be an integer");
  });

  it("alignProcessTree keeps an uncomputed result that names its reason", async () => {
    const payload = {
      family: "PROCESS_TREE", variant: "SEARCH_GRAPH_PT", computed: false, completed: false, timed_out: false,
      exhausted: true, error: "alignment search exhausted", total_cost: 0, fitness: 0,
      diagnostics: { visited_states: 1, queued_states: 1, traversed_arcs: 0, lp_solved: 0, traces_aligned: 0, message: "bound" },
      traces: [],
    };
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(payload))));
    await expect(alignProcessTree()).resolves.toMatchObject({ computed: false, exhausted: true });
  });

  it("checked helpers still throw an error-only body", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ computed: false, error: "decomposition failed" }))));
    await expect(alignDecomposed("inductive")).rejects.toThrow("decomposition failed");
  });
});

describe("the app's alignment time budget", () => {
  it("fills an unset global timeout only; an explicit one, including 0 (no limit), is kept", () => {
    expect(withAlignmentTimeBudget({})).toEqual({ max_align_time: UI_ALIGNMENT_TIME_BUDGET_SECONDS });
    expect(withAlignmentTimeBudget({ variant: "state_equation_a_star", max_states: 5 }))
      .toEqual({ variant: "state_equation_a_star", max_states: 5, max_align_time: UI_ALIGNMENT_TIME_BUDGET_SECONDS });
    expect(withAlignmentTimeBudget({ max_align_time: 0 })).toEqual({ max_align_time: 0 });
    expect(withAlignmentTimeBudget({ max_align_time: 600 })).toEqual({ max_align_time: 600 });
    expect(alignmentQuery(withAlignmentTimeBudget({}))).toBe(`max_align_time=${UI_ALIGNMENT_TIME_BUDGET_SECONDS}`);
  });

  it("alignModel hands its abort signal to the request and rethrows the abort", async () => {
    const seen: Array<RequestInit | undefined> = [];
    vi.stubGlobal("fetch", vi.fn(async (_url: string, init?: RequestInit) => {
      seen.push(init);
      if (init?.signal?.aborted) throw new DOMException("aborted", "AbortError");
      return new Response(JSON.stringify(exhausted));
    }));
    const controller = new AbortController();
    await expect(alignModel("inductive", {}, controller.signal)).resolves.toEqual(exhausted);
    expect(seen[0]?.signal).toBe(controller.signal);
    controller.abort();
    await expect(alignModel("inductive", {}, controller.signal)).rejects.toMatchObject({ name: "AbortError" });
    await alignModel("inductive");
    expect(seen[2]?.signal).toBeUndefined();
  });

  it("the exhausted notice names the app budget only when a time limit stopped the search", () => {
    const render = (props: Parameters<typeof AlignmentNotice>[0]) => renderToStaticMarkup(createElement(AlignmentNotice, props));
    const timed = render({ minerName: "Inductive", exhausted: true, timeLimitReached: true, budgetSeconds: 60 });
    expect(timed).toContain("ran out of time");
    expect(timed).toContain("stops alignments after");
    expect(timed).toContain("60");
    expect(timed).toContain("Global timeout");
    const timedExplicit = render({ minerName: "Inductive", exhausted: true, timeLimitReached: true });
    expect(timedExplicit).toContain("ran out of time");
    expect(timedExplicit).not.toContain("stops alignments after");
    expect(timedExplicit).toContain("Per-trace timeout");
    // The state limit stopped it: no time-budget advice even though the budget applied.
    const states = render({ minerName: "Inductive", exhausted: true, timeLimitReached: false, budgetSeconds: 60 });
    expect(states).toContain("reached its state limit");
    expect(states).toContain("State limit");
    expect(states).not.toContain("stops alignments after");
    expect(states).not.toContain("Global timeout");
    // An older server that does not say which bound: neutral wording, no advice.
    const unknown = render({ minerName: "Inductive", exhausted: true, budgetSeconds: 60 });
    expect(unknown).toContain("hit a configured state or time limit");
    expect(unknown).not.toContain("stops alignments after");
    for (const markup of [timed, timedExplicit, states, unknown]) {
      expect(markup).not.toContain("partial numbers");
      expect(markup).toContain("No alignment fitness, precision or cost is available");
    }
    const unsound = render({ minerName: "Alpha", exhausted: false, timeLimitReached: true, budgetSeconds: 60 });
    expect(unsound).toContain("sound workflow net");
    expect(unsound).not.toContain("stops alignments after");
  });
});
