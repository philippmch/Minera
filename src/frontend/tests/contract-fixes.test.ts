import { describe, it, expect, vi, afterEach } from "vitest";
import workspaceSource from "../src/components/OcelExecutionWorkspace.tsx?raw";
import ocpn from "./fixtures/ocel-execution-ocpn.json";
import { defaultOcelPlayout, requestExecutionPlayout } from "../src/ocel-execution";
import { isOcelPlayoutResult } from "../src/types";
import {
  ocelPlayoutQuery, requestConceptDrift, requestLogToTarget, requestLogUtility, requestTrie,
  requestTriePetriNet, requestIntervalTree, requestRandomTree, requestLinkAnalysis,
  requestKnowledgeInjection, requestOcelSqlite, throwIfBackendError,
} from "../src/api";
import { DEFAULT_GENETIC_FORM, GENETIC_MAX_ACTIVITIES, geneticOptions } from "../src/discovery-model";
import { geneticMinerQuery } from "../src/api";

afterEach(() => vi.unstubAllGlobals());

const source = new File(['{"events":[],"objects":[]}'], "real.json", { type: "application/json" });
const jsonResponse = (value: unknown, status = 200) =>
  vi.fn(async () => new Response(JSON.stringify(value), { status, headers: { "Content-Type": "application/json" } }));

describe("OCEL playout reference binding", () => {
  // The server's native default binding is OcpnEventBinding::kReference,
  // emitted as "reference" (ocel_playout.cpp).
  const reference = { ...ocpn, event_binding: "reference" };
  it("accepts the server's native-default reference binding", async () => {
    expect(isOcelPlayoutResult(reference)).toBe(true);
    const fetch = vi.fn(async () => ({ ok: true, status: 200, text: async () => JSON.stringify(reference) }));
    vi.stubGlobal("fetch", fetch);
    const run = await requestExecutionPlayout(source, defaultOcelPlayout());
    expect((run.result as { event_binding?: string }).event_binding).toBe("reference");
    expect(String((fetch.mock.calls[0] as unknown[])[0])).not.toContain("event_binding");
  });
  it("can request reference binding explicitly and still rejects unknown bindings", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => ({ ok: true, status: 200, text: async () => JSON.stringify(reference) })));
    const form = { ...defaultOcelPlayout(), binding: "reference" as const };
    await expect(requestExecutionPlayout(source, form)).resolves.toBeTruthy();
    expect(isOcelPlayoutResult({ ...ocpn, event_binding: "bogus" })).toBe(false);
    expect(ocelPlayoutQuery({ event_binding: "reference" })).toContain("event_binding=reference");
    expect(() => ocelPlayoutQuery({ event_binding: "bogus" as never })).toThrow(/event_binding/);
  });
  it("offers the reference binding in the workspace select", () => {
    // The binding select renders only on the playout tab (conformance is the
    // initial tab), so check the component source for the option.
    const tsx = workspaceSource;
    const select = tsx.slice(tsx.indexOf("value={simulation.binding}"), tsx.indexOf("</select>", tsx.indexOf("value={simulation.binding}")));
    expect(select).toContain('<option value="reference">');
  });
});

describe("backend error surfacing", () => {
  const message = "concept drift: selected activity selector 'nope' is missing";
  it("throwIfBackendError prefers the server's error, then the status", async () => {
    await expect(throwIfBackendError(new Response(JSON.stringify({ error: message }), { status: 400 }))).rejects.toThrow(message);
    await expect(throwIfBackendError(new Response("not json", { status: 502 }))).rejects.toThrow(/status 502/);
    await expect(throwIfBackendError(new Response("{}", { status: 200 }))).resolves.toBeUndefined();
  });
  const helpers: Array<[string, () => Promise<unknown>]> = [
    ["requestConceptDrift", () => requestConceptDrift({ drift_activity_attribute: "nope" })],
    ["requestLogToTarget", () => requestLogToTarget()],
    ["requestLogUtility", () => requestLogUtility()],
    ["requestTrie", () => requestTrie()],
    ["requestTriePetriNet", () => requestTriePetriNet()],
    ["requestIntervalTree", () => requestIntervalTree()],
    ["requestRandomTree", () => requestRandomTree()],
    ["requestLinkAnalysis", () => requestLinkAnalysis()],
    ["requestKnowledgeInjection", () => requestKnowledgeInjection()],
    ["requestOcelSqlite", () => requestOcelSqlite(new Blob(["x"]))],
  ];
  for (const [name, call] of helpers) it(`${name} surfaces the 4xx error body`, async () => {
    vi.stubGlobal("fetch", jsonResponse({ error: message }, 400));
    await expect(call()).rejects.toThrow(message);
  });
});

describe("genetic miner activity bound", () => {
  it("leaves the server default in place for the default form", () => {
    const options = geneticOptions(DEFAULT_GENETIC_FORM);
    expect(options.max_activities).toBeUndefined();
    expect(geneticMinerQuery(options)).not.toContain("genetic_max_activities");
  });
  it("sends explicit bounds up to the server's hard cap and rejects larger ones", () => {
    expect(GENETIC_MAX_ACTIVITIES).toBe(256);
    expect(geneticOptions({ ...DEFAULT_GENETIC_FORM, activities: "256" }).max_activities).toBe(256);
    expect(() => geneticOptions({ ...DEFAULT_GENETIC_FORM, activities: "257" })).toThrow(/Maximum activities/);
    expect(() => geneticOptions({ ...DEFAULT_GENETIC_FORM, activities: "0" })).toThrow(/Maximum activities/);
  });
});
