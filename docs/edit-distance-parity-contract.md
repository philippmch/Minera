# Edit-distance semantic parity contract

Target: PM4Py 2.7.22, commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Scope: `algo.conformance.alignments.edit_distance` and its
`variants.edit_distance` module. Both coverage rows remain partial.
This contract organizes the remaining audit; it is not a completion claim.

## Public behavior

The pinned public inventory in `docs/edit-distance-public-surface.json.gz`
records six functions: dispatcher `apply`, variant
`apply`, `apply_log`, `apply_trace`, `align_trace`, and
`project_log_on_variant`. The dispatcher has one `EDIT_DISTANCE` selector.
The variant parameter enum exposes anti-alignment, activity key, and timestamp
key. Native `EditDistanceOptions` supplies the corresponding effective controls.
Python enum-key syntax and arbitrary parameter-dictionary objects are not a
separate semantic requirement under the project parity contract.

| Requirement | Current implementation and evidence | Remaining audit |
| --- | --- | --- |
| Whole-log alignment and dispatch | String and typed native alignment APIs; core, table, typed-log captures | Reconcile default/explicit dispatcher routes and parameter coverage |
| Target choice and output order | Ordinary shortest/last-distinct ties; anti longer/last-distinct ties; duplicate and empty-target captures | Include these contracts in distinct registered module/dispatcher proofs |
| Matching blocks and costs | Ordered move, cost, fitness and best-worst-cost comparisons; 19 long-input threshold cases | 1,922 short singleton comparisons pass; reconcile these with remaining selector-domain and completion evidence |
| Direct trace helpers | Ordered typed mappings, independent list/set inputs, cache identity and reuse; direct and typed-mapping captures | Reconcile helper aliases and effective selector/anti controls |
| Fresh variant projection | String-only activity domain, typed rejection, ordered validation, timestamps and fresh metadata; 53 typed and 24 legacy comparisons | Reconcile selector/anti behavior with whole-log and adapter controls |
| Input representations | EventLog and EventDataView adapters; table grouping, typed labels, missing/null distinctions | Confirm each advertised representation and applicable control has evidence; audit empty and whitespace field names against native selector validation |
| Product entry points | Native APIs, typed JSON, CLI and local server routes | Audit the three semantic controls at applicable CLI/HTTP entry points |
| Completion proofs | Existing registered alignment tests and frozen comparison artifacts | Distinct module/dispatcher proof cases and independent requirement-by-requirement completion review |

`case_id_attribute` governs native flat-data grouping; it does not change an
already grouped EventLog. `deviation_cost` is a native extension rather than a
member of the captured reference parameter enum. Metadata-preserving projection
from supplied alignments is also a native extension; its broader accepted value
domain does not change the fresh public projector's string-only contract.

## Evidence limits

Commit `16267aa6682d0fa2789f29e0c8d12ecfd21d42a5` records 481 passing finite
comparisons and a passing native suite. These are evidence for their captured
inputs, not a proof of all inputs. The 1,922-case short-sequence matrix in
`docs/edit-distance-exhaustive-validation.json` checks
singleton targets only; existing focused cohorts carry the multi-candidate
ordering evidence. Long-input autojunk thresholds remain separately covered.

A completion decision must reconcile this contract, the frozen public surface,
the existing cohorts, applicable product adapters, registered proofs, and an
independent scope review. Catalogue validation is a structural precondition;
it cannot substitute for those behavioral checks.
