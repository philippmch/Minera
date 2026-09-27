# Edit-distance semantic parity contract

Target: PM4Py 2.7.22, commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Scope: `algo.conformance.alignments.edit_distance` and its
`variants.edit_distance` module. Both coverage rows are complete under the
project's behavioral parity contract following the independent completion audit
and the native carrier acceptance checkpoint.

## Public behavior

The pinned public inventory in `docs/edit-distance-public-surface.json.gz`
records six functions: dispatcher `apply`, variant
`apply`, `apply_log`, `apply_trace`, `align_trace`, and
`project_log_on_variant`. The dispatcher has one `EDIT_DISTANCE` selector.
The variant parameter enum exposes anti-alignment, activity key, and timestamp
key. Native `EditDistanceOptions` supplies the corresponding effective controls.
Python enum-key syntax and arbitrary parameter-dictionary objects are not a
separate semantic requirement under the project parity contract.

| Requirement | Current implementation and evidence | Completion reconciliation |
| --- | --- | --- |
| Whole-log alignment and dispatch | String and typed native alignment APIs; core, table, typed-log captures; nine public default/explicit dispatcher pairs and a named native dispatcher acceptance case | 22 flat-selector calls and the distinct dispatcher proof close the remaining checks |
| Target choice and output order | Ordinary shortest/last-distinct ties; anti longer/last-distinct ties; duplicate and empty-target captures | Focused ordering cohorts complement the distinct registered proofs |
| Matching blocks and costs | Ordered move, cost, fitness and best-worst-cost comparisons; 19 long-input threshold cases | 1,922 short singleton comparisons and focused multi-target/long-input cohorts are reconciled |
| Direct trace helpers | Ordered typed mappings, independent list/set inputs, cache identity and reuse; direct and typed-mapping captures; named module acceptance exercises helper aliases, selected activity, anti mode and cache reuse | Distinct module proof and direct-helper cohorts reconciled in completion review |
| Fresh variant projection | String-only activity domain, typed rejection, ordered validation, timestamps and fresh metadata; 53 typed and 24 legacy comparisons | Selector, anti and adapter controls reconciled by the public cohorts and native carrier acceptance |
| Input representations | EventLog and EventDataView adapters; table grouping, typed labels, missing/null distinctions; literal empty and whitespace selectors in grouped logs, direct traces and 22 flat-data calls | EventLog/table public evidence plus stream/lazy native carrier acceptance close the advertised representation checks |
| Product entry points | Native APIs, typed JSON, CLI and local server routes; two combined anti/custom-activity/custom-timestamp integration cases in `docs/edit-distance-parameter-integration-validation.json` | Fixtures verify parameter propagation for the scoped product routes; no exhaustive product-input claim |
| Completion proofs | Distinct dispatcher/module catalogue proof objects bind the registered acceptance functions and implementation symbols; frozen comparison artifacts support the behavioral audit | Carrier gap closed; independent semantic review and distinct catalogue proofs complete |

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
tables, which the suite had not exercised directly at the time of that audit.
Shared conversion tests alone do not prove their alignment/projector integration.
That finding is closed by `edit_distance_native_carrier_acceptance`, called by
the registered Release suite. It compares log, table, stream and lazy routes
against eager results with explicit expected moves and metrics; checks custom
selectors, anti mode, mixed carriers, typed activity identity, fresh projection
metadata and timestamps; and exercises stream/lazy empty-target and missing-field
errors. Counted scanners verify each lazy route consumes its rows once. Flat
carriers cannot represent empty traces; projection omission is checked separately.

The final focused Release build and CTest pass with assertions enabled. Source,
executable, unchanged production library and build/test logs are bound in
`docs/edit-distance-carrier-native-checks.json.gz`, summarized by
`docs/edit-distance-carrier-validation.json`. Independent review found no remaining
semantic issue in these two rows. The prior 2,470 differential comparisons and
product-route fixtures remain applicable because production code is unchanged.
The catalogue proofs are distinct and registered. This closes these rows, not
the remaining project-wide parity work; the finite evidence and native-extension
boundaries described above remain explicit.
