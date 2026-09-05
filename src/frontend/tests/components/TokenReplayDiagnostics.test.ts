import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import { TokenReplayDiagnostics } from "../../src/components/Conformance";
import type { TokenReplayTraceDiagnostics } from "../../src/types";

const trace = (trace_index = 0): TokenReplayTraceDiagnostics => ({
  trace_index, case_id: "shared-case", trace_is_fit: false, trace_fitness: 0.75,
  produced: 7, consumed: 6, missing: 2, remaining: 1,
  replay_stopped: true, replayed_events: 3,
  activated_transitions: [91, 4, 91], transitions_with_problems: [4],
  enabled_transitions: [27], reached_marking: [0, 2],
  unknown_event_indices: [2], unknown_activities: ["<missing-step>"], places: [],
});

const render = (traces?: TokenReplayTraceDiagnostics[]) =>
  renderToStaticMarkup(createElement(TokenReplayDiagnostics, { traces }));

describe("per-case token replay diagnostics", () => {
  it("does not show a panel for older or non-opt-in exports", () => {
    expect(render()).toBe("");
    expect(render([])).toContain("Diagnostics were requested, but there are no source traces.");
  });

  it("distinguishes duplicate case identities by source index and displays token counters", () => {
    const html = render([trace(3), { ...trace(8), trace_is_fit: true, trace_fitness: 1 }]);
    expect(html).toContain("Source trace #3");
    expect(html).toContain("Source trace #8");
    expect(html.match(/Case shared-case/g)).toHaveLength(2);
    expect(html).toContain("unfit · fitness 75.0%");
    expect(html).toContain("fit · fitness 100.0%");
    expect(html).toContain("Produced 7 · Consumed 6 · Missing 2 · Remaining 1");
  });

  it("expands explicit model transition IDs and escaped unknown labels without activity lookup", () => {
    const html = render([trace()]);
    expect(html).toContain("<details>");
    expect(html).toContain("Fired transition IDs (firing order)</dt><dd>91, 4, 91");
    expect(html).toContain("Problem transition IDs</dt><dd>4");
    expect(html).toContain("Enabled transition IDs</dt><dd>27");
    expect(html).toContain("Unknown event indices</dt><dd>2");
    expect(html).toContain("&lt;missing-step&gt;");
    expect(html).toContain("3 replayed events · replay stopped early");
  });

  it("caps source rows and detail lists with explicit omitted counts", () => {
    const rows = Array.from({ length: 32 }, (_, index) => trace(index));
    rows[0].activated_transitions = Array.from({ length: 103 }, (_, index) => index);
    const html = render(rows);
    expect(html.match(/<li class="variant">/g)).toHaveLength(30);
    expect(html).not.toContain("Source trace #30");
    expect(html).toContain("2 diagnostic rows omitted");
    expect(html).toContain("3 more omitted");
  });

  it("labels empty identity and empty detail lists without implying missing diagnostics", () => {
    const html = render([{ ...trace(), case_id: "", unknown_activities: [],
      unknown_event_indices: [], replay_stopped: false }]);
    expect(html).toContain("Case (empty ID)");
    expect(html).toContain("Unknown activity labels</dt><dd>none");
    expect(html).toContain("replay completed");
  });
});
