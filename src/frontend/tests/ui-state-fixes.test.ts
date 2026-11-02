import { afterEach, describe, expect, it, vi } from "vitest";
import { LiveSourceHandback, caseRowEntries, identityKey, optionalNumber, runSurvivesModelImport } from "../src/app-logic";
import { assertMiningSourceBinding } from "../src/active-mining-source";
import { fetchDfg, fetchXes, mineActiveDataset, petriAntiAlignmentQuery, petriMultiAlignmentQuery } from "../src/api";
import { nativeResponseFailure, readNativeResponse } from "../src/native-response";
import { structuralReplayKey, type StructuralRun } from "../src/structural-workspace";
import { workflowReplayKey, type WorkflowRun } from "../src/workflow-discovery";

afterEach(() => vi.unstubAllGlobals());

describe("failed or cancelled loads hand the previous live source back", () => {
  it("restores the previous source once the backend rollback settled", async () => {
    const handback = new LiveSourceHandback(), restored: string[] = [];
    let settle!: () => void;
    const rollback = new Promise<void>(resolve => { settle = resolve; });
    const done = handback.handback("18446744073709551615", rollback, id => restored.push(id));
    await Promise.resolve();
    expect(restored).toEqual([]);
    settle();
    await expect(done).resolves.toBe(true);
    expect(restored).toEqual(["18446744073709551615"]);
  });

  it("restores even when the rollback request itself failed", async () => {
    const restored: string[] = [];
    await expect(new LiveSourceHandback().handback("7", Promise.reject(new Error("offline")), id => restored.push(id))).resolves.toBe(true);
    expect(restored).toEqual(["7"]);
  });

  it("never restores after a newer load or going home superseded it", async () => {
    const handback = new LiveSourceHandback(), restored: string[] = [];
    let settle!: () => void;
    const done = handback.handback("7", new Promise<void>(resolve => { settle = resolve; }), id => restored.push(id));
    handback.supersede();
    settle();
    await expect(done).resolves.toBe(false);
    expect(restored).toEqual([]);
  });

  it("does nothing when no source was live before the load", async () => {
    const restore = vi.fn();
    await expect(new LiveSourceHandback().handback(null, Promise.resolve(), restore)).resolves.toBe(false);
    expect(restore).not.toHaveBeenCalled();
  });

  it("only the latest of two pending handbacks applies", async () => {
    const handback = new LiveSourceHandback(), restored: string[] = [];
    const first = handback.handback("1", Promise.resolve(), id => restored.push(id));
    const second = handback.handback("2", Promise.resolve(), id => restored.push(id));
    expect([await first, await second]).toEqual([false, true]);
    expect(restored).toEqual(["2"]);
  });
});

describe("live-only actions without a live source", () => {
  it("say that no live log is loaded instead of claiming the source changed", () => {
    expect(() => assertMiningSourceBinding(null, null, 1, 1)).toThrow(/No live backend log is loaded/);
    expect(() => assertMiningSourceBinding("1", "2", 1, 1)).toThrow(/changed while this request was pending/);
    expect(() => assertMiningSourceBinding("1", "1", 1, 2)).toThrow(/changed while this request was pending/);
    expect(() => assertMiningSourceBinding("1", "1", 2, 2)).not.toThrow();
  });
});

describe("per-file component identity", () => {
  it("keeps one key per File object and separates same-named uploads", () => {
    const first = new File(["a"], "log.csv"), second = new File(["a"], "log.csv");
    expect(identityKey(first)).toBe(identityKey(first));
    expect(identityKey(first)).not.toBe(identityKey(second));
    expect(identityKey(null)).toBe("none");
  });
});

describe("workspace runs across model imports", () => {
  it("keeps runs on discovered miners and drops runs on the replaced imported net", () => {
    expect(runSurvivesModelImport("inductive")).toBe(true);
    expect(runSurvivesModelImport("alpha")).toBe(true);
    expect(runSurvivesModelImport("imported")).toBe(false);
  });
});

describe("case rows with repeated case ids", () => {
  const traces = [{ case_id: "007" }, { case_id: "7" }, { case_id: "007" }, { case_id: "x" }];
  it("gives every trace its own key", () => {
    const rows = caseRowEntries(traces, "");
    expect(rows.map(row => row.key)).toEqual([0, 1, 2, 3]);
    expect(new Set(rows.map(row => row.key)).size).toBe(traces.length);
    expect(rows[2].trace).toBe(traces[2]);
  });
  it("keeps trace positions as keys while filtering", () => {
    expect(caseRowEntries(traces, " 007 ").map(row => row.key)).toEqual([0, 2]);
    expect(caseRowEntries(traces, "7").map(row => row.key)).toEqual([0, 1, 2]);
  });
});

