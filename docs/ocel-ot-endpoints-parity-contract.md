# OCEL object-type endpoint filtering parity contract

`algo.filtering.ocel.ot_endpoints` is complete under the repository's semantic
parity definition. Its two native entry points are
`filter_ocel_start_events_per_object_type(log, type)` and
`filter_ocel_end_events_per_object_type(log, type)`.
The existing implementation required no production change.

## Selection and propagation

For every object of the selected type, select its first or last event in
**stored relation-row order**. Keep the union of those event identities.
Changing event-table order, event timestamps or object-table order does not
replace relation-row ordering. Reversing relation rows can change endpoints.
Duplicate rows and qualifier differences retain their original storage and
can determine which occurrence is last; selection remains an event set.

Keep selected events in source event-table order. Keep their relations in
source relation-row order, including relations to other object types. Keep
objects participating in those retained relations, in source object-table
order. Retain O2O rows whose endpoints both remain, E2E rows whose endpoints
both remain, and object changes belonging to retained objects. Preserve
multiplicity and order in all six tables. An absent object type or empty
selection produces empty tables; unused objects and events are not retained
merely because they were declared.

Types and identities are literal strings, including empty, spaced and Unicode
strings. An empty type is a usable label. Empty/objects-only/events-only logs
return empty tables. Supported typed logs have coherent declared identities
and event/object references. No timestamp sorting, attribute coercion or
relation deduplication is introduced.

Both operations return independent logs without mutating the source. Globals,
declared type/schema metadata and optional table-column declarations survive
selection, including an empty result. Complete native event/object attributes,
E2O qualifiers, E2E/O2O qualifiers and object-change values are copied when
their records survive. Tests retain inactive schema/type declarations, nulls,
signed zero, duplicate attributes, NUL, arbitrary bytes and WTF-8; changing a
returned log does not change its input.

## Parameters and diagnostics

The public module exposes `EVENT_ID`, `OBJECT_ID` and `OBJECT_TYPE` column
bindings. Default, null, empty and irrelevant parameter dictionaries have the
same effective behavior. Constructor column bindings are honored when the
function's parameters are omitted; explicit string and enum aliases also
work, and the enum key wins a mixed-key conflict. The native representation
binds these roles to its event IDs, object IDs and object types before the
algorithm runs. Custom source column names normalize into those typed roles;
they do not add a Python dictionary API to C++.

The public corpus also captures six `KeyError` outcomes: one missing event,
object or type binding for each function. Each preserves its offending column
name and leaves its source unchanged. These are source-column binding errors,
not fabricated exceptions for a typed C++ call whose required fields already
exist. The complete records remain alongside the successful outcomes.

Public dataframe indices and dtype bookkeeping remain captured in the proof.
Native vectors preserve the ordered retained records, not pandas index labels.
Additional dataframe-only columns/configuration are observed but do not
create new storage fields in typed `Ocel`. The native contract preserves every
stored member it receives, including all available metadata and table schemas.

## Evidence and focused test

Root executed one public surface/pin probe with zero algorithm calls and then
one finite bank of **166 public calls**. Each function accounts for 83 calls:
**80 returned logs and three expected binding errors**. All 160 successful
results are independent objects; all 166 source/configuration and parameter
snapshots are unchanged. No successful public case was replayed.

The cases cover all sixteen two-event/two-type incidence patterns for three
selected types, eight lifetime models, duplicate and qualified rows, empty
source shapes, custom columns, literal types/IDs and independent event,
relation, object and timestamp permutations. Every case carries six table
snapshots, O2O duplicates/self-relations, E2E rows, object changes and globals;
the metadata control also includes additional event/object/relation columns.
Opaque module pins match PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899` before and after capture.
No reference implementation, bytecode or private state was inspected.

`test_ocel_ot_endpoints_module.cpp` embeds all **160 full returned native
fixtures**, not just event IDs or counts, from those observations. It compares
ordered events, objects, relations, O2O, E2E, object changes, globals and
declared table columns, as well as source before/after snapshots. A separate
native fixture exercises all stored metadata, byte labels, duplicate records,
deep-copy behavior and raw first/last relation positions.

The focused build links the validated frozen native library. The current
`ocel_filter.cpp` is byte-identical to its frozen compiled source (SHA-256
`71904d742b5f688809afd1698bb3cd1ec0e9f7d3d3bd25ff64774e1f2665f8a5`).
Assertions are enabled with `/UNDEBUG`. The target
`minera_ocel_ot_endpoints_module_tests` passed CTest: **80/80 start and 80/80
end outcomes**, plus the independent native-state checks.

Root subsequently compiled the current filter source in the joint closure build
and passed **20/20** affected suites. Both retained corpora were then grouped in
one directly main-called `retained_public_ot_endpoint_cases` function, retaining
every fixture and assertion. The rebuilt endpoint target passed again. Actual
logs are under `build/parity-closures/native-current` (`ctest-boundaries-resume.log`
and `build-endpoint-proof.log`). The archive below retains the preceding focused
test organization; the tracked proof case binds both operations directly.

The structural invariant is a first/last fold over relation positions,
followed by stable event-based propagation through the six tables. Combined
with complete typed input/output preservation, parameter-role mapping and
the observed diagnostic boundary, this establishes module closure rather
than a claim that a finite corpus exhausts arbitrary log size. Other OCEL
filtering modules, I/O and performance are not promoted by this conclusion.

`ocel-ot-endpoints-checks.jsonl.gz` (**186,184 bytes**, SHA-256
`a03d37425c6bd27c933ee1297b1bf231a5635258d7f889d2828912eb98bd3ec8`)
contains **21 byte-verified members**: the public surface probe and pins,
full 166-call recipe/cases/records and receipts, all six binding errors,
unchanged native source/header and full-result test, focused build/CTest
receipts and assertion-enabled compiler command, plus source/library/binary
hash bindings. Raw dataframe indices, dtypes and configuration are retained
in those records rather than silently discarded from the public evidence.
