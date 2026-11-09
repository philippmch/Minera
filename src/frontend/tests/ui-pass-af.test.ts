import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { parsePythonFloat } from "../src/app-logic";
import { dfgErrorText } from "../src/components/EventLogView";
import { defaultPlayoutTableId, playoutTableListed } from "../src/components/PlayoutWorkspace";
import { Landing, MiningStatusBar } from "../src/components/Landing";
import { fetchMineProgress, mineWaitText, parseMineProgress, waitForCanceledMine } from "../src/mine-progress";
import { svgCanvasBox } from "../src/visualization-download";
import filters from "../src/components/Filters.tsx?raw";

afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals(); });
const json = (value: unknown) => new Response(JSON.stringify(value), {status: 200, headers: {"Content-Type": "application/json"}});
const running = {family: "MINE_PROGRESS", running: true, request_matches: false, stage: "Reading the event log", stage_index: 1,
  stages_done: 0, stages_total: 26, elapsed_seconds: 3, stage_elapsed_seconds: 3, canceling: true};

describe("Python float() syntax for ratio controls", () => {
  it("reads decimal text as float() does and refuses what it refuses", () => {
    expect(parsePythonFloat(" 1.5 ")).toBe(1.5);
    expect(parsePythonFloat("1_000.25")).toBe(1000.25);
    expect(parsePythonFloat("-.5e1")).toBe(-5);
    expect(parsePythonFloat("5.")).toBe(5);
    expect(parsePythonFloat("-Infinity")).toBe(-Infinity);
    expect(parsePythonFloat("iNf")).toBe(Infinity);
    expect(parsePythonFloat("nan")).toBeNaN();
    for (const text of ["0x10", "1e", ".", "", " ", "1__0", "_1", "1_", "1,5", "1.2.3", "infinit", "0b1"]) expect(parsePythonFloat(text)).toBeNull();
  });
  it("leaves the Filters Declare ratio inputs without [0,1] bounds", () => {
    const declare = filters.slice(filters.indexOf("<span>Declare min support ratio</span>"), filters.indexOf("Declare templates"));
    expect(declare).not.toMatch(/min=\{0\}|max=\{1\}|step=\{0\.05\}/);
    expect(declare).toContain('step="any"');
  });
});

describe("DFG export errors", () => {
  it("names controls by their labels and a missing activity as such", () => {
    expect(dfgErrorText("most_common_paths requires noise_threshold")).toBe("“Most common paths” requires “Noise threshold”");
    expect(dfgErrorText("keep_all_activities requires paths_percentage or keep_connected")).toBe("“Keep all activities” requires “Paths percentage” or “Connected threshold”");
    expect(dfgErrorText("KeyError: 'Nonexistent'")).toBe("No activity “Nonexistent” in this log (KeyError: 'Nonexistent').");
    expect(dfgErrorText("could not reach the Minera backend")).toBe("could not reach the Minera backend");
  });
});

describe("playout inspection tables", () => {
  const table = (id: string, rows = 1) => ({id, rows: Array.from({length: rows}, () => [])});
  it("opens on the generated events and hides empty and carrier tables", () => {
    const tables = [table("summary"), table("/result/value", 2), table("/result/value/native_state/attributes/carrier", 4), table("/x/carrier/rows", 0), table("complete_generated_events", 200)];
    expect(defaultPlayoutTableId(tables)).toBe("complete_generated_events");
    expect(tables.filter(playoutTableListed).map(t => t.id)).toEqual(["summary", "/result/value", "complete_generated_events"]);
    expect(defaultPlayoutTableId([table("summary"), table("/result/value")])).toBe("/result/value");
    expect(defaultPlayoutTableId([table("/a", 0), table("b")])).toBe("b");
  });
});

describe("SVG export background", () => {
  it("covers the whole canvas when the viewBox has another aspect", () => {
    expect(svgCanvasBox([-246, -60, 852, 1442], 666, 646)).toEqual([-246 - (666 * 1442 / 646 - 852) / 2, -60, 666 * 1442 / 646, 1442]);
    expect(svgCanvasBox([0, 0, 100, 50], 100, 100, "xMinYMin meet")).toEqual([0, 0, 100, 100]);
    expect(svgCanvasBox([0, 0, 100, 50], 100, 100, "xMidYMax")).toEqual([0, -50, 100, 100]);
    expect(svgCanvasBox([-30, 10, 800, 400], 1000, 500)).toEqual([-30, 10, 800, 400]);
    expect(svgCanvasBox([0, 0, 100, 50], 100, 100, "none")).toEqual([0, 0, 100, 50]);
    expect(svgCanvasBox([0, 0, 100, 50], 100, 100, "xMidYMid slice")).toEqual([0, 0, 100, 50]);
  });
});

describe("waiting on a canceled mine", () => {
  it("names a canceled mine of this tab that still holds the backend", () => {
    expect(mineWaitText(parseMineProgress(running))).toBe("Waiting for the canceled mine to stop…");
    expect(mineWaitText(parseMineProgress({...running, canceling: false}))).toBeNull();
    expect(mineWaitText(parseMineProgress({...running, request_matches: true}))).toBeNull();
    expect(mineWaitText({running: false})).toBeNull();
  });
  it("asks about whatever mine runs and polls until the canceled one stopped", async () => {
    const answers = [running, running, {family: "MINE_PROGRESS", running: false}];
    const fetch = vi.fn(async (_path: string, _init?: RequestInit) => json(answers.shift()));
    vi.stubGlobal("fetch", fetch);
    await waitForCanceledMine(undefined, 10_000, 1);
    expect(fetch).toHaveBeenCalledTimes(3);
    expect(fetch.mock.calls.every(call => call[0] === "/api/mine-progress")).toBe(true);
    vi.stubGlobal("fetch", vi.fn(async () => json({...running, canceling: false})));
    await expect(waitForCanceledMine(undefined, 10_000, 1)).resolves.toBeUndefined();
    await expect(fetchMineProgress(null)).resolves.toMatchObject({running: true, canceling: false});
  });
  it("gives up after its timeout so the POST reports the busy server", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => json(running)));
    await expect(waitForCanceledMine(undefined, 0, 1)).resolves.toBeUndefined();
  });
});

describe("mining status while a log is open", () => {
  it("shows what is loading, the stage line and Cancel; then a stopping line", () => {
    const loading = renderToStaticMarkup(createElement(MiningStatusBar, {label: "Opening big.csv…", progress: parseMineProgress({...running, request_matches: true, canceling: false}), onCancel: () => {}}));
    expect(loading).toContain("Opening big.csv…");
    expect(loading).toContain("Reading the event log · stage 1 of 26");
    expect(loading).toContain(">Cancel</button>");
    const stopping = renderToStaticMarkup(createElement(MiningStatusBar, {label: null}));
    expect(stopping).toContain("Stopping the canceled mine…");
    expect(stopping).not.toContain("Cancel</button>");
  });
  it("says on the landing page that a canceled mine is still stopping", () => {
    const markup = renderToStaticMarkup(createElement(Landing, {onOpenFile: () => {}, onLoadSample: () => {}, onLoadOcelSample: () => {}, onLoadResourceSample: () => {}, error: null, mining: false, stopping: true}));
    expect(markup).toContain("Stopping the canceled mine…");
  });
});
