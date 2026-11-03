import { afterEach, describe, expect, it, vi } from "vitest";
import { alignDecomposed, alignModel, alignProcessTree, isUncomputedAlignmentPayload } from "../src/api";

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
