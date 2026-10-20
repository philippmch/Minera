import type {DiscoveryVectorMode} from "../discovery-string-vectors";

/** Both editors retain their own text when the input format changes. */
export function DiscoveryVectorInput({label,mode,legacyValue,literalValue,onMode,onLegacy,onLiteral,disabled=false,legacyHint="Comma separated; blank selects none."}:{
  label:string;mode:DiscoveryVectorMode;legacyValue:string;literalValue:string;
  onMode:(mode:DiscoveryVectorMode)=>void;onLegacy:(value:string)=>void;onLiteral:(value:string)=>void;disabled?:boolean;legacyHint?:string;
}) {
  return <div className="filters-field filters-field-wide">
    <label><span>{label} input format</span><select disabled={disabled} value={mode} onChange={event=>onMode(event.target.value as DiscoveryVectorMode)}><option value="legacy">Comma separated</option><option value="literal_json">Literal JSON vector</option></select></label>
    {mode === "literal_json" ? <><label><span>{label} literal JSON</span><textarea disabled={disabled} rows={4} value={literalValue} onChange={event=>onLiteral(event.target.value)} spellCheck={false}/></label><small>Each array entry is one exact name, including commas, spaces, or an empty string. [] selects none. For arbitrary bytes, use {'{"native_bytes":"hex-v1","data":"00ff"}'} as an entry. Order and repeated entries are sent to native discovery.</small></> : <><label><span>{label}</span><input disabled={disabled} type="text" value={legacyValue} onChange={event=>onLegacy(event.target.value)}/></label><small>{legacyHint}</small></>}
  </div>;
}
