import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it } from "vitest";
import { TokenReplayDiagnostics, TokenReplayDurationDiagnosticsView } from "../../src/components/Conformance";
import type { TokenReplayTraceDiagnostics, TokenReplayDurationDiagnostics } from "../../src/types";
import { TokenReplayRootCauseDiagnosticsView } from "../../src/components/Conformance";
import type { TokenReplayRootCauseDiagnostics, TokenReplayRootCauseModel } from "../../src/types";

const rootCauseModel: TokenReplayRootCauseModel = {
  fit_count: 3, problem_count: 1, feature_names: ["amount", "team@<sales>"],
  classes: ["fit", "underfed"], data: [[-1, 1], [null, 0]], target: [0, 1],
  source_trace_indices: [2, 5], tree: [
    { feature_index: 0, threshold: -0, left: 1, right: 2, class_counts: [3, 1], missing_go_to_left: false },
    { feature_index: -1, threshold: 0, left: -1, right: -1, class_counts: [3, 0], missing_go_to_left: false },
    { feature_index: -1, threshold: 0, left: -1, right: -1, class_counts: [0, 1], missing_go_to_left: false },
  ],
};
const rootCauseDiagnostics: TokenReplayRootCauseDiagnostics = {
  string_attributes: ["team"], numeric_attributes: ["amount"], enable_multiplier: false,
  transitions: [{ ...rootCauseModel, transition_index: 7 }], unknown_activities: [],
};
const renderRootCauses = (diagnostics?: TokenReplayRootCauseDiagnostics) =>
  renderToStaticMarkup(createElement(TokenReplayRootCauseDiagnosticsView, { diagnostics }));

describe("root-cause diagnostic classifiers", () => {
  it("distinguishes typed activity keys from their string spelling", () => {
    const html = renderRootCauses({ ...rootCauseDiagnostics, unknown_activities: [
      { ...rootCauseModel, activity: "1", activity_is_null: false,
        activity_attribute: { key: "concept:name", value: "1", kind: "int", container: "scalar", null: false, typed_value: 1 } },
      { ...rootCauseModel, activity: "1", activity_is_null: false },
      { ...rootCauseModel, activity: "b'1'", activity_is_null: false,
        activity_attribute: { key: "task", value: "31", kind: "bytes", container: "scalar", null: false } },
      { ...rootCauseModel, activity: "(1,)", activity_is_null: false,
        activity_attribute: { key: "concept:name", value: "", kind: "string", container: "tuple", null: false } },
      { ...rootCauseModel, activity: "9007199254740993", activity_is_null: false,
        activity_attribute: { key: "concept:name", value: "stale", kind: "int", container: "scalar", null: false, typed_value: 9007199254740992 } },
    ] });
    expect(html).toContain("<strong>1 (int)</strong>");
    expect(html).toContain("<strong>&quot;1&quot;</strong>");
    expect(html).toContain("<strong>b&#x27;1&#x27;</strong>");
    expect(html).toContain("<strong>(1,) (tuple)</strong>");
    expect(html).toContain("<strong>9007199254740993 (int)</strong>");
  });
  it("distinguishes absent and requested empty results", () => {
    expect(renderRootCauses()).toBe("");
    expect(renderRootCauses({ ...rootCauseDiagnostics, transitions: [] }))
      .toContain("Root-cause diagnostics were requested, but there are no comparisons.");
  });
  it("renders cohort counts, signed thresholds, leaves and missing routing safely", () => {
    const html = renderRootCauses(rootCauseDiagnostics);
    for (const text of ["Transition #7", "3 fit cases", "1 problem cases", "amount", "&lt;sales&gt;",
      "≤ -0", "Missing (NaN) values go to #2", "predict fit", "predict underfed",
      "associations, not causal proof", "Showing 3 of 3 tree nodes", "one-hot indicators"])
      expect(html).toContain(text);
    expect(html).not.toContain("<sales>");
  });
  it("distinguishes null and empty labels and renders missing-only splits", () => {
    const html = renderRootCauses({ ...rootCauseDiagnostics, unknown_activities: [
      { ...rootCauseModel, activity: "", activity_is_null: true },
      { ...rootCauseModel, activity: "", activity_is_null: false },
    ], transitions: [{ ...rootCauseModel, transition_index: 1,
      tree: [{ ...rootCauseModel.tree[0], missing_only: true }] }] });
    expect(html).toContain("(null activity)");
    expect(html).toContain("(empty string)");
    expect(html).toContain("is present, go to #1; if missing, go to #2");
  });
  it("caps classifiers, features and tree nodes with explicit omitted counts", () => {
    const model = { ...rootCauseModel, feature_names: Array.from({ length: 105 }, (_, i) => `feature${i}`),
      tree: Array.from({ length: 104 }, () => rootCauseModel.tree[1]) };
    const html = renderRootCauses({ ...rootCauseDiagnostics,
      transitions: Array.from({ length: 33 }, (_, i) => ({ ...model, transition_index: i })) });
    expect(html).toContain("Showing 30 of 33 classifiers (limit 30)");
    expect(html).toContain("3 classifiers omitted");
    expect(html).toContain("Showing 100 of 104 tree nodes (limit 100)");
    expect(html).toContain("4 tree nodes omitted");
    expect(html).toContain("5 more omitted");
    expect(html).not.toContain("feature104");
    expect(html).not.toContain("Transition #32");
  });
});

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

