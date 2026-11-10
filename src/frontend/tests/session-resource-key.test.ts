import { describe, expect, it } from "vitest";
import { DEFAULT_SOCIAL_NETWORK, defaultSocialNetwork, sessionResourceKey } from "../src/social-network-options";
import { ORGANIZATIONAL_FAMILIES, defaultOrganizationalWorkbenchForm, withSessionResourceKey, withSessionResourceKeys } from "../src/organizational-workbench";

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
  it("prefills every organizational family's resource fields (Review AT1)", () => {
    const empty = () => defaultOrganizationalWorkbenchForm();
    const forms = withSessionResourceKeys({roles: empty(), profiles: empty(), diagnostics: empty(), network: empty()}, "res");
    expect(forms.roles.fields).toEqual({roles_resource_attribute: "res"});
    expect(forms.profiles.fields).toEqual({resource_profiles_resource_attribute: "res"});
    expect(forms.diagnostics.fields).toEqual({local_diagnostics_resource_attribute: "res"});
    expect(forms.network.fields).toEqual({network_analysis_source_node_attribute: "res", network_analysis_target_node_attribute: "res"});
    const edited = {...forms, network: {fields: {network_analysis_source_node_attribute: "team", network_analysis_target_node_attribute: "res"}}};
    const next = withSessionResourceKeys(edited, "Performer", "res");
    expect(next.network.fields).toEqual({network_analysis_source_node_attribute: "team", network_analysis_target_node_attribute: "Performer"});
    expect(next.profiles.fields).toEqual({resource_profiles_resource_attribute: "Performer"});
    const cleared = withSessionResourceKeys(next, "org:resource", "Performer");
    for (const family of ORGANIZATIONAL_FAMILIES) expect(Object.values(cleared[family].fields)).not.toContain("Performer");
    expect(cleared.network.fields).toEqual({network_analysis_source_node_attribute: "team"});
    expect(withSessionResourceKey(empty(), "res", undefined, "profiles").fields).toEqual({resource_profiles_resource_attribute: "res"});
  });
});
