import { afterEach, describe, expect, it, vi } from "vitest";
import { CANCEL_MINING_TIMEOUT_MS, cancelMiningRequest } from "../src/active-mining-source";

afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals(); });

describe("cancel-mine timeout", () => {
  it("settles a stalled cancel as unconfirmed and aborts its request", async () => {
    vi.useFakeTimers();
    let signal: AbortSignal | undefined;
    vi.stubGlobal("fetch", vi.fn((_path: string, init?: RequestInit) => {
      signal = init?.signal ?? undefined;
      return new Promise<Response>(() => {});  // the backend never answers
    }));
    let settled: unknown;
    const pending = cancelMiningRequest("x").then(outcome => { settled = outcome; });
    await vi.advanceTimersByTimeAsync(CANCEL_MINING_TIMEOUT_MS - 1);
    expect(settled).toBeUndefined();
    expect(signal?.aborted).toBe(false);
    await vi.advanceTimersByTimeAsync(1);
    await pending;
    expect(settled).toEqual({ confirmed: false });
    expect(signal?.aborted).toBe(true);
  });

  it("settles when the answer's body stalls, and honours a custom bound", async () => {
    vi.useFakeTimers();
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, json: () => new Promise(() => {}) })));
    const pending = cancelMiningRequest("x", 50);
    await vi.advanceTimersByTimeAsync(50);
    await expect(pending).resolves.toEqual({ confirmed: false });
  });

  it("a prompt answer wins and clears the timer", async () => {
    vi.useFakeTimers();
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"family":"ACTIVE_SOURCE_CANCEL","canceled":true,"active_source_id":"4"}')));
    await expect(cancelMiningRequest("x")).resolves.toEqual({ confirmed: true, canceled: true, activeSourceId: "4" });
    expect(vi.getTimerCount()).toBe(0);
  });
});
