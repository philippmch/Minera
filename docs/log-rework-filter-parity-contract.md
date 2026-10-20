# Log rework filtering parity contract

The bounded completion target is `algo.filtering.log.rework.rework_filter`,
pinned to PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
The public callable is `apply(log, activity, parameters=None)`. Defaults are
activity key `concept:name`, minimum occurrences 2, and positive true. Omitted
parameters, None, and an empty dictionary agree. Both enum parameter keys and
their public string values were observed.

The native counterpart is `filter_log_rework` in
`include/minera/log_rework_filter.hpp`. Its `LogReworkFilterOptions` keeps exact
signed/unsigned integer minimums, bool, binary64 (including NaN and infinities),
None (`std::monostate`), and strings. None/string comparisons preserve the
observed runtime error category via `std::invalid_argument` and the exact
public error message. A string target has a convenience overload; an
`EventAttribute` target expresses primitive numeric, boolean, and present-null
values. Target attribute keys are irrelevant. Container/date/bytes/NA targets
are outside this primitive target contract and are rejected.

## Trace selection

Every event is counted using equality of its selected activity value with the
target. Labels are literal: empty strings, spaces, commas, newlines, embedded
NUL, and Unicode remain distinct. A string containing a numeric spelling does
not equal the corresponding number. Numeric comparisons preserve Python's
integer/float/bool equality without first rounding integers to binary64.
Present null equals the null target. Missing selected attributes raise
`std::out_of_range` with the quoted key; missing values are not silently
ignored even inside traces with otherwise matching events. An empty trace
contains no selected event access and therefore cannot raise a missing-key
error.

Positive selection requires that the target occurs AND its count is greater
than or equal to the minimum. Consequently, even minimum zero, negative one,
or negative infinity does not select an absent target. Negative selection
keeps absent targets OR counts strictly less than the minimum. These are
independent comparisons: at NaN, present-target traces are omitted under both
polarities, while negative still keeps absent-target traces. Infinity selects
no positive cases and all negative cases. Invalid None/string minimums raise
only when a trace contains the target and a comparison is actually needed;
there is no eager minimum validation.

Retained whole traces and events remain in their original order. All five
public log metadata containers, trace attributes/properties, and typed event
values are preserved. Source and controls remain unchanged. The reference
returns a fresh log and retains aliases to source traces; native value
ownership produces complete independent copies. Native compatibility snapshot
fields, first-class presence/source-key descriptors, and input order are
preserved by the native copy. These native storage details are additional
acceptance checks, not extra public reference calls.

The older integrated `LogFilterOptions.rework_activity` and
`rework_min_occurrences` controls remain a separate native extension: their
empty target disables filtering, they inspect the projected activity text,
and their minimum is unsigned. This closure does not change those controls or
claim that they implement the dedicated reference-semantic API.

## Evidence and executable acceptance

Root executed the frozen finite bank exactly once: **88 public apply calls**,
**78 returns**, and **10 errors**. The bank includes defaults, enum controls,
both polarities, signed/unsigned/boolean/fraction/subnormal/NaN/infinite/None/
string minimums, empty/custom/missing/mixed keys, literal special labels,
empty logs/traces, and numeric/null/string targets. All sources and controls
were unchanged. The returned **489 traces** and **893 events** retain their
complete public context. No reference source, AST, bytecode, private state,
or implementation text was inspected. The installed module matches its
pinned Git blob after only CRLF normalization; normalized SHA-256 is
`3c64797fc5e7c0ffdfd1a41ceb4add7288a6f8854f7d499a64fd6db66e3460fa`.

`log-rework-filter-public.json.gz` retains exact metadata and behavioral
captures, opaque pin receipt, and the own capture/pin/test-generator recipes.
Each archived UTF-8 member carries original byte count and SHA-256. Archive:
**246225 bytes**, SHA-256
`6b5e942e2c7863e7eb1913ca441eb6b36580ebb1ac28a5298813ea5edd34b0e6`.
The behavioral bank was never replayed to repair assertions.

`test_log_rework_filter.cpp` embeds all 88 captured cases in
`log_rework_filter_module_acceptance`. The offline generator reads only the
stored observations; it imports no reference package and performs zero
reference calls. Expected outputs copy the original fixture and select only
the observed trace indices. Full raw-log serialization compares every retained
native field, and errors compare their exact public message. Extra native
checks exercise first-class-only empty/null activity values, source-key
provenance, inactive metadata snapshots, event order, and independent result
ownership. Numeric checks exercise locale-independent float lexical parsing,
minimum subnormal, infinity/NaN spellings, signed zero, exact 2^53/2^63/2^64
integer/float boundaries, and lazy invalid-minimum handling. A focused MSVC
C++20 Release build with `/UNDEBUG` passed the complete test executable
(88 captured cases plus native state/numeric checks), CTest 1/1 passed.
Root owns CMake registration,
coverage ledger, and Git checkpoint. This bounded module closure does not
imply filtering-package, browser/UI, adapter, or performance parity.

