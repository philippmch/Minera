# OCEL minimum object-type count filtering parity contract

The public module `algo.filtering.ocel.objects_ot_count` is complete under the
repository's semantic parity definition. It filters events by a
dictionary of minimum event-object relation counts per object type. The native
unsigned convenience API is `filter_ocel_events_object_type_counts`; its exact
numeric companion is `filter_ocel_events_object_type_counts_exact`.

## Selection and propagation

Count **relation rows**, including duplicates and different qualifiers. For
each event having relations, require every requested type to occur in its
relation group and to meet the numeric lower bound. An absent type fails even
when its requested bound is zero or negative. Requirements are conjunctive;
dictionary insertion order does not affect the selected event set. An empty
dictionary admits all event groups having relations. Events with no relations
are not admitted by an empty dictionary.

Integer and boolean bounds compare as exact integers. Negative integer bounds
admit every group containing that type; positive fractional bounds admit an
integer row count only when it meets the bound. The exact native companion
uses `TokenInteger` for arbitrary signed integers and `double` for binary64
bounds. It avoids converting exact integers into rounded floating values.
The existing `std::size_t` API preserves its existing nonnegative integer
domain and behavior.

Selection preserves source event order; timestamps and source object/relation
order do not determine counts. Retain all relations of surviving events, in
source order, then participating objects in source order. Keep O2O and E2E
rows only when both endpoints survive, and object changes only when their
objects survive. All six tables retain multiplicity and order. Supported
typed logs have coherent declared identities and relation references.

Return independent native storage, preserving complete stored attributes,
qualifiers, globals, type/schema metadata and optional table-column
declarations, even for empty results. The native-state test additionally
covers inactive schemas, duplicate attributes, null, signed zero, NUL,
arbitrary bytes and WTF-8, and edits a result to check source independence.

## Parameters and diagnostics

The public module exposes `EVENT_ID`, `OBJECT_ID` and `OBJECT_TYPE` bindings.
Omitted, null, empty and irrelevant parameter dictionaries produce equivalent
results. Constructor bindings, explicit string keys, explicit enum keys and
enum-over-string precedence work. Custom column names normalize to the native
event ID, object ID and object type roles before filtering.

Nine observed diagnostics remain in the evidence: missing event/object/type
bindings raise `KeyError`; an omitted required dictionary raises `TypeError`;
null, list and string dictionaries raise `AttributeError`; null and string
threshold values raise `TypeError`. The missing type binding diagnostic says
`Column not found: missing type`. These Python binding and argument-shape
errors do not require fabricated exceptions in typed C++ calls. The exact
native threshold variant admits only its supported numeric types.

Dataframe indices/dtypes and extra dataframe-only columns remain captured.
Native vectors preserve ordered records, while typed `Ocel` has no separate
pandas index, dtype or relation-attribute storage. All stored native members
are preserved. This is the same public-to-native representation boundary as
the other OCEL filtering contracts.

## Evidence and verification

Root executed one public surface/pin probe with zero algorithm calls, followed
by one finite bank of **192 public calls**: **183 returned logs and nine
expected errors**. All sources, function parameters and requirement
dictionaries were unchanged; all successful returns were independent log
objects. No successful public case was replayed.

The bank includes all sixteen two-event/two-type incidence patterns with
eight requirement maps each; duplicate-versus-distinct objects, conjunctive
requirements, absent types, signed, fractional, boolean and exact large
integer bounds; independent event/object/relation/timestamp permutations;
literal labels and IDs; empty source shapes; binding/default/error controls;
metadata and requirement insertion order. Every call snapshots all six tables
and public configuration. The three exact large controls straddling 2^53 and
the control beyond 2^64 remain exact Python integers in the evidence.

Module SHA-256 pins match PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899` before and after capture.
No reference source, AST, bytecode or private state was inspected.
The immutable actual bank is
`build/ocel-objects-ot-count-focused-public-actual-1`; its records SHA-256 is
`b3aadc8e6823fa434a3184d6c56f375c3b599f86329623a302489aa5e1dd71ff`.
The checked archive `docs/ocel-objects-ot-count-checks.jsonl.gz` retains the
public observer, bank, actual records, configuration, pin metadata, focused
build recipe/logs, generated project, source/header/test snapshots and native
check bindings. Each member carries its original byte length and SHA-256.

`test_ocel_objects_ot_count_module.cpp` embeds all **183 complete returned
native fixtures** directly from those public snapshots, rather than deriving
expected records through the native implementation. It also runs the unsigned
API against each compatible public fixture and independently checks complete
native storage and copy behavior. The focused target
`minera_ocel_objects_ot_count_module_tests` compiles the current
`ocel_filter.cpp` and the literal fixture test, linking the validated frozen
native library for other dependencies. Assertions are enabled with
`/UNDEBUG`. Its first build and CTest passed: **183/183 full public outcomes**,
**175 compatible unsigned API controls**, and the independent complete native
state checks. No failed native attempt or public replay occurred.

The structural invariant is a fold over relation rows grouped by event and
object type, followed by a conjunction over required type/lower-bound pairs
and stable event-based propagation. Arbitrary integer bounds compare through
exact `TokenInteger` storage; binary64 bounds use their integral ceiling only
after a range check, preserving the integer comparison above 2^53. The finite
bank exercises the observable branches of those operations rather than
claiming to enumerate arbitrary log sizes.
