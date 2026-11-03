import { afterEach, describe, expect, it, vi } from "vitest";
import app from "../src/App.tsx?raw";
import { acknowledgeMiningRequest, cancelMiningRequest } from "../src/active-mining-source";
import { LiveSourceHandback } from "../src/app-logic";

afterEach(() => { vi.unstubAllGlobals(); });

describe("mining rollback acknowledgement", () => {
  it("posts the request id and reports the backend's acknowledgement", async () => {
    const fetch = vi.fn(async () => new Response('{"family":"ACTIVE_SOURCE_ACK","request_id":"r","acknowledged":true}'));
    vi.stubGlobal("fetch", fetch);
    await expect(acknowledgeMiningRequest("00000000-0000-4000-8000-000000000001")).resolves.toBe(true);
    expect(fetch).toHaveBeenCalledWith("/api/mine-ack?request_id=00000000-0000-4000-8000-000000000001", {method: "POST"});
  });

  it("never rejects: refusals, odd bodies and network failures settle false", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"error":"x"}', {status: 400})));
    await expect(acknowledgeMiningRequest("r")).resolves.toBe(false);
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"family":"ACTIVE_SOURCE_ACK","acknowledged":false}')));
    await expect(acknowledgeMiningRequest("r")).resolves.toBe(false);
    vi.stubGlobal("fetch", vi.fn(async () => new Response("not json")));
    await expect(acknowledgeMiningRequest("r")).resolves.toBe(false);
    vi.stubGlobal("fetch", vi.fn(async () => {throw new TypeError("network");}));
    await expect(acknowledgeMiningRequest("r")).resolves.toBe(false);
  });

  it("a response that arrived but failed to apply hands the previous source back after the cancel restores it", async () => {
    // The backend was never acknowledged, so cancel-mine restores the previous source "22".
    vi.stubGlobal("fetch", vi.fn(async () => new Response('{"family":"ACTIVE_SOURCE_CANCEL","request_id":"r","canceled":true,"active_source_id":"22"}')));
    const restored: string[] = [];
    await expect(new LiveSourceHandback().handback("22", cancelMiningRequest("r"), id => restored.push(id))).resolves.toBe(true);
    expect(restored).toEqual(["22"]);
  });

  it("acknowledges only after a load or re-mine was applied", () => {
    const load = app.slice(app.indexOf("const loadFile ="), app.indexOf("const loadSample ="));
    const filters = app.slice(app.indexOf("const applyFilters ="), app.indexOf("useEffect(", app.indexOf("const applyFilters =")));
    for (const flow of [load, filters]) {
      const ack = flow.indexOf("acknowledgeMiningRequest(ticket.request_id)");
      expect(ack).toBeGreaterThan(-1);
      // After the analysis is applied, before the failure path that cancels.
      expect(ack).toBeGreaterThan(flow.indexOf("setLiveSourceId(descriptor.source_id)"));
      expect(ack).toBeLessThan(flow.indexOf(".catch("));
      expect(flow.indexOf("acknowledgeMiningRequest", ack + 1)).toBe(-1);
    }
  });
});
