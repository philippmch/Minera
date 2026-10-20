# Log trace-filter parity contract

The completion target is `algo.filtering.log.traces.trace_filter`, pinned to
PM4Py 2.7.22, commit `0715c31f3f1d85924107d6aa5329806db69df899`.
Public metadata establishes the module-owned `apply(log, admitted_traces,
parameters=None)` callable and imported `translate_infix_to_regex(infix)`.
The meaningful controls are `positive` (default true) and the activity key
(default `concept:name`, reference parameter spelling
`pm4py:param:activity_key`). Omitted parameters, `None` and an empty dictionary
have the same effective defaults.

The native public counterpart is `filter_log_traces` in
`include/minera/log_trace_filter.hpp`. It accepts an original native EventLog,
ordered admitted patterns and `LogTraceFilterOptions` containing the two
controls. This implements the reference-semantic serialized-activity behavior.
The established `filter_trace_patterns`/integrated mining DP remains a separate
literal event-vector extension with its documented disabled-control semantics.
No existing mining defaults, delimiter parser, CLI or UI controls are replaced.
The EventLog is the semantic input for this log-module proof; existing native
stream/table conversions remain separate adapter dependencies. The pandas and
polars trace-filter modules retain their own statuses.

## Actual filtering semantics

Each trace's selected string activity values are joined by literal commas.
The admitted expressions use the observed infix converter grammar and are
searched as alternatives. This is not the same operation as comparing event
vectors. For example, an exact `A, B` pattern admits both two events `A` then
`B` and a single event labelled `A,B`. It also admits a trace serialized as
`A,B` followed by one final LF, because the regex end anchor permits a position
immediately before that final LF. CRLF and other whitespace are not generally
trimmed or normalized. Labels are not trimmed, split or treated as arbitrary
regex syntax; metacharacters remain literal.

Empty admitted patterns are valid. An empty pattern and one empty literal both
admit an empty trace and a trace containing one empty activity label. A lone
wildcard admits every serialized trace through its observed unanchored search
behavior. Two consecutive wildcard tokens retain the converter's unusual
expression and do not behave like a normalized universal wildcard: none of the
18 source fixtures matched that expression. Pattern alternatives are combined
by OR, and retained traces stay in original log order.

With no admitted expressions, the empty alternative matches every trace.
Positive polarity therefore retains all traces; negative polarity retains none.
Activity lookup still happens before this match decision. The captured
missing-key/no-admissions controls raise a missing-key error for both
polarities. A missing key raises a native range error identifying that key;
a selected non-string activity raises a native invalid-argument error.
These correspond to captured Python KeyError/TypeError outcomes without
claiming identical language exception classes. Zero-trace logs and all-empty
traces require no event lookup, so a missing activity key is not an error there.

The selected activity field is exact. Custom string attributes work without
being replaced by the default activity projection. Native first-class field
presence/source spelling and selected typed attributes are respected; generic
attributes with null, numeric or container values are not silently converted
into activity strings. Missing/null/non-string diagnostics are native semantic
errors rather than fabricated filtered results.

## Native matching algorithm

The new implementation recognizes the generated expression grammar directly:
literal byte sequences, repeated non-comma/comma groups, the one-group form
created by adjacent trailing wildcards, and terminal comma-prefixed groups.
It uses iterative position states and KMP literal transitions. It does not use
recursive backtracking or a second regex engine with different escaping or
end-anchor semantics. The end-state test explicitly permits one final LF;
the universal unanchored lone-wildcard case follows observed search behavior.
Working match-state memory is linear in serialized trace length plus the active
literal; compiled pattern storage is proportional to the admitted input.

Native Unicode labels use their exact UTF-8 bytes. Commas are ASCII delimiters;
group transitions finish at delimiters, so multibyte characters are not split
into activity boundaries. NUL and other byte values do not terminate matching.
The native API retains raw byte extensions without treating them as new public
reference observations.

## Retained public and native evidence

Root executed **50 public apply calls once**: an initial 40-case bank and a
10-case default/error supplement. There were **45 returned logs and five
expected errors**, with no source or control mutations. All five public log
metadata containers match their source values in every returned log. Every one
of the **347 selected full trace/event snapshots** equals its original source
snapshot. The captures include ordered typed attributes, integers above `2^53`,
floats, booleans, nulls, UTC datetimes, trace properties, log extensions,
classifiers, globals and properties. No reference implementation/private state
was inspected or executed outside the counted public APIs.

Opaque checks bind both the trace-filter module and its imported infix helper
to their pinned Git blobs, permitting only CRLF normalization. The archive
`log-trace-filter-public.json.gz` retains the original two captures, both own
capture scripts, opaque check script and pin receipt. Each member retains exact
UTF-8 text, byte count and SHA-256 for reconstruction. It contains no reference
implementation text. Archive SHA-256:
`1bebf6f36ab5b1a46ca351300863cd02e8de2b905becbd97be30852d5094f4e3` (`177757` bytes).

`src/backend/tests/test_log_trace_filter.cpp` embeds all 50 literal outcomes
from those actual calls. It manually constructs the corresponding typed native
source and compares each complete result with the independently captured
selected source positions through the full raw-log writer. It also verifies
unchanged source/options. Native-only checks preserve all four inactive
compatibility snapshots plus byte/NA cells, reject a non-string selected value,
and match a 150,000-byte literal with iterative state transitions. These
native-only checks are not counted as public reference calls.

A focused C++20 MSVC build passed with assertions explicitly enabled through
`/UNDEBUG`; the complete focused CTest passed after adding the supplement.
Root owns final production CMake registration and structural coverage
verification. This closes the log trace-filter algorithm and its meaningful
controls; it does not promote broader filtering packages or claim new UI,
stream/table adapter, performance or regex-engine parity.