describe("emptied numeric fields", () => {
  it("map an empty field to unset rather than 0", () => {
    expect(optionalNumber("")).toBeUndefined();
    expect(optionalNumber("  ")).toBeUndefined();
    expect(optionalNumber("2.5")).toBe(2.5);
    expect(optionalNumber("0")).toBe(0);
  });
  it("omit unset anti/multi exponent and epsilon from the query", () => {
    const anti = new URLSearchParams(petriAntiAlignmentQuery({ variant: "version_discounted_a_star", exponent: optionalNumber(""), epsilon: optionalNumber("") }));
    expect(anti.has("exponent")).toBe(false);
    expect(anti.has("epsilon")).toBe(false);
    expect(new URLSearchParams(petriMultiAlignmentQuery({ exponent: optionalNumber("3") })).get("exponent")).toBe("3");
  });
});

describe("checking discovered models", () => {
  it("sends a workflow replay selector only when the run used an explicit one", () => {
    const run = (replayActivityKey?: string) => ({ value: { activity_key: replayActivityKey ?? "concept:name" }, ...(replayActivityKey ? { replayActivityKey } : {}) }) as unknown as WorkflowRun;
    expect(workflowReplayKey(run())).toBeUndefined();
    expect(workflowReplayKey(run("phase"))).toBe("phase");
  });
  it("leaves the trie's default activity selector to the cached source", () => {
    const run = (activity_key: string) => ({ activity_key }) as unknown as StructuralRun;
    expect(structuralReplayKey(run("activity"))).toBeUndefined();
    expect(structuralReplayKey(run("concept:name"))).toBeUndefined();
    expect(structuralReplayKey(run("phase"))).toBe("phase");
  });
});

describe("request errors keep their real cause", () => {
  it("reports DFG and XES parameter errors as themselves, without a request", async () => {
    const fetch = vi.fn();
    vi.stubGlobal("fetch", fetch);
    await expect(fetchDfg({ most_common_paths: "A -> B" })).rejects.toThrow("most_common_paths requires noise_threshold");
    await expect(fetchDfg({ noise_threshold: Number.NaN })).rejects.toThrow("noise_threshold must be finite");
    await expect(fetchXes({ variant: "nope" as "etree" })).rejects.not.toThrow(/could not reach/);
    expect(fetch).not.toHaveBeenCalled();
  });
  it("still reports an unreachable backend as unreachable", async () => {
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(new TypeError("Failed to fetch")));
    await expect(fetchDfg({})).rejects.toThrow(/could not reach the Minera backend/);
  });
});

describe("unreadable mining responses", () => {
  it("records why a response body could not be used", async () => {
    const truncated = new Response('{"family":"ACTIVE_SOURCE_MINING","traces":[');
    expect(await readNativeResponse(truncated, { path: "/api/mine" })).toBeNull();
    expect(nativeResponseFailure(truncated)).toMatch(/body is not readable JSON/);
    const empty = new Response("");
    expect(await readNativeResponse(empty, { path: "/api/mine" })).toBeNull();
    expect(nativeResponseFailure(empty)).toBe("its body was empty");
    const fine = new Response('{"a":1}');
    expect(await readNativeResponse(fine, { path: "/x" })).toEqual({ a: 1 });
    expect(nativeResponseFailure(fine)).toBeUndefined();
  });
  it("does not call a truncated or oversized response an invalid analysis", async () => {
    const file = new File(["case_id,activity\n1,A\n"], "log.csv", { type: "text/csv" });
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response('{"family":"ACTIVE_SOURCE_MINING","traces":[', { status: 200 })));
    const truncated = mineActiveDataset(file);
    await expect(truncated).rejects.toThrow(/could not use its response: its .* body is not readable JSON/);
    await expect(truncated).rejects.not.toThrow(/invalid analysis/);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response("", { status: 413 })));
    await expect(mineActiveDataset(file)).rejects.toThrow(/larger than the Minera backend accepts/);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response('{"unrelated":true}', { status: 200 })));
    await expect(mineActiveDataset(file)).rejects.toThrow(/invalid analysis/);
  });
});
