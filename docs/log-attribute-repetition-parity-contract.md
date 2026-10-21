# Log attribute-value repetition: bounded native contract

The public target is `algo.filtering.log.attr_value_repetition.filter`, pinned
to PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Its public callable is `apply(log, value, parameters=None)`. The independent
native companion is `filter_log_attribute_value_repetition` in
`include/minera/log_attribute_repetition_filter.hpp`. The older string-only
`filter_attribute_value_repetition` convenience API retains its contract.
The canonical module remains **partial**; these observations establish the
bounded behavior below, not arbitrary Python-object parity.

## Count and range behavior

For every trace, count present event cells whose literal selected key equals
the supplied typed target, then evaluate `minimum <= count <= maximum` in
that order. Defaults are `concept:name`, 2 and 9223372036854775807. Missing
attributes do not count. A minimum of zero can retain traces with no match,
including empty traces. A reversed range simply retains no qualifying traces.
The upper comparison runs only when the lower comparison succeeds; an empty
log never evaluates either bound.

Keys preserve empty strings, spaces, delimiters, newlines, NUL and Unicode.
`activity` is a literal key and does not alias `concept:name`. First-class
native fields retain their actual source key and explicit missing/null state.
Valid legacy optional fields remain present even when their default state
marker was not populated. Nonstring primitive selectors in the C++ option
variant find no cell in the native string-key carrier.

Targets support null, booleans, exact integers, binary64 numbers, strings,
datetimes, lists, tuples, string-key maps, sets, frozensets and NativeNA in
ordinary native value carriers. Bool/integer/float equality is numeric without
rounding an integer through binary64. Numeric strings remain strings. Direct
NaN equality is false. Datetimes remain distinct from ISO-looking strings;
native datetime comparison preserves nanoseconds and offset-equivalent
instants, and distinguishes naive from aware values. Lists and tuples are
distinct. Maps compare keys and values; set and frozenset values can compare
equal. NativeNA versus null is false. Direct NativeNA scalar comparison
otherwise raises `boolean value of NA is ambiguous`; nested matching singleton
NA values use the observed container identity behavior.

Bounds retain arbitrary decimal `TokenInteger` integers, bool, binary64,
null, string and NativeNA. Null and string comparisons report their exact
observed type-error text through `std::invalid_argument`; NativeNA reports its
ambiguous-truth message. These errors occur only when their comparison is
reached. NaN comparisons are false; infinity and fractional thresholds retain
their ordinary ordered comparison behavior.

Filtering preserves complete stored log/trace/event context and order and
leaves the source unchanged. The public result aliases retained traces; the
native value result owns independent copies, as elsewhere in this native
carrier. Native compatibility snapshots, opaque bytes, source-key/presence
metadata and event input order have separate acceptance controls.

## Public observations and acceptance

Root executed the prepared 95-call public log bank once: 85 returned full
logs and 10 errors. A disjoint 14-call NativeNA continuation returned eight
logs and six ambiguous-truth errors. Across **109 calls**, all complete source,
target, effective supplied parameters and call-shape receipts were distinct;
no overlap or source/target/parameter mutation was observed. Both complete
banks were audited before any algorithm entry, with durable call intents.
Public signatures, enum values and opaque pinned module hashes were retained.
No reference source, AST, bytecode, private state or implementation text was
inspected, and no captured algorithm call was replayed.

The retained roots are
`build/filtering-attribute-repetition-log-public-actual-1` and
`build/filtering-attribute-repetition-log-na-public-actual-1`. The offline
`generate-native-tests.py` imports no reference package and translates every
returned full snapshot and exact error into
`test_log_attribute_repetition_module.cpp`: 44 deduplicated full native
snapshots and 109 outcomes. It also checks independent ownership, opaque
context, first-class fallback, explicit missing markers, nanosecond dates,
timezone equivalence and lazy controls.

The native companion matches all **109/109** full outcomes and exact errors.
The fresh feature module, workbench and real HTTP route suites pass; all four
existing trace/segment/boundary/rework regression suites pass. Fourteen fresh
owned production-handler requests, 9,688 frontend tests in 180 files,
TypeScript and the production Vite build pass. Rendered browser interaction
is not established by the request-helper and SSR tests.

The HTTP and UI use the existing original log-filter workbench. See
`reference-log-filter-workflow.md` for controls, complete downloads, opening
activity-key preservation and validation receipts.

## Remaining boundary

The C++ key and bound variants intentionally do not represent arbitrary Python
objects, container-valued bounds or unhashable selectors. Ordinary typed
container targets are supported; arbitrary custom equality objects, scalar
wrapper/children-list storage and arbitrary nonstring dictionary keys are
outside this contract. Other adapter representations, object-identity graphs,
bytes-specific equality and unobserved exotic scalar combinations are not
promoted by this finite bank. Pandas and Polars entrypoints require independent
observations and companions; log grouping intuition does not establish their
behavior. The package and canonical module remain partial until their
remaining declared domains are resolved.

The deterministic public archive has 17 UTF-8 members, **79,579 bytes**, SHA-256
`bbf5978c267fbe697afa54e355c901f691d575a4d8f58a7eef3dce58456211fb`.
The workflow acceptance archive has 28 members, **43,166 bytes**, SHA-256
`767e428f8ba73c6044bbbd4819cb45ceb3557e6fde996d930c0f2dae842de74a`.
Each member records its original byte count and SHA-256; eleven direct native
and frontend source/test files are bound to the accepted current contents.
