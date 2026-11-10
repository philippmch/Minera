import { describe, expect, it } from "vitest";
import { DEFAULT_SOCIAL_NETWORK, defaultSocialNetwork, sessionResourceKey } from "../src/social-network-options";
import { defaultOrganizationalWorkbenchForm, withSessionResourceKey } from "../src/organizational-workbench";

// pm4py's frame of a CSV upload keeps the resource column under its own
// header, so the organizational routes raise KeyError for org:resource; the
// resource fields start at the session's resource key instead (Review AS1).
describe("session resource key", () => {
  it("reads the active source's resource selector", () => {
    expect(sessionResourceKey({active_source: {source_selectors: {resource_key: "res"}}})).toBe("res");
    expect(sessionResourceKey({active_source: {source_selectors: {resource_key: ""}}})).toBeUndefined();
    expect(sessionResourceKey({})).toBeUndefined();
    expect(sessionResourceKey(null)).toBeUndefined();
  });
  it("starts the social network resource field at the session key", () => {
    expect(defaultSocialNetwork("resource")).toEqual({...DEFAULT_SOCIAL_NETWORK, resource: "resource"});
    expect(defaultSocialNetwork(undefined)).toEqual(DEFAULT_SOCIAL_NETWORK);
    expect(DEFAULT_SOCIAL_NETWORK.resource).toBe("org:resource");
  });
  it("prefills the roles resource field unless it was edited", () => {
    const empty = defaultOrganizationalWorkbenchForm();
    expect(withSessionResourceKey(empty, "res").fields).toEqual({roles_resource_attribute: "res"});
    expect(withSessionResourceKey(empty, "org:resource").fields).toEqual({});
    expect(withSessionResourceKey(empty, undefined).fields).toEqual({});
    const prefilled = withSessionResourceKey(empty, "res");
    expect(withSessionResourceKey(prefilled, "Performer", "res").fields).toEqual({roles_resource_attribute: "Performer"});
    expect(withSessionResourceKey(prefilled, "org:resource", "res").fields).toEqual({});
    const edited = {fields: {roles_resource_attribute: "team", roles_threshold: "0.5"}};
    expect(withSessionResourceKey(edited, "Performer", "res")).toBe(edited);
  });
});
