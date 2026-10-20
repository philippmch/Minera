# OCEL event attribute filtering parity contract

The public module `algo.filtering.ocel.event_attributes` exports exactly two
callables: categorical membership `apply` and interval filtering
`apply_timestamp`. It is complete under the repository's semantic parity
definition. Its faithful native companions are
`filter_ocel_event_attribute_exact` and
`filter_ocel_events_timestamp_exact`. The existing convenience APIs keep their
previous contracts.

## Categorical membership

Select events whose chosen value belongs to the supplied collection, or its
complement when positive is false. Default selection uses the event activity;
event ID and timestamp roles are also available. Empty collections are valid.
An empty attribute name is valid when the source column exists; an absent
column is an error. Enum parameter keys take precedence over conflicting
string keys. Omitted, null, empty and irrelevant parameter dictionaries and
constructor activity aliases retain their observed behavior. The native bool
argument receives the effective truth value of the public positive parameter.

Bool, integer and real scalars compare numerically. In particular, true, 1 and
1.0 match each other, while the string "1" remains distinct. Exact integer
comparisons do not round through double, including object-dtype source
integers immediately around 2^53. Explicit None and NaN remain distinct; NaN
matches a supplied NaN. Membership of a timestamp-valued column distinguishes
a typed datetime query from a literal string query, even when the string
spells that datetime.

`OcelEventAttributeFilterValue` carries scalar values and
`ParsedExactTimestamp` queries. `OcelEventAttributeColumnKind` preserves the
source column's semantic kind. `kTimestamp` selects datetime equality for any
chosen column; `kScalar` preserves literal scalar equality. `kAutomatic` uses
the canonical timestamp role or existing event activity/type declarations
whose attribute types identify dates/times. Date-looking strings do not become
datetimes merely because they parse. Callers binding a dataframe must provide
its actual column kind when that information is not already in native schema
metadata. Actual generic datetime and literal-string column controls verify
both kinds and both query types in both polarities.

## Timestamp intervals

Public **string** bounds use `%Y-%m-%d %H:%M:%S`, including the observed
unpadded month/day/clock components. ISO `T`/zone suffixes and fractional string
suffixes are rejected. A date-only string is incomplete. By contrast, native
`ParsedExactTimestamp` bounds represent the public datetime/Timestamp argument
branch, preserving nanoseconds. These two input kinds are deliberately
distinct in `OcelEventTimestampBound`.

The public datetime branch **replaces timezone information with UTC while
keeping the local clock**, rather than converting the supplied instant. A
bound at `01:00+01:00` is therefore used as `01:00 UTC`. Naive datetime bounds
also acquire UTC. The native companion reconstructs the original local clock
from the parsed instant's offset metadata before this replacement.

Compare source instants exactly, including adjacent nanoseconds at 1900, 2026
and 2100. The range is closed at both ends; valid reversed bounds select an
empty result. NaT source values are omitted. Naive source timestamp columns
raise a comparison error after bound normalization. A timestamp key can select
the canonical time, another datetime column, or an explicitly present empty
column name. Bound parsing precedes the missing-column check, preserving the
observed error precedence.

## Propagation, storage and diagnostics

Both operations retain selected events in source order, including selected
events having no object relation. Keep their relations in source row order,
including duplicates and different qualifiers. Keep participating objects in
source order; retain O2O/E2E rows when both endpoints remain and changes when
their objects remain. Preserve order and multiplicity in all six tables.
Empty selection empties all six tables while retaining metadata.

Return independent native storage. Preserve every stored event/object
attribute, qualifier, change value, global, type/schema declaration and
optional table-column declaration. Independent native controls cover inactive
schemas, duplicate attributes, null, signed zero, NUL, arbitrary byte strings,
WTF-8, explicit/automatic column kinds and editing a result without changing
its source. Supported typed logs have coherent declared identities and table
references.

The full public evidence retains **62 errors** exactly, with messages and
argument tuples. Fifty have direct counterparts within the typed API domain:
missing categorical/timestamp columns, malformed string bounds and naive
source comparisons. Native missing-column exceptions preserve the offending
name; string parse errors preserve the observed messages. A native naive-time
comparison error describes the timezone requirement; Python's original
datetime-versus-Timestamp class-specific wording remains in the public proof.
The other twelve errors concern missing required arguments, non-list-like
membership collections, or non-string/non-datetime bounds, which the native
signatures exclude.

Dataframe indices, dtypes and additional dataframe-only columns remain in the
proof. Native vectors retain ordered records rather than pandas index labels.
Stored scalar payloads retain the existing `OcelValue` domain (null, bool,
signed 64-bit integer, double and string); arbitrary Python objects and integer
payloads outside that native carrier are not silently coerced. The explicit
column kind retains datetime semantics without changing ordinary strings or
expanding the general OCEL scalar carrier. Extra relation dataframe columns
have no independent native storage member. All available native storage is
preserved.

## Evidence and native verification

Root executed three finite public banks exactly once: initial **203 calls**,
a **46-call** valid-string/datetime continuation following the observed strict
string parser, and **eight** generic datetime-versus-literal membership
controls. The continuations reuse no initial recipe excluding its audit name.
The combined **257 calls** returned **195 logs** (**157 categorical**, **38
timestamp**) and **62 errors**. All sources, parameter dictionaries, value
collections and bounds were unchanged; every successful return was an
independent log object. No successful public case was replayed.

All opaque pins match PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899` before and after each capture.
No reference implementation, AST, bytecode or private state was inspected.
The actual banks are `build/ocel-event-attributes-focused-public-actual-1`,
`build/ocel-event-attributes-time-continuation-public-actual-1` and
`build/ocel-event-attributes-datetime-membership-public-actual-1`.
The checked archive `docs/ocel-event-attributes-checks.jsonl.gz` retains the
observers, banks, actual outcomes/configuration, public metadata/pins, native
recipes/projects/logs, source/header/test snapshots and check bindings. Every
archived member carries its original byte length and SHA-256; binary, library,
fresh-object and link-map identities remain in the check bindings.

`test_ocel_event_attributes_module.cpp` embeds every returned ordered native
snapshot directly, plus all fifty representable diagnostic fixtures and the
independent native state controls. The fresh-source native target is
`minera_ocel_event_attributes_module_tests`, with assertions enabled using
`/UNDEBUG`. The first native attempt exposed MSVC `get_time` accepting an
incomplete date-only value at EOF; that failed log is preserved and unread
calendar-field sentinels now enforce complete bounds. Final native
verification passed: **157/157 categorical full returned logs**, **38/38
timestamp full returned logs**, **50/50 native diagnostic counterparts** and
the independent native metadata/kind/copy checks. The same current-source
build also passed the original OCEL filtering, closed endpoint and exact
object-count suites: **four CTest targets passed**. The failed first attempt
and successful second attempt remain in the checked evidence.

The structural invariant is a typed scalar/datetime membership predicate, or
a lexicographic seconds/nanoseconds closed-range predicate after bound-kind
normalization, followed by stable event-based propagation. Type-directed
numeric comparisons, explicit datetime column kinds and exact timestamps
cover the supported native domain without guessing from string contents.
The finite banks exercise these branches and their diagnostic boundaries;
they do not enumerate arbitrary log sizes.
