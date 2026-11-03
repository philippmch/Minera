import {describe, expect, it} from "vitest";
import {isOriginalOcPlayoutNativeType, isOriginalOcPlayoutWorkbench} from "../src/original-object-centric-playout-workbench";
import {isOriginalPetriAnalysisNativeType, isOriginalPetriAnalysisWorkbench} from "../src/original-petri-analysis-workbench";
import {isOriginalQualityNativeType, isOriginalQualityWorkbench} from "../src/original-quality-workbench";
import {isOriginalDiscoveryWorkbench} from "../src/original-discovery-workbench";
import {isOriginalGeneticMatrixNet} from "../src/original-genetic-matrix-workbench";
import {isLiteralNativeResult} from "../src/literal-controls";
import {parseNativeJson} from "../src/native-json";

// Every workbench writes PetriNet values through the backend's shared carrier
// (serialize_oc_playout_workbench_petri_net_json): nine storage fields, plus
// place_names when the net names its places (tree-converted and PNML nets do;
// token replay breaks ties by place name, so the names must survive). Every
// workbench validator accepts both shapes. These envelopes are live server
// answers (fixture requests replayed against the current server).
const nodeModule = (name: string): Promise<unknown> => import(/* @vite-ignore */ name);
const {readFileSync} = await nodeModule("node:fs") as {readFileSync(path: string, encoding: "utf8"): string};
const {dirname, resolve} = await nodeModule("node:path") as {dirname(path: string): string; resolve(...paths: string[]): string};
const {fileURLToPath} = await nodeModule("node:url") as {fileURLToPath(url: string): string};
const directory = resolve(dirname(fileURLToPath(import.meta.url)), "fixtures/petri-net-carrier-live");
const live = (name: string): unknown => parseNativeJson(readFileSync(resolve(directory, name), "utf8"));

const CARRIER_FIELDS = ["place_count", "source_storage", "sink_storage", "has_source", "has_sink", "activities", "transitions", "discovery_markings", "transition_names"];
const object = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === "object" && !Array.isArray(value);
function nets(value: unknown, found: Array<Record<string, unknown>> = []): Array<Record<string, unknown>> {
  if (Array.isArray(value)) for (const item of value) nets(item, found);
  else if (object(value)) {
    if (Object.hasOwn(value, "place_count") && Object.hasOwn(value, "transition_names")) found.push(value);
    else for (const item of Object.values(value)) nets(item, found);
  }
  return found;
}

const envelopes: Array<[string, (value: unknown) => boolean]> = [
  ["object-centric-playout-ocpn-result.response.json", isOriginalOcPlayoutWorkbench],
  ["petri-analysis-is-workflow-net.response.json", isOriginalPetriAnalysisWorkbench],
  ["quality-evaluate-simplicity.response.json", isOriginalQualityWorkbench],
  // The model_file names its places; the workbench's snapshots keep them.
  ["quality-evaluate-simplicity-named-net.response.json", isOriginalQualityWorkbench],
  ["discovery-mine-ilp.response.json", isOriginalDiscoveryWorkbench],
];

describe("live PetriNet carrier", () => {
  it.each(envelopes)("%s passes its workbench validator", (name, validate) => {
    const value = live(name);
    expect(nets(value).length).toBeGreaterThan(0);
    expect(validate(value)).toBe(true);
  });

  it("the live named net keeps its place names in every snapshot", () => {
    const found = nets(live("quality-evaluate-simplicity-named-net.response.json"));
    expect(found.length).toBeGreaterThan(0);
    for (const net of found) expect(net.place_names).toEqual(["source", "sink"]);
  });

  it("every live net has the nine carrier fields, plus place_names only as a list of names", () => {
    for (const [name] of envelopes)
      for (const net of nets(live(name))) {
        const keys = Object.keys(net).filter(key => key !== "place_names");
        expect(keys.sort()).toEqual([...CARRIER_FIELDS].sort());
        if (Object.hasOwn(net, "place_names")) expect(Array.isArray(net.place_names) && net.place_names.every(item => typeof item === "string")).toBe(true);
      }
  });

  it("every live net passes each PetriNet validator", () => {
    const all = envelopes.flatMap(([name]) => nets(live(name)));
    for (const net of all) {
      expect(isOriginalOcPlayoutNativeType("PetriNet", net)).toBe(true);
      expect(isOriginalQualityNativeType("PetriNet", net)).toBe(true);
      expect(isOriginalPetriAnalysisNativeType("PetriNet", net)).toBe(true);
      expect(isOriginalGeneticMatrixNet(net)).toBe(true);
    }
  });

  it("the live ILP result passes the literal-controls ILP validator", () => {
    const value = live("discovery-mine-ilp.response.json") as Record<string, unknown>;
    expect(value.result_type).toBe("ILPMinerResult");
    expect(isLiteralNativeResult("ilp", {type: value.result_type, value: value.native_result})).toBe(true);
  });

  it("a named net passes every PetriNet validator; a null or unknown tenth field does not", () => {
    for (const [name] of envelopes) for (const net of nets(live(name))) {
      const named = {...net, place_names: Array.from({length: Number(net.place_count)}, (_, index) => `p_${index}`)};
      for (const candidate of [named, net]) {
        expect(isOriginalOcPlayoutNativeType("PetriNet", candidate)).toBe(true);
        expect(isOriginalQualityNativeType("PetriNet", candidate)).toBe(true);
        expect(isOriginalPetriAnalysisNativeType("PetriNet", candidate)).toBe(true);
        expect(isOriginalGeneticMatrixNet(candidate)).toBe(true);
      }
      for (const wrong of [{...net, place_names: null}, {...net, place_names: [1]}, {...net, places: []}]) {
        expect(isOriginalOcPlayoutNativeType("PetriNet", wrong)).toBe(false);
        expect(isOriginalQualityNativeType("PetriNet", wrong)).toBe(false);
      }
    }
  });

  it("envelopes whose nets name their places pass their workbench validators", () => {
    const name = (net: Record<string, unknown>) => {net.place_names = Array.from({length: Number(net.place_count)}, (_, index) => index ? `p_${index}` : "source");};
    for (const [file, validate] of envelopes) {
      const value = live(file), found = nets(value);
      for (const net of found) name(net);
      expect(found.length).toBeGreaterThan(0);
      expect(validate(value)).toBe(true);
    }
    const ilp = live("discovery-mine-ilp.response.json") as Record<string, unknown>;
    for (const net of nets(ilp.native_result)) name(net);
    expect(isLiteralNativeResult("ilp", {type: ilp.result_type, value: ilp.native_result})).toBe(true);
  });
});
