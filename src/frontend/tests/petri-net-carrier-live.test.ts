import {describe, expect, it} from "vitest";
import {isOriginalOcPlayoutNativeType, isOriginalOcPlayoutWorkbench} from "../src/original-object-centric-playout-workbench";
import {isOriginalPetriAnalysisNativeType, isOriginalPetriAnalysisWorkbench} from "../src/original-petri-analysis-workbench";
import {isOriginalQualityNativeType, isOriginalQualityWorkbench} from "../src/original-quality-workbench";
import {isOriginalDiscoveryWorkbench} from "../src/original-discovery-workbench";
import {isOriginalGeneticMatrixNet} from "../src/original-genetic-matrix-workbench";
import {isLiteralNativeResult} from "../src/literal-controls";
import {parseNativeJson} from "../src/native-json";

// Every workbench writes PetriNet values through the backend's shared
// nine-field carrier (serialize_oc_playout_workbench_petri_net_json), and every
// workbench validator accepts exactly those nine fields. These envelopes are
// live server answers (fixture requests replayed against the current server);
// a native net's place names (tree-converted nets carry them) must never
// become a tenth field, null or not.
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
  ["discovery-mine-ilp.response.json", isOriginalDiscoveryWorkbench],
];

describe("live PetriNet carrier", () => {
  it.each(envelopes)("%s passes its workbench validator", (name, validate) => {
    const value = live(name);
    expect(nets(value).length).toBeGreaterThan(0);
    expect(validate(value)).toBe(true);
  });

  it("every live net has exactly the nine carrier fields", () => {
    for (const [name] of envelopes)
      for (const net of nets(live(name))) expect(Object.keys(net).sort()).toEqual([...CARRIER_FIELDS].sort());
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

  it("a tenth place_names field is what the validators reject", () => {
    const [net] = nets(live("object-centric-playout-ocpn-result.response.json"));
    const named = {...net, place_names: null};
    expect(isOriginalOcPlayoutNativeType("PetriNet", named)).toBe(false);
    expect(isOriginalQualityNativeType("PetriNet", named)).toBe(false);
  });
});
