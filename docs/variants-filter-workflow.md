# Complete activity-variant filtering

The log-filter workspace supports exact variant membership, top-k, individual
minimum/maximum coverage and cumulative coverage. The dataframe workspace adds
membership, top-k and pandas individual coverage filters. Upload genuine CSV,
XES or native-log files for logs, or complete native EventTable JSON for tables.

Membership uses `admitted_json`, an array of literal activity sequences. The
explicit `sequence_kind=list|tuple` control chooses the inner carrier. Omission
retains lists; the UI explicitly selects tuples by default. In the pinned log
and pandas APIs, lists do not match tuple variants. Empty tuples can match an
empty log trace. Commas inside log/pandas tuple elements remain literal. Polars
joins each inner sequence with commas, retaining its observed label collisions.
The request adapter never silently coerces raw list inputs into tuples.

Log membership, top-k and individual coverage filters emit complete traces in
first-encounter variant groups. Cumulative coverage emits ranked groups and
includes frequency ties. Full log, trace and event context survives. Filtering
an alternate activity attribute preserves the original activity role for XES
reopening. The cumulative API ignores `positive`; its UI and route expose no
selection control. Numeric criterion text reaches the native typed parser
without JavaScript number conversion.

pandas membership aggregates each case's actual source rows. Its statistics
instead take a contiguous activity slice starting at each case's first source
position, with length equal to its total row count. This matters when cases are
interleaved; group cases together before relying on ranking/coverage. Timestamp
selectors are inert in all four pinned pandas variants functions and are not
offered. All selected rows retain source order and complete table context.

pandas filtering optionally accepts a separate genuine `variants_file`. The
table index supplies case identities; its `variant` column contains typed tuple
cells. The downloadable example uses the ordinary EventTable input schema,
including `index.class`, names and scalar index labels. This auxiliary table
can override computed membership, including membership applied after ranking
or coverage selection. Its complete snapshot and unchanged check
are returned separately. A column containing case labels is not a substitute
for the index used by the reference.

Polars membership sorts using canonical `time:timestamp`, even when other
timestamp parameters are supplied. Top-k statistics use the selected literal
timestamp column, then final membership still uses canonical time. Thus ranking
can succeed before a later collection error, or choose a sequence that canonical
membership does not find. Native lazy construction and collection preserve
that distinction. The UI offers only working membership/top-k modes. Public
`apply_from_variant_list` and `apply_auto_filter` are faithfully represented by
the captured unavailable `DEFAULT_NAME_KEY` AttributeError, not invented
selection behavior.

`POST /api/reference-table-variants` rejects unknown, duplicate and inactive
controls, accepts multipart source/mapping Files, and returns engine, operation,
error type/message, apply/collect phase, exact counts and complete artifacts.
Before filtering, parsed source and optional mapping snapshots are retained.
Success and later native errors report preservation checks. The UI downloads
full parsed source, mapping, result and raw response bytes. Request generations
and abort signals prevent edited files or criteria from receiving stale results.

The three dedicated contracts describe 1,282 distinct public black-box calls:
380 log, 491 pandas and 411 Polars. Those retained calls must never be replayed.
Continuations reconstructed all complete prior receipts and verified saved
hashes before entry. No reference source, AST, bytecode or private state was
inspected. Native and production acceptance receipts are retained separately
from public observations. Rendered browser interaction is unverified; the
existing application was not replaced.

All three canonical module rows remain partial. The complete catalogue remains
in scope: 86 packages and 387 modules, including 201 strict open units. Remaining
domains and fidelity gaps are listed in each engine contract.

Final acceptance passes 34 assertion-enabled native suites, all 1,282 retained
projected results/diagnostics, 50 production HTTP checks, 186 frontend files /
9,755 tests, 38 final focused tests, TypeScript and Vite. The original Polars
parameter-precedence and eager-diagnostic failures are preserved; both repairs
change general native behavior while leaving expected fixtures unchanged.
`docs/variants-filter-workflow-checks.jsonl.gz` binds complete current sources,
compiled objects/executables/maps, all engine archives and acceptance receipts.
