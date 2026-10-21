# OCEL activity/type matching contract

The public `algo.filtering.ocel.activity_type_matching.apply` keeps the
event-object relation rows admitted by an object-type-to-activities mapping.
The faithful native companion is
`filter_ocel_activity_type_matching_exact`, declared in
`include/minera/ocel_activity_type_matching.hpp`. The original convenience
function retains its existing contract.

## Selection and temporary controls

For each relation, resolve its selected object-type and activity strings and
keep the row when that type's supplied activity collection contains its
activity. An absent mapping key, empty allowed collection or absent activity
does not admit the relation. Mapping collection order and duplicate activity
names do not duplicate source relations. Native activity collections are
vectors; the captured Python string-collection branch binds to a vector of its
individual characters. Literal empty, spaced, Unicode and NUL-containing
identities/type/activity names retain their significance.

The native options carry explicit activity/type selectors, temporary column
and temporary separator controls. Configured public identity/activity/type
aliases bind to canonical native fields; other selector names remain literal.
Missing selector columns raise `std::out_of_range` with the captured quoted
name, including when the source is empty. Canonical native relation roles are
available even when descriptive schema metadata retains its original public
alias. Extra dataframe-only relation cells have no native value member and
are outside the supported native selector domain.

The explicit separator controls distinguish terminal NUL from leading or
interior NUL. Captured terminal-NUL separator controls admit no matching rows;
leading/interior NUL controls retain the ordinary selection. The native
comparison preserves this observed distinction. Both tested concatenation
orientations and twelve omitted-separator collision candidates retain only
the allowed object-type/activity pairs. No undocumented default separator
spelling is inferred from those outputs.

A supplied temporary column is absent from the returned relation schema even
when it reused an existing column. Dropping a native qualifier column projects
its returned missing qualifiers to empty strings. Dropping the relation's
activity/type column changes its relation schema while keeping the actual
event/object records intact. Dropping an identity column raises the captured
missing-ID error when any relation was selected; an empty relation selection
returns the empty result with the dropped schema declaration. Reusing an
otherwise dataframe-only metadata column also removes its declaration.

The native typed options exclude nonstring temporary separators and
noncollection mapping values. The captured invalid-separator/collection and
missing-required-argument errors remain in the evidence. An empty mapping
never uses a separator: the captured empty-mapping/null-separator success
binds to the ordinary typed empty mapping.

## Propagation and storage

Keep admitted relation rows in source order and multiplicity, including
repeated equal rows and distinct qualifiers. Retain participating events and
objects in their source order. Retain O2O/E2E rows when both endpoints remain,
and changes when their object remains. No event/object remains merely because
it was unused. Empty selection empties all six tables while preserving
metadata and schema declarations, except the explicitly removed temporary
relation column. The companion reuses the established stable native
relation-based propagation after selection.

Independent native controls include arbitrary byte strings/WTF-8 and NUL,
duplicate attributes, duplicate relation/graph rows, numeric NaN, signed zero,
inactive type declarations, optional empty schemas, empty selection and
mutation of a result without changing its source. Supported typed logs have
coherent identity references. Native snapshots preserve every available
stored member; public dataframe indices, dtypes and extra relation cells
remain in the evidence archive.

## Retained public evidence and native acceptance

Root executed each of two finite public banks once:

- `build/ocel-activity-type-matching-focused-public-actual-1`: 138 calls,
  127 returned full logs and 11 errors.
- `build/ocel-activity-type-matching-continuation-public-actual-1`: 30 new
  calls, 28 returned full logs and two errors.

An offline receipt audit found one duplicate inside the focused bank:
`mapping-subset-1` and `mapping-key-domain-7` have identical source,
parameters, correspondence and invocation mode. Both observations are retained.
The continuation is disjoint from the focused bank. The **168 total calls**
therefore contain **167 distinct explicit call receipts**; the **155 returned
logs** contain **154 distinct successful call receipts**. No capture was
rerun to replace the duplicate. The duplicate audit and its fingerprint rule
are retained in the evidence archive.

All 168 observations preserve inputs, correspondence mappings and parameter
dictionaries, and all successful returns are independent log objects. The
banks cover 64 mapping subsets, omitted/null/empty and enum/string parameter
bindings, literal role aliases, all six table permutations, unused/empty
shapes, temporary-column reuse, separator controls and error boundaries.
The continuation follows the first bank's observed temporary controls without
replaying its recipes.

`test_ocel_activity_type_matching_module.cpp` embeds all **155 full ordered
native returned snapshots**, including the duplicate receipt, and
**eight native missing-column diagnostics**
directly from those records. Five public errors lie outside the native typed
signature. The assertion-enabled native target is
`minera_ocel_activity_type_matching_module_tests`; `main` directly invokes
the public snapshot, diagnostic and independent state cases. Fresh-source
native acceptance passed **155/155 full returned snapshots**, **8/8 native
diagnostic counterparts**, and the independent storage controls.

`docs/ocel-activity-type-matching-checks.jsonl.gz` retains both complete
observers, banks, durable call intents, actual snapshots, public metadata,
opaque module pins, native recipes/sources/tests and verification bindings.
Every archived member retains its original byte length and SHA-256.
The opaque reference identities match PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899` before and after the public runs.
No reference implementation, AST, bytecode or private state was inspected.
The within-bank duplicate above is preserved and disclosed; neither capture
was rerun, and the continuation repeats no focused-bank receipt.
