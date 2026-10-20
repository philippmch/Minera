# Log start/end activity filter parity contract

The completion targets are `algo.filtering.log.start_activities.start_activities_filter`
and `algo.filtering.log.end_activities.end_activities_filter`, pinned to PM4Py
2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Public signature and documentation observations establish two module-owned
callables in each module: `apply` and `filter_log_by_start_activities` or
`filter_log_by_end_activities`. There is no exported `apply_auto_filter` in
this pinned version. The decreasing-factor enum name alone is not an extra
algorithm; a captured apply call with NaN at that key still performs its
ordinary admissions operation. Previously closed common frequency sorting
and adaptive-threshold helpers have separate contracts and captures.

## Admission filtering

The native counterparts are `filter_log_start_activities` and
`filter_log_end_activities` in `include/minera/log_boundary_filter.hpp`.
`LogBoundaryFilterOptions` contains the activity key (default `concept:name`)
and positive polarity (default true). The original native EventLog is the
semantic log input. Omitted parameters, None and an empty dictionary have
equivalent effective defaults; Python dictionary syntax is not a C++ API goal.

Only the first or last event is examined. Positive filtering retains traces
whose selected endpoint value equals an admitted string; negative filtering
retains nonmatches. Empty traces are excluded for both polarities. Empty
admissions are valid: positive returns none, negative retains all nonempty
traces. Empty labels, significant spaces, commas, embedded NUL and Unicode
remain literal. No trimming, delimiter splitting or lexical conversion occurs.
In particular, an integer cell does not equal its decimal string, and null
does not equal an empty string. Custom selected keys are respected.

A missing selected endpoint key raises a native range error identifying the
key, corresponding to captured public KeyError diagnostics. Empty logs and
empty traces require no endpoint lookup. The native API preserves first-class
presence/source-key descriptors and typed generic cells rather than using a
string projection to hide a missing or non-string value. Retained traces,
events and their ordering are unchanged. All source log context is copied,
including all ten native metadata fields and inactive compatibility state.

The old `filter_start_activities`/`filter_end_activities` convenience entry
points keep their existing disabled-control/native-extension behavior. Their
implementation and callers are unchanged; in particular their negative-empty
admissions rejection is not silently replaced.

## Supplied frequency and variant helper

The native `filter_log_by_start_activities` and
`filter_log_by_end_activities` implement the other actual public callables.
They take the established ordered string/count mapping, ordered groups of
variant keys and complete traces, supplied variant ranking, a binary64
threshold, and the selected activity key. Variant keys are literal activity
vectors, the native representation of public tuple keys. Frequency counts are
nonnegative native uint64 values. These helpers do not import a log, rediscover
counts, sort the supplied groups or apply a decreasing factor internally.

The endpoint of the first trace of the first ranked variant defines a
**protected endpoint class**. All groups with that endpoint are retained even
when the threshold is infinity or NaN. This is not protection of just one
variant. Other groups are retained when their supplied endpoint frequency is
greater than or equal to the supplied threshold. Equality is admitted. NaN
admits no unprotected group; positive infinity admits none; negative infinity
admits all valid groups. The integer count is compared exactly against the
binary64 boundary, without first rounding counts above `2^53` to double.

Output groups follow original variant-map iteration order, independently of
the ranking order. Every trace in a retained group is copied in its supplied
order, including duplicate occurrences. The result has default empty **log**
context because this callable takes variant groups, not a source EventLog.
Trace attributes, properties, complete events and native state are preserved.
Missing selected keys produce key diagnostics. An empty ranking or selected
empty trace produces the observed index-range diagnostic. The native mapping
carrier rejects duplicate map keys; malformed native-only mappings are not
claimed as new public observations. Arbitrary Python object frequency keys
and unbounded integer counts are outside the supported native string/count
domain of the already closed common helpers.

## Actual evidence and verification

Root executed a 60-call bank once, then one successful supplemental call.
The supplemental recipe failed in its own next-case setup because a local
snapshot name overwrote its fixture list. That partial capture and exact
failed recipe/log are retained. A corrected continuation verified the durable
successful record and executed only the five previously uncalled cases.
No successful call was replayed. The combined evidence is **66 calls**:
33 per module, **56 returned logs and ten expected errors** (six missing-key
errors and four empty helper index errors). The records include complete
before/after source, controls, variant groups and ranking. There were no
observed argument mutations. All **426 selected complete trace contexts**
and **814 event snapshots** equal their corresponding original objects.

Opaque checks bind both installed modules to the pinned Git blobs, allowing
only CRLF normalization. Public documentation was read, but no reference
implementation text, AST, bytecode or private state was inspected. The archive
`log-boundary-filter-public.json.gz` contains 14 byte-verified members: all
three functional captures, metadata/documentation observations, own recipes,
the failed recipe/log, pin receipt and focused CTest log. Every member records
its original UTF-8 text, byte count and SHA-256 for reconstruction. Archive:
**374464 bytes**, SHA-256
`a52fa2602db6d2ad037d118755504fb2162d756c3fe7a86d14f2469a4e122298`.

`test_log_boundary_filter.cpp` embeds all 66 literal observations. It manually
constructs the typed original source and supplied maps/groups/ranking,
compares complete outputs through the full raw-log writer, checks expected
diagnostics, and verifies unchanged inputs/options. Expected outputs copy
independently selected source traces, using default context only for the
helper; they do not reuse implementation copy helpers. Native-only cases
exercise inactive metadata, raw byte/WTF-8/NUL keys and labels, multiple
traces per group, uint64 maximum and a count that would compare incorrectly
if rounded to binary64. Those are not additional public calls.

The focused MSVC C++20 build explicitly enables assertions with `/UNDEBUG`
and passed its complete CTest. Proposed registration is
`minera_log_boundary_filter_tests`, with separate acceptance functions
`log_start_activity_filter_module_acceptance` and
`log_end_activity_filter_module_acceptance`. Root owns production CMake and
ledger verification. The two log modules close independently of dataframe,
stream/table conversion, broader filtering, UI and performance claims.
