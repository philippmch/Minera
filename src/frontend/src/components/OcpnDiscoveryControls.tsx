import { OCPN_DISCOVERY_FLAGS, type OcpnDiscoveryFlags } from "../ocpn-discovery-options";

/** Keep omission, enabled and explicitly disabled discovery options distinct. */
export function OcpnDiscoveryControls({options = {}, onChange, disabled = false, scope}: {
  options?: OcpnDiscoveryFlags;
  onChange: (options: OcpnDiscoveryFlags) => void;
  disabled?: boolean;
  scope: string;
}) {
  return <>{OCPN_DISCOVERY_FLAGS.map(([key, label]) => <label className="filters-field" key={key}>
    <span>{label}</span>
    <select aria-label={`${scope}: ${label}`} disabled={disabled}
      value={options[key] === undefined ? "native" : String(options[key])}
      onChange={event => {
        const next = {...options};
        if (event.target.value === "native") delete next[key];
        else next[key] = event.target.value === "true";
        onChange(next);
      }}>
      <option value="native">Native default</option>
      <option value="true">Enabled</option>
      <option value="false">Disabled</option>
    </select>
  </label>)}</>;
}
