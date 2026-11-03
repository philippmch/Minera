import { afterEach, describe, expect, it, vi } from "vitest";
import { alignModel, checkModel, fetchPlayout } from "../src/api";
import {
  IMPORTED_MODEL_HEADER, currentImportedModelId, importedModelIdOf, importedModelRequestInit, setImportedModelId,
} from "../src/imported-model-binding";

afterEach(() => { vi.unstubAllGlobals(); setImportedModelId(undefined); });

const checkPayload = (id?: number) => ({
  ...(id === undefined ? {} : { imported_model_id: id }),
  activities: [], model: { miner: "imported", sound: true },
});
const headerOf = (init: RequestInit | undefined) => new Headers(init?.headers).get(IMPORTED_MODEL_HEADER);

describe("imported-model binding", () => {
  it("reads only positive integer ids", () => {
    expect(importedModelIdOf({ imported_model_id: 4 })).toBe(4);
    for (const value of [null, {}, { imported_model_id: 0 }, { imported_model_id: "4" }, { imported_model_id: 1.5 }])
      expect(importedModelIdOf(value)).toBeUndefined();
  });

  it("leaves requests that do not name the imported model untouched", () => {
    setImportedModelId(9);
    const init = { method: "POST" };
    expect(importedModelRequestInit("/api/align?miner=inductive", init)).toBe(init);
    expect(importedModelRequestInit("/api/align", undefined)).toBeUndefined();
    expect(headerOf(importedModelRequestInit("/api/align?miner=imported", init))).toBe("9");
    const form = new FormData(); form.append("miner", "imported");
    expect(headerOf(importedModelRequestInit("/api/decide", { method: "POST", body: form }))).toBe("9");
    setImportedModelId(undefined);
    expect(importedModelRequestInit("/api/align?miner=imported", init)).toBe(init);
  });

  it("binds runs on the imported model to the id check-model returned", async () => {
    const calls: Array<[string, RequestInit | undefined]> = [];
    vi.stubGlobal("fetch", vi.fn(async (path: string, init?: RequestInit) => {
      calls.push([path, init]);
      if (path.startsWith("/api/check-model")) return new Response(JSON.stringify(checkPayload(12)));
      if (path.startsWith("/api/playout")) return new Response("<log></log>");
      return new Response(JSON.stringify({ computed: true, exhausted: false, fitness: 1, variants: [] }));
    }));
    await checkModel(new File(["<pnml/>"], "m.pnml"));
    expect(currentImportedModelId()).toBe("12");
    // check-model itself is never bound: it is what replaces the model.
    expect(headerOf(calls[0][1])).toBeNull();
    await alignModel("imported");
    await alignModel("inductive");
    await fetchPlayout("imported", { traces: 10 });
    expect(headerOf(calls[1][1])).toBe("12");
    expect(headerOf(calls[2][1])).toBeNull();
    expect(headerOf(calls[3][1])).toBe("12");
  });

  it("forgets the id when a check-model answer carries none", async () => {
    setImportedModelId(3);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify(checkPayload()))));
    await checkModel(new File(["<pnml/>"], "m.pnml"));
    expect(currentImportedModelId()).toBeUndefined();
  });

  it("surfaces the server's stale-model refusal as the request's error", async () => {
    setImportedModelId(5);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({
      error: "The imported model changed while this request was pending; run it again.",
    }), { status: 409 })));
    await expect(alignModel("imported")).rejects.toThrow("imported model changed");
  });
});