const comparison = {
  fit_trace_indices: [1, 3], problem_trace_indices: [7], fit_count: 2, problem_count: 1,
  fit_median_seconds: 10.5, problem_median_seconds: -5.25, relative_throughput: -0.5,
};
const renderDurations = (diagnostics?: TokenReplayDurationDiagnostics) =>
  renderToStaticMarkup(createElement(TokenReplayDurationDiagnosticsView, { diagnostics }));

describe("token replay duration comparisons", () => {
  it("keeps older exports unchanged and distinguishes an empty opt-in result", () => {
    expect(renderDurations()).toBe("");
    const html = renderDurations({ timestamp_attribute: "time:timestamp", transitions: [], unknown_activities: [] });
    expect(html).toContain("Duration diagnostics were requested, but there are no comparisons.");
    expect(html).not.toContain("<table");
  });

  it("renders accessible transition cohorts, signed seconds and duration ratios", () => {
    const html = renderDurations({ timestamp_attribute: "completed_at", transitions: [
      { ...comparison, transition_index: 17 },
    ], unknown_activities: [] });
    expect(html).toContain("<caption>Transition duration comparisons</caption>");
    expect(html).toContain('<th scope="row">Transition #17</th>');
    expect(html).toContain('<th scope="col" class="num">Label present, no reported problem cases</th>');
    expect(html).toContain('<th scope="col" class="num">Problem execution median (seconds)</th>');
    expect(html).toContain('<td class="num">2</td><td class="num">1</td>');
    expect(html).toContain('<td class="num">10.5</td><td class="num">-5.25</td>');
    expect(html).toContain('<td class="num">-0.5×</td>');
    expect(html).toContain("<code>completed_at</code>");
    expect(html).toContain("Unknown activity duration comparisons: no comparisons.");
    expect(html).toContain("cases with a reported problem for it");
    expect(html).toContain("The baseline does not require a recorded firing.");
  });

  it("shows the supplied zero ratio for zero and negative baseline medians", () => {
    for (const fit_median_seconds of [0, -10]) {
      const html = renderDurations({ timestamp_attribute: "time:timestamp", transitions: [
        { ...comparison, transition_index: 1, fit_median_seconds, relative_throughput: 0 },
      ], unknown_activities: [] });
      expect(html).toContain(`<td class="num">${fit_median_seconds}</td>`);
      expect(html).toContain('<td class="num">0×</td>');
      expect(html).toContain("it is 0 when the baseline median is zero or negative.");
    }
  });

  it("distinguishes null, empty and literal activity labels and explains the unknown baseline", () => {
    const html = renderDurations({ timestamp_attribute: "time:timestamp", transitions: [],
      unknown_activities: [
        { ...comparison, activity: "", activity_is_null: true },
        { ...comparison, activity: "", activity_is_null: false },
        { ...comparison, activity: "<missing-step>", activity_is_null: false },
      ] });
    expect(html).toContain('<th scope="row">(null activity)</th>');
    expect(html).toContain('<th scope="row">(empty string)</th>');
    expect(html).toContain("&lt;missing-step&gt;");
    expect(html).not.toContain("<missing-step>");
    expect(html).toContain("cases free of all observed unknown labels");
    expect(html).toContain("Containing activity cases");
  });

  it("bounds both comparison tables and reports exact omitted counts", () => {
    const html = renderDurations({ timestamp_attribute: "time:timestamp",
      transitions: Array.from({ length: 102 }, (_, transition_index) => ({ ...comparison, transition_index })),
      unknown_activities: Array.from({ length: 103 }, (_, index) => ({ ...comparison,
        activity: `unknown-${index}`, activity_is_null: false })),
    });
    expect(html).toContain("Transition #99");
    expect(html).not.toContain("Transition #100");
    expect(html).toContain("unknown-99");
    expect(html).not.toContain("unknown-100");
    expect(html).toContain("2 comparison rows omitted.");
    expect(html).toContain("3 comparison rows omitted.");
  });
});

describe("per-case token replay diagnostics", () => {
  it("ties ordered firings and blocked transitions to source events", () => {
    const html = render([{ ...trace(), event_diagnostics: [
      { event_index: 0, activated_transitions: [91, 4], transitions_with_problems: [] },
      { event_index: 1, activated_transitions: [], transitions_with_problems: [8] },
    ] }]);
    expect(html).toContain("Event #0 · Fired: 91, 4 · Problems: none");
    expect(html).toContain("Event #1 · Fired: none · Problems: 8");
    expect(html).toContain("Final hidden routing is included in the trace firing list only.");
    expect(render([trace()])).not.toContain("Source-event firings");
  });
  it("caps event records without silently truncating the display", () => {
    const html = render([{ ...trace(), event_diagnostics: Array.from({ length: 102 }, (_, event_index) => ({
      event_index, activated_transitions: [], transitions_with_problems: [],
    })) }]);
    expect(html).toContain("Event #99 ·");
    expect(html).not.toContain("Event #100 ·");
    expect(html).toContain("2 event records omitted.");
  });
  it("identifies explicit null unknown labels without confusing empty strings", () => {
    const html = render([{ ...trace(), unknown_activities: ["", ""],
      unknown_event_indices: [0, 1], unknown_activity_is_null: [true, false] }]);
    expect(html).toContain("Unknown activity labels</dt><dd>(null), ");
  });
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
