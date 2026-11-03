/** The native discovery defaults (both true, as the reference) apply when a member is omitted. */
export interface OcpnDiscoveryFlags {
  ocpn_disable_strict_sequence_cut?: boolean;
  ocpn_disable_fallthroughs?: boolean;
}

export const OCPN_DISCOVERY_FLAGS = [
  ["ocpn_disable_strict_sequence_cut", "Disable strict sequence cut"],
  ["ocpn_disable_fallthroughs", "Disable fallthroughs"],
] as const;

/** Share strict boolean validation across all discovery request builders. */
export function appendOcpnDiscoveryFlags(query: URLSearchParams, options: OcpnDiscoveryFlags = {}): void {
  for (const [key] of OCPN_DISCOVERY_FLAGS) {
    const value = options[key];
    if (value === undefined) continue;
    if (typeof value !== "boolean") throw new Error(`${key} must be boolean`);
    query.set(key, value ? "true" : "false");
  }
}
