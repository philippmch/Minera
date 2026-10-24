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
variant find no cell in the native string-key carrier. Appended typed selector
values also admit hashable tuples, frozensets, bytes, dates and NativeNA. Mutable
list/map/set selectors raise their observed unhashable-type diagnostic only at
event lookup: empty logs and empty traces perform no lookup, but an empty event
still does.

Targets support null, booleans, exact integers, binary64 numbers, strings,
datetimes, bytes, lists, tuples, string-key maps, sets, frozensets and NativeNA in
ordinary native value carriers. Bool/integer/float equality is numeric without
rounding an integer through binary64. Numeric strings remain strings. Direct
NaN equality is false. Datetimes remain distinct from ISO-looking strings;
native datetime comparison preserves nanoseconds and offset-equivalent
instants, and distinguishes naive from aware values. Lists and tuples are
distinct. Maps compare keys and values; set and frozenset values can compare
equal. NativeNA versus null is false. Direct NativeNA scalar comparison
otherwise raises `boolean value of NA is ambiguous`; bytes remain distinct from
strings and compare by their raw contents, including inside ordinary containers.
Nested matching singleton
NA values use the observed container identity behavior.

Bounds retain arbitrary decimal `TokenInteger` integers, bool, binary64,
null, string and NativeNA. Appended ordinary `EventAttribute` alternatives also
carry bytes, datetimes and list/tuple/map/set/frozenset bounds. Their comparisons
report the captured type error only when reached. Null and string comparisons report their exact
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
objects. Ordinary typed container targets, bounds and selectors are supported
within the captured lazy comparison/lookup rules; arbitrary custom equality
objects, scalar
wrapper/children-list storage and arbitrary nonstring dictionary keys are
outside this contract. Other adapter representations, object-identity graphs
and unobserved exotic scalar combinations are not promoted by this finite bank. Pandas and Polars entrypoints require independent
observations and companions; log grouping intuition does not establish their
behavior. The package and canonical module remain partial until their
remaining declared domains are resolved.

The deterministic public archive has 17 UTF-8 members, **79,579 bytes**, SHA-256
`bbf5978c267fbe697afa54e355c901f691d575a4d8f58a7eef3dce58456211fb`.
The workflow acceptance archive has 28 members, **43,166 bytes**, SHA-256
`767e428f8ba73c6044bbbd4819cb45ceb3557e6fde996d930c0f2dae842de74a`.
Each member records its original byte count and SHA-256; eleven direct native
and frontend source/test files are bound to their accepted checkpoint contents.


The domain-bank continuation in
[attribute-repetition-domain-parity.md](attribute-repetition-domain-parity.md)
supersedes the boundary paragraphs above where they differ.

## Ordinary native-value continuation

A fresh bank at `build/filtering-attribute-repetition-log-ordinary-public-actual-1`
adds **86 distinct calls**, **56 complete returned logs** and **30 errors**.
Before all entries, complete source/typed-target/parameter/invocation receipts
were re-fingerprinted against the prior 109 calls: no overlaps or duplicate
candidates were found. The observer retains raw bytes in metadata and selected
cells, and source/target/parameters remained unchanged. The pinned opaque module
hash also remained unchanged. No retained calls were rerun.

The new bank covers lower/upper ordinary container/date/bytes bounds, skipped
upper comparisons, empty-log/trace behavior, mutable versus immutable selectors,
standalone bytes/string separation and nested byte targets. Its offline generator
embeds all 86 outcomes using 18 complete native snapshots in
`test_log_attribute_repetition_ordinary.cpp`. This continuation resolves these
ordinary native-value gaps; the custom-object and other remaining boundaries
above still keep the canonical module partial.

The assertion-enabled `minera_log_attribute_repetition_ordinary_tests` passes
all **86/86** retained outcomes; the unchanged original suite still passes
**109/109**. The first combined run passes all four log/pandas suites but also
contains a separate Polars diagnostic failure. Those named successes do not
claim that the entire first combined run passed. The repaired final combined
run passes **9/9**; both the first and final build/test receipts are retained.
Final acceptance binds the accepted source/test objects and binaries.

The continuation acceptance and compiled bindings are retained separately in
`docs/attribute-repetition-ordinary-checks.jsonl.gz`; the two earlier archives
above remain historical checkpoint evidence.
