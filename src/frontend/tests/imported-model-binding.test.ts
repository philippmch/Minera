import { afterEach, describe, expect, it, vi } from "vitest";
import { alignModel, analyzePetriModel, checkModel, fetchPlayout } from "../src/api";
import {
  IMPORTED_MODEL_HEADER, REPLACED_WHILE_PENDING_MESSAGE, STALE_IMPORTED_MODEL_MESSAGE, currentImportedModelId,
  importedModelIdOf, importedModelRequestInit, setImportedModelId,
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

  it("binds prefixed model selectors such as petri_analysis_miner", async () => {
    setImportedModelId(7);
    expect(headerOf(importedModelRequestInit("/api/petri-analysis?petri_analysis_miner=imported", {}))).toBe("7");
    expect(headerOf(importedModelRequestInit("/api/petri-analysis?petri_analysis_miner=inductive", {}))).toBeNull();
    expect(headerOf(importedModelRequestInit("/api/x?miner=inductive&left_miner=imported", {}))).toBe("7");
    // Only selector fields count: a value "imported" elsewhere does not bind.
    expect(headerOf(importedModelRequestInit("/api/x?source=imported&minerals=imported", {}))).toBeNull();
    const form = new FormData(); form.append("compare_miner", "imported");
    expect(headerOf(importedModelRequestInit("/api/compare", { method: "POST", body: form }))).toBe("7");
    const calls: Array<RequestInit | undefined> = [];
    vi.stubGlobal("fetch", vi.fn(async (_path: string, init?: RequestInit) => { calls.push(init); return new Response("{}"); }));
    await analyzePetriModel({ family: "workflow_net", miner: "imported" }).catch(() => undefined);
    expect(headerOf(calls[0])).toBe("7");
  });

  it("tells the user to check the model again when the server's model is not the page's", async () => {
    setImportedModelId(5);
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ error: STALE_IMPORTED_MODEL_MESSAGE }), { status: 409 })));
    await expect(alignModel("imported")).rejects.toThrow(STALE_IMPORTED_MODEL_MESSAGE);
    // Rerunning does not help and keeps saying so: the page still holds id 5.
    await expect(alignModel("imported")).rejects.toThrow("check the model again");
    expect(currentImportedModelId()).toBe("5");
  });

  it("says to rerun when this page checked a newer model while the request was pending", async () => {
    setImportedModelId(5);
    vi.stubGlobal("fetch", vi.fn(async () => {
      setImportedModelId(6);  // this page's own check-model answer arrived meanwhile
      return new Response(JSON.stringify({ error: STALE_IMPORTED_MODEL_MESSAGE }), { status: 409 });
    }));
    await expect(alignModel("imported")).rejects.toThrow(REPLACED_WHILE_PENDING_MESSAGE);
  });
});
