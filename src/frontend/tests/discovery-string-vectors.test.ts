import {describe,expect,it} from "vitest";
import {discoveryStringVectorJson,parseDiscoveryStringVector,discoveryStringVectorValidation} from "../src/discovery-string-vectors";
import {constraintQuery,defaultConstraintForm} from "../src/constraint-discovery";
import {dfgDiscoveryQuery,defaultDfgDiscoveryForm} from "../src/dfg-discovery";
import {mineFormEntries} from "../src/api";
import {hasActiveMinerOptions} from "../src/app-logic";

describe("exact discovery string vectors",()=>{
  const names=["A,B","  padded\t","","A,B","資料\u0000",{native_bytes:"hex-v1" as const,data:"00FF2c20"}];
  it("retains commas, padding, blanks, repeats, order, NUL, Unicode and arbitrary bytes",()=>{
    const raw=JSON.stringify(names);expect(parseDiscoveryStringVector(raw,"Names")).toEqual(names);
    expect(discoveryStringVectorJson(names,"Names")).toBe(raw);
  });
  it.each(['null','{}','"A,B"','[null]','[1]','[{"native_bytes":"hex-v1","data":"0"}]','[{"native_bytes":"hex-v1","data":"gg"}]','[{"native_bytes":"hex-v1","data":"00","extra":true}]'])('rejects non-vector or invalid NativeString input %s',raw=>{
    expect(()=>parseDiscoveryStringVector(raw,"Exact activities")).toThrow(/Exact activities/);
    expect(discoveryStringVectorValidation(raw,"Exact activities")).toMatch(/Exact activities/);
  });
  it("rejects malformed/blank JSON instead of treating it as an omitted override",()=>{
    for(const raw of ["","[",'["A",]'])expect(()=>parseDiscoveryStringVector(raw,"Activities")).toThrow(/valid JSON/);
    expect(discoveryStringVectorValidation("[]","Activities")).toBeNull();
    expect(()=>discoveryStringVectorJson(new Array(1),"Activities")).toThrow(/strings/);
  });
  it("keeps Declare inference, explicit empty and literal selection distinct",()=>{
    const form=defaultConstraintForm("declare");form.activitiesMode="literal_json";form.activitiesJson="invalid";
    const omitted=constraintQuery("declare",form,"source");expect(omitted.has("declare_considered_activities_json")).toBe(false);
    form.activitySelection="selected";form.activitiesJson="[]";expect(constraintQuery("declare",form,"source").get("declare_considered_activities_json")).toBe("[]");
    form.activities="legacy,ignored";form.activitiesJson=JSON.stringify(names);const q=constraintQuery("declare",form,"source");
    expect(JSON.parse(q.get("declare_considered_activities_json")!)).toEqual(names);expect(q.has("declare_considered_activities")).toBe(false);
    form.activitiesMode="legacy";expect(constraintQuery("declare",form,"source").get("declare_considered_activities")).toBe("legacy,ignored");
  });
  it("keeps DFG default inference, empty and literal key vectors distinct",()=>{
    const form=defaultDfgDiscoveryForm();form.attributesMode="literal_json";form.attributesJson="invalid";
    expect(dfgDiscoveryQuery(form,"source").has("dfg_case_attributes_json")).toBe(false);
    form.overrideAttributes=true;form.attributesJson="[]";expect(dfgDiscoveryQuery(form,"source").get("dfg_case_attributes_json")).toBe("[]");
    form.attributesJson=JSON.stringify(["cost,net",...names]);const q=dfgDiscoveryQuery(form,"source");
    expect(JSON.parse(q.get("dfg_case_attributes_json")!)).toEqual(["cost,net",...names]);expect(q.has("dfg_case_attributes")).toBe(false);
  });
  it("retains exact vectors through the primary mining form builder, including explicit []",()=>{
    expect(mineFormEntries({declareConsideredActivitiesExact:names,dfgCaseAttributesExact:names})).toEqual([
      ["declare_considered_activities_json",JSON.stringify(names)],["dfg_case_attributes_json",JSON.stringify(names)],
    ]);
    expect(mineFormEntries({declareConsideredActivitiesExact:[],dfgCaseAttributesExact:[]})).toEqual([
      ["declare_considered_activities_json","[]"],["dfg_case_attributes_json","[]"],
    ]);
    expect(hasActiveMinerOptions({declareConsideredActivitiesExact:[]})).toBe(true);
    expect(hasActiveMinerOptions({dfgCaseAttributesExact:[]})).toBe(true);
    expect(hasActiveMinerOptions({})).toBe(false);
  });
  it("rejects ambiguous simultaneous mining controls, even with empty legacy vectors",()=>{
    expect(()=>mineFormEntries({declareConsideredActivities:[],declareConsideredActivitiesExact:[]})).toThrow(/either/);
    expect(()=>mineFormEntries({dfgCaseAttributes:[],dfgCaseAttributesExact:[]})).toThrow(/either/);
  });
  it("rejects an invalid supplied vector before either discovery route issues a request",()=>{
    const declare=defaultConstraintForm("declare");Object.assign(declare,{activitySelection:"selected",activitiesMode:"literal_json",activitiesJson:'[1]'});
    expect(()=>constraintQuery("declare",declare,"source")).toThrow(/strings/);
    const dfg=defaultDfgDiscoveryForm();Object.assign(dfg,{overrideAttributes:true,attributesMode:"literal_json",attributesJson:'[{"native_bytes":"hex-v1","data":"x"}]'});
    expect(()=>dfgDiscoveryQuery(dfg,"source")).toThrow(/strings/);
  });
});
