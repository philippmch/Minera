import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { MINE_PROGRESS_INTERVAL_MS, fetchMineProgress, formatElapsed, mineProgressText, parseMineProgress, watchMineProgress } from "../src/mine-progress";
import { Landing } from "../src/components/Landing";
import app from "../src/App.tsx?raw";

afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals(); });

const running = {family: "MINE_PROGRESS", running: true, request_matches: true, stage: "Inductive net: reachability", stage_index: 13,
  stages_done: 12, stages_total: 26, elapsed_seconds: 65.4, stage_elapsed_seconds: 50.1, canceling: false};
const json = (value: unknown, status = 200) => new Response(JSON.stringify(value), {status, headers: {"Content-Type": "application/json"}});

describe("mine progress answers", () => {
  it("reads a running mine's stage, counts and times", () => {
    expect(parseMineProgress(running)).toEqual({running: true, requestMatches: true, stage: "Inductive net: reachability", stageIndex: 13,
      stagesDone: 12, stagesTotal: 26, elapsedSeconds: 65.4, stageElapsedSeconds: 50.1, canceling: false});
    expect(parseMineProgress({family: "MINE_PROGRESS", running: false})).toEqual({running: false});
    expect(parseMineProgress({...running, stages_total: null})?.stagesTotal).toBeNull();
  });
  it("refuses anything that is not a MINE_PROGRESS answer", () => {
    for (const value of [null, 3, "x", {family: "MINE_BUSY", running: true}, {family: "MINE_PROGRESS"}]) expect(parseMineProgress(value)).toBeNull();
  });
  it("formats elapsed time as m:ss and h:mm:ss", () => {
    expect(formatElapsed(0)).toBe("0:00");
    expect(formatElapsed(65.9)).toBe("1:05");
    expect(formatElapsed(3723)).toBe("1:02:03");
  });
  it("shows only this tab's running mine", () => {
    expect(mineProgressText(parseMineProgress(running))).toBe("Inductive net: reachability · stage 13 of 26 · 1:05 elapsed");
    expect(mineProgressText(parseMineProgress({...running, stages_total: null}))).toBe("Inductive net: reachability · stage 13 · 1:05 elapsed");
    expect(mineProgressText(parseMineProgress({...running, request_matches: false}))).toBeNull();
    expect(mineProgressText({running: false})).toBeNull();
    expect(mineProgressText(null)).toBeNull();
  });
});

describe("polling", () => {
  it("asks for this request's progress and reports a failed poll as null", async () => {
    const fetch = vi.fn(async (_path: string, _init?: RequestInit) => json(running));
    vi.stubGlobal("fetch", fetch);
    await expect(fetchMineProgress("a b")).resolves.toMatchObject({stage: "Inductive net: reachability"});
    expect(fetch.mock.calls[0][0]).toBe("/api/mine-progress?request_id=a%20b");
    vi.stubGlobal("fetch", vi.fn(async () => json({error: "x"}, 404)));
    await expect(fetchMineProgress("x")).resolves.toBeNull();
    vi.stubGlobal("fetch", vi.fn(async () => { throw new TypeError("Failed to fetch"); }));
    await expect(fetchMineProgress("x")).resolves.toBeNull();
  });
  it("polls once per interval, one request at a time, until stopped", async () => {
    vi.useFakeTimers();
    let answer: ((response: Response) => void) | undefined;
    const fetch = vi.fn(() => new Promise<Response>(resolve => { answer = resolve; }));
    vi.stubGlobal("fetch", fetch);
    const seen: unknown[] = [];
    const stop = watchMineProgress("id", progress => seen.push(progress));
    await vi.advanceTimersByTimeAsync(0);
    expect(fetch).toHaveBeenCalledTimes(1);
    // A slow answer holds the next poll back.
    await vi.advanceTimersByTimeAsync(3 * MINE_PROGRESS_INTERVAL_MS);
    expect(fetch).toHaveBeenCalledTimes(1);
    answer!(json(running));
    await vi.advanceTimersByTimeAsync(0);
    expect(seen).toHaveLength(1);
    await vi.advanceTimersByTimeAsync(MINE_PROGRESS_INTERVAL_MS - 1);
    expect(fetch).toHaveBeenCalledTimes(1);
    await vi.advanceTimersByTimeAsync(1);
    expect(fetch).toHaveBeenCalledTimes(2);
    const signal = (fetch.mock.calls[1] as unknown[])[1] as RequestInit;
    stop();
    expect(signal.signal?.aborted).toBe(true);
    answer!(json(running));
    await vi.advanceTimersByTimeAsync(5 * MINE_PROGRESS_INTERVAL_MS);
    expect(fetch).toHaveBeenCalledTimes(2);
    expect(seen).toHaveLength(1);
  });
});

describe("mining overlay", () => {
  const render = (progress: unknown) => renderToStaticMarkup(createElement(Landing, {
    onOpenFile: () => {}, onLoadSample: () => {}, onLoadOcelSample: () => {}, onLoadResourceSample: () => {},
    error: null, mining: true, onCancel: () => {}, progress: progress as never,
  }));
  it("shows the stage, the stage count and the elapsed time next to Cancel", () => {
    const markup = render(parseMineProgress(running));
    expect(markup).toContain("Inductive net: reachability · stage 13 of 26 · 1:05 elapsed");
    expect(markup).toMatch(/<progress[^>]*max="26"[^>]*value="12"/);
    expect(markup).toContain(">Cancel</button>");
  });
  it("keeps the plain status until the backend reports this mine", () => {
    for (const progress of [null, {running: false}, parseMineProgress({...running, request_matches: false})]) {
      const markup = render(progress);
      expect(markup).toContain("Mining the event log…");
      expect(markup).not.toContain("landing-progress");
    }
  });
  it("says a canceled mine is stopping", () => {
    expect(render(parseMineProgress({...running, canceling: true}))).toContain("Stopping… Inductive net: reachability");
  });
});

describe("which loads poll mine progress", () => {
  it("polls only loads that post to /api/mine, never OCEL discovery", () => {
    const begin = app.slice(app.indexOf("const beginSourceLoad ="), app.indexOf("\n", app.indexOf("const beginSourceLoad =")));
    expect(begin).toContain("setMiningRequestId(null)");
    expect(begin).not.toContain("setMiningRequestId(ticket.request_id)");
    const ocel = app.slice(app.indexOf("const changeOcelModel ="), app.indexOf("const loadFile ="));
    expect(ocel).not.toContain("setMiningRequestId");
    const load = app.slice(app.indexOf("const loadFile ="), app.indexOf("const loadSample ="));
    const filters = app.slice(app.indexOf("const applyFilters ="), app.indexOf("useEffect(", app.indexOf("const applyFilters =")));
    for (const flow of [load, filters]) expect(flow).toContain("setMiningRequestId(ticket.request_id)");
    // In a file load the id is set where it is sent, after the OCEL branch returned.
    expect(load.indexOf("setMiningRequestId(ticket.request_id)")).toBeGreaterThan(load.indexOf("options.miningRequestId = ticket.request_id"));
  });
});
