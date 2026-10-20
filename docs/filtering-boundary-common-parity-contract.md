# Boundary-activity common filtering helpers

The completion targets are the two support modules
`algo.filtering.common.start_activities.start_activities_common` and
`algo.filtering.common.end_activities.end_activities_common`, pinned to
PM4Py 2.7.22, commit `0715c31f3f1d85924107d6aa5329806db69df899`.
The modules publicly reexport their implementations from the respective
`pm4py.statistics.{start,end}_activities.common.get` modules. Public signature
inspection established exactly two callable exports in each filtering module:

- `get_sorted_start_activities_list(start_activities)` and
  `get_start_activities_threshold(salist, decreasing_factor)`;
- `get_sorted_end_activities_list(end_activities)` and
  `get_end_activities_threshold(ealist, decreasing_factor)`.

The native public entry points have those names in
`include/minera/filtering_boundary_common.hpp`. They accept ordered frequency
entries, which preserve mapping iteration order independently of label order.
The supported frequency domain is nonnegative native `uint64_t` counts with
exact byte-string activity labels. These are helpers for supplied frequencies;
they do not discover counts from a log or apply an event/trace filter. Existing
log-derived convenience overloads have separate inputs and keep their prior
behavior. No log, dataframe, polars, timestamp, attribute or DFG filtering module
is promoted by these two common-helper proofs.

## Observed semantics and implementation

Sorting uses a stable descending count order. Equal counts retain the supplied
mapping order: the captured reverse-insertion fixture returns `B, A` for tied
counts, while its forward-insertion counterpart returns `A, B`. Sorting by label
would lose that observable behavior. Empty mappings return an empty list;
zero counts, empty labels, Unicode labels and the full native integer count
range are retained. The native ordered mapping rejects duplicate labels because
a mapping cannot contain two simultaneously distinct entries for one key.

Threshold selection starts at the first supplied count. In supplied sequence
order, it replaces the current threshold whenever the next count is strictly
greater than the current threshold multiplied by the decreasing factor. It
continues examining later entries after a failed comparison; it does not stop
at the first frequency gap or implicitly sort the sequence. A captured unsorted
sequence `100, 70, 1, 90` with factor `0.6` returns `90`. Equality does not update
the threshold: `100, 60, 59` with factor `0.6` returns `100`.

Factors retain the full binary64 domain, including zero, negative values,
NaN and both infinities. There is no artificial probability-range validation.
An empty supplied sequence raises an index-range error; native C++ reports
`std::out_of_range` with the captured diagnostic `list index out of range`.
The exception classes belong to their respective languages. No algorithm result
is fabricated for an empty sequence.

The multiplication converts the current integer threshold to binary64, but
comparison must retain the exact integer candidate. A direct C++ integer/double
comparison rounds that candidate and is wrong above `2^53`. The implementation
checks NaN and the floating boundary's range, then compares the candidate to its
nonnegative integral floor. Captured probes at `2^53` and near `2^64` verify this
without requiring an unbounded Python-integer carrier. Frequencies beyond the
native nonnegative count domain and arbitrary nonnumeric Python objects are not
claimed as supported native inputs.

## Actual evidence and focused verification

Root executed **76 public helper calls once**: 60 initial calls and 16 distinct
supplemental calls. They yielded 60 values and 16 expected empty-sequence range
errors, with no argument mutations. Each call retains its original ordered
arguments, exact typed output or exception, and before/after observations.
The initial preparation failure executed zero helper calls: it incorrectly
excluded imported public aliases during metadata discovery. It is retained;
the corrected probe persisted public metadata before preflight assertions.
A separate metadata-only preflight made no functional helper calls.

Root also verified opaque installed hashes for all four public/reexport-origin
modules against pinned Git blobs. Only binary hashing and CRLF normalization
were used; no implementation text, AST, bytecode or private state was displayed
or parsed. Version/path metadata alone was not treated as a sufficient pin.

The compact archive `filtering-boundary-common-public.json.gz` retains both
successful captures, their original scripts, the zero-call failure, metadata
preflight and opaque pin receipt. Each member includes its original byte count
and SHA-256, with the original UTF-8 text for exact byte reconstruction. It
contains no reference implementation text. Archive SHA-256:
`a60723e9486274cd22d5fc956607150f4b35a3ba66df5f69e91a7853d330dfdc`
(8,896 bytes).

`src/backend/tests/test_filtering_boundary_common.cpp` embeds literal input and
expected-output records from all 76 observed calls. The separate
`filtering_start_activities_common_acceptance` and
`filtering_end_activities_common_acceptance` cases invoke both corresponding
native public functions and assert unchanged inputs. Native-only supplemental
checks cover raw invalid UTF-8, embedded NUL, WTF-8 labels and duplicate mapping
rejection; they are not described as new reference observations.

A focused C++20 MSVC build of the new implementation and test passed with
assertions explicitly enabled through `/UNDEBUG`; CTest passed its registered
focused executable. No existing kernel, application service, source cache,
coverage ledger or journal was changed by that check. Root owns production
CMake registration and final structural verification. The finite public records
are combined with the complete four-function surface, ordered mapping model,
strict arithmetic/range semantics and module-specific native tests; they are
not described as an exhaustive enumeration of all frequency maps.
