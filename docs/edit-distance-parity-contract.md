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
| Whole-log alignment and dispatch | String and typed native alignment APIs; core, table, typed-log captures; nine public default/explicit dispatcher pairs and a named native dispatcher acceptance case | Reconcile flat-data parameter coverage and bind the distinct catalogue proof |
| Target choice and output order | Ordinary shortest/last-distinct ties; anti longer/last-distinct ties; duplicate and empty-target captures | Include these contracts in distinct registered module/dispatcher proofs |
| Matching blocks and costs | Ordered move, cost, fitness and best-worst-cost comparisons; 19 long-input threshold cases | 1,922 short singleton comparisons pass; reconcile these with remaining selector-domain and completion evidence |
| Direct trace helpers | Ordered typed mappings, independent list/set inputs, cache identity and reuse; direct and typed-mapping captures; named module acceptance exercises helper aliases, selected activity, anti mode and cache reuse | Bind the distinct module proof and include the existing cohorts in completion review |
| Fresh variant projection | String-only activity domain, typed rejection, ordered validation, timestamps and fresh metadata; 53 typed and 24 legacy comparisons | Reconcile selector/anti behavior with whole-log and adapter controls |
| Input representations | EventLog and EventDataView adapters; table grouping, typed labels, missing/null distinctions; literal empty and whitespace selectors in grouped logs, direct traces and 22 flat-data calls | Reconcile the captured representations and applicable controls in the independent completion review |
| Product entry points | Native APIs, typed JSON, CLI and local server routes; two combined anti/custom-activity/custom-timestamp integration cases in `docs/edit-distance-parameter-integration-validation.json` | Reconcile the verified fixture with the final semantic scope; it is not an exhaustive product-input audit |
| Completion proofs | Distinct dispatcher/module catalogue proof objects bind the registered acceptance functions and implementation symbols; frozen comparison artifacts support the behavioral audit | Close the stream/lazy adapter evidence gap and finish independent completion review |

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

The literal-selector checkpoint in
`docs/edit-distance-selector-domain-validation.json` adds 45 comparable grouped-log
and direct-trace calls, improving the prior library's 22 matches to 45. One
additional Python projector enum-key call applies different activity selectors
in its outer and inner phases; it is retained as reference-only because a single
native activity option cannot express that call. All 2,403 earlier comparisons
and the two product-route fixtures pass against the changed library. This does
not complete either catalogue row.

The subsequent flat-selector checkpoint in
`docs/edit-distance-flat-selectors-validation.json` matches all 22 captured
DataFrame calls, improving the frozen baseline from seven matches. Native tables
accept a unique literal empty column name and preserve it through conversion.
Actual mapped index, attributes and options are checked before and after calls.
All 2,448 earlier comparable calls, five affected native suites and the two
existing product-route fixtures pass against the changed library. The fixtures
do not establish universal DataFrame metadata equivalence or product support
for every blank selector. The subsequent proof registration does not close the
adapter integration gap identified below.

A completion decision must reconcile this contract, the frozen public surface,
the existing cohorts, applicable product adapters, registered proofs, and an
independent scope review. Catalogue validation is a structural precondition;
it cannot substitute for those behavioral checks.

## Completion audit after the flat-selector checkpoint

The independent audit reconciled the six captured public functions, effective
controls, target ordering, costs, direct caches, typed values and fresh projection
with the existing cohorts. It found no new demonstrated reference mismatch.
The public native `EventDataView` overloads also accept event streams and lazy
tables, but the edit-distance suite does not yet exercise those routes directly.
Shared conversion tests alone do not prove their alignment/projector integration.
Both rows therefore remain partial. Required follow-up is a registered carrier
acceptance test comparing these routes with the established eager fixtures,
including custom selectors, anti mode, mixed carriers, projection metadata,
empty/missing boundaries and counted lazy materialization.
