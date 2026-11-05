import {afterEach, describe, expect, it, vi} from "vitest";
import {MINE_BUSY_CANCELING_MESSAGE, MINE_BUSY_MESSAGE, MINE_BUSY_WAIT_MS, MineLimitError, mineActiveDataset} from "../src/api";

afterEach(() => {vi.unstubAllGlobals(); vi.useRealTimers();});

const json = (status: number, body: unknown, headers: Record<string, string> = {}) =>
  new Response(JSON.stringify(body), {status, headers: {"Content-Type": "application/json", ...headers}});

describe("mining admission (one mine at a time, size cap)", () => {
  it("shows the busy message when another log is being mined", async () => {
    const fetch = vi.fn(async () => json(503, {family: "MINE_BUSY", error: "Another log is being mined — wait or cancel it.", canceling: false, retry_after_seconds: 5}, {"Retry-After": "5"}));
    vi.stubGlobal("fetch", fetch);
    await expect(mineActiveDataset(new File(["case_id,activity,timestamp\n"], "log.csv"))).rejects.toThrow(MINE_BUSY_MESSAGE);
    expect(fetch).toHaveBeenCalledTimes(1);
  });

  it("offers no cancel this tab cannot give for a mine started elsewhere", () => {
    // This tab cancels its own mine before it starts another, so a busy mine that is not canceling runs in
    // another tab, window or client: the message must not ask this tab to cancel it.
    expect(MINE_BUSY_MESSAGE).not.toMatch(/wait or cancel it/);
    expect(MINE_BUSY_MESSAGE).toMatch(/another tab, window or client/);
    expect(MINE_BUSY_MESSAGE).toMatch(/where it was started/);
  });

  it("says a canceled mine is still stopping when waiting for it runs out", async () => {
    vi.useFakeTimers();
    const fetch = vi.fn(async () => json(503, {family: "MINE_BUSY", canceling: true, retry_after_seconds: 30}, {"Retry-After": "30"}));
    vi.stubGlobal("fetch", fetch);
    const mined = mineActiveDataset(new File(["x"], "log.csv"));
    const outcome = expect(mined).rejects.toThrow(MINE_BUSY_CANCELING_MESSAGE);
    await vi.advanceTimersByTimeAsync(MINE_BUSY_WAIT_MS);
    await outcome;
    expect(fetch.mock.calls.length).toBeGreaterThan(1);
  });

  it("retries after Retry-After while a canceled mine winds down", async () => {
    vi.useFakeTimers();
    const busy = json(503, {family: "MINE_BUSY", error: MINE_BUSY_MESSAGE, canceling: true, retry_after_seconds: 1}, {"Retry-After": "1"});
    const refused = json(400, {error: "CSV column not found: case_id"});
    const fetch = vi.fn().mockResolvedValueOnce(busy).mockResolvedValueOnce(refused);
    vi.stubGlobal("fetch", fetch);
    const mined = mineActiveDataset(new File(["x"], "log.csv"));
    const outcome = expect(mined).rejects.toThrow("CSV column not found");
    await vi.advanceTimersByTimeAsync(999);
    expect(fetch).toHaveBeenCalledTimes(1);
    await vi.advanceTimersByTimeAsync(1);
    await outcome;
    expect(fetch).toHaveBeenCalledTimes(2);
  });

  it("stops waiting when the mine is aborted", async () => {
    const busy = json(503, {family: "MINE_BUSY", canceling: true}, {"Retry-After": "1"});
    vi.stubGlobal("fetch", vi.fn(async () => busy.clone()));
    const controller = new AbortController();
    const mined = mineActiveDataset(new File(["x"], "log.csv"), {}, controller.signal);
    setTimeout(() => controller.abort(new DOMException("stop", "AbortError")), 20);
    await expect(mined).rejects.toThrow("stop");
  });

  it("names the size cap and its estimate on 413 MINE_LIMIT", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => json(413, {
      family: "MINE_LIMIT", limit: "mine_memory_budget", estimated_events: 300, estimated_bytes: 3072000,
      error: "mining this log would need about 5 MiB (300 events), more than the server's mining memory budget of 2 MiB. Filter or split the log before uploading it (fewer cases or a shorter period), or start minera_server with a larger --mine-memory-budget or --max-mine-events.",
    })));
    const failure = await mineActiveDataset(new File(["x"], "log.csv")).catch((error: unknown) => error);
    expect(failure).toBeInstanceOf(MineLimitError);
    const limit = failure as MineLimitError;
    expect(limit.message).toContain("mining memory budget");
    expect(limit.message).toContain("--mine-memory-budget");
    expect(limit.estimatedEvents).toBe(300);
    expect(limit.estimatedBytes).toBe(3072000);
    expect(limit.limit).toBe("mine_memory_budget");
  });

  it("keeps the upload-limit message for a body-less 413", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response("", {status: 413})));
    await expect(mineActiveDataset(new File(["x"], "log.xes"))).rejects.toThrow("128 MiB limit");
  });
});
