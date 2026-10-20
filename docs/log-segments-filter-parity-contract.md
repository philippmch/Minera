# Log prefix and suffix filtering parity contract

The completion targets are `algo.filtering.log.prefixes.prefix_filter` and
`algo.filtering.log.suffixes.suffix_filter`, pinned to PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899`. Public metadata establishes each
module's `apply(log, activity, parameters=None)` callable. The meaningful
controls are activity key (`concept:name` by default), strictness (true) and
first/last occurrence (`first`). Omitted parameters, None and an empty
dictionary have identical defaults. Native options express these controls as
strings, a boolean and a first/last enum rather than Python parameter syntax.

The public native counterparts are `filter_log_prefix` and `filter_log_suffix`
in `include/minera/log_segments_filter.hpp`. Their semantic input is an
original EventLog. Existing stream/table conversion adapters retain their own
contracts, and this proof does not promote pandas or polars filtering modules.
The old `filter_prefix`/`filter_suffix` API remains a separate native extension:
it rejects empty targets and its suffix strictness includes the anchor. Its
implementation, callers and defaults are unchanged.

## Selection and slicing

Each trace is scanned in original event order for exact string equality at the
selected key. Labels are not trimmed, split, stringified or interpreted as
patterns. Empty labels, significant spaces, commas, Unicode and embedded NUL
are valid literal targets. A numeric selected value does not match a string
containing its decimal representation. Missing keys and non-string values are
nonmatches, not errors. If the trace has no matching event, it is omitted;
this also omits empty traces. An empty log returns an empty log with context.

First selects the earliest matching event and last selects the latest. The
prefix contains events before that anchor; the suffix contains events after
it. Strict true excludes the anchor for both operations, while false includes
it. A selected trace can therefore have zero events. Missing/non-string values
before or between matching events still occupy their original positions and
remain in the retained slice. The cut is never calculated from a compacted
list of present activity values.

Selected traces retain their original ordering, attributes and properties.
Retained events are complete copies with their original typed attributes and
ordering. Log attributes, extensions, classifiers, globals and properties are
preserved. Source and controls are unchanged. The native implementation also
preserves all ten metadata fields, including inactive compatibility snapshots,
and native first-class field presence/source-key descriptors. Native raw byte
labels remain exact; their extra byte domain is not claimed as a public Python
observation.

The implementation scans each trace once and copies only its selected slice.
It uses no delimiter serialization, regex engine, event reconstruction or
arbitrary input cap. Matching of generic typed attributes precedes native
first-class projection fallback, so a present non-string/null selected cell
cannot accidentally match through a lexical shadow.

## Evidence and executable acceptance

Root executed 52 public calls once, then eight distinct mixed-presence/type
controls once. All **60 returned**, with no source/control mutations. The bank
contains 30 calls per module, including all first/last and strict combinations,
defaults, empty/custom/spaced/NUL/comma/Unicode labels, numeric source fields,
missing keys, zero logs and empty traces. The supplement isolates missing and
numeric/boolean cells around repeated anchors. Across the results, all five
public log metadata containers, all **268 trace contexts**, and all **332
retained event snapshots** equal their corresponding original values.

Opaque checks compare each installed module's bytes with its pinned Git blob,
allowing only CRLF normalization. No reference implementation text, AST,
bytecode or private state was inspected. The compact archive
`log-segments-filter-public.json.gz` retains both exact captures, the two own
capture recipes, opaque check recipe and pin receipt. Its members record
UTF-8 text, original byte count and SHA-256 so they can be reconstructed.
Archive: **193208 bytes**, SHA-256
`832748ae3976cd69fff116f3068b7da04c39c80d3c6ce9cf7d4bafe5717057c1`.

`test_log_segments_filter.cpp` embeds all 60 literal observed event cuts. It
manually constructs the corresponding typed native sources, builds expected
outputs by copying the complete source and replacing only the captured event
slices, and compares through the full raw-log serializer. This independently
checks context copying rather than using the implementation's copy helpers
to construct expected results. Native-only cases retain all inactive metadata
and exercise byte/NUL/WTF-8 labels through first-class custom keys. Those are
not counted as additional public calls.

A focused MSVC C++20 build with `/UNDEBUG` passed its complete CTest. Root owns
production target registration and ledger verification. The proposed target
is `minera_log_segments_filter_tests`, with separate acceptance functions
`log_prefix_filter_module_acceptance` and
`log_suffix_filter_module_acceptance`. These are two bounded module closures;
they do not imply filtering-package, UI, adapter or performance parity.
