import {describe,expect,it} from "vitest";
import {isOriginalTemporalType,originalTemporalDefaultValue} from "../src/original-temporal-workbench";
import {isOriginalTimingType,originalTimingDefaultValue} from "../src/original-timing-workbench";

// Members the native writer emits only when true: absent reads as false, a
// present value must be a Boolean (J28 sentinel, J34 null resource).
describe("optional native flags",()=>{
 const cases=[
  {type:"TemporalDeviation",key:"zero_variance_sentinel",check:isOriginalTemporalType,value:()=>originalTemporalDefaultValue("TemporalDeviation")},
  {type:"BatchGroup",key:"resource_is_null",check:isOriginalTimingType,value:()=>originalTimingDefaultValue("BatchGroup")},
 ];
 for(const c of cases)it("accepts absent or Boolean "+c.type+"."+c.key,()=>{
  const v=c.value() as Record<string,unknown>;
  expect(c.check(c.type,v)).toBe(true);
  expect(c.check(c.type,{...v,[c.key]:true})).toBe(true);
  expect(c.check(c.type,{...v,[c.key]:"true"})).toBe(false);
  expect(c.check(c.type,{...v,other:true})).toBe(false);
 });
});
