import { afterEach, describe, expect, it, vi } from "vitest";
import { createElement, type ReactElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { ocelDiscoveryQuery, ocelPlayoutQuery, requestOcelDiscovery, requestOcelPlayout } from "../src/api";
import { OCPN_DISCOVERY_FLAGS, appendOcpnDiscoveryFlags, type OcpnDiscoveryFlags } from "../src/ocpn-discovery-options";
import { OcpnDiscoveryControls } from "../src/components/OcpnDiscoveryControls";
import { OcelExplorer } from "../src/components/OcelExplorer";
import { ocelExplorerOptions } from "../src/ocel-explorer-options";
import { defaultOcelPlayout, ocelExecutionPlayoutQuery, requestExecutionPlayout, simulatedOcelFile } from "../src/ocel-execution";
import { type OcelDiscoveryResult } from "../src/types";
import ocpn from "./fixtures/ocel-execution-ocpn.json";
import causal from "./fixtures/ocel-execution-causal_net.json";

const discovery: OcelDiscoveryResult = {family: "OCEL_DISCOVERY", variant: "ocpn", result: {petri_nets: {}, tbr_results: {}}};
const flags: OcpnDiscoveryFlags = {ocpn_disable_strict_sequence_cut: true, ocpn_disable_fallthroughs: false};
const source = new File(['{"events":[],"objects":[]}'], "source-東京.json", {type: "application/json"});
const settings = {activityAttribute: "", objectTypeAttribute: "", annotated: true, replayDiagnostics: false};
afterEach(() => vi.unstubAllGlobals());

describe("OCPN inductive discovery controls", () => {
  it.each([ocelDiscoveryQuery, ocelPlayoutQuery])("preserves omission and both explicit boolean values in a public API builder", builder => {
    expect(builder()).toBe("");
    for (const value of [true, false]) {
      const query = new URLSearchParams(builder({ocpn_disable_strict_sequence_cut: value, ocpn_disable_fallthroughs: value}));
      expect(query.get("ocpn_disable_strict_sequence_cut")).toBe(String(value));
      expect(query.get("ocpn_disable_fallthroughs")).toBe(String(value));
    }
    expect(new URLSearchParams(builder(flags)).get("ocpn_disable_fallthroughs")).toBe("false");
  });
  it.each(OCPN_DISCOVERY_FLAGS.map(([key]) => key))("rejects malformed runtime values for %s before transport", key => {
    for (const value of ["true", "false", "", 0, 1, null, [], {}]) {
      const invalid = {[key]: value} as OcpnDiscoveryFlags;
      expect(() => ocelDiscoveryQuery(invalid)).toThrow(`${key} must be boolean`);
      expect(() => ocelPlayoutQuery(invalid)).toThrow(`${key} must be boolean`);
      const form = defaultOcelPlayout(); form.ocpnDiscovery = invalid;
      expect(() => ocelExecutionPlayoutQuery(form)).toThrow(`${key} must be boolean`);
    }
  });
  it("retains unrelated query fields when appending validated flags", () => {
    const query = new URLSearchParams("seed=37"); appendOcpnDiscoveryFlags(query, flags);
    expect(query.get("seed")).toBe("37"); expect(query.get("ocpn_disable_strict_sequence_cut")).toBe("true");
  });
  it("keeps discovery flags confined to the OCPN model selector", () => {
    expect(ocelExplorerOptions("ocpn", settings)).not.toHaveProperty("ocpn_disable_fallthroughs");
    expect(ocelExplorerOptions("ocpn", {...settings, ocpnDiscovery: flags})).toMatchObject(flags);
    for (const variant of ["ocdfg", "saw", "etot", "otg"] as const) {
      const options = ocelExplorerOptions(variant, {...settings, ocpnDiscovery: flags});
      expect(options).not.toHaveProperty("ocpn_disable_strict_sequence_cut");
      expect(options).not.toHaveProperty("ocpn_disable_fallthroughs");
    }
  });
  it.each(["ocpn", "causal_net"] as const)("preserves native defaults and explicit flags before %s playout discovery", model => {
    const form = defaultOcelPlayout(); form.model = model;
    expect(ocelExecutionPlayoutQuery(form)).not.toContain("ocpn_disable_");
    for (const value of [true, false]) {
      form.ocpnDiscovery = {ocpn_disable_strict_sequence_cut: value, ocpn_disable_fallthroughs: value};
      const query = new URLSearchParams(ocelExecutionPlayoutQuery(form));
      expect(query.get("model")).toBe(model);
      for (const [key] of OCPN_DISCOVERY_FLAGS) expect(query.get(key)).toBe(String(value));
    }
  });
  it("renders both controls only for the OCPN discovery model, preserving disabled state", () => {
    const html = renderToStaticMarkup(createElement(OcelExplorer, {result: discovery, busy: true, onDiscover: () => {}}));
    expect(html).toContain('aria-label="OCEL discovery: Disable strict sequence cut"');
    expect(html).toContain('aria-label="OCEL discovery: Disable fallthroughs"');
    expect(html).toContain('value="native" selected=""');
    const other = renderToStaticMarkup(createElement(OcelExplorer, {result: {...discovery, variant: "etot"}, busy: false, onDiscover: () => {}}));
    expect(other).not.toContain("Disable fallthroughs");
  });
  it("restores omission after an explicit false selection without resetting the other flag", () => {
    let current: OcpnDiscoveryFlags = {...flags};
    const select = () => {
      const fragment = OcpnDiscoveryControls({options: current, onChange: next => {current = next;}, scope: "Playout discovery"});
      const labels = fragment.props.children as ReactElement<{children: ReactElement[]}>[];
      return labels[1].props.children[1] as ReactElement<{onChange: (event: {target: {value: string}}) => void}>;
    };
    select().props.onChange({target: {value: "native"}});
    expect(current).toEqual({ocpn_disable_strict_sequence_cut: true});
    select().props.onChange({target: {value: "false"}});
    expect(current).toEqual(flags);
  });
  it("sends explorer flags, original upload and cancellation through the existing discovery request", async () => {
    const fetch = vi.fn(async (_url: string, _init: RequestInit) => ({ok: true, json: async () => discovery}));
    vi.stubGlobal("fetch", fetch); const signal = new AbortController().signal;
    expect(await requestOcelDiscovery(source, ocelExplorerOptions("ocpn", {...settings, ocpnDiscovery: flags}), signal)).toEqual(discovery);
    const query = new URL(fetch.mock.calls[0][0], "http://localhost").searchParams;
    expect(query.get("ocpn_disable_strict_sequence_cut")).toBe("true"); expect(query.get("ocpn_disable_fallthroughs")).toBe("false");
    expect(fetch.mock.calls[0][1].signal).toBe(signal);
    expect(await ((fetch.mock.calls[0][1].body as FormData).get("log") as File).text()).toBe(await source.text());
  });
  it("sends both flags through the public original-upload playout helper", async () => {
    const fetch = vi.fn(async (_url: string, _init: RequestInit) => ({ok: true, json: async () => ocpn}));
    vi.stubGlobal("fetch", fetch); await requestOcelPlayout(source, {model: "ocpn", ...flags});
    const query = new URL(fetch.mock.calls[0][0], "http://localhost").searchParams;
    expect(query.get("ocpn_disable_strict_sequence_cut")).toBe("true"); expect(query.get("ocpn_disable_fallthroughs")).toBe("false");
    expect(await ((fetch.mock.calls[0][1].body as FormData).get("log") as File).text()).toBe(await source.text());
  });
  it.each(["ocpn", "causal_net"] as const)("binds %s execution flags in the transport and complete report without changing result downloads", async model => {
    const fixture = model === "ocpn" ? ocpn : causal;
    const fetch = vi.fn(async (_url: string, _init: RequestInit) => ({ok: true, status: 200, text: async () => JSON.stringify(fixture)}));
    vi.stubGlobal("fetch", fetch); const form = defaultOcelPlayout(); form.model = model; form.ocpnDiscovery = flags;
    const run = await requestExecutionPlayout(source, form);
    const query = new URL(fetch.mock.calls[0][0], "http://localhost").searchParams;
    expect(query.get("ocpn_disable_strict_sequence_cut")).toBe("true"); expect(query.get("ocpn_disable_fallthroughs")).toBe("false");
    expect(query.get("include_model")).toBe("true"); expect(query.get("include_open_source")).toBe("true");
    const report = JSON.parse(run.report_json);
    expect(report.request.parameters.ocpn_disable_strict_sequence_cut).toBe("true"); expect(report.request.parameters.ocpn_disable_fallthroughs).toBe("false");
    expect(report.response).toEqual(fixture); expect(run.sources.original.name).toBe(source.name);
    expect(run.raw).toBe(JSON.stringify(fixture)); expect(run.tables["generated/events"].count).toBe(Object.keys(fixture.log["ocel:events"]).length);
    expect(JSON.parse(await simulatedOcelFile(run).text())).toEqual(fixture.open_log);
  });
});
